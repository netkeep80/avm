#include "avm/integer_value.h"
#include "avm/persistent_link_store.h"
#include "avm/relations_model.h"

#include <cassert>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace
{

bool decode_rejected(const avm::LinkStore &store, const avm::IntegerVocabulary &vocabulary, avm::LinkId value)
{
	try
	{
		static_cast<void>(avm::decode_integer(store, vocabulary, value));
		return false;
	}
	catch (const std::exception &)
	{
		return true;
	}
}

bool operation_rejected(const std::function<void()> &operation)
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

avm::LinkId arithmetic_entity(avm::LinkStore &store, avm::LinkId relation, avm::LinkId left, avm::LinkId right)
{
	return avm::encode_relation_entity(store, avm::RelationEntity{relation, left, right});
}

} // namespace

int main()
{
	avm::InMemoryLinkStore store;
	const avm::IntegerVocabulary vocabulary = avm::IntegerVocabulary::create(store);
	avm::validate_integer_vocabulary(store, vocabulary);

	std::vector<std::int64_t> values;
	values.push_back(0);
	values.push_back(1);
	values.push_back(-1);
	values.push_back(2);
	values.push_back(3);
	values.push_back(7);
	values.push_back(10);
	values.push_back(42);
	values.push_back(std::numeric_limits<std::int64_t>::max());
	values.push_back(std::numeric_limits<std::int64_t>::min());

	for (const std::int64_t value : values)
	{
		const std::size_t before_find = store.size();
		const auto missing_or_existing = avm::find_integer(store, vocabulary, value);
		assert(store.size() == before_find);
		if (value != 0)
			assert(!missing_or_existing.has_value());

		const avm::LinkId realized = avm::realize_integer(store, vocabulary, value);
		assert(avm::decode_integer(store, vocabulary, realized) == value);
		assert(avm::is_integer(store, vocabulary, realized));
		assert(avm::find_integer(store, vocabulary, value) == realized);

		const std::size_t before_repeat = store.size();
		assert(avm::realize_integer(store, vocabulary, value) == realized);
		assert(store.size() == before_repeat);
	}

	const avm::LinkId arbitrary_point = store.create_point();
	const std::size_t before_bad_read = store.size();
	assert(decode_rejected(store, vocabulary, arbitrary_point));
	assert(!avm::is_integer(store, vocabulary, arbitrary_point));
	assert(store.size() == before_bad_read);

	const avm::LinkId leading_zero = store.intern(vocabulary.bit_zero, vocabulary.magnitude_end);
	const avm::LinkId malformed_integer = store.intern(vocabulary.positive, leading_zero);
	const std::size_t before_malformed_read = store.size();
	assert(decode_rejected(store, vocabulary, malformed_integer));
	assert(store.size() == before_malformed_read);

	avm::Executor executor(store);
	avm::register_integer_arithmetic(executor, vocabulary);

	const avm::LinkId zero = avm::realize_integer(store, vocabulary, 0);
	const avm::LinkId one = avm::realize_integer(store, vocabulary, 1);
	const avm::LinkId two = avm::realize_integer(store, vocabulary, 2);
	const avm::LinkId three = avm::realize_integer(store, vocabulary, 3);
	const avm::LinkId four = avm::realize_integer(store, vocabulary, 4);
	const avm::LinkId six = avm::realize_integer(store, vocabulary, 6);
	const avm::LinkId seven = avm::realize_integer(store, vocabulary, 7);
	const avm::LinkId eight = avm::realize_integer(store, vocabulary, 8);
	const avm::LinkId nine = avm::realize_integer(store, vocabulary, 9);

	const avm::LinkId add_7_3 = arithmetic_entity(store, vocabulary.add_relation, seven, three);
	const avm::LinkId ten = executor.execute(add_7_3);
	assert(avm::decode_integer(store, vocabulary, ten) == 10);

	const avm::LinkId subtract_9_4 = arithmetic_entity(store, vocabulary.subtract_relation, nine, four);
	assert(avm::decode_integer(store, vocabulary, executor.execute(subtract_9_4)) == 5);

	const avm::LinkId subtract_0_7 = arithmetic_entity(store, vocabulary.subtract_relation, zero, seven);
	assert(avm::decode_integer(store, vocabulary, executor.execute(subtract_0_7)) == -7);

	const avm::LinkId multiply_6_7 = arithmetic_entity(store, vocabulary.multiply_relation, six, seven);
	assert(avm::decode_integer(store, vocabulary, executor.execute(multiply_6_7)) == 42);

	const avm::LinkId divide_8_2 = arithmetic_entity(store, vocabulary.divide_relation, eight, two);
	assert(avm::decode_integer(store, vocabulary, executor.execute(divide_8_2)) == 4);

	const avm::LinkId minus_seven = avm::realize_integer(store, vocabulary, -7);
	const avm::LinkId divide_negative = arithmetic_entity(store, vocabulary.divide_relation, minus_seven, two);
	assert(avm::decode_integer(store, vocabulary, executor.execute(divide_negative)) == -3);

	const avm::SemanticContextView root = avm::SemanticContextView::root(avm::SemanticContextFrame{
	    store.create_point(),
	    zero,
	    store.create_point(),
	    store.create_point(),
	});
	const avm::ExecutionOutcome semantic_add = executor.execute_outcome_in_context(add_7_3, root);
	assert(semantic_add.result == ten);
	assert(semantic_add.semantic.role(avm::SemanticContextRole::RelationState) == ten);
	assert(root.role(avm::SemanticContextRole::RelationState) == zero);

	const avm::LinkId int_max = avm::realize_integer(store, vocabulary, std::numeric_limits<std::int64_t>::max());
	const avm::LinkId int_min = avm::realize_integer(store, vocabulary, std::numeric_limits<std::int64_t>::min());

	const avm::LinkId overflow_add = arithmetic_entity(store, vocabulary.add_relation, int_max, one);
	const std::size_t before_overflow_add = store.size();
	assert(operation_rejected([&] { static_cast<void>(executor.execute(overflow_add)); }));
	assert(store.size() == before_overflow_add);

	const avm::LinkId overflow_subtract = arithmetic_entity(store, vocabulary.subtract_relation, int_min, one);
	const std::size_t before_overflow_subtract = store.size();
	assert(operation_rejected([&] { static_cast<void>(executor.execute(overflow_subtract)); }));
	assert(store.size() == before_overflow_subtract);

	const avm::LinkId overflow_multiply = arithmetic_entity(store, vocabulary.multiply_relation, int_max, two);
	const std::size_t before_overflow_multiply = store.size();
	assert(operation_rejected([&] { static_cast<void>(executor.execute(overflow_multiply)); }));
	assert(store.size() == before_overflow_multiply);

	const avm::LinkId divide_by_zero = arithmetic_entity(store, vocabulary.divide_relation, one, zero);
	const std::size_t before_divide_zero = store.size();
	assert(operation_rejected([&] { static_cast<void>(executor.execute(divide_by_zero)); }));
	assert(store.size() == before_divide_zero);

	const avm::LinkId minus_one = avm::realize_integer(store, vocabulary, -1);
	const avm::LinkId overflow_divide = arithmetic_entity(store, vocabulary.divide_relation, int_min, minus_one);
	const std::size_t before_overflow_divide = store.size();
	assert(operation_rejected([&] { static_cast<void>(executor.execute(overflow_divide)); }));
	assert(store.size() == before_overflow_divide);

	const std::filesystem::path persistent_path =
	    std::filesystem::temp_directory_path() / "avm_integer_value_test.links";
	std::filesystem::remove(persistent_path);

	std::optional<avm::IntegerVocabulary> persistent_vocabulary;
	avm::LinkId persistent_42 = avm::invalid_link_id;
	{
		avm::PersistentLinkStore persistent_store(persistent_path);
		persistent_vocabulary = avm::IntegerVocabulary::create(persistent_store);
		persistent_42 = avm::realize_integer(persistent_store, *persistent_vocabulary, 42);
		assert(avm::decode_integer(persistent_store, *persistent_vocabulary, persistent_42) == 42);
	}
	{
		avm::PersistentLinkStore reopened(persistent_path);
		assert(avm::decode_integer(reopened, *persistent_vocabulary, persistent_42) == 42);
		assert(avm::find_integer(reopened, *persistent_vocabulary, 42) == persistent_42);
		const std::size_t before_repeat = reopened.size();
		assert(avm::realize_integer(reopened, *persistent_vocabulary, 42) == persistent_42);
		assert(reopened.size() == before_repeat);
	}
	std::filesystem::remove(persistent_path);

	return 0;
}
