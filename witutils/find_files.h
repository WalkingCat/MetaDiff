#pragma once
#include <string>
#include <map>

std::map<std::wstring, std::wstring> find_files(const wchar_t* pattern);
std::map<std::wstring, std::map<std::wstring, std::wstring>> find_files_ex(const std::wstring& pattern); //TODO: support recursive
