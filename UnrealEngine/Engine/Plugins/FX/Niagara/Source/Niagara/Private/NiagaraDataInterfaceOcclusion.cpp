// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceOcclusion.h"
#include "NiagaraTypes.h"
#include "NiagaraWorldManager.h"
#include "ShaderParameterUtils.h"
#include "Internationalization/Internationalization.h"
#include "NiagaraGPUSystemTick.h"
#include "NiagaraShaderParametersBuilder.h"
#include "NiagaraSystemGpuComputeProxy.h"
#include "NiagaraSystemInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceOcclusion)

#define LOCTEXT_NAMESPACE "NiagaraDataInterfaceOcclusion"

const TCHAR* UNiagaraDataInterfaceOcclusion::TemplateShaderFilePath = TEXT("/Plugin/FX/Niagara/Private/NiagaraDataInterfaceOcclusion.ush");
const FName UNiagaraDataInterfaceOcclusion::GetCameraOcclusionRectangleName(TEXT("QueryOcclusionFactorWithRectangleGPU"));
const FName UNiagaraDataInterfaceOcclusion::GetCameraOcclusionCircleName(TEXT("QueryOcclusionFactorWithCircleGPU"));

struct FNiagaraOcclusionDIFunctionVersion
{
	enum Type
	{
		InitialVersion = 0,
		LWCConversion = 1,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
};

UNiagaraDataInterfaceOcclusion::UNiagaraDataInterfaceOcclusion(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Proxy.Reset(new FNiagaraDataIntefaceProxyOcclusionQuery());
}

void UNiagaraDataInterfaceOcclusion::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void UNiagaraDataInterfaceOcclusion::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	FNiagaraFunctionSignature Sig;
	Sig.Name = GetCameraOcclusionRectangleName;
#if WITH_EDITORONLY_DATA
	Sig.Description = LOCTEXT("GetCameraOcclusionRectFunctionDescription", "This function returns the occlusion factor of a sprite. It samples the depth buffer in a rectangular grid around the given world position and compares each sample with the camera distance.");
	Sig.FunctionVersion = FNiagaraOcclusionDIFunctionVersion::LatestVersion;
#endif
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.bSupportsCPU = false;
	FText VisibilityFractionDescription = LOCTEXT("VisibilityFractionDescription", "Returns a value 0..1 depending on how many of the samples on the screen were occluded.\nFor example, a value of 0.3 means that 70% of visible samples were occluded.\nIf the sample fraction is 0 then this also returns 0.");
	FText SampleFractionDescription = LOCTEXT("SampleFractionDescription", "Returns a value 0..1 depending on how many samples were inside the viewport or outside of it.\nFor example, a value of 0.3 means that 70% of samples were outside the current viewport and therefore not visible.");
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Occlusion interface")));
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Sample Center World Position")), LOCTEXT("RectCenterPosDescription", "This world space position where the center of the sample rectangle should be."));
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Sample Window Width World")), LOCTEXT("SampleWindowWidthWorldDescription", "The total width of the sample rectangle in world space.\nIf the particle is a camera-aligned sprite then this is the sprite width."));
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Sample Window Height World")), LOCTEXT("SampleWindowHeightWorldDescription", "The total height of the sample rectangle in world space.\nIf the particle is a camera-aligned sprite then this is the sprite height."));
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Sample Steps Per Line")), LOCTEXT("StepsPerLineDescription", "The number of samples to take horizontally. The total number of samples is this value squared."));
	Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Visibility Fraction")), VisibilityFractionDescription);
	Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Sample Fraction")), SampleFractionDescription);
	OutFunctions.Add(Sig);

	Sig = FNiagaraFunctionSignature();
	Sig.Name = GetCameraOcclusionCircleName;
#if WITH_EDITORONLY_DATA
	Sig.Description = LOCTEXT("GetCameraOcclusionCircleFunctionDescription", "This function returns the occlusion factor of a sprite. It samples the depth buffer in concentric rings around the given world position and compares each sample with the camera distance.");
	Sig.FunctionVersion = FNiagaraOcclusionDIFunctionVersion::LatestVersion;
#endif
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.bSupportsCPU = false;
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("Occlusion interface")));
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Sample Center World Position")), LOCTEXT("CircleCenterPosDescription", "This world space position where the center of the sample circle should be."));
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Sample Window Diameter World")), LOCTEXT("SampleWindowDiameterDescription", "The world space diameter of the circle to sample.\nIf the particle is a spherical sprite then this is the sprite size."));
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Samples per ring")),LOCTEXT("SamplesPerRingDescription", "The number of samples for each ring inside the circle.\nThe total number of samples is NumRings * SamplesPerRing."));
	Sig.AddInput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Number of sample rings")), LOCTEXT("NumberOfSampleRingsDescription", "This number of concentric rings to sample inside the circle.\nThe total number of samples is NumRings * SamplesPerRing."));
	Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Visibility Fraction")), VisibilityFractionDescription);
	Sig.AddOutput(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Sample Fraction")), SampleFractionDescription);
	OutFunctions.Add(Sig);
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceOcclusion::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	if (!Super::AppendCompileHash(InVisitor))
		return false;

	InVisitor->UpdateString(TEXT("NiagaraDataInterfaceOcclusionHLSLSource"), GetShaderFileHash(TemplateShaderFilePath, EShaderPlatform::SP_PCD3D_SM5).ToString());
	InVisitor->UpdateShaderParameters<FShaderParameters>();
	return true;
}

void UNiagaraDataInterfaceOcclusion::GetCommonHLSL(FString& OutHLSL)
{
	OutHLSL += TEXT("#include \"/Engine/Private/DeferredShadingCommon.ush\"\n");
}

bool UNiagaraDataInterfaceOcclusion::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
	if ((FunctionInfo.DefinitionName == GetCameraOcclusionRectangleName) ||
		(FunctionInfo.DefinitionName == GetCameraOcclusionCircleName))
	{
		return true;
	}
	return false;
}

bool UNiagaraDataInterfaceOcclusion::UpgradeFunctionCall(FNiagaraFunctionSignature& FunctionSignature)
{
	if (FunctionSignature.FunctionVersion < FNiagaraOcclusionDIFunctionVersion::LatestVersion)
	{
		TArray<FNiagaraFunctionSignature> AllFunctions;
		GetFunctions(AllFunctions);
		for (const FNiagaraFunctionSignature& Sig : AllFunctions)
		{
			if (FunctionSignature.Name == Sig.Name)
			{
				FunctionSignature = Sig;
				return true;
			}
		}
	}

	return false;
}

void UNiagaraDataInterfaceOcclusion::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,	FString& OutHLSL)
{
	Super::GetParameterDefinitionHLSL(ParamInfo, OutHLSL);

	TMap<FString, FStringFormatArg> TemplateArgs =
	{
		{TEXT("ParameterName"),	ParamInfo.DataInterfaceHLSLSymbol},
	};

	FString TemplateFile;
	LoadShaderSourceFile(TemplateShaderFilePath, EShaderPlatform::SP_PCD3D_SM5, &TemplateFile, nullptr);
	OutHLSL += FString::Format(*TemplateFile, TemplateArgs);
}
#endif

void UNiagaraDataInterfaceOcclusion::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void UNiagaraDataInterfaceOcclusion::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();
	ShaderParameters->SystemLWCTile = Context.GetSystemLWCTile();
}

#undef LOCTEXT_NAMESPACE

