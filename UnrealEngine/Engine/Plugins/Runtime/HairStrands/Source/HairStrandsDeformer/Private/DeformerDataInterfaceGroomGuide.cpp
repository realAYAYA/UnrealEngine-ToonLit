// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeformerDataInterfaceGroomGuide.h"

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

FString UOptimusGroomGuideDataInterface::GetDisplayName() const
{
	return TEXT("Guides");
}

TArray<FOptimusCDIPinDefinition> UOptimusGroomGuideDataInterface::GetPinDefinitions() const
{
	FName ControlPoint(UOptimusGroomComponentSource::Domains::ControlPoint);
	FName Curve(UOptimusGroomComponentSource::Domains::Curve);

	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({ "NumGuidePoints",   "ReadNumGuidePoints" });
	Defs.Add({ "NumGuideCurves",   "ReadNumGuideCurves" });
	Defs.Add({ "Position",         "ReadPosition",          ControlPoint,   "ReadPosition" });
	Defs.Add({ "CurveOffsetPoint", "ReadCurveOffsetPoint",  Curve,          "ReadCurveOffsetPoint" });
	Defs.Add({ "CurveNumPoint",    "ReadCurveNumPoint",     Curve,          "ReadCurveNumPoint" });
	return Defs;
}
 
TSubclassOf<UActorComponent> UOptimusGroomGuideDataInterface::GetRequiredComponentClass() const
{
	return UGroomComponent::StaticClass();
}

void UOptimusGroomGuideDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumGuidePoints"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumGuideCurves"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadPosition"))
		.AddReturnType(EShaderFundamentalType::Float, 3)
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

BEGIN_SHADER_PARAMETER_STRUCT(FGroomGuideDataInterfaceParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceCommonParameters, Common)
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceResourceParameters, Resources)
END_SHADER_PARAMETER_STRUCT()

void UOptimusGroomGuideDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FGroomGuideDataInterfaceParameters>(UID);
}

TCHAR const* UOptimusGroomGuideDataInterface::TemplateFilePath = TEXT("/Plugin/Runtime/HairStrands/Private/DeformerDataInterfaceGroomGuide.ush");

TCHAR const* UOptimusGroomGuideDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UOptimusGroomGuideDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusGroomGuideDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusGroomGuideDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusGroomGuideDataProvider* Provider = NewObject<UOptimusGroomGuideDataProvider>();
	Provider->Groom = Cast<UGroomComponent>(InBinding);
	return Provider;
}


FComputeDataProviderRenderProxy* UOptimusGroomGuideDataProvider::GetRenderProxy()
{
	return new FOptimusGroomGuideDataProviderProxy(Groom);
}


FOptimusGroomGuideDataProviderProxy::FOptimusGroomGuideDataProviderProxy(UGroomComponent* InGroomComponent)
{
	GroomComponent = InGroomComponent;
}

bool FOptimusGroomGuideDataProviderProxy::IsValid(FValidationData const& InValidationData) const
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

void FOptimusGroomGuideDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
{
	Resources.Empty();
	FallbackByteAddressSRV = nullptr;
	FallbackStructuredSRV = nullptr;
	const uint32 InstanceCount = GroomComponent ? GroomComponent->GetGroupCount() : 0;
	for (uint32 Index =0; Index <InstanceCount;++Index)
	{
		if (FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(Index))
		{
			FHairStrandsInstanceResourceParameters& R = Resources.AddDefaulted_GetRef();
			if (Instance->Guides.RestResource)
			{
				R.PositionBuffer		= RegisterAsSRV(GraphBuilder, Instance->Guides.RestResource->PositionBuffer);
				R.PositionOffsetBuffer 	= RegisterAsSRV(GraphBuilder, Instance->Guides.RestResource->PositionOffsetBuffer);
				R.CurveBuffer			= RegisterAsSRV(GraphBuilder, Instance->Guides.RestResource->CurveBuffer);
				R.PointToCurveBuffer	= RegisterAsSRV(GraphBuilder, Instance->Guides.RestResource->PointToCurveBuffer);
			}

			if (!FallbackByteAddressSRV){ FallbackByteAddressSRV = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 16u)); }
			if (!FallbackStructuredSRV) { FallbackStructuredSRV  = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 16u)); }
		}
	}
}

void FOptimusGroomGuideDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const uint32 InstanceCount = GroomComponent ? GroomComponent->GetGroupCount() : 0;
	check(InDispatchData.NumInvocations == InstanceCount);
	check(uint32(Resources.Num()) == InstanceCount);

	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		// Default valid in case the instance is not valid
		FParameters& Parameters = ParameterArray[InvocationIndex];
		Parameters.Common.PointCount 				= 0;
		Parameters.Common.CurveCount 				= 0;
		Parameters.Resources.PositionBuffer			= FallbackByteAddressSRV;
		Parameters.Resources.PositionOffsetBuffer 	= FallbackStructuredSRV;
		Parameters.Resources.PointToCurveBuffer		= FallbackByteAddressSRV;
		Parameters.Resources.CurveBuffer			= FallbackByteAddressSRV;

		if (FHairGroupInstance* Instance = GroomComponent->GetGroupInstance(InvocationIndex))
		{
			const bool bIsSRVValid = Resources[InvocationIndex].PositionBuffer != nullptr;
			if (bIsSRVValid)
			{
				// Reuse the strands properties for guides, and apply guides values
				const FHairGroupPublicData::FVertexFactoryInput VFInput = ComputeHairStrandsVertexInputData(Instance, EGroomViewMode::None);
				Parameters.Common 				= VFInput.Strands.Common;
				Parameters.Common.PointCount 	= Instance->Guides.IsValid() ? Instance->Guides.GetData().Header.PointCount : 0;
				Parameters.Common.CurveCount 	= Instance->Guides.IsValid() ? Instance->Guides.GetData().Header.CurveCount : 0;
				Parameters.Resources 			= Resources[InvocationIndex];
			}
		}
	}
}