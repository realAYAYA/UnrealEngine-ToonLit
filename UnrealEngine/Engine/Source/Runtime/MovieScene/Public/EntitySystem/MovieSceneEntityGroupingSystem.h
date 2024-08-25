// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "CoreTypes.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneComponentTypeInfo.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "EntitySystem/MovieSceneEntitySystem.h"
#include "EntitySystem/MovieSceneEntitySystemTask.h"
#include "EntitySystem/MovieSceneEntitySystemTypes.h"
#include "Templates/PointerIsConvertibleFromTo.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneEntityGroupingSystem.generated.h"

class UMovieSceneEntityGroupingSystem;

namespace UE::MovieScene
{

struct FAddGroupMutation;
struct FUpdateGroupsTask;

/**
 * Utility class used by the grouping system's policies (see below) to manage groups.
 */
struct MOVIESCENE_API FEntityGroupBuilder
{
	FEntityGroupBuilder(UMovieSceneEntityGroupingSystem* InOwner, FEntityGroupingPolicyKey InPolicyKey);

	/** Make a full group ID from an existing group index */
	FEntityGroupID MakeGroupID(int32 GroupIndex) const { return FEntityGroupID(PolicyKey, GroupIndex); }
	/** Make an invalid group ID that is associated with the policy key */
	FEntityGroupID MakeInvalidGroupID() const { return FEntityGroupID(PolicyKey, INDEX_NONE); }
	/** Add the entity to the given group. The entity must already have the group ID component. */
	void AddEntityToGroup(const FMovieSceneEntityID& InEntity, const FEntityGroupID& InNewGroupID);
	/** Remove the entity from the given group. The entity must already have the group ID component. */
	bool RemoveEntityFromGroup(const FMovieSceneEntityID& InEntity, const FEntityGroupID& InPreviousGroupID);

private:
	UMovieSceneEntityGroupingSystem* Owner;
	FEntityGroupingPolicyKey PolicyKey;
};

/**
 * Base class for grouping handlers, used by the grouping system (see below).
 */
struct IEntityGroupingHandler
{
	virtual ~IEntityGroupingHandler() {}
	virtual void PreTask() {}
	virtual void ProcessAllocation(FEntityAllocationIteratorItem Item, FReadEntityIDs EntityIDs, TWrite<FEntityGroupID> GroupIDs, FEntityGroupBuilder* Builder) = 0;
	virtual void PostTask(bool bFreeGroupIDs) {}

#if WITH_EDITOR
	virtual void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap) = 0;
#endif
};

/**
 * Strongly-typed grouping handler class, which knows about the exact components to look for, and how
 * to use them to group entities.
 */
template<typename GroupingPolicy, typename ComponentIndices, typename ...ComponentTypes>
struct TEntityGroupingHandlerImpl;

template<typename GroupingPolicy, typename ...ComponentTypes>
struct TEntityGroupingHandler 
	: TEntityGroupingHandlerImpl<GroupingPolicy, TMakeIntegerSequence<int, sizeof...(ComponentTypes)>, ComponentTypes...>
{
	TEntityGroupingHandler(GroupingPolicy&& InPolicy, TComponentTypeID<ComponentTypes>... InComponents)
		: TEntityGroupingHandlerImpl<GroupingPolicy, TMakeIntegerSequence<int, sizeof...(ComponentTypes)>, ComponentTypes...>(
				MoveTemp(InPolicy), InComponents...)
	{
	}
};

template<typename GroupingPolicy, int ...ComponentIndices, typename ...ComponentTypes>
struct TEntityGroupingHandlerImpl<GroupingPolicy, TIntegerSequence<int, ComponentIndices...>, ComponentTypes...> : IEntityGroupingHandler
{
	using GroupKeyType = typename GroupingPolicy::GroupKeyType;

	/** The grouping policy */
	GroupingPolicy Policy;

	/** The components that are required for making up a group key */
	TTuple<TComponentTypeID<ComponentTypes>...> Components;

	/** The group keys that we know about, mapped to their corresponding group index */
	TMap<GroupKeyType, int32> GroupKeyToIndex;

