// Copyright Epic Games, Inc. All Rights Reserved.

#include "DeformerDataInterfaceGroomWrite.h"

#include "ComputeFramework/ShaderParamTypeDefinition.h"
#include "OptimusDataDomain.h"
#include "RenderGraphBuilder.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "ShaderParameterMetadataBuilder.h"
#include "SkeletalMeshDeformerHelpers.h"
#include "SkeletalRenderPublic.h"
#include "GroomComponent.h"
#include "GroomInstance.h"
#include "DeformerGroomComponentSource.h"
#include "RenderGraphUtils.h"
#include "SystemTextures.h"
#include "HairStrandsInterpolation.h"

FString UOptimusGroomWriteDataInterface::GetDisplayName() const
{
	return TEXT("Write Groom");
}

FName UOptimusGroomWriteDataInterface::GetCategory() const
{
	return CategoryName::OutputDataInterfaces;
}

TArray<FOptimusCDIPinDefinition> UOptimusGroomWriteDataInterface::GetPinDefinitions() const
{
	FName ControlPoint(UOptimusGroomComponentSource::Domains::ControlPoint);
	FName Curve(UOptimusGroomComponentSource::Domains::Curve);

	TArray<FOptimusCDIPinDefinition> Defs;
	Defs.Add({ "Position",         "WritePosition",         	ControlPoint, "ReadNumControlPoints" });
	Defs.Add({ "Radius",           "WriteRadius",           	ControlPoint, "ReadNumControlPoints" });
	Defs.Add({ "PositionAndRadius","WritePositionAndRadius",	ControlPoint, "ReadNumControlPoints" });

	Defs.Add({ "RootUV",			"WriteRootUV",   			Curve, 		  "ReadNumCurves" });
	Defs.Add({ "Seed",				"WriteSeed",   				Curve, 		  "ReadNumCurves" });
	Defs.Add({ "ClumpId",			"WriteClumpId",   			Curve, 		  "ReadNumCurves" });
	Defs.Add({ "Color",				"WriteColor",   			ControlPoint, "ReadNumControlPoints" });
	Defs.Add({ "Roughness",			"WriteRoughness",   		ControlPoint, "ReadNumControlPoints" });
	Defs.Add({ "AO",				"WriteAO",   				ControlPoint, "ReadNumControlPoints" });

	return Defs;
}

TSubclassOf<UActorComponent> UOptimusGroomWriteDataInterface::GetRequiredComponentClass() const
{
	return UGroomComponent::StaticClass();
}

void UOptimusGroomWriteDataInterface::GetSupportedInputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumControlPoints"))
		.AddReturnType(EShaderFundamentalType::Uint);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("ReadNumCurves"))
		.AddReturnType(EShaderFundamentalType::Uint);
}

void UOptimusGroomWriteDataInterface::GetSupportedOutputs(TArray<FShaderFunctionDefinition>& OutFunctions) const
{
	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WritePosition"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float, 3);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteRadius"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WritePositionAndRadius"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float, 4);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteRootUV"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float, 2);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteSeed"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteClumpId"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteColor"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float, 3);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteRoughness"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float);

	OutFunctions.AddDefaulted_GetRef()
		.SetName(TEXT("WriteAO"))
		.AddParam(EShaderFundamentalType::Uint)
		.AddParam(EShaderFundamentalType::Float);
}

BEGIN_SHADER_PARAMETER_STRUCT(FGroomWriteDataInterfaceParameters, )
	SHADER_PARAMETER_STRUCT_INCLUDE(FHairStrandsInstanceCommonParameters, Common)
	SHADER_PARAMETER(uint32, OutputStreamStart)
	SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer, PositionOffsetBufferSRV)
	SHADER_PARAMETER_RDG_BUFFER_SRV(ByteAddressBuffer, PositionBufferSRV)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, PositionBufferUAV)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, CurveAttributeBufferUAV)
	SHADER_PARAMETER_RDG_BUFFER_UAV(RWByteAddressBuffer, PointAttributeBufferUAV)
END_SHADER_PARAMETER_STRUCT()

void UOptimusGroomWriteDataInterface::GetShaderParameters(TCHAR const* UID, FShaderParametersMetadataBuilder& InOutBuilder, FShaderParametersMetadataAllocations& InOutAllocations) const
{
	InOutBuilder.AddNestedStruct<FGroomWriteDataInterfaceParameters>(UID);
}

