#pragma once

#include "avm/program_model.h"

#include "nlohmann/json.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace avm
{

struct JsonValueVocabulary
{
	LinkId unit;
	LinkId nil;
	LinkId null_value;
	LinkId true_value;
	LinkId false_value;
	LinkId array_relation;
	LinkId byte_relation;
	LinkId string_relation;
	LinkId unsigned_relation;
	LinkId integer_relation;
	LinkId float_relation;
	LinkId object_relation;
	LinkId entry_relation;

	static JsonValueVocabulary create(LinkStore &store)
	{
		return JsonValueVocabulary{
		    store.create_point(), store.create_point(), store.create_point(), store.create_point(),
		    store.create_point(), store.create_point(), store.create_point(), store.create_point(),
		    store.create_point(), store.create_point(), store.create_point(), store.create_point(),
		    store.create_point(),
		};
	}
};

class JsonValueCodec
{
public:
	using Json = nlohmann::json;

	explicit JsonValueCodec(LinkStore &store) : JsonValueCodec(store, JsonValueVocabulary::create(store)) {}

	JsonValueCodec(LinkStore &store, JsonValueVocabulary vocabulary) : store_(store), vocabulary_(vocabulary)
	{
		validate_vocabulary();
	}

	const JsonValueVocabulary &vocabulary() const { return vocabulary_; }

	LinkId encode(const Json &value)
	{
		switch (value.type())
		{
		case Json::value_t::null:
			return vocabulary_.null_value;
		case Json::value_t::boolean:
			return value.get<bool>() ? vocabulary_.true_value : vocabulary_.false_value;
		case Json::value_t::array:
			return encode_array(value);
		case Json::value_t::string:
			return encode_string(value.get<std::string>());
		case Json::value_t::number_unsigned:
			return encode_unsigned(value.get<Json::number_unsigned_t>(), vocabulary_.unsigned_relation);
		case Json::value_t::number_integer:
		{
			const Json::number_integer_t number = value.get<Json::number_integer_t>();
			return encode_unsigned(std::bit_cast<Json::number_unsigned_t>(number), vocabulary_.integer_relation);
		}
		case Json::value_t::number_float:
		{
			const Json::number_float_t number = value.get<Json::number_float_t>();
			return encode_unsigned(std::bit_cast<Json::number_unsigned_t>(number), vocabulary_.float_relation);
		}
		case Json::value_t::object:
			return encode_object(value);
		default:
			throw std::invalid_argument("unsupported JSON value type");
		}
	}

	Json decode(LinkId value) const
	{
		if (value == vocabulary_.null_value)
			return Json(nullptr);
		if (value == vocabulary_.true_value)
			return Json(true);
		if (value == vocabulary_.false_value)
			return Json(false);

		const RelationEntity entity = decode_relation_entity(store_, value);
		if (entity.subject != vocabulary_.unit)
			throw std::runtime_error("JSON value entity has an invalid subject");

		if (entity.relation == vocabulary_.array_relation)
			return decode_array(entity.object);
		if (entity.relation == vocabulary_.string_relation)
			return Json(decode_string(entity.object));
		if (entity.relation == vocabulary_.unsigned_relation)
			return Json(decode_unsigned(entity.object));
		if (entity.relation == vocabulary_.integer_relation)
			return Json(std::bit_cast<Json::number_integer_t>(decode_unsigned(entity.object)));
		if (entity.relation == vocabulary_.float_relation)
			return Json(std::bit_cast<Json::number_float_t>(decode_unsigned(entity.object)));
		if (entity.relation == vocabulary_.object_relation)
			return decode_object(entity.object);

		throw std::runtime_error("LinkId is not a JSON value entity");
	}

private:
	void validate_vocabulary() const
	{
		const std::vector<LinkId> ids{
		    vocabulary_.unit,
		    vocabulary_.nil,
		    vocabulary_.null_value,
		    vocabulary_.true_value,
		    vocabulary_.false_value,
		    vocabulary_.array_relation,
		    vocabulary_.byte_relation,
		    vocabulary_.string_relation,
		    vocabulary_.unsigned_relation,
		    vocabulary_.integer_relation,
		    vocabulary_.float_relation,
		    vocabulary_.object_relation,
		    vocabulary_.entry_relation,
		};

		std::set<LinkId> unique;
		for (const LinkId id : ids)
		{
			if (!store_.contains(id))
				throw std::invalid_argument("JSON value vocabulary contains an unknown LinkId");
			if (!unique.insert(id).second)
				throw std::invalid_argument("JSON value vocabulary identities must be distinct");
		}
	}

	LinkId wrap(LinkId relation, LinkId payload)
	{
		return encode_relation_entity(store_, RelationEntity{relation, vocabulary_.unit, payload});
	}

	LinkId encode_array(const Json &value)
	{
		std::vector<LinkId> items;
		items.reserve(value.size());
		for (const Json &item : value)
			items.push_back(encode(item));
		return wrap(vocabulary_.array_relation, encode_link_list(store_, vocabulary_.nil, items));
	}

	Json decode_array(LinkId payload) const
	{
		Json result = Json::array();
		for (const LinkId item : decode_link_list(store_, vocabulary_.nil, payload))
			result.push_back(decode(item));
		return result;
	}

	LinkId encode_byte(std::uint8_t byte)
	{
		std::vector<LinkId> bits;
		bits.reserve(8);
		for (std::uint8_t mask = 1; mask != 0; mask = static_cast<std::uint8_t>(mask << 1))
			bits.push_back((byte & mask) != 0 ? vocabulary_.true_value : vocabulary_.false_value);
		return wrap(vocabulary_.byte_relation, encode_link_list(store_, vocabulary_.nil, bits));
	}

	std::uint8_t decode_byte(LinkId value) const
	{
		const RelationEntity entity = decode_relation_entity(store_, value);
		if (entity.relation != vocabulary_.byte_relation || entity.subject != vocabulary_.unit)
			throw std::runtime_error("JSON string contains a non-byte entity");

		const std::vector<LinkId> bits = decode_link_list(store_, vocabulary_.nil, entity.object);
		if (bits.size() != 8)
			throw std::runtime_error("JSON byte entity must contain exactly eight bits");

		std::uint8_t byte = 0;
		std::uint8_t mask = 1;
		for (const LinkId bit : bits)
		{
			if (bit == vocabulary_.true_value)
				byte = static_cast<std::uint8_t>(byte | mask);
			else if (bit != vocabulary_.false_value)
				throw std::runtime_error("JSON byte entity contains a non-Boolean bit");
			mask = static_cast<std::uint8_t>(mask << 1);
		}
		return byte;
	}

	LinkId encode_string(const std::string &text)
	{
		std::vector<LinkId> bytes;
		bytes.reserve(text.size());
		for (const unsigned char byte : text)
			bytes.push_back(encode_byte(static_cast<std::uint8_t>(byte)));
		return wrap(vocabulary_.string_relation, encode_link_list(store_, vocabulary_.nil, bytes));
	}

	std::string decode_string(LinkId payload) const
	{
		std::string text;
		const std::vector<LinkId> bytes = decode_link_list(store_, vocabulary_.nil, payload);
		text.reserve(bytes.size());
		for (const LinkId byte : bytes)
			text.push_back(static_cast<char>(decode_byte(byte)));
		return text;
	}

	LinkId encode_unsigned(Json::number_unsigned_t number, LinkId relation)
	{
		static_assert(sizeof(Json::number_unsigned_t) == 8);
		std::vector<LinkId> bits;
		bits.reserve(64);
		for (Json::number_unsigned_t mask = 1; mask != 0; mask <<= 1)
			bits.push_back((number & mask) != 0 ? vocabulary_.true_value : vocabulary_.false_value);
		return wrap(relation, encode_link_list(store_, vocabulary_.nil, bits));
	}

	Json::number_unsigned_t decode_unsigned(LinkId payload) const
	{
		const std::vector<LinkId> bits = decode_link_list(store_, vocabulary_.nil, payload);
		if (bits.size() != 64)
			throw std::runtime_error("JSON number entity must contain exactly 64 bits");

		Json::number_unsigned_t number = 0;
		Json::number_unsigned_t mask = 1;
		for (const LinkId bit : bits)
		{
			if (bit == vocabulary_.true_value)
				number |= mask;
			else if (bit != vocabulary_.false_value)
				throw std::runtime_error("JSON number entity contains a non-Boolean bit");
			mask <<= 1;
		}
		return number;
	}

	LinkId encode_object(const Json &value)
	{
		std::vector<LinkId> entries;
		entries.reserve(value.size());
		for (auto item = value.begin(); item != value.end(); ++item)
		{
			const LinkId key = encode_string(item.key());
			const LinkId object_value = encode(item.value());
			entries.push_back(
			    encode_relation_entity(store_, RelationEntity{vocabulary_.entry_relation, key, object_value}));
		}
		return wrap(vocabulary_.object_relation, encode_link_list(store_, vocabulary_.nil, entries));
	}

	Json decode_object(LinkId payload) const
	{
		Json result = Json::object();
		for (const LinkId entry : decode_link_list(store_, vocabulary_.nil, payload))
		{
			const RelationEntity decoded = decode_relation_entity(store_, entry);
			if (decoded.relation != vocabulary_.entry_relation)
				throw std::runtime_error("JSON object contains a non-entry entity");

			const RelationEntity key = decode_relation_entity(store_, decoded.subject);
			if (key.relation != vocabulary_.string_relation || key.subject != vocabulary_.unit)
				throw std::runtime_error("JSON object entry key is not a string");
			result[decode_string(key.object)] = decode(decoded.object);
		}
		return result;
	}

	LinkStore &store_;
	JsonValueVocabulary vocabulary_;
};

} // namespace avm
