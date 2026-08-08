#include "avm/bootstrap_runtime.h"
#include "avm/execution_trace.h"
#include "avm/persistent_link_store.h"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{

std::filesystem::path temporary_path()
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("avm-trace-reopen-" + std::to_string(nonce) + ".bin");
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

std::vector<avm::ExecutionEvent> copy_complete_trace(const avm::BoundedExecutionTrace &trace)
{
	if (trace.truncated())
		throw std::invalid_argument("cannot compare a truncated execution trace");
	return std::vector<avm::ExecutionEvent>(trace.events().begin(), trace.events().end());
}

struct NormalizedExecutionEvent
{
	avm::ExecutionEventKind kind;
	std::size_t entity;
	std::size_t relation;
	std::size_t subject;
	std::size_t object;
	std::optional<std::size_t> parent;
	std::optional<std::size_t> frame;
	std::optional<std::size_t> result;
	std::optional<avm::ExecutionFailurePhase> failure_phase;

	bool operator==(const NormalizedExecutionEvent &) const = default;
};

std::vector<NormalizedExecutionEvent> normalize_trace(const avm::BoundedExecutionTrace &trace)
{
	if (trace.truncated())
		throw std::invalid_argument("cannot normalize a truncated execution trace");

	std::map<avm::LinkId, std::size_t> ordinals;
	std::size_t next_ordinal = 0;
	const auto ordinal = [&ordinals, &next_ordinal](avm::LinkId id) mutable
	{
		const auto [it, inserted] = ordinals.emplace(id, next_ordinal);
		if (inserted)
			++next_ordinal;
		return it->second;
	};
	const auto optional_ordinal = [&ordinal](std::optional<avm::LinkId> id) mutable -> std::optional<std::size_t>
	{
		if (!id)
			return std::nullopt;
		return ordinal(*id);
	};

	std::vector<NormalizedExecutionEvent> normalized;
	normalized.reserve(trace.size());
	for (const avm::ExecutionEvent &event : trace.events())
	{
		normalized.push_back(NormalizedExecutionEvent{
		    event.kind,
		    ordinal(event.context.entity),
		    ordinal(event.context.relation),
		    ordinal(event.context.subject),
		    ordinal(event.context.object),
		    optional_ordinal(event.context.parent),
		    optional_ordinal(event.context.frame),
		    optional_ordinal(event.result),
		    event.failure_phase,
		});
	}
	return normalized;
}

struct ScenarioProgram
{
	avm::BootstrapVocabulary vocabulary;
	avm::LinkId boolean_root;
	avm::LinkId call_root;
	avm::LinkId failure_root;
	avm::LinkId expected_result;
};

ScenarioProgram build_scenario(avm::LinkStore &store, avm::BootstrapRuntime &runtime)
{
	avm::ProgramBuilder builder = runtime.builder();
	const avm::BootstrapVocabulary vocabulary = runtime.vocabulary();

	const avm::LinkId true_literal = builder.literal(vocabulary.true_value);
	const avm::LinkId false_literal = builder.literal(vocabulary.false_value);
	const avm::LinkId negated = builder.logical_not(false_literal);
	const avm::LinkId boolean_root = builder.logical_and(true_literal, negated);

	const avm::LinkId formal = store.create_point();
	const avm::LinkId function = builder.create_function_handle();
	builder.define_function(function, {formal}, builder.parameter(formal));
	const avm::LinkId call_root = builder.call(function, {true_literal});

	const avm::LinkId unknown_relation = store.create_point();
	const avm::LinkId failure_subject = store.create_point();
	const avm::LinkId failure_object = store.create_point();
	const avm::LinkId failure_root =
	    avm::encode_relation_entity(store, {unknown_relation, failure_subject, failure_object});

	return ScenarioProgram{vocabulary, boolean_root, call_root, failure_root, vocabulary.true_value};
}

std::vector<avm::ExecutionEvent> capture_success(avm::BootstrapRuntime &runtime, avm::LinkStore &store,
                                                avm::LinkId root, avm::LinkId expected)
{
	avm::BoundedExecutionTrace trace(128);
	runtime.executor().set_observer(&trace);
	const std::size_t size_before = store.size();
	assert(runtime.execute(root) == expected);
	assert(store.size() == size_before);
	runtime.executor().set_observer(nullptr);
	return copy_complete_trace(trace);
}

