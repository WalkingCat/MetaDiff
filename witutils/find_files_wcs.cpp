#include "stdafx.h"
#include "find_files_wcs.h"
#include "find_files.h"

using namespace std;

wstring read_attribute_value(IXmlReader* xml_reader, const wchar_t* attribute) {
	wstring ret;
	const wchar_t* buf = nullptr; UINT len = 0;
	if (SUCCEEDED(xml_reader->MoveToAttributeByName(attribute, nullptr))
		&& SUCCEEDED(xml_reader->GetValue(&buf, &len))) {
		ret = wstring(buf, len);
	}
	return ret;
}

wstring generate_key_from_manifest(const wstring& parent, const wstring& dir) {
	wstring ret;

	wchar_t manifest_name[MAX_PATH] = {};
	wcscpy_s(manifest_name, parent.c_str());
	PathAppendW(manifest_name, dir.c_str());
	wcscat_s(manifest_name, L".manifest");

	_com_ptr_t<_com_IIID<IStream, &__uuidof(IStream)>> stream;
	SHCreateStreamOnFileW(manifest_name, STGM_READ, &stream);
	if (stream) {
		IXmlReader* xml_reader = nullptr;
		CreateXmlReader(__uuidof(IXmlReader), (void**)&xml_reader, nullptr);
		if (xml_reader) {
			xml_reader->SetProperty(XmlReaderProperty_DtdProcessing, DtdProcessing_Prohibit);
			IXmlReaderInput* xml_reader_input = nullptr;
			CreateXmlReaderInputWithEncodingCodePage(stream, nullptr, CP_UTF8, TRUE, nullptr, &xml_reader_input);
			if (xml_reader_input) {
				xml_reader->SetInput(xml_reader_input);
				XmlNodeType node_type;
				while (SUCCEEDED(xml_reader->Read(&node_type))) {
					if (node_type == XmlNodeType_Element) {
						const wchar_t * elem_name = nullptr;
						xml_reader->GetQualifiedName(&elem_name, nullptr);
						if (wcscmp(elem_name, L"assemblyIdentity") == 0) {
							auto name = read_attribute_value(xml_reader, L"name");
							auto arch = read_attribute_value(xml_reader, L"processorArchitecture"); 
							auto lang = read_attribute_value(xml_reader, L"language");
							if ((!name.empty()) && (!arch.empty())) {
								ret = arch + L"_" + name;
								if ((!lang.empty()) && (lang != L"neutral")) {
									ret += L"_" + lang;
								}
							}
							break;
						}
					}
				}
				xml_reader_input->Release();
			}
			xml_reader->Release();
		}
	}

	return ret;
}

map<wstring, map<wstring, wstring>> find_files_wcs(const wstring & directory, const std::wstring& file_pattern)
{
	map<wstring, map<wstring, wstring>> ret;

	// ex. amd64_wvms_vsft.inf.resources_31bf3856ad364e35_10.0.14393.0_en-us_f32e284e2439eb0bs
	wregex re(L"(.*)_[0-9a-f]+_[0-9\\.]+_([0-9a-z-]+)_[0-9a-f]+");
	wsmatch match;

	wchar_t path[MAX_PATH] = {};
	wcscpy_s(path, directory.c_str());
	PathAppend(path, L"*");
	WIN32_FIND_DATAW fd;
	HANDLE find = ::FindFirstFileW(path, &fd);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY) {
				wstring file(fd.cFileName);
				if (regex_match(file, match, re)) {
					auto key = generate_key_from_manifest(directory, file);
					if (key.empty()) { // fallback
						key = match[1].str();
						wstring lang = match[2].str();
						if (lang != L"none") key += L"_" + lang;
					}

					PathRemoveFileSpec(path);
					PathCombine(path, path, fd.cFileName);

					PathAppend(path, file_pattern.c_str());
					auto& files = ret[key.c_str()];
					const auto& found_files = find_files(path);
					files.insert(found_files.begin(), found_files.end());
					PathRemoveFileSpec(path);
				}
			}
		} while (::FindNextFileW(find, &fd));
		::FindClose(find);
	}
	return ret;
}

std::map<std::wstring, std::map<std::wstring, std::wstring>> find_files_wcs_ex(const std::wstring & pattern, const std::wstring& default_file_pattern)
{
	wstring directory, file_pattern = default_file_pattern;
	if (PathIsDirectoryW(pattern.c_str())) {
		directory = pattern;
	} else {
		file_pattern = PathFindFileName(pattern.c_str());
		auto dir = pattern;
		PathRemoveFileSpec((LPWSTR)dir.data());
		directory = dir;
	}

	return find_files_wcs(directory, file_pattern);
}
