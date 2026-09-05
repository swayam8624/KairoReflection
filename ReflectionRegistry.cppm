module;

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

export module Kairo.Reflection.Registry;

import Kairo.Reflection.Types;

export namespace kairo::reflection
{
    /// One property accessor for a concrete reflected type. The registry owns
    /// no object memory; callers retain lifetime and pass the exact object
    /// address associated with a descriptor when reading or writing fields.
    struct PropertyDescriptor final
    {
        PropertyMetadata Metadata;
        PropertyValueKind ValueKind = PropertyValueKind::Boolean;
        std::function<PropertyValue(const void*)> Read;
        std::function<void(void*, const PropertyValue&)> Write;
    };

    struct TypeDescriptor final
    {
        std::string Key;
        std::string DisplayName;
        std::string Category;
        std::vector<PropertyDescriptor> Properties;
    };

    namespace detail
    {
        [[nodiscard]] inline bool IsScalarNumeric(PropertyValueKind kind) noexcept
        {
            return kind == PropertyValueKind::SignedInteger ||
                kind == PropertyValueKind::UnsignedInteger ||
                kind == PropertyValueKind::FloatingPoint;
        }

        inline void ValidateEnumOptions(const PropertyDescriptor& property)
        {
            if (property.ValueKind != PropertyValueKind::Enumeration)
            {
                if (!property.Metadata.EnumOptions.empty())
                    throw std::invalid_argument("Only enumeration reflection properties may declare enum options.");
                return;
            }

            if (property.Metadata.EnumOptions.empty())
                throw std::invalid_argument("Enumeration reflection properties require at least one option.");
            if (property.Metadata.EnumOptions.size() > 512u)
                throw std::length_error("Reflection enumeration exceeds 512 options.");

            for (std::size_t index = 0; index < property.Metadata.EnumOptions.size(); ++index)
            {
                const EnumOption& option = property.Metadata.EnumOptions[index];
                if (!IsStableKey(option.Key))
                    throw std::invalid_argument("Reflection enum option key must be a stable dotted ASCII identifier.");
                if (option.DisplayName.empty() || option.DisplayName.size() > 128u)
                    throw std::invalid_argument("Reflection enum option display name must contain 1 to 128 bytes.");

                for (std::size_t previous = 0; previous < index; ++previous)
                {
                    const EnumOption& other = property.Metadata.EnumOptions[previous];
                    if (other.Key == option.Key)
                        throw std::invalid_argument("Reflection enumeration contains duplicate option key: " + option.Key);
                    if (other.Value == option.Value)
                        throw std::invalid_argument("Reflection enumeration contains duplicate numeric option value.");
                }
            }
        }

        inline void ValidateReferenceMetadata(const PropertyDescriptor& property)
        {
            if (property.ValueKind == PropertyValueKind::Reference)
            {
                if (!IsStableKey(property.Metadata.ReferenceTargetType))
                    throw std::invalid_argument("Reflection reference properties require a stable target type key.");
                if (property.Metadata.MaximumReferenceBytes > 4096u)
                    throw std::length_error("Reflection reference byte limit cannot exceed 4096.");
                return;
            }

            if (!property.Metadata.ReferenceTargetType.empty() || property.Metadata.MaximumReferenceBytes != 0u)
                throw std::invalid_argument("Only reference reflection properties may declare reference metadata.");
        }
    }

    inline void ValidatePropertyDescriptor(const PropertyDescriptor& property)
    {
        if (!IsStableKey(property.Metadata.Key))
            throw std::invalid_argument("Reflection property key must be a stable dotted ASCII identifier.");
        if (property.Metadata.DisplayName.empty() || property.Metadata.DisplayName.size() > 128u)
            throw std::invalid_argument("Reflection property display name must contain 1 to 128 bytes.");
        if (property.Metadata.Category.empty() || property.Metadata.Category.size() > 128u)
            throw std::invalid_argument("Reflection property category must contain 1 to 128 bytes.");
        if (!property.Read) throw std::invalid_argument("Reflection property requires a read accessor.");
        if (!HasFlag(property.Metadata.Flags, PropertyFlags::ReadOnly) && !property.Write)
            throw std::invalid_argument("Writable reflection property requires a write accessor.");
        if (property.Metadata.Range.has_value())
        {
            const NumericRange range = *property.Metadata.Range;
            if (!detail::IsScalarNumeric(property.ValueKind))
                throw std::invalid_argument("Only scalar numeric reflection properties may declare a range.");
            if (range.Minimum > range.Maximum || range.Step < 0.0)
                throw std::invalid_argument("Reflection numeric range is invalid.");
        }
        if (property.ValueKind != PropertyValueKind::String && property.Metadata.MaximumStringBytes != 0u)
            throw std::invalid_argument("Only string reflection properties may declare a string byte limit.");

        detail::ValidateEnumOptions(property);
        detail::ValidateReferenceMetadata(property);
    }

