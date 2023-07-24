// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "WorldPartition/DataLayer/DataLayerType.h"

#include "DataLayerInstance.generated.h"

#define DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED 1

class IStreamingGenerationErrorHandler;

UENUM(BlueprintType)
enum class EDataLayerRuntimeState : uint8
{
	// Unloaded
	Unloaded,
	
	// Loaded (meaning loaded but not visible)
	Loaded,

	// Activated (meaning loaded and visible)
	Activated
};

const inline TCHAR* GetDataLayerRuntimeStateName(EDataLayerRuntimeState State)
{
	switch (State)
	{
	case EDataLayerRuntimeState::Unloaded: return TEXT("Unloaded");
	case EDataLayerRuntimeState::Loaded: return TEXT("Loaded");
	case EDataLayerRuntimeState::Activated: return TEXT("Activated");
	default: check(0);
	}
	return TEXT("Invalid");
}

UCLASS(Config = Engine, PerObjectConfig, Within = WorldDataLayers, BlueprintType, AutoCollapseCategories = ("Data Layer|Advanced"), AutoExpandCategories = ("Data Layer|Editor", "Data Layer|Advanced|Runtime"))
class ENGINE_API UDataLayerInstance : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UDataLayerConversionInfo;
	friend class FDataLayerInstanceDetails;

public:
	virtual void PostLoad() override;

#if WITH_EDITOR
	void SetVisible(bool bIsVisible);
	void SetIsInitiallyVisible(bool bIsInitiallyVisible);
	void SetIsLoadedInEditor(bool bIsLoadedInEditor, bool bFromUserChange);
	void SetIsLocked(bool bInIsLocked) { bIsLocked = bInIsLocked; }

	bool IsInitiallyLoadedInEditor() const { return bIsInitiallyLoadedInEditor; }
	bool IsLoadedInEditor() const { return bIsLoadedInEditor; }
	bool IsEffectiveLoadedInEditor() const;
	bool IsLoadedInEditorChangedByUserOperation() const { return bIsLoadedInEditorChangedByUserOperation; }
	void ClearLoadedInEditorChangedByUserOperation() { bIsLoadedInEditorChangedByUserOperation = false; }

	const TCHAR* GetDataLayerIconName() const;

	bool CanParent(const UDataLayerInstance* InParent) const;
	bool IsDataLayerTypeValidToParent(EDataLayerType ParentDataLayerType) const;
	bool SetParent(UDataLayerInstance* InParent);

	void SetChildParent(UDataLayerInstance* InParent);

	static FText GetDataLayerText(const UDataLayerInstance* InDataLayer);

	virtual bool IsLocked() const;
	virtual bool IsReadOnly() const { return false; }
	virtual bool CanEditChange(const FProperty* InProperty) const;
	virtual bool AddActor(AActor* Actor) const { return false; }
	virtual bool RemoveActor(AActor* Actor) const { return false; }

	bool IsInActorEditorContext() const;
	bool AddToActorEditorContext();
	bool RemoveFromActorEditorContext();

	virtual bool Validate(IStreamingGenerationErrorHandler* ErrorHandler) const;

#if DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED
	virtual bool SupportRelabeling() const { return false; }
	virtual bool RelabelDataLayer(FName NewDataLayerLabel) { return false; }
#endif // DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED

#endif // WITH_EDITOR

	UFUNCTION(Category = "Data Layer", BlueprintCallable)
	virtual EDataLayerType GetType() const { return EDataLayerType::Unknown; }

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	bool IsInitiallyVisible() const;

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	bool IsVisible() const;

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	bool IsEffectiveVisible() const;

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	virtual bool IsRuntime() const { return false; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	virtual FColor GetDebugColor() const { return FColor::Black; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	EDataLayerRuntimeState GetInitialRuntimeState() const { return IsRuntime() ? InitialRuntimeState : EDataLayerRuntimeState::Unloaded; }

	virtual FString GetDataLayerShortName() const { return TEXT("Invalid Data Layer"); }
	virtual FString GetDataLayerFullName() const { return TEXT("Invalid Data Layer"); }

	const UDataLayerInstance* GetParent() const { return Parent; }
	UDataLayerInstance* GetParent() { return Parent; }

	const TArray<TObjectPtr<UDataLayerInstance>>& GetChildren() const { return Children; }
	void ForEachChild(TFunctionRef<bool(const UDataLayerInstance*)> Operation) const;

#if DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED
	virtual FName GetDataLayerFName() const { return GetFName(); }

private:
	FName GetFName() const { return Super::GetFName(); }
	FString GetName() const { return Super::GetName();  }
public:
#endif // DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED

private:
	void AddChild(UDataLayerInstance* DataLayer);
#if WITH_EDITOR
	void RemoveChild(UDataLayerInstance* DataLayer);
#endif

protected:
#if WITH_EDITORONLY_DATA
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

	UPROPERTY(Category = "Data Layer|Advanced|Runtime", EditAnywhere)
	EDataLayerRuntimeState InitialRuntimeState;

private:
	UPROPERTY()
	TObjectPtr<UDataLayerInstance> Parent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDataLayerInstance>> Children;

};