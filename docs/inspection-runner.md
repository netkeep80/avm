# `avm-inspect`: скриптовый inspection runner

Issue: #117. Parent: #106.

`avm-inspect` — тонкая process-I/O оболочка над уже существующим typed inspection tooling AVM. Она не определяет новый язык исполнения и не создаёт второй runtime.

Нормативный путь:

```text
stdin / UTF-8 script file
  -> построчное framing
  -> parse_inspection_command
  -> typed InspectionCommand
  -> execute_inspection_command(InspectionSession, command)
  -> typed InspectionResult
  -> render_inspection_result
  -> stdout
```

`InspectionSession` использует обычный `BootstrapRuntime`, обычный `Executor` и существующий `BoundedExecutionTrace`. Строковое распознавание команд заканчивается в tooling parser и не становится relation dispatch внутри VM.

## Запуск

Из файла:

```text
avm-inspect commands.avm
```

Из стандартного ввода:

```text
avm-inspect
```

Первый runner намеренно не является интерактивным REPL. Один и тот же line loop подходит для детерминированного batch-исполнения и дальнейшего UI/REPL wrapper, если такой интерфейс действительно понадобится.

## Формат скрипта

- одна typed inspection-команда на строку;
- пустые строки игнорируются;
- строка, у которой первый непробельный символ `#`, является комментарием;
- успешные результаты выводятся в порядке команд;
- первая parse/runtime ошибка печатает номер исходной строки в `stderr` и прекращает обработку следующих команд;
- команда не записывается в `LinkStore` как history или semantic state.

Поддерживаемый набор команд определяется только `parse_inspection_command`; runner его не копирует и не расширяет собственной строковой диспетчеризацией.

## Коды завершения процесса

```text
0  весь скрипт выполнен успешно
1  parse/runtime/input failure внутри скрипта
2  ошибка process-level usage или открытия файла
```

## Политика backend

Первая версия `avm-inspect` запускается только с новым in-memory store и явным созданием bootstrap vocabulary для этой сессии. Это self-contained tooling mode для скриптов и CI, а не способ открыть произвольную persistent database.

Persistent CLI mode пока намеренно отсутствует. Если он понадобится, runner должен принимать explicit persisted vocabulary/root identities либо отдельный versioned manifest. Он не имеет права после открытия произвольного store угадывать или незаметно создавать заменяющие bootstrap identities.

Numeric `LinkId` в командах является непрозрачным store-local идентификатором. Runner не добавляет sidecar symbolic registry и не превращает строковые имена в скрытые semantic identities.

## Граница чтения и записи

Inspection-команды `link`, `find`, `outgoing`, `incoming`, `relation`, `query`, `function`, `frame` используют существующие observational APIs. Они не materialize-ят отсутствующие links.

`execute` и `trace` явно запускают тот же обычный Executor, который используется вне inspection tooling, поэтому возможные runtime effects определяются canonical program semantics, а не shell-оболочкой.

Runner не предоставляет Native JSON/Anum loader и не интерпретирует их syntax. Если такие команды когда-нибудь понадобятся, они должны вызывать существующие adapters и explicit `find/realize`, а не вводить второй evaluator.