    inline void ValidateTypeDescriptor(const TypeDescriptor& type)
    {
        if (!IsStableKey(type.Key)) throw std::invalid_argument("Reflection type key must be a stable dotted ASCII identifier.");
        if (type.DisplayName.empty() || type.DisplayName.size() > 128u)
            throw std::invalid_argument("Reflection type display name must contain 1 to 128 bytes.");
        if (type.Category.empty() || type.Category.size() > 128u)
            throw std::invalid_argument("Reflection type category must contain 1 to 128 bytes.");
        if (type.Properties.size() > 1024u) throw std::length_error("Reflection type exceeds 1024 properties.");
        std::vector<std::string_view> keys;
        keys.reserve(type.Properties.size());
        for (const PropertyDescriptor& property : type.Properties)
        {
            ValidatePropertyDescriptor(property);
            if (std::ranges::find(keys, property.Metadata.Key) != keys.end())
                throw std::invalid_argument("Reflection type contains duplicate property key: " + property.Metadata.Key);
            keys.push_back(property.Metadata.Key);
        }
    }

    /// Deterministic metadata registry. Types are immutable after registration,
    /// which prevents an already-open inspector or serialized document from
    /// changing meaning as plugins load in a different order.
    class ReflectionRegistry final
    {
    public:
        void Register(TypeDescriptor type)
        {
            ValidateTypeDescriptor(type);
            const std::string key = type.Key;
            std::ranges::sort(type.Properties, [](const PropertyDescriptor& left, const PropertyDescriptor& right)
            {
                return left.Metadata.Key < right.Metadata.Key;
            });
            if (!m_Types.emplace(key, std::move(type)).second)
                throw std::invalid_argument("Reflection type is already registered: " + key);
        }

        [[nodiscard]] bool Contains(std::string_view key) const noexcept
        {
            return m_Types.find(key) != m_Types.end();
        }

        [[nodiscard]] const TypeDescriptor& Require(std::string_view key) const
        {
            const auto found = m_Types.find(key);
            if (found == m_Types.end()) throw std::out_of_range("Unknown reflection type: " + std::string(key));
            return found->second;
        }

        [[nodiscard]] std::vector<std::reference_wrapper<const TypeDescriptor>> Snapshot() const
        {
            std::vector<std::reference_wrapper<const TypeDescriptor>> result;
            result.reserve(m_Types.size());
            for (const auto& [key, type] : m_Types)
            {
                (void)key;
                result.emplace_back(type);
            }
            return result;
        }

        [[nodiscard]] PropertyValue Read(std::string_view typeKey, std::string_view propertyKey, const void* object) const
        {
            if (object == nullptr) throw std::invalid_argument("Reflection read requires a non-null object.");
            const PropertyDescriptor& property = RequireProperty(Require(typeKey), propertyKey);
            PropertyValue value = property.Read(object);
            if (value.Kind() != property.ValueKind)
                throw std::logic_error("Reflection accessor returned a value with an unexpected kind.");
            ValidateValue(property, value);
            return value;
        }

        void Write(std::string_view typeKey, std::string_view propertyKey, void* object, const PropertyValue& value) const
        {
            if (object == nullptr) throw std::invalid_argument("Reflection write requires a non-null object.");
            const PropertyDescriptor& property = RequireProperty(Require(typeKey), propertyKey);
            if (HasFlag(property.Metadata.Flags, PropertyFlags::ReadOnly))
                throw std::logic_error("Reflection property is read-only: " + property.Metadata.Key);
            if (value.Kind() != property.ValueKind)
                throw std::invalid_argument("Reflection value kind does not match property: " + property.Metadata.Key);
            ValidateValue(property, value);
            property.Write(object, value);
        }

        [[nodiscard]] std::vector<std::reference_wrapper<const TypeDescriptor>> Search(std::string_view query) const
        {
            std::string normalized = LowerAscii(query);
            std::vector<std::reference_wrapper<const TypeDescriptor>> result;
            for (const auto& [key, type] : m_Types)
            {
                const std::string searchable = LowerAscii(key + " " + type.DisplayName + " " + type.Category);
                if (normalized.empty() || searchable.contains(normalized)) result.emplace_back(type);
            }
            return result;
        }

        [[nodiscard]] std::size_t Size() const noexcept { return m_Types.size(); }

    private:
        std::map<std::string, TypeDescriptor, std::less<>> m_Types;

