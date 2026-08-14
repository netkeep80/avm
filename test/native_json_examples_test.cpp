#include "json_duplet_text.h"
#include "json_duplet_values.h"

#include "avm/bootstrap_runtime.h"
#include "avm/integer_value.h"
#include "avm/relations_model.h"
#include "avm/text_value.h"

#include "nlohmann/json.hpp"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Json = nlohmann::ordered_json;

std::string read_text(const char *path)
{
	std::ifstream input(path, std::ios::binary);
	if (!input)
		throw std::runtime_error(std::string("cannot open native JSON example: ") + path);
	std::ostringstream output;
	output << input.rdbuf();
	return output.str();
}

avm::ProjectionDescription project_example(const avm::LinkStore &store, const std::string &source,
                                           const avm::json_duplet::NativeLeafResolver &resolver)
{
	const std::size_t before = store.size();
	const avm::ProjectionDescription description =
	    avm::json_duplet::project_duplet_document_text<Json>(source, resolver);
	assert(store.size() == before);
	return description;
}

std::vector<std::uint8_t> utf8_bytes(const std::string &value)
{
	std::vector<std::uint8_t> result;
	result.reserve(value.size());
	for (const unsigned char byte : value)
		result.push_back(static_cast<std::uint8_t>(byte));
	return result;
}

} // namespace

int main(int argc, char **argv)
{
	assert(argc == 5);

	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime(store);
	const avm::IntegerVocabulary integers = avm::IntegerVocabulary::create(store);
	const avm::TextVocabulary text = avm::TextVocabulary::create(store);
	avm::register_integer_arithmetic(runtime.executor(), integers);

	const avm::LinkId left = store.create_point();
	const avm::LinkId right = store.create_point();
	const avm::LinkId relation = store.create_point();
	const avm::LinkId subject = store.create_point();
	const avm::LinkId object = store.create_point();

	avm::json_duplet::SymbolAnchors symbols{
	    {"false", runtime.vocabulary().false_value},
	    {"integer_add", integers.add_relation},
	    {"left", left},
	    {"nil", runtime.vocabulary().nil},
	    {"object", object},
	    {"relation", relation},
	    {"right", right},
	    {"subject", subject},
	    {"true", runtime.vocabulary().true_value},
	    {"unit", runtime.vocabulary().unit},
	};
	const avm::json_duplet::NativeLeafResolver resolver(integers, text, std::move(symbols));

	const avm::ProjectionDescription pair_description = project_example(store, read_text(argv[1]), resolver);
	const std::size_t before_pair_find = store.size();
	assert(!avm::find_projection(store, pair_description).has_value());
	assert(store.size() == before_pair_find);
	const avm::LinkId pair_root = avm::realize_projection(store, pair_description).root;
	const avm::Link pair = store.get(pair_root);
	assert(pair.begin == left);
	assert(pair.end == right);
	const std::size_t after_pair_realize = store.size();
	assert(avm::find_projection(store, pair_description)->root == pair_root);
	assert(store.size() == after_pair_realize);

	const avm::ProjectionDescription relation_description = project_example(store, read_text(argv[2]), resolver);
	const avm::LinkId relation_root = avm::realize_projection(store, relation_description).root;
	assert(avm::decode_relation_entity(store, relation_root) == avm::RelationEntity{relation, subject, object});

	const avm::ProjectionDescription integer_description = project_example(store, read_text(argv[3]), resolver);
	const avm::LinkId integer_root = avm::realize_projection(store, integer_description).root;
	const avm::RelationEntity integer_entity = avm::decode_relation_entity(store, integer_root);
	assert(integer_entity.relation == integers.add_relation);
	assert(avm::decode_integer(store, integers, integer_entity.subject) == 7);
	assert(avm::decode_integer(store, integers, integer_entity.object) == 3);
	const avm::LinkId integer_result = runtime.executor().execute(integer_root);
	assert(avm::decode_integer(store, integers, integer_result) == 10);

	const avm::ProjectionDescription text_description = project_example(store, read_text(argv[4]), resolver);
	const avm::LinkId text_root = avm::realize_projection(store, text_description).root;
	assert(avm::decode_text(store, text, text_root) == utf8_bytes("Привет, AVM"));

	const std::string unknown_symbol =
	    R"({"$avm":"duplet-json/1","$root":{"$symbol":"not_declared"}})";
	const std::size_t before_unknown = store.size();
	bool rejected = false;
	try
	{
		static_cast<void>(avm::json_duplet::project_duplet_document_text<Json>(unknown_symbol, resolver));
	}
	catch (const avm::json_duplet::ProjectionError &)
	{
		rejected = true;
	}
	assert(rejected);
	assert(store.size() == before_unknown);

	return 0;
}
