# Базовые измерения производительности AVM

Benchmark AVM — инструмент наблюдения за производительностью, а не oracle корректности.

## Локальный запуск

```bash
cmake -S benchmark -B build-benchmark -DCMAKE_BUILD_TYPE=Release
cmake --build build-benchmark --parallel
./build-benchmark/avm_benchmark
```

Executable записывает tab-separated данные:

```text
name    operations    elapsed_ns    ns_per_op
```

Текущие измерения покрывают:

- создание новой canonical pair через `intern`;
- повторное использование существующей пары через `intern`;
- exact `find` hit/miss;
- `outgoing`/`incoming` index queries;
- Relations Model encode/decode;
- execution минимальной bootstrap relation;
- persistent snapshot reopen/index rebuild;
- масштабирование constrained Relations queries там, где соответствующий benchmark добавлен.

## Политика CI

`.github/workflows/benchmark.yml` собирает и запускает benchmark на Ubuntu и публикует `avm-benchmark.tsv` как workflow artifact.

CI проверяет только:

1. benchmark собирается с C++20 и warnings-as-errors;
2. executable успешно завершается;
3. в TSV присутствуют ожидаемые measurements и корректная schema.

Жёсткого nanosecond threshold на GitHub-hosted shared runners намеренно нет: absolute timings слишком шумны для надёжного merge veto.

Performance comparisons следует выполнять на одной машине и toolchain. TSV artifact даёт исторический trail наблюдений для main/tag runs.

Если будущая regression требует автоматического veto, предпочтительнее сначала зафиксировать стабильный structural/complexity invariant или использовать dedicated controlled runner, а уже затем вводить wall-clock threshold.

## Область утверждений

Benchmark не доказывает, что текущие `InMemoryLinkStore` или `PersistentLinkStore` production-optimal. Его задача — делать algorithmic regressions заметными, не смешивая performance measurement с semantic correctness.
