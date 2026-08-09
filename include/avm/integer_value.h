#pragma once

#include "avm/executor.h"

#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

namespace avm
{

struct IntegerVocabulary
{
	LinkId zero;
	LinkId positive;
	LinkId negative;
	LinkId magnitude_end;
	LinkId bit_zero;
	LinkId bit_one;
	LinkId add_relation;
	LinkId subtract_relation;
	LinkId multiply_relation;
	LinkId divide_relation;

	static IntegerVocabulary create(LinkStore &store)
	{
		IntegerVocabulary vocabulary{};
		vocabulary.zero = store.create_point();
		vocabulary.positive = store.create_point();
		vocabulary.negative = store.create_point();
		vocabulary.magnitude_end = store.create_point();
		vocabulary.bit_zero = store.create_point();
		vocabulary.bit_one = store.create_point();
		vocabulary.add_relation = store.create_point();
		vocabulary.subtract_relation = store.create_point();
		vocabulary.multiply_relation = store.create_point();
		vocabulary.divide_relation = store.create_point();
		return vocabulary;
	}
};

inline void validate_integer_vocabulary(const LinkStore &store, const IntegerVocabulary &vocabulary)
{
	const LinkId ids[] = {
	    vocabulary.zero,
	    vocabulary.positive,
	    vocabulary.negative,
	    vocabulary.magnitude_end,
	    vocabulary.bit_zero,
	    vocabulary.bit_one,
	    vocabulary.add_relation,
	    vocabulary.subtract_relation,
	    vocabulary.multiply_relation,
	    vocabulary.divide_relation,
	};

	std::set<LinkId> unique;
	for (const LinkId id : ids)
	{
		if (!store.contains(id))
			throw std::invalid_argument("integer vocabulary contains an unknown LinkId");
		if (!unique.insert(id).second)
			throw std::invalid_argument("integer vocabulary identities must be distinct");
	}
}

inline std::uint64_t integer_unsigned_magnitude(std::int64_t value) noexcept
{
	if (value >= 0)
		return static_cast<std::uint64_t>(value);
	return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

inline std::vector<bool> integer_magnitude_bits(std::uint64_t magnitude)
{
	std::vector<bool> bits;
	while (magnitude != 0)
	{
		bits.push_back((magnitude & 1U) != 0);
		magnitude >>= 1U;
	}
	return bits;
}

inline std::optional<LinkId> find_integer(const LinkStore &store, const IntegerVocabulary &vocabulary,
                                          std::int64_t value)
{
	validate_integer_vocabulary(store, vocabulary);
	if (value == 0)
		return vocabulary.zero;

	const std::vector<bool> bits = integer_magnitude_bits(integer_unsigned_magnitude(value));
	LinkId suffix = vocabulary.magnitude_end;
	for (auto it = bits.rbegin(); it != bits.rend(); ++it)
	{
		const LinkId marker = *it ? vocabulary.bit_one : vocabulary.bit_zero;
		const auto cell = store.find(marker, suffix);
		if (!cell)
			return std::nullopt;
		suffix = *cell;
	}

	return store.find(value < 0 ? vocabulary.negative : vocabulary.positive, suffix);
}

inline LinkId realize_integer(LinkStore &store, const IntegerVocabulary &vocabulary, std::int64_t value)
{
	validate_integer_vocabulary(store, vocabulary);
	if (value == 0)
		return vocabulary.zero;

	const std::vector<bool> bits = integer_magnitude_bits(integer_unsigned_magnitude(value));
	LinkId suffix = vocabulary.magnitude_end;
	for (auto it = bits.rbegin(); it != bits.rend(); ++it)
	{
		const LinkId marker = *it ? vocabulary.bit_one : vocabulary.bit_zero;
		suffix = store.intern(marker, suffix);
	}

	return store.intern(value < 0 ? vocabulary.negative : vocabulary.positive, suffix);
}

inline std::int64_t integer_from_magnitude(std::uint64_t magnitude, bool negative)
{
	if (magnitude == 0)
		throw std::runtime_error("signed integer wrapper contains zero magnitude");

	const std::uint64_t positive_limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
	const std::uint64_t negative_limit = positive_limit + 1U;

	if (!negative)
	{
		if (magnitude > positive_limit)
			throw std::overflow_error("integer value exceeds int64 positive range");
		return static_cast<std::int64_t>(magnitude);
	}

	if (magnitude > negative_limit)
		throw std::overflow_error("integer value exceeds int64 negative range");
	if (magnitude == negative_limit)
		return std::numeric_limits<std::int64_t>::min();
	return -static_cast<std::int64_t>(magnitude);
}

inline std::int64_t decode_integer(const LinkStore &store, const IntegerVocabulary &vocabulary, LinkId value)
{
	validate_integer_vocabulary(store, vocabulary);
	if (!store.contains(value))
		throw std::invalid_argument("integer LinkId is not present in LinkStore");
	if (value == vocabulary.zero)
		return 0;

	const Link wrapper = store.get(value);
	bool negative = false;
	if (wrapper.begin == vocabulary.positive)
		negative = false;
	else if (wrapper.begin == vocabulary.negative)
		negative = true;
	else
		throw std::runtime_error("LinkId is not a signed integer wrapper");

	LinkId cursor = wrapper.end;
	std::set<LinkId> visited;
	std::uint64_t magnitude = 0;
	unsigned bit_index = 0;
	bool saw_bit = false;
	bool last_bit_was_one = false;

	while (cursor != vocabulary.magnitude_end)
	{
		if (!store.contains(cursor))
			throw std::runtime_error("integer magnitude references an unknown LinkId");
		if (!visited.insert(cursor).second)
			throw std::runtime_error("cycle detected in integer magnitude");

		const Link cell = store.get(cursor);
		bool bit = false;
		if (cell.begin == vocabulary.bit_zero)
			bit = false;
		else if (cell.begin == vocabulary.bit_one)
			bit = true;
		else
			throw std::runtime_error("integer magnitude contains a non-bit marker");

		if (bit)
		{
			if (bit_index >= 64)
				throw std::overflow_error("integer magnitude exceeds int64 host domain");
			magnitude |= std::uint64_t{1} << bit_index;
		}

		saw_bit = true;
		last_bit_was_one = bit;
		cursor = cell.end;
		++bit_index;
		if (bit_index > 64 && cursor != vocabulary.magnitude_end)
			throw std::overflow_error("integer magnitude exceeds int64 host domain");
	}

	if (!saw_bit)
		throw std::runtime_error("signed integer wrapper contains an empty magnitude");
	if (!last_bit_was_one)
		throw std::runtime_error("integer magnitude has a non-canonical leading zero");

	return integer_from_magnitude(magnitude, negative);
}

inline bool is_integer(const LinkStore &store, const IntegerVocabulary &vocabulary, LinkId value) noexcept
{
	try
	{
		static_cast<void>(decode_integer(store, vocabulary, value));
		return true;
	}
	catch (...)
	{
		return false;
	}
}

inline std::int64_t checked_integer_add(std::int64_t left, std::int64_t right)
{
	const auto min = std::numeric_limits<std::int64_t>::min();
	const auto max = std::numeric_limits<std::int64_t>::max();
	if ((right > 0 && left > max - right) || (right < 0 && left < min - right))
		throw std::overflow_error("integer addition overflow");
	return left + right;
}

inline std::int64_t checked_integer_subtract(std::int64_t left, std::int64_t right)
{
	const auto min = std::numeric_limits<std::int64_t>::min();
	const auto max = std::numeric_limits<std::int64_t>::max();
	if ((right > 0 && left < min + right) || (right < 0 && left > max + right))
		throw std::overflow_error("integer subtraction overflow");
	return left - right;
}

inline std::int64_t integer_from_product_magnitude(std::uint64_t magnitude, bool negative)
{
	if (magnitude == 0)
		return 0;
	return integer_from_magnitude(magnitude, negative);
}

inline std::int64_t checked_integer_multiply(std::int64_t left, std::int64_t right)
{
	if (left == 0 || right == 0)
		return 0;

	const bool negative = (left < 0) != (right < 0);
	const std::uint64_t left_magnitude = integer_unsigned_magnitude(left);
	const std::uint64_t right_magnitude = integer_unsigned_magnitude(right);
	const std::uint64_t positive_limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
	const std::uint64_t limit = negative ? positive_limit + 1U : positive_limit;

	if (left_magnitude > limit / right_magnitude)
		throw std::overflow_error("integer multiplication overflow");
	return integer_from_product_magnitude(left_magnitude * right_magnitude, negative);
}

inline std::int64_t checked_integer_divide(std::int64_t left, std::int64_t right)
{
	if (right == 0)
		throw std::domain_error("integer division by zero");
	if (left == std::numeric_limits<std::int64_t>::min() && right == -1)
		throw std::overflow_error("integer division overflow");
	return left / right;
}

template <typename Operation>
inline ExecutionOutcome execute_integer_binary(const ExecutionContext &context, Executor &executor,
                                               const IntegerVocabulary &vocabulary, Operation operation)
{
	const std::int64_t left = decode_integer(executor.store(), vocabulary, context.subject);
	const std::int64_t right = decode_integer(executor.store(), vocabulary, context.object);
	const LinkId result = realize_integer(executor.store(), vocabulary, operation(left, right));
	return ExecutionOutcome{result};
}

inline void register_integer_arithmetic(Executor &executor, const IntegerVocabulary &vocabulary)
{
	validate_integer_vocabulary(executor.store(), vocabulary);
	const auto make_handler = [vocabulary](auto operation)
	{
		return [vocabulary, operation](const ExecutionContext &context, Executor &current_executor)
		{ return execute_integer_binary(context, current_executor, vocabulary, operation); };
	};

	executor.register_native(vocabulary.add_relation, make_handler(checked_integer_add));
	executor.register_native(vocabulary.subtract_relation, make_handler(checked_integer_subtract));
	executor.register_native(vocabulary.multiply_relation, make_handler(checked_integer_multiply));
	executor.register_native(vocabulary.divide_relation, make_handler(checked_integer_divide));
}

} // namespace avm