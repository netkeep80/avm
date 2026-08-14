#include "json_duplet_values.h"
#include "jsonrvm_semantic_migrator.h"

#include "avm/bootstrap_runtime.h"
#include "avm/integer_value.h"
#include "avm/projection.h"
#include "avm/reference.h"
#include "avm/semantic_primitives.h"
#include "avm/text_value.h"

#include "nlohmann/json.hpp"

#include <cassert>
#include <fstream>
#include <stdexcept>
#include <string>

namespace
{

using Json = nlohmann::ordered_json;

Json load_json(const char *path)
{
	std::ifstream stream(path);
	if (!stream)
		throw std::runtime_error(std::string("cannot open fixture: ") + path);
	Json value;
	stream >> value;
	return value;
}

Json named_reference_fixture(const std::string &name)
{
	Json reference = Json::object();
	reference["$ref"] = name;
	Json fixture = Json::object();
	fixture["$rel/result"] = std::move(reference);
	return fixture;
}

bool realization_rejected(avm::LinkStore &store, const avm::ProjectionDescription &description)
{
	try
	{
		static_cast<void>(avm::realize_projection(store, description));
		return false;
	}
	catch (const std::invalid_argument &)
	{
		return true;
	}
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 2)
		throw std::runtime_error("expected frozen missing-reference fixture path");

	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const avm::IntegerVocabulary integers = avm::IntegerVocabulary::create(store);
	const avm::TextVocabulary text = avm::TextVocabulary::create(store);
	const avm::ReferenceVocabulary references = avm::ReferenceVocabulary::create(store);
	const avm::SemanticExecutionVocabulary semantic = avm::SemanticExecutionVocabulary::create(store);
	avm::register_semantic_execution_primitives(runtime.executor(), semantic, references, runtime.vocabulary().unit);

	avm::json_duplet::SymbolAnchors symbols;
	symbols.emplace("bootstrap_unit", runtime.vocabulary().unit);
	symbols.emplace("semantic_resolve_reference", semantic.resolve_reference_relation);
	symbols.emplace("reference_named", references.named_reference);
	const avm::json_duplet::NativeLeafResolver resolver(integers, text, symbols);

	Json frozen_missing = load_json(argv[1]);
	assert(frozen_missing.is_object());
	assert(frozen_missing.size() == 1);
	assert(frozen_missing.contains("$rel/result"));
	assert(frozen_missing.at("$rel/result").is_object());
	assert(frozen_missing.at("$rel/result").size() == 1);
	assert(frozen_missing.at("$rel/result").at("$ref") == "__avm_missing_reference_oracle__");

	const std::size_t before_missing_migration = store.size();
	bool unresolved_rejected = false;
	try
	{
		static_cast<void>(avm::jsonrvm_migration::migrate_program(frozen_missing));
	}
	catch (const avm::jsonrvm_migration::MigrationError &error)
	{
		unresolved_rejected = true;
		assert(error.kind() == avm::jsonrvm_migration::MigrationFailureKind::UnresolvedReference);
		assert(error.source_path() == "$.$rel/result.$ref");
		assert(error.source_identity() == "__avm_missing_reference_oracle__");
		assert(std::string(error.what()).find(error.source_identity()) != std::string::npos);
	}
	assert(unresolved_rejected);
	assert(store.size() == before_missing_migration);

	const avm::LinkId known_target = store.create_point();
	avm::jsonrvm_migration::LegacyNameBindings names;
	names.emplace("known-target", known_target);
	Json known_source = named_reference_fixture("known-target");
	const auto known_migration = avm::jsonrvm_migration::migrate_program(known_source, names);
	assert(known_migration.observable_json_pointer == "/result");
	assert(known_migration.document.at("$avm") == "duplet-json/1");
	known_source.clear();

	const std::size_t before_known_projection = store.size();
	const avm::ProjectionDescription known_description =
	    avm::json_duplet::project_duplet_document(known_migration.document, resolver);
	assert(store.size() == before_known_projection);
	static_cast<void>(avm::find_projection(store, known_description));
	assert(store.size() == before_known_projection);

	const avm::LinkId known_program = avm::realize_projection(store, known_description).root;
	const avm::RelationEntity known_execution = avm::decode_relation_entity(store, known_program);
	assert(known_execution.relation == semantic.resolve_reference_relation);
	assert(known_execution.subject == runtime.vocabulary().unit);
	const avm::Link named_reference = store.get(known_execution.object);
	assert(named_reference.begin == references.named_reference);
	assert(named_reference.end == known_target);

	const avm::SemanticContextView semantic_root = avm::SemanticContextView::root(avm::SemanticContextFrame{
	    known_target,
	    known_target,
	    known_target,
	    known_target,
	});
	const std::size_t converged_size = store.size();
	const avm::ExecutionOutcome known_outcome =
	    runtime.executor().execute_outcome_in_context(known_program, semantic_root);
	assert(known_outcome.result == known_target);
	assert(known_outcome.semantic == semantic_root);
	assert(store.size() == converged_size);
	const avm::ExecutionOutcome repeated_known =
	    runtime.executor().execute_outcome_in_context(known_program, semantic_root);
	assert(repeated_known == known_outcome);
	assert(store.size() == converged_size);

	const avm::LinkId absent_target = known_target + 1000000;
	avm::jsonrvm_migration::LegacyNameBindings absent_names;
	absent_names.emplace("known-but-absent", absent_target);
	const auto absent_migration =
	    avm::jsonrvm_migration::migrate_program(named_reference_fixture("known-but-absent"), absent_names);
	const std::size_t before_absent_projection = store.size();
	const avm::ProjectionDescription absent_description =
	    avm::json_duplet::project_duplet_document(absent_migration.document, resolver);
	assert(store.size() == before_absent_projection);
	assert(!avm::find_projection(store, absent_description));
	assert(store.size() == before_absent_projection);
	assert(realization_rejected(store, absent_description));
	assert(store.size() == before_absent_projection);

	Json malformed = named_reference_fixture("known-target");
	malformed["$rel/result"]["extra"] = true;
	bool malformed_rejected = false;
	try
	{
		static_cast<void>(avm::jsonrvm_migration::migrate_program(malformed, names));
	}
	catch (const avm::jsonrvm_migration::MigrationError &error)
	{
		malformed_rejected = true;
		assert(error.kind() == avm::jsonrvm_migration::MigrationFailureKind::UnsupportedSource);
		assert(error.source_path().empty());
		assert(error.source_identity().empty());
	}
	assert(malformed_rejected);

	return 0;
}
