#include "avm/foreach_runtime.h"
#include "avm/relations_model.h"

#include <cassert>
#include <cstddef>
#include <stdexcept>
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

avm::LinkId semantic_role(const avm::ExecutionContext &context, avm::SemanticContextRole role)
{
	assert(context.semantic);
	return context.semantic.role(role);
}

class RecordingObserver final : public avm::ExecutionObserver
{
public:
	void observe(const avm::ExecutionEvent &event) override { events.push_back(event); }

	std::vector<avm::ExecutionEvent> events;
};

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId list_nil = store.create_point();
	const avm::ForeachVocabulary foreach = avm::ForeachVocabulary::create(store);

	const avm::LinkId item1 = store.create_point();
	const avm::LinkId item2 = store.create_point();
	const avm::LinkId item3 = store.create_point();
	const std::vector<avm::LinkId> items{item1, item2, item3};
	const avm::LinkId input = avm::encode_link_list(store, list_nil, items);

	const avm::SemanticContextFrame root_frame{
	    store.create_point(),
	    store.create_point(),
	    store.create_point(),
	    store.create_point(),
	};
	const avm::SemanticContextView root = avm::SemanticContextView::root(root_frame);

	RecordingObserver observer;
	avm::Executor executor(store, &observer);
	avm::register_foreach_runtime(executor, foreach, list_nil);

	const avm::LinkId object_body_relation = store.create_point();
	std::vector<avm::LinkId> object_seen;
	std::vector<avm::LinkId> relation_state_seen;
	const auto object_body_handler = [&](const avm::ExecutionContext &context, avm::Executor &) -> avm::ExecutionOutcome
	{
		assert(context.semantic.depth() == 1);
		assert(context.semantic.parent().current() == root_frame);
		assert(semantic_role(context, avm::SemanticContextRole::Entity) == root_frame.entity);
		assert(semantic_role(context, avm::SemanticContextRole::Subject) == root_frame.subject);
		assert(semantic_role(context, avm::SemanticContextRole::RelationState) == root_frame.relation_state);

		const avm::LinkId item = semantic_role(context, avm::SemanticContextRole::Object);
		object_seen.push_back(item);
		relation_state_seen.push_back(semantic_role(context, avm::SemanticContextRole::RelationState));
		return avm::ExecutionOutcome{item, context.semantic.with_relation_state(item)};
	};
	executor.register_native(object_body_relation, object_body_handler);

	const avm::LinkId object_body = executable(store, object_body_relation);
	const avm::LinkId object_foreach = relation_entity(store, foreach.object_relation, object_body, input);
	const avm::ExecutionOutcome object_outcome = executor.execute_outcome_in_context(object_foreach, root);

	assert(avm::decode_link_list(store, list_nil, object_outcome.result) == items);
	assert(object_outcome.semantic == root);
	assert(object_seen == items);
	assert(relation_state_seen.size() == items.size());
	for (const avm::LinkId state : relation_state_seen)
		assert(state == root_frame.relation_state);

	std::size_t foreach_enter = 0;
	std::size_t body_enter = 0;
	for (const avm::ExecutionEvent &event : observer.events)
	{
		if (event.kind != avm::ExecutionEventKind::Enter)
			continue;
		if (event.context.entity == object_foreach)
			++foreach_enter;
		if (event.context.entity == object_body)
			++body_enter;
	}
	assert(foreach_enter == 1);
	assert(body_enter == items.size());

	const avm::LinkId subject_body_relation = store.create_point();
	std::vector<avm::LinkId> subject_seen;
	const auto subject_body_handler = [&](const avm::ExecutionContext &context, avm::Executor &)
	{
		assert(semantic_role(context, avm::SemanticContextRole::Object) == root_frame.object);
		const avm::LinkId item = semantic_role(context, avm::SemanticContextRole::Subject);
		subject_seen.push_back(item);
		return avm::ExecutionOutcome{item, context.semantic};
	};
	executor.register_native(subject_body_relation, subject_body_handler);

	const avm::LinkId subject_body = executable(store, subject_body_relation);
	const avm::LinkId subject_foreach = relation_entity(store, foreach.subject_relation, subject_body, input);
	const avm::ExecutionOutcome subject_outcome = executor.execute_outcome_in_context(subject_foreach, root);

	assert(avm::decode_link_list(store, list_nil, subject_outcome.result) == items);
	assert(subject_outcome.semantic == root);
	assert(subject_seen == items);

	const avm::LinkId empty_foreach = relation_entity(store, foreach.object_relation, object_body, list_nil);
	const std::size_t before_empty = store.size();
	const avm::ExecutionOutcome empty_outcome = executor.execute_outcome_in_context(empty_foreach, root);
	assert(empty_outcome.result == list_nil);
	assert(store.size() == before_empty);

	const avm::LinkId fail_body_relation = store.create_point();
	std::vector<avm::LinkId> failure_seen;
	const auto fail_body_handler = [&](const avm::ExecutionContext &context, avm::Executor &) -> avm::ExecutionOutcome
	{
		const avm::LinkId item = semantic_role(context, avm::SemanticContextRole::Object);
		failure_seen.push_back(item);
		if (item == item2)
			throw std::runtime_error("expected foreach body failure");
		return avm::ExecutionOutcome{item, context.semantic};
	};
	executor.register_native(fail_body_relation, fail_body_handler);

	const avm::LinkId fail_body = executable(store, fail_body_relation);
	const avm::LinkId failing_foreach = relation_entity(store, foreach.object_relation, fail_body, input);
	const std::size_t before_failure = store.size();
	bool failed = false;
	try
	{
		static_cast<void>(executor.execute_outcome_in_context(failing_foreach, root));
	}
	catch (const std::runtime_error &)
	{
		failed = true;
	}
	assert(failed);
	assert(failure_seen == std::vector<avm::LinkId>({item1, item2}));
	assert(store.size() == before_failure);

	bool missing_context_rejected = false;
	try
	{
		static_cast<void>(executor.execute(object_foreach));
	}
	catch (const std::logic_error &)
	{
		missing_context_rejected = true;
	}
	assert(missing_context_rejected);

	const avm::LinkId malformed_list = store.intern(item1, item2);
	const avm::LinkId malformed_foreach = relation_entity(store, foreach.object_relation, object_body, malformed_list);
	const std::size_t before_malformed = store.size();
	bool malformed_rejected = false;
	try
	{
		static_cast<void>(executor.execute_outcome_in_context(malformed_foreach, root));
	}
	catch (const std::runtime_error &)
	{
		malformed_rejected = true;
	}
	assert(malformed_rejected);
	assert(store.size() == before_malformed);

	return 0;
}
