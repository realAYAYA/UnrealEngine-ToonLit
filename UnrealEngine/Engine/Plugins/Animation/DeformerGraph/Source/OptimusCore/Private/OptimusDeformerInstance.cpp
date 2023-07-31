// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDeformerInstance.h"

#include "Components/MeshComponent.h"
#include "DataInterfaces/OptimusDataInterfaceGraph.h"
#include "OptimusComputeGraph.h"
#include "OptimusDataTypeRegistry.h"
#include "OptimusDeformer.h"
#include "OptimusVariableDescription.h"
#include "RenderGraphBuilder.h"
#include "SkeletalRenderPublic.h"
#include "DataInterfaces/OptimusDataInterfaceRawBuffer.h"


/** Container for a pooled buffer. */
struct FOptimusPersistentStructuredBuffer
{
	TRefCountPtr<FRDGPooledBuffer> PooledBuffer;
	int32 ElementStride = 0;
	int32 ElementCount = 0;
};


void FOptimusPersistentBufferPool::GetResourceBuffers(
	FRDGBuilder& GraphBuilder,
	FName InResourceName,
	int32 InLODIndex,
	int32 InElementStride,
	int32 InRawStride,
	TArray<int32> const& InElementCounts,
	TArray<FRDGBufferRef>& OutBuffers)
{
	OutBuffers.Reset();

	TMap<int32, TArray<FOptimusPersistentStructuredBuffer>>& LODResources = ResourceBuffersMap.FindOrAdd(InResourceName);  
	TArray<FOptimusPersistentStructuredBuffer>* ResourceBuffersPtr = LODResources.Find(InLODIndex);
	if (ResourceBuffersPtr == nullptr)
	{
		// Create pooled buffers and store.
		TArray<FOptimusPersistentStructuredBuffer> ResourceBuffers;
		ResourceBuffers.Reserve(InElementCounts.Num());

		// If we are using a raw type alias for the buffer then we need to adjust stride and count.
		check(InRawStride == 0 || InElementStride % InRawStride == 0);
		const int32 Stride = InRawStride ? InRawStride : InElementStride;
		const int32 ElementStrideMultiplier = InRawStride ? InElementStride / InRawStride : 1;

		for (int32 Index = 0; Index < InElementCounts.Num(); Index++)
		{
			FRDGBufferDesc BufferDesc = FRDGBufferDesc::CreateStructuredDesc(Stride, InElementCounts[Index] * ElementStrideMultiplier);
			FRDGBufferRef Buffer = GraphBuilder.CreateBuffer(BufferDesc, TEXT("FOptimusPersistentBuffer"), ERDGBufferFlags::None);
			OutBuffers.Add(Buffer);

			FOptimusPersistentStructuredBuffer& PersistentBuffer = ResourceBuffers.AddDefaulted_GetRef();
			PersistentBuffer.ElementStride = InElementStride;
			PersistentBuffer.ElementCount = InElementCounts[Index];
			PersistentBuffer.PooledBuffer = GraphBuilder.ConvertToExternalBuffer(Buffer);
		}

		LODResources.Add(InLODIndex, MoveTemp(ResourceBuffers));
	}
	else
	{
		// Verify that the buffers are correct based on the incoming information. 
		// If there's a mismatch, then something has gone wrong upstream.
		// Maybe either duplicated names, missing resource clearing on recompile, or something else.
		if (!ensure(ResourceBuffersPtr->Num() == InElementCounts.Num()))
		{
			return;
		}

		for (int32 Index = 0; Index < ResourceBuffersPtr->Num(); Index++)
		{
			FOptimusPersistentStructuredBuffer& PersistentBuffer = (*ResourceBuffersPtr)[Index];
			if (!ensure(PersistentBuffer.PooledBuffer.IsValid()) ||
				!ensure(PersistentBuffer.ElementStride == InElementStride) ||
				!ensure(PersistentBuffer.ElementCount == InElementCounts[Index]))
			{
				OutBuffers.Reset();
				return;
			}	

			// Register buffer back into the graph and return it.
			FRDGBufferRef Buffer = GraphBuilder.RegisterExternalBuffer(PersistentBuffer.PooledBuffer);
			OutBuffers.Add(Buffer);
		}
	}
}

void FOptimusPersistentBufferPool::ReleaseResources()
{
	check(IsInRenderingThread());
	ResourceBuffersMap.Reset();
}

