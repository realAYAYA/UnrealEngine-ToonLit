// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeformerDataInterfaceGroom.h"

#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "ComputeFramework/ComputeKernelPermutationVector.h"
#include "OptimusDataDomain.h"
#include "ShaderCompilerCore.h"
#include "ShaderParameterMetadataBuilder.h"
#include "GroomComponent.h"
#include "GroomInstance.h"
#include "RenderGraphBuilder.h"
#include "GlobalRenderResources.h"
#include "HairStrandsDefinitions.h"

FString UOptimusGroomDataInterface::GetDisplayName() const
{
	return TEXT("Groom");
}

TArray<FOptimusCDIPinDefinition> UOptimusGroomDataInterface::GetPinDefinitions() const
{
	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({"NumControlPoints", "ReadNumControlPoints"});
	Defs.Add({"NumCurves",        "ReadNumCurves" });
	Defs.Add({"Position",         "ReadPosition",          Optimus::DomainName::Vertex,   "ReadPosition"});
	Defs.Add({"Radius",           "ReadRadius",            Optimus::DomainName::Vertex,   "ReadRadius" });
	Defs.Add({"CoordU",           "ReadCoordU",            Optimus::DomainName::Vertex,   "ReadCoordU" });
	Defs.Add({"Length",           "ReadLength",            Optimus::DomainName::Vertex,   "ReadLength" });
	Defs.Add({"RootUV",           "ReadRootUV",            Optimus::DomainName::Vertex,   "ReadRootUV" });
	Defs.Add({"Seed",             "ReadSeed",              Optimus::DomainName::Vertex,   "ReadSeed"   });
	Defs.Add({"CurveOffsetPoint", "ReadCurveOffsetPoint",  Optimus::DomainName::Triangle, "ReadCurveOffsetPoint" });
	Defs.Add({"CurveNumPoint",    "ReadCurveNumPoint",     Optimus::DomainName::Triangle, "ReadCurveNumPoint" });
	return Defs;
}
 
TSubclassOf<UActorComponent> UOptimusGroomDataInterface::GetRequiredComponentClass() const
{
	return UGroomComponent::StaticClass();
}

void UOptimusGroomDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumControlPoints"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumCurves"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadPosition"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadRadius"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadCoordU"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadLength"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadRootUV"))
		.AddReturnType(EShaderFundamentalType::Float, 2)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadSeed"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadCurveOffsetPoint"))
		.AddReturnType(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadCurveNumPoint"))
		.AddReturnType(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint);
}

BEGIN_SHADER_PARAMETER_STRUCT(FGroomDataInterfaceParameters, )
	SHADER_PARAMETER(uint32, NumControlPoints)
	SHADER_PARAMETER(uint32, NumCurves)
	SHADER_PARAMETER(float, VF_HairRadius)
	SHADER_PARAMETER(float, VF_HairLength)
	SHADER_PARAMETER(float, VF_RootScale)
	SHADER_PARAMETER(float, VF_TipScale)
	SHADER_PARAMETER(uint32, VF_GroupIndex)
	SHADER_PARAMETER_ARRAY(FUintVector4, VF_AttributeOffsets, [HAIR_ATTRIBUTE_OFFSET_COUNT])
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint4>,     PositionBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, AttributeBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>,      VertexToCurveBuffer)
	SHADER_PARAMETER_RDG_BUFFER_SRV(Buffer<uint>,      CurveBuffer)
END_SHADER_PARAMETER_STRUCT()

void UOptimusGroomDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FGroomDataInterfaceParameters>(UID);
}

TCHAR const* UOptimusGroomDataInterface::TemplateFilePath = TEXT("/Plugin/Runtime/HairStrands/Private/DeformerDataInterfaceGroom.ush");

TCHAR const* UOptimusGroomDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UOptimusGroomDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusGroomDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusGroomDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusGroomDataProvider* Provider = NewObject<UOptimusGroomDataProvider>();
	Provider->Groom = Cast<UGroomComponent>(InBinding);
	return Provider;
}


FComputeDataProviderRenderProxy* UOptimusGroomDataProvider::GetRenderProxy()
{
	return new FOptimusGroomDataProviderProxy(Groom);
}


