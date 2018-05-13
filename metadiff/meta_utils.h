#pragma once
#include <string>
#include <strstream>

namespace meta_utils
{
	template<typename T> static std::wstring format_integer(T value) {
		std::wstringstream ss;
		ss << value;
		return ss.str();
	}
	std::wstring Utf8ToUtf16(const std::string& str);
	std::wstring fix_generics_name(const std::wstring& name, int generics_count);
};

