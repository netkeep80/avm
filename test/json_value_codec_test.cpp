#include "avm/json_value_codec.h"

#include <cassert>
#include <cmath>
#include <string>

namespace
{

using Json = nlohmann::json;

Json roundtrip(avm::JsonValueCodec &codec, const Json &input)
{
	const avm::LinkId encoded = codec.encode(input);
	return codec.decode(encoded);
}

void test_primitives(avm::JsonValueCodec &codec)
{
	assert(roundtrip(codec, Json(nullptr)) == Json(nullptr));
	assert(roundtrip(codec, Json(true)) == Json(true));
	assert(roundtrip(codec, Json(false)) == Json(false));

	for (Json::number_unsigned_t value : {0ULL, 1ULL, 7ULL, 42ULL, 255ULL, 65535ULL, 1000000ULL})
		assert(roundtrip(codec, Json(value)) == Json(value));

	for (Json::number_integer_t value : {-1LL, -7LL, -42LL, -255LL, -1000000LL})
		assert(roundtrip(codec, Json(value)) == Json(value));

	for (double value : {0.0, 0.1, 0.5, 1.5, -0.5, 3.14})
	{
		const Json output = roundtrip(codec, Json(value));
		assert(output.is_number_float());
		assert(output.get<double>() == value);
	}
}

void test_strings(avm::JsonValueCodec &codec)
{
	for (const char *value : {"", "a", "hello", "line1\nline2\ttab", "Hello, World!", "Привет"})
		assert(roundtrip(codec, Json(value)) == Json(value));
}

void test_arrays(avm::JsonValueCodec &codec)
{
	assert(roundtrip(codec, Json::array()) == Json::array());
	assert(roundtrip(codec, Json::array({true})) == Json::array({true}));
	assert(roundtrip(codec, Json::array({nullptr, true, false, 42, "text"})) ==
	       Json::array({nullptr, true, false, 42, "text"}));
	assert(roundtrip(codec, Json::array({Json::array(), Json::array({Json::array({true})})})) ==
	       Json::array({Json::array(), Json::array({Json::array({true})})}));
}

void test_objects(avm::JsonValueCodec &codec)
{
	assert(roundtrip(codec, Json::object()) == Json::object());
	assert((roundtrip(codec, Json{{"key", "value"}}) == Json{{"key", "value"}}));

	const Json mixed = {
	    {"array", Json::array({1, 2, 3})},
	    {"bool", true},
	    {"nested", Json{{"empty", Json::object()}, {"value", -42}}},
	    {"null", nullptr},
	    {"string", "hello"},
	};
	assert(roundtrip(codec, mixed) == mixed);
}

void test_canonical_reuse()
{
	avm::InMemoryLinkStore store;
	avm::JsonValueCodec codec(store);
	const Json value = Json{{"a", Json::array({1, 2, 3})}, {"b", "same"}};

	const avm::LinkId first = codec.encode(value);
	const std::size_t after_first = store.size();
	const avm::LinkId second = codec.encode(value);
	assert(second == first);
	assert(store.size() == after_first);
}

void test_restore_vocabulary()
{
	avm::InMemoryLinkStore store;
	avm::JsonValueCodec first(store);
	const avm::JsonValueVocabulary vocabulary = first.vocabulary();
	const avm::LinkId value = first.encode(Json{{"persisted", Json::array({true, 42, "x"})}});
	const std::size_t before_restore = store.size();

	avm::JsonValueCodec restored(store, vocabulary);
	assert(store.size() == before_restore);
	assert((restored.decode(value) == Json{{"persisted", Json::array({true, 42, "x"})}}));
}

void test_invalid_decode_rejected()
{
	avm::InMemoryLinkStore store;
	avm::JsonValueCodec codec(store);
	const avm::LinkId unrelated = store.create_point();

	bool rejected = false;
	try
	{
		static_cast<void>(codec.decode(unrelated));
	}
	catch (const std::runtime_error &)
	{
		rejected = true;
	}
	assert(rejected);
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	avm::JsonValueCodec codec(store);
	test_primitives(codec);
	test_strings(codec);
	test_arrays(codec);
	test_objects(codec);
	test_canonical_reuse();
	test_restore_vocabulary();
	test_invalid_decode_rejected();
	return 0;
}
