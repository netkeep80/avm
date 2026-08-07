#include "avm/legacy_json_compat.h"
#include "avm/json_compat.h"

#include <bit>
#include <cstdint>
#include <memory>
#include <string>

using namespace std;

namespace
{

unique_ptr<avm::JsonCompatibilitySession> compatibility_session;

avm::JsonCompatibilitySession &session()
{
	if (!compatibility_session)
		compatibility_session = make_unique<avm::JsonCompatibilitySession>();
	return *compatibility_session;
}

void export_object_chain(const rel_t *node, json &value);
void export_seq(const rel_t *entity, json &value);

} // namespace

rel_t *eval(rel_t *function, rel_t *argument)
{
	if (!function || !argument)
		return rel_t::E;
	const auto found = function->find(argument);
	if (found != function->end())
		return found->second;
	return rel_t::E;
}

rel_t *eval(rel_t *function, rel_t *first, rel_t *second)
{
	return eval(eval(function, first), second);
}

rel_t *import_json(const json &value)
{
	static_assert(sizeof(json::number_integer_t) == sizeof(json::number_unsigned_t));
	static_assert(sizeof(json::number_float_t) == sizeof(json::number_unsigned_t));

	switch (value.type())
	{
	case json::value_t::null:
		return rel_t::E;

	case json::value_t::boolean:
		return value.get<bool>() ? rel_t::True : rel_t::False;

	case json::value_t::array:
	{
		auto array = rel_t::E;
		for (const auto &item : value)
			array = rel_t::rel(rel_t::rel(array, import_json(item)), rel_t::R);
		return array;
	}

	case json::value_t::string:
	{
		const auto text = value.get<string>();
		auto array = rel_t::E;
		for (const char character : text)
		{
			const uint8_t byte = static_cast<uint8_t>(static_cast<unsigned char>(character));
			auto bits = rel_t::E;
			for (uint8_t mask = 1; mask; mask <<= 1)
				bits = rel_t::rel(rel_t::rel(bits, (byte & mask) ? rel_t::True : rel_t::False), rel_t::R);
			array = rel_t::rel(rel_t::rel(array, bits), rel_t::R);
		}
		return rel_t::rel(array, rel_t::String);
	}

	case json::value_t::number_unsigned:
	{
		const json::number_unsigned_t number = value.get<json::number_unsigned_t>();
		auto bits = rel_t::E;
		for (json::number_unsigned_t mask = 1; mask; mask <<= 1)
			bits = rel_t::rel(rel_t::rel(bits, (number & mask) ? rel_t::True : rel_t::False), rel_t::R);
		return rel_t::rel(bits, rel_t::Unsigned);
	}

	case json::value_t::number_integer:
	{
		const json::number_integer_t signed_number = value.get<json::number_integer_t>();
		const json::number_unsigned_t number = std::bit_cast<json::number_unsigned_t>(signed_number);
		auto bits = rel_t::E;
		for (json::number_unsigned_t mask = 1; mask; mask <<= 1)
			bits = rel_t::rel(rel_t::rel(bits, (number & mask) ? rel_t::True : rel_t::False), rel_t::R);
		return rel_t::rel(bits, rel_t::Integer);
	}

	case json::value_t::number_float:
	{
		const json::number_float_t floating_number = value.get<json::number_float_t>();
		const json::number_unsigned_t number = std::bit_cast<json::number_unsigned_t>(floating_number);
		auto bits = rel_t::E;
		for (json::number_unsigned_t mask = 1; mask; mask <<= 1)
			bits = rel_t::rel(rel_t::rel(bits, (number & mask) ? rel_t::True : rel_t::False), rel_t::R);
		return rel_t::rel(bits, rel_t::Float);
	}

	case json::value_t::object:
	{
		auto array = rel_t::E;
		for (auto item = value.begin(); item != value.end(); ++item)
		{
			auto key = import_json(json(item.key()));
			auto object_value = import_json(item.value());
			auto pair = rel_t::rel(key, object_value);
			array = rel_t::rel(rel_t::rel(array, pair), rel_t::R);
		}
		return rel_t::rel(array, rel_t::Object);
	}

	default:
		return rel_t::E;
	}
}

void clear_func_env()
{
	compatibility_session.reset();
}

rel_t *interpret(const json &expression)
{
	return import_json(session().interpret(expression));
}

namespace
{

void export_object_chain(const rel_t *node, json &value)
{
	if (node == rel_t::E || node->sub != rel_t::R)
		return;

	export_object_chain(node->obj->obj, value);

	const auto pair = node->obj->sub;
	json key;
	json object_value;
	export_json(pair->obj, key);
	export_json(pair->sub, object_value);
	if (key.is_string())
		value[key.get<string>()] = object_value;
}

void export_seq(const rel_t *entity, json &value)
{
	if (entity == rel_t::E)
	{
		value = json::array();
		return;
	}
	if (entity->sub != rel_t::R)
	{
		value = json::array();
		return;
	}

	export_seq(entity->obj->obj, value);
	if (value.is_null())
		value = json::array();
	json last;
	export_json(entity->obj->sub, last);
	value.push_back(last);
}

} // namespace

void export_json(const rel_t *entity, json &value)
{
	static_assert(sizeof(json::number_integer_t) == sizeof(json::number_unsigned_t));
	static_assert(sizeof(json::number_float_t) == sizeof(json::number_unsigned_t));

	if (entity == rel_t::E)
		value = json();
	else if (entity == rel_t::True)
		value = json(true);
	else if (entity == rel_t::False)
		value = json(false);
	else if (entity->sub == rel_t::R)
	{
		export_seq(entity, value);
	}
	else if (entity->sub == rel_t::String)
	{
		export_seq(entity->obj, value);
		if (value.is_array())
		{
			json::string_t text{};
			for (const auto &bits : value)
			{
				if (!bits.is_array())
					continue;

				uint8_t byte{};
				uint8_t mask{1};
				for (const auto &bit : bits)
				{
					if (bit.is_boolean() && bit.get<bool>())
						byte |= mask;
					mask <<= 1;
				}
				text += static_cast<char>(byte);
			}
			value = json(text);
		}
	}
	else if (entity->sub == rel_t::Unsigned)
	{
		export_seq(entity->obj, value);
		if (value.is_array())
		{
			json::number_unsigned_t number{};
			json::number_unsigned_t mask{1};
			for (const auto &bit : value)
			{
				if (bit.is_boolean() && bit.get<bool>())
					number |= mask;
				mask <<= 1;
			}
			value = json(number);
		}
	}
	else if (entity->sub == rel_t::Integer)
	{
		export_seq(entity->obj, value);
		if (value.is_array())
		{
			json::number_unsigned_t number{};
			json::number_unsigned_t mask{1};
			for (const auto &bit : value)
			{
				if (bit.is_boolean() && bit.get<bool>())
					number |= mask;
				mask <<= 1;
			}
			value = json(std::bit_cast<json::number_integer_t>(number));
		}
	}
	else if (entity->sub == rel_t::Float)
	{
		export_seq(entity->obj, value);
		if (value.is_array())
		{
			json::number_unsigned_t number{};
			json::number_unsigned_t mask{1};
			for (const auto &bit : value)
			{
				if (bit.is_boolean() && bit.get<bool>())
					number |= mask;
				mask <<= 1;
			}
			value = json(std::bit_cast<json::number_float_t>(number));
		}
	}
	else if (entity->sub == rel_t::Object)
	{
		value = json::object();
		export_object_chain(entity->obj, value);
	}
	else
		value = json();
}