FOptimusDeformerInstanceExecInfo::FOptimusDeformerInstanceExecInfo()
{
	GraphType = EOptimusNodeGraphType::Update;
}


bool FOptimusDeformerInstanceComponentBinding::GetSanitizedComponentName(FString& InOutName)
{
	// Remove suffix for blueprint spawned components.
	return InOutName.RemoveFromEnd(TEXT("_GEN_VARIABLE"));
}

FName FOptimusDeformerInstanceComponentBinding::GetSanitizedComponentName(FName InName)
{
	FString Name = InName.ToString();
	if (GetSanitizedComponentName(Name))
	{
		return FName(Name);
	}
	// No change.
	return InName;
}

FName FOptimusDeformerInstanceComponentBinding::GetSanitizedComponentName(UActorComponent const* InComponent)
{
	return InComponent ? GetSanitizedComponentName(InComponent->GetFName()) : FName();
}

TSoftObjectPtr<UActorComponent> FOptimusDeformerInstanceComponentBinding::GetActorComponent(AActor const* InActor, FString const& InName)
{
	if (InActor != nullptr && !InName.IsEmpty())
	{
		FString Path = InActor->GetPathName() + TEXT(".") + InName;
		return TSoftObjectPtr<UActorComponent>(FSoftObjectPath(Path));
	}
	return {};
}

TSoftObjectPtr<UActorComponent> FOptimusDeformerInstanceComponentBinding::GetActorComponent(AActor const* InActor) const
{
	return GetActorComponent(InActor, ComponentName.ToString());
}


void UOptimusDeformerInstanceSettings::InitializeSettings(UOptimusDeformer* InDeformer, UMeshComponent* InPrimaryComponent)
{
	Deformer = InDeformer;

	Bindings.SetNum(InDeformer->GetComponentBindings().Num());
	for (int32 BindingIndex = 0; BindingIndex < Bindings.Num(); ++BindingIndex)
	{
		Bindings[BindingIndex].ProviderName = InDeformer->GetComponentBindings()[BindingIndex]->BindingName;
		if (BindingIndex == 0)
		{
			Bindings[BindingIndex].ComponentName = FOptimusDeformerInstanceComponentBinding::GetSanitizedComponentName(InPrimaryComponent);
		}
	}
}

void UOptimusDeformerInstanceSettings::GetComponentBindings(
	UOptimusDeformer* InDeformer, 
	UMeshComponent* InPrimaryComponent, 
	TArray<UActorComponent*>& OutComponents) const
{
	AActor const* Actor = InPrimaryComponent != nullptr ? InPrimaryComponent->GetOwner() : nullptr;

	// Try to map onto the configured component bindings as much as possible.
	TMap<FName, UActorComponent*> ExistingBindings;

	for (FOptimusDeformerInstanceComponentBinding const& Binding : Bindings)
	{
		TSoftObjectPtr<UActorComponent> ActorComponent = Binding.GetActorComponent(Actor);
		UActorComponent* Component = ActorComponent.Get();
		ExistingBindings.Add(Binding.ProviderName, Component);
	}
	
	// Iterate component bindings and try to find a match.
	TSet<UActorComponent*> ComponentsUsed;
	const TArray<UOptimusComponentSourceBinding*>& ComponentBindings = InDeformer->GetComponentBindings();
	OutComponents.Reset(ComponentBindings.Num());
	for (const UOptimusComponentSourceBinding* Binding : ComponentBindings)
	{
		FName BindingName = Binding->BindingName;
		UActorComponent* BoundComponent = nullptr;

		// Primary binding always binds to the mesh component we're applied to.
		if (Binding->IsPrimaryBinding())
		{
			BoundComponent = InPrimaryComponent;
		}
		else
		{
			// Try an existing binding first and see if they still match by class. We ignore tags for this match
			// because we want to respect the will of the user, unless absolutely not possible (i.e. class mismatch).
			if (ExistingBindings.Contains(BindingName))
			{
				if (UActorComponent* Component = ExistingBindings[BindingName])
				{
					if (Component->IsA(Binding->GetComponentSource()->GetComponentClass()))
					{
						BoundComponent = Component;
					}
				}
			}
			
			// If not, try to find a component owned by this actor that matches the tag and class.
			if (!BoundComponent && Actor != nullptr && !Binding->ComponentTags.IsEmpty())
			{
				TSet<UActorComponent*> TaggedComponents;
				for (FName Tag: Binding->ComponentTags)
				{
					TArray<UActorComponent*> Components = Actor->GetComponentsByTag(Binding->GetComponentSource()->GetComponentClass(), Tag);

					for (UActorComponent* Component: Components)
					{
						if (!ComponentsUsed.Contains(Component))
						{
							TaggedComponents.Add(Component);
						}
					}
				}
				TArray<UActorComponent*> RankedTaggedComponents = TaggedComponents.Array();

				// Rank the components by the number of tags they match.
				RankedTaggedComponents.Sort([Tags=TSet<FName>(Binding->ComponentTags)](const UActorComponent& InCompA, const UActorComponent& InCompB)
				{
					TSet<FName> TagsA(InCompA.ComponentTags);
					TSet<FName> TagsB(InCompB.ComponentTags);
					
					return Tags.Intersect(TagsA).Num() < Tags.Intersect(TagsB).Num();
				});

				if (!RankedTaggedComponents.IsEmpty())
				{
					BoundComponent = RankedTaggedComponents[0];
				}
			}

			// Otherwise just use class matching on components owned by the actor.
			if (!BoundComponent && Actor != nullptr)
			{
				TArray<UActorComponent*> Components;
				Actor->GetComponents(Binding->GetComponentSource()->GetComponentClass(), Components);

				for (UActorComponent* Component: Components)
				{
					if (!ComponentsUsed.Contains(Component))
					{
						BoundComponent = Component;
						break;
					}
				}
			}
		}

		OutComponents.Add(BoundComponent);
		ComponentsUsed.Add(BoundComponent);
	}
}

