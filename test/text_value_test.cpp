#include "avm/persistent_link_store.h"
#include "avm/text_value.h"

#include <cassert>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <functional>
#include <vector>

namespace
{

bool rejected(const std::function<void()> &operation)
{
	try
	{
		operation();
		return false;
	}
	catch (const std::exception &)
	{
		return true;
	}
}

void verify_text_value(avm::LinkStore &store, const avm::TextVocabulary &vocabulary,
                       const std::vector<std::uint8_t> &bytes)
{
	const std::size_t before_find = store.size();
	assert(!avm::find_text(store, vocabulary, bytes).has_value());
	assert(store.size() == before_find);

	const avm::LinkId realized = avm::realize_text(store, vocabulary, bytes);
	assert(avm::decode_text(store, vocabulary, realized) == bytes);
	assert(avm::is_text(store, vocabulary, realized));
	assert(avm::find_text(store, vocabulary, bytes) == realized);

	const std::size_t before_repeat = store.size();
	assert(avm::realize_text(store, vocabulary, bytes) == realized);
	assert(store.size() == before_repeat);
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	const avm::TextVocabulary vocabulary = avm::TextVocabulary::create(store);
	avm::validate_text_vocabulary(store, vocabulary);

	const std::vector<std::uint8_t> byte_values{0x00, 0x01, 0x05, 0x7F, 0x80, 0xFF};
	for (const std::uint8_t value : byte_values)
	{
		const std::size_t before_find = store.size();
		assert(!avm::find_byte(store, vocabulary, value).has_value());
		assert(store.size() == before_find);

		const avm::LinkId realized = avm::realize_byte(store, vocabulary, value);
		assert(avm::decode_byte(store, vocabulary, realized) == value);
		assert(avm::is_byte(store, vocabulary, realized));
		assert(avm::find_byte(store, vocabulary, value) == realized);

		const std::size_t before_repeat = store.size();
		assert(avm::realize_byte(store, vocabulary, value) == realized);
		assert(store.size() == before_repeat);
	}

	const std::vector<std::uint8_t> empty;
	const std::vector<std::uint8_t> hello{'h', 'e', 'l', 'l', 'o'};
	const std::vector<std::uint8_t> embedded_nul{'A', 0x00, 'B'};
	const std::vector<std::uint8_t> utf8{0xD0, 0x9F, 0xD1, 0x80, 0xD0, 0xB8, 0xD0, 0xB2, 0xD0, 0xB5, 0xD1, 0x82};

	verify_text_value(store, vocabulary, empty);
	verify_text_value(store, vocabulary, hello);
	verify_text_value(store, vocabulary, embedded_nul);
	verify_text_value(store, vocabulary, utf8);

	const std::vector<std::uint8_t> hell{'h', 'e', 'l', 'l'};
	const avm::LinkId hello_id = *avm::find_text(store, vocabulary, hello);
	const avm::LinkId hell_id = avm::realize_text(store, vocabulary, hell);
	assert(hello_id != hell_id);
	assert(avm::decode_text(store, vocabulary, hell_id) == hell);

	const std::vector<std::uint8_t> composed{0xC3, 0xA9};
	const std::vector<std::uint8_t> decomposed{'e', 0xCC, 0x81};
	const avm::LinkId composed_id = avm::realize_text(store, vocabulary, composed);
	const avm::LinkId decomposed_id = avm::realize_text(store, vocabulary, decomposed);
	assert(composed_id != decomposed_id);
	assert(avm::decode_text(store, vocabulary, composed_id) == composed);
	assert(avm::decode_text(store, vocabulary, decomposed_id) == decomposed);

	const avm::LinkId arbitrary_point = store.create_point();
	const std::size_t before_bad_read = store.size();
	assert(rejected([&] { static_cast<void>(avm::decode_text(store, vocabulary, arbitrary_point)); }));
	assert(!avm::is_text(store, vocabulary, arbitrary_point));
	assert(rejected([&] { static_cast<void>(avm::decode_byte(store, vocabulary, arbitrary_point)); }));
	assert(!avm::is_byte(store, vocabulary, arbitrary_point));
	assert(store.size() == before_bad_read);

	const avm::LinkId short_bits = store.intern(vocabulary.bit_one, vocabulary.byte_end);
	const avm::LinkId short_byte = store.intern(vocabulary.byte_marker, short_bits);
	const std::size_t before_short_decode = store.size();
	assert(rejected([&] { static_cast<void>(avm::decode_byte(store, vocabulary, short_byte)); }));
	assert(store.size() == before_short_decode);

	avm::LinkId long_bits = vocabulary.byte_end;
	for (int index = 0; index < 9; ++index)
		long_bits = store.intern(vocabulary.bit_zero, long_bits);
	const avm::LinkId long_byte = store.intern(vocabulary.byte_marker, long_bits);
	const std::size_t before_long_decode = store.size();
	assert(rejected([&] { static_cast<void>(avm::decode_byte(store, vocabulary, long_byte)); }));
	assert(store.size() == before_long_decode);

	const avm::LinkId non_bit_cell = store.intern(vocabulary.text_marker, vocabulary.byte_end);
	const avm::LinkId non_bit_byte = store.intern(vocabulary.byte_marker, non_bit_cell);
	const std::size_t before_non_bit_decode = store.size();
	assert(rejected([&] { static_cast<void>(avm::decode_byte(store, vocabulary, non_bit_byte)); }));
	assert(store.size() == before_non_bit_decode);

	const avm::LinkId bad_text_cell = store.intern(arbitrary_point, vocabulary.text_end);
	const avm::LinkId bad_text = store.intern(vocabulary.text_marker, bad_text_cell);
	const std::size_t before_bad_text_decode = store.size();
	assert(rejected([&] { static_cast<void>(avm::decode_text(store, vocabulary, bad_text)); }));
	assert(store.size() == before_bad_text_decode);

	const std::size_t before_limited_decode = store.size();
	assert(rejected([&] { static_cast<void>(avm::decode_text(store, vocabulary, hello_id, 2)); }));
	assert(store.size() == before_limited_decode);

	avm::TextVocabulary duplicate_vocabulary = vocabulary;
	duplicate_vocabulary.bit_one = duplicate_vocabulary.bit_zero;
	assert(rejected([&] { avm::validate_text_vocabulary(store, duplicate_vocabulary); }));

	avm::TextVocabulary unknown_vocabulary = vocabulary;
	unknown_vocabulary.text_marker = store.size() + 1000;
	assert(rejected([&] { avm::validate_text_vocabulary(store, unknown_vocabulary); }));

	const std::filesystem::path persistent_path = std::filesystem::temp_directory_path() / "avm_text_value_test.links";
	std::filesystem::remove(persistent_path);

	avm::TextVocabulary persistent_vocabulary{};
	avm::LinkId persistent_text = avm::invalid_link_id;
	const std::vector<std::uint8_t> persistent_bytes{'A', 0x00, 0xD0, 0xAF};
	{
		avm::PersistentLinkStore persistent_store(persistent_path);
		persistent_vocabulary = avm::TextVocabulary::create(persistent_store);
		persistent_text = avm::realize_text(persistent_store, persistent_vocabulary, persistent_bytes);
		assert(avm::decode_text(persistent_store, persistent_vocabulary, persistent_text) == persistent_bytes);
	}
	{
		avm::PersistentLinkStore reopened(persistent_path);
		const std::size_t before_find = reopened.size();
		assert(avm::find_text(reopened, persistent_vocabulary, persistent_bytes) == persistent_text);
		assert(reopened.size() == before_find);
		assert(avm::decode_text(reopened, persistent_vocabulary, persistent_text) == persistent_bytes);

		const std::size_t before_repeat = reopened.size();
		assert(avm::realize_text(reopened, persistent_vocabulary, persistent_bytes) == persistent_text);
		assert(reopened.size() == before_repeat);
	}
	std::filesystem::remove(persistent_path);

	return 0;
}
