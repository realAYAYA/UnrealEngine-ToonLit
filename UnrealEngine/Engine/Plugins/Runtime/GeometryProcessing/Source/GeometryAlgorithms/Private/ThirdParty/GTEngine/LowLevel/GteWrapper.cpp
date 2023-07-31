// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#include <ThirdParty/GTEngine/LowLevel/GteWrapper.h>
#include <ThirdParty/GTEngine/GTEnginePCH.h>
#include <cstring>
#include <cwchar>

namespace gte
{

void Memcpy(void* target, void const* source, size_t count)
{
#if defined(__MSWINDOWS__)
    errno_t result = memcpy_s(target, count, source, count);
    (void)result;  // 0 on success
#else
    memcpy(target, source, count);
#endif
}

void Memcpy(wchar_t* target, wchar_t const* source, size_t count)
{
#if defined(__MSWINDOWS__)
    errno_t result = wmemcpy_s(target, count, source, count);
    (void)result;  // 0 on success
#else
    wmemcpy(target, source, count);
#endif
}

}
