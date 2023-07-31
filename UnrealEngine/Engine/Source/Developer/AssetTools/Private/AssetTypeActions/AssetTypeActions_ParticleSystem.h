// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "Particles/ParticleSystem.h"


class FAssetTypeActions_ParticleSystem : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_ParticleSystem", "Cascade Particle System (Legacy)"); }
	virtual FColor GetTypeColor() const override { return FColor(255,255,255); }
	virtual UClass* GetSupportedClass() const override { return UParticleSystem::StaticClass(); }
	virtual void GetActions(const TArray<UObject*>& InObjects, struct FToolMenuSection& Section) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	
private:
	/** Handler for when Copy Parameters is selected */
	void ExecuteCopyParameters(TArray<TWeakObjectPtr<UParticleSystem>> Objects);

	/** Handler for when Convert to Seeded is selected */
	void ConvertToSeeded(TArray<TWeakObjectPtr<UParticleSystem>> Objects);
};
