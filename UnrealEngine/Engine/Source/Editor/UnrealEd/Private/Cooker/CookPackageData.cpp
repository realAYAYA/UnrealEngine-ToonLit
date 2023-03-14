// Copyright Epic Games, Inc. All Rights Reserved.

#include "CookPackageData.h"

#include "Algo/AnyOf.h"
#include "Algo/Count.h"
#include "Algo/Find.h"
#include "Algo/Sort.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Async/ParallelFor.h"
#include "CompactBinaryTCP.h"
#include "Cooker/CookPlatformManager.h"
#include "Cooker/CookRequestCluster.h"
#include "Cooker/CookWorkerClient.h"
#include "CookOnTheSide/CookOnTheFlyServer.h"
#include "Containers/StringView.h"
#include "EditorDomain/EditorDomain.h"
#include "Engine/Console.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
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
FPackageData::FPlatformData::FPlatformData()
	: bRequested(false), bCookAttempted(false), bCookSucceeded(false), bExplored(false), bSaveTimedOut(false)
{
}

FPackageData::FPackageData(FPackageDatas& PackageDatas, const FName& InPackageName, const FName& InFileName)
	: GeneratedOwner(nullptr), PackageName(InPackageName), FileName(InFileName), PackageDatas(PackageDatas)
	, Instigator(EInstigator::NotYetRequested), bIsUrgent(0)
	, bIsVisited(0), bIsPreloadAttempted(0)
	, bIsPreloaded(0), bHasSaveCache(0), bPrepareSaveFailed(0), bPrepareSaveRequiresGC(0)
	, bCookedPlatformDataStarted(0), bCookedPlatformDataCalled(0), bCookedPlatformDataComplete(0), bMonitorIsCooked(0)
	, bInitializedGeneratorSave(0), bCompletedGeneration(0), bGenerated(0), bKeepReferencedDuringGC(0)
{
	SetState(EPackageState::Idle);
	SendToState(EPackageState::Idle, ESendFlags::QueueAdd);
}

FPackageData::~FPackageData()
{
	// ClearReferences should have been called earlier, but call it here in case it was missed
	ClearReferences();
	// We need to send OnLastCookedPlatformRemoved message to the monitor, so call SetPlatformsNotCooked
	ClearCookProgress();
	// Update the monitor's counters and call exit functions
	SendToState(EPackageState::Idle, ESendFlags::QueueNone);
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

int32 FPackageData::GetNumRequestedPlatforms() const
{
	int32 Result = 0;
	for (const TPair<const ITargetPlatform*, FPlatformData>& Pair : PlatformDatas)
	{
		Result += Pair.Value.bRequested ? 1 : 0;
	}
	return Result;
}

void FPackageData::SetPlatformsRequested(TConstArrayView<const ITargetPlatform*> TargetPlatforms, bool bRequested)
{
	for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
	{
		PlatformDatas.FindOrAdd(TargetPlatform).bRequested = true;
	}
}

void FPackageData::ClearRequestedPlatforms()
{
	for (TPair<const ITargetPlatform*, FPlatformData>& Pair : PlatformDatas)
	{
		Pair.Value.bRequested = false;
	}
}

bool FPackageData::HasAllRequestedPlatforms(const TArrayView<const ITargetPlatform* const>& Platforms) const
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
		const FPlatformData* PlatformData = PlatformDatas.Find(QueryPlatform);
		if (!PlatformData || !PlatformData->bRequested)
		{
			return false;
		}
	}
	return true;
}

bool FPackageData::AreAllRequestedPlatformsCooked(bool bAllowFailedCooks) const
{
	for (const TPair<const ITargetPlatform*, FPlatformData>& Pair : PlatformDatas)
	{
		if (Pair.Value.bRequested && (!Pair.Value.bCookAttempted || (!bAllowFailedCooks && !Pair.Value.bCookSucceeded)))
		{
			return false;
		}
	}
	return true;
}

bool FPackageData::AreAllRequestedPlatformsExplored() const
{
	for (const TPair<const ITargetPlatform*, FPlatformData>& Pair : PlatformDatas)
	{
		if (Pair.Value.bRequested && !Pair.Value.bExplored)
		{
			return false;
		}
	}
	return true;
}

bool FPackageData::HasAllExploredPlatforms(const TArrayView<const ITargetPlatform* const>& Platforms) const
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
		const FPlatformData* PlatformData = FindPlatformData(QueryPlatform);
		if (!PlatformData || !PlatformData->bExplored)
		{
			return false;
		}
	}
	return true;
}

void FPackageData::SetIsUrgent(bool Value)
{
	bool OldValue = static_cast<bool>(bIsUrgent);
	if (OldValue != Value)
	{
		bIsUrgent = Value != 0;
		PackageDatas.GetMonitor().OnUrgencyChanged(*this);
	}
}

void FPackageData::UpdateRequestData(const TConstArrayView<const ITargetPlatform*> InRequestedPlatforms,
	bool bInIsUrgent, FCompletionCallback&& InCompletionCallback, FInstigator&& InInstigator, bool bAllowUpdateUrgency)
{
	check(IsInProgress());
	AddCompletionCallback(MoveTemp(InCompletionCallback));

	bool bUrgencyChanged = false;
	if (bInIsUrgent && !GetIsUrgent())
	{
		bUrgencyChanged = true;
		SetIsUrgent(true);
	}

	if (!HasAllRequestedPlatforms(InRequestedPlatforms))
	{
		// Send back to the Request state (canceling any current operations) and then add the new platforms
		if (GetState() != EPackageState::Request)
		{
			SendToState(EPackageState::Request, ESendFlags::QueueAddAndRemove);
		}
		SetPlatformsRequested(InRequestedPlatforms, true);
	}
	else if (bUrgencyChanged && bAllowUpdateUrgency)
	{
		SendToState(GetState(), ESendFlags::QueueAddAndRemove);
	}
}

