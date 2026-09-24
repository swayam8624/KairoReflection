#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <variant>

import Kairo.Reflection;

using namespace kairo::reflection;

namespace
{
    struct CameraSettings final
    {
        double FieldOfView = 60.0;
        std::string Name = "Camera";
        bool Enabled = true;
        std::uint32_t Revision = 1u;
    };

    [[nodiscard]] PropertyMetadata Metadata(
        std::string key,
        std::string displayName,
        std::string category,
        std::string tooltip,
        PropertyFlags flags = PropertyFlags::None,
        std::optional<NumericRange> range = std::nullopt,
        std::size_t maximumStringBytes = 0u)
    {
        PropertyMetadata metadata;
        metadata.Key = std::move(key);
        metadata.DisplayName = std::move(displayName);
        metadata.Category = std::move(category);
        metadata.Tooltip = std::move(tooltip);
        metadata.Flags = flags;
        metadata.Range = range;
        metadata.MaximumStringBytes = maximumStringBytes;
        return metadata;
    }

    [[nodiscard]] TypeDescriptor MakeCameraDescriptor()
    {
        TypeDescriptor descriptor;
        descriptor.Key = "Kairo.Engine.CameraSettings";
        descriptor.DisplayName = "Camera Settings";
        descriptor.Category = "Rendering";
        descriptor.Properties = {
            MakeMemberProperty<CameraSettings>(
                Metadata("field-of-view", "Field Of View", "Lens",
                    "Vertical angle in degrees", PropertyFlags::None,
                    NumericRange{ 1.0, 179.0, 0.5 }),
                &CameraSettings::FieldOfView),
            MakeMemberProperty<CameraSettings>(
                Metadata("name", "Name", "General", "Display name",
                    PropertyFlags::None, std::nullopt, 32u),
                &CameraSettings::Name),
            MakeMemberProperty<CameraSettings>(
                Metadata("enabled", "Enabled", "General", ""),
                &CameraSettings::Enabled),
            MakeMemberProperty<CameraSettings>(
                Metadata("revision", "Revision", "Internal", "Read-only version",
                    PropertyFlags::ReadOnly),
                &CameraSettings::Revision)
        };
        return descriptor;
    }

    struct Float3 final
    {
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
    };

    struct FloatQuaternion final
    {
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        double W = 1.0;
    };

    enum class ProjectionMode : std::int32_t
    {
        Perspective = 1,
        Orthographic = 2
    };

    struct CompositeSettings final
    {
        Float3 Position{ 1.0, 2.0, 3.0 };
        FloatQuaternion Rotation{};
        ProjectionMode Mode = ProjectionMode::Perspective;
        std::string Mesh = "00000000-0000-4000-8000-000000000001";
    };

    [[nodiscard]] TypeDescriptor MakeCompositeDescriptor()
    {
        PropertyMetadata position = Metadata(
            "position", "Position", "Transform", "World-independent position value");
        PropertyMetadata rotation = Metadata(
            "rotation", "Rotation", "Transform", "Quaternion orientation");
        PropertyMetadata mode = Metadata(
            "projection-mode", "Projection Mode", "Camera", "Projection policy");
        mode.EnumOptions = {
            { 1, "projection.perspective", "Perspective" },
            { 2, "projection.orthographic", "Orthographic" }
        };
        PropertyMetadata mesh = Metadata(
            "mesh", "Mesh", "Rendering", "Stable mesh asset reference");
        mesh.MaximumReferenceBytes = 64u;

        TypeDescriptor descriptor;
        descriptor.Key = "Kairo.Engine.CompositeSettings";
        descriptor.DisplayName = "Composite Settings";
        descriptor.Category = "Testing";
        descriptor.Properties = {
            MakeAdaptedMemberProperty<CompositeSettings>(
                std::move(position), &CompositeSettings::Position,
                PropertyValueKind::Vector3,
                [](const Float3& value)
                {
                    return PropertyValue(Vector3Value{ value.X, value.Y, value.Z });
                },
                [](const PropertyValue& value)
                {
                    const Vector3Value& vector = value.Get<Vector3Value>();
                    return Float3{ vector.X, vector.Y, vector.Z };
                }),
            MakeAdaptedMemberProperty<CompositeSettings>(
                std::move(rotation), &CompositeSettings::Rotation,
                PropertyValueKind::Quaternion,
                [](const FloatQuaternion& value)
                {
                    return PropertyValue(QuaternionValue{ value.X, value.Y, value.Z, value.W });
                },
                [](const PropertyValue& value)
                {
                    const QuaternionValue& quaternion = value.Get<QuaternionValue>();
                    return FloatQuaternion{ quaternion.X, quaternion.Y, quaternion.Z, quaternion.W };
                }),
            MakeEnumMemberProperty<CompositeSettings>(std::move(mode), &CompositeSettings::Mode),
            MakeReferenceMemberProperty<CompositeSettings>(
                std::move(mesh), &CompositeSettings::Mesh, "Kairo.Assets.Mesh",
                [](const std::string& identifier) { return identifier; },
                [](const std::string& identifier) { return identifier; })
        };
        return descriptor;
    }
}

