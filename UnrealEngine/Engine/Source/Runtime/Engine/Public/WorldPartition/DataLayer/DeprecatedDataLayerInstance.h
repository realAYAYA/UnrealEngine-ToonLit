// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/ActorDataLayer.h"
#include "DeprecatedDataLayerInstance.generated.h"

class UDEPRECATED_DataLayer;

// Class used for Runtime Conversion of the Deprecated UDataLayer Class to UDataLayerInstance + UDataLayerAsset.
// This class is not to be inherited. It is solely used by AWorldDatalayers to convert UDataLayers to UDataLayerInstances on Level Boot.
// You will need to run the DataLayerToAsset CommandLet to convert the deprecated datalayers to UDataLayerAssets and UDataLayerInstanceWithAsset.
UCLASS(Config = Engine, PerObjectConfig, Within = WorldDataLayers, AutoCollapseCategories = ("Data Layer|Advanced"), AutoExpandCategories = ("Data Layer|Editor", "Data Layer|Advanced|Runtime"), MinimalAPI)
class UDeprecatedDataLayerInstance final : public UDataLayerInstance
{
	static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "UDeprecatedDataLayerInstance is deprecated and needs to be deleted.");

	GENERATED_UCLASS_BODY()

#if WITH_EDITOR
	//~ Begin UObject Interface
	ENGINE_API virtual void PostLoad() override;
	//~ End UObject Interface
#endif

public:
#if WITH_EDITOR
	static ENGINE_API FName MakeName();
	ENGINE_API void OnCreated();
	FActorDataLayer GetActorDataLayer() const;

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	ENGINE_API static FName MakeName(const UDEPRECATED_DataLayer* DeprecatedDataLayer);
	ENGINE_API void OnCreated(const UDEPRECATED_DataLayer* DeprecatedDataLayer);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
	
	FName GetDataLayerLabel() const { return Label; }

private:
#if WITH_EDITOR
	//~ Begin UDataLayerInstance Interface
	ENGINE_API virtual bool PerformAddActor(AActor* InActor) const override;
	ENGINE_API virtual bool PerformRemoveActor(AActor* InActor) const override;
	ENGINE_API virtual bool RelabelDataLayer(FName NewDataLayerLabel) override;
	virtual bool SupportRelabeling() const override { return true; }
	virtual bool CanEditDataLayerShortName() const override { return true; }
	virtual void PerformSetDataLayerShortName(const FString& InNewShortName) { Label = *InNewShortName; }
	//~ Endif UDataLayerInstance Interface
#endif

	//~ Begin UDataLayerInstance Interface
	virtual FName GetDataLayerFName() const override { return !DeprecatedDataLayerFName.IsNone() ? DeprecatedDataLayerFName : Super::GetDataLayerFName(); }
	virtual EDataLayerType GetType() const override { return DataLayerType; }
	virtual bool IsRuntime() const override { return DataLayerType == EDataLayerType::Runtime; }
	virtual FColor GetDebugColor() const override { return DebugColor; }
	virtual FString GetDataLayerShortName() const override { return Label.ToString(); }
	virtual FString GetDataLayerFullName() const override { return TEXT("Deprecated_") + Label.ToString(); }
	//~ End UDataLayerInstance Interface

private:
	UPROPERTY()
	FName Label;

	UPROPERTY()
	FName DeprecatedDataLayerFName;

	UPROPERTY(Category = "Data Layer", EditAnywhere)
	EDataLayerType DataLayerType;

	UPROPERTY(Category = "Data Layer|Runtime", EditAnywhere, meta = (EditConditionHides, EditCondition = "DataLayerType == EDataLayerType::Runtime"))
	FColor DebugColor;

	friend class AWorldDataLayers;
	friend class UDataLayerConversionInfo;
	friend class UDataLayerToAssetCommandletContext;
	friend class UDataLayerToAssetCommandlet;
};
