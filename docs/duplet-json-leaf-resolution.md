# Разрешение листьев в нативном Duplet JSON AVM

Родительские задачи: #169, #173. Связанные задачи: #128, #130.

## Назначение

`duplet-json/1` описывает структуру связей через пары `<<` / `>>`, но сама JSON-сериализация не определяет semantic identity листьев.

Нормативная граница:

```text
JSON token
 -> явный leaf resolver
 -> ProjectionRef / ProjectionNode
 -> find_projection | realize_projection
 -> canonical LinkId
```

Parser пары не создаёт `LinkId`, не вызывает `create_point()` и не содержит скрытой таблицы `string -> identity`.

## Грамматика листьев v1

Первый нативный resolver поддерживает четыре явные формы:

```text
Leaf := LinkAnchor | SymbolAnchor | IntegerValue | TextValue

LinkAnchor := {"$link": positive_integer}
SymbolAnchor := {"$symbol": string}
IntegerValue := {"$integer": int64_json_integer}
TextValue := {"$text": string}
```

Каждый объект-лист содержит **ровно один** marker. Raw JSON scalar сам по себе листом AVM не является.

Следовательно, это недопустимо:

```json
"integer_add"
```

```json
7
```

```json
"hello"
```

а это допустимо:

```json
{"$symbol":"integer_add"}
```

```json
{"$integer":7}
```

```json
{"$text":"hello"}
```

## `$link`: существующая identity

```json
{"$link":42}
```

проецируется как:

```text
ProjectionRef::Anchor(42)
```

Resolver не проверяет наличие `42` в конкретном store и не создаёт её. Это остаётся обязанностью последующей операции:

- `find_projection` возвращает miss без изменения store;
- `realize_projection` отклоняет отсутствующий anchor до materialization.

`0` запрещён как `invalid_link_id`.

## `$symbol`: имя, принадлежащее вызывающему коду

```json
{"$symbol":"integer_add"}
```

разрешается только через явно переданный `SymbolAnchors`:

```text
"integer_add" -> IntegerVocabulary::add_relation
"true"        -> BootstrapVocabulary::true_value
"false"       -> BootstrapVocabulary::false_value
"nil"         -> BootstrapVocabulary::nil
```

Таблица символов:

- принадлежит caller/frontend configuration;
- не хранится скрыто внутри `LinkStore`;
- не создаёт identity для неизвестного имени;
- может использовать разные spelling-и для одной и той же semantic identity;
- не является частью core execution semantics.

Неизвестный `$symbol` — deterministic projection error.

Таким образом, строка является **именем ссылки на уже определённую identity**, а не источником новой identity.

## `$integer`: каноническая проекция Integer

```json
{"$integer":7}
```

не превращается в JSON-tagged link и не хранится в host-side variant.

Resolver строит обычный `ProjectionDescription`, эквивалентный canonical Integer denotation из `IntegerVocabulary`:

```text
$integer
 -> sign / bit cells / magnitude_end
 -> ProjectionDescription
 -> find_projection | realize_projection
 -> тот же LinkId, что find_integer / realize_integer
```

Это означает:

- projection не меняет store;
- `find_projection` не материализует число;
- `realize_projection` использует обычный canonical `intern`;
- повторная realization идемпотентна;
- persistent reopen сохраняет identity и значение;
- direct C++ API и Native JSON сходятся в одну структуру.

Текущий canonical Integer contract ограничен `int64`, поэтому `$integer` принимает только целые JSON numbers в диапазоне `INT64_MIN..INT64_MAX`.

## `$text`: проекция JSON-строки в канонический Text

```json
{"$text":"hello"}
```

не создаёт JSON-specific строковый тип. После того как JSON parser разобрал escapes, resolver берёт **точную последовательность байтов** полученной строки и строит `ProjectionDescription`, эквивалентный canonical byte-string Text из `TextVocabulary`.

Нормативный путь:

```text
JSON string
 -> bytes строки после JSON parsing
 -> canonical Byte/Text projection
 -> find_projection | realize_projection
 -> тот же LinkId, что find_text | realize_text
```

AVM не выполняет Unicode normalization на этом пути.

Поэтому:

```json
{"$text":"\u00e9"}
```

и literal UTF-8 `é` после JSON parsing дают одинаковые байты `C3 A9` и сходятся к одной identity.

Но:

```json
{"$text":"e\u0301"}
```

даёт `65 CC 81` и остаётся другой byte identity.

Это не ошибка: Unicode normalization является явной политикой frontend-а, а canonical Text AVM определён побайтово.

Встроенный NUL также является обычным байтом. DOM-строка `A 00 B` проецируется как Text из трёх байтов и не обрезается C-string semantics.

Пустая JSON-строка:

```json
{"$text":""}
```

проецируется в canonical empty Text `Link(text_marker, text_end)`.

Как и `$integer`, `$text` ничего не materialize на этапе parse/project.

## Boolean и nil

`true`, `false` и `nil` уже имеют canonical identities в `BootstrapVocabulary`. Поэтому v1 не вводит отдельные JSON-tagged value classes для них.

Их рекомендуется именовать через caller-owned symbols:

```json
{"$symbol":"true"}
{"$symbol":"false"}
{"$symbol":"nil"}
```

Это сохраняет единственную identity каждого значения и не создаёт второй Boolean universe внутри JSON frontend.

## Пример: `7 + 3`

```json
{
  "<<": {"$symbol":"integer_add"},
  ">>": {
    "<<": {"$integer":7},
    ">>": {"$integer":3}
  }
}
```

После projection и explicit realization получается:

```text
Link(
  IntegerVocabulary::add_relation,
  Link(Integer(7), Integer(3))
)
```

То есть ровно та же `RelationEntity`, которую создаёт direct link-native API:

```text
(relation, subject, object)
 = (integer_add, Integer(7), Integer(3))
```

`Executor` не знает, что entity когда-либо была записана в JSON.

## Контекстные ссылки и выражения

Контекстные ссылки (`$ent/$rel/$sub/$obj`, path/reference semantics jsonRVM) не являются простыми value leaves.

Их перенос относится к #125/#126 и semantic migrator #174. Native JSON resolver не должен угадывать их по строкам и не должен превращать старые jsonRVM reference expressions в `$symbol`.

Это отдельный класс syntax/semantics поверх execution context, а не расширение таблицы значений.

## Инварианты

Для любого поддержанного листа обязательны:

1. parse/project не меняет `LinkStore`;
2. чтение и `find` не вызывают `intern`;
3. materialization происходит только через явный `realize_projection`;
4. одинаковое canonical значение сходится к одной structural identity;
5. неизвестное имя не создаёт point;
6. `$integer` сходится с canonical Integer core;
7. `$text` сходится с canonical byte-string Text core;
8. Boolean/nil используют существующие singleton identities;
9. `Executor`, `LinkStore` и canonical value core не зависят от JSON syntax;
10. persistent reopen сохраняет structural meaning.
