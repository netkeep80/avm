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
	json unknown = {{"Xor", json::array({true, false})}};
	check(interpret(unknown) == rel_t::E, "interpret({Xor: [...]}) = E");

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
	test_memory_counters();

	cout << endl;
	cout << "Passed: " << tests_passed << endl;
	cout << "Failed: " << tests_failed << endl;
	cout << "Total:  " << tests_passed + tests_failed << endl;

	return tests_failed > 0 ? 1 : 0;
}
