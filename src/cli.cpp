#include "avm/execution_trace.h"
#include "avm/json_compat.h"
#include "avm/json_value_codec.h"
#include "avm/version.h"

#include <charconv>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

namespace
{

using Json = nlohmann::json;

constexpr std::size_t default_trace_limit = 256;

struct CliOptions
{
	std::string path;
	bool trace = false;
	std::size_t trace_limit = default_trace_limit;
};

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

std::size_t parse_trace_limit(std::string_view text)
{
	std::size_t value = 0;
	const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
	if (error != std::errc{} || end != text.data() + text.size())
		throw std::invalid_argument("trace limit must be a non-negative integer");
	return value;
}

std::optional<CliOptions> parse_options(int argc, char *argv[])
{
	if (argc == 2)
		return CliOptions{argv[1], false, default_trace_limit};

	if (argc == 3 && std::string_view(argv[1]) == "--trace")
		return CliOptions{argv[2], true, default_trace_limit};

	if (argc == 4 && std::string_view(argv[1]) == "--trace-limit")
		return CliOptions{argv[3], true, parse_trace_limit(argv[2])};

	return std::nullopt;
}

const char *event_kind_name(avm::ExecutionEventKind kind)
{
	switch (kind)
	{
	case avm::ExecutionEventKind::Enter:
		return "enter";
	case avm::ExecutionEventKind::Return:
		return "return";
	case avm::ExecutionEventKind::Fail:
		return "fail";
	}
	return "unknown";
}

const char *failure_phase_name(std::optional<avm::ExecutionFailurePhase> phase)
{
	if (!phase)
		return "-";

	switch (*phase)
	{
	case avm::ExecutionFailurePhase::Dispatch:
		return "dispatch";
	case avm::ExecutionFailurePhase::Handler:
		return "handler";
	case avm::ExecutionFailurePhase::ResultValidation:
		return "result-validation";
	}
	return "unknown";
}

void print_optional_link(std::optional<avm::LinkId> id)
{
	if (id)
		std::cout << *id;
	else
		std::cout << '-';
}

void print_trace(const avm::BoundedExecutionTrace &trace)
{
	for (const avm::ExecutionEvent &event : trace.events())
	{
		std::cout << event_kind_name(event.kind) << " entity=" << event.context.entity
		          << " relation=" << event.context.relation << " subject=" << event.context.subject
		          << " object=" << event.context.object << " parent=";
		print_optional_link(event.context.parent);
		std::cout << " frame=";
		print_optional_link(event.context.frame);
		std::cout << " result=";
		print_optional_link(event.result);
		std::cout << " phase=" << failure_phase_name(event.failure_phase) << '\n';
	}

	std::cout << "trace events=" << trace.size() << " complete=" << (trace.complete() ? "true" : "false")
	          << " truncated=" << (trace.truncated() ? "true" : "false") << '\n';
}

Json execute_traced(const Json &root, std::size_t trace_limit)
{
	if (!is_expression_candidate(root))
		throw std::invalid_argument("--trace requires an executable JSON compatibility expression");

	avm::JsonCompatibilitySession session;
	const avm::LinkId program = session.import_program(root);
	avm::BoundedExecutionTrace trace(trace_limit);
	session.runtime().executor().set_observer(&trace);

	avm::LinkId result = avm::invalid_link_id;
	try
	{
		result = session.execute(program);
	}
	catch (...)
	{
		session.runtime().executor().set_observer(nullptr);
		print_trace(trace);
		throw;
	}

	session.runtime().executor().set_observer(nullptr);
	print_trace(trace);
	return session.project_result(result);
}

void print_usage()
{
	std::cout << "https://github.com/netkeep80/avm\n"
	          << "     Associative Virtual Machine [Version " << avm::version_string << "]\n\n"
	          << "Usage:\n"
	          << "       avm <entry_point>\n"
	          << "       avm --trace <entry_point>\n"
	          << "       avm --trace-limit <events> <entry_point>\n";
}

} // namespace

int main(int argc, char *argv[])
{
	Json root;
	try
	{
		const std::optional<CliOptions> options = parse_options(argc, argv);
		if (!options)
		{
			print_usage();
			return 0;
		}

		get_json(root, options->path);

		Json result;
		if (options->trace)
		{
			result = execute_traced(root, options->trace_limit);
		}
		else if (is_expression_candidate(root))
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
