#include "stdafx.h"
#include "meta_sig_parser.h"
#include "meta_utils.h"

meta_sig_parser::meta_sig_parser(IMetaDataImport *import, PCCOR_SIGNATURE signature, ULONG sig_size)
	:m_import(import), m_signature(signature), m_sig_size(sig_size), m_sig_pos(0) {}

meta_sig_parser::meta_sig_parser(IMetaDataImport *import, PCCOR_SIGNATURE signature, ULONG sig_size, const std::vector<std::wstring>& var_names)
	:m_import(import), m_signature(signature), m_sig_size(sig_size), m_sig_pos(0), m_var_names(var_names) {}

std::wstring meta_sig_parser::parse_field(wchar_t* name)
{
	uncompress_data(); // field = 0x06
	return parse_element_type() + L" " + std::wstring(name);
}

std::wstring meta_sig_parser::parse_property(wchar_t* name)
{
	uncompress_data(); // property = 0x08 or property | has_this = 0x28 
	
	ULONG param_count = uncompress_data();
	for (ULONG i = 0; i < param_count; ++i) {
		uncompress_data(); // custom_mod
	}

	return parse_element_type() + L" " + std::wstring(name);
}

std::wstring meta_sig_parser::parse_method(const wchar_t* name, const std::vector<std::wstring>& raw_param_names, bool omit_return_type)
{
	std::wstring ret = name;
	std::vector<std::wstring> param_names = raw_param_names;
	CorCallingConvention calling_conv = (CorCallingConvention) uncompress_data(); // calling convention
	ULONG param_count = uncompress_data();

	std::wstring return_type = parse_element_type();
	if (return_type != L"void")
	{
	    param_names.erase(param_names.begin());
	}

	if (!omit_return_type) ret = return_type  + L" " + ret;

	if (param_count != 0) {
		ret += L"( ";
		for (UINT i = 0; i < param_count; i++) {
			ret += parse_element_type();
			if (i < param_names.size()) ret += L" " + param_names[i];
			if (i < param_count - 1) ret += L", ";
		}

		if (calling_conv == IMAGE_CEE_CS_CALLCONV_VARARG) {
			if (param_count) ret += L", ";
			ret += L"...";
		}
		ret += L" )";
	} else ret += L"()";

	return ret;
}

std::wstring meta_sig_parser::parse_element_type()
{
	CorElementType type = ELEMENT_TYPE_END;
	m_sig_pos += CorSigUncompressElementType(m_signature + m_sig_pos, &type);

	switch (type) {
	case ELEMENT_TYPE_VOID:     return L"void";
	case ELEMENT_TYPE_BOOLEAN:  return L"bool";
	case ELEMENT_TYPE_I1:       return L"sbyte";
	case ELEMENT_TYPE_U1:       return L"byte";
	case ELEMENT_TYPE_I2:       return L"short";
	case ELEMENT_TYPE_U2:       return L"ushort";
	case ELEMENT_TYPE_CHAR:     return L"char";
	case ELEMENT_TYPE_I4:       return L"int";
	case ELEMENT_TYPE_U4:       return L"uint";
	case ELEMENT_TYPE_I8:       return L"long";
	case ELEMENT_TYPE_U8:       return L"ulong";
	case ELEMENT_TYPE_R4:       return L"float";
	case ELEMENT_TYPE_R8:       return L"double";
	case ELEMENT_TYPE_OBJECT:   return L"object";
	case ELEMENT_TYPE_STRING:   return L"string";
	case ELEMENT_TYPE_I:        return L"IntPtr";
	case ELEMENT_TYPE_U:        return L"UIntPtr";
	case ELEMENT_TYPE_VALUETYPE:
	case ELEMENT_TYPE_CLASS: {
		mdToken token = uncompress_token();
		MDUTF8CSTR name_utf8;
		if (SUCCEEDED(m_import->GetNameFromToken(token, &name_utf8))) {
			return meta_utils::Utf8ToUtf16(name_utf8);
		} else return L"**Unknown Type**";
	}
	case ELEMENT_TYPE_TYPEDBYREF:	return L"TypedReference";
	case ELEMENT_TYPE_BYREF:		return L"ref " + parse_element_type();
	case ELEMENT_TYPE_SZARRAY:      return parse_element_type() + L"[]";
	case ELEMENT_TYPE_ARRAY: {
		std::wstring ret = parse_element_type();
		ret += L"[";
		ULONG rank = uncompress_data();
		if (rank > 0) {
			ULONG sizes = uncompress_data();
			for (ULONG i = 0; i < sizes; i++) {
				ULONG dimSize = uncompress_data();
				if (i > 0) ret += L",";
			}

			ULONG lowers = uncompress_data();
			for (ULONG i = 0; i < lowers; i++) {
				int lowerBound = uncompress_int();
			}
		}
		ret += L"]";
		return ret;
	}
	case ELEMENT_TYPE_PTR:		return parse_element_type() + L"*";
	case ELEMENT_TYPE_CMOD_REQD:return L"CMOD_REQD" + parse_element_type();
	case ELEMENT_TYPE_CMOD_OPT:	return L"CMOD_OPT" + parse_element_type();
	case ELEMENT_TYPE_MODIFIER:	return parse_element_type();
	case ELEMENT_TYPE_PINNED:	return L"pinned " + parse_element_type();
	case ELEMENT_TYPE_SENTINEL:	return std::wstring();
	case ELEMENT_TYPE_GENERICINST: {
		std::wstring ret = parse_element_type();
		ULONG count = uncompress_data();
		ret = meta_utils::fix_generics_name(ret, count);
		ret += L"<";
		for (ULONG i = 0; i < count; i++) {
			ret += parse_element_type();
			if (i < (count - 1)) ret += L", ";
		}
		ret += L">";
		return ret;
	}
	case ELEMENT_TYPE_VAR: {
		ULONG num = uncompress_data();
		if (num < m_var_names.size()) return m_var_names[num];
		else {
			WCHAR num_str[64];
			_itow_s(num, num_str, 10);
			return L"!" + std::wstring(num_str);
		}
	}
	case ELEMENT_TYPE_MVAR: {
		ULONG num = uncompress_data();
		if (num < m_mvar_names.size()) return m_mvar_names[num];
		else {
			WCHAR num_str[64];
			_itow_s(num, num_str, 10);
			return L"!!" + std::wstring(num_str);
		}
	}
	case ELEMENT_TYPE_END:		return std::wstring();
	default: return L"***UNKNOWN TYPE***";
	}
}

ULONG meta_sig_parser::uncompress_data()
{
	ULONG ret = 0;
	m_sig_pos += CorSigUncompressData(m_signature + m_sig_pos, &ret);
	return ret;
}

int meta_sig_parser::uncompress_int()
{
	int ret = 0;
	m_sig_pos += CorSigUncompressSignedInt(m_signature + m_sig_pos, &ret);
	return ret;
}

mdToken meta_sig_parser::uncompress_token()
{
	mdToken ret = mdTokenNil;
	m_sig_pos += CorSigUncompressToken(&m_signature[m_sig_pos], &ret);
	return ret;
}
