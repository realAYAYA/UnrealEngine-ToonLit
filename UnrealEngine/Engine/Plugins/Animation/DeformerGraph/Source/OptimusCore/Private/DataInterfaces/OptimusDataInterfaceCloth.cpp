// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceCloth.h"

#include "Components/SkeletalMeshComponent.h"
#include "ComputeFramework/ComputeKernelPermutationVector.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "ShaderParameterMetadataBuilder.h"
#include "OptimusDataDomain.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "SkeletalMeshDeformerHelpers.h"
#include "SkeletalRenderPublic.h"

FString UOptimusClothDataInterface::GetDisplayName() const
{
	return TEXT("Cloth");
}

TArray<FOptimusCDIPinDefinition> UOptimusClothDataInterface::GetPinDefinitions() const
{
	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({ "ClothToLocal", "ReadClothToLocal" });
	Defs.Add({ "ClothWeight", "ReadClothWeight", Optimus::DomainName::Vertex, "ReadNumVertices" });
	Defs.Add({ "ClothPosition", "ReadClothPosition", Optimus::DomainName::Vertex, "ReadNumVertices" });
	Defs.Add({ "ClothTangentX", "ReadClothTangentX", Optimus::DomainName::Vertex, "ReadNumVertices" });
	Defs.Add({ "ClothTangentZ", "ReadClothTangentZ", Optimus::DomainName::Vertex, "ReadNumVertices" });
	return Defs;
}


TSubclassOf<UActorComponent> UOptimusClothDataInterface::GetRequiredComponentClass() const
{
	return USkinnedMeshComponent::StaticClass();
}


void UOptimusClothDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumVertices"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadClothToLocal"))
		.AddReturnType(EShaderFundamentalType::Float, 4, 4);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadClothWeight"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadClothPosition"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadClothTangentX"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadClothTangentZ"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
		.AddParam(EShaderFundamentalType::Uint);
}

BEGIN_SHADER_PARAMETER_STRUCT(FClothDataInterfaceParameters, )
	SHADER_PARAMETER(uint32, NumVertices)
	SHADER_PARAMETER(uint32, InputStreamStart)
	SHADER_PARAMETER(uint32, NumInfluencesPerVertex)
	SHADER_PARAMETER(float, ClothBlendWeight)
	SHADER_PARAMETER(FMatrix44f, ClothToLocal)
	SHADER_PARAMETER_SRV(Buffer<float4>, ClothBuffer)
	SHADER_PARAMETER_SRV(Buffer<float2>, ClothPositionsAndNormalsBuffer)
END_SHADER_PARAMETER_STRUCT()

void UOptimusClothDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FClothDataInterfaceParameters>(UID);
}

void UOptimusClothDataInterface::GetPermutations(FComputeKernelPermutationVector& OutPermutationVector) const
{
	OutPermutationVector.AddPermutation(TEXT("ENABLE_DEFORMER_CLOTH"), 2);
}

void UOptimusClothDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TEXT("/Plugin/Optimus/Private/DataInterfaceCloth.ush"), EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusClothDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TEXT("/Plugin/Optimus/Private/DataInterfaceCloth.ush"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusClothDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusClothDataProvider* Provider = NewObject<UOptimusClothDataProvider>();
	Provider->SkinnedMesh = Cast<USkinnedMeshComponent>(InBinding);
	return Provider;
}


bool UOptimusClothDataProvider::IsValid() const
{
	return
		SkinnedMesh != nullptr &&
		SkinnedMesh->MeshObject != nullptr;
}

FComputeDataProviderRenderProxy* UOptimusClothDataProvider::GetRenderProxy()
{
	return new FOptimusClothDataProviderProxy(SkinnedMesh);
}


FOptimusClothDataProviderProxy::FOptimusClothDataProviderProxy(USkinnedMeshComponent* SkinnedMeshComponent)
{
	SkeletalMeshObject = SkinnedMeshComponent->MeshObject;

	USkeletalMeshComponent* SkeletalMeshComponent = Cast<USkeletalMeshComponent>(SkinnedMeshComponent);
	ClothBlendWeight = SkeletalMeshComponent ? SkeletalMeshComponent->ClothBlendWeight : 1.f;

	FrameNumber = SkinnedMeshComponent->GetScene()->GetFrameNumber() + 1; // +1 matches the logic for FrameNumberToPrepare in FSkeletalMeshObjectGPUSkin::Update()
}

struct FClothDataInterfacePermutationIds
{
	uint32 EnableDeformerCloth = 0;

	FClothDataInterfacePermutationIds(FComputeKernelPermutationVector const& PermutationVector)
	{
		{
			static FString Name(TEXT("ENABLE_DEFORMER_CLOTH"));
			static uint32 Hash = GetTypeHash(Name);
			EnableDeformerCloth = PermutationVector.GetPermutationBits(Name, Hash, 1);
		}
	}
};

void FOptimusClothDataProviderProxy::GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData)
{
	if (!ensure(InDispatchSetup.ParameterStructSizeForValidation == sizeof(FClothDataInterfaceParameters)))
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

	FClothDataInterfacePermutationIds PermutationIds(InDispatchSetup.PermutationVector);

	FRHIShaderResourceView* NullSRVBinding = GWhiteVertexBufferWithSRV->ShaderResourceViewRHI.GetReference();

	for (int32 InvocationIndex = 0; InvocationIndex < InDispatchSetup.NumInvocations; ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];

		const uint32 NumWrapDeformerWeights = RenderSection.ClothMappingDataLODs.Num() > 0 ? RenderSection.ClothMappingDataLODs[0].Num() : 0;
		const bool bMultipleWrapDeformerInfluences = RenderSection.NumVertices < NumWrapDeformerWeights;
		const int32 NumClothInfluencesPerVertex = bMultipleWrapDeformerInfluences ? 5 : 1; // From ClothingMeshUtils.cpp. Could make this a permutation like with skin cache.

		const bool bPreviousFrame = false;
		FSkeletalMeshDeformerHelpers::FClothBuffers ClothBuffers = FSkeletalMeshDeformerHelpers::GetClothBuffersForReading(SkeletalMeshObject, LodIndex, InvocationIndex, FrameNumber, bPreviousFrame);
		const bool bValidCloth = (ClothBuffers.ClothInfluenceBuffer != nullptr) && (ClothBuffers.ClothSimulatedPositionAndNormalBuffer != nullptr);

		FClothDataInterfaceParameters* Parameters = (FClothDataInterfaceParameters*)(InOutDispatchData.ParameterBuffer + InDispatchSetup.ParameterBufferOffset + InDispatchSetup.ParameterBufferStride * InvocationIndex);
		Parameters->NumVertices = RenderSection.NumVertices;
		Parameters->InputStreamStart = ClothBuffers.ClothInfluenceBufferOffset;
		Parameters->ClothBlendWeight = bValidCloth ? ClothBlendWeight : 0.f;
		Parameters->NumInfluencesPerVertex = bValidCloth ? NumClothInfluencesPerVertex : 0;
		Parameters->ClothToLocal = ClothBuffers.ClothToLocal;
		Parameters->ClothBuffer = bValidCloth ? ClothBuffers.ClothInfluenceBuffer : NullSRVBinding;
		Parameters->ClothPositionsAndNormalsBuffer = bValidCloth ? ClothBuffers.ClothSimulatedPositionAndNormalBuffer : NullSRVBinding;

		InOutDispatchData.PermutationId[InvocationIndex] |= (bValidCloth ? PermutationIds.EnableDeformerCloth : 0);
	}
}