	/** The list of group indices in use */
	TBitArray<> AllocatedGroupIndices;
	/** The transient list of groups freed this frame */
	TBitArray<> FreedGroupIndices;

	TEntityGroupingHandlerImpl(GroupingPolicy&& InPolicy, TComponentTypeID<ComponentTypes>... InComponents)
		: Policy(MoveTemp(InPolicy))
		, Components(InComponents...)
	{
	}

	int32 GetOrAllocateGroupIndex(typename TCallTraits<GroupKeyType>::ParamType InGroupKey)
	{
		int32& GroupIndex = GroupKeyToIndex.FindOrAdd(InGroupKey, INDEX_NONE);
		if (GroupIndex == INDEX_NONE)
		{
			// This group key isn't known to us... let's allocate a new group index for it.
			// Try to find an available index first. Otherwise use a new high index.
			int32 NewGroupIndex = AllocatedGroupIndices.Find(false);
			if (NewGroupIndex == INDEX_NONE)
			{
				NewGroupIndex = AllocatedGroupIndices.Add(true);
			}
			else
			{
				AllocatedGroupIndices[NewGroupIndex] = true;
			}
			GroupIndex = NewGroupIndex;
		}
		else
		{
			// We know this group key, so we'll return the group index we already have
			// associated with it. We just need to "revive" it in case it was scheduled
			// for being freed.
			if (FreedGroupIndices.IsValidIndex(GroupIndex))
			{
				FreedGroupIndices[GroupIndex] = false;
			}
		}
		return GroupIndex;
	}

	void FreeGroupIndex(int32 InGroupIndex)
	{
		if (ensure(AllocatedGroupIndices.IsValidIndex(InGroupIndex) && AllocatedGroupIndices[InGroupIndex]))
		{
			AllocatedGroupIndices[InGroupIndex] = false;
		}
	}

	/** Callback on the grouping policy invoked before all grouping happens this frame */
	static void PreTaskImpl(void*, ...) {}
	template <typename T> static void PreTaskImpl(T* InPolicy, decltype(&T::PreTask)* = 0)
	{
		InPolicy->PreTask();
	}

	virtual void PreTask() override
	{
		PreTaskImpl(&Policy);
	}

	/**
	 * Callback on the grouping policy invoked before grouping happens for a given allocation.
	 * If the callback returns false, this allocation will be skipped.
	 */
	static bool PreAllocationCallbackImpl(void*, ...) { return true; }
	template <typename T> static bool PreAllocationCallbackImpl(T* InPolicy, FEntityAllocationIteratorItem Item, decltype(&T::PreProcessGroups)* = 0)
	{
		return InPolicy->PreProcessGroups(Item);
	}

	/** Process an allocation and group the entities found therein */
	virtual void ProcessAllocation(FEntityAllocationIteratorItem Item,  FReadEntityIDs EntityIDs, TWrite<FEntityGroupID> GroupIDs, FEntityGroupBuilder* Builder) override
	{
		// Check if we should process this allocation.
		const bool bProcessAllocation = PreAllocationCallbackImpl(&Policy, Item);
		if (!bProcessAllocation)
		{
			return;
		}

		const FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		const FComponentMask& AllocationType = Item.GetAllocationType();
		const bool bNeedsLink = AllocationType.Contains(BuiltInComponents->Tags.NeedsLink);
		const bool bNeedsUnlink = AllocationType.Contains(BuiltInComponents->Tags.NeedsUnlink);
		ensure(bNeedsLink || bNeedsUnlink);

		if (bNeedsLink)
		{
			VisitLinkedEntities(Item, EntityIDs, GroupIDs, Builder);
		}
		else if (bNeedsUnlink)
		{
			VisitUnlinkedEntities(Item, EntityIDs, GroupIDs, Builder);
		}

		PostAllocationCallbackImpl(&Policy, Item); //-V510
	}
	
