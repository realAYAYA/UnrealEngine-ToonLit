// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Misc/Timespan.h"
#include "Modules/ModuleInterface.h"

struct UE_DEPRECATED(5.0, "Deprecated. This class was replaced by FEngineAnalyticsSessionSummary/FEditorAnalyticsSessionSummary") FEditorAnalyticsSession;

struct EDITORANALYTICSSESSION_API FEditorAnalyticsSession
{
};

/** This function removes expired sessions that were created by the deprecated system. */
EDITORANALYTICSSESSION_API void CleanupDeprecatedAnalyticSessions(const FTimespan& MaxAge);


class FEditorAnalyticsSessionModule : public IModuleInterface
{
};
