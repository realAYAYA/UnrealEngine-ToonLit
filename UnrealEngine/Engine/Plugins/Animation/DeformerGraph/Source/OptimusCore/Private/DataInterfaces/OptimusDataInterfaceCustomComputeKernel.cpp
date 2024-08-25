// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceCustomComputeKernel.h"

#include "OptimusComponentSource.h"
#include "OptimusDeformerInstance.h"
#include "OptimusExpressionEvaluator.h"
#include "ShaderParameterMetadataBuilder.h"
#include "ComputeFramework/ComputeMetadataBuilder.h"
#include "ComputeFramework/ShaderParameterMetadataAllocation.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"

#include "Nodes/OptimusNode_CustomComputeKernel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusDataInterfaceCustomComputeKernel)

const FString UOptimusCustomComputeKernelDataInterface::NumThreadsReservedName = TEXT("NumThreads");

void UOptimusCustomComputeKernelDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(FString::Printf(TEXT("Read%s"), *NumThreadsReservedName))
		.AddReturnType(FShaderValueType::Get(EShaderFundamentalType::Int, 3));
}

void UOptimusCustomComputeKernelDataInterface::GetShaderParameters(TCHAR const* UID,
	FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	FShaderParametersMetadataBuilder Builder;
	TArray<FShaderParametersMetadata*> DummyNestedStructs;
	ComputeFramework::AddParamForType(Builder, *NumThreadsReservedName, FShaderValueType::Get(EShaderFundamentalType::Int, 3),DummyNestedStructs);

	FShaderParametersMetadata* ShaderParameterMetadata = Builder.Build(FShaderParametersMetadata::EUseCase::ShaderParameterStruct, TEXT("UCustomComputeKernelDataInterface"));

	InOutAllocations.ShaderParameterMetadatas.Add(ShaderParameterMetadata);
	InOutAllocations.ShaderParameterMetadatas.Append(DummyNestedStructs);

	// Add the generated nested struct to our builder.
	InOutBuilder.AddNestedStruct(UID, ShaderParameterMetadata);

}

void UOptimusCustomComputeKernelDataInterface::GetShaderHash(FString& InOutKey) const
{
	// UComputeGraph::BuildKernelSource hashes the result of GetHLSL()
	// Only append additional hashes here if the HLSL contains any additional includes	
}

void UOptimusCustomComputeKernelDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	const FString TypeName = FShaderValueType::Get(EShaderFundamentalType::Int, 3)->ToString();

	if (ensure(!TypeName.IsEmpty()))
	{
		// Add uniforms.
		OutHLSL += FString::Printf(TEXT("%s %s_%s;\n"), 
			*TypeName, 
			*InDataInterfaceName, 
			*NumThreadsReservedName);
			
		// Add function getters.
		OutHLSL += FString::Printf(TEXT("%s Read%s_%s()\n{\n\treturn %s_%s;\n}\n"), 
			*TypeName,
			*NumThreadsReservedName,
			*InDataInterfaceName, 
			*InDataInterfaceName, 
			*NumThreadsReservedName);
	}
}

UComputeDataProvider* UOptimusCustomComputeKernelDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding,
	uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusCustomComputeKernelDataProvider* Provider = NewObject<UOptimusCustomComputeKernelDataProvider>();

	Provider->InitFromDataInterface(this, InBinding);

	return Provider;
}

void UOptimusCustomComputeKernelDataInterface::SetExecutionDomain(const FString& InExecutionDomain)
{
	NumThreadsExpression = InExecutionDomain;
}

void UOptimusCustomComputeKernelDataInterface::SetComponentBinding(const UOptimusComponentSourceBinding* InBinding)
{
	ComponentSourceBinding = InBinding;
}

void UOptimusCustomComputeKernelDataProvider::InitFromDataInterface(const UOptimusCustomComputeKernelDataInterface* InDataInterface, const UObject* InBinding)
{
	WeakComponent = Cast<UActorComponent>(InBinding);
	WeakDataInterface = InDataInterface;
}

FComputeDataProviderRenderProxy* UOptimusCustomComputeKernelDataProvider::GetRenderProxy()
{
	TArray<int32> InvocationCounts;
	int32 TotalThreadCount = 0;
	
	if (!GetInvocationThreadCounts(InvocationCounts, TotalThreadCount))
	{
		InvocationCounts.Reset();
	}
	
	FOptimusCustomComputeKernelDataProviderProxy* Proxy = new FOptimusCustomComputeKernelDataProviderProxy(MoveTemp(InvocationCounts), TotalThreadCount);
	return Proxy;
}

