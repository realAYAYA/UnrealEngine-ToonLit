// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "ActorDataLayer.h"
#include "Math/Color.h"
#include "WorldPartition/DataLayer/DataLayerInstance.h"

#include "DataLayer.generated.h"

UENUM(BlueprintType, meta = (ScriptName = "DataLayerStateType"))
enum class UE_DEPRECATED(5.0, "Use EDataLayerRuntimeState instead.") EDataLayerState : uint8
{
	Unloaded,
	Loaded,
	Activated
};

// Used for debugging
bool inline GetDataLayerRuntimeStateFromName(const FString& InStateName, EDataLayerRuntimeState& OutState)
{
	if (InStateName.Equals(GetDataLayerRuntimeStateName(EDataLayerRuntimeState::Unloaded), ESearchCase::IgnoreCase))
	{
		OutState = EDataLayerRuntimeState::Unloaded;
		return true;
	}
	else if (InStateName.Equals(GetDataLayerRuntimeStateName(EDataLayerRuntimeState::Loaded), ESearchCase::IgnoreCase))
	{
		OutState = EDataLayerRuntimeState::Loaded;
		return true;
	}
	else if (InStateName.Equals(GetDataLayerRuntimeStateName(EDataLayerRuntimeState::Activated), ESearchCase::IgnoreCase))
	{
		OutState = EDataLayerRuntimeState::Activated;
		return true;
	}
	return false;
}

static_assert(EDataLayerRuntimeState::Unloaded < EDataLayerRuntimeState::Loaded && EDataLayerRuntimeState::Loaded < EDataLayerRuntimeState::Activated, "Streaming Query code is dependent on this being true");


static_assert(DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED, "UDEPRECATED_DataLayer class is deprecated and needs to be deleted.");
class UE_DEPRECATED(5.1, "Use UDataLayerInstance & UDataLayerAsset to create DataLayers") UDEPRECATED_DataLayer;

UCLASS(Config = Engine, PerObjectConfig, Within = WorldDataLayers, BlueprintType, Deprecated, MinimalAPI)
class UDEPRECATED_DataLayer : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UDeprecatedDataLayerInstance;

public:
#if WITH_EDITOR
	ENGINE_API virtual bool CanEditChange(const FProperty* Property) const;
	ENGINE_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	ENGINE_API void SetVisible(bool bIsVisible);
	ENGINE_API void SetIsInitiallyVisible(bool bIsInitiallyVisible);
	ENGINE_API void SetIsRuntime(bool bIsRuntime);
	ENGINE_API void SetIsLoadedInEditor(bool bIsLoadedInEditor, bool bFromUserChange);
	void SetIsLocked(bool bInIsLocked) { bIsLocked = bInIsLocked; }

	ENGINE_API bool IsLocked() const;
	bool IsInitiallyLoadedInEditor() const { return bIsInitiallyLoadedInEditor; }
	bool IsLoadedInEditor() const { return bIsLoadedInEditor; }
	ENGINE_API bool IsEffectiveLoadedInEditor() const;
	bool IsLoadedInEditorChangedByUserOperation() const { return bIsLoadedInEditorChangedByUserOperation; }
	void ClearLoadedInEditorChangedByUserOperation() { bIsLoadedInEditorChangedByUserOperation = false; }

	ENGINE_API bool CanParent(const UDEPRECATED_DataLayer* InParent) const;
	ENGINE_API void SetParent(UDEPRECATED_DataLayer* InParent);
	ENGINE_API void SetChildParent(UDEPRECATED_DataLayer* InParent);

	static ENGINE_API FText GetDataLayerText(const UDEPRECATED_DataLayer* InDataLayer);
	ENGINE_API const TCHAR* GetDataLayerIconName() const;
