#include "avm/legacy_json_compat.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

using namespace std;

namespace
{

void get_json(json &value, const string &path)
{
	ifstream input(path.c_str());
	if (!input.good())
		throw runtime_error("Can't load json from the " + path + " file!");
	input >> value;
}

void add_json(const json &value, const string &path)
{
	ofstream output(path.c_str());
	if (!output.good())
		throw runtime_error("Can't open " + path + " file!");
	output << value;
}

bool is_compatibility_operator(string_view name)
{
	return name == "Not" || name == "And" || name == "Or" || name == "If" || name == "Def" || name == "Call";
}

bool is_expression_candidate(const json &root)
{
	if (root.is_object() && root.size() == 1)
		return is_compatibility_operator(root.begin().key());

	if (!root.is_array() || root.empty() || !root[0].is_object() || root[0].size() != 1)
		return false;

	return is_compatibility_operator(root[0].begin().key());
}

void print_usage()
{
	cout << R"(https://github.com/netkeep80/avm
     Associative Virtual Machine [Version 0.0.5]

Usage:
       avm [entry_point]
)";
}

} // namespace

int main(int argc, char *argv[])
{
	if (argc != 2)
	{
		print_usage();
		return 0;
	}

	json root;
	try
	{
		get_json(root, argv[1]);

		rel_t *root_entity = nullptr;
		if (is_expression_candidate(root))
		{
			clear_func_env();
			root_entity = interpret(root);
		}
		else
		{
			root_entity = import_json(root);
		}

		json result;
		export_json(root_entity, result);
		add_json(result, "res.json");
		cout << "rel_t::created() = " << rel_t::created() << endl;
		return 0;
	}
	catch (const json::exception &error)
	{
		cerr << "json::exception: " << error.what() << ", id: " << error.id;
	}
	catch (const exception &error)
	{
		cerr << "std::exception: " << error.what();
	}
	catch (...)
	{
		cerr << "unknown exception";
	}

	add_json(root, "rvm.dump.json");
	return 1;
}
