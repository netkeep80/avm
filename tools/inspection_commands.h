#pragma once

#include "inspection_session.h"

#include <charconv>
#include <cstddef>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace avm::tooling
{

class InspectionCommandError : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

struct InspectLinkCommand
{
	LinkId id;
};

struct FindPairCommand
{
	LinkId begin;
	LinkId end;
};

struct OutgoingCommand
{
	LinkId begin;
};

struct IncomingCommand
{
	LinkId end;
};

struct DecodeRelationCommand
{
	LinkId entity;
};

struct QueryRelationsCommand
{
	RelationQuery query;
};

struct FunctionDefinitionCommand
{
	LinkId handle;
};

struct CallFrameCommand
{
	LinkId frame;
};

struct ExecuteCommand
{
	LinkId root;
};

struct TraceExecuteCommand
{
	LinkId root;
};

struct TraceResetCommand
{
};

using InspectionCommand =
    std::variant<InspectLinkCommand, FindPairCommand, OutgoingCommand, IncomingCommand, DecodeRelationCommand,
                 QueryRelationsCommand, FunctionDefinitionCommand, CallFrameCommand, ExecuteCommand, TraceExecuteCommand,
                 TraceResetCommand>;

struct InspectLinkResult
{
	LinkId id;
	Link link;
};

struct FindPairResult
{
	LinkId begin;
	LinkId end;
	std::optional<LinkId> id;
};

struct AdjacencyResult
{
	std::string_view direction;
	LinkId endpoint;
	std::vector<LinkId> ids;
};

struct DecodeRelationResult
{
	LinkId entity_id;
	RelationEntity entity;
};

struct QueryRelationsResult
{
	std::vector<RelationMatch> matches;
};

struct FunctionDefinitionResult
{
	LinkId handle;
	std::optional<FunctionDefinition> definition;
};

struct CallFrameResult
{
	DecodedCallFrame frame;
};

struct ExecuteResult
{
	LinkId result;
};

struct TraceExecuteResult
{
	LinkId result;
	std::vector<ExecutionEvent> events;
	bool truncated;
};

struct TraceResetResult
{
};

using InspectionResult =
    std::variant<InspectLinkResult, FindPairResult, AdjacencyResult, DecodeRelationResult, QueryRelationsResult,
                 FunctionDefinitionResult, CallFrameResult, ExecuteResult, TraceExecuteResult, TraceResetResult>;

namespace detail
{

inline bool is_space(char value) noexcept
{
	return value == ' ' || value == '\t' || value == '\r' || value == '\n';
}

inline std::vector<std::string_view> tokenize(std::string_view line)
{
	std::vector<std::string_view> tokens;
	std::size_t cursor = 0;
	while (cursor < line.size())
	{
		while (cursor < line.size() && is_space(line[cursor]))
			++cursor;
		if (cursor == line.size())
			break;

		const std::size_t begin = cursor;
		while (cursor < line.size() && !is_space(line[cursor]))
			++cursor;
		tokens.push_back(line.substr(begin, cursor - begin));
	}
	return tokens;
}

inline LinkId parse_link_id(std::string_view token)
{
	LinkId value = invalid_link_id;
	const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), value);
	if (token.empty() || error != std::errc{} || end != token.data() + token.size())
		throw InspectionCommandError("LinkId must be an unsigned decimal integer");
	return value;
}

inline std::optional<LinkId> parse_constraint(std::string_view token)
{
	if (token == "-")
		return std::nullopt;
	return parse_link_id(token);
}

inline void require_arity(const std::vector<std::string_view> &tokens, std::size_t expected,
                          std::string_view command)
{
	if (tokens.size() != expected)
		throw InspectionCommandError(std::string(command) + " has invalid argument count");
}

inline const char *event_kind_name(ExecutionEventKind kind) noexcept
{
	switch (kind)
	{
	case ExecutionEventKind::Enter:
		return "enter";
	case ExecutionEventKind::Return:
		return "return";
	case ExecutionEventKind::Fail:
		return "fail";
	}
	return "unknown";
}

inline const char *failure_phase_name(std::optional<ExecutionFailurePhase> phase) noexcept
{
	if (!phase)
		return "-";

	switch (*phase)
	{
	case ExecutionFailurePhase::Dispatch:
		return "dispatch";
	case ExecutionFailurePhase::Handler:
		return "handler";
	case ExecutionFailurePhase::ResultValidation:
		return "result-validation";
	}
	return "unknown";
}

