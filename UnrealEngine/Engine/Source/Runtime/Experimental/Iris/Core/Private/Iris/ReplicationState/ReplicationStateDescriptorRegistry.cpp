// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/ReplicationState/ReplicationStateDescriptorRegistry.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Iris/ReplicationSystem/ReplicationProtocolManager.h"
#include "Iris/Core/IrisProfiler.h"

namespace UE::Net::Private
{

FReplicationStateDescriptorRegistry::FReplicationStateDescriptorRegistry()
: ProtocolManager(nullptr)
{
}

void FReplicationStateDescriptorRegistry::Init(const FReplicationStateDescriptorRegistryInitParams& Params)
{
	ProtocolManager = Params.ProtocolManager;
}

void FReplicationStateDescriptorRegistry::Register(const FFieldVariant& Object, const UObject* ObjectForPruning, const FDescriptors& Descriptors)
{
	const FRegisteredDescriptors* Entry = RegisteredDescriptorsMap.Find(Object);
	
	check(ObjectForPruning != nullptr);

	if (Entry)
	{
		// if we found the entry and the WeakPtrForPruning is also a match we are trying to register descriptors for an already registered class
		if (Entry->WeakPtrForPruning.Get() == ObjectForPruning)
		{
			checkf(false, TEXT("FReplicationStateDescriptorRegistry::Trying to register descriptors for the same UObject %s"), ToCStr(ObjectForPruning->GetName()));
			return;
		}
		else
		{
			// Notify protocol manager about pruned descriptors
			InvalidateDescriptors(Entry->Descriptors);

			RegisteredDescriptorsMap.Remove(Object);
		}
	}

	FRegisteredDescriptors NewEntry;
	NewEntry.WeakPtrForPruning = TWeakObjectPtr<const UObject>(ObjectForPruning);
	NewEntry.Descriptors = Descriptors;

	RegisteredDescriptorsMap.Add(Object, NewEntry);
}

void FReplicationStateDescriptorRegistry::Register(const FFieldVariant& Object, const UObject* ObjectForPruning, const TRefCountPtr<const FReplicationStateDescriptor>& Descriptor)
{
	const FRegisteredDescriptors* Entry = RegisteredDescriptorsMap.Find(Object);

	check(ObjectForPruning != nullptr);

	if (Entry)
	{
		// if we found the entry and the WeakPtrForPruning is also a match we are trying to register descriptors for an already registered class
		if (Entry->WeakPtrForPruning.Get() == ObjectForPruning)
		{
			checkf(false, TEXT("FReplicationStateDescriptorRegistry::Trying to register descriptors for the same UObject %s"), ToCStr(ObjectForPruning->GetName()));
			return;
		}
		else
		{
			// Notify protocol manager about pruned descriptors
			InvalidateDescriptors(Entry->Descriptors);

			RegisteredDescriptorsMap.Remove(Object);
		}
	}

	FRegisteredDescriptors& NewEntry = RegisteredDescriptorsMap.Emplace(Object);
	NewEntry.WeakPtrForPruning = TWeakObjectPtr<const UObject>(ObjectForPruning);
	NewEntry.Descriptors.Add(Descriptor);
}

const FReplicationStateDescriptorRegistry::FDescriptors* FReplicationStateDescriptorRegistry::Find(const FFieldVariant& Object, const UObject* ObjectForPruning) const
{
	const FRegisteredDescriptors* Entry = RegisteredDescriptorsMap.Find(Object);

	check(ObjectForPruning != nullptr);

	if (Entry && Entry->WeakPtrForPruning.Get() == ObjectForPruning)
	{
		return &Entry->Descriptors;
	}
	else
	{
		return nullptr;
	}
}

void FReplicationStateDescriptorRegistry::PruneStaleDescriptors()
{
	IRIS_PROFILER_SCOPE(FReplicationStateDescriptorRegistry_PruneStaleDescriptors);

	// Iterate over all registered descriptors and see if they have been destroyed
	for (auto It = RegisteredDescriptorsMap.CreateIterator(); It; ++It)
	{
		const FRegisteredDescriptors& RegisteredDescriptors = It.Value();
		if (!RegisteredDescriptors.WeakPtrForPruning.IsValid())
		{
			// Notify protocol manager about pruned descriptors
			InvalidateDescriptors(RegisteredDescriptors.Descriptors);

			It.RemoveCurrent();
		}
	}
}

const UObject* FReplicationStateDescriptorRegistry::GetObjectForPruning(const FFieldVariant& FieldVariant)
{
	if (FieldVariant.IsUObject())
	{
		return FieldVariant.ToUObject();
	}
	else
	{
		const FField* Field = FieldVariant.ToField();
		const UObject* Object = Field->GetOwnerUObject();
		return Object;
	}
}

void FReplicationStateDescriptorRegistry::InvalidateDescriptors(const FDescriptors& Descriptors) const
{
	if (ProtocolManager)
	{
		for (const TRefCountPtr<const FReplicationStateDescriptor>& Descriptor : MakeArrayView(Descriptors.GetData(), Descriptors.Num()))
		{
			ProtocolManager->InvalidateDescriptor(Descriptor.GetReference());
		}
	}
}

}
