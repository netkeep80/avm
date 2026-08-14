# Каноническое состояние showcase-калькулятора

Issue: #156. Parent: #154.

Этот документ фиксирует headless domain model калькулятора, который затем должен потребляться GUI #157. UI не определяет арифметику или состояние.

## Граница модели

Calculator model использует только существующие AVM contracts:

```text
LinkStore
RelationEntity
BootstrapVocabulary
IntegerVocabulary
SemanticContextView
ExecutionOutcome
ordinary Executor
```

JSON, ImGui и string-dispatch в модели отсутствуют.

## Структура CalculatorState

Состояние — canonical RelationEntity:

```text
CalculatorState =
  Link(calculator_state,
       Link(display,
            Link(accumulator,
                 Link(pending_operation,
                      entering_new_number))))
```

В терминах `RelationEntity`:

```text
relation = calculator_state
subject  = display
object   = Link(accumulator,
                Link(pending_operation,
                     entering_new_number))
```

Поля используют уже существующие denotations:

```text
display / accumulator = canonical Integer

pending_operation =
  unit
  | integer_add
  | integer_subtract
  | integer_multiply
  | integer_divide

entering_new_number = true | false
```

`unit` означает отсутствие pending operation. Отдельный host enum/string registry не является authority.

## Чтение и materialization

```text
decode_calculator_state
```

только читает существующие links и валидирует Integer/pending/Boolean shape. Он не вызывает `intern` или `create_point`.

```text
realize_calculator_state
```

сначала observationally валидирует все поля и только затем явно materialize-ит canonical structure.

Так сохраняется общая граница AVM:

```text
read / validate != realize / write
```

## Начальное состояние

Детерминированное начальное состояние:

```text
display             = Integer(0)
accumulator         = Integer(0)
pending_operation   = unit
entering_new_number = true
```

Повторный clear сходится к той же canonical state identity при неизменной vocabulary.

## Сущность события

Пользовательское действие кодируется обычной RelationEntity:

```text
(relation = calculator event,
 subject  = current CalculatorState,
 object   = event input)
```

Relations:

```text
press_digit
press_add
press_subtract
press_multiply
press_divide
press_equals
clear
```

`press_digit` принимает canonical Integer `0..9`. Остальные события первого slice принимают `unit`.

## Явный переход semantic state

Calculator event требует explicit `SemanticContextView`, причём:

```text
input.semantic.relation_state == event.subject
```

Успешное событие возвращает:

```text
ExecutionOutcome {
  result   = next CalculatorState,
  semantic = input.semantic.with_relation_state(next_state)
}
```

Остальные semantic roles сохраняются.

Это domain-specific explicit transition. Pure Integer relations остаются state-neutral.

## Делегирование арифметики

Calculator не вызывает host `+ - * /` для вычисления результата.

Pending operation хранит **сам canonical Integer relation LinkId**. Для вычисления модель строит обычную:

```text
RelationEntity(pending_operation, accumulator, display)
```

и исполняет её тем же `Executor` в том же semantic context.

Digit append также выражается через existing Integer relations:

```text
new_display = old_display * 10 + digit
```

Checked overflow/division-by-zero semantics остаются единственными — из `IntegerVocabulary` runtime.

## Семантика операций первого slice

При `press_add/subtract/multiply/divide`:

- если текущее число только что вводилось, предыдущая pending operation сначала исполняется;
- полученный value становится display и accumulator;
- выбранная Integer relation становится новой pending operation;
- `entering_new_number = true`.

Если pending operation отсутствует, текущий display становится accumulator.

`press_equals` исполняет pending operation при наличии правого operand, затем:

```text
display = result
accumulator = result
pending_operation = unit
entering_new_number = true
```

`clear` возвращает canonical initial state.

## Семантика ошибок

Первый slice фиксирует:

- digit вне `0..9` -> deterministic reject;
- division by zero -> существующий canonical Integer failure;
- malformed Integer field -> observational reject;
- invalid pending-operation identity -> observational reject;
- semantic `relation_state`, не совпадающий с event subject -> reject;
- `equals` без введённого RHS при существующей pending operation -> reject.

Если event handler завершается ошибкой, next CalculatorState не публикуется через `ExecutionOutcome.semantic`.

## Persistent reopen

Calculator vocabulary и states — обычные LinkIds.

После `PersistentLinkStore` reopen caller восстанавливает сохранённые:

```text
BootstrapVocabulary
IntegerVocabulary
CalculatorVocabulary
current CalculatorState
SemanticContextFrame
```

затем валидирует vocabularies и регистрирует existing Integer/calculator handlers. JSON/UI reconstruction не требуется.

## Headless-проверки

`showcase_calculator_model_test.cpp` проверяет на in-memory и persistent stores:

```text
7 + 3 = 10
9 - 4 = 5
6 * 7 = 42
8 / 2 = 4
1 + 2 + 3 = 6
clear -> initial state
```

Проверяются промежуточные state fields и `semantic.relation_state`, а не только финальное отображаемое число.

## Запреты

- mutable JSON как CalculatorState;
- hidden host accumulator/pending operation как authority;
- арифметика в ImGui callback;
- string operation registry;
- второй Executor;
- automatic `relation_state := every result`;
- materialization внутри decode/read;
- GUI dependency в headless model.
