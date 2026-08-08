# JSON value codec

`JsonValueCodec` is the data roundtrip path used when JSON is treated as data rather than as an AVM program.

It replaces the historical pointer-based `rel_t` JSON representation. The codec stores every value in the same canonical `LinkStore` identity universe used by the rest of AVM.

## Boundary

```text
nlohmann::json
    |
    v
JsonValueCodec::encode
    |
    v
LinkId in LinkStore
    |
    v
JsonValueCodec::decode
    |
    v
nlohmann::json
```

This codec is not an executor. Program semantics remain in `JsonProgramImporter` + `BootstrapRuntime` / `Executor`.

## Vocabulary

`JsonValueVocabulary` introduces explicit identities for:

- `unit` and list `nil`;
- null / true / false constants;
- array;
- byte;
- string;
- unsigned integer;
- signed integer;
- floating-point number;
- object;
- object entry.

The vocabulary may be created for a new store or supplied explicitly when restoring a previously materialized value graph.

## Representation

Typed values use a Relations Model entity whose subject is the JSON codec `unit` identity:

```text
(type_relation, unit, payload)
```

Arrays and objects store their ordered members through canonical link lists. Object entries are Relations Model entities:

```text
(entry_relation, key_string, value)
```

Strings are sequences of byte entities. A byte contains exactly eight Boolean bit identities. Numeric payloads contain exactly 64 Boolean bit identities. Signed integers and floating-point values use `std::bit_cast` so the codec does not rely on pointer aliasing or undefined behavior.

Empty arrays and empty objects have typed wrappers around the empty-list payload and therefore remain distinguishable from JSON `null`.

## Canonical reuse

Encoding the same value twice with the same vocabulary and store reuses canonical links. The codec does not introduce an independent pointer/object identity layer.

## Tests

`json_value_codec_tests` cover:

- null and booleans;
- unsigned/signed/float values;
- empty and non-empty strings, including UTF-8 text;
- empty/nested arrays;
- empty/nested objects and mixed values;
- canonical reuse on repeated encode;
- restoring a codec from an existing vocabulary;
- rejection of unrelated LinkIds.

CLI JSON roundtrip fixtures additionally exercise the codec through the built executable on Linux, Windows and macOS.
