#include "json_duplet_values.h"
#include "jsonrvm_semantic_migrator.h"

#include "avm/bootstrap_runtime.h"
#include "avm/integer_value.h"
#include "avm/projection.h"
#include "avm/reference.h"
#include "avm/relations_model.h"
#include "avm/semantic_primitives.h"
#include "avm/text_value.h"

#include "nlohmann/json.hpp"

#include <cassert>
#include <cstdint>
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

Json arithmetic_fixture(const char *operation, Json subject, Json object)
{
	Json relation = Json::object();
	relation["$rel"] = operation;
	relation["$sub"] = std::move(subject);
	relation["$obj"] = std::move(object);

	Json fixture = Json::object();
	fixture["$rel/result"] = std::move(relation);
	return fixture;
}

Json sequence_fixture(Json sequence)
{
	Json fixture = Json::object();
	fixture["$rel/result"] = std::move(sequence);
	return fixture;
}

bool migration_rejected(const Json &legacy)
{
	try
	{
		static_cast<void>(avm::jsonrvm_migration::migrate_program(legacy));
		return false;
	}
	catch (const avm::jsonrvm_migration::MigrationError &)
	{
		return true;
	}
}

avm::LinkId project_and_realize(avm::LinkStore &store, const avm::json_duplet::NativeLeafResolver &resolver,
                                const Json &document)
{
	const std::size_t before_projection = store.size();
	const avm::ProjectionDescription description = avm::json_duplet::project_duplet_document(document, resolver);
	assert(store.size() == before_projection);
	static_cast<void>(avm::find_projection(store, description));
	assert(store.size() == before_projection);
	return avm::realize_projection(store, description).root;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 4)
		throw std::runtime_error("expected arithmetic, sequence-order and pure-composition fixture paths");

	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const avm::IntegerVocabulary integers = avm::IntegerVocabulary::create(store);
	const avm::TextVocabulary text = avm::TextVocabulary::create(store);
	const avm::ReferenceVocabulary references = avm::ReferenceVocabulary::create(store);
	const avm::SemanticExecutionVocabulary semantic = avm::SemanticExecutionVocabulary::create(store);
	avm::register_integer_arithmetic(runtime.executor(), integers);
	avm::register_semantic_execution_primitives(
	    runtime.executor(), semantic, references, runtime.vocabulary().unit);

	const avm::LinkId current_relation_state_reference =
	    avm::realize_context_role_reference(store, references, avm::ReferenceRole::RelationState);

	avm::json_duplet::SymbolAnchors symbols;
	symbols.emplace("integer_add", integers.add_relation);
	symbols.emplace("integer_subtract", integers.subtract_relation);
	symbols.emplace("integer_multiply", integers.multiply_relation);
	symbols.emplace("integer_divide", integers.divide_relation);
	symbols.emplace("bootstrap_unit", runtime.vocabulary().unit);
	symbols.emplace("bootstrap_nil", runtime.vocabulary().nil);
	symbols.emplace("bootstrap_quote", runtime.vocabulary().quote_relation);
	symbols.emplace("bootstrap_sequence", runtime.vocabulary().sequence_relation);
	symbols.emplace("semantic_commit_relation_state", semantic.commit_relation_state);
	symbols.emplace("semantic_resolve_reference", semantic.resolve_reference_relation);
	symbols.emplace("semantic_apply_pure_relation", semantic.apply_pure_relation);
	symbols.emplace("current_relation_state_reference", current_relation_state_reference);
	const avm::json_duplet::NativeLeafResolver resolver(integers, text, symbols);

	Json frozen_arithmetic = load_json(argv[1]);
	const avm::jsonrvm_migration::MigrationResult<Json> arithmetic_migration =
	    avm::jsonrvm_migration::migrate_program(frozen_arithmetic);
	assert(arithmetic_migration.observable_json_pointer == "/result");
	assert(arithmetic_migration.document.at("$avm") == "duplet-json/1");
	frozen_arithmetic.clear();

	const avm::LinkId arithmetic_program = project_and_realize(store, resolver, arithmetic_migration.document);
	const avm::RelationEntity decoded = avm::decode_relation_entity(store, arithmetic_program);
	assert(decoded.relation == integers.add_relation);
	assert(avm::decode_integer(store, integers, decoded.subject) == 1);
	assert(avm::decode_integer(store, integers, decoded.object) == 1);
	const avm::LinkId arithmetic_result = runtime.executor().execute(arithmetic_program);
	assert(avm::decode_integer(store, integers, arithmetic_result) == 2);

	const Json operations[] = {
	    arithmetic_fixture("+", 7, 3),
	    arithmetic_fixture("-", 9, 4),
	    arithmetic_fixture("*", 6, 7),
	    arithmetic_fixture("/", -7, 2),
	};
	const std::int64_t expected[] = {10, 5, 42, -3};

	for (std::size_t index = 0; index < 4; ++index)
	{
		const auto migrated = avm::jsonrvm_migration::migrate_program(operations[index]);
		const avm::LinkId operation_program = project_and_realize(store, resolver, migrated.document);
		const avm::LinkId operation_result = runtime.executor().execute(operation_program);
		assert(avm::decode_integer(store, integers, operation_result) == expected[index]);
	}

	const avm::LinkId semantic_entity = store.create_point();
	const avm::LinkId semantic_subject = store.create_point();
	const avm::LinkId semantic_object = store.create_point();
	const avm::LinkId zero = avm::realize_integer(store, integers, 0);
	const avm::SemanticContextView initial = avm::SemanticContextView::root(avm::SemanticContextFrame{
	    semantic_entity,
	    zero,
	    semantic_subject,
	    semantic_object,
	});

	Json frozen_sequence = load_json(argv[2]);
	const auto sequence_migration = avm::jsonrvm_migration::migrate_program(frozen_sequence);
	assert(sequence_migration.observable_json_pointer == "/result");
	assert(sequence_migration.document.at("$avm") == "duplet-json/1");
	frozen_sequence.clear();
	const avm::LinkId sequence_program = project_and_realize(store, resolver, sequence_migration.document);
	const avm::ExecutionOutcome sequence_outcome = runtime.executor().execute_outcome_in_context(sequence_program, initial);
	assert(avm::decode_integer(store, integers, sequence_outcome.result) == 3);
	assert(sequence_outcome.semantic.role(avm::SemanticContextRole::RelationState) == sequence_outcome.result);
	assert(initial.role(avm::SemanticContextRole::RelationState) == zero);

	Json frozen_composition = load_json(argv[3]);
	const auto composition_migration = avm::jsonrvm_migration::migrate_program(frozen_composition);
	assert(composition_migration.observable_json_pointer == "/result");
	assert(composition_migration.document.at("$avm") == "duplet-json/1");
	frozen_composition.clear();
	const avm::LinkId composition_program = project_and_realize(store, resolver, composition_migration.document);
	const avm::ExecutionOutcome composition_outcome =
	    runtime.executor().execute_outcome_in_context(composition_program, initial);
	assert(avm::decode_integer(store, integers, composition_outcome.result) == 5);
	assert(composition_outcome.semantic.role(avm::SemanticContextRole::RelationState) == composition_outcome.result);
	assert(initial.role(avm::SemanticContextRole::RelationState) == zero);

	const std::size_t converged_size = store.size();
	const avm::ExecutionOutcome repeated = runtime.executor().execute_outcome_in_context(composition_program, initial);
	assert(repeated.result == composition_outcome.result);
	assert(repeated.semantic == composition_outcome.semantic);
	assert(store.size() == converged_size);

	assert(migration_rejected(Json::array()));
	assert(migration_rejected(Json::object()));
	assert(migration_rejected(arithmetic_fixture("%", 7, 3)));
	assert(migration_rejected(arithmetic_fixture("+", "7", 3)));
	assert(migration_rejected(arithmetic_fixture("+", 7, 3.5)));

	Json incomplete = Json::object();
	incomplete["$rel"] = "+";
	incomplete["$sub"] = 1;
	Json incomplete_fixture = Json::object();
	incomplete_fixture["$rel/result"] = std::move(incomplete);
	assert(migration_rejected(incomplete_fixture));

	Json foreign = arithmetic_fixture("+", 1, 1);
	foreign["extra"] = true;
	assert(migration_rejected(foreign));

	Json empty_sequence = Json::array();
	assert(migration_rejected(sequence_fixture(std::move(empty_sequence))));

	Json unsupported_reference = Json::object();
	unsupported_reference["$ref"] = "$sub";
	Json unsupported_relation = arithmetic_fixture("+", std::move(unsupported_reference), 3);
	Json unsupported_sequence = Json::array();
	unsupported_sequence.push_back(std::move(unsupported_relation.at("$rel/result")));
	assert(migration_rejected(sequence_fixture(std::move(unsupported_sequence))));

	return 0;
}
