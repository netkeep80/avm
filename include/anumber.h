#pragma once
#include "avm.h"
#include <string>
#include <vector>
#include <stack>
#include <sstream>
#include <stdexcept>

/*
    Сериализация/десериализация ачисел (ассоциативных чисел) на основе МТС
    (Метатеории связей).

    Ачисло — имя связи в виде последовательности абит, описывающих полную
    структуру связи в сериализованном виде. Все ачисла неявно начинаются с ∞.

    РБНФ (расширенная форма Бэкуса-Наура) нотации ачисел:

        anumber     = "∞" { abit } ;
        abit        = "1" | "-1" | "(" abit_seq ")" ;
        abit_seq    = { abit } ;

    Четверичная нотация абит:
        0  ↔ -1    (∞♀ → ♂∞, отсутствие связи, несвязь ↛)
        1  ↔  1    (♂∞ → ∞♀, наличие связи →)
        2  ↔  (    (♂∞, начало вложенной связи, самозамыкание по ссылке)
        3  ↔  )    (∞♀, конец вложенной связи, самозамыкание по значению)

    Определения символических знаков МТС:
        ∞   — ассоциативный корень, полностью самозамкнутая связь (∞ ≡ ∞ → ∞)
        ♂∞  — связь с самозамкнутым началом (♂∞ ≡ ♂∞ → ∞), роль ссылки, "("
        ∞♀  — связь с самозамкнутым концом (∞♀ ≡ ∞ → ∞♀), роль значения, ")"
        1   — наличие связи (♂∞ → ∞♀), роль связи "→"
        -1  — отсутствие связи (∞♀ → ♂∞), роль несвязи "↛"

    Четыре вида формы связей:
        1. ∞ → ∞  — полное самозамыкание (∞)
        2. ♂∞ → ∞ — самозамыкание начала (♂)
        3. ∞ → ∞♀ — самозамыкание конца (♀)
        4. ♂∞ → ∞♀ — связь без самозамыканий (→)

    Внутренняя структура связей в rel_t:
        rel(obj, sub) — связь от obj (ссылка/источник) к sub (значение/приёмник)

    Кодирование типов данных в rel_t (из import_json):
        - null:     rel() → самоссылающийся узел (obj=self, sub=self)
        - true:     rel_t::True (singleton, obj=R, sub=R)
        - false:    rel_t::False (singleton, obj=E, sub=R)
        - array:    связный список rel(element, next), конец: R
                    [a,b,c] → rel(a, rel(b, rel(c, R)))
        - string:   rel(bit_chain, E), где bit_chain — цепочка бит
                    цепочка: rel(rel(...rel(E, bit0)..., bitN-1), R)
                    каждый уровень чередует sub=R (обёртка) и sub=True/False (бит)
        - unsigned: rel(bit_chain, Unsigned) — аналогично string
        - integer:  rel(bit_chain, Integer)
        - float:    rel(bit_chain, Float)

    Алгоритм сериализации связи в ачисло:

        Связь рекурсивно обходится и кодируется абитами:
        - E (null/пустая сущность) → пустое ачисло (только ∞)
        - R (корень массивов)      → пустое ачисло
        - True                     → "1"
        - False                    → "-1"
        - Битовая последовательность (string/number):
          биты извлекаются из цепочки и записываются как 1/-1
        - Массив: элементы записываются в скобках
          [a, b, c] → "(сер.a)(сер.b)(сер.c)"
        - Произвольная связь rel(obj, sub):
          "(сер.obj)(сер.sub)"

    Алгоритм десериализации ачисла в связь:

        Используется стэк начал связей (stack of link starts).
        - "1"  : добавить бит true к текущей битовой цепочке
        - "-1" : добавить бит false к текущей битовой цепочке
        - "("  : сохранить текущий контекст в стэк, начать новый
        - ")"  : завершить текущий контекст, встроить в родительский

    Пример ачисел:
        ∞           — null (E)
        ∞1          — true
        ∞-1         — false
        ∞(1)(-1)(1) — массив [true, false, true]
        ∞1-1-1-1-11 1-1 — строка "a" (биты: 1,0,0,0,0,1,1,0)
*/

using namespace std;