#endif
	const TArray<TObjectPtr<UDEPRECATED_DataLayer>>& GetChildren() const { return Children_DEPRECATED; }
	ENGINE_API void ForEachChild(TFunctionRef<bool(const UDEPRECATED_DataLayer*)> Operation) const;

	const UDEPRECATED_DataLayer* GetParent() const { return Parent_DEPRECATED; }
	UDEPRECATED_DataLayer* GetParent() { return Parent_DEPRECATED; }

	ENGINE_API virtual void PostLoad() override;

	UFUNCTION(Category = "Data Layer", BlueprintCallable)
	bool Equals(const FActorDataLayer& ActorDataLayer) const { return ActorDataLayer.Name == GetFName(); }

	UFUNCTION(Category = "Data Layer", BlueprintCallable)
	FName GetDataLayerLabel() const { return DataLayerLabel; }

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	ENGINE_API bool IsInitiallyVisible() const;

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	ENGINE_API bool IsVisible() const;

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	ENGINE_API bool IsEffectiveVisible() const;

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	FColor GetDebugColor() const { return DebugColor; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	bool IsRuntime() const { return bIsRuntime; }
	
	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	EDataLayerRuntimeState GetInitialRuntimeState() const { return IsRuntime() ? InitialRuntimeState : EDataLayerRuntimeState::Unloaded; }

	//~ Begin Deprecated

	UE_DEPRECATED(5.0, "Use IsRuntime() instead.")
	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Use IsRuntime instead"))
	bool IsDynamicallyLoaded() const { return IsRuntime(); }

	UE_DEPRECATED(5.0, "Use GetInitialRuntimeState() instead.")
	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Use GetInitialRuntimeState instead"))
	bool IsInitiallyActive() const { return IsRuntime() && GetInitialRuntimeState() == EDataLayerRuntimeState::Activated; }

	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use GetInitialRuntimeState() instead.")
	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable, meta = (DeprecatedFunction, DeprecationMessage = "Use GetInitialRuntimeState instead"))
	EDataLayerState GetInitialState() const { return (EDataLayerState)GetInitialRuntimeState(); }
	PRAGMA_ENABLE_DEPRECATION_WARNINGS

	//~ End Deprecated
private:

	void AddChild(UDEPRECATED_DataLayer* DataLayer);
#if WITH_EDITOR
	ENGINE_API void RemoveChild(UDEPRECATED_DataLayer* DataLayer);
	ENGINE_API void PropagateIsRuntime();
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	uint32 bIsInitiallyActive_DEPRECATED : 1;

	/** Whether actors associated with the DataLayer are visible in the viewport */
	UPROPERTY(Transient)
	uint32 bIsVisible : 1;

	/** Whether actors associated with the Data Layer should be initially visible in the viewport when loading the map */
	UPROPERTY(Category = "Data Layer|Editor", EditAnywhere)
	uint32 bIsInitiallyVisible : 1;

	/** Determines the default value of the data layer's loaded state in editor if it hasn't been changed in data layer outliner by the user */
	UPROPERTY(Category = "Data Layer|Editor", EditAnywhere, meta = (DisplayName = "Is Initially Loaded"))
	uint32 bIsInitiallyLoadedInEditor : 1;

	/** Wheter the data layer is loaded in editor (user setting) */
	UPROPERTY(Transient)
	uint32 bIsLoadedInEditor : 1;

	/** Whether this data layer editor visibility was changed by a user operation */
	UPROPERTY(Transient)
	uint32 bIsLoadedInEditorChangedByUserOperation : 1;

	/** Whether this data layer is locked, which means the user can't change actors assignation, remove or rename it */
	UPROPERTY()
	uint32 bIsLocked : 1;
#endif

	/** The display name of the Data Layer */
	UPROPERTY()
	FName DataLayerLabel;

	/** Whether the Data Layer affects actor runtime loading */
	UPROPERTY(Category = "Data Layer|Advanced", EditAnywhere)
	uint32 bIsRuntime : 1;

	UPROPERTY(Category = "Data Layer|Advanced|Runtime", EditAnywhere, meta = (EditConditionHides, EditCondition = "bIsRuntime"))
	EDataLayerRuntimeState InitialRuntimeState;

	UPROPERTY(Category = "Data Layer|Editor", EditAnywhere)
	FColor DebugColor;

	UPROPERTY()
	TObjectPtr<UDEPRECATED_DataLayer> Parent_DEPRECATED;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDEPRECATED_DataLayer>> Children_DEPRECATED;
};
