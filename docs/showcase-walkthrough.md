# Детерминированный walkthrough AVM Showcase

Issues: #158, #160. Parent: #154.

AVM Showcase — опциональный Dear ImGui consumer существующего link-native runtime. Он не входит в default package и не определяет отдельную семантику.

## Что доказывает calculator-basic

Команда:

```text
avm_showcase --demo calculator-basic
```

запускает последовательность:

```text
7 -> + -> 3 -> =
```

через **те же** `CalculatorViewport::press_digit` / `press_operation` и `execute_event`, которые вызываются интерактивными кнопками.

В demo нет отдельной арифметики. Каждый шаг строит обычную calculator `RelationEntity`, исполняет её через тот же `BootstrapRuntime::Executor` и принимает только валидный `ExecutionOutcome`.

После последнего события walkthrough дополнительно observationally декодирует canonical `CalculatorState` и требует:

```text
display = Integer(10)
```

Если любой event или итоговая проверка завершается ошибкой, executable возвращает ненулевой код. Screenshot workflow не может скрыть semantic failure картинкой.

## Канонический путь события

Для каждой кнопки и demo-step используется один путь:

```text
user/demo action
  -> calculator event relation
  -> RelationEntity(relation, current_state, input)
  -> ordinary Executor + current SemanticContextView
  -> ExecutionOutcome
  -> next canonical CalculatorState
  -> observational decode/render
```

GUI хранит только текущий `LinkId` состояния и `SemanticContextView`, необходимые event loop. Accumulator, pending operation и флаг ввода остаются полями canonical `CalculatorState`; host-side calculator state machine отсутствует.

Арифметика `+ - * /` выполняется существующими canonical Integer relations. `CalculatorViewport` не вычисляет результат операторами C++.

## Что видно после calculator-basic

После `7 + 3 =` UI должен показывать canonical display `10`.

`selected_entity` указывает на финальное событие `=`. Центральный relation graph декодирует ту же сущность:

```text
relation = press_equals
subject  = input CalculatorState
object   = bootstrap unit
```

То есть `(rel,sub,obj)` на экране — не GUI-модель и не копия состояния, а фактическая `RelationEntity`, переданная Executor.

## Controller relation и semantic $rel

В `ExecutionContext` существуют разные понятия:

```text
context.relation
```

— controller relation текущей исполняемой сущности;

```text
context.semantic.relation_state
```

— semantic `$rel`, то есть явное relation-state текущего semantic context.

Для calculator event входной semantic `$rel` равен `event.subject`, то есть canonical состоянию **до** события. Успешный calculator handler возвращает:

```text
ExecutionOutcome {
  result   = next_state,
  semantic = input.semantic.with_relation_state(next_state)
}
```

Поэтому result и выходной semantic `$rel` совпадают с новым canonical state, но controller relation остаётся relation самого события (`press_digit`, `press_add`, `press_equals` и т. п.).

Pure Integer relation внутри calculator handler state-neutral и не делает скрытого `$rel := result`.

## Graph и trace

Relation graph и trace — observational consumers того же runtime.

- graph читает `selected_entity` и существующие links;
- `BoundedExecutionTrace` подключён observer-ом к тому же Executor;
- calculator event и вложенные Integer calls попадают в trace автоматически;
- UI inspection не materialize-ит дополнительные semantic links.

После calculator-basic trace содержит реальные `Enter/Return` события sequence `7`, `+`, `3`, `=`, включая вложенное исполнение Integer relation при вычислении результата.

## Сборка Windows

Из корня репозитория можно использовать:

```bat
build_showcase.bat
```

Либо CMake напрямую:

```text
cmake -S . -B build-showcase -DAVM_BUILD_IMGUI_DEMO=ON
cmake --build build-showcase --config Release --target avm_showcase --parallel
```

После сборки запустите найденный `avm_showcase.exe`:

```text
avm_showcase.exe --demo calculator-basic
```

Без `--demo` запускается тот же интерактивный showcase.

## Сборка Linux

Нужны development packages X11/OpenGL. Для Ubuntu focused CI использует `xorg-dev` и `libgl1-mesa-dev`.

```text
cmake -S . -B build-showcase \
  -DCMAKE_BUILD_TYPE=Release \
  -DAVM_BUILD_IMGUI_DEMO=ON \
  -DAVM_BUILD_CLI=OFF \
  -DAVM_BUILD_CORE_TESTS=OFF \
  -DAVM_BUILD_JSON_COMPAT_TESTS=OFF \
  -DAVM_BUILD_ANUM_ADAPTER_TESTS=OFF
cmake --build build-showcase --target avm_showcase --parallel
./build-showcase/examples/showcase/avm_showcase --demo calculator-basic
```

Фактический путь binary зависит от CMake generator; focused workflow находит executable после сборки.

## Реальный CI screenshot

Focused workflow `.github/workflows/showcase.yml` на Linux:

1. собирает текущий `avm_showcase`;
2. запускает именно этот executable под Xvfb;
3. включает Mesa software OpenGL через `LIBGL_ALWAYS_SOFTWARE=1`;
4. передаёт `--demo calculator-basic`;
5. через CI-only `AVM_SHOWCASE_SCREENSHOT_PPM` просит приложение сохранить кадр после двух обычных ImGui render cycles;
6. приложение читает `GL_BACK` через `glReadPixels`, то есть тот же OpenGL framebuffer, который реально показывается окном;
7. workflow конвертирует raw PPM в PNG, проверяет размер и число цветов и публикует artifact:

```text
avm-showcase-calculator-basic
```

Artifact содержит:

```text
avm-showcase-calculator-basic.png
calculator-basic.log
```

Это provenance реального executable и реально отрисованного framebuffer. Mockup, ручная перерисовка, X11 root-pixmap или screenshot старого PR не считаются evidence текущего main.

Семантические действия не воспроизводятся координатными кликами: их выполняет deterministic demo через тот же canonical event path. Framebuffer capture только читает уже отрисованный кадр и не меняет AVM semantic state.

## Граница поддержки

Showcase остаётся опциональным:

```text
AVM_BUILD_IMGUI_DEMO=OFF
```

по умолчанию. Установленный `avm::core` не получает зависимости на Dear ImGui, GLFW, X11 или OpenGL.

Поддерживаемый calculator slice сейчас целочисленный. Decimal, percent и sign-toggle не симулируются host-side до появления соответствующей canonical semantics.