// Перечисление абит (ассоциативных бит)
enum class abit_t {
    LINK     = 1,   // 1  — наличие связи (♂∞ → ∞♀)
    NOLINK   = 0,   // -1 — отсутствие связи (∞♀ → ♂∞)
    OPEN     = 2,   // (  — начало вложенной связи (♂∞)
    CLOSE    = 3    // )  — конец вложенной связи (∞♀)
};

/*
    Сериализация rel_t в строку абит (МТС нотация).

    Структура данных в rel_t:
    - Битовые последовательности (строки, числа) хранятся как вложенные пары:
      Каждый уровень цепочки чередует обёртку (sub=R) и бит (sub=True/False),
      начиная с E как базы цепочки.
    - Массивы хранятся как правосвязные списки rel(elem, next) с R на конце.
*/

// Вспомогательная функция: извлечение битов из цепочки бит
// Обход цепочки от внешнего уровня к внутреннему (через obj),
// чередуя sub=R (обёртка) и sub=True/False (бит).
// Биты записываются в обратном порядке (от внутреннего к внешнему).
inline void extract_bits(const rel_t* chain, vector<bool>& bits)
{
    const rel_t* cur = chain;
    while (cur != rel_t::E)
    {
        // Уровень обёртки: sub=R, obj=пара_с_битом
        if (cur->sub == rel_t::R)
        {
            const rel_t* bit_pair = cur->obj;
            if (bit_pair)
            {
                // Уровень бита: sub=True/False, obj=предыдущий_элемент_цепочки
                bool bit_val = (bit_pair->sub == rel_t::True);
                bits.push_back(bit_val);
                cur = bit_pair->obj; // идём глубже по цепочке
            }
            else
                break;
        }
        else
            break;
    }
}

// Сериализация rel_t в строку абит (МТС нотация)
inline string serialize_anumber(const rel_t* ent)
{
    if (!ent)
        return "";

    // ∞ — пустое ачисло (корень, null)
    if (ent == rel_t::E)
        return "";

    // R — корень массивов (пустой массив)
    if (ent == rel_t::R)
        return "";

    // Базовые абиты
    if (ent == rel_t::True)
        return "1";

    if (ent == rel_t::False)
        return "-1";

    // Типы-маркеры (не должны сериализоваться напрямую)
    if (ent == rel_t::Unsigned || ent == rel_t::Integer || ent == rel_t::Float)
        return "";

    // Самоссылающийся узел (null из import_json)
    if (ent->obj == ent && ent->sub == ent)
        return "";

    // Битовые последовательности: строки и числа
    // Определяем по типу-терминатору в sub
    if (ent->sub == rel_t::E || ent->sub == rel_t::Unsigned ||
        ent->sub == rel_t::Integer || ent->sub == rel_t::Float)
    {
        // ent = rel(bit_chain, type_marker)
        // Извлекаем биты из цепочки в obj
        vector<bool> bits;
        extract_bits(ent->obj, bits);

        if (bits.empty())
            return "";

        // Биты в обратном порядке (от внутреннего к внешнему = LSB first)
        string result;
        for (auto it = bits.rbegin(); it != bits.rend(); ++it)
            result += (*it) ? "1" : "-1";

        return result;
    }

    // Массивы: правосвязный список rel(element, next), конец: R
    // Определяем массив по наличию R в цепочке sub
    {
        // Проверяем, является ли это списком (массивом)
        bool is_list = false;
        const rel_t* tail = ent;
        int depth = 0;
        while (tail != rel_t::R && tail != rel_t::E && depth < 1000)
        {
            if (tail->sub == rel_t::R)
            {
                is_list = true;
                break;
            }
            tail = tail->sub;
            depth++;
            if (tail == rel_t::R)
            {
                is_list = true;
                break;
            }
        }

        if (is_list)
        {
            string result;
            const rel_t* cur = ent;
            while (cur != rel_t::R && cur != rel_t::E)
            {
                string elem = serialize_anumber(cur->obj);
                result += "(" + elem + ")";
                cur = cur->sub;
            }
            return result;
        }
    }

    // Общий случай: произвольная связь rel(obj, sub)
    string obj_str = serialize_anumber(ent->obj);
    string sub_str = serialize_anumber(ent->sub);

    if (obj_str.empty() && sub_str.empty())
        return "";

    return "(" + obj_str + ")(" + sub_str + ")";
}

