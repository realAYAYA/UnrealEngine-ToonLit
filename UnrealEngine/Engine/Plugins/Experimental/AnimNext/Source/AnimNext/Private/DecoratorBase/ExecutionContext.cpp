// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/ExecutionContext.h"

#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorRegistry.h"
#include "DecoratorBase/DecoratorTemplate.h"
#include "DecoratorBase/NodeDescription.h"
#include "DecoratorBase/NodeInstance.h"
#include "DecoratorBase/NodeTemplate.h"
#include "DecoratorBase/NodeTemplateRegistry.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/AnimNextGraphInstance.h"

namespace UE::AnimNext
{
	FExecutionContext::FExecutionContext()
		: NodeTemplateRegistry(FNodeTemplateRegistry::Get())
		, DecoratorRegistry(FDecoratorRegistry::Get())
		, GraphInstance(nullptr)
	{
	}

	FExecutionContext::FExecutionContext(FAnimNextGraphInstancePtr& InGraphInstance)
		: FExecutionContext()
	{
		BindTo(InGraphInstance);
	}

	FExecutionContext::FExecutionContext(FAnimNextGraphInstance& InGraphInstance)
		: FExecutionContext()
	{
		BindTo(InGraphInstance);
	}

	void FExecutionContext::BindTo(FAnimNextGraphInstancePtr& InGraphInstance)
	{
		if (FAnimNextGraphInstance* Impl = InGraphInstance.GetImpl())
		{
			BindTo(*Impl);
		}
	}

	void FExecutionContext::BindTo(FAnimNextGraphInstance& InGraphInstance)
	{
		if (GraphInstance == &InGraphInstance)
		{
			return;	// Already bound to this graph instance, nothing to do
		}

		if (const UAnimNextGraph* Graph = InGraphInstance.GetGraph())
		{
			GraphInstance = &InGraphInstance;
			GraphSharedData = Graph->SharedDataBuffer;
		}
	}

	void FExecutionContext::BindTo(const FWeakDecoratorPtr& DecoratorPtr)
	{
		if (const FNodeInstance* NodeInstance = DecoratorPtr.GetNodeInstance())
		{
			BindTo(NodeInstance->GetOwner());
		}
	}

	bool FExecutionContext::IsBound() const
	{
		return GraphInstance != nullptr;
	}

	bool FExecutionContext::IsBoundTo(const FAnimNextGraphInstancePtr& InGraphInstance) const
	{
		return GraphInstance == InGraphInstance.GetImpl();
	}

	bool FExecutionContext::IsBoundTo(const FAnimNextGraphInstance& InGraphInstance) const
	{
		return GraphInstance == &InGraphInstance;
	}

