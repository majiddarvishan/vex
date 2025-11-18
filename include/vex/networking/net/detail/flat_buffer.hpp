#pragma once

#include <boost/asio/buffer.hpp>

#include <algorithm>
#include <array>
#include <type_traits>
#include <stdexcept>
#include <cstring>

namespace pa::pinex::detail
{
template<typename T, std::size_t N>
class flat_buffer
{
    static_assert(std::is_trivially_copyable_v<T>,
                  "flat_buffer requires trivially copyable types");

    std::array<T, N> buf_{};

    T* in_{buf_.data()};
    T* out_{buf_.data()};
    T* last_{buf_.data()};

    // Verify pointers are within bounds
    bool is_valid() const noexcept
    {
        auto* begin = buf_.data();
        auto* end = buf_.data() + buf_.size();
        return in_ >= begin && in_ <= end &&
               out_ >= begin && out_ <= end &&
               last_ >= begin && last_ <= end &&
               in_ <= out_ && out_ <= last_;
    }

  public:
    flat_buffer() noexcept = default;

    // Copy constructor - can throw if prepare() fails
    flat_buffer(const flat_buffer& other)
    {
        // Store buffer_copy result explicitly to ensure proper sequencing:
        // 1. prepare() allocates space and returns mutable buffer
        // 2. buffer_copy() copies data and returns bytes copied
        // 3. commit() advances the write pointer by the copied amount
        // This avoids undefined behavior from unspecified argument evaluation order
        auto copied = boost::asio::buffer_copy(this->prepare(other.size()), other.data());
        this->commit(copied);
    }

    flat_buffer& operator=(const flat_buffer& other) noexcept
    {
        if (this == &other)
            return *this;
        // Use clear() instead of consume() to reset all pointers including last_
        this->clear();
        // Explicit variable for safe sequencing (see copy constructor comment)
        auto copied = boost::asio::buffer_copy(this->prepare(other.size()), other.data());
        this->commit(copied);
        return *this;
    }

    flat_buffer(flat_buffer&& other) noexcept
    {
        buf_ = std::move(other.buf_);

        // Recalculate pointers as offsets from new buffer location
        auto offset_in = other.in_ - other.buf_.data();
        auto offset_out = other.out_ - other.buf_.data();
        auto offset_last = other.last_ - other.buf_.data();

        in_ = buf_.data() + offset_in;
        out_ = buf_.data() + offset_out;
        last_ = buf_.data() + offset_last;

        other.clear();
    }

    flat_buffer& operator=(flat_buffer&& other) noexcept
    {
        if (this == &other)
            return *this;

        buf_ = std::move(other.buf_);

        auto offset_in = other.in_ - other.buf_.data();
        auto offset_out = other.out_ - other.buf_.data();
        auto offset_last = other.last_ - other.buf_.data();

        in_ = buf_.data() + offset_in;
        out_ = buf_.data() + offset_out;
        last_ = buf_.data() + offset_last;

        other.clear();
        return *this;
    }

    ~flat_buffer() = default;

    void clear() noexcept
    {
        in_ = buf_.data();
        out_ = buf_.data();
        last_ = buf_.data();
    }

    std::size_t capacity() const noexcept
    {
        return buf_.size();
    }

    std::size_t available() const noexcept
    {
        return capacity() - size();
    }

    boost::asio::const_buffer data() const noexcept
    {
        return {in_, dist(in_, out_)};
    }

    const T* begin() const noexcept
    {
        return in_;
    }

    const T* end() const noexcept
    {
        return out_;
    }

    std::size_t size() const noexcept
    {
        return dist(in_, out_);
    }

    bool empty() const noexcept
    {
        return in_ == out_;
    }

    boost::asio::mutable_buffer prepare(std::size_t n)
    {
        // Check if there's space at the end
        if (n <= dist(out_, buf_.data() + buf_.size()))
        {
            last_ = out_ + n;
            return {out_, n};
        }

        // Need to compact - check if there's enough total space
        const auto len = size();
        if (n > capacity() - len)
            throw std::length_error{"flat_buffer::prepare buffer overflow"};

        // Compact the buffer by moving data to the beginning
        if (len > 0)
        {
            // Use correct byte count for memmove (len is element count)
            std::memmove(buf_.data(), in_, len * sizeof(T));
        }

        // Reset pointers after compaction
        in_ = buf_.data();
        out_ = in_ + len;
        last_ = out_ + n;

        return {out_, n};
    }

    void commit(std::size_t n) noexcept
    {
        out_ += std::min(n, static_cast<std::size_t>(last_ - out_));
    }

    void consume(std::size_t n) noexcept
    {
        if (n >= size())
        {
            in_ = buf_.data();
            out_ = in_;
            return;
        }
        in_ += n;
    }

  private:
    static std::size_t dist(const T* first, const T* last) noexcept
    {
        return static_cast<std::size_t>(last - first);
    }
};
}  // namespace pa::pinex::detail