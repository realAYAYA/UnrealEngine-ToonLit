// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

/// <summary>
/// AssetTypeActions_XXX class files are unreal asset content actions classes. These classes controls actions like double clicking and right clicking the asset.
/// For double click : We will load the tsx in TextureScriptingEditor mode. 
/// </summary>
class FAssetTypeActions_TSX : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	UClass*		GetSupportedClass() const override;
	FText		GetName() const override;
	FColor		GetTypeColor() const override;
	uint32		GetCategories() override;

	void		OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	
};
