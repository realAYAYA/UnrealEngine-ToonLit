// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "WorldPartition/DataLayer/DataLayerInstance.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

#include "DataLayerInstanceWithAsset.generated.h"

UCLASS(Config = Engine, PerObjectConfig, AutoCollapseCategories = ("Data Layer|Advanced"), AutoExpandCategories = ("Data Layer|Editor", "Data Layer|Advanced|Runtime"), MinimalAPI)
class UDataLayerInstanceWithAsset : public UDataLayerInstance
{
	GENERATED_UCLASS_BODY()

		friend class UDataLayerConversionInfo;

public:
#if WITH_EDITOR
	static ENGINE_API FName MakeName(const UDataLayerAsset* DeprecatedDataLayer);
	static ENGINE_API TSubclassOf<UDataLayerInstanceWithAsset> GetDataLayerInstanceClass();
	ENGINE_API void OnCreated(const UDataLayerAsset* Asset);

	ENGINE_API virtual bool CanEditChange(const FProperty* InProperty) const;
	ENGINE_API virtual void PreEditUndo() override;
	ENGINE_API virtual void PostEditUndo() override;
	ENGINE_API virtual bool IsLocked() const override;
	ENGINE_API virtual bool IsReadOnly() const override;
	ENGINE_API virtual bool CanAddActor(AActor* InActor) const override;
	ENGINE_API virtual bool CanRemoveActor(AActor* InActor) const override;

	ENGINE_API virtual bool Validate(IStreamingGenerationErrorHandler* ErrorHandler) const override;
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	ENGINE_API bool SupportsActorFilters() const;
	ENGINE_API bool IsIncludedInActorFilterDefault() const;
#endif

	const UDataLayerAsset* GetAsset() const override { return DataLayerAsset; }

	virtual EDataLayerType GetType() const override { return DataLayerAsset != nullptr ? DataLayerAsset->GetType() : EDataLayerType::Unknown; }

	virtual bool IsRuntime() const override { return DataLayerAsset != nullptr ? DataLayerAsset->IsRuntime() : false; }

	virtual FColor GetDebugColor() const override { return DataLayerAsset != nullptr ? DataLayerAsset->GetDebugColor() : FColor::Black; }

	virtual FString GetDataLayerShortName() const override { return DataLayerAsset != nullptr ? DataLayerAsset->GetName() : GetDataLayerFName().ToString(); }
	virtual FString GetDataLayerFullName() const override { return DataLayerAsset != nullptr ? DataLayerAsset->GetPathName() : GetDataLayerFName().ToString(); }

protected:
#if WITH_EDITOR
	ENGINE_API virtual bool PerformAddActor(AActor* InActor) const;
	ENGINE_API virtual bool PerformRemoveActor(AActor* InActor) const;
#endif

private:
	UPROPERTY(Category = "Data Layer", EditAnywhere)
	TObjectPtr<const UDataLayerAsset> DataLayerAsset;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Category = "Data Layer|Actor Filter", EditAnywhere, meta = (DisplayName = "Is Included", ToolTip = "Whether actors assigned to this DataLayer are included by default when used in a filter"))
	bool bIsIncludedInActorFilterDefault;
#endif

#if WITH_EDITOR
	// Used to compare state pre/post undo
	TObjectPtr<const UDataLayerAsset> CachedDataLayerAsset;
#endif
};
