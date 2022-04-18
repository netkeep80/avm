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

1. сериализация/десериализация bool
2. сериализация/десериализация number
3. сериализация/десериализация string

	Проблемы:

1. хранение и освобождение ent/rel

установка rel в true и приведение его к bool
true : { $obj : $ent, $rel : bool }
т.е. взять себя, преобразовать к bool и поместить в $rel
(True: Bool TrueRel)
(TrueRel: Bool TrueRel)

(False: Bool False)

(Null: Ent Null)
*/

#define new_ent(name)                                   \
	auto unique##name = unique_ptr<ent_t>(new ent_t()); \
	ent_t &name = *unique##name.get();

#define new_obj(name)                                   \
	auto unique##name = unique_ptr<obj_t>(new obj_t()); \
	obj_t &name = *unique##name.get();

#define new_rel(name)                                   \
	auto unique##name = unique_ptr<rel_t>(new rel_t()); \
	rel_t &name = *unique##name.get();

#define new_sub(name)                                   \
	auto unique##name = unique_ptr<sub_t>(new sub_t()); \
	sub_t &name = *unique##name.get();

//	root entity
new_ent(Ent);
new_obj(Obj);
//	root relation
new_rel(Rel);
new_sub(Sub);

//	json = null
new_ent(Null);

new_ent(Bool);
new_ent(True);
new_ent(False);

void build_base_vocabulary()
{
	//	Configure base vocabulary
	Ent.update(&Sub, &Obj);
	Obj.update(&Ent, &Rel);
	Rel.update(&Obj, &Sub);
	Sub.update(&Rel, &Ent);

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

void import_json(sub_t &s, const json &j)
{
	switch (j.type())
	{
	case json::value_t::null: //	null - означает отсутствие отношения, т.е. неизменность проекции
		s.update(s.rel, &Null);
		return;

	case json::value_t::boolean:
		if (j.get<json::boolean_t>())
			s.update(s.rel, &True);
		else
			s.update(s.rel, &False);
		return;

	case json::value_t::string: //	иерархический путь к json значению
		return;

	case json::value_t::array: //	лямбда вектор, который управляет последовательным изменением проекции сущности
	{
		auto it = j.begin();
		auto end = j.end();
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

void export_json(const sub_t &s, json &j)
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
          /                 V
     ,--> E +---------------O----.
    /     |                 +     \
   /      |  E = S x O x R  |      \
   |      |  R = O x S x E  |      |
   |      |  S = R x E x O  |      |
   \      |  O = E x R x S  |      /
    \     +                 |     /
     `----S---------------+ R <--`
          A                 |
           \               /
            \_____________/

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
