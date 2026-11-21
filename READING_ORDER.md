# KairoReflection: linked source reading guide

Start with reflection metadata type definitions, then the registry used to register/query property descriptors, then the exported umbrella module and tests. KairoEngineCore publishes component metadata; KairoEditor uses it to build an inspector and apply validated commands without hardcoding every field.

## Concepts and cross-module APIs

Distinguish metadata identifiers/constraints from an instance's mutable value. The reflection registry defines introspection contracts, not a generic serialization engine or an ImGui widget renderer.

## Source order (4 files)

1. [`ReflectionTypes.cppm`](ReflectionTypes.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
2. [`ReflectionRegistry.cppm`](ReflectionRegistry.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
3. [`KairoReflection.cppm`](KairoReflection.cppm) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.
4. [`tests/ReflectionTests.cpp`](tests/ReflectionTests.cpp) — read the exported API, invariants, dependency imports, implementation and relevant test relationships.

## Reproduce and extend

Use [README.md](README.md) for setup and the relevant build-system/tests for independent verification. A green isolated build is not proof of the complete 27-module Kairo workspace, whose heads must pass [KairoGameEngine acceptance](https://github.com/swayam8624/KairoGameEngine). Original code is [MIT-licensed](LICENSE) for learning, reuse and further development; third-party dependencies retain their licenses. The index is an initial teaching map, not complete per-function annotation.
