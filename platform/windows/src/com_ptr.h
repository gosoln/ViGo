#pragma once

#include <utility>

namespace nakl::windows
{

template <typename Interface> class ComPtr final
{
  public:
    ComPtr() = default;
    ~ComPtr()
    {
        Reset();
    }

    ComPtr(const ComPtr &) = delete;
    ComPtr &operator=(const ComPtr &) = delete;

    ComPtr(ComPtr &&other) noexcept : pointer_(std::exchange(other.pointer_, nullptr))
    {
    }

    ComPtr &operator=(ComPtr &&other) noexcept
    {
        if (this != &other)
        {
            Reset();
            pointer_ = std::exchange(other.pointer_, nullptr);
        }
        return *this;
    }

    [[nodiscard]] Interface *Get() const noexcept
    {
        return pointer_;
    }

    [[nodiscard]] Interface **Put() noexcept
    {
        Reset();
        return &pointer_;
    }

    [[nodiscard]] Interface *operator->() const noexcept
    {
        return pointer_;
    }

    void Reset(Interface *replacement = nullptr) noexcept
    {
        if (pointer_ != nullptr)
        {
            pointer_->Release();
        }
        pointer_ = replacement;
    }

  private:
    Interface *pointer_ = nullptr;
};

} // namespace nakl::windows
