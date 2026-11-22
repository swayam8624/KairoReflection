# Reflection laboratory: stable property schemas and reversible Editor edits

Use [ReflectionTypes.cppm](../ReflectionTypes.cppm), [ReflectionRegistry.cppm](../ReflectionRegistry.cppm) and [ReflectionTests.cpp](../tests/ReflectionTests.cpp). For the consumer, open [KairoEditor's SceneCommands.cppm](https://github.com/swayam8624/KairoEditor/blob/main/SceneCommands.cppm) at SetReflectedPropertyCommand. This is a validation and ownership lesson, not a claim that reflection by itself is a secure script sandbox.

## 1. Why schema identity is neither RTTI nor a visible label

C++ std::type_index identifies a compiler/runtime type within a particular process; a user-visible DisplayName can be localized or renamed. Saved properties require durable keys. KairoReflection uses dotted ASCII identifiers such as Kairo.Engine.CameraSettings, independent of compiler RTTI. IsStableKey rejects empty segments, invalid segment-leading characters, and overly long keys.

ReflectionRegistry rejects duplicate types and duplicate field keys in a type. Register sorts properties by stable key, so registration order does not accidentally change the inspector/document ordering.

**Exercise:** attempt empty key, .Bad, Engine..Camera, 7Camera, Engine.Camera and Engine.Camera-1. Identify acceptance by tracing the exact IsStableKey loop, rather than applying conventional C++ identifier assumptions.

## 2. PropertyValue owns values, not target objects

PropertyValue contains an owning, tagged std::variant of boolean, signed/unsigned integers, double, string, vector/quaternion records, enum, typed reference and homogeneous array. Its Kind is the active variant alternative, not a string inferred from display labels. Constructors reject nonfinite numeric/vector values before those values can reach matrices, physics or Editor serialization.

Canonical vector records use doubles as a transport policy. They do not imply that every destination subsystem uses double internally. Narrowing back to float or smaller integral types requires explicit range checks.

An ArrayValue declares one ElementKind and must contain matching elements. V3 disallows nested arrays, caps values at 65,536, and validates individual strings, references, enums and finite math values. Descriptor metadata may impose a tighter per-property limit.

**Exercise:** construct an array with ElementKind FloatingPoint but one String element, then an array containing 65,537 doubles. Both must fail construction. An array with three floats may be a valid transport value but fail a property whose metadata limit is two.

## 3. Kind checks do not perform coercion

A SignedInteger PropertyValue is not interchangeable with UnsignedInteger even if both represent 3. ReflectionRegistry::Write first checks the exact PropertyValueKind. DecodePropertyValue<T> then checks whether a canonical int64/uint64/double fits the target C++ field.

**Worked example:** a reflected uint8 field can represent 42 and 255 but cannot represent canonical uint64 value 300. Blind narrowing would wrap 300 to 44. A SignedInteger(-1) must also fail the unsigned property's variant kind contract.

ReadOnly is enforced by the registry before calling the setter. A pointer to a real object does not override the descriptor's access contract.

**Exercise:** attempt values 255, 256, and SignedInteger(-1) on a uint8 field; confirm invalid writes do not mutate it.

## 4. Metadata validity versus component invariants

Descriptors may impose numeric range, maximum string bytes, enumeration options, reference target type or homogeneous array kind/count. Register validates whether each constraint is applicable. For example, only scalar numeric properties may declare a numeric range.

ValidateValue checks the supplied value against these constraints, but does not automatically inspect cross-field semantics. Setting CameraNear and CameraFar to individually positive values does not imply CameraNear is smaller than CameraFar.

KairoEditor's SetReflectedPropertyCommand captures the before value, uses ReflectionRegistry::Write for the after value, then validates the complete EngineCore component. If component validation rejects the change, the command attempts to restore the old value before rethrowing. Undo and Redo belong to CommandHistory, not the reflection metadata registry.

**Exercise:** start with Near=0.1 and Far=100, then set Far=0.05. A per-field positive range could accept that; the component-level camera validator must reject it and restore Far=100. Check both data and undo cursor.

## 5. An enum has two correlated identities

EnumerationValue contains stable option Key and numeric Value. ValidateValue requires both to match the *same* registered EnumOption. Otherwise serialized data could claim Key shadow.disabled with Value 2, even if numeric 2 represents another option.

The C++ enum adapter also checks representability in its underlying integral type. A valid metadata option is not permission to overflow a smaller enum storage type.

**Exercise:** register two distinct enum options. Mix the first option's Key and second option's numeric Value. The mutation must fail.

## 6. Typed reference validation is not resource resolution

ReferenceValue contains a TargetType stable key and a bounded identifier without control bytes. Its property descriptor requires matching target type. Reflection can validate this shape without knowing whether an AssetID exists in KairoAssets, an EngineCore entity remains alive or a GPU resource is loaded.

KairoAssets::AssetRegistry, EngineCore::Scene and renderer adapters separately resolve those identities and verify liveness.

**Exercise:** create a syntactically valid MeshAsset reference whose ID is missing from the project. Reflection's schema validation can succeed, but AssetRegistry::Resolve must reject it.

## 7. The registry does not own reflected memory

ReflectionRegistry stores metadata and trusted accessor callbacks. MakeMemberProperty/MakeAdaptedMemberProperty expect a correctly typed, still-live object pointer from the caller. The registry does not dynamically identify the concrete type behind arbitrary void*. Supplying a mismatched object pointer is undefined behavior regardless of whether the stable key exists.

This is why reflection must not be treated as a general authorization boundary for untrusted plugins or LLM commands. The Editor/host selects which objects are writable, resolves the correct type, and wraps changes in validated undo commands.

Snapshot and Search return borrowed descriptor reference wrappers, not owning copies. Keep the metadata registry alive while an inspector consumes those results.

## 8. Trace a complete reversible inspector change

Suppose a camera panel shows an FOV slider. The operational flow is:

1. EngineCore supplies a CameraComponent for a stable Scene Entity ID.
2. ReflectionRegistry resolves stable type/property keys and reads the before PropertyValue.
3. Editor constructs SetReflectedPropertyCommand containing before/after values.
4. Execute checks exact kind/range, applies accessor, and validates the complete component.
5. CommandHistory records the successful edit and may merge adjacent gesture updates to the same Entity/type/property.
6. Undo applies the original before value; Redo applies saved after value.
7. Scene serialization persists authored state; render extraction converts it into camera parameters for a native backend.

**Exercise:** drag 60 to 65 to 70 degrees with gesture coalescing. One Undo should restore 60. Interleave an edit to a different Entity and verify that commands do not merge across it.

## 9. Test scope and verification

Use ReflectionTests.cpp to cover stable key grammar, duplicate metadata, type/range rejection, enum pair consistency, reference target types, array bounds, nonfinite values, narrowing and read-only writes. Use Editor tests for component rollback and undo, and EngineCore serialization tests for persisted identity.

Record exact commit SHA, compiler, fixture and expected failure types. Source-level validation is not proof that arbitrary native pointers from untrusted plugins are safe.

*Documentation-only lesson. No new compiler or runtime test was executed.*
