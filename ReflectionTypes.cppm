module;

#include <cmath>
#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module Kairo.Reflection.Types;

export namespace kairo::reflection
{
    /// A type/property/reference key is a stable dotted ASCII identifier such as
    /// `Kairo.Engine.Transform`. It intentionally is not an RTTI name, which
    /// keeps serialized data and plugin boundaries compiler-independent.
    [[nodiscard]] inline bool IsStableKey(std::string_view key) noexcept
    {
        if (key.empty() || key.size() > 192u) return false;
        bool segmentStart = true;
        for (const unsigned char character : key)
        {
            if (character == '.')
            {
                if (segmentStart) return false;
                segmentStart = true;
                continue;
            }
            const bool alpha = (character >= 'a' && character <= 'z') ||
                (character >= 'A' && character <= 'Z') || character == '_';
            const bool continuation = alpha || (character >= '0' && character <= '9') || character == '-';
            if ((segmentStart && !alpha) || (!segmentStart && !continuation)) return false;
            segmentStart = false;
        }
        return !segmentStart;
    }

    struct Vector2Value final
    {
        double X = 0.0;
        double Y = 0.0;
        friend bool operator==(const Vector2Value&, const Vector2Value&) = default;
    };

    struct Vector3Value final
    {
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        friend bool operator==(const Vector3Value&, const Vector3Value&) = default;
    };

    struct Vector4Value final
    {
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        double W = 0.0;
        friend bool operator==(const Vector4Value&, const Vector4Value&) = default;
    };

    /// Quaternion components use the engine-independent `(x,y,z,w)` convention.
    /// Reflection validates finiteness only. Unit-length/domain constraints remain
    /// the responsibility of the owning component validator.
    struct QuaternionValue final
    {
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
        double W = 1.0;
        friend bool operator==(const QuaternionValue&, const QuaternionValue&) = default;
    };

    struct EnumerationValue final
    {
        std::int64_t Value = 0;
        std::string Key;
        friend bool operator==(const EnumerationValue&, const EnumerationValue&) = default;
    };

    /// Stable subsystem-owned reference encoded without importing that subsystem.
    /// Examples are `Kairo.Assets.Mesh` + UUID text or `Kairo.Engine.Entity` +
    /// a stable entity identifier. An empty Identifier represents a null reference.
    struct ReferenceValue final
    {
        std::string TargetType;
        std::string Identifier;
        friend bool operator==(const ReferenceValue&, const ReferenceValue&) = default;
    };

    struct EnumOption final
    {
        std::int64_t Value = 0;
        std::string Key;
        std::string DisplayName;
        friend bool operator==(const EnumOption&, const EnumOption&) = default;
    };

    enum class PropertyValueKind : std::uint8_t
    {
        Boolean,
        SignedInteger,
        UnsignedInteger,
        FloatingPoint,
        String,
        Vector2,
        Vector3,
        Vector4,
        Quaternion,
        Enumeration,
        Reference
    };

    enum class PropertyFlags : std::uint32_t
    {
        None = 0u,
        ReadOnly = 1u << 0u,
        Multiline = 1u << 1u,
        Advanced = 1u << 2u
    };

    [[nodiscard]] constexpr PropertyFlags operator|(PropertyFlags left, PropertyFlags right) noexcept
    {
        return static_cast<PropertyFlags>(static_cast<std::uint32_t>(left) | static_cast<std::uint32_t>(right));
    }

    [[nodiscard]] constexpr bool HasFlag(PropertyFlags flags, PropertyFlags flag) noexcept
    {
        return (static_cast<std::uint32_t>(flags) & static_cast<std::uint32_t>(flag)) != 0u;
    }

    /// An owned, UI-neutral value. Composite records deliberately use doubles so
    /// editor/document transports do not need the concrete engine math scalar type.
    class PropertyValue final
    {
    public:
        using Storage = std::variant<
            bool,
            std::int64_t,
            std::uint64_t,
            double,
            std::string,
            Vector2Value,
            Vector3Value,
            Vector4Value,
            QuaternionValue,
            EnumerationValue,
            ReferenceValue>;

        PropertyValue(bool value) : m_Value(value) {}
        PropertyValue(std::int64_t value) : m_Value(value) {}
        PropertyValue(std::uint64_t value) : m_Value(value) {}
        PropertyValue(double value) : m_Value(value) { RequireFinite(value); }
        PropertyValue(std::string value) : m_Value(std::move(value)) {}
        PropertyValue(const char* value) : PropertyValue(std::string(value == nullptr ? "" : value)) {}
        PropertyValue(Vector2Value value) : m_Value(value)
        {
            RequireFinite(value.X); RequireFinite(value.Y);
        }
        PropertyValue(Vector3Value value) : m_Value(value)
        {
            RequireFinite(value.X); RequireFinite(value.Y); RequireFinite(value.Z);
        }
        PropertyValue(Vector4Value value) : m_Value(value)
        {
            RequireFinite(value.X); RequireFinite(value.Y); RequireFinite(value.Z); RequireFinite(value.W);
        }
        PropertyValue(QuaternionValue value) : m_Value(value)
        {
            RequireFinite(value.X); RequireFinite(value.Y); RequireFinite(value.Z); RequireFinite(value.W);
        }
        PropertyValue(EnumerationValue value) : m_Value(std::move(value))
        {
            const auto& stored = std::get<EnumerationValue>(m_Value);
            if (!IsStableKey(stored.Key))
                throw std::invalid_argument("Reflection enumeration keys must be stable dotted ASCII identifiers.");
        }
        PropertyValue(ReferenceValue value) : m_Value(std::move(value))
        {
            const auto& stored = std::get<ReferenceValue>(m_Value);
            if (!IsStableKey(stored.TargetType))
                throw std::invalid_argument("Reflection reference target type must be a stable dotted ASCII identifier.");
            if (stored.Identifier.size() > 4096u)
                throw std::length_error("Reflection reference identifier exceeds 4096 bytes.");
            for (const unsigned char character : stored.Identifier)
                if (character < 0x20u || character == 0x7Fu)
                    throw std::invalid_argument("Reflection reference identifiers cannot contain control bytes.");
        }

