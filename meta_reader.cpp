#include "stdafx.h"
#include "meta_reader.h"
#include "meta_sig_parser.h"
#include "meta_utils.h"

using namespace std;
using namespace meta_utils;

#pragma comment(lib, "mscoree.lib")

meta_reader::meta_reader() {}

// bool metadata_reader::init_alt(const wchar_t* filename)
// {
// 	// data from this dispenser has very occasional and subtle differences than MetaDataGetDispenser dispenser, wtf ?
// 
// 	CComPtr<ICLRMetaHost> meta_host;
// 	CComPtr<ICLRRuntimeInfo> runtime_info;
// 	CComPtr<IMetaDataDispenserEx> dispenser;
// 
// 	HRESULT hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, (void **)&meta_host);
// 	if (SUCCEEDED(hr)) hr = meta_host->GetRuntime(L"v4.0.30319", IID_ICLRRuntimeInfo, (void **)&runtime_info);
// 	if (SUCCEEDED(hr)) hr = runtime_info->GetInterface(CLSID_CorMetaDataDispenser, IID_IMetaDataDispenserEx, (void **)&dispenser);
// 	if (SUCCEEDED(hr)) hr = dispenser->OpenScope(filename, ofReadOnly, IID_IMetaDataImport2, (IUnknown**)&metadata_import);
// 	return SUCCEEDED(hr) && metadata_import;
// }

bool meta_reader::init(const wchar_t* filename)
{
	typedef HRESULT (__stdcall *MetaDataGetDispenser_func)(__in REFCLSID rclsid, __in REFIID riid, __deref_out LPVOID FAR * ppv);
	MetaDataGetDispenser_func _MetaDataGetDispenser = nullptr;

	auto rometadata = LoadLibraryW(L"RoMetadata.dll");
	if (rometadata != NULL) _MetaDataGetDispenser = (MetaDataGetDispenser_func) GetProcAddress(rometadata, "MetaDataGetDispenser");

	if (_MetaDataGetDispenser == nullptr) {
		auto mscoree = LoadLibraryW(L"MSCOREE.dll");
		if (mscoree != NULL) _MetaDataGetDispenser = (MetaDataGetDispenser_func) GetProcAddress(mscoree, "MetaDataGetDispenser");		
	}

	if (_MetaDataGetDispenser != nullptr) {
		CComPtr<IMetaDataDispenserEx> dispenser;
		HRESULT hr = _MetaDataGetDispenser(CLSID_CorMetaDataDispenser, IID_IMetaDataDispenserEx, (void **)&dispenser);
		if (SUCCEEDED(hr)) hr = dispenser->OpenScope(filename, ofReadOnly, IID_IMetaDataImport2, (IUnknown**)&metadata_import);
		return SUCCEEDED(hr) && metadata_import;
	}

	return false;
}

std::wstring meta_reader::get_name( mdToken token )
{
	if (metadata_import) {
		MDUTF8CSTR name = nullptr;
		if (SUCCEEDED(metadata_import->GetNameFromToken(token, &name))) {
			return Utf8ToUtf16(name);
		}
	}

	// fall back
	std::wstringstream s;

	switch (TypeFromToken(token)) {
	case mdtModule: s << L"module"; break;
	case mdtTypeRef: s << L"type_ref"; break;
	case mdtTypeDef: s << L"type"; break;
	case mdtFieldDef: s << L"field"; break;
	case mdtMethodDef: s << L"method"; break;
	case mdtParamDef: s << L"param"; break;
	case mdtInterfaceImpl: s << L"if_impl"; break;
	case mdtMemberRef: s << L"member_ref"; break;
	case mdtModuleRef: s << L"module_ref"; break;
	case mdtTypeSpec: s << L"type_spec"; break;
	case mdtGenericParam: s << L"gen_param"; break;
	case mdtMethodSpec: s << L"method_spec"; break;
	default: s << std::uppercase << std::hex << std::setfill(L'0') << std::setw(2) << (TypeFromToken(token) >> 24);
		break;
	}
	s << L"_" << std::uppercase << std::hex << std::setfill(L'0') << std::setw(6) << RidFromToken(token);
	return s.str();

}

