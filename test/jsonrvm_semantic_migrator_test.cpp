#include "json_duplet_values.h"
#include "jsonrvm_semantic_migrator.h"

#include "avm/bootstrap_runtime.h"
#include "avm/foreach_runtime.h"
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
#include <vector>

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

Json foreach_fixture(Json collection, const char *relation_name = "foreachobj", const char *reference = "$obj")
{
	Json item_reference = Json::object();
	item_reference["$ref"] = reference;

	Json body = Json::object();
	body["$obj"] = std::move(item_reference);
	body["$rel"] = "=";

	Json relation = Json::object();
	relation["$obj"] = std::move(collection);
	relation["$rel"] = relation_name;
	relation["$sub"] = std::move(body);

	Json fixture = Json::object();
	fixture["$rel/result"] = std::move(relation);
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

std::vector<std::int64_t> decode_integer_list(const avm::LinkStore &store, avm::LinkId list_nil, avm::LinkId head,
                                              const avm::IntegerVocabulary &integers)
{
	std::vector<std::int64_t> values;
	for (const avm::LinkId item : avm::decode_link_list(store, list_nil, head))
		values.push_back(avm::decode_integer(store, integers, item));
	return values;
}

} // namespace

int main(int argc, char **argv)
{
	if (argc != 5)
		throw std::runtime_error("expected arithmetic, sequence-order, pure-composition and foreach fixture paths");

	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const avm::IntegerVocabulary integers = avm::IntegerVocabulary::create(store);
	const avm::TextVocabulary text = avm::TextVocabulary::create(store);
	const avm::ReferenceVocabulary references = avm::ReferenceVocabulary::create(store);
	const avm::SemanticExecutionVocabulary semantic = avm::SemanticExecutionVocabulary::create(store);
	const avm::ForeachVocabulary foreach = avm::ForeachVocabulary::create(store);
	avm::register_integer_arithmetic(runtime.executor(), integers);
	avm::register_semantic_execution_primitives(runtime.executor(), semantic, references, runtime.vocabulary().unit);
	avm::register_foreach_runtime(runtime.executor(), foreach, runtime.vocabulary().nil);

	const avm::LinkId current_relation_state_reference =
	    avm::realize_context_role_reference(store, references, avm::ReferenceRole::RelationState);
	const avm::LinkId current_object_reference =
	    avm::realize_context_role_reference(store, references, avm::ReferenceRole::Object);

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
	symbols.emplace("current_object_reference", current_object_reference);
	symbols.emplace("foreach_object", foreach.object_relation);
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
	const avm::ExecutionOutcome sequence_outcome =
	    runtime.executor().execute_outcome_in_context(sequence_program, initial);
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

	const std::size_t composition_converged_size = store.size();
	const avm::ExecutionOutcome repeated_composition =
	    runtime.executor().execute_outcome_in_context(composition_program, initial);
	assert(repeated_composition.result == composition_outcome.result);
	assert(repeated_composition.semantic == composition_outcome.semantic);
	assert(store.size() == composition_converged_size);

	Json frozen_foreach = load_json(argv[4]);
	const auto foreach_migration = avm::jsonrvm_migration::migrate_program(frozen_foreach);
	assert(foreach_migration.observable_json_pointer == "/result");
	assert(foreach_migration.document.at("$avm") == "duplet-json/1");
	frozen_foreach.clear();
	const avm::LinkId foreach_program = project_and_realize(store, resolver, foreach_migration.document);
	const avm::RelationEntity foreach_entity = avm::decode_relation_entity(store, foreach_program);
	assert(foreach_entity.relation == foreach.object_relation);
	assert(decode_integer_list(store, runtime.vocabulary().nil, foreach_entity.object, integers) ==
	       std::vector<std::int64_t>({1, 2, 3}));

	const avm::RelationEntity foreach_body = avm::decode_relation_entity(store, foreach_entity.subject);
	assert(foreach_body.relation == semantic.resolve_reference_relation);
	assert(foreach_body.subject == runtime.vocabulary().unit);
	assert(foreach_body.object == current_object_reference);

	const std::size_t before_foreach_execution = store.size();
	const avm::ExecutionOutcome foreach_outcome =
	    runtime.executor().execute_outcome_in_context(foreach_program, initial);
	assert(foreach_outcome.result == foreach_entity.object);
	assert(decode_integer_list(store, runtime.vocabulary().nil, foreach_outcome.result, integers) ==
	       std::vector<std::int64_t>({1, 2, 3}));
	assert(foreach_outcome.semantic == initial);
	assert(initial.role(avm::SemanticContextRole::RelationState) == zero);
	assert(store.size() == before_foreach_execution);

	const avm::ExecutionOutcome repeated_foreach =
	    runtime.executor().execute_outcome_in_context(foreach_program, initial);
	assert(repeated_foreach.result == foreach_outcome.result);
	assert(repeated_foreach.semantic == foreach_outcome.semantic);
	assert(store.size() == before_foreach_execution);

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

	assert(migration_rejected(foreach_fixture(Json::array({1, 2, 3}), "foreachsub")));
	assert(migration_rejected(foreach_fixture(Json::array({1, 2, 3}), "foreachobj", "$sub")));
	assert(migration_rejected(foreach_fixture(Json::array({1, "two", 3}))));
	assert(migration_rejected(foreach_fixture(Json::array())));

	Json modified_foreach = foreach_fixture(Json::array({1, 2, 3}));
	modified_foreach["$rel/result"]["$sub"]["$sub"] = 0;
	assert(migration_rejected(modified_foreach));

	Json ambiguous_foreach = foreach_fixture(Json::array({1, 2, 3}));
	ambiguous_foreach["$rel/result"]["extra"] = true;
	assert(migration_rejected(ambiguous_foreach));

	return 0;
}