	void VisitLinkedEntities(FEntityAllocationIteratorItem Item, FReadEntityIDs EntityIDs, TWrite<FEntityGroupID> GroupIDs, FEntityGroupBuilder* Builder)
	{
		const FEntityAllocation* Allocation = Item.GetAllocation();
		const int32 Num = Allocation->Num();

		const FEntityGroupID InvalidGroupID = Builder->MakeInvalidGroupID();

		TTuple<TComponentReader<ComponentTypes>...> ComponentReaders(
				Allocation->ReadComponents(Components.template Get<ComponentIndices>())...);

		for (int32 Index = 0; Index < Num; ++Index)
		{
			GroupKeyType GroupKey;
			const bool bValidGroupKey = Policy.GetGroupKey(ComponentReaders.template Get<ComponentIndices>()[Index]..., GroupKey);

			const FMovieSceneEntityID EntityID(EntityIDs[Index]);
			FEntityGroupID& GroupID(GroupIDs[Index]);

			if (bValidGroupKey)
			{
				// Find or create the appropriate group and put the entity in it.
				int32 NewGroupIndex = GetOrAllocateGroupIndex(GroupKey);
				FEntityGroupID NewGroupID = Builder->MakeGroupID(NewGroupIndex);
				Builder->AddEntityToGroup(EntityID, NewGroupID);
				GroupID = NewGroupID;
			}
			else
			{
				// This entity doesn't belong to any group.
				// Let's assign an invalid group ID that nonetheless has a valid policy key
				// pointing to this grouping.
				GroupID = InvalidGroupID;
			}
		}
	}

	void VisitUnlinkedEntities(FEntityAllocationIteratorItem Item, FReadEntityIDs EntityIDs, TWrite<FEntityGroupID> GroupIDs, FEntityGroupBuilder* Builder)
	{
		const FEntityAllocation* Allocation = Item.GetAllocation();
		const int32 Num = Allocation->Num();

		for (int32 Index = 0; Index < Num; ++Index)
		{
			const FMovieSceneEntityID EntityID(EntityIDs[Index]);
			FEntityGroupID& GroupID(GroupIDs[Index]);

			if (GroupID.HasGroup())
			{
				const bool bIsGroupEmpty = Builder->RemoveEntityFromGroup(EntityID, GroupID);
				if (bIsGroupEmpty)
				{
					// The group is now empty, we can re-use its index for a new group... but we don't
					// want to re-use it until later, because we could end up in two situations we
					// want to avoid:
					//
					// 1) At the same time that a group is emptied of all its entities, new entities
					//    come in that belong to that group because they generate the exact same key.
					//    We want that group to effectively "persist" with the same index.
					//
					// 2) If we freed the index right away, we couldn't tell the difference between
					//	  the above situation, and a brand new group that just happens to re-use the
					//	  recently freed index.
					//
					FreedGroupIndices.PadToNum(GroupID.GroupIndex + 1, false);
					FreedGroupIndices[GroupID.GroupIndex] = true;
				}
				// Leave the GroupID on the entity so that downstream systems can use it to track
				// that this entity is leaving its group, but flag it so we don't re-free it.
				ensure(!EnumHasAllFlags(GroupID.Flags , EEntityGroupFlags::RemovedFromGroup));
				GroupID.Flags |= EEntityGroupFlags::RemovedFromGroup;
			}
		}
	}
	
	/** Callback on the grouping policy invoked after a given allocation has been processed */
	static void PostAllocationCallbackImpl(void*, ...) {}
	template <typename T> static void PostAllocationCallbackImpl(T* InPolicy, FEntityAllocationIteratorItem Item, decltype(&T::PostProcessGroups)* = 0)
	{
		InPolicy->PostProcessGroups(Item);
	}

	static void PostTaskImpl(void*, ...){}
	template <typename T> static void PostTaskImpl(T* InPolicy, bool bInFreeGroupIDs, decltype(&T::PostTask)* = 0)
	{
		InPolicy->PostTask(bInFreeGroupIDs);
	}
	
