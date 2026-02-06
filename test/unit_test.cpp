#include "avm.h"

#include <cassert>
#include <cmath>
#include <functional>
#include <iostream>
#include <string>

using namespace std;

//	Прототипы функций из main.cpp
rel_t *import_json(const json &j);
void export_json(const rel_t *ent, json &j);
rel_t *eval(rel_t *func, rel_t *arg);
rel_t *eval(rel_t *func, rel_t *arg1, rel_t *arg2);
rel_t *interpret(const json &expr);
void clear_func_env();

static int tests_passed = 0;
static int tests_failed = 0;

void check(bool condition, const string &test_name)
{
	if (condition)
	{
		tests_passed++;
	}
	else
	{
		tests_failed++;
		cerr << "FAIL: " << test_name << endl;
	}
}

//	Вспомогательная функция: import→export roundtrip
json roundtrip(const json &input)
{
	auto ent = import_json(input);
	json output;
	export_json(ent, output);
	return output;
}

//	=== Тесты базового словаря ===

void test_base_vocabulary()
{
	check(rel_t::R != nullptr, "R is not null");
	check(rel_t::E != nullptr, "E is not null");
	check(rel_t::True != nullptr, "True is not null");
	check(rel_t::False != nullptr, "False is not null");
	check(rel_t::Unsigned != nullptr, "Unsigned is not null");
	check(rel_t::Integer != nullptr, "Integer is not null");
	check(rel_t::Float != nullptr, "Float is not null");
	check(rel_t::String != nullptr, "String is not null");
	check(rel_t::Object != nullptr, "Object is not null");

	//	Все базовые элементы должны быть различными
	check(rel_t::R != rel_t::E, "R != E");
	check(rel_t::True != rel_t::False, "True != False");
	check(rel_t::R != rel_t::True, "R != True");
	check(rel_t::E != rel_t::False, "E != False");

	//	Проверяем структуру базовых связей
	check(rel_t::R->obj == rel_t::E, "R->obj == E");
	check(rel_t::R->sub == rel_t::E, "R->sub == E");
	check(rel_t::E->obj == rel_t::R, "E->obj == R");
	check(rel_t::E->sub == rel_t::E, "E->sub == E");
	check(rel_t::True->obj == rel_t::R, "True->obj == R");
	check(rel_t::True->sub == rel_t::R, "True->sub == R");
	check(rel_t::False->obj == rel_t::E, "False->obj == E");
	check(rel_t::False->sub == rel_t::R, "False->sub == R");
}

//	=== Тесты null ===

void test_null()
{
	json input = nullptr;
	auto ent = import_json(input);
	check(ent == rel_t::E, "null imports as E");

	json output = roundtrip(input);
	check(output.is_null(), "null roundtrips as null");
}

//	=== Тесты boolean ===

void test_boolean()
{
	auto ent_true = import_json(json(true));
	check(ent_true == rel_t::True, "true imports as True");

	auto ent_false = import_json(json(false));
	check(ent_false == rel_t::False, "false imports as False");

	check(roundtrip(json(true)) == json(true), "true roundtrips");
	check(roundtrip(json(false)) == json(false), "false roundtrips");
}

//	=== Тесты массивов ===

void test_empty_array()
{
	json input = json::array();
	auto ent = import_json(input);
	check(ent == rel_t::E, "empty array imports as E (same as null)");

	json output;
	export_json(ent, output);
	check(output.is_null(), "empty array exports as null (known limitation)");
}

void test_single_element_array()
{
	json input = json::array({true});
	json output = roundtrip(input);
	check(output == input, "[true] roundtrips");

	input = json::array({false});
	output = roundtrip(input);
	check(output == input, "[false] roundtrips");

	input = json::array({nullptr});
	output = roundtrip(input);
	check(output == input, "[null] roundtrips");
}

void test_multi_element_array()
{
	json input = json::array({true, false, true});
	json output = roundtrip(input);
	check(output == input, "[true,false,true] roundtrips");

	input = json::array({nullptr, true, nullptr, false});
	output = roundtrip(input);
	check(output == input, "[null,true,null,false] roundtrips");
}

void test_nested_array()
{
	json input = json::array({json::array({true}), json::array({false})});
	json output = roundtrip(input);
	check(output == input, "[[true],[false]] roundtrips");

	input = json::array({json::array({json::array({true})})});
	output = roundtrip(input);
	check(output == input, "[[[true]]] roundtrips");
}

//	=== Тесты чисел ===

void test_unsigned_numbers()
{
	for (json::number_unsigned_t val : {0ULL, 1ULL, 7ULL, 42ULL, 255ULL, 65535ULL, 1000000ULL})
	{
		json input = json(val);
		json output = roundtrip(input);
		check(output == input, "unsigned " + to_string(val) + " roundtrips");
	}
}

void test_integer_numbers()
{
	for (json::number_integer_t val : {-1LL, -7LL, -42LL, -255LL, -1000000LL})
	{
		json input = json(val);
		json output = roundtrip(input);
		check(output == input, "integer " + to_string(val) + " roundtrips");
	}
}

void test_float_numbers()
{
	for (double val : {0.1, 0.5, 1.5, -0.5, 3.14})
	{
		json input = json(val);
		json output = roundtrip(input);
		check(output == input, "float " + to_string(val) + " roundtrips");
	}
}

void test_number_array()
{
	json input = json::array({1, 2, 3, 4, 5});
	json output = roundtrip(input);
	check(output == input, "[1,2,3,4,5] roundtrips");
}

//	=== Тесты строк ===

void test_empty_string()
{
	json input = json("");
	json output = roundtrip(input);
	check(output == input, "empty string roundtrips");
}

