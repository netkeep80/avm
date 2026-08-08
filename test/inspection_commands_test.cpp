#include "inspection_commands.h"

#include <cassert>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>

namespace
{

struct Fixture
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime{store};
	avm::BootstrapVocabulary vocabulary{runtime.vocabulary()};
	avm::LinkId point_a{store.create_point()};
	avm::LinkId point_b{store.create_point()};
	avm::LinkId point_c{store.create_point()};
	avm::LinkId missing_begin{store.create_point()};
	avm::LinkId missing_end{store.create_point()};
	avm::LinkId pair{store.intern(point_a, point_b)};
	avm::LinkId relation{store.create_point()};
	avm::LinkId relation_entity{avm::encode_relation_entity(store, {relation, point_a, point_c})};
	avm::LinkId formal{store.create_point()};
	avm::LinkId function{runtime.builder().create_function_handle()};
	avm::LinkId parameter{runtime.builder().parameter(formal)};
	avm::LinkId definition{runtime.builder().define_function(function, {formal}, parameter)};
	avm::LinkId true_literal{runtime.builder().literal(vocabulary.true_value)};
	avm::LinkId call{runtime.builder().call(function, {true_literal})};
	avm::LinkId boolean_root{runtime.builder().logical_not(runtime.builder().literal(vocabulary.false_value))};
};

std::string command(std::string name, avm::LinkId id)
{
	return std::move(name) + " " + std::to_string(id);
}

std::string command(std::string name, avm::LinkId first, avm::LinkId second)
{
	return std::move(name) + " " + std::to_string(first) + " " + std::to_string(second);
}

void expect_parse_error(std::string_view line)
{
	bool rejected = false;
	try
	{
		static_cast<void>(avm::tooling::parse_inspection_command(line));
	}
	catch (const avm::tooling::InspectionCommandError &)
	{
		rejected = true;
	}
	assert(rejected);
}

void verify_parser_produces_typed_commands()
{
	const auto link = avm::tooling::parse_inspection_command("  link\t42  ");
	assert(std::holds_alternative<avm::tooling::InspectLinkCommand>(link));
	assert(std::get<avm::tooling::InspectLinkCommand>(link).id == 42);

	const auto find = avm::tooling::parse_inspection_command("find 7 9");
	assert(std::holds_alternative<avm::tooling::FindPairCommand>(find));
	assert(std::get<avm::tooling::FindPairCommand>(find).begin == 7);
	assert(std::get<avm::tooling::FindPairCommand>(find).end == 9);

	const auto query = avm::tooling::parse_inspection_command("query 10 - 12");
	assert(std::holds_alternative<avm::tooling::QueryRelationsCommand>(query));
	const avm::RelationQuery parsed = std::get<avm::tooling::QueryRelationsCommand>(query).query;
	assert(parsed.relation == 10);
	assert(!parsed.subject);
	assert(parsed.object == 12);

	assert(std::holds_alternative<avm::tooling::TraceResetCommand>(
	    avm::tooling::parse_inspection_command("trace-reset")));
}

void verify_parse_errors_are_pre_execution_and_non_mutating()
{
	Fixture fixture;
	avm::tooling::InspectionSession session(fixture.store, fixture.vocabulary);
	const std::size_t size_before = fixture.store.size();

	expect_parse_error("");
	expect_parse_error("unknown 1");
	expect_parse_error("link");
	expect_parse_error("link 1 2");
	expect_parse_error("link -1");
	expect_parse_error("link 18446744073709551616");
	expect_parse_error("query - - -");
	expect_parse_error("query 1 2");

	assert(fixture.store.size() == size_before);
	assert(session.trace_events().empty());
}

