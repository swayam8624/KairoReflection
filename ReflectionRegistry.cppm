module;

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
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
            if (property.ValueKind != PropertyValueKind::SignedInteger &&
                property.ValueKind != PropertyValueKind::UnsignedInteger &&
                property.ValueKind != PropertyValueKind::FloatingPoint)
                throw std::invalid_argument("Only numeric reflection properties may declare a range.");
            if (range.Minimum > range.Maximum || range.Step < 0.0)
                throw std::invalid_argument("Reflection numeric range is invalid.");
        }
        if (property.ValueKind != PropertyValueKind::String && property.Metadata.MaximumStringBytes != 0u)
            throw std::invalid_argument("Only string reflection properties may declare a string byte limit.");
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
            if (property.Metadata.MaximumStringBytes != 0u &&
                value.Get<std::string>().size() > property.Metadata.MaximumStringBytes)
                throw std::length_error("Reflection string value exceeds its configured byte limit.");
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
    /// provide inspector-ready adapters without runtime RTTI or UI coupling.
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
}

