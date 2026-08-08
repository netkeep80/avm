#include "inspection_session.h"

#include "avm/persistent_link_store.h"
#include "avm/relations_model.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

std::filesystem::path temporary_path()
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() /
	       ("avm-persistent-inspection-" + std::to_string(nonce) + ".bin");
}

struct FileCleanup
{
	std::filesystem::path path;

	~FileCleanup()
	{
		std::error_code error;
		std::filesystem::remove(path, error);
	}
};

std::vector<avm::ExecutionEvent> copy_complete_trace(const avm::tooling::InspectionSession &session)
{
	assert(!session.trace_truncated());
	return std::vector<avm::ExecutionEvent>(session.trace_events().begin(), session.trace_events().end());
}

struct PersistentInspectionBaseline
{
	avm::BootstrapVocabulary vocabulary;
	avm::LinkId point_a;
	avm::LinkId point_b;
	avm::LinkId pair;
	avm::LinkId relation;
	avm::LinkId relation_entity;
	avm::LinkId function;
	avm::LinkId definition;
	avm::LinkId call_root;
	avm::LinkId boolean_root;
	avm::LinkId expected_result;
	std::vector<avm::ExecutionEvent> call_trace;
	std::size_t store_size;
};

PersistentInspectionBaseline create_baseline(const std::filesystem::path &path)
{
	avm::PersistentLinkStore store(path);
	avm::BootstrapRuntime runtime(store);
	const avm::BootstrapVocabulary vocabulary = runtime.vocabulary();
	avm::ProgramBuilder builder = runtime.builder();

	const avm::LinkId point_a = store.create_point();
	const avm::LinkId point_b = store.create_point();
	const avm::LinkId pair = store.intern(point_a, point_b);
	const avm::LinkId relation = store.create_point();
	const avm::LinkId relation_entity = avm::encode_relation_entity(store, {relation, point_a, point_b});

	const avm::LinkId formal = store.create_point();
	const avm::LinkId function = builder.create_function_handle();
	const avm::LinkId parameter = builder.parameter(formal);
	const avm::LinkId definition = builder.define_function(function, {formal}, parameter);
	const avm::LinkId true_literal = builder.literal(vocabulary.true_value);
	const avm::LinkId call_root = builder.call(function, {true_literal});
	const avm::LinkId boolean_root = builder.logical_not(builder.literal(vocabulary.false_value));

	assert(runtime.execute(call_root) == vocabulary.true_value);
	const std::size_t converged_size = store.size();
	assert(runtime.execute(call_root) == vocabulary.true_value);
	assert(store.size() == converged_size);

	avm::tooling::InspectionSession session(store, vocabulary, 256);
	assert(store.size() == converged_size);
	assert(session.trace_events().empty());

	assert(session.inspect_link(pair) == (avm::Link{point_a, point_b}));
	assert(session.find_pair(point_a, point_b) == pair);
	assert(session.decode_relation(relation_entity) == (avm::RelationEntity{relation, point_a, point_b}));

	const auto found_definition = session.function_definition(function);
	assert(found_definition);
	assert(found_definition->entity == definition);
	assert(found_definition->handle == function);

	const avm::RelationQuery query{
	    .relation = relation,
	    .subject = point_a,
	    .object = point_b,
	};
	const auto matches = session.query_relations(query);
	assert(matches.size() == 1);
	assert(matches.front().entity_id == relation_entity);
	assert(store.size() == converged_size);

	assert(session.trace_execute(call_root) == vocabulary.true_value);
	assert(store.size() == converged_size);
	const std::vector<avm::ExecutionEvent> call_trace = copy_complete_trace(session);
	assert(!call_trace.empty());

	return PersistentInspectionBaseline{
	    vocabulary,
	    point_a,
	    point_b,
	    pair,
	    relation,
	    relation_entity,
	    function,
	    definition,
	    call_root,
	    boolean_root,
	    vocabulary.true_value,
	    call_trace,
	    store.size(),
	};
}

void verify_reopened_session(const std::filesystem::path &path, const PersistentInspectionBaseline &baseline)
{
	avm::PersistentLinkStore store(path);
	assert(store.size() == baseline.store_size);

	avm::tooling::InspectionSession session(store, baseline.vocabulary, 256);
	assert(store.size() == baseline.store_size);
	assert(session.trace_events().empty());

	const avm::Link expected_pair{baseline.point_a, baseline.point_b};
	assert(session.inspect_link(baseline.pair) == expected_pair);
	assert(session.find_pair(baseline.point_a, baseline.point_b) == baseline.pair);

	const avm::RelationEntity expected_entity{baseline.relation, baseline.point_a, baseline.point_b};
	assert(session.decode_relation(baseline.relation_entity) == expected_entity);
	const avm::RelationQuery query{
	    .relation = baseline.relation,
	    .subject = baseline.point_a,
	    .object = baseline.point_b,
	};
	const auto matches = session.query_relations(query);
	assert(matches.size() == 1);
	assert(matches.front().entity_id == baseline.relation_entity);

	const auto definition = session.function_definition(baseline.function);
	assert(definition);
	assert(definition->entity == baseline.definition);
	assert(store.size() == baseline.store_size);

	assert(session.execute(baseline.boolean_root) == baseline.expected_result);
	assert(store.size() == baseline.store_size);

	assert(session.trace_execute(baseline.call_root) == baseline.expected_result);
	assert(copy_complete_trace(session) == baseline.call_trace);
	assert(store.size() == baseline.store_size);

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
	const avm::DecodedCallFrame decoded_frame = session.call_frame(*frame);
	assert(decoded_frame.entity == *frame);
	assert(decoded_frame.function == baseline.function);
	assert(store.size() == baseline.store_size);

	session.reset_trace();
	assert(session.trace_events().empty());
	assert(store.size() == baseline.store_size);
}

void verify_persistent_session_survives_multiple_reopens()
{
	const std::filesystem::path path = temporary_path();
	FileCleanup cleanup{path};
	const PersistentInspectionBaseline baseline = create_baseline(path);

	verify_reopened_session(path, baseline);
	verify_reopened_session(path, baseline);
}

} // namespace

int main()
{
	verify_persistent_session_survives_multiple_reopens();
	return 0;
}
