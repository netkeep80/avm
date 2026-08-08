#include "avm/execution_trace.h"
#include "avm/program_model.h"
#include "avm/semantic_execution.h"

#include <array>
#include <cassert>
#include <stdexcept>
#include <type_traits>
#include <vector>

static_assert(std::is_nothrow_copy_constructible_v<avm::ExecutionOutcome>);
static_assert(std::is_nothrow_copy_constructible_v<avm::ExecutionEvent>);

int main()
{
	avm::InMemoryLinkStore store;
	avm::BoundedExecutionTrace trace(64);
	avm::Executor executor(store, &trace);

	const avm::LinkId initial_state = store.create_point();
	const avm::LinkId value_2 = store.create_point();
	const avm::LinkId value_3 = store.create_point();
	const avm::LinkId value_5 = store.create_point();
	const avm::LinkId semantic_entity = store.create_point();
	const avm::LinkId semantic_subject = store.create_point();
	const avm::LinkId semantic_object = store.create_point();
	const avm::SemanticContextView root = avm::SemanticContextView::root(avm::SemanticContextFrame{
	    semantic_entity,
	    initial_state,
	    semantic_subject,
	    semantic_object,
	});

	const avm::LinkId set_state_relation = store.create_point();
	const avm::LinkId return_only_relation = store.create_point();
	const avm::LinkId combine_relation = store.create_point();
	const avm::LinkId invalid_result_relation = store.create_point();
	const avm::LinkId sequence_relation = store.create_point();
	const avm::LinkId dispatch_subject = store.create_point();
	const avm::LinkId nil = store.create_point();

	executor.register_native(set_state_relation,
	                         [](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         if (!context.semantic)
			                         throw std::runtime_error("set-state requires semantic context");
		                         return avm::ExecutionOutcome{
		                             context.object,
		                             context.semantic.with_relation_state(context.object),
		                         };
	                         });

	// This intentionally returns only LinkId. Implicit conversion to
	// ExecutionOutcome proves old native handlers remain source-compatible.
	executor.register_native(return_only_relation,
	                         [](const avm::ExecutionContext &context, avm::Executor &) { return context.object; });

	executor.register_native(combine_relation,
	                         [value_2, value_3, value_5](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         if (!context.semantic)
			                         throw std::runtime_error("combine requires semantic context");
		                         if (context.semantic.role(avm::SemanticContextRole::RelationState) != value_2 ||
		                             context.object != value_3)
			                         throw std::runtime_error("unexpected semantic composition input");
		                         return avm::ExecutionOutcome{
		                             value_5,
		                             context.semantic.with_relation_state(value_5),
		                         };
	                         });

	executor.register_native(invalid_result_relation,
	                         [value_3](const avm::ExecutionContext &context, avm::Executor &)
	                         {
		                         return avm::ExecutionOutcome{
		                             avm::invalid_link_id,
		                             context.semantic.with_relation_state(value_3),
		                         };
	                         });

	executor.register_native(sequence_relation,
	                         [nil](const avm::ExecutionContext &context, avm::Executor &current_executor)
	                         {
		                         const std::vector<avm::LinkId> children =
		                             avm::decode_link_list(current_executor.store(), nil, context.object);
		                         return avm::execute_same_context_sequence(current_executor, children, nil, context);
	                         });

	const avm::LinkId set_value_2 =
	    avm::encode_relation_entity(store, avm::RelationEntity{set_state_relation, dispatch_subject, value_2});
	const avm::LinkId return_value_3 =
	    avm::encode_relation_entity(store, avm::RelationEntity{return_only_relation, dispatch_subject, value_3});
	const avm::LinkId combine_value_3 =
	    avm::encode_relation_entity(store, avm::RelationEntity{combine_relation, dispatch_subject, value_3});
	const std::array<avm::LinkId, 3> children{set_value_2, return_value_3, combine_value_3};
	const avm::LinkId child_list = avm::encode_link_list(store, nil, children);
	const avm::LinkId sequence_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{sequence_relation, dispatch_subject, child_list});

	const avm::SemanticContextView root_before = root;
	const std::size_t store_before_sequence = store.size();
	const avm::ExecutionOutcome sequence_outcome = executor.execute_outcome_in_context(sequence_entity, root);
	assert(store.size() == store_before_sequence);
	assert(sequence_outcome.result == value_5);
	assert(sequence_outcome.semantic.role(avm::SemanticContextRole::RelationState) == value_5);
	assert(root == root_before);
	assert(root.role(avm::SemanticContextRole::RelationState) == initial_state);

	const auto events = trace.events();
	assert(events.size() == 8);
	assert(events[0].kind == avm::ExecutionEventKind::Enter);
	assert(events[0].context.entity == sequence_entity);
	assert(events[0].context.semantic == root);
	assert(!events[0].semantic_result);

	assert(events[2].kind == avm::ExecutionEventKind::Return);
	assert(events[2].context.entity == set_value_2);
	assert(events[2].context.semantic.role(avm::SemanticContextRole::RelationState) == initial_state);
	assert(events[2].semantic_result.role(avm::SemanticContextRole::RelationState) == value_2);

	assert(events[4].kind == avm::ExecutionEventKind::Return);
	assert(events[4].context.entity == return_value_3);
	assert(events[4].result == value_3);
	assert(events[4].context.semantic.role(avm::SemanticContextRole::RelationState) == value_2);
	assert(events[4].semantic_result.role(avm::SemanticContextRole::RelationState) == value_2);

	assert(events[6].kind == avm::ExecutionEventKind::Return);
	assert(events[6].context.entity == combine_value_3);
	assert(events[6].context.semantic.role(avm::SemanticContextRole::RelationState) == value_2);
	assert(events[6].result == value_5);
	assert(events[6].semantic_result.role(avm::SemanticContextRole::RelationState) == value_5);

	assert(events[7].kind == avm::ExecutionEventKind::Return);
	assert(events[7].context.entity == sequence_entity);
	assert(events[7].context.semantic == root);
	assert(events[7].result == value_5);
	assert(events[7].semantic_result == sequence_outcome.semantic);

	trace.reset();
	const avm::LinkId invalid_result_entity =
	    avm::encode_relation_entity(store, avm::RelationEntity{invalid_result_relation, dispatch_subject, value_3});
	const std::size_t store_before_failure = store.size();
	bool rejected = false;
	try
	{
		static_cast<void>(executor.execute_outcome_in_context(invalid_result_entity, root));
	}
	catch (const std::runtime_error &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(store.size() == store_before_failure);
	assert(trace.size() == 2);
	assert(trace.events()[0].kind == avm::ExecutionEventKind::Enter);
	assert(trace.events()[1].kind == avm::ExecutionEventKind::Fail);
	assert(trace.events()[1].failure_phase == avm::ExecutionFailurePhase::ResultValidation);
	assert(!trace.events()[1].semantic_result);
	assert(root.role(avm::SemanticContextRole::RelationState) == initial_state);

	return 0;
}
