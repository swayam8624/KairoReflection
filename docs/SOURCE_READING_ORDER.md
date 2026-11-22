# Teaching source map — KairoReflection
**Deep source walkthrough:** [Stable reflection metadata and checked inspector writes](REFLECTION_METADATA_TO_SAFE_EDITOR.md).

Stable type/field metadata for authoring, validation and serialization.

Follow source links sequentially. For each API/function, identify its input contract, owning data structure, called modules, failure modes, outputs, test and downstream consumers.

## Read in this order

1. [`CMakeLists.txt`](../CMakeLists.txt)
2. [`KairoReflection.cppm`](../KairoReflection.cppm)
3. [`ReflectionTypes.cppm`](../ReflectionTypes.cppm)
4. [`ReflectionRegistry.cppm`](../ReflectionRegistry.cppm)
5. [`tests`](../tests)
6. [`docs`](../docs)

## How this repository interacts with others

EngineCore registers runtime components and KairoEditor consumes metadata for inspectors/document validation. Reflection must not mutate arbitrary state without checked constraints.

## Verification and study rules

- Read the public C++23 module/entrypoint first, then its implementation and tests.
- Explain invariants, ownership, error states, concurrency or numerical assumptions in comments, not trivial syntax.
- A passing CI job covers only tests actually run on that commit/host; source presence is not runtime certification.
- Check [STATUS.md](../STATUS.md) and [STATUS.yaml](../STATUS.yaml) for current explicit verification limitations.
- Original work may be modified and extended under the repository license; third-party content retains upstream terms.


## Complete source-file inventory

After the curated teaching path, follow this deterministic inventory of first-party source, scripts, shaders, and tests. Grouping is a suggested learning order, not an indication that every function is already annotated. Vendored dependencies, ROMs, binaries, data, media, and generated evidence are excluded.

### Entry points and build (1)

1. [`CMakeLists.txt`](../CMakeLists.txt)

### Interfaces and modules (3)

1. [`KairoReflection.cppm`](../KairoReflection.cppm)
2. [`ReflectionRegistry.cppm`](../ReflectionRegistry.cppm)
3. [`ReflectionTypes.cppm`](../ReflectionTypes.cppm)

### Verification and tests (1)

1. [`tests/ReflectionTests.cpp`](../tests/ReflectionTests.cpp)

Snapshot totals: 5 first-party source/build/test files, 13 tracked files overall. Re-audit when files are renamed or added.