void verify_read_only_commands_match_canonical_apis()
{
	Fixture fixture;
	avm::tooling::InspectionSession session(fixture.store, fixture.vocabulary);
	const std::size_t size_before = fixture.store.size();

	const std::string link_text = avm::tooling::run_inspection_command(session, command("link", fixture.point_a));
	assert(link_text == "link id=" + std::to_string(fixture.point_a) + " begin=" + std::to_string(fixture.point_a) +
	                        " end=" + std::to_string(fixture.point_a));

	const std::string find_text =
	    avm::tooling::run_inspection_command(session, command("find", fixture.point_a, fixture.point_b));
	assert(find_text.find("id=" + std::to_string(fixture.pair)) != std::string::npos);

	const std::string missing_text =
	    avm::tooling::run_inspection_command(session, command("find", fixture.missing_begin, fixture.missing_end));
	assert(missing_text.ends_with("id=-"));

	const auto outgoing_command = avm::tooling::parse_inspection_command(command("outgoing", fixture.point_a));
	const auto outgoing_result = avm::tooling::execute_inspection_command(session, outgoing_command);
	assert(std::get<avm::tooling::AdjacencyResult>(outgoing_result).ids == fixture.store.outgoing(fixture.point_a));

	const auto incoming_command = avm::tooling::parse_inspection_command(command("incoming", fixture.point_b));
	const auto incoming_result = avm::tooling::execute_inspection_command(session, incoming_command);
	assert(std::get<avm::tooling::AdjacencyResult>(incoming_result).ids == fixture.store.incoming(fixture.point_b));

	const auto relation_command = avm::tooling::parse_inspection_command(command("relation", fixture.relation_entity));
	const auto relation_result = avm::tooling::execute_inspection_command(session, relation_command);
	const avm::tooling::DecodeRelationResult decoded = std::get<avm::tooling::DecodeRelationResult>(relation_result);
	assert(decoded.entity.relation == fixture.relation);
	assert(decoded.entity.subject == fixture.point_a);
	assert(decoded.entity.object == fixture.point_c);

	const std::string query_line = "query " + std::to_string(fixture.relation) + " " +
	                               std::to_string(fixture.point_a) + " " + std::to_string(fixture.point_c);
	const auto query_result = avm::tooling::execute_inspection_command(
	    session, avm::tooling::parse_inspection_command(query_line));
	const auto &matches = std::get<avm::tooling::QueryRelationsResult>(query_result).matches;
	assert(matches.size() == 1);
	assert(matches.front().entity_id == fixture.relation_entity);

	const std::string function_text =
	    avm::tooling::run_inspection_command(session, command("function", fixture.function));
	assert(function_text.find("entity=" + std::to_string(fixture.definition)) != std::string::npos);

	assert(fixture.store.size() == size_before);
}

void verify_execute_and_trace_reuse_session_runtime()
{
	Fixture fixture;
	avm::tooling::InspectionSession session(fixture.store, fixture.vocabulary, 128);

	const std::string execute_text =
	    avm::tooling::run_inspection_command(session, command("execute", fixture.boolean_root));
	assert(execute_text == "execute result=" + std::to_string(fixture.vocabulary.true_value));

	const std::string trace_text = avm::tooling::run_inspection_command(session, command("trace", fixture.boolean_root));
	assert(trace_text.find("trace result=" + std::to_string(fixture.vocabulary.true_value)) == 0);
	assert(trace_text.find("enter entity=") != std::string::npos);
	assert(trace_text.find("return entity=") != std::string::npos);
	assert(trace_text.find("complete=true truncated=false") != std::string::npos);

	const std::string reset_text = avm::tooling::run_inspection_command(session, "trace-reset");
	assert(reset_text == "trace reset");
	assert(session.trace_events().empty());
}

void verify_frame_command_uses_structural_decoder()
{
	Fixture fixture;
	avm::tooling::InspectionSession session(fixture.store, fixture.vocabulary, 128);
	assert(session.trace_execute(fixture.call) == fixture.vocabulary.true_value);

	std::optional<avm::LinkId> frame;
	for (const avm::ExecutionEvent &event : session.trace_events())
	{
		if (event.context.frame)
		{
			frame = event.context.frame;
			break;
		}
	}
	assert(frame);

	const std::string frame_text = avm::tooling::run_inspection_command(session, command("frame", *frame));
	assert(frame_text.find("frame entity=" + std::to_string(*frame)) == 0);
	assert(frame_text.find("function=" + std::to_string(fixture.function)) != std::string::npos);
}

void verify_trace_truncation_and_failure_state()
{
	Fixture fixture;
	avm::tooling::InspectionSession short_session(fixture.store, fixture.vocabulary, 1);
	const std::string short_trace =
	    avm::tooling::run_inspection_command(short_session, command("trace", fixture.boolean_root));
	assert(short_trace.find("trace events=1 complete=false truncated=true") != std::string::npos);

	const avm::LinkId unknown_relation = fixture.store.create_point();
	const avm::LinkId failure_subject = fixture.store.create_point();
	const avm::LinkId failure_object = fixture.store.create_point();
	const avm::LinkId failure_root =
	    avm::encode_relation_entity(fixture.store, {unknown_relation, failure_subject, failure_object});
	avm::tooling::InspectionSession failure_session(fixture.store, fixture.vocabulary, 16);

	bool rejected = false;
	try
	{
		static_cast<void>(
		    avm::tooling::run_inspection_command(failure_session, command("trace", failure_root)));
	}
	catch (const std::runtime_error &)
	{
		rejected = true;
	}
	assert(rejected);

	const std::string retained = avm::tooling::render_current_trace(failure_session);
	assert(retained.find("fail entity=") != std::string::npos);
	assert(retained.find("phase=dispatch") != std::string::npos);
	assert(retained.find("complete=true truncated=false") != std::string::npos);
}

} // namespace

int main()
{
	verify_parser_produces_typed_commands();
	verify_parse_errors_are_pre_execution_and_non_mutating();
	verify_read_only_commands_match_canonical_apis();
	verify_execute_and_trace_reuse_session_runtime();
	verify_frame_command_uses_structural_decoder();
	verify_trace_truncation_and_failure_state();
	return 0;
}