FOptimusGroomDataProviderProxy::FOptimusGroomDataProviderProxy(UGroomComponent* InGroomComponent)
{
	GroomComponent = InGroomComponent;
}

bool FOptimusGroomDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		return false;
	}
	if (GroomComponent == nullptr)
	{
		return false;
	}
	if (InValidationData.NumInvocations != GroomComponent->GetGroupCount())
	{
		return false;
	}
	
	return true;
}

void FOptimusGroomDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
{
	Resources.Empty();
	const uint32 InstanceCount = GroomComponent ? GroomComponent->GetGroupCount() : 0;
	for (uint32 Index =0; Index <InstanceCount;++Index)
	{
		if (FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(Index))
		{
			FResources& R = Resources.AddDefaulted_GetRef();
			R.PositionSRV		= Register(GraphBuilder, Instance->Strands.RestResource->PositionBuffer, ERDGImportedBufferFlags::CreateSRV).SRV;
			R.AttributeSRV		= Register(GraphBuilder, Instance->Strands.RestResource->AttributeBuffer, ERDGImportedBufferFlags::CreateSRV).SRV;
			R.VertexToCurveSRV	= Register(GraphBuilder, Instance->Strands.RestResource->VertexToCurveBuffer, ERDGImportedBufferFlags::CreateSRV).SRV;
			R.CurveSRV			= Register(GraphBuilder, Instance->Strands.RestResource->CurveBuffer, ERDGImportedBufferFlags::CreateSRV).SRV;
			R.FallbackSRV		= GraphBuilder.CreateSRV(GraphBuilder.RegisterExternalBuffer(GWhiteVertexBufferWithRDG->Buffer), PF_R16G16B16A16_UINT);
		}
	}
}

FHairGroupPublicData::FVertexFactoryInput ComputeHairStrandsVertexInputData(const FHairGroupInstance* Instance, EGroomViewMode ViewMode);
void FOptimusGroomDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const uint32 InstanceCount = GroomComponent ? GroomComponent->GetGroupCount() : 0;
	check(InDispatchData.NumInvocations == InstanceCount);
	check(uint32(Resources.Num()) == InstanceCount);

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		if (FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(InvocationIndex))
		{
			const bool bIsSRVValid = Resources[InvocationIndex].PositionSRV != nullptr;
			const int32 NumControlPoints = bIsSRVValid ? Instance->Strands.Data->PointCount : 0;
			const int32 NumCurves = bIsSRVValid ? Instance->Strands.Data->CurveCount : 0;

			const FHairGroupPublicData::FVertexFactoryInput VFInput = ComputeHairStrandsVertexInputData(Instance, EGroomViewMode::None);
			
			FParameters& Parameters = ParameterArray[InvocationIndex];
			Parameters.NumControlPoints = NumControlPoints;
			Parameters.NumCurves = NumCurves;
			Parameters.VF_HairRadius = VFInput.Strands.HairRadius;
			Parameters.VF_HairLength = VFInput.Strands.HairLength;
			Parameters.VF_RootScale = VFInput.Strands.HairRootScale;
			Parameters.VF_TipScale = VFInput.Strands.HairTipScale;
			Parameters.VF_GroupIndex = Instance->Debug.GroupIndex;
			PACK_HAIR_ATTRIBUTE_OFFSETS(Parameters.VF_AttributeOffsets, VFInput.Strands.AttributeOffsets);

			if (bIsSRVValid)
			{
				Parameters.PositionBuffer		= Resources[InvocationIndex].PositionSRV;
				Parameters.AttributeBuffer		= Resources[InvocationIndex].AttributeSRV;
				Parameters.VertexToCurveBuffer	= Resources[InvocationIndex].VertexToCurveSRV;
				Parameters.CurveBuffer			= Resources[InvocationIndex].CurveSRV;
			}
			else
			{
				Parameters.PositionBuffer		= Resources[InvocationIndex].FallbackSRV;
				Parameters.AttributeBuffer		= Resources[InvocationIndex].FallbackSRV;
				Parameters.VertexToCurveBuffer	= Resources[InvocationIndex].FallbackSRV;
				Parameters.CurveBuffer			= Resources[InvocationIndex].FallbackSRV;
			}
		}
	}
}