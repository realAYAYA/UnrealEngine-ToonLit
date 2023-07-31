// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

static bool CompareNames(const TSharedPtr<FString>& sp1, const TSharedPtr<FString>& sp2)
{
    if (const FString* s1 = sp1.Get())
    {
        if (const FString* s2 = sp2.Get())
        {
            return (s1->Compare(*s2, ESearchCase::IgnoreCase) < 0);
        }
        else
        {
            return false;
        }
    }
    else
    {
        return true;
    }
}