void test_simple_strings()
{
	for (const string &str : {"a", "hello", "test", "abc"})
	{
		json input = json(str);
		json output = roundtrip(input);
		check(output == input, "string \"" + str + "\" roundtrips");
	}
}

void test_unicode_strings()
{
	json input = json("Hello, World!");
	json output = roundtrip(input);
	check(output == input, "string with punctuation roundtrips");
}

void test_string_with_special_chars()
{
	json input = json("line1\nline2\ttab");
	json output = roundtrip(input);
	check(output == input, "string with special chars roundtrips");
}

//	=== Тесты объектов ===

void test_empty_object()
{
	json input = json::object();
	json output = roundtrip(input);
	check(output.is_object(), "empty object exports as object");
	check(output.empty(), "empty object is empty");
}

void test_simple_object()
{
	json input = {{"key", "value"}};
	json output = roundtrip(input);
	check(output == input, "{\"key\":\"value\"} roundtrips");
}

void test_object_with_multiple_keys()
{
	json input = {{"a", 1}, {"b", 2}, {"c", 3}};
	json output = roundtrip(input);
	check(output == input, "object with multiple keys roundtrips");
}

void test_object_with_null_values()
{
	json input = {{"key", nullptr}};
	json output = roundtrip(input);
	check(output == input, "object with null value roundtrips");
}

void test_object_with_mixed_types()
{
	json input = {{"bool", true}, {"num", 42}, {"str", "hello"}, {"null_val", nullptr}};
	json output = roundtrip(input);
	check(output == input, "object with mixed types roundtrips");
}

void test_nested_object()
{
	json input = {{"outer", {{"inner", "value"}}}};
	json output = roundtrip(input);
	check(output == input, "nested object roundtrips");
}

//	=== Тесты смешанных структур ===

void test_array_of_objects()
{
	json input = json::array({{{"a", 1}}, {{"b", 2}}});
	json output = roundtrip(input);
	check(output == input, "array of objects roundtrips");
}

void test_object_with_array()
{
	json input = {{"arr", json::array({1, 2, 3})}};
	json output = roundtrip(input);
	check(output == input, "object with array roundtrips");
}

//	=== Тесты логических операций ===

void test_logical_vocabulary()
{
	check(rel_t::Not != nullptr, "Not is not null");
	check(rel_t::And != nullptr, "And is not null");
	check(rel_t::Or != nullptr, "Or is not null");

	//	Все логические операции должны быть различными
	check(rel_t::Not != rel_t::And, "Not != And");
	check(rel_t::Not != rel_t::Or, "Not != Or");
	check(rel_t::And != rel_t::Or, "And != Or");

	//	Логические операции являются сущностями (sub == E)
	check(rel_t::Not->sub == rel_t::E, "Not->sub == E");
	check(rel_t::And->sub == rel_t::E, "And->sub == E");
	check(rel_t::Or->sub == rel_t::E, "Or->sub == E");
}

void test_not()
{
	//	NOT[True] = False
	check(eval(rel_t::Not, rel_t::True) == rel_t::False, "NOT[True] = False");
	//	NOT[False] = True
	check(eval(rel_t::Not, rel_t::False) == rel_t::True, "NOT[False] = True");
	//	NOT[R] = E (не определено для R)
	check(eval(rel_t::Not, rel_t::R) == rel_t::E, "NOT[R] = E (undefined for R)");
}

void test_and()
{
	//	AND[True][True] = True
	check(eval(rel_t::And, rel_t::True, rel_t::True) == rel_t::True, "AND[True][True] = True");
	//	AND[True][False] = False
	check(eval(rel_t::And, rel_t::True, rel_t::False) == rel_t::False, "AND[True][False] = False");
	//	AND[False][True] = False
	check(eval(rel_t::And, rel_t::False, rel_t::True) == rel_t::False, "AND[False][True] = False");
	//	AND[False][False] = False
	check(eval(rel_t::And, rel_t::False, rel_t::False) == rel_t::False, "AND[False][False] = False");
}

void test_or()
{
	//	OR[True][True] = True
	check(eval(rel_t::Or, rel_t::True, rel_t::True) == rel_t::True, "OR[True][True] = True");
	//	OR[True][False] = True
	check(eval(rel_t::Or, rel_t::True, rel_t::False) == rel_t::True, "OR[True][False] = True");
	//	OR[False][True] = True
	check(eval(rel_t::Or, rel_t::False, rel_t::True) == rel_t::True, "OR[False][True] = True");
	//	OR[False][False] = False
	check(eval(rel_t::Or, rel_t::False, rel_t::False) == rel_t::False, "OR[False][False] = False");
}

void test_eval_chained()
{
	//	Проверка составных выражений: NOT[AND[True, False]] = NOT[False] = True
	auto and_result = eval(rel_t::And, rel_t::True, rel_t::False);
	check(eval(rel_t::Not, and_result) == rel_t::True, "NOT[AND[True][False]] = True");

	//	NOT[OR[False, False]] = NOT[False] = True
	auto or_result = eval(rel_t::Or, rel_t::False, rel_t::False);
	check(eval(rel_t::Not, or_result) == rel_t::True, "NOT[OR[False][False]] = True");

	//	AND[NOT[False], NOT[True]] = AND[True, False] = False
	auto not_false = eval(rel_t::Not, rel_t::False);
	auto not_true = eval(rel_t::Not, rel_t::True);
	check(eval(rel_t::And, not_false, not_true) == rel_t::False, "AND[NOT[False]][NOT[True]] = False");

	//	OR[NOT[True], NOT[False]] = OR[False, True] = True
	check(eval(rel_t::Or, not_true, not_false) == rel_t::True, "OR[NOT[True]][NOT[False]] = True");
}

