#include "avm/independent_projection.h"
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

avm::LinkId relation_entity(avm::LinkStore &store, avm::LinkId relation, avm::LinkId subject, avm::LinkId object)
{
	return avm::encode_relation_entity(store, avm::RelationEntity{relation, subject, object});
}

avm::LinkId executable(avm::LinkStore &store, avm::LinkId relation)
{
	return relation_entity(store, relation, store.create_point(), store.create_point());
}

class RecordingObserver final : public avm::ExecutionObserver
{
public:
	void observe(const avm::ExecutionEvent &event) override { events.push_back(event); }

	std::vector<avm::ExecutionEvent> events;
};

std::filesystem::path temporary_path(const std::string &suffix)
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() /
	       ("avm-independent-projection-" + std::to_string(nonce) + "-" + suffix + ".bin");
}

struct FileCleanup
{
	std::filesystem::path path;
	~FileCleanup()
	{
		std::error_code error;
		std::filesystem::remove_all(path, error);
	}
};

void register_fixed_body(avm::Executor &executor, avm::LinkId relation, avm::LinkId result)
{
	executor.register_native(relation,
	                         [result](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         assert(context.semantic);
		                         return avm::ExecutionOutcome{
		                             result,
		                             context.semantic.with_relation_state(result),
		                         };
	                         });
}

void test_independent_projection()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId list_nil = store.create_point();
	const avm::IndependentProjectionVocabulary projection = avm::IndependentProjectionVocabulary::create(store);

	const avm::LinkId result1 = store.create_point();
	const avm::LinkId result2 = store.create_point();
	const avm::LinkId result3 = store.create_point();
	const std::vector<avm::LinkId> expected_results{result1, result2, result3};

	const avm::LinkId body_relation1 = store.create_point();
	const avm::LinkId body_relation2 = store.create_point();
	const avm::LinkId body_relation3 = store.create_point();
	const avm::LinkId body1 = executable(store, body_relation1);
	const avm::LinkId body2 = executable(store, body_relation2);
	const avm::LinkId body3 = executable(store, body_relation3);
	const std::vector<avm::LinkId> bodies{body1, body2, body3};
	const avm::LinkId body_list = avm::encode_link_list(store, list_nil, bodies);
	const avm::LinkId projection_entity = relation_entity(store, projection.relation, body_list, list_nil);

	const avm::SemanticContextFrame root_frame{
	    store.create_point(),
	    store.create_point(),
	    store.create_point(),
	    store.create_point(),
	};
	const avm::SemanticContextView root = avm::SemanticContextView::root(root_frame);

	RecordingObserver observer;
	avm::Executor executor(store, &observer);
	avm::register_independent_projection_runtime(executor, projection);

	std::vector<avm::SemanticContextView> seen_semantics;
	std::vector<std::optional<avm::LinkId>> seen_parents;
	const auto register_body = [&](avm::LinkId relation, avm::LinkId result)
	{
		executor.register_native(relation,
		                         [&, result](const avm::ExecutionContext &context, avm::Executor &)
		                         {
			                         assert(context.semantic);
			                         seen_semantics.push_back(context.semantic);
			                         seen_parents.push_back(context.parent);
			                         return avm::ExecutionOutcome{
			                             result,
			                             context.semantic.with_relation_state(result),
			                         };
		                         });
	};
	register_body(body_relation1, result1);
	register_body(body_relation2, result2);
	register_body(body_relation3, result3);

	const std::size_t before_first = store.size();
	const avm::ExecutionOutcome first = executor.execute_outcome_in_context(projection_entity, root);
	assert(avm::decode_link_list(store, list_nil, first.result) == expected_results);
	assert(first.semantic == root);
	assert(seen_semantics.size() == bodies.size());
	assert(seen_parents.size() == bodies.size());
	for (std::size_t index = 0; index < bodies.size(); ++index)
	{
		assert(seen_semantics[index] == root);
		assert(seen_semantics[index].current().relation_state == root_frame.relation_state);
		assert(seen_parents[index] == projection_entity);
	}
	assert(store.size() >= before_first);

	assert(observer.events.size() == 8);
	assert(observer.events[0].kind == avm::ExecutionEventKind::Enter);
	assert(observer.events[0].context.entity == projection_entity);
	for (std::size_t index = 0; index < bodies.size(); ++index)
	{
		const std::size_t enter = 1 + index * 2;
		const std::size_t returned = enter + 1;
		assert(observer.events[enter].kind == avm::ExecutionEventKind::Enter);
		assert(observer.events[enter].context.entity == bodies[index]);
		assert(observer.events[enter].context.parent == projection_entity);
		assert(observer.events[returned].kind == avm::ExecutionEventKind::Return);
		assert(observer.events[returned].context.entity == bodies[index]);
		assert(observer.events[returned].result == expected_results[index]);
	}
	assert(observer.events[7].kind == avm::ExecutionEventKind::Return);
	assert(observer.events[7].context.entity == projection_entity);
	assert(observer.events[7].result == first.result);
	assert(observer.events[7].semantic_result == root);

	const std::size_t converged_size = store.size();
	const avm::ExecutionOutcome repeated = executor.execute_outcome_in_context(projection_entity, root);
	assert(repeated.result == first.result);
	assert(repeated.semantic == root);
	assert(store.size() == converged_size);

	const avm::LinkId empty_projection = relation_entity(store, projection.relation, list_nil, list_nil);
	const std::size_t before_empty = store.size();
	const avm::ExecutionOutcome empty = executor.execute_outcome_in_context(empty_projection, root);
	assert(empty.result == list_nil);
	assert(empty.semantic == root);
	assert(store.size() == before_empty);

	bool missing_context_rejected = false;
	try
	{
		static_cast<void>(executor.execute(projection_entity));
	}
	catch (const std::logic_error &)
	{
		missing_context_rejected = true;
	}
	assert(missing_context_rejected);

	const avm::LinkId fail_relation1 = store.create_point();
	const avm::LinkId fail_relation2 = store.create_point();
	const avm::LinkId fail_relation3 = store.create_point();
	const avm::LinkId fail_body1 = executable(store, fail_relation1);
	const avm::LinkId fail_body2 = executable(store, fail_relation2);
	const avm::LinkId fail_body3 = executable(store, fail_relation3);
	const avm::LinkId fail_bodies = avm::encode_link_list(store, list_nil, {fail_body1, fail_body2, fail_body3});
	const avm::LinkId failing_projection = relation_entity(store, projection.relation, fail_bodies, list_nil);

	std::vector<avm::LinkId> failure_seen;
	executor.register_native(fail_relation1,
	                         [&](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         failure_seen.push_back(context.entity);
		                         return avm::ExecutionOutcome{result1, context.semantic};
	                         });
	executor.register_native(fail_relation2,
	                         [&](const avm::ExecutionContext &context, avm::Executor &) -> avm::ExecutionOutcome
	                         {
		                         failure_seen.push_back(context.entity);
		                         throw std::runtime_error("expected independent projection failure");
	                         });
	executor.register_native(fail_relation3,
	                         [&](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         failure_seen.push_back(context.entity);
		                         return avm::ExecutionOutcome{result3, context.semantic};
	                         });

	const std::size_t before_failure = store.size();
	bool failed = false;
	try
	{
		static_cast<void>(executor.execute_outcome_in_context(failing_projection, root));
	}
	catch (const std::runtime_error &)
	{
		failed = true;
	}
	assert(failed);
	assert(failure_seen == std::vector<avm::LinkId>({fail_body1, fail_body2}));
	assert(store.size() == before_failure);

	const avm::LinkId malformed_body_list = store.intern(body1, result1);
	const avm::LinkId malformed_projection = relation_entity(store, projection.relation, malformed_body_list, list_nil);
	const std::size_t before_malformed = store.size();
	const std::size_t bodies_before_malformed = seen_semantics.size();
	bool malformed_rejected = false;
	try
	{
		static_cast<void>(executor.execute_outcome_in_context(malformed_projection, root));
	}
	catch (const std::runtime_error &)
	{
		malformed_rejected = true;
	}
	assert(malformed_rejected);
	assert(seen_semantics.size() == bodies_before_malformed);
	assert(store.size() == before_malformed);
}

