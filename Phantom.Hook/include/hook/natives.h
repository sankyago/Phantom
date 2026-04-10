#pragma once

#include "hook/memory.h"

#include <cstdint>
#include <expected>
#include <span>

namespace phantom::hook {

// ---------------------------------------------------------------------------
// NativeError
// ---------------------------------------------------------------------------

enum class NativeError {
    NotInitialized,
    NativeNotFound,
    InvocationFailed,
};

[[nodiscard]] const char* native_error_to_string(NativeError error);

// ---------------------------------------------------------------------------
// NativeContext
// ---------------------------------------------------------------------------

struct NativeContext {
    static constexpr size_t MAX_ARGS = 32;

    uint64_t args[MAX_ARGS] = {};
    uint32_t arg_count = 0;
    uint64_t result[4] = {};

    void reset() {
        arg_count = 0;
    }

    template <typename T>
    void push(T value) {
        static_assert(sizeof(T) <= sizeof(uint64_t));
        if (arg_count >= MAX_ARGS)
            return;
        *reinterpret_cast<T*>(&args[arg_count]) = value;
        ++arg_count;
    }

    template <typename T>
    [[nodiscard]] T get_result() const {
        static_assert(sizeof(T) <= sizeof(uint64_t));
        return *reinterpret_cast<const T*>(&result[0]);
    }
};

// ---------------------------------------------------------------------------
// NativeHandler
// ---------------------------------------------------------------------------

using NativeHandler = void (*)(NativeContext*);

// ---------------------------------------------------------------------------
// NativeInvoker
// ---------------------------------------------------------------------------

class NativeInvoker {
public:
    [[nodiscard]] static std::expected<NativeInvoker, NativeError>
    create(MemoryHandle registration_table, size_t table_size);

    [[nodiscard]] std::expected<NativeHandler, NativeError>
    find_native(uint64_t hash) const;

    template <typename Ret, typename... Args>
    [[nodiscard]] std::expected<Ret, NativeError>
    invoke(uint64_t hash, Args... args) const {
        auto handler = find_native(hash);
        if (!handler.has_value())
            return std::unexpected(handler.error());

        NativeContext ctx;
        (ctx.push(args), ...);
        (*handler)(&ctx);
        return ctx.get_result<Ret>();
    }

    template <typename... Args>
    [[nodiscard]] std::expected<void, NativeError>
    invoke_void(uint64_t hash, Args... args) const {
        auto handler = find_native(hash);
        if (!handler.has_value())
            return std::unexpected(handler.error());

        NativeContext ctx;
        (ctx.push(args), ...);
        (*handler)(&ctx);
        return {};
    }

private:
    NativeInvoker(MemoryHandle table, size_t size);

    MemoryHandle table_;
    size_t table_size_ = 0;
};

} // namespace phantom::hook