TEST_CASE("Reflection registry reads and writes typed member properties", "[KairoReflection][Access]")
{
    ReflectionRegistry registry;
    registry.Register(MakeCameraDescriptor());
    CameraSettings camera;

    CHECK(registry.Read("Kairo.Engine.CameraSettings", "field-of-view", &camera) == PropertyValue(60.0));
    registry.Write("Kairo.Engine.CameraSettings", "field-of-view", &camera, PropertyValue(75.0));
    registry.Write("Kairo.Engine.CameraSettings", "name", &camera, PropertyValue("Gameplay Camera"));
    registry.Write("Kairo.Engine.CameraSettings", "enabled", &camera, PropertyValue(false));

    CHECK(camera.FieldOfView == 75.0);
    CHECK(camera.Name == "Gameplay Camera");
    CHECK_FALSE(camera.Enabled);
    CHECK(registry.Read("Kairo.Engine.CameraSettings", "revision", &camera) == PropertyValue(std::uint64_t{ 1u }));
}

TEST_CASE("Reflection rejects invalid values before objects mutate", "[KairoReflection][Validation]")
{
    ReflectionRegistry registry;
    registry.Register(MakeCameraDescriptor());
    CameraSettings camera;

    REQUIRE_THROWS_AS(registry.Write("Kairo.Engine.CameraSettings", "field-of-view", &camera, PropertyValue(180.0)), std::out_of_range);
    CHECK(camera.FieldOfView == 60.0);
    REQUIRE_THROWS_AS(registry.Write("Kairo.Engine.CameraSettings", "field-of-view", &camera, PropertyValue(std::int64_t{ 60 })), std::invalid_argument);
    REQUIRE_THROWS_AS(registry.Write("Kairo.Engine.CameraSettings", "name", &camera, PropertyValue(std::string(33u, 'x'))), std::length_error);
    REQUIRE_THROWS_AS(registry.Write("Kairo.Engine.CameraSettings", "revision", &camera, PropertyValue(std::uint64_t{ 2u })), std::logic_error);
    REQUIRE_THROWS_AS(registry.Read("Kairo.Engine.CameraSettings", "missing", &camera), std::out_of_range);
    REQUIRE_THROWS_AS(registry.Read("Kairo.Engine.CameraSettings", "name", nullptr), std::invalid_argument);
}

