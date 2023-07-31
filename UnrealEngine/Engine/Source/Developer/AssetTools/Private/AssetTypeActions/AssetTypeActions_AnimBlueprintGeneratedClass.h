// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_BlueprintGeneratedClass.h"
#include "Animation/AnimBlueprintGeneratedClass.h"

class UBlueprintGeneratedClass;
struct FAssetData;
class IClassTypeActions;
class UFactory;

class FAssetTypeActions_AnimBlueprintGeneratedClass : public FAssetTypeActions_BlueprintGeneratedClass
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_AnimBlueprintGeneratedClass", "Compiled Anim Blueprint Class"); }
	virtual FColor GetTypeColor() const override { return FColor(240, 156, 0); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Animation; }
	virtual UClass* GetSupportedClass() const override { return UAnimBlueprintGeneratedClass::StaticClass(); }
	// End IAssetTypeActions Implementation
};