void test_eval_null_safety()
{
	//	eval с nullptr аргументами возвращает E
	check(eval(nullptr, rel_t::True) == rel_t::E, "eval(nullptr, True) = E");
	check(eval(rel_t::Not, nullptr) == rel_t::E, "eval(Not, nullptr) = E");
	check(eval(nullptr, nullptr) == rel_t::E, "eval(nullptr, nullptr) = E");
}

//	=== Тесты интерпретатора выражений ===

void test_interpret_primitives()
{
	//	Примитивные значения
	check(interpret(json(true)) == rel_t::True, "interpret(true) = True");
	check(interpret(json(false)) == rel_t::False, "interpret(false) = False");
	check(interpret(json(nullptr)) == rel_t::E, "interpret(null) = E");
}

void test_interpret_not()
{
	//	NOT: {"Not": [true]} = False, {"Not": [false]} = True
	json not_true = {{"Not", json::array({true})}};
	json not_false = {{"Not", json::array({false})}};

	check(interpret(not_true) == rel_t::False, "interpret({Not: [true]}) = False");
	check(interpret(not_false) == rel_t::True, "interpret({Not: [false]}) = True");
}

void test_interpret_and()
{
	//	AND: {"And": [arg1, arg2]}
	json and_tt = {{"And", json::array({true, true})}};
	json and_tf = {{"And", json::array({true, false})}};
	json and_ft = {{"And", json::array({false, true})}};
	json and_ff = {{"And", json::array({false, false})}};

	check(interpret(and_tt) == rel_t::True, "interpret({And: [true, true]}) = True");
	check(interpret(and_tf) == rel_t::False, "interpret({And: [true, false]}) = False");
	check(interpret(and_ft) == rel_t::False, "interpret({And: [false, true]}) = False");
	check(interpret(and_ff) == rel_t::False, "interpret({And: [false, false]}) = False");
}

void test_interpret_or()
{
	//	OR: {"Or": [arg1, arg2]}
	json or_tt = {{"Or", json::array({true, true})}};
	json or_tf = {{"Or", json::array({true, false})}};
	json or_ft = {{"Or", json::array({false, true})}};
	json or_ff = {{"Or", json::array({false, false})}};

	check(interpret(or_tt) == rel_t::True, "interpret({Or: [true, true]}) = True");
	check(interpret(or_tf) == rel_t::True, "interpret({Or: [true, false]}) = True");
	check(interpret(or_ft) == rel_t::True, "interpret({Or: [false, true]}) = True");
	check(interpret(or_ff) == rel_t::False, "interpret({Or: [false, false]}) = False");
}

void test_interpret_nested()
{
	//	NOT[AND[True, False]] = NOT[False] = True
	json not_and = {{"Not", json::array({{{"And", json::array({true, false})}}})}};
	check(interpret(not_and) == rel_t::True, "interpret({Not: [{And: [true, false]}]}) = True");

	//	NOT[OR[False, False]] = NOT[False] = True
	json not_or = {{"Not", json::array({{{"Or", json::array({false, false})}}})}};
	check(interpret(not_or) == rel_t::True, "interpret({Not: [{Or: [false, false]}]}) = True");

	//	AND[NOT[False], NOT[True]] = AND[True, False] = False
	json and_nots = {{"And", json::array({{{"Not", json::array({false})}}, {{"Not", json::array({true})}}})}};
	check(interpret(and_nots) == rel_t::False, "interpret({And: [{Not: [false]}, {Not: [true]}]}) = False");

	//	OR[NOT[True], NOT[False]] = OR[False, True] = True
	json or_nots = {{"Or", json::array({{{"Not", json::array({true})}}, {{"Not", json::array({false})}}})}};
	check(interpret(or_nots) == rel_t::True, "interpret({Or: [{Not: [true]}, {Not: [false]}]}) = True");
}

void test_interpret_deeply_nested()
{
	//	NOT[NOT[True]] = NOT[False] = True
	json not_not_true = {{"Not", json::array({{{"Not", json::array({true})}}})}};
	check(interpret(not_not_true) == rel_t::True, "interpret({Not: [{Not: [true]}]}) = True");

	//	NOT[NOT[False]] = NOT[True] = False
	json not_not_false = {{"Not", json::array({{{"Not", json::array({false})}}})}};
	check(interpret(not_not_false) == rel_t::False, "interpret({Not: [{Not: [false]}]}) = False");

	//	AND[OR[True, False], AND[True, True]] = AND[True, True] = True
	json complex = {{"And", json::array({{{"Or", json::array({true, false})}}, {{"And", json::array({true, true})}}})}};
	check(interpret(complex) == rel_t::True, "interpret({And: [{Or: [true, false]}, {And: [true, true]}]}) = True");

	//	OR[AND[False, True], AND[True, True]] = OR[False, True] = True
	json complex2 = {{"Or", json::array({{{"And", json::array({false, true})}}, {{"And", json::array({true, true})}}})}};
	check(interpret(complex2) == rel_t::True, "interpret({Or: [{And: [false, true]}, {And: [true, true]}]}) = True");
}

void test_interpret_error_cases()
{
	//	Пустой объект — некорректное выражение
	check(interpret(json::object()) == rel_t::E, "interpret({}) = E");

	//	Объект с несколькими ключами — некорректное выражение
	json multi_key = {{"Not", json::array({true})}, {"And", json::array({true, true})}};
	check(interpret(multi_key) == rel_t::E, "interpret(multi-key object) = E");

	//	Неизвестный оператор
	json unknown = {{"Unknown", json::array({true, false})}};
	check(interpret(unknown) == rel_t::E, "interpret({Unknown: [...]}) = E");

	//	Пустой массив аргументов
	json empty_args = {{"Not", json::array()}};
	check(interpret(empty_args) == rel_t::E, "interpret({Not: []}) = E");

	//	Аргументы не массив
	json non_array_args = {{"Not", true}};
	check(interpret(non_array_args) == rel_t::E, "interpret({Not: true}) = E");
}

