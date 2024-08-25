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
#include "DeformerGroomComponentSource.h"
#include "HairStrandsInterpolation.h"
#include "SystemTextures.h"

FString UOptimusGroomDataInterface::GetDisplayName() const
{
	return TEXT("Groom");
}

TArray<FOptimusCDIPinDefinition> UOptimusGroomDataInterface::GetPinDefinitions() const
{
	FName ControlPoint(UOptimusGroomComponentSource::Domains::ControlPoint);
	FName Curve(UOptimusGroomComponentSource::Domains::Curve);

	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({ "NumControlPoints", "ReadNumControlPoints" });
	Defs.Add({ "NumCurves",        "ReadNumCurves" });
	Defs.Add({ "Position",         "ReadPosition",          ControlPoint,   "ReadPosition" });
	Defs.Add({ "Radius",           "ReadRadius",            ControlPoint,   "ReadRadius" });
	Defs.Add({ "CoordU",           "ReadCoordU",            ControlPoint,   "ReadCoordU" });
	Defs.Add({ "Length",           "ReadLength",            ControlPoint,   "ReadLength" });
	Defs.Add({ "RootUV",           "ReadRootUV",            ControlPoint,   "ReadRootUV" });
	Defs.Add({ "Seed",             "ReadSeed",              ControlPoint,   "ReadSeed" });
	Defs.Add({ "ClumpId",          "ReadClumpId",           ControlPoint,   "ReadClumpId" });
	Defs.Add({ "Color",            "ReadColor",             ControlPoint,   "ReadColor" });
	Defs.Add({ "Roughness",        "ReadRoughness",         ControlPoint,   "ReadRoughness" });
	Defs.Add({ "AO",               "ReadAO",                ControlPoint,   "ReadAO" });
	Defs.Add({ "CurveOffsetPoint", "ReadCurveOffsetPoint",  Curve,          "ReadCurveOffsetPoint" });
	Defs.Add({ "CurveNumPoint",    "ReadCurveNumPoint",     Curve,          "ReadCurveNumPoint" });
	Defs.Add({ "GuideIndex",       "ReadGuideIndex",        ControlPoint,   "ReadGuideIndex" });
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
		.SetName(TEXT("ReadClumpId"))
		.AddReturnType(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadColor"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadRoughnes"))
		.AddReturnType(EShaderFundamentalType::Float)
		.AddParam(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadAO"))
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

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadGuideIndex"))
		.AddReturnType(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Uint);
}

BEGIN_SHADER_PARAMETER_STRUCT(FGroomDataInterfaceParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceCommonParameters, Common)
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceResourceParameters, Resources)
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceInterpolationParameters, Interpolation)
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
	const uint32 GroupCount = GroomComponent->GetGroupCount();
	if (InValidationData.NumInvocations != GroupCount)
	{
		return false;
	}
	// Earlier invalidation in case one of the instance is not ready/valid
	for (uint32 GroupIndex = 0; GroupIndex < GroupCount; ++GroupIndex)
	{
		if (GroomComponent->GetGroupInstance(GroupIndex) == nullptr)
		{
			return false;
		}
	}
	
	return true;
}

void FOptimusGroomDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
{
	Resources.Empty();
	FallbackByteAddressSRV = nullptr;
	FallbackStructuredSRV = nullptr;
	const uint32 InstanceCount = GroomComponent ? GroomComponent->GetGroupCount() : 0;
	for (uint32 Index =0; Index <InstanceCount;++Index)
	{
		if (FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(Index))
		{
			{
				FHairStrandsInstanceResourceParameters& R = Resources.AddDefaulted_GetRef();
				R.PositionBuffer		= RegisterAsSRV(GraphBuilder, Instance->Strands.RestResource->PositionBuffer);
				R.PositionOffsetBuffer 	= RegisterAsSRV(GraphBuilder, Instance->Strands.RestResource->PositionOffsetBuffer);
				R.CurveBuffer			= RegisterAsSRV(GraphBuilder, Instance->Strands.RestResource->CurveBuffer);
				R.PointToCurveBuffer	= RegisterAsSRV(GraphBuilder, Instance->Strands.RestResource->PointToCurveBuffer);
				R.CurveAttributeBuffer	= RegisterAsSRV(GraphBuilder, Instance->Strands.RestResource->CurveAttributeBuffer);
				R.PointAttributeBuffer	= RegisterAsSRV(GraphBuilder, Instance->Strands.RestResource->PointAttributeBuffer);
			}

			{
				FHairStrandsInstanceInterpolationParameters& R = Interpolations.AddDefaulted_GetRef();
				R.CurveInterpolationBuffer = Instance->Strands.InterpolationResource ? RegisterAsSRV(GraphBuilder, Instance->Strands.InterpolationResource->CurveInterpolationBuffer) : nullptr;
				R.PointInterpolationBuffer = Instance->Strands.InterpolationResource ? RegisterAsSRV(GraphBuilder, Instance->Strands.InterpolationResource->PointInterpolationBuffer) : nullptr;
			}

			if (!FallbackByteAddressSRV){ FallbackByteAddressSRV = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 16u)); }
			if (!FallbackStructuredSRV) { FallbackStructuredSRV  = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 16u)); }
		}
	}
}

void FOptimusGroomDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const uint32 InstanceCount = GroomComponent ? GroomComponent->GetGroupCount() : 0;
	check(InDispatchData.NumInvocations == InstanceCount);
	check(uint32(Resources.Num()) == InstanceCount);

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		// Default valid in case the instance is not valid
		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.Common.PointCount = 0;
		Parameters.Common.CurveCount = 0;
		Parameters.Resources.PositionBuffer			= FallbackByteAddressSRV;
		Parameters.Resources.PositionOffsetBuffer 	= FallbackStructuredSRV;
		Parameters.Resources.CurveAttributeBuffer	= FallbackByteAddressSRV;
		Parameters.Resources.PointAttributeBuffer	= FallbackByteAddressSRV;
		Parameters.Resources.PointToCurveBuffer		= FallbackByteAddressSRV;
		Parameters.Resources.CurveBuffer			= FallbackByteAddressSRV;
		Parameters.Interpolation.CurveInterpolationBuffer = FallbackByteAddressSRV;
		Parameters.Interpolation.PointInterpolationBuffer = FallbackByteAddressSRV;

		if (FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(InvocationIndex))
		{
			const FHairGroupPublicData::FVertexFactoryInput VFInput = ComputeHairStrandsVertexInputData(Instance, EGroomViewMode::None);
			Parameters.Common = VFInput.Strands.Common;

			if (Resources[InvocationIndex].PositionBuffer != nullptr)
			{
				Parameters.Resources = Resources[InvocationIndex];
			}
			if (Interpolations[InvocationIndex].CurveInterpolationBuffer != nullptr)
			{
				Parameters.Interpolation = Interpolations[InvocationIndex];
			}
		}
	}
}