TCHAR const* UOptimusGroomWriteDataInterface::TemplateFilePath = TEXT("/Plugin/Runtime/HairStrands/Private/DeformerDataInterfaceGroomWrite.ush");

TCHAR const* UOptimusGroomWriteDataInterface::GetShaderVirtualPath() const
{
	return TemplateFilePath;
}

void UOptimusGroomWriteDataInterface::GetShaderHash(FString& InOutKey) const
{
	GetShaderFileHash(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5).AppendString(InOutKey);
}

void UOptimusGroomWriteDataInterface::GetHLSL(FString& OutHLSL, FString const& InDataInterfaceName) const
{
	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{ TEXT("DataInterfaceName"), InDataInterfaceName },
	};

	FString TemplateFile;
	LoadShaderSourceFile(TemplateFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}

UComputeDataProvider* UOptimusGroomWriteDataInterface::CreateDataProvider(TObjectPtr<UObject> InBinding, uint64 InInputMask, uint64 InOutputMask) const
{
	UOptimusGroomWriteDataProvider* Provider = NewObject<UOptimusGroomWriteDataProvider>();
	Provider->GroomComponent = Cast<UGroomComponent>(InBinding);
	Provider->OutputMask = InOutputMask;
	return Provider;
}


FComputeDataProviderRenderProxy* UOptimusGroomWriteDataProvider::GetRenderProxy()
{
	return new FOptimusGroomWriteDataProviderProxy(GroomComponent, OutputMask);
}


FOptimusGroomWriteDataProviderProxy::FOptimusGroomWriteDataProviderProxy(UGroomComponent* InGroomComponent, uint64 InOutputMask)
{
	OutputMask = InOutputMask;
	const uint32 InstanceCount = InGroomComponent ? InGroomComponent->GetGroupCount() : 0;
	for (uint32 Index = 0; Index < InstanceCount; ++Index)
	{
		Instances.Add(InGroomComponent->GetGroupInstance(Index));
	}
}

bool FOptimusGroomWriteDataProviderProxy::IsValid(FValidationData const& InValidationData) const
{
	if (InValidationData.ParameterStructSize != sizeof(FParameters))
	{
		return false;
	}
	if (InValidationData.NumInvocations != Instances.Num())
	{
		return false;
	}

	return true;
}