	virtual void PostTask(bool bInFreeGroupIDs) override
	{
		if (bInFreeGroupIDs && FreedGroupIndices.Find(true) != INDEX_NONE)
		{
			// Build a reverse lookup map to figure out which group keys we don't need anymore
			// based on the group indices we have freed.
			TMap<int32, GroupKeyType> GroupIndexToKey;
			for (const TPair<GroupKeyType, int32>& Pair : GroupKeyToIndex)
			{
				GroupIndexToKey.Add(Pair.Value, Pair.Key);
			}

			// Free the indices we don't use anymore, and free the group key lookup entry too.
			for (TConstSetBitIterator<> It(FreedGroupIndices); It; ++It)
			{
				const GroupKeyType* GroupKey = GroupIndexToKey.Find(It.GetIndex());
				if (ensure(GroupKey))
				{
					const int NumRemoved = GroupKeyToIndex.Remove(*GroupKey);
					ensure(NumRemoved == 1);
				}
				FreeGroupIndex(It.GetIndex());
			}
			FreedGroupIndices.Reset();
		}

		PostTaskImpl(&Policy);
	}

#if WITH_EDITOR
	virtual void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap) override
	{
		// Get a list of keys that contain replaced objects.
		TMap<GroupKeyType, GroupKeyType> ReplacedKeys;
		for (const TPair<GroupKeyType, int32>& Pair : GroupKeyToIndex)
		{
			GroupKeyType NewKey = Pair.Key;
			if (Policy.OnObjectsReplaced(NewKey, ReplacementMap))
			{
				ReplacedKeys.Add(Pair.Key, NewKey);
			}
		}
		// Replace the keys but keep the group indices.
		for (const TPair<GroupKeyType, GroupKeyType>& Pair : ReplacedKeys)
		{
			int32 GroupIndex;
			const bool bRemoved = GroupKeyToIndex.RemoveAndCopyValue(Pair.Key, GroupIndex);
			if (ensure(bRemoved))
			{
				GroupKeyToIndex.Add(Pair.Value, GroupIndex);
			}
		}
	}
#endif
};

namespace Private
{
	template<typename T>
	bool ReplaceGroupKeyObjectElement(T&& InElem, const TMap<UObject*, UObject*>& ReplacementMap)
	{
		return false;
	}

	template<typename T>
	typename TEnableIf<TPointerIsConvertibleFromTo<T, UObject>::Value, bool>::Type ReplaceGroupKeyObjectElement(T& InOutElem, const TMap<UObject*, UObject*>& ReplacementMap)
	{
		UObject* CurrentObject = InOutElem;
		if (UObject* const* NewObject = ReplacementMap.Find(CurrentObject))
		{
			InOutElem = NewObject;
			return true;
		}
		return false;
	}
}

/**
 * A simple grouping policy that uses a tuple of component values as the group key.
 */
template<typename... ComponentTypes>
struct TTupleGroupingPolicy
{
	using GroupKeyType = TTuple<ComponentTypes...>;

	bool GetGroupKey(ComponentTypes... InComponents, GroupKeyType& OutGroupKey)
	{
		OutGroupKey = MakeTuple(InComponents...);
		return true;
	}

	bool OnObjectsReplaced(GroupKeyType& InOutKey, const TMap<UObject*, UObject*>& ReplacementMap)
	{
		bool bReplaced = false;
		VisitTupleElements([&ReplacementMap, &bReplaced](auto& InElem)
			{
				bReplaced |= Private::ReplaceGroupKeyObjectElement(InElem, ReplacementMap);
			},
			InOutKey);
		return bReplaced;
	}
};

} // namespace UE::MovieScene


UCLASS()
class MOVIESCENE_API UMovieSceneEntityGroupingSystem : public UMovieSceneEntitySystem
{
public:

	using FEntityGroupID = UE::MovieScene::FEntityGroupID;
	using FEntityGroupingPolicyKey = UE::MovieScene::FEntityGroupingPolicyKey;
	using IEntityGroupingHandler = UE::MovieScene::IEntityGroupingHandler;
	using FMovieSceneEntityID = UE::MovieScene::FMovieSceneEntityID;

	GENERATED_BODY()

	UMovieSceneEntityGroupingSystem(const FObjectInitializer& ObjInit);

