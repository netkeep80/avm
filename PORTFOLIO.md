# Portfolio roadmap

`avm` является частью portfolio [`netkeep80`](https://github.com/netkeep80).

Portfolio-level направление, приоритет, lifecycle, cross-repo dependencies и следующий gate **намеренно не дублируются здесь**. Authoritative sources:

- [netkeep80/roadmap](https://github.com/netkeep80/roadmap) — главный portfolio control plane;
- [Current status](https://github.com/netkeep80/roadmap/blob/main/STATUS.md) — live GitHub state;
- [Execution order](https://github.com/netkeep80/roadmap/blob/main/EXECUTION.md) — cross-repo gates;
- [Architecture](https://github.com/netkeep80/roadmap/blob/main/ARCHITECTURE.md) — canonical ownership/dependencies.

AVM issues, code, tests, semantic inventories и release mechanics остаются local implementation source of truth.

```text
roadmap decides portfolio direction;
this repository executes its runtime responsibilities;
GitHub facts feed the central live status.
```

Если local runtime gate меняет cross-repo dependency, обновляется central roadmap, а не создаётся конкурирующая portfolio-карта здесь.
