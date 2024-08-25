// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceSkinnedMeshWrite.h"

#include "Components/SkinnedMeshComponent.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "OptimusDataDomain.h"
#include "RenderGraphBuilder.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalMeshDeformerHelpers.h"
#include "SkeletalRenderPublic.h"
#include "DataDrivenShaderPlatformInfo.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusDataInterfaceSkinnedMeshWrite)


FString UOptimusSkinnedMeshWriteDataInterface::GetDisplayName() const
{
	return TEXT("Write Skinned Mesh");
}

FName UOptimusSkinnedMeshWriteDataInterface::GetCategory() const
{
	return CategoryName::OutputDataInterfaces;
}

TArray<FOptimusCDIPinDefinition> UOptimusSkinnedMeshWriteDataInterface::GetPinDefinitions() const
{
	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({ "Position", "WritePosition", Optimus::DomainName::Vertex, "ReadNumVertices" });
	Defs.Add({ "TangentX", "WriteTangentX", Optimus::DomainName::Vertex, "ReadNumVertices" });
	Defs.Add({ "TangentZ", "WriteTangentZ", Optimus::DomainName::Vertex, "ReadNumVertices" });
	Defs.Add({ "Color", "WriteColor", Optimus::DomainName::Vertex, "ReadNumVertices" });
	return Defs;
}


TSubclassOf<UActorComponent> UOptimusSkinnedMeshWriteDataInterface::GetRequiredComponentClass() const
{
	return USkinnedMeshComponent::StaticClass();
}


void UOptimusSkinnedMeshWriteDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumVertices"))
		.AddReturnType(EShaderFundamentalType::Uint);
}

void UOptimusSkinnedMeshWriteDataInterface::GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WritePosition"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float, 3);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteTangentX"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float, 4);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteTangentZ"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float, 4);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteColor"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float, 4);
}

BEGIN_SHADER_PARAMETER_STRUCT(FSkinedMeshWriteDataInterfaceParameters, )
	SHADER_PARAMETER(uint32, NumVertices)
	SHADER_PARAMETER(uint32, OutputStreamStart)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float>, PositionBufferUAV)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<SNORM float4>, TangentBufferUAV)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<UNORM float4>, ColorBufferUAV)
END_SHADER_PARAMETER_STRUCT()

void UOptimusSkinnedMeshWriteDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FSkinedMeshWriteDataInterfaceParameters>(UID);
}

TCHAR const* UOptimusSkinnedMeshWriteDataInterface::TemplateFilePath = TEXT("/Plugin/Optimus/Private/DataInterfaceSkinnedMeshWrite.ush");

TCHAR const* UOptimusSkinnedMeshWriteDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UOptimusSkinnedMeshWriteDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusSkinnedMeshWriteDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusSkinnedMeshWriteDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusSkinnedMeshWriteDataProvider* Provider = NewObject<UOptimusSkinnedMeshWriteDataProvider>();
	Provider->SkinnedMesh = Cast<USkinnedMeshComponent>(InBinding);
	Provider->OutputMask = InOutputMask;
	return Provider;
}


FComputeDataProviderRenderProxy* UOptimusSkinnedMeshWriteDataProvider::GetRenderProxy()
{
	return new FOptimusSkinnedMeshWriteDataProviderProxy(SkinnedMesh, OutputMask);
}


FOptimusSkinnedMeshWriteDataProviderProxy::FOptimusSkinnedMeshWriteDataProviderProxy(USkinnedMeshComponent* InSkinnedMeshComponent, uint64 InOutputMask)
{
	SkeletalMeshObject = InSkinnedMeshComponent != nullptr ? InSkinnedMeshComponent->MeshObject : nullptr;
	OutputMask = InOutputMask;
}

bool FOptimusSkinnedMeshWriteDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		return false;
	}
	if (SkeletalMeshObject == nullptr)
	{
		return false;
	}
	if (SkeletalMeshObject->IsCPUSkinned())
	{
		return false;
	}
	if (SkeletalMeshObject->GetSkeletalMeshRenderData().LODRenderData[SkeletalMeshObject->GetLOD()].RenderSections.Num() != InValidationData.NumInvocations)
	{
		return false;
	}

	return true;
}

void FOptimusSkinnedMeshWriteDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
{
	// Allocate required buffers
	const int32 LodIndex = SkeletalMeshObject->GetLOD();

	// We will extract buffers from RDG.
	// It could be better to for memory to use QueueBufferExtraction instead of ConvertToExternalBuffer but that will require an extra hook after graph execution.
	TRefCountPtr<FRDGPooledBuffer> PositionBufferExternal;

	if (OutputMask & 1)
	{
		PositionBuffer = FSkeletalMeshDeformerHelpers::AllocateVertexFactoryPositionBuffer(GraphBuilder, SkeletalMeshObject, LodIndex, TEXT("OptimusSkinnedMeshPosition"));
		PositionBufferUAV = GraphBuilder.CreateUAV(PositionBuffer, PF_R32_FLOAT, ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	else
	{
		PositionBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(GWhiteVertexBufferWithRDG->Buffer), PF_R32_FLOAT);
	}

	// OpenGL ES does not support writing to RGBA16_SNORM images, instead pack data into SINT in the shader
	const EPixelFormat TangentsFormat = IsOpenGLPlatform(GMaxRHIShaderPlatform) ? PF_R16G16B16A16_SINT : PF_R16G16B16A16_SNORM;

	if (OutputMask & 2)
	{
		TangentBuffer = FSkeletalMeshDeformerHelpers::AllocateVertexFactoryTangentBuffer(GraphBuilder, SkeletalMeshObject, LodIndex, TEXT("OptimusSkinnedMeshTangent"));
		TangentBufferUAV = GraphBuilder.CreateUAV(TangentBuffer, TangentsFormat, ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	else
	{
		TangentBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(GWhiteVertexBufferWithRDG->Buffer), TangentsFormat);
	}

	if (OutputMask & 8)
	{
		ColorBuffer = FSkeletalMeshDeformerHelpers::AllocateVertexFactoryColorBuffer(GraphBuilder, SkeletalMeshObject, LodIndex, TEXT("OptimusSkinnedMeshColor"));
		ColorBufferUAV = GraphBuilder.CreateUAV(ColorBuffer, PF_B8G8R8A8, ERDGUnorderedAccessViewFlags::SkipBarrier);
	}
	else
	{
		ColorBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(GWhiteVertexBufferWithRDG->Buffer), PF_B8G8R8A8);
	}

	FSkeletalMeshDeformerHelpers::UpdateVertexFactoryBufferOverrides(GraphBuilder.RHICmdList, SkeletalMeshObject, LodIndex);
}

void FOptimusSkinnedMeshWriteDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];

		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.NumVertices = InDispatchData.bUnifiedDispatch ? LodRenderData->GetNumVertices() : RenderSection.GetNumVertices();
		Parameters.OutputStreamStart = InDispatchData.bUnifiedDispatch ? 0 : RenderSection.GetVertexBufferIndex();
		Parameters.PositionBufferUAV = PositionBufferUAV;
		Parameters.TangentBufferUAV = TangentBufferUAV;
		Parameters.ColorBufferUAV = ColorBufferUAV;
	}
}