	FDecoratorPtr FExecutionContext::AllocateNodeInstance(const FWeakDecoratorPtr& ParentBinding, FAnimNextDecoratorHandle ChildDecoratorHandle) const
	{
		if (!ensure(ChildDecoratorHandle.IsValid()))
		{
			return FDecoratorPtr();	// Attempting to allocate a node using an invalid decorator handle
		}

		if (!ensure(IsBound()))
		{
			return FDecoratorPtr();	// The execution context must be bound to a valid graph instance
		}

		const FNodeHandle ChildNodeHandle = ChildDecoratorHandle.GetNodeHandle();
		const FNodeDescription& NodeDesc = GetNodeDescription(ChildNodeHandle);

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
		{
			return FDecoratorPtr();	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const uint32 ChildDecoratorIndex = ChildDecoratorHandle.GetDecoratorIndex();

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

		const uint32 InstanceSize = NodeDesc.GetNodeInstanceDataSize();
		uint8* NodeInstanceBuffer = reinterpret_cast<uint8*>(FMemory::Malloc(InstanceSize, 16));
		FNodeInstance* NodeInstance = new(NodeInstanceBuffer) FNodeInstance(*GraphInstance, ChildNodeHandle);

		// Start construction with the bottom decorator
		const FDecoratorTemplate* StartDesc = DecoratorDescs;
		const FDecoratorTemplate* EndDesc = DecoratorDescs + NodeTemplate->GetNumDecorators();

		for (const FDecoratorTemplate* DecoratorDesc = StartDesc; DecoratorDesc != EndDesc; ++DecoratorDesc)
		{
			const FDecorator* Decorator = GetDecorator(*DecoratorDesc);
			if (Decorator == nullptr)
			{
				continue;	// Decorator hasn't been loaded or registered, skip it
			}

			const uint32 DecoratorIndex = DecoratorDesc - DecoratorDescs;

			FWeakDecoratorPtr DecoratorPtr(NodeInstance, DecoratorIndex);

			const FAnimNextDecoratorSharedData* SharedData = DecoratorDesc->GetDecoratorDescription(NodeDesc);
			FDecoratorInstanceData* InstanceData = DecoratorDesc->GetDecoratorInstance(*NodeInstance);

			const FDecoratorBinding Binding(nullptr, DecoratorDesc, &NodeDesc, DecoratorPtr);
			Decorator->ConstructDecoratorInstance(*this, Binding);
		}

		return FDecoratorPtr(NodeInstance, ChildDecoratorIndex);
	}

	void FExecutionContext::ReleaseNodeInstance(FDecoratorPtr& NodePtr) const
	{
		if (!NodePtr.IsValid())
		{
			return;
		}

		FNodeInstance* NodeInstance = NodePtr.GetNodeInstance();

		if (!ensure(IsBoundTo(NodeInstance->GetOwner())))
		{
			return;	// The execution context isn't bound to the right graph instance
		}

		// Reset the handle here to simplify the multiple return statements below
		NodePtr.PackedPointerAndFlags = 0;
		NodePtr.DecoratorIndex = 0;

		if (NodeInstance->RemoveReference())
		{
			return;	// Node instance still has references, we can't release it
		}

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
		{
			return;	// Node template wasn't found, node descriptor is perhaps corrupted (we'll leak the node memory)
		}

		const FDecoratorTemplate* DecoratorDescs = NodeTemplate->GetDecorators();

		// Start destruction with the top decorator
		const FDecoratorTemplate* StartDesc = DecoratorDescs + NodeTemplate->GetNumDecorators() - 1;
		const FDecoratorTemplate* EndDesc = DecoratorDescs - 1;
		for (const FDecoratorTemplate* DecoratorDesc = StartDesc; DecoratorDesc != EndDesc; --DecoratorDesc)
		{
			if (const FDecorator* Decorator = GetDecorator(*DecoratorDesc))
			{
				const uint32 DecoratorIndex = DecoratorDesc - DecoratorDescs;

				FWeakDecoratorPtr DecoratorPtr(NodeInstance, DecoratorIndex);

				const FDecoratorBinding Binding(nullptr, DecoratorDesc, &NodeDesc, DecoratorPtr);
				Decorator->DestructDecoratorInstance(*this, Binding);
			}
		}

		NodeInstance->~FNodeInstance();
		FMemory::Free(NodeInstance);
	}

	bool FExecutionContext::GetInterfaceImpl(FDecoratorInterfaceUID InterfaceUID, const FWeakDecoratorPtr& DecoratorPtr, FDecoratorBinding& InterfaceBinding) const
	{
		if (!DecoratorPtr.IsValid())
		{
			return false;
		}

		// TODO: Revisit this to avoid querying each decorator over and over
		//
		// If we have D decorators on a node and I unique interfaces per decorator then finding an interface
		// has O(D*I) complexity: for each decorator we have to test every interface
		// However, if the I interfaces are not unique per decorator and instead are unique for the system,
		// the below code will still have the same complexity. We'll end up testing the same interface multiple
		// times, once for each decorator. Because we will have a small set of interfaces per node that
		// multiple decorators implement (e.g IUpdate, IEvaluate), we can do better.
		// 
		// It would be much cheaper if instead we could test if we implement the interface first and then
		// look for a list of decorators (in stack order) that implement it. To do this, when we build the node
		// template, we could aggregate all interfaces that the decorators implement (sorted by their interface id
		// for determinism or perhaps some other criteria to put popular/hot interfaces first). Then, in the node
		// template we store a mapping of InterfaceUID to InterfaceIndex (local to that node template). The interface
		// index can then be used to index into a list of decorator indices that implement it.
		// Code would look like:
		//    * uint32 InterfaceIndex = NodeTemplate.GetInterfaceUIDs().FindIndex(InterfaceUID);	// INDEX_NONE means that none of the decorators implement the interface
		//    * uint16 DecoratorIndicesStartOffset = NodeTemplate.InterfaceIndexToDecoratorIndices(InterfaceIndex);		// An offset relative to the NodeTemplate*
		//    * const uint8* DecoratorIndicesForInterface = ((const uint8*NodeTemplate) + DecoratorIndicesStartOffset;
		//    * uint8 NumDecoratorsWithInterface = DecoratorIndicesForInterface[0];		// Reserve first index for the decorator count or if we use a guard value at the end of the array we have to reserve a value
		//    * uint8 TopDecoratorIndex = DecoratorIndicesForInterface[1];				// First index is top of stack
		// Super decorator lookup would be similar:
		//    * uint8 SuperDecoratorIndex = DecoratorIndicesForInterface[DecoratorIndicesForInterface.FindIndex(CurrentDecoratorIndex) + 1];	// Need to check if we exceed the max count (bottom of stack)
		// Once we have the decorator index, we use the decorator template as we do now
		//
		// The above would have a number of advantages:
		//    * Querying for an interface that the node doesn't implement would be much cheaper (we test each interface once)
		//    * InterfaceIndex query can easily be implemented in SIMD, we look for a uint32 in a list sequentially
		//    * Bulk/interleaved querying of interfaces would be much easier (e.g. query for IEvaluate for 4 nodes)
		//    * A lot less branching is involved, improving throughput
		//    * We can handle multiple base decorators by checking the decorator index found which has comparable cost
		//    * We could store the interface offset from the decorator ptr base to avoid the call to GetInterface()
		//      Instead of testing to find our interface again to return a custom static_cast, we'd store the interface/decorator offset
		//      in the node template alongside the decorator index. We can then query for the FDecorator* and add the offset
		//      improving bulk query feasibility.
		//
		// However, querying for a Super interface might be slower since we have to start the search from the start.
		// In contrast, we now start searching at the current decorator. In order to keep that cost down and amortize
		// the search, we would have to cache the InterfaceIndex and the current index in the decorator list.
		// We could use 8-bits for each entry allowing for a max of 256 interfaces per node and 256 decorators per node (existing limitation).
		//
		// Adding new data to the decorator binding isn't ideal. It's current size on 64-bit systems is:
		//    * 8 bytes for interface pointer (to forward function calls to)
		//    * 8 bytes for decorator template (to get the offsets for shared/instance/latent data)
		//    * 8 bytes for node description pointer (base of shared data)
		//    * 8 bytes for node instance pointer (base of instance data, in weak decorator ptr)
		//    * 4 bytes for decorator index (in weak decorator ptr)
		//    * 4 bytes of padding (in weak decorator ptr)
		// We current use 4 bytes for the decorator index but in practice we only need 1 byte. We use 4 bytes because we pay
		// for padding anyway. We could use 2 extra bytes for our mapping and still have 5 bytes to space in padding (due to alignment).
		// In practice, decorator bindings always live on the stack and they shouldn't be getting copied/moved around.
		// We populate them in place when we query, then we use it from its storage on the stack that we first populated.
		//
		// To be able to support all of this, instead of having FDecorator::GetInterface() implemented by derived types
		// we would need instead to implement FDecorator::GetInterfaceUIDs(). Then on load, when we build a node template and
		// finalize it, we can build the list of unique interface UIDs and sort it and we can build the mapping structures.
		// On load, as long as we fit within 64KB we are fine. We have to update FNodeTemplate::Finalize and FNodeTemplateBuilder::BuildNodeTemplate.
		// Perhaps the builder should use the same code and build in a buffer on the stack and copy the final size out.
		// We must build the mapping on load because the interfaces we implement are only known at runtime (e.g. can be changed through defines/configuration).
		// We can still implement GetInterface() in terms of GetInterfaceUIDs if it returns the interface offsets as well.

		FNodeInstance* NodeInstance = DecoratorPtr.GetNodeInstance();

		if (!ensure(IsBoundTo(NodeInstance->GetOwner())))
		{
			return false;	// The execution context isn't bound to the right graph instance
		}

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
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
			if (Decorator == nullptr)
			{
				continue;	// Decorator hasn't been loaded or registered, skip it
			}

			if (const IDecoratorInterface* Interface = Decorator->GetDecoratorInterface(InterfaceUID))
			{
				const FAnimNextDecoratorSharedData* SharedData = DecoratorDesc->GetDecoratorDescription(NodeDesc);
				FDecoratorInstanceData* InstanceData = DecoratorDesc->GetDecoratorInstance(*NodeInstance);

				const uint32 DecoratorIndex = DecoratorDesc - DecoratorDescs;
				FWeakDecoratorPtr InterfaceDecoratorPtr(NodeInstance, DecoratorIndex);

				InterfaceBinding = FDecoratorBinding(Interface, DecoratorDesc, &NodeDesc, InterfaceDecoratorPtr);
				return true;
			}
		}

		// We failed to find a decorator that handles this interface
		return false;
	}