//	=== Тесты условной конструкции If ===

void test_if_vocabulary()
{
	check(rel_t::If != nullptr, "If is not null");

	//	If должен отличаться от других операций
	check(rel_t::If != rel_t::Not, "If != Not");
	check(rel_t::If != rel_t::And, "If != And");
	check(rel_t::If != rel_t::Or, "If != Or");

	//	If является сущностью (sub == E)
	check(rel_t::If->sub == rel_t::E, "If->sub == E");
}

void test_if_eval()
{
	//	IF[True] = True (условие истинно)
	check(eval(rel_t::If, rel_t::True) == rel_t::True, "IF[True] = True");
	//	IF[False] = False (условие ложно)
	check(eval(rel_t::If, rel_t::False) == rel_t::False, "IF[False] = False");
	//	IF[R] = E (не определено для R)
	check(eval(rel_t::If, rel_t::R) == rel_t::E, "IF[R] = E (undefined for R)");
}

void test_interpret_if_basic()
{
	//	{"If": [true, true, false]} = true (условие истинно → then-ветка)
	json if_true = {{"If", json::array({true, true, false})}};
	check(interpret(if_true) == rel_t::True, "interpret({If: [true, true, false]}) = True");

	//	{"If": [false, true, false]} = false (условие ложно → else-ветка)
	json if_false = {{"If", json::array({false, true, false})}};
	check(interpret(if_false) == rel_t::False, "interpret({If: [false, true, false]}) = False");

	//	{"If": [true, false, true]} = false (условие истинно → then-ветка = false)
	json if_true_rev = {{"If", json::array({true, false, true})}};
	check(interpret(if_true_rev) == rel_t::False, "interpret({If: [true, false, true]}) = False");

	//	{"If": [false, false, true]} = true (условие ложно → else-ветка = true)
	json if_false_rev = {{"If", json::array({false, false, true})}};
	check(interpret(if_false_rev) == rel_t::True, "interpret({If: [false, false, true]}) = True");
}

void test_interpret_if_with_expressions()
{
	//	{"If": [{"Not": [false]}, true, false]} = true
	//	условие: NOT[False] = True → then-ветка = true
	json if_not = {{"If", json::array({{{"Not", json::array({false})}}, true, false})}};
	check(interpret(if_not) == rel_t::True, "interpret({If: [{Not: [false]}, true, false]}) = True");

	//	{"If": [{"And": [true, false]}, true, false]} = false
	//	условие: AND[True][False] = False → else-ветка = false
	json if_and = {{"If", json::array({{{"And", json::array({true, false})}}, true, false})}};
	check(interpret(if_and) == rel_t::False, "interpret({If: [{And: [true, false]}, true, false]}) = False");

	//	{"If": [{"Or": [false, true]}, true, false]} = true
	//	условие: OR[False][True] = True → then-ветка = true
	json if_or = {{"If", json::array({{{"Or", json::array({false, true})}}, true, false})}};
	check(interpret(if_or) == rel_t::True, "interpret({If: [{Or: [false, true]}, true, false]}) = True");
}

void test_interpret_if_nested()
{
	//	Вложенные If: {"If": [true, {"If": [false, true, false]}, true]} = false
	//	условие: true → then = {"If": [false, true, false]} = false
	json nested_if = {{"If", json::array({true, {{"If", json::array({false, true, false})}}, true})}};
	check(interpret(nested_if) == rel_t::False, "interpret(nested If: then-branch evaluated) = False");

	//	{"If": [false, true, {"If": [true, false, true]}]} = false
	//	условие: false → else = {"If": [true, false, true]} = false
	json nested_if2 = {{"If", json::array({false, true, {{"If", json::array({true, false, true})}}})}};
	check(interpret(nested_if2) == rel_t::False, "interpret(nested If: else-branch evaluated) = False");

	//	Комбинация If и логических операций
	//	{"If": [{"And": [true, true]}, {"Not": [false]}, {"Or": [false, false]}]}
	//	условие: AND[True][True] = True → then = NOT[False] = True
	json complex_if = {{"If", json::array({{{"And", json::array({true, true})}}, {{"Not", json::array({false})}}, {{"Or", json::array({false, false})}}})}};
	check(interpret(complex_if) == rel_t::True, "interpret(complex If with logical ops) = True");
}

void test_interpret_if_error_cases()
{
	//	If с неправильным количеством аргументов
	json if_no_args = {{"If", json::array()}};
	check(interpret(if_no_args) == rel_t::E, "interpret({If: []}) = E");

	json if_one_arg = {{"If", json::array({true})}};
	check(interpret(if_one_arg) == rel_t::E, "interpret({If: [true]}) = E");

	json if_two_args = {{"If", json::array({true, false})}};
	check(interpret(if_two_args) == rel_t::E, "interpret({If: [true, false]}) = E");

	//	If с условием null (не boolean)
	json if_null_cond = {{"If", json::array({nullptr, true, false})}};
	check(interpret(if_null_cond) == rel_t::E, "interpret({If: [null, true, false]}) = E");
}

//	=== Тесты рекурсивных функций (Def/Call) ===