inline void append_optional_link(std::ostringstream &output, std::optional<LinkId> id)
{
	if (id)
		output << *id;
	else
		output << '-';
}

inline void append_ids(std::ostringstream &output, const std::vector<LinkId> &ids)
{
	output << '[';
	for (std::size_t index = 0; index < ids.size(); ++index)
	{
		if (index != 0)
			output << ',';
		output << ids[index];
	}
	output << ']';
}

inline void append_event(std::ostringstream &output, const ExecutionEvent &event)
{
	output << event_kind_name(event.kind) << " entity=" << event.context.entity << " relation=" << event.context.relation
	       << " subject=" << event.context.subject << " object=" << event.context.object << " parent=";
	append_optional_link(output, event.context.parent);
	output << " frame=";
	append_optional_link(output, event.context.frame);
	output << " result=";
	append_optional_link(output, event.result);
	output << " phase=" << failure_phase_name(event.failure_phase);
}

inline void append_trace(std::ostringstream &output, const std::vector<ExecutionEvent> &events, bool truncated)
{
	for (const ExecutionEvent &event : events)
	{
		append_event(output, event);
		output << '\n';
	}
	output << "trace events=" << events.size() << " complete=" << (truncated ? "false" : "true")
	       << " truncated=" << (truncated ? "true" : "false");
}

} // namespace detail

inline InspectionCommand parse_inspection_command(std::string_view line)
{
	const std::vector<std::string_view> tokens = detail::tokenize(line);
	if (tokens.empty())
		throw InspectionCommandError("inspection command is empty");

	const std::string_view name = tokens.front();
	if (name == "link")
	{
		detail::require_arity(tokens, 2, name);
		return InspectLinkCommand{detail::parse_link_id(tokens[1])};
	}
	if (name == "find")
	{
		detail::require_arity(tokens, 3, name);
		return FindPairCommand{detail::parse_link_id(tokens[1]), detail::parse_link_id(tokens[2])};
	}
	if (name == "outgoing")
	{
		detail::require_arity(tokens, 2, name);
		return OutgoingCommand{detail::parse_link_id(tokens[1])};
	}
	if (name == "incoming")
	{
		detail::require_arity(tokens, 2, name);
		return IncomingCommand{detail::parse_link_id(tokens[1])};
	}
	if (name == "relation")
	{
		detail::require_arity(tokens, 2, name);
		return DecodeRelationCommand{detail::parse_link_id(tokens[1])};
	}
	if (name == "query")
	{
		detail::require_arity(tokens, 4, name);
		RelationQuery query{
		    .relation = detail::parse_constraint(tokens[1]),
		    .subject = detail::parse_constraint(tokens[2]),
		    .object = detail::parse_constraint(tokens[3]),
		};
		if (!query.relation && !query.subject && !query.object)
			throw InspectionCommandError("query requires at least one relation/subject/object constraint");
		return QueryRelationsCommand{query};
	}
	if (name == "function")
	{
		detail::require_arity(tokens, 2, name);
		return FunctionDefinitionCommand{detail::parse_link_id(tokens[1])};
	}
	if (name == "frame")
	{
		detail::require_arity(tokens, 2, name);
		return CallFrameCommand{detail::parse_link_id(tokens[1])};
	}
	if (name == "execute")
	{
		detail::require_arity(tokens, 2, name);
		return ExecuteCommand{detail::parse_link_id(tokens[1])};
	}
	if (name == "trace")
	{
		detail::require_arity(tokens, 2, name);
		return TraceExecuteCommand{detail::parse_link_id(tokens[1])};
	}
	if (name == "trace-reset")
	{
		detail::require_arity(tokens, 1, name);
		return TraceResetCommand{};
	}

	throw InspectionCommandError("unknown inspection command: " + std::string(name));
}

