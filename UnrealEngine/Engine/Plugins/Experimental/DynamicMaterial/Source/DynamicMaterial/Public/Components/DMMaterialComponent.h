// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "DMDefs.h"
#include "Misc/NotifyHook.h"
#include "DMMaterialComponent.generated.h"

class UDynamicMaterialModel;
enum class EDMUpdateType : uint8;
struct FDMComponentPathSegment;
struct FDMComponentPath;

UENUM(BlueprintType)
enum class EDMComponentLifetimeState : uint8
{
	Created,
	Added,
	Removed
};

/**
 * The base class for all material components. Has a few useful things.
 */
UCLASS(Abstract, BlueprintType, meta = (DisplayName = "Material Designer Component"))
class DYNAMICMATERIAL_API UDMMaterialComponent : public UObject, public FNotifyHook
{
	GENERATED_BODY()

	friend class UDynamicMaterialEditorSettings;

public:
	UDMMaterialComponent();

	/** Checks object flags and IsValid() */
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsComponentValid() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual bool IsRootComponent() const { return false; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialComponent* GetComponentByPath(const FString& InPath) const;

	UDMMaterialComponent* GetComponentByPath(FDMComponentPath& InPath) const;

	template<typename InComponentClass>
	UDMMaterialComponent* GetComponentByPath(FDMComponentPath& InPath) const
	{
		return Cast<InComponentClass>(GetComponentByPath(InPath));
	}

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual void Update(EDMUpdateType InUpdateType);

#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	/** Returns the complete path from the model to this component. */
	FString GetComponentPath() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	virtual UDMMaterialComponent* GetParentComponent() const;

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	UDMMaterialComponent* GetTypedParent(UClass* InParentClass, bool bInAllowSubclasses) const;

	template<class InParentClass>
	InParentClass* GetTypedParent(bool bInAllowSubclasses) const
	{
		return Cast<InParentClass>(GetTypedParent(InParentClass::StaticClass(), bInAllowSubclasses));
	}

	/* Returns a description of this class/object. */
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual FText GetComponentDescription() const;

	/** Returns true if we can attempt to un-dirty this component. */
	static bool CanClean();

	/** Called to prevent cleaning for MinTimeBeforeClean. **/
	static void PreventClean(double DelayFor = MinTimeBeforeClean);

	/** Returns true if this component has been marked dirty. */
	bool NeedsClean();

	/** Performs whatever operation is involved in cleaning this component. */
	virtual void DoClean();

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMComponentLifetimeState GetComponentState() const { return ComponentState; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsComponentCreated() const { return ComponentState == EDMComponentLifetimeState::Created; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool HasComponentBeenCreated() const
	{
		// This is kind of a "useless" check, a component has _always_ been created.
		return true;
	}

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsComponentAdded() const { return ComponentState == EDMComponentLifetimeState::Added; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool HasComponentBeenAdded() const { return ComponentState >= EDMComponentLifetimeState::Added; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool IsComponentRemoved() const { return ComponentState == EDMComponentLifetimeState::Removed; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool HasComponentBeenRemoved() const { return ComponentState >= EDMComponentLifetimeState::Removed; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetComponentState(EDMComponentLifetimeState NewState);

	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostLoad() override;
	//~ End UObject

	/*** Begin FNotifyHook */
	virtual void NotifyPreChange(class FEditPropertyChain* PropertyAboutToChange) {}
	/*** End FNotifyHook */

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const TArray<FName>& GetEditableProperties() const { return EditableProperties; }

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	virtual bool IsPropertyVisible(FName Property) const { return true; }

	/** Returns the part of the component representing just this object */
	virtual FString GetComponentPathComponent() const;

	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent);

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnUpdate, UDMMaterialComponent*, EDMUpdateType)
	FOnUpdate& GetOnUpdate() { return OnUpdate; }

	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnLifetimeStateChanged, UDMMaterialComponent*, EDMComponentLifetimeState)
	FOnLifetimeStateChanged& GetOnAdded() { return OnAdded; }
	FOnLifetimeStateChanged& GetOnRemoved() { return OnRemoved; }
#endif

protected:
	static const double MinTimeBeforeClean;
	static double MinCleanTime;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	EDMComponentLifetimeState ComponentState;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Material Designer")
	bool bComponentDirty;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient, Category = "Material Designer")
	TArray<FName> EditableProperties;

	FOnUpdate OnUpdate;
	FOnLifetimeStateChanged OnAdded;
	FOnLifetimeStateChanged OnRemoved;
#endif

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UObject* GetOuterSafe() const;

	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const;

	void MarkComponentDirty();

#if WITH_EDITOR
	/** Allows this object to modify the child path when generating a path. */
	virtual void GetComponentPathInternal(TArray<FString>& OutChildComponentPathComponents) const;

	virtual void OnComponentStateChange(EDMComponentLifetimeState NewState);
	virtual void OnComponentAdded();
	virtual void OnComponentRemoved();
#endif
};