// Форматирование ачисла с префиксом ∞
inline string format_anumber(const rel_t* ent)
{
    string body = serialize_anumber(ent);
    return "\xe2\x88\x9e" + body; // ∞ в UTF-8
}

/*
    Десериализация ачисла (строки абит) в связь (rel_t).

    Парсер читает последовательность абит и строит дерево связей,
    используя стэк для обработки вложенных контекстов.

    Токены:
        "1"   → abit_t::LINK   (наличие связи)
        "-1"  → abit_t::NOLINK (отсутствие связи)
        "("   → abit_t::OPEN   (начало вложенного контекста)
        ")"   → abit_t::CLOSE  (конец вложенного контекста)
*/

// Токенизация строки ачисла в последовательность абит
inline vector<abit_t> tokenize_anumber(const string& str)
{
    vector<abit_t> tokens;
    size_t pos = 0;

    // Пропускаем опциональный префикс ∞ (UTF-8: E2 88 9E)
    if (pos + 3 <= str.size() &&
        (unsigned char)str[pos] == 0xE2 &&
        (unsigned char)str[pos+1] == 0x88 &&
        (unsigned char)str[pos+2] == 0x9E)
    {
        pos += 3;
    }

    while (pos < str.size())
    {
        char c = str[pos];

        if (c == '(')
        {
            tokens.push_back(abit_t::OPEN);
            pos++;
        }
        else if (c == ')')
        {
            tokens.push_back(abit_t::CLOSE);
            pos++;
        }
        else if (c == '1')
        {
            tokens.push_back(abit_t::LINK);
            pos++;
        }
        else if (c == '-' && pos + 1 < str.size() && str[pos+1] == '1')
        {
            tokens.push_back(abit_t::NOLINK);
            pos += 2;
        }
        else if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
        {
            pos++; // пропуск пробелов
        }
        else
        {
            throw runtime_error(
                "anumber parse error: unexpected character '"s +
                c + "' at position " + to_string(pos));
        }
    }

    return tokens;
}