void test_persistent_reopen()
{
	const std::filesystem::path path = temporary_path("reopen");
	FileCleanup cleanup{path};

	avm::LinkId list_nil = avm::invalid_link_id;
	avm::LinkId projection_relation = avm::invalid_link_id;
	avm::LinkId body_relation1 = avm::invalid_link_id;
	avm::LinkId body_relation2 = avm::invalid_link_id;
	avm::LinkId body1 = avm::invalid_link_id;
	avm::LinkId body2 = avm::invalid_link_id;
	avm::LinkId result1 = avm::invalid_link_id;
	avm::LinkId result2 = avm::invalid_link_id;
	avm::LinkId projection_entity = avm::invalid_link_id;
	avm::LinkId result_list = avm::invalid_link_id;
	avm::SemanticContextFrame root_frame{};
	std::size_t converged_size = 0;

	{
		avm::PersistentLinkStore store(path);
		list_nil = store.create_point();
		const avm::IndependentProjectionVocabulary projection = avm::IndependentProjectionVocabulary::create(store);
		projection_relation = projection.relation;
		result1 = store.create_point();
		result2 = store.create_point();
		body_relation1 = store.create_point();
		body_relation2 = store.create_point();
		body1 = executable(store, body_relation1);
		body2 = executable(store, body_relation2);
		const avm::LinkId body_list = avm::encode_link_list(store, list_nil, {body1, body2});
		projection_entity = relation_entity(store, projection_relation, body_list, list_nil);
		root_frame = avm::SemanticContextFrame{
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		};
		const avm::SemanticContextView root = avm::SemanticContextView::root(root_frame);

		avm::Executor executor(store);
		avm::register_independent_projection_runtime(executor, projection);
		register_fixed_body(executor, body_relation1, result1);
		register_fixed_body(executor, body_relation2, result2);

		const avm::ExecutionOutcome outcome = executor.execute_outcome_in_context(projection_entity, root);
		assert(avm::decode_link_list(store, list_nil, outcome.result) == std::vector<avm::LinkId>({result1, result2}));
		result_list = outcome.result;
		converged_size = store.size();
	}

	{
		avm::PersistentLinkStore store(path);
		assert(store.size() == converged_size);
		const avm::IndependentProjectionVocabulary projection{projection_relation};
		const avm::SemanticContextView root = avm::SemanticContextView::root(root_frame);
		avm::Executor executor(store);
		avm::register_independent_projection_runtime(executor, projection);
		register_fixed_body(executor, body_relation1, result1);
		register_fixed_body(executor, body_relation2, result2);

		const std::size_t before = store.size();
		const avm::ExecutionOutcome outcome = executor.execute_outcome_in_context(projection_entity, root);
		assert(outcome.result == result_list);
		assert(outcome.semantic == root);
		assert(avm::decode_link_list(store, list_nil, outcome.result) == std::vector<avm::LinkId>({result1, result2}));
		assert(store.size() == before);
	}
}

} // namespace

int main()
{
	test_independent_projection();
	test_persistent_reopen();
	return 0;
}
