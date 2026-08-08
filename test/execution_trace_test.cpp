#include "avm/bootstrap_runtime.h"
#include "avm/execution_trace.h"

#include <array>
#include <cassert>
#include <stdexcept>

namespace
{

struct NestedProgram
{
	avm::LinkId root;
	avm::LinkId true_literal;
	avm::LinkId negated;
};

NestedProgram build_nested_program(avm::BootstrapRuntime &runtime)
{
	avm::ProgramBuilder builder = runtime.builder();
	const avm::BootstrapVocabulary &vocabulary = runtime.vocabulary();
	const avm::LinkId true_literal = builder.literal(vocabulary.true_value);
	const avm::LinkId false_literal = builder.literal(vocabulary.false_value);
	const avm::LinkId negated = builder.logical_not(false_literal);
	return NestedProgram{builder.logical_and(true_literal, negated), true_literal, negated};
}

void verify_exact_capacity_trace_is_complete()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const NestedProgram program = build_nested_program(runtime);
	avm::BoundedExecutionTrace trace(8);
	runtime.executor().set_observer(&trace);

	const std::size_t size_before = store.size();
	assert(runtime.execute(program.root) == runtime.vocabulary().true_value);
	assert(store.size() == size_before);
	assert(trace.max_events() == 8);
	assert(trace.size() == 8);
	assert(!trace.empty());
	assert(trace.complete());
	assert(!trace.truncated());
	assert(trace.events().front().kind == avm::ExecutionEventKind::Enter);
	assert(trace.events().front().context.entity == program.root);
	assert(trace.events().back().kind == avm::ExecutionEventKind::Return);
	assert(trace.events().back().context.entity == program.root);
	assert(trace.events().back().result == runtime.vocabulary().true_value);
}

void verify_short_capacity_retains_prefix_and_marks_truncation()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const NestedProgram program = build_nested_program(runtime);

	avm::BoundedExecutionTrace full(8);
	runtime.executor().set_observer(&full);
	assert(runtime.execute(program.root) == runtime.vocabulary().true_value);
	assert(full.complete());

	avm::BoundedExecutionTrace short_trace(3);
	runtime.executor().set_observer(&short_trace);
	assert(runtime.execute(program.root) == runtime.vocabulary().true_value);
	assert(short_trace.size() == 3);
	assert(short_trace.truncated());
	assert(!short_trace.complete());
	for (std::size_t i = 0; i < short_trace.size(); ++i)
		assert(short_trace.events()[i] == full.events()[i]);
}

void verify_zero_capacity_records_no_events_and_marks_truncation()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const NestedProgram program = build_nested_program(runtime);
	avm::BoundedExecutionTrace trace(0);
	runtime.executor().set_observer(&trace);

	assert(runtime.execute(program.root) == runtime.vocabulary().true_value);
	assert(trace.max_events() == 0);
	assert(trace.empty());
	assert(trace.events().empty());
	assert(trace.truncated());
	assert(!trace.complete());
}

void verify_reset_preserves_capacity_and_reuses_storage_policy()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const NestedProgram program = build_nested_program(runtime);
	avm::BoundedExecutionTrace trace(4);
	runtime.executor().set_observer(&trace);

	assert(runtime.execute(program.root) == runtime.vocabulary().true_value);
	assert(trace.size() == 4);
	assert(trace.truncated());

	trace.reset();
	assert(trace.max_events() == 4);
	assert(trace.empty());
	assert(trace.complete());
	assert(!trace.truncated());

	assert(runtime.execute(program.true_literal) == runtime.vocabulary().true_value);
	assert(trace.size() == 2);
	assert(trace.complete());
	assert(trace.events()[0].context.entity == program.true_literal);
	assert(trace.events()[1].context.entity == program.true_literal);
}

void verify_failure_phase_is_retained_unchanged()
{
	avm::InMemoryLinkStore store;
	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();
	const avm::LinkId entity = avm::encode_relation_entity(store, {relation, subject, object});
	avm::BoundedExecutionTrace trace(2);
	avm::Executor executor(store, &trace);

	const std::size_t size_before = store.size();
	bool rejected = false;
	try
	{
		static_cast<void>(executor.execute(entity));
	}
	catch (const std::runtime_error &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(store.size() == size_before);
	assert(trace.complete());
	assert(trace.size() == 2);
	assert(trace.events()[0].kind == avm::ExecutionEventKind::Enter);
	assert(trace.events()[1].kind == avm::ExecutionEventKind::Fail);
	assert(trace.events()[1].failure_phase == avm::ExecutionFailurePhase::Dispatch);
}

struct EffectRun
{
	std::size_t growth;
	bool materialized;
};

EffectRun run_pair_effect(bool attach_trace)
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	avm::ProgramBuilder builder = runtime.builder();
	const avm::LinkId begin = store.create_point();
	const avm::LinkId end = store.create_point();
	const avm::LinkId expression = builder.pair_intern(builder.literal(begin), builder.literal(end));
	avm::BoundedExecutionTrace trace(16);
	if (attach_trace)
		runtime.executor().set_observer(&trace);

	const std::size_t size_before = store.size();
	const avm::LinkId result = runtime.execute(expression);
	const std::size_t growth = store.size() - size_before;
	const bool materialized = store.find(begin, end) == result;
	if (attach_trace)
	{
		assert(trace.complete());
		assert(!trace.empty());
	}
	return EffectRun{growth, materialized};
}

void verify_collector_does_not_change_program_effects()
{
	const EffectRun without_trace = run_pair_effect(false);
	const EffectRun with_trace = run_pair_effect(true);
	assert(without_trace.growth == 1);
	assert(with_trace.growth == without_trace.growth);
	assert(without_trace.materialized);
	assert(with_trace.materialized);
}

void verify_repeated_deterministic_execution_reuses_collector()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const NestedProgram program = build_nested_program(runtime);
	avm::BoundedExecutionTrace trace(8);
	runtime.executor().set_observer(&trace);

	assert(runtime.execute(program.root) == runtime.vocabulary().true_value);
	const std::array<avm::ExecutionEvent, 8> first{
	    trace.events()[0], trace.events()[1], trace.events()[2], trace.events()[3],
	    trace.events()[4], trace.events()[5], trace.events()[6], trace.events()[7],
	};

	trace.reset();
	assert(runtime.execute(program.root) == runtime.vocabulary().true_value);
	assert(trace.complete());
	assert(trace.size() == first.size());
	for (std::size_t i = 0; i < first.size(); ++i)
		assert(trace.events()[i] == first[i]);
}

} // namespace

int main()
{
	verify_exact_capacity_trace_is_complete();
	verify_short_capacity_retains_prefix_and_marks_truncation();
	verify_zero_capacity_records_no_events_and_marks_truncation();
	verify_reset_preserves_capacity_and_reuses_storage_policy();
	verify_failure_phase_is_retained_unchanged();
	verify_collector_does_not_change_program_effects();
	verify_repeated_deterministic_execution_reuses_collector();
	return 0;
}
