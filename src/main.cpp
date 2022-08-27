#include "avm.h"

#include <memory>
#include <iostream>
#include <string>
#include <iostream>
#include <fstream>

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

#define new_ent(name)                                   \
	auto unique##name = unique_ptr<ent_os>(new ent_os()); \
	ent_os &name = *unique##name.get();

#define new_obj(name)                                   \
	auto unique##name = unique_ptr<obj_er>(new obj_er()); \
	obj_er &name = *unique##name.get();

#define new_rel(name)                                   \
	auto unique##name = unique_ptr<rel_so>(new rel_so()); \
	rel_so &name = *unique##name.get();

#define new_sub(name)                                   \
	auto unique##name = unique_ptr<sub_re>(new sub_re()); \
	sub_re &name = *unique##name.get();

//	root entity
new_ent(Ent);
new_obj(Obj);
//	root relation
new_rel(Rel);
new_sub(Sub);

//	json = null
new_ent(Null);

//new_ent(Bool);
new_ent(True);
new_ent(False);

new_ent(Then);

void build_base_vocabulary()
{
	//	Configure base vocabulary
	Ent.update(&Ent, &Ent);

	Ent.update(&Obj, &Sub);
	Obj.update(&Ent, &Rel);
	Sub.update(&Rel, &Ent);
	Rel.update(&Sub, &Obj);

	Ent.obj->ent->obj;
	Ent.obj->ent->sub;
	Ent.obj->rel->sub;
	Ent.obj->rel->obj;
	Ent.sub->rel;
	Ent.sub->ent;

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

void import_json(sub_re &s, const json &j)
{
	switch (j.type())
	{
	case json::value_t::null: //	null - означает отсутствие отношения
		s.update(s.rel, &Null);
		return;

	case json::value_t::boolean:
		if (j.get<json::boolean_t>())
			s.update(s.rel, &True);
		else
			s.update(s.rel, &False);
		return;

	case json::value_t::string:
		return;

	case json::value_t::array: //	лямбда вектор, который управляет последовательным изменением проекции сущности
	{
		auto it = j.begin();
		auto end = j.end();

		//Then
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

void export_json(const sub_re &s, json &j)
{
	if (s.ent == &Null)
		j = json();
	else if (s.ent == &True)
		j = json(true);
	else if (s.ent == &False)
		j = json(false);
	else
		j = json("unknown entity");
}

int main(int argc, char *argv[])
{
	//	links db test
	UnitedMemoryLinks<uint64_t> links64("db.uint64_t.links"s);

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
		new_sub(root_sub);
		root_sub.update(&Rel, &Null);

		get_json(root, entry_point);
		import_json(root_sub, root);
		export_json(root_sub, res);
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
