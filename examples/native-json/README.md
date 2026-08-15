# Примеры Native JSON `duplet-json/1`

Issue: #175. Parent: #169.

Этот каталог — пользовательский corpus канонической дуплетной JSON-нотации AVM. Файлы являются обычным transport/source representation и не определяют отдельную семантику исполнения.

Нормативный путь для каждого примера:

```text
source duplet-json/1
  -> strict parse/project
  -> ProjectionDescription
  -> find_projection | realize_projection
  -> canonical LinkStore
  -> ordinary Executor, если root исполним
```

`project` и `find` не materialize-ят отсутствующие links. Запись происходит только через явный `realize_projection`.

## `pair.json` — явный дуплет

```json
{
  "$avm": "duplet-json/1",
  "$root": {
    "<<": {"$symbol": "left"},
    ">>": {"$symbol": "right"}
  }
}
```

`left` и `right` — caller-owned anchors. Пример не разрешает строки сам и не создаёт hidden points. Для fresh store `find_projection` точной пары возвращает miss без роста хранилища; `realize_projection` затем явно materialize-ит canonical `Link(left,right)`.

## `relation.json` — `RelationEntity`

Триединая сущность представлена только вложенными дуплетами:

```text
(relation, subject, object)
= Link(relation, Link(subject, object))
```

В файле `relation`, `subject`, `object` — caller-owned symbols. После realization обычный `decode_relation_entity` возвращает те же три роли. Отдельного физического triplet-типа или swapped `(rel,(obj,sub))` режима нет.

## `integer-add.json` — canonical Integer arithmetic

```text
(integer_add, Integer(7), Integer(3))
```

`$integer` проецируется в существующую canonical Integer denotation, а `$symbol: integer_add` разрешается caller-ом в `IntegerVocabulary::add_relation`.

После realization root является обычной `RelationEntity`. Тот же `Executor`, где зарегистрирован canonical Integer vocabulary, возвращает canonical `Integer(10)`.

В JSON нет opcode-dispatch: строка `integer_add` только имя caller-owned anchor для уже существующей relation identity.

## `text.json` — canonical Text

`$text` строит structural Text denotation из bytes UTF-8. Пример содержит `Привет, AVM`; после realization значение читается обычным `decode_text`.

AVM не нормализует Unicode скрыто: Text хранит точную byte sequence, которую передал frontend.

## Минимальный library walkthrough

Новый Native JSON CLI специально не добавляется: для #175 достаточно явного library path, а отдельный CLI создал бы лишний mode-selection surface.

Концептуально runner выглядит так:

```cpp
avm::InMemoryLinkStore store;
avm::BootstrapRuntime runtime(store);
const auto integers = avm::IntegerVocabulary::create(store);
const auto text = avm::TextVocabulary::create(store);

avm::register_integer_arithmetic(runtime.executor(), integers);

avm::json_duplet::SymbolAnchors symbols{
    {"integer_add", integers.add_relation},
    {"unit", runtime.vocabulary().unit},
};

const avm::json_duplet::NativeLeafResolver resolver(integers, text, symbols);
const avm::ProjectionDescription description =
    avm::json_duplet::project_duplet_document_text<nlohmann::ordered_json>(source, resolver);

const auto found = avm::find_projection(store, description);       // read-only
const auto realized = avm::realize_projection(store, description); // explicit write
const avm::LinkId result = runtime.executor().execute(realized.root);
```

Реальный CI runner `native_json_examples_tests` читает файлы этого каталога и проверяет точные invariants для pair, RelationEntity, Integer и Text.

## Structural converter и semantic migrator — разные инструменты

`avm-json-convert` преобразует только explicit representation:

```text
{$rel:R,$sub:S,$obj:O}
  <->
{<<:R,>>:{<<:S,>>:O}}
```

Он не исполняет программу и не переносит historical jsonRVM semantics.

Semantic migrator #174 — отдельный evidence-driven adapter: legacy source может быть скомпилирован в native document либо завершиться typed source failure. В Native JSON examples нет legacy `$rel/$sub/$obj` fallback.

## Showcase

Dear ImGui Showcase использует тот же strict text projector, `NativeLeafResolver`, `find_projection`, `realize_projection` и ordinary Executor. Source pane является только presentation: он не содержит второго parser/evaluator.

Кнопка `Find` показывает observational miss/hit без записи. `Realize + execute` явно materialize-ит `integer-add.json` shape, выбирает полученную canonical `RelationEntity` для существующего graph pane и исполняет её тем же Executor, поэтому события попадают в общий `BoundedExecutionTrace`.

## Граница идентичности

`$symbol` никогда не означает «создай сущность с таким именем». Symbol table принадлежит caller-у:

```text
text name -> existing LinkId
```

Unknown symbol является ошибкой projection. Это сохраняет фундаментальную границу AVM:

```text
не найдено != не существует
read/find != realize/write
```
