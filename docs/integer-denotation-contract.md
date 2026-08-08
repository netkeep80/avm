# Контракт канонической денотации целых чисел AVM

Родительская задача: #128. Узкий gate: #165. Showcase: #154/#156.

## Статус

Этот документ фиксирует первый numeric value contract AVM: как знаковые целые числа денотируются только связями и как над этой денотацией строится минимальный арифметический vocabulary.

Контракт не вводит JSON runtime values, host-side `variant` или строковые имена операторов.

Главный принцип:

```text
математическое значение
    -> canonical link structure
    -> LinkId
```

`std::int64_t` используется только как ограниченный host-интерфейс первой реализации arithmetic primitives. Он **не является semantic representation** числа в `LinkStore`.

## Требования

Денотация должна одновременно обеспечивать:

1. единственное допустимое структурное представление каждого целого;
2. observational `find/decode/is` без materialization;
3. explicit `realize`;
4. одинаковый structural meaning в memory/persistent backends;
5. отсутствие зависимости от JSON, Anum syntax и GUI;
6. возможность позднее добавить arbitrary-precision arithmetic без смены уже сохранённой структуры числа.

## Рассмотренные варианты

### Десятичная или текстовая запись

Например, число можно было бы представить как canonical byte/text sequence `"-123"`.

Плюсы:

- просто отображать человеку;
- просто импортировать/экспортировать.

Минусы:

- serialization начинает определять semantic identity;
- появляются вопросы `+0`, leading zero, locale и text codec;
- арифметика вынуждена сначала интерпретировать строку;
- representation плохо выражает внутреннюю структурную природу значения.

Этот вариант отвергается как semantic denotation. Текст остаётся внешней projection.

### Host integer как скрытый payload

Можно было бы хранить `std::int64_t` в C++ map по `LinkId`.

Это проще всего программно, но создаёт вторую невидимую вселенную данных, не сохраняемую самой Моделью Связей.

Вариант запрещён.

### Унарная link-chain

Число `n` можно представить цепочкой длины `n`.

Плюс — крайне простая структура.

Минус — линейный размер значения и непригодность даже для обычных 64-bit значений.

Вариант отвергается.

### Двоичная link-chain

Выбранный вариант: magnitude кодируется цепочкой бинарных разрядов, sign задаётся отдельной typed-wrapper связью.

Размер структуры `O(log |n|)`, representation остаётся полностью link-native, а structural validation проста.

## Vocabulary первой версии

Нужны независимые identity-точки:

```text
integer_zero
integer_positive
integer_negative
magnitude_end
bit_zero
bit_one

integer_add
integer_subtract
integer_multiply
integer_divide
```

Все identities должны быть различны и присутствовать в `LinkStore`.

Arithmetic relation identities не являются частью битового encoding и перечислены здесь только потому, что #165 реализует замкнутый первый integer slice.

## Каноническая magnitude

Биты идут от младшего к старшему.

```text
Magnitude(bits[0..k]) =
    Link(bit(bits[0]),
      Link(bit(bits[1]),
        ...
          Link(bit(bits[k]), magnitude_end)))
```

где:

```text
bit(0) = bit_zero
bit(1) = bit_one
```

Каноническое условие:

```text
bits[k] = 1
```

То есть последний, старший разряд всегда единица.

Пустая magnitude и magnitude со старшим нулём недопустимы.

Примеры концептуально:

```text
1  -> [1]
2  -> [0,1]
3  -> [1,1]
4  -> [0,0,1]
5  -> [1,0,1]
```

Здесь квадратные скобки показывают порядок link-chain от младшего бита к старшему, а не отдельный container type.

## Знаковое целое

Ноль не имеет знака:

```text
Integer(0) = integer_zero
```

Для `n > 0`:

```text
Integer(+n) = Link(integer_positive, Magnitude(n))
```

Для `n < 0`:

```text
Integer(-n) = Link(integer_negative, Magnitude(|n|))
```

Следствия:

- `+0` не существует;
- `-0` не существует;
- positive/negative wrapper никогда не может ссылаться на пустую/нулевую magnitude;
- одно числовое значение имеет единственную допустимую link-структуру при фиксированном vocabulary.

## Почему representation не ограничено int64

Bit-chain не имеет нормативного ограничения длины.

Первая C++ реализация `decode_integer` для arithmetic API может выдавать `std::int64_t` и детерминированно отвергать magnitude вне диапазона.

Это означает:

```text
stored integer denotation: structurally unbounded
first host arithmetic domain: signed int64
```

Поздний arbitrary-precision primitive сможет читать те же LinkIds без migration stored values.

## Контракт поиска и материализации

### `find_integer`

Концептуально:

```text
find_integer(store, vocabulary, int64_value)
    -> optional<LinkId>
```

Функция только строит ожидаемый путь через последовательные `find(begin,end)` от старших structural suffix к wrapper-у.

При отсутствии любого звена возвращается `nullopt`.

`LinkStore::size()` до и после вызова совпадает.

### `realize_integer`

Концептуально:

```text
realize_integer(store, vocabulary, int64_value)
    -> LinkId
```

Явно создаёт недостающие bit-chain links через `intern`, затем sign wrapper.

Повторный вызов для того же значения возвращает тот же canonical `LinkId` в данном store.

Для нуля возвращается `integer_zero` и ничего не создаётся.

### `decode_integer`

Концептуально:

```text
decode_integer(store, vocabulary, LinkId)
    -> int64
```

Observation only.

Проверяет:

