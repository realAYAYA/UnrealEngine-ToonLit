// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookPackageData.h"

#include "Algo/AnyOf.h"
#include "Algo/Count.h"
#include "Algo/Find.h"
#include "Algo/Sort.h"
#include "Algo/Unique.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/ParallelFor.h"
#include "CompactBinaryTCP.h"
#include "Cooker/CookPlatformManager.h"
#include "Cooker/CookRequestCluster.h"
#include "Cooker/CookWorkerClient.h"
#include "Cooker/PackageTracker.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Containers/StringView.h"
#include "EditorDomain/EditorDomain.h"
#include "Engine/Console.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreMiscDefines.h"
#include "Misc/PackageAccessTrackingOps.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/PreloadableFile.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Serialization/CompactBinaryWriter.h"
#include "ShaderCompiler.h"
#include "UObject/Object.h"
#include "UObject/ObjectRedirector.h"
#include "UObject/Package.h"
#include "UObject/ReferenceChainSearch.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace UE::Cook
{

float GPollAsyncPeriod = .100f;
static FAutoConsoleVariableRef CVarPollAsyncPeriod(
	TEXT("cook.PollAsyncPeriod"),
	GPollAsyncPeriod,
	TEXT("Minimum time in seconds between PollPendingCookedPlatformDatas."),
	ECVF_Default);
	
//////////////////////////////////////////////////////////////////////////
// FPackageData
FPackagePlatformData::FPackagePlatformData()
	: bReachable(0), bVisitedByCluster(0), bSaveTimedOut(0), bCookable(1), bExplorable(1), bExplorableOverride(0)
	, bIterativelyUnmodified(0), bRegisteredForCachedObjectsInOuter(0), CookResults((uint8)ECookResult::NotAttempted)
{
}

void FPackagePlatformData::ResetReachable()
{
	SetReachable(false);
	SetVisitedByCluster(false);
	SetCookable(true);
	SetExplorable(true);
}

void FPackagePlatformData::MarkAsExplorable()
{
	ResetReachable();
	SetExplorableOverride(true);
}

void FPackagePlatformData::MarkCookableForWorker(FCookWorkerClient& CookWorkerClient)
{
	SetReachable(true);
	SetVisitedByCluster(true);
	SetExplorable(true);
	SetCookable(true);
	SetCookResults(ECookResult::NotAttempted);
}

bool FPackagePlatformData::NeedsCooking(const ITargetPlatform* PlatformItBelongsTo) const
{
	return IsReachable() && PlatformItBelongsTo != CookerLoadingPlatformKey && IsCookable() && !IsCookAttempted();
}

FPackageData::FPackageData(FPackageDatas& PackageDatas, const FName& InPackageName, const FName& InFileName)
	: GeneratedOwner(nullptr), PackageName(InPackageName), FileName(InFileName), PackageDatas(PackageDatas)
	, Instigator(EInstigator::NotYetRequested), bIsUrgent(0), bIsCookLast(0)
	, bIsVisited(0), bIsPreloadAttempted(0)
	, bIsPreloaded(0), bHasSaveCache(0), bPrepareSaveFailed(0), bPrepareSaveRequiresGC(0)
	, bCookedPlatformDataStarted(0), bCookedPlatformDataCalled(0), bCookedPlatformDataComplete(0)
	, MonitorCookResult((uint8)ECookResult::NotAttempted)
	, bInitializedGeneratorSave(0), bCompletedGeneration(0), bGenerated(0), bKeepReferencedDuringGC(0)
	, bWasCookedThisSession(0)
{
	SetState(EPackageState::Idle);
	SendToState(EPackageState::Idle, ESendFlags::QueueAdd, EStateChangeReason::Discovered);
}

FPackageData::~FPackageData()
{
	// ClearReferences should have been called earlier, but call it here in case it was missed
	ClearReferences();
	// We need to send OnLastCookedPlatformRemoved message to the monitor, so call SetPlatformsNotCooked
	ClearCookResults();
	// Update the monitor's counters and call exit functions
	SendToState(EPackageState::Idle, ESendFlags::QueueNone, EStateChangeReason::CookerShutdown);
}

void FPackageData::ClearReferences()
{
	DestroyGeneratorPackage();
}

const FName& FPackageData::GetPackageName() const
{
	return PackageName;
}

const FName& FPackageData::GetFileName() const
{
	return FileName;
}

void FPackageData::SetFileName(const FName& InFileName)
{
	FileName = InFileName;
}

int32 FPackageData::GetPlatformsNeedingCookingNum() const
{
	int32 Result = 0;
	for (const TPair<const ITargetPlatform*, FPackagePlatformData>& Pair : PlatformDatas)
	{
		if (Pair.Value.NeedsCooking(Pair.Key))
		{
			++Result;
		}
	}
	return Result;
}

bool FPackageData::IsPlatformVisitedByCluster(const ITargetPlatform* Platform) const
{
	const FPackagePlatformData* PlatformData = FindPlatformData(Platform);
	return PlatformData && PlatformData->IsVisitedByCluster();
}

bool FPackageData::HasReachablePlatforms(const TArrayView<const ITargetPlatform* const>& Platforms) const
{
	if (Platforms.Num() == 0)
	{
		return true;
	}
	if (PlatformDatas.Num() == 0)
	{
		return false;
	}

	for (const ITargetPlatform* QueryPlatform : Platforms)
	{
		const FPackagePlatformData* PlatformData = PlatformDatas.Find(QueryPlatform);
		if (!PlatformData || !PlatformData->IsReachable())
		{
			return false;
		}
	}
	return true;
}

bool FPackageData::AreAllReachablePlatformsVisitedByCluster() const
{
	for (const TPair<const ITargetPlatform*, FPackagePlatformData>& Pair : PlatformDatas)
	{
		if (Pair.Value.IsReachable() && !Pair.Value.IsVisitedByCluster())
		{
			return false;
		}
	}
	return true;
}

void FPackageData::GetReachablePlatformsForInstigator(UCookOnTheFlyServer& COTFS, FName InInstigator,
	TArray<const ITargetPlatform*>& Platforms)
{
	return GetReachablePlatformsForInstigator(COTFS,
		COTFS.PackageDatas->TryAddPackageDataByPackageName(InInstigator), Platforms);
}

void FPackageData::GetReachablePlatformsForInstigator(UCookOnTheFlyServer& COTFS, UE::Cook::FPackageData* InInstigator,
	TArray<const ITargetPlatform*>& Platforms)
{
	if (InInstigator)
	{
		InInstigator->GetReachablePlatforms(Platforms);
	}
	else
	{
		const TArray<const ITargetPlatform*>& SessionPlatforms = COTFS.PlatformManager->GetSessionPlatforms();
		Platforms.Reset(SessionPlatforms.Num() + 1);
		Platforms.Append(SessionPlatforms);
	}
}


void FPackageData::AddReachablePlatforms(FRequestCluster& RequestCluster,
	TConstArrayView<const ITargetPlatform*> Platforms, FInstigator&& InInstigator)
{
	AddReachablePlatformsInternal(*this, Platforms, MoveTemp(InInstigator));
}

void FPackageData::AddReachablePlatformsInternal(FPackageData& PackageData,
	TConstArrayView<const ITargetPlatform*> Platforms, FInstigator&& InInstigator)
{
	// This is a static helper function to make it impossible to make a typo and use this->Instigator instead of InInstigator
	bool bSessionPlatformModified = false;
	for (const ITargetPlatform* Platform : Platforms)
	{
		FPackagePlatformData& PlatformData = PackageData.FindOrAddPlatformData(Platform);
		bSessionPlatformModified |= (Platform != CookerLoadingPlatformKey && !PlatformData.IsReachable());
		PlatformData.SetReachable(true);
	}
	if (bSessionPlatformModified)
	{
		PackageData.SetInstigatorInternal(MoveTemp(InInstigator));
	}
}

void FPackageData::QueueAsDiscovered(FInstigator&& InInstigator, FDiscoveredPlatformSet&& ReachablePlatforms, bool bUrgent)
{
	QueueAsDiscoveredInternal(*this, MoveTemp(InInstigator), MoveTemp(ReachablePlatforms), bUrgent);
}

void FPackageData::QueueAsDiscoveredInternal(FPackageData& PackageData, FInstigator&& InInstigator,
	FDiscoveredPlatformSet&& ReachablePlatforms, bool bUrgent)
{
	// This is a static helper function to make it impossible to make a typo and use this->Instigator instead of InInstigator
	TRingBuffer<FDiscoveryQueueElement>& Queue = PackageData.PackageDatas.GetRequestQueue().GetDiscoveryQueue();
	Queue.Add(FDiscoveryQueueElement{ &PackageData, MoveTemp(InInstigator), MoveTemp(ReachablePlatforms), bUrgent });
}

void FPackageData::SetIsUrgent(bool Value)
{
	bool OldValue = static_cast<bool>(bIsUrgent);
	if (OldValue != Value)
	{
		bIsUrgent = Value != 0;
		check(IsInProgress() || !bIsUrgent);
		PackageDatas.GetMonitor().OnUrgencyChanged(*this);
	}
}

void FPackageData::AddUrgency(bool bUrgent, bool bAllowUpdateState)
{
	if (!bUrgent)
	{
		return;
	}
	bool bWasUrgent = GetIsUrgent();
	SetIsUrgent(true);
	if (!bWasUrgent && bAllowUpdateState)
	{
		SendToState(GetState(), ESendFlags::QueueAddAndRemove, EStateChangeReason::UrgencyUpdated);
	}
}

void FPackageData::SetIsCookLast(bool bValue)
{
	bool bWasCookLast = GetIsCookLast();
	if (bWasCookLast != bValue)
	{
		bIsCookLast = static_cast<uint32>(bValue);
		PackageDatas.GetMonitor().OnCookLastChanged(*this);
	}
}

void FPackageData::ClearCookLastUrgency()
{
	if (!GetIsCookLast() || !GetIsUrgent())
	{
		return;
	}
	SetIsUrgent(false);
}

void FPackageData::SetInstigator(FRequestCluster& Cluster, FInstigator&& InInstigator)
{
	SetInstigatorInternal(MoveTemp(InInstigator));
}

void FPackageData::SetInstigator(FCookWorkerClient& Cluster, FInstigator&& InInstigator)
{
	SetInstigatorInternal(MoveTemp(InInstigator));
}

void FPackageData::SetInstigator(FGeneratorPackage& Cluster, FInstigator&& InInstigator)
{
	SetInstigatorInternal(MoveTemp(InInstigator));
}

void FPackageData::SetInstigatorInternal(FInstigator&& InInstigator)
{
	if (this->Instigator.Category == EInstigator::NotYetRequested)
	{
		OnPackageDataFirstMarkedReachable(MoveTemp(InInstigator));
	}
}

void FPackageData::ClearInProgressData()
{
	SetIsUrgent(false);
	CompletionCallback = FCompletionCallback();
}

void FPackageData::SetPlatformsCooked(
	const TConstArrayView<const ITargetPlatform*> TargetPlatforms,
	const TConstArrayView<ECookResult> Result,
	const bool bInWasCookedThisSession)
{
	check(TargetPlatforms.Num() == Result.Num());
	for (int32 n = 0; n < TargetPlatforms.Num(); ++n)
	{
		SetPlatformCooked(TargetPlatforms[n], Result[n], bInWasCookedThisSession);
	}
}

void FPackageData::SetPlatformsCooked(
	const TConstArrayView<const ITargetPlatform*> TargetPlatforms, 
	ECookResult Result,
	const bool bInWasCookedThisSession)
{
	for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
	{
		SetPlatformCooked(TargetPlatform, Result, bInWasCookedThisSession);
	}
}

void FPackageData::SetPlatformCooked(
	const ITargetPlatform* TargetPlatform, 
	ECookResult CookResult, 
	const bool bInWasCookedThisSession)
{
	bWasCookedThisSession |= bInWasCookedThisSession && (CookResult == ECookResult::Succeeded);

	bool bNewCookAttemptedValue = (CookResult != ECookResult::NotAttempted);
	bool bModifiedCookAttempted = false;
	bool bHasAnyOtherCookAttempted = false;
	bool bExists = false;
	for (TPair<const ITargetPlatform*, FPackagePlatformData>& Pair : PlatformDatas)
	{
		if (Pair.Key == TargetPlatform)
		{
			bExists = true;
			bModifiedCookAttempted = bModifiedCookAttempted | (Pair.Value.IsCookAttempted() != bNewCookAttemptedValue);
			Pair.Value.SetCookResults(CookResult);
			// Clear the SaveTimedOut when get a cook result, in case we save again later and need to allow retry again
			Pair.Value.SetSaveTimedOut(false);
		}
		else
		{
			bHasAnyOtherCookAttempted = bHasAnyOtherCookAttempted | Pair.Value.IsCookAttempted();
		}
	}

	if (!bExists && bNewCookAttemptedValue)
	{
		FPackagePlatformData& Value = PlatformDatas.FindOrAdd(TargetPlatform);
		Value.SetCookResults(CookResult);
		Value.SetSaveTimedOut(false);
		bModifiedCookAttempted = true;
	}

	if (bModifiedCookAttempted && !bHasAnyOtherCookAttempted)
	{
		if (bNewCookAttemptedValue)
		{
			PackageDatas.GetMonitor().OnFirstCookedPlatformAdded(*this, CookResult);
		}
		else
		{
			bWasCookedThisSession = false;
			PackageDatas.GetMonitor().OnLastCookedPlatformRemoved(*this);
		}
	}
}

void FPackageData::ClearCookResults(const TConstArrayView<const ITargetPlatform*> TargetPlatforms)
{
	for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
	{
		ClearCookResults(TargetPlatform);
	}
}

void FPackageData::ResetReachable()
{
	for (TPair<const ITargetPlatform*, FPackagePlatformData>& Pair : PlatformDatas)
	{
		Pair.Value.ResetReachable();
	}
}

void FPackageData::ClearCookResults()
{
	bool bModifiedCookAttempted = false;
	for (TPair<const ITargetPlatform*, FPackagePlatformData>& Pair : PlatformDatas)
	{
		bModifiedCookAttempted = bModifiedCookAttempted | Pair.Value.IsCookAttempted();
		Pair.Value.SetCookResults(ECookResult::NotAttempted);
		Pair.Value.SetSaveTimedOut(false);
	}
	if (bModifiedCookAttempted)
	{
		bWasCookedThisSession = false;
		PackageDatas.GetMonitor().OnLastCookedPlatformRemoved(*this);
	}
}

void FPackageData::ClearCookResults(const ITargetPlatform* TargetPlatform)
{
	bool bHasAnyOthers = false;
	bool bModifiedCookAttempted = false;
	for (TPair<const ITargetPlatform*, FPackagePlatformData>& Pair : PlatformDatas)
	{
		if (Pair.Key == TargetPlatform)
		{
			bModifiedCookAttempted = bModifiedCookAttempted | Pair.Value.IsCookAttempted();
			Pair.Value.SetCookResults(ECookResult::NotAttempted);
			Pair.Value.SetSaveTimedOut(false);
		}
		else
		{
			bHasAnyOthers = bHasAnyOthers | Pair.Value.IsCookAttempted();
		}
	}
	if (bModifiedCookAttempted && !bHasAnyOthers)
	{
		bWasCookedThisSession = false;
		PackageDatas.GetMonitor().OnLastCookedPlatformRemoved(*this);
	}
}

const TSortedMap<const ITargetPlatform*, FPackagePlatformData, TInlineAllocator<1>>& FPackageData::GetPlatformDatas() const
{
	return PlatformDatas;
}

TSortedMap<const ITargetPlatform*, FPackagePlatformData, TInlineAllocator<1>>& FPackageData::GetPlatformDatasConstKeysMutableValues()
{
	return PlatformDatas;
}

FPackagePlatformData& FPackageData::FindOrAddPlatformData(const ITargetPlatform* TargetPlatform)
{
	return PlatformDatas.FindOrAdd(TargetPlatform);
}

FPackagePlatformData* FPackageData::FindPlatformData(const ITargetPlatform* TargetPlatform)
{
	return PlatformDatas.Find(TargetPlatform);
}

const FPackagePlatformData* FPackageData::FindPlatformData(const ITargetPlatform* TargetPlatform) const
{
	return PlatformDatas.Find(TargetPlatform);
}

bool FPackageData::HasAnyCookedPlatform() const
{
	return Algo::AnyOf(PlatformDatas,
		[](const TPair<const ITargetPlatform*, FPackagePlatformData>& Pair)
		{
			return Pair.Key != CookerLoadingPlatformKey && Pair.Value.IsCookAttempted();
		});
}

bool FPackageData::HasAnyCookedPlatforms(const TArrayView<const ITargetPlatform* const>& Platforms,
	bool bIncludeFailed) const
{
	if (PlatformDatas.Num() == 0)
	{
		return false;
	}

	for (const ITargetPlatform* QueryPlatform : Platforms)
	{
		if (HasCookedPlatform(QueryPlatform, bIncludeFailed))
		{
			return true;
		}
	}
	return false;
}

bool FPackageData::HasAllCookedPlatforms(const TArrayView<const ITargetPlatform* const>& Platforms,
	bool bIncludeFailed) const
{
	if (Platforms.Num() == 0)
	{
		return true;
	}
	if (PlatformDatas.Num() == 0)
	{
		return false;
	}

	for (const ITargetPlatform* QueryPlatform : Platforms)
	{
		if (!HasCookedPlatform(QueryPlatform, bIncludeFailed))
		{
			return false;
		}
	}
	return true;
}

bool FPackageData::HasCookedPlatform(const ITargetPlatform* Platform, bool bIncludeFailed) const
{
	ECookResult Result = GetCookResults(Platform);
	return (Result == ECookResult::Succeeded) | ((Result != ECookResult::NotAttempted) & (bIncludeFailed != 0));
}

ECookResult FPackageData::GetCookResults(const ITargetPlatform* Platform) const
{
	const FPackagePlatformData* PlatformData = PlatformDatas.Find(Platform);
	if (PlatformData)
	{
		return PlatformData->GetCookResults();
	}
	return ECookResult::NotAttempted;
}

UPackage* FPackageData::GetPackage() const
{
	return Package.Get();
}

void FPackageData::SetPackage(UPackage* InPackage)
{
	Package = InPackage;
}

EPackageState FPackageData::GetState() const
{
	return static_cast<EPackageState>(State);
}

/** Boilerplate-reduction struct that defines all multi-state properties and sets them based on the given state. */
struct FStateProperties
{
	EPackageStateProperty Properties;
	explicit FStateProperties(EPackageState InState)
	{
		switch (InState)
		{
		case EPackageState::Idle:
			Properties = EPackageStateProperty::None;
			break;
		case EPackageState::Request:
			Properties = EPackageStateProperty::InProgress;
			break;
		case EPackageState::AssignedToWorker:
			Properties = EPackageStateProperty::InProgress;
			break;
		case EPackageState::LoadPrepare:
			Properties = EPackageStateProperty::InProgress | EPackageStateProperty::Loading;
			break;
		case EPackageState::LoadReady:
			Properties = EPackageStateProperty::InProgress | EPackageStateProperty::Loading;
			break;
		// TODO_SaveQueue: When we add state PrepareForSave, it will also have bHasPackage = true, 
		case EPackageState::Save:
			Properties = EPackageStateProperty::InProgress | EPackageStateProperty::HasPackage;
			break;
		default:
			check(false);
			Properties = EPackageStateProperty::None;
			break;
		}
	}
};

void FPackageData::SendToState(EPackageState NextState, ESendFlags SendFlags, EStateChangeReason ReleaseSaveReason)
{
	EPackageState OldState = GetState();
	switch (OldState)
	{
	case EPackageState::Idle:
		OnExitIdle();
		break;
	case EPackageState::Request:
		if (!!(SendFlags & ESendFlags::QueueRemove))
		{
			ensure(PackageDatas.GetRequestQueue().Remove(this) == 1);
		}
		OnExitRequest();
		break;
	case EPackageState::AssignedToWorker:
		if (!!(SendFlags & ESendFlags::QueueRemove))
		{
			ensure(PackageDatas.GetAssignedToWorkerSet().Remove(this) == 1);
		}
		OnExitAssignedToWorker();
		break;
	case EPackageState::LoadPrepare:
		if (!!(SendFlags & ESendFlags::QueueRemove))
		{
			ensure(PackageDatas.GetLoadPrepareQueue().Remove(this) == 1);
		}
		OnExitLoadPrepare();
		break;
	case EPackageState::LoadReady:
		if (!!(SendFlags & ESendFlags::QueueRemove))
		{
			ensure(PackageDatas.GetLoadReadyQueue().Remove(this) == 1);
		}
		OnExitLoadReady();
		break;
	case EPackageState::Save:
		if (!!(SendFlags & ESendFlags::QueueRemove))
		{
			ensure(PackageDatas.GetSaveQueue().Remove(this) == 1);
		}
		OnExitSave(ReleaseSaveReason);
		break;
	default:
		check(false);
		break;
	}

	FStateProperties OldProperties(OldState);
	FStateProperties NewProperties(NextState);
	// Exit state properties from highest to lowest; enter state properties from lowest to highest.
	// This ensures that properties that rely on earlier properties are constructed later and torn down earlier
	// than the earlier properties.
	for (EPackageStateProperty Iterator = EPackageStateProperty::Max;
		Iterator >= EPackageStateProperty::Min;
		Iterator = static_cast<EPackageStateProperty>(static_cast<uint32>(Iterator) >> 1))
	{
		if (((OldProperties.Properties & Iterator) != EPackageStateProperty::None) &
			((NewProperties.Properties & Iterator) == EPackageStateProperty::None))
		{
			switch (Iterator)
			{
			case EPackageStateProperty::InProgress:
				OnExitInProgress();
				break;
			case EPackageStateProperty::Loading:
				OnExitLoading();
				break;
			case EPackageStateProperty::HasPackage:
				OnExitHasPackage();
				break;
			default:
				check(false);
				break;
			}
		}
	}
	for (EPackageStateProperty Iterator = EPackageStateProperty::Min;
		Iterator <= EPackageStateProperty::Max;
		Iterator = static_cast<EPackageStateProperty>(static_cast<uint32>(Iterator) << 1))
	{
		if (((OldProperties.Properties & Iterator) == EPackageStateProperty::None) &
			((NewProperties.Properties & Iterator) != EPackageStateProperty::None))
		{
			switch (Iterator)
			{
			case EPackageStateProperty::InProgress:
				OnEnterInProgress();
				break;
			case EPackageStateProperty::Loading:
				OnEnterLoading();
				break;
			case EPackageStateProperty::HasPackage:
				OnEnterHasPackage();
				break;
			default:
				check(false);
				break;
			}
		}
	}


	SetState(NextState);
	switch (NextState)
	{
	case EPackageState::Idle:
		OnEnterIdle();
		break;
	case EPackageState::Request:
		OnEnterRequest();
		if (((SendFlags & ESendFlags::QueueAdd) != ESendFlags::QueueNone))
		{
			PackageDatas.GetRequestQueue().AddRequest(this);
		}
		break;
	case EPackageState::AssignedToWorker:
		OnEnterAssignedToWorker();
		if (((SendFlags & ESendFlags::QueueAdd) != ESendFlags::QueueNone))
		{
			PackageDatas.GetAssignedToWorkerSet().Add(this);
		}
		break;
	case EPackageState::LoadPrepare:
		OnEnterLoadPrepare();
		if ((SendFlags & ESendFlags::QueueAdd) != ESendFlags::QueueNone)
		{
			if (GetIsUrgent())
			{
				PackageDatas.GetLoadPrepareQueue().AddFront(this);
			}
			else
			{
				PackageDatas.GetLoadPrepareQueue().Add(this);
			}
		}
		break;
	case EPackageState::LoadReady:
		OnEnterLoadReady();
		if ((SendFlags & ESendFlags::QueueAdd) != ESendFlags::QueueNone)
		{
			if (GetIsUrgent())
			{
				PackageDatas.GetLoadReadyQueue().AddFront(this);
			}
			else
			{
				PackageDatas.GetLoadReadyQueue().Add(this);
			}
		}
		break;
	case EPackageState::Save:
		OnEnterSave();
		if (((SendFlags & ESendFlags::QueueAdd) != ESendFlags::QueueNone))
		{
			if (GetIsUrgent())
			{
				PackageDatas.GetSaveQueue().AddFront(this);
			}
			else
			{
				PackageDatas.GetSaveQueue().Add(this);
			}
		}
		break;
	default:
		check(false);
		break;
	}

	PackageDatas.GetMonitor().OnStateChanged(*this, OldState);
}

void FPackageData::CheckInContainer() const
{
	switch (GetState())
	{
	case EPackageState::Idle:
		break;
	case EPackageState::Request:
		check(PackageDatas.GetRequestQueue().Contains(this));
		break;
	case EPackageState::AssignedToWorker:
		check(PackageDatas.GetAssignedToWorkerSet().Contains(this));
		break;
	case EPackageState::LoadPrepare:
		check(PackageDatas.GetLoadPrepareQueue().Contains(this));
		break;
	case EPackageState::LoadReady:
		check(Algo::Find(PackageDatas.GetLoadReadyQueue(), this) != nullptr);
		break;
	case EPackageState::Save:
		// The save queue is huge and often pushed at end. Check last element first and then scan.
		check(PackageDatas.GetSaveQueue().Num() && (PackageDatas.GetSaveQueue().Last() == this || Algo::Find(PackageDatas.GetSaveQueue(), this)));
		break;
	default:
		check(false);
		break;
	}
}

bool FPackageData::IsInProgress() const
{
	return IsInStateProperty(EPackageStateProperty::InProgress);
}

bool FPackageData::IsInStateProperty(EPackageStateProperty Property) const
{
	return (FStateProperties(GetState()).Properties & Property) != EPackageStateProperty::None;
}

void FPackageData::OnEnterIdle()
{
	// Note that this might be on construction of the PackageData
}

void FPackageData::OnExitIdle()
{
}

void FPackageData::OnEnterRequest()
{
}

void FPackageData::OnExitRequest()
{
}

void FPackageData::OnEnterAssignedToWorker()
{
}

void FPackageData::SetWorkerAssignment(FWorkerId InWorkerAssignment, ESendFlags SendFlags)
{
	if (WorkerAssignment.IsValid())
	{
		checkf(InWorkerAssignment.IsInvalid(), TEXT("Package %s is being assigned to worker %d while it is already assigned to worker %d."),
			*GetPackageName().ToString(), WorkerAssignment.GetRemoteIndex(), WorkerAssignment.GetRemoteIndex());
		if (EnumHasAnyFlags(SendFlags, ESendFlags::QueueRemove))
		{
			PackageDatas.GetCookOnTheFlyServer().NotifyRemovedFromWorker(*this);
		}
		WorkerAssignment = FWorkerId::Invalid();
	}
	else
	{
		if (InWorkerAssignment.IsValid())
		{
			checkf(GetState() == EPackageState::AssignedToWorker, TEXT("Package %s is being assigned to worker %d while in a state other than AssignedToWorker. This is invalid."),
				*GetPackageName().ToString(), GetWorkerAssignment().GetRemoteIndex());
		}
		WorkerAssignment = InWorkerAssignment;
	}
}

void FPackageData::OnExitAssignedToWorker()
{
	SetWorkerAssignment(FWorkerId::Invalid());
}

void FPackageData::OnEnterLoadPrepare()
{
}

void FPackageData::OnExitLoadPrepare()
{
}

void FPackageData::OnEnterLoadReady()
{
}

void FPackageData::OnExitLoadReady()
{
}

void FPackageData::OnEnterSave()
{
	check(GetPackage() != nullptr && GetPackage()->IsFullyLoaded());

	check(!HasPrepareSaveFailed());
	CheckObjectCacheEmpty();
	CheckCookedPlatformDataEmpty();
}

void FPackageData::OnExitSave(EStateChangeReason ReleaseSaveReason)
{
	PackageDatas.GetCookOnTheFlyServer().ReleaseCookedPlatformData(*this, ReleaseSaveReason);
	ClearObjectCache();
	SetHasPrepareSaveFailed(false);
	SetIsPrepareSaveRequiresGC(false);
}

void FPackageData::OnEnterInProgress()
{
	PackageDatas.GetMonitor().OnInProgressChanged(*this, true);
}

void FPackageData::OnExitInProgress()
{
	PackageDatas.GetMonitor().OnInProgressChanged(*this, false);
	UE::Cook::FCompletionCallback LocalCompletionCallback(MoveTemp(GetCompletionCallback()));
	if (LocalCompletionCallback)
	{
		LocalCompletionCallback(this);
	}
	ClearInProgressData();
}

void FPackageData::OnEnterLoading()
{
	CheckPreloadEmpty();
}

void FPackageData::OnExitLoading()
{
	ClearPreload();
}

void FPackageData::OnEnterHasPackage()
{
}

void FPackageData::OnExitHasPackage()
{
	SetPackage(nullptr);
}

void FPackageData::OnPackageDataFirstMarkedReachable(FInstigator&& InInstigator)
{
	TracePackage(GetPackageName().ToUnstableInt(), GetPackageName().ToString());
	Instigator = MoveTemp(InInstigator);
	PackageDatas.DebugInstigator(*this);
	PackageDatas.UpdateThreadsafePackageData(*this);
}

void FPackageData::SetState(EPackageState NextState)
{
	State = static_cast<uint32>(NextState);
}

FCompletionCallback& FPackageData::GetCompletionCallback()
{
	return CompletionCallback;
}

void FPackageData::AddCompletionCallback(TConstArrayView<const ITargetPlatform*> TargetPlatforms, 
	FCompletionCallback&& InCompletionCallback)
{
	if (!InCompletionCallback)
	{
		return;
	}

	for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
	{
		FPackagePlatformData* PlatformData = FindPlatformData(TargetPlatform);
		// Adding a completion callback is only allowed after marking the requested platforms as reachable
		check(PlatformData);
		check(PlatformData->IsReachable());
		// Adding a completion callback is only allowed after putting the PackageData in progress.
		// If it's not in progress because it already finished the desired platforms, that is allowed.
		check(IsInProgress() || PlatformData->IsCookAttempted() || !PlatformData->IsCookable());
	}

	if (IsInProgress())
	{
		// We don't yet have a mechanism for calling two completion callbacks.
		// CompletionCallbacks only come from external requests, and it should not be possible to request twice,
		// so a failed check here shouldn't happen.
		check(!CompletionCallback);
		CompletionCallback = MoveTemp(InCompletionCallback);
	}
	else
	{
		// Already done; call the completioncallback immediately
		InCompletionCallback(this);
	}
}

bool FPackageData::TryPreload()
{
	check(IsInStateProperty(EPackageStateProperty::Loading));
	if (GetIsPreloadAttempted())
	{
		return true;
	}
	if (FindObjectFast<UPackage>(nullptr, GetPackageName()))
	{
		if (AsyncRequest && !AsyncRequest->bHasFinished)
		{
			// In case of async loading, the object can be found while still being asynchronously serialized, we need to wait until 
			// the callback is called and the async request is completely done.
			return false;
		}

		// If the package has already loaded, then there is no point in further preloading
		ClearPreload();
		SetIsPreloadAttempted(true);
		return true;
	}
	if (IsGenerated())
	{
		// Deferred populate generated packages are loaded from their generator, not from disk
		ClearPreload();
		SetIsPreloadAttempted(true);
		return true;
	}
	if (IsAsyncLoadingMultithreaded())
	{
		if (!AsyncRequest.IsValid())
		{
			PackageDatas.GetMonitor().OnPreloadAllocatedChanged(*this, true);
			AsyncRequest = MakeShared<FAsyncRequest>();
			AsyncRequest->RequestID = LoadPackageAsync(
				GetFileName().ToString(), 
				FLoadPackageAsyncDelegate::CreateLambda(
					[AsyncRequest = AsyncRequest](const FName&, UPackage*, EAsyncLoadingResult::Type) { AsyncRequest->bHasFinished = true; }
				),
				32 /* Use arbitrary higher priority for preload as we're going to need them very soon */
			);
		}

		// always return false so we continue to check the status of the load until FindObjectFast above finds the loaded object
		return false;
	}
	if (!PreloadableFile.Get())
	{
		if (FEditorDomain* EditorDomain(FEditorDomain::Get());
			EditorDomain && EditorDomain->IsReadingPackages())
		{
			EditorDomain->PrecachePackageDigest(GetPackageName());
		}
		TStringBuilder<NAME_SIZE> FileNameString;
		GetFileName().ToString(FileNameString);
		PreloadableFile.Set(MakeShared<FPreloadableArchive>(FileNameString.ToString()), *this);
		PreloadableFile.Get()->InitializeAsync([this]()
			{
				TStringBuilder<NAME_SIZE> FileNameString;
				// Note this async callback has an read of this->GetFilename and a write of PreloadableFileOpenResult
				// outside of a critical section. This read and write is allowed because GetFilename does
				// not change until this is destructed, and the destructor does not run and other threads do not read
				// or write PreloadableFileOpenResult until after PreloadableFile.Get() has finished initialization
				// and this callback is therefore complete.
				// The code that accomplishes that waiting is in TryPreload (IsInitialized) and ClearPreload (ReleaseCache)
				this->GetFileName().ToString(FileNameString);
				FPackagePath PackagePath = FPackagePath::FromLocalPath(FileNameString);
				FOpenPackageResult Result = IPackageResourceManager::Get().OpenReadPackage(PackagePath);
				if (Result.Archive)
				{
					this->PreloadableFileOpenResult.CopyMetaData(Result);
				}
				return Result.Archive.Release();
			},
			FPreloadableFile::Flags::PreloadHandle | FPreloadableFile::Flags::Prime);
	}
	const TSharedPtr<FPreloadableArchive>& FilePtr = PreloadableFile.Get();
	if (!FilePtr->IsInitialized())
	{
		if (GetIsUrgent())
		{
			// For urgent requests, wait on them to finish preloading rather than letting them run asynchronously
			// and coming back to them later
			FilePtr->WaitForInitialization();
			check(FilePtr->IsInitialized());
		}
		else
		{
			return false;
		}
	}
	if (FilePtr->TotalSize() < 0)
	{
		UE_LOG(LogCook, Warning, TEXT("Failed to find file when preloading %s."), *GetFileName().ToString());
		SetIsPreloadAttempted(true);
		PreloadableFile.Reset(*this);
		PreloadableFileOpenResult = FOpenPackageResult();
		return true;
	}

	TStringBuilder<NAME_SIZE> FileNameString;
	GetFileName().ToString(FileNameString);
	if (!IPackageResourceManager::TryRegisterPreloadableArchive(FPackagePath::FromLocalPath(FileNameString),
		FilePtr, PreloadableFileOpenResult))
	{
		UE_LOG(LogCook, Warning, TEXT("Failed to register %s for preload."), *GetFileName().ToString());
		SetIsPreloadAttempted(true);
		PreloadableFile.Reset(*this);
		PreloadableFileOpenResult = FOpenPackageResult();
		return true;
	}

	SetIsPreloaded(true);
	SetIsPreloadAttempted(true);
	return true;
}

void FPackageData::FTrackedPreloadableFilePtr::Set(TSharedPtr<FPreloadableArchive>&& InPtr, FPackageData& Owner)
{
	Reset(Owner);
	if (InPtr)
	{
		Ptr = MoveTemp(InPtr);
		Owner.PackageDatas.GetMonitor().OnPreloadAllocatedChanged(Owner, true);
	}
}

void FPackageData::FTrackedPreloadableFilePtr::Reset(FPackageData& Owner)
{
	if (Ptr)
	{
		Owner.PackageDatas.GetMonitor().OnPreloadAllocatedChanged(Owner, false);
		Ptr.Reset();
	}
}

void FPackageData::ClearPreload()
{
	if (AsyncRequest)
	{
		if (!AsyncRequest->bHasFinished)
		{
			FlushAsyncLoading(AsyncRequest->RequestID);
			check(AsyncRequest->bHasFinished);
		}
		PackageDatas.GetMonitor().OnPreloadAllocatedChanged(*this, false);
		AsyncRequest.Reset();
	}

	const TSharedPtr<FPreloadableArchive>& FilePtr = PreloadableFile.Get();
	if (GetIsPreloaded())
	{
		check(FilePtr);
		TStringBuilder<NAME_SIZE> FileNameString;
		GetFileName().ToString(FileNameString);
		if (IPackageResourceManager::UnRegisterPreloadableArchive(FPackagePath::FromLocalPath(FileNameString)))
		{
			UE_LOG(LogCook, Display, TEXT("PreloadableFile was created for %s but never used. This is wasteful and bad for cook performance."),
				*PackageName.ToString());
		}
		FilePtr->ReleaseCache(); // ReleaseCache to conserve memory if the Linker still has a pointer to it
	}
	else
	{
		check(!FilePtr || !FilePtr->IsCacheAllocated());
	}

	PreloadableFile.Reset(*this);
	PreloadableFileOpenResult = FOpenPackageResult();
	SetIsPreloaded(false);
	SetIsPreloadAttempted(false);
}

void FPackageData::CheckPreloadEmpty()
{
	check(!AsyncRequest);
	check(!GetIsPreloadAttempted());
	check(!PreloadableFile.Get());
	check(!GetIsPreloaded());
}

TArray<FCachedObjectInOuter>& FPackageData::GetCachedObjectsInOuter()
{
	return CachedObjectsInOuter;
}

const TArray<FCachedObjectInOuter>& FPackageData::GetCachedObjectsInOuter() const
{
	return CachedObjectsInOuter;
}

void FPackageData::CheckObjectCacheEmpty() const
{
	check(CachedObjectsInOuter.Num() == 0);
	check(!GetHasSaveCache());
}

void FPackageData::CreateObjectCache()
{
	if (GetHasSaveCache())
	{
		return;
	}

	UPackage* LocalPackage = GetPackage();
	if (LocalPackage && LocalPackage->IsFullyLoaded())
	{
		PackageName = LocalPackage->GetFName();
		TArray<UObject*> ObjectsInOuter;
		// ignore RF_Garbage objects; they will not be serialized out so we don't need to call
		// BeginCacheForCookedPlatformData on them
		GetObjectsWithOuter(LocalPackage, ObjectsInOuter, true /* bIncludeNestedObjects */, RF_NoFlags, EInternalObjectFlags::Garbage);
		CachedObjectsInOuter.Reset(ObjectsInOuter.Num());
		for (UObject* Object : ObjectsInOuter)
		{
			FWeakObjectPtr ObjectWeakPointer(Object);
			check(ObjectWeakPointer.Get()); // GetObjectsWithOuter with Garbage filtered out should only return valid-for-weakptr objects
			CachedObjectsInOuter.Emplace(ObjectWeakPointer);
		}

		for (TPair<const ITargetPlatform*, FPackagePlatformData>& Pair : PlatformDatas)
		{
			FPackagePlatformData& PlatformData = Pair.Value;
			check(!PlatformData.IsRegisteredForCachedObjectsInOuter());
			if (PlatformData.NeedsCooking(Pair.Key))
			{
				PlatformData.SetRegisteredForCachedObjectsInOuter(true);
			}
		}

		SetHasSaveCache(true);
	}
	else
	{
		check(false);
	}
}

static TArray<UObject*> SetDifference(TArray<UObject*>& A, TArray<UObject*>& B)
{
	Algo::Sort(A); // Don't use TArray.Sort, it sorts pointers as references and we want to sort them as pointers
	Algo::Sort(B);
	int32 ANum = A.Num();
	int32 BNum = B.Num();
	UObject** AData = A.GetData();
	UObject** BData = B.GetData();

	// Always move to the smallest next element from the two remaining lists and if it's in one set and not the
	// other add it to the output if in A or skip it if in B.
	int32 AIndex = 0;
	int32 BIndex = 0;
	TArray<UObject*> AMinusB;
	while (AIndex < ANum && BIndex < BNum)
	{
		if (AData[AIndex] == BData[BIndex])
		{
			++AIndex;
			++BIndex;
			continue;
		}
		if (AData[AIndex] < BData[BIndex])
		{
			AMinusB.Add(AData[AIndex++]);
		}
		else
		{
			++BIndex;
		}
	}

	// When we reach the end of B, all remaining elements of A are not in B.
	while (AIndex < ANum)
	{
		AMinusB.Add(AData[AIndex++]);
	}
	return AMinusB;
}

EPollStatus FPackageData::RefreshObjectCache(bool& bOutFoundNewObjects)
{
	check(Package.Get() != nullptr);

	TArray<UObject*> OldObjects;
	OldObjects.Reserve(CachedObjectsInOuter.Num());
	for (FCachedObjectInOuter& Object : CachedObjectsInOuter)
	{
		UObject* ObjectPtr = Object.Object.Get();
		if (ObjectPtr)
		{
			OldObjects.Add(ObjectPtr);
		}
	}
	TArray<UObject*> CurrentObjects;
	GetObjectsWithOuter(Package.Get(), CurrentObjects, true /* bIncludeNestedObjects */, RF_NoFlags, EInternalObjectFlags::Garbage);

	TArray<UObject*> NewObjects = SetDifference(CurrentObjects, OldObjects);
	bOutFoundNewObjects = NewObjects.Num() > 0;
	if (bOutFoundNewObjects)
	{
		CachedObjectsInOuter.Reserve(CachedObjectsInOuter.Num() + NewObjects.Num());
		for (UObject* Object : NewObjects)
		{
			FWeakObjectPtr ObjectWeakPointer(Object);
			check(ObjectWeakPointer.Get()); // GetObjectsWithOuter with Garbage filtered out should only return valid-for-weakptr objects
			CachedObjectsInOuter.Emplace(MoveTemp(ObjectWeakPointer));
		}
		// GetCookedPlatformDataNextIndex is already where it should be, pointing at the first of the objects we have added
		// Change our state back so we know we need to CallBeginCacheOnObjects again 
		SetCookedPlatformDataCalled(false);

		if (++GetNumRetriesBeginCacheOnObjects() > FPackageData::GetMaxNumRetriesBeginCacheOnObjects())
		{
			UE_LOG(LogCook, Error, TEXT("Cooker has repeatedly tried to call BeginCacheForCookedPlatformData on all objects in the package, but keeps finding new objects.\n")
				TEXT("Aborting the save of the package; programmer needs to debug why objects keep getting added to the package.\n")
				TEXT("Package: %s. Most recent created object: %s."),
				*GetPackageName().ToString(), *NewObjects[0]->GetFullName());
			return EPollStatus::Error;
		}
	}
	return EPollStatus::Success;
}

void FPackageData::ClearObjectCache()
{
	// Note we do not need to remove objects in CachedObjectsInOuter from CachedCookedPlatformDataObjects
	// That removal is handled by ReleaseCookedPlatformData, and the caller is responsible for calling
	// ReleaseCookedPlatformData before calling ClearObjectCache
	CachedObjectsInOuter.Empty();
	for (TPair<const ITargetPlatform*, FPackagePlatformData>& Pair : PlatformDatas)
	{
		Pair.Value.SetRegisteredForCachedObjectsInOuter(false);
	}
	SetHasSaveCache(false);
}

const int32& FPackageData::GetNumPendingCookedPlatformData() const
{
	return NumPendingCookedPlatformData;
}

int32& FPackageData::GetNumPendingCookedPlatformData()
{
	return NumPendingCookedPlatformData;
}

const int32& FPackageData::GetCookedPlatformDataNextIndex() const
{
	return CookedPlatformDataNextIndex;
}

int32& FPackageData::GetCookedPlatformDataNextIndex()
{
	return CookedPlatformDataNextIndex;
}

int32& FPackageData::GetNumRetriesBeginCacheOnObjects()
{
	return NumRetriesBeginCacheOnObject;
}

int32 FPackageData::GetMaxNumRetriesBeginCacheOnObjects()
{
	return 10;
}

void FPackageData::CheckCookedPlatformDataEmpty() const
{
	check(GetCookedPlatformDataNextIndex() <= 0);
	check(!GetCookedPlatformDataStarted());
	check(!GetCookedPlatformDataCalled());
	check(!GetCookedPlatformDataComplete());
	check(!GetGeneratorPackage() ||
		GetGeneratorPackage()->GetOwnerInfo().GetSaveState() <= FCookGenerationInfo::ESaveState::StartPopulate);
	if (GetGeneratedOwner())
	{
		FCookGenerationInfo* Info = GetGeneratedOwner()->FindInfo(*this);
		check(!Info || Info->GetSaveState() <= FCookGenerationInfo::ESaveState::StartPopulate);
	}
}

void FPackageData::ClearCookedPlatformData()
{
	CookedPlatformDataNextIndex = -1;
	NumRetriesBeginCacheOnObject = 0;
	// Note that GetNumPendingCookedPlatformData is not cleared; it persists across Saves and CookSessions
	SetCookedPlatformDataStarted(false);
	SetCookedPlatformDataCalled(false);
	SetCookedPlatformDataComplete(false);
}

void FPackageData::OnRemoveSessionPlatform(const ITargetPlatform* Platform)
{
	PlatformDatas.Remove(Platform);
}

bool FPackageData::HasReferencedObjects() const
{
	return Package != nullptr || CachedObjectsInOuter.Num() > 0;
}

void FPackageData::RemapTargetPlatforms(const TMap<ITargetPlatform*, ITargetPlatform*>& Remap)
{
	typedef TSortedMap<const ITargetPlatform*, FPackagePlatformData, TInlineAllocator<1>> MapType;
	MapType NewPlatformDatas;
	NewPlatformDatas.Reserve(PlatformDatas.Num());
	for (TPair<const ITargetPlatform*, FPackagePlatformData>& ExistingPair : PlatformDatas)
	{
		ITargetPlatform* NewKey = Remap[ExistingPair.Key];
		NewPlatformDatas.FindOrAdd(NewKey) = MoveTemp(ExistingPair.Value);
	}

	// The save state (and maybe more in the future) by contract can depend on the order of the request platforms remaining
	// unchanged. If we change that order due to the remap, we need to demote back to request.
	if (IsInProgress() && GetState() != EPackageState::Request)
	{
		bool bDemote = true;
		MapType::TConstIterator OldIter = PlatformDatas.CreateConstIterator();
		MapType::TConstIterator NewIter = NewPlatformDatas.CreateConstIterator();
		for (; OldIter; ++OldIter, ++NewIter)
		{
			if (OldIter.Key() != NewIter.Key())
			{
				bDemote = true;
			}
		}
		if (bDemote)
		{
			SendToState(EPackageState::Request, ESendFlags::QueueAddAndRemove, EStateChangeReason::ForceRecook);
		}
	}
	PlatformDatas = MoveTemp(NewPlatformDatas);
}

void FPackageData::UpdateSaveAfterGarbageCollect(bool& bOutDemote)
{
	bOutDemote = false;
	if (GetState() != EPackageState::Save)
	{
		return;
	}

	// Reexecute PrepareSave if we already completed it; we need to refresh our CachedObjectsInOuter list
	// and call BeginCacheOnCookedPlatformData on any new objects.
	SetCookedPlatformDataComplete(false);

	if (GetPackage() == nullptr || !GetPackage()->IsFullyLoaded())
	{
		bOutDemote = true;
	}
	else
	{
		for (FCachedObjectInOuter& CachedObjectInOuter : CachedObjectsInOuter)
		{
			if (CachedObjectInOuter.Object.Get() == nullptr)
			{
				// Deleting a public object puts the package in an invalid state; demote back to request
				// and load/save it again
				bool bPublicDeleted = !!(CachedObjectInOuter.ObjectFlags & RF_Public);;
				bOutDemote |= bPublicDeleted;
			}
		}
	}

	if (GeneratorPackage)
	{
		GeneratorPackage->UpdateSaveAfterGarbageCollect(*this, bOutDemote);
	}
	else if (IsGenerated())
	{
		if (!GeneratedOwner)
		{
			bOutDemote = true;
		}
		else
		{
			GeneratedOwner->UpdateSaveAfterGarbageCollect(*this, bOutDemote);
		}
	}
}

void FPackageData::SetGeneratedOwner(FGeneratorPackage* InGeneratedOwner)
{
	check(IsGenerated());
	check(!(GeneratedOwner && InGeneratedOwner));
	GeneratedOwner = InGeneratedOwner;
}

UE::Cook::FGeneratorPackage* FPackageData::GetGeneratorPackage() const
{
	UE::Cook::FGeneratorPackage* Result = GeneratorPackage.Get();
	if (Result && Result->IsInitialized())
	{
		return Result;
	}
	return nullptr;
}

UE::Cook::FGeneratorPackage& FPackageData::CreateGeneratorPackage(const UObject* InSplitDataObject,
	ICookPackageSplitter* InCookPackageSplitterInstance)
{
	if (!GeneratorPackage)
	{
		GeneratorPackage.Reset(new UE::Cook::FGeneratorPackage(*this, InSplitDataObject,
			InCookPackageSplitterInstance));
	}
	else
	{
		GeneratorPackage->InitializeSave(InSplitDataObject, InCookPackageSplitterInstance);
		if (GeneratorPackage->IsInitialized())
		{
			// The earlier exit from SaveState should have reset the progress back to StartGeneratorSave or earlier
			check(GeneratorPackage->GetOwnerInfo().GetSaveState() <= FCookGenerationInfo::ESaveState::StartPopulate);
		}
	}
	FGeneratorPackage* Result = GeneratorPackage.Get();
	check(Result);
	return *Result;
}

FConstructPackageData FPackageData::CreateConstructData()
{
	FConstructPackageData ConstructData;
	ConstructData.PackageName = PackageName;
	ConstructData.NormalizedFileName = FileName;
	return ConstructData;
}

TMap<FPackageData*, EInstigator>& FPackageData::CreateOrGetUnsolicited()
{
	if (!Unsolicited)
	{
		Unsolicited = MakeUnique<TMap<FPackageData*, EInstigator>>();
	}
	return *Unsolicited;
}

TMap<FPackageData*, EInstigator> FPackageData::DetachUnsolicited()
{
	TMap<FPackageData*, EInstigator> Result;
	if (Unsolicited)
	{
		Result = MoveTemp(*Unsolicited);
		Unsolicited.Reset();
	}
	return Result;
}

void FPackageData::ClearUnsolicited()
{
	Unsolicited.Reset();
}

}

