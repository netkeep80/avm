#include "avm.h"

#include <memory>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>
#include "str_switch/str_switch.h"

using namespace std;
using namespace Platform::Data::Doublets::Memory::United::Generic;

/*
	План:

Сделать версию которая десериализует json в АМО а потом сериализует
 на ней отработать преобразование и его корректность

1. сериализация/десериализация null................V
2. сериализация/десериализация bool................V
3. сериализация/десериализация array...............V
4. сериализация/десериализация number..............
5. сериализация/десериализация string..............
6. сериализация/десериализация object..............

Есть вариант для простых типов данных смоделировать память с линейной дресацией через бинарное дерево.
Можно использовать 16-тиричную систему адресов.
Либо использовать 2х уровневую система адресации:
1. последовательность бит (true/false) для описания байт
2. последовательность байт (букв) для описания чисел и строк

Для создания root json документа надо уметь создавать:
1. bit: false/true
2. array
3. number is array of bits
4. string is array of numbers
5. object is string tree



	Проблемы:

1. хранение и освобождение ent/rel
2. непонятно как представлять примитивные типы, такие как bool, числа и строки.
	На первое время есть вариант создать для данных типов встроенные сущности.
	Второй вариант использовать одно и тоже отношение для обозначения последовательности,
	но в зависимости от сущности первичного субъекта определять примитивный тип

	Размышления:

Если 1 бит есть значение bool, а число как битовая последовательность
используются для адресации в бинарном дереве,
а бинарное дерево образовано троичными сущностями,
то true/false должны выбирать между правой/левой веткой, т.е. obj/sub.
Это ещё одно подтверждение, что obj/sub однозначно соответствуют true/false,
а так же понятию правый/левый.

array это компактная сериализация дерева map, которая может использоваться для адресации в бинарном дереве.

*/

void get_json(json &ent, const string &PathName)
{
	std::ifstream in(PathName.c_str());
	if (!in.good())
		throw runtime_error(__func__ + ": Can't load json from the "s + PathName + " file!");
	in >> ent;
}

void add_json(const json &ent, const string &PathName)
{
	std::ofstream out(PathName.c_str());
	if (!out.good())
		throw runtime_error(__func__ + ": Can't open "s + PathName + " file!"s);
	out << ent;
}

