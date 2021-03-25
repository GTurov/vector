#pragma once
#include <cassert>
#include <cstdlib>
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
    Vector() = default;

    explicit Vector(size_t size)
        : data_(size)
        , size_(size)  //
    {
            size_t i = 0;
            try {
                for (; i != size; ++i) {
                    new (data_ + i) T();
                }
            } catch (...) {
                // В переменной i содержится количество созданных элементов.
                // Теперь их надо разрушить
                DestroyN(data_, i);
                // Перевыбрасываем пойманное исключение, чтобы сообщить об ошибке создания объекта
                throw;
            }
        }

    Vector(const Vector& other)
            : data_(other.size_)
            , size_(other.size_)  //
    {
        size_t i = 0;
        try {
            for (; i != other.size_; ++i) {
                CopyConstruct(data_ + i, other.data_[i]);
            }
        } catch (...) {
            // В переменной i содержится количество созданных элементов.
            // Теперь их надо разрушить
            DestroyN(data_, i);
            // Перевыбрасываем пойманное исключение, чтобы сообщить об ошибке создания объекта
            throw;
        }

    }

    ~Vector() {
           DestroyN(data_, size_);
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
        //T* new_data = nullptr;
        size_t i = 0;
        try {
            //new_data = Allocate(new_capacity);
            RawMemory<T> new_memory(new_capacity);
            for (; i != size_; ++i) {
                CopyConstruct(new_data + i, data_[i]);
            }
            DestroyN(data_, size_);
            Deallocate(data_);

            data_.Swap(new_memory);
        } catch (...) {
            DestroyN(new_data, i);
            throw;
        }
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }


private:
    // Вызывает деструкторы n объектов массива по адресу buf
    static void DestroyN(T* buf, size_t n) noexcept {
        for (size_t i = 0; i != n; ++i) {
            Destroy(buf + i);
        }
    }

    // Создаёт копию объекта elem в сырой памяти по адресу buf
    static void CopyConstruct(T* buf, const T& elem) {
        new (buf) T(elem);
    }

    // Вызывает деструктор объекта по адресу buf
    static void Destroy(T* buf) noexcept {
        buf->~T();
    }

private:
//    T* data_ = nullptr;
//    size_t capacity_ = 0;
    RawMemory<T> data_;
    size_t size_ = 0;
};
