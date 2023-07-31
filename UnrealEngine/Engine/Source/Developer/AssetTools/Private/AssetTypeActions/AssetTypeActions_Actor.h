// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"
#include "GameFramework/Actor.h"

class FAssetTypeActions_Actor : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Actor", "Actor"); }
	virtual FColor GetTypeColor() const override { return FColor(0,232,0); }
	virtual UClass* GetSupportedClass() const override { return AActor::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::None; }
	virtual FString GetObjectDisplayName(UObject* Object) const override { return CastChecked<AActor>(Object)->GetActorLabel(); }
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
};
