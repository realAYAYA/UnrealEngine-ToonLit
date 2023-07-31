// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceRawBuffer.h"

#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "OptimusDeformerInstance.h"
#include "OptimusExpressionEvaluator.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalRenderPublic.h"
#include "ComponentSources/OptimusSkinnedMeshComponentSource.h"


const UOptimusComponentSource* UOptimusRawBufferDataInterface::GetComponentSource() const
{
	if (const UOptimusComponentSourceBinding* ComponentSourceBindingPtr = ComponentSourceBinding.Get())
	{
		return ComponentSourceBindingPtr->GetComponentSource();
	}
	return nullptr;
}

bool UOptimusRawBufferDataInterface::SupportsAtomics() const
{
	return ValueType->Type == EShaderFundamentalType::Int;
}

FString UOptimusRawBufferDataInterface::GetRawType() const
{
	// Currently the only case for using a raw typed buffer is for vectors with size 3.
	// We _may_ also need something for user structures if we they don't obey alignment restrictions.
	if (ValueType->DimensionType == EShaderFundamentalDimensionType::Vector && ValueType->VectorElemCount == 3)
	{
		return FShaderValueType::Get(ValueType->Type)->ToString();
	}
	return {};
}

int32 UOptimusRawBufferDataInterface::GetRawStride() const
{
	// Currently the only case for using a raw typed buffer is for vectors with size 3.
	// We _may_ also need something for user structures if we they don't obey alignment restrictions.
	if (ValueType->DimensionType == EShaderFundamentalDimensionType::Vector && ValueType->VectorElemCount == 3)
	{
		return 4;
	}
	return 0;
}

TArray<FOptimusCDIPinDefinition> UOptimusRawBufferDataInterface::GetPinDefinitions() const
{
	// FIXME: Multi-level support by proxying through a data interface.
	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({"ValueIn", "ReadValue", DataDomain.DimensionNames[0], "ReadNumValues"});
	Defs.Add({"ValueOut", "WriteValue", DataDomain.DimensionNames[0], "ReadNumValues"});
	return Defs;
}

TSubclassOf<UActorComponent> UOptimusRawBufferDataInterface::GetRequiredComponentClass() const
{
	return USceneComponent::StaticClass();
}

void UOptimusRawBufferDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumValues"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadValue"))
		.AddReturnType(ValueType)
		.AddParam(EShaderFundamentalType::Uint);
}

void UOptimusRawBufferDataInterface::GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	// Functions in order of EOptimusBufferWriteType
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteValue"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(ValueType);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteAtomicAdd"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(ValueType);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteAtomicMin"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(ValueType);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteAtomicMax"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(ValueType);
}

int32 UOptimusRawBufferDataInterface::GetReadValueInputIndex()
{
	return 1;
}

int32 UOptimusRawBufferDataInterface::GetWriteValueOutputIndex(EOptimusBufferWriteType WriteType)
{
	return (int32)WriteType;
}

BEGIN_SHADER_PARAMETER_STRUCT(FTransientBufferDataInterfaceParameters, )
	SHADER_PARAMETER(uint32, StartOffset)
	SHADER_PARAMETER(uint32, BufferSize)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<int>, BufferSRV)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int>, BufferUAV)
END_SHADER_PARAMETER_STRUCT()

void UOptimusTransientBufferDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FTransientBufferDataInterfaceParameters>(UID);
}

BEGIN_SHADER_PARAMETER_STRUCT(FPersistentBufferDataInterfaceParameters, )
	SHADER_PARAMETER(uint32, StartOffset)
	SHADER_PARAMETER(uint32, BufferSize)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<int>, BufferUAV)
END_SHADER_PARAMETER_STRUCT()

void UOptimusPersistentBufferDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FPersistentBufferDataInterfaceParameters>(UID);
}

void UOptimusRawBufferDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TEXT("/Plugin/Optimus/Private/DataInterfaceRawBuffer.ush"), EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusRawBufferDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	const FString PublicType = ValueType->ToString();
	const FString RawType = GetRawType();
	const bool bUseRawType = !RawType.IsEmpty();

	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
		{ TEXT("PublicType"), PublicType },
		{ TEXT("BufferType"), bUseRawType ? RawType : PublicType },
		{ TEXT("BufferTypeRaw"), bUseRawType },
		{ TEXT("SupportAtomic"), SupportsAtomics() ? 1 : 0 },
		{ TEXT("SplitReadWrite"), UseSplitBuffers() ? 1 : 0 },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TEXT("/Plugin/Optimus/Private/DataInterfaceRawBuffer.ush"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

FString UOptimusTransientBufferDataInterface::GetDisplayName() const
{
	return TEXT("Transient");
}

UComputeDataProvider* UOptimusTransientBufferDataInterface::CreateDataProvider(
	TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask
	) const
{
	UOptimusTransientBufferDataProvider *Provider = CreateProvider<UOptimusTransientBufferDataProvider>(InBinding);
	Provider->ElementStride = ValueType->GetResourceElementSize();
	Provider->RawStride = GetRawStride();
	return Provider;
}


FString UOptimusPersistentBufferDataInterface::GetDisplayName() const
{
	return TEXT("Persistent");
}


UComputeDataProvider* UOptimusPersistentBufferDataInterface::CreateDataProvider(
	TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask
	) const
{
	UOptimusPersistentBufferDataProvider *Provider = CreateProvider<UOptimusPersistentBufferDataProvider>(InBinding);
	
	Provider->ResourceName = ResourceName;

	return Provider;
}


bool UOptimusRawBufferDataProvider::IsValid() const
{
	if (!Component.IsValid())
	{
		return false;
	}
	
	const UOptimusComponentSource* ComponentSourcePtr = ComponentSource.Get();
	if (!ComponentSourcePtr)
	{
		return false;
	}

	switch(DataDomain.Type)
	{
	case EOptimusDataDomainType::Dimensional:
		{
			if (DataDomain.DimensionNames.IsEmpty())
			{
				return false;
			}
			return ComponentSourcePtr->GetExecutionDomains().Contains(DataDomain.DimensionNames[0]); 
		}
	case EOptimusDataDomainType::Expression:
		{
			using namespace Optimus::Expression;
			TMap<FName, int32> DomainNames;

			for (FName DomainName: ComponentSourcePtr->GetExecutionDomains())
			{
				DomainNames.Add(DomainName, 0);
			}

			// Return true if there was no error. Verify returns a TOptional.
			return !FEngine{DomainNames}.Verify(DataDomain.Expression).IsSet();
		}
	}

	return false;
}


bool UOptimusRawBufferDataProvider::GetLodAndInvocationElementCounts(
	int32& OutLodIndex,
	TArray<int32>& OutInvocationElementCounts
	) const
{
	const UOptimusComponentSource* ComponentSourcePtr = ComponentSource.Get();
	if (!ComponentSourcePtr)
	{
		return false;
	}

	const UActorComponent* ComponentPtr = Component.Get();
	if (!ComponentPtr)
	{
		return false;
	}

	OutLodIndex = ComponentSourcePtr->GetLodIndex(ComponentPtr);

	switch(DataDomain.Type)
	{
	case EOptimusDataDomainType::Dimensional:
		{
			const FName ExecutionDomain = !DataDomain.DimensionNames.IsEmpty() ? DataDomain.DimensionNames[0] : NAME_None;
			if (ComponentSourcePtr->GetComponentElementCountsForExecutionDomain(ExecutionDomain, ComponentPtr, OutLodIndex, OutInvocationElementCounts))
			{
				if (DataDomain.DimensionNames.Num() == 1)
				{
					for (int32& Count: OutInvocationElementCounts)
					{
						Count *= FMath::Max(1, DataDomain.Multiplier);
					}
				}
				return true;
			}
			break;
		}
	case EOptimusDataDomainType::Expression:
		{
			TMap<FName, int32> EngineConstants;
			TMap<FName, TArray<int32>> ElementCountsPerDomain;
			int32 NumInvocations = 0;

			for(FName ExecutionDomain: ComponentSource->GetExecutionDomains())
			{
				EngineConstants.Add(ExecutionDomain, 0);

				TArray<int32>& ElementCounts = ElementCountsPerDomain.FindOrAdd(ExecutionDomain);
				if (!ComponentSourcePtr->GetComponentElementCountsForExecutionDomain(ExecutionDomain, ComponentPtr, OutLodIndex, ElementCounts))
				{
					return false;
				}

				NumInvocations = FMath::Max(NumInvocations, ElementCounts.Num());
			}
			
			using namespace Optimus::Expression;

			FEngine Engine(EngineConstants);
			TVariant<FExpressionObject, FParseError> ParseResult = Engine.Parse(DataDomain.Expression);
			if (ParseResult.IsType<FParseError>())
			{
				return false;
			}

			OutInvocationElementCounts.Reset(NumInvocations);
			for (int32 Index = 0; Index < NumInvocations; Index++)
			{
				for (auto& [ConstantName, Value]: EngineConstants)
				{
					const TArray<int32>& ElementCounts = ElementCountsPerDomain[ConstantName]; 
					Value = ElementCounts.IsValidIndex(Index) ? ElementCounts[Index] : 1;
				}
				Engine.UpdateConstantValues(EngineConstants);
				const int32 Count = Engine.Execute(ParseResult.Get<FExpressionObject>());
				OutInvocationElementCounts.Add(Count);
			}
			return true;
		}
	}
	return false;
}


FComputeDataProviderRenderProxy* UOptimusTransientBufferDataProvider::GetRenderProxy()
{
	int32 LodIndex;
	TArray<int32> InvocationCounts;
	GetLodAndInvocationElementCounts(LodIndex, InvocationCounts);
	
	return new FOptimusTransientBufferDataProviderProxy(InvocationCounts, ElementStride, RawStride);
}


bool UOptimusPersistentBufferDataProvider::IsValid() const
{
	if (!BufferPool.IsValid())
	{
		return false;
	}

	return UOptimusRawBufferDataProvider::IsValid();
}


FComputeDataProviderRenderProxy* UOptimusPersistentBufferDataProvider::GetRenderProxy()
{
	int32 LodIndex = 0;
	TArray<int32> InvocationCounts;
	GetLodAndInvocationElementCounts(LodIndex, InvocationCounts);
	
	return new FOptimusPersistentBufferDataProviderProxy(InvocationCounts, ElementStride, RawStride, BufferPool, ResourceName, LodIndex);
}


FOptimusTransientBufferDataProviderProxy::FOptimusTransientBufferDataProviderProxy(
	TArray<int32> InInvocationElementCounts,
	int32 InElementStride,
	int32 InRawStride
	) :
	InvocationElementCounts(InInvocationElementCounts),
	ElementStride(InElementStride),
	RawStride(InRawStride)
{
}


void FOptimusTransientBufferDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
{
	// If we are using a raw type alias for the buffer then we need to adjust stride and count.
	check(RawStride == 0 || ElementStride % RawStride == 0);
	const int32 Stride = RawStride ? RawStride : ElementStride;
	const int32 ElementStrideMultiplier = RawStride ? ElementStride / RawStride : 1;

	for (const int32 NumElements: InvocationElementCounts)
	{
		Buffer.Add(GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc(Stride, NumElements * ElementStrideMultiplier), TEXT("TransientBuffer"), ERDGBufferFlags::None));
		BufferSRV.Add(GraphBuilder.CreateSRV(Buffer.Last()));
		BufferUAV.Add(GraphBuilder.CreateUAV(Buffer.Last()));
	}
}

