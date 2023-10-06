// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Object.h"
#include "WorldPartition/DataLayer/DataLayerType.h"

#include "DataLayerInstance.generated.h"

#define DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED 1

class AWorldDataLayers;
class FText;
class IStreamingGenerationErrorHandler;
class UWorld;
class UDataLayerAsset;

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

UCLASS(Config = Engine, PerObjectConfig, BlueprintType, AutoCollapseCategories = ("Data Layer|Advanced"), AutoExpandCategories = ("Data Layer|Editor", "Data Layer|Advanced|Runtime"), MinimalAPI)
class UDataLayerInstance : public UObject
{
	GENERATED_UCLASS_BODY()

	friend class UDataLayerConversionInfo;
	friend class FDataLayerInstanceDetails;

public:
	ENGINE_API virtual void PostLoad() override;
	
	template<class T>
	T* GetTypedOuter() const
	{
		static_assert(!std::is_same<T, UWorld>::value, "Use GetOuterWorld instead");
		static_assert(!std::is_same<T, ULevel>::value, "Use GetOuterWorld()->PersistentLevel instead");
		static_assert(!std::is_same<T, AWorldDataLayers>::value, "Use GetOuterWorldDataLayers instead");
		return Super::GetTypedOuter<T>();
	}

	ENGINE_API UWorld* GetOuterWorld() const;
	ENGINE_API AWorldDataLayers* GetOuterWorldDataLayers() const;

#if WITH_EDITOR
	ENGINE_API virtual void PreEditUndo() override;
	ENGINE_API virtual void PostEditUndo() override;

	ENGINE_API void SetVisible(bool bIsVisible);
	ENGINE_API void SetIsInitiallyVisible(bool bIsInitiallyVisible);
	ENGINE_API void SetIsLoadedInEditor(bool bIsLoadedInEditor, bool bFromUserChange);
	void SetIsLocked(bool bInIsLocked) { bIsLocked = bInIsLocked; }

	bool IsInitiallyLoadedInEditor() const { return bIsInitiallyLoadedInEditor; }
	bool IsLoadedInEditor() const { return bIsLoadedInEditor; }
	ENGINE_API bool IsEffectiveLoadedInEditor() const;
	bool IsLoadedInEditorChangedByUserOperation() const { return bIsLoadedInEditorChangedByUserOperation; }
	void ClearLoadedInEditorChangedByUserOperation() { bIsLoadedInEditorChangedByUserOperation = false; }

	ENGINE_API const TCHAR* GetDataLayerIconName() const;

	ENGINE_API bool CanBeChildOf(const UDataLayerInstance* InParent, FText* OutReason = nullptr) const;
	ENGINE_API bool SetParent(UDataLayerInstance* InParent);

	ENGINE_API void SetChildParent(UDataLayerInstance* InParent);

	static ENGINE_API FText GetDataLayerText(const UDataLayerInstance* InDataLayer);

	ENGINE_API virtual bool IsLocked() const;
	ENGINE_API virtual bool IsReadOnly() const;
	ENGINE_API virtual bool CanEditChange(const FProperty* InProperty) const;
	
	ENGINE_API virtual bool CanUserAddActors() const;
	ENGINE_API virtual bool CanAddActor(AActor* Actor) const;
	ENGINE_API virtual bool AddActor(AActor* Actor) const;
	ENGINE_API virtual bool CanUserRemoveActors() const;
	ENGINE_API virtual bool CanRemoveActor(AActor* Actor) const;
	ENGINE_API virtual bool RemoveActor(AActor* Actor) const;

	ENGINE_API virtual bool CanBeInActorEditorContext() const;
	ENGINE_API bool IsInActorEditorContext() const;
	ENGINE_API bool AddToActorEditorContext();
	ENGINE_API bool RemoveFromActorEditorContext();

	virtual bool CanEditDataLayerShortName() const { return false; }

	virtual bool SupportsActorFilters() const { return false; }
	virtual bool IsIncludedInActorFilterDefault() const { return false; }

	// Whether the DataLayer was created by a user and can be deleted by a user.
	virtual bool IsUserManaged() const { return true; }

	ENGINE_API virtual bool Validate(IStreamingGenerationErrorHandler* ErrorHandler) const;

	UE_DEPRECATED(5.3, "Use CanEditShortName instead")
	virtual bool SupportRelabeling() const { return false; }

	UE_DEPRECATED(5.3, "Use SetShortName instead")
	virtual bool RelabelDataLayer(FName NewDataLayerLabel) { return false; }

