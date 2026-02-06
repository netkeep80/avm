#include "avm.h"

#include <memory>
#include <iostream>
#include <string>
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
4. сериализация/десериализация number..............V
5. сериализация/десериализация string..............V
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

//	Оператор относительной адресации [] / Relative addressing operator []
//	Выполняет поиск значения в entity map ассоциации
//	func[arg] — одномерная адресация (NOT[True] = False)
//	func[arg1][arg2] — многомерная адресация через цепочку (AND[True][False] = False)
rel_t *eval(rel_t *func, rel_t *arg)
{
	if (!func || !arg)
		return rel_t::E;
	auto it = func->find(arg);
	if (it != func->end())
		return it->second;
	return rel_t::E; //	не найдено — возвращаем null (E)
}

//	Вычисление функции с двумя аргументами через вложенную адресацию
//	func[arg1][arg2] = eval(eval(func, arg1), arg2)
rel_t *eval(rel_t *func, rel_t *arg1, rel_t *arg2)
{
	return eval(eval(func, arg1), arg2);
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

rel_t *import_json(const json &j)
{
	switch (j.type())
	{
	case json::value_t::null: //	null - означает отсутствие сущности
		return rel_t::E;

	case json::value_t::boolean:
		if (j.get<bool>())
			return rel_t::True;
		else
			return rel_t::False;

	case json::value_t::array: //	последовательность элементов, представленная цепочкой rel(rel(prev, element), R)
	{
		auto array = rel_t::E;

		for (auto &it : j)
			array = rel_t::rel(rel_t::rel(array, import_json(it)), rel_t::R);
		return array;
	}

	case json::value_t::string: //	строка как последовательность символов (каждый символ — битовая последовательность)
	{
		auto str = j.get<string>();
		auto array = rel_t::E; //	начало массива

		for (auto &it : str)
		{
			uint8_t val = *reinterpret_cast<uint8_t *>(&it);
			auto sing = rel_t::E;
			for (uint8_t i = 1; i; i <<= 1)
				sing = rel_t::rel(rel_t::rel(sing, (val & i) ? rel_t::True : rel_t::False), rel_t::R);
			array = rel_t::rel(rel_t::rel(array, sing), rel_t::R);
		}
		return array = rel_t::rel(array, rel_t::String);
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

	case json::value_t::object: //	объект как последовательность пар (ключ, значение)
	{
		auto array = rel_t::E;

		for (auto it = j.begin(); it != j.end(); ++it)
		{
			auto key = import_json(json(it.key()));
			auto value = import_json(it.value());
			auto pair = rel_t::rel(key, value);
			array = rel_t::rel(rel_t::rel(array, pair), rel_t::R);
		}
		return array = rel_t::rel(array, rel_t::Object);
	}

	default:
		return rel_t::E;
	}
}

//	Интерпретатор выражений / Expression interpreter
//	Вычисляет логические выражения, представленные как JSON, в МАО
//	Формат выражений:
//	  true, false, null — примитивные значения
//	  {"Not": [expr]} — логическое НЕ
//	  {"And": [expr1, expr2]} — логическое И
//	  {"Or": [expr1, expr2]} — логическое ИЛИ
//	  Вложенные выражения: {"Not": [{"And": [true, false]}]} = NOT[AND[True][False]] = True
//
//	Поиск оператора по имени в базовом словаре
rel_t *resolve_operator(const string &name)
{
	if (name == "Not")
		return rel_t::Not;
	if (name == "And")
		return rel_t::And;
	if (name == "Or")
		return rel_t::Or;
	return nullptr;
}

//	Рекурсивная интерпретация JSON-выражения
//	Возвращает результат вычисления как ARM-сущность
rel_t *interpret(const json &expr)
{
	switch (expr.type())
	{
	case json::value_t::null:
		return rel_t::E;

	case json::value_t::boolean:
		return expr.get<bool>() ? rel_t::True : rel_t::False;

	case json::value_t::object:
	{
		//	Объект с одним ключом — оператор с аргументами
		//	{"Not": [true]} или {"And": [true, false]}
		if (expr.size() != 1)
			return rel_t::E; //	некорректное выражение

		auto it = expr.begin();
		const string &op_name = it.key();
		const json &args = it.value();

		rel_t *op = resolve_operator(op_name);
		if (!op)
			return rel_t::E; //	неизвестный оператор

		if (!args.is_array() || args.empty())
			return rel_t::E; //	аргументы должны быть непустым массивом

		//	Вычисляем аргументы рекурсивно и применяем оператор
		//	Унарный оператор: func[arg]
		if (args.size() == 1)
			return eval(op, interpret(args[0]));

		//	Бинарный оператор: func[arg1][arg2]
		if (args.size() == 2)
			return eval(op, interpret(args[0]), interpret(args[1]));

		//	N-арный оператор: последовательное применение func[a1][a2]...[aN]
		rel_t *result = eval(op, interpret(args[0]));
		for (size_t i = 1; i < args.size(); ++i)
			result = eval(result, interpret(args[i]));
		return result;
	}

	default:
		//	Для остальных типов (числа, строки, массивы) — импортируем как данные
		return import_json(expr);
	}
}

void export_json(const rel_t *ent, json &j);

void export_object_chain(const rel_t *node, json &j)
{
	if (node == rel_t::E)
		return;
	if (node->sub != rel_t::R)
		return;
	//	node->obj — пара (prev, pair_rel), node->sub == R
	export_object_chain(node->obj->obj, j); //	рекурсия по prev

	//	node->obj->sub — это rel(key, value)
	auto pair_rel = node->obj->sub;
	json key_json, value_json;
	export_json(pair_rel->obj, key_json); //	экспорт ключа
	export_json(pair_rel->sub, value_json); //	экспорт значения
	if (key_json.is_string())
		j[key_json.get<string>()] = value_json;
}

void export_seq(const rel_t *ent, json &j)
{
	//	Рекурсивный экспорт цепочки rel(rel(prev, element), R) в json массив
	if (ent == rel_t::E) //	конец цепочки
	{
		j = json::array();
		return;
	}
	if (ent->sub != rel_t::R) //	не является шагом последовательности
	{
		j = json::array();
		return;
	}
	//	ent->obj — пара (prev, element), ent->sub == R
	export_seq(ent->obj->obj, j); //	рекурсия по prev
	if (j.is_null())
		j = json::array();
	json last;
	export_json(ent->obj->sub, last); //	экспорт текущего элемента
	j.push_back(last);
}

void export_json(const rel_t *ent, json &j)
{
	if (ent == rel_t::E)		  //	R[E] — null
		j = json();
	else if (ent == rel_t::True)  //	R[R] — true
		j = json(true);
	else if (ent == rel_t::False) //	E[R] — false
		j = json(false);
	else if (ent->sub == rel_t::R) //	шаг последовательности (массив)
	{
		export_seq(ent, j);
	}
	else if (ent->sub == rel_t::String) //	строка sub[String]
	{
		export_seq(ent->obj, j);
		if (j.is_array())
		{
			json::string_t str{};
			for (auto &sing : j)
				if (sing.is_array())
				{
					uint8_t val{}, i{1};
					for (auto &it : sing)
					{
						if (it.is_boolean())
							if (it.get<bool>())
								val |= i;
						i <<= 1;
					}
					str += *reinterpret_cast<char *>(&val);
				}
			j = json(str);
		}
	}
	else if (ent->sub == rel_t::Unsigned) //	sub[Unsigned]
	{
		export_seq(ent->obj, j);
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
	else if (ent->sub == rel_t::Integer) //	sub[Integer]
	{
		export_seq(ent->obj, j);
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
	else if (ent->sub == rel_t::Float) //	sub[Float]
	{
		export_seq(ent->obj, j);
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
	else if (ent->sub == rel_t::Object) //	sub[Object]
	{
		j = json::object();
		export_object_chain(ent->obj, j);
	}
	else
		j = json();
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

#ifndef AVM_NO_MAIN
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
     Associative Virtual Machine [Version 0.0.3]
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
		//	Проверяем, является ли вход выражением для интерпретации
		//	Выражение — JSON объект с одним ключом-оператором (Not, And, Or)
		bool is_expression = root.is_object() && root.size() == 1 &&
			resolve_operator(root.begin().key()) != nullptr;
		rel_t *root_ent;
		if (is_expression)
		{
			//	Интерпретируем выражение
			root_ent = interpret(root);
		}
		else
		{
			//	Импортируем как данные
			root_ent = import_json(root);
		}
		export_json(root_ent, res);
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
#endif // AVM_NO_MAIN
