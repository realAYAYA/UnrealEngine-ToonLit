// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceSkinnedMeshWrite.h"

#include "Components/SkinnedMeshComponent.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "OptimusDataDomain.h"
#include "RenderGraphBuilder.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalMeshDeformerHelpers.h"
#include "SkeletalRenderPublic.h"


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

void UOptimusSkinnedMeshWriteDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TEXT("/Plugin/Optimus/Private/DataInterfaceSkinnedMeshWrite.ush"), EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusSkinnedMeshWriteDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TEXT("/Plugin/Optimus/Private/DataInterfaceSkinnedMeshWrite.ush"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusSkinnedMeshWriteDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusSkinnedMeshWriteDataProvider* Provider = NewObject<UOptimusSkinnedMeshWriteDataProvider>();
	Provider->SkinnedMesh = Cast<USkinnedMeshComponent>(InBinding);
	Provider->OutputMask = InOutputMask;
	return Provider;
}


bool UOptimusSkinnedMeshWriteDataProvider::IsValid() const
{
	return
		SkinnedMesh != nullptr &&
		SkinnedMesh->MeshObject != nullptr;
}

FComputeDataProviderRenderProxy* UOptimusSkinnedMeshWriteDataProvider::GetRenderProxy()
{
	return new FOptimusSkinnedMeshWriteDataProviderProxy(SkinnedMesh, OutputMask);
}


FOptimusSkinnedMeshWriteDataProviderProxy::FOptimusSkinnedMeshWriteDataProviderProxy(USkinnedMeshComponent* InSkinnedMeshComponent, uint64 InOutputMask)
{
	SkeletalMeshObject = InSkinnedMeshComponent->MeshObject;
	OutputMask = InOutputMask;
}

void FOptimusSkinnedMeshWriteDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
{
	// Allocate required buffers
	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];
	const int32 NumVertices = LodRenderData->GetNumVertices();

	// We will extract buffers from RDG.
	// It could be better to for memory to use QueueBufferExtraction instead of ConvertToExternalBuffer but that will require an extra hook after graph execution.
	TRefCountPtr<FRDGPooledBuffer> PositionBufferExternal;
	TRefCountPtr<FRDGPooledBuffer> TangentBufferExternal;
	TRefCountPtr<FRDGPooledBuffer> ColorBufferExternal;

	if (OutputMask & 1)
	{
		const uint32 PosBufferBytesPerElement = 4;
		PositionBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(PosBufferBytesPerElement, NumVertices * 3), TEXT("SkinnedMeshPositionBuffer"), ERDGBufferFlags::None);
		PositionBufferUAV = GraphBuilder.CreateUAV(PositionBuffer, PF_R32_FLOAT, ERDGUnorderedAccessViewFlags::SkipBarrier);
		PositionBufferExternal = GraphBuilder.ConvertToExternalBuffer(PositionBuffer);
		GraphBuilder.SetBufferAccessFinal(PositionBuffer, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask);
	}
	else
	{
		PositionBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(GWhiteVertexBufferWithRDG->Buffer), PF_R32_FLOAT);
	}

	// OpenGL ES does not support writing to RGBA16_SNORM images, instead pack data into SINT in the shader
	const EPixelFormat TangentsFormat = IsOpenGLPlatform(GMaxRHIShaderPlatform) ? PF_R16G16B16A16_SINT : PF_R16G16B16A16_SNORM;

	if (OutputMask & 2)
	{
		const uint32 TangentBufferBytesPerElement = 8;
		TangentBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(TangentBufferBytesPerElement, NumVertices * 2), TEXT("SkinnedMeshTangentBuffer"), ERDGBufferFlags::None);
		TangentBufferUAV = GraphBuilder.CreateUAV(TangentBuffer, TangentsFormat, ERDGUnorderedAccessViewFlags::SkipBarrier);
		TangentBufferExternal = GraphBuilder.ConvertToExternalBuffer(TangentBuffer);
		GraphBuilder.SetBufferAccessFinal(TangentBuffer, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask);
	}
	else
	{
		TangentBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(GWhiteVertexBufferWithRDG->Buffer), TangentsFormat);
	}

	if (OutputMask & 8)
	{
		const uint32 ColorBufferBytesPerElement = 4;
		ColorBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(ColorBufferBytesPerElement, NumVertices), TEXT("SkinnedMeshColorBuffer"), ERDGBufferFlags::None);
		ColorBufferUAV = GraphBuilder.CreateUAV(ColorBuffer, PF_B8G8R8A8, ERDGUnorderedAccessViewFlags::SkipBarrier);
		ColorBufferExternal = GraphBuilder.ConvertToExternalBuffer(ColorBuffer);
		GraphBuilder.SetBufferAccessFinal(ColorBuffer, ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask);
	}
	else
	{
		ColorBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(GWhiteVertexBufferWithRDG->Buffer), PF_B8G8R8A8);
	}

	// Set to vertex factories
	const int32 NumSections = LodRenderData->RenderSections.Num();
	FSkeletalMeshDeformerHelpers::SetVertexFactoryBufferOverrides(SkeletalMeshObject, LodIndex, FSkeletalMeshDeformerHelpers::EOverrideType::Partial, PositionBufferExternal, TangentBufferExternal, ColorBufferExternal);

#if RHI_RAYTRACING
	if (PositionBufferExternal.IsValid())
	{
		// This can create RHI resources but it queues and doesn't actually build ray tracing structures. Not sure if we need to put inside a render graph pass?
		// Also note that for ray tracing we may want to support a second graph execution if ray tracing LOD needs to be different to render LOD.
		FSkeletalMeshDeformerHelpers::UpdateRayTracingGeometry(SkeletalMeshObject, LodIndex, PositionBufferExternal);
	}
#endif
}

void FOptimusSkinnedMeshWriteDataProviderProxy::GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData)
{
	if (!ensure(InDispatchSetup.ParameterStructSizeForValidation == sizeof(FSkinedMeshWriteDataInterfaceParameters)))
	{
		return;
	}

	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];
	if (!ensure(LodRenderData->RenderSections.Num() == InDispatchSetup.NumInvocations))
	{
		return;
	}

	for (int32 InvocationIndex = 0; InvocationIndex < InDispatchSetup.NumInvocations; ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];

		FSkinedMeshWriteDataInterfaceParameters* Parameters = (FSkinedMeshWriteDataInterfaceParameters*)(InOutDispatchData.ParameterBuffer + InDispatchSetup.ParameterBufferOffset + InDispatchSetup.ParameterBufferStride * InvocationIndex);
		Parameters->NumVertices = RenderSection.GetNumVertices();
		Parameters->OutputStreamStart = RenderSection.GetVertexBufferIndex();
		Parameters->PositionBufferUAV = PositionBufferUAV;
		Parameters->TangentBufferUAV = TangentBufferUAV;
		Parameters->ColorBufferUAV = ColorBufferUAV;
	}
}
