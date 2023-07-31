// Copyright Epic Games, Inc. All Rights Reserved.

#include "EntitySystem/MovieSceneSequenceUpdaters.h"
#include "EntitySystem/MovieSceneSequenceInstance.h"
#include "EntitySystem/MovieSceneSequenceInstanceHandle.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneEntitySystemRunner.h"
#include "EntitySystem/MovieSceneEntitySystem.h"

#include "Templates/UniquePtr.h"
#include "Containers/BitArray.h"
#include "Containers/SortedMap.h"

#include "MovieSceneSequence.h"
#include "MovieSceneSequenceID.h"
#include "Evaluation/MovieScenePlayback.h"
#include "Compilation/MovieSceneCompiledDataManager.h"

#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Evaluation/MovieSceneRootOverridePath.h"

#include "MovieSceneTimeHelpers.h"

#include "Algo/IndexOf.h"
#include "Algo/Transform.h"

namespace UE
{
namespace MovieScene
{

/** Flat sequence updater (ie, no hierarchy) */
struct FSequenceUpdater_Flat : ISequenceUpdater
{
	explicit FSequenceUpdater_Flat(FMovieSceneCompiledDataID InCompiledDataID);
	~FSequenceUpdater_Flat();

	virtual void DissectContext(UMovieSceneEntitySystemLinker* Linker, IMovieScenePlayer* InPlayer, const FMovieSceneContext& Context, TArray<TRange<FFrameTime>>& OutDissections) override;
	virtual void Start(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer, const FMovieSceneContext& InContext) override;
	virtual void Update(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer, const FMovieSceneContext& Context) override;
	virtual void Finish(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer) override;
	virtual void InvalidateCachedData(UMovieSceneEntitySystemLinker* Linker) override;
	virtual void Destroy(UMovieSceneEntitySystemLinker* Linker) override;
	virtual TUniquePtr<ISequenceUpdater> MigrateToHierarchical() override;
	virtual FInstanceHandle FindSubInstance(FMovieSceneSequenceID SubSequenceID) const override { return FInstanceHandle(); }
	virtual void OverrideRootSequence(UMovieSceneEntitySystemLinker* InLinker, FRootInstanceHandle InstanceHandle, FMovieSceneSequenceID NewRootOverrideSequenceID) override {}

private:

	TRange<FFrameNumber> CachedEntityRange;

	TOptional<TArray<FFrameTime>> CachedDeterminismFences;
	FMovieSceneCompiledDataID CompiledDataID;
};

/** Hierarchical sequence updater */
struct FSequenceUpdater_Hierarchical : ISequenceUpdater
{
	explicit FSequenceUpdater_Hierarchical(FMovieSceneCompiledDataID InCompiledDataID);

	~FSequenceUpdater_Hierarchical();

	virtual void DissectContext(UMovieSceneEntitySystemLinker* Linker, IMovieScenePlayer* InPlayer, const FMovieSceneContext& Context, TArray<TRange<FFrameTime>>& OutDissections) override;
	virtual void Start(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer, const FMovieSceneContext& InContext) override;
	virtual void Update(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer, const FMovieSceneContext& Context) override;
	virtual void Finish(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer) override;
	virtual void InvalidateCachedData(UMovieSceneEntitySystemLinker* Linker) override;
	virtual void Destroy(UMovieSceneEntitySystemLinker* Linker) override;
	virtual TUniquePtr<ISequenceUpdater> MigrateToHierarchical() override { return nullptr; }
	virtual FInstanceHandle FindSubInstance(FMovieSceneSequenceID SubSequenceID) const override { return SequenceInstances.FindRef(SubSequenceID).Handle; }
	virtual void OverrideRootSequence(UMovieSceneEntitySystemLinker* InLinker, FRootInstanceHandle InstanceHandle, FMovieSceneSequenceID NewRootOverrideSequenceID) override;

private:

	TRange<FFrameNumber> UpdateEntitiesForSequence(const FMovieSceneEntityComponentField* ComponentField, FFrameTime SequenceTime, FMovieSceneEvaluationFieldEntitySet& OutEntities);

