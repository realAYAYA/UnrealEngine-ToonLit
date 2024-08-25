// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "LiveLinkEditorSettings.generated.h"

/**
 * Editor preferences settings for LiveLink.
 */
UCLASS(config = EditorPerProjectUserSettings)
class LIVELINKEDITOR_API ULiveLinkEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	/** Whether the livelink panel should be in read-only mode. */
	UPROPERTY(config, EditAnywhere, Category = "LiveLink")
	bool bReadOnly = false;
};