	/**
	 * Add a new grouping policy that will use the given components to make up a group key.
	 *
	 * Grouping policies must be simple structs that can be copied and owned by the grouping system, and
	 * that implement the following members:
	 *
	 * - GroupKeyType [mandatory]
	 *		A typedef or alias to the group key type to use to group entities together.
	 *
	 * - GetGroupKey(ComponentTypes... InComponents, GroupKeyType& OutGroupKey) [mandatory]
	 *		A function that creates a group key used to group entities together.
	 *		Returns true to indicate the key is valid, false otherwise. If false, the entity
	 *		corresponding to the given components will not be grouped.
	 *
	 *	- OnObjectsReplaced(GroupKeyType& InOut, const TMap<UObject*, UObject*>&) [mandatory, sadly]
	 *		Potentially changes a key if it contains a pointer to a replaced object. Should
	 *		return true if any replacement occured.
	 *
	 *	- PreTask() [optional]
	 *		A function called before any grouping is done during an instantiation phase.
	 *
	 *	- PostTask() [optional]
	 *		A function called after any grouping is done dunring an instantiation phase.
	 *
	 *	- PreProcessGroups() [optional]
	 *
	 *	- PostProcessGroups() [optional]
	 */
	template<typename GroupingPolicy, typename ...ComponentTypes>
	FEntityGroupingPolicyKey AddGrouping(GroupingPolicy&& InPolicy, TComponentTypeID<ComponentTypes>... InComponents)
	{
		using namespace UE::MovieScene;

		using NewGroupHandlerType = TEntityGroupingHandler<GroupingPolicy, ComponentTypes...>;

		static_assert(sizeof(NewGroupHandlerType) <= 256, "Handler type too big! Please increase the TInlineValue size.");

		const int32 HandlerIndex = GroupHandlers.Emplace();
		FEntityGroupingHandlerInfo& HandlerInfo = GroupHandlers[HandlerIndex];
		HandlerInfo.Handler.Emplace<NewGroupHandlerType>(MoveTemp(InPolicy), InComponents...);
		HandlerInfo.ComponentMask = FComponentMask({ InComponents... });

		FEntityGroupingPolicyKey NewPolicyKey{ HandlerIndex };
		return NewPolicyKey;
	}

	/**
	 * Add a new grouping policy that will make a key that is a tuple of the given components' values.
	 */
	template<typename ...ComponentTypes>
	FEntityGroupingPolicyKey AddGrouping(TComponentTypeID<ComponentTypes>... InComponents)
	{
		UE::MovieScene::TTupleGroupingPolicy<ComponentTypes...> TuplePolicy;
		return AddGrouping(MoveTemp(TuplePolicy), InComponents...);
	}

	/**
	 * Remove a previously added grouping policy.
	 */
	void RemoveGrouping(FEntityGroupingPolicyKey InPolicyKey);

	/**
	 * Get the list of entities in the given group.
	 */
	TArrayView<const UE::MovieScene::FMovieSceneEntityID> GetGroup(const FEntityGroupID& InGroupID) const;

private:

	virtual bool IsRelevantImpl(UMovieSceneEntitySystemLinker* InLinker) const override;
	virtual void OnRun(FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents) override;
	virtual void OnLink() override;
	virtual void OnUnlink() override;
	virtual void OnCleanTaggedGarbage() override;

#if WITH_EDITOR
	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);
#endif

private:

	struct FEntityGroupInfo
	{
		TArray<UE::MovieScene::FMovieSceneEntityID> Entities;
	};

	TMap<FEntityGroupID, FEntityGroupInfo> Groups;

	struct FEntityGroupingHandlerInfo
	{
		TInlineValue<IEntityGroupingHandler, 256> Handler;
		FComponentMask ComponentMask;
	};

	TSparseArray<FEntityGroupingHandlerInfo> GroupHandlers;

	friend struct UE::MovieScene::FAddGroupMutation;
	friend struct UE::MovieScene::FUpdateGroupsTask;
	friend struct UE::MovieScene::FEntityGroupBuilder;
};