void test_def_call_vocabulary()
{
	check(rel_t::Def != nullptr, "Def is not null");
	check(rel_t::Call != nullptr, "Call is not null");

	//	Def и Call должны отличаться от других операций
	check(rel_t::Def != rel_t::Not, "Def != Not");
	check(rel_t::Def != rel_t::And, "Def != And");
	check(rel_t::Def != rel_t::Or, "Def != Or");
	check(rel_t::Def != rel_t::If, "Def != If");
	check(rel_t::Def != rel_t::Call, "Def != Call");
	check(rel_t::Call != rel_t::Not, "Call != Not");
	check(rel_t::Call != rel_t::If, "Call != If");

	//	Def и Call являются сущностями (sub == E)
	check(rel_t::Def->sub == rel_t::E, "Def->sub == E");
	check(rel_t::Call->sub == rel_t::E, "Call->sub == E");
}

void test_def_call_simple()
{
	clear_func_env();

	//	Определяем функцию identity: f(x) = x
	//	[{"Def": ["identity", ["x"], "x"]}, {"Call": ["identity", true]}]
	json program = json::array({
		{{"Def", json::array({"identity", json::array({"x"}), "x"})}},
		{{"Call", json::array({"identity", true})}}
	});
	check(interpret(program) == rel_t::True, "identity(true) = True");

	clear_func_env();

	//	identity(false) = false
	json program2 = json::array({
		{{"Def", json::array({"identity", json::array({"x"}), "x"})}},
		{{"Call", json::array({"identity", false})}}
	});
	check(interpret(program2) == rel_t::False, "identity(false) = False");
}

void test_def_call_not_function()
{
	clear_func_env();

	//	Определяем функцию myNot: f(x) = Not[x]
	//	[{"Def": ["myNot", ["x"], {"Not": ["x"]}]}, {"Call": ["myNot", true]}]
	json program = json::array({
		{{"Def", json::array({"myNot", json::array({"x"}), {{"Not", json::array({"x"})}}})}},
		{{"Call", json::array({"myNot", true})}}
	});
	check(interpret(program) == rel_t::False, "myNot(true) = False");

	clear_func_env();

	json program2 = json::array({
		{{"Def", json::array({"myNot", json::array({"x"}), {{"Not", json::array({"x"})}}})}},
		{{"Call", json::array({"myNot", false})}}
	});
	check(interpret(program2) == rel_t::True, "myNot(false) = True");
}

void test_def_call_two_params()
{
	clear_func_env();

	//	Определяем функцию myAnd: f(a, b) = And[a, b]
	json program = json::array({
		{{"Def", json::array({"myAnd", json::array({"a", "b"}), {{"And", json::array({"a", "b"})}}})}},
		{{"Call", json::array({"myAnd", true, false})}}
	});
	check(interpret(program) == rel_t::False, "myAnd(true, false) = False");

	clear_func_env();

	json program2 = json::array({
		{{"Def", json::array({"myAnd", json::array({"a", "b"}), {{"And", json::array({"a", "b"})}}})}},
		{{"Call", json::array({"myAnd", true, true})}}
	});
	check(interpret(program2) == rel_t::True, "myAnd(true, true) = True");
}

void test_def_call_recursive()
{
	clear_func_env();

	//	Рекурсивная функция: toggle вызывает себя для отрицания аргумента
	//	Использует If для базового случая (терминации)
	//	toggleOnce(x) = If[x, false, true] — простая версия Not
	//	Но для демонстрации рекурсии:
	//	rec(x, depth) = If[depth, x, Call["rec", Not[x], true]]
	//	rec(true, false) → else: rec(Not[true], true) = rec(false, true) → then: false
	json program = json::array({
		{{"Def", json::array({
			"rec",
			json::array({"x", "depth"}),
			{{"If", json::array({
				"depth",
				"x",
				{{"Call", json::array({"rec", {{"Not", json::array({"x"})}}, true})}}
			})}}
		})}},
		{{"Call", json::array({"rec", true, false})}}
	});
	check(interpret(program) == rel_t::False, "rec(true, false) = False (one recursion step)");

	clear_func_env();

	//	rec(false, false) → else: rec(Not[false], true) = rec(true, true) → then: true
	json program2 = json::array({
		{{"Def", json::array({
			"rec",
			json::array({"x", "depth"}),
			{{"If", json::array({
				"depth",
				"x",
				{{"Call", json::array({"rec", {{"Not", json::array({"x"})}}, true})}}
			})}}
		})}},
		{{"Call", json::array({"rec", false, false})}}
	});
	check(interpret(program2) == rel_t::True, "rec(false, false) = True (one recursion step)");
}

void test_def_call_multiple_functions()
{
	clear_func_env();

	//	Определяем две функции и вызываем одну из другой
	//	f(x) = Not[x], g(x) = Call["f", x]
	json program = json::array({
		{{"Def", json::array({"f", json::array({"x"}), {{"Not", json::array({"x"})}}})}},
		{{"Def", json::array({"g", json::array({"x"}), {{"Call", json::array({"f", "x"})}}})}},
		{{"Call", json::array({"g", true})}}
	});
	check(interpret(program) == rel_t::False, "g(true) = f(true) = Not[true] = False");
}

void test_def_call_nested_recursion()
{
	clear_func_env();

	//	Рекурсивная функция с вложенным If
	//	f(x) = If[x, true, Call["f", true]]
	//	f(true) → then: true (базовый случай)
	json program = json::array({
		{{"Def", json::array({
			"f",
			json::array({"x"}),
			{{"If", json::array({"x", true, {{"Call", json::array({"f", true})}}})}}
		})}},
		{{"Call", json::array({"f", true})}}
	});
	check(interpret(program) == rel_t::True, "f(true) = true (base case)");

	clear_func_env();

	//	f(false) → else: Call["f", true] → then: true
	json program2 = json::array({
		{{"Def", json::array({
			"f",
			json::array({"x"}),
			{{"If", json::array({"x", true, {{"Call", json::array({"f", true})}}})}}
		})}},
		{{"Call", json::array({"f", false})}}
	});
	check(interpret(program2) == rel_t::True, "f(false) = f(true) = true (one recursion step)");
}