/*
    Десериализация последовательности абит в связь.

    Контекстная модель:
    - Каждый контекст собирает биты (1/-1) в битовую цепочку
    - Скобки "(" ... ")" создают вложенный контекст и встраивают
      результат как элемент массива в родительский контекст.
    - Если контекст содержит только биты → битовая последовательность
      (строка с терминатором E)
    - Если контекст содержит вложенные элементы → массив (список)
*/
inline rel_t* deserialize_anumber(const string& str)
{
    vector<abit_t> tokens = tokenize_anumber(str);

    if (tokens.empty())
        return rel_t::E; // пустое ачисло = ∞ ≡ null/E

    // Контекст десериализации
    struct context_t {
        rel_t* bit_chain;       // текущая цепочка бит (начинается с E)
        bool has_bits;           // есть ли биты в текущем контексте?
        vector<rel_t*> elements; // элементы массива (из вложенных контекстов)
    };

    stack<context_t> ctx_stack;
    ctx_stack.push({rel_t::E, false, {}});

    for (size_t i = 0; i < tokens.size(); i++)
    {
        abit_t tok = tokens[i];

        switch (tok)
        {
        case abit_t::LINK:
        {
            auto& ctx = ctx_stack.top();
            // Добавить бит true к текущей цепочке бит
            // Структура: rel(rel(prev_chain, True), R)
            ctx.bit_chain = rel_t::rel(
                rel_t::rel(ctx.bit_chain, rel_t::True),
                rel_t::R
            );
            ctx.has_bits = true;
            break;
        }

        case abit_t::NOLINK:
        {
            auto& ctx = ctx_stack.top();
            // Добавить бит false к текущей цепочке бит
            ctx.bit_chain = rel_t::rel(
                rel_t::rel(ctx.bit_chain, rel_t::False),
                rel_t::R
            );
            ctx.has_bits = true;
            break;
        }

        case abit_t::OPEN:
        {
            // Открыть новый контекст
            ctx_stack.push({rel_t::E, false, {}});
            break;
        }

        case abit_t::CLOSE:
        {
            if (ctx_stack.size() <= 1)
                throw runtime_error(
                    "anumber parse error: unexpected ')' — no matching '('");

            // Завершить текущий контекст
            context_t inner = ctx_stack.top();
            ctx_stack.pop();

            // Определить результат внутреннего контекста
            rel_t* inner_result;

            if (!inner.elements.empty())
            {
                // Массив: строим правосвязный список
                inner_result = rel_t::R;
                for (auto it = inner.elements.rbegin(); it != inner.elements.rend(); ++it)
                    inner_result = rel_t::rel(*it, inner_result);
            }
            else if (inner.has_bits)
            {
                // Проверяем: если это одиночный бит, возвращаем True/False
                // Одиночный бит: rel(rel(E, True/False), R)
                if (inner.bit_chain->sub == rel_t::R &&
                    inner.bit_chain->obj != nullptr &&
                    inner.bit_chain->obj->obj == rel_t::E)
                {
                    if (inner.bit_chain->obj->sub == rel_t::True)
                        inner_result = rel_t::True;
                    else if (inner.bit_chain->obj->sub == rel_t::False)
                        inner_result = rel_t::False;
                    else
                        inner_result = rel_t::rel(inner.bit_chain, rel_t::E);
                }
                else
                {
                    // Битовая последовательность → строка (терминатор E)
                    inner_result = rel_t::rel(inner.bit_chain, rel_t::E);
                }
            }
            else
            {
                // Пустой контекст → null
                inner_result = rel_t::E;
            }

            // Встроить результат в родительский контекст как элемент
            auto& parent = ctx_stack.top();
            parent.elements.push_back(inner_result);
            break;
        }
        }
    }

    if (ctx_stack.size() != 1)
        throw runtime_error(
            "anumber parse error: unclosed '(' — missing ')'");

    auto& final_ctx = ctx_stack.top();

    // Определить результат корневого контекста
    if (!final_ctx.elements.empty())
    {
        // Массив: строим правосвязный список
        rel_t* result = rel_t::R;
        for (auto it = final_ctx.elements.rbegin(); it != final_ctx.elements.rend(); ++it)
            result = rel_t::rel(*it, result);
        return result;
    }

    if (final_ctx.has_bits)
    {
        // Проверяем: если это одиночный бит, возвращаем True/False
        if (final_ctx.bit_chain->sub == rel_t::R &&
            final_ctx.bit_chain->obj != nullptr &&
            final_ctx.bit_chain->obj->obj == rel_t::E)
        {
            if (final_ctx.bit_chain->obj->sub == rel_t::True)
                return rel_t::True;
            else if (final_ctx.bit_chain->obj->sub == rel_t::False)
                return rel_t::False;
        }
        // Битовая последовательность → строка (терминатор E)
        return rel_t::rel(final_ctx.bit_chain, rel_t::E);
    }

    return rel_t::E;
}

/*
    Вспомогательная функция: преобразование ачисла в четверичную нотацию.

    Четверичная система:
        -1 → 0
         1 → 1
         ( → 2
         ) → 3
*/
inline string anumber_to_quaternary(const string& anumber_str)
{
    vector<abit_t> tokens = tokenize_anumber(anumber_str);
    string result;

    for (auto tok : tokens)
    {
        switch (tok)
        {
        case abit_t::NOLINK: result += '0'; break;
        case abit_t::LINK:   result += '1'; break;
        case abit_t::OPEN:   result += '2'; break;
        case abit_t::CLOSE:  result += '3'; break;
        }
    }

    return result;
}

/*
    Вспомогательная функция: преобразование четверичной нотации в ачисло.

    Четверичная система:
        0 → -1
        1 →  1
        2 →  (
        3 →  )
*/
inline string quaternary_to_anumber(const string& quat_str)
{
    string result = "\xe2\x88\x9e"; // ∞ prefix

    for (char c : quat_str)
    {
        switch (c)
        {
        case '0': result += "-1"; break;
        case '1': result += "1";  break;
        case '2': result += "(";  break;
        case '3': result += ")";  break;
        case ' ': case '\t': case '\n': break; // пропуск пробелов
        default:
            throw runtime_error(
                "quaternary parse error: unexpected character '"s + c + "'");
        }
    }

    return result;
}
