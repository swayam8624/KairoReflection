# KairoReflection — typed properties, checked writes and stable editor metadata

This guide connects
[`ReflectionTypes.cppm`](../ReflectionTypes.cppm),
[`ReflectionRegistry.cppm`](../ReflectionRegistry.cppm),
[`KairoReflection.cppm`](../KairoReflection.cppm)
and [`tests/ReflectionTests.cpp`](../tests/ReflectionTests.cpp).
EngineCore registers authored component descriptors; KairoEditor
uses them to display inspectable properties and execute undoable edits.

## 1. Why reflection metadata exists

An editor cannot safely guess a component's layout from a display
label such as “Position.” The schema needs a stable *type key*,
a stable *property key*, value kind, display/category metadata,
limits and typed read/write accessors.

ReflectionRegistry maps dotted ASCII keys to immutable
`TypeDescriptor` entries. Register validates type and property
contracts and orders properties by stable key, so a different
registration sequence does not reorder the inspector or change
serialization behavior unexpectedly.

Compiler RTTI/typeid or a human display name is not a substitute
for the portable key used by saved project documents.

## 2. The caller owns the object

`PropertyDescriptor` stores `Read(const void*)` and
`Write(void*, const PropertyValue&)` callbacks. The registry owns
their descriptors, **not the memory** those callbacks access.

Passing a pointer to the wrong concrete component type, a destroyed
object or a component moved by ECS swap-remove is a violation of the
calling contract. Validating a variant's value kind does not make
an arbitrary `void*` safe.

## 3. Validate every value before writing

`ReflectionRegistry::Write` checks the property is known,
the pointer is non-null, the property isn't read-only, the supplied
`PropertyValueKind` matches and its value satisfies metadata.

Properties may declare numeric bounds, text byte limits, enumeration
options, typed target references, or array element kinds/count limits.
Nested arrays are currently excluded by the schema. Bounded array,
reference and string limits prevent an inspector or document from
accidentally accepting arbitrarily large values.

The actual write callback runs only after validation. The Editor
should still wrap that call in a reversible SceneCommand so undo
restores the authored component rather than only the UI widget.

## 4. Read and snapshot semantics

`Read` invokes the registered getter and validates the returned
kind/value; a broken accessor must not silently put the wrong
variant into inspector state. `Snapshot` produces borrowed
references to stable descriptors, not copied component objects.
The ReflectionRegistry must outlive such borrowed descriptors.

A reflection snapshot is **schema metadata**. It is not a
serialization of all live scene entities, and it does not hold
pointer lifetime across project reload, ECS compaction or deletion.

## 5. Cross-repository flow

```text
EngineCore component schema
   -> Reflection type + property registration
   -> Editor inspector reads metadata and component
   -> User proposes new typed value
   -> Editor SceneCommand/CommandHistory transaction
   -> ReflectionRegistry::Write validates value
   -> EngineCore Scene publishes authored component change
   -> Renderer/Physics adapters reconcile if necessary
```

This preserves the difference between metadata validation, authoritative
scene mutation, visual UI state and native runtime resource lifetime.

## 6. Exercises and tests

Read [`tests/ReflectionTests.cpp`](../tests/ReflectionTests.cpp).
Register a component with an enum, numeric range and read-only field.
Attempt to write an invalid enum, the wrong value kind, a value outside
bounds and a read-only property; each must fail without mutation.
Then integrate a valid property edit with an Editor CommandHistory
undo/redo cycle and verify exact restoration.

Inspect stable key and schema compatibility before adding a new field
to a persistent .kscene or authoring document. Changing display
text need not change a stable serialized key.

*Evidence note: source-inspection documentation, not a new compiler or
native-editor UI test run.*