std::wstring meta_reader::get_full_name( mdToken token )
{
	if (metadata_import) {
		if (TypeFromToken(token) == mdtTypeDef) {
			wchar_t name[1024] = {};
			if(SUCCEEDED(metadata_import->GetTypeDefProps(token, name, _countof(name), NULL, NULL, NULL))) {
				return std::wstring(name);
			}
		}

		if (TypeFromToken(token) == mdtTypeRef) {
			wchar_t name[1024] = {};
			if(SUCCEEDED(metadata_import->GetTypeRefProps(token, NULL, name, _countof(name), NULL))) {
				return std::wstring(name);
			}
		}

		if (TypeFromToken(token) == mdtGenericParam) {
			wchar_t name[1024] = {};
			if(SUCCEEDED(metadata_import->GetGenericParamProps(token, NULL, NULL, NULL, NULL, name, _countof(name), NULL))) {
				return std::wstring(name);
			}
		}
	}

	// fall back
	return get_name(token);
}

std::vector<mdToken> meta_reader::enum_tokens(std::function<HRESULT(HCORENUM*, mdToken*, ULONG, ULONG*)> enum_func) const
{
	std::vector<mdToken> ret;

	if (metadata_import) {
		HCORENUM e = NULL; mdTypeDef tokens[256] = {}; ULONG count = 0;
		while (enum_func(&e, tokens, _countof(tokens), &count) == S_OK) {
			for(ULONG i = 0; i < count; i++) {
				ret.push_back(tokens[i]);
			}
		}
		metadata_import->CloseEnum(e);
	}

	return ret;
}

std::vector<mdTypeDef> meta_reader::enum_types() const
{
	return enum_tokens([&](HCORENUM* en, mdToken* tokens, ULONG max, ULONG* count){ return metadata_import->EnumTypeDefs(en, tokens, max, count); });
}

std::vector<mdFieldDef> meta_reader::enum_fields(mdTypeDef type)
{
	return enum_tokens([&](HCORENUM* en, mdToken* tokens, ULONG max, ULONG* count){ return metadata_import->EnumFields(en, type, tokens, max, count); });
}

std::vector<mdProperty> meta_reader::enum_properties(mdTypeDef type)
{
	return enum_tokens([&](HCORENUM* en, mdToken* tokens, ULONG max, ULONG* count){ return metadata_import->EnumProperties(en, type, tokens, max, count); });
}

std::vector<mdMethodDef> meta_reader::enum_methods(mdTypeDef type)
{
	return enum_tokens([&](HCORENUM* en, mdToken* tokens, ULONG max, ULONG* count){ return metadata_import->EnumMethods(en, type, tokens, max, count); });
}

std::vector<mdEvent> meta_reader::enum_events(mdTypeDef type)
{
	return enum_tokens([&](HCORENUM* en, mdToken* tokens, ULONG max, ULONG* count){ return metadata_import->EnumEvents(en, type, tokens, max, count); });
}

std::vector<mdGenericParam> meta_reader::enum_generic_params( mdToken type_or_method )
{
	return enum_tokens([&](HCORENUM* en, mdToken* tokens, ULONG max, ULONG* count){ return metadata_import->EnumGenericParams(en, type_or_method, tokens, max, count); });
}

std::vector<mdParamDef> meta_reader::enum_params( mdMethodDef method )
{
	return enum_tokens([&](HCORENUM* en, mdToken* tokens, ULONG max, ULONG* count){ return metadata_import->EnumParams(en, method, tokens, max, count); });
}

