// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#ifdef _MSC_VER
    #pragma warning(push)
    #pragma warning(disable : 4365 4987)
#endif
#include <cstddef>
#include <cstdint>
#include <memory>
#ifdef _MSC_VER
    #pragma warning(pop)
#endif

namespace terse {

template<typename TOffset>
struct ArchiveOffset {
    using ValueType = TOffset;

    struct Proxy {
        ArchiveOffset* target;

        explicit Proxy(ArchiveOffset& ptr) : target{std::addressof(ptr)} {
            target->proxy = this;
        }

        ~Proxy() = default;

        Proxy(const Proxy&) = delete;
        Proxy& operator=(const Proxy&) = delete;

        Proxy(Proxy&& rhs) {
            std::swap(target, rhs.target);
            target->proxy = this;
        }

        Proxy& operator=(Proxy&& rhs) {
            std::swap(target, rhs.target);
            target->proxy = this;
            return *this;
        }

    };

    // The position of the marker itself in the stream (this is a runtime-only value
    // which is not written to the file, needed only for the serializer to know where
    // to seek within the stream when the marker's actual value needs to be written)
    std::size_t position{};
    // The position in the stream where the marker wants to point (this is the actual
    // value that is written to the file)
    ValueType value{};
    // When offset is moved, it's associated proxy must be updated about the new address
    Proxy* proxy{};

    ArchiveOffset() = default;
    ~ArchiveOffset() = default;

    ArchiveOffset(const ArchiveOffset&) = delete;
    ArchiveOffset& operator=(const ArchiveOffset&) = delete;

    ArchiveOffset(ArchiveOffset&& rhs) {
        std::swap(position, rhs.position);
        std::swap(value, rhs.value);
        std::swap(proxy, rhs.proxy);
        // Update proxy with new address
        proxy->target = this;
    }

    ArchiveOffset& operator=(ArchiveOffset&& rhs) {
        std::swap(position, rhs.position);
        std::swap(value, rhs.value);
        std::swap(proxy, rhs.proxy);
        // Update proxy with new address
        proxy->target = this;
        return *this;
    }

};

}  // namespace terse
