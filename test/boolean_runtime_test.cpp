#include "avm/bootstrap_runtime.h"
#include "avm/execution_trace.h"
#include "avm/reference.h"
#include "avm/semantic_primitives.h"

#include <cassert>
#include <stdexcept>

int main()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::BootstrapVocabulary &v = runtime.vocabulary();

	const avm::LinkId t = builder.literal(v.true_value);
	const avm::LinkId f = builder.literal(v.false_value);
	assert(runtime.execute(t) == v.true_value);
	assert(runtime.execute(f) == v.false_value);

	assert(runtime.execute(builder.logical_not(t)) == v.false_value);
	assert(runtime.execute(builder.logical_not(f)) == v.true_value);

	assert(runtime.execute(builder.logical_and(f, f)) == v.false_value);
	assert(runtime.execute(builder.logical_and(f, t)) == v.false_value);
	assert(runtime.execute(builder.logical_and(t, f)) == v.false_value);
	assert(runtime.execute(builder.logical_and(t, t)) == v.true_value);

	assert(runtime.execute(builder.logical_or(f, f)) == v.false_value);
	assert(runtime.execute(builder.logical_or(f, t)) == v.true_value);
	assert(runtime.execute(builder.logical_or(t, f)) == v.true_value);
	assert(runtime.execute(builder.logical_or(t, t)) == v.true_value);

	const avm::LinkId nested = builder.logical_not(builder.logical_and(t, f));
	const std::size_t before_nested_execute = store.size();
	assert(runtime.execute(nested) == v.true_value);
	assert(store.size() == before_nested_execute);

	const avm::LinkId arbitrary_value = store.create_point();
	const avm::LinkId invalid_not = builder.logical_not(builder.literal(arbitrary_value));
	const std::size_t before_invalid_not = store.size();
	bool invalid_boolean_rejected = false;
	try
	{
		static_cast<void>(runtime.execute(invalid_not));
	}
	catch (const std::runtime_error &)
	{
		invalid_boolean_rejected = true;
	}
	assert(invalid_boolean_rejected);
	assert(store.size() == before_invalid_not);

	const avm::LinkId unknown_relation = store.create_point();
	const avm::LinkId failing_branch =
	    avm::encode_relation_entity(store, avm::RelationEntity{unknown_relation, v.unit, v.false_value});

	const avm::LinkId lazy_true = builder.conditional(t, f, failing_branch);
	assert(runtime.execute(lazy_true) == v.false_value);

	const avm::LinkId lazy_false = builder.conditional(f, failing_branch, t);
	assert(runtime.execute(lazy_false) == v.true_value);

	const avm::LinkId selected_failure = builder.conditional(t, failing_branch, f);
	bool selected_branch_executed = false;
	try
	{
		static_cast<void>(runtime.execute(selected_failure));
	}
	catch (const std::runtime_error &)
	{
		selected_branch_executed = true;
	}
	assert(selected_branch_executed);

	const avm::LinkId invalid_condition = builder.conditional(builder.literal(arbitrary_value), t, f);
	bool invalid_condition_rejected = false;
	try
	{
		static_cast<void>(runtime.execute(invalid_condition));
	}
	catch (const std::runtime_error &)
	{
		invalid_condition_rejected = true;
	}
	assert(invalid_condition_rejected);

	const avm::LinkId sequence = builder.sequence({t, f, nested});
	assert(runtime.execute(sequence) == v.true_value);
	assert(runtime.execute(builder.sequence({})) == v.nil);

	const avm::LinkId malformed_not =
	    avm::encode_relation_entity(store, avm::RelationEntity{v.not_relation, v.unit, v.nil});
	bool arity_rejected = false;
	try
	{
		static_cast<void>(runtime.execute(malformed_not));
	}
	catch (const std::runtime_error &)
	{
		arity_rejected = true;
	}
	assert(arity_rejected);

	const avm::ReferenceVocabulary references = avm::ReferenceVocabulary::create(store);
	const avm::SemanticExecutionVocabulary semantic = avm::SemanticExecutionVocabulary::create(store);
	avm::register_semantic_execution_primitives(runtime.executor(), semantic, references, v.unit);
	const avm::LinkId current_relation_state_reference =
	    avm::realize_context_role_reference(store, references, avm::ReferenceRole::RelationState);
	const avm::LinkId read_relation_state =
	    avm::materialize_reference_resolution(store, semantic, v.unit, current_relation_state_reference);

	const avm::LinkId semantic_entity = store.create_point();
	const avm::LinkId semantic_subject = store.create_point();
	const avm::LinkId semantic_object = store.create_point();
	const avm::SemanticContextView initial = avm::SemanticContextView::root(avm::SemanticContextFrame{
	    semantic_entity,
	    v.true_value,
	    semantic_subject,
	    semantic_object,
	});
	const avm::SemanticContextView initial_before = initial;

	avm::BoundedExecutionTrace trace(64);
	runtime.executor().set_observer(&trace);
	const avm::LinkId semantic_lazy = builder.conditional(read_relation_state, f, failing_branch);
	const avm::ExecutionOutcome semantic_lazy_outcome =
	    runtime.executor().execute_outcome_in_context(semantic_lazy, initial);
	assert(semantic_lazy_outcome.result == v.false_value);
	assert(semantic_lazy_outcome.semantic == initial);
	assert(initial == initial_before);
	for (const avm::ExecutionEvent &event : trace.events())
		assert(event.context.entity != failing_branch);

	const avm::LinkId stateful_condition_relation = store.create_point();
	const avm::LinkId stateful_condition = avm::encode_relation_entity(
	    store, avm::RelationEntity{stateful_condition_relation, v.unit, v.nil});
	runtime.executor().register_native(
	    stateful_condition_relation,
	    [&v](const avm::ExecutionContext &context, avm::Executor &)
	    {
		    if (!context.semantic)
			    throw std::runtime_error("stateful Boolean condition requires semantic context");
		    return avm::ExecutionOutcome{v.true_value, context.semantic.with_relation_state(v.false_value)};
	    });

	const avm::LinkId threaded_condition = builder.conditional(stateful_condition, read_relation_state, failing_branch);
	const avm::ExecutionOutcome threaded_outcome =
	    runtime.executor().execute_outcome_in_context(threaded_condition, initial);
	assert(threaded_outcome.result == v.false_value);
	assert(threaded_outcome.semantic.role(avm::SemanticContextRole::RelationState) == v.false_value);
	assert(initial == initial_before);

	const avm::LinkId commit_false =
	    avm::materialize_relation_state_commit(store, semantic, v.unit, builder.literal(v.false_value));
	const avm::LinkId branch_state_transition = builder.conditional(t, commit_false, failing_branch);
	const avm::ExecutionOutcome branch_outcome =
	    runtime.executor().execute_outcome_in_context(branch_state_transition, initial);
	assert(branch_outcome.result == v.false_value);
	assert(branch_outcome.semantic.role(avm::SemanticContextRole::RelationState) == v.false_value);
	assert(initial == initial_before);

	bool semantic_invalid_condition_rejected = false;
	try
	{
		static_cast<void>(runtime.executor().execute_outcome_in_context(invalid_condition, initial));
	}
	catch (const std::runtime_error &)
	{
		semantic_invalid_condition_rejected = true;
	}
	assert(semantic_invalid_condition_rejected);

	runtime.executor().set_observer(nullptr);
	const std::size_t converged_size = store.size();
	const avm::ExecutionOutcome repeated =
	    runtime.executor().execute_outcome_in_context(threaded_condition, initial);
	assert(repeated.result == threaded_outcome.result);
	assert(repeated.semantic == threaded_outcome.semantic);
	assert(store.size() == converged_size);

	return 0;
}