std::shared_ptr<meta_field> meta_type_reader::get_field( mdFieldDef token )
{
	auto ret = make_shared<meta_field>();
	ret->token = token;

	wchar_t name[1024] = {}; PCCOR_SIGNATURE sig = nullptr; ULONG sig_size = 0;
	DWORD value_type = 0; UVCP_CONSTANT value = nullptr; ULONG value_length = 0;
	if(SUCCEEDED(metadata_import->GetFieldProps(token, nullptr, name, _countof(name), nullptr, &ret->attributes, &sig, &sig_size, &value_type, &value, &value_length))) {
		ret->name = name;
		
		if ((result->semantics == meta_type::type_semantics::enum_type) && IsFdStatic(ret->attributes) && IsFdPublic(ret->attributes)) {
			ret->visibility = meta_visibility::vis_public;
			ret->display_name = ret->name;
		} else {
			if ((result->semantics == meta_type::type_semantics::enum_type) && IsFdSpecialName(ret->attributes) && !wcscmp(name, COR_ENUM_FIELD_NAME_W)) {
				ret->set_special(meta_field::field_specials::enum_value);
			}

			ret->display_name = meta_sig_parser(metadata_import, sig, sig_size).parse_field(name);
			if (IsFdInitOnly(ret->attributes)) ret->display_name = L"readonly " + ret->display_name;
			if (IsFdStatic(ret->attributes)) ret->display_name = L"static " + ret->display_name;

			if (IsFdPrivate(ret->attributes)) ret->visibility = meta_visibility::vis_private;
			else if (IsFdFamily(ret->attributes)) ret->visibility = meta_visibility::vis_protected;
			else if (IsFdFamORAssem(ret->attributes)) ret->visibility = meta_visibility::vis_protected_internal;
			else if (IsFdPublic(ret->attributes)) ret->visibility = meta_visibility::vis_public;

			wstring access_modifier = format_visibility(ret->visibility);
			if (!access_modifier.empty()) ret->display_name = access_modifier + L" " + ret->display_name;
		}

		if (value != nullptr) {
			switch (value_type) {
			case ELEMENT_TYPE_I1: ret->display_name += L" = " + format_integer(*(char*)value); break;
			case ELEMENT_TYPE_I2: ret->display_name += L" = " + format_integer(*(short*)value); break;
			case ELEMENT_TYPE_I4: ret->display_name += L" = " + format_integer(*(int*)value); break;
			case ELEMENT_TYPE_I8: ret->display_name += L" = " + format_integer(*(__int64*)value); break;
			case ELEMENT_TYPE_U1: ret->display_name += L" = " + format_integer(*(unsigned char*)value); break;
			case ELEMENT_TYPE_U2: ret->display_name += L" = " + format_integer(*(unsigned short*)value); break;
			case ELEMENT_TYPE_U4: ret->display_name += L" = " + format_integer(*(unsigned int*)value); break;
			case ELEMENT_TYPE_U8: ret->display_name += L" = " + format_integer(*(unsigned __int64*)value); break;
			default:
				break;
			}
		}
	}
	return ret;
}

std::shared_ptr<meta_property> meta_type_reader::get_property( mdProperty token )
{
	auto ret = make_shared<meta_property>();
	ret->token = token;

	wchar_t name[1024] = {}; PCCOR_SIGNATURE sig = nullptr; ULONG sig_size = 0;
	mdMethodDef getter = mdMethodDefNil, setter = mdMethodDefNil;
	if(SUCCEEDED(metadata_import->GetPropertyProps(token, nullptr, name, _countof(name), nullptr, &ret->attributes, &sig, &sig_size, nullptr, nullptr, nullptr, &setter, &getter, nullptr, 0, nullptr))) {
		ret->name = name;

		ret->display_name = meta_sig_parser(metadata_import, sig, sig_size, type_generic_param_names).parse_property(name);

		meta_visibility setter_visibility = meta_visibility::vis_unknown, getter_visibility = meta_visibility::vis_unknown;
		for (const auto& m : result->methods) {
			if (m->token == getter) {
				m->set_semantic(meta_method::method_semantics::property_get);
				getter_visibility = m->visibility;
			}
			if (m->token == setter) {
				m->set_semantic(meta_method::method_semantics::property_set);
				setter_visibility = m->visibility;
			}
		}

		meta_visibility property_visibiliy = static_cast<meta_visibility>(max(static_cast<int>(getter_visibility), static_cast<int>(setter_visibility)));
		ret->visibility = property_visibiliy;

		ret->display_name += L" {";
		if (!IsNilToken(getter)) {
			if(property_visibiliy != getter_visibility) {
				auto access_modifier = format_visibility(getter_visibility);
				if (!access_modifier.empty()) ret->display_name += L" " + access_modifier;
			}
			ret->display_name += L" get;";
		}
		if (!IsNilToken(setter)) {
			if(property_visibiliy != setter_visibility) {
				auto access_modifier = format_visibility(setter_visibility);
				if (!access_modifier.empty()) ret->display_name += L" " + access_modifier;
			}
			ret->display_name += L" set;";
		}
		ret->display_name += L" }";

		auto access_modifier = format_visibility(getter_visibility);
		if (!access_modifier.empty()) ret->display_name = access_modifier + L" " + ret->display_name;
	}

	return ret;

}

