#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace lego::ctr {

using Handle = std::uint32_t;
using Result = std::uint32_t;

inline constexpr Result kResultSuccess = 0x00000000U;
inline constexpr Result kResultInvalidHandle = 0xD8E007F7U;
inline constexpr Result kResultOutOfHandles = 0xD8600413U;

inline constexpr Handle kCurrentThreadPseudoHandle = 0xFFFF8000U;
inline constexpr Handle kCurrentProcessPseudoHandle = 0xFFFF8001U;

class KernelObject {
public:
    enum class Type : std::uint8_t {
        Process,
        Thread,
        Event,
        Mutex,
        Semaphore,
        Timer,
        SharedMemory,
        Session,
        Other,
    };

    explicit KernelObject(Type type) noexcept : type_(type) {}
    virtual ~KernelObject() = default;

    [[nodiscard]] Type type() const noexcept { return type_; }

private:
    Type type_;
};

class ProcessObject final : public KernelObject {
public:
    explicit ProcessObject(std::uint32_t process_id) noexcept
        : KernelObject(Type::Process), process_id(process_id) {}

    std::uint32_t process_id{};
};

class ThreadObject final : public KernelObject {
public:
    explicit ThreadObject(std::uint32_t thread_id) noexcept
        : KernelObject(Type::Thread), thread_id(thread_id) {}

    std::uint32_t thread_id{};
};

class GenericObject final : public KernelObject {
public:
    explicit GenericObject(Type type) noexcept : KernelObject(type) {}
};

class HandleTable final {
public:
    static constexpr std::size_t kMaxCount = 4096;

    HandleTable();

    void SetPseudoObjects(std::shared_ptr<KernelObject> current_thread,
                          std::shared_ptr<KernelObject> current_process) noexcept;

    Result Create(Handle* out_handle, std::shared_ptr<KernelObject> object) noexcept;
    Result Duplicate(Handle* out_handle, Handle handle) noexcept;
    Result Close(Handle handle) noexcept;

    [[nodiscard]] bool IsValid(Handle handle) const noexcept;
    [[nodiscard]] std::shared_ptr<KernelObject> Get(Handle handle) const noexcept;
    [[nodiscard]] std::size_t OpenHandleCount() const noexcept;

    void Clear() noexcept;

private:
    static constexpr std::uint16_t Slot(Handle handle) noexcept {
        return static_cast<std::uint16_t>(handle >> 15U);
    }

    static constexpr std::uint16_t Generation(Handle handle) noexcept {
        return static_cast<std::uint16_t>(handle & 0x7FFFU);
    }

    std::array<std::shared_ptr<KernelObject>, kMaxCount> objects_{};
    std::array<std::uint16_t, kMaxCount> generations_{};
    std::uint16_t next_generation_{1};
    std::uint16_t next_free_slot_{0};
    std::shared_ptr<KernelObject> current_thread_{};
    std::shared_ptr<KernelObject> current_process_{};
};

class Kernel final {
public:
    Kernel(std::uint32_t process_id = 1, std::uint32_t thread_id = 1);

    [[nodiscard]] HandleTable& handles() noexcept { return handles_; }
    [[nodiscard]] const HandleTable& handles() const noexcept { return handles_; }
    [[nodiscard]] const std::shared_ptr<ProcessObject>& current_process() const noexcept {
        return current_process_;
    }
    [[nodiscard]] const std::shared_ptr<ThreadObject>& current_thread() const noexcept {
        return current_thread_;
    }

    Result DuplicateHandle(Handle* out_handle, Handle handle) noexcept;
    Result CloseHandle(Handle handle) noexcept;

private:
    std::shared_ptr<ProcessObject> current_process_;
    std::shared_ptr<ThreadObject> current_thread_;
    HandleTable handles_;
};

}  // namespace lego::ctr