        [[nodiscard]] static const PropertyDescriptor& RequireProperty(const TypeDescriptor& type, std::string_view key)
        {
            const auto found = std::ranges::find_if(type.Properties, [key](const PropertyDescriptor& property)
            {
                return property.Metadata.Key == key;
            });
            if (found == type.Properties.end())
                throw std::out_of_range("Unknown reflection property: " + std::string(key));
            return *found;
        }

        static void ValidateValue(const PropertyDescriptor& property, const PropertyValue& value)
        {
            if (property.ValueKind == PropertyValueKind::String && property.Metadata.MaximumStringBytes != 0u &&
                value.Get<std::string>().size() > property.Metadata.MaximumStringBytes)
                throw std::length_error("Reflection string value exceeds its configured byte limit.");

            if (property.ValueKind == PropertyValueKind::Enumeration)
            {
                const EnumerationValue& enumeration = value.Get<EnumerationValue>();
                const auto found = std::ranges::find_if(property.Metadata.EnumOptions,
                    [&enumeration](const EnumOption& option)
                    {
                        return option.Value == enumeration.Value && option.Key == enumeration.Key;
                    });
                if (found == property.Metadata.EnumOptions.end())
                    throw std::out_of_range("Reflection enumeration value is not a registered option.");
            }

            if (property.ValueKind == PropertyValueKind::Reference)
            {
                const ReferenceValue& reference = value.Get<ReferenceValue>();
                if (reference.TargetType != property.Metadata.ReferenceTargetType)
                    throw std::invalid_argument("Reflection reference target type does not match the property contract.");
                if (property.Metadata.MaximumReferenceBytes != 0u &&
                    reference.Identifier.size() > property.Metadata.MaximumReferenceBytes)
                    throw std::length_error("Reflection reference identifier exceeds its configured byte limit.");
            }

            if (!property.Metadata.Range.has_value()) return;
            const NumericRange range = *property.Metadata.Range;
            double numeric = 0.0;
            switch (property.ValueKind)
            {
                case PropertyValueKind::SignedInteger: numeric = static_cast<double>(value.Get<std::int64_t>()); break;
                case PropertyValueKind::UnsignedInteger: numeric = static_cast<double>(value.Get<std::uint64_t>()); break;
                case PropertyValueKind::FloatingPoint: numeric = value.Get<double>(); break;
                default: throw std::logic_error("Non-numeric reflection value was given a numeric range.");
            }
            if (numeric < range.Minimum || numeric > range.Maximum)
                throw std::out_of_range("Reflection value lies outside its configured range.");
        }

        [[nodiscard]] static std::string LowerAscii(std::string_view value)
        {
            std::string result(value);
            for (char& character : result)
                if (character >= 'A' && character <= 'Z') character = static_cast<char>(character + ('a' - 'A'));
            return result;
        }
    };

    /// Input: concrete object/member type and semantic metadata. Output: a
    /// descriptor safe to register under that object's TypeDescriptor. Task:
    /// provide inspector-ready primitive adapters without runtime RTTI or UI coupling.
    template<typename Object, ReflectablePrimitive Member>
    [[nodiscard]] PropertyDescriptor MakeMemberProperty(PropertyMetadata metadata, Member Object::* member)
    {
        if (member == nullptr) throw std::invalid_argument("Reflection member property requires a valid member pointer.");
        PropertyDescriptor descriptor;
        descriptor.Metadata = std::move(metadata);
        descriptor.ValueKind = PropertyKindOf<Member>();
        descriptor.Read = [member](const void* object)
        {
            return EncodePropertyValue(static_cast<const Object*>(object)->*member);
        };
        if (!HasFlag(descriptor.Metadata.Flags, PropertyFlags::ReadOnly))
        {
            descriptor.Write = [member](void* object, const PropertyValue& value)
            {
                static_cast<Object*>(object)->*member = DecodePropertyValue<Member>(value);
            };
        }
        return descriptor;
    }

    /// Generic bridge for subsystem-owned composite types. Reflection owns the
    /// canonical PropertyValue record while the subsystem supplies lossless
    /// encode/decode functions for its concrete math/reference type.
    template<typename Object, typename Member, typename Encoder, typename Decoder>
    [[nodiscard]] PropertyDescriptor MakeAdaptedMemberProperty(
        PropertyMetadata metadata,
        Member Object::* member,
        PropertyValueKind valueKind,
        Encoder encoder,
        Decoder decoder)
    {
        if (member == nullptr) throw std::invalid_argument("Reflection adapted member property requires a valid member pointer.");
        PropertyDescriptor descriptor;
        descriptor.Metadata = std::move(metadata);
        descriptor.ValueKind = valueKind;
        descriptor.Read = [member, encoder = std::move(encoder)](const void* object) mutable
        {
            return std::invoke(encoder, static_cast<const Object*>(object)->*member);
        };
        if (!HasFlag(descriptor.Metadata.Flags, PropertyFlags::ReadOnly))
        {
            descriptor.Write = [member, decoder = std::move(decoder)](void* object, const PropertyValue& value) mutable
            {
                static_cast<Object*>(object)->*member = std::invoke(decoder, value);
            };
        }
        return descriptor;
    }

