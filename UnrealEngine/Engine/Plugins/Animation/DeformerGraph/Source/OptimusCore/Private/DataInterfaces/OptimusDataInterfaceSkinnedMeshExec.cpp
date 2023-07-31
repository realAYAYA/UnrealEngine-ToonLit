// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceSkinnedMeshExec.h"

#include "Components/SkinnedMeshComponent.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalRenderPublic.h"

FString UOptimusSkinnedMeshExecDataInterface::GetDisplayName() const
{
	return TEXT("Execute Skinned Mesh");
}

FName UOptimusSkinnedMeshExecDataInterface::GetCategory() const
{
	return CategoryName::ExecutionDataInterfaces;
}

TArray<FOptimusCDIPinDefinition> UOptimusSkinnedMeshExecDataInterface::GetPinDefinitions() const
{
	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({ "NumThreads", "ReadNumThreads" });
	return Defs;
}


TSubclassOf<UActorComponent> UOptimusSkinnedMeshExecDataInterface::GetRequiredComponentClass() const
{
	return USkinnedMeshComponent::StaticClass();
}


void UOptimusSkinnedMeshExecDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumThreads"))
		.AddReturnType(EShaderFundamentalType::Int, 3);
}


BEGIN_SHADER_PARAMETER_STRUCT(FSkinedMeshExecDataInterfaceParameters, )
	SHADER_PARAMETER(FIntVector, NumThreads)
END_SHADER_PARAMETER_STRUCT()

void UOptimusSkinnedMeshExecDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FSkinedMeshExecDataInterfaceParameters>(UID);
}

void UOptimusSkinnedMeshExecDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TEXT("/Plugin/Optimus/Private/DataInterfaceSkinnedMeshExec.ush"), EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusSkinnedMeshExecDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TEXT("/Plugin/Optimus/Private/DataInterfaceSkinnedMeshExec.ush"), EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusSkinnedMeshExecDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusSkinnedMeshExecDataProvider* Provider = NewObject<UOptimusSkinnedMeshExecDataProvider>();
	Provider->SkinnedMesh = Cast<USkinnedMeshComponent>(InBinding);
	Provider->Domain = Domain;
	return Provider;
}


bool UOptimusSkinnedMeshExecDataProvider::IsValid() const
{
	return
		SkinnedMesh != nullptr &&
		SkinnedMesh->MeshObject != nullptr;
}

FComputeDataProviderRenderProxy* UOptimusSkinnedMeshExecDataProvider::GetRenderProxy()
{
	return new FOptimusSkinnedMeshExecDataProviderProxy(SkinnedMesh, Domain);
}


FOptimusSkinnedMeshExecDataProviderProxy::FOptimusSkinnedMeshExecDataProviderProxy(USkinnedMeshComponent* InSkinnedMeshComponent, EOptimusSkinnedMeshExecDomain InDomain)
{
	SkeletalMeshObject = InSkinnedMeshComponent->MeshObject;
	Domain = InDomain;
}

int32 FOptimusSkinnedMeshExecDataProviderProxy::GetDispatchThreadCount(TArray<FIntVector>& ThreadCounts) const
{
	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];
	const int32 NumInvocations = LodRenderData->RenderSections.Num();

	ThreadCounts.Reset();
	ThreadCounts.Reserve(NumInvocations);
	for (int32 InvocationIndex = 0; InvocationIndex < NumInvocations; ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];
		const int32 NumThreads = Domain == EOptimusSkinnedMeshExecDomain::Vertex ? RenderSection.NumVertices : RenderSection.NumTriangles;
		ThreadCounts.Add(FIntVector(NumThreads, 1, 1));
	}
	
	return NumInvocations;
}

void FOptimusSkinnedMeshExecDataProviderProxy::GatherDispatchData(FDispatchSetup const& InDispatchSetup, FCollectedDispatchData& InOutDispatchData)
{
	if (!ensure(InDispatchSetup.ParameterStructSizeForValidation == sizeof(FSkinedMeshExecDataInterfaceParameters)))
	{
		return;
	}

	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];

	const int32 NumInvocations = LodRenderData->RenderSections.Num();
	if (!ensure(NumInvocations == InDispatchSetup.NumInvocations))
	{
		return;
	}

	for (int32 InvocationIndex = 0; InvocationIndex < NumInvocations; ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];
		const int32 NumThreads = Domain == EOptimusSkinnedMeshExecDomain::Vertex ? RenderSection.NumVertices : RenderSection.NumTriangles;

		FSkinedMeshExecDataInterfaceParameters* Parameters = (FSkinedMeshExecDataInterfaceParameters*)(InOutDispatchData.ParameterBuffer + InDispatchSetup.ParameterBufferOffset + InDispatchSetup.ParameterBufferStride * InvocationIndex);
		Parameters->NumThreads = FIntVector(NumThreads, 1, 1);
	}
}