	FInstanceHandle GetOrCreateSequenceInstance(IMovieScenePlayer* InPlayer, FInstanceRegistry* InstanceRegistry, FRootInstanceHandle RootInstanceHandle, FMovieSceneSequenceID SequenceID);

private:

	TRange<FFrameNumber> CachedEntityRange;

	struct FSubInstanceData
	{
		FInstanceHandle Handle;
		bool bNeedsDestroy = false;
	};
	TSortedMap<FMovieSceneSequenceID, FSubInstanceData, TInlineAllocator<8>> SequenceInstances;

	FMovieSceneCompiledDataID CompiledDataID;

	FMovieSceneSequenceID RootOverrideSequenceID;
};

void DissectRange(TArrayView<const FFrameTime> InDissectionTimes, const TRange<FFrameTime>& Bounds, TArray<TRange<FFrameTime>>& OutDissections)
{
	if (InDissectionTimes.Num() == 0)
	{
		return;
	}

	TRangeBound<FFrameTime> LowerBound = Bounds.GetLowerBound();

	for (int32 Index = 0; Index < InDissectionTimes.Num(); ++Index)
	{
		FFrameTime DissectionTime = InDissectionTimes[Index];

		TRange<FFrameTime> Dissection(LowerBound, TRangeBound<FFrameTime>::Exclusive(DissectionTime));
		if (!Dissection.IsEmpty())
		{
			ensureAlwaysMsgf(Bounds.Contains(Dissection), TEXT("Dissection specified for a range outside of the current bounds"));

			OutDissections.Add(Dissection);

			LowerBound = TRangeBound<FFrameTime>::Inclusive(DissectionTime);
		}
	}

	TRange<FFrameTime> TailRange(LowerBound, Bounds.GetUpperBound());
	if (!TailRange.IsEmpty())
	{
		OutDissections.Add(TailRange);
	}
}

TArrayView<const FFrameTime> GetFencesWithinRange(TArrayView<const FFrameTime> Fences, const TRange<FFrameTime>& Boundary)
{
	if (Fences.Num() == 0 || Boundary.IsEmpty())
	{
		return TArrayView<const FFrameTime>();
	}

	// Take care to include or exclude the lower bound of the range if it's on a whole frame numbe
	const int32 StartFence = Boundary.GetLowerBound().IsClosed() ? Algo::UpperBound(Fences, Boundary.GetLowerBoundValue()) : 0;
	if (StartFence >= Fences.Num())
	{
		return TArrayView<const FFrameTime>();
	}

	const int32 EndFence = Boundary.GetUpperBound().IsClosed() ? Algo::UpperBound(Fences, Boundary.GetUpperBoundValue()) : Fences.Num();
	const int32 NumFences = FMath::Max(0, EndFence - StartFence);
	if (NumFences == 0)
	{
		return TArrayView<const FFrameTime>();
	}

	return MakeArrayView(Fences.GetData() + StartFence, NumFences);
}


void ISequenceUpdater::FactoryInstance(TUniquePtr<ISequenceUpdater>& OutPtr, UMovieSceneCompiledDataManager* CompiledDataManager, FMovieSceneCompiledDataID CompiledDataID)
{
	const bool bHierarchical = CompiledDataManager->FindHierarchy(CompiledDataID) != nullptr;

	if (!OutPtr)
	{
		if (!bHierarchical)
		{
			OutPtr = MakeUnique<FSequenceUpdater_Flat>(CompiledDataID);
		}
		else
		{
			OutPtr = MakeUnique<FSequenceUpdater_Hierarchical>(CompiledDataID);
		}
	}
	else if (bHierarchical)
	{ 
		TUniquePtr<ISequenceUpdater> NewHierarchical = OutPtr->MigrateToHierarchical();
		if (NewHierarchical)
		{
			OutPtr = MoveTemp(NewHierarchical);
		}
	}
}

FSequenceUpdater_Flat::FSequenceUpdater_Flat(FMovieSceneCompiledDataID InCompiledDataID)
	: CompiledDataID(InCompiledDataID)
{
	CachedEntityRange = TRange<FFrameNumber>::Empty();
}

FSequenceUpdater_Flat::~FSequenceUpdater_Flat()
{
}

TUniquePtr<ISequenceUpdater> FSequenceUpdater_Flat::MigrateToHierarchical()
{
	return MakeUnique<FSequenceUpdater_Hierarchical>(CompiledDataID);
}

void FSequenceUpdater_Flat::DissectContext(UMovieSceneEntitySystemLinker* Linker, IMovieScenePlayer* InPlayer, const FMovieSceneContext& Context, TArray<TRange<FFrameTime>>& OutDissections)
{
	if (!CachedDeterminismFences.IsSet())
	{
		UMovieSceneCompiledDataManager* CompiledDataManager = InPlayer->GetEvaluationTemplate().GetCompiledDataManager();
		TArrayView<const FFrameTime>    DeterminismFences   = CompiledDataManager->GetEntryRef(CompiledDataID).DeterminismFences;

		if (DeterminismFences.Num() != 0)
		{
			CachedDeterminismFences = TArray<FFrameTime>(DeterminismFences.GetData(), DeterminismFences.Num());
		}
		else
		{
			CachedDeterminismFences.Emplace();
		}
	}

	if (CachedDeterminismFences->Num() != 0)
	{
		TArrayView<const FFrameTime> TraversedFences = GetFencesWithinRange(CachedDeterminismFences.GetValue(), Context.GetRange());
		UE::MovieScene::DissectRange(TraversedFences, Context.GetRange(), OutDissections);
	}
}

void FSequenceUpdater_Flat::Start(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer, const FMovieSceneContext& InContext)
{
}

void FSequenceUpdater_Flat::Update(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer, const FMovieSceneContext& Context)
{
	FSequenceInstance& SequenceInstance = Linker->GetInstanceRegistry()->MutateInstance(InstanceHandle);
	SequenceInstance.SetContext(Context);

	const FMovieSceneEntityComponentField* ComponentField = InPlayer->GetEvaluationTemplate().GetCompiledDataManager()->FindEntityComponentField(CompiledDataID);
	UMovieSceneSequence* Sequence = InPlayer->GetEvaluationTemplate().GetSequence(MovieSceneSequenceID::Root);
	if (Sequence == nullptr)
	{
		SequenceInstance.Ledger.UnlinkEverything(Linker);
		return;
	}

	FMovieSceneEvaluationFieldEntitySet EntitiesScratch;

	FFrameNumber ImportTime = Context.GetEvaluationFieldTime();
		
	const bool bOutsideCachedRange = !CachedEntityRange.Contains(ImportTime);
	if (bOutsideCachedRange)
	{
		if (ComponentField)
		{
			ComponentField->QueryPersistentEntities(ImportTime, CachedEntityRange, EntitiesScratch);
		}
		else
		{
			CachedEntityRange = TRange<FFrameNumber>::All();
		}

		FEntityImportSequenceParams Params;
		Params.InstanceHandle = InstanceHandle;
		Params.RootInstanceHandle = InstanceHandle;
		Params.DefaultCompletionMode = Sequence->DefaultCompletionMode;
		Params.HierarchicalBias = 0;

		SequenceInstance.Ledger.UpdateEntities(Linker, Params, ComponentField, EntitiesScratch);
	}

	// Update any one-shot entities for the current frame
	if (ComponentField && ComponentField->HasAnyOneShotEntities())
	{
		EntitiesScratch.Reset();
		ComponentField->QueryOneShotEntities(Context.GetFrameNumberRange(), EntitiesScratch);

		if (EntitiesScratch.Num() != 0)
		{
			FEntityImportSequenceParams Params;
			Params.InstanceHandle = InstanceHandle;
			Params.RootInstanceHandle = InstanceHandle;
			Params.DefaultCompletionMode = Sequence->DefaultCompletionMode;
			Params.HierarchicalBias = 0;

			SequenceInstance.Ledger.UpdateOneShotEntities(Linker, Params, ComponentField, EntitiesScratch);
		}
	}
}

void FSequenceUpdater_Flat::Finish(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer)
{
	InvalidateCachedData(Linker);
}


void FSequenceUpdater_Flat::Destroy(UMovieSceneEntitySystemLinker* Linker)
{
}

void FSequenceUpdater_Flat::InvalidateCachedData(UMovieSceneEntitySystemLinker* Linker)
{
	CachedEntityRange = TRange<FFrameNumber>::Empty();
	CachedDeterminismFences.Reset();
}





FSequenceUpdater_Hierarchical::FSequenceUpdater_Hierarchical(FMovieSceneCompiledDataID InCompiledDataID)
	: CompiledDataID(InCompiledDataID)
{
	CachedEntityRange = TRange<FFrameNumber>::Empty();
	RootOverrideSequenceID = MovieSceneSequenceID::Root;
}

FSequenceUpdater_Hierarchical::~FSequenceUpdater_Hierarchical()
{
}

void FSequenceUpdater_Hierarchical::DissectContext(UMovieSceneEntitySystemLinker* Linker, IMovieScenePlayer* InPlayer, const FMovieSceneContext& Context, TArray<TRange<FFrameTime>>& OutDissections)
{
	UMovieSceneCompiledDataManager* CompiledDataManager = InPlayer->GetEvaluationTemplate().GetCompiledDataManager();

	TRange<FFrameNumber> TraversedRange = Context.GetFrameNumberRange();
	TArray<FFrameTime>   RootDissectionTimes;

	{
		const FMovieSceneCompiledDataEntry& DataEntry       = CompiledDataManager->GetEntryRef(CompiledDataID);
		TArrayView<const FFrameTime>        TraversedFences = GetFencesWithinRange(DataEntry.DeterminismFences, Context.GetRange());

		UE::MovieScene::DissectRange(TraversedFences, Context.GetRange(), OutDissections);
	}

	// @todo: should this all just be compiled into the root hierarchy?
	if (const FMovieSceneSequenceHierarchy* Hierarchy = CompiledDataManager->FindHierarchy(CompiledDataID))
	{
		FMovieSceneEvaluationTreeRangeIterator SubSequenceIt = Hierarchy->GetTree().IterateFromLowerBound(TraversedRange.GetLowerBound());
		for ( ; SubSequenceIt && SubSequenceIt.Range().Overlaps(TraversedRange); ++SubSequenceIt)
		{
			TRange<FFrameTime> RootClampRange = TRange<FFrameTime>::Intersection(ConvertRange<FFrameNumber, FFrameTime>(SubSequenceIt.Range()), Context.GetRange());

			// When Context.GetRange() does not fall on whole frame boundaries, we can sometimes end up with a range that clamps to being empty, even though the range overlapped
			// the traversed range. ie if we evaluated range (1.5, 10], our traversed range would be [2, 11). If we have a sub sequence range of (10, 20), it would still be iterated here
			// because [2, 11) overlaps (10, 20), but when clamped to the evaluated range, the range is (10, 10], which is empty.
			if (!RootClampRange.IsEmpty())
			{
				continue;
			}

			for (const FMovieSceneSubSequenceTreeEntry& Entry : Hierarchy->GetTree().GetAllData(SubSequenceIt.Node()))
			{
				const FMovieSceneSubSequenceData* SubData = Hierarchy->FindSubData(Entry.SequenceID);
				checkf(SubData, TEXT("Sub data does not exist for a SequenceID that exists in the hierarchical tree - this indicates a corrupt compilation product."));

				UMovieSceneSequence*      SubSequence = SubData     ? SubData->GetSequence()                      : nullptr;
				FMovieSceneCompiledDataID SubDataID   = SubSequence ? CompiledDataManager->GetDataID(SubSequence) : FMovieSceneCompiledDataID();
				if (!SubDataID.IsValid())
				{
					continue;
				}

				TArrayView<const FFrameTime> SubDeterminismFences = CompiledDataManager->GetEntryRef(SubDataID).DeterminismFences;
				if (SubDeterminismFences.Num() > 0)
				{
					TRange<FFrameTime>   InnerRange           = SubData->RootToSequenceTransform.TransformRangeUnwarped(RootClampRange);

					TArrayView<const FFrameTime> TraversedFences  = GetFencesWithinRange(SubDeterminismFences, InnerRange);
					if (TraversedFences.Num() > 0)
					{
						FMovieSceneWarpCounter WarpCounter;
						FFrameTime Unused;
						SubData->RootToSequenceTransform.TransformTime(RootClampRange.GetLowerBoundValue(), Unused, WarpCounter);

						FMovieSceneTimeTransform InverseTransform = SubData->RootToSequenceTransform.InverseFromWarp(WarpCounter);
						Algo::Transform(TraversedFences, RootDissectionTimes, [InverseTransform](FFrameTime In){ return In * InverseTransform; });
					}
				}
			}
		}
	}

	if (RootDissectionTimes.Num() > 0)
	{
		Algo::Sort(RootDissectionTimes);
		UE::MovieScene::DissectRange(RootDissectionTimes, Context.GetRange(), OutDissections);
	}
}

FInstanceHandle FSequenceUpdater_Hierarchical::GetOrCreateSequenceInstance(IMovieScenePlayer* InPlayer, FInstanceRegistry* InstanceRegistry, FRootInstanceHandle RootInstanceHandle, FMovieSceneSequenceID SequenceID)
{
	if (FSubInstanceData* Existing = SequenceInstances.Find(SequenceID))
	{
		Existing->bNeedsDestroy = false;
		return Existing->Handle;
	}

	FInstanceHandle InstanceHandle = InstanceRegistry->AllocateSubInstance(InPlayer, SequenceID, RootInstanceHandle);
	SequenceInstances.Add(SequenceID, FSubInstanceData { InstanceHandle });

	return InstanceHandle;
}

void FSequenceUpdater_Hierarchical::OverrideRootSequence(UMovieSceneEntitySystemLinker* InLinker, FRootInstanceHandle MasterInstanceHandle, FMovieSceneSequenceID NewRootOverrideSequenceID)
{
	if (RootOverrideSequenceID != NewRootOverrideSequenceID)
	{
		if (RootOverrideSequenceID == MovieSceneSequenceID::Root)
		{
			// When specifying a new root override where there was none before (ie, when we were previously evaluating from the master)
			// We unlink everything from the master sequence since we know they won't be necessary.
			// This is because the root sequence instance is handled separately in FSequenceUpdater_Hierarchical::Update, and it wouldn't
			// get automatically unlinked like other sub sequences would (by way of not being present in the ActiveSequences map)
			FInstanceRegistry* InstanceRegistry = InLinker->GetInstanceRegistry();
			InstanceRegistry->MutateInstance(MasterInstanceHandle).Ledger.UnlinkEverything(InLinker);
		}

		InvalidateCachedData(InLinker);
		RootOverrideSequenceID = NewRootOverrideSequenceID;
	}
}

void FSequenceUpdater_Hierarchical::Start(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer, const FMovieSceneContext& InContext)
{
}

void FSequenceUpdater_Hierarchical::Update(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer, const FMovieSceneContext& Context)
{
	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();
	UMovieSceneCompiledDataManager* CompiledDataManager = InPlayer->GetEvaluationTemplate().GetCompiledDataManager();

	FMovieSceneEvaluationFieldEntitySet EntitiesScratch;

	FInstanceHandle             RootInstanceHandle = InstanceHandle;
	FMovieSceneCompiledDataID   RootCompiledDataID = CompiledDataID;
	FSubSequencePath            RootOverridePath;
	FMovieSceneContext          RootContext = Context;

	TArray<FMovieSceneSequenceID, TInlineAllocator<16>> ActiveSequences;

	if (RootOverrideSequenceID != MovieSceneSequenceID::Root)
	{
		const FMovieSceneSequenceHierarchy* MasterHierarchy = CompiledDataManager->FindHierarchy(CompiledDataID);
		const FMovieSceneSubSequenceData*   SubData         = MasterHierarchy ? MasterHierarchy->FindSubData(RootOverrideSequenceID) : nullptr;
		UMovieSceneSequence*                RootSequence    = SubData ? SubData->GetSequence() : nullptr;
		if (ensure(RootSequence))
		{
			RootInstanceHandle = GetOrCreateSequenceInstance(InPlayer, InstanceRegistry, InstanceHandle, RootOverrideSequenceID);
			RootCompiledDataID = CompiledDataManager->GetDataID(RootSequence);
			RootContext        = Context.Transform(SubData->RootToSequenceTransform, SubData->TickResolution);

			RootOverridePath.Reset(RootOverrideSequenceID, MasterHierarchy);

			ActiveSequences.Add(RootOverrideSequenceID);
		}
	}

	FFrameNumber ImportTime = RootContext.GetEvaluationFieldTime();
	const bool bGatherEntities = !CachedEntityRange.Contains(ImportTime);

	// ------------------------------------------------------------------------------------------------
	// Handle the root sequence entities first
	{
		// Set the context for the root sequence instance
		FSequenceInstance& RootInstance = InstanceRegistry->MutateInstance(RootInstanceHandle);
		RootInstance.SetContext(RootContext);

		const FMovieSceneEntityComponentField* RootComponentField = CompiledDataManager->FindEntityComponentField(RootCompiledDataID);
		UMovieSceneSequence* RootSequence = CompiledDataManager->GetEntryRef(RootCompiledDataID).GetSequence();

		if (RootSequence == nullptr)
		{
			RootInstance.Ledger.UnlinkEverything(Linker);
		}
		else
		{
			// Update entities if necessary
			if (bGatherEntities)
			{
				CachedEntityRange = UpdateEntitiesForSequence(RootComponentField, ImportTime, EntitiesScratch);

				FEntityImportSequenceParams Params;
				Params.InstanceHandle = RootInstanceHandle;
				Params.RootInstanceHandle = InstanceHandle;
				Params.DefaultCompletionMode = RootSequence->DefaultCompletionMode;
				Params.HierarchicalBias = 0;

				RootInstance.Ledger.UpdateEntities(Linker, Params, RootComponentField, EntitiesScratch);
			}

			// Update any one-shot entities for the current root frame
			if (RootComponentField && RootComponentField->HasAnyOneShotEntities())
			{
				EntitiesScratch.Reset();
				RootComponentField->QueryOneShotEntities(RootContext.GetFrameNumberRange(), EntitiesScratch);

				if (EntitiesScratch.Num() != 0)
				{
					FEntityImportSequenceParams Params;
					Params.InstanceHandle = RootInstanceHandle;
					Params.RootInstanceHandle = InstanceHandle;
					Params.DefaultCompletionMode = RootSequence->DefaultCompletionMode;
					Params.HierarchicalBias = 0;

					RootInstance.Ledger.UpdateOneShotEntities(Linker, Params, RootComponentField, EntitiesScratch);
				}
			}
		}
	}

	// ------------------------------------------------------------------------------------------------
	// Handle sub sequence entities next
	const FMovieSceneSequenceHierarchy* RootOverrideHierarchy = CompiledDataManager->FindHierarchy(RootCompiledDataID);
	if (RootOverrideHierarchy)
	{
		FMovieSceneEvaluationTreeRangeIterator SubSequenceIt = RootOverrideHierarchy->GetTree().IterateFromTime(ImportTime);
		
		if (bGatherEntities)
		{
			CachedEntityRange = TRange<FFrameNumber>::Intersection(CachedEntityRange, SubSequenceIt.Range());
		}

		for (const FMovieSceneSubSequenceTreeEntry& Entry : RootOverrideHierarchy->GetTree().GetAllData(SubSequenceIt.Node()))
		{
			// When a root override path is specified, we always remap the 'local' sequence IDs to their equivalents from the master sequence.
			FMovieSceneSequenceID SequenceIDFromMaster = RootOverridePath.ResolveChildSequenceID(Entry.SequenceID);

			ActiveSequences.Add(SequenceIDFromMaster);

			const FMovieSceneSubSequenceData* SubData = RootOverrideHierarchy->FindSubData(Entry.SequenceID);
			checkf(SubData, TEXT("Sub data does not exist for a SequenceID that exists in the hierarchical tree - this indicates a corrupt compilation product."));

			UMovieSceneSequence* SubSequence = SubData->GetSequence();
			if (SubSequence == nullptr)
			{
				FInstanceHandle SubSequenceHandle = SequenceInstances.FindRef(SequenceIDFromMaster).Handle;
				if (SubSequenceHandle.IsValid())
				{
					FSequenceInstance& SubSequenceInstance = InstanceRegistry->MutateInstance(SubSequenceHandle);
					SubSequenceInstance.Ledger.UnlinkEverything(Linker);
				}
			}
			else
			{
				FMovieSceneCompiledDataID SubDataID = CompiledDataManager->GetDataID(SubSequence);

				// Set the context for the root sequence instance
				FInstanceHandle    SubSequenceHandle = GetOrCreateSequenceInstance(InPlayer, InstanceRegistry, InstanceHandle, SequenceIDFromMaster);
				FSequenceInstance& SubSequenceInstance = InstanceRegistry->MutateInstance(SubSequenceHandle);

				// Update the sub sequence's context
				FMovieSceneContext SubContext = RootContext.Transform(SubData->RootToSequenceTransform, SubData->TickResolution);
				SubContext.ReportOuterSectionRanges(SubData->PreRollRange.Value, SubData->PostRollRange.Value);
				SubContext.SetHierarchicalBias(SubData->HierarchicalBias);

				// Handle crossing a pre/postroll boundary
				const bool bWasPreRoll  = SubSequenceInstance.GetContext().IsPreRoll();
				const bool bWasPostRoll = SubSequenceInstance.GetContext().IsPostRoll();
				const bool bIsPreRoll   = SubContext.IsPreRoll();
				const bool bIsPostRoll  = SubContext.IsPostRoll();

				if (bWasPreRoll != bIsPreRoll || bWasPostRoll != bIsPostRoll)
				{
					// When crossing a pre/postroll boundary, we invalidate all entities currently imported, which results in them being re-imported 
					// with the same EntityID. This ensures that the state is maintained for such entities across prerolls (ie entities with a
					// spawnable binding component on them will not cause the spawnable to be destroyed and recreated again).
					// The one edge case that this could open up is where a preroll entity has meaningfully different components from its 'normal' entity,
					// and there are systems that track the link/unlink lifetime for such components. Under this circumstance, the unlink for the entity
					// will not be seen until the whole entity goes away, not just the preroll region. This is a very nuanced edge-case however, and can
					// be solved by giving the entities unique IDs (FMovieSceneEvaluationFieldEntityKey::EntityID) in the evaluation field.
					SubSequenceInstance.Ledger.Invalidate();
				}

				SubSequenceInstance.SetContext(SubContext);
				SubSequenceInstance.SetFinished(false);

				const FMovieSceneEntityComponentField* SubComponentField = CompiledDataManager->FindEntityComponentField(SubDataID);

				// Update entities if necessary
				const FFrameTime SubSequenceTime = SubContext.GetEvaluationFieldTime();

				FEntityImportSequenceParams Params;
				Params.InstanceHandle = SubSequenceHandle;
				Params.RootInstanceHandle = InstanceHandle;
				Params.DefaultCompletionMode = SubSequence->DefaultCompletionMode;
				Params.HierarchicalBias = SubData->HierarchicalBias;
				Params.bPreRoll  = bIsPreRoll;
				Params.bPostRoll = bIsPostRoll;
				Params.bHasHierarchicalEasing = SubData->bHasHierarchicalEasing;

				if (bGatherEntities)
				{
					EntitiesScratch.Reset();

					TRange<FFrameNumber> SubEntityRange = UpdateEntitiesForSequence(SubComponentField, SubSequenceTime, EntitiesScratch);

					SubSequenceInstance.Ledger.UpdateEntities(Linker, Params, SubComponentField, EntitiesScratch);

					// Clamp to the current warp loop if necessary
					FMovieSceneWarpCounter WarpCounter;
					FFrameTime Unused;
					SubData->RootToSequenceTransform.TransformTime(ImportTime, Unused, WarpCounter);

					if (WarpCounter.WarpCounts.Num() > 0)
					{
						FMovieSceneTimeTransform InverseTransform = SubData->RootToSequenceTransform.InverseFromWarp(WarpCounter);
						CachedEntityRange = TRange<FFrameNumber>::Intersection(CachedEntityRange, SubData->PlayRange.Value * InverseTransform);
					}

					const FMovieSceneSequenceTransform SequenceToRootOverrideTransform = FMovieSceneSequenceTransform(SubContext.GetSequenceToRootTransform()) * RootContext.GetRootToSequenceTransform();
					SubEntityRange = SequenceToRootOverrideTransform.TransformRangeConstrained(SubEntityRange);

					CachedEntityRange = TRange<FFrameNumber>::Intersection(CachedEntityRange, SubEntityRange);
				}

				// Update any one-shot entities for the sub sequence
				if (SubComponentField && SubComponentField->HasAnyOneShotEntities())
				{
					EntitiesScratch.Reset();
					SubComponentField->QueryOneShotEntities(SubContext.GetFrameNumberRange(), EntitiesScratch);

					if (EntitiesScratch.Num() != 0)
					{
						SubSequenceInstance.Ledger.UpdateOneShotEntities(Linker, Params, SubComponentField, EntitiesScratch);
					}
				}
			}
		}
	}

	FMovieSceneEntitySystemRunner* Runner = Linker->GetActiveRunner();
	check(Runner);

	for (auto InstanceIt = SequenceInstances.CreateIterator(); InstanceIt; ++InstanceIt)
	{
		FSubInstanceData SubData = InstanceIt.Value();
		if (SubData.bNeedsDestroy)
		{
			InstanceRegistry->DestroyInstance(SubData.Handle);
			InstanceIt.RemoveCurrent();
			continue;
		}

		Runner->MarkForUpdate(SubData.Handle, ERunnerUpdateFlags::None);

		if (!ActiveSequences.Contains(InstanceIt.Key()))
		{
			// Remove all entities from this instance since it is no longer active
			InstanceRegistry->MutateInstance(SubData.Handle).Finish(Linker);
			InstanceIt.Value().bNeedsDestroy = true;
		}
	}
}

void FSequenceUpdater_Hierarchical::Finish(UMovieSceneEntitySystemLinker* Linker, FRootInstanceHandle InstanceHandle, IMovieScenePlayer* InPlayer)
{
	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	// Finish all sub sequences as well
	for (TPair<FMovieSceneSequenceID, FSubInstanceData> Pair : SequenceInstances)
	{
		InstanceRegistry->MutateInstance(Pair.Value.Handle).Finish(Linker);
	}

	InvalidateCachedData(Linker);
}

void FSequenceUpdater_Hierarchical::Destroy(UMovieSceneEntitySystemLinker* Linker)
{
	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	for (TPair<FMovieSceneSequenceID, FSubInstanceData> Pair : SequenceInstances)
	{
		InstanceRegistry->DestroyInstance(Pair.Value.Handle);
	}
}

void FSequenceUpdater_Hierarchical::InvalidateCachedData(UMovieSceneEntitySystemLinker* Linker)
{
	CachedEntityRange = TRange<FFrameNumber>::Empty();

	FInstanceRegistry* InstanceRegistry = Linker->GetInstanceRegistry();

	for (TPair<FMovieSceneSequenceID, FSubInstanceData> Pair : SequenceInstances)
	{
		InstanceRegistry->MutateInstance(Pair.Value.Handle).Ledger.Invalidate();
	}
}



TRange<FFrameNumber> FSequenceUpdater_Hierarchical::UpdateEntitiesForSequence(const FMovieSceneEntityComponentField* ComponentField, FFrameTime SequenceTime, FMovieSceneEvaluationFieldEntitySet& OutEntities)
{
	TRange<FFrameNumber> CachedRange = TRange<FFrameNumber>::All();

	if (ComponentField)
	{
		// Extract all the entities for the current time
		ComponentField->QueryPersistentEntities(SequenceTime.FrameNumber, CachedRange, OutEntities);
	}

	return CachedRange;
}


} // namespace MovieScene
} // namespace UE
