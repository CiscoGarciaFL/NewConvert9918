#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace retrovdp::core {

enum class StableIdError {
    None,
    Empty,
    TooLong,
    InvalidFirstCharacter,
    InvalidCharacter,
    ConsecutiveSeparator,
    TrailingSeparator,
};

template <typename Tag>
class BasicStableId final {
public:
    static constexpr std::size_t maximumSize = 64;

    [[nodiscard]] static std::optional<BasicStableId>
    create(std::string_view value, StableIdError* error = nullptr)
    {
        const auto fail = [error](StableIdError value) {
            if (error != nullptr) *error = value;
            return std::optional<BasicStableId>{};
        };

        if (value.empty()) return fail(StableIdError::Empty);
        if (value.size() > maximumSize) return fail(StableIdError::TooLong);
        if (!isAlphaNumeric(value.front())) {
            return fail(StableIdError::InvalidFirstCharacter);
        }

        bool previousSeparator = false;
        for (const char character : value) {
            if (character == '-') {
                if (previousSeparator) return fail(StableIdError::ConsecutiveSeparator);
                previousSeparator = true;
                continue;
            }
            if (!isAlphaNumeric(character)) return fail(StableIdError::InvalidCharacter);
            previousSeparator = false;
        }
        if (previousSeparator) return fail(StableIdError::TrailingSeparator);

        if (error != nullptr) *error = StableIdError::None;
        return BasicStableId(std::string(value));
    }

    [[nodiscard]] const std::string& value() const { return value_; }
    [[nodiscard]] const char* data() const { return value_.data(); }
    [[nodiscard]] std::size_t size() const { return value_.size(); }
    [[nodiscard]] bool empty() const { return value_.empty(); }
    [[nodiscard]] operator std::string_view() const { return value_; }

    [[nodiscard]] friend bool operator==(const BasicStableId&, const BasicStableId&) = default;
    [[nodiscard]] friend bool operator==(const BasicStableId& left, std::string_view right)
    {
        return left.value_ == right;
    }
    [[nodiscard]] friend bool operator==(std::string_view left, const BasicStableId& right)
    {
        return right == left;
    }

private:
    explicit BasicStableId(std::string value)
        : value_(std::move(value))
    {
    }

    [[nodiscard]] static constexpr bool isAlphaNumeric(char value)
    {
        return (value >= 'a' && value <= 'z') || (value >= '0' && value <= '9');
    }

    std::string value_;
};

struct TargetIdTag;
struct ModeIdTag;
struct FormatIdTag;
struct RegionRoleIdTag;

using TargetId = BasicStableId<TargetIdTag>;
using ModeId = BasicStableId<ModeIdTag>;
using FormatId = BasicStableId<FormatIdTag>;
using RegionRoleId = BasicStableId<RegionRoleIdTag>;

struct StableIdHash {
    template <typename Tag>
    [[nodiscard]] std::size_t operator()(const BasicStableId<Tag>& id) const noexcept
    {
        return std::hash<std::string_view>{}(id.value());
    }
};

} // namespace retrovdp::core
