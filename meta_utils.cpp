#include "stdafx.h"
#include "meta_utils.h"

std::wstring meta_utils::fix_generics_name( const std::wstring& name, int generics_count )
{
	std::wstring ret = name;
	if (generics_count > 0) {
		std::wstring appendix = L"`" + format_integer(generics_count);
		auto pos = ret.rfind(appendix);
		if (pos == (ret.length() - appendix.length()))
			ret.erase(pos);
	}
	return ret;
}

std::wstring meta_utils::Utf8ToUtf16( const std::string& str )
{
	std::wstring ret;

	int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), -1, nullptr, 0);

	if (len > 0) {
		ret.resize(len - 1);
		MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), len - 1, const_cast<wchar_t*>(ret.c_str()), len);
	}

	return ret;
}
