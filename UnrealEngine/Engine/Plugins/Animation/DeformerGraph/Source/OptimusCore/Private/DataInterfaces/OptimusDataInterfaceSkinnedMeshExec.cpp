// Copyright Epic Games, Inc. All Rights Reserved.

#include "OptimusDataInterfaceSkinnedMeshExec.h"

#include "Components/SkinnedMeshComponent.h"
#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalRenderPublic.h"
#include "ComponentSources/OptimusSkinnedMeshComponentSource.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OptimusDataInterfaceSkinnedMeshExec)

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

TCHAR const* UOptimusSkinnedMeshExecDataInterface::TemplateFilePath = TEXT("/Plugin/Optimus/Private/DataInterfaceSkinnedMeshExec.ush");

TCHAR const* UOptimusSkinnedMeshExecDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UOptimusSkinnedMeshExecDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusSkinnedMeshExecDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusSkinnedMeshExecDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusSkinnedMeshExecDataProvider* Provider = NewObject<UOptimusSkinnedMeshExecDataProvider>();
	Provider->SkinnedMesh = Cast<USkinnedMeshComponent>(InBinding);
	Provider->Domain = Domain;
	return Provider;
}

FName UOptimusSkinnedMeshExecDataInterface::GetSelectedExecutionDomainName() const
{
	if (Domain == EOptimusSkinnedMeshExecDomain::Vertex)
	{
		return UOptimusSkinnedMeshComponentSource::Domains::Vertex;
	}

	if (Domain == EOptimusSkinnedMeshExecDomain::Triangle)
	{
		return UOptimusSkinnedMeshComponentSource::Domains::Triangle;
	}

	return NAME_None;
}


FComputeDataProviderRenderProxy* UOptimusSkinnedMeshExecDataProvider::GetRenderProxy()
{
	return new FOptimusSkinnedMeshExecDataProviderProxy(SkinnedMesh, Domain);
}


FOptimusSkinnedMeshExecDataProviderProxy::FOptimusSkinnedMeshExecDataProviderProxy(USkinnedMeshComponent* InSkinnedMeshComponent, EOptimusSkinnedMeshExecDomain InDomain)
{
	SkeletalMeshObject = InSkinnedMeshComponent != nullptr ? InSkinnedMeshComponent->MeshObject : nullptr;
	Domain = InDomain;
}

int32 FOptimusSkinnedMeshExecDataProviderProxy::GetDispatchThreadCount(TArray<FIntVector>& ThreadCounts) const
{
	ThreadCounts.Reset();
	if (SkeletalMeshObject == nullptr)
	{
		return 0;
	}

	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];
	const int32 NumInvocations = LodRenderData->RenderSections.Num();

	ThreadCounts.Reserve(NumInvocations);
	for (int32 InvocationIndex = 0; InvocationIndex < NumInvocations; ++InvocationIndex)
	{
		FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];
		const int32 NumThreads = Domain == EOptimusSkinnedMeshExecDomain::Vertex ? RenderSection.NumVertices : RenderSection.NumTriangles;
		ThreadCounts.Add(FIntVector(NumThreads, 1, 1));
	}
	
	return NumInvocations;
}

bool FOptimusSkinnedMeshExecDataProviderProxy::IsValid(FValidationData const& InValidationData) const
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

void FOptimusSkinnedMeshExecDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const int32 LodIndex = SkeletalMeshObject->GetLOD();
	FSkeletalMeshRenderData const& SkeletalMeshRenderData = SkeletalMeshObject->GetSkeletalMeshRenderData();
	FSkeletalMeshLODRenderData const* LodRenderData = &SkeletalMeshRenderData.LODRenderData[LodIndex];

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		int32 NumThreads = 0;

		if (InDispatchData.bUnifiedDispatch)
		{
			NumThreads = Domain == EOptimusSkinnedMeshExecDomain::Vertex ? LodRenderData->GetNumVertices() : LodRenderData->GetTotalFaces();
		}
		else
		{
			FSkelMeshRenderSection const& RenderSection = LodRenderData->RenderSections[InvocationIndex];
			NumThreads = Domain == EOptimusSkinnedMeshExecDomain::Vertex ? RenderSection.NumVertices : RenderSection.NumTriangles;
		}

		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.NumThreads = FIntVector(NumThreads, 1, 1);
	}
}
