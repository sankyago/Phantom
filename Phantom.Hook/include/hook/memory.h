#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace phantom::hook {

// ---------------------------------------------------------------------------
// MemoryError
// ---------------------------------------------------------------------------

enum class MemoryError {
    NullPointer,
    OutOfBounds,
    ModuleNotFound,
    InvalidPEHeader,
};

[[nodiscard]] std::string_view memory_error_to_string(MemoryError error) noexcept;

// ---------------------------------------------------------------------------
// MemoryHandle
// ---------------------------------------------------------------------------

class MemoryHandle {
public:
    constexpr MemoryHandle() noexcept : address_(0) {}

    constexpr explicit MemoryHandle(uintptr_t address) noexcept : address_(address) {}

    explicit MemoryHandle(void* ptr) noexcept
        : address_(reinterpret_cast<uintptr_t>(ptr)) {}

    [[nodiscard]] constexpr uintptr_t address() const noexcept { return address_; }

    [[nodiscard]] constexpr bool valid() const noexcept { return address_ != 0; }

    template <typename T>
    [[nodiscard]] T as() const noexcept {
        return reinterpret_cast<T>(address_);
    }

    [[nodiscard]] constexpr MemoryHandle add(ptrdiff_t offset) const noexcept {
        return MemoryHandle(static_cast<uintptr_t>(static_cast<ptrdiff_t>(address_) + offset));
    }

    [[nodiscard]] constexpr MemoryHandle sub(ptrdiff_t offset) const noexcept {
        return MemoryHandle(static_cast<uintptr_t>(static_cast<ptrdiff_t>(address_) - offset));
    }

    /// RIP-relative resolution: reads an int32_t at the current address and
    /// returns address + offset + 4.
    [[nodiscard]] MemoryHandle rip() const noexcept {
        auto offset = *reinterpret_cast<const int32_t*>(address_);
        return MemoryHandle(address_ + static_cast<uintptr_t>(offset) + 4);
    }

    constexpr auto operator<=>(const MemoryHandle&) const noexcept = default;
    constexpr bool operator==(const MemoryHandle&) const noexcept = default;

private:
    uintptr_t address_;
};

// ---------------------------------------------------------------------------
// MemoryRegion
// ---------------------------------------------------------------------------

class MemoryRegion {
public:
    constexpr MemoryRegion() noexcept : base_(), size_(0) {}

    constexpr MemoryRegion(MemoryHandle base, size_t size) noexcept
        : base_(base), size_(size) {}

    [[nodiscard]] constexpr MemoryHandle base() const noexcept { return base_; }
    [[nodiscard]] constexpr size_t size() const noexcept { return size_; }

    [[nodiscard]] constexpr bool contains(MemoryHandle handle) const noexcept {
        auto addr = handle.address();
        auto start = base_.address();
        return addr >= start && addr < start + size_;
    }

    [[nodiscard]] std::span<const uint8_t> as_bytes() const noexcept {
        return { base_.as<const uint8_t*>(), size_ };
    }

private:
    MemoryHandle base_;
    size_t size_;
};

// ---------------------------------------------------------------------------
// Module
// ---------------------------------------------------------------------------

class Module {
public:
    [[nodiscard]] static std::expected<Module, MemoryError> from_name(std::string_view name) noexcept;

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    [[nodiscard]] MemoryHandle base() const noexcept { return base_; }
    [[nodiscard]] size_t size() const noexcept { return size_; }
    [[nodiscard]] MemoryRegion region() const noexcept { return { base_, size_ }; }

    [[nodiscard]] std::expected<MemoryHandle, MemoryError> get_export(std::string_view export_name) const noexcept;

private:
    Module(std::string name, MemoryHandle base, size_t size) noexcept
        : name_(static_cast<std::string&&>(name)), base_(base), size_(size) {}

    std::string name_;
    MemoryHandle base_;
    size_t size_;
};

} // namespace phantom::hook
