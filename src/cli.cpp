#include "avm/json_compat.h"
#include "avm/json_value_codec.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace
{

using Json = nlohmann::json;

void get_json(Json &value, const std::string &path)
{
	std::ifstream input(path);
	if (!input.good())
		throw std::runtime_error("Can't load json from the " + path + " file!");
	input >> value;
}

void add_json(const Json &value, const std::string &path)
{
	std::ofstream output(path);
	if (!output.good())
		throw std::runtime_error("Can't open " + path + " file!");
	output << value;
}

bool is_compatibility_operator(std::string_view name)
{
	return name == "Not" || name == "And" || name == "Or" || name == "If" || name == "Def" || name == "Call";
}

bool is_expression_candidate(const Json &root)
{
	if (root.is_object() && root.size() == 1)
		return is_compatibility_operator(root.begin().key());

	if (!root.is_array() || root.empty() || !root[0].is_object() || root[0].size() != 1)
		return false;

	return is_compatibility_operator(root[0].begin().key());
}

Json roundtrip_value(const Json &root)
{
	avm::InMemoryLinkStore store;
	avm::JsonValueCodec codec(store);
	return codec.decode(codec.encode(root));
}

void print_usage()
{
	std::cout << R"(https://github.com/netkeep80/avm
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

	Json root;
	try
	{
		get_json(root, argv[1]);

		Json result;
		if (is_expression_candidate(root))
		{
			avm::JsonCompatibilitySession session;
			result = session.interpret(root);
		}
		else
		{
			result = roundtrip_value(root);
		}

		add_json(result, "res.json");
		return 0;
	}
	catch (const Json::exception &error)
	{
		std::cerr << "json::exception: " << error.what() << ", id: " << error.id;
	}
	catch (const std::exception &error)
	{
		std::cerr << "std::exception: " << error.what();
	}
	catch (...)
	{
		std::cerr << "unknown exception";
	}

	add_json(root, "rvm.dump.json");
	return 1;
}
