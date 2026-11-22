# KairoReflection
**Deep source walkthrough:** [Stable reflection metadata and checked inspector writes](docs/REFLECTION_METADATA_TO_SAFE_EDITOR.md).

**Worked validation lab:** [From stable property keys to type-checked writes and Editor undo](docs/SAFE_REFLECTION_VALUES_TO_EDITOR_COMMANDS_LAB.md) · [Reflection, descriptors and safe editing](docs/REFLECTION_WALKTHROUGH.md) · [File-by-file source map](docs/SOURCE_READING_ORDER.md)

`KairoReflection` is Kairo's engine-owned C++23 reflection metadata layer. It
turns typed runtime fields into stable, validated property descriptors for the
editor inspector, serializers, graph parameter panels, search, and future
plugin tooling without coupling those systems to Dear ImGui, a scene graph, or
an ECS storage implementation.

## Why It Is Separate

```text
KairoECS             owns entities and component storage
KairoEngineCore      owns scene/runtime objects
KairoReflection      describes fields and performs validated access
KairoEditor          renders inspector controls through KairoUI
```

Reflection does not allocate or own reflected objects. Callers retain object
lifetime and pass the correct object pointer for the type descriptor they use.
This keeps the module usable for plain structs, ECS components, scene records,
and editor documents.

## Surface

```cpp
import Kairo.Reflection;

struct CameraSettings {
    double FieldOfView = 60.0;
    std::string Name = "Camera";
};

kairo::reflection::TypeDescriptor camera{
    .Key = "Kairo.Engine.CameraSettings",
    .DisplayName = "Camera Settings",
    .Category = "Rendering",
    .Properties = {
        kairo::reflection::MakeMemberProperty<CameraSettings>(
            { "field-of-view", "Field Of View", "Lens", "Degrees", {},
              kairo::reflection::NumericRange{ 1.0, 179.0, 0.5 } },
            &CameraSettings::FieldOfView),
        kairo::reflection::MakeMemberProperty<CameraSettings>(
            { "name", "Name", "General", "Display name", {}, std::nullopt, 128u },
            &CameraSettings::Name)
    }
};

kairo::reflection::ReflectionRegistry registry;
registry.Register(std::move(camera));
```

`ReflectionRegistry::Write()` validates the property key, value kind, numeric
range, string byte limit, and read-only status before invoking an accessor.
Primitive values never silently coerce between signed, unsigned, floating, and
boolean types.

## Conventions

- Type and property keys are stable dotted ASCII identifiers. They are data
  identifiers, not RTTI names.
- Descriptors are immutable after registration. Duplicate type/property keys
  fail instead of replacing metadata at runtime.
- Property snapshots and search results are deterministic by stable type key.
- `MakeMemberProperty` supports `bool`, signed/unsigned integral types,
  floating-point types, and `std::string` fields.
- Vector/quaternion and subsystem-owned composite values use explicit
  `MakeAdaptedMemberProperty` adapters rather than hidden coercion.
- Enum properties retain stable symbolic keys plus validated numeric values.
- Reference properties declare one stable target type and bounded identifier.
- V3 homogeneous arrays are bounded, non-recursive, explicitly typed, and may
  use the primitive vector adapter or a subsystem-owned composite adapter.
- Custom UI drawers remain a consumer concern; reflection describes semantics,
  not Dear ImGui/Qt rendering policy.

## Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++
cmake --build build
ctest --test-dir build --output-on-failure
```

## V1 Boundaries

This repository intentionally does not contain serialization, an ECS, an
editor UI, a property-grid implementation, code generation, or a plugin ABI.
Those systems consume this metadata layer; keeping ownership separate avoids a
second scene model and leaves KairoUI free to evolve independently.


## V3 Collections

`PropertyValueKind::Array` carries a homogeneous `ArrayValue` with an explicit
element kind. The canonical transport hard-limits arrays to 65,536 elements;
individual properties may specify a tighter `MaximumArrayElements`. Nested
arrays are intentionally rejected in V3 so reflection metadata does not become
an unbounded document tree.

`MakePrimitiveArrayMemberProperty` round-trips bounded vectors of supported
primitive values through the same pre-mutation validation path as scalar
properties. More specialized arrays can use the generic adapted-property
boundary.

See [STATUS.md](STATUS.md) for the frozen Wave-B scope and 80% exit contract.


## Source learning pathway

Follow [the linked reading order](docs/SOURCE_READING_ORDER.md) to study the public interfaces, implementations, tests and cross-repository dependencies in a deliberate sequence. It explains what each subsystem owns, where its outputs go, and which behaviors require verification. Distinguish C++23 module interfaces, actual backend implementations and planned APIs.

## Open-source learning and extension

Original project code is available to learn from, modify and develop further under the applicable [LICENSE](LICENSE). Keep copyright/license notices when redistributing substantial portions. Dependencies, code adapted from other sources, media, datasets, models and other third-party materials remain subject to their original licenses. Consult [STATUS.md](STATUS.md) for evidence limitations rather than treating documentation or an unverified completion target as a guarantee of functionality.
