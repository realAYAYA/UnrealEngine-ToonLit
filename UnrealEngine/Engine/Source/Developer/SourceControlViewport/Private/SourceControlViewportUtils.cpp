// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourceControlViewportUtils.h"
#include "Settings/EditorStyleSettings.h"
#include "LevelEditorViewport.h"

namespace SourceControlViewportUtils
{

// Helper method to get the outline color index for a setting.
static uint32 GetOutlineColorIndex(ESourceControlStatus Status)
{
	FLinearColor ColorToUse;
	switch (Status)
	{
	case ESourceControlStatus::CheckedOutByOtherUser:
		ColorToUse = FColor::Red;
		break;
	case ESourceControlStatus::NotAtHeadRevision:
		ColorToUse = FColor::Yellow;
		break;
	case ESourceControlStatus::CheckedOut:
		ColorToUse = FColor::Blue;
		break;
	case ESourceControlStatus::OpenForAdd:
		ColorToUse = FColor::Green;
		break;
	default:
		checkNoEntry();
		break;
	}

	// The available colors are defined in the UEditorStyleSettings::AdditionalSelectionColors array.
	// The loop below gets the selection color index closest to the desired color.

	int32 MinIndex = -1;
	float MinDist = UE_MAX_FLT;

	const UEditorStyleSettings* Settings = GetDefault<UEditorStyleSettings>();
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Settings->AdditionalSelectionColors); ++Index)
	{
		FLinearColor Color = Settings->AdditionalSelectionColors[Index];

		float Dist = FLinearColor::Dist(ColorToUse, Color);
		if (Dist < MinDist)
		{
			MinIndex = Index;
			MinDist = Dist;
		}
	}

	return MinIndex;
}

bool GetOutlineSetting(FViewportClient* ViewportClient, ESourceControlStatus Status)
{
	uint32 ColorIndex = GetOutlineColorIndex(Status);
	switch (ColorIndex)
	{
	case 0:
		return ViewportClient->GetEngineShowFlags()->SelectionOutlineColor0;
	case 1:
		return ViewportClient->GetEngineShowFlags()->SelectionOutlineColor1;
	case 2:
		return ViewportClient->GetEngineShowFlags()->SelectionOutlineColor2;
	case 3:
		return ViewportClient->GetEngineShowFlags()->SelectionOutlineColor3;
	case 4:
		return ViewportClient->GetEngineShowFlags()->SelectionOutlineColor4;
	case 5:
		return ViewportClient->GetEngineShowFlags()->SelectionOutlineColor5;
	default:
		ensure(false);
		break;
	}

	return false;
}

void SetOutlineSetting(FViewportClient* ViewportClient, ESourceControlStatus Status, bool bEnabled)
{
	uint32 ColorIndex = GetOutlineColorIndex(Status);
	switch (ColorIndex)
	{
	case 0:
		ViewportClient->GetEngineShowFlags()->SelectionOutlineColor0 = bEnabled;
		break;
	case 1:
		ViewportClient->GetEngineShowFlags()->SelectionOutlineColor1 = bEnabled;
		break;
	case 2:
		ViewportClient->GetEngineShowFlags()->SelectionOutlineColor2 = bEnabled;
		break;
	case 3:
		ViewportClient->GetEngineShowFlags()->SelectionOutlineColor3 = bEnabled;
		break;
	case 4:
		ViewportClient->GetEngineShowFlags()->SelectionOutlineColor4 = bEnabled;
		break;
	case 5:
		ViewportClient->GetEngineShowFlags()->SelectionOutlineColor5 = bEnabled;
		break;
	default:
		ensure(false);
		break;
	}
}

bool GetFeedbackEnabled(FViewportClient* ViewportClient, ESourceControlStatus Status)
{
	return GetOutlineSetting(ViewportClient, Status);
}

void SetFeedbackEnabled(FViewportClient* ViewportClient, ESourceControlStatus Status, bool bEnabled)
{
	SetOutlineSetting(ViewportClient, Status, bEnabled);
}

}