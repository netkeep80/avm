# Кодек JSON-значений

`JsonValueCodec` — путь roundtrip для случая, когда JSON рассматривается как **данные**, а не как программа AVM.

Он заменяет историческое pointer-based JSON-представление через `rel_t` и хранит каждое значение в том же каноническом пространстве `LinkStore`, которое использует остальная AVM.

## Граница

```text
nlohmann::json
    |
    v
JsonValueCodec::encode
    |
    v
LinkId в LinkStore
    |
    v
JsonValueCodec::decode
    |
    v
nlohmann::json
```

Этот codec не является executor. Program semantics находятся в `JsonProgramImporter` и `BootstrapRuntime` / `Executor`.

## Словарь значений

`JsonValueVocabulary` вводит явные identities для:

- `unit` и list `nil`;
- null / true / false;
- array;
- byte;
- string;
- unsigned integer;
- signed integer;
- floating-point number;
- object;
- object entry.

Vocabulary можно создать для нового store или явно передать при восстановлении уже materialized value graph.

## Представление

Typed values используют сущность Модели Отношений с subject, равным `unit` JSON codec:

```text
(type_relation, unit, payload)
```

Arrays и objects хранят ordered members через canonical link lists. Object entries представлены сущностями:

```text
(entry_relation, key_string, value)
```

Strings — последовательности byte entities. Byte содержит ровно восемь Boolean bit identities. Numeric payload содержит ровно 64 Boolean bit identities. Signed integers и floating-point values используют `std::bit_cast`, поэтому codec не зависит от pointer aliasing или undefined behavior.

Empty arrays и empty objects имеют typed wrappers вокруг empty-list payload и остаются отличимыми от JSON `null`.

## Каноническое переиспользование

Повторное encoding одного значения с тем же vocabulary и store переиспользует canonical links. Codec не вводит независимый слой pointer/object identity.

## Покрытие тестами

`json_value_codec_tests` проверяет:

- null и Boolean values;
- unsigned/signed/float;
- пустые и непустые строки, включая UTF-8;
- пустые и вложенные arrays;
- пустые и вложенные objects и mixed values;
- canonical reuse при repeated encode;
- восстановление codec по существующему vocabulary;
- отклонение unrelated `LinkId`.

CLI JSON roundtrip fixtures дополнительно проверяют codec через собранный executable на Linux, Windows и macOS.

## Роль в AVM 1.5

`JsonValueCodec` доказывает, что structured JSON data может быть представлена links, но JSON type universe не становится автоматически канонической value semantics Модели Отношений. AVM 1.5 #128 должен определить, какие числовые, текстовые и collection denotations являются семантическими контрактами независимо от JSON frontend.
