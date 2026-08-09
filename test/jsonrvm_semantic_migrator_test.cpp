#include "json_duplet_values.h"
#include "jsonrvm_semantic_migrator.h"

#include "avm/executor.h"
#include "avm/integer_value.h"
#include "avm/projection.h"
#include "avm/relations_model.h"
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

} // namespace

int main(int argc, char **argv)
{
	if (argc != 2)
		throw std::runtime_error("expected arithmetic fixture path");

	const Json frozen = load_json(argv[1]);
	const avm::jsonrvm_migration::MigrationResult<Json> migration =
	    avm::jsonrvm_migration::migrate_program(frozen);

	assert(migration.observable_json_pointer == "/result");
	assert(migration.document.is_object());
	assert(migration.document.at("$avm") == "duplet-json/1");

	Json frozen_copy = frozen;
	frozen_copy.clear();

	avm::InMemoryLinkStore store;
	const avm::IntegerVocabulary integers = avm::IntegerVocabulary::create(store);
	const avm::TextVocabulary text = avm::TextVocabulary::create(store);

	avm::json_duplet::SymbolAnchors symbols;
	symbols.emplace("integer_add", integers.add_relation);
	symbols.emplace("integer_subtract", integers.subtract_relation);
	symbols.emplace("integer_multiply", integers.multiply_relation);
	symbols.emplace("integer_divide", integers.divide_relation);
	const avm::json_duplet::NativeLeafResolver resolver(integers, text, symbols);

	const std::size_t before_projection = store.size();
	const avm::ProjectionDescription description =
	    avm::json_duplet::project_duplet_document(migration.document, resolver);
	assert(store.size() == before_projection);
	assert(!avm::find_projection(store, description).has_value());
	assert(store.size() == before_projection);

	const avm::LinkId program = avm::realize_projection(store, description).root;
	const avm::RelationEntity decoded = avm::decode_relation_entity(store, program);
	assert(decoded.relation == integers.add_relation);
	assert(avm::decode_integer(store, integers, decoded.subject) == 1);
	assert(avm::decode_integer(store, integers, decoded.object) == 1);

	avm::Executor executor(store);
	avm::register_integer_arithmetic(executor, integers);
	const avm::LinkId result = executor.execute(program);
	assert(avm::decode_integer(store, integers, result) == 2);

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
		const avm::ProjectionDescription operation_description =
		    avm::json_duplet::project_duplet_document(migrated.document, resolver);
		const avm::LinkId operation_program = avm::realize_projection(store, operation_description).root;
		const avm::LinkId operation_result = executor.execute(operation_program);
		assert(avm::decode_integer(store, integers, operation_result) == expected[index]);
	}

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

	return 0;
}
