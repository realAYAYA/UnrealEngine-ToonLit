// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaDefs.h"
#include "Delegates/IDelegateInstance.h"
#include "Modifiers/ActorModifierCoreExtension.h"
#include "UObject/WeakInterfacePtr.h"
#include "AvaSceneTreeUpdateModifierExtension.generated.h"

enum class EAvaOutlinerHierarchyChangeType : uint8;
enum class EAvaReferenceContainer : uint8;

USTRUCT(BlueprintType)
struct FAvaSceneTreeActor
{
	friend class FAvaSceneTreeUpdateModifierExtension;

	GENERATED_BODY()

	FAvaSceneTreeActor() = default;

	explicit FAvaSceneTreeActor(AActor* InActor)
		: ReferenceContainer(EAvaReferenceContainer::Other)
		, ReferenceActorWeak(InActor)
	{}

	/** The method for finding a reference actor based on it's position in the parent's hierarchy */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Outliner")
	EAvaReferenceContainer ReferenceContainer = EAvaReferenceContainer::Other;

	/** The actor being followed by the modifier. This is user selectable if the Reference Container is set to "Other" */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Outliner", meta=(DisplayName="Reference Actor", EditCondition = "ReferenceContainer == EAvaReferenceContainer::Other"))
	TWeakObjectPtr<AActor> ReferenceActorWeak = nullptr;

	/** If true, will search for the next visible actor based on the selected reference container */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Outliner", meta=(EditCondition="ReferenceContainer != EAvaReferenceContainer::Other", EditConditionHides))
	bool bSkipHiddenActors = false;

	bool operator==(const FAvaSceneTreeActor& InOther) const
	{
		return ReferenceContainer == InOther.ReferenceContainer
			&& ReferenceActorWeak == InOther.ReferenceActorWeak
			&& bSkipHiddenActors == InOther.bSkipHiddenActors;
	}

protected:
	/** All children of reference actor to compare with new set children for changes */
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> ReferenceActorChildrenWeak;

	/** Direct children of reference actor where order counts */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> ReferenceActorDirectChildrenWeak;

	/** Tracked references actors, if we skip hidden actors, we still need to track those for visibility changes, can be rebuilt */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> ReferenceActorsWeak;

	/** Parents of the reference actor */
	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AActor>> ReferenceActorParentsWeak;

	/** Actor from which we start resolving this reference actor */
	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> LocalActorWeak = nullptr;
};

UINTERFACE(MinimalAPI, NotBlueprintType, meta=(CannotImplementInterfaceInBlueprint))
class UAvaSceneTreeUpdateHandler : public UInterface
{
	GENERATED_BODY()
};

/** Implement this interface to handle extension event */
class IAvaSceneTreeUpdateHandler
{
	GENERATED_BODY()

public:
	virtual void OnSceneTreeTrackedActorChanged(int32 InIdx, AActor* InPreviousActor, AActor* InNewActor) = 0;

	virtual void OnSceneTreeTrackedActorChildrenChanged(int32 InIdx, const TSet<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TSet<TWeakObjectPtr<AActor>>& InNewChildrenActors) = 0;

	virtual void OnSceneTreeTrackedActorDirectChildrenChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousChildrenActors, const TArray<TWeakObjectPtr<AActor>>& InNewChildrenActors) = 0;

	virtual void OnSceneTreeTrackedActorParentChanged(int32 InIdx, const TArray<TWeakObjectPtr<AActor>>& InPreviousParentActor, const TArray<TWeakObjectPtr<AActor>>& InNewParentActor) = 0;

	virtual void OnSceneTreeTrackedActorRearranged(int32 InIdx, AActor* InRearrangedActor) = 0;
};

/**
 * This extension tracks specific actors for render state updates,
 * when an update happens it will dirty the modifier it is attached on if filter passes
 */
class FAvaSceneTreeUpdateModifierExtension : public FActorModifierCoreExtension
{

public:
	explicit FAvaSceneTreeUpdateModifierExtension(IAvaSceneTreeUpdateHandler* InExtensionHandler);

	void TrackSceneTree(int32 InTrackedActorIdx, FAvaSceneTreeActor* InTrackedActor);
	void UntrackSceneTree(int32 InTrackedActorIdx);

	FAvaSceneTreeActor* GetTrackedActor(int32 InTrackedActorIdx) const;

	void CheckTrackedActorsUpdate() const;
	void CheckTrackedActorUpdate(int32 InIdx) const;

	TSet<TWeakObjectPtr<AActor>> GetChildrenActorsRecursive(const AActor* InActor) const;
	TArray<TWeakObjectPtr<AActor>> GetDirectChildrenActor(AActor* InActor) const;
	TArray<TWeakObjectPtr<AActor>> GetParentActors(const AActor* InActor) const;

protected:
	//~ Begin FActorModifierCoreExtension
	virtual void OnExtensionEnabled(EActorModifierCoreEnableReason InReason) override;
	virtual void OnExtensionDisabled(EActorModifierCoreDisableReason InReason) override;
	//~ End FActorModifierCoreExtension

private:
#if WITH_EDITOR
	virtual void OnOutlinerLoaded();
	virtual void OnActorHierarchyChanged(AActor* InActor, const AActor* InParentActor, EAvaOutlinerHierarchyChangeType InChangeType);
#endif

	void OnRenderStateDirty(UActorComponent& InComponent);
	void OnWorldActorDestroyed(AActor* InActor);

	bool IsSameActorArray(const TArray<TWeakObjectPtr<AActor>>& InPreviousActorWeak, const TArray<TWeakObjectPtr<AActor>>& InNewActorWeak) const;

	TArray<TWeakObjectPtr<AActor>> GetReferenceActors(const FAvaSceneTreeActor* InTrackedActor) const;

	TWeakInterfacePtr<IAvaSceneTreeUpdateHandler> ExtensionHandlerWeak;

	TMap<int32, FAvaSceneTreeActor*> TrackedActors;

	FDelegateHandle WorldActorDestroyedDelegate;
};