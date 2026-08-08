# Эталонный persistent LinkStore

`PersistentLinkStore` — reference backend для проверки persistence-семантики контракта `LinkStore`. Его цель — доказать стабильную identity связей и детерминированное поведение после reopen. Это намеренно простой snapshot backend, а не production storage engine.

## Контракт

Backend реализует тот же semantic interface, что и `InMemoryLinkStore`:

```text
create_point
intern(begin, end)
find(begin, end)
get(id)
outgoing(begin)
incoming(end)
contains(id)
size()
```

`Executor`, Relations Model codec и projection layer зависят только от `LinkStore` и не знают, находится store в памяти или сохраняется на диск.

## Идентичность и reopen

`LinkId` начинаются с `1` и сохраняются явно. Records упорядочены по `LinkId`; format v1 требует непрерывного namespace.

После reopen:

- каждый сохранённый `LinkId` обозначает ту же пару `(begin,end)`;
- exact pair identity восстанавливается из records;
- `outgoing` и `incoming` indexes перестраиваются детерминированно;
- `intern(a,b)` возвращает тот же canonical `LinkId`;
- `find` и другие read operations не переписывают snapshot.

Point остаётся обычной self-link `(id,id)`.

## Формат snapshot v1

Все integers — unsigned little-endian:

```text
8 bytes   magic = "AVMLNK1\0"
u32       version = 1
u32       reserved = 0
u64       record_count
repeat record_count times:
    u64   LinkId
    u64   begin LinkId
    u64   end LinkId
```

Loader отклоняет:

- неверный или truncated magic;
- неподдерживаемую version или non-zero `reserved`;
- truncated integers/records;
- non-contiguous или duplicate `LinkId`;
- duplicate canonical `(begin,end)`;
- endpoints, которых нет в завершённом snapshot;
- невозможные partial self-references;
- trailing bytes после объявленных records.

Эта строгость не позволяет молча принять повреждённый файл как другую асеть.

## Долговечность мутаций и faulted state

В v1 каждая новая point или canonical pair полностью переписывает snapshot. Повторное использование существующей пары записи не выполняет.

Новая mutation может завершиться исключением как во время обновления in-memory indexes, так и при open/write/flush snapshot. После выдачи нового `LinkId` AVM рассматривает весь участок:

```text
insert_link + persist
```

как единую guarded mutation region.

Если любой шаг бросает exception, живой объект `PersistentLinkStore` переходит в явное **faulted state** и больше не позволяет воспринимать потенциально частичное или uncommitted in-memory состояние как корректный store.

После fault:

- `faulted()` возвращает `true`, не обращаясь к backend;
- `path()` остаётся доступен для diagnostics;
- все `LinkStore` reads и mutations отклоняются;
- исходное exception операции продолжает распространяться;
- объект не пытается скрыто retry или repair состояние.

`intern(a,b)` для уже существующей пары является read-like: он возвращает canonical `LinkId` без snapshot write. Недоступный output path сам по себе не fault-ит здоровый store, пока не потребуется новая mutation.

Корректная recovery boundary — отбросить faulted object и явно reopen/repair backing store согласно policy caller. Если failed direct snapshot write повредил файл, reopen может отклонить его обычными corruption checks.

### Exception safety не равна crash consistency

Faulted-state contract описывает **in-process exception safety**. Он не является обещанием crash-atomic durability.

Format v1 по-прежнему не имеет:

- WAL;
- fsync protocol;
- atomic snapshot swap;
- locking;
- concurrent-writer protocol.

Process crash или power loss во время прямой перезаписи может оставить файл неполным. Эти свойства должны быть отдельным контрактом production backend, например будущей PMM integration, и не должны менять semantics `Executor`.

## Зачем нужен этот backend

AVM требуется конкретное доказательство, что persistence не проникает в семантику VM:

```text
один LinkStore contract
        |
        +-- InMemoryLinkStore
        |
        +-- PersistentLinkStore -- close/reopen --> те же LinkId и links
```

Будущий PMM, LinksPlatform или иной adapter должен проходить тот же backend-neutral conformance suite, а не изменять поведение Executor.
