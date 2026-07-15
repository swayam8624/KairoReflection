module;

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

export module Kairo.Reflection.Types;

export namespace kairo::reflection
{
    /// Canonical scalar values that a reflection client may exchange. Complex
    /// engine objects remain references owned by their defining subsystem;
    /// their editor adapters can compose this primitive surface later.
    enum class PropertyValueKind : std::uint8_t
    {
        Boolean,
        SignedInteger,
        UnsignedInteger,
        FloatingPoint,
        String
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

    /// An owned, UI-neutral value. A property descriptor always states which
    /// alternative it accepts, so the variant never silently coerces values.
    class PropertyValue final
    {
    public:
        using Storage = std::variant<bool, std::int64_t, std::uint64_t, double, std::string>;

        PropertyValue(bool value) : m_Value(value) {}
        PropertyValue(std::int64_t value) : m_Value(value) {}
        PropertyValue(std::uint64_t value) : m_Value(value) {}
        PropertyValue(double value) : m_Value(value)
        {
            if (value != value || value == std::numeric_limits<double>::infinity() ||
                value == -std::numeric_limits<double>::infinity())
                throw std::invalid_argument("Reflection floating-point values must be finite.");
        }
        PropertyValue(std::string value) : m_Value(std::move(value)) {}
        PropertyValue(const char* value) : PropertyValue(std::string(value == nullptr ? "" : value)) {}

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
            if (left.m_Value.index() != right.m_Value.index()) return false;
            return std::visit([](const auto& first, const auto& second) noexcept
            {
                using First = std::remove_cvref_t<decltype(first)>;
                using Second = std::remove_cvref_t<decltype(second)>;
                if constexpr (std::same_as<First, Second>) return first == second;
                else return false;
            }, left.m_Value, right.m_Value);
        }

    private:
        Storage m_Value;
    };

    /// Inclusive numeric editor range. It is applied only to numeric value
    /// kinds; the actual object type still controls final conversion bounds.
    struct NumericRange final
    {
        double Minimum = 0.0;
        double Maximum = 0.0;
        double Step = 0.0;
    };

    /// Stable property-facing metadata. `Key` is serialized; display strings
    /// can change without invalidating documents or graph/property references.
    struct PropertyMetadata final
    {
        std::string Key;
        std::string DisplayName;
        std::string Category = "General";
        std::string Tooltip;
        PropertyFlags Flags = PropertyFlags::None;
        std::optional<NumericRange> Range;
        std::size_t MaximumStringBytes = 0u;
    };

    /// A type key is a stable dotted ASCII identifier such as
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
