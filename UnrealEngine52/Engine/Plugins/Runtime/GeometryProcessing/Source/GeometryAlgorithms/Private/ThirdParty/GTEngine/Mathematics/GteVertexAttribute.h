// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.3.0 (2016/08/29)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <string>

namespace gte
{

struct VertexAttribute
{
    inline VertexAttribute();
    inline VertexAttribute(std::string const& inSemantic, void* inSource, size_t inStride);

    // The 'semantic' string allows you to query for a specific vertex
    // attribute and use the 'source' and 'stride' to access the data
    // of the attribute.  For example, you might use the semantics
    // "position" (px,py,pz), "normal" (nx,ny,nz), "tcoord" (texture
    // coordinates (u,v)), "dpdu" (derivative of position with respect
    // to u), or "dpdv" (derivative of position with respect to v) for
    // mesh vertices.
    //
    // The source pointer must be 4-byte aligned.  The stride must be
    // positive and a multiple of 4.  The pointer alignment constraint is
    // guaranteed on 32-bit and 64-bit architectures.  The stride constraint
    // is reasonable given that (usually) geometric attributes are usually
    // arrays of 'float' or 'double'.

    std::string semantic;
    void* source;
    size_t stride;
};

inline VertexAttribute::VertexAttribute()
    :
    semantic(""),
    source(nullptr),
    stride(0)
{
}

inline VertexAttribute::VertexAttribute(std::string const& inSemantic, void* inSource, size_t inStride)
    :
    semantic(inSemantic),
    source(inSource),
    stride(inStride)
{
}

}