std::vector<avm::ExecutionEvent> capture_failure(avm::BootstrapRuntime &runtime, avm::LinkStore &store,
                                                avm::LinkId root)
{
	avm::BoundedExecutionTrace trace(128);
	runtime.executor().set_observer(&trace);
	const std::size_t size_before = store.size();
	bool rejected = false;
	try
	{
		static_cast<void>(runtime.execute(root));
	}
	catch (const std::runtime_error &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(store.size() == size_before);
	runtime.executor().set_observer(nullptr);
	const std::vector<avm::ExecutionEvent> events = copy_complete_trace(trace);
	assert(!events.empty());
	assert(events.back().kind == avm::ExecutionEventKind::Fail);
	assert(events.back().failure_phase == avm::ExecutionFailurePhase::Dispatch);
	return events;
}

struct PersistentBaseline
{
	ScenarioProgram program;
	std::vector<avm::ExecutionEvent> boolean_trace;
	std::vector<avm::ExecutionEvent> call_trace;
	std::vector<avm::ExecutionEvent> failure_trace;
	std::size_t store_size;
};

PersistentBaseline create_persistent_baseline(const std::filesystem::path &path)
{
	avm::PersistentLinkStore store(path);
	avm::BootstrapRuntime runtime(store);
	const ScenarioProgram program = build_scenario(store, runtime);

	static_cast<void>(runtime.execute(program.call_root));
	const std::size_t converged_size = store.size();
	assert(runtime.execute(program.call_root) == program.expected_result);
	assert(store.size() == converged_size);

	const std::vector<avm::ExecutionEvent> boolean_trace =
	    capture_success(runtime, store, program.boolean_root, program.expected_result);
	const std::vector<avm::ExecutionEvent> call_trace =
	    capture_success(runtime, store, program.call_root, program.expected_result);
	const std::vector<avm::ExecutionEvent> failure_trace = capture_failure(runtime, store, program.failure_root);

	return PersistentBaseline{program, boolean_trace, call_trace, failure_trace, store.size()};
}

void assert_persistent_reopen_matches(const std::filesystem::path &path, const PersistentBaseline &baseline)
{
	avm::PersistentLinkStore store(path);
	assert(store.size() == baseline.store_size);
	avm::BootstrapRuntime runtime(store, baseline.program.vocabulary);
	assert(store.size() == baseline.store_size);

	assert(capture_success(runtime, store, baseline.program.boolean_root, baseline.program.expected_result) ==
	       baseline.boolean_trace);
	assert(capture_success(runtime, store, baseline.program.call_root, baseline.program.expected_result) ==
	       baseline.call_trace);
	assert(capture_failure(runtime, store, baseline.program.failure_root) == baseline.failure_trace);
	assert(store.size() == baseline.store_size);
}

void verify_persistent_trace_identity_survives_multiple_reopens()
{
	const std::filesystem::path path = temporary_path();
	FileCleanup cleanup{path};
	const PersistentBaseline baseline = create_persistent_baseline(path);
	assert_persistent_reopen_matches(path, baseline);
	assert_persistent_reopen_matches(path, baseline);
}

struct NormalizedScenario
{
	std::vector<NormalizedExecutionEvent> boolean_trace;
	std::vector<NormalizedExecutionEvent> call_trace;
	std::vector<NormalizedExecutionEvent> failure_trace;

	bool operator==(const NormalizedScenario &) const = default;
};

NormalizedScenario capture_normalized_scenario(avm::LinkStore &store)
{
	avm::BootstrapRuntime runtime(store);
	const ScenarioProgram program = build_scenario(store, runtime);

	static_cast<void>(runtime.execute(program.call_root));
	const std::size_t converged_size = store.size();

	avm::BoundedExecutionTrace boolean_trace(128);
	runtime.executor().set_observer(&boolean_trace);
	assert(runtime.execute(program.boolean_root) == program.expected_result);
	assert(store.size() == converged_size);

	avm::BoundedExecutionTrace call_trace(128);
	runtime.executor().set_observer(&call_trace);
	assert(runtime.execute(program.call_root) == program.expected_result);
	assert(store.size() == converged_size);

	avm::BoundedExecutionTrace failure_trace(128);
	runtime.executor().set_observer(&failure_trace);
	bool rejected = false;
	try
	{
		static_cast<void>(runtime.execute(program.failure_root));
	}
	catch (const std::runtime_error &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(store.size() == converged_size);
	runtime.executor().set_observer(nullptr);

	return NormalizedScenario{
	    normalize_trace(boolean_trace),
	    normalize_trace(call_trace),
	    normalize_trace(failure_trace),
	};
}

void verify_independent_backends_match_modulo_opaque_link_id_renaming()
{
	avm::InMemoryLinkStore memory_store;
	const NormalizedScenario memory = capture_normalized_scenario(memory_store);

	const std::filesystem::path path = temporary_path();
	FileCleanup cleanup{path};
	avm::PersistentLinkStore persistent_store(path);
	const NormalizedScenario persistent = capture_normalized_scenario(persistent_store);

	assert(memory == persistent);
}

void verify_normalization_rejects_truncated_trace()
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const ScenarioProgram program = build_scenario(store, runtime);
	avm::BoundedExecutionTrace trace(1);
	runtime.executor().set_observer(&trace);
	static_cast<void>(runtime.execute(program.boolean_root));
	assert(trace.truncated());

	bool rejected = false;
	try
	{
		static_cast<void>(normalize_trace(trace));
	}
	catch (const std::invalid_argument &)
	{
		rejected = true;
	}
	assert(rejected);
}

} // namespace

int main()
{
	verify_persistent_trace_identity_survives_multiple_reopens();
	verify_independent_backends_match_modulo_opaque_link_id_renaming();
	verify_normalization_rejects_truncated_trace();
	return 0;
}
