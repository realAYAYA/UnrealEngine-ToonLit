// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundleBase.h"

#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/ContentBundle/ContentBundleClient.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"
#include "WorldPartition/ContentBundle/ContentBundlePaths.h"
#include "Engine/World.h"

FContentBundleBase::FContentBundleBase(TSharedPtr<FContentBundleClient>& InClient, UWorld* InWorld)
	: Client(InClient)
	, InjectedWorld(InWorld)
	, Descriptor(InClient->GetDescriptor())
	, Status(EContentBundleStatus::Unknown)
{}

FContentBundleBase::~FContentBundleBase()
{
	check(GetStatus() == EContentBundleStatus::Unknown);
}

void FContentBundleBase::Initialize()
{
	check(GetStatus() == EContentBundleStatus::Unknown);

	DoInitialize();

	check(GetStatus() == EContentBundleStatus::Registered);
}

void FContentBundleBase::Uninitialize()
{
	check(GetStatus() != EContentBundleStatus::Unknown);

	if (GetStatus() == EContentBundleStatus::ReadyToInject || GetStatus() == EContentBundleStatus::ContentInjected)
	{
		RemoveContent();
	}

	DoUninitialize();

	check(GetStatus() == EContentBundleStatus::Unknown);
}

void FContentBundleBase::InjectContent()
{
	check(GetStatus() == EContentBundleStatus::Registered);

	DoInjectContent();

	check(GetStatus() == EContentBundleStatus::ReadyToInject || GetStatus() == EContentBundleStatus::ContentInjected || GetStatus() == EContentBundleStatus::FailedToInject);

	if (TSharedPtr<FContentBundleClient> PinnedClient = Client.Pin())
	{
		PinnedClient->OnContentInjectedInWorld(GetStatus(), GetInjectedWorld());
	}
}

void FContentBundleBase::RemoveContent()
{
	check(GetStatus() == EContentBundleStatus::ReadyToInject || GetStatus() == EContentBundleStatus::ContentInjected || GetStatus() == EContentBundleStatus::FailedToInject);

	if (GetStatus() == EContentBundleStatus::ContentInjected || GetStatus() == EContentBundleStatus::ReadyToInject)
	{
		DoRemoveContent();
	}

	check(GetStatus() == EContentBundleStatus::Registered);

	if (TSharedPtr<FContentBundleClient> PinnedClient = Client.Pin())
	{
		PinnedClient->OnContentRemovedFromWorld(GetStatus(), GetInjectedWorld());
	}
}

const TWeakPtr<FContentBundleClient>& FContentBundleBase::GetClient() const
{
	return Client;
}

const UContentBundleDescriptor* FContentBundleBase::GetDescriptor() const
{
	return Descriptor;
}

FString FContentBundleBase::GetExternalStreamingObjectPackageName() const
{
	return ContentBundlePaths::GetCookedContentBundleLevelFolder(*this) + ContentBundlePaths::GetGeneratedFolderName() + TEXT("/StreamingObject");
}

FString FContentBundleBase::GetExternalStreamingObjectName() const
{
	return SlugStringForValidName(GetDisplayName() + TEXT("_") + GetDescriptor()->GetGuid().ToString() + TEXT("_ExternalStreamingObject"));
}

UWorld* FContentBundleBase::GetInjectedWorld() const
{
	return InjectedWorld;
}

const FString& FContentBundleBase::GetDisplayName() const
{
	return GetDescriptor()->GetDisplayName();
}

void FContentBundleBase::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(Descriptor);
}

void FContentBundleBase::SetStatus(EContentBundleStatus NewStatus)
{
	check(NewStatus != Status);

	UE_LOG(LogContentBundle, Log, TEXT("[CB: %s] State changing from %s to %s"), *GetDescriptor()->GetDisplayName(), *UEnum::GetDisplayValueAsText(Status).ToString(), *UEnum::GetDisplayValueAsText(NewStatus).ToString());
	Status = NewStatus;
}