std::shared_ptr<meta_method> meta_type_reader::get_method( mdMethodDef token )
{
	auto ret = make_shared<meta_method>();
	ret->token = token;

	wchar_t name[1024] = {}; PCCOR_SIGNATURE sig = nullptr; ULONG sig_size = 0;
	if(SUCCEEDED(metadata_import->GetMethodProps(token, nullptr, name, _countof(name), nullptr, &ret->attributes, &sig, &sig_size, nullptr, nullptr))) {
		ret->name = name;

		std::vector<std::wstring> param_names;
		auto params = enum_params(token);
		for (const auto& p : params) param_names.push_back(get_name(p));

		auto generic_params = enum_generic_params(token);
		if (!generic_params.empty()) {
			ret->display_name += L"<";
			bool first = true;
			for (const auto& gp : generic_params) {
				if (!first) ret->display_name += L", "; else first = false;
				ret->display_name += get_full_name(gp);
			}
			ret->display_name += L">";
		}

		bool is_ctor_cctor = false;
		if (IsMdInstanceInitializerW(ret->attributes, name)) {
			ret->set_special(meta_method::method_specials::constructor);
			is_ctor_cctor = true;
		} else if (IsMdClassConstructorW(ret->attributes, name)) {
			ret->set_special(meta_method::method_specials::class_constructor);
			is_ctor_cctor = true;
		}

		if (is_ctor_cctor) {
			ret->display_name = meta_sig_parser(metadata_import, sig, sig_size, type_generic_param_names).parse_method(get_name(result->token).c_str(), param_names, true);
		} else {
			ret->display_name = meta_sig_parser(metadata_import, sig, sig_size, type_generic_param_names).parse_method(name, param_names);
		}
		
		if (IsMdStatic(ret->attributes)) ret->display_name = L"static " + ret->display_name;

		if (IsMdPrivate(ret->attributes)) ret->visibility = meta_visibility::vis_private;
		else if (IsMdFamily(ret->attributes)) ret->visibility = meta_visibility::vis_protected;
		else if (IsMdFamORAssem(ret->attributes)) ret->visibility = meta_visibility::vis_protected_internal;
		else if (IsMdPublic(ret->attributes)) ret->visibility = meta_visibility::vis_public;

		if (IsMdPinvokeImpl(ret->attributes)) ret->display_name = L"extern " + ret->display_name;

		wstring access_modifier = format_visibility(ret->visibility);
		if (!access_modifier.empty()) ret->display_name = access_modifier + L" " + ret->display_name;

		if (IsMdSpecialName(ret->attributes) && !wcscmp(name, L"Invoke")) ret->set_special(meta_method::method_specials::delegate_invoke);
	}
	return ret;
}

std::shared_ptr<meta_event> meta_type_reader::get_event( mdEvent token )
{
	auto ret = make_shared<meta_event>();
	ret->token = token;

	wchar_t name[1024] = {}; PCCOR_SIGNATURE sig = nullptr; ULONG sig_size = 0;
	mdToken type = mdTokenNil; 	mdMethodDef add = mdMethodDefNil, remove = mdMethodDefNil, fire = mdMethodDefNil;
	if(SUCCEEDED(metadata_import->GetEventProps(token, nullptr, name, _countof(name), nullptr, &ret->attributes, &type, &add, &remove, &fire, nullptr, 0, nullptr))) {
		ret->name = name;
		ret->display_name = get_full_name(type) + L" " + ret->name;
	}

	for (const auto& m : result->methods) {
		if (m->token == add) m->set_semantic(meta_method::method_semantics::event_add);
		if (m->token == remove) m->set_semantic(meta_method::method_semantics::event_remove);
		if (m->token == fire) m->set_semantic(meta_method::method_semantics::event_fire);
	}

	return ret;
}

