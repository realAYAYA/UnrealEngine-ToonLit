// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

#include "DataLayerInstanceWithAsset.generated.h"

UCLASS(Config = Engine, PerObjectConfig, Within = WorldDataLayers, AutoCollapseCategories = ("Data Layer|Advanced"), AutoExpandCategories = ("Data Layer|Editor", "Data Layer|Advanced|Runtime"))
class ENGINE_API UDataLayerInstanceWithAsset : public UDataLayerInstance
{
	GENERATED_UCLASS_BODY()

		friend class UDataLayerConversionInfo;

public:
#if WITH_EDITOR
	static FName MakeName(const UDataLayerAsset* DeprecatedDataLayer);
	void OnCreated(const UDataLayerAsset* Asset);

	virtual bool IsLocked() const override;
	virtual bool IsReadOnly() const override;
	virtual bool AddActor(AActor* Actor) const override;
	virtual bool RemoveActor(AActor* Actor) const override;

	virtual bool Validate(IStreamingGenerationErrorHandler* ErrorHandler) const override;
#endif

	const UDataLayerAsset* GetAsset() const { return DataLayerAsset; }

	virtual EDataLayerType GetType() const override { return DataLayerAsset != nullptr ? DataLayerAsset->GetType() : EDataLayerType::Unknown; }

	virtual bool IsRuntime() const override { return DataLayerAsset != nullptr ? DataLayerAsset->IsRuntime() : false; }

	virtual FColor GetDebugColor() const override { return DataLayerAsset != nullptr ? DataLayerAsset->GetDebugColor() : FColor::Black; }

	virtual FString GetDataLayerShortName() const override { return DataLayerAsset != nullptr ? DataLayerAsset->GetName() : GetDataLayerFName().ToString(); }
	virtual FString GetDataLayerFullName() const override { return DataLayerAsset != nullptr ? DataLayerAsset->GetPathName() : GetDataLayerFName().ToString(); }

private:
	UPROPERTY(Category = "Data Layer", EditAnywhere)
	TObjectPtr<const UDataLayerAsset> DataLayerAsset;
};