	bool FExecutionContext::GetInterfaceSuperImpl(FDecoratorInterfaceUID InterfaceUID, const FWeakDecoratorPtr& DecoratorPtr, FDecoratorBinding& SuperBinding) const
	{
		if (!DecoratorPtr.IsValid())
		{
			return false;	// Decorator pointer isn't valid, can't find a 'super'
		}

		FNodeInstance* NodeInstance = DecoratorPtr.GetNodeInstance();

		if (!ensure(IsBoundTo(NodeInstance->GetOwner())))
		{
			return false;	// The execution context isn't bound to the right graph instance
		}

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
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
			if (Decorator == nullptr)
			{
				continue;	// Decorator hasn't been loaded or registered, skip it
			}

			if (const IDecoratorInterface* Interface = Decorator->GetDecoratorInterface(InterfaceUID))
			{
				const FAnimNextDecoratorSharedData* SharedData = DecoratorDesc->GetDecoratorDescription(NodeDesc);
				FDecoratorInstanceData* InstanceData = DecoratorDesc->GetDecoratorInstance(*NodeInstance);

				const uint32 DecoratorIndex = DecoratorDesc - DecoratorDescs;
				FWeakDecoratorPtr SuperPtr(NodeInstance, DecoratorIndex);

				SuperBinding = FDecoratorBinding(Interface, DecoratorDesc, &NodeDesc, SuperPtr);
				return true;
			}
		}

