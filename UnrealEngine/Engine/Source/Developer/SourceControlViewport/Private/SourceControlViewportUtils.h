// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

class FViewportClient;

UENUM()
enum class ESourceControlStatus : uint8
{
	/* The file is checked out by another user */
	CheckedOutByOtherUser,

	/* The file is not at the latest revision */
	NotAtHeadRevision,

	/* The file is checked out by self */
	CheckedOut,

	/* The file is added by self */
	OpenForAdd,
};

namespace SourceControlViewportUtils
{

// Helper method to get the viewport feedback value from the viewport.
bool GetFeedbackEnabled(FViewportClient* ViewportClient, ESourceControlStatus Status);

// Helper method to set the viewport feedback on or off on the viewport.
void SetFeedbackEnabled(FViewportClient* ViewportClient, ESourceControlStatus Status, bool bEnabled);

}