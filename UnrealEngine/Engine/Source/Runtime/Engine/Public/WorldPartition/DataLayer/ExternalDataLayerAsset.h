// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorldPartition/DataLayer/DataLayerAsset.h"
#include "WorldPartition/DataLayer/ExternalDataLayerUID.h"

#include "ExternalDataLayerAsset.generated.h"

struct FAssetData;

UCLASS(BlueprintType, editinlinenew)
class ENGINE_API UExternalDataLayerAsset : public UDataLayerAsset
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	static constexpr FColor EditorUXColor = FColor(255, 167, 26);
#endif

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	//~ End UObject Interface

	static bool GetAssetRegistryInfoFromPackage(const FAssetData& InAsset, FExternalDataLayerUID& OutExternalDataLayerUID);
#endif

	//~ Begin UDataLayerAsset Interface
#if WITH_EDITOR
	virtual void OnCreated() override;
	virtual bool CanEditDataLayerType() const override { return false; }
#endif
	virtual EDataLayerType GetType() const override { return EDataLayerType::Runtime; }
	//~ End UDataLayerAsset Interface

	const FExternalDataLayerUID& GetUID() const { return UID; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Data Layer", AdvancedDisplay, DuplicateTransient, meta = (DisplayName = "External Data Layer UID"))
	FExternalDataLayerUID UID;
};