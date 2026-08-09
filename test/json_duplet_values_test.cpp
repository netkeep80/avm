#include "json_duplet_text.h"
#include "json_duplet_values.h"

#include "avm/executor.h"
#include "avm/integer_value.h"
#include "avm/persistent_link_store.h"
#include "avm/program_model.h"
#include "avm/relations_model.h"

#include "nlohmann/json.hpp"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using Json = nlohmann::ordered_json;

Json tagged(const char *name, Json value)
{
	Json result = Json::object();
	result[name] = std::move(value);
	return result;
}

Json pair(Json begin, Json end)
{
	Json result = Json::object();
	result["<<"] = std::move(begin);
	result[">>"] = std::move(end);
	return result;
}

Json add_expression(std::int64_t left, std::int64_t right)
{
	return pair(tagged("$symbol", "integer_add"), pair(tagged("$integer", left), tagged("$integer", right)));
}

bool projection_rejected(const Json &value, const avm::json_duplet::NativeLeafResolver &resolver)
{
	try
	{
		static_cast<void>(avm::json_duplet::project_duplet_term(value, resolver));
		return false;
	}
	catch (const avm::json_duplet::ProjectionError &)
	{
		return true;
	}
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	const avm::BootstrapVocabulary bootstrap = avm::BootstrapVocabulary::create(store);
	const avm::IntegerVocabulary integers = avm::IntegerVocabulary::create(store);
	avm::json_duplet::SymbolAnchors symbols;
	symbols.emplace("integer_add", integers.add_relation);
	symbols.emplace("nil", bootstrap.nil);
	symbols.emplace("false", bootstrap.false_value);
	symbols.emplace("true", bootstrap.true_value);
	symbols.emplace("unit", bootstrap.unit);
	const avm::json_duplet::NativeLeafResolver resolver(integers, symbols);

	const Json expression = add_expression(7, 3);
	const std::size_t before_projection = store.size();
	const avm::ProjectionDescription description = avm::json_duplet::project_duplet_term(expression, resolver);
	assert(store.size() == before_projection);
	assert(!avm::find_projection(store, description).has_value());
	assert(store.size() == before_projection);

	const avm::ProjectionResult realized = avm::realize_projection(store, description);
	const auto seven = avm::find_integer(store, integers, 7);
	const auto three = avm::find_integer(store, integers, 3);
	assert(seven.has_value());
	assert(three.has_value());
	const avm::RelationEntity decoded = avm::decode_relation_entity(store, realized.root);
	const avm::RelationEntity expected{integers.add_relation, *seven, *three};
	assert(decoded == expected);

	const std::size_t before_direct_encode = store.size();
	assert(avm::encode_relation_entity(store, expected) == realized.root);
	assert(store.size() == before_direct_encode);

	avm::Executor executor(store);
	avm::register_integer_arithmetic(executor, integers);
	const avm::LinkId ten = executor.execute(realized.root);
	assert(avm::decode_integer(store, integers, ten) == 10);

	const std::string raw_expression =
	    R"({"<<":{"$symbol":"integer_add"},">>":{"<<":{"$integer":7},">>":{"$integer":3}}})";
	const std::size_t before_text_projection = store.size();
	const avm::ProjectionDescription text_description =
	    avm::json_duplet::project_duplet_term_text<Json>(raw_expression, resolver);
	assert(text_description.nodes == description.nodes);
	assert(text_description.root == description.root);
	assert(store.size() == before_text_projection);
	const auto text_found = avm::find_projection(store, text_description);
	assert(text_found.has_value());
	assert(text_found->root == realized.root);

	const Json true_leaf = tagged("$symbol", "true");
	const avm::ProjectionDescription true_description = avm::json_duplet::project_duplet_term(true_leaf, resolver);
	assert(true_description.nodes.empty());
	assert(true_description.root == avm::ProjectionRef::anchor(bootstrap.true_value));
	assert(avm::find_projection(store, true_description)->root == bootstrap.true_value);

	const Json false_leaf = tagged("$symbol", "false");
	const avm::ProjectionDescription false_description = avm::json_duplet::project_duplet_term(false_leaf, resolver);
	assert(false_description.root == avm::ProjectionRef::anchor(bootstrap.false_value));

	const Json nil_leaf = tagged("$symbol", "nil");
	const avm::ProjectionDescription nil_description = avm::json_duplet::project_duplet_term(nil_leaf, resolver);
	assert(nil_description.root == avm::ProjectionRef::anchor(bootstrap.nil));

	const Json zero_leaf = tagged("$integer", 0);
	const avm::ProjectionDescription zero_description = avm::json_duplet::project_duplet_term(zero_leaf, resolver);
	assert(zero_description.nodes.empty());
	assert(zero_description.root == avm::ProjectionRef::anchor(integers.zero));
	assert(avm::find_projection(store, zero_description)->root == integers.zero);

	const Json negative_leaf = tagged("$integer", -7);
	const avm::ProjectionDescription negative_description =
	    avm::json_duplet::project_duplet_term(negative_leaf, resolver);
	const avm::LinkId negative_root = avm::realize_projection(store, negative_description).root;
	assert(avm::decode_integer(store, integers, negative_root) == -7);
	assert(negative_root == avm::realize_integer(store, integers, -7));

	assert(projection_rejected(tagged("$symbol", "missing"), resolver));
	assert(projection_rejected(tagged("$symbol", 7), resolver));
	assert(projection_rejected(Json("integer_add"), resolver));
	assert(projection_rejected(Json(7), resolver));
	assert(projection_rejected(tagged("$integer", 1.5), resolver));

	Json mixed_leaf = tagged("$integer", 7);
	mixed_leaf["extra"] = true;
	assert(projection_rejected(mixed_leaf, resolver));

	const std::uint64_t too_large = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) + 1U;
	assert(projection_rejected(tagged("$integer", too_large), resolver));

	const avm::LinkId missing_link = store.size() + 1000;
	const avm::ProjectionDescription missing_description =
	    avm::json_duplet::project_duplet_term(tagged("$link", missing_link), resolver);
	const std::size_t before_missing = store.size();
	assert(!avm::find_projection(store, missing_description).has_value());
	assert(store.size() == before_missing);
	bool missing_realize_rejected = false;
	try
	{
		static_cast<void>(avm::realize_projection(store, missing_description));
	}
	catch (const std::invalid_argument &)
	{
		missing_realize_rejected = true;
	}
	assert(missing_realize_rejected);
	assert(store.size() == before_missing);

	const std::filesystem::path persistent_path =
	    std::filesystem::temp_directory_path() / "avm_json_duplet_values_test.links";
	std::filesystem::remove(persistent_path);

	avm::IntegerVocabulary persistent_integers{};
	avm::LinkId persistent_value = avm::invalid_link_id;
	{
		avm::PersistentLinkStore persistent_store(persistent_path);
		persistent_integers = avm::IntegerVocabulary::create(persistent_store);
		const avm::json_duplet::SymbolAnchors persistent_symbols;
		const avm::json_duplet::NativeLeafResolver persistent_resolver(persistent_integers, persistent_symbols);
		const avm::ProjectionDescription persistent_description =
		    avm::json_duplet::project_duplet_term(tagged("$integer", 42), persistent_resolver);
		persistent_value = avm::realize_projection(persistent_store, persistent_description).root;
		assert(avm::decode_integer(persistent_store, persistent_integers, persistent_value) == 42);
	}
	{
		avm::PersistentLinkStore reopened(persistent_path);
		const avm::json_duplet::SymbolAnchors persistent_symbols;
		const avm::json_duplet::NativeLeafResolver persistent_resolver(persistent_integers, persistent_symbols);
		const avm::ProjectionDescription persistent_description =
		    avm::json_duplet::project_duplet_term(tagged("$integer", 42), persistent_resolver);
		const std::size_t before_reopen_find = reopened.size();
		const auto persistent_found = avm::find_projection(reopened, persistent_description);
		assert(persistent_found.has_value());
		assert(persistent_found->root == persistent_value);
		assert(reopened.size() == before_reopen_find);
		assert(avm::decode_integer(reopened, persistent_integers, persistent_value) == 42);
	}
	std::filesystem::remove(persistent_path);

	return 0;
}
