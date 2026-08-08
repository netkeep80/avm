# Контракт persistent-сессии инспекции

Inspection tooling AVM 1.4 остаётся backend-neutral. Поэтому persistent session — не новый class и не backend adapter: та же typed `InspectionSession` создаётся поверх уже открытого `PersistentLinkStore` и явно переданного persisted `BootstrapVocabulary`.

## Граница reopen

Caller/tool владеет persistent store и identities, необходимыми для работы:

```text
persistent file
  -> PersistentLinkStore open / validate / rebuild indexes
  -> explicit BootstrapVocabulary LinkIds
  -> explicit program/root/function LinkIds
  -> InspectionSession(store, vocabulary)
  -> существующие read/query/execute/trace APIs
```

`InspectionSession` не знает filesystem path и не открывает, repair-ит или reinterpret-ит backend. Corruption/version errors остаются ответственностью `PersistentLinkStore` и возникают до construction session.

## Идентичность

В одном logical persistent store `LinkId` сохраняются точно через close/reopen. Tooling переиспользует caller-supplied vocabulary/root/handle IDs без преобразования.

Contract не объявляет numeric `LinkId` portable между independent stores. Для их сравнения продолжает действовать AVM 1.3 rule equivalence modulo bijective renaming.

## Никакого hidden bootstrap

Создание inspection session над полным explicit vocabulary не должно materialize-ить replacement bootstrap identities. Read-only session после open/inspect/destroy оставляет `LinkStore::size()` неизменным.

Session не хранит скрытую symbolic database выбранных roots/handles. File path, selected root, command history и trace capacity являются host tooling configuration, пока для них не спроектирована отдельная explicit persistence projection.

## Сошедшееся execution state

Function calls могут materialize-ить canonical binding/frame links как часть обычного runtime. Поэтому exact persistent trace equality проверяется после convergence этого link-native state.

Conformance sequence:

1. создать persistent store и bootstrap vocabulary;
2. materialize-ить function/program и один раз выполнить function для convergence call state;
3. сохранить полный bounded call trace и final store size;
4. закрыть store;
5. reopen с теми же vocabulary/root/handle `LinkId`;
6. создать `InspectionSession` и доказать, что construction/read operations не увеличивают store;
7. выполнить уже materialized root без reparsing frontend data;
8. trace-нуть converged function call и потребовать точное равенство `ExecutionEvent`;
9. повторить reopen и потребовать те же identities, trace и store size.

## Гарантии инспекции после reopen

Сохраняются наблюдения:

- `LinkId -> (begin,end)`;
- exact pair lookup;
- Relations Model decode и constrained query results;
- function definition identity;
- structural decode call frame после execution;
- deterministic execution result;
- bounded trace events/failure phases.

`reset_trace()` меняет только host tooling state и никогда persistent store.

## Явные ограничения

Persistent inspection не добавляет:

- hidden bootstrap или identity migration;
- shell sidecar database;
- implicit persistence trace/history/bookmarks;
- backend-specific VM semantics;
- repair/recovery logic поверх `PersistentLinkStore`;
- cross-store numeric `LinkId` equivalence;
- frontend reparsing как условие execution после reopen.
