// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/NodeTemplateRegistry.h"

#include "DecoratorBase/DecoratorTemplate.h"
#include "DecoratorBase/NodeTemplate.h"

namespace UE::AnimNext
{
	namespace Private
	{
		static FNodeTemplateRegistry* GNodeTemplateRegistry = nullptr;
	}

	FNodeTemplateRegistry& FNodeTemplateRegistry::Get()
	{
		checkf(Private::GNodeTemplateRegistry, TEXT("Node Template Registry is not instanced. It is only valid to access this while the engine module is loaded."));
		return *Private::GNodeTemplateRegistry;
	}

	void FNodeTemplateRegistry::Init()
	{
		if (ensure(Private::GNodeTemplateRegistry == nullptr))
		{
			Private::GNodeTemplateRegistry = new FNodeTemplateRegistry();
		}
	}

	void FNodeTemplateRegistry::Destroy()
	{
		if (ensure(Private::GNodeTemplateRegistry != nullptr))
		{
			delete Private::GNodeTemplateRegistry;
			Private::GNodeTemplateRegistry = nullptr;
		}
	}

	FNodeTemplateRegistryHandle FNodeTemplateRegistry::Find(uint32 NodeTemplateUID) const
	{
		const FNodeTemplateRegistryHandle* TemplateHandle = TemplateUIDToHandleMap.Find(NodeTemplateUID);
		return TemplateHandle != nullptr ? *TemplateHandle : FNodeTemplateRegistryHandle();
	}

	FNodeTemplateRegistryHandle FNodeTemplateRegistry::FindOrAdd(const FNodeTemplate* NodeTemplate)
	{
		if (NodeTemplate == nullptr)
		{
			return FNodeTemplateRegistryHandle();
		}

		const uint32 TemplateUID = NodeTemplate->GetUID();

		if (FNodeTemplateRegistryHandle* TemplateHandle = TemplateUIDToHandleMap.Find(TemplateUID))
		{
			// We have already seen this template before, return its handle
			return *TemplateHandle;
		}

		// This is a new template
		const uint32 TemplateSize = NodeTemplate->GetNodeTemplateSize();

		// Allocate and copy the template in our global buffer
		const uint32 TemplateOffset = TemplateBuffer.AddUninitialized(TemplateSize);
		FMemory::Memcpy(&TemplateBuffer[TemplateOffset], NodeTemplate, TemplateSize);

		// Update our mapping
		FNodeTemplateRegistryHandle TemplateHandle = FNodeTemplateRegistryHandle::MakeHandle(TemplateOffset);
		TemplateUIDToHandleMap.Add(TemplateUID, TemplateHandle);

		return TemplateHandle;
	}

	void FNodeTemplateRegistry::Unregister(const FNodeTemplate* NodeTemplate)
	{
		if (NodeTemplate == nullptr)
		{
			return;
		}

		const uint32 TemplateUID = NodeTemplate->GetUID();
		TemplateUIDToHandleMap.Remove(TemplateUID);
	}

	FNodeTemplate* FNodeTemplateRegistry::FindMutable(FNodeTemplateRegistryHandle TemplateHandle)
	{
		if (!TemplateHandle.IsValid())
		{
			return nullptr;
		}

		return reinterpret_cast<FNodeTemplate*>(&TemplateBuffer[TemplateHandle.GetTemplateOffset()]);
	}

	const FNodeTemplate* FNodeTemplateRegistry::Find(FNodeTemplateRegistryHandle TemplateHandle) const
	{
		if (!TemplateHandle.IsValid())
		{
			return nullptr;
		}

		return reinterpret_cast<const FNodeTemplate*>(&TemplateBuffer[TemplateHandle.GetTemplateOffset()]);
	}

	uint32 FNodeTemplateRegistry::GetNum() const
	{
		return TemplateUIDToHandleMap.Num();
	}
}