void test_def_call_error_cases()
{
	clear_func_env();

	//	Def с неправильным количеством аргументов
	json def_no_args = {{"Def", json::array()}};
	check(interpret(def_no_args) == rel_t::E, "interpret({Def: []}) = E");

	json def_one_arg = {{"Def", json::array({"name"})}};
	check(interpret(def_one_arg) == rel_t::E, "interpret({Def: [name]}) = E");

	//	Def с нестроковым именем
	json def_bad_name = {{"Def", json::array({42, json::array({"x"}), true})}};
	check(interpret(def_bad_name) == rel_t::E, "interpret({Def: [42, ...]}) = E");

	//	Def с нестроковыми параметрами
	json def_bad_params = {{"Def", json::array({"f", json::array({42}), true})}};
	check(interpret(def_bad_params) == rel_t::E, "interpret({Def: [f, [42], ...]}) = E");

	//	Call несуществующей функции
	json call_undef = {{"Call", json::array({"undefined"})}};
	check(interpret(call_undef) == rel_t::E, "interpret({Call: [undefined]}) = E");

	//	Call с неправильным количеством аргументов
	json program = json::array({
		{{"Def", json::array({"f", json::array({"x"}), "x"})}},
		{{"Call", json::array({"f", true, false})}}
	});
	check(interpret(program) == rel_t::E, "interpret({Call: [f, true, false]}) = E (wrong arity)");

	clear_func_env();

	//	Call с нестроковым именем функции
	json call_bad_name = {{"Call", json::array({42})}};
	check(interpret(call_bad_name) == rel_t::E, "interpret({Call: [42]}) = E");

	//	Call с пустым массивом аргументов
	json call_empty = {{"Call", json::array()}};
	check(interpret(call_empty) == rel_t::E, "interpret({Call: []}) = E");

	clear_func_env();
}

void test_def_call_recursion_depth_limit()
{
	clear_func_env();

	//	Бесконечная рекурсия: f(x) = Call["f", x] (без базового случая)
	//	Должна завершиться E из-за лимита глубины рекурсии
	json program = json::array({
		{{"Def", json::array({"inf", json::array({"x"}), {{"Call", json::array({"inf", "x"})}}})}},
		{{"Call", json::array({"inf", true})}}
	});
	check(interpret(program) == rel_t::E, "infinite recursion returns E (depth limit)");

	clear_func_env();
}

void test_interpret_array_sequential()
{
	//	Массив выражений: последовательное выполнение
	//	[true, false, true] → результат последнего = true
	json program = json::array({true, false, true});
	// Note: this is interpreted as a data array, not sequential execution
	// because it starts with primitives, not operators
	// For sequential execution, array must contain operator objects

	clear_func_env();

	//	Массив с Def и примитивами
	json program2 = json::array({
		{{"Def", json::array({"id", json::array({"x"}), "x"})}},
		{{"Call", json::array({"id", true})}}
	});
	check(interpret(program2) == rel_t::True, "sequential: [Def, Call] = True");

	clear_func_env();
}

//	=== Тесты стандартной библиотеки логических операций ===

void test_stdlib_vocabulary()
{
	check(rel_t::Xor != nullptr, "Xor is not null");
	check(rel_t::Nand != nullptr, "Nand is not null");
	check(rel_t::Nor != nullptr, "Nor is not null");
	check(rel_t::Implies != nullptr, "Implies is not null");
	check(rel_t::Eq != nullptr, "Eq is not null");

	//	Все операции стандартной библиотеки должны быть различными
	check(rel_t::Xor != rel_t::And, "Xor != And");
	check(rel_t::Xor != rel_t::Or, "Xor != Or");
	check(rel_t::Nand != rel_t::And, "Nand != And");
	check(rel_t::Nor != rel_t::Or, "Nor != Or");
	check(rel_t::Implies != rel_t::Eq, "Implies != Eq");

	//	Все операции стандартной библиотеки являются сущностями (sub == E)
	check(rel_t::Xor->sub == rel_t::E, "Xor->sub == E");
	check(rel_t::Nand->sub == rel_t::E, "Nand->sub == E");
	check(rel_t::Nor->sub == rel_t::E, "Nor->sub == E");
	check(rel_t::Implies->sub == rel_t::E, "Implies->sub == E");
	check(rel_t::Eq->sub == rel_t::E, "Eq->sub == E");
}

void test_xor_eval()
{
	//	XOR[True][True] = False
	check(eval(rel_t::Xor, rel_t::True, rel_t::True) == rel_t::False, "XOR[True][True] = False");
	//	XOR[True][False] = True
	check(eval(rel_t::Xor, rel_t::True, rel_t::False) == rel_t::True, "XOR[True][False] = True");
	//	XOR[False][True] = True
	check(eval(rel_t::Xor, rel_t::False, rel_t::True) == rel_t::True, "XOR[False][True] = True");
	//	XOR[False][False] = False
	check(eval(rel_t::Xor, rel_t::False, rel_t::False) == rel_t::False, "XOR[False][False] = False");
}