- special zero identity;
- wrapper begin равен ровно `integer_positive` или `integer_negative`;
- magnitude-chain состоит только из `bit_zero/bit_one`;
- chain заканчивается ровно `magnitude_end`;
- последний бит перед `magnitude_end` равен `bit_one`;
- chain не циклический;
- значение укладывается в signed int64 с отдельной корректной обработкой `INT64_MIN`.

Malformed shape и overflow — deterministic error.

### `is_integer`

`is_integer` является безопасным observational predicate над тем же validator-ом и не создаёт links.

## Точность `find_integer`

Важно не путать две операции:

```text
find semantic value
realize semantic value
```

`find_integer(42)` не имеет права вызвать `intern`, даже если часть suffix-chain уже существует.

Это продолжает общий AVM contract `find != realize`, уже закреплённый для links и PairTarget.

## Direct arithmetic relations

Первый vocabulary:

```text
integer_add
integer_subtract
integer_multiply
integer_divide
```

Исполняемая entity имеет meaningful subject:

```text
(rel = integer_add,
 sub = Integer(a),
 obj = Integer(b))
```

Результат:

```text
Integer(a + b)
```

Никакого `unit` sentinel для binary operand pair не используется.

Controller получает два canonical LinkId operand-а непосредственно из triune entity.

## Materialization результата арифметики

Arithmetic primitive является **value-producing realization operation**.

Алгоритм:

```text
1. observationally decode subject;
2. observationally decode object;
3. checked host arithmetic;
4. explicit realize canonical result;
5. return result LinkId.
```

Следовательно arithmetic execution может увеличить `LinkStore`, если результирующее число ещё не денотировано.

Это не нарушает `find != realize`: arithmetic relation по контракту сама является явной операцией вычисления/реализации нового значения.

Если позже потребуется чисто observational табличная арифметика, для неё должен появиться отдельный relation contract, а не скрытый режим этой relation.

## Арифметическая семантика первого host-domain

Host-domain: `std::int64_t`.

### Сложение

Checked exact signed addition. Overflow -> deterministic execution failure.

### Вычитание

Checked exact signed subtraction. Overflow -> deterministic execution failure.

### Умножение

Checked exact signed multiplication. Overflow -> deterministic execution failure.

### Деление

Целочисленное signed division с C++20 truncation toward zero для representable operands.

Детерминированные ошибки:

```text
b == 0
INT64_MIN / -1
```

Оба случая завершают handler failure до materialization result.

## `INT64_MIN`

Нельзя вычислять `abs(INT64_MIN)` в signed domain.

Encoder/decoder используют unsigned magnitude arithmetic, например `std::uint64_t`, так чтобы magnitude `2^63` корректно представляла `INT64_MIN`.

Это обязательный sanitizer-tested case.

## Ошибки формы

Не являются integer values:

- сами markers `integer_positive`, `integer_negative`, `bit_zero`, `bit_one`, `magnitude_end`;
- arbitrary point;
- wrapper с неизвестным sign marker;
- sign wrapper, указывающий прямо на `magnitude_end`;
- chain с последним `bit_zero`;
- chain с произвольным begin вместо bit marker;
- циклическая/несходящаяся chain;
- wrapper/chain, содержащий неизвестный `LinkId`.

Decoder не пытается «исправить» malformed structure materialization-ом.

## Persistent semantics

Vocabulary identities и все structural links сохраняются обычным `LinkStore` backend-ом.

После reopen при восстановленном том же vocabulary:

```text
decode(Integer(n)) = n
find_integer(n) = прежний structural value
realize_integer(n) = прежний structural value
```

Никакая host-side таблица `LinkId -> int64` не нужна.

## Связь с CalculatorState

#156 не должен знать bit-chain encoding.

Calculator model видит только публичный integer contract и relation identities:

```text
press_digit(state, Integer(7)) -> next_state
integer_add(Integer(7), Integer(3)) -> Integer(10)
```

Presentation layer может вызвать observational decoder для отображения `10`, но authoritative value остаётся `LinkId`.

## Связь с Anum

Этот contract не является сериализацией Anum и не определяет raw Anum representation числа.

Если Anum frontend позже денотирует целое, его projection должна приводить к этой canonical L4 structure.

Сохраняется принцип:

```text
raw(A) != den(A)
```

## Инварианты реализации

Обязательно:

1. vocabulary markers distinct;
2. zero unique and unsigned by construction;
3. magnitude non-empty;
4. most-significant bit is one;
5. observation never calls `intern/create_point`;
6. realize canonical through `intern`;
7. arithmetic accepts only valid integers;
8. arithmetic overflow cannot publish partial result;
9. GUI/JSON/Anum layers отсутствуют в integer core;
10. persistent and in-memory meanings equivalent.

## Conformance gate

Минимальные значения:

```text
0, 1, -1, 2, 3, 7, 10, 42,
INT64_MAX,
INT64_MIN
```

Arithmetic:

```text
7 + 3 = 10
9 - 4 = 5
0 - 7 = -7
6 * 7 = 42
8 / 2 = 4
-7 / 2 = -3
```

Failures:

```text
INT64_MAX + 1
INT64_MIN - 1
overflow multiply
1 / 0
INT64_MIN / -1
malformed integer shapes
```

Проверки:

- strict warnings-as-errors;
- ASan/UBSan;
- in-memory;
- persistent reopen;
- installed package consumer;
- Linux/Windows/macOS portable build.

После прохождения этого gate #156 может строить `CalculatorState`, не вводя нового numeric representation.