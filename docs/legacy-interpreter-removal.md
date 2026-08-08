# Удаление legacy JSON semantic interpreter

В AVM существует один production execution model. Исторический recursive JSON interpreter был удалён после того, как link-native path прошёл side-by-side conformance на Linux, Windows и macOS.

## Что удалено

Старый runtime совмещал несвязанные ответственности и фактически содержал вторую виртуальную машину:

- string-to-operator dispatch через `resolve_operator()`;
- `func_def` с JSON function bodies;
- глобальный `func_env` с именами функций;
- глобальный `param_stack` с bindings параметров;
- recursive traversal JSON nodes в `interpret(const json&)`;
- позднее удалённый pointer-based `rel_t` storage/identity compatibility path.

Эти механизмы больше не существуют в working tree. Историю реализации хранит Git; dead production copy намеренно не сохраняется.

## Текущие границы исходного кода

`src/cli.cpp`
: File I/O, CLI policy и presentation. Text tokens могут распознаваться как frontend syntax, но не выбирают runtime semantics.

`include/avm/json_compat.h`
: JSON program projection в link-native graph и обратная result projection.

`include/avm/json_value_codec.h`
: Представление JSON как данных в canonical `LinkStore`; это не executor.

`BootstrapRuntime` / `Executor`
: Единственный production semantic execution path.

## Почему side-by-side harness удалён

Во время миграции differential harness был полезен, потому что одновременно существовали две semantic implementations. Он сравнивал старый interpreter с путем:

```text
JSON -> LinkStore -> Executor
```

для Boolean operations, control flow, functions, recursion, errors и scalar results.

После зелёного conformance на поддерживаемых OS хранить harness означало бы сохранять obsolete implementation только ради бесконечного сравнения с ней. Поэтому old interpreter и временный side-by-side suite были удалены на одной архитектурной границе.

Постоянное покрытие обеспечивают:

- independent core suites;
- JSON projection/session/value-codec tests;
- CLI JSON fixtures;
- strict warnings и formatting gates;
- ASan/UBSan;
- Linux/Windows/macOS portable builds;
- архитектурные grep/quality guards;
- AVM 1.5 semantic inventory и frozen `jsonRVM` corpus для дальнейшей differential migration.

## CI regression guards

Quality job запрещает возврат бывших semantic side-channel identifiers в production sources:

```text
resolve_operator
func_def
param_stack
```

Отдельный guard запрещает возврат удалённого pointer-based storage path:

```text
rel_t
legacy_json_compat
UnitedMemoryLinks
```

Это сильнее документационного обещания: PR, восстанавливающий второй runtime/storage universe, не может пройти quality gate без явного изменения архитектурной политики.

## Правило развития

Compatibility реализуется переносом observable behavior на canonical execution path, а не сохранением старого evaluator как fallback.

После миграции consumers legacy implementation удаляется. Git является механизмом исторической совместимости для археологии кода; production tree содержит только актуальную архитектуру.
