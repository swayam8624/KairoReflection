# Reflection V2 Composite Contract

Reflection V2 extends the original scalar-only property transport without making KairoReflection
own engine math, assets, entities, scene storage, or editor widgets.

## Canonical value kinds

In addition to boolean, integer, floating-point, and string values, `PropertyValue` can carry:

- `Vector2Value`
- `Vector3Value`
- `Vector4Value`
- `QuaternionValue`
- `EnumerationValue`
- `ReferenceValue`

Composite numeric records use finite `double` components so the reflection boundary remains
independent of the concrete scalar type used by an owning subsystem. Quaternion normalization,
transform scale rules, and other semantic invariants remain owner-side validation.

## Enumeration metadata

Enumeration properties declare bounded, deterministic `EnumOption` metadata. Each option owns a
stable key, display label, and canonical signed numeric value. Duplicate keys or values are rejected
at registration. Reads fail if an object's enum value is absent from its registered contract; writes
accept only an exact registered key/value pair.

## Stable references

A reference property declares one stable target-type key such as `Kairo.Assets.Mesh` or
`Kairo.Engine.Entity`. The transported `ReferenceValue` contains that target type plus an opaque,
bounded identifier string. KairoReflection validates target identity and byte limits but never parses
or resolves subsystem-owned identifiers.

## Adapters

`MakeAdaptedMemberProperty` lets a subsystem map its concrete vector/quaternion/reference type to a
canonical value without adding a dependency from Reflection back to that subsystem.
`MakeEnumMemberProperty` provides the common enum path and `MakeReferenceMemberProperty` provides the
common typed-reference path.

This preserves the ownership rule:

```text
KairoReflection describes and validates property transport
KairoMath / KairoAssets / KairoEngineCore own the concrete data types
KairoEditor chooses the visual control
```
