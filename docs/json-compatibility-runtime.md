# JSON compatibility как слой проекции

AVM не исполняет JSON AST. JSON поддерживается как внешний compatibility syntax, но семантический путь единственный:

```text
JSON
  -> JsonProgramImporter
  -> program graph в LinkStore
  -> BootstrapRuntime / Executor
  -> result LinkId
  -> JSON result projection
```

После того как `import_program()` вернул root `LinkId`, execution больше не зависит от исходного JSON object. Conformance tests уничтожают/заменяют исходное JSON value до запуска imported graph и тем самым проверяют эту границу.

## Текстовые имена заканчиваются в importer

Operator names (`Not`, `And`, `Or`, `If`, `Def`, `Call`), function names и formal parameter names являются projection syntax. `JsonProgramImporter` разрешает их в opaque `LinkId`.

Runtime получает:

- relation `LinkId` для dispatch;
- function-handle `LinkId`;
- formal-parameter `LinkId`;
- link-native call frames и bindings;
- expression payloads как canonical dyads.

В production execution нет `resolve_operator`, JSON function-body map или parameter-name stack.

## Function symbols и deferred Def

Importer может выделить function handle до исполнения definition. Это поддерживает recursion и forward references внутри projected graph.

JSON `Def` превращается в deferred definition expression из `deferred-definitions.md`. Import не создаёт callable function-definition row; materialization происходит только при execution node `Def`, что сохраняет порядок `Def`/`Call`.

Синтаксическое redefinition получает новый handle, чтобы последующие projected calls могли ссылаться на новое immutable definition без mutation старой строки.

## JSON literals и значения

Boolean/null в program projection отображаются в bootstrap vocabulary identities. Данные JSON кодируются отдельным `JsonValueCodec`.

Важно: JSON является frontend/value codec, а не окончательной primitive-value model AVM. AVM 1.5 #128 определяет canonical value denotations независимо от quirks `nlohmann::json`.

## Compatibility sequence

Core `sequence_relation` является fail-fast: exception child expression прерывает исполнение.

Если JSON-facing compatibility contract требует иного поведения, эта policy должна находиться на adapter boundary и использовать уже projected child expressions через тот же `Executor`, а не ослаблять core semantics.

Общее правило:

```text
frontend compatibility policy != invariant ядра VM
```

## Граница ошибок

`JsonProgramImporter` бросает `JsonProjectionError` для malformed syntax. Нижние уровни AVM используют явные runtime/logic errors для malformed link structures или execution failures.

Convenience facade `JsonCompatibilitySession::interpret()` остаётся только тонкой формой:

```text
JSON -> projection -> LinkId -> canonical execute -> result projection
```

Он не содержит recursive JSON evaluator.

## Исторический semantic interpreter удалён

Старый production path с:

```text
resolve_operator
func_env
param_stack
recursive interpret(json)
rel_t storage/identity universe
legacy_json_compat
```

удалён после миграции consumers и conformance. Git хранит историю; рабочее дерево не сохраняет dead compatibility implementation.

CI содержит явные regression guards против возврата этих identifiers в production/build sources.

Поэтому текущее утверждение о совместимости относится к **внешнему JSON формату и наблюдаемому поведению поддерживаемого subset**, а не к сохранению старой C++ implementation.

## Постоянное доказательство миграции

После удаления side-by-side old/new interpreter harness остаются постоянные suites:

- core link-native tests;
- `json_projection_tests`;
- `json_session_tests`;
- `json_value_codec_tests`;
- CLI JSON roundtrip/conformance;
- warnings-as-errors и architecture guards;
- ASan/UBSan;
- portable Linux/Windows/macOS matrix.

AVM 1.5 добавляет поверх этого versioned `jsonRVM` semantic inventory и differential golden corpus, чтобы дальнейшая миграция проверяла именно Relations Model semantics, а не возвращала старую реализацию.
