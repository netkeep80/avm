# Capability boundary для host effects

Issue: #129. Первый vertical slice опирается на `REF-LAZY-DB-001` из frozen jsonRVM semantic inventory.

AVM сохраняет один обычный `Executor`. Внешний effect не получает отдельный executor/runtime и не становится скрытым поведением pure reference resolver.

Нормативный путь первого slice:

```text
canonical lookup relation entity
  -> ordinary Executor
  -> registered effect handler
  -> explicit capability policy
  -> explicit ExternalEntityProvider
  -> existing LinkId OR deterministic failure
```

## Почему первым выбран external entity lookup

`compat/jsonrvm-semantics.json` классифицирует `REF-LAZY-DB-001` как `effect-adapter`: старый runtime мог при неудачном named-reference lookup обратиться к `database_api`. Для AVM это поведение нельзя оставлять внутри pure resolve/find.

Первый slice намеренно не переносит database protocol. Он фиксирует меньший contract: host может предоставить deterministic provider, который по canonical Text request возвращает уже существующий `LinkId` либо miss.

Это отделяет три действия:

```text
pure resolve/find
!=
external lookup
!=
explicit AVM realization
```

Provider первого slice вообще не имеет API для `intern/create_point`. Если он возвращает identity, которой нет в текущем `LinkStore`, handler отклоняет результат. Поэтому внешнее чтение не может автоматически materialize-ить graph.

## Модель authority

Host/session обязан явно предоставить четыре независимые части:

- canonical relation identity конкретного effect;
- отдельную capability identity;
- `EffectCapabilityPolicy` с allow/deny;
- `ExternalEntityProvider`.

Program structure сама по себе authority не выдаёт. Наличие relation entity в graph не означает разрешение host lookup.

Capability policy хранится вне semantic graph как execution/session policy. Relation и capability identities должны уже существовать в выбранном store и передаются caller-ом; core не создаёт глобальный effect universe и не выводит identity из строковых имён.

## Контракт request/result

Request первого slice — canonical Text в object роли relation entity:

```text
(effect_lookup_relation, unit, Text(name))
```

Handler декодирует только существующий canonical Text. Provider получает bytes как host string view для lookup и может вернуть:

- existing `LinkId` — success;
- отсутствие значения — deterministic `EffectLookupMiss`;
- identity вне текущего store — deterministic rejection.

`EffectCapabilityDenied` возникает до вызова provider. Отсутствующий provider при разрешённой capability даёт `EffectProviderUnavailable`.

Диагностические тексты исключений не являются semantic identity или compatibility contract.

## Наблюдаемость effect

Отдельный effect observer не добавляется. Existing `ExecutionObserver` уже даёт достаточный deterministic boundary:

```text
Enter(effect relation)   = request
Return(effect relation)  = success
Fail(effect relation, Handler) = denied/provider/miss failure
```

Canonical relation identity в event позволяет отличить effect от других executions. Observer остаётся read-only и не управляет execution.

## Pure execution без provider

Pure AVM программы не требуют capability policy или provider. Effect binding добавляется только явным вызовом `register_external_entity_lookup_effect` к существующему `Executor`.

## Что намеренно не входит в первый slice

- реальная database connection;
- filesystem paths;
- HTTP/network;
- clock/sleep;
- DLL/plugin loading;
- implicit lazy lookup внутри `resolve_reference`;
- automatic projection/realization external payload;
- global provider singleton;
- второй `EffectExecutor`.

Следующий host effect должен добавляться только как отдельный evidence-driven slice поверх того же authority model либо обосновать расширение контракта.
