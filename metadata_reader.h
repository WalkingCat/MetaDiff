#pragma once
#include <comip.h>
#include <functional>

class metadata_reader
{
protected:
	CComPtr<IMetaDataImport2> metadata_import;
public:
	metadata_reader();
	metadata_reader(const metadata_reader& that) { metadata_import = that.metadata_import; }
	bool init_alt(const wchar_t* _filename);
	bool init(const wchar_t* _filename);
	std::wstring get_name(mdToken token);
	virtual std::wstring get_full_name(mdToken token);
	std::vector<mdToken> enum_tokens(std::function<HRESULT(HCORENUM*, mdToken*, ULONG, ULONG*)>) const;
	std::vector<mdTypeDef> enum_types() const;
	std::vector<mdFieldDef> enum_fields(mdTypeDef type);
	std::vector<mdProperty> enum_properties(mdTypeDef type);
	std::vector<mdMethodDef> enum_methods(mdTypeDef type);
	std::vector<mdEvent> enum_events(mdTypeDef type);
	std::vector<mdParamDef> enum_params(mdMethodDef method);
	std::vector<mdGenericParam> enum_generic_params(mdToken type_or_method);
};

enum class meta_visibility : int {
	// this is (roughly) ordered. some of them are not comparable (protected/internal), but here doesn't matter
	vis_unknown, vis_private, vis_protected, vis_internal, vis_protected_internal, vis_public
};

struct meta_elem {
	mdToken token;
	DWORD attributes;
	std::wstring name;
	std::wstring display_name;
	meta_visibility visibility;
	meta_elem() : token(mdTokenNil), attributes(0), visibility(meta_visibility::vis_unknown) {}
};

struct meta_field : public meta_elem {
	enum class field_specials : int {
		none = 0x0,
		enum_value = 0x1,
	} specials;
	void set_special(field_specials special) { specials = static_cast<field_specials>(static_cast<int>(specials) | static_cast<int>(special)); }
	meta_field() : specials(field_specials::none) {}
};

struct meta_method : public meta_elem {
	enum class method_semantics : int {
		normal = 0x0,
		property_get = 0x1,
		property_set = 0x2,
		property_other = 0x4,
		event_add = 0x8,
		event_remove = 0x10,
		event_fire = 0x20,
		event_other = 0x40,
	} semantics;
	void set_semantic(method_semantics semantic) { semantics = static_cast<method_semantics>(static_cast<int>(semantics) | static_cast<int>(semantic)); }
	enum class method_specials : int {
		none = 0x0,
		constructor = 0x1,
		class_constructor = 0x2,
		delegate_invoke = 0x4,
	} specials;
	void set_special(method_specials special) { specials = static_cast<method_specials>(static_cast<int>(specials) | static_cast<int>(special)); }
	meta_method() : semantics(method_semantics::normal), specials(method_specials::none) {}
};

struct meta_property : public meta_elem {};

struct meta_event : public meta_elem {};

struct meta_type : public meta_elem {
	std::wstring namespace_name;

	enum class type_semantics : int {
		class_type = 0,
		interface_type = 1,
		value_type = 2,
		enum_type = 4,
		delegate_type = 8,
	} semantics;
	void set_semantic(type_semantics semantic) { semantics = static_cast<type_semantics>(static_cast<int>(semantics) | static_cast<int>(semantic)); }
	meta_type() : semantics(type_semantics::class_type) {}

	std::vector<std::shared_ptr<meta_field>> fields;
	std::vector<std::shared_ptr<meta_property>> properties;
	std::vector<std::shared_ptr<meta_method>> methods;
	std::vector<std::shared_ptr<meta_event>> events;
};

class meta_type_reader : metadata_reader {
	std::shared_ptr<meta_type> result;
	std::vector<std::wstring> type_generic_param_names;
	std::shared_ptr<meta_field> get_field(mdFieldDef token);
	std::shared_ptr<meta_property> get_property(mdProperty token);
	std::shared_ptr<meta_method> get_method(mdMethodDef token);
	std::shared_ptr<meta_event> get_event(mdEvent token);
	static std::wstring format_visibility(meta_visibility visibility);
	virtual std::wstring get_full_name(mdToken token) override;
public:
	meta_type_reader(const metadata_reader& reader, mdTypeDef token) : metadata_reader(reader), result(std::make_shared<meta_type>()) {  result->token = token; }
	std::shared_ptr<meta_type> get_type();
};