AActor* UOptimusDeformerInstanceSettings::GetActor() const
{
	// We should be owned by an actor at some point.
	return GetTypedOuter<AActor>();
}

UOptimusComponentSourceBinding const* UOptimusDeformerInstanceSettings::GetComponentBindingByName(FName InBindingName) const
{
	if (const UOptimusDeformer* DeformerResolved = Deformer.Get())
	{
		for (UOptimusComponentSourceBinding* Binding: DeformerResolved->GetComponentBindings())
		{
			if (Binding->BindingName == InBindingName)
			{
				return Binding;
			}
		}
	}
	return nullptr;
}


void UOptimusDeformerInstance::SetMeshComponent(UMeshComponent* InMeshComponent)
{ 
	MeshComponent = InMeshComponent;
}

void UOptimusDeformerInstance::SetInstanceSettings(UOptimusDeformerInstanceSettings* InInstanceSettings)
{
	InstanceSettings = InInstanceSettings; 
}


void UOptimusDeformerInstance::SetupFromDeformer(UOptimusDeformer* InDeformer)
{
	// If we're doing a recompile, ditch all stored render resources.
	ReleaseResources();

	// Update the component bindings before creating data providers. 
	// The bindings are in the same order as the component bindings in the deformer.
	TArray<UActorComponent*> BoundComponents;
	UOptimusDeformerInstanceSettings* InstanceSettingsPtr = InstanceSettings.Get(); 
	if (InstanceSettingsPtr == nullptr)
	{
		// If we don't have any settings, then create a temporary object to get bindings.
		InstanceSettingsPtr = NewObject<UOptimusDeformerInstanceSettings>();
		InstanceSettingsPtr->InitializeSettings(InDeformer, MeshComponent.Get());
	}
	InstanceSettingsPtr->GetComponentBindings(InDeformer, MeshComponent.Get(), BoundComponents);

	// Create the persistent buffer pool
	BufferPool = MakeShared<FOptimusPersistentBufferPool>();
	
	// (Re)Create and bind data providers.
	ComputeGraphExecInfos.Reset();
	GraphsToRunOnNextTick.Reset();

	for (const FOptimusComputeGraphInfo& ComputeGraphInfo : InDeformer->ComputeGraphs)
	{
		FOptimusDeformerInstanceExecInfo& Info = ComputeGraphExecInfos.AddDefaulted_GetRef();
		Info.GraphName = ComputeGraphInfo.GraphName;
		Info.GraphType = ComputeGraphInfo.GraphType;
		Info.ComputeGraph = ComputeGraphInfo.ComputeGraph;

		if (BoundComponents.Num())
		{
			UMeshComponent* ActorComponent = MeshComponent.Get();
			AActor const* Actor = ActorComponent ? ActorComponent->GetOwner() : nullptr;
			for (int32 Index = 0; Index < BoundComponents.Num(); Index++)
			{
				Info.ComputeGraphInstance.CreateDataProviders(Info.ComputeGraph, Index, BoundComponents[Index]);
			}
		}
		else
		{
			// Fall back on everything being the given component.
			for (int32 Index = 0; Index < InDeformer->GetComponentBindings().Num(); Index++)
			{
				Info.ComputeGraphInstance.CreateDataProviders(Info.ComputeGraph, Index, MeshComponent.Get());
			}
		}

		int32 LODIndex = 0;
		if (const USkinnedMeshComponent* SkinnedMeshComponent = Cast<USkinnedMeshComponent>(MeshComponent))
		{
			// This guff should be a utility function on USkinnedMeshComponent. 
			LODIndex = SkinnedMeshComponent->GetPredictedLODLevel();
			
			if (SkinnedMeshComponent->GetSkinnedAsset() && SkinnedMeshComponent->GetSkinnedAsset()->IsStreamable() && SkinnedMeshComponent->MeshObject)
			{
				LODIndex = FMath::Max<int32>(LODIndex, SkinnedMeshComponent->MeshObject->GetSkeletalMeshRenderData().PendingFirstLODIdx);
			}
		}

		for(TObjectPtr<UComputeDataProvider> DataProvider: Info.ComputeGraphInstance.GetDataProviders())
		{
			// Make the persistent buffer data provider aware of the buffer pool and current LOD index.
			// TBD: Interface-based.
			if (UOptimusPersistentBufferDataProvider* PersistentBufferProvider = Cast<UOptimusPersistentBufferDataProvider>(DataProvider))
			{
				PersistentBufferProvider->BufferPool = BufferPool;
			}
		}

		// Schedule the setup graph to run.
		if (Info.GraphType == EOptimusNodeGraphType::Setup)
		{
			GraphsToRunOnNextTick.Add(Info.GraphName);
		}
	}
	
	// Create local storage for deformer graph variables.
	Variables = NewObject<UOptimusVariableContainer>(this);
	Variables->Descriptions.Reserve(InDeformer->GetVariables().Num());
	TSet<const UOptimusVariableDescription*> Visited;
	for (const UOptimusVariableDescription* VariableDescription : InDeformer->GetVariables())
	{
		if (!VariableDescription)
		{
			continue;
		}
		if (Visited.Contains(VariableDescription))
		{
			continue;
		}
		Visited.Add(VariableDescription);
		
		UOptimusVariableDescription* VariableDescriptionCopy = NewObject<UOptimusVariableDescription>();
		VariableDescriptionCopy->Guid = VariableDescription->Guid;
		VariableDescriptionCopy->VariableName = VariableDescription->VariableName;
		VariableDescriptionCopy->DataType = VariableDescription->DataType;
		VariableDescriptionCopy->DefaultValue = nullptr; // No need to copy the default value.
		VariableDescriptionCopy->ValueData = VariableDescription->ValueData;
		Variables->Descriptions.Add(VariableDescriptionCopy);
	}

	if (UMeshComponent* Ptr = MeshComponent.Get())
	{
		Ptr->MarkRenderDynamicDataDirty();
	}
}


