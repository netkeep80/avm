#include "json_duplet_converter.h"
#include "nlohmann/json.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{

using Json = nlohmann::ordered_json;

struct Options
{
	std::string from;
	std::string to;
	std::string input = "-";
	std::string output = "-";
	bool check_only = false;
};

[[noreturn]] void usage_error(const std::string &message)
{
	throw std::invalid_argument(
	    message + "\nusage: avm-json-convert --from=jsonrvm-triplet --to=avm-duplet [input|-] "
	              "[-o output|-] [--check]");
}

std::string option_value(const std::string &argument, const std::string &name)
{
	const std::string prefix = name + "=";
	if (argument.rfind(prefix, 0) != 0)
		return {};
	return argument.substr(prefix.size());
}

Options parse_options(int argc, char **argv)
{
	Options options;
	bool input_seen = false;

	for (int index = 1; index < argc; ++index)
	{
		const std::string argument = argv[index];
		if (argument == "--check")
		{
			options.check_only = true;
			continue;
		}

		if (const std::string value = option_value(argument, "--from"); !value.empty())
		{
			options.from = value;
			continue;
		}
		if (argument == "--from")
		{
			if (++index >= argc)
				usage_error("--from requires a value");
			options.from = argv[index];
			continue;
		}

		if (const std::string value = option_value(argument, "--to"); !value.empty())
		{
			options.to = value;
			continue;
		}
		if (argument == "--to")
		{
			if (++index >= argc)
				usage_error("--to requires a value");
			options.to = argv[index];
			continue;
		}

		if (argument == "-o" || argument == "--output")
		{
			if (++index >= argc)
				usage_error(argument + " requires a value");
			options.output = argv[index];
			continue;
		}
		if (const std::string value = option_value(argument, "--output"); !value.empty())
		{
			options.output = value;
			continue;
		}

		if (!argument.empty() && argument[0] == '-' && argument != "-")
			usage_error("unknown option: " + argument);
		if (input_seen)
			usage_error("only one input path is allowed");
		options.input = argument;
		input_seen = true;
	}

	if (options.from.empty())
		usage_error("--from is required");
	if (options.to.empty())
		usage_error("--to is required");

	const bool forward = options.from == "jsonrvm-triplet" && options.to == "avm-duplet";
	const bool reverse = options.from == "avm-duplet" && options.to == "jsonrvm-explicit-triplet";
	if (!forward && !reverse)
		usage_error("unsupported conversion direction");

	return options;
}

Json read_json(const std::string &path)
{
	if (path == "-")
	{
		Json value;
		std::cin >> value;
		return value;
	}

	std::ifstream input(path);
	if (!input)
		throw std::runtime_error("cannot open input file: " + path);

	Json value;
	input >> value;
	return value;
}

void write_json(const std::string &path, const Json &value)
{
	if (path == "-")
	{
		std::cout << value.dump(2) << '\n';
		return;
	}

	std::ofstream output(path, std::ios::trunc);
	if (!output)
		throw std::runtime_error("cannot open output file: " + path);
	output << value.dump(2) << '\n';
	if (!output)
		throw std::runtime_error("failed to write output file: " + path);
}

} // namespace

int main(int argc, char **argv)
{
	try
	{
		const Options options = parse_options(argc, argv);
		const Json input = read_json(options.input);

		Json output;
		if (options.from == "jsonrvm-triplet")
			output = avm::json_duplet::convert_explicit_triplets_to_duplets(input);
		else
			output = avm::json_duplet::convert_relation_duplets_to_explicit_triplets(input);

		if (!options.check_only)
			write_json(options.output, output);
		return 0;
	}
	catch (const std::exception &error)
	{
		std::cerr << "avm-json-convert: " << error.what() << '\n';
		return 1;
	}
}