rel_t *import_json(const json &j)
{
	switch (j.type())
	{
	case json::value_t::null: //	начало массива и конец строки
		return rel_t::E;

	case json::value_t::boolean:
		if (j.get<bool>())
			return rel_t::True;
		else
			return rel_t::False;

	case json::value_t::array: //	лямбда вектор, который управляет последовательным изменением отношения сущности
	{
		auto array = rel_t::E;

		for (auto &it : j)
			array = rel_t::rel(rel_t::rel(array, import_json(it)), rel_t::R);
		return array;
	}

	case json::value_t::string: //	битовая последовательность для описания строк
	{
		auto str = j.get<string>();
		auto array = rel_t::E; //	начало массива

		for (auto &it : str)
		{
			uint8_t val = *reinterpret_cast<uint8_t *>(&it);
			for (uint8_t i = 1; i; i <<= 1)
				array = rel_t::rel(rel_t::rel(array, (val & i) ? rel_t::True : rel_t::False), rel_t::R);
		}
		return array = rel_t::rel(array, rel_t::E); //	конец массива
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
	{
		/*auto end = j.end();

		if (auto ref = j.find("$ref"); ref != end)
		{ //	это ссылка на json значение
			if (ref->is_string())
			{
				// try { exec_ent($, string_ref_to<rval>($, ref->get_ref<string const&>())); }
				// catch (json& j) { throw json({ {ref->get_ref<string const&>(), j} }); }
			}
			else
				throw json({{"$ref", *ref}});
		}
		else if (auto rel = j.find("$rel"); rel != end)
		{ //	это сущность, которую надо исполнить в новом контексте?
			auto obj = j.find("$obj");
			auto sub = j.find("$sub");
		}
		else
		{ //	контроллер это лямбда структура, которая управляет параллельным проецированием сущностей
			auto it = j.begin();

			// for (auto &it : vct)
			// if (!it.exc.is_null())
			//  throw json({ {it.key, it.exc} });
		}*/
		return rel_t::E;
	}

	default:
		return rel_t::E;
	}
}

void export_json(const rel_t *ent, json &j)
{
	if (ent == rel_t::E)		  //	R[E]
		j = json();				  //	null
	else if (ent == rel_t::True)  //	E[R]
		j = json(true);			  //	true
	else if (ent == rel_t::False) //	R[R]
		j = json(false);		  //	false
	else if (ent == rel_t::R)	  //	E[E]
	{
		j = json::array(); //	[]
		j.push_back(json());
	}
	else if (ent->obj == rel_t::R) //	array
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
	else if (ent->obj == rel_t::E) //	битовая последовательность для описания строк
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
	else if (ent->obj == rel_t::Unsigned) //	sub[Unsigned]
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
	else if (ent->obj == rel_t::Integer) //	sub[Integer]
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
	else if (ent->obj == rel_t::Float) //	sub[Float]
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

size_t link_name(vector<json *> &sub, const string &str, size_t start_pos, size_t end_pos)
{
	if (end_pos > start_pos)
	{
		auto name = str.substr(start_pos, end_pos - start_pos);
		auto res = sub.back()->find(name);

		if (res == sub.back()->end())
			sub.back() = &((*sub.back())[name] = json::object());
		else
			sub.back() = &res.value();
	}
	return end_pos + 1;
}

json &parse_string(json &rel, const string &str) noexcept
{
	size_t pos = 0, last_pos = 0;
	vector<json *> sub;
	vector<size_t> start_pos; //	начальная позиция после [

	sub.push_back(&rel); //	по умолчанию субъектом является корневое отношение
	start_pos.push_back(pos);

	while (true) //	берём следующую букву имени
	{
		switch (str[pos])
		{
		case ',': //	следующая ассоциация
			sub.pop_back();
			last_pos = link_name(sub, str, start_pos.back(), pos);
			start_pos.pop_back();
		case '[': //	вложенная ассоциация
			start_pos.push_back(link_name(sub, str, last_pos, pos));
			sub.push_back(&rel);
			last_pos = start_pos.back();
			break;

		case '.': //	сущность
			start_pos.push_back(link_name(sub, str, last_pos, pos));
			sub.push_back(&rel);
			last_pos = start_pos.back();
		case ']': //	конец текущей вложенной ассоциации
			sub.pop_back();
			last_pos = link_name(sub, str, start_pos.back(), pos);
			start_pos.pop_back();
			break;

		case '\0':
			return *sub.back();

		default:
			break;
		}
		pos++;
	}
}

void parse_json(const json &j, json &r)
{
	switch (j.type())
	{
	case json::value_t::string:
	{
		parse_string(r, j.get_ref<string const &>());
		return;
	}

	case json::value_t::array:
	{
		auto it = j.cbegin();
		auto end = j.cend();
		for (; it != end; ++it)
			parse_json(*it, r);
		return;
	}

	default:
		return;
	}
}

int main(int argc, char *argv[])
{
	//	links db test
	// UnitedMemoryLinks<uint64_t> links64("db.uint64_t.links"s);

	json res;
	char *entry_point = NULL;

	switch (argc)
	{
	case 2:
		entry_point = argv[1];
		break;
	default:
		cout << R"(https://github.com/netkeep80/avm
     Associative Virtual Machine [Version 0.0.1]
             _____________
            /             \
           /               \
          /                 +
     ,--> E +---------------O----.
    /     |                 A     \
   /      |  E =(O x S)x R  |      \
   |      |  O =(E x R)x S  |      |
   |      |  S =(R x E)x O  |      |
   \      |  R =(S x O)x E  |      /
    \     V                 |     /
     `----S---------------+ R <--`
          +                 |
           \               /
            \_____________/

E = OS = (ER * RE)
O = ER = (OS * SO)
S = RE = (SO * OS)
R = SO = (RE * ER)

Licensed under the MIT License <http://opensource.org/licenses/MIT>
Copyright (c) 2022 Vertushkin Roman Pavlovich <https://vk.com/earthbirthbook>

Usage:
       avm.exe [entry_point]
		)";
		return 0; //	ok
	}

	json root;

	try
	{
		get_json(root, entry_point);
		res = json::object();
		// parse_json(root, res);
		//	импортируем в корневой контекст
		auto root_ent = import_json(root);
		// cout << root.dump() << endl;
		export_json(root_ent, res);
		// cout << res.dump() << endl;
		add_json(res, "res.json"s);
		std::cout << "rel_t::created() = " << rel_t::created() << std::endl;
		return 0; //	ok
	}
	catch (json &j)
	{
		cerr << j.dump(2);
	}
	catch (json::exception &e)
	{
		cerr << __func__ << "json::exception: "s + e.what() + ", id: "s + to_string(e.id);
	}
	catch (std::exception &e)
	{
		cerr << __func__ << "std::exception: "s + e.what();
	}
	catch (...)
	{
		cerr << __func__ << "unknown exception"s;
	}

	add_json(root, "rvm.dump.json"s);
	return 1; //	error
}
