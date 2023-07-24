// Copyright Epic Games, Inc. All Rights Reserved.
#include "WorldPartition/ContentBundle/ContentBundle.h"

#include "WorldPartition/ContentBundle/ContentBundleStatus.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/ContentBundle/ContentBundleDescriptor.h"
#include "WorldPartition/WorldPartitionRuntimeHash.h"
#include "WorldPartition/ContentBundle/ContentBundleLog.h"

#if WITH_EDITOR
#include "WorldPartition/ContentBundle/ContentBundleWorldSubsystem.h"
#endif

int32 FContentBundle::ContentBundlesEpoch = 0;

FContentBundle::FContentBundle(TSharedPtr<FContentBundleClient>& InClient, UWorld* InWorld)
	: FContentBundleBase(InClient, InWorld)
	, ExternalStreamingObjectPackage(nullptr)
	, ExternalStreamingObject(nullptr)
{

}

void FContentBundle::DoInitialize()
{
#if WITH_EDITOR
	InitializeForPIE();
#else
	ExternalStreamingObjectPackage = LoadPackage(nullptr, *GetExternalStreamingObjectPackagePath(), LOAD_None);
	if (ExternalStreamingObjectPackage != nullptr)
	{
		if (UObject* Object = StaticFindObjectFast(URuntimeHashExternalStreamingObjectBase::StaticClass(), ExternalStreamingObjectPackage, *GetExternalStreamingObjectName()))
		{
			ExternalStreamingObject = CastChecked<URuntimeHashExternalStreamingObjectBase>(Object);
			ExternalStreamingObject->OnStreamingObjectLoaded();
		}
		else
		{
			UE_LOG(LogContentBundle, Error, TEXT("%s No streaming object found in package %s."), *ContentBundle::Log::MakeDebugInfoString(*this), *GetExternalStreamingObjectPackagePath());
		}
	}
	else
	{
		UE_LOG(LogContentBundle, Log, TEXT("%s No streaming object found. No content will be injected."), *ContentBundle::Log::MakeDebugInfoString(*this));
	}
#endif

	SetStatus(EContentBundleStatus::Registered);
}

void FContentBundle::DoUninitialize()
{
	SetStatus(EContentBundleStatus::Unknown);

	ExternalStreamingObject = nullptr;
	ExternalStreamingObjectPackage = nullptr;
}

void FContentBundle::DoInjectContent()
{
	if (ExternalStreamingObject != nullptr)
	{
		if (GetInjectedWorld()->GetWorldPartition()->RuntimeHash->InjectExternalStreamingObject(ExternalStreamingObject))
		{
			UE_LOG(LogContentBundle, Log, TEXT("%s Streaming Object Injected."), *ContentBundle::Log::MakeDebugInfoString(*this));
			SetStatus(EContentBundleStatus::ContentInjected);

			ContentBundlesEpoch++;
		}
		else
		{
			UE_LOG(LogContentBundle, Error, TEXT("%s Failed to inject streaming object."), *ContentBundle::Log::MakeDebugInfoString(*this));
			SetStatus(EContentBundleStatus::FailedToInject);
		}
	}
	else
	{
		UE_LOG(LogContentBundle, Log, TEXT("%s No streaming object to inject."), *ContentBundle::Log::MakeDebugInfoString(*this));
		SetStatus(EContentBundleStatus::ContentInjected);
	}
}

void FContentBundle::DoRemoveContent()
{
	if (ExternalStreamingObject != nullptr)
	{
		if (GetInjectedWorld()->GetWorldPartition()->RuntimeHash->RemoveExternalStreamingObject(ExternalStreamingObject))
		{
			ContentBundlesEpoch++;
		}
		else
		{
			UE_LOG(LogContentBundle, Error, TEXT("%s Error while removing streaming object."), *ContentBundle::Log::MakeDebugInfoString(*this));
		}
	}
	
	SetStatus(EContentBundleStatus::Registered);
}

void FContentBundle::AddReferencedObjects(FReferenceCollector& Collector)
{
	FContentBundleBase::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(ExternalStreamingObjectPackage);
	Collector.AddReferencedObject(ExternalStreamingObject);
}

bool FContentBundle::IsValid() const
{
	return GetDescriptor()->IsValid();
}

#if WITH_EDITOR
void FContentBundle::InitializeForPIE()
{
	UContentBundleManager* ContentBundleManager = GetInjectedWorld()->ContentBundleManager;
	if (UContentBundleDuplicateForPIEHelper* PIEHelper = ContentBundleManager->GetPIEDuplicateHelper())
	{
		ExternalStreamingObject = PIEHelper->RetrieveContentBundleStreamingObject(*this);
		if (ExternalStreamingObject == nullptr)
		{
			UE_LOG(LogContentBundle, Log, TEXT("%s No streaming object found. There are %u existing streaming objects."), *ContentBundle::Log::MakeDebugInfoString(*this), PIEHelper->GetStreamingObjectCount());
		}
	}
}
#endif