void FOptimusGroomWriteDataProviderProxy::AllocateResources(FRDGBuilder& GraphBuilder)
{
	for (FHairGroupInstance* GroomInstance : Instances)
	{
		if (GroomInstance)
		{
			// Allocate required buffers
			const int32 NumControlPoints = GroomInstance->Strands.GetData().GetNumPoints();
			const int32 NumCurves = GroomInstance->Strands.GetData().GetNumCurves();

			FResources& R = Resources.AddDefaulted_GetRef();

			R.PositionBufferSRV_fallback = GraphBuilder.CreateSRV(GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 16u));
			R.PositionBufferUAV_fallback = GraphBuilder.CreateUAV(GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(16u), TEXT("Groom.Deformer.FallbackDeformedPositionBuffer")), ERDGUnorderedAccessViewFlags::SkipBarrier);
			R.AttributeBufferUAV_fallback = GraphBuilder.CreateUAV(GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(16u), TEXT("Groom.Deformer.FallbackDeformedAttributeBuffer")), ERDGUnorderedAccessViewFlags::SkipBarrier);

			// Positions / Radius
			if (OutputMask & 0x7)
			{
				R.PositionOffsetBufferSRV 	= Register(GraphBuilder, GroomInstance->Strands.RestResource->PositionOffsetBuffer, ERDGImportedBufferFlags::CreateSRV).SRV;
				R.PositionBufferSRV 		= Register(GraphBuilder, GroomInstance->Strands.RestResource->PositionBuffer, ERDGImportedBufferFlags::CreateSRV).SRV;
				R.PositionBufferUAV 		= Register(GraphBuilder, GroomInstance->Strands.DeformedResource->GetDeformerBuffer(GraphBuilder), ERDGImportedBufferFlags::CreateUAV).UAV;
			}
			else
			{
				R.PositionOffsetBufferSRV 	= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultStructuredBuffer(GraphBuilder, 16u), PF_A32B32G32R32F);
				R.PositionBufferSRV 		= GraphBuilder.CreateSRV(GSystemTextures.GetDefaultByteAddressBuffer(GraphBuilder, 16u));
				R.PositionBufferUAV 		= GraphBuilder.CreateUAV(GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateByteAddressDesc(16u), TEXT("Hair.Deformer.DummyBuffer")));
			}
			
			// Curve Attributes
			if (OutputMask & 0x38)
			{
				FRDGExternalBuffer CurveAttributeBufferExt = GroomInstance->Strands.DeformedResource->GetDeformerCurveAttributeBuffer(GraphBuilder);
				if (CurveAttributeBufferExt.Buffer)
				{
					// Always copy the attributes from the rest asset, so that if deformer write different attribute at different tick, it all remains consistent with the source data
					FRDGImportedBuffer DstCurveAttributeBuffer = Register(GraphBuilder, CurveAttributeBufferExt, ERDGImportedBufferFlags::CreateUAV);
					FRDGImportedBuffer SrcCurveAttributeBuffer = Register(GraphBuilder, GroomInstance->Strands.RestResource->CurveAttributeBuffer, ERDGImportedBufferFlags::None);
					if (SrcCurveAttributeBuffer.Buffer)
					{
						AddCopyBufferPass(GraphBuilder, DstCurveAttributeBuffer.Buffer, SrcCurveAttributeBuffer.Buffer);
					}
					R.CurveAttributeBufferUAV = DstCurveAttributeBuffer.UAV;
				}
			}
			if (R.CurveAttributeBufferUAV == nullptr)
			{
				R.CurveAttributeBufferUAV = R.AttributeBufferUAV_fallback;
			}

			// Point Attributes
			if (OutputMask & 0x1c0)
			{
				FRDGExternalBuffer PointAttributeBufferExt = GroomInstance->Strands.DeformedResource->GetDeformerPointAttributeBuffer(GraphBuilder);
				if (PointAttributeBufferExt.Buffer)
				{
					FRDGImportedBuffer PointAttributeBuffer = Register(GraphBuilder, PointAttributeBufferExt, ERDGImportedBufferFlags::CreateUAV);
					AddCopyBufferPass(GraphBuilder, PointAttributeBuffer.Buffer, Register(GraphBuilder, GroomInstance->Strands.RestResource->PointAttributeBuffer, ERDGImportedBufferFlags::None).Buffer);
					R.PointAttributeBufferUAV = PointAttributeBuffer.UAV;
				}
			}
			if (R.PointAttributeBufferUAV == nullptr)
			{
				R.PointAttributeBufferUAV = R.AttributeBufferUAV_fallback;
				//R.PointAttributeBufferUAV = GraphBuilder.CreateUAV(GraphBuilder.RegisterExternalBuffer(GWhiteVertexBufferWithRDG->Buffer), PF_R32_UINT);
			}
		}	
	}
}

void FOptimusGroomWriteDataProviderProxy::GatherDispatchData(FDispatchData const& InDispatchData)
{
	const TStridedView<FParameters> ParameterArray = MakeStridedParameterView<FParameters>(InDispatchData);
	for (int32 InvocationIndex = 0; InvocationIndex < ParameterArray.Num(); ++InvocationIndex)
	{
		if (FHairGroupInstance* GroomInstance = Instances[InvocationIndex])
		{
			FResources& Resource = Resources[InvocationIndex];

			const bool bValid = Resource.PositionBufferUAV != nullptr && Resource.PositionBufferSRV != nullptr && Resource.PositionOffsetBufferSRV != nullptr;

			const FHairGroupPublicData::FVertexFactoryInput VFInput = ComputeHairStrandsVertexInputData(GroomInstance, EGroomViewMode::None);		

			FParameters& Parameters = ParameterArray[InvocationIndex];
			Parameters.Common 					= VFInput.Strands.Common;
			Parameters.OutputStreamStart 		= 0;
			Parameters.PositionOffsetBufferSRV 	= bValid ? Resource.PositionOffsetBufferSRV : Resource.PositionBufferSRV_fallback;
			Parameters.PositionBufferSRV 		= bValid ? Resource.PositionBufferSRV : Resource.PositionBufferSRV_fallback;
			Parameters.PositionBufferUAV 		= bValid ? Resource.PositionBufferUAV : Resource.PositionBufferUAV_fallback;
			Parameters.PointAttributeBufferUAV 	= bValid ? Resource.PointAttributeBufferUAV : Resource.AttributeBufferUAV_fallback;
			Parameters.CurveAttributeBufferUAV 	= bValid ? Resource.CurveAttributeBufferUAV : Resource.AttributeBufferUAV_fallback;
		}
	}
}