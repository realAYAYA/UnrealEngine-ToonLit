// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

// portable array

#ifndef RAIL_SDK_BASE_RAIL_ARRAY_H
#define RAIL_SDK_BASE_RAIL_ARRAY_H

#include <assert.h>
#include <cstring>

#ifndef __GNUC__
// --- windows ---
#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define USE_MANUAL_ALLOC 1
#if USE_MANUAL_ALLOC
#include <new>
#endif
#else
// --- linux ---
// only windows can use heap alloc
#define USE_MANUAL_ALLOC 0
#endif

#pragma push_macro("new")
#undef new

namespace rail {
#define RAIL_SDK_PACKING 8
#pragma pack(push, RAIL_SDK_PACKING)

#if defined(__GNUC__)
#define memcpy_s(dest, elements, src, count) memcpy((dest), (src), (count))
#endif

template<typename Ch>
class RailArray {
  public:
    typedef Ch ch_t;

    RailArray() {
        init_member();
    }

    explicit RailArray(size_t n) {
        init_member();
        prepare_capacity(n, false);
    }

    RailArray(const ch_t *buf, size_t n) {
        init_member();
        cp(buf, n);
    }

    RailArray(const RailArray<ch_t>& rs) {
        init_member();
        cp(rs.p_, rs.count_);
    }

    RailArray &operator=(const RailArray<ch_t>& rs) {
        if (&rs != this) {
            cp(rs.buf(), rs.size());
        }
        return *this;
    }

    template<size_t N>
    RailArray &operator=(ch_t (&rs)[N]) {
        init_member();
        cp(rs, N);
        return *this;
    }

#if __cplusplus >= 201103L || _MSC_VER >= 1600
    RailArray(RailArray<ch_t> &&rs) : p_(rs.p_), count_(rs.count_), capacity_(rs.capacity_) {
        rs.init_member();
    }

    RailArray &operator=(RailArray<ch_t> &&rs) {
        if (&rs == this) return *this;
        ch_t* old_p = p_;
        size_t old_capacity = capacity_;
        p_ = rs.p_;
        count_ = rs.count_;
        capacity_ = rs.capacity_;
        // release old array
        if (old_p) {
            ReleaseArray(old_p, &old_capacity);
        }
        rs.init_member();
        return *this;
    }
#endif

    virtual ~RailArray() {
        destory();
    }

    void assign(const ch_t* val, size_t elements) {
        cp(val, elements);
    }

    const ch_t* buf() const {
        return p_;
    }

    size_t size() const {
        return count_;
    }

    ch_t &operator[](size_t index) {
        assert(index < count_);
        return p_[index];
    }

    const ch_t &operator[](size_t index) const {
        assert(index < count_);
        return buf()[index];
    }

    void resize(size_t n) {
        prepare_capacity(n);
        count_ = n;
    }

    void push_back(const ch_t& ch) {
        assert(count_ <= capacity_);
        prepare_capacity(count_ + 1);
        p_[count_++] = ch;
    }

    void clear() {
        destory();
    }

    void erase(size_t index) {
        if (index >= count_)
            return;

        if (index == count_ - 1) {
            --count_;
            return;
        }

        for (size_t i = index; i < count_ - 1; ++i) {
            p_[i] = p_[i + 1];
        }
        --count_;
    }

  private:
    ch_t*   p_;
    size_t  count_;
    size_t  capacity_;

  private:
    void prepare_capacity(size_t n, bool keep = true) {
        if (n <= capacity_) {
            return;
        }
        ch_t* old_p = p_;
        size_t old_capacity = capacity_;
        capacity_ = n > 1024 * 1024 ? n + 1024 * 1024 : 2 * n;
        p_ = NewArray(capacity_);

        if (keep && old_p && p_) {
            for (size_t i = 0; i < count_; ++i) {
                p_[i] = old_p[i];
            }
        } else {
            count_ = 0;
        }

        // release array
        if (old_p) {
            ReleaseArray(old_p, &old_capacity);
        }
    }

    void cp(const ch_t* sp, size_t size) {
        // the source object is null or empty
        if (sp == NULL || size == 0) {
            clear();
            return;
        }

        assert(sp);
        assert(size > 0);
        prepare_capacity(size, false);
        count_ = size;
        // do not use memcpy for objects
        for (size_t i = 0; i < size; ++i) {
            p_[i] = sp[i];
        }
    }

    void init_member() {
        p_ = NULL;
        count_ = 0;
        capacity_ = 0;
    }

    void destory() {
        ReleaseArray(p_, &capacity_);
        count_ = 0;
    }

    ch_t* NewArray(size_t size) {
        if (size == 0) return NULL;
#if USE_MANUAL_ALLOC
        // windows and use heap alloc
        size_t bytes = sizeof(ch_t) * size;
        // use heap alloc
        ch_t* value = reinterpret_cast<ch_t*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes));
        for (size_t i = 0; i < size; ++i) {
            new(&value[i]) ch_t();
        }
        return value;
#else
        // linux or use new
        ch_t* value = new ch_t[size];
        return value;
#endif
    }

    void ReleaseArray(ch_t*& arr, size_t* size) {
        if (*size == 0) return;
#if USE_MANUAL_ALLOC
        // windows and use heap alloc
        for (size_t i = 0; i < *size; ++i) {
            arr[i].~ch_t();
        }

        // use heap alloc
        HeapFree(GetProcessHeap(), 0, arr);

        arr = NULL;
        *size = 0;
#else
        // linux or new deleate
        delete []arr;
        arr = NULL;
        *size = 0;
#endif
    }
};

#pragma pack(pop)
}  // namespace rail

#pragma pop_macro("new")
#endif  // RAIL_SDK_BASE_RAIL_ARRAY_H

