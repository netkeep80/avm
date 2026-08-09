# Денотация значений AVM v1

Родительская задача: #128. Runtime epic: #122. Числовой контракт: #165. Text contract: #180. Semantic outcome contract: #153.

## Статус и назначение

Этот документ фиксирует **минимальный frontend-neutral набор денотаций значений**, достаточный для текущего AVM 1.5 migration corpus и для дальнейшей реализации #174.

Он не вводит новый универсальный runtime type и не утверждает, что AVM уже должен иметь заранее завершённую иерархию всех возможных типов данных.

Нормативный принцип:

```text
external syntax
    ↓
explicit frontend projection
    ↓
canonical link structure / canonical LinkId
    ↓
relation execution
    ↓
canonical LinkId result
```

JSON, Anum, текстовый DSL, GUI и host C++ values не являются authoritative semantic storage.

## Нет глобального `Value`-контейнера

AVM v1 сознательно **не** вводит конструкцию вида:

```text
Value = variant<Integer, Text, Boolean, List, ...>
```

и не хранит рядом с `LinkId` скрытый host payload.

Вместо этого semantic domain определяется сочетанием:

1. link structure;
2. явного vocabulary;
3. domain-specific validator/decoder;
4. relation contract, который ожидает конкретную денотацию.

Следовательно один `LinkId` не обязан нести глобальный runtime type tag, понятный каждому relation handler-у.

Это сохраняет LinkStore единственной authoritative структурной вселенной и не создаёт второй object model рядом с ним.

## Принцип `raw != denotation`

Внешнее представление не тождественно semantic identity.

```text
raw(A) != den(A)
```

Примеры:

- JSON `{"$integer": 42}` — protocol-level запись, а не Integer runtime object;
- JSON `{"$text":"abc"}` — frontend string, который проецируется в canonical byte-string Text;
- будущая Anum-запись числа не определяет автоматически Integer link encoding;
- числовая десятичная строка не является canonical Integer identity.

Разные frontends должны сходиться к одной и той же accepted core denotation, если выражают одно semantic значение соответствующего domain.

## Принятая граница v1

Минимальный value substrate состоит из четырёх уже существующих групп.

### Singleton-идентичности

Bootstrap vocabulary предоставляет независимые canonical identities, включая:

```text
unit
nil
true
false
```

Они не являются host `bool/null` payload.

Их semantic meaning определяется конкретным vocabulary и relation contracts.

Boolean relations AVM уже используют `true`/`false` как link-native operands/results без JSON runtime values.

### Знаковые целые

Canonical Integer определён отдельным `IntegerVocabulary` и структурным binary encoding.

Нормативные API:

```text
find_integer
realize_integer
decode_integer
is_integer
```

Подробная grammar, canonicality и host boundary зафиксированы в `docs/integer-denotation-contract.md`.

Основные свойства:

- zero имеет единственную identity;
- sign задаётся structural wrapper-ом;
- magnitude — canonical bit-chain без leading-zero alternative;
- stored structural representation принципиально не обязано ограничиваться 64 битами;
- текущий C++ arithmetic host-domain — checked `int64_t`;
- `find/decode/is` observational;
- `realize` explicit;
- persistent reopen сохраняет structural meaning.

Arithmetic relations:

```text
integer_add
integer_subtract
integer_multiply
integer_divide
```

получают meaningful direct subject/object operands и возвращают canonical Integer result.

Value computation не получает права скрыто переписывать semantic context только потому, что появился result. Общий contract #153 различает:

```text
result value
semantic state transition
```

Если relation действительно должна изменить `$rel`-совместимый state, это должно быть её явной semantic outcome, а не универсальным следствием вычисления значения.

Accepted division contract задаётся #165: signed C++20 truncation toward zero в текущем host-domain с deterministic failures для division by zero и `INT64_MIN / -1`.

### Текст как последовательность байтов