bool UOptimusCustomComputeKernelDataProvider::GetInvocationThreadCounts(
	TArray<int32>& OutInvocationThreadCount,
	int32& OutTotalThreadCount
	) const
{
	if (!WeakDataInterface.IsValid())
	{
		return false;
	}

	if (!WeakDataInterface->ComponentSourceBinding.IsValid())
	{
		return false;
	}
	
	const UOptimusComponentSource* ComponentSource = WeakDataInterface->ComponentSourceBinding->GetComponentSource();
	const FString& NumThreadsExpression = WeakDataInterface->NumThreadsExpression;
	
	if (!WeakComponent.IsValid() || !ComponentSource)
	{
		return false;
	}
	
	const UActorComponent* Component = WeakComponent.Get();
	int32 LodIndex = ComponentSource->GetLodIndex(Component);
	int32 NumInvocations = ComponentSource->GetDefaultNumInvocations(Component, LodIndex);

	// In some cases the component can be set to a unusable state intentionally
	// such as when the OptimusEditor shuts down,
	// in which case we don't have data to work with so simply do nothing
	if (NumInvocations < 1)
	{
		return false;
	}
	
	TMap<FName, float> EngineConstants;
	TMap<FName, TArray<int32>> ElementCountsPerDomain;

	for(FName ExecutionDomain: ComponentSource->GetExecutionDomains())
	{
		EngineConstants.Add(ExecutionDomain, 0);

		TArray<int32>& ElementCounts = ElementCountsPerDomain.FindOrAdd(ExecutionDomain);
		if (!ComponentSource->GetComponentElementCountsForExecutionDomain(
				ExecutionDomain,
				Component,
				LodIndex,
				ElementCounts))
		{
			return false;
		}

		if (!ensure(NumInvocations == ElementCounts.Num()))
		{
			// Component source needs to provide as many values for each of its execution domains as there are invocations
			return false;
		}
	}

	using namespace Optimus::Expression;

	FEngine Engine;
	TVariant<FExpressionObject, FParseError> ParseResult = Engine.Parse(NumThreadsExpression, [EngineConstants](FName InName)->TOptional<float>
	{
		if (const float* Value = EngineConstants.Find(InName))
		{
			return *Value;
		};

		return {};
	});
	
	if (ParseResult.IsType<FParseError>())
	{
		return false;
	}

	OutInvocationThreadCount.Reset(NumInvocations);
	for (int32 Index = 0; Index < NumInvocations; Index++)
	{
		for (TPair<FName, float>& Constant: EngineConstants)
		{
			const FName ConstantName = Constant.Key;
			float& Value = Constant.Value;
			
			const TArray<int32>& ElementCounts = ElementCountsPerDomain[ConstantName]; 
			Value = ElementCounts.IsValidIndex(Index) ? ElementCounts[Index] : 1;
		}
		
		const int32 Count = static_cast<int32>(Engine.Execute(ParseResult.Get<FExpressionObject>(),
			[EngineConstants](FName InName)->TOptional<float>
			{
				if (const float* Value = EngineConstants.Find(InName))
				{
					return *Value;
				};

				return {};
			}));

		if (Count < 0)
		{
			return false;
		}
		
		OutInvocationThreadCount.Add(Count);
	}

	OutTotalThreadCount = 0;
	for (int32 Count : OutInvocationThreadCount)
	{
		OutTotalThreadCount += Count;
	}

	// Extra validation for unified dispatch

	// We want to make sure the results are the same for unified and non-unified
	// Do the sum across invocations first and then evaluate the expression
	for (TPair<FName, float>& Constant : EngineConstants)
	{
		const FName ConstantName = Constant.Key;
		float& Value = Constant.Value;
		Value = 0;
		const TArray<int32>& ElementCounts = ElementCountsPerDomain[ConstantName];
		
		for (int32 Count : ElementCounts )
		{
			Value += Count;
		}
	}
	
	
	const int32 TotalElementCountForUnifiedDispatch = static_cast<int32>(Engine.Execute(ParseResult.Get<FExpressionObject>(), 
		[EngineConstants](FName InName)->TOptional<float>
		{
			if (const float* Value = EngineConstants.Find(InName))
			{
				return *Value;
			};

			return {};
		}));
	
	if (OutTotalThreadCount != TotalElementCountForUnifiedDispatch)
	{
		return false;
	}

	return true;	
}

FOptimusCustomComputeKernelDataProviderProxy::FOptimusCustomComputeKernelDataProviderProxy(
	TArray<int32>&& InInvocationThreadCounts,
	int32 InTotalThreadCount
	) :
	InvocationThreadCounts(MoveTemp(InInvocationThreadCounts)),
	TotalThreadCount(InTotalThreadCount)
{
}

bool FOptimusCustomComputeKernelDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InvocationThreadCounts.Num() == 0)
	{
		return false;
	}

	if (TotalThreadCount <= 0)
	{
		return false;
	}
	
	return true;
}

int32 FOptimusCustomComputeKernelDataProviderProxy::GetDispatchThreadCount(TArray<FIntVector>& InOutThreadCounts) const
{
	InOutThreadCounts.Reset(InvocationThreadCounts.Num());
	for (const int32 Count : InvocationThreadCounts)
	{
		InOutThreadCounts.Add({Count, 1, 1});
	}
	return InOutThreadCounts.Num();
}

void FOptimusCustomComputeKernelDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	for (int32 InvocationIndex = 0; InvocationIndex < InDispatchData.NumInvocations; ++InvocationIndex)
	{
		int32 NumThreads;
		if (InDispatchData.bUnifiedDispatch)
		{
			NumThreads = TotalThreadCount;
		}
		else
		{
			NumThreads = InvocationThreadCounts[InvocationIndex];
		}

		uint8* ParameterBuffer = (InDispatchData.ParameterBuffer + InDispatchData.ParameterBufferOffset + InDispatchData.ParameterBufferStride * InvocationIndex);
		
		const int32 ThreadCount3D[] = {NumThreads, 1, 1};
		FMemory::Memcpy(ParameterBuffer, ThreadCount3D, 3 * sizeof(int32));
	}
}


