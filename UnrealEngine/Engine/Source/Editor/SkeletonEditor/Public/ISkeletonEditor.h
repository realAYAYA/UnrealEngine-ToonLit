// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PersonaAssetEditorToolkit.h"
#include "IHasPersonaToolkit.h"

class IAnimationSequenceBrowser;

class ISkeletonEditor : public FPersonaAssetEditorToolkit, public IHasPersonaToolkit
{
	public:
	/** Get the asset browser we host */
	virtual IAnimationSequenceBrowser* GetAssetBrowser() const = 0;
};
