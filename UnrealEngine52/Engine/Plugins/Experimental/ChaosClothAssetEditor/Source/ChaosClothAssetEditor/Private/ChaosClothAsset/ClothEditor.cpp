// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/ClothEditor.h"
#include "ChaosClothAsset/ClothEditorToolkit.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ClothEditor)

TSharedPtr<FBaseAssetToolkit> UChaosClothAssetEditor::CreateToolkit()
{
	TSharedPtr<FChaosClothAssetEditorToolkit> Toolkit = MakeShared<FChaosClothAssetEditorToolkit>(this);
	return Toolkit;
}