TEST_CASE("Reflection registration is deterministic and validates stable metadata", "[KairoReflection][Registry]")
{
    ReflectionRegistry registry;
    TypeDescriptor descriptor = MakeCameraDescriptor();
    descriptor.Properties.push_back(MakeMemberProperty<CameraSettings>(
        Metadata("alpha", "Alpha", "General", ""), &CameraSettings::Enabled));
    registry.Register(descriptor);

    const TypeDescriptor& registered = registry.Require("Kairo.Engine.CameraSettings");
    REQUIRE(registered.Properties.size() == 5u);
    CHECK(registered.Properties.front().Metadata.Key == "alpha");
    CHECK(registry.Search("camera").size() == 1u);
    CHECK(registry.Search("rendering").size() == 1u);
    REQUIRE_THROWS_AS(registry.Register(MakeCameraDescriptor()), std::invalid_argument);

    TypeDescriptor invalid = MakeCameraDescriptor();
    invalid.Key = "not a stable key";
    REQUIRE_THROWS_AS(ReflectionRegistry{}.Register(std::move(invalid)), std::invalid_argument);
}

TEST_CASE("Reflection V2 round trips composite enum and stable reference values", "[KairoReflection][V2]")
{
    ReflectionRegistry registry;
    registry.Register(MakeCompositeDescriptor());
    CompositeSettings settings;

    CHECK(registry.Read("Kairo.Engine.CompositeSettings", "position", &settings) ==
        PropertyValue(Vector3Value{ 1.0, 2.0, 3.0 }));
    CHECK(registry.Read("Kairo.Engine.CompositeSettings", "rotation", &settings) ==
        PropertyValue(QuaternionValue{ 0.0, 0.0, 0.0, 1.0 }));
    CHECK(registry.Read("Kairo.Engine.CompositeSettings", "projection-mode", &settings) ==
        PropertyValue(EnumerationValue{ 1, "projection.perspective" }));
    CHECK(registry.Read("Kairo.Engine.CompositeSettings", "mesh", &settings) ==
        PropertyValue(ReferenceValue{ "Kairo.Assets.Mesh", settings.Mesh }));

    registry.Write("Kairo.Engine.CompositeSettings", "position", &settings,
        PropertyValue(Vector3Value{ -4.0, 5.0, 6.5 }));
    registry.Write("Kairo.Engine.CompositeSettings", "rotation", &settings,
        PropertyValue(QuaternionValue{ 0.0, 1.0, 0.0, 0.0 }));
    registry.Write("Kairo.Engine.CompositeSettings", "projection-mode", &settings,
        PropertyValue(EnumerationValue{ 2, "projection.orthographic" }));
    registry.Write("Kairo.Engine.CompositeSettings", "mesh", &settings,
        PropertyValue(ReferenceValue{ "Kairo.Assets.Mesh", "00000000-0000-4000-8000-000000000099" }));

    CHECK(settings.Position.X == -4.0);
    CHECK(settings.Position.Y == 5.0);
    CHECK(settings.Position.Z == 6.5);
    CHECK(settings.Rotation.Y == 1.0);
    CHECK(settings.Rotation.W == 0.0);
    CHECK(settings.Mode == ProjectionMode::Orthographic);
    CHECK(settings.Mesh == "00000000-0000-4000-8000-000000000099");
}

TEST_CASE("Reflection V2 validates composite enum and reference contracts", "[KairoReflection][V2][Validation]")
{
    ReflectionRegistry registry;
    registry.Register(MakeCompositeDescriptor());
    CompositeSettings settings;

    REQUIRE_THROWS_AS(
        registry.Write("Kairo.Engine.CompositeSettings", "projection-mode", &settings,
            PropertyValue(EnumerationValue{ 9, "projection.unknown" })),
        std::out_of_range);
    CHECK(settings.Mode == ProjectionMode::Perspective);

    REQUIRE_THROWS_AS(
        registry.Write("Kairo.Engine.CompositeSettings", "mesh", &settings,
            PropertyValue(ReferenceValue{ "Kairo.Assets.Texture", "id" })),
        std::invalid_argument);
    REQUIRE_THROWS_AS(
        registry.Write("Kairo.Engine.CompositeSettings", "mesh", &settings,
            PropertyValue(ReferenceValue{ "Kairo.Assets.Mesh", std::string(65u, 'a') })),
        std::length_error);

    REQUIRE_THROWS_AS(
        PropertyValue(Vector3Value{ std::numeric_limits<double>::infinity(), 0.0, 0.0 }),
        std::invalid_argument);

    TypeDescriptor invalid = MakeCompositeDescriptor();
    invalid.Properties.front().Metadata.Range = NumericRange{ -1.0, 1.0, 0.1 };
    REQUIRE_THROWS_AS(ReflectionRegistry{}.Register(std::move(invalid)), std::invalid_argument);

    TypeDescriptor duplicateEnum = MakeCompositeDescriptor();
    // Registered property order is not sorted until registry insertion; locate enum explicitly.
    for (PropertyDescriptor& property : duplicateEnum.Properties)
    {
        if (property.ValueKind == PropertyValueKind::Enumeration)
        {
            property.Metadata.EnumOptions.push_back({ 1, "projection.duplicate", "Duplicate" });
            REQUIRE_THROWS_AS(ReflectionRegistry{}.Register(std::move(duplicateEnum)), std::invalid_argument);
            return;
        }
    }
    FAIL("Composite descriptor did not contain its enumeration property.");
}


