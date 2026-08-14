#include "avm/bootstrap_runtime.h"
#include "avm/execution_trace.h"
#include "avm/integer_value.h"
#include "avm/persistent_link_store.h"
#include "avm/reference.h"
#include "avm/semantic_primitives.h"

#include <cassert>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{

bool rejected(const std::function<void()> &operation)
{
	try
	{
		operation();
		return false;
	}
	catch (const std::exception &)
	{
		return true;
	}
}

struct SemanticProgram
{
	avm::LinkId root;
	avm::LinkId first_apply;
	avm::LinkId second_apply;
	avm::LinkId current_relation_state_reference;
};

SemanticProgram build_composition(avm::LinkStore &store, avm::BootstrapRuntime &runtime,
                                  const avm::IntegerVocabulary &integers,
                                  const avm::ReferenceVocabulary &references,
                                  const avm::SemanticExecutionVocabulary &semantic)
{
	avm::ProgramBuilder builder = runtime.builder();
	const avm::LinkId one = avm::realize_integer(store, integers, 1);
	const avm::LinkId three = avm::realize_integer(store, integers, 3);
	const avm::LinkId quote_one = builder.literal(one);
	const avm::LinkId quote_three = builder.literal(three);
	const avm::LinkId current_relation_state_reference =
	    avm::realize_context_role_reference(store, references, avm::ReferenceRole::RelationState);
	const avm::LinkId read_relation_state = avm::materialize_reference_resolution(
	    store, semantic, runtime.vocabulary().unit, current_relation_state_reference);

	const avm::LinkId first_apply =
	    avm::materialize_pure_relation_application(store, semantic, integers.add_relation, quote_one, quote_one);
	const avm::LinkId first_commit =
	    avm::materialize_relation_state_commit(store, semantic, runtime.vocabulary().unit, first_apply);

	const avm::LinkId second_apply = avm::materialize_pure_relation_application(
	    store, semantic, integers.add_relation, read_relation_state, quote_three);
	const avm::LinkId second_commit =
	    avm::materialize_relation_state_commit(store, semantic, runtime.vocabulary().unit, second_apply);

	return SemanticProgram{
	    runtime.builder().sequence({first_commit, second_commit}),
	    first_apply,
	    second_apply,
	    current_relation_state_reference,
	};
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const avm::IntegerVocabulary integers = avm::IntegerVocabulary::create(store);
	const avm::ReferenceVocabulary references = avm::ReferenceVocabulary::create(store);
	const avm::SemanticExecutionVocabulary semantic = avm::SemanticExecutionVocabulary::create(store);
	avm::register_integer_arithmetic(runtime.executor(), integers);
	avm::register_semantic_execution_primitives(runtime.executor(), semantic, references, runtime.vocabulary().unit);

	const avm::LinkId zero = avm::realize_integer(store, integers, 0);
	const avm::LinkId semantic_entity = store.create_point();
	const avm::LinkId semantic_subject = store.create_point();
	const avm::LinkId semantic_object = store.create_point();
	const avm::SemanticContextView root = avm::SemanticContextView::root(avm::SemanticContextFrame{
	    semantic_entity,
	    zero,
	    semantic_subject,
	    semantic_object,
	});
	const avm::SemanticContextView root_before = root;

	const SemanticProgram program = build_composition(store, runtime, integers, references, semantic);

	const std::size_t before_reference_read = store.size();
	const avm::LinkId reference_read = avm::materialize_reference_resolution(
	    store, semantic, runtime.vocabulary().unit, program.current_relation_state_reference);
	const std::size_t after_reference_expression = store.size();
	const avm::ExecutionOutcome reference_outcome = runtime.executor().execute_outcome_in_context(reference_read, root);
	assert(reference_outcome.result == zero);
	assert(reference_outcome.semantic == root);
	assert(store.size() == after_reference_expression);
	assert(after_reference_expression >= before_reference_read);

	avm::BoundedExecutionTrace trace(128);
	runtime.executor().set_observer(&trace);

	const avm::ExecutionOutcome pure_first = runtime.executor().execute_outcome_in_context(program.first_apply, root);
	assert(avm::decode_integer(store, integers, pure_first.result) == 2);
	assert(pure_first.semantic == root);
	assert(root == root_before);

	bool saw_child_integer_add = false;
	for (const avm::ExecutionEvent &event : trace.events())
	{
		if (event.kind != avm::ExecutionEventKind::Enter || event.context.relation != integers.add_relation)
			continue;
		assert(event.context.semantic.depth() == root.depth() + 1);
		assert(event.context.semantic.current().entity == program.first_apply);
		assert(avm::decode_integer(store, integers, event.context.semantic.current().subject) == 1);
		assert(avm::decode_integer(store, integers, event.context.semantic.current().object) == 1);
		assert(event.context.semantic.current().relation_state == zero);
		saw_child_integer_add = true;
	}
	assert(saw_child_integer_add);

	trace.reset();
	const avm::ExecutionOutcome sequence = runtime.executor().execute_outcome_in_context(program.root, root);
	assert(avm::decode_integer(store, integers, sequence.result) == 5);
	assert(sequence.semantic.role(avm::SemanticContextRole::RelationState) == sequence.result);
	assert(root == root_before);
	assert(root.role(avm::SemanticContextRole::RelationState) == zero);

	bool saw_second_add = false;
	for (const avm::ExecutionEvent &event : trace.events())
	{
		if (event.kind != avm::ExecutionEventKind::Enter || event.context.relation != integers.add_relation)
			continue;
		if (avm::decode_integer(store, integers, event.context.subject) != 2 ||
		    avm::decode_integer(store, integers, event.context.object) != 3)
			continue;
		assert(event.context.semantic.current().entity == program.second_apply);
		const avm::LinkId observed_state =
		    event.context.semantic.role(avm::SemanticContextRole::RelationState);
		assert(avm::decode_integer(store, integers, observed_state) == 2);
		saw_second_add = true;
	}
	assert(saw_second_add);

	runtime.executor().set_observer(nullptr);
	const std::size_t converged_size = store.size();
	const avm::ExecutionOutcome repeated = runtime.executor().execute_outcome_in_context(program.root, root);
	assert(repeated.result == sequence.result);
	assert(repeated.semantic == sequence.semantic);
	assert(store.size() == converged_size);

	const avm::SemanticContextView missing_state = avm::SemanticContextView::root(avm::SemanticContextFrame{
	    semantic_entity,
	    avm::invalid_link_id,
	    semantic_subject,
	    semantic_object,
	});
	const std::size_t before_missing_reference = store.size();
	const auto execute_with_missing_reference = [&]
	{
		static_cast<void>(runtime.executor().execute_outcome_in_context(program.second_apply, missing_state));
	};
	assert(rejected(execute_with_missing_reference));
	assert(store.size() == before_missing_reference);

	avm::ProgramBuilder builder = runtime.builder();
	const avm::LinkId one = avm::realize_integer(store, integers, 1);
	const avm::LinkId quote_one = builder.literal(one);
	const avm::LinkId stateful_operand =
	    avm::materialize_relation_state_commit(store, semantic, runtime.vocabulary().unit, quote_one);
	const avm::LinkId invalid_pure_application =
	    avm::materialize_pure_relation_application(store, semantic, integers.add_relation, stateful_operand, quote_one);
	const auto execute_stateful_operand = [&]
	{
		static_cast<void>(runtime.executor().execute_outcome_in_context(invalid_pure_application, root));
	};
	assert(rejected(execute_stateful_operand));

	const avm::LinkId stateful_target = store.create_point();
	const auto stateful_handler = [one](const avm::ExecutionContext &context, avm::Executor &)
	{
		if (!context.semantic)
			throw std::runtime_error("stateful target requires semantic context");
		return avm::ExecutionOutcome{one, context.semantic.with_relation_state(one)};
	};
	runtime.executor().register_native(stateful_target, stateful_handler);
	const avm::LinkId invalid_pure_target =
	    avm::materialize_pure_relation_application(store, semantic, stateful_target, quote_one, quote_one);
	const auto execute_stateful_target = [&]
	{
		static_cast<void>(runtime.executor().execute_outcome_in_context(invalid_pure_target, root));
	};
	assert(rejected(execute_stateful_target));

	const std::filesystem::path persistent_path =
	    std::filesystem::temp_directory_path() / "avm_semantic_primitives_test.links";
	std::filesystem::remove(persistent_path);

	std::optional<avm::BootstrapVocabulary> persistent_bootstrap;
	std::optional<avm::IntegerVocabulary> persistent_integers;
	std::optional<avm::ReferenceVocabulary> persistent_references;
	std::optional<avm::SemanticExecutionVocabulary> persistent_semantic;
	avm::SemanticContextFrame persistent_frame{};
	avm::LinkId persistent_root = avm::invalid_link_id;
	avm::LinkId persistent_result = avm::invalid_link_id;
	std::size_t persistent_converged_size = 0;

	{
		avm::PersistentLinkStore persistent_store(persistent_path);
		avm::BootstrapRuntime persistent_runtime(persistent_store);
		persistent_bootstrap = persistent_runtime.vocabulary();
		persistent_integers = avm::IntegerVocabulary::create(persistent_store);
		persistent_references = avm::ReferenceVocabulary::create(persistent_store);
		persistent_semantic = avm::SemanticExecutionVocabulary::create(persistent_store);
		avm::register_integer_arithmetic(persistent_runtime.executor(), *persistent_integers);
		avm::register_semantic_execution_primitives(persistent_runtime.executor(), *persistent_semantic,
		                                            *persistent_references, persistent_bootstrap->unit);

		persistent_frame = avm::SemanticContextFrame{
		    persistent_store.create_point(),
		    avm::realize_integer(persistent_store, *persistent_integers, 0),
		    persistent_store.create_point(),
		    persistent_store.create_point(),
		};
		const SemanticProgram persistent_program = build_composition(
		    persistent_store, persistent_runtime, *persistent_integers, *persistent_references, *persistent_semantic);
		persistent_root = persistent_program.root;
		const avm::ExecutionOutcome outcome = persistent_runtime.executor().execute_outcome_in_context(
		    persistent_root, avm::SemanticContextView::root(persistent_frame));
		persistent_result = outcome.result;
		assert(avm::decode_integer(persistent_store, *persistent_integers, persistent_result) == 5);
		assert(outcome.semantic.role(avm::SemanticContextRole::RelationState) == persistent_result);
		persistent_converged_size = persistent_store.size();
	}
	{
		avm::PersistentLinkStore reopened(persistent_path);
		avm::BootstrapRuntime reopened_runtime(reopened, *persistent_bootstrap);
		avm::register_integer_arithmetic(reopened_runtime.executor(), *persistent_integers);
		avm::register_semantic_execution_primitives(reopened_runtime.executor(), *persistent_semantic,
		                                            *persistent_references, persistent_bootstrap->unit);
		assert(reopened.size() == persistent_converged_size);
		const avm::ExecutionOutcome outcome = reopened_runtime.executor().execute_outcome_in_context(
		    persistent_root, avm::SemanticContextView::root(persistent_frame));
		assert(outcome.result == persistent_result);
		assert(outcome.semantic.role(avm::SemanticContextRole::RelationState) == persistent_result);
		assert(avm::decode_integer(reopened, *persistent_integers, outcome.result) == 5);
		assert(reopened.size() == persistent_converged_size);
	}
	std::filesystem::remove(persistent_path);

	return 0;
}
