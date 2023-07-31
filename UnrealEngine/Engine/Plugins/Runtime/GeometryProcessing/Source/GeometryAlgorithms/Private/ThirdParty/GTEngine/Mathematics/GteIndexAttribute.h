// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.3.1 (2016/11/14)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <cstddef>
#include <cstdint>

// The IndexAttribute class represents an array of triples of indices into a
// vertex array for an indexed triangle mesh.  For now, the source must be
// either uint16_t or uint32_t.

namespace gte
{

struct IndexAttribute
{
    // Construction.
    inline IndexAttribute();
    inline IndexAttribute(void* inSource, size_t inSize);

    // Triangle access.
    inline void SetTriangle(uint32_t t, uint32_t v0, uint32_t v1, uint32_t v2);
    inline void GetTriangle(uint32_t t, uint32_t& v0, uint32_t& v1, uint32_t& v2) const;

    // The source pointer must be 4-byte aligned, which is guaranteed on
    // 32-bit and 64-bit architectures.  The size is the number of bytes
    // per index.
    void* source;
    size_t size;
};


inline IndexAttribute::IndexAttribute()
    :
    source(nullptr),
    size(0)
{
}

inline IndexAttribute::IndexAttribute(void* inSource, size_t inSize)
    :
    source(inSource),
    size(inSize)
{
}

inline void IndexAttribute::SetTriangle(uint32_t t, uint32_t v0, uint32_t v1, uint32_t v2)
{
    if (size == sizeof(uint32_t))
    {
        uint32_t* index = reinterpret_cast<uint32_t*>(source) + 3 * t;
        index[0] = v0;
        index[1] = v1;
        index[2] = v2;
        return;
    }

    if (size == sizeof(uint16_t))
    {
        uint16_t* index = reinterpret_cast<uint16_t*>(source) + 3 * t;
        index[0] = static_cast<uint16_t>(v0);
        index[1] = static_cast<uint16_t>(v1);
        index[2] = static_cast<uint16_t>(v2);
        return;
    }

    // Unsupported type.
}

inline void IndexAttribute::GetTriangle(uint32_t t, uint32_t& v0, uint32_t& v1, uint32_t& v2) const
{
    if (size == sizeof(uint32_t))
    {
        uint32_t* index = reinterpret_cast<uint32_t*>(source) + 3 * t;
        v0 = index[0];
        v1 = index[1];
        v2 = index[2];
        return;
    }

    if (size == sizeof(uint16_t))
    {
        uint16_t* index = reinterpret_cast<uint16_t*>(source) + 3 * t;
        v0 = static_cast<uint32_t>(index[0]);
        v1 = static_cast<uint32_t>(index[1]);
        v2 = static_cast<uint32_t>(index[2]);
        return;
    }

    // Unsupported type.
    v0 = 0;
    v1 = 0;
    v2 = 0;
}

}
