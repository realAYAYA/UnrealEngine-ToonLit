// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WorldPartition/DataLayer/DataLayerInstance.h"

#include "DeprecatedDataLayerInstance.generated.h"

class UDEPRECATED_DataLayer;

// Class used for Runtime Conversion of the Deprecated UDataLayer Class to UDataLayerInstance + UDataLayerAsset.
// This class is not to be inherited. It is solely used by AWorldDatalayers to convert UDataLayers to UDataLayerInstances on Level Boot.
// You will need to run the DataLayerToAsset CommandLet to convert the deprecated datalayers to UDataLayerAssets and UDataLayerInstanceWithAsset.
UCLASS(Config = Engine, PerObjectConfig, Within = WorldDataLayers, AutoCollapseCategories = ("Data Layer|Advanced"), AutoExpandCategories = ("Data Layer|Editor", "Data Layer|Advanced|Runtime"))
class ENGINE_API UDeprecatedDataLayerInstance final : public UDataLayerInstance
{
	static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "UDeprecatedDataLayerInstance is deprecated and needs to be deleted.");

	GENERATED_UCLASS_BODY()

	friend class AWorldDataLayers;
	friend class UDataLayerConversionInfo;
	friend class UDataLayerToAssetCommandletContext;
	friend class UDataLayerToAssetCommandlet;

#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostLoad() override;
	//~ End UObject Interface
#endif

public:
#if WITH_EDITOR
	static FName MakeName();
	void OnCreated();

	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	static FName MakeName(const UDEPRECATED_DataLayer* DeprecatedDataLayer);
	void OnCreated(const UDEPRECATED_DataLayer* DeprecatedDataLayer);
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	void SetDataLayerLabel(FName InDataLayerLabel);
#endif
	
	FName GetDataLayerLabel() const { return Label; }

private:
#if WITH_EDITOR
	virtual bool AddActor(AActor* Actor) const override;
	virtual bool RemoveActor(AActor* Actor) const override;

	virtual bool SupportRelabeling() const override { return true; }
	virtual bool RelabelDataLayer(FName NewDataLayerLabel) override;
#endif

	virtual FName GetDataLayerFName() const override { return !DeprecatedDataLayerFName.IsNone() ? DeprecatedDataLayerFName : Super::GetDataLayerFName(); }

	virtual EDataLayerType GetType() const override { return DataLayerType; }

	virtual bool IsRuntime() const override { return DataLayerType == EDataLayerType::Runtime; }

	virtual FColor GetDebugColor() const override { return DebugColor; }

	virtual FString GetDataLayerShortName() const override { return Label.ToString(); }
	virtual FString GetDataLayerFullName() const override { return TEXT("Deprecated_") + Label.ToString(); }

private:
	UPROPERTY()
	FName Label;

	UPROPERTY()
	FName DeprecatedDataLayerFName;

	UPROPERTY(Category = "Data Layer", EditAnywhere)
	EDataLayerType DataLayerType;

	UPROPERTY(Category = "Data Layer|Runtime", EditAnywhere, meta = (EditConditionHides, EditCondition = "DataLayerType == EDataLayerType::Runtime"))
	FColor DebugColor;
};
