#include "stdafx.h"
#include "meta_reader.h"

using namespace std;

enum class meta_diff : int { same, removed, changed, child_changed, added };

bool print_diff(meta_diff diff) {
	switch (diff) {
	case meta_diff::same: /* printf_s("= "); return true;*/ return false;
	case meta_diff::removed: printf_s("     - "); return true;
	case meta_diff::changed: printf_s("     * "); return true;
	case meta_diff::child_changed: printf_s("     | "); return true;
	case meta_diff::added: printf_s("     + "); return true;
	default: printf_s("? "); return true;
	}
}

template<typename T> // T: meta_elem
struct meta_diff_elem {
	meta_diff diff;
	shared_ptr<T> elem;
	meta_diff_elem() : diff(meta_diff::same) {}
	meta_diff_elem(meta_diff _diff, const shared_ptr<T>& _elem) : diff(_diff), elem(_elem) {}
};

template<typename T>
bool find_changes(vector<meta_diff_elem<T>>& diffs, bool public_only, const vector<shared_ptr<T>>& old_list, const vector<shared_ptr<T>>& new_list,
	const function<int(const shared_ptr<T>&, const shared_ptr<T>&)>& filter = nullptr)
{
	bool any_changes = false;

	for (const auto& new_elem : new_list) {
		shared_ptr<T> old_elem;

		bool ignore = false;

		for (const auto& elem : old_list) {
			bool match = (elem->name == new_elem->name);

			if (match && (filter != nullptr)) {
				auto filter_res = filter(elem, new_elem);

				if (filter_res == 0) { // found match, proceed
					match = false; // ignore this old_elem
				} else if (filter_res < 0) {
					ignore = true; // ignore this new_elem;
					break;
				}
			}

			if (match) {
				old_elem = elem;
				break;
			}
		}

		if (ignore) continue;

		if (old_elem) {
			if (public_only && ((old_elem->visibility != meta_visibility::vis_public) && (new_elem->visibility != meta_visibility::vis_public)))
				continue; // both are not public, ignore. if ANY of them are public, should proceed. 

			if (old_elem->display_name != new_elem->display_name) {
				diffs.push_back(meta_diff_elem<T>(meta_diff::changed, new_elem));
				any_changes = true;
			}
		} else {
			if (public_only && (new_elem->visibility != meta_visibility::vis_public))
				continue; // not public, ignore.

			if (filter && (filter(old_elem, new_elem) < 0))
				continue; // ignore

			diffs.push_back(meta_diff_elem<T>(meta_diff::added, new_elem));
			any_changes = true;
		}
	}

	for (const auto& old_elem : old_list) {
		if (find_if(begin(new_list), end(new_list), [&old_elem](const shared_ptr<T>& elem) ->bool { return elem->name == old_elem->name; }) == end(new_list)) {
			if (public_only && (old_elem->visibility != meta_visibility::vis_public))
				continue; // not public, ignore.
			diffs.push_back(meta_diff_elem<T>(meta_diff::removed, old_elem));
			any_changes = true;
		}
	}
	
	return any_changes;
}

enum diff_options {
	diffNone		= 0x0,
	diffOld			= 0x1,
	diffNew			= 0x2,
	diffNonPublic	= 0x4,
	diffTypeInclude	= 0x8,
	diffHelp = 0x80000000,
};

const struct { const wchar_t* arg; const wchar_t* arg_alt; const wchar_t* params_desc; const wchar_t* description; const diff_options options; } cmd_options[] = {
	{ L"?",		L"help",			nullptr,		L"show this help",						diffHelp },
	{ L"n",		L"new",				L"<filename>",	L"specify new file(s)",					diffNew },
	{ L"o",		L"old",				L"<filename>",	L"specify old file(s)",					diffOld },
	{ L"np",	L"non-public",		nullptr,		L"show non-public members",				diffNonPublic },
	{ L"t+",	L"type-include",	L"<filter>",	L"show types when name match filter",	diffTypeInclude },
};

void print_usage() {
	printf_s("\tUsage: metadiff [options]\n\n");
	for (auto o = std::begin(cmd_options); o != std::end(cmd_options); ++o) {
		if (o->arg != nullptr) printf_s("\t-%S", o->arg); else printf_s("\t");

		int len = 0;
		if (o->arg_alt != nullptr){
			len = wcslen(o->arg_alt);
			printf_s("\t--%S", o->arg_alt);
		} else printf_s("\t");

		if (len < 6) printf_s("\t");

		if(o->params_desc != nullptr) len += printf_s(" %S", o->params_desc);

		if (len < 14) printf_s("\t");

		printf_s("\t: %S\n", o->description);
	}
}

struct meta_diff_type : meta_diff_elem<meta_type> {
	vector<meta_diff_elem<meta_field>>		fields;
	vector<meta_diff_elem<meta_property>>	properties;
	vector<meta_diff_elem<meta_method>>		methods;
	vector<meta_diff_elem<meta_event>>		events;
	meta_diff_type(meta_diff _diff, const shared_ptr<meta_type>& _elem) : meta_diff_elem<meta_type>(_diff, _elem) {}
};