		// We failed to find a decorator that handles this interface
		return false;
	}

	void FExecutionContext::SnapshotLatentProperties(const FWeakDecoratorPtr& DecoratorPtr, bool bIsFrozen) const
	{
		if (!DecoratorPtr.IsValid())
		{
			return;	// Nothing to do
		}

		FNodeInstance* NodeInstance = DecoratorPtr.GetNodeInstance();

		if (!ensure(IsBoundTo(NodeInstance->GetOwner())))
		{
			return;	// The execution context isn't bound to the right graph instance
		}

		const FNodeDescription& NodeDesc = GetNodeDescription(NodeInstance->GetNodeHandle());

		const FNodeTemplate* NodeTemplate = GetNodeTemplate(NodeDesc);
		if (!ensure(NodeTemplate != nullptr))
		{
			return;	// Node template wasn't found, node descriptor is perhaps corrupted
		}

		const FDecoratorTemplate* DecoratorDescs = NodeTemplate->GetDecorators();

		// We only snapshot the partial stack the specified decorator lives in
		const FDecoratorTemplate* CurrentDecoratorDesc = DecoratorDescs + DecoratorPtr.GetDecoratorIndex();
		const FDecoratorTemplate* BaseDecoratorDesc = CurrentDecoratorDesc->GetMode() == EDecoratorMode::Base ? CurrentDecoratorDesc : (CurrentDecoratorDesc - CurrentDecoratorDesc->GetAdditiveDecoratorIndex());

		const FLatentPropertiesHeader& LatentHeader = BaseDecoratorDesc->GetDecoratorLatentPropertiesHeader(NodeDesc);
		if (!LatentHeader.bHasValidLatentProperties)
		{
			return;	// All latent properties are inline, nothing to snapshot
		}
		else if (bIsFrozen && LatentHeader.bCanAllPropertiesFreeze)
		{
			return;	// We are frozen and all latent properties support freezing, nothing to snapshot
		}

		const FLatentPropertyHandle* LatentHandles = BaseDecoratorDesc->GetDecoratorLatentPropertyHandles(NodeDesc);
		const uint32 NumLatentHandles = BaseDecoratorDesc->GetNumSubStackLatentPropreties();

		GraphInstance->ExecuteLatentPins(TConstArrayView<FLatentPropertyHandle>(LatentHandles, NumLatentHandles), NodeInstance, bIsFrozen);
	}

	FGraphInstanceComponent* FExecutionContext::TryGetComponent(int32 ComponentNameHash, FName ComponentName) const
	{
		return GraphInstance->TryGetComponent(ComponentNameHash, ComponentName);
	}

	FGraphInstanceComponent& FExecutionContext::AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<FGraphInstanceComponent>&& Component) const
	{
		return GraphInstance->AddComponent(ComponentNameHash, ComponentName, MoveTemp(Component));
	}

	GraphInstanceComponentMapType::TConstIterator FExecutionContext::GetComponentIterator() const
	{
		return GraphInstance->GetComponentIterator();
	}

	const FNodeDescription& FExecutionContext::GetNodeDescription(FNodeHandle NodeHandle) const
	{
		check(NodeHandle.IsValid());
		return *reinterpret_cast<const FNodeDescription*>(&GraphSharedData[NodeHandle.GetSharedOffset()]);
	}

	const FNodeTemplate* FExecutionContext::GetNodeTemplate(const FNodeDescription& NodeDesc) const
	{
		check(NodeDesc.GetTemplateHandle().IsValid());
		return NodeTemplateRegistry.Find(NodeDesc.GetTemplateHandle());
	}

	const FDecorator* FExecutionContext::GetDecorator(const FDecoratorTemplate& Template) const
	{
		check(Template.GetRegistryHandle().IsValid());
		return DecoratorRegistry.Find(Template.GetRegistryHandle());
	}
}