FCbWriter& operator<<(FCbWriter& Writer, const UE::Cook::FConstructPackageData& PackageData)
{
	Writer.BeginObject();
	Writer << "P" << PackageData.PackageName;
	Writer << "F" << PackageData.NormalizedFileName;
	Writer.EndObject();
	return Writer;
}

bool LoadFromCompactBinary(FCbFieldView Field, UE::Cook::FConstructPackageData& PackageData)
{
	LoadFromCompactBinary(Field["P"], PackageData.PackageName);
	LoadFromCompactBinary(Field["F"], PackageData.NormalizedFileName);
	return !PackageData.PackageName.IsNone() && !PackageData.NormalizedFileName.IsNone();
}

namespace UE::Cook
{

//////////////////////////////////////////////////////////////////////////
// FGeneratorPackage

FGeneratorPackage::FGeneratorPackage(UE::Cook::FPackageData& InOwner, const UObject* InSplitDataObject,
	ICookPackageSplitter* InCookPackageSplitterInstance)
: OwnerInfo(InOwner, true /* bInGenerated */)
{
	InitializeSave(InSplitDataObject, InCookPackageSplitterInstance);
}

void FGeneratorPackage::InitializeSave(const UObject* InSplitDataObject,
	ICookPackageSplitter* InCookPackageSplitterInstance)
{
	if (InCookPackageSplitterInstance)
	{
		// If we already have a splitter, keep the old and throw out the new. The old one
		// still contains some state.
		if (!CookPackageSplitterInstance)
		{
			CookPackageSplitterInstance.Reset(InCookPackageSplitterInstance);
		}
		else
		{
			delete InCookPackageSplitterInstance;
		}

		bInitialized = true;
		FName InSplitDataObjectName = *InSplitDataObject->GetFullName();
		check(SplitDataObjectName.IsNone() || SplitDataObjectName == InSplitDataObjectName);
		SplitDataObjectName = InSplitDataObjectName;
		bUseInternalReferenceToAvoidGarbageCollect = CookPackageSplitterInstance->UseInternalReferenceToAvoidGarbageCollect();
		SetOwnerPackage(GetOwner().GetPackage());
	}
}

FGeneratorPackage::~FGeneratorPackage()
{
	if (!IsInitialized())
	{
		return;
	}

	ConditionalNotifyCompletion(ICookPackageSplitter::ETeardown::Canceled);
	ClearGeneratedPackages();
}

void FGeneratorPackage::ConditionalNotifyCompletion(ICookPackageSplitter::ETeardown Status)
{
	if (!bNotifiedCompletion)
	{
		bNotifiedCompletion = true;
		CookPackageSplitterInstance->Teardown(Status);
		CookPackageSplitterInstance.Reset();
	}
}

void FGeneratorPackage::ClearGeneratedPackages()
{
	check(IsInitialized());
	for (FCookGenerationInfo& Info: PackagesToGenerate)
	{
		if (Info.PackageData)
		{
			check(Info.PackageData->GetGeneratedOwner() == this);
			Info.PackageData->SetGeneratedOwner(nullptr);
			Info.PackageData = nullptr;
		}
	}
}

bool FGeneratorPackage::TryGenerateList(UObject* OwnerObject, FPackageDatas& PackageDatas)
{
	check(IsInitialized());

	FPackageData& OwnerPackageData = GetOwner();
	UPackage* LocalOwnerPackage = OwnerPackageData.GetPackage();
	check(LocalOwnerPackage);
	UCookOnTheFlyServer& COTFS = GetOwner().GetPackageDatas().GetCookOnTheFlyServer();

	TArray<ICookPackageSplitter::FGeneratedPackage> GeneratorDatas;
	{
		UCookOnTheFlyServer::FScopedActivePackage ScopedActivePackage(COTFS, OwnerPackageData.GetPackageName(),
			PackageAccessTrackingOps::NAME_CookerBuildObject);
		GeneratorDatas = GetCookPackageSplitterInstance()->GetGenerateList(LocalOwnerPackage, OwnerObject);
	}
	PackagesToGenerate.Reset(GeneratorDatas.Num());
	TArray<const ITargetPlatform*, TInlineAllocator<1>> PlatformsToCook;
	OwnerPackageData.GetPlatformsNeedingCooking(PlatformsToCook);
	bool bHybridIterativeEnabled = COTFS.bHybridIterativeEnabled;

	int32 NumIterativeUnmodified = 0;
	int32 NumIterativeModified = 0; 
	int32 NumIterativeRemoved = 0;
	int32 NumIterativePrevious = PreviousGeneratedPackages.Num();

	for (ICookPackageSplitter::FGeneratedPackage& SplitterData : GeneratorDatas)
	{
		if (!SplitterData.GetCreateAsMap().IsSet())
		{
			UE_LOG(LogCook, Error, TEXT("PackageSplitter did not specify whether CreateAsMap is true for generated package. Splitter=%s, Generated=%s."),
				*this->GetSplitDataObjectName().ToString(), *OwnerPackageData.GetPackageName().ToString());
			return false;
		}
		bool bCreateAsMap = *SplitterData.GetCreateAsMap();

		FString PackageRoot = SplitterData.GeneratedRootPath.IsEmpty() ? OwnerPackageData.GetPackageName().ToString() : SplitterData.GeneratedRootPath;

		FString PackageName = FPaths::RemoveDuplicateSlashes(FString::Printf(TEXT("/%s/%s/%s"),
			*PackageRoot, GeneratedPackageSubPath, *SplitterData.RelativePath));
		const FName PackageFName(*PackageName);
		UE::Cook::FPackageData* PackageData = PackageDatas.TryAddPackageDataByPackageName(PackageFName,
			false /* bRequireExists */, bCreateAsMap);
		if (!PackageData)
		{
			UE_LOG(LogCook, Error, TEXT("PackageSplitter could not find mounted filename for generated packagepath. Splitter=%s, Generated=%s."),
				*this->GetSplitDataObjectName().ToString(), *PackageName);
			return false;
		}
		PackageData->SetGenerated(true);
		// No package should be generated by two different splitters. If an earlier run of this splitter generated
		// the package, the package's owner should have been reset to null when we called ClearGeneratedPackages
		// between then and now
		check(PackageData->GetGeneratedOwner() == nullptr);
		if (IFileManager::Get().FileExists(*PackageData->GetFileName().ToString()))
		{
			UE_LOG(LogCook, Warning, TEXT("PackageSplitter specified a generated package that already exists in the workspace domain. Splitter=%s, Generated=%s."),
				*this->GetSplitDataObjectName().ToString(), *PackageName);
			return false;
		}

		FCookGenerationInfo& GeneratedInfo = PackagesToGenerate.Emplace_GetRef(*PackageData, false /* bInGenerator */);
		GeneratedInfo.RelativePath = MoveTemp(SplitterData.RelativePath);
		GeneratedInfo.GeneratedRootPath = MoveTemp(SplitterData.GeneratedRootPath);
		GeneratedInfo.PackageDependencies = MoveTemp(SplitterData.PackageDependencies);
		for (TArray<FAssetDependency>::TIterator Iter(GeneratedInfo.PackageDependencies); Iter; ++Iter)
		{
			if (Iter->Category != UE::AssetRegistry::EDependencyCategory::Package)
			{
				UE_LOG(LogCook, Error,
					TEXT("PackageSplitter specified a dependency with category %d rather than category Package. Dependency will be ignored. Splitter=%s, Generated=%s."),
					(int32)Iter->Category, *this->GetSplitDataObjectName().ToString(), *PackageName);
				Iter.RemoveCurrent();
			}
		}
		Algo::Sort(GeneratedInfo.PackageDependencies,
			[](const FAssetDependency& A, const FAssetDependency& B) { return A.LexicalLess(B); });
		GeneratedInfo.PackageDependencies.SetNum(Algo::Unique(GeneratedInfo.PackageDependencies));
		GeneratedInfo.SetIsCreateAsMap(bCreateAsMap);
		PackageData->SetGeneratedOwner(this);
		PackageData->SetWorkerAssignmentConstraint(FWorkerId::Local());

		// Create the Hash from the GenerationHash and Dependencies
		GeneratedInfo.CreatePackageHash();

		FIoHash PreviousHash;
		if (PreviousGeneratedPackages.RemoveAndCopyValue(PackageFName, PreviousHash) && !bHybridIterativeEnabled)
		{
			bool bIterativelyUnmodified;
			GeneratedInfo.IterativeCookValidateOrClear(*this, PlatformsToCook, PreviousHash, bIterativelyUnmodified);
			++(bIterativelyUnmodified ? NumIterativeUnmodified : NumIterativeModified);
		}
	}
	if (!PreviousGeneratedPackages.IsEmpty())
	{
		NumIterativeRemoved = PreviousGeneratedPackages.Num();
		for (TPair<FName, FIoHash>& Pair : PreviousGeneratedPackages)
		{
			for (const ITargetPlatform* TargetPlatform : PlatformsToCook)
			{
				COTFS.DeleteOutputForPackage(Pair.Key, TargetPlatform);
			}
		}
		PreviousGeneratedPackages.Empty();
	}
	if (NumIterativePrevious > 0 && !bHybridIterativeEnabled)
	{
		UE_LOG(LogCook, Display, TEXT("Found %d cooked package(s) in package store for generator package %s."),
			NumIterativePrevious, *WriteToString<256>(GetOwner().GetPackageName()));
		UE_LOG(LogCook, Display, TEXT("Keeping %d. Recooking %d. Removing %d."),
			NumIterativeUnmodified, NumIterativeModified, NumIterativeRemoved);
	}

	RemainingToPopulate = GeneratorDatas.Num() + 1; // GeneratedPackaged plus one for the Generator
	return true;
}

void FGeneratorPackage::FetchExternalActorDependencies()
{
	check(IsInitialized());

	// The Generator package declares all its ExternalActor dependencies in its AssetRegistry dependencies
	// The Generator's generated packages can also include ExternalActors from other maps due to level instancing,
	// these are included in the dependencies reported by the Generator for each GeneratedPackage in the data
	// returned from GetGenerateList. These sets will overlap; take the union.
	ExternalActorDependencies.Reset();
	IAssetRegistry::GetChecked().GetDependencies(GetOwner().GetPackageName(), ExternalActorDependencies,
		UE::AssetRegistry::EDependencyCategory::Package);
	for (const FCookGenerationInfo& Info : PackagesToGenerate)
	{
		ExternalActorDependencies.Reserve(Info.GetDependencies().Num() + ExternalActorDependencies.Num());
		for (const FAssetDependency& Dependency : Info.GetDependencies())
		{
			ExternalActorDependencies.Add(Dependency.AssetId.PackageName);
		}
	}
	Algo::Sort(ExternalActorDependencies, FNameFastLess());
	ExternalActorDependencies.SetNum(Algo::Unique(ExternalActorDependencies));
	FPackageDatas& PackageDatas = this->GetOwner().GetPackageDatas();
	FThreadSafeSet<FName>& NeverCookPackageList =
		GetOwner().GetPackageDatas().GetCookOnTheFlyServer().PackageTracker->NeverCookPackageList;

	// Keep only on-disk PackageDatas that are marked as NeverCook
	ExternalActorDependencies.RemoveAll([&PackageDatas, &NeverCookPackageList](FName PackageName)
		{
			FPackageData* PackageData = PackageDatas.TryAddPackageDataByPackageName(PackageName);
			if (!PackageData)
			{
				return true;
			}
			bool bIsNeverCook = NeverCookPackageList.Contains(PackageData->GetFileName());
			return !bIsNeverCook;
		});
	ExternalActorDependencies.Shrink();
}

FCookGenerationInfo* FGeneratorPackage::FindInfo(const FPackageData& PackageData)
{
	check(IsInitialized());

	if (&PackageData == &GetOwner())
	{
		return &OwnerInfo;
	}
	for (FCookGenerationInfo& Info : PackagesToGenerate)
	{
		if (Info.PackageData == &PackageData)
		{
			return &Info;
		}
	}
	return nullptr;
}
const FCookGenerationInfo* FGeneratorPackage::FindInfo(const FPackageData& PackageData) const
{
	return const_cast<FGeneratorPackage*>(this)->FindInfo(PackageData);
}

ICookPackageSplitter* FGeneratorPackage::GetCookPackageSplitterInstance() const
{
	checkf(!bNotifiedCompletion, TEXT("It is illegal for the cooker to try to access the CookPackageSplitterInstance after calling Teardown on it."));
	return CookPackageSplitterInstance.Get();
}

UObject* FGeneratorPackage::FindSplitDataObject() const
{
	check(IsInitialized());
	FString ObjectPath = GetSplitDataObjectName().ToString();

	// SplitDataObjectName is a FullObjectPath; strip off the leading <ClassName> in
	// "<ClassName> <Package>.<Object>:<SubObject>"
	int32 ClassDelimiterIndex = -1;
	if (ObjectPath.FindChar(' ', ClassDelimiterIndex))
	{
		ObjectPath.RightChopInline(ClassDelimiterIndex + 1);
	}
	return FindObject<UObject>(nullptr, *ObjectPath);
}

void FGeneratorPackage::PreGarbageCollect(FCookGenerationInfo& Info, TArray<TObjectPtr<UObject>>& GCKeepObjects,
	TArray<UPackage*>& GCKeepPackages, TArray<FPackageData*>& GCKeepPackageDatas, bool& bOutShouldDemote)
{
	if (!IsInitialized())
	{
		return;
	}

	bOutShouldDemote = false;
	check(Info.PackageData); // Caller validates this is non-null
	if (Info.GetSaveState() > FCookGenerationInfo::ESaveState::CallPopulate)
	{
		if (IsUseInternalReferenceToAvoidGarbageCollect() || Info.PackageData->GetIsCookLast())
		{
			UPackage* Package = Info.PackageData->GetPackage();
			if (Package)
			{
				GCKeepPackages.Add(Package);
				GCKeepPackageDatas.Add(Info.PackageData);
			}
		}
		else
		{
			bOutShouldDemote = true;
		}
	}
	if (Info.HasTakenOverCachedCookedPlatformData())
	{
		if (IsUseInternalReferenceToAvoidGarbageCollect())
		{
			// For the UseInternalReferenceToAvoidGarbageCollect case, part of the CookPackageSplitter contract is that
			// the Cooker will keep referenced the package and all objects returned from GetObjectsToMove* functions
			// until the PostSave function is called
			UPackage* Package = Info.PackageData->GetPackage();
			if (Package)
			{
				GCKeepPackages.Add(Package);
				GCKeepPackageDatas.Add(Info.PackageData);
			}
			GCKeepPackages.Append(Info.KeepReferencedPackages);
			for (FCachedObjectInOuter& CachedObjectInOuter : Info.PackageData->GetCachedObjectsInOuter())
			{
				UObject* Object = CachedObjectInOuter.Object.Get();
				if (Object)
				{
					GCKeepObjects.Add(Object);
				}
			}
		}
	}
}

void FGeneratorPackage::PostGarbageCollect()
{
	if (!IsInitialized())
	{
		return;
	}

	FPackageData& Owner = GetOwner();
	if (Owner.GetState() == EPackageState::Save)
	{
		// UCookOnTheFlyServer::PreGarbageCollect adds references for the Generator package and all its public
		// objects, so it should still be loaded
		if (!Owner.GetPackage() || !FindSplitDataObject())
		{
			UE_LOG(LogCook, Error, TEXT("PackageSplitter object was deleted by garbage collection while generation was still ongoing. This will break the generation.")
				TEXT("\n\tSplitter=%s."), *GetSplitDataObjectName().ToString());
		}
	}
	else
	{
		// After the Generator Package is saved, we drop our references to it and it can be garbage collected
		// If we have any packages left to populate, our splitter contract requires that it be garbage collected
		// because we promise that the package is not partially GC'd during calls to TryPopulateGeneratedPackage
		// The splitter can opt-out of this contract and keep it referenced itself if it desires.
		UPackage* LocalOwnerPackage = FindObject<UPackage>(nullptr, *Owner.GetPackageName().ToString());
		if (LocalOwnerPackage)
		{
			if (RemainingToPopulate > 0 &&
				!Owner.IsKeepReferencedDuringGC() &&
				!IsUseInternalReferenceToAvoidGarbageCollect())
			{
				UE_LOG(LogCook, Error, TEXT("PackageSplitter found the Generator package still in memory after it should have been deleted by GC.")
					TEXT("\n\tThis is unexpected since garbage has been collected and the package should have been unreferenced so it should have been collected, and will break population of Generated packages.")
					TEXT("\n\tSplitter=%s"), *GetSplitDataObjectName().ToString());
				EReferenceChainSearchMode SearchMode = EReferenceChainSearchMode::Shortest
					| EReferenceChainSearchMode::PrintAllResults
					| EReferenceChainSearchMode::FullChain;
				FReferenceChainSearch RefChainSearch(LocalOwnerPackage, SearchMode);
			}
		}
	}

	bool bHasIssuedWarning = false;
	for (FCookGenerationInfo& Info : PackagesToGenerate)
	{
		if (FindObject<UPackage>(nullptr, *Info.PackageData->GetPackageName().ToString()))
		{
			if (!Info.PackageData->IsKeepReferencedDuringGC() && !Info.HasSaved() && !bHasIssuedWarning)
			{
				UE_LOG(LogCook, Warning, TEXT("PackageSplitter found a package it generated that was not removed from memory during garbage collection. This will cause errors later during population.")
					TEXT("\n\tSplitter=%s, Generated=%s."), *GetSplitDataObjectName().ToString(), *Info.PackageData->GetPackageName().ToString());
				
				{
					// Compute UCookOnTheFlyServer's references so they are gathered by OBJ REFS below 
					UCookOnTheFlyServer::FScopeFindCookReferences(Owner.GetPackageDatas().GetCookOnTheFlyServer());

					StaticExec(nullptr, *FString::Printf(TEXT("OBJ REFS NAME=%s"), *Info.PackageData->GetPackageName().ToString()));
				}
				
				bHasIssuedWarning = true; // Only issue the warning once per GC
			}
		}
		else
		{
			Info.SetHasCreatedPackage(false);
		}
	}
}

UPackage* FGeneratorPackage::CreateGeneratedUPackage(FCookGenerationInfo& GeneratedInfo,
	const UPackage* InOwnerPackage, const TCHAR* GeneratedPackageName)
{
	check(IsInitialized());
#if ENABLE_COOK_STATS
	++DetailedCookStats::NumRequestedLoads;
#endif
	UPackage* GeneratedPackage = CreatePackage(GeneratedPackageName);
	GeneratedPackage->SetSavedHash(GeneratedInfo.PackageHash);
	GeneratedPackage->SetPersistentGuid(InOwnerPackage->GetPersistentGuid());
	GeneratedPackage->SetPackageFlags(PKG_CookGenerated);
	GeneratedInfo.SetHasCreatedPackage(true);
	if (!InOwnerPackage->IsLoadedByEditorPropertiesOnly())
	{
		GeneratedPackage->SetLoadedByEditorPropertiesOnly(false);
	}

	return GeneratedPackage;
}

void FGeneratorPackage::SetPackageSaved(FCookGenerationInfo& Info, FPackageData& PackageData)
{
	check(IsInitialized());
	if (Info.HasSaved())
	{
		return;
	}
	Info.SetHasSaved(true);
	--RemainingToPopulate;
	check(RemainingToPopulate >= 0);
	if (IsComplete())
	{
		ConditionalNotifyCompletion(ICookPackageSplitter::ETeardown::Complete);
	}
}

bool FGeneratorPackage::IsComplete() const
{
	check(IsInitialized());
	return RemainingToPopulate == 0;
}

void FGeneratorPackage::ResetSaveState(FCookGenerationInfo& Info, UPackage* Package, EStateChangeReason ReleaseSaveReason)
{
	check(IsInitialized());
	if (Info.GetSaveState() > FCookGenerationInfo::ESaveState::CallPopulate)
	{
		UObject* SplitObject = FindSplitDataObject();
		if (!SplitObject || !Package)
		{
			UE_LOG(LogCook, Warning, TEXT("PackageSplitter: %s on %s was GarbageCollected before we finished saving it. This prevents us from calling PostSave and may corrupt other packages that it altered during Populate. Splitter=%s."),
				(!Package ? TEXT("UPackage") : TEXT("SplitDataObject")),
				Info.PackageData ? *Info.PackageData->GetPackageName().ToString() : *Info.RelativePath,
				*GetSplitDataObjectName().ToString());
		}
		else
		{
			UCookOnTheFlyServer& COTFS = Info.PackageData->GetPackageDatas().GetCookOnTheFlyServer();
			UCookOnTheFlyServer::FScopedActivePackage ScopedActivePackage(COTFS, GetOwner().GetPackageName(),
				PackageAccessTrackingOps::NAME_CookerBuildObject);
			if (Info.IsGenerator())
			{
				GetCookPackageSplitterInstance()->PostSaveGeneratorPackage(Package, SplitObject);
			}
			else
			{
				ICookPackageSplitter::FGeneratedPackageForPopulate PopulateInfo;
				PopulateInfo.RelativePath = Info.RelativePath;
				PopulateInfo.GeneratedRootPath = Info.GeneratedRootPath;
				PopulateInfo.bCreatedAsMap = Info.IsCreateAsMap();
				PopulateInfo.Package = Package;
				GetCookPackageSplitterInstance()->PostSaveGeneratedPackage(GetOwnerPackage(), SplitObject, PopulateInfo);
			}
		}
	}

	if (Info.IsGenerator())
	{
		if (ReleaseSaveReason == EStateChangeReason::RecreateObjectCache ||
			ReleaseSaveReason == EStateChangeReason::DoneForNow)
		{
			if (Info.GetSaveState() >= FCookGenerationInfo::ESaveState::StartPopulate)
			{
				Info.SetSaveState(FCookGenerationInfo::ESaveState::StartPopulate);
			}
			else
			{
				// Redo all the steps since we didn't make it to the FinishCachePreObjectsToMove.
				// Restarting in the middle of that flow is not robust
				Info.SetSaveState(FCookGenerationInfo::ESaveState::StartGenerate);
			}
		}
		else
		{
			Info.SetSaveState(FCookGenerationInfo::ESaveState::StartGenerate);
		}
	}
	else
	{
		Info.SetSaveState(FCookGenerationInfo::ESaveState::StartPopulate);
	}
	if (Info.HasTakenOverCachedCookedPlatformData())
	{
		if (Info.PackageData && Info.PackageData->GetCachedObjectsInOuter().Num() != 0 &&
			IsUseInternalReferenceToAvoidGarbageCollect() &&
			(ReleaseSaveReason != EStateChangeReason::Completed && ReleaseSaveReason != EStateChangeReason::DoneForNow &&
			 ReleaseSaveReason != EStateChangeReason::SaveError && ReleaseSaveReason != EStateChangeReason::CookerShutdown))
		{
			UE_LOG(LogCook, Error, TEXT("CookPackageSplitter failure: We are demoting a %s package from save and removing our references that keep its objects loaded.\n")
				TEXT("This will allow the objects to be garbage collected and cause failures in the splitter which expects them to remain loaded.\n")
				TEXT("Package=%s, Splitter=%s, ReleaseSaveReason=%s"),
				Info.IsGenerator() ? TEXT("generator") : TEXT("generated"),
				Info.PackageData ? *Info.PackageData->GetPackageName().ToString() : *Info.RelativePath,
				*GetSplitDataObjectName().ToString(), LexToString(ReleaseSaveReason));
		}
		Info.SetHasTakenOverCachedCookedPlatformData(false);
	}
	Info.SetHasIssuedUndeclaredMovedObjectsWarning(false);
	Info.KeepReferencedPackages.Reset();
}

void FGeneratorPackage::UpdateSaveAfterGarbageCollect(const FPackageData& PackageData, bool& bInOutDemote)
{
	if (!IsInitialized())
	{
		return;
	}
	FCookGenerationInfo* Info = FindInfo(PackageData);
	if (!Info)
	{
		bInOutDemote = true;
		return;
	}

	if (!Info->IsGenerator())
	{
		UPackage* LocalPackage = OwnerPackage.Get();
		if (!LocalPackage || !LocalPackage->IsFullyLoaded())
		{
			bInOutDemote = true;
			return;
		}
	}

	if (bInOutDemote && IsUseInternalReferenceToAvoidGarbageCollect() && Info->HasTakenOverCachedCookedPlatformData())
	{
		// No public objects should have been deleted; we are supposed to keep them referenced by keeping the package
		// referenced in UCookOnTheFlyServer::PreGarbageCollect, and the package keeping its public objects referenced
		// by UPackage::AddReferencedObjects. Since no public objects were deleted, our caller should not have
		// set bInOutDemote=true.
		// Allowing demotion after the splitter has started moving objects breaks our contract with the splitter
		// and can cause a crash. So log this as an error.
		// For better feedback, look in our extra data to identify the name of the public UObject that was deleted.
		FString DeletedObject;
		if (!PackageData.GetPackage())
		{
			DeletedObject = FString::Printf(TEXT("UPackage %s"), *PackageData.GetPackageName().ToString());
		}
		else
		{
			TSet<UObject*> ExistingObjectsAfterSave;
			for (const FCachedObjectInOuter& CachedObjectInOuter : PackageData.GetCachedObjectsInOuter())
			{
				UObject* Ptr = CachedObjectInOuter.Object.Get();
				if (Ptr)
				{
					ExistingObjectsAfterSave.Add(Ptr);
				}
			}

			for (const TPair<UObject*, FCachedObjectInOuterGeneratorInfo>& Pair : Info->CachedObjectsInOuterInfo)
			{
				if (Pair.Value.bPublic && !ExistingObjectsAfterSave.Contains(Pair.Key))
				{
					DeletedObject = Pair.Value.FullName;
					break;
				}
			}
			if (DeletedObject.IsEmpty())
			{
				if (!PackageData.GetPackage()->IsFullyLoaded())
				{
					DeletedObject = FString::Printf(TEXT("UPackage %s is no longer FullyLoaded"), *PackageData.GetPackageName().ToString());
				}
				else
				{
					DeletedObject = TEXT("<Unknown>");
				}
			}
		}
		UE_LOG(LogCook, Error, TEXT("A %s package had some of its UObjects deleted during garbage collection after it started generating. This will cause errors during save of the package.")
			TEXT("\n\tDeleted object: %s")
			TEXT("\n\tSplitter=%s%s"),
			Info->IsGenerator() ? TEXT("Generator") : TEXT("Generated"),
			*DeletedObject,
			*GetSplitDataObjectName().ToString(),
			Info->IsGenerator() ? TEXT(".") : *FString::Printf(TEXT(", Generated=%s."), *Info->PackageData->GetPackageName().ToString()));
	}

	// Remove raw pointers from RootMovedObjects if they no longer exist in the weakpointers in CachedObjectsInOuter
	TSet<UObject*> CachedObjectsInOuterSet;
	for (FCachedObjectInOuter& CachedObjectInOuter : Info->PackageData->GetCachedObjectsInOuter())
	{
		UObject* Object = CachedObjectInOuter.Object.Get();
		if (Object)
		{
			CachedObjectsInOuterSet.Add(Object);
		}
	}
	for (TMap<UObject*, FCachedObjectInOuterGeneratorInfo>::TIterator Iter(Info->CachedObjectsInOuterInfo);
		Iter; ++Iter)
	{
		if (!CachedObjectsInOuterSet.Contains(Iter->Key))
		{
			Iter.RemoveCurrent();
		}
	}
}

FCookGenerationInfo::FCookGenerationInfo(FPackageData& InPackageData, bool bInGenerator)
	: PackageData(&InPackageData)
	, GeneratorSaveState(bInGenerator ? ESaveState::StartGenerate : ESaveState::StartPopulate)
	, bCreateAsMap(false), bHasCreatedPackage(false), bHasSaved(false), bTakenOverCachedCookedPlatformData(false)
	, bIssuedUndeclaredMovedObjectsWarning(false), bGenerator(bInGenerator)
{
}

void FCookGenerationInfo::SetSaveStateComplete(ESaveState CompletedState)
{
	GeneratorSaveState = CompletedState;
	if (GeneratorSaveState < ESaveState::Last)
	{
		GeneratorSaveState = static_cast<ESaveState>(static_cast<uint8>(GeneratorSaveState) + 1);
	}
}

void FCachedObjectInOuterGeneratorInfo::Initialize(UObject* Object)
{
	if (Object)
	{
		FullName = Object->GetFullName();
		bPublic = Object->HasAnyFlags(RF_Public);
	}
	else
	{
		FullName.Empty();
		bPublic = false;
	}

	bInitialized = true;
}

void FCookGenerationInfo::TakeOverCachedObjectsAndAddMoved(FGeneratorPackage& Generator,
	TArray<FCachedObjectInOuter>& CachedObjectsInOuter, TArray<UObject*>& MovedObjects)
{
	CachedObjectsInOuterInfo.Reset();

	for (FCachedObjectInOuter& ObjectInOuter : CachedObjectsInOuter)
	{
		UObject* Object = ObjectInOuter.Object.Get();
		if (Object)
		{
			CachedObjectsInOuterInfo.FindOrAdd(Object).Initialize(Object);
		}
	}

	TArray<UObject*> ChildrenOfMovedObjects;
	for (UObject* Object : MovedObjects)
	{
		if (!IsValid(Object))
		{
			UE_LOG(LogCook, Warning, TEXT("CookPackageSplitter found non-valid object %s returned from %s on Splitter %s%s. Ignoring it."),
				Object ? *Object->GetFullName() : TEXT("<null>"),
				IsGenerator() ? TEXT("PopulateGeneratorPackage") : TEXT("PopulateGeneratedPackage"),
				*Generator.GetSplitDataObjectName().ToString(),
				IsGenerator() ? TEXT("") : *FString::Printf(TEXT(", Package %s"), *PackageData->GetPackageName().ToString()));
			continue;
		}
		FCachedObjectInOuterGeneratorInfo& Info = CachedObjectsInOuterInfo.FindOrAdd(Object);
		if (!Info.bInitialized)
		{
			Info.Initialize(Object);
			Info.bMoved = true;
			Info.bMovedRoot = true;
			CachedObjectsInOuter.Emplace(Object);
			GetObjectsWithOuter(Object, ChildrenOfMovedObjects, true /* bIncludeNestedObjects */, RF_NoFlags, EInternalObjectFlags::Garbage);
		}
	}

	for (UObject* Object : ChildrenOfMovedObjects)
	{
		check(IsValid(Object));
		FCachedObjectInOuterGeneratorInfo& Info = CachedObjectsInOuterInfo.FindOrAdd(Object);
		if (!Info.bInitialized)
		{
			Info.Initialize(Object);
			Info.bMoved = true;
			CachedObjectsInOuter.Emplace(Object);
		}
	}

	SetHasTakenOverCachedCookedPlatformData(true);
}

EPollStatus FCookGenerationInfo::RefreshPackageObjects(FGeneratorPackage& Generator, UPackage* Package,
	bool& bOutFoundNewObjects, ESaveState DemotionState)
{
	bOutFoundNewObjects = false;
	TArray<UObject*> CurrentObjectsInOuter;
	GetObjectsWithOuter(Package, CurrentObjectsInOuter, true /* bIncludeNestedObjects */, RF_NoFlags, EInternalObjectFlags::Garbage);

	check(PackageData); // RefreshPackageObjects is only called when there is a PackageData
	TArray<FCachedObjectInOuter>& CachedObjectsInOuter = PackageData->GetCachedObjectsInOuter();
	UObject* FirstNewObject = nullptr;
	for (UObject* Object : CurrentObjectsInOuter)
	{
		FCachedObjectInOuterGeneratorInfo& Info = CachedObjectsInOuterInfo.FindOrAdd(Object);
		if (!Info.bInitialized)
		{
			Info.Initialize(Object);
			CachedObjectsInOuter.Emplace(Object);
			if (!FirstNewObject)
			{
				FirstNewObject = Object;
			}
		}
	}
	bOutFoundNewObjects = FirstNewObject != nullptr;

	if (FirstNewObject != nullptr && DemotionState != ESaveState::Last)
	{
		SetSaveState(DemotionState);
		if (++PackageData->GetNumRetriesBeginCacheOnObjects() > FPackageData::GetMaxNumRetriesBeginCacheOnObjects())
		{
			UE_LOG(LogCook, Error, TEXT("Cooker has repeatedly tried to call BeginCacheForCookedPlatformData on all objects in a generated package, but keeps finding new objects.\n")
				TEXT("Aborting the save of the package; programmer needs to debug why objects keep getting added to the package.\n")
				TEXT("Splitter: %s%s. Most recent created object: %s."),
				*Generator.GetSplitDataObjectName().ToString(),
				IsGenerator() ? TEXT("") : *FString::Printf(TEXT(", Package: %s"), *PackageData->GetPackageName().ToString()),
				*FirstNewObject->GetFullName());
			return EPollStatus::Error;
		}
	}
	return EPollStatus::Success;
}

void FCookGenerationInfo::CreatePackageHash()
{
	FBlake3 Blake3;
	Blake3.Update(&GenerationHash, sizeof(GenerationHash));
	IAssetRegistry& AssetRegistry = IAssetRegistry::GetChecked();
	for (const FAssetDependency& Dependency : PackageDependencies)
	{
		TOptional<FAssetPackageData> DependencyData = AssetRegistry.GetAssetPackageDataCopy(Dependency.AssetId.PackageName);
		if (DependencyData)
		{
			Blake3.Update(&DependencyData->GetPackageSavedHash().GetBytes(),
				sizeof(decltype(DependencyData->GetPackageSavedHash().GetBytes())));
		}
	}
	PackageHash = FIoHash(Blake3.Finalize());
	// We store the PackageHash as a FIoHash, but UPackage and FAssetPackageData store it as a FGuid, which is smaller,
	// so we have to remove any data which doesn't fit into FGuid. This can be removed when we remove the deprecated
	// Guid storage on UPackage.
	PRAGMA_DISABLE_DEPRECATION_WARNINGS;
	constexpr int SizeDifference = sizeof(PackageHash) - sizeof(decltype(DeclVal<UPackage>().GetGuid()));
	if (SizeDifference > 0)
	{
		FMemory::Memset(((uint8*)&PackageHash.GetBytes()) + (sizeof(decltype(PackageHash.GetBytes())) - SizeDifference),
			0, SizeDifference);
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS;
}

void FCookGenerationInfo::IterativeCookValidateOrClear(FGeneratorPackage& Generator,
	TConstArrayView<const ITargetPlatform*> RequestedPlatforms, const FIoHash& PreviousPackageHash,
	bool& bOutIterativelyUnmodified)
{
	UCookOnTheFlyServer& COTFS = Generator.GetOwner().GetPackageDatas().GetCookOnTheFlyServer();
	bOutIterativelyUnmodified = PreviousPackageHash == this->PackageHash;
	if (bOutIterativelyUnmodified)
	{
		// If not directly modified, mark it as indirectly modified if any of its dependencies
		// were detected as modified during PopulateCookedPackages.
		for (const FAssetDependency& Dependency : this->PackageDependencies)
		{
			FPackageData* DependencyData = COTFS.PackageDatas->FindPackageDataByPackageName(Dependency.AssetId.PackageName);
			if (!DependencyData)
			{
				bOutIterativelyUnmodified = false;
				break;
			}
			for (const ITargetPlatform* TargetPlatform : RequestedPlatforms)
			{
				FPackagePlatformData* DependencyPlatformData = DependencyData->FindPlatformData(TargetPlatform);
				if (!DependencyPlatformData || !DependencyPlatformData->IsIterativelyUnmodified())
				{
					bOutIterativelyUnmodified = false;
					break;
				}
			}
			if (!bOutIterativelyUnmodified)
			{
				break;
			}
		}
	}

	bool bFirstPlatform = true;
	for (const ITargetPlatform* TargetPlatform : RequestedPlatforms)
	{
		if (bOutIterativelyUnmodified)
		{
			PackageData->FindOrAddPlatformData(TargetPlatform).SetIterativelyUnmodified(true);
		}
		bool bShouldIterativelySkip = bOutIterativelyUnmodified;
		ICookedPackageWriter& PackageWriter = COTFS.FindOrCreatePackageWriter(TargetPlatform);
		PackageWriter.UpdatePackageModificationStatus(PackageData->GetPackageName(), bOutIterativelyUnmodified,
			bShouldIterativelySkip);
		if (bShouldIterativelySkip)
		{
			PackageData->SetPlatformCooked(TargetPlatform, ECookResult::Succeeded);
			if (bFirstPlatform)
			{
				COOK_STAT(++DetailedCookStats::NumPackagesIterativelySkipped);
			}
			// Declare the package to the EDLCookInfo verification so we don't warn about missing exports from it
			UE::SavePackageUtilities::EDLCookInfoAddIterativelySkippedPackage(PackageData->GetPackageName());
		}
		else
		{
			COTFS.DeleteOutputForPackage(PackageData->GetPackageName(), TargetPlatform);
		}
		bFirstPlatform = false;
	}
}

//////////////////////////////////////////////////////////////////////////
// FPendingCookedPlatformData


FPendingCookedPlatformData::FPendingCookedPlatformData(UObject* InObject, const ITargetPlatform* InTargetPlatform,
	FPackageData& InPackageData, bool bInNeedsResourceRelease, UCookOnTheFlyServer& InCookOnTheFlyServer)
	: Object(InObject), TargetPlatform(InTargetPlatform), PackageData(InPackageData)
	, CookOnTheFlyServer(InCookOnTheFlyServer),	CancelManager(nullptr), ClassName(InObject->GetClass()->GetFName())
	, bHasReleased(false), bNeedsResourceRelease(bInNeedsResourceRelease)
{
	check(InObject);
	PackageData.GetNumPendingCookedPlatformData() += 1;
}

FPendingCookedPlatformData::FPendingCookedPlatformData(FPendingCookedPlatformData&& Other)
	: Object(Other.Object), TargetPlatform(Other.TargetPlatform), PackageData(Other.PackageData)
	, CookOnTheFlyServer(Other.CookOnTheFlyServer), CancelManager(Other.CancelManager), ClassName(Other.ClassName)
	, UpdatePeriodMultiplier(Other.UpdatePeriodMultiplier), bHasReleased(Other.bHasReleased)
	, bNeedsResourceRelease(Other.bNeedsResourceRelease)
{
	Other.Object = nullptr;
	Other.bHasReleased = true;
}

FPendingCookedPlatformData::~FPendingCookedPlatformData()
{
	Release();
}

bool FPendingCookedPlatformData::PollIsComplete()
{
	if (bHasReleased)
	{
		return true;
	}

	UObject* LocalObject = Object.Get();
	if (!LocalObject)
	{
		Release();
		return true;
	}
	UCookOnTheFlyServer& COTFS = PackageData.GetPackageDatas().GetCookOnTheFlyServer();
	if (COTFS.RouteIsCachedCookedPlatformDataLoaded(PackageData, LocalObject, TargetPlatform, nullptr /* ExistingEvent */))
	{
		Release();
		return true;
	}
	else
	{
#if DEBUG_COOKONTHEFLY
		UE_LOG(LogCook, Display, TEXT("Object %s isn't cached yet"), *LocalObject->GetFullName());
#endif
		/*if ( LocalObject->IsA(UMaterial::StaticClass()) )
		{
			if (GShaderCompilingManager->HasShaderJobs() == false)
			{
				UE_LOG(LogCook, Warning, TEXT("Shader compiler is in a bad state!  Shader %s is finished compile but shader compiling manager did not notify shader.  "),
					*LocalObject->GetPathName());
			}
		}*/
		return false;
	}
}

void FPendingCookedPlatformData::Release()
{
	if (bHasReleased)
	{
		return;
	}

	if (bNeedsResourceRelease)
	{
		int32* CurrentAsyncCache = CookOnTheFlyServer.CurrentAsyncCacheForType.Find(ClassName);
		// bNeedsRelease should not have been set if the AsyncCache does not have an entry for the class
		check(CurrentAsyncCache != nullptr);
		*CurrentAsyncCache += 1;
	}

	PackageData.GetNumPendingCookedPlatformData() -= 1;
	check(PackageData.GetNumPendingCookedPlatformData() >= 0);
	if (CancelManager)
	{
		CancelManager->Release(*this);
		CancelManager = nullptr;
	}

	Object = nullptr;
	bHasReleased = true;
}

void FPendingCookedPlatformData::RemapTargetPlatforms(const TMap<ITargetPlatform*, ITargetPlatform*>& Remap)
{
	TargetPlatform = Remap[TargetPlatform];
}

void FPendingCookedPlatformData::ClearCachedCookedPlatformData(UObject* Object, FPackageData& PackageData,
	bool bCompletedSuccesfully)
{
	FPackageDatas& PackageDatas = PackageData.GetPackageDatas();
	UCookOnTheFlyServer& COTFS = PackageDatas.GetCookOnTheFlyServer();
	using FCCPDMapType = TMap<UObject*, FCachedCookedPlatformDataState>;
	FCCPDMapType& CCPDs = PackageDatas.GetCachedCookedPlatformDataObjects();

	uint32 ObjectKeyHash = FCCPDMapType::KeyFuncsType::GetKeyHash(Object);
	FCachedCookedPlatformDataState* CCPDState = CCPDs.FindByHash(ObjectKeyHash, Object);
	if (!CCPDState)
	{
		return;
	}

	CCPDState->ReleaseFrom(&PackageData);
	if (!CCPDState->IsReferenced())
	{
		for (const TPair<const ITargetPlatform*, ECachedCookedPlatformDataEvent>&
			PlatformPair : CCPDState->PlatformStates)
		{
			Object->ClearCachedCookedPlatformData(PlatformPair.Key);
		}

		// ClearAllCachedCookedPlatformData and WillNeverCacheCookedPlatformDataAgain are not used in editor
		if (!COTFS.IsCookingInEditor())
		{
			Object->ClearAllCachedCookedPlatformData();
			if (bCompletedSuccesfully && COTFS.IsDirectorCookByTheBook())
			{
				Object->WillNeverCacheCookedPlatformDataAgain();
			}
		}

		CCPDs.RemoveByHash(ObjectKeyHash, Object);
	}
};


//////////////////////////////////////////////////////////////////////////
// FPendingCookedPlatformDataCancelManager


void FPendingCookedPlatformDataCancelManager::Release(FPendingCookedPlatformData& Data)
{
	--NumPendingPlatforms;
	if (NumPendingPlatforms <= 0)
	{
		check(NumPendingPlatforms == 0);
		UObject* LocalObject = Data.Object.Get();
		if (LocalObject)
		{
			FPendingCookedPlatformData::ClearCachedCookedPlatformData(LocalObject, Data.PackageData,
				false /* bCompletedSuccesfully */);
		}
		delete this;
	}
}


//////////////////////////////////////////////////////////////////////////
// FPackageDataMonitor
FPackageDataMonitor::FPackageDataMonitor()
{
	FMemory::Memset(NumUrgentInState, 0);
	FMemory::Memset(NumCookLastInState, 0);
}

int32 FPackageDataMonitor::GetNumUrgent() const
{
	int32 NumUrgent = 0;
	for (EPackageState State = EPackageState::Min;
		State <= EPackageState::Max;
		State = static_cast<EPackageState>(static_cast<uint32>(State) + 1))
	{
		NumUrgent += NumUrgentInState[static_cast<uint32>(State) - static_cast<uint32>(EPackageState::Min)];
	}
	return NumUrgent;
}

int32 FPackageDataMonitor::GetNumCookLast() const
{
	int32 Num = 0;
	for (EPackageState State = EPackageState::Min;
		State <= EPackageState::Max;
		State = static_cast<EPackageState>(static_cast<uint32>(State) + 1))
	{
		Num += NumCookLastInState[static_cast<uint32>(State) - static_cast<uint32>(EPackageState::Min)];
	}
	return Num;
}

int32 FPackageDataMonitor::GetNumUrgent(EPackageState InState) const
{
	check(EPackageState::Min <= InState && InState <= EPackageState::Max);
	return NumUrgentInState[static_cast<uint32>(InState) - static_cast<uint32>(EPackageState::Min)];
}

int32 FPackageDataMonitor::GetNumCookLast(EPackageState InState) const
{
	check(EPackageState::Min <= InState && InState <= EPackageState::Max);
	return NumUrgentInState[static_cast<uint32>(InState) - static_cast<uint32>(EPackageState::Min)];
}

int32 FPackageDataMonitor::GetNumPreloadAllocated() const
{
	return NumPreloadAllocated;
}

int32 FPackageDataMonitor::GetNumInProgress() const
{
	return NumInProgress;
}

int32 FPackageDataMonitor::GetNumCooked(ECookResult Result) const
{
	return NumCooked[(uint8)Result];
}

void FPackageDataMonitor::OnInProgressChanged(FPackageData& PackageData, bool bInProgress)
{
	NumInProgress += bInProgress ? 1 : -1;
	check(NumInProgress >= 0);
}

void FPackageDataMonitor::OnPreloadAllocatedChanged(FPackageData& PackageData, bool bPreloadAllocated)
{
	NumPreloadAllocated += bPreloadAllocated ? 1 : -1;
	check(NumPreloadAllocated >= 0);
}

void FPackageDataMonitor::OnFirstCookedPlatformAdded(FPackageData& PackageData, ECookResult CookResult)
{
	check(CookResult != ECookResult::NotAttempted);
	if (PackageData.GetMonitorCookResult() == ECookResult::NotAttempted)
	{
		PackageData.SetMonitorCookResult(CookResult);
		++NumCooked[(uint8)CookResult];
	}
}

void FPackageDataMonitor::OnLastCookedPlatformRemoved(FPackageData& PackageData)
{
	ECookResult CookResult = PackageData.GetMonitorCookResult();
	if (CookResult != ECookResult::NotAttempted)
	{
		--NumCooked[(uint8)CookResult];
		PackageData.SetMonitorCookResult(ECookResult::NotAttempted);
	}
}

void FPackageDataMonitor::OnUrgencyChanged(FPackageData& PackageData)
{
	int32 Delta = PackageData.GetIsUrgent() ? 1 : -1;
	TrackUrgentRequests(PackageData.GetState(), Delta);
}

void FPackageDataMonitor::OnCookLastChanged(FPackageData& PackageData)
{
	int32 Delta = PackageData.GetIsCookLast() ? 1 : -1;
	TrackCookLastRequests(PackageData.GetState(), Delta);
}

void FPackageDataMonitor::OnStateChanged(FPackageData& PackageData, EPackageState OldState)
{
	EPackageState NewState = PackageData.GetState();
	if (PackageData.GetIsUrgent())
	{
		TrackUrgentRequests(OldState, -1);
		TrackUrgentRequests(NewState, 1);
	}
	if (PackageData.GetIsCookLast())
	{
		TrackCookLastRequests(OldState, -1);
		TrackCookLastRequests(NewState, 1);
	}
	bool bOldStateAssignedToLocal = OldState != EPackageState::Idle && OldState != EPackageState::AssignedToWorker;
	bool bNewStateAssignedToLocal = NewState != EPackageState::Idle && NewState != EPackageState::AssignedToWorker;
	if (bOldStateAssignedToLocal != bNewStateAssignedToLocal)
	{
		++(bNewStateAssignedToLocal ? MPCookAssignedFenceMarker : MPCookRetiredFenceMarker);
	}
}

void FPackageDataMonitor::TrackUrgentRequests(EPackageState State, int32 Delta)
{
	check(EPackageState::Min <= State && State <= EPackageState::Max);
	NumUrgentInState[static_cast<uint32>(State) - static_cast<uint32>(EPackageState::Min)] += Delta;
	check(NumUrgentInState[static_cast<uint32>(State) - static_cast<uint32>(EPackageState::Min)] >= 0);
}

void FPackageDataMonitor::TrackCookLastRequests(EPackageState State, int32 Delta)
{
	check(EPackageState::Min <= State && State <= EPackageState::Max);
	if (State != EPackageState::Idle)
	{
		NumCookLastInState[static_cast<uint32>(State) - static_cast<uint32>(EPackageState::Min)] += Delta;
		check(NumCookLastInState[static_cast<uint32>(State) - static_cast<uint32>(EPackageState::Min)] >= 0);
	}
}

int32 FPackageDataMonitor::GetMPCookAssignedFenceMarker() const
{
	return MPCookAssignedFenceMarker;
}

int32 FPackageDataMonitor::GetMPCookRetiredFenceMarker() const
{
	return MPCookRetiredFenceMarker;
}

//////////////////////////////////////////////////////////////////////////
// FPackageDatas

IAssetRegistry* FPackageDatas::AssetRegistry = nullptr;

FPackageDatas::FPackageDatas(UCookOnTheFlyServer& InCookOnTheFlyServer)
	: CookOnTheFlyServer(InCookOnTheFlyServer)
	, LastPollAsyncTime(0)
{
	Allocator.SetMinBlockSize(1024);
	Allocator.SetMaxBlockSize(65536);
}

void FPackageDatas::SetBeginCookConfigSettings(FStringView CookShowInstigator)
{
	ShowInstigatorPackageData = nullptr;
	if (!CookShowInstigator.IsEmpty())
	{
		FString LocalPath;
		FString PackageName;
		if (!FPackageName::TryConvertToMountedPath(CookShowInstigator, &LocalPath, &PackageName, nullptr, nullptr, nullptr))
		{
			UE_LOG(LogCook, Fatal, TEXT("-CookShowInstigator argument %.*s is not a mounted filename or packagename"),
				CookShowInstigator.Len(), CookShowInstigator.GetData());
		}
		else
		{
			FName PackageFName(*PackageName);
			ShowInstigatorPackageData = TryAddPackageDataByPackageName(PackageFName);
			if (!ShowInstigatorPackageData)
			{
				UE_LOG(LogCook, Fatal, TEXT("-CookShowInstigator argument %.*s could not be found on disk"),
					CookShowInstigator.Len(), CookShowInstigator.GetData());
			}
		}
	}
}

FPackageDatas::~FPackageDatas()
{
	Clear();
}

void FPackageDatas::OnAssetRegistryGenerated(IAssetRegistry& InAssetRegistry)
{
	AssetRegistry = &InAssetRegistry;
}

FString FPackageDatas::GetReferencerName() const
{
	return TEXT("CookOnTheFlyServer");
}

void FPackageDatas::AddReferencedObjects(FReferenceCollector& Collector)
{
	return CookOnTheFlyServer.CookerAddReferencedObjects(Collector);
}

FPackageDataMonitor& FPackageDatas::GetMonitor()
{
	return Monitor;
}

UCookOnTheFlyServer& FPackageDatas::GetCookOnTheFlyServer()
{
	return CookOnTheFlyServer;
}

FRequestQueue& FPackageDatas::GetRequestQueue()
{
	return RequestQueue;
}

FPackageDataQueue& FPackageDatas::GetSaveQueue()
{
	return SaveQueue;
}

FPackageData& FPackageDatas::FindOrAddPackageData(const FName& PackageName, const FName& NormalizedFileName)
{
	{
		FReadScopeLock ExistenceReadLock(ExistenceLock);
		FPackageData** PackageDataMapAddr = PackageNameToPackageData.Find(PackageName);
		if (PackageDataMapAddr != nullptr)
		{
			FPackageData** FileNameMapAddr = FileNameToPackageData.Find(NormalizedFileName);
			checkf(FileNameMapAddr, TEXT("Package %s is being added with filename %s, but it already exists with filename %s, ")
				TEXT("and it is not present in FileNameToPackageData map under the new name."),
				*PackageName.ToString(), *NormalizedFileName.ToString(), *(*PackageDataMapAddr)->GetFileName().ToString());
			checkf(*FileNameMapAddr == *PackageDataMapAddr,
				TEXT("Package %s is being added with filename %s, but that filename maps to a different package %s."),
				*PackageName.ToString(), *NormalizedFileName.ToString(), *(*FileNameMapAddr)->GetPackageName().ToString());
			return **PackageDataMapAddr;
		}

		checkf(FileNameToPackageData.Find(NormalizedFileName) == nullptr,
			TEXT("Package \"%s\" and package \"%s\" share the same filename \"%s\"."),
			*PackageName.ToString(), *(*FileNameToPackageData.Find(NormalizedFileName))->GetPackageName().ToString(),
			*NormalizedFileName.ToString());
	}
	return CreatePackageData(PackageName, NormalizedFileName);
}

FPackageData* FPackageDatas::FindPackageDataByPackageName(const FName& PackageName)
{
	if (PackageName.IsNone())
	{
		return nullptr;
	}

	FReadScopeLock ExistenceReadLock(ExistenceLock);
	FPackageData** PackageDataMapAddr = PackageNameToPackageData.Find(PackageName);
	return PackageDataMapAddr ? *PackageDataMapAddr : nullptr;
}

FPackageData* FPackageDatas::TryAddPackageDataByPackageName(const FName& PackageName, bool bRequireExists,
	bool bCreateAsMap)
{
	if (PackageName.IsNone())
	{
		return nullptr;
	}

	{
		FReadScopeLock ExistenceReadLock(ExistenceLock);
		FPackageData** PackageDataMapAddr = PackageNameToPackageData.Find(PackageName);
		if (PackageDataMapAddr != nullptr)
		{
			return *PackageDataMapAddr;
		}
	}

	FName FileName = LookupFileNameOnDisk(PackageName, bRequireExists, bCreateAsMap);
	if (FileName.IsNone())
	{
		// This will happen if PackageName does not exist on disk
		return nullptr;
	}
	{
		FReadScopeLock ExistenceReadLock(ExistenceLock);
		checkf(FileNameToPackageData.Find(FileName) == nullptr,
			TEXT("Package \"%s\" and package \"%s\" share the same filename \"%s\"."),
			*PackageName.ToString(), *(*FileNameToPackageData.Find(FileName))->GetPackageName().ToString(),
			*FileName.ToString());
	}
	return &CreatePackageData(PackageName, FileName);
}

FPackageData& FPackageDatas::AddPackageDataByPackageNameChecked(const FName& PackageName, bool bRequireExists,
	bool bCreateAsMap)
{
	FPackageData* PackageData = TryAddPackageDataByPackageName(PackageName, bRequireExists, bCreateAsMap);
	check(PackageData);
	return *PackageData;
}

FPackageData* FPackageDatas::FindPackageDataByFileName(const FName& InFileName)
{
	FName FileName(GetStandardFileName(InFileName));
	if (FileName.IsNone())
	{
		return nullptr;
	}

	FReadScopeLock ExistenceReadLock(ExistenceLock);
	FPackageData** PackageDataMapAddr = FileNameToPackageData.Find(FileName);
	return PackageDataMapAddr ? *PackageDataMapAddr : nullptr;
}

FPackageData* FPackageDatas::TryAddPackageDataByFileName(const FName& InFileName)
{
	return TryAddPackageDataByStandardFileName(GetStandardFileName(InFileName));
}

FPackageData* FPackageDatas::TryAddPackageDataByStandardFileName(const FName& FileName, bool bExactMatchRequired,
	FName* OutFoundFileName)
{
	FName FoundFileName = FileName;
	ON_SCOPE_EXIT{ if (OutFoundFileName) { *OutFoundFileName = FoundFileName; } };
	if (FileName.IsNone())
	{
		return nullptr;
	}

	{
		FReadScopeLock ExistenceReadLock(ExistenceLock);
		FPackageData** PackageDataMapAddr = FileNameToPackageData.Find(FileName);
		if (PackageDataMapAddr != nullptr)
		{
			return *PackageDataMapAddr;
		}
	}

	FName ExistingFileName;
	FName PackageName = LookupPackageNameOnDisk(FileName, bExactMatchRequired, ExistingFileName);
	if (PackageName.IsNone())
	{
		return nullptr;
	}
	if (ExistingFileName.IsNone())
	{
		if (!bExactMatchRequired)
		{
			FReadScopeLock ExistenceReadLock(ExistenceLock);
			FPackageData** PackageDataMapAddr = PackageNameToPackageData.Find(PackageName);
			if (PackageDataMapAddr != nullptr)
			{
				FoundFileName = (*PackageDataMapAddr)->GetFileName();
				return *PackageDataMapAddr;
			}
		}
		UE_LOG(LogCook, Warning, TEXT("Unexpected failure to cook filename '%s'. It is mapped to PackageName '%s', but does not exist on disk and we cannot verify the extension."),
			*FileName.ToString(), *PackageName.ToString());
		return nullptr;
	}
	FoundFileName = ExistingFileName;
	return &CreatePackageData(PackageName, ExistingFileName);
}

FPackageData& FPackageDatas::CreatePackageData(FName PackageName, FName FileName)
{
	check(!PackageName.IsNone());
	check(!FileName.IsNone());

	FWriteScopeLock ExistenceWriteLock(ExistenceLock);
	FPackageData*& ExistingByPackageName = PackageNameToPackageData.FindOrAdd(PackageName);
	FPackageData*& ExistingByFileName = FileNameToPackageData.FindOrAdd(FileName);
	if (ExistingByPackageName)
	{
		// The other CreatePackageData call should have added the FileName as well
		check(ExistingByFileName == ExistingByPackageName);
		return *ExistingByPackageName;
	}
	// If no other CreatePackageData added the PackageName, then they should not have added
	// the FileName either
	check(!ExistingByFileName);
	FPackageData* PackageData = Allocator.NewElement(*this, PackageName, FileName);
	ExistingByPackageName = PackageData;
	ExistingByFileName = PackageData;
	return *PackageData;
}

FPackageData& FPackageDatas::AddPackageDataByFileNameChecked(const FName& FileName)
{
	FPackageData* PackageData = TryAddPackageDataByFileName(FileName);
	check(PackageData);
	return *PackageData;
}

FName FPackageDatas::GetFileNameByPackageName(FName PackageName, bool bRequireExists, bool bCreateAsMap)
{
	FPackageData* PackageData = TryAddPackageDataByPackageName(PackageName, bRequireExists, bCreateAsMap);
	return PackageData ? PackageData->GetFileName() : NAME_None;
}

FName FPackageDatas::GetFileNameByFlexName(FName PackageOrFileName, bool bRequireExists, bool bCreateAsMap)
{
	FString Buffer = PackageOrFileName.ToString();
	if (!FPackageName::TryConvertFilenameToLongPackageName(Buffer, Buffer))
	{
		return NAME_None;
	}
	return GetFileNameByPackageName(FName(Buffer), bRequireExists, bCreateAsMap);
}

FName FPackageDatas::LookupFileNameOnDisk(FName PackageName, bool bRequireExists, bool bCreateAsMap)
{
	FString FilenameOnDisk;
	if (TryLookupFileNameOnDisk(PackageName, FilenameOnDisk))
	{
	}
	else if (!bRequireExists)
	{
		FString Extension = bCreateAsMap ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension();
		if (!FPackageName::TryConvertLongPackageNameToFilename(PackageName.ToString(), FilenameOnDisk, Extension))
		{
			return NAME_None;
		}
	}
	else
	{
		return NAME_None;
	}
	FilenameOnDisk = FPaths::ConvertRelativePathToFull(FilenameOnDisk);
	FPaths::MakeStandardFilename(FilenameOnDisk);
	return FName(FilenameOnDisk);
}

bool FPackageDatas::TryLookupFileNameOnDisk(FName PackageName, FString& OutFileName)
{
	FString PackageNameStr = PackageName.ToString();

	// Verse packages are editor-generated in-memory packages which don't have a corresponding 
	// asset file (yet). However, we still want to cook these packages out, producing cooked 
	// asset files for packaged projects.
	if (FPackageName::IsVersePackage(PackageNameStr))
	{
		if (FindPackage(/*Outer =*/nullptr, *PackageNameStr))
		{
			if (!FPackageName::TryConvertLongPackageNameToFilename(PackageNameStr, OutFileName,
				FPackageName::GetAssetPackageExtension()))
			{
				UE_LOG(LogCook, Warning, TEXT("Package %s exists in memory but its PackageRoot is not mounted. It will not be cooked."),
					*PackageNameStr);
				return false;
			}
			return true;
		}
		// else, the cooker could be responding to a NotifyUObjectCreated() event, and the object hasn't
		// been fully constructed yet (missing from the FindObject() list) -- in this case, we've found 
		// that the linker loader is creating a dummy object to fill a referencing import slot, not loading
		// the proper object (which means we want to ignore it).
	}

	if (!AssetRegistry)
	{
		return FPackageName::DoesPackageExist(PackageNameStr, &OutFileName, false /* InAllowTextFormats */);
	}
	else
	{
		FString PackageExtension;
		if (!AssetRegistry->DoesPackageExistOnDisk(PackageName, nullptr, &PackageExtension))
		{
			return false;
		}

		return FPackageName::TryConvertLongPackageNameToFilename(PackageNameStr, OutFileName, PackageExtension);
	}
}

FName FPackageDatas::LookupPackageNameOnDisk(FName NormalizedFileName, bool bExactMatchRequired, FName& FoundFileName)
{
	FoundFileName = NormalizedFileName;
	if (NormalizedFileName.IsNone())
	{
		return NAME_None;
	}
	FString Buffer = NormalizedFileName.ToString();
	if (!FPackageName::TryConvertFilenameToLongPackageName(Buffer, Buffer))
	{
		return NAME_None;
	}
	FName PackageName = FName(*Buffer);

	FName DiscoveredFileName = LookupFileNameOnDisk(PackageName, true /* bRequireExists */, false /* bCreateAsMap */);
	if (DiscoveredFileName == NormalizedFileName || !bExactMatchRequired)
	{
		FoundFileName = DiscoveredFileName;
		return PackageName;
	}
	else
	{
		// Either the file does not exist on disk or NormalizedFileName did not match its format or extension
		return NAME_None;
	}
}

FName FPackageDatas::GetStandardFileName(FName FileName)
{
	FString FileNameString(FileName.ToString());
	FPaths::MakeStandardFilename(FileNameString);
	return FName(FileNameString);
}

FName FPackageDatas::GetStandardFileName(FStringView InFileName)
{
	FString FileName(InFileName);
	FPaths::MakeStandardFilename(FileName);
	return FName(FileName);
}

void FPackageDatas::AddExistingPackageDatasForPlatform(TConstArrayView<FConstructPackageData> ExistingPackages,
	const ITargetPlatform* TargetPlatform, bool bExpectPackageDatasAreNew, int32& OutPackageDataFromBaseGameNum)
{
	int32 NumPackages = ExistingPackages.Num();
	if (NumPackages == 0)
	{
		return;
	}

	// Make the list unique
	TArray<FConstructPackageData> UniqueArray(ExistingPackages);
	Algo::Sort(UniqueArray, [](const FConstructPackageData& A, const FConstructPackageData& B)
		{
			return A.PackageName.FastLess(B.PackageName);
		});
	UniqueArray.SetNum(Algo::Unique(UniqueArray, [](const FConstructPackageData& A, const FConstructPackageData& B)
		{
			return A.PackageName == B.PackageName;
		}));
	ExistingPackages = UniqueArray;

	FWriteScopeLock ExistenceWriteLock(ExistenceLock);
	if (bExpectPackageDatasAreNew)
	{
		Allocator.ReserveDelta(NumPackages);
		FileNameToPackageData.Reserve(FileNameToPackageData.Num() + NumPackages);
		PackageNameToPackageData.Reserve(PackageNameToPackageData.Num() + NumPackages);
	}

	// Create the PackageDatas and mark them as cooked
	for (const FConstructPackageData& ConstructData : ExistingPackages)
	{
		FName PackageName = ConstructData.PackageName;
		FName NormalizedFileName = ConstructData.NormalizedFileName;
		check(!PackageName.IsNone());
		check(!NormalizedFileName.IsNone());

		FPackageData*& PackageData = FileNameToPackageData.FindOrAdd(NormalizedFileName, nullptr);
		if (!PackageData)
		{
			// create the package data (copied from CreatePackageData)
			FPackageData* NewPackageData = Allocator.NewElement(*this, PackageName, NormalizedFileName);
			FPackageData* ExistingByPackageName = PackageNameToPackageData.FindOrAdd(PackageName, NewPackageData);
			// If no other CreatePackageData added the FileName, then they should not have added
			// the PackageName either
			check(ExistingByPackageName == NewPackageData);

			PackageData = NewPackageData;
		}
		PackageData->SetPlatformCooked(TargetPlatform, ECookResult::Succeeded, /*bWasCookedThisSession=*/false);
	}
	OutPackageDataFromBaseGameNum += ExistingPackages.Num();
}

FPackageData* FPackageDatas::UpdateFileName(FName PackageName)
{
	FWriteScopeLock ExistenceWriteLock(ExistenceLock);

	FPackageData** PackageDataAddr = PackageNameToPackageData.Find(PackageName);
	if (!PackageDataAddr)
	{
		FName NewFileName = LookupFileNameOnDisk(PackageName);
		check(NewFileName.IsNone() || !FileNameToPackageData.Find(NewFileName));
		return nullptr;
	}
	FPackageData* PackageData = *PackageDataAddr;
	FName OldFileName = PackageData->GetFileName();
	bool bIsMap = FPackageName::IsMapPackageExtension(*FPaths::GetExtension(OldFileName.ToString()));
	FName NewFileName = LookupFileNameOnDisk(PackageName, false /* bRequireExists */, bIsMap);
	if (OldFileName == NewFileName)
	{
		return PackageData;
	}
	if (NewFileName.IsNone())
	{
		UE_LOG(LogCook, Error, TEXT("Cannot update FileName for package %s because the package is no longer mounted."),
			*PackageName.ToString())
			return PackageData;
	}

	check(!OldFileName.IsNone());
	FPackageData* ExistingByFileName;
	ensure(FileNameToPackageData.RemoveAndCopyValue(OldFileName, ExistingByFileName));
	check(ExistingByFileName == PackageData);

	PackageData->SetFileName(NewFileName);
	FPackageData* AddedByFileName = FileNameToPackageData.FindOrAdd(NewFileName, PackageData);
	check(AddedByFileName == PackageData);

	return PackageData;
}

FThreadsafePackageData::FThreadsafePackageData()
	: bInitialized(false)
	, bHasLoggedDiscoveryWarning(false)
	, bHasLoggedDependencyWarning(false)
{
}

void FPackageDatas::UpdateThreadsafePackageData(const FPackageData& PackageData)
{
	UpdateThreadsafePackageData(PackageData.GetPackageName(),
		[&PackageData](FThreadsafePackageData& ThreadsafeData, bool bNew)
		{
			ThreadsafeData.Instigator = PackageData.GetInstigator();
			FGeneratorPackage* Generator = PackageData.GetGeneratedOwner();
			ThreadsafeData.Generator = Generator ? Generator->GetOwner().GetPackageName() : NAME_None;
		});
}

int32 FPackageDatas::GetNumCooked()
{
	int32 Count = 0;
	for (uint8 CookResult = 0; CookResult < (uint8)ECookResult::Count; ++CookResult)
	{
		Count += Monitor.GetNumCooked((ECookResult)CookResult);
	}
	return Count;
}

int32 FPackageDatas::GetNumCooked(ECookResult CookResult)
{
	return Monitor.GetNumCooked(CookResult);
}

void FPackageDatas::GetCookedPackagesForPlatform(const ITargetPlatform* Platform, TArray<FPackageData*>& SucceededPackages,
	TArray<FPackageData*>& FailedPackages)
{
	LockAndEnumeratePackageDatas(
	[Platform, &SucceededPackages, &FailedPackages](FPackageData* PackageData)
	{
		ECookResult CookResults = PackageData->GetCookResults(Platform);
		if (CookResults != ECookResult::NotAttempted)
		{
			(CookResults == ECookResult::Succeeded ? SucceededPackages : FailedPackages).Add(PackageData);
		}
	});
}

void FPackageDatas::Clear()
{
	FWriteScopeLock ExistenceWriteLock(ExistenceLock);
	PendingCookedPlatformDataLists.Empty(); // These destructors will read/write PackageDatas
	RequestQueue.Empty();
	SaveQueue.Empty();
	PackageNameToPackageData.Empty();
	FileNameToPackageData.Empty();
	CachedCookedPlatformDataObjects.Empty();
	{
		// All references must be cleared before any PackageDatas are destroyed
		EnumeratePackageDatasWithinLock([](FPackageData* PackageData)
		{
			PackageData->ClearReferences();
		});
		EnumeratePackageDatasWithinLock([](FPackageData* PackageData)
		{
			PackageData->~FPackageData();
		});
		Allocator.Empty();
	}

	ShowInstigatorPackageData = nullptr;
}

void FPackageDatas::ClearCookedPlatforms()
{
	LockAndEnumeratePackageDatas([](FPackageData* PackageData)
	{
		PackageData->ResetReachable();
		PackageData->ClearCookResults();
	});
}

void FPackageDatas::ClearCookResultsForPackages(const TSet<FName>& InPackages)
{
	int32 AffectedPackagesCount = 0;
	LockAndEnumeratePackageDatas([InPackages, &AffectedPackagesCount](FPackageData* PackageData)
		{
			const FName& PackageName = PackageData->GetPackageName();
			if (InPackages.Contains(PackageName))
			{
				PackageData->ClearCookResults();
				AffectedPackagesCount++;
			}
		});

	UE_LOG(LogCook, Display, TEXT("Cleared the cook results of %d packages because ClearCookResultsForPackages requested them to be recooked."), AffectedPackagesCount);
}

void FPackageDatas::OnRemoveSessionPlatform(const ITargetPlatform* TargetPlatform)
{
	LockAndEnumeratePackageDatas([TargetPlatform](FPackageData* PackageData)
	{
		PackageData->OnRemoveSessionPlatform(TargetPlatform);
	});
}

constexpr int32 PendingPlatformDataReservationSize = 128;
constexpr int32 PendingPlatformDataMaxUpdatePeriod = 16;

void FPackageDatas::AddPendingCookedPlatformData(FPendingCookedPlatformData&& Data)
{
	if (PendingCookedPlatformDataLists.IsEmpty())
	{
		PendingCookedPlatformDataLists.Emplace();
		PendingCookedPlatformDataLists.Last().Reserve(PendingPlatformDataReservationSize );
	}
	PendingCookedPlatformDataLists.First().Add(MoveTemp(Data));
	++PendingCookedPlatformDataNum;
}

void FPackageDatas::PollPendingCookedPlatformDatas(bool bForce, double& LastCookableObjectTickTime)
{
	if (PendingCookedPlatformDataNum == 0)
	{
		return;
	}

	double CurrentTime = FPlatformTime::Seconds();
	if (!bForce)
	{
		// ProcessAsyncResults and IsCachedCookedPlatformDataLoaded can be expensive to call
		// Cap the frequency at which we call them. We only update the last poll time at completion
		// so that we don't suddenly saturate the game thread by making derived data key strings
		// when the time to do the polls increases to GPollAsyncPeriod.
		if (CurrentTime < LastPollAsyncTime + GPollAsyncPeriod)
		{
			return;
		}
	}
	LastPollAsyncTime = CurrentTime;

	// PendingPlatformDataLists is a rotating list of lists of PendingPlatformDatas
	// The first list contains all of the PendingPlatformDatas that we should poll on this tick
	// The nth list is all of the PendingPlatformDatas that we should poll after N more ticks
	// Each poll period we pull the front list off and all other lists move frontwards by 1.
	// New PendingPlatformDatas are inserted into the first list, to be polled in the next poll period
	// When a PendingPlatformData signals it is not ready after polling, we increase its poll period
	// exponentially - we double it.
	// A poll period of N times the default poll period means we insert it into the Nth list in
	// PendingPlatformDataLists.
	FPendingCookedPlatformDataContainer List = PendingCookedPlatformDataLists.PopFrontValue();
	if (!bForce && List.IsEmpty())
	{
		return;
	}

	if (bForce)
	{
		// When we are forced, because the caller has an urgent package to save, call ProcessAsyncResults
		// with a small timeslice in case we need to process shaders to unblock the package
		constexpr float TimeSlice = 0.01f;
		GShaderCompilingManager->ProcessAsyncResults(TimeSlice, false /* bBlockOnGlobalShaderCompletion */);
	}

	FDelegateHandle EventHandle = FAssetCompilingManager::Get().OnPackageScopeEvent().AddLambda(
		[this](UPackage* Package, bool bEntering)
		{
			if (bEntering)
			{
				CookOnTheFlyServer.SetActivePackage(Package->GetFName(), PackageAccessTrackingOps::NAME_CookerBuildObject);
			}
			else
			{
				CookOnTheFlyServer.ClearActivePackage();
			}
		});
	FAssetCompilingManager::Get().ProcessAsyncTasks(true);
	FAssetCompilingManager::Get().OnPackageScopeEvent().Remove(EventHandle);

	if (LastCookableObjectTickTime + TickCookableObjectsFrameTime <= CurrentTime)
	{
		UE_SCOPED_COOKTIMER(TickCookableObjects);
		FTickableCookObject::TickObjects(static_cast<float>(CurrentTime - LastCookableObjectTickTime), false /* bTickComplete */);
		LastCookableObjectTickTime = CurrentTime;
	}

	if (!bForce)
	{
		for (FPendingCookedPlatformData& Data : List)
		{
			if (Data.PollIsComplete())
			{
				// We are destructing all elements of List after the for loop is done; we leave
				// the completed Data on List to be destructed.
				--PendingCookedPlatformDataNum;
			}
			else
			{
				Data.UpdatePeriodMultiplier = FMath::Clamp(Data.UpdatePeriodMultiplier*2, 1, PendingPlatformDataMaxUpdatePeriod);
				int32 ContainerIndex = Data.UpdatePeriodMultiplier - 1;
				while (PendingCookedPlatformDataLists.Num() <= ContainerIndex)
				{
					PendingCookedPlatformDataLists.Emplace();
					PendingCookedPlatformDataLists.Last().Reserve(PendingPlatformDataReservationSize);
				}
				PendingCookedPlatformDataLists[ContainerIndex].Add(MoveTemp(Data));
			}
		}
	}
	else
	{
		// When called with bForce, we poll all PackageDatas in all lists, and do not update
		// any PollPeriods.
		PendingCookedPlatformDataLists.AddFront(MoveTemp(List));
		for (FPendingCookedPlatformDataContainer& ForceList : PendingCookedPlatformDataLists)
		{
			for (int32 Index = 0; Index < ForceList.Num(); )
			{
				FPendingCookedPlatformData& Data = ForceList[Index];
				if (Data.PollIsComplete())
				{
					ForceList.RemoveAtSwap(Index, 1, EAllowShrinking::No);
					--PendingCookedPlatformDataNum;
				}
				else
				{
					++Index;
				}
			}
		}
	}
}

void FPackageDatas::ClearCancelManager(FPackageData& PackageData)
{
	ForEachPendingCookedPlatformData(
		[&PackageData](FPendingCookedPlatformData& PendingCookedPlatformData)
		{
			if (&PendingCookedPlatformData.PackageData == &PackageData)
			{
				if (!PendingCookedPlatformData.PollIsComplete())
				{
					// Abandon it
					PendingCookedPlatformData.Release();
				}
			}
		});
}

void FPackageDatas::RemapTargetPlatforms(const TMap<ITargetPlatform*, ITargetPlatform*>& Remap)
{
	LockAndEnumeratePackageDatas([&Remap](FPackageData* PackageData)
	{
		PackageData->RemapTargetPlatforms(Remap);
	});
	ForEachPendingCookedPlatformData([&Remap](FPendingCookedPlatformData& CookedPlatformData)
	{
		CookedPlatformData.RemapTargetPlatforms(Remap);
	});
}

void FPackageDatas::DebugInstigator(FPackageData& PackageData)
{
	if (ShowInstigatorPackageData == &PackageData)
	{
		TArray<FInstigator> Chain = CookOnTheFlyServer.GetInstigatorChain(PackageData.GetPackageName());
		TStringBuilder<256> ChainText;
		if (Chain.Num() == 0)
		{
			ChainText << TEXT("<NoInstigator>");
		}
		bool bFirst = true;
		for (FInstigator& Instigator : Chain)
		{
			if (!bFirst) ChainText << TEXT(" <- ");
			ChainText << TEXT("{ ") << Instigator.ToString() << TEXT(" }");
			bFirst = false;
		}
		UE_LOG(LogCook, Display, TEXT("Instigator chain of %s: %s"), *PackageData.GetPackageName().ToString(), ChainText.ToString());
	}
	UpdateThreadsafePackageData(PackageData);
}

void FRequestQueue::Empty()
{
	NormalRequests.Empty();
	UrgentRequests.Empty();
}

bool FRequestQueue::IsEmpty() const
{
	return Num() == 0;
}

uint32 FRequestQueue::Num() const
{
	uint32 Count = RestartedRequests.Num() + ReadyRequestsNum();
	for (const FRequestCluster& RequestCluster : RequestClusters)
	{
		Count += RequestCluster.NumPackageDatas();
	}
	return Count;
}

bool FRequestQueue::Contains(const FPackageData* InPackageData) const
{
	FPackageData* PackageData = const_cast<FPackageData*>(InPackageData);
	if (RestartedRequests.Contains(PackageData) || NormalRequests.Contains(PackageData) ||
		UrgentRequests.Contains(PackageData))
	{
		return true;
	}
	for (const FRequestCluster& RequestCluster : RequestClusters)
	{
		if (RequestCluster.Contains(PackageData))
		{
			return true;
		}
	}
	return false;
}

bool FRequestQueue::DiscoveryQueueContains(FPackageData* PackageData) const
{
	return Algo::FindBy(DiscoveryQueue, PackageData,
		[](const FDiscoveryQueueElement& Element) { return Element.PackageData; }) != nullptr;
}

uint32 FRequestQueue::RemoveRequestExceptFromCluster(FPackageData* PackageData, FRequestCluster* ExceptFromCluster)
{
	uint32 OriginalNum = Num();
	RestartedRequests.Remove(PackageData);
	NormalRequests.Remove(PackageData);
	UrgentRequests.Remove(PackageData);
	for (FRequestCluster& RequestCluster : RequestClusters)
	{
		if (&RequestCluster != ExceptFromCluster)
		{
			RequestCluster.RemovePackageData(PackageData);
		}
	}
	uint32 Result = OriginalNum - Num();
	check(Result == 0 || Result == 1);
	return Result;
}

uint32 FRequestQueue::RemoveRequest(FPackageData* PackageData)
{
	return RemoveRequestExceptFromCluster(PackageData, nullptr);
}

uint32 FRequestQueue::Remove(FPackageData* PackageData)
{
	return RemoveRequest(PackageData);
}

bool FRequestQueue::IsReadyRequestsEmpty() const
{
	return ReadyRequestsNum() == 0;
}

bool FRequestQueue::HasRequestsToExplore() const
{
	return !RequestClusters.IsEmpty() | !RestartedRequests.IsEmpty() | !DiscoveryQueue.IsEmpty();
}

uint32 FRequestQueue::ReadyRequestsNum() const
{
	return UrgentRequests.Num() + NormalRequests.Num();
}

FPackageData* FRequestQueue::PopReadyRequest()
{
	for (auto Iterator = UrgentRequests.CreateIterator(); Iterator; ++Iterator)
	{
		FPackageData* PackageData = *Iterator;
		Iterator.RemoveCurrent();
		return PackageData;
	}
	for (auto Iterator = NormalRequests.CreateIterator(); Iterator; ++Iterator)
	{
		FPackageData* PackageData = *Iterator;
		Iterator.RemoveCurrent();
		return PackageData;
	}
	return nullptr;
}

void FRequestQueue::AddRequest(FPackageData* PackageData, bool bForceUrgent)
{
	RestartedRequests.Add(PackageData);
}

void FRequestQueue::AddReadyRequest(FPackageData* PackageData, bool bForceUrgent)
{
	if (bForceUrgent || PackageData->GetIsUrgent())
	{
		UrgentRequests.Add(PackageData);
	}
	else
	{
		NormalRequests.Add(PackageData);
	}
}

bool FLoadPrepareQueue::IsEmpty()
{
	return Num() == 0;
}

int32 FLoadPrepareQueue::Num() const
{
	return PreloadingQueue.Num() + EntryQueue.Num();
}

FPackageData* FLoadPrepareQueue::PopFront()
{
	if (!PreloadingQueue.IsEmpty())
	{
		return PreloadingQueue.PopFrontValue();
	}
	else
	{
		return EntryQueue.PopFrontValue();
	}
}

void FLoadPrepareQueue::Add(FPackageData* PackageData)
{
	EntryQueue.Add(PackageData);
}

void FLoadPrepareQueue::AddFront(FPackageData* PackageData)
{
	PreloadingQueue.AddFront(PackageData);
}

bool FLoadPrepareQueue::Contains(const FPackageData* PackageData) const
{
	return (Algo::Find(PreloadingQueue, PackageData) != nullptr) ||
		(Algo::Find(EntryQueue, PackageData) != nullptr);
}

uint32 FLoadPrepareQueue::Remove(FPackageData* PackageData)
{
	return PreloadingQueue.Remove(PackageData) + EntryQueue.Remove(PackageData);
}

FPoppedPackageDataScope::FPoppedPackageDataScope(FPackageData& InPackageData)
#if COOK_CHECKSLOW_PACKAGEDATA
	: PackageData(InPackageData)
#endif
{
}

#if COOK_CHECKSLOW_PACKAGEDATA
FPoppedPackageDataScope::~FPoppedPackageDataScope()
{
	PackageData.CheckInContainer();
}
#endif

const TCHAR* LexToString(ECachedCookedPlatformDataEvent Value)
{
	switch (Value)
	{
	case ECachedCookedPlatformDataEvent::None: return TEXT("None");
	case ECachedCookedPlatformDataEvent::BeginCacheForCookedPlatformDataCalled: return TEXT("BeginCacheForCookedPlatformDataCalled");
	case ECachedCookedPlatformDataEvent::IsCachedCookedPlatformDataLoadedCalled: return TEXT("IsCachedCookedPlatformDataLoadedCalled");
	case ECachedCookedPlatformDataEvent::IsCachedCookedPlatformDataLoadedReturnedTrue: return TEXT("IsCachedCookedPlatformDataLoadedReturnedTrue");
	case ECachedCookedPlatformDataEvent::ClearCachedCookedPlatformDataCalled: return TEXT("ClearCachedCookedPlatformDataCalled");
	case ECachedCookedPlatformDataEvent::ClearAllCachedCookedPlatformDataCalled: return TEXT("ClearAllCachedCookedPlatformDataCalled");
	default: return TEXT("Invalid");
	}
}

void FPackageDatas::CachedCookedPlatformDataObjectsPostGarbageCollect(const TSet<UObject*>& SaveQueueObjectsThatStillExist)
{
	for (TMap<UObject*, FCachedCookedPlatformDataState>::TIterator Iter(this->CachedCookedPlatformDataObjects);
		Iter; ++Iter)
	{
		if (!SaveQueueObjectsThatStillExist.Contains(Iter->Key))
		{
			Iter.RemoveCurrent();
		}
	}
}

void FCachedCookedPlatformDataState::AddRefFrom(FPackageData* PackageData)
{
	// Most objects will only be referenced by a single package.
	// The exceptions:
	//   1) Generator Packages that move the object from the generator into a generated
	//   2) Bugs
	// Even in case (1), the number of referencers will be 2.
	// We therefore for now just use a flat array and AddUnique, to minimize memory and performance in the 
	// usual case on only a single referencer.
	PackageDatas.AddUnique(PackageData);
}

void FCachedCookedPlatformDataState::ReleaseFrom(FPackageData* PackageData)
{
	PackageDatas.Remove(PackageData);
}

bool FCachedCookedPlatformDataState::IsReferenced() const
{
	return !PackageDatas.IsEmpty();
}

} // namespace UE::Cook
