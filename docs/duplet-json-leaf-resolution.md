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

Первый нативный resolver поддерживает три явные формы:

```text
Leaf := LinkAnchor | SymbolAnchor | IntegerValue

LinkAnchor := {"$link": positive_integer}
SymbolAnchor := {"$symbol": string}
IntegerValue := {"$integer": int64_json_integer}
```

Каждый объект-лист содержит **ровно один** marker. Raw JSON scalar сам по себе листом AVM не является.

Следовательно, это недопустимо:

```json
"integer_add"
```

```json
7
```

а это допустимо:

```json
{"$symbol":"integer_add"}
```

```json
{"$integer":7}
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

## Text не входит в этот gate

Форма вроде:

```json
{"$text":"hello"}
```

**не вводится**, пока #128 не зафиксирует canonical Text/byte-string denotation.

Причина принципиальная: UTF-8/JSON serialization не должна автоматически становиться semantic identity. Сначала нужен frontend-neutral link encoding текста с отдельными `find/realize/decode` контрактами, затем `$text` сможет быть лишь projection adapter-ом к этой структуре.

До этого любой `$text` должен отклоняться как неизвестный leaf marker.

## Контекстные ссылки и выражения

Контекстные ссылки (`$ent/$rel/$sub/$obj`, path/reference semantics jsonRVM) также не являются простыми value leaves. Их перенос относится к #125/#126 и semantic migrator #174.

Native JSON resolver не должен угадывать их по строкам.

## Инварианты

Для любого поддержанного листа обязательны:

1. parse/project не меняет `LinkStore`;
2. чтение и `find` не вызывают `intern`;
3. materialization происходит только через явный `realize_projection`;
4. одинаковое canonical значение сходится к одной structural identity;
5. неизвестное имя не создаёт point;
6. `Executor`, `LinkStore` и canonical value core не зависят от JSON syntax;
7. persistent reopen сохраняет structural meaning.
