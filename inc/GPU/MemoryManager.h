#ifndef GDEL2D_MEMORYMANAGER_H
#define GDEL2D_MEMORYMANAGER_H

#include <thrust/device_malloc.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>

////////////////////////////////////////////////////////////////// DevVector //

template <typename T>
class DevVector
{
  public:
    // Types
    using DevPtr = thrust::device_ptr<T>;

    // Properties
    DevPtr _ptr;
    size_t _size     = 0;
    size_t _capacity = 0;
    bool   _owned    = true;

    DevVector() = default;

    explicit DevVector(size_t n) : _size(0), _capacity(0)
    {
        resize(n);
    }

    DevVector(size_t n, T value) : _size(0), _capacity(0)
    {
        assign(n, value);
    }

    ~DevVector()
    {
        free();
    }

    void free()
    {
        if (_capacity > 0 && _owned)
            CudaSafeCall(cudaFree(_ptr.get()));

        _size     = 0;
        _capacity = 0;
    }

    // Use only for cases where new size is within capacity
    // So, old data remains in-place
    void expand(size_t n)
    {
        assert((_capacity >= n) && "New size not within current capacity! Use resize!");
        _size = n;
    }

    // Resize with data remains
    void grow(size_t n)
    {
        assert((n >= _size) && "New size not larger than old size.");

        if (_capacity >= n)
        {
            _size = n;
            return;
        }

        DevVector<T> tempVec(n);
        thrust::copy(begin(), end(), tempVec.begin());
        swapAndFree(tempVec);
    }

    void resize(size_t n)
    {
        if (_capacity >= n)
        {
            _size = n;
            return;
        }

        if (!_owned && _capacity > 0)
        {
            std::cerr << "WARNING: Resizing a DevVector with borrowing pointer!" << std::endl;
        }

        free();

        _size     = n;
        _capacity = (n == 0) ? 1 : n;
        _owned    = true;

        try
        {
            _ptr = thrust::device_malloc<T>(_capacity);
        }
        catch (...)
        {
            const int OneMB = (1 << 20);
            throw(std::runtime_error(
                "thrust::device_malloc failed to allocate " + std::to_string((sizeof(T) * _capacity) / OneMB) +
                " MB!\nsize=" + std::to_string(_size) + " sizeof(T)=" + std::to_string(sizeof(T))));
        }
    }

    void assign(size_t n, const T &value)
    {
        resize(n);
        thrust::fill_n(begin(), n, value);
    }

    size_t size() const
    {
        return _size;
    }
    size_t capacity() const
    {
        return _capacity;
    }

    thrust::device_reference<T> operator[](const size_t index) const
    {
        return _ptr[index];
    }

    DevPtr begin() const
    {
        return _ptr;
    }

    DevPtr end() const
    {
        return _ptr + _size;
    }

    void erase(const DevPtr &first, const DevPtr &last)
    {
        if (last == end())
        {
            _size -= (last - first);
        }
        else
        {
            assert(false && "Not supported right now!");
        }
    }

    void swap(DevVector<T> &arr)
    {
        size_t tempSize  = _size;
        size_t tempCap   = _capacity;
        bool   tempOwned = _owned;
        T     *tempPtr   = (_capacity > 0) ? _ptr.get() : 0;

        _size     = arr._size;
        _capacity = arr._capacity;
        _owned    = arr._owned;

        if (_capacity > 0)
        {
            _ptr = thrust::device_ptr<T>(arr._ptr.get());
        }

        arr._size     = tempSize;
        arr._capacity = tempCap;
        arr._owned    = tempOwned;

        if (tempCap > 0)
        {
            arr._ptr = thrust::device_ptr<T>(tempPtr);
        }
    }

    // Input array is freed
    void swapAndFree(DevVector<T> &inArr)
    {
        swap(inArr);
        inArr.free();
    }

    void copyFrom(const DevVector<T> &inArr)
    {
        resize(inArr.size());
        thrust::copy(inArr.begin(), inArr.end(), begin());
    }

    void fill(const T &value)
    {
        thrust::fill_n(_ptr, _size, value);
    }

    void copyToHost(thrust::host_vector<T> &dest) const
    {
        dest.insert(dest.begin(), begin(), end());
    }

    // Do NOT remove! Useful for debugging.
    void copyFromHost(const thrust::host_vector<T> &inArr)
    {
        resize(inArr.size());
        thrust::copy(inArr.begin(), inArr.end(), begin());
    }
};

//////////////////////////////////////////////////////////// Memory pool //

struct Buffer
{
    void  *ptr;
    size_t sizeInBytes;
    bool   avail;
};

class MemoryPool
{
  private:
    std::vector<Buffer> _memPool; // Two items

  public:
    MemoryPool() = default;

    ~MemoryPool()
    {
        free();
    }

    void free(bool report = false)
    {
        for (int i = 0; i < _memPool.size(); ++i)
        {
            if (report)
                std::cout << "MemoryPool: [" << i << "]" << _memPool[i].sizeInBytes << std::endl;

            if (!_memPool[i].avail)
                std::cerr << "WARNING: MemoryPool item not released!" << std::endl;
            else
                CudaSafeCall(cudaFree(_memPool[i].ptr));
        }

        _memPool.clear();
    }

    template <typename T>
    int reserve(size_t size)
    {
        DevVector<T> vec(size);

        vec._owned = false;

        Buffer buf = {(void *)vec._ptr.get(), size * sizeof(T), true};

        _memPool.push_back(buf);

        return static_cast<int>(_memPool.size()) - 1;
    }

    template <typename T>
    DevVector<T> allocateAny(size_t size, bool tempOnly = false)
    {
        // Find best fit block
        size_t sizeInBytes = size * sizeof(T);
        int    bufIdx      = -1;

        for (int i = 0; i < _memPool.size(); ++i)
            if (_memPool[i].avail && _memPool[i].sizeInBytes >= sizeInBytes)
                if (bufIdx == -1 || _memPool[i].sizeInBytes < _memPool[bufIdx].sizeInBytes)
                    bufIdx = i;

        if (bufIdx == -1)
        {
            std::cout << "MemoryPool: Allocating " << sizeInBytes << std::endl;
            bufIdx = reserve<T>(size);
        }

        DevVector<T> vec;

        vec._ptr      = thrust::device_ptr<T>((T *)_memPool[bufIdx].ptr);
        vec._capacity = _memPool[bufIdx].sizeInBytes / sizeof(T);
        vec._size     = 0;
        vec._owned    = false;

        //std::cout << "MemoryPool: Requesting "
        //    << sizeInBytes << ", giving " << memPool[ bufIdx ].sizeInBytes << std::endl;

        // Disable the buffer in the pool
        if (!tempOnly)
            _memPool[bufIdx].avail = false;

        return vec;
    }

    template <typename T>
    void release(DevVector<T> &vec)
    {
        for (auto &i : _memPool)
            if (i.ptr == (void *)vec._ptr.get())
            {
                assert(!i.avail);
                assert(!vec._owned);

                // Return the buffer to the pool
                i.avail = true;

                //std::cout << "MemoryPool: Returning " << memPool[d_i].sizeInBytes << std::endl;

                // Reset the vector to 0 size
                vec.free();

                return;
            }

        std::cerr << "WARNING: Releasing a DevVector not in the MemoryPool!" << std::endl;

        // Release the vector
        vec._owned = true; // Set this to true so it releases itself.
        vec.free();
    }
};

#endif //GDEL2D_MEMORYMANAGER_H