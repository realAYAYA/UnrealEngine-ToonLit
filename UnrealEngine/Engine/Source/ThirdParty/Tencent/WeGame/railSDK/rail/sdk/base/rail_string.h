// Copyright (C) 2020, Entropy Game Global Limited.
// All rights reserved.

// portable string

#ifndef RAIL_SDK_BASE_RAIL_STRING_H
#define RAIL_SDK_BASE_RAIL_STRING_H

#include "rail/sdk/base/rail_array.h"

namespace rail {
#pragma pack(push, RAIL_SDK_PACKING)

template<typename Ch>
class RailBaseString : public RailArray<Ch> {
  public:
    typedef Ch ch_t;

    RailBaseString() : RailArray<ch_t>() {
        // construct empty string
        RailArray<ch_t>::push_back(ch_t(0));
    }
    RailBaseString(const ch_t* str) : RailArray<ch_t>(str, len(str) + 1) {}
    RailBaseString& operator=(const Ch* rs) {
        RailArray<ch_t>::assign(rs, len(rs) + 1);
        return *this;
    }

    void assign(const ch_t* val, size_t elements) {
        RailArray<ch_t>::resize(elements + 1);
        RailArray<ch_t>::assign(val, elements);
        ch_t terminal = 0;
        RailArray<ch_t>::push_back(terminal);
    }

    const ch_t* c_str() const { return buf(); }

    const char* data() const { return c_str(); }

    void clear() {
        RailArray<ch_t>::clear();
        RailArray<ch_t>::push_back(ch_t(0));
    }

    size_t size() const {
        size_t len = RailArray<ch_t>::size();
        return len > 0 ? len - 1 : len;  // not include '\0'
    }

    friend bool operator==(const RailBaseString& lval, const RailBaseString& rval) {
        return compare(lval, rval) == 0 ? true : false;
    }

    friend bool operator!=(const RailBaseString& lval, const RailBaseString& rval) {
        return compare(lval, rval) != 0 ? true : false;
    }

    friend bool operator>(const RailBaseString& lval, const RailBaseString& rval) {
        return compare(lval, rval) > 0 ? true : false;
    }

    friend bool operator<(const RailBaseString& lval, const RailBaseString& rval) {
        return compare(lval, rval) < 0 ? true : false;
    }

  private:
    using RailArray<ch_t>::push_back;
    using RailArray<ch_t>::buf;

  private:
    size_t len(const ch_t* str) {
        assert(str);
        assert(sizeof(ch_t) <= 2);
        size_t l = 0;
        while (str[l] != ch_t(0))
            ++l;
        return l;
    }

    static int32_t compare(const RailBaseString& lval, const RailBaseString& rval) {
        const ch_t* ptr_l = lval.data();
        const ch_t* ptr_r = rval.data();
        if (ptr_l == NULL && ptr_r == NULL) {
            return 0;
        } else if (ptr_l == NULL) {
            return -1;
        } else if (ptr_r == NULL) {
            return 1;
        }

        size_t size = lval.size();
        if (size > rval.size()) {
            size = rval.size();
        }
        int32_t result = memcmp(ptr_l, ptr_r, size);
        if (result == 0) {
            result = static_cast<int32_t>(lval.size()) - static_cast<int32_t>(rval.size());
        }
        return result;
    }
};

typedef RailBaseString<char> RailString;
typedef RailBaseString<wchar_t> RailWString;

#pragma pack(pop)
}  // namespace rail

#endif  // RAIL_SDK_BASE_RAIL_STRING_H
