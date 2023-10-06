// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/ExecutionContext.h"

#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/DecoratorTemplate.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeInstance.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateRegistry.h"

namespace UE::AnimNext
{
	namespace Private
	{
		static thread_local UE::AnimNext::FExecutionContext* GThreadLocalExecutionContext = nullptr;
	}

	FExecutionContext::FExecutionContext(TArrayView<const uint8> GraphSharedData_)
		: GraphSharedData(GraphSharedData_)
	{
		// There can be only one execution context alive per thread
		ensure(Private::GThreadLocalExecutionContext == nullptr);
		Private::GThreadLocalExecutionContext = this;
	}

	FExecutionContext::~FExecutionContext()
	{
		// There can be only one execution context alive per thread
		ensure(Private::GThreadLocalExecutionContext == this);
		Private::GThreadLocalExecutionContext = nullptr;
	}

	FDecoratorPtr FExecutionContext::AllocateNodeInstance(FWeakDecoratorPtr ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle)
	{
		ensure(ChildDecoratorHandle.IsValid());
		if (!ChildDecoratorHandle.IsValid())
		{
			return FDecoratorPtr();	// Attempting to allocate a node using an invalid decorator handle
		}

		const FNodeHandle ChildNodeHandle = ChildDecoratorHandle.GetNodeHandle();
		const FNodeDescription& NodeDesc = GetNodeDescription(ChildNodeHandle);

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		ensure(NodeTemplate != nullptr);
		if (NodeTemplate == nullptr)
		{
			return FDecoratorPtr();	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const uint32_t ChildDecoratorIndex = ChildDecoratorHandle.GetDecoratorIndex();

		if (ChildDecoratorIndex >= NodeTemplate->GetNumDecorators())
		{
			return FDecoratorPtr();	// The requested decorator index doesn't exist on that node descriptor
		}

		// If the decorator we wish to allocate lives in the parent node, return a weak handle to it
		// We use a weak handle to avoid issues when multiple base decorators live within the same node
		// When this happens, a decorator can end up pointing to another within the same node causing
		// the reference count to never reach zero when all other handles are released
		if (FNodeInstance* ParentNodeInstance = ParentBinding.GetNodeInstance())
		{
			if (ParentNodeInstance->GetNodeHandle() == ChildNodeHandle)
			{
				return FDecoratorPtr(ParentNodeInstance, FDecoratorPtr::IS_WEAK_BIT, ChildDecoratorIndex);
			}
		}

		// We need to allocate a new node instance
		const FDecoratorTemplate* DecoratorDescs = NodeTemplate->GetDecorators();

		const uint32 InstanceSize = NodeTemplate->GetInstanceSize();
		uint8* NodeInstanceBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(InstanceSize, 16));
		FNodeInstance* NodeInstance = new(NodeInstanceBuffer) FNodeInstance(ChildNodeHandle);

		// Start construction with the bottom decorator
		const FDecoratorTemplate* StartDesc = DecoratorDescs;
		const FDecoratorTemplate* EndDesc = DecoratorDescs + NodeTemplate->GetNumDecorators();

		const FDecoratorTemplate* FailedDecoratorDesc = nullptr;
		for (const FDecoratorTemplate* DecoratorDesc = StartDesc; DecoratorDesc != EndDesc; ++DecoratorDesc)
		{
			const FDecorator* Decorator = GetDecorator(*DecoratorDesc);
			ensure(Decorator != nullptr);
			if (Decorator == nullptr)
			{
				// Failed to find the matching decorator, did it get unregistered or is the decorator descriptor corrupted?
				FailedDecoratorDesc = DecoratorDesc;
				break;
			}

			const uint32 DecoratorIndex = DecoratorDesc - DecoratorDescs;

			FWeakDecoratorPtr DecoratorPtr(NodeInstance, DecoratorIndex);

			const FAnimNextDecoratorSharedData* SharedData = DecoratorDesc->GetDecoratorDescription(NodeDesc);
			FDecoratorInstanceData* InstanceData = DecoratorDesc->GetDecoratorInstance(*NodeInstance);

			Decorator->ConstructDecoratorInstance(*this, DecoratorPtr, *SharedData, *InstanceData);
		}

		if (FailedDecoratorDesc != nullptr)
		{
			// We failed to construct our node instance, destroy it
			// Start destruction with the decorator prior to the one that failed
			StartDesc = FailedDecoratorDesc - 1;
			EndDesc = DecoratorDescs - 1;
			for (const FDecoratorTemplate* DecoratorDesc = StartDesc; DecoratorDesc != EndDesc; --DecoratorDesc)
			{
				const FDecorator* Decorator = GetDecorator(*DecoratorDesc);
				const uint32 DecoratorIndex = DecoratorDesc - DecoratorDescs;

				FWeakDecoratorPtr DecoratorPtr(NodeInstance, DecoratorIndex);

				const FAnimNextDecoratorSharedData* SharedData = DecoratorDesc->GetDecoratorDescription(NodeDesc);
				FDecoratorInstanceData* InstanceData = DecoratorDesc->GetDecoratorInstance(*NodeInstance);

				Decorator->DestructDecoratorInstance(*this, DecoratorPtr, *SharedData, *InstanceData);
			}

			FMemory::Free(NodeInstance);

			return FDecoratorPtr();
		}

		return FDecoratorPtr(NodeInstance, ChildDecoratorIndex);
	}

