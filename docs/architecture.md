# Архитектурный контракт AVM

Этот документ задаёт инженерные границы ядра AVM. Он намеренно не определяет всю Метатеорию Связей (МТС): его задача — определить, как AVM хранит и исполняет ассоциативные структуры.

## Уровни

```text
A0  Модель связи       LinkId -> (begin, end)
A1  LinkStore          каноническая идентичность, запросы и явные записи
A2  Модель Отношений   (relation, subject, object) <-> вложенные дуплеты
A3  Исполнение         контекст + dispatch отношения + вычисление программы
A4  Проекция           JSON, Anum и другие внешние представления
A5  Backend            in-memory, persistent adapter или другое хранилище
```

Верхний уровень не должен скрыто переопределять семантику нижнего.

## A0. Модель связи

Физический примитив ядра AVM — направленный дуплет:

```text
LinkId -> (begin: LinkId, end: LinkId)
```

`LinkId` — непрозрачная идентичность. Семантический код не должен выводить смысл из значения C++ pointer или физического layout backend.

Независимая идентичность bootstrap-ится как self-link `(x, x)`. Благодаря этому эталонная in-memory реализация остаётся полностью link-only и не вводит второй физический тип записи «атом».

## A1. Хранилище LinkStore

Канонический контракт хранения разделяет чтение и запись.

| Операция | Может изменять store | Смысл |
|---|---:|---|
| `create_point()` | да | Создать новую независимую self-link identity |
| `intern(begin,end)` | да | Вернуть каноническую связь пары, создав её при отсутствии |
| `find(begin,end)` | нет | Найти уже существующую точную пару |
| `get(id)` | нет | Прочитать полюса связи |
| `outgoing(begin)` | нет | Получить связи с указанным begin |
| `incoming(end)` | нет | Получить связи с указанным end |

Ключевой инвариант:

```text
find(a, b) никогда не создаёт Link(a, b)
```

Канонизация означает:

```text
intern(a, b) == intern(a, b)
```

в пределах одного логического store.

## A2. Совместимость с Моделью Отношений

Триединая сущность имеет форму:

```text
(relation, subject, object)
```

AVM использует единственный канонический порядок вложения:

```text
subject_object = Link(subject, object)
entity         = Link(relation, subject_object)
```

следовательно:

```text
(relation, subject, object)
= (relation, (subject, object))
```

За это отображение отвечает Relations Model codec. Сам `LinkStore` не знает, какие связи играют роли relation, subject или object.

Важно различать **представление** и **семантику исполнения**. Lossless encoding триплета через два дуплета не определяет автоматически, что означает исполнение всех трёх ролей. Этот более сильный контракт развивается в AVM 1.5.

## A3. Исполнение

`Executor` принимает root/entity `LinkId`, а не JSON AST. Он декодирует сущность Модели Отношений, формирует явный execution context и выполняет dispatch по identity отношения.

Native C++ handlers допускаются как bootstrap vocabulary, но ключом является relation `LinkId` в ассоциативном store. Program structures, bindings и call frames принадлежат ассоциативному представлению; native registry не должен становиться второй базой программ.

Исторический pointer-based `rel_t` storage/runtime и JSON-centric `eval()`/`interpret()` semantic path удалены после миграции consumers. Они доступны только в Git history и не являются compatibility API AVM.

### Частный и общий случаи исполнения

Первый bootstrap program model использует удобную форму:

```text
(relation, unit, payload)
```

Это допустимый частный случай, но не определение всей Модели Отношений. AVM 1.5 расширяет контракт до meaningful execution произвольной сущности:

```text
(relation, subject, object)
```

без требования `subject == unit`.

## A4. Проекция

JSON и Anum являются внешними представлениями:

```text
внешнее представление
-> parser / codec / projection
-> LinkStore + Relations Model
-> execution
```

`Executor` не должен зависеть от `nlohmann::json` как внутреннего instruction/value type.

Для Anum AVM следует разделению L3/L4:

```text
raw(A) != den(A)
load(A) не означает материализацию den(A)
find(A) не изменяет store
realize(A) является явной materializing operation
```

Anum parser, grammar, quotation/context semantics находятся вне storage layer. Канонический источник этих правил — `netkeep80/anum_docs`.

## A5. Физический backend

Физические backends реализуют один и тот же `LinkStore` contract. Они не определяют VM relations, JSON rules, Anum semantics или правила исполнения.

`InMemoryLinkStore` — эталонный backend. `PersistentLinkStore` — reference persistence implementation, доказывающая reopen/identity semantics.

Production-grade crash consistency, WAL, locking, concurrent writers и другие физические свойства должны развиваться как backend contracts, не изменяя semantic kernel.

## Основные инварианты

1. Один физический примитив ядра — направленный дуплет.
2. `LinkId` является непрозрачной identity.
3. Точная пара имеет каноническую identity в одном logical store.
4. Read/query operations не материализуют missing links.
5. Триплет Модели Отношений имеет единственный порядок `(relation, (subject, object))`.
6. JSON является проекцией, а не внутренним хранилищем инструкций VM.
7. Исполнение принимает identities связей.
8. Program structures и call state принадлежат ассоциативной модели.
9. Backend implementation не определяет VM semantics.
10. External protocol parsing не находится внутри semantic core.
11. Observation/inspection не создают второй executor.
12. Legacy paths удаляются после миграции; историю хранит Git.
13. Эффекты и materialization должны быть явными.
14. Полная triune semantics не сводится к `subject = unit`.

## Эволюция архитектуры

Фундамент AVM 1.0 строился в порядке:

```text
архитектурный контракт
-> LinkStore
-> Relations Model codec
-> execution kernel
-> program-as-links
-> external protocol boundary
-> persistence / vertical slice / performance hardening
-> release readiness
```

Следующие версии сохраняют тот же фундамент:

```text
AVM 1.1  read-only Relations queries
AVM 1.2  structural standard library
AVM 1.3  execution observability
AVM 1.4  inspection tooling
AVM 1.5  semantic migration from jsonRVM
```

Актуальный dependency-ordered status поддерживается в `plan.md` и GitHub issues.
