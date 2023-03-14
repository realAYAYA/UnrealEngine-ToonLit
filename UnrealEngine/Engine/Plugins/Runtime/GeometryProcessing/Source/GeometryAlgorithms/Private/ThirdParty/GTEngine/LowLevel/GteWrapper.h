// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>
#include <cstddef>

// Wrappers around platform-specific low-level library calls.

namespace gte
{

void Memcpy(void* target, void const* source, size_t count);
void Memcpy(wchar_t* target, wchar_t const* source, size_t count);

}