TEST_CASE("Reflection V3 round trips bounded primitive arrays", "[KairoReflection][V3][Array]")
{
    struct CollectionSettings final
    {
        std::vector<std::int32_t> Layers{ 1, 3, 7 };
        std::vector<std::string> Labels{ "player", "visible" };
    };

    PropertyMetadata layers = Metadata(
        "layers", "Layers", "Runtime", "Layer indices");
    layers.MaximumArrayElements = 8u;

    PropertyMetadata labels = Metadata(
        "labels", "Labels", "Runtime", "Stable labels");
    labels.MaximumArrayElements = 4u;

    TypeDescriptor type;
    type.Key = "Kairo.Engine.CollectionSettings";
    type.DisplayName = "Collection Settings";
    type.Category = "Testing";
    type.Properties = {
        MakePrimitiveArrayMemberProperty<CollectionSettings>(
            std::move(layers), &CollectionSettings::Layers),
        MakePrimitiveArrayMemberProperty<CollectionSettings>(
            std::move(labels), &CollectionSettings::Labels)
    };

    ReflectionRegistry registry;
    registry.Register(std::move(type));
    CollectionSettings settings;

    const PropertyValue initial =
        registry.Read("Kairo.Engine.CollectionSettings", "layers", &settings);
    REQUIRE(initial.Kind() == PropertyValueKind::Array);
    const ArrayValue& initialArray = initial.Get<ArrayValue>();
    CHECK(initialArray.ElementKind == PropertyValueKind::SignedInteger);
    REQUIRE(initialArray.Values.size() == 3u);
    CHECK(std::get<std::int64_t>(initialArray.Values[2]) == 7);

    ArrayValue replacement;
    replacement.ElementKind = PropertyValueKind::SignedInteger;
    replacement.Values = {
        ArrayElementStorage(std::int64_t{ 2 }),
        ArrayElementStorage(std::int64_t{ 4 }),
        ArrayElementStorage(std::int64_t{ 6 })
    };
    registry.Write(
        "Kairo.Engine.CollectionSettings", "layers", &settings,
        PropertyValue(std::move(replacement)));
    REQUIRE((settings.Layers == std::vector<std::int32_t>{ 2, 4, 6 }));

    ArrayValue tooLarge;
    tooLarge.ElementKind = PropertyValueKind::String;
    for (int index = 0; index < 5; ++index)
        tooLarge.Values.emplace_back(std::string("tag"));
    REQUIRE_THROWS_AS(
        registry.Write(
            "Kairo.Engine.CollectionSettings", "labels", &settings,
            PropertyValue(std::move(tooLarge))),
        std::length_error);

    ArrayValue wrongKind;
    wrongKind.ElementKind = PropertyValueKind::UnsignedInteger;
    wrongKind.Values.emplace_back(std::uint64_t{ 1u });
    REQUIRE_THROWS_AS(
        registry.Write(
            "Kairo.Engine.CollectionSettings", "layers", &settings,
            PropertyValue(std::move(wrongKind))),
        std::invalid_argument);
}