	void FExecutionContext::ReleaseNodeInstance(FNodeInstance* NodeInstance)
	{
		ensure(NodeInstance != nullptr && NodeInstance->IsValid());
		if (NodeInstance == nullptr || !NodeInstance->IsValid())
		{
			return;	// Invalid node instance provided
		}

		ensure(NodeInstance->GetReferenceCount() == 0);
		if (NodeInstance->GetReferenceCount() != 0)
		{
			return;	// Node instance still has references, we can't release it
		}

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		ensure(NodeTemplate != nullptr);
		if (NodeTemplate == nullptr)
		{
			return;	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const FDecoratorTemplate* DecoratorDescs = NodeTemplate->GetDecorators();

		// Start destruction with the top decorator
		const FDecoratorTemplate* StartDesc = DecoratorDescs + NodeTemplate->GetNumDecorators() - 1;
		const FDecoratorTemplate* EndDesc = DecoratorDescs - 1;
		for (const FDecoratorTemplate* DecoratorDesc = StartDesc; DecoratorDesc != EndDesc; --DecoratorDesc)
		{
			const FDecorator* Decorator = GetDecorator(*DecoratorDesc);
			if (ensure(Decorator != nullptr))
			{
				const uint32 DecoratorIndex = DecoratorDesc - DecoratorDescs;

				FWeakDecoratorPtr DecoratorPtr(NodeInstance, DecoratorIndex);

				const FAnimNextDecoratorSharedData* SharedData = DecoratorDesc->GetDecoratorDescription(NodeDesc);
				FDecoratorInstanceData* InstanceData = DecoratorDesc->GetDecoratorInstance(*NodeInstance);

				Decorator->DestructDecoratorInstance(*this, DecoratorPtr, *SharedData, *InstanceData);
			}
		}

		NodeInstance->~FNodeInstance();
		FMemory::Free(NodeInstance);
	}

	bool FExecutionContext::GetInterfaceImpl(FDecoratorInterfaceUID InterfaceUID, FWeakDecoratorPtr DecoratorPtr, FDecoratorBinding& InterfaceBinding) const
	{
		if (!DecoratorPtr.IsValid())
		{
			return false;
		}

		FNodeInstance* NodeInstance = DecoratorPtr.GetNodeInstance();

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		ensure(NodeTemplate != nullptr);
		if (NodeTemplate == nullptr)
		{
			return false;	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const FDecoratorTemplate* DecoratorDescs = NodeTemplate->GetDecorators();

		// We only search within the partial stack of the provided decorator
		const FDecoratorTemplate* CurrentDecoratorDesc = DecoratorDescs + DecoratorPtr.GetDecoratorIndex();
		const FDecoratorTemplate* BaseDecoratorDesc = CurrentDecoratorDesc->GetMode() == EDecoratorMode::Base ? CurrentDecoratorDesc : (CurrentDecoratorDesc - CurrentDecoratorDesc->GetAdditiveDecoratorIndex());

		// Start searching with the top additive decorator towards our base decorator
		const FDecoratorTemplate* StartDesc = BaseDecoratorDesc + BaseDecoratorDesc->GetNumAdditiveDecorators();
		const FDecoratorTemplate* EndDesc = BaseDecoratorDesc - 1;
		for (const FDecoratorTemplate* DecoratorDesc = StartDesc; DecoratorDesc != EndDesc; --DecoratorDesc)
		{
			const FDecorator* Decorator = GetDecorator(*DecoratorDesc);
			ensure(Decorator != nullptr);
			if (Decorator == nullptr)
			{
				return false;	// Failed to find the matching decorator, did it get unregistered or is the decorator descriptor corrupted?
			}

			if (const IDecoratorInterface* Interface = Decorator->GetDecoratorInterface(InterfaceUID))
			{
				const FAnimNextDecoratorSharedData* SharedData = DecoratorDesc->GetDecoratorDescription(NodeDesc);
				FDecoratorInstanceData* InstanceData = DecoratorDesc->GetDecoratorInstance(*NodeInstance);

				const uint32 DecoratorIndex = DecoratorDesc - DecoratorDescs;
				FWeakDecoratorPtr InterfaceDecoratorPtr(NodeInstance, DecoratorIndex);

				InterfaceBinding = FDecoratorBinding(Interface, SharedData, InstanceData, InterfaceDecoratorPtr);
				return true;
			}
		}

		// We failed to find a decorator that handles this interface
		return false;
	}

	bool FExecutionContext::GetInterfaceSuperImpl(FDecoratorInterfaceUID InterfaceUID, FWeakDecoratorPtr DecoratorPtr, FDecoratorBinding& SuperBinding) const
	{
		if (!DecoratorPtr.IsValid())
		{
			return false;	// Decorator pointer isn't valid, can't find a 'super'
		}

		FNodeInstance* NodeInstance = DecoratorPtr.GetNodeInstance();

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		ensure(NodeTemplate != nullptr);
		if (NodeTemplate == nullptr)
		{
			return false;	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const FDecoratorTemplate* DecoratorDescs = NodeTemplate->GetDecorators();

		// We only search within the partial stack of the provided decorator
		const FDecoratorTemplate* CurrentDecoratorDesc = DecoratorDescs + DecoratorPtr.GetDecoratorIndex();
		if (CurrentDecoratorDesc->GetMode() == EDecoratorMode::Base)
		{
			return false;	// We've reached a base decorator, we don't forward to any parent decorator we might have
		}

		const FDecoratorTemplate* BaseDecoratorDesc = CurrentDecoratorDesc - CurrentDecoratorDesc->GetAdditiveDecoratorIndex();

		// Start searching with the next additive decorator towards our base decorator
		const FDecoratorTemplate* StartDesc = CurrentDecoratorDesc - 1;
		const FDecoratorTemplate* EndDesc = BaseDecoratorDesc - 1;
		for (const FDecoratorTemplate* DecoratorDesc = StartDesc; DecoratorDesc != EndDesc; --DecoratorDesc)
		{
			const FDecorator* Decorator = GetDecorator(*DecoratorDesc);
			ensure(Decorator != nullptr);
			if (Decorator == nullptr)
			{
				return false;	// Failed to find the matching decorator, did it get unregistered or is the decorator descriptor corrupted?
			}

			if (const IDecoratorInterface* Interface = Decorator->GetDecoratorInterface(InterfaceUID))
			{
				const FAnimNextDecoratorSharedData* SharedData = DecoratorDesc->GetDecoratorDescription(NodeDesc);
				FDecoratorInstanceData* InstanceData = DecoratorDesc->GetDecoratorInstance(*NodeInstance);

				const uint32 DecoratorIndex = DecoratorDesc - DecoratorDescs;
				FWeakDecoratorPtr SuperPtr(NodeInstance, DecoratorIndex);

				SuperBinding = FDecoratorBinding(Interface, SharedData, InstanceData, SuperPtr);
				return true;
			}
		}

		// We failed to find a decorator that handles this interface
		return false;
	}

	const FNodeDescription& FExecutionContext::GetNodeDescription(FNodeHandle NodeHandle) const
	{
		check(NodeHandle.IsValid());
		return *reinterpret_cast<const FNodeDescription*>(&GraphSharedData[NodeHandle.GetSharedOffset()]);
	}

	const FNodeTemplate* FExecutionContext::GetNodeTemplate(const FNodeDescription& NodeDesc) const
	{
		check(NodeDesc.GetTemplateHandle().IsValid());
		return FNodeTemplateRegistry::Get().Find(NodeDesc.GetTemplateHandle());
	}

	const FDecorator* FExecutionContext::GetDecorator(const FDecoratorTemplate& Template) const
	{
		check(Template.GetRegistryHandle().IsValid());
		return FDecoratorRegistry::Get().Find(Template.GetRegistryHandle());
	}

	FExecutionContext* GetThreadExecutionContext()
	{
		return Private::GThreadLocalExecutionContext;
	}
}