void FOptimusTransientBufferDataProviderProxy::GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData)
{
	if (!ensure(InDispatchSetup.ParameterStructSizeForValidation == sizeof(FTransientBufferDataInterfaceParameters)))
	{
		return;
	}

	for (int32 InvocationIndex = 0; InvocationIndex < InvocationElementCounts.Num(); ++InvocationIndex)
	{
		FTransientBufferDataInterfaceParameters* Parameters =
			reinterpret_cast<FTransientBufferDataInterfaceParameters*>(InOutDispatchData.ParameterBuffer + InDispatchSetup.ParameterBufferOffset + InDispatchSetup.ParameterBufferStride * InvocationIndex);
		Parameters->StartOffset = 0;
		Parameters->BufferSize = InvocationElementCounts[InvocationIndex];
		Parameters->BufferSRV = BufferSRV[InvocationIndex];
		Parameters->BufferUAV = BufferUAV[InvocationIndex];
	}
}



FOptimusPersistentBufferDataProviderProxy::FOptimusPersistentBufferDataProviderProxy(
	TArray<int32> InInvocationElementCounts,
	int32 InElementStride,
	int32 InRawStride,
	TSharedPtr<FOptimusPersistentBufferPool> InBufferPool,
	FName InResourceName,
	int32 InLODIndex
	) :
	InvocationElementCounts(InInvocationElementCounts),
	ElementStride(InElementStride),
	RawStride(InRawStride),
	BufferPool(InBufferPool),
	ResourceName(InResourceName),
	LODIndex(InLODIndex)
{
}


void FOptimusPersistentBufferDataProviderProxy::AllocateResources(
	FRDGBuilder& GraphBuilder
	)
{
	BufferPool->GetResourceBuffers(GraphBuilder, ResourceName, LODIndex, ElementStride, RawStride, InvocationElementCounts, Buffers);
	BufferUAVs.Reserve(Buffers.Num());
	for (FRDGBufferRef BufferRef : Buffers)
	{
		BufferUAVs.Add(GraphBuilder.CreateUAV(BufferRef));
	}
}


void FOptimusPersistentBufferDataProviderProxy::GatherDispatchData(
	FDispatchSetup const& InDispatchSetup,
	FCollectedDispatchData& InOutDispatchData
	)
{
	if (!ensure(InDispatchSetup.ParameterStructSizeForValidation == sizeof(FPersistentBufferDataInterfaceParameters)))
	{
		return;
	}

	if (!ensure(Buffers.Num() == InvocationElementCounts.Num()))
	{
		return;
	}
	
	for (int32 InvocationIndex = 0; InvocationIndex < InvocationElementCounts.Num(); ++InvocationIndex)
	{
		FPersistentBufferDataInterfaceParameters* Parameters =
			reinterpret_cast<FPersistentBufferDataInterfaceParameters*>(InOutDispatchData.ParameterBuffer + InDispatchSetup.ParameterBufferOffset + InDispatchSetup.ParameterBufferStride * InvocationIndex);
		
		Parameters->StartOffset = 0;
		Parameters->BufferSize = InvocationElementCounts[InvocationIndex];
		Parameters->BufferUAV = BufferUAVs[InvocationIndex];
	}

	FComputeDataProviderRenderProxy::GatherDispatchData(InDispatchSetup, InOutDispatchData);
}
