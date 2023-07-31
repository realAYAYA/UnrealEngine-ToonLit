// Copyright Epic Games, Inc. All Rights Reserved.

#include "SceneOutlinerTreeItemSCC.h"

#include "ActorTreeItem.h"
#include "ActorDescTreeItem.h"
#include "ActorFolderTreeItem.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "WorldPartition/WorldPartitionActorDesc.h"

FSceneOutlinerTreeItemSCC::FSceneOutlinerTreeItemSCC(FSceneOutlinerTreeItemPtr InTreeItemPtr)
{
	TreeItemPtr = InTreeItemPtr;

	if (TreeItemPtr.IsValid())
	{
		if (FActorTreeItem* ActorItem = TreeItemPtr->CastTo<FActorTreeItem>())
		{
			if (AActor* Actor = ActorItem->Actor.Get())
			{
				if (Actor->IsPackageExternal())
				{
					ExternalPackageName = USourceControlHelpers::PackageFilename(Actor->GetExternalPackage());
					ExternalPackage = Actor->GetExternalPackage();
				}

				ActorPackingModeChangedDelegateHandle = Actor->OnPackagingModeChanged.AddLambda([this](AActor* InActor, bool bExternal)
					{
						if (bExternal)
						{
							ExternalPackageName = USourceControlHelpers::PackageFilename(InActor->GetExternalPackage());
							ExternalPackage = InActor->GetExternalPackage();
							ConnectSourceControl();
						}
						else
						{
							ExternalPackageName = FString();
							ExternalPackage = nullptr;
							DisconnectSourceControl();
						}
					});
			}
		}
		else if (FActorFolderTreeItem* ActorFolderItem = TreeItemPtr->CastTo<FActorFolderTreeItem>())
		{
			if (const UActorFolder* ActorFolder = ActorFolderItem->GetActorFolder())
			{
				if (ActorFolder->IsPackageExternal())
				{
					ExternalPackageName = USourceControlHelpers::PackageFilename(ActorFolder->GetExternalPackage());
					ExternalPackage = ActorFolder->GetExternalPackage();
				}
			}
		}
		else if (FActorDescTreeItem* ActorDescItem = TreeItemPtr->CastTo<FActorDescTreeItem>())
		{
			if (const FWorldPartitionActorDesc* ActorDesc = ActorDescItem->ActorDescHandle.Get())
			{
				ExternalPackageName =  USourceControlHelpers::PackageFilename(ActorDesc->GetActorPackage().ToString());
				ExternalPackage = FindPackage(nullptr, *ActorDesc->GetActorPackage().ToString());
			}
		}

		if (!ExternalPackageName.IsEmpty())
		{
			ConnectSourceControl();
		}
	}
}

FSceneOutlinerTreeItemSCC::~FSceneOutlinerTreeItemSCC()
{
	DisconnectSourceControl();
}

FSourceControlStatePtr FSceneOutlinerTreeItemSCC::GetSourceControlState()
{
	return ISourceControlModule::Get().GetProvider().GetState(ExternalPackageName, EStateCacheUsage::Use);
}

FSourceControlStatePtr FSceneOutlinerTreeItemSCC::RefreshSourceControlState()
{
	return ISourceControlModule::Get().GetProvider().GetState(ExternalPackageName, EStateCacheUsage::ForceUpdate);
}

void FSceneOutlinerTreeItemSCC::ConnectSourceControl()
{
	check(!ExternalPackageName.IsEmpty());

	ISourceControlModule& SCCModule = ISourceControlModule::Get();
	SourceControlProviderChangedDelegateHandle = SCCModule.RegisterProviderChanged(FSourceControlProviderChanged::FDelegate::CreateRaw(this, &FSceneOutlinerTreeItemSCC::HandleSourceControlProviderChanged));
	SourceControlStateChangedDelegateHandle = SCCModule.GetProvider().RegisterSourceControlStateChanged_Handle(FSourceControlStateChanged::FDelegate::CreateRaw(this, &FSceneOutlinerTreeItemSCC::HandleSourceControlStateChanged, EStateCacheUsage::Use));

	// Check if there is already a cached state for this item
	FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(ExternalPackageName, EStateCacheUsage::Use);
	if (SourceControlState.IsValid() && !SourceControlState->IsUnknown())
	{
		BroadcastNewState(SourceControlState);
	}
	else
	{
		SCCModule.QueueStatusUpdate(ExternalPackageName);
	}
}

void FSceneOutlinerTreeItemSCC::DisconnectSourceControl()
{
	if (TreeItemPtr.IsValid())
	{
		if (FActorTreeItem* ActorItem = TreeItemPtr->CastTo<FActorTreeItem>())
		{
			if (AActor* Actor = ActorItem->Actor.Get())
			{
				Actor->OnPackagingModeChanged.Remove(ActorPackingModeChangedDelegateHandle);
			}
		}
	}
	ISourceControlModule::Get().GetProvider().UnregisterSourceControlStateChanged_Handle(SourceControlStateChangedDelegateHandle);
	ISourceControlModule::Get().UnregisterProviderChanged(SourceControlProviderChangedDelegateHandle);
}

void FSceneOutlinerTreeItemSCC::HandleSourceControlStateChanged(EStateCacheUsage::Type CacheUsage)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FSceneOutlinerTreeItemSCC::HandleSourceControlStateChanged);

	FSourceControlStatePtr SourceControlState = ISourceControlModule::Get().GetProvider().GetState(ExternalPackageName, CacheUsage);
	if (SourceControlState.IsValid())
	{
		BroadcastNewState(SourceControlState);
	}
}

void FSceneOutlinerTreeItemSCC::HandleSourceControlProviderChanged(ISourceControlProvider& OldProvider, ISourceControlProvider& NewProvider)
{
	OldProvider.UnregisterSourceControlStateChanged_Handle(SourceControlStateChangedDelegateHandle);
	SourceControlStateChangedDelegateHandle = NewProvider.RegisterSourceControlStateChanged_Handle(FSourceControlStateChanged::FDelegate::CreateRaw(this, &FSceneOutlinerTreeItemSCC::HandleSourceControlStateChanged, EStateCacheUsage::Use));
	
	BroadcastNewState(nullptr);

	ISourceControlModule::Get().QueueStatusUpdate(ExternalPackageName);
}

void FSceneOutlinerTreeItemSCC::BroadcastNewState(FSourceControlStatePtr SourceControlState)
{
	OnSourceControlStateChanged.ExecuteIfBound(SourceControlState);
}
