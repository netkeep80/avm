# AVM performance baseline

The AVM benchmark is an observation tool, not a correctness oracle.

## Local run

```bash
cmake -S benchmark -B build-benchmark -DCMAKE_BUILD_TYPE=Release
cmake --build build-benchmark --parallel
./build-benchmark/avm_benchmark
```

The executable writes tab-separated data:

```text
name    operations    elapsed_ns    ns_per_op
```

Current measurements cover:

- creating a new canonical pair with `intern`;
- reusing an existing pair with `intern`;
- exact `find` hit and miss;
- outgoing and incoming index queries;
- Relations Model encode/decode;
- execution of a minimal bootstrap relation;
- persistent snapshot reopen/index rebuild.

## CI policy

`.github/workflows/benchmark.yml` builds and runs the benchmark on Ubuntu and uploads `avm-benchmark.tsv` as a workflow artifact.

CI validates only that:

1. the benchmark builds with C++20 warnings-as-errors;
2. the executable completes;
3. every expected measurement is present in the TSV output.

There is intentionally no hard nanosecond threshold on GitHub-hosted shared runners. Absolute timing there is too noisy to be a reliable merge veto.

Performance comparisons should be made on the same machine and toolchain. The TSV artifact provides a historical observation trail for main/tag runs. If a future regression needs an automated veto, prefer a stable structural/complexity invariant or a dedicated controlled runner before introducing a wall-clock threshold.

## Scope

The benchmark does not claim that the current in-memory or persistent reference backend is production-optimal. Its purpose is to make algorithmic regressions visible while AVM 1.0 architecture is still being stabilized.
