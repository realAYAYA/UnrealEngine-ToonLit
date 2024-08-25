// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SequencerCoreFwd.h"
#include "MVVM/ViewModelTypeID.h"
#include "Delegates/DelegateCombinations.h"
#include "MVVM/Extensions/DynamicExtensionContainer.h"
#include "MVVM/ViewModelPtr.h"

struct FGuid;

namespace UE::Sequencer
{

class IHierarchicalCache;

DECLARE_MULTICAST_DELEGATE_OneParam(FPreUpdateCachesEvent, FViewModelPtr);

class SEQUENCERCORE_API IHierarchicalCache
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(IHierarchicalCache)

	virtual ~IHierarchicalCache(){}

	virtual void BeginUpdate() = 0;
	virtual void PreVisitChildren(const FViewModelPtr& ViewModel) = 0;
	virtual void PostVisitChildren(const FViewModelPtr& ViewModel) = 0;
	virtual void EndUpdate() = 0;
};


class SEQUENCERCORE_API FHierarchicalCacheExtension
	: public IDynamicExtension
{
public:

	~FHierarchicalCacheExtension();

	FPreUpdateCachesEvent PreUpdateCachesEvent;

	void Initialize(const FViewModelPtr& InRootModel);

	void OnCreated(TSharedRef<FViewModel> InWeakOwner) override;

	void OnHierarchyUpdated();

	void UpdateCachedFlags();
	void UpdateAllCachedFlags(const FViewModelPtr& ViewModel, TArrayView<IHierarchicalCache* const> HierarchicalCaches);

private:

	void UpdateCachedFlagsForModel(const FViewModelPtr& ViewModel, TArrayView<IHierarchicalCache* const> HierarchicalCaches);

protected:

	FWeakViewModelPtr WeakOwnerModel;
	FWeakViewModelPtr WeakRootModel;
	EViewModelListType ModelListFilter;
};



template<typename FlagsType>
class TFlagStateCacheExtension
	: public IDynamicExtension
	, public IHierarchicalCache
{
public:

	using Implements = TImplements<IDynamicExtension, IHierarchicalCache>;

	virtual ~TFlagStateCacheExtension() {}

	FlagsType GetRootFlags() const
	{
		return RootFlags;
	}
	FlagsType GetCachedFlags(const FViewModelPtr& ViewModel) const
	{
		return GetCachedFlags(ViewModel->GetModelID());
	}
	FlagsType GetCachedFlags(uint32 InModelID) const
	{
		return CachedFlagsFromNodeID.FindRef(InModelID);
	}

private:

	virtual FlagsType ComputeFlagsForModel(const FViewModelPtr& ViewModel) = 0;
	virtual void PostComputeChildrenFlags(const FViewModelPtr& ViewModel, FlagsType& OutThisModelFlags, FlagsType& OutPropagateToParentFlags) {}

protected:

	void BeginUpdate() override
	{
		CachedFlagsFromNodeID.Empty();
		IndividualItemFlags.Emplace(FlagsType::None);
		AccumulatedChildFlags.Emplace(FlagsType::None);
	}
	void PreVisitChildren(const FViewModelPtr& ViewModel) override
	{
		FlagsType ThisModelFlags = ComputeFlagsForModel(ViewModel);
		IndividualItemFlags.Emplace(ThisModelFlags);
		AccumulatedChildFlags.Emplace(FlagsType::None);
	}
	void PostVisitChildren(const FViewModelPtr& ViewModel) override
	{
		FlagsType ThisModelFlags              = IndividualItemFlags.Pop();
		FlagsType FlagsPropagatedFromChildren = AccumulatedChildFlags.Pop();

		ThisModelFlags |= FlagsPropagatedFromChildren;
		PostComputeChildrenFlags(ViewModel, ThisModelFlags, AccumulatedChildFlags.Last());

		AccumulatedChildFlags.Last() |= (ThisModelFlags & FlagsType::InheritedFromChildren);

		if (ThisModelFlags != FlagsType::None)
		{
			CachedFlagsFromNodeID.Add(ViewModel->GetModelID(), ThisModelFlags);
		}
	}
	void EndUpdate() override
	{
		RootFlags = AccumulatedChildFlags.Last();
		AccumulatedChildFlags.Empty();
		IndividualItemFlags.Empty();

		CachedFlagsFromNodeID.Shrink();
	}

protected:

	TArray<FlagsType> AccumulatedChildFlags;
	TArray<FlagsType> IndividualItemFlags;
	TMap<uint32, FlagsType> CachedFlagsFromNodeID;
	FlagsType RootFlags;
};


} // namespace UE::Sequencer

