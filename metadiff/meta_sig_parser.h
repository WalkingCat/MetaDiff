#pragma once
#include <string>
#include <vector>

class meta_sig_parser
{
private:
	PCCOR_SIGNATURE m_signature;
	const ULONG		m_sig_size;
	ULONG			m_sig_pos;
	IMetaDataImport*m_import;
	ULONG uncompress_data();
	int uncompress_int();
	mdToken uncompress_token();
	const std::vector<std::wstring> m_var_names;
	const std::vector<std::wstring> m_mvar_names;
public:
	meta_sig_parser(IMetaDataImport* import, PCCOR_SIGNATURE signature, ULONG sig_size);
	meta_sig_parser(IMetaDataImport* import, PCCOR_SIGNATURE signature, ULONG sig_size, const std::vector<std::wstring>& var_names);
	std::wstring parse_element_type();
	std::wstring parse_field(wchar_t* name);
	std::wstring parse_property(wchar_t* name);
	std::wstring parse_method(const wchar_t* name, const std::vector<std::wstring>& param_names, bool omit_return_type = false);
};

