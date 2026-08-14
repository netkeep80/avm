#include "json_duplet_values.h"
#include "jsonrvm_semantic_migrator.h"

#include "avm/bootstrap_runtime.h"
#include "avm/integer_value.h"
#include "avm/persistent_link_store.h"
#include "avm/projection.h"
#include "avm/reference.h"
#include "avm/semantic_primitives.h"
#include "avm/text_value.h"

#include "nlohmann/json.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{

using Json = nlohmann::ordered_json;

std::filesystem::path temporary_path()
{
	const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
	return std::filesystem::temp_directory_path() / ("avm-15-release-" + std::to_string(nonce) + ".bin");
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

Json load_json(const char *path)
{
	std::ifstream stream(path);
	if (!stream)
		throw std::runtime_error(std::string("cannot open frozen composition fixture: ") + path);
	Json value;
	stream >> value;
	return value;
}

struct PersistedReleaseState
{
	avm::BootstrapVocabulary bootstrap;
	avm::IntegerVocabulary integers;
	avm::ReferenceVocabulary references;
	avm::SemanticExecutionVocabulary semantic;
	avm::LinkId current_relation_state_reference = avm::invalid_link_id;
	avm::LinkId program_root = avm::invalid_link_id;
	avm::SemanticContextFrame initial_frame{};
};

avm::json_duplet::SymbolAnchors release_symbols(const PersistedReleaseState &state)
{
	avm::json_duplet::SymbolAnchors symbols;
	symbols.emplace("integer_add", state.integers.add_relation);
	symbols.emplace("integer_subtract", state.integers.subtract_relation);
	symbols.emplace("integer_multiply", state.integers.multiply_relation);
	symbols.emplace("integer_divide", state.integers.divide_relation);
	symbols.emplace("bootstrap_unit", state.bootstrap.unit);
	symbols.emplace("bootstrap_nil", state.bootstrap.nil);
	symbols.emplace("bootstrap_quote", state.bootstrap.quote_relation);
	symbols.emplace("bootstrap_sequence", state.bootstrap.sequence_relation);
	symbols.emplace("semantic_commit_relation_state", state.semantic.commit_relation_state);
	symbols.emplace("semantic_resolve_reference", state.semantic.resolve_reference_relation);
	symbols.emplace("semantic_apply_pure_relation", state.semantic.apply_pure_relation);
	symbols.emplace("current_relation_state_reference", state.current_relation_state_reference);
	return symbols;
}

void register_release_handlers(avm::BootstrapRuntime &runtime, const PersistedReleaseState &state)
{
	avm::LinkStore &store = runtime.executor().store();
	avm::validate_integer_vocabulary(store, state.integers);
	avm::validate_reference_vocabulary(store, state.references);
	avm::validate_semantic_execution_vocabulary(store, state.semantic);
	avm::register_integer_arithmetic(runtime.executor(), state.integers);
	avm::register_semantic_execution_primitives(runtime.executor(), state.semantic, state.references,
	                                           state.bootstrap.unit);
}

avm::SemanticContextView initial_context(const PersistedReleaseState &state)
{
	return avm::SemanticContextView::root(state.initial_frame);
}

void assert_composition_outcome(avm::LinkStore &store, avm::BootstrapRuntime &runtime,
                                const PersistedReleaseState &state)
{
	const avm::SemanticContextView input = initial_context(state);
	const std::size_t before_execution = store.size();
	const avm::ExecutionOutcome outcome = runtime.executor().execute_outcome_in_context(state.program_root, input);
	assert(avm::decode_integer(store, state.integers, outcome.result) == 5);
	assert(outcome.semantic.role(avm::SemanticContextRole::RelationState) == outcome.result);
	assert(input.role(avm::SemanticContextRole::RelationState) == state.integers.zero);
	assert(store.size() == before_execution);
}

PersistedReleaseState import_once(avm::PersistentLinkStore &store, const char *fixture_path)
{
	PersistedReleaseState state;
	avm::BootstrapRuntime runtime(store);
	state.bootstrap = runtime.vocabulary();
	state.integers = avm::IntegerVocabulary::create(store);
	const avm::TextVocabulary text = avm::TextVocabulary::create(store);
	state.references = avm::ReferenceVocabulary::create(store);
	state.semantic = avm::SemanticExecutionVocabulary::create(store);
	state.current_relation_state_reference =
	    avm::realize_context_role_reference(store, state.references, avm::ReferenceRole::RelationState);
	state.initial_frame = avm::SemanticContextFrame{
	    store.create_point(),
	    state.integers.zero,
	    store.create_point(),
	    store.create_point(),
	};
	register_release_handlers(runtime, state);

	{
		Json legacy = load_json(fixture_path);
		const auto migration = avm::jsonrvm_migration::migrate_program(legacy);
		legacy.clear();
		assert(migration.observable_json_pointer == "/result");
		assert(migration.document.at("$avm") == "duplet-json/1");

		const avm::json_duplet::NativeLeafResolver resolver(state.integers, text, release_symbols(state));
		const std::size_t before_projection = store.size();
		const avm::ProjectionDescription projection =
		    avm::json_duplet::project_duplet_document(migration.document, resolver);
		assert(store.size() == before_projection);
		assert(!avm::find_projection(store, projection).has_value());
		assert(store.size() == before_projection);

		state.program_root = avm::realize_projection(store, projection).root;
		assert(store.contains(state.program_root));
	}

	const avm::SemanticContextView input = initial_context(state);
	const avm::ExecutionOutcome first = runtime.executor().execute_outcome_in_context(state.program_root, input);
	assert(avm::decode_integer(store, state.integers, first.result) == 5);
	assert(first.semantic.role(avm::SemanticContextRole::RelationState) == first.result);
	assert(input.role(avm::SemanticContextRole::RelationState) == state.integers.zero);

	const std::size_t converged_size = store.size();
	assert_composition_outcome(store, runtime, state);
	assert(store.size() == converged_size);
	return state;
}

void assert_reopen_executes_without_remigration(avm::PersistentLinkStore &store, const PersistedReleaseState &state)
{
	const std::size_t before_restore = store.size();
	avm::BootstrapRuntime runtime(store, state.bootstrap);
	assert(store.size() == before_restore);
	register_release_handlers(runtime, state);
	assert(store.size() == before_restore);

	assert(store.contains(state.program_root));
	assert(store.contains(state.current_relation_state_reference));
	const auto current_relation =
	    avm::find_context_role_reference(store, state.references, avm::ReferenceRole::RelationState);
	assert(current_relation.has_value());
	assert(*current_relation == state.current_relation_state_reference);
	assert(store.size() == before_restore);

	assert_composition_outcome(store, runtime, state);
	assert(store.size() == before_restore);
	assert_composition_outcome(store, runtime, state);
	assert(store.size() == before_restore);
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 2)
		throw std::runtime_error("expected frozen pure-relation-composition fixture path");

	const std::filesystem::path path = temporary_path();
	FileCleanup cleanup{path};
	PersistedReleaseState state{};

	{
		avm::PersistentLinkStore store(path);
		state = import_once(store, argv[1]);
	}

	// Reopen-фаза намеренно получает только canonical LinkIds одного logical store: source JSON и projection уже уничтожены.
	{
		avm::PersistentLinkStore reopened(path);
		assert_reopen_executes_without_remigration(reopened, state);
	}

	{
		avm::PersistentLinkStore reopened_again(path);
		assert_reopen_executes_without_remigration(reopened_again, state);
	}

	return 0;
}