void UOptimusDeformerInstance::SetCanBeActive(bool bInCanBeActive)
{
	bCanBeActive = bInCanBeActive;

}

void UOptimusDeformerInstance::AllocateResources()
{
	
}


void UOptimusDeformerInstance::ReleaseResources()
{
	if (BufferPool)
	{
		ENQUEUE_RENDER_COMMAND(FOptimusReleasePoolMemory)(
			[BufferPool=MoveTemp(BufferPool)](FRHICommandListImmediate& InCmdList)
			{
				BufferPool->ReleaseResources();
			});
	}
}


bool UOptimusDeformerInstance::IsActive() const
{
	if (!bCanBeActive)
	{
		return false;
	}
		
	for (const FOptimusDeformerInstanceExecInfo& Info: ComputeGraphExecInfos)
	{
		if (!Info.ComputeGraphInstance.ValidateDataProviders(Info.ComputeGraph))
		{
			return false;
		}
	}
	return !ComputeGraphExecInfos.IsEmpty();
}

void UOptimusDeformerInstance::EnqueueWork(FSceneInterface* InScene, EWorkLoad InWorkLoadType, FName InOwnerName)
{
	TSet<FName> GraphsToRun;
	{
		UE::TScopeLock<FCriticalSection> Lock(GraphsToRunOnNextTickLock);
		Swap(GraphsToRunOnNextTick, GraphsToRun);
	}
	
	for (FOptimusDeformerInstanceExecInfo& Info: ComputeGraphExecInfos)
	{
		if (Info.GraphType == EOptimusNodeGraphType::Update || GraphsToRun.Contains(Info.GraphName))
		{
			Info.ComputeGraphInstance.EnqueueWork(Info.ComputeGraph, InScene, InOwnerName);
		}
	}
}

