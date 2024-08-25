// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceSkinnedMesh.h"

#include "OptimusDataDomain.h"
#include "ComponentSources/OptimusSkinnedMeshComponentSource.h"

#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalRenderPublic.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusDataInterfaceSkinnedMesh)


FString UOptimusSkinnedMeshDataInterface::GetDisplayName() const
{
	return TEXT("Skinned Mesh");
}

TArray<FOptimusCDIPinDefinition> UOptimusSkinnedMeshDataInterface::GetPinDefinitions() const
{
	FName Vertex(UOptimusSkinnedMeshComponentSource::Domains::Vertex);
	FName Triangle(UOptimusSkinnedMeshComponentSource::Domains::Triangle);
	
	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({"NumVertices", "ReadNumVertices", false});
	Defs.Add({"Position", "ReadPosition", Vertex, "ReadNumVertices", false});
	Defs.Add({"TangentX", "ReadTangentX", Vertex, "ReadNumVertices", false});
	Defs.Add({"TangentZ", "ReadTangentZ", Vertex, "ReadNumVertices", false});
	Defs.Add({"NumUVChannels", "ReadNumUVChannels", false});
	Defs.Add({"UV", "ReadUV", {{Vertex, "ReadNumVertices"}, {Optimus::DomainName::UVChannel, "ReadNumUVChannels"}}, false});
	Defs.Add({"Color", "ReadColor", Vertex, "ReadColor", false});
	Defs.Add({"NumTriangles", "ReadNumTriangles", false});
	Defs.Add({"IndexBuffer", "ReadIndexBuffer", Triangle, 3, "ReadNumTriangles", false});
	return Defs;
}


TSubclassOf<UActorComponent> UOptimusSkinnedMeshDataInterface::GetRequiredComponentClass() const
{
	return USkinnedMeshComponent::StaticClass();
}


void UOptimusSkinnedMeshDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumVertices"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumTriangles"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumUVChannels"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadIndexBuffer"))
		.AddReturnType(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadPosition"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadTangentX"))
		.AddReturnType(EShaderFundamentalType::Float, 4)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadTangentZ"))
		.AddReturnType(EShaderFundamentalType::Float, 4)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadUV"))
		.AddReturnType(EShaderFundamentalType::Float, 2)
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadColor"))
		.AddReturnType(EShaderFundamentalType::Float, 4)
		.AddParam(EShaderFundamentalType::Uint);
}

BEGIN_SHADER_PARAMETER_STRUCT(FSkinnedMeshDataInterfaceParameters, )
	SHADER_PARAMETER(uint32, NumVertices)
	SHADER_PARAMETER(uint32, NumTriangles)
	SHADER_PARAMETER(uint32, NumUVChannels)
	SHADER_PARAMETER(uint32, IndexBufferStart)
	SHADER_PARAMETER(uint32, InputStreamStart)
	SHADER_PARAMETER_SRV(Buffer<uint>, IndexBuffer)
	SHADER_PARAMETER_SRV(Buffer<float>, PositionInputBuffer)
	SHADER_PARAMETER_SRV(Buffer<SNORM float4>, TangentInputBuffer)
	SHADER_PARAMETER_SRV(Buffer<float2>, UVInputBuffer)
	SHADER_PARAMETER_SRV(Buffer<float4>, ColorInputBuffer)
END_SHADER_PARAMETER_STRUCT()

void UOptimusSkinnedMeshDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FSkinnedMeshDataInterfaceParameters>(UID);
}

TCHAR const* UOptimusSkinnedMeshDataInterface::TemplateFilePath = TEXT("/Plugin/Optimus/Private/DataInterfaceSkinnedMesh.ush");

TCHAR const* UOptimusSkinnedMeshDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UOptimusSkinnedMeshDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusSkinnedMeshDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusSkinnedMeshDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusSkinnedMeshDataProvider* Provider = NewObject<UOptimusSkinnedMeshDataProvider>();
	Provider->SkinnedMesh = Cast<USkinnedMeshComponent>(InBinding);
	return Provider;
}


FComputeDataProviderRenderProxy* UOptimusSkinnedMeshDataProvider::GetRenderProxy()
{
	return new FOptimusSkinnedMeshDataProviderProxy(SkinnedMesh);
}


FOptimusSkinnedMeshDataProviderProxy::FOptimusSkinnedMeshDataProviderProxy(USkinnedMeshComponent* SkinnedMeshComponent)
{
	SkeletalMeshObject = SkinnedMeshComponent != nullptr ? SkinnedMeshComponent->MeshObject : nullptr;
}

bool FOptimusSkinnedMeshDataProviderProxy::IsValid(FValidationData const& InValidationData) const
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

void FOptimusSkinnedMeshDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];

	FRHIShaderResourceView* NullSRVBinding = GWhiteVertexBufferWithSRV->ShaderResourceViewRHI.GetReference();

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];

		FRHIShaderResourceView* IndexBufferSRV = LodRenderData->MultiSizeIndexContainer.GetIndexBuffer()->GetSRV();
		FRHIShaderResourceView* MeshVertexBufferSRV = LodRenderData->StaticVertexBuffers.PositionVertexBuffer.GetSRV();
		FRHIShaderResourceView* MeshTangentBufferSRV = LodRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.GetTangentsSRV();
		FRHIShaderResourceView* MeshUVBufferSRV = LodRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.GetTexCoordsSRV();
		FRHIShaderResourceView* MeshColorBufferSRV = LodRenderData->StaticVertexBuffers.ColorVertexBuffer.GetColorComponentsSRV();

		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.NumVertices = InDispatchData.bUnifiedDispatch ? LodRenderData->GetNumVertices() : RenderSection.NumVertices;
		Parameters.NumTriangles = InDispatchData.bUnifiedDispatch ? LodRenderData->GetTotalFaces() : RenderSection.NumTriangles;
		Parameters.NumUVChannels = LodRenderData->StaticVertexBuffers.StaticMeshVertexBuffer.GetNumTexCoords();
		Parameters.IndexBufferStart = InDispatchData.bUnifiedDispatch ? 0 : RenderSection.BaseIndex;
		Parameters.InputStreamStart = InDispatchData.bUnifiedDispatch ? 0 : RenderSection.BaseVertexIndex;
		Parameters.IndexBuffer = IndexBufferSRV != nullptr ? IndexBufferSRV : NullSRVBinding;
		Parameters.PositionInputBuffer = MeshVertexBufferSRV != nullptr ? MeshVertexBufferSRV : NullSRVBinding;
		Parameters.TangentInputBuffer = MeshTangentBufferSRV != nullptr ? MeshTangentBufferSRV : NullSRVBinding;
		Parameters.UVInputBuffer = MeshUVBufferSRV != nullptr ? MeshUVBufferSRV : NullSRVBinding;
		Parameters.ColorInputBuffer = MeshColorBufferSRV != nullptr ? MeshColorBufferSRV : NullSRVBinding;
	}
}
