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

1. сериализация/десериализация bool................V
2. сериализация/десериализация array
3. сериализация/десериализация number
4. сериализация/десериализация string
5. сериализация/десериализация object


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

установка rel в true и приведение его к bool
true : { $obj : $ent, $rel : bool }
т.е. взять себя, преобразовать к bool и поместить в $rel
(True: Bool TrueRel)
(TrueRel: Bool TrueRel)

(False: Bool False)

(Null: Ent Null)
*/

#define new_rel(name)                                     \
	auto unique##name = unique_ptr<rel_t>(new rel_t()); \
	rel_t &name = *unique##name.get();

//	root entity
new_rel(Ent);
new_rel(Obj);
//	root relation
new_rel(Rel);
new_rel(Sub);

//	json = null
new_rel(Null);

// new_ent(Bool);
new_rel(True);
new_rel(False);

new_rel(Then);

void build_base_vocabulary()
{
	//	Configure base vocabulary
	Ent.update(&Obj, &Sub);
	Obj.update(&Ent, &Rel);
	Sub.update(&Rel, &Ent);
	Rel.update(&Sub, &Obj);

	//	известно что при { $rel : bool } текущее отношение должно быть преобразовано к типу bool
	//	либо приравлять rel в bool
	//	bool bool() { $rel = isEnt($rel) ? true : false; }
	// Bool.update(&Ent, &BoolRel);
	// True.update(&Bool, &TrueRel);
	// False.update(&Bool, &FalseRel);
}

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

void import_json(rel_t &s, const json &j)
{
	switch (j.type())
	{
	case json::value_t::null: //	null - означает отсутствие отношения
		s.update(s.sub, &Null);
		return;

	case json::value_t::boolean:
		if (j.get<json::boolean_t>())
			s.update(s.sub, &True);
		else
			s.update(s.sub, &False);
		return;

	case json::value_t::string:
		return;

	case json::value_t::array: //	лямбда вектор, который управляет последовательным изменением проекции сущности
	{
		auto it = j.begin();
		auto end = j.end();

		// Then
		return;
	}

	case json::value_t::object:
	{
		auto end = j.end();

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
		}
		return;
	}

	default:
		return;
	}
}


void export_json(const rel_t &s, json &j)
{
	if (s.obj == &Null)
		j = json();
	else if (s.obj == &True)
		j = json(true);
	else if (s.obj == &False)
		j = json(false);
	else
		j = json("unknown entity");
}

//	ent = "." = "[]"
//	obj = "[" = ".,"
//	sub = "]" = ",."
//	rel = "," = "]["

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
	//	base vocabulary
	build_base_vocabulary();

	try
	{
		//	создаём субъект для помещения в него корня с дефолтным значением Null
		new_rel(root_sub);
		root_sub.update(&Rel, &Null);

		get_json(root, entry_point);
		res = json::object();
		parse_json(root, res);
		// import_json(root_sub, root);
		// export_json(root_sub, res);
		add_json(res, "res.json"s);
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
