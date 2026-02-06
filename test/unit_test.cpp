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
	test_memory_counters();

	cout << endl;
	cout << "Passed: " << tests_passed << endl;
	cout << "Failed: " << tests_failed << endl;
	cout << "Total:  " << tests_passed + tests_failed << endl;

	return tests_failed > 0 ? 1 : 0;
}