void test_nand_eval()
{
	//	NAND[True][True] = False
	check(eval(rel_t::Nand, rel_t::True, rel_t::True) == rel_t::False, "NAND[True][True] = False");
	//	NAND[True][False] = True
	check(eval(rel_t::Nand, rel_t::True, rel_t::False) == rel_t::True, "NAND[True][False] = True");
	//	NAND[False][True] = True
	check(eval(rel_t::Nand, rel_t::False, rel_t::True) == rel_t::True, "NAND[False][True] = True");
	//	NAND[False][False] = True
	check(eval(rel_t::Nand, rel_t::False, rel_t::False) == rel_t::True, "NAND[False][False] = True");
}

void test_nor_eval()
{
	//	NOR[True][True] = False
	check(eval(rel_t::Nor, rel_t::True, rel_t::True) == rel_t::False, "NOR[True][True] = False");
	//	NOR[True][False] = False
	check(eval(rel_t::Nor, rel_t::True, rel_t::False) == rel_t::False, "NOR[True][False] = False");
	//	NOR[False][True] = False
	check(eval(rel_t::Nor, rel_t::False, rel_t::True) == rel_t::False, "NOR[False][True] = False");
	//	NOR[False][False] = True
	check(eval(rel_t::Nor, rel_t::False, rel_t::False) == rel_t::True, "NOR[False][False] = True");
}

void test_implies_eval()
{
	//	IMPLIES[True][True] = True (T → T = T)
	check(eval(rel_t::Implies, rel_t::True, rel_t::True) == rel_t::True, "IMPLIES[True][True] = True");
	//	IMPLIES[True][False] = False (T → F = F)
	check(eval(rel_t::Implies, rel_t::True, rel_t::False) == rel_t::False, "IMPLIES[True][False] = False");
	//	IMPLIES[False][True] = True (F → T = T)
	check(eval(rel_t::Implies, rel_t::False, rel_t::True) == rel_t::True, "IMPLIES[False][True] = True");
	//	IMPLIES[False][False] = True (F → F = T)
	check(eval(rel_t::Implies, rel_t::False, rel_t::False) == rel_t::True, "IMPLIES[False][False] = True");
}

void test_eq_eval()
{
	//	EQ[True][True] = True
	check(eval(rel_t::Eq, rel_t::True, rel_t::True) == rel_t::True, "EQ[True][True] = True");
	//	EQ[True][False] = False
	check(eval(rel_t::Eq, rel_t::True, rel_t::False) == rel_t::False, "EQ[True][False] = False");
	//	EQ[False][True] = False
	check(eval(rel_t::Eq, rel_t::False, rel_t::True) == rel_t::False, "EQ[False][True] = False");
	//	EQ[False][False] = True
	check(eval(rel_t::Eq, rel_t::False, rel_t::False) == rel_t::True, "EQ[False][False] = True");
}

void test_interpret_xor()
{
	//	XOR: {"Xor": [arg1, arg2]}
	json xor_tt = {{"Xor", json::array({true, true})}};
	json xor_tf = {{"Xor", json::array({true, false})}};
	json xor_ft = {{"Xor", json::array({false, true})}};
	json xor_ff = {{"Xor", json::array({false, false})}};

	check(interpret(xor_tt) == rel_t::False, "interpret({Xor: [true, true]}) = False");
	check(interpret(xor_tf) == rel_t::True, "interpret({Xor: [true, false]}) = True");
	check(interpret(xor_ft) == rel_t::True, "interpret({Xor: [false, true]}) = True");
	check(interpret(xor_ff) == rel_t::False, "interpret({Xor: [false, false]}) = False");
}

void test_interpret_nand()
{
	//	NAND: {"Nand": [arg1, arg2]}
	json nand_tt = {{"Nand", json::array({true, true})}};
	json nand_tf = {{"Nand", json::array({true, false})}};
	json nand_ft = {{"Nand", json::array({false, true})}};
	json nand_ff = {{"Nand", json::array({false, false})}};

	check(interpret(nand_tt) == rel_t::False, "interpret({Nand: [true, true]}) = False");
	check(interpret(nand_tf) == rel_t::True, "interpret({Nand: [true, false]}) = True");
	check(interpret(nand_ft) == rel_t::True, "interpret({Nand: [false, true]}) = True");
	check(interpret(nand_ff) == rel_t::True, "interpret({Nand: [false, false]}) = True");
}

void test_interpret_nor()
{
	//	NOR: {"Nor": [arg1, arg2]}
	json nor_tt = {{"Nor", json::array({true, true})}};
	json nor_tf = {{"Nor", json::array({true, false})}};
	json nor_ft = {{"Nor", json::array({false, true})}};
	json nor_ff = {{"Nor", json::array({false, false})}};

	check(interpret(nor_tt) == rel_t::False, "interpret({Nor: [true, true]}) = False");
	check(interpret(nor_tf) == rel_t::False, "interpret({Nor: [true, false]}) = False");
	check(interpret(nor_ft) == rel_t::False, "interpret({Nor: [false, true]}) = False");
	check(interpret(nor_ff) == rel_t::True, "interpret({Nor: [false, false]}) = True");
}

void test_interpret_implies()
{
	//	IMPLIES: {"Implies": [arg1, arg2]}
	json imp_tt = {{"Implies", json::array({true, true})}};
	json imp_tf = {{"Implies", json::array({true, false})}};
	json imp_ft = {{"Implies", json::array({false, true})}};
	json imp_ff = {{"Implies", json::array({false, false})}};

	check(interpret(imp_tt) == rel_t::True, "interpret({Implies: [true, true]}) = True");
	check(interpret(imp_tf) == rel_t::False, "interpret({Implies: [true, false]}) = False");
	check(interpret(imp_ft) == rel_t::True, "interpret({Implies: [false, true]}) = True");
	check(interpret(imp_ff) == rel_t::True, "interpret({Implies: [false, false]}) = True");
}