    template<typename Object, typename Enum>
        requires std::is_enum_v<Enum>
    [[nodiscard]] PropertyDescriptor MakeEnumMemberProperty(
        PropertyMetadata metadata,
        Enum Object::* member)
    {
        if (member == nullptr) throw std::invalid_argument("Reflection enum member property requires a valid member pointer.");
        const std::vector<EnumOption> options = metadata.EnumOptions;
        PropertyDescriptor descriptor;
        descriptor.Metadata = std::move(metadata);
        descriptor.ValueKind = PropertyValueKind::Enumeration;
        descriptor.Read = [member, options](const void* object)
        {
            using Underlying = std::underlying_type_t<Enum>;
            const Underlying raw = static_cast<Underlying>(static_cast<const Object*>(object)->*member);
            std::int64_t numeric = 0;
            if constexpr (std::is_unsigned_v<Underlying>)
            {
                if (static_cast<std::uint64_t>(raw) > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
                    throw std::out_of_range("Reflected enumeration value cannot fit the canonical signed representation.");
                numeric = static_cast<std::int64_t>(raw);
            }
            else numeric = static_cast<std::int64_t>(raw);

            const auto found = std::ranges::find_if(options, [numeric](const EnumOption& option) { return option.Value == numeric; });
            if (found == options.end())
                throw std::logic_error("Reflected enum object contains a value absent from its metadata options.");
            return PropertyValue(EnumerationValue{ found->Value, found->Key });
        };
        if (!HasFlag(descriptor.Metadata.Flags, PropertyFlags::ReadOnly))
        {
            descriptor.Write = [member, options](void* object, const PropertyValue& value)
            {
                const EnumerationValue& requested = value.Get<EnumerationValue>();
                const auto found = std::ranges::find_if(options, [&requested](const EnumOption& option)
                {
                    return option.Value == requested.Value && option.Key == requested.Key;
                });
                if (found == options.end())
                    throw std::out_of_range("Reflected enumeration value is not a registered option.");

                using Underlying = std::underlying_type_t<Enum>;
                if constexpr (std::is_unsigned_v<Underlying>)
                {
                    if (found->Value < 0 || static_cast<std::uint64_t>(found->Value) >
                        static_cast<std::uint64_t>(std::numeric_limits<Underlying>::max()))
                        throw std::out_of_range("Reflected enumeration value does not fit its destination type.");
                }
                else if (found->Value < static_cast<std::int64_t>(std::numeric_limits<Underlying>::min()) ||
                    found->Value > static_cast<std::int64_t>(std::numeric_limits<Underlying>::max()))
                    throw std::out_of_range("Reflected enumeration value does not fit its destination type.");

                static_cast<Object*>(object)->*member = static_cast<Enum>(static_cast<Underlying>(found->Value));
            };
        }
        return descriptor;
    }

    template<typename Object, typename Member, typename Encoder, typename Decoder>
    [[nodiscard]] PropertyDescriptor MakeReferenceMemberProperty(
        PropertyMetadata metadata,
        Member Object::* member,
        std::string targetType,
        Encoder encodeIdentifier,
        Decoder decodeIdentifier)
    {
        if (!IsStableKey(targetType))
            throw std::invalid_argument("Reflection reference adapter target type must be a stable key.");
        metadata.ReferenceTargetType = targetType;
        return MakeAdaptedMemberProperty<Object, Member>(
            std::move(metadata), member, PropertyValueKind::Reference,
            [targetType, encodeIdentifier = std::move(encodeIdentifier)](const Member& value) mutable
            {
                return PropertyValue(ReferenceValue{ targetType, std::invoke(encodeIdentifier, value) });
            },
            [targetType, decodeIdentifier = std::move(decodeIdentifier)](const PropertyValue& value) mutable
            {
                const ReferenceValue& reference = value.Get<ReferenceValue>();
                if (reference.TargetType != targetType)
                    throw std::invalid_argument("Reflection reference adapter received the wrong target type.");
                return std::invoke(decodeIdentifier, reference.Identifier);
            });
    }
}