Canonical Text определяется `TextVocabulary` как конечная последовательность canonical Byte values.

Normative core API оперирует байтами:

```text
span<uint8_t>
vector<uint8_t>
```

а не JSON string и не C string.

Основные свойства:

- exact byte identity;
- embedded NUL допустим;
- empty Text имеет canonical representation;
- `find/decode/is` observational;
- `realize` explicit;
- persistent reopen сохраняет exact bytes;
- Unicode normalization не выполняется core-ом.

UTF-8 является одной из frontend policies, а не частью L4 Text identity.

Поэтому composed и decomposed Unicode byte sequences могут иметь разные Text identities даже при визуально одинаковом отображении.

### Упорядоченные link-list

AVM уже использует canonical ordered link-list для:

- program sequence;
- function arguments/bindings;
- foreach input/output;
- independent execution projection results.

Структура задаётся явным terminator identity:

```text
[item1, item2, ..., itemN]
```

как link-chain, а не host `std::vector` runtime value.

`decode_link_list` является observation над существующей структурой.

`encode_link_list` является explicit realization/materialization operation.

Порядок элементов является semantic и не выводится из unordered host collection iteration.

V1 не объявляет любой link-list «универсальным List type» без vocabulary/context. Это structural ordered collection contract, переиспользуемый теми domains, которые явно его выбирают.

## Значение и структура связи

AVM не разделяет память на физически разные хранилища «program objects», «data objects» и «value objects».

Один и тот же `LinkStore` содержит canonical links.

Но из этого не следует, что любая ссылка автоматически является Integer, Text или executable program.

Semantic interpretation появляется только через explicit contract:

```text
LinkId + expected vocabulary/domain -> validated meaning
```

Поэтому:

- arbitrary point не является Integer;
- arbitrary pair не является Text;
- arbitrary RelationEntity не обязательно executable без зарегистрированной relation semantics;
- frontend syntax не создаёт meaning только формой JSON node.

## Observation и realization

Общий boundary v1:

```text
observe/find/decode/inspect != realize/materialize/write
```

Observation не имеет права создавать отсутствующие links только ради удобства lookup.

Для domain APIs это означает:

- `find_*` возвращает существующую canonical identity или miss;
- `decode_*` проверяет существующую structure;
- `is_*` является observational predicate;
- `realize_*` явно создаёт canonical structure;
- value-producing executable relation может explicit realize result, если это часть её relation contract.

Последний случай не превращает любую read-operation в materializing operation.

## Value result и semantic state — разные сущности

`ExecutionOutcome` разделяет:

```text
result: LinkId
semantic: SemanticContextView
```

Это принципиально для migration jsonRVM.

Старый mutable runtime мог использовать один JSON slot одновременно как:

- текущий relation-state;
- destination;
- вычисленное значение;
- intermediate result.

AVM v1 не наследует такое aliasing.

Pure/value relation может вернуть result при неизменном semantic context.

Stateful relation может вернуть explicit updated immutable semantic view.

Ordered sequence #153/#162 thread-ит state только из explicit child outcome.

Именно semantic migrator #174 отвечает за перевод legacy construct в правильную комбинацию value computation и explicit state transition.

## Pure computation и store materialization

Термин «pure» в value vocabulary относится прежде всего к отсутствию hidden external/contextual semantics:

- операция детерминирована canonical operands;
- не читает JSON/GUI/global mutable registry;
- не обращается к filesystem/network/time;
- не меняет semantic context без явного relation contract.

При этом создание ранее отсутствующей canonical result structure может быть explicit value-realization effect внутри relation.

Например:

```text
Integer(7) + Integer(3) -> realize Integer(10) -> LinkId(10)
```

может увеличить `LinkStore`, если `10` ранее не materialize-ировано.

Store materialization и semantic-context mutation являются разными эффектами и не должны смешиваться.

## Сходимость frontend-ов

Native `duplet-json/1` уже доказывает convergence boundary:

