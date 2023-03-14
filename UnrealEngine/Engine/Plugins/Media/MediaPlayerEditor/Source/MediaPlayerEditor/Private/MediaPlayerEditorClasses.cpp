// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Shared/MediaPlayerEditorSettings.h"


UMediaPlayerEditorSettings::UMediaPlayerEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ShowTextOverlays(true)
	, ViewportScale(EMediaPlayerEditorScale::Fit)
{ }
