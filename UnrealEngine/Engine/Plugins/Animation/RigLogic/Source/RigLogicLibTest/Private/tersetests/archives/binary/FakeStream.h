// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cassert>
#include <cstring>
#include <cstddef>
#include <vector>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace tersetests {

struct FakeStream {

    FakeStream() : position{} {
    }

    void open() {
        position = 0ul;
    }

    void close() {
        position = 0ul;
    }

    std::size_t tell() {
        return position;
    }

    void seek(std::size_t position_) {
        position = position_;
    }

    void read(char* buffer, std::size_t size) {
        assert(position + size <= data.size());
        std::memcpy(buffer, &data[position], size);
        position += size;
    }

    void write(const char* buffer, std::size_t size) {
        std::size_t available = data.size() - position;
        if (available < size) {
            data.resize(data.size() + (size - available));
        }
        std::memcpy(&data[position], buffer, size);
        position += size;
    }

    std::size_t size() {
        return data.size();
    }

    std::vector<char> data;
    std::size_t position;
};

}  // namespace tersetests
