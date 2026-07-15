# KairoReflection

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
- Enums, arrays, object references, vectors, and custom drawers will be added
  as explicit adapters rather than hidden conversions.

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
