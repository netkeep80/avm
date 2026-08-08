# Анализ проекта AVM

## Текущее архитектурное состояние

AVM 1.0 больше не является JSON-интерпретатором вокруг pointer-based `rel_t`. После Architecture Foundation 2.0 ядро строится вокруг одного канонического пространства ссылок и исполняет программы по `LinkId`.

```text
external representation
  -> projection / protocol adapter
  -> canonical LinkStore
  -> Relations Model codec
  -> LinkId program
  -> BootstrapRuntime / Executor
  -> result LinkId
```

Это единственный целевой semantic path. Удалённый `rel_t` runtime, глобальный словарь и старый `eval()`/`interpret()` путь не являются совместимым вторым ядром и не должны возвращаться.

## Слои

### Link model и LinkStore

Физический примитив — направленная диада `LinkId -> (begin,end)`. `LinkId` непрозрачен для семантического кода. `LinkStore` разделяет наблюдение и материализацию: `find()`/`get()`/queries не создают ссылки, а `intern()` явно возвращает каноническую идентичность пары.

`InMemoryLinkStore` служит reference implementation. Persistent backends обязаны сохранять те же наблюдаемые свойства и проверяются conformance-тестами.

### Relations Model

Исполнимая сущность кодируется единственным порядком вложения:

```text
(relation, subject, object) = (relation, (subject, object))
```

Storage layer не знает значения relation/subject/object; это ответственность Relations Model codec и верхних слоёв.

### Execution

`BootstrapRuntime` формирует явную bootstrap vocabulary identity. `Executor` принимает entity `LinkId`, декодирует Relations Model entity, создаёт явный execution context и dispatch-ит по relation `LinkId`.

Пользовательские структуры программы, bindings и call frames представлены ассоциативными структурами, а не отдельной JSON AST или скрытым C++ окружением.

### Projection boundary

JSON, Anum и другие форматы находятся вне execution kernel. Внешний adapter владеет grammar, parsing, validation и context resolution, после чего предоставляет structural projection над существующими anchors.

Ключевая граница:

```text
raw(A) != den(A)
load(A) does not imply den(A)
find(A) is observational
realize(A) explicitly materializes denotation
```

JSON value codec и JSON program/session adapter — boundary-компоненты. Они не определяют семантику VM.

## Что уже является сильной стороной

1. **Один physical/semantic core.** Нет двух конкурирующих identity universes.
2. **Каноническая идентичность ссылок.** Семантика не зависит от адресов C++ объектов или layout backend-а.
3. **Наблюдение отделено от записи.** Read/query API не материализует отсутствующие связи.
4. **Явная архитектурная граница проекций.** JSON и будущий Anum adapter не проникают в executor.
5. **Backend replaceability.** In-memory и persistent реализации разделяют один контракт.
6. **Миграционная дисциплина.** Legacy-код удаляется после миграции consumers; Git хранит историю.
7. **CI как архитектурный gate.** Кроссплатформенные сборки, warnings-as-errors, sanitizers, quality guards и benchmark regression checks защищают фундамент.

## Оставшиеся риски AVM 1.0

### 1. Persistent production backend

Persistent contract существует отдельно от семантики VM, но production-grade backend и его crash/reopen guarantees ещё должны пройти полный conformance и end-to-end цикл.

### 2. Bootstrap/native boundary

Native handlers допустимы как bootstrap механизм, однако registry не должен становиться вторым хранилищем программы. Новая функциональность обязана расширять link-native representation, а не обходить её.

### 3. Protocol integrations

Конкретные Anum/MTS, JSON и другие adapters должны оставаться тонкими projection layers. Особая опасность — незаметно вернуть string-dispatch или external AST как второй execution path.

### 4. Performance

Производительность должна оцениваться на link-native вертикальном срезе и backend operations. Оптимизации допустимы только при сохранении identity/query semantics и должны проходить benchmark gates.

## Что больше не является актуальной проблемой

Следующие утверждения относились к старой реализации и удалены из текущей оценки:

- `src/main.cpp` как монолитное semantic core;
- singleton/global `rel_t` storage как архитектура AVM;
- pointer identity как представление сущностей;
- `eval()`/relative addressing как вычислительное ядро;
- JSON expression interpreter как VM execution model;
- LinksPlatform как обязательная зависимость для самой семантики VM;
- hard-coded количество тестов как показатель зрелости.

## Приоритет

До AVM 1.0 приоритет имеют только foundation gates: единый storage/execution path, persistence conformance, protocol boundaries, end-to-end vertical slice, performance/quality regressions и согласованная документация. GUI, широкая stdlib, дополнительные frontends и packaging идут после них.

## Основная документация

- `docs/architecture.md` — архитектурный контракт AVM 1.0;
- `docs/execution-kernel.md` — execution kernel;
- `docs/protocol-adapter-contract.md` — внешний protocol boundary;
- `docs/persistent-link-store.md` — persistent backend contract;
- `plan.md` — dependency-ordered roadmap.

## Вывод

AVM перешёл от исторического JSON/`rel_t` прототипа к более строгому link-native фундаменту. Главный критерий дальнейшей разработки — не количество новых features, а сохранение одного канонического пути `projection -> LinkStore -> Relations Model -> Executor` при расширении persistence, adapters и языка программ.