        [[nodiscard]] PropertyValueKind Kind() const noexcept
        {
            return static_cast<PropertyValueKind>(m_Value.index());
        }

        template<typename Value>
        [[nodiscard]] const Value& Get() const
        {
            return std::get<Value>(m_Value);
        }

        [[nodiscard]] friend bool operator==(const PropertyValue& left, const PropertyValue& right) noexcept
        {
            return left.m_Value == right.m_Value;
        }

    private:
        static void RequireFinite(double value)
        {
            if (!std::isfinite(value))
                throw std::invalid_argument("Reflection floating-point values must be finite.");
        }

        Storage m_Value;
    };

    /// Inclusive numeric editor range. It is applied only to scalar numeric value
    /// kinds; component-wise constraints belong to the owning component validator.
    struct NumericRange final
    {
        double Minimum = 0.0;
        double Maximum = 0.0;
        double Step = 0.0;
    };

    /// Stable property-facing metadata. Existing aggregate initializers remain
    /// source-compatible because V2 enum/reference metadata is appended.
    struct PropertyMetadata final
    {
        std::string Key;
        std::string DisplayName;
        std::string Category = "General";
        std::string Tooltip;
        PropertyFlags Flags = PropertyFlags::None;
        std::optional<NumericRange> Range;
        std::size_t MaximumStringBytes = 0u;
        std::vector<EnumOption> EnumOptions;
        std::string ReferenceTargetType;
        std::size_t MaximumReferenceBytes = 0u;
    };

    template<typename Value>
    concept ReflectablePrimitive = std::same_as<std::remove_cvref_t<Value>, bool> ||
        (std::integral<std::remove_cvref_t<Value>> && std::is_signed_v<std::remove_cvref_t<Value>> &&
            !std::same_as<std::remove_cvref_t<Value>, bool>) ||
        (std::integral<std::remove_cvref_t<Value>> && std::is_unsigned_v<std::remove_cvref_t<Value>>) ||
        std::floating_point<std::remove_cvref_t<Value>> ||
        std::same_as<std::remove_cvref_t<Value>, std::string>;

    template<ReflectablePrimitive Value>
    [[nodiscard]] constexpr PropertyValueKind PropertyKindOf() noexcept
    {
        using Clean = std::remove_cvref_t<Value>;
        if constexpr (std::same_as<Clean, bool>) return PropertyValueKind::Boolean;
        else if constexpr (std::integral<Clean> && std::is_signed_v<Clean>) return PropertyValueKind::SignedInteger;
        else if constexpr (std::integral<Clean>) return PropertyValueKind::UnsignedInteger;
        else if constexpr (std::floating_point<Clean>) return PropertyValueKind::FloatingPoint;
        else return PropertyValueKind::String;
    }

    template<ReflectablePrimitive Value>
    [[nodiscard]] inline PropertyValue EncodePropertyValue(const Value& value)
    {
        using Clean = std::remove_cvref_t<Value>;
        if constexpr (std::same_as<Clean, bool>) return PropertyValue(value);
        else if constexpr (std::integral<Clean> && std::is_signed_v<Clean>) return PropertyValue(static_cast<std::int64_t>(value));
        else if constexpr (std::integral<Clean>) return PropertyValue(static_cast<std::uint64_t>(value));
        else if constexpr (std::floating_point<Clean>) return PropertyValue(static_cast<double>(value));
        else return PropertyValue(value);
    }

    template<ReflectablePrimitive Value>
    [[nodiscard]] inline Value DecodePropertyValue(const PropertyValue& value)
    {
        using Clean = std::remove_cvref_t<Value>;
        if constexpr (std::same_as<Clean, bool>) return value.Get<bool>();
        else if constexpr (std::integral<Clean> && std::is_signed_v<Clean>)
        {
            const std::int64_t source = value.Get<std::int64_t>();
            if (source < static_cast<std::int64_t>(std::numeric_limits<Clean>::min()) ||
                source > static_cast<std::int64_t>(std::numeric_limits<Clean>::max()))
                throw std::out_of_range("Reflected signed integer does not fit its destination type.");
            return static_cast<Clean>(source);
        }
        else if constexpr (std::integral<Clean>)
        {
            const std::uint64_t source = value.Get<std::uint64_t>();
            if (source > static_cast<std::uint64_t>(std::numeric_limits<Clean>::max()))
                throw std::out_of_range("Reflected unsigned integer does not fit its destination type.");
            return static_cast<Clean>(source);
        }
        else if constexpr (std::floating_point<Clean>)
        {
            const double source = value.Get<double>();
            if (source < -static_cast<double>(std::numeric_limits<Clean>::max()) ||
                source > static_cast<double>(std::numeric_limits<Clean>::max()))
                throw std::out_of_range("Reflected floating-point value does not fit its destination type.");
            return static_cast<Clean>(source);
        }
        else return value.Get<std::string>();
    }
}