void test_interpret_eq()
{
	//	EQ: {"Eq": [arg1, arg2]}
	json eq_tt = {{"Eq", json::array({true, true})}};
	json eq_tf = {{"Eq", json::array({true, false})}};
	json eq_ft = {{"Eq", json::array({false, true})}};
	json eq_ff = {{"Eq", json::array({false, false})}};

	check(interpret(eq_tt) == rel_t::True, "interpret({Eq: [true, true]}) = True");
	check(interpret(eq_tf) == rel_t::False, "interpret({Eq: [true, false]}) = False");
	check(interpret(eq_ft) == rel_t::False, "interpret({Eq: [false, true]}) = False");
	check(interpret(eq_ff) == rel_t::True, "interpret({Eq: [false, false]}) = True");
}

void test_stdlib_nested()
{
	//	XOR с вложенными выражениями: XOR[NOT[false], AND[true, true]] = XOR[true, true] = false
	json xor_nested = {{"Xor", json::array({{{"Not", json::array({false})}}, {{"And", json::array({true, true})}}})}};
	check(interpret(xor_nested) == rel_t::False, "interpret({Xor: [{Not: [false]}, {And: [true, true]}]}) = False");

	//	IMPLIES с вложенными: IMPLIES[AND[true, true], OR[false, true]] = IMPLIES[true, true] = true
	json imp_nested = {{"Implies", json::array({{{"And", json::array({true, true})}}, {{"Or", json::array({false, true})}}})}};
	check(interpret(imp_nested) == rel_t::True, "interpret({Implies: [{And: [true, true]}, {Or: [false, true]}]}) = True");

	//	EQ[XOR[true, false], true] = EQ[true, true] = true
	json eq_xor = {{"Eq", json::array({{{"Xor", json::array({true, false})}}, true})}};
	check(interpret(eq_xor) == rel_t::True, "interpret({Eq: [{Xor: [true, false]}, true]}) = True");

	//	NAND[IMPLIES[false, true], NOR[true, false]] = NAND[true, false] = true
	json nand_complex = {{"Nand", json::array({{{"Implies", json::array({false, true})}}, {{"Nor", json::array({true, false})}}})}};
	check(interpret(nand_complex) == rel_t::True, "interpret({Nand: [{Implies: [false, true]}, {Nor: [true, false]}]}) = True");
}

void test_stdlib_with_def_call()
{
	clear_func_env();

	//	Определяем функцию myXor через стандартную библиотеку и вызываем
	json program = json::array({
		{{"Def", json::array({"myXor", json::array({"a", "b"}), {{"Xor", json::array({"a", "b"})}}})}},
		{{"Call", json::array({"myXor", true, false})}}
	});
	check(interpret(program) == rel_t::True, "myXor(true, false) = True");

	clear_func_env();

	//	Определяем функцию implies и используем в условном выражении
	json program2 = json::array({
		{{"Def", json::array({"myImpl", json::array({"a", "b"}), {{"Implies", json::array({"a", "b"})}}})}},
		{{"If", json::array({{{"Call", json::array({"myImpl", true, true})}}, true, false})}}
	});
	check(interpret(program2) == rel_t::True, "If[myImpl(true, true), true, false] = True");

	clear_func_env();
}

//	=== Тесты счётчиков памяти ===

void test_memory_counters()
{
	auto count_before = rel_t::created();
	import_json(json(true));
	check(rel_t::created() >= count_before, "created counter is non-decreasing");
	check(rel_t::count() > 0, "count is positive");
}

int main()
{
	cout << "Running unit tests..." << endl;

	test_base_vocabulary();
	test_null();
	test_boolean();
	test_empty_array();
	test_single_element_array();
	test_multi_element_array();
	test_nested_array();
	test_unsigned_numbers();
	test_integer_numbers();
	test_float_numbers();
	test_number_array();
	test_empty_string();
	test_simple_strings();
	test_unicode_strings();
	test_string_with_special_chars();
	test_empty_object();
	test_simple_object();
	test_object_with_multiple_keys();
	test_object_with_null_values();
	test_object_with_mixed_types();
	test_nested_object();
	test_array_of_objects();
	test_object_with_array();
	test_logical_vocabulary();
	test_not();
	test_and();
	test_or();
	test_eval_chained();
	test_eval_null_safety();
	test_interpret_primitives();
	test_interpret_not();
	test_interpret_and();
	test_interpret_or();
	test_interpret_nested();
	test_interpret_deeply_nested();
	test_interpret_error_cases();
	test_if_vocabulary();
	test_if_eval();
	test_interpret_if_basic();
	test_interpret_if_with_expressions();
	test_interpret_if_nested();
	test_interpret_if_error_cases();
	test_def_call_vocabulary();
	test_def_call_simple();
	test_def_call_not_function();
	test_def_call_two_params();
	test_def_call_recursive();
	test_def_call_multiple_functions();
	test_def_call_nested_recursion();
	test_def_call_error_cases();
	test_def_call_recursion_depth_limit();
	test_interpret_array_sequential();
	test_stdlib_vocabulary();
	test_xor_eval();
	test_nand_eval();
	test_nor_eval();
	test_implies_eval();
	test_eq_eval();
	test_interpret_xor();
	test_interpret_nand();
	test_interpret_nor();
	test_interpret_implies();
	test_interpret_eq();
	test_stdlib_nested();
	test_stdlib_with_def_call();
	test_memory_counters();

	cout << endl;
	cout << "Passed: " << tests_passed << endl;
	cout << "Failed: " << tests_failed << endl;
	cout << "Total:  " << tests_passed + tests_failed << endl;

	return tests_failed > 0 ? 1 : 0;
}
