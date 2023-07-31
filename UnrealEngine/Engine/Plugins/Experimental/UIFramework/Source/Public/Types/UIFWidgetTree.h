// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/SoftObjectPtr.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "Types/UIFWidgetId.h"

#include "UIFWidgetTree.generated.h"

struct FReplicationFlags;
class FOutBunch;
class UActorChannel;
class UUIFrameworkPlayerComponent;
class UUIFrameworkWidget;

/**
 *
 */
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkWidgetTreeEntry : public FFastArraySerializerItem
{
	GENERATED_BODY()

	FUIFrameworkWidgetTreeEntry() = default;
	FUIFrameworkWidgetTreeEntry(UUIFrameworkWidget* Parent, UUIFrameworkWidget* Child);

	bool IsParentValid() const;
	bool IsChildValid() const;

	UPROPERTY()
	TObjectPtr<UUIFrameworkWidget> Parent = nullptr;

	UPROPERTY()
	TObjectPtr<UUIFrameworkWidget> Child = nullptr;
	
	UPROPERTY()
	FUIFrameworkWidgetId ParentId;

	UPROPERTY()
	FUIFrameworkWidgetId ChildId;
};


/**
 * A valid snapshot of the widget tree that can be replicated to local instance.
 * Authority widgets know their parent/children relation. That information is not replicated to the local widgets.
 * When a widget is added to the tree, the tree is updated. The widget now has to inform the tree when that relationship changes until it's remove from the tree.
 */
USTRUCT()
struct UIFRAMEWORK_API FUIFrameworkWidgetTree : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	FUIFrameworkWidgetTree() = default;
	FUIFrameworkWidgetTree(UUIFrameworkPlayerComponent* InOwnerComponent)
		: OwnerComponent(InOwnerComponent)
	{
	}

public:
	//~ Begin of FFastArraySerializer
	void PreReplicatedRemove(const TArrayView<int32> RemovedIndices, int32 FinalSize);
	void PostReplicatedAdd(const TArrayView<int32> AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	//~ End of FFastArraySerializer

	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FUIFrameworkWidgetTreeEntry, FUIFrameworkWidgetTree>(Entries, DeltaParms, *this);
	}

	bool ReplicateSubWidgets(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags);

	/** Add a new widget to the top hierarchy. */
	void AddRoot(UUIFrameworkWidget* Widget);
	/**
	 * Change the parent / child relationship of the child widget.
	 * If the child widget had a parent, that relationship entry will replaced it by a new one.
	 */
	void AddWidget(UUIFrameworkWidget* Parent, UUIFrameworkWidget* Child);
	/**
	 * Remove the widget and all of its children and grand-children from the tree.
	 * It will clean all the parent relationship from the tree.
	 */
	void RemoveWidget(UUIFrameworkWidget* Widget);

	FUIFrameworkWidgetTreeEntry* GetEntryByReplicationId(int32 ReplicationId);
	const FUIFrameworkWidgetTreeEntry* GetEntryByReplicationId(int32 ReplicationId) const;

	/** Find the widget by its unique Id. The widget needs to be in the Tree. */
	UUIFrameworkWidget* FindWidgetById(FUIFrameworkWidgetId WidgetId);
	const UUIFrameworkWidget* FindWidgetById(FUIFrameworkWidgetId WidgetId) const;

private:
	void AddChildInternal(UUIFrameworkWidget* Parent, UUIFrameworkWidget* Child);
	void AddChildRecursiveInternal(UUIFrameworkWidget* Widget);

private:
	UPROPERTY()
	TArray<FUIFrameworkWidgetTreeEntry> Entries;

	UPROPERTY(NotReplicated)
	TMap<FUIFrameworkWidgetId, TWeakObjectPtr<UUIFrameworkWidget>> WidgetByIdMap;

	UPROPERTY(NotReplicated, Transient)
	TObjectPtr<UUIFrameworkPlayerComponent> OwnerComponent = nullptr;
};

template<>
struct TStructOpsTypeTraits<FUIFrameworkWidgetTree> : public TStructOpsTypeTraitsBase2<FUIFrameworkWidgetTree>
{
	enum { WithNetDeltaSerializer = true };
};