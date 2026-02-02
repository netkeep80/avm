#include "json_io.h"
#include "anumber.h"
#include <iostream>
#include <cassert>

using namespace std;

// Вспомогательная функция для проверки утверждений с сообщениями
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            cerr << "FAIL: " << msg << endl; \
            cerr << "  at " << __FILE__ << ":" << __LINE__ << endl; \
            failed++; \
        } else { \
            passed++; \
        } \
    } while (0)

// Вспомогательная функция: извлечь биты из битовой цепочки строки/числа
vector<bool> get_bits(const rel_t* ent)
{
    vector<bool> bits;
    if (!ent || ent == rel_t::E || ent == rel_t::R)
        return bits;

    // ent = rel(bit_chain, terminator)
    const rel_t* cur = ent->obj;
    while (cur != rel_t::E)
    {
        if (cur->sub == rel_t::R)
        {
            const rel_t* bit_pair = cur->obj;
            if (bit_pair)
            {
                bits.push_back(bit_pair->sub == rel_t::True);
                cur = bit_pair->obj;
            }
            else break;
        }
        else break;
    }
    // Reverse: bits are stored from outermost (last) to innermost (first)
    std::reverse(bits.begin(), bits.end());
    return bits;
}

int main()
{
    int passed = 0, failed = 0;

    cout << "=== Тесты сериализации/десериализации ачисел (МТС) ===" << endl;

    // --- Тест 1: Токенизация ---
    cout << endl << "--- Тест 1: Токенизация ---" << endl;
    {
        auto tokens = tokenize_anumber("\xe2\x88\x9e" "1-1(1-1)1");
        TEST_ASSERT(tokens.size() == 7, "tokenize '∞1-1(1-1)1' should produce 7 tokens, got " + to_string(tokens.size()));
        TEST_ASSERT(tokens[0] == abit_t::LINK,   "token[0] should be LINK");
        TEST_ASSERT(tokens[1] == abit_t::NOLINK, "token[1] should be NOLINK");
        TEST_ASSERT(tokens[2] == abit_t::OPEN,   "token[2] should be OPEN");
        TEST_ASSERT(tokens[3] == abit_t::LINK,   "token[3] should be LINK");
        TEST_ASSERT(tokens[4] == abit_t::NOLINK, "token[4] should be NOLINK");
        TEST_ASSERT(tokens[5] == abit_t::CLOSE,  "token[5] should be CLOSE");
        TEST_ASSERT(tokens[6] == abit_t::LINK,   "token[6] should be LINK");
        cout << "  tokenize '∞1-1(1-1)1': " << tokens.size() << " tokens OK" << endl;
    }

    // --- Тест 2: Токенизация без префикса ∞ ---
    cout << endl << "--- Тест 2: Токенизация без префикса ∞ ---" << endl;
    {
        auto tokens = tokenize_anumber("1-11");
        TEST_ASSERT(tokens.size() == 3, "tokenize '1-11' should produce 3 tokens");
        TEST_ASSERT(tokens[0] == abit_t::LINK,   "token[0] should be LINK");
        TEST_ASSERT(tokens[1] == abit_t::NOLINK, "token[1] should be NOLINK");
        TEST_ASSERT(tokens[2] == abit_t::LINK,   "token[2] should be LINK");
        cout << "  tokenize '1-11': " << tokens.size() << " tokens OK" << endl;
    }

    // --- Тест 3: Четверичная нотация ---
    cout << endl << "--- Тест 3: Четверичная нотация ---" << endl;
    {
        string quat = anumber_to_quaternary("\xe2\x88\x9e" "1-1(1-1)1");
        TEST_ASSERT(quat == "1021031", "quaternary of '∞1-1(1-1)1' should be '1021031', got '" + quat + "'");
        cout << "  anumber_to_quaternary('∞1-1(1-1)1') = '" << quat << "'" << endl;

        string anumber = quaternary_to_anumber("1021031");
        auto tokens = tokenize_anumber(anumber);
        TEST_ASSERT(tokens.size() == 7, "roundtrip quaternary should produce 7 tokens");
        cout << "  quaternary_to_anumber('1021031') roundtrip OK" << endl;
    }

    // --- Тест 4: Сериализация базовых связей ---
    cout << endl << "--- Тест 4: Сериализация базовых связей ---" << endl;
    {
        string e_str = serialize_anumber(rel_t::E);
        TEST_ASSERT(e_str.empty(), "serialize E should be empty, got '" + e_str + "'");
        cout << "  serialize(E) = '" << format_anumber(rel_t::E) << "'" << endl;

        string true_str = serialize_anumber(rel_t::True);
        TEST_ASSERT(true_str == "1", "serialize True should be '1', got '" + true_str + "'");
        cout << "  serialize(True) = '" << format_anumber(rel_t::True) << "'" << endl;

        string false_str = serialize_anumber(rel_t::False);
        TEST_ASSERT(false_str == "-1", "serialize False should be '-1', got '" + false_str + "'");
        cout << "  serialize(False) = '" << format_anumber(rel_t::False) << "'" << endl;

        string r_str = serialize_anumber(rel_t::R);
        TEST_ASSERT(r_str.empty(), "serialize R should be empty, got '" + r_str + "'");
        cout << "  serialize(R) = '" << format_anumber(rel_t::R) << "'" << endl;
    }

    // --- Тест 5: Сериализация строки "a" ---
    cout << endl << "--- Тест 5: Сериализация строки 'a' ---" << endl;
    {
        // 'a' = 0x61, binary: 01100001, LSB first: 1,0,0,0,0,1,1,0
        json j = json("a");
        rel_t* str_rel = import_json(j);
        string str_anumber = serialize_anumber(str_rel);
        cout << "  serialize('a') = '" << format_anumber(str_rel) << "'" << endl;

        auto tokens = tokenize_anumber(str_anumber);
        TEST_ASSERT(tokens.size() == 8, "serialize 'a' should produce 8 abit tokens, got " + to_string(tokens.size()));

        if (tokens.size() == 8) {
            // LSB first: 1,0,0,0,0,1,1,0
            TEST_ASSERT(tokens[0] == abit_t::LINK,   "'a' bit 0 should be 1 (LINK)");
            TEST_ASSERT(tokens[1] == abit_t::NOLINK, "'a' bit 1 should be 0 (NOLINK)");
            TEST_ASSERT(tokens[2] == abit_t::NOLINK, "'a' bit 2 should be 0 (NOLINK)");
            TEST_ASSERT(tokens[3] == abit_t::NOLINK, "'a' bit 3 should be 0 (NOLINK)");
            TEST_ASSERT(tokens[4] == abit_t::NOLINK, "'a' bit 4 should be 0 (NOLINK)");
            TEST_ASSERT(tokens[5] == abit_t::LINK,   "'a' bit 5 should be 1 (LINK)");
            TEST_ASSERT(tokens[6] == abit_t::LINK,   "'a' bit 6 should be 1 (LINK)");
            TEST_ASSERT(tokens[7] == abit_t::NOLINK, "'a' bit 7 should be 0 (NOLINK)");
        }
    }

    // --- Тест 6: Roundtrip десериализация строки "a" ---
    cout << endl << "--- Тест 6: Roundtrip десериализация строки 'a' ---" << endl;
    {
        json j_orig = json("a");
        rel_t* orig = import_json(j_orig);
        string anumber = serialize_anumber(orig);

        rel_t* restored = deserialize_anumber(anumber);

        TEST_ASSERT(restored->sub == rel_t::E, "restored sub should be E (string terminator)");

        auto orig_bits = get_bits(orig);
        auto rest_bits = get_bits(restored);
        TEST_ASSERT(orig_bits.size() == rest_bits.size(),
            "bit count should match: orig=" + to_string(orig_bits.size()) +
            " restored=" + to_string(rest_bits.size()));
        if (orig_bits.size() == rest_bits.size())
        {
            bool bits_match = true;
            for (size_t i = 0; i < orig_bits.size(); i++)
                if (orig_bits[i] != rest_bits[i]) { bits_match = false; break; }
            TEST_ASSERT(bits_match, "all bits should match after roundtrip");
        }
        cout << "  roundtrip string 'a' OK" << endl;
    }

    // --- Тест 7: Десериализация пустого ачисла ---
    cout << endl << "--- Тест 7: Десериализация пустого ачисла ---" << endl;
    {
        rel_t* result = deserialize_anumber("\xe2\x88\x9e");
        TEST_ASSERT(result == rel_t::E, "deserialize '∞' should be E (null)");
        cout << "  deserialize('∞') = E (null) OK" << endl;
    }

    // --- Тест 8: Сериализация числа 42 ---
    cout << endl << "--- Тест 8: Сериализация числа 42 ---" << endl;
    {
        json j_orig = json(42u);
        rel_t* orig = import_json(j_orig);
        string anumber = serialize_anumber(orig);
        cout << "  serialize(42u) = '∞" << anumber << "'" << endl;

        auto tokens = tokenize_anumber(anumber);
        TEST_ASSERT(tokens.size() == 64, "serialize 42u should produce 64 abit tokens (64-bit), got " + to_string(tokens.size()));

        if (tokens.size() >= 8) {
            // 42 = 0x2A = 00101010, LSB first: 0,1,0,1,0,1,0,0
            TEST_ASSERT(tokens[0] == abit_t::NOLINK, "42u bit 0 should be 0");
            TEST_ASSERT(tokens[1] == abit_t::LINK,   "42u bit 1 should be 1");
            TEST_ASSERT(tokens[2] == abit_t::NOLINK, "42u bit 2 should be 0");
            TEST_ASSERT(tokens[3] == abit_t::LINK,   "42u bit 3 should be 1");
            TEST_ASSERT(tokens[4] == abit_t::NOLINK, "42u bit 4 should be 0");
            TEST_ASSERT(tokens[5] == abit_t::LINK,   "42u bit 5 should be 1");
        }

        string quat = anumber_to_quaternary(anumber);
        cout << "  quaternary(42u) = '" << quat << "'" << endl;
    }

    // --- Тест 9: Сериализация массива [true, false, true] ---
    cout << endl << "--- Тест 9: Сериализация массива [true, false, true] ---" << endl;
    {
        json j_orig = json::array({true, false, true});
        rel_t* orig = import_json(j_orig);
        string anumber = serialize_anumber(orig);
        cout << "  serialize([true, false, true]) = '∞" << anumber << "'" << endl;

        TEST_ASSERT(anumber == "(1)(-1)(1)",
            "serialize [true, false, true] should be '(1)(-1)(1)', got '" + anumber + "'");
    }

    // --- Тест 10: Roundtrip массива ---
    cout << endl << "--- Тест 10: Roundtrip массива ---" << endl;
    {
        json j_orig = json::array({true, false, true});
        rel_t* orig = import_json(j_orig);
        string anumber = serialize_anumber(orig);

        rel_t* restored = deserialize_anumber(anumber);

        // [true, false, true] → rel(True, rel(False, rel(True, R)))
        TEST_ASSERT(restored->obj == rel_t::True, "first element should be True");
        TEST_ASSERT(restored->sub != rel_t::R, "should have more elements");
        if (restored->sub != rel_t::R)
        {
            TEST_ASSERT(restored->sub->obj == rel_t::False, "second element should be False");
            if (restored->sub->sub != rel_t::R)
            {
                TEST_ASSERT(restored->sub->sub->obj == rel_t::True, "third element should be True");
                TEST_ASSERT(restored->sub->sub->sub == rel_t::R, "list should end with R");
            }
        }
        cout << "  roundtrip [true, false, true] OK" << endl;
    }

    // --- Тест 11: Токенизация с пробелами ---
    cout << endl << "--- Тест 11: Токенизация с пробелами ---" << endl;
    {
        auto tokens = tokenize_anumber("\xe2\x88\x9e" " 1 -1 ( 1 ) ");
        TEST_ASSERT(tokens.size() == 5, "tokenize with spaces should produce 5 tokens, got " + to_string(tokens.size()));
        cout << "  tokenize '∞ 1 -1 ( 1 )': " << tokens.size() << " tokens OK" << endl;
    }

    // --- Тест 12: Ошибка парсинга — лишняя ) ---
    cout << endl << "--- Тест 12: Ошибка парсинга — лишняя ) ---" << endl;
    {
        bool caught = false;
        try {
            deserialize_anumber("1)");
        } catch (const runtime_error& e) {
            caught = true;
            cout << "  caught expected error: " << e.what() << endl;
        }
        TEST_ASSERT(caught, "deserialize '1)' should throw runtime_error");
    }

    // --- Тест 13: Ошибка парсинга — незакрытая ( ---
    cout << endl << "--- Тест 13: Ошибка парсинга — незакрытая ( ---" << endl;
    {
        bool caught = false;
        try {
            deserialize_anumber("(1");
        } catch (const runtime_error& e) {
            caught = true;
            cout << "  caught expected error: " << e.what() << endl;
        }
        TEST_ASSERT(caught, "deserialize '(1' should throw runtime_error");
    }

    // --- Тест 14: Format anumber ---
    cout << endl << "--- Тест 14: Format anumber ---" << endl;
    {
        string formatted = format_anumber(rel_t::True);
        TEST_ASSERT(formatted == "\xe2\x88\x9e" "1",
            "format_anumber(True) should be '∞1'");
        cout << "  format_anumber(True) = '" << formatted << "'" << endl;

        formatted = format_anumber(rel_t::E);
        TEST_ASSERT(formatted == "\xe2\x88\x9e",
            "format_anumber(E) should be '∞'");
        cout << "  format_anumber(E) = '" << formatted << "'" << endl;
    }

    // --- Тест 15: Сериализация строки "ab" ---
    cout << endl << "--- Тест 15: Сериализация строки 'ab' ---" << endl;
    {
        json j = json("ab");
        rel_t* str_rel = import_json(j);
        string str_anumber = serialize_anumber(str_rel);
        cout << "  serialize('ab') = '∞" << str_anumber << "'" << endl;

        auto tokens = tokenize_anumber(str_anumber);
        TEST_ASSERT(tokens.size() == 16, "serialize 'ab' should produce 16 abit tokens, got " + to_string(tokens.size()));
    }

    // --- Тест 16: Roundtrip строки "ab" ---
    cout << endl << "--- Тест 16: Roundtrip строки 'ab' ---" << endl;
    {
        json j_orig = json("ab");
        rel_t* orig = import_json(j_orig);
        string anumber = serialize_anumber(orig);

        rel_t* restored = deserialize_anumber(anumber);

        auto orig_bits = get_bits(orig);
        auto rest_bits = get_bits(restored);
        TEST_ASSERT(orig_bits.size() == rest_bits.size(),
            "bit count should match: orig=" + to_string(orig_bits.size()) +
            " restored=" + to_string(rest_bits.size()));
        if (orig_bits.size() == rest_bits.size())
        {
            bool bits_match = true;
            for (size_t i = 0; i < orig_bits.size(); i++)
                if (orig_bits[i] != rest_bits[i]) { bits_match = false; break; }
            TEST_ASSERT(bits_match, "all bits should match after roundtrip for 'ab'");
        }
        cout << "  roundtrip string 'ab' OK" << endl;
    }

    // --- Тест 17: Сериализация вложенного массива ---
    cout << endl << "--- Тест 17: Сериализация вложенного массива ---" << endl;
    {
        // [[true], [false]]
        json j_orig = json::array({json::array({true}), json::array({false})});
        rel_t* orig = import_json(j_orig);
        string anumber = serialize_anumber(orig);
        cout << "  serialize([[true], [false]]) = '∞" << anumber << "'" << endl;

        TEST_ASSERT(anumber == "((1))((-1))",
            "serialize [[true],[false]] should be '((1))((-1))', got '" + anumber + "'");
    }

    // --- Тест 18: Roundtrip вложенного массива ---
    cout << endl << "--- Тест 18: Roundtrip вложенного массива ---" << endl;
    {
        string anumber = "((1))((-1))";
        rel_t* restored = deserialize_anumber(anumber);

        // [[true], [false]] → rel([true], rel([false], R))
        // [true] → rel(True, R), [false] → rel(False, R)
        TEST_ASSERT(restored->obj != nullptr, "first element should exist");
        if (restored->obj) {
            TEST_ASSERT(restored->obj->obj == rel_t::True, "first inner array should contain True");
            TEST_ASSERT(restored->obj->sub == rel_t::R, "first inner array should end with R");
        }
        TEST_ASSERT(restored->sub != rel_t::R, "should have second element");
        if (restored->sub != rel_t::R) {
            TEST_ASSERT(restored->sub->obj != nullptr, "second element should exist");
            if (restored->sub->obj) {
                TEST_ASSERT(restored->sub->obj->obj == rel_t::False, "second inner array should contain False");
            }
            TEST_ASSERT(restored->sub->sub == rel_t::R, "outer array should end with R");
        }
        cout << "  roundtrip [[true],[false]] OK" << endl;
    }

    // --- Тест 19: Десериализация одиночных абит ---
    cout << endl << "--- Тест 19: Десериализация одиночных абит ---" << endl;
    {
        // "1" десериализуется как True (одиночный абит = наличие связи)
        rel_t* r1 = deserialize_anumber("1");
        TEST_ASSERT(r1 == rel_t::True, "deserialize '1' should be True");
        cout << "  deserialize('1') = True OK" << endl;

        // "-1" десериализуется как False (одиночный абит = отсутствие связи)
        rel_t* r0 = deserialize_anumber("-1");
        TEST_ASSERT(r0 == rel_t::False, "deserialize '-1' should be False");
        cout << "  deserialize('-1') = False OK" << endl;

        // Roundtrip: serialize(True) = "1", deserialize("1") = True
        TEST_ASSERT(serialize_anumber(r1) == "1", "serialize(True) should be '1'");
        TEST_ASSERT(serialize_anumber(r0) == "-1", "serialize(False) should be '-1'");
        cout << "  roundtrip True/False OK" << endl;
    }

    // --- Тест 20: Стабильность двойного roundtrip ---
    cout << endl << "--- Тест 20: Стабильность двойного roundtrip ---" << endl;
    {
        string example = "1-1(1-1)1";
        rel_t* result = deserialize_anumber(example);
        string reserialized = serialize_anumber(result);
        cout << "  '∞1-1(1-1)1' -> deserialize -> serialize -> '∞" << reserialized << "'" << endl;

        rel_t* result2 = deserialize_anumber(reserialized);
        string reserialized2 = serialize_anumber(result2);
        TEST_ASSERT(reserialized == reserialized2, "double roundtrip should be stable");
        cout << "  double roundtrip stable OK" << endl;
    }

    // --- Итоги ---
    cout << endl << "========================================" << endl;
    cout << "=== Итоги ===" << endl;
    cout << "  Passed: " << passed << endl;
    cout << "  Failed: " << failed << endl;
    cout << "========================================" << endl;

    if (failed > 0)
    {
        cerr << endl << "SOME TESTS FAILED!" << endl;
        return 1;
    }

    cout << endl << "ALL TESTS PASSED!" << endl;
    return 0;
}
