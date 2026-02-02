# Алгоритм сериализации/десериализации JSON в связи МАО

[English](#english) | [Русский](#russian)

---

<a name="russian"></a>
## Русский

### Содержание

1. [Введение](#введение)
2. [Базовые понятия](#базовые-понятия)
3. [Базовый словарь](#базовый-словарь)
4. [Структура связи (rel_t)](#структура-связи)
5. [Импорт JSON → связи](#импорт-json--связи)
   - [null](#null-импорт)
   - [boolean](#boolean-импорт)
   - [array](#array-импорт)
   - [number](#number-импорт)
   - [string](#string-импорт)
   - [object](#object-импорт)
6. [Экспорт связи → JSON](#экспорт-связи--json)
   - [null](#null-экспорт)
   - [boolean](#boolean-экспорт)
   - [array](#array-экспорт)
   - [number](#number-экспорт)
   - [string](#string-экспорт)
   - [object](#object-экспорт)
7. [Примеры roundtrip-преобразований](#примеры-roundtrip-преобразований)
8. [Диаграммы структур](#диаграммы-структур)

---

### Введение

AVM выполняет двунаправленное преобразование между форматом JSON и внутренним представлением Модели Ассоциативных Отношений (МАО). Алгоритм реализован в двух функциях:

- **`import_json(json)`** — преобразует JSON-значение в граф связей МАО
- **`export_json(rel_t*, json&)`** — преобразует граф связей МАО обратно в JSON

Результат roundtrip-преобразования (импорт → экспорт) должен давать JSON, эквивалентный исходному.

### Базовые понятия

**Связь (link)** — упорядоченная пара элементов, представленная структурой `rel_t` с двумя указателями:
- **obj** (объект) — источник/контекст связи
- **sub** (субъект) — приёмник/тип связи

Каждая связь `rel(A, B)` означает: «A связано с B», где A — объект, B — субъект. Связь также содержит карту `ent_aspect` — множество сущностей данного отношения.

**Ключевой принцип**: все JSON-типы кодируются через комбинации связей. Тип значения определяется по полю `sub` корневой связи:
- `sub == R` → массив (шаг последовательности)
- `sub == String` → строка
- `sub == Unsigned` → беззнаковое число
- `sub == Integer` → целое число со знаком
- `sub == Float` → число с плавающей точкой
- `sub == Object` → объект

### Базовый словарь

При инициализации создаются 9 корневых связей:

| Связь | obj | sub | Назначение |
|-------|-----|-----|------------|
| **R** | E | E | Корневое отношение. Маркер шага последовательности (массива) |
| **E** | R | E | Корневая сущность. Представляет `null` и конец цепочки |
| **True** | R | R | Логическая истина |
| **False** | E | R | Логическая ложь |
| **Unsigned** | Unsigned | R | Маркер типа unsigned int |
| **Integer** | Integer | R | Маркер типа signed int |
| **Float** | Float | R | Маркер типа float |
| **String** | String | R | Маркер типа string |
| **Object** | Object | R | Маркер типа object |

Корневые связи R и E определяют базовую структуру:
```
R = (E, E)  — отображение сущности в сущность
E = (R, E)  — соответствие отношения и сущности
```

### Структура связи

```
rel_t
├── obj: rel_t*    — указатель на объект (источник)
├── sub: rel_t*    — указатель на субъект (приёмник/тип)
└── ent_aspect: map<rel_t*, rel_t*>  — карта сущностей
```

Создание связи `rel_t::rel(src, dst)`:
1. Если в карте `src` уже есть запись с ключом `dst` — возвращается существующая связь
2. Иначе создаётся новая связь с `obj = src`, `sub = dst` и регистрируется в карте `src`

---

### Импорт JSON → связи

Функция `import_json` принимает JSON-значение и возвращает указатель на корневую связь графа МАО.

<a name="null-импорт"></a>
#### null

```
JSON:  null
МАО:   rel_t::E
```

`null` отображается на корневую сущность E — это простейший случай, не требующий создания новых связей.

<a name="boolean-импорт"></a>
#### boolean

```
JSON:  true   →  rel_t::True   (obj=R, sub=R)
JSON:  false  →  rel_t::False  (obj=E, sub=R)
```

Булевые значения отображаются на предопределённые связи базового словаря.

<a name="array-импорт"></a>
#### array

Массив кодируется как **цепочка вложенных пар** с маркером R:

```
Паттерн: rel(rel(prev, element), R)
```

Алгоритм:
```
1. array = E                                    // начало цепочки
2. Для каждого элемента json-массива:
   element = import_json(элемент)               // рекурсивный импорт
   inner = rel(array, element)                  // пара (предыдущее, элемент)
   array = rel(inner, R)                        // обернуть с маркером R
3. Вернуть array
```

Пример для `[true, false]`:
```
Шаг 1: array = E
Шаг 2: element₀ = True
        inner₀ = rel(E, True)
        array = rel(inner₀, R)               // = rel(rel(E, True), R)
Шаг 3: element₁ = False
        inner₁ = rel(rel(rel(E, True), R), False)
        array = rel(inner₁, R)               // = rel(rel(rel(rel(E, True), R), False), R)
```

Структура в виде дерева:
```
rel ─────────────────── R        ← шаг 1 (элемент false)
 └─ obj: rel
         ├─ obj: rel ─── R      ← шаг 0 (элемент true)
         │    └─ obj: rel
         │         ├─ obj: E    ← конец цепочки
         │         └─ sub: True
         └─ sub: False
```

<a name="number-импорт"></a>
#### number

Числа кодируются как **последовательность бит** (от младшего к старшему) с маркером типа.

Алгоритм (для unsigned):
```
1. val = числовое значение
2. array = E                                    // начало цепочки бит
3. Для каждого бита (i = 1, 2, 4, 8, ..., до переполнения):
   bit = (val & i) ? True : False
   array = rel(rel(array, bit), R)
4. Вернуть rel(array, Unsigned)                 // обернуть с маркером типа
```

Для `integer` и `float` алгоритм тот же, но:
- `integer`: значение интерпретируется через `reinterpret_cast` как unsigned для побитового кодирования, маркер — `Integer`
- `float`: значение интерпретируется через `reinterpret_cast` как unsigned для побитового кодирования, маркер — `Float`

Пример для числа `5` (unsigned):
```
5 в двоичном = ...0000101

Биты (от младшего к старшему):
  бит 0 (маска 1):  5 & 1 = 1  → True
  бит 1 (маска 2):  5 & 2 = 0  → False
  бит 2 (маска 4):  5 & 4 = 4  → True
  бит 3 (маска 8):  5 & 8 = 0  → False
  ... (все остальные биты — False)

Результат: rel(цепочка_бит, Unsigned)
```

Структура:
```
rel ──── Unsigned       ← маркер типа
 └─ obj: цепочка бит
         rel ─── R      ← бит 63 (False)
          └─ ...
              rel ─── R ← бит 2 (True)
               └─ rel
                   ├─ obj: rel ── R  ← бит 1 (False)
                   │    └─ rel
                   │        ├─ obj: rel ── R  ← бит 0 (True)
                   │        │    └─ rel
                   │        │        ├─ obj: E  ← конец
                   │        │        └─ sub: True
                   │        └─ sub: False
                   └─ sub: True
```

<a name="string-импорт"></a>
#### string

Строка кодируется как **последовательность символов**, где каждый символ — **последовательность из 8 бит**.

Алгоритм:
```
1. str = строковое значение
2. array = E                                    // начало цепочки символов
3. Для каждого символа строки:
   a. val = ASCII-код символа (uint8_t)
   b. sing = E                                  // начало цепочки бит символа
   c. Для каждого бита (i = 1, 2, 4, ..., 128):
      bit = (val & i) ? True : False
      sing = rel(rel(sing, bit), R)
   d. array = rel(rel(array, sing), R)          // добавить символ в цепочку
4. Вернуть rel(array, String)                   // обернуть с маркером типа
```

Пример для строки `"A"`:
```
'A' = ASCII 65 = 0b01000001

Биты (от младшего к старшему):
  бит 0 (маска 1):   → True
  бит 1 (маска 2):   → False
  бит 2 (маска 4):   → False
  бит 3 (маска 8):   → False
  бит 4 (маска 16):  → False
  бит 5 (маска 32):  → False
  бит 6 (маска 64):  → True
  бит 7 (маска 128): → False
```

Структура (двухуровневая):
```
rel ──── String                          ← маркер типа
 └─ obj: цепочка символов
         rel ──── R                      ← символ 'A'
          └─ obj: rel
                  ├─ obj: E              ← конец цепочки символов
                  └─ sub: цепочка бит    ← 8 бит символа 'A'
                         rel ── R        ← бит 7 (False)
                          └─ ...
                              rel ── R   ← бит 0 (True)
                               └─ rel
                                   ├─ obj: E
                                   └─ sub: True
```

<a name="object-импорт"></a>
#### object

JSON-объект кодируется как **последовательность пар (ключ, значение)** с маркером Object.

Алгоритм:
```
1. array = E                                    // начало цепочки пар
2. Для каждой пары (key, value) в JSON-объекте:
   key_rel = import_json(json(key))             // ключ — всегда строка
   value_rel = import_json(value)               // значение — любой тип
   pair = rel(key_rel, value_rel)               // пара (ключ → значение)
   array = rel(rel(array, pair), R)             // добавить пару в цепочку
3. Вернуть rel(array, Object)                   // обернуть с маркером типа
```

Пример для `{"name": "John"}`:
```
key   = import_json("name")    → связь строки "name"
value = import_json("John")    → связь строки "John"
pair  = rel(key, value)        → пара (ключ, значение)
```

Структура:
```
rel ──── Object                              ← маркер типа
 └─ obj: цепочка пар
         rel ──── R                          ← пара "name":"John"
          └─ obj: rel
                  ├─ obj: E                  ← конец цепочки
                  └─ sub: rel                ← пара
                         ├─ obj: String(...) ← "name"
                         └─ sub: String(...) ← "John"
```

---

### Экспорт связи → JSON

Функция `export_json` определяет тип связи по полю `sub` и вызывает соответствующий обработчик.

<a name="null-экспорт"></a>
#### null

```
Если ent == E  →  json(null)
```

<a name="boolean-экспорт"></a>
#### boolean

```
Если ent == True   →  json(true)
Если ent == False  →  json(false)
```

<a name="array-экспорт"></a>
#### array

```
Если ent->sub == R  →  вызов export_seq(ent, j)
```

Функция `export_seq` рекурсивно обходит цепочку:
```
export_seq(ent, j):
  Если ent == E:
    j = []                         // пустой массив, конец цепочки
    return
  Если ent->sub != R:
    j = []                         // не шаг последовательности
    return
  export_seq(ent->obj->obj, j)     // рекурсия по prev (obj внутренней пары)
  export_json(ent->obj->sub, last) // экспорт текущего элемента (sub внутренней пары)
  j.push_back(last)                // добавить в массив
```

Навигация по цепочке:
```
ent                         — текущий шаг: rel(rel(prev, element), R)
ent->obj                    — внутренняя пара: rel(prev, element)
ent->obj->obj               — prev: предыдущий шаг или E
ent->obj->sub               — element: текущий элемент
ent->sub                    — R: маркер шага
```

<a name="number-экспорт"></a>
#### number

```
Если ent->sub == Unsigned/Integer/Float:
  1. export_seq(ent->obj, j)     // экспорт цепочки бит как массив boolean
  2. Восстановить число из массива бит:
     val = 0, i = 1
     Для каждого элемента массива:
       Если элемент == true:
         val |= i
       i <<= 1
  3. Для Integer: reinterpret_cast<int64_t>(val)
     Для Float: reinterpret_cast<double>(val)
```

<a name="string-экспорт"></a>
#### string

```
Если ent->sub == String:
  1. export_seq(ent->obj, j)     // экспорт цепочки символов как массив
  2. Для каждого элемента массива (массив бит символа):
     val = 0, i = 1
     Для каждого бита:
       Если бит == true:
         val |= i
       i <<= 1
     str += char(val)
  3. j = str
```

<a name="object-экспорт"></a>
#### object

```
Если ent->sub == Object:
  1. j = {}                      // пустой объект
  2. export_object_chain(ent->obj, j)
```

Функция `export_object_chain` рекурсивно обходит цепочку пар:
```
export_object_chain(node, j):
  Если node == E: return          // конец цепочки
  Если node->sub != R: return     // не шаг последовательности

  export_object_chain(node->obj->obj, j)  // рекурсия по prev

  pair_rel = node->obj->sub               // пара (ключ, значение)
  export_json(pair_rel->obj, key_json)     // экспорт ключа
  export_json(pair_rel->sub, value_json)   // экспорт значения
  Если key_json — строка:
    j[key_json] = value_json
```

---

### Примеры roundtrip-преобразований

#### Пример 1: `null`
```
JSON → import_json(null) → E → export_json(E) → null ✓
```

#### Пример 2: `[true, false]`
```
JSON:  [true, false]

Импорт:
  array = E
  array = rel(rel(E, True), R)
  array = rel(rel(rel(rel(E, True), R), False), R)

Экспорт:
  ent->sub == R → export_seq
  → рекурсия: ent->obj->obj->sub == R → export_seq
    → рекурсия: ent->obj->obj->obj->obj == E → j = []
    → элемент: ent->obj->obj->obj->sub == True → push true
  → элемент: ent->obj->sub == False → push false
  → j = [true, false] ✓
```

#### Пример 3: `5` (unsigned)
```
JSON:  5

Импорт:
  5 = 0b...0101
  Цепочка бит: [True, False, True, False, ..., False] (64 бита)
  Результат: rel(цепочка, Unsigned)

Экспорт:
  ent->sub == Unsigned
  → export_seq(ent->obj) → [true, false, true, false, ..., false]
  → val = 0 | 1 | 0 | 4 | 0 | ... = 5
  → j = 5 ✓
```

#### Пример 4: `"Hi"`
```
JSON:  "Hi"

Импорт:
  'H' = 72 = 0b01001000 → бит-цепочка [F,F,F,T,F,F,T,F]
  'i' = 105 = 0b01101001 → бит-цепочка [T,F,F,T,F,T,T,F]
  Символы обёрнуты в цепочку: rel(rel(rel(rel(E, bits_H), R), bits_i), R)
  Результат: rel(цепочка_символов, String)

Экспорт:
  ent->sub == String
  → export_seq(ent->obj) → [[F,F,F,T,F,F,T,F], [T,F,F,T,F,T,T,F]]
  → символ 0: 0|0|0|8|0|0|64|0 = 72 = 'H'
  → символ 1: 1|0|0|8|0|32|64|0 = 105 = 'i'
  → j = "Hi" ✓
```

#### Пример 5: `{"a": 1}`
```
JSON:  {"a": 1}

Импорт:
  key = import_json("a")    → rel(бит-цепочка_a, String)
  value = import_json(1)    → rel(бит-цепочка_1, Unsigned)
  pair = rel(key, value)
  array = rel(rel(E, pair), R)
  Результат: rel(array, Object)

Экспорт:
  ent->sub == Object
  → j = {}
  → export_object_chain(ent->obj):
    → node->obj->obj == E → конец рекурсии
    → pair = node->obj->sub
    → key = export_json(pair->obj) → "a"
    → value = export_json(pair->sub) → 1
    → j["a"] = 1
  → j = {"a": 1} ✓
```

### Диаграммы структур

#### Общая схема кодирования

```
JSON тип        Структура МАО                     Определение типа
─────────────   ─────────────────────────────────  ──────────────────
null            E                                  ent == E
true            True                               ent == True
false           False                              ent == False
array           rel(rel(prev, elem), R)            ent->sub == R
unsigned        rel(цепочка_бит, Unsigned)         ent->sub == Unsigned
integer         rel(цепочка_бит, Integer)          ent->sub == Integer
float           rel(цепочка_бит, Float)            ent->sub == Float
string          rel(цепочка_символов, String)      ent->sub == String
object          rel(цепочка_пар, Object)           ent->sub == Object
```

#### Паттерн цепочки (последовательности)

Все составные типы используют единый паттерн цепочки:

```
                    rel ──── R         ← шаг N (последний)
                     │
                     └─ obj: rel
                             ├─ obj: rel ──── R         ← шаг N-1
                             │        │
                             │        └─ obj: rel
                             │                ├─ obj: ...  ← шаг N-2
                             │                └─ sub: элемент[N-1]
                             │
                             └─ sub: элемент[N]

Начало цепочки (шаг 0):

                    rel ──── R
                     │
                     └─ obj: rel
                             ├─ obj: E      ← конец цепочки
                             └─ sub: элемент[0]
```

#### Иерархия типов

```
             E ◄──────────── null
             │
             R ◄──────────── маркер массива (шаг последовательности)
            ╱│╲
           ╱ │ ╲
     True   False   Unsigned  Integer  Float  String  Object
      ↑       ↑
    (R,R)   (E,R)
```

---

<a name="english"></a>
## English

### Contents

1. [Introduction](#introduction)
2. [Core Concepts](#core-concepts)
3. [Base Vocabulary](#base-vocabulary)
4. [Link Structure (rel_t)](#link-structure)
5. [Import: JSON → Links](#import-json--links)
6. [Export: Links → JSON](#export-links--json)
7. [Roundtrip Examples](#roundtrip-examples)
8. [Structure Diagrams](#structure-diagrams)

---

### Introduction

AVM performs bidirectional conversion between JSON format and the internal representation of the Associative Relations Model (ARM). The algorithm is implemented in two functions:

- **`import_json(json)`** — converts a JSON value into a graph of ARM links
- **`export_json(rel_t*, json&)`** — converts an ARM link graph back to JSON

The roundtrip conversion (import → export) produces JSON equivalent to the original.

### Core Concepts

**Link** — an ordered pair of elements, represented by the `rel_t` structure with two pointers:
- **obj** (object) — source/context of the link
- **sub** (subject) — destination/type of the link

Each link `rel(A, B)` means: "A is linked to B", where A is the object and B is the subject. A link also contains an `ent_aspect` map — the set of entities of this relation.

**Key principle**: all JSON types are encoded through combinations of links. The value type is determined by the `sub` field of the root link:
- `sub == R` → array (sequence step)
- `sub == String` → string
- `sub == Unsigned` → unsigned number
- `sub == Integer` → signed integer
- `sub == Float` → floating-point number
- `sub == Object` → object

### Base Vocabulary

9 root links are created during initialization:

| Link | obj | sub | Purpose |
|------|-----|-----|---------|
| **R** | E | E | Root relation. Sequence step (array) marker |
| **E** | R | E | Root entity. Represents `null` and end of chain |
| **True** | R | R | Boolean true |
| **False** | E | R | Boolean false |
| **Unsigned** | Unsigned | R | Unsigned int type marker |
| **Integer** | Integer | R | Signed int type marker |
| **Float** | Float | R | Float type marker |
| **String** | String | R | String type marker |
| **Object** | Object | R | Object type marker |

Root links R and E define the base structure:
```
R = (E, E)  — mapping of entity to entity
E = (R, E)  — correspondence of relation and entity
```

### Link Structure

```
rel_t
├── obj: rel_t*    — pointer to object (source)
├── sub: rel_t*    — pointer to subject (destination/type)
└── ent_aspect: map<rel_t*, rel_t*>  — entity map
```

Creating a link `rel_t::rel(src, dst)`:
1. If `src`'s map already has an entry with key `dst` — the existing link is returned
2. Otherwise a new link with `obj = src`, `sub = dst` is created and registered in `src`'s map

---

### Import: JSON → Links

The `import_json` function takes a JSON value and returns a pointer to the root link of the ARM graph.

#### null
```
JSON:  null
ARM:   rel_t::E
```

#### boolean
```
JSON:  true   →  rel_t::True   (obj=R, sub=R)
JSON:  false  →  rel_t::False  (obj=E, sub=R)
```

#### array

Arrays are encoded as a **chain of nested pairs** with the R marker:

```
Pattern: rel(rel(prev, element), R)
```

Algorithm:
```
1. array = E                              // start of chain
2. For each element in JSON array:
   element = import_json(item)            // recursive import
   inner = rel(array, element)            // pair (previous, element)
   array = rel(inner, R)                  // wrap with R marker
3. Return array
```

#### number

Numbers are encoded as a **sequence of bits** (LSB first) with a type marker.

Algorithm (for unsigned):
```
1. val = numeric value
2. array = E                              // start of bit chain
3. For each bit (i = 1, 2, 4, 8, ..., up to overflow):
   bit = (val & i) ? True : False
   array = rel(rel(array, bit), R)
4. Return rel(array, Unsigned)            // wrap with type marker
```

For `integer` and `float`: the value is reinterpreted as unsigned via `reinterpret_cast` for bitwise encoding, with markers `Integer` and `Float` respectively.

#### string

Strings are encoded as a **sequence of characters**, where each character is a **sequence of 8 bits**.

Algorithm:
```
1. str = string value
2. array = E                              // start of character chain
3. For each character:
   a. val = ASCII code (uint8_t)
   b. sing = E                            // start of bit chain for character
   c. For each bit (i = 1, 2, 4, ..., 128):
      bit = (val & i) ? True : False
      sing = rel(rel(sing, bit), R)
   d. array = rel(rel(array, sing), R)    // add character to chain
4. Return rel(array, String)              // wrap with type marker
```

#### object

JSON objects are encoded as a **sequence of key-value pairs** with the Object marker.

Algorithm:
```
1. array = E                              // start of pair chain
2. For each (key, value) in JSON object:
   key_rel = import_json(json(key))       // key is always a string
   value_rel = import_json(value)         // value is any type
   pair = rel(key_rel, value_rel)         // pair (key → value)
   array = rel(rel(array, pair), R)       // add pair to chain
3. Return rel(array, Object)              // wrap with type marker
```

---

### Export: Links → JSON

The `export_json` function determines the link type by the `sub` field and calls the corresponding handler.

#### null
```
If ent == E  →  json(null)
```

#### boolean
```
If ent == True   →  json(true)
If ent == False  →  json(false)
```

#### array

The `export_seq` function recursively traverses the chain:
```
export_seq(ent, j):
  If ent == E: j = []                      // empty array, end of chain
  If ent->sub != R: j = []                 // not a sequence step
  export_seq(ent->obj->obj, j)             // recurse to prev
  export_json(ent->obj->sub, last)         // export current element
  j.push_back(last)
```

Chain navigation:
```
ent              — current step: rel(rel(prev, element), R)
ent->obj         — inner pair: rel(prev, element)
ent->obj->obj    — prev: previous step or E
ent->obj->sub    — element: current element
ent->sub         — R: step marker
```

#### number

```
If ent->sub == Unsigned/Integer/Float:
  1. export_seq(ent->obj, j)     // export bit chain as boolean array
  2. Reconstruct number from bit array:
     val = 0, i = 1
     For each array element:
       If element == true: val |= i
       i <<= 1
  3. For Integer: reinterpret_cast<int64_t>(val)
     For Float: reinterpret_cast<double>(val)
```

#### string

```
If ent->sub == String:
  1. export_seq(ent->obj, j)     // export character chain as array
  2. For each element (bit array of a character):
     val = 0, i = 1
     For each bit:
       If bit == true: val |= i
       i <<= 1
     str += char(val)
  3. j = str
```

#### object

The `export_object_chain` function recursively traverses key-value pairs:
```
export_object_chain(node, j):
  If node == E: return                     // end of chain
  If node->sub != R: return                // not a sequence step
  export_object_chain(node->obj->obj, j)   // recurse to prev
  pair_rel = node->obj->sub               // pair (key, value)
  export_json(pair_rel->obj, key_json)    // export key
  export_json(pair_rel->sub, value_json)  // export value
  If key_json is string: j[key_json] = value_json
```

---

### Roundtrip Examples

#### Example: `[true, false]`
```
Import:
  array = E
  array = rel(rel(E, True), R)
  array = rel(rel(prev, False), R)

Export:
  sub == R → export_seq
  → recurse to prev (sub == R → export_seq)
    → recurse to prev (== E → j = [])
    → element = True → push true
  → element = False → push false
  → [true, false] ✓
```

#### Example: `{"a": 1}`
```
Import:
  key = import_json("a") → String link
  value = import_json(1) → Unsigned link
  pair = rel(key, value)
  array = rel(rel(E, pair), R)
  result = rel(array, Object)

Export:
  sub == Object → j = {}
  → export_object_chain:
    → pair = node->obj->sub
    → key = "a", value = 1
    → j["a"] = 1
  → {"a": 1} ✓
```

### Structure Diagrams

#### Encoding Overview

```
JSON type       ARM structure                        Type detection
─────────────   ──────────────────────────────────   ──────────────────
null            E                                    ent == E
true            True                                 ent == True
false           False                                ent == False
array           rel(rel(prev, elem), R)              ent->sub == R
unsigned        rel(bit_chain, Unsigned)             ent->sub == Unsigned
integer         rel(bit_chain, Integer)              ent->sub == Integer
float           rel(bit_chain, Float)                ent->sub == Float
string          rel(char_chain, String)              ent->sub == String
object          rel(pair_chain, Object)              ent->sub == Object
```

#### Chain Pattern (sequences)

All composite types use the same chain pattern:

```
                    rel ──── R         ← step N (last)
                     │
                     └─ obj: rel
                             ├─ obj: rel ──── R         ← step N-1
                             │        │
                             │        └─ obj: rel
                             │                ├─ obj: ...  ← step N-2
                             │                └─ sub: element[N-1]
                             │
                             └─ sub: element[N]

Start of chain (step 0):

                    rel ──── R
                     │
                     └─ obj: rel
                             ├─ obj: E      ← end of chain
                             └─ sub: element[0]
```

#### Type Hierarchy

```
             E ◄──────────── null
             │
             R ◄──────────── array marker (sequence step)
            ╱│╲
           ╱ │ ╲
     True   False   Unsigned  Integer  Float  String  Object
      ↑       ↑
    (R,R)   (E,R)
```
