// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PersonaAssetEditorToolkit.h"
#include "IHasPersonaToolkit.h"

class ISkeletalMeshEditorBinding;

class ISkeletalMeshEditor : public FPersonaAssetEditorToolkit, public IHasPersonaToolkit
{
public:
	virtual TSharedPtr<ISkeletalMeshEditorBinding> GetBinding() = 0;
};

