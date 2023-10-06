// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace AutoRTFM
{

template<typename T>
class TScopedGuard
{
public:
    TScopedGuard(T& Ref, const T& Value)
        : OldValue(Ref)
        , Ref(Ref)
    {
        Ref = Value;
    }

    ~TScopedGuard()
    {
        Ref = OldValue;
    }

private:
    T OldValue;
    T& Ref;
};

} // namespace AutoRTFM
