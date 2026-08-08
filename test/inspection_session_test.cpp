#include "inspection_session.h"

#include "avm/relations_model.h"

#include <cassert>
#include <optional>
#include <stdexcept>
#include <vector>

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
	avm::LinkId pair{store.intern(point_a, point_b)};
	avm::LinkId relation{store.create_point()};
	avm::LinkId relation_entity{avm::encode_relation_entity(store, {relation, point_a, point_c})};
	avm::LinkId formal{store.create_point()};
	avm::LinkId function{runtime.builder().create_function_handle()};
	avm::LinkId body{runtime.builder().parameter(formal)};
	avm::LinkId definition{runtime.builder().define_function(function, {formal}, body)};
	avm::LinkId literal_true{runtime.builder().literal(vocabulary.true_value)};
	avm::LinkId call{runtime.builder().call(function, {literal_true})};
	avm::LinkId boolean_root{runtime.builder().logical_not(runtime.builder().literal(vocabulary.false_value))};
};

void verify_session_construction_is_idempotent()
{
	Fixture fixture;
	const std::size_t size_before = fixture.store.size();
	avm::tooling::InspectionSession session(fixture.store, fixture.vocabulary, 128);
	assert(fixture.store.size() == size_before);
	assert(session.vocabulary().true_value == fixture.vocabulary.true_value);
}

void verify_read_only_inspection_never_materializes()
{
	Fixture fixture;
	avm::tooling::InspectionSession session(fixture.store, fixture.vocabulary, 128);
	const std::size_t size_before = fixture.store.size();

	const avm::Link expected_point{fixture.point_a, fixture.point_a};
	const avm::Link expected_pair{fixture.point_a, fixture.point_b};
	assert(session.inspect_link(fixture.point_a) == expected_point);
	assert(session.inspect_link(fixture.pair) == expected_pair);
	assert(session.find_pair(fixture.point_a, fixture.point_b) == fixture.pair);

	const avm::LinkId missing_begin = fixture.store.create_point();
	const avm::LinkId missing_end = fixture.store.create_point();
	const std::size_t size_after_endpoints = fixture.store.size();
	assert(!session.find_pair(missing_begin, missing_end));
	assert(fixture.store.size() == size_after_endpoints);

	assert(session.outgoing(fixture.point_a) == fixture.store.outgoing(fixture.point_a));
	assert(session.incoming(fixture.point_b) == fixture.store.incoming(fixture.point_b));

	const avm::RelationQuery query{
	    .relation = fixture.relation,
	    .subject = fixture.point_a,
	    .object = fixture.point_c,
	};
	assert(session.query_relations(query) == avm::query_relation_entities(fixture.store, query));
	const avm::RelationEntity expected_entity{fixture.relation, fixture.point_a, fixture.point_c};
	assert(session.decode_relation(fixture.relation_entity) == expected_entity);

	const auto definition = session.function_definition(fixture.function);
	assert(definition);
	assert(definition->entity == fixture.definition);
	assert(definition->handle == fixture.function);
	assert(definition->parameters == std::vector<avm::LinkId>{fixture.formal});
	assert(definition->body == fixture.body);

	bool rejected = false;
	try
	{
		static_cast<void>(session.inspect_link(fixture.store.size() + 1000));
	}
	catch (const std::out_of_range &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(fixture.store.size() == size_after_endpoints);
	assert(size_after_endpoints == size_before + 2);
}

void verify_execute_reuses_canonical_runtime_semantics()
{
	Fixture fixture;
	avm::tooling::InspectionSession session(fixture.store, fixture.vocabulary, 128);

	const avm::LinkId direct = fixture.runtime.execute(fixture.boolean_root);
	const std::size_t size_before = fixture.store.size();
	assert(session.execute(fixture.boolean_root) == direct);
	assert(fixture.store.size() == size_before);
}

void verify_trace_and_frame_inspection()
{
	Fixture fixture;
	avm::tooling::InspectionSession session(fixture.store, fixture.vocabulary, 128);

	assert(session.trace_execute(fixture.call) == fixture.vocabulary.true_value);
	assert(!session.trace_events().empty());
	assert(!session.trace_truncated());

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

	const avm::DecodedCallFrame decoded = session.call_frame(*frame);
	assert(decoded.entity == *frame);
	assert(decoded.function == fixture.function);
	assert(decoded.bindings.size() == 1);

	session.reset_trace();
	assert(session.trace_events().empty());
	assert(!session.trace_truncated());
}

void verify_bounded_trace_and_failure_retention()
{
	Fixture fixture;
	avm::tooling::InspectionSession short_session(fixture.store, fixture.vocabulary, 1);
	assert(short_session.trace_execute(fixture.boolean_root) == fixture.vocabulary.true_value);
	assert(short_session.trace_events().size() == 1);
	assert(short_session.trace_truncated());

	const avm::LinkId unknown_relation = fixture.store.create_point();
	const avm::LinkId failure_subject = fixture.store.create_point();
	const avm::LinkId failure_object = fixture.store.create_point();
	const avm::LinkId failure_root =
	    avm::encode_relation_entity(fixture.store, {unknown_relation, failure_subject, failure_object});

	avm::tooling::InspectionSession failure_session(fixture.store, fixture.vocabulary, 16);
	const std::size_t size_before = fixture.store.size();
	bool rejected = false;
	try
	{
		static_cast<void>(failure_session.trace_execute(failure_root));
	}
	catch (const std::runtime_error &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(fixture.store.size() == size_before);
	assert(failure_session.trace_events().size() == 2);
	assert(failure_session.trace_events().front().kind == avm::ExecutionEventKind::Enter);
	assert(failure_session.trace_events().back().kind == avm::ExecutionEventKind::Fail);
	assert(failure_session.trace_events().back().failure_phase == avm::ExecutionFailurePhase::Dispatch);

	failure_session.reset_trace();
	assert(failure_session.execute(fixture.boolean_root) == fixture.vocabulary.true_value);
	assert(failure_session.trace_events().empty());
}

} // namespace

int main()
{
	verify_session_construction_is_idempotent();
	verify_read_only_inspection_never_materializes();
	verify_execute_reuses_canonical_runtime_semantics();
	verify_trace_and_frame_inspection();
	verify_bounded_trace_and_failure_retention();
	return 0;
}