void FPackageData::SetRequestData(const TArrayView<const ITargetPlatform* const>& InRequestedPlatforms,
	bool bInIsUrgent, FCompletionCallback&& InCompletionCallback, FInstigator&& InInstigator)
{
	check(!CompletionCallback);
	check(GetNumRequestedPlatforms() == 0)
	check(!bIsUrgent);

	check(InRequestedPlatforms.Num() != 0);
	SetPlatformsRequested(InRequestedPlatforms, true);
	SetIsUrgent(bInIsUrgent);
	AddCompletionCallback(MoveTemp(InCompletionCallback));
	if (Instigator.Category == EInstigator::NotYetRequested)
	{
		OnPackageDataFirstRequested(MoveTemp(InInstigator));
	}
}

void FPackageData::ClearInProgressData()
{
	ClearRequestedPlatforms();
	SetIsUrgent(false);
	CompletionCallback = FCompletionCallback();
}

void FPackageData::SetPlatformsCooked(const TConstArrayView<const ITargetPlatform*> TargetPlatforms,
	const TConstArrayView<bool> Succeeded)
{
	check(TargetPlatforms.Num() == Succeeded.Num());
	for (int32 n = 0; n < TargetPlatforms.Num(); ++n)
	{
		SetPlatformCooked(TargetPlatforms[n], Succeeded[n]);
	}
}

void FPackageData::SetPlatformsCooked(const TConstArrayView<const ITargetPlatform*> TargetPlatforms,
	bool bSucceeded)
{
	for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
	{
		SetPlatformCooked(TargetPlatform, bSucceeded);
	}
}

void FPackageData::SetPlatformCooked(const ITargetPlatform* TargetPlatform, bool bSucceeded)
{
	bool bHasAnyOthers = false;
	bool bModified = false;
	bool bExists = false;
	for (TPair<const ITargetPlatform*, FPlatformData>& Pair : PlatformDatas)
	{
		if (Pair.Key == TargetPlatform)
		{
			bExists = true;
			bModified = bModified | (Pair.Value.bCookAttempted == false);
			Pair.Value.bCookAttempted = true;
			Pair.Value.bCookSucceeded = bSucceeded;
			// Clear the SaveTimedOut when get a cook result, in case we save again later and need to allow retry again
			Pair.Value.bSaveTimedOut = false;
		}
		else
		{
			bHasAnyOthers = bHasAnyOthers | (Pair.Value.bCookAttempted != false);
		}
	}
	if (!bExists)
	{
		FPlatformData& Value = PlatformDatas.FindOrAdd(TargetPlatform);
		Value.bCookAttempted = true;
		Value.bCookSucceeded = bSucceeded;
		Value.bSaveTimedOut = false;
		bModified = true;
	}
	if (bModified && !bHasAnyOthers)
	{
		PackageDatas.GetMonitor().OnFirstCookedPlatformAdded(*this);
	}
}

void FPackageData::ClearCookProgress(const TConstArrayView<const ITargetPlatform*> TargetPlatforms)
{
	for (const ITargetPlatform* TargetPlatform : TargetPlatforms)
	{
		ClearCookProgress(TargetPlatform);
	}
}

void FPackageData::ClearCookProgress()
{
	bool bModifiedCookAttempted = false;
	for (TPair<const ITargetPlatform*, FPlatformData>& Pair : PlatformDatas)
	{
		bModifiedCookAttempted = bModifiedCookAttempted | (Pair.Value.bCookAttempted != false);
		Pair.Value.bCookAttempted = false;
		Pair.Value.bCookSucceeded = false;
		Pair.Value.bExplored = false;
		Pair.Value.bSaveTimedOut = false;
	}
	if (bModifiedCookAttempted)
	{
		PackageDatas.GetMonitor().OnLastCookedPlatformRemoved(*this);
	}
}

void FPackageData::ClearCookProgress(const ITargetPlatform* TargetPlatform)
{
	bool bHasAnyOthers = false;
	bool bModifiedCookAttempted = false;
	for (TPair<const ITargetPlatform*, FPlatformData>& Pair : PlatformDatas)
	{
		if (Pair.Key == TargetPlatform)
		{
			bModifiedCookAttempted = bModifiedCookAttempted | (Pair.Value.bCookAttempted != false);
			Pair.Value.bCookAttempted = false;
			Pair.Value.bCookSucceeded = false;
			Pair.Value.bExplored = false;
			Pair.Value.bSaveTimedOut = false;
		}
		else
		{
			bHasAnyOthers = bHasAnyOthers | (Pair.Value.bCookAttempted != false);
		}
	}
	if (bModifiedCookAttempted && !bHasAnyOthers)
	{
		PackageDatas.GetMonitor().OnLastCookedPlatformRemoved(*this);
	}
}

const TSortedMap<const ITargetPlatform*, FPackageData::FPlatformData>& FPackageData::GetPlatformDatas() const
{
	return PlatformDatas;
}

