// Copyright Epic Games, Inc. All Rights Reserved.

#include "MLDeformerGraphDataInterface.h"

#include "Components/SkeletalMeshComponent.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "MLDeformerModel.h"
#include "MLDeformerModelInstance.h"
#include "MLDeformerAsset.h"
#include "MLDeformerComponent.h"
#include "NeuralNetwork.h"
#include "OptimusDataDomain.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderGraphResources.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalRenderPublic.h"

TArray<FOptimusCDIPinDefinition> UMLDeformerGraphDataInterface::GetPinDefinitions() const
{
	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({ "PositionDelta", "ReadPositionDelta", Optimus::DomainName::Vertex, "ReadNumVertices" });
	return Defs;
}

TSubclassOf<UActorComponent> UMLDeformerGraphDataInterface::GetRequiredComponentClass() const
{
	return UMLDeformerComponent::StaticClass();
}

void UMLDeformerGraphDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumVertices"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadPositionDelta"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
		.AddParam(EShaderFundamentalType::Uint);
}

BEGIN_SHADER_PARAMETER_STRUCT(FMLDeformerGraphDataInterfaceParameters, )
	MLDEFORMER_SHADER_PARAMETERS()
END_SHADER_PARAMETER_STRUCT()

FString UMLDeformerGraphDataInterface::GetDisplayName() const
{
	return TEXT("ML Deformer");
}

void UMLDeformerGraphDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const \
{
	InOutBuilder.AddNestedStruct<FMLDeformerGraphDataInterfaceParameters>(UID);
}

void UMLDeformerGraphDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TEXT("/Plugin/MLDeformerFramework/Private/MLDeformerGraphDataInterface.ush"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UMLDeformerGraphDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UMLDeformerGraphDataProvider* Provider = NewObject<UMLDeformerGraphDataProvider>();
	Provider->DeformerComponent = Cast<UMLDeformerComponent>(InBinding);
	return Provider;
}

FComputeDataProviderRenderProxy* UMLDeformerGraphDataProvider::GetRenderProxy()
{
	return new UE::MLDeformer::FMLDeformerGraphDataProviderProxy(DeformerComponent);
}

bool UMLDeformerGraphDataProvider::IsValid() const
{
	if (DeformerComponent == nullptr || DeformerComponent->GetDeformerAsset() == nullptr || DeformerComponent->GetModelInstance() == nullptr)
	{
		return false;
	}

	return DeformerComponent->GetModelInstance()->IsValidForDataProvider();
}

namespace UE::MLDeformer
{
	FMLDeformerGraphDataProviderProxy::FMLDeformerGraphDataProviderProxy(UMLDeformerComponent* DeformerComponent)
	{
		using namespace UE::MLDeformer;

		const UMLDeformerAsset* DeformerAsset = DeformerComponent->GetDeformerAsset();
		if (DeformerAsset)
		{
			const UMLDeformerModel* Model = DeformerAsset->GetModel();
			const UMLDeformerModelInstance* ModelInstance = DeformerComponent->GetModelInstance();
		
			SkeletalMeshObject = ModelInstance->GetSkeletalMeshComponent()->MeshObject;
			NeuralNetwork = Model->GetNeuralNetwork();
			NeuralNetworkInferenceHandle = ModelInstance->GetNeuralNetworkInferenceHandle();
			bCanRunNeuralNet = ModelInstance->IsCompatible() && Model->IsNeuralNetworkOnGPU();
			Weight = DeformerComponent->GetWeight();
			VertexMapBufferSRV = Model->GetVertexMapBuffer().ShaderResourceViewRHI;
		}
	}

	void FMLDeformerGraphDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
	{
		if (bCanRunNeuralNet)
		{
			check(NeuralNetwork != nullptr);
			Buffer = GraphBuilder.RegisterExternalBuffer(NeuralNetwork->GetOutputTensorForContext(NeuralNetworkInferenceHandle).GetPooledBuffer());
		}
		else
		{
			// TODO: use an actual buffer that is of the right size, and filled with zero's.
			Buffer = GraphBuilder.RegisterExternalBuffer(GWhiteVertexBufferWithRDG->Buffer);
		}

		BufferSRV = GraphBuilder.CreateSRV(Buffer, PF_R32_FLOAT);
	}

	void FMLDeformerGraphDataProviderProxy::GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData)
	{
		MLDEFORMER_GRAPH_DISPATCH_START(FMLDeformerGraphDataInterfaceParameters, InDispatchSetup, InOutDispatchData)
		MLDEFORMER_GRAPH_DISPATCH_DEFAULT_PARAMETERS()
		MLDEFORMER_GRAPH_DISPATCH_END()
	}
}	// namespace UE::MLDeformer
