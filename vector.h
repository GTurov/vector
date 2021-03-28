#pragma once
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

#include <iostream> //debug

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity)
        : buffer_(Allocate(capacity))
        , capacity_(capacity) {
    }

    RawMemory(const RawMemory&) = delete;

    RawMemory& operator=(const RawMemory& rhs) = delete;

    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }

    RawMemory& operator=(RawMemory&& rhs) noexcept {
        buffer_ = std::exchange(rhs.buffer_, nullptr);
        capacity_ = std::exchange(rhs.capacity_, 0);
        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        // Разрешается получать адрес ячейки памяти, следующей за последним элементом массива
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    // Выделяет сырую память под n элементов и возвращает указатель на неё
    static T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    // Освобождает сырую память, выделенную ранее по адресу buf при помощи Allocate
    static void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    iterator begin() noexcept {
        return data_.GetAddress();
    }
    iterator end() noexcept {
        return data_+size_;
    }
    const_iterator begin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator end() const noexcept {
        return data_+size_;
    }
    const_iterator cbegin() const noexcept {
        return data_.GetAddress();
    }
    const_iterator cend() const noexcept {
        return data_+size_;
    }

    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other)
        : data_(other.size_)
        , size_(other.size_)  //
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept
        : data_(std::move(other.data_))
        , size_(std::exchange(other.size_,0))  //
    {
        //std::uninitialized_move_n(other.data_.GetAddress(), size_, data_.GetAddress());
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                /* Скопировать элементы из rhs, создав при необходимости новые
                   или удалив существующие */
                if (rhs.size_ < this->size_) {
                    for (size_t i = 0; i != rhs.size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    for (size_t i = rhs.size_; i != this->size_; ++i) {
                        data_[i].~T();
                    }
                } else {
                    for (size_t i = 0; i != this->size_; ++i) {
                        data_[i] = rhs.data_[i];
                    }
                    std::uninitialized_copy_n(rhs.data_+this->size_, rhs.size_ - this->size_,
                                              data_+this->size_);
                }
                this->size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::move(rhs.data_);
            size_ = std::exchange(rhs.size_,0);
        }
        return *this;
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }
        RawMemory<T> new_data(new_capacity);
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
        } else {
            std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
        }
        data_.Swap(new_data);
        std::destroy_n(new_data.GetAddress(), size_);
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(this->size_, other.size_);
    }

    void Resize(size_t new_size) {
        if (new_size > size_) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_+size_, new_size-size_);
        } else {
            for (size_t i = new_size; i != size_; ++i) {
                data_[i].~T();
            }
        }
        size_ = new_size;
    }

    void PushBack(const T& value) {
        if (size_ == data_.Capacity()) {
            RawMemory<T> new_data(data_.Capacity()==0?1:data_.Capacity()*2);
            new (new_data+size_) T(value);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new (data_+size_) T(value);
        }
        ++size_;
    }

    void PushBack(T&& value) {
        if (size_ == data_.Capacity()) {
            RawMemory<T> new_data(data_.Capacity()==0?1:data_.Capacity()*2);
            new (new_data+size_) T(std::move(value));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new (data_+size_) T(std::move(value));
        }
        ++size_;
    }

    void PopBack()  noexcept {
        data_[size_-1].~T();
        --size_;
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (size_ == data_.Capacity()) {
            RawMemory<T> new_data(data_.Capacity()==0?1:data_.Capacity()*2);
            new (new_data+size_) T(std::forward<Args>(args)...);
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(data_.GetAddress(), size_, new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(data_.GetAddress(), size_, new_data.GetAddress());
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        } else {
            new (data_+size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return data_[size_-1];
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        uint64_t n = pos - data_.GetAddress();
        //std::cerr<<size_<<' '<<data_.Capacity();
        if (size_ != data_.Capacity() && data_.Capacity() != 0) {
            //T tmp_value(std::forward<Args>(args)...);
            if (size_ != 0) {
//                std::uninitialized_value_construct_n(data_+size_, 1);
//                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    new (data_+size_) T(std::move(data_[size_-1]));
//                } else {
//                    new (data_+size_) T(data_[size_-1]);
//                }
//                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
//                    std::uninitialized_move_n(data_+size_-1, 1, data_+size_);
//                } else {
//                    std::uninitialized_copy_n(data_+size_-1, 1, data_+size_);
//                }

                std::cerr<<std::distance(data_+n, data_+size_-1);
                std::move_backward(data_+n, data_+size_-1, data_+size_);
            }
//            T tmp_obj(std::forward<Args>(args)...);
//            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
//                std::uninitialized_move_n(data_+n, 1, &tmp_obj);
//            } else {
//                std::uninitialized_copy_n(data_+n, 1, &tmp_obj);
//            }
//            data_[n] = (tmp_value);

//            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
//                new (data_+n) T(std::move(tmp_value));
//            } else {
//                new (data_+n) T(tmp_value);
//            }
            new (data_+n) T(std::forward<Args>(args)...);
        } else {
            RawMemory<T> new_data(data_.Capacity()==0?1:data_.Capacity()*2);
            new (new_data+n) T(std::forward<Args>(args)...);
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), n, new_data.GetAddress());
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(), n, new_data.GetAddress());
                }
            } catch (...) {
                new_data[n].~T();
                throw;
            }
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_+n, size_-n, new_data+n+1);
                } else {
                    std::uninitialized_copy_n(data_+n, size_-n, new_data+n+1);
                }
            } catch (...) {
                std::destroy_n(new_data.GetAddress(), n+1);
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return data_+n;
    }
    iterator Erase(const_iterator pos) /*noexcept(std::is_nothrow_move_assignable_v<T>)*/ {
        uint64_t n = pos - data_.GetAddress();
        std::move(data_+n+1, end(), data_+n);
        data_[size_-1].~T();
        --size_;
        return data_ + n;
    }
    iterator Insert(const_iterator pos, const T& value) {
        uint64_t n = pos - data_.GetAddress();
        if (size_ != data_.Capacity() && data_.Capacity() != 0) {
            T tmp_value(value);
            if (size_ != 0) {
//                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    new (data_+size_) T(std::move(data_[size_-1]));
//                } else {
//                    new (data_+size_) T(data_[size_-1]);
//                }
            }
            std::move_backward(data_+n, data_+size_-1, data_+size_);
            //new (data_+n) T(std::move(*tmp_data));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                new (data_+n) T(std::move(tmp_value));
            } else {
                new (data_+n) T(tmp_value);
            }
        } else {
            RawMemory<T> new_data(data_.Capacity()==0?1:data_.Capacity()*2);
            new (new_data+n) T(value);
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), n, new_data.GetAddress());
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(), n, new_data.GetAddress());
                }
            } catch (...) {
                new_data[n].~T();
                throw;
            }
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_+n, size_-n, new_data+n+1);
                } else {
                    std::uninitialized_copy_n(data_+n, size_-n, new_data+n+1);
                }
            } catch (...) {
                std::destroy_n(new_data.GetAddress(), n+1);
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return data_+n;
    }
    iterator Insert(const_iterator pos, T&& value) {
        uint64_t n = pos - data_.GetAddress();
        if (size_ != data_.Capacity() && data_.Capacity() != 0) {
            T tmp_value(std::move(value));
            if (size_ != 0) {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    new (data_+size_) T(std::move(data_[size_-1]));
                } else {
                    new (data_+size_) T(data_[size_-1]);
                }
            }
            std::move_backward(data_+n, data_+size_-1, data_+size_);
            //new (data_+n) T(std::move(*tmp_data));
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                new (data_+n) T(std::move(tmp_value));
            } else {
                new (data_+n) T(tmp_value);
            }
        } else {
            RawMemory<T> new_data(data_.Capacity()==0?1:data_.Capacity()*2);
            new (new_data+n) T(std::move(value));
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_.GetAddress(), n, new_data.GetAddress());
                } else {
                    std::uninitialized_copy_n(data_.GetAddress(), n, new_data.GetAddress());
                }
            } catch (...) {
                new_data[n].~T();
                throw;
            }
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                    std::uninitialized_move_n(data_+n, size_-n, new_data+n+1);
                } else {
                    std::uninitialized_copy_n(data_+n, size_-n, new_data+n+1);
                }
            } catch (...) {
                std::destroy_n(new_data.GetAddress(), n+1);
                throw;
            }
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        ++size_;
        return data_+n;
    }

private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