FPackageData::FPlatformData& FPackageData::FindOrAddPlatformData(const ITargetPlatform* TargetPlatform)
{
	return PlatformDatas.FindOrAdd(TargetPlatform);
}

FPackageData::FPlatformData* FPackageData::FindPlatformData(const ITargetPlatform* TargetPlatform)
{
	return PlatformDatas.Find(TargetPlatform);
}

const FPackageData::FPlatformData* FPackageData::FindPlatformData(const ITargetPlatform* TargetPlatform) const
{
	return PlatformDatas.Find(TargetPlatform);
}

bool FPackageData::HasAnyCookedPlatform() const
{
	return Algo::AnyOf(PlatformDatas,
		[](const TPair<const ITargetPlatform*, FPlatformData>& Pair) { return Pair.Value.bCookAttempted; });
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
	return (Result == ECookResult::Succeeded) | ((Result == ECookResult::Failed) & (bIncludeFailed != 0));
}

ECookResult FPackageData::GetCookResults(const ITargetPlatform* Platform) const
{
	const FPlatformData* PlatformData = PlatformDatas.Find(Platform);
	if (PlatformData && PlatformData->bCookAttempted)
	{
		return PlatformData->bCookSucceeded ? ECookResult::Succeeded : ECookResult::Failed;
	}
	return ECookResult::Unseen;
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

void FPackageData::SendToState(EPackageState NextState, ESendFlags SendFlags)
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
		OnExitSave();
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
	if (PackageDatas.GetLogDiscoveredPackages())
	{
		UE_LOG(LogCook, Warning, TEXT("Missing dependency: Package %s discovered after initial dependency search."), *WriteToString<256>(PackageName));
	}
}

void FPackageData::OnEnterRequest()
{
	// It is not valid to enter the request state without requested platforms; it indicates a bug due to e.g.
	// calling SendToState without UpdateRequestData from Idle
	check(GetNumRequestedPlatforms() > 0);
}

void FPackageData::OnExitRequest()
{
}

void FPackageData::OnEnterAssignedToWorker()
{
}

void FPackageData::OnExitAssignedToWorker()
{
	if (GetWorkerAssignment().IsValid())
	{
		PackageDatas.GetCookOnTheFlyServer().NotifyRemovedFromWorker(*this);
	}
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
	check(!PackageRemoteResult);
}

void FPackageData::OnExitSave()
{
	PackageDatas.GetCookOnTheFlyServer().ReleaseCookedPlatformData(*this, EReleaseSaveReason::Demoted);
	ClearObjectCache();
	SetHasPrepareSaveFailed(false);
	SetIsPrepareSaveRequiresGC(false);
	PackageRemoteResult.Reset();
}

FPackageRemoteResult& FPackageData::GetOrAddPackageRemoteResult()
{
	check(GetState() == EPackageState::Save);
	if (!PackageRemoteResult)
	{
		PackageRemoteResult = MakeUnique<FPackageRemoteResult>();
	}
	return *PackageRemoteResult;
}