namespace
{
	template <typename T>
	bool SetVariableValue(UOptimusVariableContainer const* InVariables, FName InVariableName, FName InTypeName, T const& InValue)
	{
		const uint8* ValueBytes = reinterpret_cast<const uint8*>(&InValue);

		FOptimusDataTypeHandle WantedType = FOptimusDataTypeRegistry::Get().FindType(InTypeName);
		for (UOptimusVariableDescription* VariableDesc : InVariables->Descriptions)
		{
			if (VariableDesc->VariableName == InVariableName && VariableDesc->DataType == WantedType)
			{
				TUniquePtr<FProperty> Property(WantedType->CreateProperty(nullptr, NAME_None));
				if (ensure(Property->GetSize() == sizeof(T)))
				{
					FShaderValueType::FValue ValueResult = WantedType->MakeShaderValue();
					WantedType->ConvertPropertyValueToShader(TArrayView<const uint8>((const uint8*)&InValue, sizeof(T)), ValueResult);
					VariableDesc->ValueData = MoveTemp(ValueResult.ShaderValue);
				}

				return true;
			}
		}

		return false;
	}
}


bool UOptimusDeformerInstance::SetBoolVariable(FName InVariableName, bool InValue)
{
	return SetVariableValue(Variables, InVariableName, FBoolProperty::StaticClass()->GetFName(), InValue);
}

bool UOptimusDeformerInstance::SetIntVariable(FName InVariableName, int32 InValue)
{
	return SetVariableValue<int32>(Variables, InVariableName, FIntProperty::StaticClass()->GetFName(), InValue);
}

bool UOptimusDeformerInstance::SetFloatVariable(FName InVariableName, double InValue)
{
	if (SetVariableValue<double>(Variables, InVariableName, FDoubleProperty::StaticClass()->GetFName(), InValue))
	{
		return true;
	}
	
	// Fall back on float
	return SetVariableValue<float>(Variables, InVariableName, FFloatProperty::StaticClass()->GetFName(), static_cast<float>(InValue));
}

bool UOptimusDeformerInstance::SetVectorVariable(FName InVariableName, const FVector& InValue)
{
	return SetVariableValue<FVector>(Variables, InVariableName, "FVector", InValue);
}

bool UOptimusDeformerInstance::SetVector4Variable(FName InVariableName, const FVector4& InValue)
{
	return SetVariableValue<FVector4>(Variables, InVariableName, "FVector4", InValue);
}

const TArray<UOptimusVariableDescription*>& UOptimusDeformerInstance::GetVariables() const
{
	return Variables->Descriptions;
}


bool UOptimusDeformerInstance::EnqueueTriggerGraph(FName InTriggerGraphName)
{
	for(FOptimusDeformerInstanceExecInfo& ExecInfo: ComputeGraphExecInfos)
	{
		if (ExecInfo.GraphType == EOptimusNodeGraphType::ExternalTrigger && ExecInfo.GraphName == InTriggerGraphName)
		{
			UE::TScopeLock<FCriticalSection> Lock(GraphsToRunOnNextTickLock);
			GraphsToRunOnNextTick.Add(ExecInfo.GraphName);
			return true;
		}
	}
	
	return false;
}


void UOptimusDeformerInstance::SetConstantValueDirect(FString const& InVariableName, TArray<uint8> const& InValue)
{
	// Poke constants into the UGraphDataProvider objects.
	// This is an editor only operation when constant nodes are edited in the graph and we want to see the result without a full compile step.
	// Not sure that this is the best approach, or whether to store constants on the UOptimusDeformerInstance like variables?
	for (FOptimusDeformerInstanceExecInfo& ExecInfo : ComputeGraphExecInfos)
	{
		TArray< TObjectPtr<UComputeDataProvider> >& DataProviders = ExecInfo.ComputeGraphInstance.GetDataProviders();
		for (UComputeDataProvider* DataProvider : DataProviders)
		{
			if (UOptimusGraphDataProvider* GraphDataProvider = Cast<UOptimusGraphDataProvider>(DataProvider))
			{
				GraphDataProvider->SetConstant(InVariableName, InValue);
				break;
			}
		}
	}
}
