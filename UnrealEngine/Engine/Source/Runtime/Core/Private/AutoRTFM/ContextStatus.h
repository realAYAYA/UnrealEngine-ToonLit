// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AutoRTFM/AutoRTFMConstants.h"
#include "Utils.h"

namespace AutoRTFM
{
static_assert(static_cast<unsigned>(EContextStatus::OnTrack) == Constants::Context_Status_OnTrack, "Not equal");

inline const char* GetContextStatusName(EContextStatus Status)
{
    switch (Status)
    {
    default:
        Unreachable();
        return nullptr;
    case EContextStatus::Idle:
        return "Idle";
    case EContextStatus::OnTrack:
        return "OnTrack";
    case EContextStatus::AbortedByFailedLockAcquisition:
        return "AbortedByFailedLockAcquisition";
    case EContextStatus::AbortedByLanguage:
        return "AbortedByLanguage";
    case EContextStatus::AbortedByRequest:
        return "AbortedByRequest";
    case EContextStatus::Committing:
        return "Committing";
    }
}

} // namespace AutoRTFM
