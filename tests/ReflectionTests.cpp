#include <catch2/catch_test_macros.hpp>

#include <stdexcept>
#include <string>

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

    [[nodiscard]] TypeDescriptor MakeCameraDescriptor()
    {
        TypeDescriptor descriptor;
        descriptor.Key = "Kairo.Engine.CameraSettings";
        descriptor.DisplayName = "Camera Settings";
        descriptor.Category = "Rendering";
        descriptor.Properties = {
            MakeMemberProperty<CameraSettings>({ "field-of-view", "Field Of View", "Lens", "Vertical angle in degrees", PropertyFlags::None,
                NumericRange{ 1.0, 179.0, 0.5 }, 0u }, &CameraSettings::FieldOfView),
            MakeMemberProperty<CameraSettings>({ "name", "Name", "General", "Display name", PropertyFlags::None, std::nullopt, 32u }, &CameraSettings::Name),
            MakeMemberProperty<CameraSettings>({ "enabled", "Enabled", "General", "", PropertyFlags::None, std::nullopt, 0u }, &CameraSettings::Enabled),
            MakeMemberProperty<CameraSettings>({ "revision", "Revision", "Internal", "Read-only version", PropertyFlags::ReadOnly, std::nullopt, 0u }, &CameraSettings::Revision)
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
    descriptor.Properties.push_back(MakeMemberProperty<CameraSettings>({ "alpha", "Alpha", "General", "", PropertyFlags::None, std::nullopt, 0u }, &CameraSettings::Enabled));
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