TUniquePtr<FPackageRemoteResult>& FPackageData::GetPackageRemoteResult()
{
	return PackageRemoteResult;
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

void FPackageData::OnPackageDataFirstRequested(FInstigator&& InInstigator)
{
	TracePackage(GetPackageName().ToUnstableInt(), GetPackageName().ToString());
	Instigator = MoveTemp(InInstigator);
	PackageDatas.DebugInstigator(*this);
}

void FPackageData::SetState(EPackageState NextState)
{
	State = static_cast<uint32>(NextState);
}

FCompletionCallback& FPackageData::GetCompletionCallback()
{
	return CompletionCallback;
}

void FPackageData::AddCompletionCallback(FCompletionCallback&& InCompletionCallback)
{
	if (InCompletionCallback)
	{
		// We don't yet have a mechanism for calling two completion callbacks.
		// CompletionCallbacks only come from external requests, and it should not be possible to request twice,
		// so a failed check here shouldn't happen.
		check(!CompletionCallback);
		CompletionCallback = MoveTemp(InCompletionCallback);
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
	if (!PreloadableFile.Get())
	{
		if (FEditorDomain* EditorDomain = FEditorDomain::Get())
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
	check(!GetIsPreloadAttempted());
	check(!PreloadableFile.Get());
	check(!GetIsPreloaded());
}

TArray<FWeakObjectPtr>& FPackageData::GetCachedObjectsInOuter()
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
			CachedObjectsInOuter.Emplace(MoveTemp(ObjectWeakPointer));
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
	// Caller will only call this function after CallBeginCacheOnObjects finished successfully
	int32 NumPlatforms = GetNumRequestedPlatforms();
	check(NumPlatforms > 0);
	check(GetCookedPlatformDataNextIndex()/NumPlatforms == GetCachedObjectsInOuter().Num());
	check(Package.Get() != nullptr);

	TArray<UObject*> OldObjects;
	OldObjects.Reserve(CachedObjectsInOuter.Num());
	for (FWeakObjectPtr& Object : CachedObjectsInOuter)
	{
		UObject* ObjectPtr = Object.Get();
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
	CachedObjectsInOuter.Empty();
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
	check(GetCookedPlatformDataNextIndex() == 0);
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
	CookedPlatformDataNextIndex = 0;
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
	typedef TSortedMap<const ITargetPlatform*, FPlatformData> MapType;
	MapType NewPlatformDatas;
	NewPlatformDatas.Reserve(PlatformDatas.Num());
	for (TPair<const ITargetPlatform*, FPlatformData>& ExistingPair : PlatformDatas)
	{
		ITargetPlatform* NewKey = Remap[ExistingPair.Key];
		NewPlatformDatas.FindOrAdd(NewKey) = MoveTemp(ExistingPair.Value);
	}

	// The save state (and maybe more in the future) depend on the order of the request platforms remaining
	// unchanged, due to CookedPlatformDataNextIndex. If we change that order due to the remap, we need to
	// demote back to request.
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
			SendToState(EPackageState::Request, ESendFlags::QueueAddAndRemove);
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

	if (GetPackage() == nullptr || !GetPackage()->IsFullyLoaded() ||
		Algo::AnyOf(CachedObjectsInOuter, [](const FWeakObjectPtr& WeakPtr)
			{
				// TODO: Keep track of which objects were public, and only invalidate the save if the object
				// that has been deleted or marked pending kill was public
				// Until we make that change, we will unnecessarily invalidate and demote some packages after a
				// garbage collect
				return WeakPtr.Get() == nullptr;
			}))
	{
		bOutDemote = true;
		return;
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

UE::Cook::FGeneratorPackage* FPackageData::CreateGeneratorPackage(const UObject* InSplitDataObject,
	ICookPackageSplitter* InCookPackageSplitterInstance)
{
	if (!GetGeneratorPackage())
	{
		GeneratorPackage.Reset(new UE::Cook::FGeneratorPackage(*this, InSplitDataObject,
			InCookPackageSplitterInstance));
	}
	else
	{
		// The earlier exit from SaveState should have reset the progress back to StartGeneratorSave or earlier
		check(GetGeneratorPackage()->GetOwnerInfo().GetSaveState() <= FCookGenerationInfo::ESaveState::StartPopulate);
	}
	return GetGeneratorPackage();
}

FConstructPackageData FPackageData::CreateConstructData()
{
	FConstructPackageData ConstructData;
	ConstructData.PackageName = PackageName;
	ConstructData.NormalizedFileName = FileName;
	return ConstructData;
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
, SplitDataObjectName(*InSplitDataObject->GetFullName())
{
	check(InCookPackageSplitterInstance);
	CookPackageSplitterInstance.Reset(InCookPackageSplitterInstance);
	SetOwnerPackage(InOwner.GetPackage());
}

FGeneratorPackage::~FGeneratorPackage()
{
	ConditionalNotifyCompletion(ICookPackageSplitter::ETeardown::Canceled);
	ClearGeneratedPackages();
}

void FGeneratorPackage::ConditionalNotifyCompletion(ICookPackageSplitter::ETeardown Status)
{
	if (!bNotifiedCompletion)
	{
		bNotifiedCompletion = true;
		CookPackageSplitterInstance->Teardown(Status);
	}
}

void FGeneratorPackage::ClearGeneratedPackages()
{
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
	FPackageData& OwnerPackageData = GetOwner();
	UPackage* LocalOwnerPackage = OwnerPackageData.GetPackage();
	check(LocalOwnerPackage);
	TArray<ICookPackageSplitter::FGeneratedPackage> GeneratorDatas =
		CookPackageSplitterInstance->GetGenerateList(LocalOwnerPackage, OwnerObject);
	PackagesToGenerate.Reset(GeneratorDatas.Num());
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
		GeneratedInfo.Dependencies = MoveTemp(SplitterData.Dependencies);
		GeneratedInfo.SetIsCreateAsMap(bCreateAsMap);
		PackageData->SetGeneratedOwner(this);
		PackageData->SetWorkerAssignmentConstraint(FWorkerId::Local());
	}
	RemainingToPopulate = GeneratorDatas.Num() + 1; // GeneratedPackaged plus one for the Generator
	return true;
}

FCookGenerationInfo* FGeneratorPackage::FindInfo(const FPackageData& PackageData)
{
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

UObject* FGeneratorPackage::FindSplitDataObject() const
{
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

void FGeneratorPackage::PreGarbageCollect(FCookGenerationInfo& Info, TArray<UObject*>& GCKeepObjects,
	TArray<UPackage*>& GCKeepPackages, TArray<FPackageData*>& GCKeepPackageDatas, bool& bOutShouldDemote)
{
	bOutShouldDemote = false;
	check(Info.PackageData); // Caller validates this is non-null
	if (Info.GetSaveState() > FCookGenerationInfo::ESaveState::CallPopulate)
	{
		if (GetCookPackageSplitterInstance()->UseInternalReferenceToAvoidGarbageCollect())
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
	if (Info.GetSaveState() > FCookGenerationInfo::ESaveState::CallObjectsToMove)
	{
		if (GetCookPackageSplitterInstance()->UseInternalReferenceToAvoidGarbageCollect())
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
			for (FBeginCacheObject& BeginCacheObject : Info.BeginCacheObjects.Objects)
			{
				UObject* Object = BeginCacheObject.Object.Get();
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
		// If we have any packages left to populate, our splitter contract requires that it be garbage collected;
		// we promise that the package is not partially GC'd during calls to TryPopulateGeneratedPackage
		// The splitter can opt-out of this contract and keep it referenced itself if it desires.
		UPackage* LocalOwnerPackage = FindObject<UPackage>(nullptr, *Owner.GetPackageName().ToString());
		if (LocalOwnerPackage)
		{
			if (RemainingToPopulate > 0 &&
				!Owner.IsKeepReferencedDuringGC() &&
				!CookPackageSplitterInstance->UseInternalReferenceToAvoidGarbageCollect())
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
	UPackage* GeneratedPackage = CreatePackage(GeneratedPackageName);
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	GeneratedPackage->SetGuid(InOwnerPackage->GetGuid());
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	GeneratedPackage->SetPersistentGuid(InOwnerPackage->GetPersistentGuid());
	GeneratedPackage->SetPackageFlags(PKG_CookGenerated);
	GeneratedInfo.SetHasCreatedPackage(true);
	return GeneratedPackage;
}

void FGeneratorPackage::SetPackageSaved(FCookGenerationInfo& Info, FPackageData& PackageData)
{
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
	return RemainingToPopulate == 0;
}

void FGeneratorPackage::ResetSaveState(FCookGenerationInfo& Info, UPackage* Package, EReleaseSaveReason ReleaseSaveReason)
{
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
		if (ReleaseSaveReason == EReleaseSaveReason::RecreateObjectCache ||
			ReleaseSaveReason == EReleaseSaveReason::Demoted ||
			ReleaseSaveReason == EReleaseSaveReason::DoneForNow)
		{
			if (Info.GetSaveState() >= FCookGenerationInfo::ESaveState::StartPopulate)
			{
				Info.SetSaveState(FCookGenerationInfo::ESaveState::StartPopulate);
			}
			else
			{
				// Redo all the steps since we didn't make it to the FinishCachePreObjectsToMove.
				// Restarting in the middle of that flow after a GarbageCollect is not robust
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
	if (Info.BeginCacheObjects.Objects.Num() != 0 &&
		GetCookPackageSplitterInstance()->UseInternalReferenceToAvoidGarbageCollect() &&
		(ReleaseSaveReason == EReleaseSaveReason::Demoted || ReleaseSaveReason == EReleaseSaveReason::RecreateObjectCache))
	{
		UE_LOG(LogCook, Error, TEXT("CookPackageSplitter failure: We are demoting a generated package from save and removing our references that keep its objects loaded.\n")
			TEXT("This will allow the objects to be garbage collected and cause failures in the splitter which expects them to remain loaded.\n")
			TEXT("Package=%s, Splitter=%s, ReleaseSaveReason=%s"),
			Info.PackageData ? *Info.PackageData->GetPackageName().ToString() : *Info.RelativePath,
			*GetSplitDataObjectName().ToString(), LexToString(ReleaseSaveReason));
	}
	Info.BeginCacheObjects.Reset();
	Info.SetHasTakenOverCachedCookedPlatformData(false);
	Info.SetHasIssuedUndeclaredMovedObjectsWarning(false);
	Info.KeepReferencedPackages.Reset();
}

void FGeneratorPackage::UpdateSaveAfterGarbageCollect(const FPackageData& PackageData, bool& bInOutDemote)
{
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

	for (TArray<FBeginCacheObject>::TIterator Iter(Info->BeginCacheObjects.Objects); Iter; ++Iter)
	{
		const FBeginCacheObject& Object = *Iter;
		if (!Object.Object.IsValid())
		{
			if (GetCookPackageSplitterInstance()->UseInternalReferenceToAvoidGarbageCollect())
			{
				// No objects should be allowed to be deleted; we are supposed to keep them referenced
				// But allowing demotion will break things for sure.
				// Log a cook error but remove the invalidated object.
				UE_LOG(LogCook, Error, TEXT("PackageSplitter found an object returned from %s that was removed from memory during garbage collection. This will cause errors during save of the package.")
					TEXT("\n\tSplitter=%s%s."),
					Info->IsGenerator() ? TEXT("PopulateGeneratorPackage") : TEXT("PopulateGeneratedPackage"),
					*GetSplitDataObjectName().ToString(),
					Info->IsGenerator() ? TEXT("") : *FString::Printf(TEXT(", Generated=%s."), *Info->PackageData->GetPackageName().ToString()));
				Iter.RemoveCurrent();
			}
			else
			{
				bInOutDemote = true;
			}
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

void FCookGenerationInfo::TakeOverCachedObjectsAndAddMoved(FGeneratorPackage& Generator,
	TArray<FWeakObjectPtr>& CachedObjectsInOuter, TArray<UObject*>& MovedObjects)
{
	BeginCacheObjects.Objects.Reset();
	TSet<UObject*> ObjectSet;
	for (FWeakObjectPtr& ObjectInOuter : CachedObjectsInOuter)
	{
		UObject* Object = ObjectInOuter.Get();
		if (Object)
		{
			bool bAlreadyExists;
			ObjectSet.Add(Object, &bAlreadyExists);
			if (!bAlreadyExists)
			{
				FBeginCacheObject& Added = BeginCacheObjects.Objects.Add_GetRef(FBeginCacheObject{ Object });
				Added.bHasFinishedRound = true;
			}
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

		bool bAlreadyExists;
		ObjectSet.Add(Object, &bAlreadyExists);
		if (!bAlreadyExists)
		{
			FBeginCacheObject& Added = BeginCacheObjects.Objects.Add_GetRef(FBeginCacheObject{ Object });
			Added.bHasFinishedRound = false;
			Added.bIsRootMovedObject = true;
			GetObjectsWithOuter(Object, ChildrenOfMovedObjects, true /* bIncludeNestedObjects */, RF_NoFlags, EInternalObjectFlags::Garbage);
		}
	}

	for (UObject* Object : ChildrenOfMovedObjects)
	{
		check(IsValid(Object));
		bool bAlreadyExists;
		ObjectSet.Add(Object, &bAlreadyExists);
		if (!bAlreadyExists)
		{
			FBeginCacheObject& Added = BeginCacheObjects.Objects.Add_GetRef(FBeginCacheObject{ Object });
			Added.bHasFinishedRound = false;
		}
	}

	BeginCacheObjects.StartRound();

	CachedObjectsInOuter.Reset();
	SetHasTakenOverCachedCookedPlatformData(true);
}

EPollStatus FCookGenerationInfo::RefreshPackageObjects(FGeneratorPackage& Generator, UPackage* Package,
	bool& bOutFoundNewObjects, ESaveState DemotionState)
{
	bOutFoundNewObjects = false;
	TArray<UObject*> CurrentObjectsInOuter;
	GetObjectsWithOuter(Package, CurrentObjectsInOuter, true /* bIncludeNestedObjects */, RF_NoFlags, EInternalObjectFlags::Garbage);

	TSet<UObject*> ObjectSet;
	ObjectSet.Reserve(BeginCacheObjects.Objects.Num());
	for (FBeginCacheObject& ExistingObject : BeginCacheObjects.Objects)
	{
		UObject* Object = ExistingObject.Object.Get();
		if (Object)
		{
			bool bAlreadyExists;
			ObjectSet.Add(Object, &bAlreadyExists);
			check(!bAlreadyExists); // Objects in BeginCacheObjects.Objects are guaranteed unique and we haven't added any others yet
			if (ExistingObject.bIsRootMovedObject)
			{
				GetObjectsWithOuter(Object, CurrentObjectsInOuter, true /* bIncludeNestedObjects */, RF_NoFlags, EInternalObjectFlags::Garbage);
			}
		}
	}
	UObject* FirstNewObject = nullptr;
	for (UObject* Object : CurrentObjectsInOuter)
	{
		bool bAlreadyExists;
		ObjectSet.Add(Object, &bAlreadyExists);
		if (!bAlreadyExists)
		{
			FBeginCacheObject& Added = BeginCacheObjects.Objects.Add_GetRef(FBeginCacheObject{ Object });
			Added.bHasFinishedRound = false;
			if (!FirstNewObject )
			{
				FirstNewObject  = Object;
			}
		}
	}
	bOutFoundNewObjects = FirstNewObject != nullptr;

	BeginCacheObjects.StartRound();

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

void FBeginCacheObjects::Reset()
{
	Objects.Reset();
	ObjectsInRound.Reset();
	NextIndexInRound = 0;
}

void FBeginCacheObjects::StartRound()
{
	ObjectsInRound.Reset(Objects.Num());
	for (FBeginCacheObject& Object : Objects)
	{
		if (!Object.bHasFinishedRound)
		{
			ObjectsInRound.Add(Object.Object);
		}
	}
	NextIndexInRound = 0;
}

void FBeginCacheObjects::EndRound(int32 NumPlatforms)
{
	check(NextIndexInRound == ObjectsInRound.Num()*NumPlatforms);
	ObjectsInRound.Reset();
	NextIndexInRound = 0;
	for (FBeginCacheObject& Object : Objects)
	{
		Object.bHasFinishedRound = true;
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
	, bHasReleased(Other.bHasReleased), bNeedsResourceRelease(Other.bNeedsResourceRelease)
{
	Other.Object = nullptr;
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
	UE_TRACK_REFERENCING_PACKAGE_SCOPED(LocalObject, PackageAccessTrackingOps::NAME_CookerBuildObject);
	if (RouteIsCachedCookedPlatformDataLoaded(LocalObject, TargetPlatform))
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
			LocalObject->ClearAllCachedCookedPlatformData();
		}
		delete this;
	}
}


//////////////////////////////////////////////////////////////////////////
// FPackageDataMonitor
FPackageDataMonitor::FPackageDataMonitor()
{
	FMemory::Memset(NumUrgentInState, 0);
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

int32 FPackageDataMonitor::GetNumUrgent(EPackageState InState) const
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

int32 FPackageDataMonitor::GetNumCooked() const
{
	return NumCooked;
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

void FPackageDataMonitor::OnFirstCookedPlatformAdded(FPackageData& PackageData)
{
	if (!PackageData.GetMonitorIsCooked())
	{
		++NumCooked;
		PackageData.SetMonitorIsCooked(true);
	}
}

void FPackageDataMonitor::OnLastCookedPlatformRemoved(FPackageData& PackageData)
{
	if (PackageData.GetMonitorIsCooked())
	{
		--NumCooked;
		PackageData.SetMonitorIsCooked(false);
	}
}

void FPackageDataMonitor::OnUrgencyChanged(FPackageData& PackageData)
{
	int32 Delta = PackageData.GetIsUrgent() ? 1 : -1;
	TrackUrgentRequests(PackageData.GetState(), Delta);
}

void FPackageDataMonitor::OnStateChanged(FPackageData& PackageData, EPackageState OldState)
{
	if (!PackageData.GetIsUrgent())
	{
		return;
	}

	TrackUrgentRequests(OldState, -1);
	TrackUrgentRequests(PackageData.GetState(), 1);
}

void FPackageDataMonitor::TrackUrgentRequests(EPackageState State, int32 Delta)
{
	check(EPackageState::Min <= State && State <= EPackageState::Max);
	NumUrgentInState[static_cast<uint32>(State) - static_cast<uint32>(EPackageState::Min)] += Delta;
	check(NumUrgentInState[static_cast<uint32>(State) - static_cast<uint32>(EPackageState::Min)] >= 0);
}


//////////////////////////////////////////////////////////////////////////
// FPackageDatas

IAssetRegistry* FPackageDatas::AssetRegistry = nullptr;

FPackageDatas::FPackageDatas(UCookOnTheFlyServer& InCookOnTheFlyServer)
	: CookOnTheFlyServer(InCookOnTheFlyServer)
	, LastPollAsyncTime(0)
{
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

	// Discoveries during the processing of the initial cluster are expected, so LogDiscoveredPackages must be off.
	SetLogDiscoveredPackages(false);
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
	return TEXT("FPackageDatas");
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
	FPackageData* PackageData = new FPackageData(*this, PackageName, FileName);

	FWriteScopeLock ExistenceWriteLock(ExistenceLock);
	FPackageData*& ExistingByPackageName = PackageNameToPackageData.FindOrAdd(PackageName);
	FPackageData*& ExistingByFileName = FileNameToPackageData.FindOrAdd(FileName);
	if (ExistingByPackageName)
	{
		// The other CreatePackageData call should have added the FileName as well
		check(ExistingByFileName == ExistingByPackageName);
		delete PackageData;
		return *ExistingByPackageName;
	}
	// If no other CreatePackageData added the PackageName, then they should not have added
	// the FileName either
	check(!ExistingByFileName);
	ExistingByPackageName = PackageData;
	ExistingByFileName = PackageData;
	PackageDatas.Add(PackageData);
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
	const ITargetPlatform* TargetPlatform)
{
	int32 NumPackages = ExistingPackages.Num();

	// Make the list unique
	TMap<FName, FName> UniquePackages;
	UniquePackages.Reserve(NumPackages);
	for (const FConstructPackageData& PackageToAdd : ExistingPackages)
	{
		FName& AddedFileName = UniquePackages.FindOrAdd(PackageToAdd.PackageName, PackageToAdd.NormalizedFileName);
		check(AddedFileName == PackageToAdd.NormalizedFileName);
	}
	TArray<FConstructPackageData> UniqueArray;
	if (UniquePackages.Num() != NumPackages)
	{
		NumPackages = UniquePackages.Num();
		UniqueArray.Reserve(NumPackages);
		for (TPair<FName, FName>& Pair : UniquePackages)
		{
			UniqueArray.Add(FConstructPackageData{ Pair.Key, Pair.Value });
		}
		ExistingPackages = UniqueArray;
	}

	// parallelize the read-only operations (and write NewPackageDataObjects by index which has no threading issues)
	TArray<FPackageData*> NewPackageDataObjects;
	NewPackageDataObjects.AddZeroed(NumPackages);
	FWriteScopeLock ExistenceWriteLock(ExistenceLock);
	ParallelFor(NumPackages,
		[&ExistingPackages, TargetPlatform, &NewPackageDataObjects, this](int Index)
	{
		FName PackageName = ExistingPackages[Index].PackageName;
		FName NormalizedFileName = ExistingPackages[Index].NormalizedFileName;
		check(!PackageName.IsNone());
		check(!NormalizedFileName.IsNone());

		FPackageData** PackageDataMapAddr = FileNameToPackageData.Find(NormalizedFileName);
		FPackageData* PackageData = nullptr;
		if (PackageDataMapAddr != nullptr)
		{
			PackageData = *PackageDataMapAddr;
		}
		else
		{
			// create the package data and remember it for updating caches after the the ParallelFor
			PackageData = new FPackageData(*this, PackageName, NormalizedFileName);
			NewPackageDataObjects[Index] = PackageData;
		}
		PackageData->SetPlatformCooked(TargetPlatform, true /* Succeeded */);
	});

	// update cache for all newly created objects (copied from CreatePackageData)
	for (FPackageData* PackageData : NewPackageDataObjects)
	{
		if (PackageData)
		{
			FPackageData* ExistingByFileName = FileNameToPackageData.Add(PackageData->FileName, PackageData);
			// We looked up by FileName in the loop; it should still have been unset before the write we just did
			check(ExistingByFileName == PackageData);
			FPackageData* ExistingByPackageName = PackageNameToPackageData.FindOrAdd(PackageData->PackageName, PackageData);
			// If no other CreatePackageData added the FileName, then they should not have added
			// the PackageName either
			check(ExistingByPackageName == PackageData);
			PackageDatas.Add(PackageData);
		}
	}
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

int32 FPackageDatas::GetNumCooked()
{
	return Monitor.GetNumCooked();
}

void FPackageDatas::GetCookedPackagesForPlatform(const ITargetPlatform* Platform, TArray<FPackageData*>& CookedPackages,
	bool bGetFailedCookedPackages, bool bGetSuccessfulCookedPackages)
{
	for (FPackageData* PackageData : PackageDatas)
	{
		ECookResult CookResults = PackageData->GetCookResults(Platform);
		if (((CookResults == ECookResult::Succeeded) & (bGetSuccessfulCookedPackages != 0)) |
			((CookResults == ECookResult::Failed) & (bGetFailedCookedPackages != 0)))
		{
			CookedPackages.Add(PackageData);
		}
	}
}

void FPackageDatas::Clear()
{
	FWriteScopeLock ExistenceWriteLock(ExistenceLock);
	PendingCookedPlatformDatas.Empty(); // These destructors will dereference PackageDatas
	RequestQueue.Empty();
	SaveQueue.Empty();
	PackageNameToPackageData.Empty();
	FileNameToPackageData.Empty();
	for (FPackageData* PackageData : PackageDatas)
	{
		PackageData->ClearReferences();
	}
	for (FPackageData* PackageData : PackageDatas)
	{
		delete PackageData;
	}
	PackageDatas.Empty();
	ShowInstigatorPackageData = nullptr;
}

void FPackageDatas::ClearCookedPlatforms()
{
	for (FPackageData* PackageData : PackageDatas)
	{
		PackageData->ClearCookProgress();
	}
}

void FPackageDatas::OnRemoveSessionPlatform(const ITargetPlatform* TargetPlatform)
{
	for (FPackageData* PackageData : PackageDatas)
	{
		PackageData->OnRemoveSessionPlatform(TargetPlatform);
	}
}

TArray<FPendingCookedPlatformData>& FPackageDatas::GetPendingCookedPlatformDatas()
{
	return PendingCookedPlatformDatas;
}

void FPackageDatas::PollPendingCookedPlatformDatas(bool bForce, double& LastCookableObjectTickTime)
{
	if (PendingCookedPlatformDatas.Num() == 0)
	{
		return;
	}

	double CurrentTime = FPlatformTime::Seconds();
	if (!bForce)
	{
		// ProcessAsyncResults and IsCachedCookedPlatformDataLoaded can be expensive to call
		// Cap the frequency at which we call them.
		if (CurrentTime < LastPollAsyncTime + GPollAsyncPeriod)
		{
			return;
		}
		LastPollAsyncTime = CurrentTime;
	}

	GShaderCompilingManager->ProcessAsyncResults(true /* bLimitExecutionTime */,
		false /* bBlockOnGlobalShaderCompletion */);
	FAssetCompilingManager::Get().ProcessAsyncTasks(true);
	if (LastCookableObjectTickTime + TickCookableObjectsFrameTime <= CurrentTime)
	{
		UE_SCOPED_COOKTIMER(TickCookableObjects);
		FTickableCookObject::TickObjects(static_cast<float>(CurrentTime - LastCookableObjectTickTime), false /* bTickComplete */);
		LastCookableObjectTickTime = CurrentTime;
	}

	FPendingCookedPlatformData* Datas = PendingCookedPlatformDatas.GetData();
	for (int Index = 0; Index < PendingCookedPlatformDatas.Num();)
	{
		if (Datas[Index].PollIsComplete())
		{
			PendingCookedPlatformDatas.RemoveAtSwap(Index, 1 /* Count */, false /* bAllowShrinking */);
		}
		else
		{
			++Index;
		}
	}
}

TArray<FPackageData*>::RangedForIteratorType FPackageDatas::begin()
{
	return PackageDatas.begin();
}

TArray<FPackageData*>::RangedForIteratorType FPackageDatas::end()
{
	return PackageDatas.end();
}

void FPackageDatas::RemapTargetPlatforms(const TMap<ITargetPlatform*, ITargetPlatform*>& Remap)
{
	for (FPackageData* PackageData : PackageDatas)
	{
		PackageData->RemapTargetPlatforms(Remap);
	}
	for (FPendingCookedPlatformData& CookedPlatformData : PendingCookedPlatformDatas)
	{
		CookedPlatformData.RemapTargetPlatforms(Remap);
	}
}

void FPackageDatas::DebugInstigator(FPackageData& PackageData)
{
	if (ShowInstigatorPackageData != &PackageData)
	{
		return;
	}

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
	uint32 Count = UnclusteredRequests.Num() + ReadyRequestsNum();
	for (const FRequestCluster& RequestCluster : RequestClusters)
	{
		Count += RequestCluster.NumPackageDatas();
	}
	return Count;
}

bool FRequestQueue::Contains(const FPackageData* InPackageData) const
{
	FPackageData* PackageData = const_cast<FPackageData*>(InPackageData);
	if (UnclusteredRequests.Contains(PackageData) || NormalRequests.Contains(PackageData) || UrgentRequests.Contains(PackageData))
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

uint32 FRequestQueue::RemoveRequest(FPackageData* PackageData)
{
	uint32 OriginalNum = Num();
	UnclusteredRequests.Remove(PackageData);
	NormalRequests.Remove(PackageData);
	UrgentRequests.Remove(PackageData);
	for (FRequestCluster& RequestCluster : RequestClusters)
	{
		RequestCluster.RemovePackageData(PackageData);
	}
	uint32 Result = OriginalNum - Num();
	check(Result == 0 || Result == 1);
	return Result;
}

uint32 FRequestQueue::Remove(FPackageData* PackageData)
{
	return RemoveRequest(PackageData);
}

bool FRequestQueue::IsReadyRequestsEmpty() const
{
	return ReadyRequestsNum() == 0;
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
	if (!PackageData->AreAllRequestedPlatformsExplored())
	{
		UnclusteredRequests.Add(PackageData);
	}
	else
	{
		AddReadyRequest(PackageData, bForceUrgent);
	}
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

}
