// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetTypeActions_Base.h"
#include "ActorFolder.h"

class FAssetTypeActions_ActorFolder : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ActorFolder", "Actor Folder"); }
	virtual FColor GetTypeColor() const override { return FColor(182, 143, 85); }
	virtual UClass* GetSupportedClass() const override { return UActorFolder::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::None; }
	virtual FString GetObjectDisplayName(UObject* Object) const override { UActorFolder* ActorFolder = CastChecked<UActorFolder>(Object); return ActorFolder->GetDisplayName(); }
};
