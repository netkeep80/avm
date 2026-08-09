#pragma once

#include "avm/link_store.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <span>
#include <stdexcept>
#include <vector>

namespace avm
{

struct TextVocabulary
{
	LinkId text_marker;
	LinkId text_end;
	LinkId byte_marker;
	LinkId byte_end;
	LinkId bit_zero;
	LinkId bit_one;

	static TextVocabulary create(LinkStore &store)
	{
		TextVocabulary vocabulary{};
		vocabulary.text_marker = store.create_point();
		vocabulary.text_end = store.create_point();
		vocabulary.byte_marker = store.create_point();
		vocabulary.byte_end = store.create_point();
		vocabulary.bit_zero = store.create_point();
		vocabulary.bit_one = store.create_point();
		return vocabulary;
	}
};

inline void validate_text_vocabulary(const LinkStore &store, const TextVocabulary &vocabulary)
{
	std::set<LinkId> unique;
	const auto require_identity = [&store, &unique](LinkId id)
	{
		if (!store.contains(id))
			throw std::invalid_argument("text vocabulary contains an unknown LinkId");
		if (!unique.insert(id).second)
			throw std::invalid_argument("text vocabulary identities must be distinct");
	};

	require_identity(vocabulary.text_marker);
	require_identity(vocabulary.text_end);
	require_identity(vocabulary.byte_marker);
	require_identity(vocabulary.byte_end);
	require_identity(vocabulary.bit_zero);
	require_identity(vocabulary.bit_one);
}

namespace detail
{

inline std::optional<LinkId> find_byte_unchecked(const LinkStore &store, const TextVocabulary &vocabulary,
                                                 std::uint8_t value)
{
	LinkId suffix = vocabulary.byte_end;
	for (int bit = 7; bit >= 0; --bit)
	{
		const bool one = (value & static_cast<std::uint8_t>(1U << bit)) != 0;
		const LinkId marker = one ? vocabulary.bit_one : vocabulary.bit_zero;
		const auto cell = store.find(marker, suffix);
		if (!cell)
			return std::nullopt;
		suffix = *cell;
	}
	return store.find(vocabulary.byte_marker, suffix);
}

inline LinkId realize_byte_unchecked(LinkStore &store, const TextVocabulary &vocabulary, std::uint8_t value)
{
	LinkId suffix = vocabulary.byte_end;
	for (int bit = 7; bit >= 0; --bit)
	{
		const bool one = (value & static_cast<std::uint8_t>(1U << bit)) != 0;
		const LinkId marker = one ? vocabulary.bit_one : vocabulary.bit_zero;
		suffix = store.intern(marker, suffix);
	}
	return store.intern(vocabulary.byte_marker, suffix);
}

inline std::uint8_t decode_byte_unchecked(const LinkStore &store, const TextVocabulary &vocabulary, LinkId value)
{
	if (!store.contains(value))
		throw std::invalid_argument("byte LinkId is not present in LinkStore");

	const Link wrapper = store.get(value);
	if (wrapper.begin != vocabulary.byte_marker)
		throw std::runtime_error("LinkId is not a canonical byte wrapper");

	LinkId cursor = wrapper.end;
	std::set<LinkId> visited;
	std::uint8_t result = 0;

	for (std::size_t bit = 0; bit < 8; ++bit)
	{
		if (cursor == vocabulary.byte_end)
			throw std::runtime_error("byte bit chain is shorter than eight bits");
		if (!store.contains(cursor))
			throw std::runtime_error("byte bit chain references an unknown LinkId");
		if (!visited.insert(cursor).second)
			throw std::runtime_error("cycle detected in byte bit chain");

		const Link cell = store.get(cursor);
		if (cell.begin == vocabulary.bit_one)
			result = static_cast<std::uint8_t>(result | static_cast<std::uint8_t>(1U << bit));
		else if (cell.begin != vocabulary.bit_zero)
			throw std::runtime_error("byte bit chain contains a non-bit marker");
		cursor = cell.end;
	}

	if (cursor != vocabulary.byte_end)
		throw std::runtime_error("byte bit chain is longer than eight bits");
	return result;
}

} // namespace detail

inline std::optional<LinkId> find_byte(const LinkStore &store, const TextVocabulary &vocabulary, std::uint8_t value)
{
	validate_text_vocabulary(store, vocabulary);
	return detail::find_byte_unchecked(store, vocabulary, value);
}

inline LinkId realize_byte(LinkStore &store, const TextVocabulary &vocabulary, std::uint8_t value)
{
	validate_text_vocabulary(store, vocabulary);
	return detail::realize_byte_unchecked(store, vocabulary, value);
}

inline std::uint8_t decode_byte(const LinkStore &store, const TextVocabulary &vocabulary, LinkId value)
{
	validate_text_vocabulary(store, vocabulary);
	return detail::decode_byte_unchecked(store, vocabulary, value);
}

inline bool is_byte(const LinkStore &store, const TextVocabulary &vocabulary, LinkId value) noexcept
{
	try
	{
		static_cast<void>(decode_byte(store, vocabulary, value));
		return true;
	}
	catch (...)
	{
		return false;
	}
}

inline std::optional<LinkId> find_text(const LinkStore &store, const TextVocabulary &vocabulary,
                                       std::span<const std::uint8_t> bytes)
{
	validate_text_vocabulary(store, vocabulary);

	LinkId tail = vocabulary.text_end;
	for (auto byte = bytes.rbegin(); byte != bytes.rend(); ++byte)
	{
		const auto byte_id = detail::find_byte_unchecked(store, vocabulary, *byte);
		if (!byte_id)
			return std::nullopt;
		const auto cell = store.find(*byte_id, tail);
		if (!cell)
			return std::nullopt;
		tail = *cell;
	}
	return store.find(vocabulary.text_marker, tail);
}

inline LinkId realize_text(LinkStore &store, const TextVocabulary &vocabulary, std::span<const std::uint8_t> bytes)
{
	validate_text_vocabulary(store, vocabulary);

	LinkId tail = vocabulary.text_end;
	for (auto byte = bytes.rbegin(); byte != bytes.rend(); ++byte)
	{
		const LinkId byte_id = detail::realize_byte_unchecked(store, vocabulary, *byte);
		tail = store.intern(byte_id, tail);
	}
	return store.intern(vocabulary.text_marker, tail);
}

inline std::vector<std::uint8_t> decode_text(const LinkStore &store, const TextVocabulary &vocabulary, LinkId value,
                                             std::size_t max_bytes = 1000000)
{
	validate_text_vocabulary(store, vocabulary);
	if (!store.contains(value))
		throw std::invalid_argument("text LinkId is not present in LinkStore");

	const Link wrapper = store.get(value);
	if (wrapper.begin != vocabulary.text_marker)
		throw std::runtime_error("LinkId is not a canonical text wrapper");

	std::vector<std::uint8_t> result;
	LinkId cursor = wrapper.end;
	std::set<LinkId> visited;
	while (cursor != vocabulary.text_end)
	{
		if (result.size() >= max_bytes)
			throw std::runtime_error("text byte sequence exceeds configured limit");
		if (!store.contains(cursor))
			throw std::runtime_error("text byte sequence references an unknown LinkId");
		if (!visited.insert(cursor).second)
			throw std::runtime_error("cycle detected in text byte sequence");

		const Link cell = store.get(cursor);
		result.push_back(detail::decode_byte_unchecked(store, vocabulary, cell.begin));
		cursor = cell.end;
	}
	return result;
}

inline bool is_text(const LinkStore &store, const TextVocabulary &vocabulary, LinkId value) noexcept
{
	try
	{
		static_cast<void>(decode_text(store, vocabulary, value));
		return true;
	}
	catch (...)
	{
		return false;
	}
}

} // namespace avm