int _tmain(int argc, _TCHAR* argv[])
{
	auto out = stdout;

	int options = diffNone;
	const wchar_t* err_arg = nullptr;
	wstring new_files_pattern, old_files_pattern, type_filter;

	printf_s("\n MetaDiff v0.2 https://github.com/WalkingCat/MetaDiff\n\n");

	for(int i = 1; i < argc; ++i) {
		const wchar_t* arg = argv[i];
		if ((arg[0] == '-') || ((arg[0] == '/'))) {
			diff_options curent_option = diffNone;
			if ((arg[0] == '-') && (arg[1] == '-')) {
				for (auto o = std::begin(cmd_options); o != std::end(cmd_options); ++o) {
					if ((o->arg_alt != nullptr) &&(_wcsicmp(arg + 2, o->arg_alt) == 0)) { curent_option = o->options; }
				}
			} else {
				for (auto o = std::begin(cmd_options); o != std::end(cmd_options); ++o) {
					if ((o->arg != nullptr) && (_wcsicmp(arg + 1, o->arg) == 0)) { curent_option = o->options; }
				}
			}

			bool valid = false;
			if (curent_option != diffNone) {
				valid = true;
				if (curent_option == diffNew) {
					if ((i + 1) < argc) new_files_pattern = argv[++i];
					else valid = false;
				} else if (curent_option == diffOld) {
					if ((i + 1) < argc) old_files_pattern = argv[++i];
					else valid = false;
				} else if (curent_option == diffTypeInclude) {
					if ((i + 1) < argc) type_filter = argv[++i];
					else valid = false;
				} else options = (options | curent_option);
			} 
			if (!valid && (err_arg == nullptr)) err_arg = arg;
		} else { if (new_files_pattern.empty()) new_files_pattern = arg; else err_arg = arg; }
	}

	if ((new_files_pattern.empty() && old_files_pattern.empty()) || (err_arg != nullptr) || (options & diffHelp)) {
		if (err_arg != nullptr) printf_s("\tError in option: %S\n\n", err_arg);
		print_usage();
		return 0;
	}

	auto new_files = find_files(new_files_pattern.c_str());
	auto old_files = find_files(old_files_pattern.c_str());

	printf_s(" new files: %S%S\n", new_files_pattern.c_str(), !new_files.empty() ? L"" : L" (NOT EXISTS!)");
	printf_s(" old files: %S%S\n", old_files_pattern.c_str(), !old_files.empty() ? L"" : L" (NOT EXISTS!)");

	if (!type_filter.empty()) printf_s(" type name filter: %S\n", type_filter.c_str());

	printf_s("\n");

	if (new_files.empty() & old_files.empty()) return 0; // at least one of them must exists

	printf_s(" diff legends: +: added, -: removed, *: changed, |: type member changed\n");
	printf_s("\n");

	auto get_meta_types = [](const wstring& file) -> unordered_map<wstring, shared_ptr<meta_type>> {
		unordered_map<wstring, shared_ptr<meta_type>> ret;
		meta_reader reader;
		if (reader.init(file.c_str())) {
			auto typedefs = reader.enum_types();
			for (const auto& t : typedefs) {
				auto type = meta_type_reader(reader, t).get_type();
				ret[type->name] = type;
			}
		} else {
			printf_s("Can't read metadata from file %S\n", file.c_str());
		}
		return ret;
	};

	bool printed_previous_file_name = false;
	diff_maps(new_files, old_files, [&](const wstring& file_name, const wstring* new_file, const wstring* old_file){
		bool printed_file_name = false;
		auto print_file_name = [&]() {
			if (!printed_file_name) {
				//print_group_name(new_files ? old_files ? L'*' : L'+' : L'-');
				if (printed_previous_file_name) {
					fwprintf_s(out, L"\n");
				}
				fwprintf_s(out, L"   %wc %ws\n", new_file ? old_file ? L'*' : L'+' : L'-', file_name.c_str());
				printed_previous_file_name = printed_file_name = true;
			}
		};

		if (old_file == nullptr) {
			print_file_name();
			return;
		}

		const auto new_types = new_file ? get_meta_types(*new_file) : unordered_map<wstring, shared_ptr<meta_type>>();
		const auto old_types = old_file ? get_meta_types(*old_file) : unordered_map<wstring, shared_ptr<meta_type>>();

		vector<meta_diff_type> diff_types;

		const bool public_only = (options & diff_options::diffNonPublic) == 0;

		for (const auto& new_type : new_types) {
			auto type_diff = meta_diff_type(meta_diff::same, new_type.second);

			auto old_type = old_types.find(new_type.first);
			if (old_type == old_types.end()) { // not found in old, so its new, and all members are new
				if (public_only && (new_type.second->visibility != meta_visibility::vis_public)) continue; // not public, ignore

				for (const auto& f : new_type.second->fields)		type_diff.fields.push_back(meta_diff_elem<meta_field>(meta_diff::added, f));
				for (const auto& p : new_type.second->properties)	type_diff.properties.push_back(meta_diff_elem<meta_property>(meta_diff::added, p));
				for (const auto& m : new_type.second->methods)		type_diff.methods.push_back(meta_diff_elem<meta_method>(meta_diff::added, m));
				for (const auto& e : new_type.second->events)		type_diff.events.push_back(meta_diff_elem<meta_event>(meta_diff::added, e));

				type_diff.diff = meta_diff::added;
				diff_types.push_back(type_diff);
			} else {
				if (public_only && ((new_type.second->visibility != meta_visibility::vis_public) && (old_type->second->visibility != meta_visibility::vis_public)))
					continue; // both are not public, ignore. if ANY of them are public, should proceed. 

				auto method_filter = [](const shared_ptr<meta_method>& old_method, const shared_ptr<meta_method>& new_method) -> int {
					if (new_method) {
						if (old_method) {
							bool match = (old_method->display_name == new_method->display_name); // methods can be overloaded, so compare the full signature

							if (match) {
								if ((old_method->semantics != meta_method::method_semantics::normal) && (new_method->semantics != meta_method::method_semantics::normal))
									return -1; // only care about normal methods, ignore others
								else
									return 1;
							}
						} else {
							if (new_method->semantics != meta_method::method_semantics::normal)
								return -1; // ignore
						}
					}
					return 0;  // not match, try next
				};

				bool member_changed = find_changes(type_diff.fields, public_only, old_type->second->fields, new_type.second->fields);
				member_changed = find_changes(type_diff.properties, public_only, old_type->second->properties, new_type.second->properties) || member_changed;
				member_changed = find_changes<meta_method>(type_diff.methods, public_only, old_type->second->methods, new_type.second->methods, method_filter) || member_changed;
				member_changed = find_changes(type_diff.events, public_only, old_type->second->events, new_type.second->events) || member_changed;

				if (member_changed) {
					type_diff.diff = meta_diff::child_changed;
					diff_types.push_back(type_diff);
				} else if (old_type->second->display_name != new_type.second->display_name) {
					type_diff.diff = meta_diff::changed;
					diff_types.push_back(type_diff);
				}
			}
		}

		for (const auto& old_type : old_types) {
			if (new_types.find(old_type.first) == new_types.end()) {
				if (public_only && (old_type.second->visibility != meta_visibility::vis_public))
					continue; // not public, ignore

				diff_types.push_back(meta_diff_type(meta_diff::removed, old_type.second));
			}
		}

		sort(begin(diff_types), end(diff_types), [](const meta_diff_type& left, const meta_diff_type& right) -> bool {
			const auto ns = left.elem->namespace_name.compare(right.elem->namespace_name);
			if (ns == 0) {
				return left.elem->name < right.elem->name;
			} else return ns < 0;
			});

		if (!diff_types.empty()) {
			print_file_name();

			for (const auto& type : diff_types) {
				if (!type_filter.empty() && (type.elem->namespace_name.find(type_filter) == wstring::npos)) continue;

				if (!print_diff(type.diff)) continue;

				printf_s("%S", type.elem->display_name.c_str());

				if ((type.diff == meta_diff::removed) || (type.diff == meta_diff::changed)) {
					printf_s(";\n\n");
					continue;
				}

				if (type.elem->semantics == meta_type::type_semantics::delegate_type) {
					printf_s(";\n");
				} else {
					printf_s(" {\n");
					for (const auto& f : type.fields) {
						if ((type.elem->semantics == meta_type::type_semantics::enum_type) && (f.elem->specials == meta_field::field_specials::enum_value)) {
							continue; // hide value__ for enums;
						}

						if (!print_diff(f.diff)) continue;

						printf_s("  %S", f.elem->display_name.c_str());

						if (type.elem->semantics == meta_type::type_semantics::enum_type) {
							printf_s(",\n");
						} else {
							printf_s(";\n");
						}
					}

					for (const auto& p : type.properties) {
						if (!print_diff(p.diff)) continue;
						printf_s("  %S\n", p.elem->display_name.c_str());
					}

					for (const auto& m : type.methods) {
						if (m.elem->semantics == meta_method::method_semantics::normal) {
							if (!print_diff(m.diff)) continue;
							printf_s("  %S;\n", m.elem->display_name.c_str());
						}
					}

					for (const auto& e : type.events) {
						if (!print_diff(e.diff)) continue;
						printf_s("  %S;\n", e.elem->display_name.c_str());
					}

					print_diff(type.diff);
					printf_s("}\n");
				}
			}
		}
	});

	fwprintf_s(out, L"\n");

	return 0;
}

vector<wstring> _find_files(const wchar_t * pattern)
{
	vector<wstring> ret;
	wchar_t path[MAX_PATH] = {};
	wcscpy_s(path, pattern);
	WIN32_FIND_DATA fd;
	HANDLE find = ::FindFirstFile(pattern, &fd);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
				PathRemoveFileSpec(path);
				PathCombine(path, path, fd.cFileName);
				ret.emplace_back(path);
			}
		} while (::FindNextFile(find, &fd));
		::FindClose(find);
	}
	return ret;
}
