#pragma once
#include "avm.h"
#include <string>
#include <fstream>
#include <stdexcept>
#include <algorithm>

using namespace std;

inline void get_json(json &ent, const string &PathName)
{
	std::ifstream in(PathName.c_str());
	if (!in.good())
		throw runtime_error(__func__ + ": Can't load json from the "s + PathName + " file!");
	in >> ent;
}

inline void add_json(const json &ent, const string &PathName)
{
	std::ofstream out(PathName.c_str());
	if (!out.good())
		throw runtime_error(__func__ + ": Can't open "s + PathName + " file!"s);
	out << ent;
}

inline rel_t *import_json(const json &j)
{
	switch (j.type())
	{
	case json::value_t::null:
		return rel_t::rel();

	case json::value_t::boolean:
		if (j.get<bool>())
			return rel_t::True;
		else
			return rel_t::False;

	case json::value_t::array:
	{
		auto array = rel_t::R;
		auto lambda_func = [&array](const json& j) {
		    array = rel_t::rel(import_json(j), array);
		};
		std::for_each(j.rbegin(), j.rend(), lambda_func);
		return array;
	}

	case json::value_t::string:
	{
		auto str = j.get<string>();
		auto array = rel_t::E;

		for (auto &it : str)
		{
			uint8_t val = *reinterpret_cast<uint8_t *>(&it);
			for (uint8_t i = 1; i; i <<= 1)
				array = rel_t::rel(rel_t::rel(array, (val & i) ? rel_t::True : rel_t::False), rel_t::R);
		}
		return array = rel_t::rel(array, rel_t::E);
	}

	case json::value_t::number_unsigned:
	{
		json::number_unsigned_t val = j.get<json::number_unsigned_t>();
		auto array = rel_t::E;
		for (json::number_unsigned_t i = 1; i; i <<= 1)
			array = rel_t::rel(rel_t::rel(array, (val & i) ? rel_t::True : rel_t::False), rel_t::R);
		return array = rel_t::rel(array, rel_t::Unsigned);
	}

	case json::value_t::number_integer:
	{
		json::number_integer_t ival = j.get<json::number_integer_t>();
		json::number_unsigned_t val = *reinterpret_cast<json::number_unsigned_t *>(&ival);
		auto array = rel_t::E;
		for (json::number_unsigned_t i = 1; i; i <<= 1)
			array = rel_t::rel(rel_t::rel(array, (val & i) ? rel_t::True : rel_t::False), rel_t::R);
		return array = rel_t::rel(array, rel_t::Integer);
	}

	case json::value_t::number_float:
	{
		json::number_float_t fval = j.get<json::number_float_t>();
		json::number_unsigned_t val = *reinterpret_cast<json::number_unsigned_t *>(&fval);
		auto array = rel_t::E;
		for (json::number_unsigned_t i = 1; i; i <<= 1)
			array = rel_t::rel(rel_t::rel(array, (val & i) ? rel_t::True : rel_t::False), rel_t::R);
		return array = rel_t::rel(array, rel_t::Float);
	}

	case json::value_t::object:
		return rel_t::E;

	default:
		return rel_t::E;
	}
}

inline void export_json(const rel_t *ent, json &j)
{
	if (ent == rel_t::E)
		j = json();
	else if (ent == rel_t::R)
	{
		j = json::array();
		j.push_back(json());
	}
	else if (ent == rel_t::True)
		j = json(true);
	else if (ent == rel_t::False)
		j = json(false);
	else if (ent->obj == rel_t::R)
	{
		j = json::array();
		auto cur = ent;
		do
		{
			json last;
			export_json(cur->sub->obj, last);
			j.push_back(last);
		} while ((cur = cur->sub->sub) != rel_t::E);

		std::reverse(j.begin(), j.end());
	}
	else if (ent->obj == rel_t::E)
	{
		export_json(ent->sub, j);
		if (j.is_array())
		{
			json::string_t str{};
			uint8_t val{}, i{1};
			for (auto &it : j)
			{
				if (it.is_boolean())
					if (it.get<bool>())
						val |= i;

				if ((i <<= 1) == 0x00)
				{
					str += *reinterpret_cast<char *>(&val);
					i = 1;
					val = 0;
				}
			}
			j = json(str);
		}
	}
	else if (ent->obj == rel_t::Unsigned)
	{
		export_json(ent->sub, j);
		if (j.is_array())
		{
			json::number_unsigned_t val{}, i{1};
			for (auto &it : j)
			{
				if (it.is_boolean())
					if (it.get<bool>())
						val |= i;
				i <<= 1;
			}
			j = json(val);
		}
	}
	else if (ent->obj == rel_t::Integer)
	{
		export_json(ent->sub, j);
		if (j.is_array())
		{
			json::number_unsigned_t val{}, i{1};
			for (auto &it : j)
			{
				if (it.is_boolean())
					if (it.get<bool>())
						val |= i;
				i <<= 1;
			}
			j = json(*reinterpret_cast<json::number_integer_t *>(&val));
		}
	}
	else if (ent->obj == rel_t::Float)
	{
		export_json(ent->sub, j);
		if (j.is_array())
		{
			json::number_unsigned_t val{}, i{1};
			for (auto &it : j)
			{
				if (it.is_boolean())
					if (it.get<bool>())
						val |= i;
				i <<= 1;
			}
			j = json(*reinterpret_cast<json::number_float_t *>(&val));
		}
	}
	else
		j = json("is string");
}