```text
{"$integer": 7}
 -> IntegerVocabulary projection
 -> canonical Integer(7)

{"$text": "abc"}
 -> exact frontend bytes
 -> TextVocabulary projection
 -> canonical Text(bytes)
```

`$symbol` разрешается только через caller-owned explicit anchors; неизвестный symbol не создаёт новую identity автоматически.

`$link` означает explicit existing anchor, а не «число как LinkId value».

Будущий Anum frontend должен соблюдать тот же принцип: parser/projection может иметь другой raw syntax, но accepted denotation должна сходиться к тем же core domains, если semantic contract совпадает.

## Что намеренно не входит в v1

Следующие domains не блокируют текущий #174 сами по себе и не должны появляться speculative implementation-ом:

- floating-point/real-number denotation;
- arbitrary-precision arithmetic backend;
- generic JSON Object/Map как runtime type;
- generic mutable property bag;
- Unicode-normalized Text identity;
- universal error/missing/optional value;
- datetime;
- filesystem/network values;
- host object/function pointers.

Они добавляются только отдельными gates, когда migration corpus или реальный consumer доказывает необходимость.

Особенно важно: отсутствие generic JSON Object value не является дефектом AVM. Historical JSON object semantics раскладывается на explicit structural projection, reference, context, collection и effect contracts.

## Отношение к jsonRVM oracle

`jsonRVM` остаётся frozen oracle observable behavior, а не источником внутренней representation AVM.

Migration equivalence означает:

```text
legacy observable meaning
≈
AVM canonical result/state/effect behavior
```

а не:

```text
тот же nlohmann::json object внутри runtime
```

Поэтому #174 должен для каждого supported construct явно определить:

1. какой raw legacy value встречается;
2. в какой accepted AVM domain он проецируется;
3. какой relation вычисляет result;
4. нужен ли explicit semantic state transition;
5. какой observable result/trace сравнивается с oracle.

## Контракт persistence

Accepted value domains v1 не требуют host-side authoritative registry `LinkId -> payload`.

После reopen того же logical store и восстановления vocabulary:

- canonical Integer decode/find сохраняет meaning;
- canonical Text decode/find сохраняет exact bytes;
- ordered structural lists продолжают декодироваться через те же terminator identities;
- singleton identities остаются теми же persisted LinkIds.

Backend может иметь другую внутреннюю реализацию, но observable LinkStore/domain contracts должны сохраняться.

## Инварианты v1

1. `LinkStore` остаётся единственным authoritative structural universe.
2. Нет глобального host `Value` variant как второй семантики.
3. Raw syntax не определяет denotation автоматически.
4. Domain vocabulary identities явны.
5. Observation не materialize-ит отсутствующие структуры.
6. Realization является явной операцией.
7. Equal canonical domain values имеют одну structural identity в одном store при фиксированном vocabulary.
8. Result value не означает automatic semantic-state transition.
9. JSON/Anum/GUI не входят в core value model.
10. Persistence сохраняет structural meaning без hidden payload table.
11. Новые value domains добавляются consumer/corpus-driven gates, а не speculative completeness.

## Критерий завершения #128

Для AVM 1.5 gate #128 считается завершённым, когда:

- Integer contract и representative arithmetic приняты и проходят conformance;
- Text byte-string contract принят и проходит conformance;
- Boolean singleton/value baseline сохраняется;
- ordered structural collection contract доступен sequence/foreach/projection;
- pure value result отделён от semantic-context transition;
- Native JSON value leaves сходятся к тем же core denotations;
- in-memory/persistent и strict/sanitizer/portable gates зелёные.

Это **минимальный accepted value substrate**, достаточный, чтобы #174 расширял поддержку construct-by-construct вместе с frozen oracle evidence.

Новые domains не переоткрывают #128 автоматически. Для них создаются отдельные issues с собственными semantic и conformance gates.