inline InspectionResult execute_inspection_command(InspectionSession &session, const InspectionCommand &command)
{
	return std::visit(
	    [&session](const auto &typed) -> InspectionResult
	    {
		    using Command = std::decay_t<decltype(typed)>;
		    if constexpr (std::is_same_v<Command, InspectLinkCommand>)
			    return InspectLinkResult{typed.id, session.inspect_link(typed.id)};
		    else if constexpr (std::is_same_v<Command, FindPairCommand>)
			    return FindPairResult{typed.begin, typed.end, session.find_pair(typed.begin, typed.end)};
		    else if constexpr (std::is_same_v<Command, OutgoingCommand>)
			    return AdjacencyResult{"outgoing", typed.begin, session.outgoing(typed.begin)};
		    else if constexpr (std::is_same_v<Command, IncomingCommand>)
			    return AdjacencyResult{"incoming", typed.end, session.incoming(typed.end)};
		    else if constexpr (std::is_same_v<Command, DecodeRelationCommand>)
			    return DecodeRelationResult{typed.entity, session.decode_relation(typed.entity)};
		    else if constexpr (std::is_same_v<Command, QueryRelationsCommand>)
			    return QueryRelationsResult{session.query_relations(typed.query)};
		    else if constexpr (std::is_same_v<Command, FunctionDefinitionCommand>)
			    return FunctionDefinitionResult{typed.handle, session.function_definition(typed.handle)};
		    else if constexpr (std::is_same_v<Command, CallFrameCommand>)
			    return CallFrameResult{session.call_frame(typed.frame)};
		    else if constexpr (std::is_same_v<Command, ExecuteCommand>)
			    return ExecuteResult{session.execute(typed.root)};
		    else if constexpr (std::is_same_v<Command, TraceExecuteCommand>)
		    {
			    const LinkId result = session.trace_execute(typed.root);
			    return TraceExecuteResult{
			        result,
			        std::vector<ExecutionEvent>(session.trace_events().begin(), session.trace_events().end()),
			        session.trace_truncated(),
			    };
		    }
		    else
		    {
			    session.reset_trace();
			    return TraceResetResult{};
		    }
	    },
	    command);
}

inline std::string render_inspection_result(const InspectionResult &result)
{
	return std::visit(
	    [](const auto &typed)
	    {
		    using Result = std::decay_t<decltype(typed)>;
		    std::ostringstream output;
		    if constexpr (std::is_same_v<Result, InspectLinkResult>)
			    output << "link id=" << typed.id << " begin=" << typed.link.begin << " end=" << typed.link.end;
		    else if constexpr (std::is_same_v<Result, FindPairResult>)
		    {
			    output << "find begin=" << typed.begin << " end=" << typed.end << " id=";
			    detail::append_optional_link(output, typed.id);
		    }
		    else if constexpr (std::is_same_v<Result, AdjacencyResult>)
		    {
			    output << typed.direction << " endpoint=" << typed.endpoint << " ids=";
			    detail::append_ids(output, typed.ids);
		    }
		    else if constexpr (std::is_same_v<Result, DecodeRelationResult>)
			    output << "relation entity=" << typed.entity_id << " relation=" << typed.entity.relation
			           << " subject=" << typed.entity.subject << " object=" << typed.entity.object;
		    else if constexpr (std::is_same_v<Result, QueryRelationsResult>)
		    {
			    output << "query matches=" << typed.matches.size();
			    for (const RelationMatch &match : typed.matches)
			        output << '\n'
			               << "entity=" << match.entity_id << " relation=" << match.entity.relation
			               << " subject=" << match.entity.subject << " object=" << match.entity.object;
		    }
		    else if constexpr (std::is_same_v<Result, FunctionDefinitionResult>)
		    {
			    output << "function handle=" << typed.handle;
			    if (!typed.definition)
				    output << " definition=-";
			    else
			    {
				    output << " entity=" << typed.definition->entity << " parameters=";
				    detail::append_ids(output, typed.definition->parameters);
				    output << " body=" << typed.definition->body;
			    }
		    }
		    else if constexpr (std::is_same_v<Result, CallFrameResult>)
		    {
			    output << "frame entity=" << typed.frame.entity << " parent=" << typed.frame.parent
			           << " function=" << typed.frame.function << " bindings=";
			    detail::append_ids(output, typed.frame.bindings);
		    }
		    else if constexpr (std::is_same_v<Result, ExecuteResult>)
			    output << "execute result=" << typed.result;
		    else if constexpr (std::is_same_v<Result, TraceExecuteResult>)
		    {
			    output << "trace result=" << typed.result << '\n';
			    detail::append_trace(output, typed.events, typed.truncated);
		    }
		    else
			    output << "trace reset";
		    return output.str();
	    },
	    result);
}

inline std::string render_current_trace(const InspectionSession &session)
{
	std::ostringstream output;
	const std::vector<ExecutionEvent> events(session.trace_events().begin(), session.trace_events().end());
	detail::append_trace(output, events, session.trace_truncated());
	return output.str();
}

inline std::string run_inspection_command(InspectionSession &session, std::string_view line)
{
	return render_inspection_result(execute_inspection_command(session, parse_inspection_command(line)));
}

} // namespace avm::tooling