std::shared_ptr<meta_type> meta_type_reader::get_type()
{
	wchar_t name[1024] = {}; PCCOR_SIGNATURE sig = nullptr; ULONG sig_size = 0;
	mdToken extends = mdTokenNil;
	if(SUCCEEDED(metadata_import->GetTypeDefProps(result->token, name, _countof(name), nullptr, &result->attributes, &extends))) {
		result->name = name;

		auto self_name = get_name(result->token);
		auto pos = result->name.rfind(self_name);
		if (pos > 1) result->namespace_name = result->name.substr(0, pos-1);

		wstring semantic_name;
		wstring extends_name;

		if (!IsNilToken(extends)) extends_name = get_full_name(extends);

		if (IsTdClass(result->attributes)) {
			semantic_name = L"class";
			result->set_semantic(meta_type::type_semantics::class_type);
			//TODO: this is dirty and stupid
			if (extends_name == L"System.Enum") {
				result->set_semantic(meta_type::type_semantics::enum_type);
				semantic_name = L"enum";
				extends_name.clear();
			} else if (extends_name == L"System.ValueType") {
				result->set_semantic(meta_type::type_semantics::value_type);
				semantic_name = L"struct";
				extends_name.clear();
			} else if (extends_name == L"System.MulticastDelegate") {
				result->set_semantic(meta_type::type_semantics::delegate_type);
				semantic_name = L"delegate";
				extends_name.clear();
			} else if (extends_name == L"System.Object") {
				// this type just implements some interfaces
				extends_name.clear();
			}
		} else if (IsTdInterface(result->attributes)) {
			semantic_name = L"interface";
			result->set_semantic(meta_type::type_semantics::interface_type);
		}

		auto generic_params = enum_generic_params(result->token);

		result->name = fix_generics_name(result->name, generic_params.size());

		result->display_name = result->name;

		if (!semantic_name.empty()) result->display_name = semantic_name + L" " + result->display_name;

		if (IsTdPublic(result->attributes)) result->visibility = meta_visibility::vis_public;
		else if (IsTdNotPublic(result->attributes)) result->visibility = meta_visibility::vis_internal;

		for (const auto& gp : generic_params) { type_generic_param_names.push_back(get_full_name(gp)); }

		if (!type_generic_param_names.empty()) {
			result->display_name += L"<";
			bool first = true;
			for (const auto& gp_name : type_generic_param_names) {
				if (!first) result->display_name += L", "; else first = false;
				result->display_name += gp_name;
			}
			result->display_name += L">";
		}

		//TODO: this is stupid
		if (result->semantics == meta_type::type_semantics::delegate_type) {
			auto methods = enum_methods(result->token);
			for (const auto& m : methods) {
				wchar_t method_name[1024] = {}; PCCOR_SIGNATURE sig = nullptr; ULONG sig_size = 0; DWORD method_attr;
				if(SUCCEEDED(metadata_import->GetMethodProps(m, nullptr, method_name, _countof(method_name), nullptr, &method_attr, &sig, &sig_size, nullptr, nullptr))) {
					if (IsMdSpecialName(method_attr) && !wcscmp(method_name, L"Invoke")) {
						std::vector<std::wstring> method_param_names;
						auto method_params = enum_params(m);
						for (const auto& p : method_params) method_param_names.push_back(get_name(p));
						result->display_name = meta_sig_parser(metadata_import, sig, sig_size, type_generic_param_names).parse_method(result->display_name.c_str(), method_param_names);
						break;
					}
				}
			}
		}

		if (!extends_name.empty()) {
			result->display_name += L" : " + extends_name;
		}

		wstring access_modifier = format_visibility(result->visibility);
		if (!access_modifier.empty()) result->display_name = access_modifier + L" " + result->display_name;
	}

	auto fields = enum_fields(result->token);
	for (const auto& f : fields) result->fields.push_back(get_field(f));

	auto methods = enum_methods(result->token);
	for (const auto& m : methods) result->methods.push_back(get_method(m));

	auto properties = enum_properties(result->token);
	for (const auto& p : properties) result->properties.push_back(get_property(p));

	auto events = enum_events(result->token);
	for (const auto e : events) result->events.push_back(get_event(e));

	return result;
}

std::wstring meta_type_reader::format_visibility( meta_visibility visibility )
{
	switch (visibility) {
	case meta_visibility::vis_unknown: return wstring();
	case meta_visibility::vis_private: return L"private";
	case meta_visibility::vis_protected: return L"protected";
	case meta_visibility::vis_internal: return L"internal";
	case meta_visibility::vis_protected_internal: return L"protected internal";
	case meta_visibility::vis_public: return L"public";
	default: return wstring();
	}
}

std::wstring meta_type_reader::get_full_name( mdToken token )
{
	if (metadata_import) {
		if (TypeFromToken(token) == mdtTypeSpec) {
			PCCOR_SIGNATURE sig = nullptr; ULONG sig_size = 0;
			if(SUCCEEDED(metadata_import->GetTypeSpecFromToken(token, &sig, &sig_size))) {
				return meta_sig_parser(metadata_import, sig, sig_size, type_generic_param_names).parse_element_type();
			}
		}
	}

	return meta_reader::get_full_name(token);
}
