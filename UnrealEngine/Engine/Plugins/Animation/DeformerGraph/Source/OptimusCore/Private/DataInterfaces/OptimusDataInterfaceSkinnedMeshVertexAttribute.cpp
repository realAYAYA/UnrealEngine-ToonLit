// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceSkinnedMeshVertexAttribute.h"

#include "ComponentSources/OptimusSkinnedMeshComponentSource.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "Rendering/SkeletalMeshAttributeVertexBuffer.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalRenderPublic.h"


FString UOptimusSkinnedMeshVertexAttributeDataInterface::GetDisplayName() const
{
	return TEXT("Skinned Mesh Vertex Attribute");
}


TArray<FOptimusCDIPinDefinition> UOptimusSkinnedMeshVertexAttributeDataInterface::GetPinDefinitions() const
{
	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({"NumVertices", "ReadNumVertices"});
	Defs.Add({"Value", "ReadValue", UOptimusSkinnedMeshComponentSource::Domains::Vertex, "ReadNumVertices"});
	return Defs;
}


TSubclassOf<UActorComponent> UOptimusSkinnedMeshVertexAttributeDataInterface::GetRequiredComponentClass() const
{
	return USkinnedMeshComponent::StaticClass();
}


void UOptimusSkinnedMeshVertexAttributeDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumVertices"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadValue"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Uint);
}

BEGIN_SHADER_PARAMETER_STRUCT(FSkinnedMeshVertexAttributeDataInterfaceParameters, )
	SHADER_PARAMETER(uint32, NumVertices)
	SHADER_PARAMETER(uint32, InputStreamStart)
	SHADER_PARAMETER(uint32, bIsValid)
	SHADER_PARAMETER_SRV(Buffer<float>, ValueBuffer)
END_SHADER_PARAMETER_STRUCT()


void UOptimusSkinnedMeshVertexAttributeDataInterface::GetShaderParameters(
	TCHAR const* InUID,
	FShaderParametersMetadataBuilder& InOutBuilder, 
	FShaderParametersMetadataAllocations& InOutAllocations
	) const
{
	InOutBuilder.AddNestedStruct<FSkinnedMeshVertexAttributeDataInterfaceParameters>(InUID);
}


void UOptimusSkinnedMeshVertexAttributeDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TEXT("/Plugin/Optimus/Private/DataInterfaceSkinnedMeshVertexAttribute.ush"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}


void UOptimusSkinnedMeshVertexAttributeDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TEXT("/Plugin/Optimus/Private/DataInterfaceSkinnedMeshVertexAttribute.ush"), EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}


UComputeDataProvider* UOptimusSkinnedMeshVertexAttributeDataInterface::CreateDataProvider(
	TObjectPtr<UObject> InBinding,
	uint64 InInputMask,
	uint64 InOutputMask
	) const
{
	UOptimusSkinnedMeshVertexAttributeDataProvider* Provider = NewObject<UOptimusSkinnedMeshVertexAttributeDataProvider>();
	Provider->SkinnedMeshComponent = Cast<USkinnedMeshComponent>(InBinding);
	Provider->AttributeName = AttributeName;

	return Provider;
}


bool UOptimusSkinnedMeshVertexAttributeDataProvider::IsValid() const
{
	if (SkinnedMeshComponent == nullptr || SkinnedMeshComponent->MeshObject == nullptr)
	{
		return false;
	}
	
	const int32 LodIndex = SkinnedMeshComponent->MeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkinnedMeshComponent->MeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];

	return LodRenderData->VertexAttributeBuffers.GetAttributeBuffer(AttributeName) != nullptr;
}


FComputeDataProviderRenderProxy* UOptimusSkinnedMeshVertexAttributeDataProvider::GetRenderProxy()
{
	return new FOptimusSkinnedMeshVertexAttributeDataProviderProxy(SkinnedMeshComponent, AttributeName);
}


FOptimusSkinnedMeshVertexAttributeDataProviderProxy::FOptimusSkinnedMeshVertexAttributeDataProviderProxy(
	USkinnedMeshComponent* InSkinnedMeshComponent, 
	FName InAttributeName
	)
{
	SkeletalMeshObject = InSkinnedMeshComponent->MeshObject;
	AttributeName = InAttributeName;
}


bool FOptimusSkinnedMeshVertexAttributeDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		return false;
	}
	if (SkeletalMeshObject == nullptr)
	{
		return false;
	}
	if (SkeletalMeshObject->GetSkeletalMeshRenderData().LODRenderData[SkeletalMeshObject->GetLOD()].RenderSections.Num() != InValidationData.NumInvocations)
	{
		return false;
	}

	return true;
}


void FOptimusSkinnedMeshVertexAttributeDataProviderProxy::GatherDispatchData(
	FDispatchData const& InDispatchData
	)
{
	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];
	
	const TStridedView<FSkinnedMeshVertexAttributeDataInterfaceParameters> ParameterArray =
		MakeStridedParameterView<FSkinnedMeshVertexAttributeDataInterfaceParameters>(InDispatchData);
	
	FRHIShaderResourceView* NullSRVBinding = GWhiteVertexBufferWithSRV->ShaderResourceViewRHI.GetReference();
	const FSkeletalMeshAttributeVertexBuffer* AttributeBuffer = LodRenderData->VertexAttributeBuffers.GetAttributeBuffer(AttributeName);
	FRHIShaderResourceView *AttributeSRV = AttributeBuffer ? AttributeBuffer->GetSRV() : NullSRVBinding;

	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];

		FSkinnedMeshVertexAttributeDataInterfaceParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.NumVertices = InDispatchData.bUnifiedDispatch ? LodRenderData->GetNumVertices() : RenderSection.NumVertices;
		Parameters.InputStreamStart = InDispatchData.bUnifiedDispatch ? 0 : RenderSection.BaseVertexIndex;
		Parameters.ValueBuffer = AttributeSRV;
		Parameters.bIsValid = AttributeBuffer != nullptr;
	}
}
