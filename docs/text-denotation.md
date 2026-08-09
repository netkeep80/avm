# Каноническая денотация Text в AVM

Родительская задача: #180. Связанные задачи: #128, #173, #169.

## Решение

Первый канонический Text-контракт AVM определяет **последовательность байтов**, а не Unicode code points и не JSON string.

Semantic domain:

```text
Text := finite sequence of bytes 0..255
```

Это сознательное решение.

AVM core не должен решать:

- является ли последовательность корректным UTF-8;
- требуется ли Unicode normalization;
- как frontend представил escape-последовательности;
- является ли набор байтов текстом, именем, содержимым файла или сетевым payload.

Эти политики принадлежат frontend/protocol layer.

Для Native JSON будущий `$text` будет брать байты UTF-8 строки, уже полученной JSON parser-ом, и проецировать их в этот же canonical Text. Другой frontend может подать ту же последовательность байтов без JSON.

## Почему не Unicode tree

Unicode scalar sequence выглядит более «текстовым», но делает L4 зависимым от внешней кодовой модели. Тогда пришлось бы нормативно решить normalization, invalid scalar handling и versioning Unicode semantics.

Byte string имеет более фундаментальный контракт:

```text
raw bytes -> links
```

а интерпретация байтов остаётся отдельной связью/протоколом.

## Vocabulary

`TextVocabulary` содержит шесть независимых identity:

```text
text_marker
text_end
byte_marker
byte_end
bit_zero
bit_one
```

Все они:

- должны существовать в `LinkStore`;
- должны быть различны;
- не несут JSON/UTF-specific meaning;
- после persistent reopen продолжают задавать тот же structural contract.

## Канонический Byte

Один byte кодируется ровно восемью bit cells, от младшего бита к старшему:

```text
bit0 -> bit1 -> ... -> bit7 -> byte_end
```

Каждая cell:

```text
Link(bit_zero | bit_one, tail)
```

Byte root:

```text
Link(byte_marker, bit_chain)
```

Например значение `5 = 00000101b` имеет логическую форму:

```text
Byte(1,0,1,0,0,0,0,0)
```

Фиксированная длина восемь бит исключает неоднозначность ведущих нулей.

## Канонический Text

Text — список canonical Byte roots в исходном порядке.

Последовательность:

```text
byte0 -> byte1 -> ... -> byteN -> text_end
```

Каждая cell:

```text
Link(byte_root, tail)
```

Text root:

```text
Link(text_marker, byte_sequence)
```

Пустая строка байтов:

```text
Link(text_marker, text_end)
```

Префикс и полная строка различаются, потому что каждая последовательность завершается `text_end`.

## Каноничность

Каноничность обеспечивается единственным `LinkStore::intern(begin,end)`.

Для одного store и одного `TextVocabulary`:

```text
same bytes -> same Byte roots -> same sequence cells -> same Text root
```

Никакая таблица `std::string -> LinkId` не нужна.

## Наблюдающий поиск

```text
find_byte(store, vocabulary, value)
find_text(store, vocabulary, bytes)
```

только вызывают `find` и read-only validation.

Обязательный инвариант:

```text
store.size() before == store.size() after
```

при hit и miss.

## Явная materialization

```text
realize_byte(...)
realize_text(...)
```

явно используют `intern`.

Повторная realization того же значения идемпотентна и возвращает тот же canonical `LinkId` без дальнейшего роста store.

## Наблюдающее декодирование

```text
decode_byte(...)
decode_text(...)
is_byte(...)
is_text(...)
```

не создают links.

Decoder проверяет:

- vocabulary anchors;
- существование root;
- правильный marker wrapper;
- ровно восемь bit cells для Byte;
- только `bit_zero` / `bit_one` markers;
- корректный `byte_end`;
- для Text — только canonical Byte roots;
- `text_end`;
- циклы/аномальную цепочку;
- настроенный предел длины Text.

Malformed структура не «исправляется» materialization-ом и не интерпретируется приблизительно.

## UTF-8

Core API оперирует bytes:

```text
span<uint8_t> -> Text LinkId
Text LinkId -> vector<uint8_t>
```

UTF-8 — один из возможных frontend contracts поверх этого слоя.

Для Native JSON это означает:

1. JSON parser разбирает JSON string и выдаёт строку в своей UTF-8 representation;
2. adapter берёт точные bytes результата;
3. bytes проецируются в canonical Text;
4. `Executor` и Text core не знают о JSON.

Каноническая эквивалентность определяется байтами, а не Unicode normalization:

```text
C3 A9 != 65 CC 81
```

если frontend заранее не применил свою явную normalization policy.

## Embedded NUL

Байт `0x00` является обычным значением и не завершает Text.

Следовательно:

```text
[0x41, 0x00, 0x42]
```

— валидный Text из трёх байтов.

Это ещё одна причина не использовать C-string semantics в core.

## Отношение к JsonValueCodec

Существующий `JsonValueCodec` остаётся compatibility/serialization механизмом. Его `string_relation` и JSON-specific vocabulary не становятся canonical Text identity.

Новый Text contract:

- не зависит от `nlohmann::json`;
- не использует `JsonValueVocabulary`;
- не знает JSON value types;
- может использоваться любым frontend.

## Будущий `$text`

После этого gate Native JSON сможет добавить:

```json
{"$text":"hello"}
```

но эта форма будет только adapter-ом:

```text
JSON UTF-8 bytes
 -> canonical Text ProjectionDescription
 -> find_projection | realize_projection
```

Она не создаёт отдельный JSON Text universe.

## Инварианты

1. representation состоит только из `LinkId` и `Link(begin,end)`;
2. `find/decode/is_*` не пишут в store;
3. `realize_*` — единственная materialization boundary;
4. равные byte sequences имеют одну canonical identity в одном store/vocabulary;
5. exact byte content сохраняется, включая `0x00`;
6. persistent reopen сохраняет structural meaning;
7. core не зависит от JSON, Anum, UTF parser или UI;
8. Text serialization не определяет execution semantics.