	UE_DEPRECATED(5.3, "Use CanBeChildOf instead")
	bool CanParent(const UDataLayerInstance* InParent) const { return CanBeChildOf(InParent); }
#endif // WITH_EDITOR

	UFUNCTION(Category = "Data Layer", BlueprintCallable)
	virtual EDataLayerType GetType() const { return EDataLayerType::Unknown; }

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	ENGINE_API bool IsInitiallyVisible() const;

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	ENGINE_API bool IsVisible() const;

	UFUNCTION(Category = "Data Layer|Editor", BlueprintCallable)
	ENGINE_API bool IsEffectiveVisible() const;

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	virtual bool IsRuntime() const { return false; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	virtual FColor GetDebugColor() const { return FColor::Black; }

	UFUNCTION(Category = "Data Layer|Runtime", BlueprintCallable)
	EDataLayerRuntimeState GetInitialRuntimeState() const { return IsRuntime() ? InitialRuntimeState : EDataLayerRuntimeState::Unloaded; }

	virtual FString GetDataLayerShortName() const { return TEXT("Invalid Data Layer"); }
	virtual FString GetDataLayerFullName() const { return TEXT("Invalid Data Layer"); }

	virtual bool CanHaveChildDataLayers() const { return true; }
	virtual bool CanHaveParentDataLayer() const { return true; }

	const UDataLayerInstance* GetParent() const { return Parent; }
	UDataLayerInstance* GetParent() { return Parent; }

	ENGINE_API EDataLayerRuntimeState GetRuntimeState() const;
	ENGINE_API EDataLayerRuntimeState GetEffectiveRuntimeState() const;

	virtual const UDataLayerAsset* GetAsset() const { return nullptr; }

	const TArray<TObjectPtr<UDataLayerInstance>>& GetChildren() const { return Children; }
	ENGINE_API void ForEachChild(TFunctionRef<bool(const UDataLayerInstance*)> Operation) const;

#if DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED
	virtual FName GetDataLayerFName() const { return GetFName(); }

private:
	FName GetFName() const { return Super::GetFName(); }
	FString GetName() const { return Super::GetName();  }
public:
#endif // DATALAYER_TO_INSTANCE_RUNTIME_CONVERSION_ENABLED

private:
	ENGINE_API void AddChild(UDataLayerInstance* DataLayer);
	ENGINE_API bool SetRuntimeState(EDataLayerRuntimeState InState, bool bInIsRecursive = false) const;
	friend class UDataLayerManager;
	friend class FDataLayerUtils;

#if WITH_EDITOR
	ENGINE_API void RemoveChild(UDataLayerInstance* DataLayer);
#endif

protected:
#if WITH_EDITOR
	ENGINE_API bool IsParentDataLayerTypeCompatible(const UDataLayerInstance* InParent) const;

	virtual bool PerformAddActor(AActor* InActor) const { return false; }
	virtual bool PerformRemoveActor(AActor* InActor) const { return false;  }
	virtual void PerformSetDataLayerShortName(const FString& InNewShortName) {}
#endif

#if WITH_EDITORONLY_DATA
	/** Whether actors associated with the DataLayer are visible in the viewport */
	UPROPERTY(Transient)
	uint32 bIsVisible : 1;

	/** Whether actors associated with the Data Layer should be initially visible in the viewport when loading the map */
	UPROPERTY(Category = "Editor", EditAnywhere)
	uint32 bIsInitiallyVisible : 1;

	/** Determines the default value of the data layer's loaded state in editor if it hasn't been changed in data layer outliner by the user */
	UPROPERTY(Category = "Editor", EditAnywhere, meta = (DisplayName = "Is Initially Loaded"))
	uint32 bIsInitiallyLoadedInEditor : 1;

	/** Wheter the data layer is loaded in editor (user setting) */
	UPROPERTY(Transient)
	uint32 bIsLoadedInEditor : 1;
	uint32 bUndoIsLoadedInEditor : 1;

	/** Whether this data layer editor visibility was changed by a user operation */
	UPROPERTY(Transient)
	uint32 bIsLoadedInEditorChangedByUserOperation : 1;

	/** Whether this data layer is locked, which means the user can't change actors assignation, remove or rename it */
	UPROPERTY()
	uint32 bIsLocked : 1;
#endif

	UPROPERTY(Category = "Runtime", EditAnywhere)
	EDataLayerRuntimeState InitialRuntimeState;

private:
	UPROPERTY()
	TObjectPtr<UDataLayerInstance> Parent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDataLayerInstance>> Children;
};
