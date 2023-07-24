// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HLSLMaterialTranslator.cpp: Translates material expressions into HLSL code.
=============================================================================*/

#include "HLSLMaterialTranslator.h"
#include "Containers/LazyPrintf.h"
#include "VT/VirtualTextureScalability.h"
#include "Engine/Engine.h"
#include "MaterialDomain.h"
#include "Engine/Texture.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Field/FieldSystemTypes.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Materials/MaterialExpressionMaterialAttributeLayers.h"
#include "Materials/HLSLMaterialDerivativeAutogen.h"
#include "Materials/MaterialFunction.h"
#include "MaterialExpressionSettings.h"
#include "Engine/RendererSettings.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Materials/Material.h"
#include <functional>
#include "Materials/MaterialExpressionAbsorptionMediumMaterialOutput.h"
#include "Materials/MaterialExpressionCustomOutput.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionSingleLayerWaterMaterialOutput.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionThinTranslucentMaterialOutput.h"
#include "Materials/MaterialExpressionVectorNoise.h"
#include "Materials/MaterialExpressionViewProperty.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialOutput.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/StrataMaterial.h"
#include "ParameterCollection.h"
#include "RenderUtils.h"
#include "Stats/StatsMisc.h"
#include "Stats/StatsTrace.h"
#include "StrataDefinitions.h"
#include "VT/RuntimeVirtualTexture.h"
#include <memory>
#include <tuple>

#if WITH_EDITORONLY_DATA
#include "Materials/MaterialExpressionStrata.h"
#include "ShaderPlatformCachedIniValue.h"
#endif

static int32 GetLWCTruncateMode()
{
	static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.MaterialEditor.LWCTruncateMode"));
	const int32 LWCTruncateValue = CVar ? CVar->GetValueOnAnyThread() : 0;
	return LWCTruncateValue;
}

bool IsExpressionClassPermitted(const UClass* const Class)
{
	if (Class)
	{
		// Always allow cooked content
		if (Class->bCooked)
		{
			return true;
		}

#if WITH_EDITOR
		// Uncooked content must be explicitly allowed if in restrictive mode
		const bool bIsClassPathAllowed = FMaterialExpressionSettings::Get()->IsClassPathAllowed(FTopLevelAssetPath(Class->GetPathName()));
		if (!bIsClassPathAllowed)
		{
			return false;
		}
#endif
	}
	return true;
}

#if WITH_EDITORONLY_DATA

static int32 GAnalyticDerivEnabled = 1;
static FAutoConsoleVariableRef CVarAnalyticDerivEnabled(
	TEXT("r.MaterialEditor.AnalyticDeriv"),
	GAnalyticDerivEnabled,
	TEXT("Enable analytic derivative code generation.")
	);

// Debugging options
static int32 GDebugTextureSampleEnabled = 0;
static FAutoConsoleVariableRef CVarAnalyticDerivDebugTextureSample(
	TEXT("r.MaterialEditor.AnalyticDeriv.DebugTextureSample"),
	GDebugTextureSampleEnabled,
	TEXT("Debug: Instrument texture sampling with modes that can be controlled with r.GeneralPurposeTweak/r.GeneralPurposeTweak2.")
);

static int32 GDebugEmitInvalidDerivTokensEnabled = 0;
static FAutoConsoleVariableRef CVarAnalyticDerivDebugEmitInvalidDerivTokens(
	TEXT("r.MaterialEditor.AnalyticDeriv.DebugEmitInvalidDerivTokens"),
	GDebugEmitInvalidDerivTokensEnabled,
	TEXT("Debug: Emit '$' tokens to mark expressions with invalid derivatives.\n")
);

static int32 GLWCEnabled = 1;
static FAutoConsoleVariableRef CVarLWCEnabled(
	TEXT("r.MaterialEditor.LWCEnabled"),
	GLWCEnabled,
	TEXT("Enable generation of LWC values in materials. If disabled, materials will perform all operations at float-precision")
);

static bool GPedanticErrorChecksEnabled = false;
static FAutoConsoleVariableRef CVarPedanticErrorChecksEnabled(
	TEXT("r.Material.PedanticErrorChecksEnabled"),
	GPedanticErrorChecksEnabled,
	TEXT("Enables material compilation pedantic error checking"));

static inline bool IsAnalyticDerivEnabled()
{
	return GAnalyticDerivEnabled != 0;
}

static inline bool IsDebugTextureSampleEnabled()
{
	return IsAnalyticDerivEnabled() && (GDebugTextureSampleEnabled != 0);
}

#define DEBUG_STRATA_TREE_STACK 0

/** @return the vector type containing a given number of components. */
static inline EMaterialValueType GetVectorType(uint32 NumComponents)
{
	switch(NumComponents)
	{
		case 1: return MCT_Float;
		case 2: return MCT_Float2;
		case 3: return MCT_Float3;
		case 4: return MCT_Float4;
		default: return MCT_Unknown;
	};
}

static inline EMaterialValueType GetLWCVectorType(uint32 NumComponents)
{
	switch (NumComponents)
	{
	case 1: return MCT_LWCScalar;
	case 2: return MCT_LWCVector2;
	case 3: return MCT_LWCVector3;
	case 4: return MCT_LWCVector4;
	default: return MCT_Unknown;
	};
}

static inline EMaterialValueType GetMaterialValueType(UE::Shader::EValueType Type)
{
	switch (Type)
	{
	case UE::Shader::EValueType::Void: return MCT_VoidStatement;

	case UE::Shader::EValueType::Float1: return MCT_Float;
	case UE::Shader::EValueType::Float2: return MCT_Float2;
	case UE::Shader::EValueType::Float3: return MCT_Float3;
	case UE::Shader::EValueType::Float4: return MCT_Float4;

	case UE::Shader::EValueType::Double1: return MCT_LWCScalar;
	case UE::Shader::EValueType::Double2: return MCT_LWCVector2;
	case UE::Shader::EValueType::Double3: return MCT_LWCVector3;
	case UE::Shader::EValueType::Double4: return MCT_LWCVector4;

	case UE::Shader::EValueType::Int1: return MCT_Float;
	case UE::Shader::EValueType::Int2: return MCT_Float2;
	case UE::Shader::EValueType::Int3: return MCT_Float3;
	case UE::Shader::EValueType::Int4: return MCT_Float4;

	case UE::Shader::EValueType::Bool1: return MCT_Float;
	case UE::Shader::EValueType::Bool2: return MCT_Float2;
	case UE::Shader::EValueType::Bool3: return MCT_Float3;
	case UE::Shader::EValueType::Bool4: return MCT_Float4;

	case UE::Shader::EValueType::Struct: return MCT_MaterialAttributes;
	default: checkNoEntry(); return MCT_Unknown;
	}
}

static inline EMaterialValueType GetMaterialValueType(EMaterialParameterType Type)
{
	switch (Type)
	{
	case EMaterialParameterType::Scalar: return MCT_Float;
	case EMaterialParameterType::Vector: return MCT_Float4;
	case EMaterialParameterType::DoubleVector: return MCT_LWCVector4;
	case EMaterialParameterType::StaticSwitch: return MCT_Bool;
	default: checkNoEntry(); return MCT_Unknown;
	}
}

static inline int32 SwizzleComponentToIndex(TCHAR Component)
{
	switch (Component)
	{
	case TCHAR('x'): case TCHAR('X'): case TCHAR('r'): case TCHAR('R'): return 0;
	case TCHAR('y'): case TCHAR('Y'): case TCHAR('g'): case TCHAR('G'): return 1;
	case TCHAR('z'): case TCHAR('Z'): case TCHAR('b'): case TCHAR('B'): return 2;
	case TCHAR('w'): case TCHAR('W'): case TCHAR('a'): case TCHAR('A'): return 3;
	default:
		return -1;
	}
}

static inline const TCHAR * GetFloatZeroVector(uint32 NumComponents)
{
	switch (NumComponents)
	{
	case 1:
		return TEXT("0.0f");
	case 2:
		return TEXT("float2(0.0f, 0.0f)");
	case 3:
		return TEXT("float3(0.0f, 0.0f, 0.0f)");
	case 4:
		return TEXT("float4(0.0f, 0.0f, 0.0f, 0.0f)");
	default:
		check(0);
		return TEXT("");
	}
}

static inline const TCHAR* GetStrataOperatorStr(int32 OperatorType)
{
	switch (OperatorType)
	{
	case STRATA_OPERATOR_WEIGHT:
	{
		return TEXT("WEIGHT    ");
	}
	case STRATA_OPERATOR_VERTICAL:
	{
		return TEXT("VERTICAL  ");
	}
	case STRATA_OPERATOR_HORIZONTAL:
	{
		return TEXT("HORIZONTAL");
	}
	case STRATA_OPERATOR_ADD:
	{
		return TEXT("ADD       ");
	}
	case STRATA_OPERATOR_BSDF:
	{
		return TEXT("BSDF      ");
	}
	case STRATA_OPERATOR_BSDF_LEGACY:
	{
		return TEXT("BSDFLEGACY");
	}
	}
	return TEXT("UNKNOWN   ");
};


FHLSLMaterialTranslator::FHLSLMaterialTranslator(FMaterial* InMaterial,
	FMaterialCompilationOutput& InMaterialCompilationOutput,
	const FStaticParameterSet& InStaticParameters,
	EShaderPlatform InPlatform,
	EMaterialQualityLevel::Type InQualityLevel,
	ERHIFeatureLevel::Type InFeatureLevel,
	const ITargetPlatform* InTargetPlatform, //if InTargetPlatform is nullptr, we use the current active
	const FStrataCompilationConfig* InStrataCompilationConfig)
:	ShaderFrequency(SF_Pixel)
,	MaterialProperty(MP_EmissiveColor)
,	CurrentScopeChunks(nullptr)
,	CurrentScopeID(0u)
,	NextTempScopeID(SF_NumFrequencies)
,	Material(InMaterial)
,	MaterialCompilationOutput(InMaterialCompilationOutput)
,	StaticParameters(InStaticParameters)
,	Platform(InPlatform)
,	QualityLevel(InQualityLevel)
,	FeatureLevel(InFeatureLevel)
,	MaterialTemplateLineNumber(INDEX_NONE)
,	NextSymbolIndex(INDEX_NONE)
,	NextVertexInterpolatorIndex(0)
,	CurrentCustomVertexInterpolatorOffset(0)
,	CompileErrorsSink(nullptr)
,	CompileErrorExpressionsSink(nullptr)
,	bSuccess(false)
,	bCompileForComputeShader(false)
,	bUsesSceneDepth(false)
,	bNeedsParticlePosition(false)
,	bNeedsParticleVelocity(false)
,	bNeedsParticleTime(false)
,	bUsesParticleMotionBlur(false)
,	bNeedsParticleRandom(false)
,	bUsesSphericalParticleOpacity(false)
,   bUsesParticleSubUVs(false)
,	bUsesLightmapUVs(false)
,	bUsesAOMaterialMask(false)
,	bUsesSpeedTree(false)
,	bNeedsWorldPositionExcludingShaderOffsets(false)
,	bNeedsParticleSize(false)
,	bNeedsSceneTexturePostProcessInputs(false)
,	bUsesSkyAtmosphere(false)
,	bUsesVertexColor(false)
,	bUsesParticleColor(false)
,	bUsesParticleLocalToWorld(false)
,	bUsesParticleWorldToLocal(false)
,	bUsesInstanceLocalToWorldPS(false)
,	bUsesInstanceWorldToLocalPS(false)
,	bUsesVertexPosition(false)
,	bUsesTransformVector(false)
,	bCompilingPreviousFrame(false)
,	bOutputsBasePassVelocities(true)
,	bUsesPixelDepthOffset(false)
,   bUsesWorldPositionOffset(false)
,	bUsesEmissiveColor(false)
,	bUsesDistanceCullFade(false)
,	bIsFullyRough(0)
,	bAllowCodeChunkGeneration(true)
,	bUsesAnisotropy(false)
,	bMaterialIsStrata(false)
,	bUsesCurvature(false)
,	AllocatedUserTexCoords()
,	AllocatedUserVertexTexCoords()
,	DynamicParticleParameterMask(0)
,	NextFreeStrataShaderNormalIndex(0)
,	FinalUsedSharedLocalBasesCount(0)
,	NumVtSamples(0)
,	TargetPlatform(InTargetPlatform)
,	StrataCompilationConfig()
{
	FMemory::Memzero(SharedPixelProperties);

	FMemory::Memzero(NumForLoops);

	FMemory::Memzero(MaterialAttributesReturned);

	InStaticParameters.GetMaterialLayers(CachedMaterialLayers);

	SharedPixelProperties[MP_Normal] = true;
	SharedPixelProperties[MP_Tangent] = true;
	SharedPixelProperties[MP_EmissiveColor] = true;
	SharedPixelProperties[MP_Opacity] = true;
	SharedPixelProperties[MP_OpacityMask] = true;
	SharedPixelProperties[MP_BaseColor] = true;
	SharedPixelProperties[MP_Metallic] = true;
	SharedPixelProperties[MP_Specular] = true;
	SharedPixelProperties[MP_Roughness] = true;
	SharedPixelProperties[MP_Anisotropy] = true;
	SharedPixelProperties[MP_AmbientOcclusion] = true;
	SharedPixelProperties[MP_Refraction] = true;
	SharedPixelProperties[MP_PixelDepthOffset] = true;
	SharedPixelProperties[MP_SubsurfaceColor] = true;
	SharedPixelProperties[MP_ShadingModel] = true;
	SharedPixelProperties[MP_SurfaceThickness] = true;
	SharedPixelProperties[MP_FrontMaterial] = true;

	for (int32 Frequency = 0; Frequency < SF_NumFrequencies; ++Frequency)
	{
		FunctionStacks[Frequency].Add(new FMaterialFunctionCompileState(nullptr));
	}

	// Default value for attribute stack added to simplify code when compiling new attributes, see SetMaterialProperty.
	const FGuid& MissingAttribute = FMaterialAttributeDefinitionMap::GetID(MP_MAX);
	MaterialAttributesStack.Add(MissingAttribute);

	// Default owner for parameters
	ParameterOwnerStack.Add(FMaterialParameterInfo());

	if (TargetPlatform == nullptr)
	{
		ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
		if (TPM)
		{
			TargetPlatform = TPM->GetRunningTargetPlatform();
		}
	}

	StrataMaterialRootOperator = nullptr;
	StrataMaterialExpressionRegisteredOperators.Reserve(STRATA_MAX_OPERATOR_COUNT);
	StrataMaterialExpressionToOperatorIndex.Reserve(STRATA_MAX_OPERATOR_COUNT);
	StrataMaterialBSDFCount = 0;
	StrataMaterialRequestedSizeByte = 0;
	bStrataMaterialIsSimple = false;
	bStrataMaterialIsSingle = false;
	bStrataMaterialIsUnlitNode = false;
	bStrataUsesConversionFromLegacy = false;
	bStrataOutputsOpaqueRoughRefractions = false;

	// Default value used as the root of the tree for the first path (when a node parent==nullptr).
	StrataNodeIdentifierStack.Push(FGuid(0x7AEE, 0xBAD, 0xDEAD, 0xBEEF));
	bStrataTreeOutOfStackDepthOccurred = false;

	if (InStrataCompilationConfig)
	{
		StrataCompilationConfig = *InStrataCompilationConfig;
	}
}

FHLSLMaterialTranslator::~FHLSLMaterialTranslator()
{
	ClearAllFunctionStacks();
}

int32 FHLSLMaterialTranslator::GetNumUserTexCoords() const
{
	return AllocatedUserTexCoords.FindLast(true) + 1;
}

int32 FHLSLMaterialTranslator::GetNumUserVertexTexCoords() const
{
	return AllocatedUserVertexTexCoords.FindLast(true) + 1;
}

void FHLSLMaterialTranslator::ClearAllFunctionStacks()
{
	for (uint32 Frequency = 0; Frequency < SF_NumFrequencies; ++Frequency)
	{
		ClearFunctionStack(Frequency);
	}
}

void FHLSLMaterialTranslator::ClearFunctionStack(uint32 Frequency)
{
	check(Frequency < SF_NumFrequencies);

	if (FunctionStacks[Frequency].Num() == 0)
	{
		return;  // Already cleared (at the end of Translate(), for example)
	}

	check(FunctionStacks[Frequency].Num() == 1);  // All states should be popped off, leaving only the null state
	delete FunctionStacks[Frequency][0];
	FunctionStacks[Frequency].Empty();
}

void FHLSLMaterialTranslator::AssignTempScope(TArray<FShaderCodeChunk>& InScope)
{
	CurrentScopeChunks = &InScope;
	CurrentScopeID = NextTempScopeID++;
}

void FHLSLMaterialTranslator::AssignShaderFrequencyScope(EShaderFrequency InShaderFrequency)
{
	check(InShaderFrequency < SF_NumFrequencies);
	check(InShaderFrequency < NextTempScopeID);
	CurrentScopeChunks = &SharedPropertyCodeChunks[InShaderFrequency];
	CurrentScopeID = (uint64)InShaderFrequency;
}

template<typename ExpressionsArrayType>
void FHLSLMaterialTranslator::GatherCustomVertexInterpolators(const ExpressionsArrayType& Expressions)
{
	for (UMaterialExpression* Expression : Expressions)
	{
		if (UMaterialExpressionVertexInterpolator* Interpolator = Cast<UMaterialExpressionVertexInterpolator>(Expression))
		{
			TArray<FShaderCodeChunk> CustomExpressionChunks;
			AssignTempScope(CustomExpressionChunks);

			// Errors are appended to a temporary pool as it's not known at this stage which interpolators are required
			CompileErrorsSink = &Interpolator->CompileErrors;
			CompileErrorExpressionsSink = &Interpolator->CompileErrorExpressions;

			// Compile node and store those successfully translated
			int32 Ret = Interpolator->CompileInput(this, NextVertexInterpolatorIndex);
			if (Ret != INDEX_NONE)
			{
				CustomVertexInterpolators.AddUnique(Interpolator);
				NextVertexInterpolatorIndex++;
			}

			// Restore error handling
			CompileErrorsSink = nullptr;
			CompileErrorExpressionsSink = nullptr;

			// Each interpolator chain must be handled as an independent compile
			for (FMaterialFunctionCompileState* FunctionStack : FunctionStacks[SF_Vertex])
			{
				FunctionStack->Reset();
			}
		}
		else if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (FunctionCall->MaterialFunction)
			{
				FMaterialFunctionCompileState LocalState(FunctionCall);
				FunctionCall->LinkFunctionIntoCaller(this);
				PushFunction(&LocalState);

				GatherCustomVertexInterpolators(FunctionCall->MaterialFunction->GetExpressions());

				FMaterialFunctionCompileState* CompileState = PopFunction();
				check(CompileState->ExpressionStack.Num() == 0);
				FunctionCall->UnlinkFunctionFromCaller(this);
			}
		}
		else if (UMaterialExpressionMaterialAttributeLayers* LayersExpression = Cast<UMaterialExpressionMaterialAttributeLayers>(Expression))
		{
			const FMaterialLayersFunctions* OverrideLayers = GetMaterialLayers();
			if (OverrideLayers)
			{
				LayersExpression->OverrideLayerGraph(OverrideLayers);
			}

			if (LayersExpression->bIsLayerGraphBuilt)
			{
				for (auto& Layer : LayersExpression->LayerCallers)
				{
					if (Layer && Layer->MaterialFunction)
					{
						FMaterialFunctionCompileState LocalState(Layer);
						Layer->LinkFunctionIntoCaller(this);
						PushFunction(&LocalState);
						
						GatherCustomVertexInterpolators(Layer->MaterialFunction->GetExpressions());

						FMaterialFunctionCompileState* CompileState = PopFunction();
						check(CompileState->ExpressionStack.Num() == 0);
						Layer->UnlinkFunctionFromCaller(this);
					}
				}

				for (auto& Blend : LayersExpression->BlendCallers)
				{
					if (Blend && Blend->MaterialFunction)
					{
						FMaterialFunctionCompileState LocalState(Blend);
						Blend->LinkFunctionIntoCaller(this);
						PushFunction(&LocalState);

						GatherCustomVertexInterpolators(Blend->MaterialFunction->GetExpressions());

						FMaterialFunctionCompileState* CompileState = PopFunction();
						check(CompileState->ExpressionStack.Num() == 0);
						Blend->UnlinkFunctionFromCaller(this);
					}
				}
			}

			if (OverrideLayers)
			{
				LayersExpression->OverrideLayerGraph(nullptr);
			}
		}
	}
}

void FHLSLMaterialTranslator::CompileCustomOutputs(TArray<UMaterialExpressionCustomOutput*>& CustomOutputExpressions, TSet<UClass*>& SeenCustomOutputExpressionsClasses, bool bIsBeforeAttributes)
{
	for (UMaterialExpressionCustomOutput* CustomOutput : CustomOutputExpressions)
	{
		if (CustomOutput->HasCustomSourceOutput() || CustomOutput->ShouldCompileBeforeAttributes() != bIsBeforeAttributes)
		{
			continue;
		}

		if (!CustomOutput->AllowMultipleCustomOutputs() && SeenCustomOutputExpressionsClasses.Contains(CustomOutput->GetClass()))
		{
			Errorf(TEXT("The material can contain only one %s node"), *CustomOutput->GetDescription());
		}
		else
		{
			SeenCustomOutputExpressionsClasses.Add(CustomOutput->GetClass());
			int32 NumOutputs = CustomOutput->GetNumOutputs();

			if (CustomOutput->NeedsCustomOutputDefines())
			{
				ResourcesString += FString::Printf(TEXT("#define NUM_MATERIAL_OUTPUTS_%s %d\r\n"), *CustomOutput->GetFunctionName().ToUpper(), NumOutputs);
			}

			if (NumOutputs > 0)
			{
				EShaderFrequency CustomOutputShaderFrequency = CustomOutput->GetShaderFrequency();
				for (int32 Index = 0; Index < NumOutputs; Index++)
				{
					{
						ClearFunctionStack(CustomOutputShaderFrequency);
						FunctionStacks[CustomOutputShaderFrequency].Add(new FMaterialFunctionCompileState(nullptr));
					}
					MaterialProperty = MP_MAX; // Indicates we're not compiling any material property.
					ShaderFrequency = CustomOutputShaderFrequency;
					TArray<FShaderCodeChunk> CustomExpressionChunks;
					AssignTempScope(CustomExpressionChunks);
					CustomOutput->Compile(this, Index);
				}

				ClearFunctionStack(CustomOutputShaderFrequency);
				FunctionStacks[CustomOutputShaderFrequency].Add(new FMaterialFunctionCompileState(nullptr));
			}
		}
	}
}

template<typename ExpressionsArrayType>
EMaterialExpressionVisitResult FHLSLMaterialTranslator::VisitExpressionsRecursive(const ExpressionsArrayType& Expressions, IMaterialExpressionVisitor& InVisitor)
{
	EMaterialExpressionVisitResult VisitResult = MVR_CONTINUE;
	for (UMaterialExpression* Expression : Expressions)
	{
		VisitResult = InVisitor.Visit(Expression);
		if (VisitResult == MVR_STOP)
		{
			break;
		}

		if (UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
		{
			if (FunctionCall->MaterialFunction)
			{
				FMaterialFunctionCompileState LocalState(FunctionCall);
				FunctionCall->LinkFunctionIntoCaller(this);
				PushFunction(&LocalState);

				VisitResult = VisitExpressionsRecursive(FunctionCall->MaterialFunction->GetExpressions(), InVisitor);

				FMaterialFunctionCompileState* CompileState = PopFunction();
				check(CompileState->ExpressionStack.Num() == 0);
				FunctionCall->UnlinkFunctionFromCaller(this);

				if (VisitResult == MVR_STOP)
				{
					break;
				}
			}
		}
		else if (UMaterialExpressionMaterialAttributeLayers* LayersExpression = Cast<UMaterialExpressionMaterialAttributeLayers>(Expression))
		{
			const FMaterialLayersFunctions* OverrideLayers = GetMaterialLayers();
			if (OverrideLayers)
			{
				LayersExpression->OverrideLayerGraph(OverrideLayers);
			}

			if (LayersExpression->bIsLayerGraphBuilt)
			{
				for (auto& Layer : LayersExpression->LayerCallers)
				{
					if (Layer && Layer->MaterialFunction)
					{
						FMaterialFunctionCompileState LocalState(Layer);
						Layer->LinkFunctionIntoCaller(this);
						PushFunction(&LocalState);

						VisitResult = VisitExpressionsRecursive(Layer->MaterialFunction->GetExpressions(), InVisitor);

						FMaterialFunctionCompileState* CompileState = PopFunction();
						check(CompileState->ExpressionStack.Num() == 0);
						Layer->UnlinkFunctionFromCaller(this);

						if (VisitResult == MVR_STOP)
						{
							break;
						}
					}
				}

				for (auto& Blend : LayersExpression->BlendCallers)
				{
					if (Blend && Blend->MaterialFunction)
					{
						FMaterialFunctionCompileState LocalState(Blend);
						Blend->LinkFunctionIntoCaller(this);
						PushFunction(&LocalState);

						VisitResult = VisitExpressionsRecursive(Blend->MaterialFunction->GetExpressions(), InVisitor);

						FMaterialFunctionCompileState* CompileState = PopFunction();
						check(CompileState->ExpressionStack.Num() == 0);
						Blend->UnlinkFunctionFromCaller(this);

						if (VisitResult == MVR_STOP)
						{
							break;
						}
					}
				}
			}

			if (OverrideLayers)
			{
				LayersExpression->OverrideLayerGraph(nullptr);
			}

			if (VisitResult == MVR_STOP)
			{
				break;
			}
		}
	}

	return VisitResult;
}

EMaterialExpressionVisitResult FHLSLMaterialTranslator::VisitExpressionsForProperty(EMaterialProperty InProperty, IMaterialExpressionVisitor& InVisitor)
{
	UMaterialInterface *MatIf = Material->GetMaterialInterface();
	// Some proxies return null for this. But the main one we are interested in doesn't
	if (MatIf)
	{
		TArray<UMaterialExpression*> InputExpressions;
		MatIf->GetMaterial()->GetExpressionsInPropertyChain(InProperty, InputExpressions, &StaticParameters);
		return VisitExpressionsRecursive(InputExpressions, InVisitor);
	}
	return MVR_STOP;
}

void FHLSLMaterialTranslator::ValidateVtPropertyLimits()
{
	class FFindVirtualTextureVisitor : public IMaterialExpressionVisitor
	{
	public:
		virtual EMaterialExpressionVisitResult Visit(UMaterialExpression* InExpression) override
		{
			if (UMaterialExpressionTextureBase *TextureExpr = Cast<UMaterialExpressionTextureBase>(InExpression))
			{
				if (IsVirtualSamplerType(TextureExpr->SamplerType))
				{
					FoundVirtualTexture = true;
					return MVR_STOP;
				}
			}
			return MVR_CONTINUE;
		}

		bool FoundVirtualTexture = false;
	};
	
	for (uint32 PropertyIndex = 0u; PropertyIndex < MP_MAX; ++PropertyIndex)
	{
		const EMaterialProperty PropertyToValidate = (EMaterialProperty)PropertyIndex;
		if (PropertyToValidate == MP_MaterialAttributes || PropertyToValidate == MP_CustomOutput)
		{
			// These properties are "special", attempting to pass them to FMaterialAttributeDefinitionMap::GetShaderFrequency() will generate log spam
			continue;
		}

		const EShaderFrequency ShaderFrequencyToValidate = FMaterialAttributeDefinitionMap::GetShaderFrequency(PropertyToValidate);

		// check to see if this is a property that doesn't support virtual texture connections
		if (PropertyToValidate == MP_OpacityMask || ShaderFrequencyToValidate != SF_Pixel)
		{
			FFindVirtualTextureVisitor Visitor;
			VisitExpressionsForProperty(PropertyToValidate, Visitor);
			if (Visitor.FoundVirtualTexture)
			{
				if (PropertyToValidate == MP_OpacityMask && GetDefault<URendererSettings>()->bEnableVirtualTextureOpacityMask == false)
				{
					Errorf(TEXT("Sampling a virtual texture for the Opacity Mask material attribute is disabled in the Renderer Settings."));
				}
			}
		}
	}
}
 
bool FHLSLMaterialTranslator::Translate()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FHLSLMaterialTranslator::Translate);

	STAT(double HLSLTranslateTime = 0);
	{
		SCOPE_SECONDS_COUNTER(HLSLTranslateTime);

		check(ScopeStack.Num() == 0);
		bSuccess = true;

		// WARNING: No compile outputs should be stored on the UMaterial / FMaterial / FMaterialResource, unless they are transient editor-only data (like error expressions)
		// Compile outputs that need to be saved must be stored in MaterialCompilationOutput, which will be saved to the DDC.

		Material->CompileErrors.Empty();
		Material->ErrorExpressions.Empty();

		// If pedantic error checking is enabled, log out the pre-compilation errors
		if (GPedanticErrorChecksEnabled)
		{
			bSuccess = Material->CheckInValidStateForCompilation(this);
		}

		bEnableExecutionFlow = Material->IsUsingControlFlow();
		bCompileForComputeShader = Material->IsLightFunction();
		
		//
		// Process the strata tree representing the material topology.
		//
		const bool bStrataEnabled = Strata::IsStrataEnabled();
		FStrataMaterialInput* FrontMaterialInput = Material->GetMaterialInterface() ? &Material->GetMaterialInterface()->GetMaterial()->GetEditorOnlyData()->FrontMaterial : nullptr;
		UMaterialExpression* FrontMaterialExpr = FrontMaterialInput ? FrontMaterialInput->GetTracedInput().Expression : nullptr;
		if (bStrataEnabled && FrontMaterialExpr)
		{
			// Temp code chunk scope (e.g.needed for the creation of static booleans from static switch parameter node, see UMaterialExpressionStaticSwitch::GetEffectiveInput).
			TArray<FShaderCodeChunk> TempChunks;
			AssignTempScope(TempChunks);

		#if DEBUG_STRATA_TREE_STACK
			UE_LOG(LogMaterial, Display, TEXT(" StrataTreeStack: StrataGenerateMaterialTopologyTree"));
		#endif
			{
				StrataThicknessIndexToExpressionInput.SetNum(0);
				StrataThicknessStack.SetNum(0);
			
				FExpressionInput* SurfaceThickness = Material->IsThinSurface() && Material->GetMaterialInterface() ? &Material->GetMaterialInterface()->GetMaterial()->GetEditorOnlyData()->SurfaceThickness : nullptr;
				StrataThicknessStackPush(nullptr, SurfaceThickness);
				FrontMaterialExpr->StrataGenerateMaterialTopologyTree(this, nullptr, FrontMaterialInput->OutputIndex);
				StrataThicknessStackPop();

				check(StrataThicknessStack.Num() == 0);
			}

			if (bStrataTreeOutOfStackDepthOccurred)
			{
				Errorf(TEXT(" %s [%s]: Substrate - Cyclic graph detected when we only support acyclic graph."), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
			}
			if (!StrataGenerateDerivedMaterialOperatorData())
			{
				Errorf(TEXT("Substrate material errors encountered."));
			}
		}

		// Generate code:
		// Normally one would expect the generator to emit something like
		//		float Local0 = ...
		//		...
		//		float Local3= ...
		//		...
		//		float Localn= ...
		//		PixelMaterialInputs.EmissiveColor = Local0 + ...
		//		PixelMaterialInputs.Normal = Local3 * ...
		// However because the Normal can be used in the middle of generating other Locals (which happens when using a node like PixelNormalWS)
		// instead we generate this:
		//		float Local0 = ...
		//		...
		//		float Local3= ...
		//		PixelMaterialInputs.Normal = Local3 * ...
		//		...
		//		float Localn= ...
		//		PixelMaterialInputs.EmissiveColor = Local0 + ...
		// in other words, compile Normal first, then emit all the expressions up to the last one Normal requires;
		// assign the normal into the shared struct, then emit the remaining expressions; finally assign the rest of the shared struct inputs.
		// Inputs that are not shared, have false in the SharedPixelProperties array, and those ones will emit the full code.

		int32 NormalCodeChunkEnd = -1;
		int32 Chunk[CompiledMP_MAX];
		int32 VertexAttributesChunk = INDEX_NONE;
		int32 PixelAttributesChunk = INDEX_NONE;
			
		memset(Chunk, INDEX_NONE, sizeof(Chunk));

		// Translate all custom vertex interpolators before main attributes so type information is available
		{
			CustomVertexInterpolators.Empty();
			CurrentCustomVertexInterpolatorOffset = 0;
			NextVertexInterpolatorIndex = 0;
			MaterialProperty = MP_MAX;
			ShaderFrequency = SF_Vertex;

			TArray<UMaterialExpression*> Expressions;
			Material->GatherExpressionsForCustomInterpolators(Expressions);
			GatherCustomVertexInterpolators(Expressions);

			// Reset shared stack data
			while (FunctionStacks[SF_Vertex].Num() > 1)
			{
				FMaterialFunctionCompileState* Stack = FunctionStacks[SF_Vertex].Pop(false);
				delete Stack;
			}
			FunctionStacks[SF_Vertex][0]->Reset();

			// Whilst expression list is available, apply node count limits
			int32 NumMaterialLayersAttributes = 0;
			for (UMaterialExpression* Expression : Expressions)
			{
				if (UMaterialExpressionMaterialAttributeLayers* Layers = Cast<UMaterialExpressionMaterialAttributeLayers>(Expression))
				{
					++NumMaterialLayersAttributes;

					if (NumMaterialLayersAttributes > 1)
					{
						Errorf(TEXT("Materials can contain only one Material Attribute Layers node."));
						break;
					}
				}
			}
		}

			
		const EShaderFrequency NormalShaderFrequency = FMaterialAttributeDefinitionMap::GetShaderFrequency(MP_Normal);
		const EMaterialDomain Domain = Material->GetMaterialDomain();
		const EBlendMode BlendMode = Material->GetBlendMode();

		// Gather the implementation for any custom output expressions
		TArray<UMaterialExpressionCustomOutput*> CustomOutputExpressions;
		Material->GatherCustomOutputExpressions(CustomOutputExpressions);
		TSet<UClass*> SeenCustomOutputExpressionsClasses;

		// Some custom outputs must be pre-compiled so they can be re-used as shared inputs
		CompileCustomOutputs(CustomOutputExpressions, SeenCustomOutputExpressionsClasses, true);			

		// Validate some things on the VT system. Since generated code for expressions shared between multiple properties
		// (e.g. a texture sample connected to both diffuse and opacity mask) is reused we can't check based on the MaterialProperty
		// variable inside the actual code generation pass. So we do a pre-pass over it here.
		if (UseVirtualTexturing(FeatureLevel, TargetPlatform))
		{
			ValidateVtPropertyLimits();
		}

		if (bEnableExecutionFlow)
		{
			PixelAttributesChunk = Material->CompilePropertyAndSetMaterialProperty(MP_MaterialAttributes, this, SF_Pixel);
			VertexAttributesChunk = Material->CompilePropertyAndSetMaterialProperty(MP_MaterialAttributes, this, SF_Vertex);
		}
		else
		{
		// Normal must always be compiled first; this will ensure its chunk calculations are the first to be added
		{
			// Verify that start chunk is 0
			check(SharedPropertyCodeChunks[NormalShaderFrequency].Num() == 0);
			Chunk[MP_Normal]					= Material->CompilePropertyAndSetMaterialProperty(MP_Normal					,this);
			NormalCodeChunkEnd = SharedPropertyCodeChunks[NormalShaderFrequency].Num();
		}

		// Rest of properties
		Chunk[MP_EmissiveColor]					= Material->CompilePropertyAndSetMaterialProperty(MP_EmissiveColor			,this);
		Chunk[MP_DiffuseColor]					= Material->CompilePropertyAndSetMaterialProperty(MP_DiffuseColor			,this);
		Chunk[MP_SpecularColor]					= Material->CompilePropertyAndSetMaterialProperty(MP_SpecularColor			,this);
		Chunk[MP_BaseColor]						= Material->CompilePropertyAndSetMaterialProperty(MP_BaseColor				,this);
		Chunk[MP_Metallic]						= Material->CompilePropertyAndSetMaterialProperty(MP_Metallic				,this);
		Chunk[MP_Specular]						= Material->CompilePropertyAndSetMaterialProperty(MP_Specular				,this);
		Chunk[MP_Roughness]						= Material->CompilePropertyAndSetMaterialProperty(MP_Roughness				,this);
		Chunk[MP_Anisotropy]					= Material->CompilePropertyAndSetMaterialProperty(MP_Anisotropy				,this);
		Chunk[MP_Opacity]						= Material->CompilePropertyAndSetMaterialProperty(MP_Opacity				,this);
		Chunk[MP_OpacityMask]					= Material->CompilePropertyAndSetMaterialProperty(MP_OpacityMask			,this);
		Chunk[MP_Tangent]						= Material->CompilePropertyAndSetMaterialProperty(MP_Tangent				,this);
		Chunk[MP_WorldPositionOffset]			= Material->CompilePropertyAndSetMaterialProperty(MP_WorldPositionOffset	,this);

		// Make sure to compile this property before using ShadingModelsFromCompilation
		Chunk[MP_ShadingModel]					= Material->CompilePropertyAndSetMaterialProperty(MP_ShadingModel			,this);
		
		
	#if DEBUG_STRATA_TREE_STACK
		UE_LOG(LogMaterial, Display, TEXT(" StrataTreeStack: Material->CompilePropertyAndSetMaterialProperty(MP_FrontMaterial)"));
	#endif
		Chunk[MP_SurfaceThickness]				= Material->CompilePropertyAndSetMaterialProperty(MP_SurfaceThickness		,this);
		Chunk[MP_FrontMaterial]					= Material->CompilePropertyAndSetMaterialProperty(MP_FrontMaterial			,this);
		}

		// Get shading models from compilation (or material).
		FMaterialShadingModelField MaterialShadingModels = GetCompiledShadingModels(); 
		
		ValidateShadingModelsForFeatureLevel(MaterialShadingModels);
		
		if (!bEnableExecutionFlow)
		{
			if (Domain == MD_Volume || (Domain == MD_Surface && IsSubsurfaceShadingModel(MaterialShadingModels)))
			{
				// Note we don't test for the blend mode as you can have a translucent material using the subsurface shading model

				// another ForceCast as CompilePropertyAndSetMaterialProperty() can return MCT_Float which we don't want here
				int32 SubsurfaceColor = Material->CompilePropertyAndSetMaterialProperty(MP_SubsurfaceColor, this);
				SubsurfaceColor = ForceCast(SubsurfaceColor, FMaterialAttributeDefinitionMap::GetValueType(MP_SubsurfaceColor), MFCF_ExactMatch | MFCF_ReplicateValue);

				static FName NameSubsurfaceProfile(TEXT("__SubsurfaceProfile"));

				// 1.0f is is a not used profile - later this gets replaced with the actual profile
				int32 CodeSubsurfaceProfile = ForceCast(ScalarParameter(NameSubsurfaceProfile, 1.0f), MCT_Float1);

				Chunk[MP_SubsurfaceColor] = AppendVector(SubsurfaceColor, CodeSubsurfaceProfile);
			}

			Chunk[MP_CustomData0] = Material->CompilePropertyAndSetMaterialProperty(MP_CustomData0, this);
			Chunk[MP_CustomData1] = Material->CompilePropertyAndSetMaterialProperty(MP_CustomData1, this);
			Chunk[MP_AmbientOcclusion] = Material->CompilePropertyAndSetMaterialProperty(MP_AmbientOcclusion, this);

			if (IsTranslucentBlendMode(BlendMode) || MaterialShadingModels.HasShadingModel(MSM_SingleLayerWater))
			{
				// Cast to exact match is needed for float parameter to be correctly cast to float2.
				int32 UserRefraction = ForceCast(Material->CompilePropertyAndSetMaterialProperty(MP_Refraction, this), MCT_Float2, MFCF_ExactMatch);
				int32 RefractionDepthBias = ForceCast(ScalarParameter(FName(TEXT("RefractionDepthBias")), Material->GetRefractionDepthBiasValue()), MCT_Float1);

				Chunk[MP_Refraction] = AppendVector(UserRefraction, RefractionDepthBias);
			}

			if (bCompileForComputeShader)
			{
				Chunk[CompiledMP_EmissiveColorCS] = Material->CompilePropertyAndSetMaterialProperty(MP_EmissiveColor, this, SF_Compute);
			}

			if (Chunk[MP_WorldPositionOffset] != INDEX_NONE)
			{
				// Only calculate previous WPO if there is a current WPO
				Chunk[CompiledMP_PrevWorldPositionOffset] = Material->CompilePropertyAndSetMaterialProperty(MP_WorldPositionOffset, this, SF_Vertex, true);
			}

			Chunk[MP_PixelDepthOffset] = Material->CompilePropertyAndSetMaterialProperty(MP_PixelDepthOffset, this);
		}

		ResourcesString = TEXT("");

#if HANDLE_CUSTOM_OUTPUTS_AS_MATERIAL_ATTRIBUTES
		// Handle custom outputs when using material attribute output
		if (Material->HasMaterialAttributesConnected())
		{
			TArray<FMaterialCustomOutputAttributeDefintion> CustomAttributeList;
			FMaterialAttributeDefinitionMap::GetCustomAttributeList(CustomAttributeList);
			TArray<FShaderCodeChunk> CustomExpressionChunks;

			for (FMaterialCustomOutputAttributeDefintion& Attribute : CustomAttributeList)
			{
				// Compile all outputs for attribute
				bool bValidResultCompiled = false;
				int32 NumOutputs = 1;//CustomOutput->GetNumOutputs();

				for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
				{
					MaterialProperty = Attribute.Property;
					ShaderFrequency = Attribute.ShaderFrequency;
					FunctionStacks[ShaderFrequency].Empty();
					FunctionStacks[ShaderFrequency].Add(FMaterialFunctionCompileState(nullptr));

					CustomExpressionChunks.Empty();
					AssignTempScope(CustomExpressionChunks);
					int32 Result = Material->CompileCustomAttribute(Attribute.AttributeID, this);

					// Consider attribute used if varies from default value
					if (Result != INDEX_NONE)
					{
						bool bValueNonDefault = true;

						if (FMaterialUniformExpression* Expression = GetParameterUniformExpression(Result))
						{
							FLinearColor Value;
							FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
							Expression->GetNumberValue(DummyContext, Value);

							bool bEqualValue = Value.R == Attribute.DefaultValue.X;
							bEqualValue &= Value.G == Attribute.DefaultValue.Y || Attribute.ValueType < MCT_Float2;
							bEqualValue &= Value.B == Attribute.DefaultValue.Z || Attribute.ValueType < MCT_Float3;
							bEqualValue &= Value.A == Attribute.DefaultValue.W || Attribute.ValueType < MCT_Float4;

							if (Expression->IsConstant() && bEqualValue)
							{
								bValueNonDefault = false;
							}
						}

						// Valid, non-default value so generate shader code
						if (bValueNonDefault)
						{
							GenerateCustomAttributeCode(OutputIndex, Result, Attribute.ValueType, Attribute.FunctionName);
							bValidResultCompiled = true;
						}
					}
				}

				// If used, add compile data
				if (bValidResultCompiled)
				{
					ResourcesString += FString::Printf(TEXT("#define NUM_MATERIAL_OUTPUTS_%s %d\r\n"), *Attribute.FunctionName.ToUpper(), NumOutputs);
				}
			}
		}
		else
#endif // #if HANDLE_CUSTOM_OUTPUTS_AS_MATERIAL_ATTRIBUTES
		{
			CompileCustomOutputs(CustomOutputExpressions, SeenCustomOutputExpressionsClasses, false);
		}

		// No more calls to non-vertex shader CompilePropertyAndSetMaterialProperty beyond this point
		const uint32 SavedNumUserTexCoords = GetNumUserTexCoords();

		if (!bEnableExecutionFlow)
		{
			for (uint32 CustomUVIndex = MP_CustomizedUVs0; CustomUVIndex <= MP_CustomizedUVs7; CustomUVIndex++)
			{
				// Only compile custom UV inputs for UV channels requested by the pixel shader inputs
				// Any unconnected inputs will have a texcoord generated for them in Material->CompileProperty, which will pass through the vertex (uncustomized) texture coordinates
				// Note: this is using NumUserTexCoords, which is set by translating all the pixel properties above
				if (CustomUVIndex - MP_CustomizedUVs0 < SavedNumUserTexCoords)
				{
					Chunk[CustomUVIndex] = Material->CompilePropertyAndSetMaterialProperty((EMaterialProperty)CustomUVIndex, this);
				}
			}
		}

		// Output the implementation for any custom expressions we will call below.
		for (int32 ExpressionIndex = 0; ExpressionIndex < CustomExpressions.Num(); ExpressionIndex++)
		{
			ResourcesString += CustomExpressions[ExpressionIndex].Implementation + "\r\n\r\n";
		}

		// Translation is designed to have a code chunk generation phase followed by several passes that only has readonly access to the code chunks.
		// At this point we mark the code chunk generation complete.
		bAllowCodeChunkGeneration = false;

		bUsesEmissiveColor = IsMaterialPropertyUsed(MP_EmissiveColor, Chunk[MP_EmissiveColor], FLinearColor(0, 0, 0, 0), 3);
		bUsesPixelDepthOffset = (AllowPixelDepthOffset(Platform) && IsMaterialPropertyUsed(MP_PixelDepthOffset, Chunk[MP_PixelDepthOffset], FLinearColor(0, 0, 0, 0), 1));

		bool bUsesWorldPositionOffsetCurrent = IsMaterialPropertyUsed(MP_WorldPositionOffset, Chunk[MP_WorldPositionOffset], FLinearColor(0, 0, 0, 0), 3);
		bool bUsesWorldPositionOffsetPrevious = IsMaterialPropertyUsed(MP_WorldPositionOffset, Chunk[CompiledMP_PrevWorldPositionOffset], FLinearColor(0, 0, 0, 0), 3);
		bUsesWorldPositionOffset = bUsesWorldPositionOffsetCurrent || bUsesWorldPositionOffsetPrevious;

		MaterialCompilationOutput.bModifiesMeshPosition = bUsesPixelDepthOffset || bUsesWorldPositionOffset;
		MaterialCompilationOutput.bUsesWorldPositionOffset = bUsesWorldPositionOffset;
		MaterialCompilationOutput.bUsesPixelDepthOffset = bUsesPixelDepthOffset;

		// Fully rough if we have a roughness code chunk and it's constant and evaluates to 1.
		bIsFullyRough = Chunk[MP_Roughness] != INDEX_NONE && IsMaterialPropertyUsed(MP_Roughness, Chunk[MP_Roughness], FLinearColor(1, 0, 0, 0), 1) == false;

		bUsesAnisotropy = IsMaterialPropertyUsed(MP_Anisotropy, Chunk[MP_Anisotropy], FLinearColor(0, 0, 0, 0), 1);
		MaterialCompilationOutput.bUsesAnisotropy = bUsesAnisotropy;

		EMaterialDecalResponse MDR = (EMaterialDecalResponse)Material->GetMaterialDecalResponse();
		if (MDR == MDR_Color || MDR == MDR_ColorNormal || MDR == MDR_ColorRoughness || MDR == MDR_ColorNormalRoughness)
		{
			MaterialCompilationOutput.SetIsDBufferTextureUsed(0);
			AddEstimatedTextureSample(1);
		}
		if (MDR == MDR_Normal || MDR == MDR_ColorNormal || MDR == MDR_NormalRoughness || MDR == MDR_ColorNormalRoughness)
		{
			MaterialCompilationOutput.SetIsDBufferTextureUsed(1);
			AddEstimatedTextureSample(1);
		}
		if (MDR == MDR_Roughness || MDR == MDR_ColorRoughness || MDR == MDR_NormalRoughness || MDR == MDR_ColorNormalRoughness)
		{
			MaterialCompilationOutput.SetIsDBufferTextureUsed(2);
			AddEstimatedTextureSample(1);
		}

		bUsesCurvature = FeatureLevel == ERHIFeatureLevel::ES3_1 && 
						((MaterialShadingModels.HasShadingModel(MSM_SubsurfaceProfile) && IsMaterialPropertyUsed(MP_CustomData0, Chunk[MP_CustomData0], FLinearColor(1, 0, 0, 0), 1))
						|| (MaterialShadingModels.HasShadingModel(MSM_Eye) && IsMaterialPropertyUsed(MP_Opacity, Chunk[MP_Opacity], FLinearColor(1, 0, 0, 0), 1)));

		if (!bStrataEnabled && IsModulateBlendMode(BlendMode) && MaterialShadingModels.IsLit() && !Material->IsDeferredDecal())
		{
			Errorf(TEXT("Dynamically lit translucency is not supported for BLEND_Modulate materials."));
		}

		if (Domain == MD_Surface)
		{
			if (IsModulateBlendMode(BlendMode) && Material->IsTranslucencyAfterDOFEnabled() && !RHISupportsDualSourceBlending(Platform))
			{
				Errorf(TEXT("Translucency after DOF with BLEND_Modulate is only allowed on platforms that support dual-blending. Consider using BLEND_Translucent with black emissive"));
			}
		}

		// Don't allow opaque and masked materials to scene depth as the results are undefined
		if (bUsesSceneDepth && Domain != MD_PostProcess && !IsTranslucentBlendMode(BlendMode))
		{
			Errorf(TEXT("Only transparent or postprocess materials can read from scene depth."));
		}

		if (bUsesSceneDepth)
		{
			MaterialCompilationOutput.SetIsSceneTextureUsed(PPI_SceneDepth);
		}

		MaterialCompilationOutput.bUsesDistanceCullFade = bUsesDistanceCullFade;

		if (MaterialCompilationOutput.RequiresSceneColorCopy())
		{
			if (Domain != MD_Surface)
			{
				Errorf(TEXT("Only 'surface' material domain can use the scene color node."));
			}
			else if (!IsTranslucentBlendMode(BlendMode))
			{
				Errorf(TEXT("Only translucent materials can use the scene color node."));
			}
		}

		if (IsAlphaHoldoutBlendMode(BlendMode) && !MaterialShadingModels.IsUnlit())
		{
			Errorf(TEXT("Alpha Holdout blend mode must use unlit shading model."));
		}

		if (Domain == MD_Volume && BlendMode != BLEND_Additive)
		{
			Errorf(TEXT("Volume materials must use an Additive blend mode."));
		}
		if (Domain == MD_Volume && Material->IsUsedWithSkeletalMesh())
		{
			Errorf(TEXT("Volume materials are not compatible with skinned meshes: they are voxelised as boxes anyway. Please disable UsedWithSkeletalMesh on the material."));
		}

		if (Material->IsLightFunction() && !IsOpaqueBlendMode(BlendMode))
		{
			Errorf(TEXT("Light function materials must be opaque."));
		}

		if (Material->IsLightFunction() && MaterialShadingModels.IsLit())
		{
			Errorf(TEXT("Light function materials must use unlit."));
		}

		if (Domain == MD_PostProcess && MaterialShadingModels.IsLit())
		{
			Errorf(TEXT("Post process materials must use unlit."));
		}

		if (Material->AllowNegativeEmissiveColor() && MaterialShadingModels.IsLit())
		{
			Errorf(TEXT("Only unlit materials can output negative emissive color."));
		}

		if (Material->IsSky() && (!MaterialShadingModels.IsUnlit() || !(IsOpaqueOrMaskedBlendMode(BlendMode))))
		{
			Errorf(TEXT("Sky materials must be opaque or masked, and unlit. They are expected to completely replace the background."));
		}

		if (MaterialShadingModels.HasShadingModel(MSM_SingleLayerWater))
		{
			if (!IsOpaqueOrMaskedBlendMode(BlendMode))
			{
				Errorf(TEXT("SingleLayerWater materials must be opaque or masked."));
			}
			if (!MaterialShadingModels.HasOnlyShadingModel(MSM_SingleLayerWater))
			{
				Errorf(TEXT("SingleLayerWater materials cannot be combined with other shading models.")); // Simply untested for now
			}

			if (Material->GetMaterialInterface() && !Material->GetMaterialInterface()->GetMaterial()->HasAnyExpressionsInMaterialAndFunctionsOfType<UMaterialExpressionSingleLayerWaterMaterialOutput>()
				&& !bStrataEnabled
				)
			{
				Errorf(TEXT("SingleLayerWater materials requires the use of SingleLayerWaterMaterial output node."));
			}
		}

		if (MaterialShadingModels.HasShadingModel(MSM_ThinTranslucent))
		{
			if (!IsTranslucentOnlyBlendMode(BlendMode))
			{
				Errorf(TEXT("ThinTranslucent materials must be translucent."));
			}

			const ETranslucencyLightingMode TranslucencyLightingMode = Material->GetTranslucencyLightingMode();

			if (TranslucencyLightingMode != ETranslucencyLightingMode::TLM_SurfacePerPixelLighting)
			{
				Errorf(TEXT("ThinTranslucent materials must use Surface Per Pixel Lighting (Translucency->LightingMode=Surface ForwardShading).\n"));
			}
			if (!MaterialShadingModels.HasOnlyShadingModel(MSM_ThinTranslucent))
			{
				Errorf(TEXT("ThinTranslucent materials cannot be combined with other shading models.")); // Simply untested for now
			}
			if (Material->GetMaterialInterface() && !Material->GetMaterialInterface()->GetMaterial()->HasAnyExpressionsInMaterialAndFunctionsOfType<UMaterialExpressionThinTranslucentMaterialOutput>())
			{
				Errorf(TEXT("ThinTranslucent materials requires the use of ThinTranslucentMaterial output node."));
			}
			if (Material->IsTranslucencyAfterMotionBlurEnabled())
			{
				// We don't currently have a separate translucency modulation pass for After Motion Blur
				Errorf(TEXT("ThinTranslucent materials are not currently supported in the \"After Motion Blur\" translucency pass."));
			}
		}

		if (Material->GetMaterialInterface() && Material->GetMaterialInterface()->GetMaterial()->HasAnyExpressionsInMaterialAndFunctionsOfType<UMaterialExpressionAbsorptionMediumMaterialOutput>())
		{
			// User placed an AbsorptionMedium node, make sure default lit is being used
			if (!MaterialShadingModels.HasShadingModel(MSM_DefaultLit))
			{
				Errorf(TEXT("AbsorptionMedium custom output requires Default Lit shading model."));
			}
			if (Material->GetRefractionMode() != RM_IndexOfRefraction)
			{
				Errorf(TEXT("AbsorptionMedium custom output requires \"Index of Refraction\" as the \"Refraction Mode\"."));
			}
		}

		if (Domain == MD_DeferredDecal && !(IsTranslucentOnlyBlendMode(BlendMode) || BlendMode == BLEND_AlphaComposite || IsModulateBlendMode(BlendMode)))
		{
			// We could make the change for the user but it would be confusing when going to DeferredDecal and back
			// or we would have to pay a performance cost to make the change more transparently.
			// The change saves performance as with translucency we don't need to test for MeshDecals in all opaque rendering passes
			Errorf(TEXT("Material using the DeferredDecal domain can only use the BlendModes Translucent, AlphaComposite or Modulate"));
		}

		if (MaterialCompilationOutput.bNeedsSceneTextures)
		{
			if (Domain != MD_DeferredDecal && Domain != MD_PostProcess)
			{
				if (!MaterialShadingModels.HasShadingModel(MSM_SingleLayerWater) && IsOpaqueOrMaskedBlendMode(BlendMode))
				{
					// In opaque pass, none of the textures are available
					Errorf(TEXT("SceneTexture expressions cannot be used in opaque materials except if used with the Single Layer Water shading model."));
				}
				else if (bNeedsSceneTexturePostProcessInputs)
				{
					Errorf(TEXT("SceneTexture expressions cannot use post process inputs or scene color in non post process domain materials"));
				}
			}
		}

		if (IsModulateBlendMode(BlendMode) && Material->IsTranslucencyAfterMotionBlurEnabled())
		{
			// We don't currently have a separate translucency modulation pass for After Motion Blur
			Errorf(TEXT("Blend Mode \"Modulate\" materials are not currently supported in the \"After Motion Blur\" translucency pass."));
		}

		// Catch any modifications to NumUserTexCoords that will not seen by customized UVs
		check(SavedNumUserTexCoords == GetNumUserTexCoords());

		FString InterpolatorsOffsetsDefinitionCode;
		TBitArray<> FinalAllocatedCoords = GetVertexInterpolatorsOffsets(InterpolatorsOffsetsDefinitionCode);

		// Finished compilation, verify final interpolator count restrictions
		if (CurrentCustomVertexInterpolatorOffset > 0)
		{
			const int32 MaxNumScalars = 8 * 2;
			const int32 TotalUsedScalars = FinalAllocatedCoords.FindLast(true) + 1;

 			if (TotalUsedScalars > MaxNumScalars)
			{
				Errorf(TEXT("Maximum number of custom vertex interpolators exceeded. (%i / %i scalar values) (TexCoord: %i scalars, Custom: %i scalars)"),
					TotalUsedScalars, MaxNumScalars, GetNumUserTexCoords() * 2, CurrentCustomVertexInterpolatorOffset);
			}
		}

		MaterialCompilationOutput.NumUsedUVScalars = GetNumUserTexCoords() * 2;
		MaterialCompilationOutput.NumUsedCustomInterpolatorScalars = CurrentCustomVertexInterpolatorOffset;

		if (bEnableExecutionFlow)
		{
			if (VertexAttributesChunk != INDEX_NONE)
			{
				LinkParentScopes(SharedPropertyCodeChunks[SF_Vertex]);

				TSet<int32> EmittedChunks;
				GetScopeCode(1, VertexAttributesChunk, SharedPropertyCodeChunks[SF_Vertex], EmittedChunks, TranslatedAttributesCodeChunks[SF_Vertex]);
			}

			if (PixelAttributesChunk != INDEX_NONE)
			{
				LinkParentScopes(SharedPropertyCodeChunks[SF_Pixel]);

				TSet<int32> EmittedChunks;
				GetScopeCode(1, PixelAttributesChunk, SharedPropertyCodeChunks[SF_Pixel], EmittedChunks, TranslatedAttributesCodeChunks[SF_Pixel]);
			}
		}
		else
		{
			for (int32 VariationIter = 0; VariationIter < CompiledPDV_MAX; VariationIter++)
			{
				ECompiledPartialDerivativeVariation Variation = (ECompiledPartialDerivativeVariation)VariationIter;

		// Do Normal Chunk first
		{
			GetFixedParameterCode(
				0,
				NormalCodeChunkEnd,
				Chunk[MP_Normal],
				SharedPropertyCodeChunks[NormalShaderFrequency],
						DerivativeVariations[Variation].TranslatedCodeChunkDefinitions[MP_Normal],
						DerivativeVariations[Variation].TranslatedCodeChunks[MP_Normal],
						Variation);

			// Always gather MP_Normal definitions as they can be shared by other properties
					if (DerivativeVariations[Variation].TranslatedCodeChunkDefinitions[MP_Normal].IsEmpty())
			{
						DerivativeVariations[Variation].TranslatedCodeChunkDefinitions[MP_Normal] = GetDefinitions(SharedPropertyCodeChunks[NormalShaderFrequency], 0, NormalCodeChunkEnd, Variation);
			}
		}

		// Now the rest, skipping Normal
				for (uint32 PropertyId = 0; PropertyId < MP_MAX; ++PropertyId)
		{
			if (PropertyId == MP_MaterialAttributes || PropertyId == MP_Normal || PropertyId == MP_CustomOutput)
			{
				continue;
			}

			const EShaderFrequency PropertyShaderFrequency = FMaterialAttributeDefinitionMap::GetShaderFrequency((EMaterialProperty)PropertyId);

			int32 StartChunk = 0;
			if (PropertyShaderFrequency == NormalShaderFrequency && SharedPixelProperties[PropertyId])
			{
				// When processing shared properties, do not generate the code before the Normal was generated as those are already handled
				StartChunk = NormalCodeChunkEnd;
			}

			// Reduce definition statements that don't contribute to the function's return value.
			// @todo-lh: This should be expanded to a general reduction, but is currently only intended to fix an FXC internal compiler error reported in UE-117831
			const bool bReduceAfterReturnValue = (PropertyId == MP_WorldPositionOffset || PropertyId == CompiledMP_PrevWorldPositionOffset);

			GetFixedParameterCode(
				StartChunk,
				SharedPropertyCodeChunks[PropertyShaderFrequency].Num(),
				Chunk[PropertyId],
				SharedPropertyCodeChunks[PropertyShaderFrequency],
				DerivativeVariations[Variation].TranslatedCodeChunkDefinitions[PropertyId],
				DerivativeVariations[Variation].TranslatedCodeChunks[PropertyId],
				Variation,
				bReduceAfterReturnValue);

		}

				for (uint32 PropertyId = MP_MAX; PropertyId < CompiledMP_MAX; ++PropertyId)
		{
					switch (PropertyId)
			{
			case CompiledMP_EmissiveColorCS:
			    if (bCompileForComputeShader)
				{
							GetFixedParameterCode(Chunk[PropertyId], SharedPropertyCodeChunks[SF_Compute], DerivativeVariations[Variation].TranslatedCodeChunkDefinitions[PropertyId], DerivativeVariations[Variation].TranslatedCodeChunks[PropertyId], Variation);
				}
				break;
			case CompiledMP_PrevWorldPositionOffset:
				{
						GetFixedParameterCode(Chunk[PropertyId], SharedPropertyCodeChunks[SF_Vertex], DerivativeVariations[Variation].TranslatedCodeChunkDefinitions[PropertyId], DerivativeVariations[Variation].TranslatedCodeChunks[PropertyId], Variation);
				}
				break;
			default: check(0);
				break;
			}
		}

		// Output the implementation for any custom output expressions
				for (int32 ExpressionIndex = 0; ExpressionIndex < DerivativeVariations[Variation].CustomOutputImplementations.Num(); ExpressionIndex++)
		{
					ResourcesString += DerivativeVariations[Variation].CustomOutputImplementations[ExpressionIndex] + "\r\n\r\n";
				}
			}
		}

		// Store the number of float4s
		MaterialCompilationOutput.UniformExpressionSet.UniformPreshaderBufferSize = (UniformPreshaderOffset + 3u) / 4u;

		for (uint32 TypeIndex = 0u; TypeIndex < NumMaterialTextureParameterTypes; ++TypeIndex)
		{
			MaterialCompilationOutput.UniformExpressionSet.UniformTextureParameters[TypeIndex].Empty(UniformTextureExpressions[TypeIndex].Num());
			for (FMaterialUniformExpressionTexture* TextureExpression : UniformTextureExpressions[TypeIndex])
			{
				TextureExpression->GetTextureParameterInfo(MaterialCompilationOutput.UniformExpressionSet.UniformTextureParameters[TypeIndex].AddDefaulted_GetRef());
			}
		}
		MaterialCompilationOutput.UniformExpressionSet.UniformExternalTextureParameters.Empty(UniformExternalTextureExpressions.Num());
		for (FMaterialUniformExpressionExternalTexture* TextureExpression : UniformExternalTextureExpressions)
		{
			TextureExpression->GetExternalTextureParameterInfo(MaterialCompilationOutput.UniformExpressionSet.UniformExternalTextureParameters.AddDefaulted_GetRef());
		}


		const bool bStrataFrontMaterialProvided = Chunk[MP_FrontMaterial] != INDEX_NONE;
		bool bStrataFrontMaterialIsValid = bStrataFrontMaterialProvided;
		if (bStrataFrontMaterialIsValid)
		{
			// The material can be null when some entries are automatically generated, for instance in the material layer blending system
			bStrataFrontMaterialIsValid &= StrataMaterialBSDFCount > 0;
		}

		if (bStrataFrontMaterialIsValid)
		{
			bMaterialIsStrata = true;

			if (StrataMaterialRootOperator)
			{
				// Now implement the functions needed to process the material topology

				// Pre-Update the slab BSDF with operators (like thin film coating, which can alter F0/F90)
				{
					// Update the coverage/transmittance of each node in the graph
					check(StrataMaterialRootOperator);
					int32 RootMaximumDistanceToLeaves = StrataMaterialRootOperator->MaxDistanceFromLeaves;

					ResourcesString += "void PreUpdateAllBSDFWithBottomUpOperatorVisit(inout FStrataPixelHeader StrataPixelHeader, inout FStrataTree StrataTree, FStrataAddressing NullStrataAddressing, float3 V)\n";
					ResourcesString += "{\n";
					for (uint32 BSDFIndex = 0; BSDFIndex < StrataMaterialBSDFCount; ++BSDFIndex)
					{
						ResourcesString += "\t{\n";
						for (auto& It : StrataMaterialExpressionRegisteredOperators)
						{
							if (!It.IsDiscarded() && It.BSDFIndex == BSDFIndex)
							{
								// Walk up the graph to the root node and apply weight factors
								std::function<void(const FStrataOperator&, int32)> WalkOperatorsUp = [&](const FStrataOperator& CurrentOperator, int32 PreviousOperatorIndex) -> void
								{
									switch (CurrentOperator.OperatorType)
									{
									case STRATA_OPERATOR_WEIGHT:
									{
										break; // NOP
									}
									case STRATA_OPERATOR_HORIZONTAL:
									{
										break; // NOP
									}
									case STRATA_OPERATOR_VERTICAL:
									{
										// example ResourcesString += FString::Printf(TEXT("\t PreUpdateAllBSDFWithBottomUpOperatorVisit_Vertical(StrataPixelHeader, StrataTree, StrataTree.BSDFs[%d], NullStrataAddressing, V, %d /*Op index*/, %d /*PreviousIsInputA*/);\n"), BSDFIndex, CurrentOperator.Index, CurrentOperator.LeftIndex == PreviousOperatorIndex ? 1 : 0);
										break; // NOP
									}
									case STRATA_OPERATOR_ADD:
									{
										break; // NOP
									}
									default:
									case STRATA_OPERATOR_BSDF:
									{
										check(false);
									}
									}

									if (CurrentOperator.ParentIndex != INDEX_NONE)
									{
										WalkOperatorsUp(StrataMaterialExpressionRegisteredOperators[CurrentOperator.ParentIndex], CurrentOperator.Index);
									}
								};

								const int32 BSDFOperatorIndex = It.Index;
								const FStrataOperator& BSDFOperator = StrataMaterialExpressionRegisteredOperators[BSDFOperatorIndex];

								// Start visiting node up from the BSDF leaf only if it has a parent.
								if (BSDFOperator.ParentIndex != INDEX_NONE)
								{
									WalkOperatorsUp(StrataMaterialExpressionRegisteredOperators[BSDFOperator.ParentIndex], BSDFOperator.Index);
								}
							}
						}
						ResourcesString += "\t}\n";
					}
					ResourcesString += "}\n";
				}
			
				// Update the coverage/transmittance of each leaves (==BSDFs) of the strata tree.
				{
					ResourcesString += "void UpdateAllBSDFsOperatorCoverageTransmittance(inout FStrataPixelHeader StrataPixelHeader, inout FStrataTree StrataTree, FStrataIntegrationSettings Settings, FStrataAddressing NullStrataAddressing, float3 V)\n";
					ResourcesString += "{\n";
					for (uint32 BSDFIndex = 0; BSDFIndex < StrataMaterialBSDFCount; ++BSDFIndex)
					{
						ResourcesString += FString::Printf(TEXT("\t UpdateSingleBSDFOperatorCoverageTransmittance(StrataPixelHeader, StrataTree, StrataTree.BSDFs[%d], Settings, NullStrataAddressing, V);\n"), BSDFIndex);
					}
					ResourcesString += "}\n";
				}

				// Propagate up the coverage/transmittance of each node in the strata tree.
				// For that we visit all the operator according to their distance from the Strata tree leaves, from small to large.
				{
					check(StrataMaterialRootOperator);
					int32 RootMaximumDistanceToLeaves = StrataMaterialRootOperator->MaxDistanceFromLeaves;

					ResourcesString += "void UpdateAllOperatorsCoverageTransmittance(inout FStrataTree StrataTree)\n";
					ResourcesString += "{\n";
					for (int32 DistanceToLeaves = 1; DistanceToLeaves <= RootMaximumDistanceToLeaves; ++DistanceToLeaves)
					{
						ResourcesString += FString::Printf(TEXT("\t// MaxDistanceFromLeaves = %d \n"), DistanceToLeaves);
						for (auto& It : StrataMaterialExpressionRegisteredOperators)
						{
							if (!It.IsDiscarded() && It.MaxDistanceFromLeaves == DistanceToLeaves)
							{
								ResourcesString += FString::Printf(TEXT("\t UpdateSingleOperatorCoverageTransmittance(StrataTree, %d /*operator index*/);\n"), It.Index);
							}
						}
					}
					ResourcesString += "}\n";
				}

				// Update the luminance weight of each BSDF according to the operators it has to traverse bottom-up to the strata tree root node.
				{
					// Update the coverage/transmittance of each node in the graph
					check(StrataMaterialRootOperator);
					int32 RootMaximumDistanceToLeaves = StrataMaterialRootOperator->MaxDistanceFromLeaves;

					ResourcesString += "void UpdateAllBSDFWithBottomUpOperatorVisit(inout FStrataTree StrataTree)\n";
					ResourcesString += "{\n";
					for (uint32 BSDFIndex = 0; BSDFIndex < StrataMaterialBSDFCount; ++BSDFIndex)
					{
						for (auto& It : StrataMaterialExpressionRegisteredOperators)
						{
							if (!It.IsDiscarded() && It.BSDFIndex == BSDFIndex)
							{
								// Walk up the graph to the root node and apply weight factors
								std::function<void(const FStrataOperator&, int32)> WalkOperatorsUp = [&](const FStrataOperator& CurrentOperator, int32 PreviousOperatorIndex) -> void
								{
									switch (CurrentOperator.OperatorType)
									{
									case STRATA_OPERATOR_WEIGHT:
									{
										ResourcesString += FString::Printf(TEXT("\t UpdateAllBSDFWithBottomUpOperatorVisit_Weight(StrataTree, StrataTree.BSDFs[%d], %d /*Op index*/, %d /*PreviousIsInputA*/);\n"), BSDFIndex, CurrentOperator.Index, 1);
										break;
									}
									case STRATA_OPERATOR_HORIZONTAL:
									{
										ResourcesString += FString::Printf(TEXT("\t UpdateAllBSDFWithBottomUpOperatorVisit_Horizontal(StrataTree, StrataTree.BSDFs[%d], %d /*Op index*/, %d /*PreviousIsInputA*/);\n"), BSDFIndex, CurrentOperator.Index, CurrentOperator.LeftIndex == PreviousOperatorIndex ? 1 : 0);
										break;
									}
									case STRATA_OPERATOR_VERTICAL:
									{
										ResourcesString += FString::Printf(TEXT("\t UpdateAllBSDFWithBottomUpOperatorVisit_Vertical(StrataTree, StrataTree.BSDFs[%d], %d /*Op index*/, %d /*PreviousIsInputA*/);\n"), BSDFIndex, CurrentOperator.Index, CurrentOperator.LeftIndex == PreviousOperatorIndex ? 1 : 0);
										break;
									}
									case STRATA_OPERATOR_ADD:
									{
										break; // NOP
									}
									default:
									case STRATA_OPERATOR_BSDF:
									{
										check(false);
									}
									}

									if (CurrentOperator.ParentIndex != INDEX_NONE)
									{
										WalkOperatorsUp(StrataMaterialExpressionRegisteredOperators[CurrentOperator.ParentIndex], CurrentOperator.Index);
									}
								};

								const int32 BSDFOperatorIndex = It.Index;
								const FStrataOperator& BSDFOperator = StrataMaterialExpressionRegisteredOperators[BSDFOperatorIndex];

								// Start visiting node up from the BSDF leaf only if it has a parent.
								if (BSDFOperator.ParentIndex != INDEX_NONE)
								{
									WalkOperatorsUp(StrataMaterialExpressionRegisteredOperators[BSDFOperator.ParentIndex], BSDFOperator.Index);
								}
							}
						}
					}
					ResourcesString += "}\n";
				}
			}

			// Check if normal/tangent basis are valid
			{
				uint8 UsedSharedLocalBasesCount = 0;
				uint8 RequestedSharedLocalBasesCount = 0;
				StrataEvaluateSharedLocalBases(UsedSharedLocalBasesCount, RequestedSharedLocalBasesCount, nullptr, nullptr);
				if (RequestedSharedLocalBasesCount > STRATA_MAX_SHAREDLOCALBASES_REGISTERS)
				{
					Errorf(TEXT(" %s [%s]: Substrate - Material has more unique normal/tangent basis than the allowed limit %d/%d."), *Material->GetDebugName(), *Material->GetAssetPath().ToString(), RequestedSharedLocalBasesCount, STRATA_MAX_SHAREDLOCALBASES_REGISTERS);
				}
			}
		}
		else
		{
			MaterialCompilationOutput.StrataMaterialDescription = "";
			ResourcesString += "// No Strata material provided\r\n";
			
			// Adde default strata functions
			ResourcesString += "#if TEMPLATE_USES_STRATA\n";
			ResourcesString += "void UpdateAllBSDFsOperatorCoverageTransmittance(inout FStrataPixelHeader StrataPixelHeader, inout FStrataTree StrataTree, FStrataIntegrationSettings Settings, FStrataAddressing NullStrataAddressing, float3 V) {}\n";
			ResourcesString += "void UpdateAllOperatorsCoverageTransmittance(inout FStrataTree StrataTree) {}\n";
			ResourcesString += "void UpdateAllBSDFWeightAfterOperatorVisit(inout FStrataTree StrataTree) {}\n";
			ResourcesString += "#endif\n";
		}

		LoadShaderSourceFileChecked(TEXT("/Engine/Private/MaterialTemplate.ush"), GetShaderPlatform(), MaterialTemplate);

		// Find the string index of the '#line' statement in MaterialTemplate.usf
		const int32 LineIndex = MaterialTemplate.Find(TEXT("#line"), ESearchCase::CaseSensitive);
		check(LineIndex != INDEX_NONE);

		// Count line endings before the '#line' statement
		MaterialTemplateLineNumber = INDEX_NONE;
		int32 StartPosition = LineIndex + 1;
		do 
		{
			MaterialTemplateLineNumber++;
			// Using \n instead of LINE_TERMINATOR as not all of the lines are terminated consistently
			// Subtract one from the last found line ending index to make sure we skip over it
			StartPosition = MaterialTemplate.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, StartPosition - 1);
		} 
		while (StartPosition != INDEX_NONE);
		check(MaterialTemplateLineNumber != INDEX_NONE);
		// At this point MaterialTemplateLineNumber is one less than the line number of the '#line' statement
		// For some reason we have to add 2 more to the #line value to get correct error line numbers from D3DXCompileShader
		MaterialTemplateLineNumber += 3;

		MaterialCompilationOutput.UniformExpressionSet.SetParameterCollections(ParameterCollections);

		// This will be created shortly after the Translate call in FMaterial::BeginCompileShaderMap()
		//MaterialCompilationOutput.UniformExpressionSet.CreateBufferStruct();

		// Store the number of unique VT samples
		MaterialCompilationOutput.EstimatedNumVirtualTextureLookups = NumVtSamples;
	}
	ClearAllFunctionStacks();
		
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HLSLTranslation,(float)HLSLTranslateTime);
	return bSuccess;
}

void FHLSLMaterialTranslator::ValidateShadingModelsForFeatureLevel(const FMaterialShadingModelField& ShadingModels)
{
	if (FeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		const TArray<EMaterialShadingModel>& InvalidShadingModels = {};
		for (EMaterialShadingModel InvalidShadingModel : InvalidShadingModels)
		{
			if (ShadingModels.HasShadingModel(InvalidShadingModel))
			{
				FString FeatureLevelName;
				GetFeatureLevelName(FeatureLevel, FeatureLevelName);

				FString ShadingModelName;
				const UEnum* EnumPtr = FindObject<UEnum>(nullptr, TEXT("/Script/Engine.EMaterialShadingModel"), true);
				if (EnumPtr)
				{
					ShadingModelName = EnumPtr->GetNameStringByValue(InvalidShadingModel);
				}

				Errorf(TEXT("ShadingModel %s not supported in feature level %s"), *ShadingModelName, *FeatureLevelName);
			}
		}
	}
}

void FHLSLMaterialTranslator::GetMaterialEnvironment(EShaderPlatform InPlatform, FShaderCompilerEnvironment& OutEnvironment)
{
	bool bMaterialRequestsDualSourceBlending = false;

	if (bNeedsParticlePosition || Material->ShouldGenerateSphericalParticleNormals() || bUsesSphericalParticleOpacity)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_POSITION"), 1);
	}

	if (bNeedsParticleVelocity || Material->IsUsedWithNiagaraMeshParticles())
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_VELOCITY"), 1);
	}

	if (DynamicParticleParameterMask)
	{
		OutEnvironment.SetDefine(TEXT("USE_DYNAMIC_PARAMETERS"), 1);
		OutEnvironment.SetDefine(TEXT("DYNAMIC_PARAMETERS_MASK"), DynamicParticleParameterMask);
	}

	if (bNeedsParticleTime)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_TIME"), 1);
	}

	if (bUsesParticleMotionBlur)
	{
		OutEnvironment.SetDefine(TEXT("USES_PARTICLE_MOTION_BLUR"), 1);
	}

	if (bNeedsParticleRandom)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_RANDOM"), 1);
	}

	if (bUsesSphericalParticleOpacity)
	{
		OutEnvironment.SetDefine(TEXT("SPHERICAL_PARTICLE_OPACITY"), TEXT("1"));
	}

	if (bUsesParticleSubUVs)
	{
		OutEnvironment.SetDefine(TEXT("USE_PARTICLE_SUBUVS"), TEXT("1"));
	}

	if (bUsesLightmapUVs)
	{
		OutEnvironment.SetDefine(TEXT("LIGHTMAP_UV_ACCESS"),TEXT("1"));
	}

	if (bUsesAOMaterialMask)
	{
		OutEnvironment.SetDefine(TEXT("USES_AO_MATERIAL_MASK"),TEXT("1"));
	}

	if (bUsesSpeedTree)
	{
		OutEnvironment.SetDefine(TEXT("USES_SPEEDTREE"),TEXT("1"));
	}

	if (bNeedsWorldPositionExcludingShaderOffsets)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS"), TEXT("1"));
	}

	if (bNeedsParticleSize)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_SIZE"), TEXT("1"));
	}

	if (MaterialCompilationOutput.bNeedsSceneTextures)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_SCENE_TEXTURES"), TEXT("1"));
	}
	if (MaterialCompilationOutput.bUsesEyeAdaptation)
	{
		OutEnvironment.SetDefine(TEXT("USES_EYE_ADAPTATION"), TEXT("1"));
	}

	if (MaterialCompilationOutput.bHasRuntimeVirtualTextureOutputNode)
	{
		OutEnvironment.SetDefine(TEXT("VIRTUAL_TEXTURE_OUTPUT"), 1);
	}

	OutEnvironment.SetDefine(TEXT("USES_PER_INSTANCE_CUSTOM_DATA"), MaterialCompilationOutput.bUsesPerInstanceCustomData && Material->IsUsedWithInstancedStaticMeshes());
	OutEnvironment.SetDefine(TEXT("USES_PER_INSTANCE_RANDOM"), MaterialCompilationOutput.bUsesPerInstanceRandom && Material->IsUsedWithInstancedStaticMeshes());
	OutEnvironment.SetDefine(TEXT("USES_VERTEX_INTERPOLATOR"), MaterialCompilationOutput.bUsesVertexInterpolator);

	OutEnvironment.SetDefine(TEXT("MATERIAL_SKY_ATMOSPHERE"), bUsesSkyAtmosphere);
	OutEnvironment.SetDefine(TEXT("INTERPOLATE_VERTEX_COLOR"), bUsesVertexColor);
	OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_COLOR"), bUsesParticleColor); 
	OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_LOCAL_TO_WORLD"), bUsesParticleLocalToWorld);
	OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_WORLD_TO_LOCAL"), bUsesParticleWorldToLocal);
	OutEnvironment.SetDefine(TEXT("NEEDS_INSTANCE_LOCAL_TO_WORLD_PS"), bUsesInstanceLocalToWorldPS);
	OutEnvironment.SetDefine(TEXT("NEEDS_INSTANCE_WORLD_TO_LOCAL_PS"), bUsesInstanceWorldToLocalPS);
	OutEnvironment.SetDefine(TEXT("USES_TRANSFORM_VECTOR"), bUsesTransformVector);
	OutEnvironment.SetDefine(TEXT("WANT_PIXEL_DEPTH_OFFSET"), bUsesPixelDepthOffset);
	OutEnvironment.SetDefine(TEXT("USES_WORLD_POSITION_OFFSET"), bUsesWorldPositionOffset);
	OutEnvironment.SetDefine(TEXT("USES_EMISSIVE_COLOR"), bUsesEmissiveColor);
	// Distortion uses tangent space transform 
	OutEnvironment.SetDefine(TEXT("USES_DISTORTION"), Material->IsDistorted()); 

	OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_FOGGING"), Material->ShouldApplyFogging());
	OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_CLOUD_FOGGING"), Material->ShouldApplyCloudFogging());
	OutEnvironment.SetDefine(TEXT("MATERIAL_IS_SKY"), Material->IsSky());
	OutEnvironment.SetDefine(TEXT("MATERIAL_COMPUTE_FOG_PER_PIXEL"), Material->ComputeFogPerPixel());
	OutEnvironment.SetDefine(TEXT("MATERIAL_FULLY_ROUGH"), bIsFullyRough || Material->IsFullyRough());
	OutEnvironment.SetDefine(TEXT("MATERIAL_USES_ANISOTROPY"), bUsesAnisotropy && FDataDrivenShaderPlatformInfo::GetSupportsAnisotropicMaterials(InPlatform));

	OutEnvironment.SetDefine(TEXT("MATERIAL_DECAL_READ_MASK"), MaterialCompilationOutput.UsedDBufferTextures);

	// Count the number of VTStacks (each stack will allocate a feedback slot)
	OutEnvironment.SetDefine(TEXT("NUM_VIRTUALTEXTURE_SAMPLES"), VTStacks.Num());

	// Setup defines to map each VT stack to either 1 or 2 page table textures, depending on how many layers it uses
	for (int i = 0; i < VTStacks.Num(); ++i)
	{
		const FMaterialVirtualTextureStack& Stack = MaterialCompilationOutput.UniformExpressionSet.VTStacks[i];
		FString PageTableValue = FString::Printf(TEXT("Material.VirtualTexturePageTable0_%d"), i);
		if (Stack.GetNumLayers() > 4u)
		{
			PageTableValue += FString::Printf(TEXT(", Material.VirtualTexturePageTable1_%d"), i);
		}
		if (VTStacks[i].bAdaptive)
		{
			PageTableValue += FString::Printf(TEXT(", Material.VirtualTexturePageTableIndirection_%d"), i);
		}
		OutEnvironment.SetDefine(*FString::Printf(TEXT("VIRTUALTEXTURE_PAGETABLE_%d"), i), *PageTableValue);
	}

	for (int32 CollectionIndex = 0; CollectionIndex < ParameterCollections.Num(); CollectionIndex++)
	{
		// Add uniform buffer declarations for any parameter collections referenced
		const FString CollectionName = FString::Printf(TEXT("MaterialCollection%u"), CollectionIndex);
		// This can potentially become an issue for MaterialCollection Uniform Buffers if they ever get non-numeric resources (eg Textures), as
		// OutEnvironment.ResourceTableMap has a map by name, and the N ParameterCollection Uniform Buffers ALL are names "MaterialCollection"
		// (and the hlsl cbuffers are named MaterialCollection0, etc, so the names don't match the layout)
		FShaderUniformBufferParameter::ModifyCompilationEnvironment(*CollectionName, ParameterCollections[CollectionIndex]->GetUniformBufferStruct(), InPlatform, OutEnvironment);
	}
	OutEnvironment.SetDefine(TEXT("IS_MATERIAL_SHADER"), TEXT("1"));

	// Set all the shading models for this material here 
	FMaterialShadingModelField ShadingModels = Material->GetShadingModels();

	// If the material gets its shading model from the material expressions, then we use the result from the compilation (assuming it's valid).
	// This result will potentially be tighter than what GetShadingModels() returns, because it only picks up the shading models from the expressions that get compiled for a specific feature level and quality level
	// For example, the material might have shading models behind static switches. GetShadingModels() will return both the true and the false paths from that switch, whereas the shading model field from the compilation will only contain the actual shading model selected 
	if (Material->IsShadingModelFromMaterialExpression() && ShadingModelsFromCompilation.IsValid())
	{
		// Shading models fetched from the compilation of the expression graph
		ShadingModels = ShadingModelsFromCompilation;
	}

	ensure(ShadingModels.IsValid());

	if (ShadingModels.IsLit())
	{	
		int NumSetMaterials = 0;
		if (ShadingModels.HasShadingModel(MSM_DefaultLit))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_Subsurface))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_PreintegratedSkin))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_PREINTEGRATED_SKIN"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_SubsurfaceProfile))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_SUBSURFACE_PROFILE"), TEXT("1"));
			NumSetMaterials++;

			if (bUsesCurvature)
			{
				OutEnvironment.SetDefine(TEXT("MATERIAL_SUBSURFACE_PROFILE_USE_CURVATURE"), TEXT("1"));
			}
		}
		if (ShadingModels.HasShadingModel(MSM_ClearCoat))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_CLEAR_COAT"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_TwoSidedFoliage))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_TWOSIDED_FOLIAGE"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_Hair))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_HAIR"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_Cloth))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_CLOTH"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_Eye))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_EYE"), TEXT("1"));
			NumSetMaterials++;

			if (bUsesCurvature)
			{
				OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_EYE_USE_CURVATURE"), TEXT("1"));
			}
		}
		if (ShadingModels.HasShadingModel(MSM_SingleLayerWater))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_SINGLELAYERWATER"), TEXT("1"));
			NumSetMaterials++;
		}
		if (ShadingModels.HasShadingModel(MSM_ThinTranslucent))
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_THIN_TRANSLUCENT"), TEXT("1"));
			NumSetMaterials++;

			bMaterialRequestsDualSourceBlending = true;
		}

		if (ShadingModels.HasShadingModel(MSM_SingleLayerWater) && FDataDrivenShaderPlatformInfo::GetRequiresDisableForwardLocalLights(Platform))
		{
			OutEnvironment.SetDefine(TEXT("DISABLE_FORWARD_LOCAL_LIGHTS"), TEXT("1"));
		}

		// This is to have platforms use the simple single layer water shading similar to mobile: no dynamic lights, only sun and sky, no distortion, no colored transmittance on background, no custom depth read.
		const bool bSingleLayerWaterUsesSimpleShading = FDataDrivenShaderPlatformInfo::GetWaterUsesSimpleForwardShading(InPlatform) && IsForwardShadingEnabled(InPlatform);
		if (ShadingModels.HasShadingModel(MSM_SingleLayerWater) && bSingleLayerWaterUsesSimpleShading)
		{
			// Value must match SINGLE_LAYER_WATER_SHADING_QUALITY_MOBILE_WITH_DEPTH_TEXTURE in SingleLayerWaterCommon.ush!
			OutEnvironment.SetDefine(TEXT("SINGLE_LAYER_WATER_SHADING_QUALITY"), TEXT("1"));
		}

		const bool bIsWaterDistanceFieldShadowEnabled = IsWaterDistanceFieldShadowEnabled(InPlatform);
		const bool bIsWaterVSMFilteringEnabled = IsWaterVirtualShadowMapFilteringEnabled(InPlatform);
		if (ShadingModels.HasShadingModel(MSM_SingleLayerWater) && (bIsWaterDistanceFieldShadowEnabled || bIsWaterVSMFilteringEnabled))
		{
			OutEnvironment.SetDefine(TEXT("SINGLE_LAYER_WATER_SEPARATED_MAIN_LIGHT"), TEXT("1"));
		}

		if (NumSetMaterials == 1)
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SINGLE_SHADINGMODEL"), TEXT("1"));
		}

		ensure(NumSetMaterials != 0);
		if (NumSetMaterials == 0)
		{
			// Should not really end up here
			UE_LOG(LogMaterial, Warning, TEXT("Unknown material shading model(s). Setting to MSM_DefaultLit"));
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT"),TEXT("1"));
		}
	}
	else
	{
		// Unlit shading model can only exist by itself
		OutEnvironment.SetDefine(TEXT("MATERIAL_SINGLE_SHADINGMODEL"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_UNLIT"), TEXT("1"));
	}

	if (Material->GetMaterialDomain() == MD_Volume)
	{
		TArray<const UMaterialExpressionVolumetricAdvancedMaterialOutput*> VolumetricAdvancedExpressions;
		Material->GetMaterialInterface()->GetMaterial()->GetAllExpressionsOfType(VolumetricAdvancedExpressions);
		if (VolumetricAdvancedExpressions.Num() > 0)
		{
			if (VolumetricAdvancedExpressions.Num() > 1)
			{
				UE_LOG(LogMaterial, Fatal, TEXT("Only a single UMaterialExpressionVolumetricAdvancedMaterialOutput node is supported."));
			}

			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED"), TEXT("1"));

			const UMaterialExpressionVolumetricAdvancedMaterialOutput* VolumetricAdvancedNode = VolumetricAdvancedExpressions[0];
			if (VolumetricAdvancedNode->GetEvaluatePhaseOncePerSample())
			{
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_PHASE_PERSAMPLE"), TEXT("1"));
			}
			else
			{
				OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_PHASE_PERPIXEL"), TEXT("1"));
			}

			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_GRAYSCALE_MATERIAL"), VolumetricAdvancedNode->bGrayScaleMaterial ? TEXT("1") : TEXT("0"));
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_RAYMARCH_VOLUME_SHADOW"), VolumetricAdvancedNode->bRayMarchVolumeShadow ? TEXT("1") : TEXT("0"));
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_CLAMP_MULTISCATTERING_CONTRIBUTION"), VolumetricAdvancedNode->bClampMultiScatteringContribution ? TEXT("1") : TEXT("0"));

			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_MULTISCATTERING_OCTAVE_COUNT"), VolumetricAdvancedNode->GetMultiScatteringApproximationOctaveCount());

			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_CONSERVATIVE_DENSITY"),
				VolumetricAdvancedNode->ConservativeDensity.IsConnected() ? TEXT("1") : TEXT("0"));

			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_OVERRIDE_AMBIENT_OCCLUSION"),
				Material->HasAmbientOcclusionConnected() ? TEXT("1") : TEXT("0"));

			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_GROUND_CONTRIBUTION"),
				VolumetricAdvancedNode->bGroundContribution ? TEXT("1") : TEXT("0"));
		}

		TArray<const UMaterialExpressionVolumetricCloudEmptySpaceSkippingOutput*> EmptySpaceSkippingOutputExpressions;
		Material->GetMaterialInterface()->GetMaterial()->GetAllExpressionsOfType(EmptySpaceSkippingOutputExpressions);
		if (EmptySpaceSkippingOutputExpressions.Num() > 0)
		{
			if (VolumetricAdvancedExpressions.Num() > 1)
			{
				UE_LOG(LogMaterial, Fatal, TEXT("Only a single UMaterialExpressionEmptySpaceSkippingOutput node is supported."));
			}
			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_CLOUD_EMPTY_SPACE_SKIPPING_OUTPUT"), TEXT("1"));
		}
	}

	OutEnvironment.SetDefine(TEXT("MATERIAL_IS_STRATA"), bMaterialIsStrata ? TEXT("1") : TEXT("0"));

	// Strata requests dual source blending only for BLEND_TranslucentColoredTransmittance
	bMaterialRequestsDualSourceBlending |= bMaterialIsStrata && Material->GetBlendMode() == EBlendMode::BLEND_TranslucentColoredTransmittance;

	// if duals source blending (colored transmittance) is not supported on a platform, it will fall back to standard alpha blending (grey scale transmittance)
	OutEnvironment.SetDefine(TEXT("DUAL_SOURCE_COLOR_BLENDING_ENABLED"), bMaterialRequestsDualSourceBlending && Material->IsDualBlendingEnabled(Platform) ? TEXT("1") : TEXT("0"));

	if (bMaterialIsStrata)
	{
		OutEnvironment.SetDefine(TEXT("STRATA_MATERIAL_EXPORT_TYPE"), (int32)GetStrataMaterialExportType());
		OutEnvironment.SetDefine(TEXT("STRATA_MATERIAL_EXPORT_CONTEXT"), (int32)GetStrataMaterialExportContext());
		OutEnvironment.SetDefine(TEXT("STRATA_MATERIAL_EXPORT_LEGACY_BLEND_MODE"), (int32)GetStrataMaterialExportLegacyBlendMode());

		OutEnvironment.SetDefine(TEXT("STRATA_SINGLEPATH"), bStrataMaterialIsSingle ? TEXT("1") : TEXT("0"));
		OutEnvironment.SetDefine(TEXT("STRATA_FASTPATH"),   bStrataMaterialIsSingle ? TEXT("0") : (bStrataMaterialIsSimple ? TEXT("1") : TEXT("0")));

		check(StrataMaterialRootOperator);
		uint32 RootMaximumDistanceToLeaves = StrataMaterialRootOperator->MaxDistanceFromLeaves;
		OutEnvironment.SetDefine(TEXT("MATERIAL_TOPOLOGY_TREE_MAX_DEPTH"), RootMaximumDistanceToLeaves);

		OutEnvironment.SetDefine(TEXT("STRATA_USES_CONVERSION_FROM_LEGACY"), bStrataUsesConversionFromLegacy ? 1 : 0);

		OutEnvironment.SetDefine(TEXT("STRATA_MATERIAL_OUTPUT_OPAQUE_ROUGH_REFRACTIONS"), bStrataOutputsOpaqueRoughRefractions ? 1 : 0);

		OutEnvironment.SetDefine(TEXT("STRATA_OPTIMIZED_UNLIT"), bStrataMaterialIsUnlitNode ? 1 : 0);

		// Compute the shared local basis count and generate the hlsl shader code for it.
		StrataPixelNormalInitializerValues = FString::Printf(TEXT("\n\n\n\t// Strata normal and tangent\n"));
		FinalUsedSharedLocalBasesCount = 0;
		uint8 RequestedSharedLocalBasesCount = 0;
		StrataEvaluateSharedLocalBases(FinalUsedSharedLocalBasesCount, RequestedSharedLocalBasesCount, &StrataPixelNormalInitializerValues, &OutEnvironment);

		// Now write some feedback to the user
		{
			// Output some debug info as comment in code and in the material stat window
			const uint32 StrataBytePerPixel = Strata::GetBytePerPixel(InPlatform);

			OutEnvironment.SetDefine(TEXT("STRATA_CLAMPED_BSDF_COUNT"), StrataMaterialBSDFCount);

			FString StrataMaterialDescription;

			StrataMaterialDescription += FString::Printf(TEXT("----- SUBSTRATE -----\r\n"));
			StrataMaterialDescription += FString::Printf(TEXT("Substrate Compilation Info -\r\n"));
			StrataMaterialDescription += FString::Printf(TEXT(" - Byte Per Pixel Budget                      %u\r\n"), StrataBytePerPixel);
			StrataMaterialDescription += FString::Printf(TEXT(" - Requested Byte Size before simplification  %u (%d UINT32)\r\n"), StrataSimplificationStatus.OriginalRequestedByteSize, StrataSimplificationStatus.OriginalRequestedByteSize / 4);
			StrataMaterialDescription += FString::Printf(TEXT(" - Requested Byte Size after simplification   %u (%d UINT32)\r\n"), StrataMaterialRequestedSizeByte, StrataMaterialRequestedSizeByte / 4);
			StrataMaterialDescription += FString::Printf(TEXT(" - Material complexity                        %s\r\n"), bStrataMaterialIsSingle ? TEXT("SINGLE") : (bStrataMaterialIsSimple ? TEXT("SIMPLE") : TEXT("COMPLEX")));
			StrataMaterialDescription += FString::Printf(TEXT(" - TotalBSDFCount                             %i\r\n"), StrataMaterialBSDFCount);
			if (RequestedSharedLocalBasesCount > STRATA_MAX_SHAREDLOCALBASES_REGISTERS)
			{
				StrataMaterialDescription += FString::Printf(TEXT(" - SharedLocalBasesCount                      %i (Requested:%i)\r\n"), FinalUsedSharedLocalBasesCount, RequestedSharedLocalBasesCount);
			}
			else
			{
				StrataMaterialDescription += FString::Printf(TEXT(" - SharedLocalBasesCount                      %i\r\n"), FinalUsedSharedLocalBasesCount);
			}

			for (int32 OpIt = 0; OpIt < StrataMaterialExpressionRegisteredOperators.Num(); ++OpIt)
			{
				FStrataOperator& BSDFOperator = StrataMaterialExpressionRegisteredOperators[OpIt];
				if (BSDFOperator.BSDFIndex == INDEX_NONE || BSDFOperator.IsDiscarded())
				{
					continue;	// not a BSDF or if discarded (i.e. not the root of a parameter blending subtree), then there is no local basis to register
				}
				if (BSDFOperator.BSDFRegisteredSharedLocalBasis.NormalCodeChunk == INDEX_NONE && BSDFOperator.BSDFRegisteredSharedLocalBasis.TangentCodeChunk == INDEX_NONE)
				{
					continue;	// We skip null normal on certain BSDF, for instance unlit.
				}
				const FStrataSharedLocalBasesInfo& StrataSharedLocalBasesInfo = StrataCompilationInfoGetMatchingSharedLocalBasisInfo(BSDFOperator.BSDFRegisteredSharedLocalBasis);

				StrataMaterialDescription += FString::Printf(TEXT("     - %s - SharedLocalBasisIndexMacro = %s \r\n"), *GetStrataBSDFName(BSDFOperator.BSDFType), *GetStrataSharedLocalBasisIndexMacro(BSDFOperator.BSDFRegisteredSharedLocalBasis));
			}

			StrataMaterialDescription += FString::Printf(TEXT("----------- SUBSTRATE TREE -----------\r\n"));
			StrataMaterialDescription += FString::Printf(TEXT("Graph maximum distance to leaves %u\r\n"), RootMaximumDistanceToLeaves);
			// Debug print operators according to depth from root.
			{
				check(StrataMaterialRootOperator);
				for (int32 DistanceToLeaves = RootMaximumDistanceToLeaves; DistanceToLeaves >= 0; --DistanceToLeaves)
				{
					StrataMaterialDescription += FString::Printf(TEXT("----- DistanceFromLeaves = %d -----\r\n"), DistanceToLeaves);
					for (auto& It : StrataMaterialExpressionRegisteredOperators)
					{
						if (!It.IsDiscarded() && It.MaxDistanceFromLeaves == DistanceToLeaves)
						{
							StrataMaterialDescription += FString::Printf(TEXT("\tIdx=%d Op=%s ParentIdx=%d LeftIndex=%d RightIndex=%d BSDFIdx=%d LayerDepth=%d IsTop=%d IsBot=%d BSDFType=%s SSS=%d MFP=%d F90=%d Rough2=%d Fuzz=%d  Aniso=%d \r\n"),
								It.Index, GetStrataOperatorStr(It.OperatorType), It.ParentIndex, It.LeftIndex, It.RightIndex, It.BSDFIndex, It.LayerDepth, It.bIsTop, It.bIsBottom, 
								*GetStrataBSDFName(It.BSDFType), It.bBSDFHasSSS, It.bBSDFHasMFPPluggedIn, It.bBSDFHasEdgeColor, It.bBSDFHasSecondRoughnessOrSimpleClearCoat, It.bBSDFHasFuzz, It.bBSDFHasAnisotropy);
						}
					}
				}
			}

			ResourcesString += TEXT("/*");
			ResourcesString += StrataMaterialDescription;
			ResourcesString += TEXT("*/");

			MaterialCompilationOutput.StrataMaterialDescription = StrataMaterialDescription;
		}

	}

	OutEnvironment.SetDefine(TEXT("TEXTURE_SAMPLE_DEBUG"), IsDebugTextureSampleEnabled() ? TEXT("1") : TEXT("0"));
}

// Assign custom interpolators to slots, packing them as much as possible in unused slots.
TBitArray<> FHLSLMaterialTranslator::GetVertexInterpolatorsOffsets(FString& VertexInterpolatorsOffsetsDefinitionCode) const
{
	TBitArray<> AllocatedCoords = AllocatedUserTexCoords; // Don't mess with the already assigned sets of UV coords

	int32 CurrentSlot = INDEX_NONE;
	int32 EndAllocatedSlot = INDEX_NONE;

	auto GetNextUVSlot = [&CurrentSlot, &EndAllocatedSlot, &AllocatedCoords]() -> int32
	{
		if (CurrentSlot == EndAllocatedSlot)
		{
			CurrentSlot = AllocatedCoords.FindAndSetFirstZeroBit();
			if (CurrentSlot == INDEX_NONE)
			{
				CurrentSlot = AllocatedCoords.Add(true);
			}

			// Track one slot per component (u,v)
			const int32 NUM_COMPONENTS = 2;
			CurrentSlot *= NUM_COMPONENTS;
			EndAllocatedSlot = CurrentSlot + NUM_COMPONENTS;
		}

		int32 ResultUVSlot = CurrentSlot / 2;
		CurrentSlot++;

		return ResultUVSlot;
	};

	TArray<UMaterialExpressionVertexInterpolator*> SortedInterpolators;
	Algo::TransformIf(CustomVertexInterpolators, 
						SortedInterpolators, 
						[](const UMaterialExpressionVertexInterpolator* Interpolator) { return Interpolator && Interpolator->InterpolatorIndex != INDEX_NONE && Interpolator->InterpolatorOffset != INDEX_NONE; },
						[](UMaterialExpressionVertexInterpolator* Interpolator) { return Interpolator; });
						
	SortedInterpolators.Sort([](const UMaterialExpressionVertexInterpolator& LHS, const UMaterialExpressionVertexInterpolator& RHS)  { return LHS.InterpolatorOffset < RHS.InterpolatorOffset; });
		
	for (UMaterialExpressionVertexInterpolator* Interpolator : SortedInterpolators)
	{
		int32 Index = Interpolator->InterpolatorIndex;

		const EMaterialValueType Type = Interpolator->InterpolatedType == MCT_Float ? MCT_Float1 : Interpolator->InterpolatedType;

		VertexInterpolatorsOffsetsDefinitionCode += LINE_TERMINATOR;
		VertexInterpolatorsOffsetsDefinitionCode += FString::Printf(TEXT("#define VERTEX_INTERPOLATOR_%i_TEXCOORDS_X\t%i") LINE_TERMINATOR, Index, GetNextUVSlot());

		if (Type >= MCT_Float2)
		{
			VertexInterpolatorsOffsetsDefinitionCode += FString::Printf(TEXT("#define VERTEX_INTERPOLATOR_%i_TEXCOORDS_Y\t%i") LINE_TERMINATOR, Index, GetNextUVSlot());

			if (Type >= MCT_Float3)
			{
				VertexInterpolatorsOffsetsDefinitionCode += FString::Printf(TEXT("#define VERTEX_INTERPOLATOR_%i_TEXCOORDS_Z\t%i") LINE_TERMINATOR, Index, GetNextUVSlot());

				if (Type == MCT_Float4)
				{
					VertexInterpolatorsOffsetsDefinitionCode += FString::Printf(TEXT("#define VERTEX_INTERPOLATOR_%i_TEXCOORDS_W\t%i") LINE_TERMINATOR, Index, GetNextUVSlot());
				}
			}
		}
			
		VertexInterpolatorsOffsetsDefinitionCode += LINE_TERMINATOR;
	}

	return AllocatedCoords;
}

void FHLSLMaterialTranslator::GetSharedInputsMaterialCode(FString& PixelMembersDeclaration, FString& NormalAssignment, FString& PixelMembersInitializationEpilog, ECompiledPartialDerivativeVariation DerivativeVariation)
{
	{
		int32 LastProperty = -1;

		FString PixelInputInitializerValues;
		FString NormalInitializerValue;

		for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
		{
			// Skip non-shared properties
			if (!SharedPixelProperties[PropertyIndex])
			{
				continue;
			}

			const EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
			check(FMaterialAttributeDefinitionMap::GetShaderFrequency(Property) == SF_Pixel);
			// Special case MP_SubsurfaceColor as the actual property is a combination of the color and the profile but we don't want to expose the profile
			const FString PropertyName = Property == MP_SubsurfaceColor ? "Subsurface" : FMaterialAttributeDefinitionMap::GetAttributeName(Property);
			check(PropertyName.Len() > 0);				
			const EMaterialValueType Type = Property == MP_SubsurfaceColor ? MCT_Float4 : FMaterialAttributeDefinitionMap::GetValueType(Property);

			if (!bEnableExecutionFlow)
			{
			// Normal requires its own separate initializer
			if (Property == MP_Normal)
			{
					NormalInitializerValue = FString::Printf(TEXT("\tPixelMaterialInputs.%s = %s;\n"), *PropertyName, *DerivativeVariations[DerivativeVariation].TranslatedCodeChunks[Property]);
			}
			else
			{
					if (DerivativeVariations[DerivativeVariation].TranslatedCodeChunkDefinitions[Property].Len() > 0)
					{
						LastProperty = Property;
					}
				}

				PixelInputInitializerValues += FString::Printf(TEXT("\tPixelMaterialInputs.%s = %s;\n"), *PropertyName, *DerivativeVariations[DerivativeVariation].TranslatedCodeChunks[Property]);
			}

			PixelMembersDeclaration += FString::Printf(TEXT("\t%s %s;\n"), HLSLTypeString(Type), *PropertyName);
		}

		NormalAssignment = NormalInitializerValue;
		if (LastProperty != -1)
		{
			PixelMembersInitializationEpilog += DerivativeVariations[DerivativeVariation].TranslatedCodeChunkDefinitions[LastProperty] + TEXT("\n");
		}

		if (CodeChunkToStrataSharedLocalBasis.Num() > 0)
		{
			PixelInputInitializerValues += StrataPixelNormalInitializerValues;
		}

		PixelMembersInitializationEpilog += PixelInputInitializerValues;
	}
}

FString FHLSLMaterialTranslator::GetMaterialShaderCode()
{	
	// use "/Engine/Private/MaterialTemplate.ush" to create the functions to get data (e.g. material attributes) and code (e.g. material expressions to create specular color) from C++
	FLazyPrintf LazyPrintf(*MaterialTemplate);

	// Assign slots to vertex interpolators
	FString VertexInterpolatorsOffsetsDefinition;
	TBitArray<> FinalAllocatedCoords = GetVertexInterpolatorsOffsets(VertexInterpolatorsOffsetsDefinition);

	const uint32 NumUserVertexTexCoords = GetNumUserVertexTexCoords();
	const uint32 NumUserTexCoords = GetNumUserTexCoords();
	const uint32 NumCustomVectors = FMath::DivideAndRoundUp((uint32)CurrentCustomVertexInterpolatorOffset, 2u);
	const uint32 NumTexCoordVectors = FinalAllocatedCoords.FindLast(true) + 1;

	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),NumUserVertexTexCoords));
	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),NumUserTexCoords));
	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),NumCustomVectors));
	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),NumTexCoordVectors));

	LazyPrintf.PushParam(*VertexInterpolatorsOffsetsDefinition);

	FString MaterialAttributesDeclaration;
	FString MaterialAttributesUtilities;

	const EMaterialShadingModel DefaultShadingModel = Material->GetShadingModels().GetFirstShadingModel();

	MaterialAttributesDeclaration += TEXT("struct FMaterialAttributes\n{\n");

	const TArray<FGuid>& OrderedVisibleAttributes = FMaterialAttributeDefinitionMap::GetOrderedVisibleAttributeList();
	for (const FGuid& AttributeID : OrderedVisibleAttributes)
	{
		const FString PropertyName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
		const EMaterialValueType PropertyType = FMaterialAttributeDefinitionMap::GetValueType(AttributeID);
		const TCHAR* HLSLType = nullptr;

		switch (PropertyType)
		{
		case MCT_Float1: case MCT_Float: HLSLType = TEXT("float"); break;
		case MCT_Float2: HLSLType = TEXT("float2"); break;
		case MCT_Float3: HLSLType = TEXT("float3"); break;
		case MCT_Float4: HLSLType = TEXT("float4"); break;
		case MCT_UInt: case MCT_UInt1: case MCT_ShadingModel: HLSLType = TEXT("uint"); break;
		case MCT_UInt2: HLSLType = TEXT("uint2"); break;
		case MCT_UInt3: HLSLType = TEXT("uint3"); break;
		case MCT_UInt4: HLSLType = TEXT("uint4"); break;
		case MCT_Strata: HLSLType = TEXT("FStrataData"); break;
		default: break;
		}

		if (HLSLType)
		{
			const FVector4f DefaultValue = FMaterialAttributeDefinitionMap::GetDefaultValue(AttributeID);

			MaterialAttributesDeclaration += FString::Printf(TEXT("\t%s %s;") LINE_TERMINATOR, HLSLType, *PropertyName);

			// Chainable method to set the attribute
			MaterialAttributesUtilities += FString::Printf(TEXT("FMaterialAttributes FMaterialAttributes_Set%s(FMaterialAttributes InAttributes, %s InValue) { InAttributes.%s = InValue; return InAttributes; }") LINE_TERMINATOR,
				*PropertyName, HLSLType, *PropertyName);
		}
	}

	MaterialAttributesDeclaration += TEXT("};\n");

	LazyPrintf.PushParam(*MaterialAttributesDeclaration);
	LazyPrintf.PushParam(*MaterialAttributesUtilities);

	// Stores the shared shader results member declarations
	FString PixelMembersDeclaration[CompiledPDV_MAX];

	FString NormalAssignment[CompiledPDV_MAX];

	// Stores the code to initialize all inputs after MP_Normal
	FString PixelMembersSetupAndAssignments[CompiledPDV_MAX];

	for (int32 Index = 0; Index < CompiledPDV_MAX; Index++)
	{
		GetSharedInputsMaterialCode(PixelMembersDeclaration[Index], NormalAssignment[Index], PixelMembersSetupAndAssignments[Index], (ECompiledPartialDerivativeVariation)Index);
	}

	// PixelMembersDeclaration should be the same for all variations, but might change in the future. There are cases where work is shared
	// between the pixel and vertex shader, but with Nanite all work has to be moved into the pixel shader, which means we will want
	// different inputs. But for now, we are keeping them the same.
	LazyPrintf.PushParam(*PixelMembersDeclaration[CompiledPDV_FiniteDifferences]);

	{
		FString DerivativeHelpers = DerivativeAutogen.GenerateUsedFunctions(*this);
		FString DerivativeHelpersAndResources = DerivativeHelpers + ResourcesString;
		//LazyPrintf.PushParam(*ResourcesString);
		LazyPrintf.PushParam(*DerivativeHelpersAndResources);
	}

	// Anything used bye the GenerationFunctionCode() like WorldPositionOffset shouldn't be using texures, right?
	// Let those use the standard finite differences textures, since they should be the same. If we actually want
	// those to handle texture reads properly, we'll have to make extra versions.
	ECompiledPartialDerivativeVariation BaseDerivativeVariation = CompiledPDV_FiniteDifferences;

	if (bCompileForComputeShader)
	{
		LazyPrintf.PushParam(*GenerateFunctionCode(CompiledMP_EmissiveColorCS, BaseDerivativeVariation));
	}
	else
	{
		LazyPrintf.PushParam(TEXT("return 0"));
	}

	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material->GetTranslucencyDirectionalLightingIntensity()));

	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material->GetTranslucentShadowDensityScale()));
	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material->GetTranslucentSelfShadowDensityScale()));
	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material->GetTranslucentSelfShadowSecondDensityScale()));
	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material->GetTranslucentSelfShadowSecondOpacity()));
	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material->GetTranslucentBackscatteringExponent()));

	{
		FLinearColor Extinction = Material->GetTranslucentMultipleScatteringExtinction();

		LazyPrintf.PushParam(*FString::Printf(TEXT("return MaterialFloat3(%.5f, %.5f, %.5f)"), Extinction.R, Extinction.G, Extinction.B));
	}

	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material->GetOpacityMaskClipValue()));

	LazyPrintf.PushParam(!bEnableExecutionFlow ? *GenerateFunctionCode(MP_WorldPositionOffset, BaseDerivativeVariation) : TEXT("return Parameters.MaterialAttributes.WorldPositionOffset"));
	LazyPrintf.PushParam(!bEnableExecutionFlow ? *GenerateFunctionCode(CompiledMP_PrevWorldPositionOffset, BaseDerivativeVariation) : TEXT("return 0.0f"));
	LazyPrintf.PushParam(!bEnableExecutionFlow ? *GenerateFunctionCode(MP_CustomData0, BaseDerivativeVariation) : TEXT("return 0.0f"));
	LazyPrintf.PushParam(!bEnableExecutionFlow ? *GenerateFunctionCode(MP_CustomData1, BaseDerivativeVariation) : TEXT("return 0.0f"));

	// Print custom texture coordinate assignments, should be fine with regular derivatives
	FString CustomUVAssignments;

	int32 LastProperty = -1;
	for (uint32 CustomUVIndex = 0; CustomUVIndex < NumUserTexCoords; CustomUVIndex++)
	{
		if (bEnableExecutionFlow)
		{
			const FString AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName((EMaterialProperty)(MP_CustomizedUVs0 + CustomUVIndex));
			CustomUVAssignments += FString::Printf(TEXT("\tOutTexCoords[%u] = Parameters.MaterialAttributes.%s;") LINE_TERMINATOR, CustomUVIndex, *AttributeName);
		}
		else
		{
		if (CustomUVIndex == 0)
		{
				CustomUVAssignments += DerivativeVariations[BaseDerivativeVariation].TranslatedCodeChunkDefinitions[MP_CustomizedUVs0 + CustomUVIndex];
		}

			if (DerivativeVariations[BaseDerivativeVariation].TranslatedCodeChunkDefinitions[MP_CustomizedUVs0 + CustomUVIndex].Len() > 0)
			{
				LastProperty = MP_CustomizedUVs0 + CustomUVIndex;
			}
			CustomUVAssignments += FString::Printf(TEXT("\tOutTexCoords[%u] = %s;") LINE_TERMINATOR, CustomUVIndex, *DerivativeVariations[BaseDerivativeVariation].TranslatedCodeChunks[MP_CustomizedUVs0 + CustomUVIndex]);
		}
	}

	LazyPrintf.PushParam(*CustomUVAssignments);

	// Print custom vertex shader interpolator assignments
	FString CustomInterpolatorAssignments;

	for (UMaterialExpressionVertexInterpolator* Interpolator : CustomVertexInterpolators)
	{
		if (Interpolator->InterpolatorOffset != INDEX_NONE)
		{
			check(Interpolator->InterpolatorIndex != INDEX_NONE);
			check(Interpolator->InterpolatedType & MCT_Float);

			const EMaterialValueType Type = Interpolator->InterpolatedType == MCT_Float ? MCT_Float1 : Interpolator->InterpolatedType;
			const TCHAR* Swizzle[2] = { TEXT("x"), TEXT("y") };
			const int32 Offset = Interpolator->InterpolatorOffset;
			const int32 Index = Interpolator->InterpolatorIndex;

			// Note: We reference the UV define directly to avoid having to pre-accumulate UV counts before property translation
			CustomInterpolatorAssignments += FString::Printf(TEXT("\tOutTexCoords[VERTEX_INTERPOLATOR_%i_TEXCOORDS_X].%s = VertexInterpolator%i(Parameters).x;") LINE_TERMINATOR, Index, Swizzle[Offset%2], Index);
				
			if (Type >= MCT_Float2)
			{
				CustomInterpolatorAssignments += FString::Printf(TEXT("\tOutTexCoords[VERTEX_INTERPOLATOR_%i_TEXCOORDS_Y].%s = VertexInterpolator%i(Parameters).y;") LINE_TERMINATOR, Index, Swizzle[(Offset+1)%2], Index);

				if (Type >= MCT_Float3)
				{
					CustomInterpolatorAssignments += FString::Printf(TEXT("\tOutTexCoords[VERTEX_INTERPOLATOR_%i_TEXCOORDS_Z].%s = VertexInterpolator%i(Parameters).z;") LINE_TERMINATOR, Index, Swizzle[(Offset+2)%2], Index);

					if (Type == MCT_Float4)
					{
						CustomInterpolatorAssignments += FString::Printf(TEXT("\tOutTexCoords[VERTEX_INTERPOLATOR_%i_TEXCOORDS_W].%s = VertexInterpolator%i(Parameters).w;") LINE_TERMINATOR, Index, Swizzle[(Offset+3)%2], Index);
					}
				}
			}
		}
	}

	LazyPrintf.PushParam(*CustomInterpolatorAssignments);

	if (bEnableExecutionFlow)
	{
		FString EvaluateVertexCode;

		// Set default texcoords in the VS
		for (uint32 TexCoordIndex = 0; TexCoordIndex < NumUserVertexTexCoords; ++TexCoordIndex)
		{
			const FString AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName((EMaterialProperty)(MP_CustomizedUVs0 + TexCoordIndex));

			EvaluateVertexCode += FString::Printf(TEXT("\tDefaultMaterialAttributes.%s = Parameters.TexCoords[%d];") LINE_TERMINATOR, *AttributeName, TexCoordIndex);
		}

		EvaluateVertexCode += TranslatedAttributesCodeChunks[SF_Vertex];

		LazyPrintf.PushParam(*TranslatedAttributesCodeChunks[SF_Pixel]);

		FString EvaluateMaterialAttributesCode = TEXT("    FMaterialAttributes MaterialAttributes = EvaluatePixelMaterialAttributes(Parameters);" LINE_TERMINATOR);

		for (int32 PropertyIndex = 0; PropertyIndex < MP_MAX; ++PropertyIndex)
		{
			// Skip non-shared properties
			if (!SharedPixelProperties[PropertyIndex])
			{
				continue;
			}

			const EMaterialProperty Property = (EMaterialProperty)PropertyIndex;
			check(FMaterialAttributeDefinitionMap::GetShaderFrequency(Property) == SF_Pixel);
			// Special case MP_SubsurfaceColor as the actual property is a combination of the color and the profile but we don't want to expose the profile
			const FString PropertyName = FMaterialAttributeDefinitionMap::GetAttributeName(Property);

			if (PropertyIndex == MP_SubsurfaceColor)
			{
				// TODO - properly handle subsurface profile
				EvaluateMaterialAttributesCode += FString::Printf("    PixelMaterialInputs.Subsurface = float4(MaterialAttributes.%s, 0.0f);" LINE_TERMINATOR, *PropertyName);
			}
			else
			{
				EvaluateMaterialAttributesCode += FString::Printf("    PixelMaterialInputs.%s = MaterialAttributes.%s;" LINE_TERMINATOR, *PropertyName, *PropertyName);
			}
		}

		// TODO - deriv
		for (int32 Iter = 0; Iter < CompiledPDV_MAX; Iter++)
		{
			LazyPrintf.PushParam(*EvaluateMaterialAttributesCode);
			LazyPrintf.PushParam(TEXT(""));
			LazyPrintf.PushParam(TEXT(""));
		}
	}
	else
	{
		// skip material attributes code
		LazyPrintf.PushParam(TEXT(""));

		for (int32 Iter = 0; Iter < CompiledPDV_MAX; Iter++)
		{
			ECompiledPartialDerivativeVariation Variation = (ECompiledPartialDerivativeVariation)Iter;

	// Initializers required for Normal
			LazyPrintf.PushParam(*DerivativeVariations[Variation].TranslatedCodeChunkDefinitions[MP_Normal]);
			LazyPrintf.PushParam(*NormalAssignment[Variation]);
	// Finally the rest of common code followed by assignment into each input
			LazyPrintf.PushParam(*PixelMembersSetupAndAssignments[Variation]);
		}
	}

	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"),MaterialTemplateLineNumber));

	return LazyPrintf.GetResultString();
}


// ========== PROTECTED: ========== //

bool FHLSLMaterialTranslator::IsMaterialPropertyUsed(EMaterialProperty Property, int32 PropertyChunkIndex, const FLinearColor& ReferenceValue, int32 NumComponents) const
{
	const int32 Frequency = (int32)FMaterialAttributeDefinitionMap::GetShaderFrequency(Property);
	bool bPropertyUsed = false;

	if ((MaterialAttributesReturned[Frequency] & (1ull << Property)) != 0u)
	{
		// Property was set via a 'Return Material Attributes' expression
		bPropertyUsed = true;
	}
	else if (PropertyChunkIndex == -1)
	{
		bPropertyUsed = false;
	}
	else
	{
		const FShaderCodeChunk& PropertyChunk = SharedPropertyCodeChunks[Frequency][PropertyChunkIndex];

		// Determine whether the property is used. 
		// If the output chunk has a uniform expression, it is constant, and GetNumberValue returns the default property value then property isn't used.
		bPropertyUsed = true;

		if( PropertyChunk.UniformExpression && PropertyChunk.UniformExpression->IsConstant() )
		{
			FLinearColor Value;
			FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
			PropertyChunk.UniformExpression->GetNumberValue(DummyContext, Value);

			if ((NumComponents < 1 || Value.R == ReferenceValue.R)
				&& (NumComponents < 2 || Value.G == ReferenceValue.G)
				&& (NumComponents < 3 || Value.B == ReferenceValue.B)
				&& (NumComponents < 4 || Value.A == ReferenceValue.A))
			{
				bPropertyUsed = false;
			}
		}
	}

	return bPropertyUsed;
}

// only used by GetMaterialShaderCode()
// @param Index ECompiledMaterialProperty or EMaterialProperty
FString FHLSLMaterialTranslator::GenerateFunctionCode(uint32 Index, ECompiledPartialDerivativeVariation Variation) const
{
	check(Index < CompiledMP_MAX);
	return DerivativeVariations[Variation].TranslatedCodeChunkDefinitions[Index] + TEXT("	return ") + DerivativeVariations[Variation].TranslatedCodeChunks[Index] + TEXT(";");
}


const FShaderCodeChunk& FHLSLMaterialTranslator::AtParameterCodeChunk(int32 Index) const
{
	check(Index != INDEX_NONE);
	checkf(Index >= 0 && Index < CurrentScopeChunks->Num(), TEXT("Index %d/%d, Platform=%d"), Index, CurrentScopeChunks->Num(), (int)Platform);
	const FShaderCodeChunk& CodeChunk = (*CurrentScopeChunks)[Index];
	return CodeChunk;
}

EDerivativeStatus FHLSLMaterialTranslator::GetDerivativeStatus(int32 Index) const
{
	return AtParameterCodeChunk(Index).DerivativeStatus;
}

FDerivInfo FHLSLMaterialTranslator::GetDerivInfo(int32 Index, bool bAllowNonFloat) const
{
	const FShaderCodeChunk& CodeChunk = AtParameterCodeChunk(Index);
	return FDerivInfo { CodeChunk.Type, GetDerivType(CodeChunk.Type, bAllowNonFloat), CodeChunk.DerivativeStatus };
}

// Similar to GetParameterCode, but has no default, and is derivative aware. Making it a separate function in case it needs to diverge,
// but after looking at it, it has the same logic as GetParameterCode() for now.
FString FHLSLMaterialTranslator::GetParameterCodeDeriv(int32 Index, ECompiledPartialDerivativeVariation Variation)
{
	// In the case of a uniform expression, both finite and deriv versions are the same (raw floats, known zero)
	checkf(Index >= 0 && Index < CurrentScopeChunks->Num(), TEXT("Index %d/%d, Platform=%d"), Index, CurrentScopeChunks->Num(), (int)Platform);
	const FShaderCodeChunk& CodeChunk = (*CurrentScopeChunks)[Index];

	FString Result;
	if((CodeChunk.UniformExpression && CodeChunk.UniformExpression->IsConstant()) || CodeChunk.bInline)
	{
		// Constant uniform expressions and code chunks which are marked to be inlined are accessed via Definition
		Result = CodeChunk.AtDefinition(Variation);
	}
	else
	{
		if (CodeChunk.UniformExpression)
		{
			// If the code chunk has a uniform expression, create a new code chunk to access it
			const int32 AccessedIndex = AccessUniformExpression(Index);
			const FShaderCodeChunk& AccessedCodeChunk = (*CurrentScopeChunks)[AccessedIndex];
			if(AccessedCodeChunk.bInline)
			{
				// Handle the accessed code chunk being inlined
				Result = AccessedCodeChunk.AtDefinition(Variation);
			}
			else
			{
				// Return the symbol used to reference this code chunk
				check(AccessedCodeChunk.SymbolName.Len() > 0);
				Result = AccessedCodeChunk.SymbolName;
			}
		}
		else
		{
			// Return the symbol used to reference this code chunk
			check(CodeChunk.SymbolName.Len() > 0);
			Result = CodeChunk.SymbolName;
		}
	}

	return Result;
}

FString FHLSLMaterialTranslator::GetParameterCode(int32 Index, const TCHAR* Default)
{
	FString Ret = GetParameterCodeRaw(Index,Default);

	if (GetDerivativeStatus(Index) == EDerivativeStatus::Valid)
	{
		Ret = TEXT("DERIV_BASE_VALUE(") + Ret + TEXT(")");
	}

	return Ret;
}

// GetParameterCode
FString FHLSLMaterialTranslator::GetParameterCodeRaw(int32 Index, const TCHAR* Default)
{
	if(Index == INDEX_NONE && Default)
	{
		return Default;
	}

	checkf(Index >= 0 && Index < CurrentScopeChunks->Num(), TEXT("Index %d/%d, Platform=%d"), Index, CurrentScopeChunks->Num(), (int)Platform);
	const FShaderCodeChunk& CodeChunk = (*CurrentScopeChunks)[Index];
	if((CodeChunk.UniformExpression && CodeChunk.UniformExpression->IsConstant()) || CodeChunk.bInline)
	{
		// Constant uniform expressions and code chunks which are marked to be inlined are accessed via Definition
		return CodeChunk.DefinitionFinite;
	}
	else
	{
		// TODO - don't access uniform expressions for intermediate values
		if (CodeChunk.UniformExpression)
		{
			// If the code chunk has a uniform expression, create a new code chunk to access it
			const int32 AccessedIndex = AccessUniformExpression(Index);
			const FShaderCodeChunk& AccessedCodeChunk = (*CurrentScopeChunks)[AccessedIndex];
			if(AccessedCodeChunk.bInline)
			{
				// Handle the accessed code chunk being inlined
				return AccessedCodeChunk.DefinitionFinite;
			}
			// Return the symbol used to reference this code chunk
			check(AccessedCodeChunk.SymbolName.Len() > 0);
			return AccessedCodeChunk.SymbolName;
		}
			
		ReferencedCodeChunks.AddUnique(Index);
			
		// Return the symbol used to reference this code chunk
		check(CodeChunk.SymbolName.Len() > 0);
		return CodeChunk.SymbolName;
	}
}

uint64 FHLSLMaterialTranslator::GetParameterHash(int32 Index)
{
	if (Index == INDEX_NONE)
	{
		return 0u;
	}

	checkf(Index >= 0 && Index < CurrentScopeChunks->Num(), TEXT("Index %d/%d, Platform=%d"), Index, CurrentScopeChunks->Num(), (int)Platform);
	const FShaderCodeChunk& CodeChunk = (*CurrentScopeChunks)[Index];

	if (CodeChunk.UniformExpression && !CodeChunk.UniformExpression->IsConstant())
	{
		// Non-constant uniform expressions are accessed through a separate code chunk...need to give the hash of that
		const int32 AccessedIndex = AccessUniformExpression(Index);
		const FShaderCodeChunk& AccessedCodeChunk = (*CurrentScopeChunks)[AccessedIndex];
		return AccessedCodeChunk.Hash;
	}

	return CodeChunk.Hash;
}

uint64 FHLSLMaterialTranslator::GetParameterMaterialAttributeMask(int32 Index)
{
	if (Index == INDEX_NONE)
	{
		return 0u;
	}

	checkf(Index >= 0 && Index < CurrentScopeChunks->Num(), TEXT("Index %d/%d, Platform=%d"), Index, CurrentScopeChunks->Num(), (int)Platform);
	const FShaderCodeChunk& CodeChunk = (*CurrentScopeChunks)[Index];
	check(CodeChunk.Type == MCT_MaterialAttributes);
	check(!CodeChunk.UniformExpression);

	return CodeChunk.MaterialAttributeMask;
}

void FHLSLMaterialTranslator::SetParameterMaterialAttributes(int32 Index, uint64 Mask)
{
	checkf(Index >= 0 && Index < CurrentScopeChunks->Num(), TEXT("Index %d/%d, Platform=%d"), Index, CurrentScopeChunks->Num(), (int)Platform);
	FShaderCodeChunk& CodeChunk = (*CurrentScopeChunks)[Index];
	check(CodeChunk.Type == MCT_MaterialAttributes);
	CodeChunk.MaterialAttributeMask |= Mask;
}


/** Creates a string of all definitions needed for the given material input. */
FString FHLSLMaterialTranslator::GetDefinitions(const TArray<FShaderCodeChunk>& CodeChunks, int32 StartChunk, int32 EndChunk, ECompiledPartialDerivativeVariation Variation, const TCHAR* ReturnValueSymbolName) const
{
	FString Definitions;
	for (int32 ChunkIndex = StartChunk; ChunkIndex < EndChunk; ChunkIndex++)
	{
		const FShaderCodeChunk& CodeChunk = CodeChunks[ChunkIndex];
		// Uniform expressions (both constant and variable) and inline expressions don't have definitions.
		if (!CodeChunk.UniformExpression &&
			(!CodeChunk.bInline || CodeChunk.Type == MCT_VoidStatement))
		{
			Definitions += CodeChunk.AtDefinition(Variation);

			// If we found the definition of the return value, there is no need to add more definitions as they won't contribute to the outcome
			if (ReturnValueSymbolName != nullptr && CodeChunk.SymbolName == ReturnValueSymbolName)
			{
				break;
			}
		}
	}
	return Definitions;
}

// GetFixedParameterCode
void FHLSLMaterialTranslator::GetFixedParameterCode(int32 StartChunk, int32 EndChunk, int32 ResultIndex, TArray<FShaderCodeChunk>& CodeChunks, FString& OutDefinitions, FString& OutValue, ECompiledPartialDerivativeVariation OriginalVariation, bool bReduceAfterReturnValue)
{
	// Only allow the analytic variation for pixel shaders.
	ECompiledPartialDerivativeVariation Variation = OriginalVariation;

	// This function is hardcoded to finite differences for now.
	if (ResultIndex != INDEX_NONE)
	{
		checkf(ResultIndex >= 0 && ResultIndex < CodeChunks.Num(), TEXT("Index out of range %d/%d [%s]"), ResultIndex, CodeChunks.Num(), *Material->GetFriendlyName());
		check(!CodeChunks[ResultIndex].UniformExpression || CodeChunks[ResultIndex].UniformExpression->IsConstant());
		if (CodeChunks[ResultIndex].UniformExpression && CodeChunks[ResultIndex].UniformExpression->IsConstant())
		{
			// Handle a constant uniform expression being the only code chunk hooked up to a material input
			const FShaderCodeChunk& ResultChunk = CodeChunks[ResultIndex];
			OutValue = ResultChunk.AtDefinition(Variation);
		}
		else
		{
			const FShaderCodeChunk& ResultChunk = CodeChunks[ResultIndex];
			// Combine the definition lines and the return statement.
			// Also specify the return symbol name to terminate the iteration over all definitions earlier,
			// if there are unnecessary statements that don't contribute to the outcome.
			check(ResultChunk.bInline || ResultChunk.SymbolName.Len() > 0);
			const TCHAR* ReturnValueSymbolName = (bReduceAfterReturnValue && !ResultChunk.bInline ? *ResultChunk.SymbolName : nullptr);
			OutDefinitions = GetDefinitions(CodeChunks, StartChunk, EndChunk, Variation, ReturnValueSymbolName);
			OutValue = ResultChunk.bInline ? ResultChunk.AtDefinition(Variation) : ResultChunk.SymbolName;
			if (Variation == CompiledPDV_Analytic && ResultChunk.DerivativeStatus == EDerivativeStatus::Valid)
			{
				OutValue = FString(TEXT("(")) + OutValue + TEXT(").Value");
			}
		}
	}
	else
	{
		OutValue = TEXT("0");
	}
}

void FHLSLMaterialTranslator::GetFixedParameterCode(int32 ResultIndex, TArray<FShaderCodeChunk>& CodeChunks, FString& OutDefinitions, FString& OutValue, ECompiledPartialDerivativeVariation Variation)
{
	GetFixedParameterCode(0, CodeChunks.Num(), ResultIndex, CodeChunks, OutDefinitions, OutValue, Variation);
}

static void AppendIndent(int32 Level, FString& OutValue)
{
	for (int32 i = 0; i < Level; ++i)
	{
		OutValue += '\t';
	}
}

void FHLSLMaterialTranslator::LinkParentScopes(TArray<FShaderCodeChunk>& CodeChunks)
{
	// Add all chunks to parent's child array
	for (int32 ChunkIndex = 0; ChunkIndex < CodeChunks.Num(); ++ChunkIndex)
	{
		FShaderCodeChunk& ChildChunk = CodeChunks[ChunkIndex];
		if (ChildChunk.UsedScopeIndex != INDEX_NONE)
		{
			FShaderCodeChunk& ParentChunk = CodeChunks[ChildChunk.UsedScopeIndex];
			ParentChunk.ScopedChunks.Add(ChunkIndex);
		}
	}
}

void FHLSLMaterialTranslator::GetScopeCode(int32 IndentLevel, int32 ScopeChunkIndex, const TArray<FShaderCodeChunk>& CodeChunks, TSet<int32>& EmittedChunks, FString& OutValue)
{
	bool bAlreadyAddedChunk = false;
	EmittedChunks.Add(ScopeChunkIndex, &bAlreadyAddedChunk);
	if (bAlreadyAddedChunk)
	{
		// Only emit code for the chunk the first time we visit it
		return;
	}

	const FShaderCodeChunk& CodeChunk = CodeChunks[ScopeChunkIndex];

	// Add code for any dependencies of the current chunk
	for (int32 ReferencedChunkIndex : CodeChunk.ReferencedCodeChunks)
{
		GetScopeCode(IndentLevel, ReferencedChunkIndex, CodeChunks, EmittedChunks, OutValue);
	}

	if ((!CodeChunk.UniformExpression && !CodeChunk.bInline) || CodeChunk.Type == MCT_VoidStatement)
	{
		AppendIndent(IndentLevel, OutValue);
		OutValue += CodeChunk.AtDefinition(CompiledPDV_FiniteDifferences); // TODO - deriv
	}

	if (CodeChunk.ScopedChunks.Num() > 0)
	{
		AppendIndent(IndentLevel, OutValue);
		OutValue += TEXT("{\n");
		for (int32 ChildChunkIndex : CodeChunk.ScopedChunks)
		{
			const FShaderCodeChunk& ChildChunk = CodeChunks[ChildChunkIndex];
			check(ChildChunk.UsedScopeIndex == ScopeChunkIndex);
			if (ChildChunk.DeclaredScopeIndex != ScopeChunkIndex)
			{
				GetScopeCode(IndentLevel + 1, ChildChunkIndex, CodeChunks, EmittedChunks, OutValue);
			}
		}
		for (int32 ChildChunkIndex : CodeChunk.ScopedChunks)
		{
			const FShaderCodeChunk& ChildChunk = CodeChunks[ChildChunkIndex];
			if (ChildChunk.DeclaredScopeIndex == ScopeChunkIndex)
			{
				GetScopeCode(IndentLevel + 1, ChildChunkIndex, CodeChunks, EmittedChunks, OutValue);
			}
		}
		AppendIndent(IndentLevel, OutValue);
		OutValue += TEXT("}\n");
	}
}

/** Used to get a user friendly type from EMaterialValueType */
const TCHAR* FHLSLMaterialTranslator::DescribeType(EMaterialValueType Type) const
{
	switch(Type)
	{
	case MCT_Float1:				return TEXT("float");
	case MCT_Float2:				return TEXT("float2");
	case MCT_Float3:				return TEXT("float3");
	case MCT_Float4:				return TEXT("float4");
	case MCT_Float:					return TEXT("float");
	case MCT_Texture2D:				return TEXT("texture2D");
	case MCT_TextureCube:			return TEXT("textureCube");
	case MCT_Texture2DArray:		return TEXT("texture2DArray");
	case MCT_TextureCubeArray:		return TEXT("textureCubeArray");
	case MCT_VolumeTexture:			return TEXT("volumeTexture");
	case MCT_StaticBool:			return TEXT("static bool");
	case MCT_Bool:					return TEXT("bool");
	case MCT_MaterialAttributes:	return TEXT("MaterialAttributes");
	case MCT_TextureExternal:		return TEXT("TextureExternal");
	case MCT_TextureVirtual:		return TEXT("TextureVirtual");
	case MCT_VTPageTableResult:		return TEXT("VTPageTableResult");
	case MCT_ShadingModel:			return TEXT("ShadingModel");
	case MCT_UInt:					return TEXT("uint");
	case MCT_UInt1:					return TEXT("uint");
	case MCT_UInt2:					return TEXT("uint2");
	case MCT_UInt3:					return TEXT("uint3");
	case MCT_UInt4:					return TEXT("uint4");
	case MCT_Strata:				return TEXT("Strata");
	case MCT_LWCScalar:				return TEXT("LWCScalar");
	case MCT_LWCVector2:			return TEXT("LWCVector2");
	case MCT_LWCVector3:			return TEXT("LWCVector3");
	case MCT_LWCVector4:			return TEXT("LWCVector4");
	default:						return TEXT("unknown");
	};
}

/** Used to get an HLSL type from EMaterialValueType */
const TCHAR* FHLSLMaterialTranslator::HLSLTypeString(EMaterialValueType Type) const
{
	switch(Type)
	{
	case MCT_Float1:				return TEXT("MaterialFloat");
	case MCT_Float2:				return TEXT("MaterialFloat2");
	case MCT_Float3:				return TEXT("MaterialFloat3");
	case MCT_Float4:				return TEXT("MaterialFloat4");
	case MCT_Float:					return TEXT("MaterialFloat");
	case MCT_Texture2D:				return TEXT("texture2D");
	case MCT_TextureCube:			return TEXT("textureCube");
	case MCT_Texture2DArray:		return TEXT("texture2DArray");
	case MCT_TextureCubeArray:		return TEXT("textureCubeArray");
	case MCT_VolumeTexture:			return TEXT("volumeTexture");
	case MCT_StaticBool:			return TEXT("static bool");
	case MCT_Bool:					return TEXT("bool");
	case MCT_MaterialAttributes:	return TEXT("FMaterialAttributes");
	case MCT_TextureExternal:		return TEXT("TextureExternal");
	case MCT_TextureVirtual:		return TEXT("TextureVirtual");
	case MCT_VTPageTableResult:		return TEXT("VTPageTableResult");
	case MCT_ShadingModel:			return TEXT("uint");
	case MCT_UInt:					return TEXT("uint");
	case MCT_UInt1:					return TEXT("uint");
	case MCT_UInt2:					return TEXT("uint2");
	case MCT_UInt3:					return TEXT("uint3");
	case MCT_UInt4:					return TEXT("uint4");
	case MCT_Strata:				return TEXT("FStrataData");
	case MCT_LWCScalar:				return TEXT("FLWCScalar");
	case MCT_LWCVector2:			return TEXT("FLWCVector2");
	case MCT_LWCVector3:			return TEXT("FLWCVector3");
	case MCT_LWCVector4:			return TEXT("FLWCVector4");
	default:						return TEXT("unknown");
	};
}

/** Used to get an HLSL type from EMaterialValueType */
const TCHAR* FHLSLMaterialTranslator::HLSLTypeStringDeriv(EMaterialValueType Type, EDerivativeStatus DerivativeStatus) const
{
	switch(Type)
	{
	case MCT_Float1:				return (DerivativeStatus == EDerivativeStatus::Valid) ? TEXT("FloatDeriv") : TEXT("MaterialFloat");
	case MCT_Float2:				return (DerivativeStatus == EDerivativeStatus::Valid) ? TEXT("FloatDeriv2") : TEXT("MaterialFloat2");
	case MCT_Float3:				return (DerivativeStatus == EDerivativeStatus::Valid) ? TEXT("FloatDeriv3") : TEXT("MaterialFloat3");
	case MCT_Float4:				return (DerivativeStatus == EDerivativeStatus::Valid) ? TEXT("FloatDeriv4") : TEXT("MaterialFloat4");
	case MCT_Float:					return (DerivativeStatus == EDerivativeStatus::Valid) ? TEXT("FloatDeriv") : TEXT("MaterialFloat");
	case MCT_Texture2D:				return TEXT("texture2D");
	case MCT_TextureCube:			return TEXT("textureCube");
	case MCT_Texture2DArray:		return TEXT("texture2DArray");
	case MCT_TextureCubeArray:		return TEXT("textureCubeArray");
	case MCT_VolumeTexture:			return TEXT("volumeTexture");
	case MCT_StaticBool:			return TEXT("static bool");
	case MCT_Bool:					return TEXT("bool");
	case MCT_MaterialAttributes:	return TEXT("FMaterialAttributes");
	case MCT_TextureExternal:		return TEXT("TextureExternal");
	case MCT_TextureVirtual:		return TEXT("TextureVirtual");
	case MCT_VTPageTableResult:		return TEXT("VTPageTableResult");
	case MCT_ShadingModel:			return TEXT("uint");
	case MCT_UInt:					return TEXT("uint");
	case MCT_UInt1:					return TEXT("uint");
	case MCT_UInt2:					return TEXT("uint2");
	case MCT_UInt3:					return TEXT("uint3");
	case MCT_UInt4:					return TEXT("uint4");
	case MCT_Strata:				return TEXT("FStrataData");
	case MCT_LWCScalar:				return (DerivativeStatus == EDerivativeStatus::Valid) ? TEXT("FLWCScalarDeriv") : TEXT("FLWCScalar");
	case MCT_LWCVector2:			return (DerivativeStatus == EDerivativeStatus::Valid) ? TEXT("FLWCVector2Deriv") : TEXT("FLWCVector2");
	case MCT_LWCVector3:			return (DerivativeStatus == EDerivativeStatus::Valid) ? TEXT("FLWCVector3Deriv") : TEXT("FLWCVector3");
	case MCT_LWCVector4:			return (DerivativeStatus == EDerivativeStatus::Valid) ? TEXT("FLWCVector4Deriv") : TEXT("FLWCVector4");
	default:						return TEXT("unknown");
	};
}

int32 FHLSLMaterialTranslator::NonPixelShaderExpressionError()
{
	return Errorf(TEXT("Invalid node used in vertex/hull/domain shader input!"));
}

int32 FHLSLMaterialTranslator::ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::Type RequiredFeatureLevel)
{
	if (FeatureLevel < RequiredFeatureLevel)
	{
		FString FeatureLevelName, RequiredLevelName;
		GetFeatureLevelName(FeatureLevel, FeatureLevelName);
		GetFeatureLevelName(RequiredFeatureLevel, RequiredLevelName);
		return Errorf(TEXT("Node not supported in feature level %s. %s required."), *FeatureLevelName, *RequiredLevelName);
	}

	return 0;
}

int32 FHLSLMaterialTranslator::NonVertexShaderExpressionError()
{
	return Errorf(TEXT("Invalid node used in pixel/hull/domain shader input!"));
}

int32 FHLSLMaterialTranslator::NonVertexOrPixelShaderExpressionError()
{
	return Errorf(TEXT("Invalid node used in hull/domain shader input!"));
}

void FHLSLMaterialTranslator::AddEstimatedTextureSample(const uint32 Count)
{
	if (IsCurrentlyCompilingForPreviousFrame())
	{
		// Ignore non-actionable cases
		return;
	}

	if (ShaderFrequency == SF_Pixel || ShaderFrequency == SF_Compute)
	{
		MaterialCompilationOutput.EstimatedNumTextureSamplesPS += Count;
	}
	else
	{
		MaterialCompilationOutput.EstimatedNumTextureSamplesVS += Count;
	}
}

/** Creates a unique symbol name and adds it to the symbol list. */
FString FHLSLMaterialTranslator::CreateSymbolName(const TCHAR* SymbolNameHint)
{
	NextSymbolIndex++;
	return FString(SymbolNameHint) + FString::FromInt(NextSymbolIndex);
}

/** Adds an already formatted inline or referenced code chunk */
int32 FHLSLMaterialTranslator::AddCodeChunkInner(uint64 Hash, const TCHAR* FormattedCode, EMaterialValueType Type, EDerivativeStatus DerivativeStatus, bool bInlined)
{
	check(bAllowCodeChunkGeneration);

	if (Type == MCT_Unknown)
	{
		return INDEX_NONE;
	}

	int32 CodeIndex = INDEX_NONE;
	if (Type == MCT_VoidStatement)
	{
		CodeIndex = CurrentScopeChunks->Num();
		const FString Statement = FString("") + FormattedCode + LINE_TERMINATOR;
		new(*CurrentScopeChunks) FShaderCodeChunk(Hash, *Statement, *Statement, TEXT(""), Type, DerivativeStatus, true);
	}
	else if (bInlined)
	{
		CodeIndex = CurrentScopeChunks->Num();
		// Adding an inline code chunk, the definition will be the code to inline
		new(*CurrentScopeChunks) FShaderCodeChunk(Hash, FormattedCode, FormattedCode, TEXT(""), Type, DerivativeStatus, true);
	}
	// Can only create temporaries for certain types
	else if ((Type & (MCT_Float | MCT_LWCType | MCT_VTPageTableResult | MCT_UInt)) || Type == MCT_ShadingModel || Type == MCT_MaterialAttributes || Type == MCT_Strata || Type == MCT_UInt)
	{
		// Check for existing
		for (int32 i = 0; i < CurrentScopeChunks->Num(); ++i)
		{
			if ((*CurrentScopeChunks)[i].Hash == Hash)
			{
				CodeIndex = i;
				break;
			}
		}

		if (CodeIndex == INDEX_NONE)
		{
			CodeIndex = CurrentScopeChunks->Num();
		// Allocate a local variable name
		const FString SymbolName = CreateSymbolName(TEXT("Local"));
		// Construct the definition string which stores the result in a temporary and adds a newline for readability
			const FString LocalVariableDefinitionFinite = FString("	") + HLSLTypeString(Type) + TEXT(" ") + SymbolName + TEXT(" = ") + FormattedCode + TEXT(";") + LINE_TERMINATOR;
			// Construct the definition string which stores the result in a temporary and adds a newline for readability
			const FString LocalVariableDefinitionAnalytic = FString("	") + HLSLTypeString(Type) + TEXT(" ") + SymbolName + TEXT(" = ") + FormattedCode + TEXT(";") + LINE_TERMINATOR;
		// Adding a code chunk that creates a local variable
			new(*CurrentScopeChunks) FShaderCodeChunk(Hash, *LocalVariableDefinitionFinite, *LocalVariableDefinitionAnalytic, SymbolName, Type, DerivativeStatus, false);
		}
	}
	else
	{
		if (Type & MCT_Texture)
		{
			return Errorf(TEXT("Operation not supported on a Texture"));
		}
		else if (Type == MCT_StaticBool)
		{
			return Errorf(TEXT("Operation not supported on a Static Bool"));
		}
		else
		{
			return Errorf(TEXT("Operation not supported for type %s"), DescribeType(Type));
		}
	}
	
	AddCodeChunkToCurrentScope(CodeIndex);
	return CodeIndex;
}
			
static inline uint32 GetTCharStringBytes(const TCHAR* String)
{
	uint32 Length = 0u;
	while (String[Length])
	{
		++Length;
	}
	return Length * sizeof(TCHAR);
}

/** Adds an already formatted inline or referenced code chunk, and notes the derivative status. */
int32 FHLSLMaterialTranslator::AddCodeChunkInnerDeriv(const TCHAR* FormattedCodeFinite, const TCHAR* FormattedCodeAnalytic, EMaterialValueType Type, bool bInlined, EDerivativeStatus DerivativeStatus)
{
	const bool bEmitInvalidDerivToken = GDebugEmitInvalidDerivTokensEnabled && !IsDerivativeValid(DerivativeStatus);

	const uint64 Hash = CityHash64WithSeed((const char*)FormattedCodeFinite, GetTCharStringBytes(FormattedCodeFinite),
		CityHash64((const char*)FormattedCodeAnalytic, GetTCharStringBytes(FormattedCodeAnalytic)));

	check(bAllowCodeChunkGeneration);

	if (Type == MCT_Unknown)
	{
		return INDEX_NONE;
	}

	check(Type != MCT_VoidStatement);

	int32 CodeIndex = INDEX_NONE;
	if (bInlined)
	{
		CodeIndex = CurrentScopeChunks->Num();
		// Adding an inline code chunk, the definition will be the code to inline
		new(*CurrentScopeChunks) FShaderCodeChunk(Hash, FormattedCodeFinite, FormattedCodeAnalytic, TEXT(""), Type, DerivativeStatus, true);
	}
	// Can only create temporaries for certain types
	else if ((Type & (MCT_Float | MCT_LWCType | MCT_VTPageTableResult | MCT_UInt)) || Type == MCT_ShadingModel || Type == MCT_MaterialAttributes || Type == MCT_Strata)
	{
		// Check for existing
		for (int32 i = 0; i < CurrentScopeChunks->Num(); ++i)
		{
			if ((*CurrentScopeChunks)[i].Hash == Hash)
			{
				CodeIndex = i;
				break;
			}
		}

		if (CodeIndex == INDEX_NONE)
		{
			CodeIndex = CurrentScopeChunks->Num();
			// Allocate a local variable name
			const FString SymbolName = CreateSymbolName(TEXT("Local"));
			// Construct the definition string which stores the result in a temporary and adds a newline for readability
			const FString LocalVariableDefinitionFinite = FString("	") + HLSLTypeString(Type) + TEXT(" ") + SymbolName + TEXT(" = ") + FormattedCodeFinite + TEXT(";") + LINE_TERMINATOR;
			// Analytic version too
			const FString LocalVariableDefinitionAnalytic = FString("	") + (bEmitInvalidDerivToken ? "$" : "") + HLSLTypeStringDeriv(Type, DerivativeStatus) + TEXT(" ") + SymbolName + TEXT(" = ") + FormattedCodeAnalytic + TEXT(";") + LINE_TERMINATOR;
			// Adding a code chunk that creates a local variable
			new(*CurrentScopeChunks) FShaderCodeChunk(Hash, *LocalVariableDefinitionFinite, *LocalVariableDefinitionAnalytic, SymbolName, Type, DerivativeStatus, false);
		}
	}
	else
	{
		if (Type & MCT_Texture)
		{
			return Errorf(TEXT("Operation not supported on a Texture"));
		}

		if (Type == MCT_StaticBool)
		{
			return Errorf(TEXT("Operation not supported on a Static Bool"));
		}

	}

	AddCodeChunkToCurrentScope(CodeIndex);
	return CodeIndex;
}

int32 FHLSLMaterialTranslator::AddCodeChunkInnerDeriv(const TCHAR* FormattedCode, EMaterialValueType Type, bool bInlined, EDerivativeStatus DerivativeStatus)
{
	return AddCodeChunkInnerDeriv(FormattedCode, FormattedCode, Type, bInlined, DerivativeStatus);
}

/** 
	* Constructs the formatted code chunk and creates a new local variable definition from it. 
	* This should be used over AddInlinedCodeChunk when the code chunk adds actual instructions, and especially when calling a function.
	* Creating local variables instead of inlining simplifies the generated code and reduces redundant expression chains,
	* Making compiles faster and enabling the shader optimizer to do a better job.
	*/
int32 FHLSLMaterialTranslator::AddCodeChunkInner(EMaterialValueType Type, EDerivativeStatus DerivativeStatus, bool bInlined, const TCHAR* Format, ...)
{
	int32	BufferSize		= 256;
	TCHAR*	FormattedCode	= NULL;
	int32	Result			= -1;

	while(Result == -1)
	{
		FormattedCode = (TCHAR*) FMemory::Realloc( FormattedCode, BufferSize * sizeof(TCHAR) );
		GET_VARARGS_RESULT( FormattedCode, BufferSize, BufferSize-1, Format, Format, Result );
		BufferSize *= 2;
	};
	FormattedCode[Result] = 0;

	const uint64 Hash = CityHash64((char*)FormattedCode, Result * sizeof(TCHAR));
	const int32 CodeIndex = AddCodeChunkInner(Hash, FormattedCode, Type, DerivativeStatus, bInlined);
	FMemory::Free(FormattedCode);

	return CodeIndex;
}

int32 FHLSLMaterialTranslator::AddCodeChunkWithHash(uint64 BaseHash, EMaterialValueType Type, const TCHAR* Format, ...)
{
	int32	BufferSize = 256;
	TCHAR*	FormattedCode = NULL;
	int32	Result = -1;

	while (Result == -1)
	{
		FormattedCode = (TCHAR*)FMemory::Realloc(FormattedCode, BufferSize * sizeof(TCHAR));
		GET_VARARGS_RESULT(FormattedCode, BufferSize, BufferSize - 1, Format, Format, Result);
		BufferSize *= 2;
	};
	FormattedCode[Result] = 0;

	uint64 Hash = CityHash64((char*)FormattedCode, Result * sizeof(TCHAR));
	Hash = CityHash128to64({ BaseHash, Hash });
	const int32 CodeIndex = AddCodeChunkInner(Hash, FormattedCode, Type, EDerivativeStatus::NotAware, false);
	FMemory::Free(FormattedCode);

	return CodeIndex;
}

int32 FHLSLMaterialTranslator::AddUniformExpressionInner(uint64 Hash, FMaterialUniformExpression* UniformExpression, EMaterialValueType Type, const TCHAR* FormattedCode)
{
	check(bAllowCodeChunkGeneration);

	if (Type == MCT_Unknown)
	{
		return INDEX_NONE;
	}

	check(UniformExpression);

	// Only a texture uniform expression can have MCT_Texture type
	if ((Type & MCT_Texture) && !UniformExpression->GetTextureUniformExpression()
		&& !UniformExpression->GetExternalTextureUniformExpression())
	{
		return Errorf(TEXT("Operation not supported on a Texture"));
	}

	// External textures must have an external texture uniform expression
	if ((Type & MCT_TextureExternal) && !UniformExpression->GetExternalTextureUniformExpression())
	{
		return Errorf(TEXT("Operation not supported on an external texture"));
	}

	if (Type == MCT_StaticBool)
	{
		return Errorf(TEXT("Operation not supported on a Static Bool"));
	}

	if (Type == MCT_MaterialAttributes)
	{
		return Errorf(TEXT("Operation not supported on a MaterialAttributes"));
	}

	bool bFoundExistingExpression = false;
	// Search for an existing code chunk with the same uniform expression in the array of all uniform expressions used by this material.
	for (int32 ExpressionIndex = 0; ExpressionIndex < UniformExpressions.Num() && !bFoundExistingExpression; ExpressionIndex++)
	{
		FMaterialUniformExpression* TestExpression = UniformExpressions[ExpressionIndex].UniformExpression;
		check(TestExpression);
		if (TestExpression->IsIdentical(UniformExpression))
		{
			bFoundExistingExpression = true;
			// This code chunk has an identical uniform expression to the new expression, reuse it.
			// This allows multiple material properties to share uniform expressions because AccessUniformExpression uses AddUniqueItem when adding uniform expressions.
			check(Type == UniformExpressions[ExpressionIndex].Type);
			// Search for an existing code chunk with the same uniform expression in the array of code chunks for this material property.
			for (int32 ChunkIndex = 0; ChunkIndex < CurrentScopeChunks->Num(); ChunkIndex++)
			{
				FMaterialUniformExpression* OtherExpression = (*CurrentScopeChunks)[ChunkIndex].UniformExpression;
				if (OtherExpression && OtherExpression->IsIdentical(UniformExpression))
				{
					delete UniformExpression;
					// Reuse the entry in CurrentScopeChunks
					AddCodeChunkToCurrentScope(ChunkIndex);
					return ChunkIndex;
				}
			}
			delete UniformExpression;
			// Use the existing uniform expression from a different material property,
			// And continue so that a code chunk using the uniform expression will be generated for this material property.
			UniformExpression = TestExpression;
			break;
		}

#if 0
		// Test for the case where we have non-identical expressions of the same type and name.
		// This means they exist with separate values and the one retrieved for shading will
		// effectively be random, as we evaluate the first found during expression traversal
		if (TestExpression->GetType() == UniformExpression->GetType())
		{
			if (TestExpression->GetType() == &FMaterialUniformExpressionScalarParameter::StaticType)
			{
				FMaterialUniformExpressionScalarParameter* ScalarParameterA = (FMaterialUniformExpressionScalarParameter*)TestExpression;
				FMaterialUniformExpressionScalarParameter* ScalarParameterB = (FMaterialUniformExpressionScalarParameter*)UniformExpression;

				if (!ScalarParameterA->GetParameterInfo().Name.IsNone() && ScalarParameterA->GetParameterInfo() == ScalarParameterB->GetParameterInfo())
				{
					delete UniformExpression;
					return Errorf(TEXT("Invalid scalar parameter '%s' found. Identical parameters must have the same value."), *(ScalarParameterA->GetParameterInfo().Name.ToString()));
				}
			}
			else if (TestExpression->GetType() == &FMaterialUniformExpressionVectorParameter::StaticType)
			{
				FMaterialUniformExpressionVectorParameter* VectorParameterA = (FMaterialUniformExpressionVectorParameter*)TestExpression;
				FMaterialUniformExpressionVectorParameter* VectorParameterB = (FMaterialUniformExpressionVectorParameter*)UniformExpression;

				// Note: Skipping NAME_SelectionColor here as this behavior is relied on for editor materials
				if (!VectorParameterA->GetParameterInfo().Name.IsNone() && VectorParameterA->GetParameterInfo() == VectorParameterB->GetParameterInfo()
					&& VectorParameterA->GetParameterInfo().Name != NAME_SelectionColor)
				{
					delete UniformExpression;
					return Errorf(TEXT("Invalid vector parameter '%s' found. Identical parameters must have the same value."), *(VectorParameterA->GetParameterInfo().Name.ToString()));
				}
			}
		}
#endif
	}

	const int32 ReturnIndex = CurrentScopeChunks->Num();
	// Create a new code chunk for the uniform expression
	// Note that uniforms have a known-zero derivative
	new(*CurrentScopeChunks) FShaderCodeChunk(Hash, UniformExpression, FormattedCode, FormattedCode, Type, EDerivativeStatus::Zero);

	if (!bFoundExistingExpression)
	{
		// Add an entry to the material-wide list of uniform expressions
		new(UniformExpressions) FShaderCodeChunk(Hash, UniformExpression, FormattedCode, FormattedCode, Type, EDerivativeStatus::Zero);
	}

	AddCodeChunkToCurrentScope(ReturnIndex);
	return ReturnIndex;
}

// AddUniformExpression - Adds an input to the Code array and returns its index.
int32 FHLSLMaterialTranslator::AddUniformExpression(FMaterialUniformExpression* UniformExpression,EMaterialValueType Type, const TCHAR* Format,...)
{
	int32	BufferSize = 256;
	TCHAR*	FormattedCode = NULL;
	int32	Result = -1;

	while (Result == -1)
	{
		FormattedCode = (TCHAR*)FMemory::Realloc(FormattedCode, BufferSize * sizeof(TCHAR));
		GET_VARARGS_RESULT(FormattedCode, BufferSize, BufferSize - 1, Format, Format, Result);
		BufferSize *= 2;
	};
	FormattedCode[Result] = 0;

	const uint64 Hash = CityHash64((char*)FormattedCode, Result * sizeof(TCHAR));
	const int32 CodeIndex = AddUniformExpressionInner(Hash, UniformExpression, Type, FormattedCode);
	FMemory::Free(FormattedCode);
	return CodeIndex;
}

// From MaterialUniformExpressions.cpp
extern void WriteMaterialUniformAccess(UE::Shader::EValueComponentType ComponentType, uint32 NumComponents, uint32 UniformOffset, FStringBuilderBase& OutResult);

// AccessUniformExpression - Adds code to access the value of a uniform expression to the Code array and returns its index.
int32 FHLSLMaterialTranslator::AccessUniformExpression(int32 Index)
{
	check(Index >= 0 && Index < CurrentScopeChunks->Num());
	const FShaderCodeChunk&	CodeChunk = (*CurrentScopeChunks)[Index];
	check(CodeChunk.UniformExpression && !CodeChunk.UniformExpression->IsConstant());
	const bool bIsLWC = IsLWCType(CodeChunk.Type);

	FMaterialUniformExpressionTexture* TextureUniformExpression = CodeChunk.UniformExpression->GetTextureUniformExpression();
	FMaterialUniformExpressionExternalTexture* ExternalTextureUniformExpression = CodeChunk.UniformExpression->GetExternalTextureUniformExpression();

	// Any code chunk can have a texture uniform expression (eg FMaterialUniformExpressionFlipBookTextureParameter),
	// But a texture code chunk must have a texture uniform expression
	check(!(CodeChunk.Type & MCT_Texture) || TextureUniformExpression || ExternalTextureUniformExpression);
	// External texture samples must have a corresponding uniform expression
	check(!(CodeChunk.Type & MCT_TextureExternal) || ExternalTextureUniformExpression);
	// Virtual texture samples must have a corresponding uniform expression
	check(!(CodeChunk.Type & MCT_TextureVirtual) || TextureUniformExpression);

	TStringBuilder<1024> FormattedCode;

	if (CodeChunk.Type == MCT_Bool)
	{
		check(CodeChunk.UniformExpression->GetType() == &FMaterialUniformExpressionStaticBoolParameter::StaticType);
		FMaterialUniformExpressionStaticBoolParameter* StaticBoolParameter = static_cast<FMaterialUniformExpressionStaticBoolParameter*>(CodeChunk.UniformExpression.GetReference());
		check(!bIsLWC);

		const uint32 NumComponents = GetNumComponents(CodeChunk.Type);

		if (CodeChunk.UniformExpression->UniformOffset == INDEX_NONE)
		{
			// 'Bool' uniforms are packed into bits
			if (CurrentNumBoolComponents + NumComponents > 32u)
			{
				CurrentBoolUniformOffset = UniformPreshaderOffset++;
				CurrentNumBoolComponents = 0u;
			}

			int32 UniformOffset = CurrentBoolUniformOffset * 32u + CurrentNumBoolComponents;

			FUniformExpressionSet& UniformExpressionSet = MaterialCompilationOutput.UniformExpressionSet;
			FMaterialUniformPreshaderHeader& PreshaderHeader = UniformExpressionSet.UniformPreshaders.AddDefaulted_GetRef();
			PreshaderHeader.FieldIndex = UniformExpressionSet.UniformPreshaderFields.Num();
			PreshaderHeader.NumFields = 1;
			PreshaderHeader.OpcodeOffset = UniformExpressionSet.UniformPreshaderData.Num();
			CodeChunk.UniformExpression->WriteNumberOpcodes(UniformExpressionSet.UniformPreshaderData);
			PreshaderHeader.OpcodeSize = UniformExpressionSet.UniformPreshaderData.Num() - PreshaderHeader.OpcodeOffset;

			FMaterialUniformPreshaderField& PreshaderField = MaterialCompilationOutput.UniformExpressionSet.UniformPreshaderFields.AddDefaulted_GetRef();
			PreshaderField.BufferOffset = UniformOffset;
			PreshaderField.Type = UE::Shader::MakeValueType(UE::Shader::EValueComponentType::Bool, NumComponents);
			PreshaderField.ComponentIndex = 0;

			CodeChunk.UniformExpression->UniformOffset = UniformOffset;
			CurrentNumBoolComponents += NumComponents;
		}

		const uint32 UniformOffset = CodeChunk.UniformExpression->UniformOffset / 32u;
		const uint32 NumBoolComponents = CodeChunk.UniformExpression->UniformOffset % 32u;

		const uint32 RegisterIndex = UniformOffset / 4;
		const uint32 RegisterOffset = UniformOffset % 4;
		FormattedCode.Appendf(TEXT("UnpackUniform_%s(asuint(Material.PreshaderBuffer[%u][%u]), %u)"),
			TEXT("bool"),
			RegisterIndex,
			RegisterOffset,
			NumBoolComponents);
	}
	else if (IsFloatNumericType(CodeChunk.Type))
	{
		check(CodeChunk.DerivativeStatus != EDerivativeStatus::Valid);
		const uint32 NumComponents = GetNumComponents(CodeChunk.Type);

		if (CodeChunk.UniformExpression->UniformOffset == INDEX_NONE)
		{
			const uint32 RegisterOffset = UniformPreshaderOffset % 4;
			if (!bIsLWC && RegisterOffset + NumComponents > 4u)
			{
				// If this uniform would span multiple registers, align offset to the next register to avoid this
				UniformPreshaderOffset = Align(UniformPreshaderOffset, 4u);
			}

			const UE::Shader::EValueComponentType ComponentType = bIsLWC ? UE::Shader::EValueComponentType::Double : UE::Shader::EValueComponentType::Float;

			FMaterialUniformPreshaderHeader& Preshader = MaterialCompilationOutput.UniformExpressionSet.UniformPreshaders.AddDefaulted_GetRef();
			Preshader.OpcodeOffset = MaterialCompilationOutput.UniformExpressionSet.UniformPreshaderData.Num();
			CodeChunk.UniformExpression->WriteNumberOpcodes(MaterialCompilationOutput.UniformExpressionSet.UniformPreshaderData);
			Preshader.OpcodeSize = MaterialCompilationOutput.UniformExpressionSet.UniformPreshaderData.Num() - Preshader.OpcodeOffset;
			Preshader.FieldIndex = MaterialCompilationOutput.UniformExpressionSet.UniformPreshaderFields.Num();
			Preshader.NumFields = 1;

			FMaterialUniformPreshaderField& PreshaderField = MaterialCompilationOutput.UniformExpressionSet.UniformPreshaderFields.AddDefaulted_GetRef();
			PreshaderField.BufferOffset = UniformPreshaderOffset;
			PreshaderField.Type = UE::Shader::MakeValueType(ComponentType, NumComponents);
			PreshaderField.ComponentIndex = 0;

			CodeChunk.UniformExpression->UniformOffset = UniformPreshaderOffset;
			UniformPreshaderOffset += bIsLWC ? NumComponents * 2u : NumComponents;
		}

		const uint32 UniformOffset = CodeChunk.UniformExpression->UniformOffset;
		if (bIsLWC)
		{
			if (NumComponents == 1)
			{
				FormattedCode.Append(TEXT("MakeLWCScalar("));
			}
			else
			{
				FormattedCode.Appendf(TEXT("MakeLWCVector%d("), NumComponents);
			}

			WriteMaterialUniformAccess(UE::Shader::EValueComponentType::Float, NumComponents, UniformOffset, FormattedCode); // Tile
			FormattedCode.Append(TEXT(","));
			WriteMaterialUniformAccess(UE::Shader::EValueComponentType::Float, NumComponents, UniformOffset + NumComponents, FormattedCode); // Offset
			FormattedCode.Append(TEXT(")"));
		}
		else
		{
			WriteMaterialUniformAccess(UE::Shader::EValueComponentType::Float, NumComponents, UniformOffset, FormattedCode);
		}
	}
	else if(CodeChunk.Type & MCT_Texture)
	{
		int32 TextureInputIndex = INDEX_NONE;
		const TCHAR* BaseName = TEXT("");
		bool GenerateCode = true;
		switch(CodeChunk.Type)
		{
		case MCT_Texture2D:
			TextureInputIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Standard2D].AddUnique(TextureUniformExpression);
			BaseName = TEXT("Texture2D");
			break;
		case MCT_TextureCube:
			TextureInputIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Cube].AddUnique(TextureUniformExpression);
			BaseName = TEXT("TextureCube");
			break;
		case MCT_Texture2DArray:
			TextureInputIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Array2D].AddUnique(TextureUniformExpression);
			BaseName = TEXT("Texture2DArray");
			break;
		case MCT_TextureCubeArray:
			TextureInputIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::ArrayCube].AddUnique(TextureUniformExpression);
			BaseName = TEXT("TextureCubeArray");
			break;
		case MCT_VolumeTexture:
			TextureInputIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Volume].AddUnique(TextureUniformExpression);
			BaseName = TEXT("VolumeTexture");
			break;
		case MCT_TextureExternal:
			TextureInputIndex = UniformExternalTextureExpressions.AddUnique(ExternalTextureUniformExpression);
			BaseName = TEXT("ExternalTexture");
			break;
		case MCT_TextureVirtual:
			TextureInputIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Virtual].AddUnique(TextureUniformExpression);
			GenerateCode = false;
			break;
		default: UE_LOG(LogMaterial, Fatal,TEXT("Unrecognized texture material value type: %u"),(int32)CodeChunk.Type);
		};
		if(GenerateCode)
		{
			FormattedCode.Appendf(TEXT("Material.%s_%u"), BaseName, TextureInputIndex);
		}
	}
	else
	{
		UE_LOG(LogMaterial, Fatal,TEXT("User input of unknown type: %s"),DescribeType(CodeChunk.Type));
	}


	return AddInlinedCodeChunkZeroDeriv(CodeChunk.Type,FormattedCode.ToString());
}

FString FHLSLMaterialTranslator::CoerceValue(const FString& Code, EMaterialValueType SourceType, EMaterialValueType DestType)
{
	EMaterialCastFlags CastFlags = EMaterialCastFlags::ReplicateScalar;
	if (DestType == MCT_Float || DestType == MCT_Float1 || DestType == MCT_LWCScalar)
	{
		// CoerceValue allows truncating to scalar types only
		CastFlags |= EMaterialCastFlags::AllowTruncate;
	}
	return CastValue(Code, SourceType, DestType, CastFlags);
}

// CoerceParameter
FString FHLSLMaterialTranslator::CoerceParameter(int32 Index,EMaterialValueType DestType)
{
	return CoerceValue(GetParameterCode(Index), GetParameterType(Index), DestType);
}

// GetParameterType
EMaterialValueType FHLSLMaterialTranslator::GetParameterType(int32 Index) const
{
	check(Index >= 0 && Index < CurrentScopeChunks->Num());
	return (*CurrentScopeChunks)[Index].Type;
}

// GetParameterUniformExpression
FMaterialUniformExpression* FHLSLMaterialTranslator::GetParameterUniformExpression(int32 Index) const
{
	check(Index >= 0 && Index < CurrentScopeChunks->Num());

	const FShaderCodeChunk& Chunk = (*CurrentScopeChunks)[Index];

	return Chunk.UniformExpression;
}

bool FHLSLMaterialTranslator::GetTextureForExpression(int32 Index, int32& OutTextureIndex, EMaterialSamplerType& OutSamplerType, TOptional<FName>& OutParameterName) const
{
	check(Index >= 0 && Index < CurrentScopeChunks->Num());
	const FShaderCodeChunk& Chunk = (*CurrentScopeChunks)[Index];
	const EMaterialValueType TexInputType = Chunk.Type;
	if (!(TexInputType & MCT_Texture))
	{
		return false;
	}

	// If 'InputExpression' is connected, we use need to find the texture object that was passed in
	// In this case, the texture/sampler assigned on this expression node are not used
	FMaterialUniformExpression* TextureUniformBase = Chunk.UniformExpression;
	checkf(TextureUniformBase, TEXT("TexInputType is %d, but missing FMaterialUniformExpression"), TexInputType);

	if (FMaterialUniformExpressionTexture* TextureUniform = TextureUniformBase->GetTextureUniformExpression())
	{
		OutSamplerType = TextureUniform->GetSamplerType();
		OutTextureIndex = TextureUniform->GetTextureIndex();
		if (FMaterialUniformExpressionTextureParameter* TextureParameterUniform = TextureUniform->GetTextureParameterUniformExpression())
		{
			OutParameterName = TextureParameterUniform->GetParameterName();
		}
	}
	else if (FMaterialUniformExpressionExternalTexture* ExternalTextureUniform = TextureUniformBase->GetExternalTextureUniformExpression())
	{
		OutTextureIndex = ExternalTextureUniform->GetSourceTextureIndex();
		OutSamplerType = SAMPLERTYPE_External;
		if (FMaterialUniformExpressionExternalTextureParameter* ExternalTextureParameterUniform = ExternalTextureUniform->GetExternalTextureParameterUniformExpression())
		{
			OutParameterName = ExternalTextureParameterUniform->GetParameterName();
		}
	}

	return true;
}

// GetArithmeticResultType
EMaterialValueType FHLSLMaterialTranslator::GetArithmeticResultType(EMaterialValueType TypeA, EMaterialValueType TypeB)
{
	if (!IsNumericType(TypeA) || !IsNumericType(TypeB))
	{
		Errorf(TEXT("Attempting to perform arithmetic on non-numeric types: %s %s"), DescribeType(TypeA),DescribeType(TypeB));
		return MCT_Unknown;
	}

	if(TypeA == TypeB)
	{
		return TypeA;
	}
	else if (IsLWCType(TypeA) || IsLWCType(TypeB))
	{
		const EMaterialValueType LWCTypeA = MakeLWCType(TypeA);
		const EMaterialValueType LWCTypeB = MakeLWCType(TypeB);
		if (LWCTypeA == LWCTypeB)
		{
			return LWCTypeA;
		}
		else if (LWCTypeA == MCT_LWCScalar && IsFloatNumericType(LWCTypeB))
		{
			return LWCTypeB;
		}
		else if (LWCTypeB == MCT_LWCScalar && IsFloatNumericType(LWCTypeA))
		{
			return LWCTypeA;
		}
	}
	else if(TypeA & TypeB)
	{
		if(TypeA == MCT_Float)
		{
			return TypeB;
		}
		else
		{
			check(TypeB == MCT_Float);
			return TypeA;
		}
	}

	Errorf(TEXT("Arithmetic between types %s and %s are undefined"), DescribeType(TypeA), DescribeType(TypeB));
	return MCT_Unknown;
}

EMaterialValueType FHLSLMaterialTranslator::GetArithmeticResultType(int32 A,int32 B)
{
	check(A >= 0 && A < CurrentScopeChunks->Num());
	check(B >= 0 && B < CurrentScopeChunks->Num());

	EMaterialValueType	TypeA = (*CurrentScopeChunks)[A].Type,
		TypeB = (*CurrentScopeChunks)[B].Type;

	return GetArithmeticResultType(TypeA,TypeB);
}

// FMaterialCompiler interface.

/** 
	* Sets the current material property being compiled.  
	* This affects the internal state of the compiler and the results of all functions except GetFixedParameterCode.
	* @param OverrideShaderFrequency SF_NumFrequencies to not override
	*/
void FHLSLMaterialTranslator::SetMaterialProperty(EMaterialProperty InProperty, EShaderFrequency OverrideShaderFrequency, bool bUsePreviousFrameTime)
{
	MaterialProperty = InProperty;
	SetBaseMaterialAttribute(FMaterialAttributeDefinitionMap::GetID(InProperty));

	if(OverrideShaderFrequency != SF_NumFrequencies)
	{
		ShaderFrequency = OverrideShaderFrequency;
	}
	else
	{
		ShaderFrequency = FMaterialAttributeDefinitionMap::GetShaderFrequency(InProperty);
	}

	bCompilingPreviousFrame = bUsePreviousFrameTime;
	AssignShaderFrequencyScope(ShaderFrequency);
}

void FHLSLMaterialTranslator::PushMaterialAttribute(const FGuid& InAttributeID)
{
	MaterialAttributesStack.Push(InAttributeID);
}

FGuid FHLSLMaterialTranslator::PopMaterialAttribute()
{
	return MaterialAttributesStack.Pop(false);
}

const FGuid FHLSLMaterialTranslator::GetMaterialAttribute()
{
	checkf(MaterialAttributesStack.Num() > 0, TEXT("Tried to query empty material attributes stack."));
	return MaterialAttributesStack.Top();
}

void FHLSLMaterialTranslator::SetBaseMaterialAttribute(const FGuid& InAttributeID)
{
	// This is atypical behavior but is done to allow cleaner code and preserve existing paths.
	// A base property is kept on the stack and updated by SetMaterialProperty(), the stack is only utilized during translation
	checkf(MaterialAttributesStack.Num() == 1, TEXT("Tried to set non-base attribute on stack."));
	MaterialAttributesStack.Top() = InAttributeID;
}

void FHLSLMaterialTranslator::PushParameterOwner(const FMaterialParameterInfo& InOwnerInfo)
{
	ParameterOwnerStack.Push(InOwnerInfo);
}

FMaterialParameterInfo FHLSLMaterialTranslator::PopParameterOwner()
{
	return ParameterOwnerStack.Pop(false);
}

EShaderFrequency FHLSLMaterialTranslator::GetCurrentShaderFrequency() const
{
	return ShaderFrequency;
}

bool FHLSLMaterialTranslator::IsTangentSpaceNormal() const
{
	check(Material);
	return Material->GetMaterialDomain() == MD_DeferredDecal || Material->IsTangentSpaceNormal();
}

FMaterialShadingModelField FHLSLMaterialTranslator::GetMaterialShadingModels() const
{
	check(Material);
	return Material->GetShadingModels();
}

FMaterialShadingModelField FHLSLMaterialTranslator::GetCompiledShadingModels() const
{
	check(Material);

	// If the material gets its shading model from material expressions and we have compiled one or more shading model expressions already, 
	// then use that shading model field instead. It's the most optimal set of shading models
	if (Material->IsShadingModelFromMaterialExpression() && ShadingModelsFromCompilation.IsValid())
	{
		return ShadingModelsFromCompilation;
	}

	return Material->GetShadingModels();
}

int32 FHLSLMaterialTranslator::Error(const TCHAR* Text)
{
	// Optionally append errors into proxy arrays which allow pre-translation stages to selectively include errors later
	bool bUsingErrorProxy = (CompileErrorsSink && CompileErrorExpressionsSink);	
	TArray<FString>& CompileErrors = bUsingErrorProxy ? *CompileErrorsSink : Material->CompileErrors;
	TArray<UMaterialExpression*>& ErrorExpressions = bUsingErrorProxy ? *CompileErrorExpressionsSink : Material->ErrorExpressions;

	FString ErrorString;
	UMaterialExpression* ExpressionToError = nullptr;

	check(ShaderFrequency < SF_NumFrequencies);
	auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
	if (CurrentFunctionStack.Num() > 1)
	{
		// If we are inside a function, add that to the error message.  
		// Only add the function call node to ErrorExpressions, since we can't add a reference to the expressions inside the function as they are private objects.
		// Add the first function node on the stack because that's the one visible in the material being compiled, the rest are all nested functions.
		UMaterialExpressionMaterialFunctionCall* ErrorFunction = CurrentFunctionStack[1]->FunctionCall;
		check(ErrorFunction);
		ExpressionToError = ErrorFunction;			
		ErrorString = FString(TEXT("Function ")) + ErrorFunction->MaterialFunction->GetName() + TEXT(": ");
	}

	if (CurrentFunctionStack.Last()->ExpressionStack.Num() > 0)
	{
		UMaterialExpression* ErrorExpression = CurrentFunctionStack.Last()->ExpressionStack.Last().Expression;
		check(ErrorExpression);

		if (ErrorExpression->GetClass() != UMaterialExpressionMaterialFunctionCall::StaticClass()
			&& ErrorExpression->GetClass() != UMaterialExpressionFunctionInput::StaticClass()
			&& ErrorExpression->GetClass() != UMaterialExpressionFunctionOutput::StaticClass())
		{
			// Add the expression currently being compiled to ErrorExpressions so we can draw it differently
			ExpressionToError = ErrorExpression;

			const int32 ChopCount = FCString::Strlen(TEXT("MaterialExpression"));
			const FString ErrorClassName = ErrorExpression->GetClass()->GetName();

			// Add the node type to the error message
			ErrorString += FString(TEXT("(Node ")) + ErrorClassName.Right(ErrorClassName.Len() - ChopCount) + TEXT(") ");
		}
	}
			
	ErrorString += Text;

	if (!bUsingErrorProxy)
	{
		// Standard error handling, immediately append one-off errors and signal failure
		bSuccess = false;
		if (ExpressionToError)
		{
			ExpressionToError->LastErrorText = Text;
			for (int32 i = 0; i < ErrorExpressions.Num(); i++)
			{
				if(ErrorExpressions[i] == ExpressionToError && CompileErrors[i] == ErrorString)
				{
					return INDEX_NONE;
				}
			}
		}
	}

	// When a proxy is intercepting errors, ignore the failure and match arrays to allow later error type selection
	CompileErrors.Add(ErrorString);
	ErrorExpressions.Add(ExpressionToError);

	return INDEX_NONE;
}

void FHLSLMaterialTranslator::AppendExpressionError(UMaterialExpression* Expression, const TCHAR* Text)
{
	if (Expression && Text)
	{
		FString ErrorText(Text);

		Material->ErrorExpressions.Add(Expression);
		Expression->LastErrorText = ErrorText;
		Material->CompileErrors.Add(ErrorText);
	}
}

int32 FHLSLMaterialTranslator::CallExpression(FMaterialExpressionKey ExpressionKey,FMaterialCompiler* Compiler)
{
	// For any translated result not relying on material attributes, we can discard the attribute ID from the key
	// to allow result sharing. In cases where we detect an expression loop we must err on the side of caution
	if (ExpressionKey.Expression && !ExpressionKey.Expression->ContainsInputLoop() && !ExpressionKey.Expression->IsResultMaterialAttributes(ExpressionKey.OutputIndex))
	{
		ExpressionKey.MaterialAttributeID = FGuid(0, 0, 0, 0);
	}

	// Some expressions can discard output indices and share compiles with a swizzle/mask
	if (ExpressionKey.Expression && ExpressionKey.Expression->CanIgnoreOutputIndex())
	{
		ExpressionKey.OutputIndex = INDEX_NONE;
	}

	// Strata BSDF expression should not be de-duplicated using expression output hash. 
	// This is automatically handled via the compiler StrataTreeStack.
	// It means that a node can be blended at multiple point of the graph (allowing acyclic graph instead of tree, e.g. a Slab can be used into multiple input).
	// It is worth noting that only strata BSDF can be duplicated today according to StrataTreeStack.
	const bool bExpressionIsStrataBSDF = ExpressionKey.Expression && ExpressionKey.Expression->IsA<UMaterialExpressionStrataSlabBSDF>();

	// Check if this expression has already been translated.
	check(ShaderFrequency < SF_NumFrequencies);
	auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
	FMaterialFunctionCompileState* CurrentFunctionState = CurrentFunctionStack.Last();

	static bool sDebugCacheDuplicateCode = true;
	int32* ExistingCodeIndex = sDebugCacheDuplicateCode && !bExpressionIsStrataBSDF ? CurrentFunctionState->ExpressionCodeMap.Find(ExpressionKey) : nullptr;
	int32 Result = INDEX_NONE;
	if (ExistingCodeIndex)
	{
		Result = *ExistingCodeIndex;
		AddCodeChunkToCurrentScope(Result);
	}
	else
	{
		// Disallow reentrance.
		if (CurrentFunctionState->ExpressionStack.Find(ExpressionKey) != INDEX_NONE)
		{
			return Error(TEXT("Reentrant expression"));
		}

		// The first time this expression is called, translate it.
		CurrentFunctionState->ExpressionStack.Add(ExpressionKey);
		const int32 FunctionDepth = CurrentFunctionStack.Num();
			
		// Attempt to share function states between function calls
		UMaterialExpressionMaterialFunctionCall* FunctionCall = Cast<UMaterialExpressionMaterialFunctionCall>(ExpressionKey.Expression);
		if (FunctionCall)
		{
			FMaterialExpressionKey ReuseCompileStateExpressionKey = ExpressionKey;
			ReuseCompileStateExpressionKey.OutputIndex = INDEX_NONE; // Discard the output so we can share the stack internals
			ReuseCompileStateExpressionKey.MaterialAttributeID = FGuid(0, 0, 0, 0); //Discard the Material Attribute ID so we can share the stack internals

			FMaterialFunctionCompileState* SharedFunctionState = CurrentFunctionState->FindOrAddSharedFunctionState(ReuseCompileStateExpressionKey, FunctionCall);
			FunctionCall->SetSharedCompileState(SharedFunctionState);
		}

		ReferencedCodeChunks.Reset();

		Result = ExpressionKey.Expression->Compile(Compiler, ExpressionKey.OutputIndex);

		// Restore state
		if (FunctionCall)
		{
			FunctionCall->SetSharedCompileState(nullptr);
		}

		FMaterialExpressionKey PoppedExpressionKey = CurrentFunctionState->ExpressionStack.Pop();

		// Verify state integrity
		check(PoppedExpressionKey == ExpressionKey);
		check(FunctionDepth == CurrentFunctionStack.Num());

		// Cache the translation.
		CurrentFunctionStack.Last()->ExpressionCodeMap.Add(ExpressionKey,Result);

		if (Result != INDEX_NONE)
		{
			FShaderCodeChunk& ResultChunk = (*CurrentScopeChunks)[Result];
			ResultChunk.ReferencedCodeChunks = MoveTemp(ReferencedCodeChunks);
		}
		ReferencedCodeChunks.Reset();
	}

		return Result;
	}

int32 FHLSLMaterialTranslator::CallExpressionExec(UMaterialExpression* Expression)
{
	check(ShaderFrequency < SF_NumFrequencies);
	auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
	FMaterialFunctionCompileState* CurrentFunctionState = CurrentFunctionStack.Last();

	int32 Result = INDEX_NONE;
	/*int32* ExistingCodeIndex = CurrentFunctionState->ExecExpressionCodeMap.Find(Expression);
	if (ExistingCodeIndex)
	{
		Result = *ExistingCodeIndex;
		FShaderCodeChunk& Chunk = (*CurrentScopeChunks)[Result];
		const int32 CurrentScopeIndex = ScopeStack.Last();
		const FShaderCodeChunk& CurrentScope = (*CurrentScopeChunks)[CurrentScopeIndex];
		const FShaderCodeChunk& PrevScope = (*CurrentScopeChunks)[Chunk.UsedScopeIndex];
		if (CurrentScope.UsedScopeIndex == PrevScope.UsedScopeIndex)
		{
			AddCodeChunkToScope(Result, CurrentScope.UsedScopeIndex);
		}
		else
		{
			return Errorf(TEXT("Invalid scopes"));
		}
	}
	else*/
	{
		ReferencedCodeChunks.Reset();
		Result = Expression->Compile(this, UMaterialExpression::CompileExecutionOutputIndex);
		CurrentFunctionState->ExecExpressionCodeMap.Add(Expression, Result);

		if (Result != INDEX_NONE)
		{
			FShaderCodeChunk& ResultChunk = (*CurrentScopeChunks)[Result];
			ResultChunk.ReferencedCodeChunks = MoveTemp(ReferencedCodeChunks);
		}
		ReferencedCodeChunks.Reset();
	}
	return Result;
}

void FHLSLMaterialTranslator::AddCodeChunkToCurrentScope(int32 ChunkIndex)
{
	if (ChunkIndex != INDEX_NONE && ScopeStack.Num() > 0)
	{
		const int32 CurrentScopeIndex = ScopeStack.Last();
		AddCodeChunkToScope(ChunkIndex, CurrentScopeIndex);
	}
}

void FHLSLMaterialTranslator::AddCodeChunkToScope(int32 ChunkIndex, int32 ScopeIndex)
{
	if (ChunkIndex != INDEX_NONE)
	{
		FShaderCodeChunk& CurrentScope = (*CurrentScopeChunks)[ScopeIndex];

		FShaderCodeChunk& Chunk = (*CurrentScopeChunks)[ChunkIndex];
		if (Chunk.DeclaredScopeIndex == INDEX_NONE)
		{
			check(Chunk.UsedScopeIndex == INDEX_NONE);
			Chunk.DeclaredScopeIndex = ScopeIndex;
			Chunk.UsedScopeIndex = ScopeIndex;
			Chunk.ScopeLevel = CurrentScope.ScopeLevel + 1;
		}
		else if (Chunk.UsedScopeIndex != ScopeIndex)
		{
			// Find the most derived scope that's shared by the current scope, and the scope this code was previously referenced from
			int32 ScopeIndex0 = ScopeIndex;
			int32 ScopeIndex1 = Chunk.UsedScopeIndex;
			while (ScopeIndex0 != ScopeIndex1)
			{
				const FShaderCodeChunk& Scope0 = (*CurrentScopeChunks)[ScopeIndex0];
				const FShaderCodeChunk& Scope1 = (*CurrentScopeChunks)[ScopeIndex1];
				if (Scope0.ScopeLevel > Scope1.ScopeLevel)
				{
					check(Scope0.UsedScopeIndex != INDEX_NONE);
					ScopeIndex0 = Scope0.UsedScopeIndex;
				}
				else
				{
					check(Scope1.UsedScopeIndex != INDEX_NONE);
					ScopeIndex1 = Scope1.UsedScopeIndex;
				}
			}

			const FShaderCodeChunk& Scope = (*CurrentScopeChunks)[ScopeIndex0];
			Chunk.UsedScopeIndex = ScopeIndex0;
			Chunk.ScopeLevel = Scope.ScopeLevel + 1;
		}
	}
}

EMaterialValueType FHLSLMaterialTranslator::GetType(int32 Code)
{
	if(Code != INDEX_NONE)
	{
		return GetParameterType(Code);
	}
	else
	{
		return MCT_Unknown;
	}
}

EMaterialQualityLevel::Type FHLSLMaterialTranslator::GetQualityLevel()
{
	return QualityLevel;
}

ERHIFeatureLevel::Type FHLSLMaterialTranslator::GetFeatureLevel()
{
	return FeatureLevel;
}

EShaderPlatform FHLSLMaterialTranslator::GetShaderPlatform()
{
	return Platform;
}

const ITargetPlatform* FHLSLMaterialTranslator::GetTargetPlatform() const
{
	return TargetPlatform;
}

bool FHLSLMaterialTranslator::IsMaterialPropertyUsed(EMaterialProperty Property, int32 PropertyChunkIndex) const
{
	if (PropertyChunkIndex == -1)
	{
		return false;
	}
	else
	{
		FVector4f DefaultValue = FMaterialAttributeDefinitionMap::GetDefaultValue(Property);
		EMaterialValueType ValueType = FMaterialAttributeDefinitionMap::GetValueType(Property);
		int32 ComponentCount = GetNumComponents(ValueType);

		return IsMaterialPropertyUsed(Property, PropertyChunkIndex, FLinearColor(DefaultValue), ComponentCount);
	}
}

/** 
	* Casts the passed in code to DestType, or generates a compile error if the cast is not valid. 
	* This will truncate a type (float4 -> float3) but not add components (float2 -> float3), however a float1 can be cast to any float type by replication. 
	*/
int32 FHLSLMaterialTranslator::ValidCast(int32 Code, EMaterialValueType DestType)
{
	if(Code == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const EMaterialCastFlags Flags = EMaterialCastFlags::ValidCast;
	const EMaterialValueType SourceType = GetParameterType(Code);
	int32 CompiledResult = INDEX_NONE;

	if (SourceType & DestType)
	{
		CompiledResult = Code;
	}
	else if(GetParameterUniformExpression(Code) && !GetParameterUniformExpression(Code)->IsConstant())
	{
		if ((SourceType & MCT_TextureVirtual) && (DestType & MCT_Texture2D))
		{
			return Code;
		}
		else
		{
			return ValidCast(AccessUniformExpression(Code), DestType);
		}
	}
	else if(IsFloatNumericType(SourceType) && IsFloatNumericType(DestType))
	{
		const FDerivInfo CodeDerivInfo = GetDerivInfo(Code);

		FString FiniteCode = CastValue(GetParameterCode(Code), SourceType, DestType, Flags);
		if (IsAnalyticDerivEnabled() && IsDerivativeValid(CodeDerivInfo.DerivativeStatus))
		{
			if (CodeDerivInfo.DerivativeStatus == EDerivativeStatus::Valid)
			{
				FString DerivString = *GetParameterCodeDeriv(Code, CompiledPDV_Analytic);
				FString DDXCode = CastValue(DerivString + TEXT(".Ddx"), MakeNonLWCType(SourceType), MakeNonLWCType(DestType), Flags);
				FString DDYCode = CastValue(DerivString + TEXT(".Ddy"), MakeNonLWCType(SourceType), MakeNonLWCType(DestType), Flags);
				FString AnalyticCode = DerivativeAutogen.ConstructDeriv(FiniteCode, DDXCode, DDYCode, GetDerivType(DestType));
				return AddCodeChunkInnerDeriv(*FiniteCode, *AnalyticCode, DestType, false, EDerivativeStatus::Valid);
			}
			else
			{
				return AddInlinedCodeChunkZeroDeriv(DestType, *FiniteCode);
			}
		}
		else
		{
			return AddInlinedCodeChunk(DestType, *FiniteCode);
		}
	}
	else
	{
		//We can feed any type into a material attributes socket as we're really just passing them through.
		if( DestType == MCT_MaterialAttributes )
		{
			CompiledResult = Code;
		}
		else
		{
			CompiledResult = Errorf(TEXT("Cannot cast from %s to %s."), DescribeType(SourceType), DescribeType(DestType));
		}
	}

	return CompiledResult;
}

int32 FHLSLMaterialTranslator::ForceCast(int32 Code, EMaterialValueType DestType, uint32 ForceCastFlags)
{
	if(Code == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(Code) && !GetParameterUniformExpression(Code)->IsConstant())
	{
		return ForceCast(AccessUniformExpression(Code),DestType,ForceCastFlags);
	}

	const EMaterialValueType SourceType = GetParameterType(Code);
	const bool bExactMatch = (ForceCastFlags & MFCF_ExactMatch) ? true : false;

	if (bExactMatch ? (SourceType == DestType) : (SourceType & DestType))
	{
		return Code;
	}
	else if (IsFloatNumericType(SourceType) && IsFloatNumericType(DestType))
	{
		const FDerivInfo CodeDerivInfo = GetDerivInfo(Code);

		EMaterialCastFlags CastFlags = EMaterialCastFlags::AllowTruncate | EMaterialCastFlags::AllowAppendZeroes;
		if ((ForceCastFlags & MFCF_ReplicateValue) || !bExactMatch)
		{
			// Replicate scalar if requested, or if we don't require an exact match (this can happen when force-casting to/from LWC
			// The only way we *don't* replicate scalar is requesting an exact match without the ReplicateValue flag
			// TODO - My guess is that case probably isn't relevant, and we should just always replicate scalar on cast, but trying to preserve behavior for now
			CastFlags |= EMaterialCastFlags::ReplicateScalar;
		}

		FString FiniteCode = CastValue(GetParameterCode(Code), SourceType, DestType, CastFlags);
		if (IsAnalyticDerivEnabled() && IsDerivativeValid(CodeDerivInfo.DerivativeStatus))
		{
			if (CodeDerivInfo.DerivativeStatus == EDerivativeStatus::Valid)
			{
				FString DerivString = *GetParameterCodeDeriv(Code, CompiledPDV_Analytic);
				FString DDXCode = CastValue(DerivString + TEXT(".Ddx"), MakeNonLWCType(SourceType), MakeNonLWCType(DestType), CastFlags);
				FString DDYCode = CastValue(DerivString + TEXT(".Ddy"), MakeNonLWCType(SourceType), MakeNonLWCType(DestType), CastFlags);
				FString AnalyticCode = DerivativeAutogen.ConstructDeriv(FiniteCode, DDXCode, DDYCode, GetDerivType(DestType));
				return AddCodeChunkInnerDeriv(*FiniteCode, *AnalyticCode, DestType, false, EDerivativeStatus::Valid);
			}
			else
			{
				return AddInlinedCodeChunkZeroDeriv(DestType, *FiniteCode);
			}
		}
		else
		{
			return AddInlinedCodeChunk(DestType, *FiniteCode);
		}
	}
	else if ((SourceType & MCT_TextureVirtual) && (DestType & MCT_Texture2D))
	{
		return Code;
	}
	else
	{
		return Errorf(TEXT("Cannot force a cast between non-numeric types."));
	}
}

int32 FHLSLMaterialTranslator::CastShadingModelToFloat(int32 Code)
{
	const EMaterialValueType SourceType = GetParameterType(Code);
	if (SourceType != MCT_ShadingModel)
	{
		return Errorf(TEXT("Operation only supported for shading model"));
	}

	return AddCodeChunk(MCT_Float1, TEXT("((%s)(%s))"), HLSLTypeString(MCT_Float1), *GetParameterCode(Code));
}

int32 FHLSLMaterialTranslator::TruncateLWC(int32 Code)
{
	const int32 LWCTruncateMode = GetLWCTruncateMode();
	const bool bAllowLWCTruncate = LWCTruncateMode == 1 || LWCTruncateMode == 2;

	if (!bAllowLWCTruncate)
	{
		return Code;
	}

	const EMaterialValueType TruncateType = GetParameterType(Code);
	if (IsLWCType(TruncateType))
	{
		EMaterialValueType TruncatedType = MakeNonLWCType(TruncateType);
		
		if (TruncateType == MCT_LWCScalar)
		{
			TruncatedType = MCT_Float; // Remap from MCT_Float1 to MCT_Float to support replication
		}

		return ValidCast(Code, TruncatedType);
	}

	return Code;
}

int32 FHLSLMaterialTranslator::CastToNonLWCIfDisabled(int32 Code)
{
	int32 Result = Code;
	if (!GLWCEnabled)
	{
		const EMaterialValueType Type = GetParameterType(Code);
		if (IsLWCType(Type))
		{
			Result = ValidCast(Code, MakeNonLWCType(Type));
		}
	}
	return Result;
}

FString FHLSLMaterialTranslator::CastValue(const FString& Code, EMaterialValueType SourceType, EMaterialValueType DestType, EMaterialCastFlags Flags)
{
	const bool bAllowTruncate = EnumHasAnyFlags(Flags, EMaterialCastFlags::AllowTruncate);
	const bool bAllowAppendZeroes = EnumHasAnyFlags(Flags, EMaterialCastFlags::AllowAppendZeroes);
	bool bReplicateScalar = EnumHasAnyFlags(Flags, EMaterialCastFlags::ReplicateScalar);

	if (SourceType == DestType)
	{
		return Code;
	}
	else if (IsFloatNumericType(SourceType) && IsFloatNumericType(DestType))
	{
		const uint32 NumSourceComponents = GetNumComponents(SourceType);
		const uint32 NumDestComponents = GetNumComponents(DestType);
		if (NumSourceComponents != 1)
		{
			bReplicateScalar = false;
		}
		if (!bReplicateScalar && !bAllowAppendZeroes && NumDestComponents > NumSourceComponents)
		{
			Errorf(TEXT("Cannot cast from smaller type %s to larger type %s."), DescribeType(SourceType), DescribeType(DestType));
			return FString();
		}
		if (!bReplicateScalar && !bAllowTruncate && NumDestComponents < NumSourceComponents)
		{
			Errorf(TEXT("Cannot cast from larger type %s to smaller type %s."), DescribeType(SourceType), DescribeType(DestType));
			return FString();
		}

		const bool bIsLWC = IsLWCType(DestType);
		if (bIsLWC != IsLWCType(SourceType))
		{
			if (bIsLWC)
			{
				// float->LWC
				return FString::Printf(TEXT("LWCPromote(%s)"), *CastValue(Code, SourceType, MakeNonLWCType(DestType), Flags));
			}
			else
			{
				//LWC->float
				return CastValue(FString::Printf(TEXT("LWCToFloat(%s)"), *Code), MakeNonLWCType(SourceType), DestType, Flags);
			}
		}

		FString Result;
		uint32 NumComponents = 0u;
		bool bNeedClosingParen = false;
		if (bIsLWC)
		{
			Result = TEXT("MakeLWCVector(");
			bNeedClosingParen = true;
		}
		else
		{
			if (NumSourceComponents == NumDestComponents)
			{
				NumComponents = NumDestComponents;
				Result += Code;
			}
			else if (bReplicateScalar)
			{
				NumComponents = NumDestComponents;
				// Cast the scalar to the correct type, HLSL language will replicate the scalar when performing this cast
				Result += FString::Printf(TEXT("((%s)%s)"), HLSLTypeString(DestType), *Code);
			}
			else
			{
				NumComponents = FMath::Min(NumSourceComponents, NumDestComponents);
				if (NumComponents < NumDestComponents)
				{
					Result = FString(HLSLTypeString(DestType)) + TEXT("(");
					bNeedClosingParen = true;
				}
				if (NumComponents == NumSourceComponents)
				{
					// If we're taking all the components from the source, can avoid adding a swizzle
					Result += Code;
				}
				else
				{
					static const TCHAR* Mask[] = { TEXT("<ERROR>"), TEXT("x"), TEXT("xy"), TEXT("xyz"), TEXT("xyzw") };
					check(NumComponents <= 4);
					Result += FString::Printf(TEXT("%s.%s"), *Code, Mask[NumComponents]);
				}
			}
		}

		if (bNeedClosingParen)
		{
			for (uint32 ComponentIndex = NumComponents; ComponentIndex < NumDestComponents; ++ComponentIndex)
			{
				if (ComponentIndex > 0u)
				{
					Result += TEXT(",");
				}
				if (bIsLWC)
				{
					if (!bReplicateScalar && ComponentIndex >= NumSourceComponents)
					{
						check(bAllowAppendZeroes);
						Result += TEXT("LWCPromote(0.0f)");
					}
					else
					{
						Result += FString::Printf(TEXT("LWCGetComponent(%s, %d)"), *Code, bReplicateScalar ? 0 : ComponentIndex);
					}
				}
				else
				{
					// Non-LWC case should only be zero-filling here, other cases should have already been handled
					check(bAllowAppendZeroes);
					check(!bReplicateScalar);
					check(ComponentIndex >= NumSourceComponents);
					Result += TEXT("0.0f");
				}
			}
			NumComponents = NumDestComponents;
			Result += TEXT(")");
		}
		check(NumComponents == NumDestComponents);
		return Result;
	}
	else
	{
		Errorf(TEXT("Cannot cast between non-numeric types %s to %s."), DescribeType(SourceType), DescribeType(DestType));
		return FString();
	}
}

/** Pushes a function onto the compiler's function stack, which indicates that compilation is entering a function. */
void FHLSLMaterialTranslator::PushFunction(FMaterialFunctionCompileState* FunctionState)
{
	check(ShaderFrequency < SF_NumFrequencies);
	auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
	CurrentFunctionStack.Push(FunctionState);
}	

/** Pops a function from the compiler's function stack, which indicates that compilation is leaving a function. */
FMaterialFunctionCompileState* FHLSLMaterialTranslator::PopFunction()
{
	check(ShaderFrequency < SF_NumFrequencies);
	auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
	return CurrentFunctionStack.Pop();
}

int32 FHLSLMaterialTranslator::GetCurrentFunctionStackDepth()
{
	check(ShaderFrequency < SF_NumFrequencies);
	auto& CurrentFunctionStack = FunctionStacks[ShaderFrequency];
	return CurrentFunctionStack.Num();
}

int32 FHLSLMaterialTranslator::AccessCollectionParameter(UMaterialParameterCollection* ParameterCollection, int32 ParameterIndex, int32 ComponentIndex)
{
	if (!ParameterCollection || ParameterIndex == -1)
	{
		return INDEX_NONE;
	}

	int32 CollectionIndex = ParameterCollections.Find(ParameterCollection);

	if (CollectionIndex == INDEX_NONE)
	{
		if (ParameterCollections.Num() >= MaxNumParameterCollectionsPerMaterial)
		{
			return Error(TEXT("Material references too many MaterialParameterCollections!  A material may only reference 2 different collections."));
		}

		ParameterCollections.Add(ParameterCollection);
		CollectionIndex = ParameterCollections.Num() - 1;
	}

	int32 VectorChunk = AddCodeChunkZeroDeriv(MCT_Float4,TEXT("MaterialCollection%u.Vectors[%u]"),CollectionIndex,ParameterIndex);

	return ComponentMask(VectorChunk, 
		ComponentIndex == -1 ? true : ComponentIndex % 4 == 0,
		ComponentIndex == -1 ? true : ComponentIndex % 4 == 1,
		ComponentIndex == -1 ? true : ComponentIndex % 4 == 2,
		ComponentIndex == -1 ? true : ComponentIndex % 4 == 3);
}

bool FHLSLMaterialTranslator::GetParameterOverrideValueForCurrentFunction(EMaterialParameterType ParameterType, FName ParameterName, FMaterialParameterMetadata& OutResult) const
{
	const TArray<FMaterialFunctionCompileState*>& FunctionStack = FunctionStacks[ShaderFrequency];

	// Give every function in the callstack on opportunity to override the parameter value
	// Parameters in outer functions take priority
	// For example, if a layer instance calls a function instance that includes an overriden parameter, we want to use the value from the layer instance rather than the function instance
	bool bResult = false;
	for (const FMaterialFunctionCompileState* FunctionState : FunctionStack)
	{
		const UMaterialFunctionInterface* CurrentFunction = (FunctionState && FunctionState->FunctionCall) ? FunctionState->FunctionCall->MaterialFunction : nullptr;
		if (CurrentFunction)
		{
			if (CurrentFunction->GetParameterOverrideValue(ParameterType, ParameterName, OutResult))
			{
				bResult = true;
				break;
			}
		}
	}

	return bResult;
}

int32 FHLSLMaterialTranslator::NumericParameter(EMaterialParameterType ParameterType, FName ParameterName, const UE::Shader::FValue& InDefaultValue)
{
	const UE::Shader::EValueType ValueType = GetShaderValueType(ParameterType);
	check(InDefaultValue.GetType() == ValueType);
	UE::Shader::FValue DefaultValue(InDefaultValue);

	// If we're compiling a function, give the function a chance to override the default parameter value
	FMaterialParameterMetadata Meta;
	if (GetParameterOverrideValueForCurrentFunction(ParameterType, ParameterName, Meta))
	{
		DefaultValue = Meta.Value.AsShaderValue();
		check(DefaultValue.GetType() == ValueType);
	}

	const uint32* PrevDefaultOffset = DefaultUniformValues.Find(DefaultValue);
	uint32 DefaultOffset;
	if (PrevDefaultOffset)
	{
		DefaultOffset = *PrevDefaultOffset;
	}
	else
	{
		DefaultOffset = MaterialCompilationOutput.UniformExpressionSet.AddDefaultParameterValue(DefaultValue);
		DefaultUniformValues.Add(DefaultValue, DefaultOffset);
	}

	FMaterialParameterInfo ParameterInfo = GetParameterAssociationInfo();
	ParameterInfo.Name = ParameterName;

	const int32 ParameterIndex = MaterialCompilationOutput.UniformExpressionSet.FindOrAddNumericParameter(ParameterType, ParameterInfo, DefaultOffset);
	return AddUniformExpression(new FMaterialUniformExpressionNumericParameter(ParameterInfo, ParameterIndex), GetMaterialValueType(ParameterType), TEXT(""));
}

int32 FHLSLMaterialTranslator::Constant(float X)
{
	return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,X,X,X),MCT_Float),MCT_Float,TEXT("%0.8f"),X);
}

int32 FHLSLMaterialTranslator::Constant2(float X,float Y)
{
	return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,0,0),MCT_Float2),MCT_Float2,TEXT("MaterialFloat2(%0.8f,%0.8f)"),X,Y);
}

int32 FHLSLMaterialTranslator::Constant3(float X,float Y,float Z)
{
	return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,Z,0),MCT_Float3),MCT_Float3,TEXT("MaterialFloat3(%0.8f,%0.8f,%0.8f)"),X,Y,Z);
}

int32 FHLSLMaterialTranslator::Constant4(float X,float Y,float Z,float W)
{
	return AddUniformExpression(new FMaterialUniformExpressionConstant(FLinearColor(X,Y,Z,W),MCT_Float4),MCT_Float4,TEXT("MaterialFloat4(%0.8f,%0.8f,%0.8f,%0.8f)"),X,Y,Z,W);
}

int32 FHLSLMaterialTranslator::GenericConstant(const UE::Shader::FValue& Value)
{
	TStringBuilder<1024> String;
	Value.ToString(UE::Shader::EValueStringFormat::HLSL, String);
	return AddUniformExpression(new FMaterialUniformExpressionGenericConstant(Value), GetMaterialValueType(Value.GetType()), String.ToString());
}
	
int32 FHLSLMaterialTranslator::ViewProperty(EMaterialExposedViewProperty Property, bool InvProperty)
{
	check(Property < MEVP_MAX);

	// Compile time struct storing all EMaterialExposedViewProperty's enumerations' HLSL compilation specific meta information
	struct EMaterialExposedViewPropertyMeta
	{
		EMaterialExposedViewProperty EnumValue;
		EMaterialValueType Type;
		const TCHAR * PropertyCode;
		const TCHAR * InvPropertyCode;
	};

	static const EMaterialExposedViewPropertyMeta ViewPropertyMetaArray[] = {
		{MEVP_BufferSize, MCT_Float2, TEXT("View.BufferSizeAndInvSize.xy"), TEXT("View.BufferSizeAndInvSize.zw")},
		{MEVP_FieldOfView, MCT_Float2, TEXT("View.<PREV>FieldOfViewWideAngles"), nullptr},
		{MEVP_TanHalfFieldOfView, MCT_Float2, TEXT("Get<PREV>TanHalfFieldOfView()"), TEXT("Get<PREV>CotanHalfFieldOfView()")},
		{MEVP_ViewSize, MCT_Float2, TEXT("View.ViewSizeAndInvSize.xy"), TEXT("View.ViewSizeAndInvSize.zw")},
		{MEVP_WorldSpaceViewPosition, MCT_LWCVector3, TEXT("ResolvedView.<PREV>WorldViewOrigin"), nullptr},
		{MEVP_WorldSpaceCameraPosition, MCT_LWCVector3, TEXT("ResolvedView.<PREV>WorldCameraOrigin"), nullptr},
		{MEVP_ViewportOffset, MCT_Float2, TEXT("View.ViewRectMin.xy"), nullptr},
		{MEVP_TemporalSampleCount, MCT_Float1, TEXT("View.TemporalAAParams.y"), nullptr},
		{MEVP_TemporalSampleIndex, MCT_Float1, TEXT("View.TemporalAAParams.x"), nullptr},
		{MEVP_TemporalSampleOffset, MCT_Float2, TEXT("View.TemporalAAParams.zw"), nullptr},
		{MEVP_RuntimeVirtualTextureOutputLevel, MCT_Float1, TEXT("View.RuntimeVirtualTextureMipLevel.x"), nullptr},
		{MEVP_RuntimeVirtualTextureOutputDerivative, MCT_Float2, TEXT("View.RuntimeVirtualTextureMipLevel.zw"), nullptr},
		{MEVP_PreExposure, MCT_Float1, TEXT("View.PreExposure.x"), TEXT("View.OneOverPreExposure.x")},
		{MEVP_RuntimeVirtualTextureMaxLevel, MCT_Float1, TEXT("View.RuntimeVirtualTextureMipLevel.y"), nullptr},
		{MEVP_ResolutionFraction, MCT_Float1, TEXT("View.ResolutionFractionAndInv.x"), TEXT("View.ResolutionFractionAndInv.y")},
	};
	static_assert((sizeof(ViewPropertyMetaArray) / sizeof(ViewPropertyMetaArray[0])) == MEVP_MAX, "incoherency between EMaterialExposedViewProperty and ViewPropertyMetaArray");

	auto& PropertyMeta = ViewPropertyMetaArray[Property];
	check(Property == PropertyMeta.EnumValue);

	FString Code = PropertyMeta.PropertyCode;

	if (InvProperty && PropertyMeta.InvPropertyCode)
	{
		Code = PropertyMeta.InvPropertyCode;
	}

	// Resolved templated code
	Code.ReplaceInline(TEXT("<PREV>"), bCompilingPreviousFrame ? TEXT("Prev") : TEXT(""));
		
	if (InvProperty && !PropertyMeta.InvPropertyCode)
	{
		// fall back to compute the property's inverse from PropertyCode
		return Div(Constant(1.f), AddCodeChunkZeroDeriv(PropertyMeta.Type, *Code));
	}

	const int32 Result = AddCodeChunkZeroDeriv(PropertyMeta.Type, *Code);
	return CastToNonLWCIfDisabled(Result);
}


int32 FHLSLMaterialTranslator::IsOrthographic()
{
	return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("((View.ViewToClip[3][3] < 1.0f) ? 0.0f : 1.0f)"));
}


int32 FHLSLMaterialTranslator::GameTime(bool bPeriodic, float Period)
{
	if (!bPeriodic)
	{
		return AddInlinedCodeChunkZeroDeriv(MCT_Float, bCompilingPreviousFrame ? TEXT("View.PrevFrameGameTime") : TEXT("View.GameTime"));
	}
	else if (Period == 0.0f)
	{
		return Constant(0.0f);
	}

	int32 PeriodChunk = Constant(Period);

	if (bCompilingPreviousFrame)
	{
		return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("fmod(View.PrevFrameGameTime,%s)"), *GetParameterCode(PeriodChunk));
	}

	// Note: not using FHLSLMaterialTranslator::Fmod(), which will emit MaterialFloat types which will be converted to fp16 on mobile.
	// We want full 32 bit float precision until the fmod when using a period.
	return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("fmod(View.GameTime,%s)"), *GetParameterCode(PeriodChunk));
}

int32 FHLSLMaterialTranslator::RealTime(bool bPeriodic, float Period)
{
	if (!bPeriodic)
	{
		if (bCompilingPreviousFrame)
		{
			return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("View.PrevFrameRealTime"));
		}

		return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("View.RealTime"));
	}
	else if (Period == 0.0f)
	{
		return Constant(0.0f);
	}

	int32 PeriodChunk = Constant(Period);

	if (bCompilingPreviousFrame)
	{
		return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("fmod(View.PrevFrameRealTime,%s)"), *GetParameterCode(PeriodChunk));
	}

	return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("fmod(View.RealTime,%s)"), *GetParameterCode(PeriodChunk));
}

int32 FHLSLMaterialTranslator::DeltaTime()
{
	// explicitly avoid trying to return previous frame's delta time for bCompilingPreviousFrame here
	// DeltaTime expression is designed to be used when generating custom motion vectors, by using world position offset along with previous frame switch
	// in this context, we will technically be evaluating the previous frame, but we want to use the current frame's delta tick in order to offset the vector used to create previous position
	return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("View.DeltaTime"));
}

int32 FHLSLMaterialTranslator::PeriodicHint(int32 PeriodicCode)
{
	if(PeriodicCode == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(PeriodicCode))
	{
		return AddUniformExpression(new FMaterialUniformExpressionPeriodic(GetParameterUniformExpression(PeriodicCode)),GetParameterType(PeriodicCode),TEXT("%s"),*GetParameterCode(PeriodicCode));
	}
	else
	{
		return PeriodicCode;
	}
}

int32 FHLSLMaterialTranslator::Sine(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(X),TMO_Sin),MCT_Float,TEXT("sin(%s)"),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this,FMaterialDerivativeAutogen::EFunc1::Sin,X);
		}
		else
		{
		return AddCodeChunk(GetParameterType(X),TEXT("sin(%s)"),*GetParameterCode(X));
	}
}
}

int32 FHLSLMaterialTranslator::Cosine(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(X),TMO_Cos),MCT_Float,TEXT("cos(%s)"),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this,FMaterialDerivativeAutogen::EFunc1::Cos,X);
		}
		else
		{
		return AddCodeChunk(GetParameterType(X),TEXT("cos(%s)"),*GetParameterCode(X));
	}
}
}

int32 FHLSLMaterialTranslator::Tangent(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(X),TMO_Tan),MCT_Float,TEXT("tan(%s)"),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Tan, X);
		}
		else
		{
		return AddCodeChunk(GetParameterType(X),TEXT("tan(%s)"),*GetParameterCode(X));
	}
}
}

int32 FHLSLMaterialTranslator::Arcsine(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(X),TMO_Asin),MCT_Float,TEXT("asin(%s)"),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Asin, X);
		}
		else
		{
		return AddCodeChunk(GetParameterType(X),TEXT("asin(%s)"),*GetParameterCode(X));
	}
}
}

int32 FHLSLMaterialTranslator::ArcsineFast(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(X),TMO_Asin),MCT_Float,TEXT("asinFast(%s)"),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::AsinFast, X);
		}
		else
		{
			return AddCodeChunk(GetParameterType(X), TEXT("asinFast(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Arccosine(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(X),TMO_Acos),MCT_Float,TEXT("acos(%s)"),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Acos, X);
		}
		else
		{
			return AddCodeChunk(GetParameterType(X), TEXT("acos(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::ArccosineFast(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(X),TMO_Acos),MCT_Float,TEXT("acosFast(%s)"),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::AcosFast, X);
		}
		else
		{
			return AddCodeChunk(GetParameterType(X), TEXT("acosFast(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Arctangent(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(X),TMO_Atan),MCT_Float,TEXT("atan(%s)"),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Atan, X);
		}
		else
		{
			return AddCodeChunk(GetParameterType(X), TEXT("atan(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::ArctangentFast(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(X),TMO_Atan),MCT_Float,TEXT("atanFast(%s)"),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::AtanFast, X);
		}
		else
		{
			return AddCodeChunk(GetParameterType(X), TEXT("atanFast(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Arctangent2(int32 Y, int32 X)
{
	if(Y == INDEX_NONE || X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(Y) && GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(Y),GetParameterUniformExpression(X),TMO_Atan2),MCT_Float,TEXT("atan2(%s, %s)"),*CoerceParameter(Y,MCT_Float),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Atan2, Y, X);
		}
		else
		{
			return AddCodeChunk(GetParameterType(Y), TEXT("atan2(%s, %s)"), *GetParameterCode(Y), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Arctangent2Fast(int32 Y, int32 X)
{
	if(Y == INDEX_NONE || X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(Y) && GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTrigMath(GetParameterUniformExpression(Y),GetParameterUniformExpression(X),TMO_Atan2),MCT_Float,TEXT("atan2Fast(%s, %s)"),*CoerceParameter(Y,MCT_Float),*CoerceParameter(X,MCT_Float));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Atan2Fast, Y, X);
		}
		else
		{
			return AddCodeChunk(GetParameterType(Y), TEXT("atan2Fast(%s, %s)"), *GetParameterCode(Y), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Floor(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionFloor(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("floor(%s)"),*GetParameterCode(X));
	}
	else
	{
		const EMaterialValueType ValueType = GetParameterType(X);
		if (IsLWCType(ValueType))
		{
			return AddCodeChunkZeroDeriv(ValueType, TEXT("LWCFloor(%s)"), *GetParameterCode(X));
		}
		else
		{
			return AddCodeChunkZeroDeriv(ValueType, TEXT("floor(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Ceil(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionCeil(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("ceil(%s)"),*GetParameterCode(X));
	}
	else
	{
		const EMaterialValueType ValueType = GetParameterType(X);
		if (IsLWCType(ValueType))
		{
			return AddCodeChunkZeroDeriv(ValueType, TEXT("LWCCeil(%s)"), *GetParameterCode(X));
		}
		else
		{
			return AddCodeChunkZeroDeriv(ValueType, TEXT("ceil(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Round(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionRound(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("round(%s)"),*GetParameterCode(X));
	}
	else
	{
		const EMaterialValueType ValueType = GetParameterType(X);
		if (IsLWCType(ValueType))
		{
			return AddCodeChunkZeroDeriv(ValueType, TEXT("LWCRound(%s)"), *GetParameterCode(X));
		}
		else
		{
			return AddCodeChunkZeroDeriv(ValueType, TEXT("round(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Truncate(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionTruncate(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("trunc(%s)"),*GetParameterCode(X));
	}
	else
	{
		const EMaterialValueType ValueType = GetParameterType(X);
		if (IsLWCType(ValueType))
		{
			return AddCodeChunkZeroDeriv(ValueType, TEXT("LWCTrunc(%s)"), *GetParameterCode(X));
		}
		else
		{
			return AddCodeChunkZeroDeriv(ValueType, TEXT("trunc(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Sign(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionSign(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("sign(%s)"),*GetParameterCode(X));
	}
	else
	{
		const EMaterialValueType ValueType = GetParameterType(X);
		if (IsLWCType(ValueType))
		{
			return AddCodeChunkZeroDeriv(MakeNonLWCType(ValueType), TEXT("LWCSign(%s)"), *GetParameterCode(X));
		}
		else
		{
			return AddCodeChunkZeroDeriv(ValueType, TEXT("sign(%s)"), *GetParameterCode(X));
		}
	}
}	

int32 FHLSLMaterialTranslator::Frac(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionFrac(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("frac(%s)"),*GetParameterCode(X));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Frac, X);
		}
		else
		{
			const EMaterialValueType Type = GetParameterType(X);
			if (IsLWCType(Type))
			{
				return AddCodeChunk(MakeNonLWCType(Type), TEXT("LWCFrac(%s)"), *GetParameterCode(X));
			}
			else
			{
				return AddCodeChunk(Type, TEXT("frac(%s)"), *GetParameterCode(X));
			}
		}
	}
}

int32 FHLSLMaterialTranslator::Fmod(int32 A, int32 B)
{
	if ((A == INDEX_NONE) || (B == INDEX_NONE))
	{
		return INDEX_NONE;
	}

	if (GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
	{
		return AddUniformExpression(new FMaterialUniformExpressionFmod(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),
			GetParameterType(A),TEXT("fmod(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
	}
	else
	{
		const FDerivInfo& BDerivInfo = GetDerivInfo(B);
		if (IsAnalyticDerivEnabled() && BDerivInfo.DerivativeStatus == EDerivativeStatus::Zero)
		{
			// Analytic derivatives only make sense when RHS derivatives are zero.
			return DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Fmod, A, B);
		}
		else
		{
			return AddCodeChunk(GetParameterType(A), TEXT("fmod(%s,%s)"), *GetParameterCode(A), *CoerceParameter(B, GetParameterType(A)));
		}
		
	}
}

/**
* Creates the new shader code chunk needed for the Abs expression
*
* @param	X - Index to the FMaterialCompiler::CodeChunk entry for the input expression
* @return	Index to the new FMaterialCompiler::CodeChunk entry for this expression
*/	
int32 FHLSLMaterialTranslator::Abs(int32 X)
{
	if (X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionAbs(GetParameterUniformExpression(X)), GetParameterType(X), TEXT("abs(%s)"), *GetParameterCode(X));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
	{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Abs, X);
	}
	else
	{
			return AddCodeChunk(GetParameterType(X), TEXT("abs(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::ReflectionVector()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}
	if (ShaderFrequency != SF_Vertex)
	{
		bUsesTransformVector = true;
	}
	return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.ReflectionVector"));
}

int32 FHLSLMaterialTranslator::ReflectionAboutCustomWorldNormal(int32 CustomWorldNormal, int32 bNormalizeCustomWorldNormal)
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}

	if (CustomWorldNormal == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (ShaderFrequency != SF_Vertex)
	{
		bUsesTransformVector = true;
	}

	const TCHAR* ShouldNormalize = (!!bNormalizeCustomWorldNormal) ? TEXT("true") : TEXT("false");

	return AddCodeChunk(MCT_Float3,TEXT("ReflectionAboutCustomWorldNormal(Parameters, %s, %s)"), *CoerceParameter(CustomWorldNormal, MCT_Float3), ShouldNormalize);
}

int32 FHLSLMaterialTranslator::CameraVector()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}
	if (ShaderFrequency != SF_Vertex)
	{
		bUsesTransformVector = true;
	}
	return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.CameraVector"));
}

int32 FHLSLMaterialTranslator::LightVector()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}

	if (!Material->IsLightFunction() && !Material->IsDeferredDecal())
	{
		return Errorf(TEXT("LightVector can only be used in LightFunction or DeferredDecal materials"));
	}

	return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.LightVector"));
}

int32 FHLSLMaterialTranslator::GetViewportUV()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex)
	{
		return Errorf(TEXT("GetViewportUV() node is only available in vertex or pixel shader input."));
	}

	FString FiniteCode = TEXT("GetViewportUV(Parameters)");
	if (IsAnalyticDerivEnabled())
	{
		FString AnalyticCode = DerivativeAutogen.ConstructDeriv(FiniteCode, TEXT("float2(View.ViewSizeAndInvSize.z, 0.0f)"), TEXT("float2(0.0f, View.ViewSizeAndInvSize.w)"), EDerivativeType::Float2);
		return AddCodeChunkInnerDeriv(*FiniteCode, *AnalyticCode, MCT_Float2, false, EDerivativeStatus::Valid);
	}
	else
	{
		return AddCodeChunk(MCT_Float2, *FiniteCode);
	}
}

int32 FHLSLMaterialTranslator::GetPixelPosition()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex)
	{
		return Errorf(TEXT("GetPixelPosition() node is only available in vertex or pixel shader input."));
	}
	return AddCodeChunk(MCT_Float2, TEXT("GetPixelPosition(Parameters)"));
}

int32 FHLSLMaterialTranslator::ParticleMacroUV()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}

	return AddCodeChunk(MCT_Float2,TEXT("GetParticleMacroUV(Parameters)"));
}

int32 FHLSLMaterialTranslator::ParticleSubUV(int32 TextureIndex, EMaterialSamplerType SamplerType, int32 MipValue0Index, int32 MipValue1Index, ETextureMipValueMode MipValueMode, bool bBlend)
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}

	if (TextureIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	int32 ParticleSubUV;
	const FString TexCoordCode = TEXT("Parameters.Particle.SubUVCoords[%u].xy");
	const int32 TexCoord1 = AddCodeChunk(MCT_Float2,*TexCoordCode,0);

	if(bBlend)
	{
		// Out	 = linear interpolate... using 2 sub-images of the texture
		// A	 = RGB sample texture with Parameters.Particle.SubUVCoords[0]
		// B	 = RGB sample texture with Parameters.Particle.SubUVCoords[1]
		// Alpha = Parameters.Particle.SubUVLerp

		const int32 TexCoord2 = AddCodeChunk( MCT_Float2,*TexCoordCode,1);
		const int32 SubImageLerp = AddCodeChunk(MCT_Float, TEXT("Parameters.Particle.SubUVLerp"));

		const int32 TexSampleA = TextureSample(TextureIndex, TexCoord1, SamplerType, MipValue0Index, MipValue1Index, MipValueMode);
		const int32 TexSampleB = TextureSample(TextureIndex, TexCoord2, SamplerType, MipValue0Index, MipValue1Index, MipValueMode);
		ParticleSubUV = Lerp( TexSampleA,TexSampleB, SubImageLerp);
	} 
	else
	{
		ParticleSubUV = TextureSample(TextureIndex, TexCoord1, SamplerType, MipValue0Index, MipValue1Index, MipValueMode);
	}
	
	bUsesParticleSubUVs = true;
	return ParticleSubUV;
}

int32 FHLSLMaterialTranslator::ParticleSubUVProperty(int32 PropertyIndex)
{
	int32 Result = INDEX_NONE;
	switch (PropertyIndex)
	{
	case 0:
		Result = AddCodeChunk(MCT_Float2, TEXT("Parameters.Particle.SubUVCoords[0].xy"));
		break;
	case 1:
		Result = AddCodeChunk(MCT_Float2, TEXT("Parameters.Particle.SubUVCoords[1].xy"));
		break;
	case 2:
		Result = AddCodeChunk(MCT_Float, TEXT("Parameters.Particle.SubUVLerp"));
		break;
	default:
		checkNoEntry();
		break;
	}

	bUsesParticleSubUVs = true;
	return Result;
}

int32 FHLSLMaterialTranslator::ParticleColor()
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	bUsesParticleColor |= (ShaderFrequency != SF_Vertex);
	return AddInlinedCodeChunkZeroDeriv(MCT_Float4,TEXT("Parameters.Particle.Color"));
}

int32 FHLSLMaterialTranslator::ParticlePosition()
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	bNeedsParticlePosition = true;
	const int32 Result = AddInlinedCodeChunkZeroDeriv(MCT_LWCVector3,TEXT("LWCSubtract(Parameters.Particle.TranslatedWorldPositionAndSize.xyz, ResolvedView.PreViewTranslation)"));
	return CastToNonLWCIfDisabled(Result);
}

int32 FHLSLMaterialTranslator::ParticleRadius()
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	bNeedsParticlePosition = true;
	return AddInlinedCodeChunkZeroDeriv(MCT_Float,TEXT("max(Parameters.Particle.TranslatedWorldPositionAndSize.w, .001f)"));
}

int32 FHLSLMaterialTranslator::SphericalParticleOpacity(int32 Density)
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}

	if (Density == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	bNeedsParticlePosition = true;
	bUsesSphericalParticleOpacity = true;
	bNeedsWorldPositionExcludingShaderOffsets = true;
	bUsesSceneDepth = true;
	return AddCodeChunk(MCT_Float, TEXT("GetSphericalParticleOpacity(Parameters,%s)"), *GetParameterCode(Density));
}

int32 FHLSLMaterialTranslator::ParticleRelativeTime()
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	bNeedsParticleTime = true;
	return AddInlinedCodeChunkZeroDeriv(MCT_Float,TEXT("Parameters.Particle.RelativeTime"));
}

int32 FHLSLMaterialTranslator::ParticleMotionBlurFade()
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	bUsesParticleMotionBlur = true;
	return AddInlinedCodeChunkZeroDeriv(MCT_Float,TEXT("Parameters.Particle.MotionBlurFade"));
}

int32 FHLSLMaterialTranslator::ParticleRandom()
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	bNeedsParticleRandom = true;
	return AddInlinedCodeChunkZeroDeriv(MCT_Float,TEXT("Parameters.Particle.Random"));
}


int32 FHLSLMaterialTranslator::ParticleDirection()
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	bNeedsParticleVelocity = true;
	return AddInlinedCodeChunkZeroDeriv(MCT_Float3,TEXT("Parameters.Particle.Velocity.xyz"));
}

int32 FHLSLMaterialTranslator::ParticleSpeed()
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	bNeedsParticleVelocity = true;
	return AddInlinedCodeChunkZeroDeriv(MCT_Float,TEXT("Parameters.Particle.Velocity.w"));
}

int32 FHLSLMaterialTranslator::ParticleSize()
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	bNeedsParticleSize = true;
	return AddInlinedCodeChunkZeroDeriv(MCT_Float2,TEXT("Parameters.Particle.Size"));
}

int32 FHLSLMaterialTranslator::WorldPosition(EWorldPositionIncludedOffsets WorldPositionIncludedOffsets)
{
	FString FunctionNamePattern;
	EMaterialValueType Type = (EMaterialValueType)0;

	// If this material has no expressions for world position offset or world displacement, the non-offset world position will
	// be exactly the same as the offset one, so there is no point bringing in the extra code.
	// Also, we can't access the full offset world position in anything other than the pixel shader, because it won't have
	// been calculated yet
	switch (WorldPositionIncludedOffsets)
	{
	case WPT_Default:
		{
			FunctionNamePattern = TEXT("Get<PREV>WorldPosition");
			Type = MCT_LWCVector3;
			break;
		}

	case WPT_ExcludeAllShaderOffsets:
		{
			bNeedsWorldPositionExcludingShaderOffsets = true;
			FunctionNamePattern = TEXT("Get<PREV>WorldPosition<NO_MATERIAL_OFFSETS>");
			Type = MCT_LWCVector3;
			break;
		}

	case WPT_CameraRelative:
		{
			FunctionNamePattern = TEXT("Get<PREV>TranslatedWorldPosition");
			Type = MCT_Float3;
			break;
		}

	case WPT_CameraRelativeNoOffsets:
		{
			bNeedsWorldPositionExcludingShaderOffsets = true;
			FunctionNamePattern = TEXT("Get<PREV>TranslatedWorldPosition<NO_MATERIAL_OFFSETS>");
			Type = MCT_Float3;
			break;
		}

	default:
		{
			Errorf(TEXT("Encountered unknown world position type '%d'"), WorldPositionIncludedOffsets);
			return INDEX_NONE;
		}
	}

	// If compiling for the previous frame in the vertex shader
	FunctionNamePattern.ReplaceInline(TEXT("<PREV>"), bCompilingPreviousFrame && ShaderFrequency == SF_Vertex ? TEXT("Prev") : TEXT(""));
		
	if (ShaderFrequency == SF_Pixel)
	{
		// No material offset only available in the vertex shader.
		FunctionNamePattern.ReplaceInline(TEXT("<NO_MATERIAL_OFFSETS>"), TEXT("_NoMaterialOffsets"));
	}
	else
	{
		FunctionNamePattern.ReplaceInline(TEXT("<NO_MATERIAL_OFFSETS>"), TEXT(""));
	}

	bUsesVertexPosition = true;

	FString FiniteCode = FString::Printf(TEXT("%s(Parameters)"), *FunctionNamePattern);
	int32 Result = INDEX_NONE;
	if (IsAnalyticDerivEnabled())
	{
		FString AnalyticCode = DerivativeAutogen.ConstructDeriv(FiniteCode, TEXT("Parameters.WorldPosition_DDX"), TEXT("Parameters.WorldPosition_DDY"), GetDerivType(Type));
		Result = AddCodeChunkInnerDeriv(*FiniteCode, *AnalyticCode, Type, false, EDerivativeStatus::Valid);
	}
	else
	{
		Result = AddInlinedCodeChunk(Type, *FiniteCode);
	}

	return CastToNonLWCIfDisabled(Result);
}

int32 FHLSLMaterialTranslator::ObjectWorldPosition()
{
	const int32 Result = AddInlinedCodeChunkZeroDeriv(MCT_LWCVector3,TEXT("GetObjectWorldPosition(Parameters)"));
	return CastToNonLWCIfDisabled(Result);
}

int32 FHLSLMaterialTranslator::ObjectRadius()
{
	return GetPrimitiveProperty(MCT_Float, TEXT("ObjectRadius"), TEXT("ObjectRadius"));		
}

int32 FHLSLMaterialTranslator::ObjectBounds()
{
	return AddInlinedCodeChunk(MCT_Float3, TEXT("float3(GetPrimitiveData(Parameters).ObjectBoundsX, GetPrimitiveData(Parameters).ObjectBoundsY, GetPrimitiveData(Parameters).ObjectBoundsZ)"));
}

int32 FHLSLMaterialTranslator::ObjectLocalBounds(int32 OutputIndex)
{
	switch (OutputIndex)
	{
	case 0: // Half extents
		return AddInlinedCodeChunk(MCT_Float3, TEXT("((GetPrimitiveData(Parameters).LocalObjectBoundsMax - GetPrimitiveData(Parameters).LocalObjectBoundsMin) / 2.0f)"));
	case 1: // Full extents
		return AddInlinedCodeChunk(MCT_Float3, TEXT("(GetPrimitiveData(Parameters).LocalObjectBoundsMax - GetPrimitiveData(Parameters).LocalObjectBoundsMin)"));
	case 2: // Min point
		return GetPrimitiveProperty(MCT_Float3, TEXT("ObjectLocalBounds"), TEXT("LocalObjectBoundsMin"));
	case 3: // Max point
		return GetPrimitiveProperty(MCT_Float3, TEXT("ObjectLocalBounds"), TEXT("LocalObjectBoundsMax"));
	default:
		check(false);
	}

	return INDEX_NONE; 
}

int32 FHLSLMaterialTranslator::PreSkinnedLocalBounds(int32 OutputIndex)
{
	switch (OutputIndex)
	{
	case 0: // Half extents
		return AddInlinedCodeChunk(MCT_Float3, TEXT("((GetPrimitiveData(Parameters).PreSkinnedLocalBoundsMax - GetPrimitiveData(Parameters).PreSkinnedLocalBoundsMin) / 2.0f)"));
	case 1: // Full extents
		return AddInlinedCodeChunk(MCT_Float3, TEXT("(GetPrimitiveData(Parameters).PreSkinnedLocalBoundsMax - GetPrimitiveData(Parameters).PreSkinnedLocalBoundsMin)"));
	case 2: // Min point
		return GetPrimitiveProperty(MCT_Float3, TEXT("PreSkinnedLocalBounds"), TEXT("PreSkinnedLocalBoundsMin"));
	case 3: // Max point
		return GetPrimitiveProperty(MCT_Float3, TEXT("PreSkinnedLocalBounds"), TEXT("PreSkinnedLocalBoundsMax"));
	default:
		check(false);
	}

	return INDEX_NONE; 
}

int32 FHLSLMaterialTranslator::DistanceCullFade()
{
	bUsesDistanceCullFade = true;

	return AddInlinedCodeChunk(MCT_Float,TEXT("GetDistanceCullFade()"));		
}

int32 FHLSLMaterialTranslator::ActorWorldPosition()
{
	int32 Result = INDEX_NONE;
	if (bCompilingPreviousFrame && ShaderFrequency == SF_Vertex)
	{
		// Decal VS doesn't have material code so FMaterialVertexParameters
		// and primitve uniform buffer are guaranteed to exist if ActorPosition
		// material node is used in VS
		Result = AddInlinedCodeChunkZeroDeriv(
			MCT_LWCVector3,
			TEXT("LWCMultiply(LWCMultiply(GetActorWorldPosition(Parameters), GetWorldToInstance(Parameters)), Parameters.PrevFrameLocalToWorld)"));
	}
	else
	{
		Result = AddInlinedCodeChunkZeroDeriv(MCT_LWCVector3, TEXT("GetActorWorldPosition(Parameters)"));
	}

	return CastToNonLWCIfDisabled(Result);
}

int32 FHLSLMaterialTranslator::DynamicBranch(int32 Condition, int32 A, int32 B)
{
	if (Condition == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (B == INDEX_NONE)
	{
		return A;
	}

	if (A == INDEX_NONE)
	{
		return B;
	}

	EMaterialValueType TypeA = GetParameterType(A);
	EMaterialValueType TypeB = GetParameterType(B);
	if (IsLWCType(TypeA) || IsLWCType(TypeB))
	{
		TypeA = MakeLWCType(TypeA);
		TypeB = MakeLWCType(TypeB);
	}

	EMaterialValueType ResultType = MCT_Unknown;
	if (TypeA == TypeB)
	{
		ResultType = TypeA;
	}
	else if (!IsFloatNumericType(TypeA) || !IsFloatNumericType(TypeB))
	{
		Errorf(TEXT("Cannot branch on non float numeric Types if they are not equal: %s %s"), DescribeType(TypeA), DescribeType(TypeB));
		return INDEX_NONE;
	}
	else
	{
		ResultType = GetNumComponents(TypeA) > GetNumComponents(TypeB) ? TypeA : TypeB;
	}

	A = ForceCast(A, ResultType, MFCF_ReplicateValue);
	B = ForceCast(B, ResultType, MFCF_ReplicateValue);
	FString SymbolName = CreateSymbolName(TEXT("Static"));

	checkf(Condition >= 0 && Condition < CurrentScopeChunks->Num(), TEXT("Index %d/%d, Platform=%d"), Condition, CurrentScopeChunks->Num(), (int)Platform);
	const FShaderCodeChunk& CodeChunk = (*CurrentScopeChunks)[Condition];

	if (CodeChunk.UniformExpression && CodeChunk.UniformExpression->GetType() == &FMaterialUniformExpressionStaticBoolParameter::StaticType)
	{
		FMaterialUniformExpressionStaticBoolParameter* StaticBoolParameter = static_cast<FMaterialUniformExpressionStaticBoolParameter*>(CodeChunk.UniformExpression.GetReference());
		AddCodeChunk(MCT_VoidStatement, TEXT("//%s"), *StaticBoolParameter->GetParameterName().ToString());
	}

	AddCodeChunk(MCT_VoidStatement, TEXT("%s %s;"), HLSLTypeString(ResultType), *SymbolName);
	AddCodeChunk(MCT_VoidStatement, TEXT("[branch] switch (int(%s)){ default: %s = %s; break; case 0: %s = %s; break;}"), *GetParameterCode(Condition), *SymbolName, *GetParameterCode(A), *SymbolName, *GetParameterCode(B));
	return AddCodeChunk(ResultType, *SymbolName);
}

// Compare two inputs and return true if they are either the same, or if they evaluate to equal constant expressions.
static bool AreEqualExpressions(int32 A, int32 B, TArray<FShaderCodeChunk> const& InCurrentScopeChunks, FMaterial const& InMaterial)
{
	if (A == B)
	{
		return true;
	}
	if (A == -1 || B == -1)
	{
		return false;
	}
	
	FShaderCodeChunk const& ChunkA = InCurrentScopeChunks[A];
	FShaderCodeChunk const& ChunkB = InCurrentScopeChunks[B];

	if (ChunkA.UniformExpression != nullptr && ChunkB.UniformExpression != nullptr)
	{
		if (ChunkA.UniformExpression == ChunkB.UniformExpression)
		{
			return true;
		}
		if (ChunkA.UniformExpression->IsConstant() && ChunkB.UniformExpression->IsConstant() && ChunkA.Type == ChunkB.Type)
		{
			FMaterialRenderContext DummyContext(nullptr, InMaterial, nullptr);
			FLinearColor ValueA, ValueB;
			ChunkA.UniformExpression->GetNumberValue(DummyContext, ValueA);
			ChunkB.UniformExpression->GetNumberValue(DummyContext, ValueB);
			if (ValueA == ValueB)
			{
				return true;
			}
		}
	}

	return false;
}

int32 FHLSLMaterialTranslator::If(int32 A,int32 B,int32 AGreaterThanB,int32 AEqualsB,int32 ALessThanB,int32 ThresholdArg)
{
	if (A == INDEX_NONE || B == INDEX_NONE || AGreaterThanB == INDEX_NONE || ALessThanB == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (AreEqualExpressions(AGreaterThanB, ALessThanB, *CurrentScopeChunks, *Material))
	{
		if (AEqualsB == INDEX_NONE || AreEqualExpressions(AGreaterThanB, AEqualsB, *CurrentScopeChunks, *Material))
		{
			// All inputs are considered equal. So simply return one of them.
			return AGreaterThanB;
		}
	}

	if (IsAnalyticDerivEnabled())
	{
		return DerivativeAutogen.GenerateIfFunc(*this, A, B, AGreaterThanB, AEqualsB, ALessThanB, ThresholdArg);
	}
	
	if (AEqualsB != INDEX_NONE)
	{
		if (ThresholdArg == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		EMaterialValueType ResultType = GetArithmeticResultType(GetParameterType(AGreaterThanB),GetArithmeticResultType(AEqualsB,ALessThanB));

		int32 CoercedAGreaterThanB = ForceCast(AGreaterThanB,ResultType);
		int32 CoercedAEqualsB = ForceCast(AEqualsB,ResultType);
		int32 CoercedALessThanB = ForceCast(ALessThanB,ResultType);

		if(CoercedAGreaterThanB == INDEX_NONE || CoercedAEqualsB == INDEX_NONE || CoercedALessThanB == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		return AddCodeChunk(
			ResultType,
			TEXT("select(abs(%s - %s) > %s, select(%s >= %s, %s, %s), %s)"),
			*GetParameterCode(A),
			*GetParameterCode(B),
			*GetParameterCode(ThresholdArg),
			*GetParameterCode(A),
			*GetParameterCode(B),
			*GetParameterCode(CoercedAGreaterThanB),
			*GetParameterCode(CoercedALessThanB),
			*GetParameterCode(CoercedAEqualsB)
			);
	}
	else
	{
		EMaterialValueType ResultType = GetArithmeticResultType(AGreaterThanB,ALessThanB);

		int32 CoercedAGreaterThanB = ForceCast(AGreaterThanB,ResultType);
		int32 CoercedALessThanB = ForceCast(ALessThanB,ResultType);

		if(CoercedAGreaterThanB == INDEX_NONE || CoercedALessThanB == INDEX_NONE)
		{
			return INDEX_NONE;
		}

		return AddCodeChunk(
			ResultType,
			TEXT("select(%s >= %s, %s, %s)"),
			*GetParameterCode(A),
			*GetParameterCode(B),
			*GetParameterCode(CoercedAGreaterThanB),
			*GetParameterCode(CoercedALessThanB)
			);
	}
}

void FHLSLMaterialTranslator::AllocateSlot(TBitArray<>& InBitArray, int32 InSlotIndex, int32 InSlotCount) const
{
	// Grow as needed
	int32 NumSlotsNeeded = InSlotIndex + InSlotCount;
	int32 CurrentNumSlots = InBitArray.Num();
	if(NumSlotsNeeded > CurrentNumSlots)
	{
		InBitArray.Add(false, NumSlotsNeeded - CurrentNumSlots);
	}

	// Allocate the requested slot(s)
	for (int32 i = InSlotIndex; i < NumSlotsNeeded; ++i)
	{
		InBitArray[i] = true;
	}
}

#if WITH_EDITOR
int32 FHLSLMaterialTranslator::MaterialBakingWorldPosition()
{
	if (ShaderFrequency == SF_Vertex)
	{
		AllocateSlot(AllocatedUserVertexTexCoords, 6, 2);
	}
	else
	{
		AllocateSlot(AllocatedUserTexCoords, 6, 2);
	}

	// Note: inlining is important so that on GLES devices, where half precision is used in the pixel shader, 
	// The UV does not get assigned to a half temporary in cases where the texture sample is done directly from interpolated UVs
	return AddInlinedCodeChunk(MCT_Float3, TEXT("float3(Parameters.TexCoords[6].x, Parameters.TexCoords[6].y, Parameters.TexCoords[7].x)"));
}
#endif
		

int32 FHLSLMaterialTranslator::TextureCoordinate(uint32 CoordinateIndex, bool UnMirrorU, bool UnMirrorV)
{
	const uint32 MaxNumCoordinates = 8;

	if (CoordinateIndex >= MaxNumCoordinates)
	{
		return Errorf(TEXT("Only %u texture coordinate sets can be used by this feature level, currently using %u"), MaxNumCoordinates, CoordinateIndex + 1);
	}

	if (ShaderFrequency == SF_Vertex)
	{
		AllocateSlot(AllocatedUserVertexTexCoords, CoordinateIndex);
	}
	else
	{
		AllocateSlot(AllocatedUserTexCoords, CoordinateIndex);
	}

	FString TexCoordCode = FString::Printf(TEXT("Parameters.TexCoords[%u].xy"), CoordinateIndex);
	FString	SampleCodeFinite = TexCoordCode;
	
	if (UnMirrorU || UnMirrorV)
	{
		SampleCodeFinite = FString::Printf(TEXT("%s(%s, Parameters)"), (UnMirrorU && UnMirrorV) ? TEXT("UnMirrorUV") : (UnMirrorU ? TEXT("UnMirrorU") : TEXT("UnMirrorV")), *SampleCodeFinite);
	}

	if (IsAnalyticDerivEnabled())
	{
		FString TexCoordCodeDDX = FString::Printf(TEXT("Parameters.TexCoords_DDX[%u].xy"), CoordinateIndex);
		FString TexCoordCodeDDY = FString::Printf(TEXT("Parameters.TexCoords_DDY[%u].xy"), CoordinateIndex);
		FString TexCoordCodeAnalytic = DerivativeAutogen.ConstructDeriv(TexCoordCode, TexCoordCodeDDX, TexCoordCodeDDY, EDerivativeType::Float2);
		FString SampleCodeAnalytic = DerivativeAutogen.ApplyUnMirror(TexCoordCodeAnalytic, UnMirrorU, UnMirrorV);

		return AddCodeChunkInnerDeriv(*SampleCodeFinite, *SampleCodeAnalytic, MCT_Float2, false, EDerivativeStatus::Valid);
	}
	else
	{
	// Note: inlining is important so that on GLES devices, where half precision is used in the pixel shader, 
	// The UV does not get assigned to a half temporary in cases where the texture sample is done directly from interpolated UVs
		return AddInlinedCodeChunk(MCT_Float2, *SampleCodeFinite, CoordinateIndex);
	}
}

static const TCHAR* GetVTAddressMode(TextureAddress Address)
{
	switch (Address)
	{
	case TA_Wrap: return TEXT("VTADDRESSMODE_WRAP");
	case TA_Clamp: return TEXT("VTADDRESSMODE_CLAMP");
	case TA_Mirror: return TEXT("VTADDRESSMODE_MIRROR");
	default: checkNoEntry(); return nullptr;
	}
}

uint32 FHLSLMaterialTranslator::AcquireVTStackIndex(
	ETextureMipValueMode MipValueMode, 
	TextureAddress AddressU, TextureAddress AddressV, 
	float AspectRatio, 
	int32 CoordinateIndex, 
	int32 MipValue0Index, int32 MipValue1Index, 
	int32 PreallocatedStackTextureIndex, 
	const FString& UV_Value,
	const FString& UV_Ddx,
	const FString& UV_Ddy,
	bool bAdaptive, bool bGenerateFeedback)
{
	const uint64 CoordinatHash = GetParameterHash(CoordinateIndex);
	const uint64 MipValue0Hash = GetParameterHash(MipValue0Index);
	const uint64 MipValue1Hash = GetParameterHash(MipValue1Index);

	uint64 Hash = CityHash128to64({ CurrentScopeID, CoordinatHash });
	Hash = CityHash128to64({ Hash, MipValue0Hash });
	Hash = CityHash128to64({ Hash, MipValue1Hash });
	Hash = CityHash128to64({ Hash, (uint64)MipValueMode });
	Hash = CityHash128to64({ Hash, (uint64)AddressU });
	Hash = CityHash128to64({ Hash, (uint64)AddressV });
	Hash = CityHash128to64({ Hash, (uint64)(AspectRatio * 1000.0f) });
	Hash = CityHash128to64({ Hash, (uint64)PreallocatedStackTextureIndex });
	Hash = CityHash128to64({ Hash, (uint64)(bAdaptive ? 1 : 0) });
	Hash = CityHash128to64({ Hash, (uint64)(bGenerateFeedback ? 1 : 0) });

	// First check to see if we have an existing VTStack that matches this key, that can still fit another layer
	for (int32 Index = VTStackHash.First(Hash); VTStackHash.IsValid(Index); Index = VTStackHash.Next(Index))
	{
		const FMaterialVirtualTextureStack& Stack = MaterialCompilationOutput.UniformExpressionSet.VTStacks[Index];
		const FMaterialVTStackEntry& Entry = VTStacks[Index];
		if (!Stack.AreLayersFull() &&
			Entry.ScopeID == CurrentScopeID &&
			Entry.CoordinateHash == CoordinatHash &&
			Entry.MipValue0Hash == MipValue0Hash &&
			Entry.MipValue1Hash == MipValue1Hash &&
			Entry.MipValueMode == MipValueMode &&
			Entry.AddressU == AddressU &&
			Entry.AddressV == AddressV &&
			Entry.AspectRatio == AspectRatio &&
			Entry.PreallocatedStackTextureIndex == PreallocatedStackTextureIndex &&
			Entry.bAdaptive == bAdaptive &&
			Entry.bGenerateFeedback == bGenerateFeedback)
		{
			return Index;
		}
	}

	// Need to allocate a new VTStack
	const int32 StackIndex = VTStacks.AddDefaulted();
	VTStackHash.Add(Hash, StackIndex);
	FMaterialVTStackEntry& Entry = VTStacks[StackIndex];
	Entry.ScopeID = CurrentScopeID;
	Entry.CoordinateHash = CoordinatHash;
	Entry.MipValue0Hash = MipValue0Hash;
	Entry.MipValue1Hash = MipValue1Hash;
	Entry.MipValueMode = MipValueMode;
	Entry.AddressU = AddressU;
	Entry.AddressV = AddressV;
	Entry.AspectRatio = AspectRatio;
	Entry.DebugCoordinateIndex = CoordinateIndex;
	Entry.DebugMipValue0Index = MipValue0Index;
	Entry.DebugMipValue1Index = MipValue1Index;
	Entry.PreallocatedStackTextureIndex = PreallocatedStackTextureIndex;
	Entry.bAdaptive = bAdaptive;
	Entry.bGenerateFeedback = bGenerateFeedback;

	MaterialCompilationOutput.UniformExpressionSet.VTStacks.Add(FMaterialVirtualTextureStack(PreallocatedStackTextureIndex));

	// These two arrays need to stay in sync
	check(VTStacks.Num() == MaterialCompilationOutput.UniformExpressionSet.VTStacks.Num());

	// Select LoadVirtualPageTable function name for this context
	FString BaseFunctionName = bAdaptive ? TEXT("TextureLoadVirtualPageTableAdaptive") : TEXT("TextureLoadVirtualPageTable");

	// Optionally sample without virtual texture feedback but only for miplevel mode
	check(bGenerateFeedback || MipValueMode == TMVM_MipLevel)
	FString FeedbackParameter = bGenerateFeedback ? FString::Printf(TEXT(", %dU + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback"), StackIndex) : TEXT("");

	EDerivativeStatus UVDerivativeStatus = GetDerivativeStatus(CoordinateIndex);
	const bool bHasValidDerivative = IsAnalyticDerivEnabled() && IsDerivativeValid(UVDerivativeStatus);

	// Code to load the VT page table...this will execute the first time a given VT stack is accessed
	// Additional stack layers will simply reuse these results
	switch (MipValueMode)
	{
	case TMVM_None:
	{
		FString SampleCodeFinite = FString::Printf(TEXT(
			"%s("
			"VIRTUALTEXTURE_PAGETABLE_%d, "
			"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%d*2], Material.VTPackedPageTableUniform[%d*2+1]), "
			"%s, %s, %s, "
			"0, Parameters.SvPosition.xy, "
			"%dU + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback)"),
			*BaseFunctionName,
			StackIndex, StackIndex, StackIndex,
			*CoerceParameter(CoordinateIndex, MCT_Float2), GetVTAddressMode(AddressU), GetVTAddressMode(AddressV),
			StackIndex);

		if (bHasValidDerivative)
		{
			FString SampleCodeAnalytic = FString::Printf(TEXT(
				"%sGrad("
				"VIRTUALTEXTURE_PAGETABLE_%d, "
				"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%d*2], Material.VTPackedPageTableUniform[%d*2+1]), "
				"%s, %s, %s, "
				"%s, %s, Parameters.SvPosition.xy, "
				"%dU + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback)"),
				*BaseFunctionName,
				StackIndex, StackIndex, StackIndex,
				*UV_Value, GetVTAddressMode(AddressU), GetVTAddressMode(AddressV), 
				*UV_Ddx, *UV_Ddy,
				StackIndex);
			Entry.CodeIndex = AddCodeChunkInnerDeriv(*SampleCodeFinite, *SampleCodeAnalytic, MCT_VTPageTableResult, false, EDerivativeStatus::NotValid);
		}
		else
		{
			Entry.CodeIndex = AddCodeChunk(MCT_VTPageTableResult, *SampleCodeFinite);
		}
		break;
	}
		
	case TMVM_MipBias:
	{
		FString SampleCodeFinite = FString::Printf(TEXT(
			"%s("
			"VIRTUALTEXTURE_PAGETABLE_%d, "
			"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%d*2], Material.VTPackedPageTableUniform[%d*2+1]), "
			"%s, %s, %s, "
			"%s, Parameters.SvPosition.xy, "
			"%dU + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback)"),
			*BaseFunctionName,
			StackIndex, StackIndex, StackIndex, 
			*CoerceParameter(CoordinateIndex, MCT_Float2), GetVTAddressMode(AddressU), GetVTAddressMode(AddressV), 
			*CoerceParameter(MipValue0Index, MCT_Float1),
			StackIndex);
		if (bHasValidDerivative)
		{
			FString SampleCodeAnalytic = FString::Printf(TEXT(
				"%sGrad("
				"VIRTUALTEXTURE_PAGETABLE_%d, "
				"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%d*2], Material.VTPackedPageTableUniform[%d*2+1]), "
				"%s, %s, %s, "
				"%s, %s, Parameters.SvPosition.xy, "
				"%dU + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback)"),
				*BaseFunctionName,
				StackIndex, StackIndex, StackIndex,
				*UV_Value, GetVTAddressMode(AddressU), GetVTAddressMode(AddressV),
				*UV_Ddx, *UV_Ddy,
				StackIndex);
			Entry.CodeIndex = AddCodeChunkInnerDeriv(*SampleCodeFinite, *SampleCodeAnalytic, MCT_VTPageTableResult, false, EDerivativeStatus::NotValid);
		}
		else
		{
			Entry.CodeIndex = AddCodeChunk(MCT_VTPageTableResult, *SampleCodeFinite);
		}
		break;
	}
	case TMVM_MipLevel:
		Entry.CodeIndex = AddCodeChunk(MCT_VTPageTableResult, TEXT(
			"%sLevel("
			"VIRTUALTEXTURE_PAGETABLE_%d, " 
			"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%d*2], Material.VTPackedPageTableUniform[%d*2+1]), "
			"%s, %s, %s, "
			"%s"
			"%s)"),
			*BaseFunctionName,
			StackIndex, StackIndex, StackIndex,
			*CoerceParameter(CoordinateIndex, MCT_Float2), GetVTAddressMode(AddressU), GetVTAddressMode(AddressV), 
			*CoerceParameter(MipValue0Index, MCT_Float1),
			*FeedbackParameter);
		break;
	case TMVM_Derivative:
		Entry.CodeIndex = AddCodeChunk(MCT_VTPageTableResult, TEXT(
			"%sGrad("
			"VIRTUALTEXTURE_PAGETABLE_%d, "
			"VTPageTableUniform_Unpack(Material.VTPackedPageTableUniform[%d*2], Material.VTPackedPageTableUniform[%d*2+1]), "
			"%s, %s, %s, "
			"%s, %s, Parameters.SvPosition.xy, "
			"%dU + LIGHTMAP_VT_ENABLED, Parameters.VirtualTextureFeedback)"),
			*BaseFunctionName,
			StackIndex, StackIndex, StackIndex, 
			*CoerceParameter(CoordinateIndex, MCT_Float2), GetVTAddressMode(AddressU), GetVTAddressMode(AddressV),
			*CoerceParameter(MipValue0Index, MCT_Float2), *CoerceParameter(MipValue1Index, MCT_Float2),
			StackIndex);
		break;
	default:
		checkNoEntry();
		break;
	}

	return StackIndex;
}


static FString ApplySamplerType(const FString& InSampleCode, EMaterialSamplerType SamplerType)
{
	FString DstSampleCode;

	switch( SamplerType )
	{
	case SAMPLERTYPE_External:
		DstSampleCode = FString::Printf(TEXT("ProcessMaterialExternalTextureLookup(%s)"), *InSampleCode);
		break;

	case SAMPLERTYPE_Color:
		DstSampleCode = FString::Printf( TEXT("ProcessMaterialColorTextureLookup(%s)"), *InSampleCode );
		break;
	case SAMPLERTYPE_VirtualColor:
		// has a mobile specific workaround
		DstSampleCode = FString::Printf( TEXT("ProcessMaterialVirtualColorTextureLookup(%s)"), *InSampleCode );
		break;

	case SAMPLERTYPE_LinearColor:
	case SAMPLERTYPE_VirtualLinearColor:
		DstSampleCode = FString::Printf(TEXT("ProcessMaterialLinearColorTextureLookup(%s)"), *InSampleCode);
		break;

	case SAMPLERTYPE_Alpha:
	case SAMPLERTYPE_VirtualAlpha:
	case SAMPLERTYPE_DistanceFieldFont:
		DstSampleCode = FString::Printf( TEXT("ProcessMaterialAlphaTextureLookup(%s)"), *InSampleCode );
		break;

	case SAMPLERTYPE_Grayscale:
	case SAMPLERTYPE_VirtualGrayscale:
		DstSampleCode = FString::Printf( TEXT("ProcessMaterialGreyscaleTextureLookup(%s)"), *InSampleCode );
		break;

	case SAMPLERTYPE_LinearGrayscale:
	case SAMPLERTYPE_VirtualLinearGrayscale:
		DstSampleCode = FString::Printf(TEXT("ProcessMaterialLinearGreyscaleTextureLookup(%s)"), *InSampleCode);
		break;

	case SAMPLERTYPE_Normal:
	case SAMPLERTYPE_VirtualNormal:
		// Normal maps need to be unpacked in the pixel shader.
		DstSampleCode = FString::Printf( TEXT("UnpackNormalMap(%s)"), *InSampleCode );
		break;

	case SAMPLERTYPE_Masks:
	case SAMPLERTYPE_VirtualMasks:
		DstSampleCode = InSampleCode;
		break;

	case SAMPLERTYPE_Data:
		DstSampleCode = InSampleCode;
		break;

	default:
		check(0);
		break;
	}

	return DstSampleCode;
}

static bool SamplerDebugSupported(EMaterialValueType TextureType, bool bVirtualTexture, bool bDecal)
{
	return IsDebugTextureSampleEnabled() && (TextureType == MCT_Texture2D || bVirtualTexture) && !bDecal;
}

int32 FHLSLMaterialTranslator::TextureSample(
	int32 TextureIndex,
	int32 CoordinateIndex,
	EMaterialSamplerType SamplerType,
	int32 MipValue0Index,
	int32 MipValue1Index,
	ETextureMipValueMode MipValueMode,
	ESamplerSourceMode SamplerSource,
	int32 TextureReferenceIndex,
	bool AutomaticViewMipBias,
	bool AdaptiveVirtualTexture,
	bool EnableFeedback
)
{
	if (TextureIndex == INDEX_NONE || CoordinateIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	EMaterialValueType TextureType = GetParameterType(TextureIndex);

	if (!(TextureType & MCT_Texture))
	{
		Errorf(TEXT("Sampling unknown texture type: %s"), DescribeType(TextureType));
		return INDEX_NONE;
	}

	if (ShaderFrequency != SF_Pixel && MipValueMode == TMVM_MipBias)
	{
		Errorf(TEXT("MipBias is only supported in the pixel shader"));
		return INDEX_NONE;
	}

	const bool bVirtualTexture = TextureType == MCT_TextureVirtual;
	if (bVirtualTexture)
	{
		if (Material->GetMaterialDomain() != MD_Surface && Material->GetMaterialDomain() != MD_DeferredDecal)
		{
			return Errorf(TEXT("Sampling a virtual texture is currently only supported for surface and decal materials."));
		}
	}

	if (MipValueMode == TMVM_Derivative)
	{
		if (MipValue0Index == INDEX_NONE)
		{
			return Errorf(TEXT("Missing DDX(UVs) parameter"));
		}
		else if (MipValue1Index == INDEX_NONE)
		{
			return Errorf(TEXT("Missing DDY(UVs) parameter"));
		}
		else if (!IsFloatNumericType(GetParameterType(MipValue0Index)))
		{
			return Errorf(TEXT("Invalid DDX(UVs) parameter"));
		}
		else if (!IsFloatNumericType(GetParameterType(MipValue1Index)))
		{
			return Errorf(TEXT("Invalid DDY(UVs) parameter"));
		}
	}
	else if (MipValueMode != TMVM_None && MipValue0Index != INDEX_NONE && !IsFloatNumericType(GetParameterType(MipValue0Index)))
	{
		return Errorf(TEXT("Invalid mip map parameter"));
	}

	// if we are not in the PS we need a mip level
	if (ShaderFrequency != SF_Pixel)
	{
		MipValueMode = TMVM_MipLevel;
		AutomaticViewMipBias = false;

		if (MipValue0Index == INDEX_NONE)
		{
			MipValue0Index = Constant(0.f);
		}
	}

	// Automatic view mip bias is only for surface and decal domains.
	if (Material->GetMaterialDomain() != MD_Surface && Material->GetMaterialDomain() != MD_DeferredDecal)
	{
		AutomaticViewMipBias = false;
	}

	// If mobile, then disabling AutomaticViewMipBias.
	if (FeatureLevel < ERHIFeatureLevel::SM5)
	{
		AutomaticViewMipBias = false;
	}

	// If not 2D texture, disable AutomaticViewMipBias.
	if (!(TextureType & (MCT_Texture2D | MCT_TextureVirtual)))
	{
		AutomaticViewMipBias = false;
	}

	FString TextureName;
	int32 VirtualTextureIndex = INDEX_NONE;
	if (TextureType == MCT_TextureCube)
	{
		TextureName = CoerceParameter(TextureIndex, MCT_TextureCube);
	}
	else if (TextureType == MCT_Texture2DArray)
	{
		TextureName = CoerceParameter(TextureIndex, MCT_Texture2DArray);
	}
	else if (TextureType == MCT_TextureCubeArray)
	{
		TextureName = CoerceParameter(TextureIndex, MCT_TextureCubeArray);
	}
	else if (TextureType == MCT_VolumeTexture)
	{
		TextureName = CoerceParameter(TextureIndex, MCT_VolumeTexture);
	}
	else if (TextureType == MCT_TextureExternal)
	{
		TextureName = CoerceParameter(TextureIndex, MCT_TextureExternal);
	}
	else if (bVirtualTexture)
	{
		// Note, this does not really do anything (by design) other than adding it to the UniformExpressionSet
		/*TextureName =*/ CoerceParameter(TextureIndex, TextureType);

		FMaterialUniformExpression* UniformExpression = GetParameterUniformExpression(TextureIndex); 
		if (UniformExpression == nullptr)
		{
			return Errorf(TEXT("Unable to find VT uniform expression."));
		}
		FMaterialUniformExpressionTexture* TextureUniformExpression = UniformExpression->GetTextureUniformExpression();
		if (TextureUniformExpression == nullptr)
		{
			return Errorf(TEXT("The provided uniform expression is not a texture"));
		}

		VirtualTextureIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Virtual].Find(TextureUniformExpression);
		check(UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Virtual].IsValidIndex(VirtualTextureIndex));

		if (SamplerSource != SSM_FromTextureAsset)
		{
			// VT doesn't care if the shared sampler is wrap or clamp. It only cares if it is aniso or not.
			// The wrap/clamp/mirror operation is handled in the shader explicitly.
			const bool bUseAnisoSampler = VirtualTextureScalability::IsAnisotropicFilteringEnabled() && MipValueMode != TMVM_MipLevel;
			const TCHAR* SharedSamplerName = bUseAnisoSampler ? TEXT("View.SharedBilinearAnisoClampedSampler") : TEXT("View.SharedBilinearClampedSampler");
			TextureName += FString::Printf(TEXT("Material.VirtualTexturePhysical_%d, GetMaterialSharedSampler(Material.VirtualTexturePhysical_%dSampler, %s)")
				, VirtualTextureIndex, VirtualTextureIndex, SharedSamplerName);
		}
		else
		{
			TextureName += FString::Printf(TEXT("Material.VirtualTexturePhysical_%d, Material.VirtualTexturePhysical_%dSampler")
				, VirtualTextureIndex, VirtualTextureIndex);
		}

		NumVtSamples++;
	}
	else // MCT_Texture2D
	{
		TextureName = CoerceParameter(TextureIndex, MCT_Texture2D);
	}

	// Won't be able to get the texture, if this is an external texture sample
	const UTexture* Texture = nullptr;
	if (TextureType != MCT_TextureExternal)
	{
		FMaterialUniformExpression* Expression = (*CurrentScopeChunks)[TextureIndex].UniformExpression;
		const FMaterialUniformExpressionTexture* TextureExpression = Expression ? Expression->GetTextureUniformExpression() : nullptr;
		if (ensure(TextureExpression))
		{
			Texture = Cast<UTexture>(Material->GetReferencedTextures()[TextureExpression->GetTextureIndex()]);
		}
	}

	// StaticAddress mode at time of compile
	// This may not be 100% correct, if SamplerSource is set to 'SSM_FromTextureAsset', as texture parameter may change the address mode in a derived instance
	TextureAddress StaticAddressX = TA_Wrap;
	TextureAddress StaticAddressY = TA_Wrap;
	TextureAddress StaticAddressZ = TA_Wrap;
	if (Texture && Texture->Source.GetNumBlocks() > 1)
	{
		// UDIM (multi-block) texture are forced to use wrap address mode
		// This is important for supporting VT stacks made from UDIMs with differing number of blocks, as this requires wrapping vAddress for certain layers
		StaticAddressX = TA_Wrap;
		StaticAddressY = TA_Wrap;
		StaticAddressZ = TA_Wrap;
	}
	else
	{
		switch (SamplerSource)
		{
		case SSM_FromTextureAsset:
			if (Texture)
			{
				StaticAddressX = Texture->GetTextureAddressX();
				StaticAddressY = Texture->GetTextureAddressY();
				StaticAddressZ = Texture->GetTextureAddressZ();
			}
			break;
		case SSM_Wrap_WorldGroupSettings:
			StaticAddressX = TA_Wrap;
			StaticAddressY = TA_Wrap;
			StaticAddressZ = TA_Wrap;
			break;
		case SSM_Clamp_WorldGroupSettings:
		case SSM_TerrainWeightmapGroupSettings:
			StaticAddressX = TA_Clamp;
			StaticAddressY = TA_Clamp;
			StaticAddressZ = TA_Clamp;
			break;
		default:
			checkNoEntry();
			break;
		}
	}

	EMaterialValueType UVsType;
	switch (TextureType)
	{
	case MCT_TextureCubeArray:
		UVsType = MCT_Float4;
		break;
	case MCT_TextureCube:
	case MCT_Texture2DArray:
	case MCT_VolumeTexture:
		UVsType = MCT_Float3;
		break;
	default:
		UVsType = MCT_Float2;
		break;
	}
	
	int32 NonLWCCoordinateIndex = CoordinateIndex;
	const bool bLWCCoordinates = IsLWCType(GetParameterType(CoordinateIndex));
	if (bLWCCoordinates)
	{
		// Apply texture address math manually, using LWC-scale operations, then convert the result to float
		// This could potentially cause problems if content is relying on SSM_FromTextureAsset, and having texture parameters change address mode in MI
		// Trade-off would be skip manual address mode in this case, and just accept precision loss
		auto AddressModeToString = [](TextureAddress InAddress)
		{
			switch (InAddress)
			{
			case TA_Clamp: return TEXT("LWCADDRESSMODE_CLAMP");
			case TA_Wrap: return TEXT("LWCADDRESSMODE_WRAP");
			case TA_Mirror: return TEXT("LWCADDRESSMODE_MIRROR");
			default: checkNoEntry(); return TEXT("");
			}
		};
		const uint32 NumComponents = GetNumComponents(UVsType);
		switch (NumComponents)
		{
		case 1u:
			NonLWCCoordinateIndex = AddCodeChunkZeroDeriv(UVsType, TEXT("LWCApplyAddressMode(%s, %s)"),
				*CoerceParameter(CoordinateIndex, MCT_LWCScalar),
				AddressModeToString(StaticAddressX));
			break;
		case 2u:
			NonLWCCoordinateIndex = AddCodeChunkZeroDeriv(UVsType, TEXT("LWCApplyAddressMode(%s, %s, %s)"),
				*CoerceParameter(CoordinateIndex, MCT_LWCVector2),
				AddressModeToString(StaticAddressX),
				AddressModeToString(StaticAddressY));
			break;
		case 3u:
			NonLWCCoordinateIndex = AddCodeChunkZeroDeriv(UVsType, TEXT("LWCApplyAddressMode(%s, %s, %s, %s)"),
				*CoerceParameter(CoordinateIndex, MCT_LWCVector3),
				AddressModeToString(StaticAddressX),
				AddressModeToString(StaticAddressY),
				AddressModeToString(StaticAddressZ));
			break;
		default:
			checkf(false, TEXT("Invalid number of components %d"), NumComponents);
			break;
		}

		// Explicitly compute the derivatives for LWC UVs
		// This is needed for 100% correct functionality, otherwise filtering seams are possible where there is discontinuity in LWC->float UV conversion
		// This is expensive though, and discontinuities can be minimized by carefully choosing conversion operation
		// Disabled for now, may enable as an option in the future
		const bool bExplicitLWCDerivatives = false;
		if (bExplicitLWCDerivatives && (MipValueMode == TMVM_None || MipValueMode == TMVM_MipBias))
		{
			int32 MipScaleIndex = INDEX_NONE;
			if (MipValueMode == TMVM_MipBias)
			{
				MipScaleIndex = AddCodeChunkZeroDeriv(UVsType, TEXT("exp2(%s)"), *CoerceParameter(MipValue0Index, MCT_Float1));
			}

			MipValue0Index = AddCodeChunkZeroDeriv(UVsType, TEXT("LWCDdx(%s)"), *GetParameterCode(CoordinateIndex));
			MipValue1Index = AddCodeChunkZeroDeriv(UVsType, TEXT("LWCDdy(%s)"), *GetParameterCode(CoordinateIndex));
			if (MipScaleIndex != INDEX_NONE)
			{
				MipValue0Index = Mul(MipValue0Index, MipScaleIndex);
				MipValue1Index = Mul(MipValue1Index, MipScaleIndex);
			}

			MipValueMode = TMVM_Derivative;
		}
	}

	FString SamplerStateCode;
	bool RequiresManualViewMipBias = AutomaticViewMipBias;

	if (!bVirtualTexture) //VT does not have explict samplers (and always requires manual view mip bias)
	{
		if (SamplerSource == SSM_FromTextureAsset)
		{
			SamplerStateCode = FString::Printf(TEXT("%sSampler"), *TextureName);
		}
		else if (SamplerSource == SSM_Wrap_WorldGroupSettings)
		{
			// Use the shared sampler to save sampler slots
			SamplerStateCode = FString::Printf(TEXT("GetMaterialSharedSampler(%sSampler,%s)"),
				*TextureName, AutomaticViewMipBias ? TEXT("View.MaterialTextureBilinearWrapedSampler") : TEXT("Material.Wrap_WorldGroupSettings"));
			RequiresManualViewMipBias = false;
		}
		else if (SamplerSource == SSM_Clamp_WorldGroupSettings)
		{
			// Use the shared sampler to save sampler slots
			SamplerStateCode = FString::Printf(TEXT("GetMaterialSharedSampler(%sSampler,%s)"),
				*TextureName, AutomaticViewMipBias ? TEXT("View.MaterialTextureBilinearClampedSampler") : TEXT("Material.Clamp_WorldGroupSettings"));
			RequiresManualViewMipBias = false;
		}
		else if (SamplerSource == SSM_TerrainWeightmapGroupSettings)
		{
			SamplerStateCode = FString::Printf(TEXT("GetMaterialSharedSampler(%sSampler,%s)"),
				*TextureName, TEXT("View.LandscapeWeightmapSampler"));
			RequiresManualViewMipBias = false;
		}
	}

	const EDerivativeStatus UvDerivativeStatus = GetDerivativeStatus(CoordinateIndex);

	if (RequiresManualViewMipBias)
	{
		if (MipValueMode == TMVM_Derivative)
		{
			// When doing derivative based sampling, multiply.
			int32 Multiplier = AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("View.MaterialTextureDerivativeMultiply"));
			MipValue0Index = Mul(MipValue0Index, Multiplier);
			MipValue1Index = Mul(MipValue1Index, Multiplier);
		}
		else if (MipValue0Index != INDEX_NONE && (MipValueMode == TMVM_MipLevel || MipValueMode == TMVM_MipBias))
		{
			// Adds bias to existing input level bias.
			MipValue0Index = Add(MipValue0Index, AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("View.MaterialTextureMipBias")));
		}
		else
		{
			// Sets bias.
			MipValue0Index = AddInlinedCodeChunkZeroDeriv(MCT_Float1, TEXT("View.MaterialTextureMipBias"));
		}

		// If no Mip mode, then use MipBias.
		MipValueMode = MipValueMode == TMVM_None ? TMVM_MipBias : MipValueMode;
	}

	FString MipValue0Code = TEXT("0.0f");
	FString MipValue1Code = TEXT("0.0f");
	if (MipValue0Index != INDEX_NONE && (MipValueMode == TMVM_MipBias || MipValueMode == TMVM_MipLevel))
	{
		MipValue0Code = CoerceParameter(MipValue0Index, MCT_Float1);
	}
	else if (MipValueMode == TMVM_Derivative)
	{
		MipValue0Code = CoerceParameter(MipValue0Index, UVsType);
		MipValue1Code = CoerceParameter(MipValue1Index, UVsType);
	}

	FString TextureTypeName;
	if (TextureType == MCT_TextureCube)
	{
		TextureTypeName = TEXT("TextureCubeSample");
	}
	else if (TextureType == MCT_Texture2DArray)
	{
		TextureTypeName = TEXT("Texture2DArraySample");
	}
	else if (TextureType == MCT_TextureCubeArray)
	{
		TextureTypeName = TEXT("TextureCubeArraySample");
	}
	else if (TextureType == MCT_VolumeTexture)
	{
		TextureTypeName = TEXT("Texture3DSample");
	}
	else if (TextureType == MCT_TextureExternal)
	{
		TextureTypeName = TEXT("TextureExternalSample");
	}
	else if (bVirtualTexture)
	{
		TextureTypeName = TEXT("TextureVirtualSample");
	}
	else // MCT_Texture2D
	{
		TextureTypeName = TEXT("Texture2DSample");
	}

	const bool bStoreTexCoordScales = ShaderFrequency == SF_Pixel && TextureReferenceIndex != INDEX_NONE;
	const bool bDecal = ShaderFrequency == SF_Pixel && Material->GetMaterialDomain() == MD_DeferredDecal && MipValueMode == TMVM_None;

	if (IsDebugTextureSampleEnabled() && !IsDerivativeValid(UvDerivativeStatus))
	{
		UE_LOG(LogMaterial, Warning, TEXT("Unknown derivatives: '%s'[%s]: %s"), *Material->GetDebugName(), *Material->GetAssetPath().ToString(), *AtParameterCodeChunk(CoordinateIndex).DefinitionAnalytic);
	}

	if (bStoreTexCoordScales)
	{
		AddCodeChunk(MCT_Float, TEXT("MaterialStoreTexCoordScale(Parameters, %s, %d)"), *CoerceParameter(NonLWCCoordinateIndex, UVsType), (int)TextureReferenceIndex);
	}

	const FString UV_Value = CoerceParameter(NonLWCCoordinateIndex, UVsType);
	FString UV_Ddx = TEXT("0.0f");
	FString UV_Ddy = TEXT("0.0f");
	FString UV_Scale = TEXT("1.0f");
	if (IsAnalyticDerivEnabled() && UvDerivativeStatus == EDerivativeStatus::Valid)
	{
		const EMaterialValueType SourceUVsType = GetParameterType(CoordinateIndex);
		const FString UVAnalytic = GetParameterCodeDeriv(CoordinateIndex, CompiledPDV_Analytic);
		UV_Ddx = CoerceValue(UVAnalytic + TEXT(".Ddx"), MakeNonLWCType(SourceUVsType), UVsType); // Ddx/y are never LWC scale
		UV_Ddy = CoerceValue(UVAnalytic + TEXT(".Ddy"), MakeNonLWCType(SourceUVsType), UVsType);
		if (MipValueMode == TMVM_MipBias)
		{
			UV_Scale = FString::Printf(TEXT("exp2(%s)"), *MipValue0Code);
			UV_Ddx = FString::Printf(TEXT("(%s)*exp2(%s)"), *UV_Ddx, *MipValue0Code);
			UV_Ddy = FString::Printf(TEXT("(%s)*exp2(%s)"), *UV_Ddy, *MipValue0Code);
		}
	}

	int32 SamplingCodeIndex = INDEX_NONE;
	if (bVirtualTexture)
	{
		int32 VTStackIndex = INDEX_NONE;
		int32 VTLayerIndex = INDEX_NONE;
		int32 VTPageTableIndex = INDEX_NONE;

		check(VirtualTextureIndex >= 0);

		// Only support GPU feedback from pixel shader
		//todo[vt]: Support feedback from other shader types
		const bool bGenerateFeedback = EnableFeedback && ShaderFrequency == SF_Pixel;

		VTLayerIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Virtual][VirtualTextureIndex]->GetTextureLayerIndex();
		if (VTLayerIndex != INDEX_NONE)
		{
			// The layer index in the virtual texture stack is already known
			// Create a page table sample for each new combination of virtual texture and sample parameters
			VTStackIndex = AcquireVTStackIndex(MipValueMode, StaticAddressX, StaticAddressY, 1.0f, NonLWCCoordinateIndex, MipValue0Index, MipValue1Index, TextureReferenceIndex, UV_Value, UV_Ddx, UV_Ddy, AdaptiveVirtualTexture, bGenerateFeedback);
			VTPageTableIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Virtual][VirtualTextureIndex]->GetPageTableLayerIndex();
		}
		else
		{
			// Textures can only be combined in a VT stack if they have the same aspect ratio
			// This also means that any texture parameters set in material instances for VTs must match the aspect ratio of the texture in the parent material
			// (Otherwise could potentially break stacks)
			check(Texture);

			// Using Source size because we care about the aspect ratio of each block (each block of multi-block texture must have same aspect ratio)
			// We can still combine multi-block textures of different block aspect ratios, as long as each block has the same ratio
			// This is because we only need to overlay VT pages from within a given block
			const float TextureAspectRatio = (float)Texture->Source.GetSizeX() / (float)Texture->Source.GetSizeY();

			// Create a page table sample for each new set of sample parameters
			VTStackIndex = AcquireVTStackIndex(MipValueMode, StaticAddressX, StaticAddressY, TextureAspectRatio, NonLWCCoordinateIndex, MipValue0Index, MipValue1Index, INDEX_NONE, UV_Value, UV_Ddx, UV_Ddy, AdaptiveVirtualTexture, bGenerateFeedback);
			// Allocate a layer in the virtual texture stack for this physical sample
			VTLayerIndex = MaterialCompilationOutput.UniformExpressionSet.VTStacks[VTStackIndex].AddLayer();
			VTPageTableIndex = VTLayerIndex;
		}

		MaterialCompilationOutput.UniformExpressionSet.VTStacks[VTStackIndex].SetLayer(VTLayerIndex, VirtualTextureIndex);

		// VT MipValueMode logic (most of work for VT case is in page table lookup)
		if (MipValueMode == TMVM_MipLevel)
		{
			TextureTypeName += TEXT("Level");
		}

		const FMaterialVTStackEntry& VTStackEntry = VTStacks[VTStackIndex];
		const FString VTPageTableResult_Finite = GetParameterCode(VTStackEntry.CodeIndex);

		// 'Texture name/sampler', 'PageTableResult', 'LayerIndex', 'PackedUniform'
		FString SampleCodeFinite = FString::Printf(TEXT("%s(%s, %s, %d, VTUniform_Unpack(Material.VTPackedUniform[%d]))"), *TextureTypeName, *TextureName, *VTPageTableResult_Finite, VTPageTableIndex, VirtualTextureIndex);
		SampleCodeFinite = ApplySamplerType(SampleCodeFinite, SamplerType);

		if (IsAnalyticDerivEnabled() && IsDerivativeValid(UvDerivativeStatus))
		{
			const FString VTPageTableResult_Analytic = GetParameterCodeDeriv(VTStackEntry.CodeIndex, CompiledPDV_Analytic);
			// 'Texture name/sampler', 'PageTableResult', 'LayerIndex', 'PackedUniform'
			FString SampleCodeAnalytic;

			if (SamplerDebugSupported(TextureType, bVirtualTexture, bDecal))
			{
				SampleCodeAnalytic = FString::Printf(TEXT("Debug%s(%s, %s, %d, VTUniform_Unpack(Material.VTPackedUniform[%d]), %s)"), *TextureTypeName, *TextureName, *VTPageTableResult_Analytic, VTPageTableIndex, VirtualTextureIndex, *UV_Scale);
			}
			else
			{
				SampleCodeAnalytic = FString::Printf(TEXT("%s(%s, %s, %d, VTUniform_Unpack(Material.VTPackedUniform[%d]))"), *TextureTypeName, *TextureName, *VTPageTableResult_Analytic, VTPageTableIndex, VirtualTextureIndex);
			}

			SampleCodeAnalytic = ApplySamplerType(SampleCodeAnalytic, SamplerType);
			SamplingCodeIndex = AddCodeChunkInnerDeriv(*SampleCodeFinite, *SampleCodeAnalytic, MCT_Float4, false, EDerivativeStatus::NotValid);
		}
		else
		{
			SamplingCodeIndex = AddCodeChunk(MCT_Float4, *SampleCodeFinite, *TextureName, *VTPageTableResult_Finite, VTPageTableIndex, VirtualTextureIndex);
		}
	
		const bool bStoreVTSampleInfo = ShaderFrequency == SF_Pixel && TextureReferenceIndex != INDEX_NONE;
		if (bStoreVTSampleInfo)
		{
			AddCodeChunk(MCT_Float, TEXT("MaterialStoreVTSampleInfo(Parameters.TexCoordScalesParams, %s, %d, %d)"), *VTPageTableResult_Finite, VTPageTableIndex, TextureReferenceIndex);
		}
	}
	else
	{
		// Non-VT MipValueMode logic
		// 
		// Re-route decal texture sampling so platforms may add specific workarounds there
		if (bDecal)
		{
			TextureTypeName += TEXT("_Decal");
		}

		FString SampleCodeFinite;
		if (MipValueMode == TMVM_None)
		{
			SampleCodeFinite = FString::Printf(TEXT("%s(%s,%s,%s)"), *TextureTypeName, *TextureName, *SamplerStateCode, *UV_Value);
		}
		else if (MipValueMode == TMVM_MipLevel)
		{
			SampleCodeFinite = FString::Printf(TEXT("%sLevel(%s,%s,%s,%s)"), *TextureTypeName, *TextureName, *SamplerStateCode, *UV_Value, *MipValue0Code);
		}
		else if (MipValueMode == TMVM_MipBias)
		{
			SampleCodeFinite = FString::Printf(TEXT("%sBias(%s,%s,%s,%s)"), *TextureTypeName, *TextureName, *SamplerStateCode, *UV_Value, *MipValue0Code);
		}
		else if (MipValueMode == TMVM_Derivative)
		{
			SampleCodeFinite = FString::Printf(TEXT("%sGrad(%s,%s,%s,%s,%s)"), *TextureTypeName, *TextureName, *SamplerStateCode, *UV_Value, *MipValue0Code, *MipValue1Code);
		}
		else
		{
			check(0);
		}

		SampleCodeFinite = ApplySamplerType(SampleCodeFinite, SamplerType);

		if (IsAnalyticDerivEnabled() && IsDerivativeValid(UvDerivativeStatus) && !bDecal)
		{
			FString SampleCodeAnalytic = SampleCodeFinite;
			if (MipValueMode == TMVM_None || MipValueMode == TMVM_MipBias)
			{
				
				if (SamplerDebugSupported(TextureType, bVirtualTexture, bDecal))
					SampleCodeAnalytic = FString::Printf(TEXT("Debug%sGrad(%s,%s,%s,%s,%s,%s)"), *TextureTypeName, *TextureName, *SamplerStateCode, *UV_Value, *UV_Ddx, *UV_Ddy, *UV_Scale);
				else
					SampleCodeAnalytic = FString::Printf(TEXT("%sGrad(%s,%s,%s,%s,%s)"), *TextureTypeName, *TextureName, *SamplerStateCode, *UV_Value, *UV_Ddx, *UV_Ddy);

				SampleCodeAnalytic = ApplySamplerType(SampleCodeAnalytic, SamplerType);
			}

			SamplingCodeIndex = AddCodeChunkInnerDeriv(*SampleCodeFinite, *SampleCodeAnalytic, MCT_Float4, false /*?*/, EDerivativeStatus::NotValid);
		}
		else
		{
			SamplingCodeIndex = AddCodeChunk(MCT_Float4, *SampleCodeFinite);
		}
	}

	AddEstimatedTextureSample();
	if (bStoreTexCoordScales)
	{
		FString SamplingCode = CoerceParameter(SamplingCodeIndex, MCT_Float4);
		AddCodeChunk(MCT_Float, TEXT("MaterialStoreTexSample(Parameters, %s, %d)"), *SamplingCode, (int)TextureReferenceIndex);
	}

	return SamplingCodeIndex;
}

int32 FHLSLMaterialTranslator::TextureProperty(int32 TextureIndex, EMaterialExposedTextureProperty Property)
{
	const EMaterialValueType TextureType = GetParameterType(TextureIndex);
	if (TextureType != MCT_Texture2D &&
		TextureType != MCT_TextureVirtual &&
		TextureType != MCT_VolumeTexture &&
		TextureType != MCT_Texture2DArray)
	{
		return Errorf(TEXT("Texture size only available for Texture2D, TextureVirtual, Texture2DArray, and VolumeTexture, not %s"), DescribeType(TextureType));
	}

	FMaterialUniformExpressionTexture* TextureExpression = (*CurrentScopeChunks)[TextureIndex].UniformExpression->GetTextureUniformExpression();
	if (!TextureExpression)
	{
		return Errorf(TEXT("Expected a texture expression"));
	}

	const EMaterialValueType ValueType = (TextureType == MCT_VolumeTexture || TextureType == MCT_Texture2DArray) ? MCT_Float3 : MCT_Float2;
	return AddUniformExpression(new FMaterialUniformExpressionTextureProperty(TextureExpression, Property), ValueType, TEXT(""));
}

int32 FHLSLMaterialTranslator::TextureDecalMipmapLevel(int32 TextureSizeInput)
{
	if (Material->GetMaterialDomain() != MD_DeferredDecal)
	{
		return Errorf(TEXT("Decal mipmap level only available in the decal material domain."));
	}

	EMaterialValueType TextureSizeType = GetParameterType(TextureSizeInput);

	if (TextureSizeType != MCT_Float2)
	{
		Errorf(TEXT("Unmatching conversion %s -> float2"), DescribeType(TextureSizeType));
		return INDEX_NONE;
	}

	FString TextureSize = CoerceParameter(TextureSizeInput, MCT_Float2);

	return AddCodeChunk(
		MCT_Float1,
		TEXT("ComputeDecalMipmapLevel(Parameters,%s)"),
		*TextureSize
		);
}

int32 FHLSLMaterialTranslator::TextureDecalDerivative(bool bDDY)
{
	if (Material->GetMaterialDomain() != MD_DeferredDecal)
	{
		return Errorf(TEXT("Decal derivatives only available in the decal material domain."));
	}

	return AddCodeChunk(
		MCT_Float2,
		bDDY ? TEXT("ComputeDecalDDY(Parameters)") : TEXT("ComputeDecalDDX(Parameters)")
		);
}

int32 FHLSLMaterialTranslator::DecalColor()
{
	if (Material->GetMaterialDomain() != MD_DeferredDecal)
	{
		return Errorf(TEXT("Decal color is only available in the decal material domain."));
	}

	if (ShaderFrequency != SF_Pixel)
	{
		return Errorf(TEXT("Decal color is only available in the pixel shader."));
	}

	return AddCodeChunkZeroDeriv(
		MCT_Float4,
		TEXT("DecalColor()")
	);
}

int32 FHLSLMaterialTranslator::DecalLifetimeOpacity()
{
	if (Material->GetMaterialDomain() != MD_DeferredDecal)
	{
		return Errorf(TEXT("Decal lifetime fade is only available in the decal material domain."));
	}

	if (ShaderFrequency != SF_Pixel)
	{
		return Errorf(TEXT("Decal lifetime fade is only available in the pixel shader."));
	}

	return AddCodeChunkZeroDeriv(
		MCT_Float,
		TEXT("DecalLifetimeOpacity()")
		);
}

int32 FHLSLMaterialTranslator::PixelDepth()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex)
	{
		return Errorf(TEXT("Invalid node used in hull/domain shader input!"));
	}

	FString FiniteCode = TEXT("GetPixelDepth(Parameters)");
	if (IsAnalyticDerivEnabled())
	{
		FString AnalyticCode = DerivativeAutogen.ConstructDeriv(TEXT("Parameters.ScreenPosition.w"), TEXT("Parameters.ScreenPosition_DDX.w"), TEXT("Parameters.ScreenPosition_DDY.w"), EDerivativeType::Float1);
		return AddCodeChunkInnerDeriv(*FiniteCode, *AnalyticCode, MCT_Float, false, EDerivativeStatus::Valid);
	}
	else
	{
		return AddInlinedCodeChunk(MCT_Float, *FiniteCode);
	}
}

/** Calculate screen aligned UV coordinates from an offset fraction or texture coordinate */
int32 FHLSLMaterialTranslator::GetScreenAlignedUV(int32 Offset, int32 ViewportUV, bool bUseOffset)
{
	if(bUseOffset)
	{
		return AddCodeChunk(MCT_Float2, TEXT("CalcScreenUVFromOffsetFraction(GetScreenPosition(Parameters), %s)"), *GetParameterCode(Offset));
	}
	else if (ViewportUV != INDEX_NONE)
	{
		int32 BufferUV = AddCodeChunk(MCT_Float2, TEXT("MaterialFloat2(ViewportUVToBufferUV(%s))"), *CoerceParameter(ViewportUV, MCT_Float2));

		EMaterialDomain MaterialDomain = Material->GetMaterialDomain();
		int32 Min = AddInlinedCodeChunkZeroDeriv(MCT_Float2, MaterialDomain == MD_Surface ? TEXT("ResolvedView.BufferBilinearUVMinMax.xy") : TEXT("View.BufferBilinearUVMinMax.xy"));
		int32 Max = AddInlinedCodeChunkZeroDeriv(MCT_Float2, MaterialDomain == MD_Surface ? TEXT("ResolvedView.BufferBilinearUVMinMax.zw") : TEXT("View.BufferBilinearUVMinMax.zw"));
		return Clamp(BufferUV, Min, Max);
	}
	else
	{
		return AddInlinedCodeChunk(MCT_Float2, TEXT("ScreenAlignedPosition(GetScreenPosition(Parameters))"));
	}
}

int32 FHLSLMaterialTranslator::SceneDepth(int32 Offset, int32 ViewportUV, bool bUseOffset)
{
	if (ShaderFrequency == SF_Vertex && FeatureLevel <= ERHIFeatureLevel::ES3_1)
	{
		// mobile currently does not support this, we need to read a separate copy of the depth, we must disable framebuffer fetch and force scene texture reads.
		return Errorf(TEXT("Cannot read scene depth from the vertex shader with feature level ES3.1 or below."));
	}

	if (Material->IsTranslucencyWritingVelocity())
	{
		return Errorf(TEXT("Translucenct material with 'Output Velocity' enabled will write to depth buffer, therefore cannot read from depth buffer at the same time."));
	}

	if (Offset == INDEX_NONE && bUseOffset)
	{
		return INDEX_NONE;
	}

	bUsesSceneDepth = true;
	AddEstimatedTextureSample();

	FString	UserDepthCode(TEXT("CalcSceneDepth(%s)"));
	int32 TexCoordCode = GetScreenAlignedUV(Offset, ViewportUV, bUseOffset);
	// add the code string
	return AddCodeChunk(
		MCT_Float,
		*UserDepthCode,
		*GetParameterCode(TexCoordCode)
		);
}

// @param SceneTextureId of type ESceneTextureId e.g. PPI_SubsurfaceColor
int32 FHLSLMaterialTranslator::SceneTextureLookup(int32 ViewportUV, uint32 InSceneTextureId, bool bFiltered)
{
	ESceneTextureId SceneTextureId = (ESceneTextureId)InSceneTextureId;

	// Guard against using unsupported textures with SLW
	const bool bHasSingleLayerWaterSM = GetMaterialShadingModels().HasShadingModel(MSM_SingleLayerWater);
	if (bHasSingleLayerWaterSM && SceneTextureId != PPI_CustomDepth && SceneTextureId != PPI_CustomStencil)
	{
		return Error(TEXT("Only custom depth and custom stencil can be sampled with SceneTexture when used with the Single Layer Water shading model."));
	}
	
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Vertex)
	{
		// we can relax this later if needed
		return NonPixelShaderExpressionError();
	}

	if (SceneTextureId == PPI_DecalMask)
	{
		return Error(TEXT("Decal Mask bit was moved from GBuffer to the Stencil Buffer for performance optimisation so therefore no longer available."));
	}

	UseSceneTextureId(SceneTextureId, true);

	int32 BufferUV;
	if (ViewportUV != INDEX_NONE)
	{
		BufferUV = AddCodeChunk(MCT_Float2,
			TEXT("ClampSceneTextureUV(ViewportUVToSceneTextureUV(%s, %d), %d)"),
			*CoerceParameter(ViewportUV, MCT_Float2), (int)SceneTextureId, (int)SceneTextureId);
	}
	else
	{
		BufferUV = AddInlinedCodeChunk(MCT_Float2, TEXT("GetDefaultSceneTextureUV(Parameters, %d)"), (int)SceneTextureId);
	}

	AddEstimatedTextureSample();

	int32 LookUp = INDEX_NONE;

	if (FeatureLevel >= ERHIFeatureLevel::SM5)
	{
		LookUp = AddCodeChunk(
			MCT_Float4,
			TEXT("SceneTextureLookup(%s, %d, %s)"),
			*CoerceParameter(BufferUV, MCT_Float2), (int)SceneTextureId, bFiltered ? TEXT("true") : TEXT("false")
		);
	}
	else // mobile
	{
		LookUp = AddCodeChunk(MCT_Float4, TEXT("MobileSceneTextureLookup(Parameters, %d, %s)"), (int32)SceneTextureId, *CoerceParameter(BufferUV, MCT_Float2));
	}

	// Strata only
	// When SceneTexture lookup node is used, single/simple paths are disabled to ensure texture decoding is properly handled.
	// Reading SceneTexture, when Strata is enabled, implies unpacking material buffer data. The unpacking function exists in different 'flavor' 
	// for optimization purpose (simple/single/complex). To avoid compiling out single or complex unpacking paths (due to defines set by analyzing 
	// the current shader, vs. scene texture pixels), we force Simple/Single versions to be disabled
	bStrataMaterialIsSimple = false;
	bStrataMaterialIsSingle = false;
	
	if (SceneTextureId == PPI_PostProcessInput0 && Material->GetMaterialDomain() == MD_PostProcess && Material->GetBlendableLocation() != BL_AfterTonemapping)
	{
		return AddInlinedCodeChunk(MCT_Float4, TEXT("(float4(View.OneOverPreExposure.xxx, 1) * %s)"), *CoerceParameter(LookUp, MCT_Float4));
	}
	else
	{
		return LookUp;
	}
}

int32 FHLSLMaterialTranslator::GetSceneTextureViewSize(int32 SceneTextureId, bool InvProperty)
{
	if (InvProperty)
	{
		return AddCodeChunkZeroDeriv(MCT_Float2, TEXT("GetSceneTextureViewSize(%d).zw"), SceneTextureId);
	}
	return AddCodeChunkZeroDeriv(MCT_Float2, TEXT("GetSceneTextureViewSize(%d).xy"), SceneTextureId);
}

// @param bTextureLookup true: texture, false:no texture lookup, usually to get the size
void FHLSLMaterialTranslator::UseSceneTextureId(ESceneTextureId SceneTextureId, bool bTextureLookup)
{
	MaterialCompilationOutput.bNeedsSceneTextures = true;
	MaterialCompilationOutput.SetIsSceneTextureUsed(SceneTextureId);

	if (Material->GetMaterialDomain() == MD_DeferredDecal)
	{
		const bool bSceneTextureSupportsDecal = SceneTextureId == PPI_SceneDepth || SceneTextureId == PPI_WorldNormal || SceneTextureId == PPI_CustomDepth || SceneTextureId == PPI_CustomStencil;
		if (!bSceneTextureSupportsDecal)
		{
			// Note: For DBuffer decals CustomDepth and CustomStencil are not available if r.CustomDepth.Order = 1
			Errorf(TEXT("Decals can only access SceneDepth, CustomDepth, CustomStencil, and WorldNormal."));
		}

		const bool bSceneTextureRequiresSM5 = SceneTextureId == PPI_WorldNormal;
		if (bSceneTextureRequiresSM5)
		{
			ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5);
		}

		if (SceneTextureId == PPI_WorldNormal && Material->HasNormalConnected() && !IsUsingDBuffers(Platform))
		{
			// GBuffer decals can't bind Normal for read and write.
			// Note: DBuffer decals can support this but only if the sampled WorldNormal isn't connected to the output normal.
			Errorf(TEXT("Decals that read WorldNormal cannot output to normal at the same time. Enable DBuffer to support this."));
		}
	}

	if(SceneTextureId == PPI_SceneColor && Material->GetMaterialDomain() != MD_Surface)
	{
		if(Material->GetMaterialDomain() == MD_PostProcess)
		{
			Errorf(TEXT("SceneColor lookups are only available when MaterialDomain = Surface. PostProcessMaterials should use the SceneTexture PostProcessInput0."));
		}
		else
		{
			Errorf(TEXT("SceneColor lookups are only available when MaterialDomain = Surface."));
		}
	}

	if(bTextureLookup)
	{
		bNeedsSceneTexturePostProcessInputs = bNeedsSceneTexturePostProcessInputs
			|| ((SceneTextureId >= PPI_PostProcessInput0 && SceneTextureId <= PPI_PostProcessInput6)
			|| SceneTextureId == PPI_Velocity
			|| SceneTextureId == PPI_SceneColor);

	}

	if (SceneTextureId == PPI_SceneDepth && bTextureLookup)
	{
		bUsesSceneDepth = true;
	}

	const bool bNeedsGBuffer = MaterialCompilationOutput.NeedsGBuffer();

	if (bNeedsGBuffer)
	{
		if (IsForwardShadingEnabled(Platform) || (IsMobilePlatform(Platform) && !IsMobileDeferredShadingEnabled(Platform)))
		{
			Errorf(TEXT("GBuffer scene textures not available with forward shading (platform id %d)."), Platform);
		}
		
		// Post-process can't access memoryless GBuffer on mobile
		if (IsMobilePlatform(Platform))
		{
			if (Material->GetMaterialDomain() == MD_PostProcess)
			{
				Errorf(TEXT("GBuffer scene textures not available in post-processing with mobile shading (platform id %d)."), Platform);
			}

			if (Material->IsMobileSeparateTranslucencyEnabled())
			{
				Errorf(TEXT("GBuffer scene textures not available for separate translucency with mobile shading (platform id %d)."), Platform);
			}
		}
	}

	if (SceneTextureId == PPI_Velocity)
	{
		if (Material->GetMaterialDomain() != MD_PostProcess)
		{
			Errorf(TEXT("Velocity scene textures are only available in post process materials."));
		}
	}

	// not yet tracked:
	//   PPI_SeparateTranslucency, PPI_CustomDepth, PPI_AmbientOcclusion
}

int32 FHLSLMaterialTranslator::SceneColor(int32 Offset, int32 ViewportUV, bool bUseOffset)
{
	if (Offset == INDEX_NONE && bUseOffset)
	{
		return INDEX_NONE;
	}

	if (ShaderFrequency != SF_Pixel)
	{
		return NonPixelShaderExpressionError();
	}

	if(Material->GetMaterialDomain() != MD_Surface)
	{
		Errorf(TEXT("SceneColor lookups are only available when MaterialDomain = Surface."));
	}

	if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	MaterialCompilationOutput.SetIsSceneTextureUsed(PPI_SceneColor);
	AddEstimatedTextureSample();

	int32 ScreenUVCode = GetScreenAlignedUV(Offset, ViewportUV, bUseOffset);
	return AddCodeChunk(
		MCT_Float3,
		TEXT("DecodeSceneColorForMaterialNode(%s)"),
		*GetParameterCode(ScreenUVCode)
		);
}

int32 FHLSLMaterialTranslator::DBufferTextureLookup(int32 ViewportUV, uint32 DBufferTextureIndex)
{
	if (Material->GetMaterialDomain() != MD_Surface || IsTranslucentBlendMode(Material->GetBlendMode()))
	{
		Errorf(TEXT("DBuffer scene textures are only available on opaque or masked surfaces."));
	}

	int32 BufferUV = INDEX_NONE;
	if (ViewportUV != INDEX_NONE)
	{
		BufferUV = AddCodeChunk(MCT_Float2,	TEXT("ClampSceneTextureUV(ViewportUVToBufferUV(%s), 0)"), *CoerceParameter(ViewportUV, MCT_Float2));
	}
	else
	{
		BufferUV = AddInlinedCodeChunk(MCT_Float2, TEXT("GetDefaultSceneTextureUV(Parameters, 0)"));
	}

	MaterialCompilationOutput.SetIsDBufferTextureUsed(DBufferTextureIndex);
	AddEstimatedTextureSample();

	return AddCodeChunk(MCT_Float4, TEXT("MaterialExpressionDBufferTextureLookup(Parameters, %s, %d)"), *CoerceParameter(BufferUV, MCT_Float2), (int)DBufferTextureIndex);
}

int32 FHLSLMaterialTranslator::Switch(int32 SwitchValueInput, int32 DefaultInput, TArray<int32>& CompiledInputs)
{
	if (SwitchValueInput == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (DefaultInput == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (CompiledInputs.Num() == 0)
	{
		return INDEX_NONE;
	}

	for (int32 Input : CompiledInputs)
	{
		if (Input == INDEX_NONE)
		{
			return INDEX_NONE;
		}
	}

	// If our selector is constant, we can skip the generation of code below and simply
    // grab/return the appropriate input directly. This should greatly simplify the
    // expression in constant parameter cases.
	FMaterialUniformExpression* ExpressionSwitchValue = GetParameterUniformExpression(SwitchValueInput);
	EMaterialValueType SwitchValueType = GetParameterType(SwitchValueInput);
	if (SwitchValueType == MCT_Float && ExpressionSwitchValue && ExpressionSwitchValue->IsConstant())
	{		
		FLinearColor Value;
		FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
		ExpressionSwitchValue->GetNumberValue(DummyContext, Value);

		for (int32 InputIndex = 0; InputIndex < CompiledInputs.Num(); ++InputIndex)
		{
			if (Value.R >= InputIndex && Value.R < InputIndex + 1)
			{
				return CompiledInputs[InputIndex];
			}
		}
		return DefaultInput;
	}
	
	EMaterialValueType ResultType = GetParameterType(CompiledInputs[0]);
	for (int32 Input : CompiledInputs)
	{
		ResultType = GetArithmeticResultType(ResultType, GetParameterType(Input));
	}

	FString SwitchCode = TEXT("");
	FString SwitchValueParameter = CoerceParameter(SwitchValueInput, MCT_Float);
	FString DefaultParameter = CoerceParameter(DefaultInput, ResultType);


	// TODO: The following represents two potential implementations for the dynamic switch logic in HLSL.
	// The first option is an expression that mathamatically cancels out any "unmatched" case,
	// leaving only the desired value passing through. This approach works, but is less optimal 
	// due to needing to evalulate every potential case value as part of the process.
	// The second option is potentially more optimal, as it uses a native switch statement in HLSL, something
	// that the compiler should be able to optimize more easily with less unneeded evaluations. However,
	// this approach currently doesn't work well with the existing material translation code in the engine.
	// The existing design supports expressions returning floating point type values, which are used by
	// the analyical derivative components elsewhere. Attempting to return a void type or insert type-less
	// statements into the generated code has been problematic.
	//
	// Ideally, the second option can be made to function correctly and replace the first, which is why
	// it's being left in as a preprocessor switch for the time being. However, for expedieceny, we want
	//to use the first option for the time being, to provide a working, if less efficicent, switch implementation.
#if 1
	// We form the "switch" statement from an equivalent math expression,
    // using step functions and additions to select between inputs.
	for (int32 InputIndex = 0; InputIndex < CompiledInputs.Num(); ++InputIndex)
	{
		FString InputParameter = CoerceParameter(CompiledInputs[InputIndex], ResultType);
		SwitchCode += FString::Printf(TEXT("step(%f, floor(%s) + 0.5f) * step(floor(%s) + 0.5f, %f) * %s +"),
			float(InputIndex), *SwitchValueParameter,        // If greater than the lower bound
			*SwitchValueParameter, float(InputIndex) + 1.0f, // and less  than the upper bound parameters
			*InputParameter);                             // return the selected selected input
	}
	SwitchCode += FString::Printf(TEXT("(step(floor(%s) + 0.5f, %f) + step(%f, floor(%s) + 0.5f)) * %s"),
	    *SwitchValueParameter, float(0),                    // If less than Zero,
		float(CompiledInputs.Num()), *SwitchValueParameter, // and greater than number of inputs,
		*DefaultParameter);                              // return the default
	return AddCodeChunk(ResultType, *SwitchCode);
#else
	int32 ReturnValue = AddCodeChunk(ResultType, TEXT("0"));
	FString ReturnParameter = CoerceParameter(ReturnValue, ResultType);

	SwitchCode += FString::Printf(TEXT("switch(%s){"), *SwitchValueParameter);
	for (int32 InputIndex = 0; InputIndex < CompiledInputs.Num(); ++InputIndex)
	{
		FString InputParameter = CoerceParameter(CompiledInputs[InputIndex], ResultType);
		SwitchCode += FString::Printf(TEXT("case %d: %s = %s; break;"), InputIndex, *ReturnParameter, *InputParameter);
	}
	SwitchCode += FString::Printf(TEXT("default: %s = %s; }"), *ReturnParameter, *DefaultParameter);
	
	return AddCodeChunkInner(MCT_VoidStatement, EDerivativeStatus::NotAware, true, *SwitchCode);
#endif
}

int32 FHLSLMaterialTranslator::Texture(UTexture* InTexture, int32& TextureReferenceIndex, EMaterialSamplerType SamplerType, ESamplerSourceMode SamplerSource, ETextureMipValueMode MipValueMode)
{
	EMaterialValueType ShaderType = InTexture->GetMaterialType();
	TextureReferenceIndex = Material->GetReferencedTextures().Find(InTexture);

#if DO_CHECK
	// UE-3518: Additional pre-assert logging to help determine the cause of this failure.
	if (TextureReferenceIndex == INDEX_NONE)
	{
		const auto ReferencedTextures = Material->GetReferencedTextures();
		UE_LOG(LogMaterial, Error, TEXT("Compiler->Texture() failed to find texture '%s' in referenced list of size '%i':"), *InTexture->GetName(), ReferencedTextures.Num());
		for (int32 i = 0; i < ReferencedTextures.Num(); ++i)
		{
			UE_LOG(LogMaterial, Error, TEXT("%i: '%s'"), i, ReferencedTextures[i] ? *ReferencedTextures[i]->GetName() : TEXT("nullptr"));
		}
	}
#endif
	checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->Texture() without implementing UMaterialExpression::GetReferencedTexture properly"));

	const bool bVirtualTexturesEnabeled = UseVirtualTexturing(FeatureLevel, TargetPlatform);
	bool bVirtual = ShaderType == MCT_TextureVirtual;
	if (bVirtualTexturesEnabeled == false && ShaderType == MCT_TextureVirtual)
	{
		bVirtual = false;
		ShaderType = MCT_Texture2D;
	}
	return AddUniformExpression(new FMaterialUniformExpressionTexture(TextureReferenceIndex, SamplerType, SamplerSource, bVirtual),ShaderType,TEXT(""));
}

int32 FHLSLMaterialTranslator::TextureParameter(FName ParameterName, UTexture* InDefaultValue, int32& TextureReferenceIndex, EMaterialSamplerType SamplerType, ESamplerSourceMode SamplerSource)
{
	UTexture* DefaultValue = InDefaultValue;

	// If we're compiling a function, give the function a chance to override the default parameter value
	FMaterialParameterMetadata Meta;
	if (GetParameterOverrideValueForCurrentFunction(EMaterialParameterType::Texture, ParameterName, Meta))
	{
		DefaultValue = Meta.Value.Texture;
	}

	EMaterialValueType ShaderType = DefaultValue->GetMaterialType();
	TextureReferenceIndex = Material->GetReferencedTextures().Find(DefaultValue);
	if (TextureReferenceIndex == INDEX_NONE)
	{
		FString TextureName;
		DefaultValue->GetName(TextureName);
		Errorf(TEXT("Could not resolve referenced texture '%s'."), *TextureName);
		return INDEX_NONE;
	}

	FMaterialParameterInfo ParameterInfo = GetParameterAssociationInfo();
	ParameterInfo.Name = ParameterName;

	const bool bVirtualTexturesEnabled = UseVirtualTexturing(FeatureLevel, TargetPlatform);
	bool bVirtual = ShaderType == MCT_TextureVirtual;
	if (bVirtualTexturesEnabled == false && ShaderType == MCT_TextureVirtual)
	{
		bVirtual = false;
		ShaderType = MCT_Texture2D;
	}
	return AddUniformExpression(new FMaterialUniformExpressionTextureParameter(ParameterInfo, TextureReferenceIndex, SamplerType, SamplerSource, bVirtual),ShaderType,TEXT(""));
}

int32 FHLSLMaterialTranslator::VirtualTexture(URuntimeVirtualTexture* InTexture, int32 TextureLayerIndex, int32 PageTableLayerIndex, int32& TextureReferenceIndex, EMaterialSamplerType SamplerType) 
{
	if (!UseVirtualTexturing(FeatureLevel, TargetPlatform))
	{
		return INDEX_NONE;
	}

	TextureReferenceIndex = Material->GetReferencedTextures().Find(InTexture);
	checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->VirtualTexture() without implementing UMaterialExpression::GetReferencedTexture properly"));

	return AddUniformExpression(new FMaterialUniformExpressionTexture(TextureReferenceIndex, TextureLayerIndex, PageTableLayerIndex, SamplerType), MCT_TextureVirtual, TEXT(""));
}

int32 FHLSLMaterialTranslator::VirtualTextureParameter(FName ParameterName, URuntimeVirtualTexture* InDefaultValue, int32 TextureLayerIndex, int32 PageTableLayerIndex, int32& TextureReferenceIndex, EMaterialSamplerType SamplerType)
{
	if (!UseVirtualTexturing(FeatureLevel, TargetPlatform))
	{
		return INDEX_NONE;
	}

	URuntimeVirtualTexture* DefaultValue = InDefaultValue;

	// If we're compiling a function, give the function a chance to override the default parameter value
	FMaterialParameterMetadata Meta;
	if (GetParameterOverrideValueForCurrentFunction(EMaterialParameterType::RuntimeVirtualTexture, ParameterName, Meta))
	{
		DefaultValue = Meta.Value.RuntimeVirtualTexture;
	}

	TextureReferenceIndex = Material->GetReferencedTextures().Find(DefaultValue);
	checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->VirtualTexture() without implementing UMaterialExpression::GetReferencedTexture properly"));

	FMaterialParameterInfo ParameterInfo = GetParameterAssociationInfo();
	ParameterInfo.Name = ParameterName;

	return AddUniformExpression(new FMaterialUniformExpressionTextureParameter(ParameterInfo, TextureReferenceIndex, TextureLayerIndex, PageTableLayerIndex, SamplerType), MCT_TextureVirtual, TEXT(""));
}

int32 FHLSLMaterialTranslator::VirtualTextureUniform(int32 TextureIndex, int32 VectorIndex, UE::Shader::EValueType Type)
{
	return AddUniformExpression(new FMaterialUniformExpressionRuntimeVirtualTextureUniform(TextureIndex, VectorIndex), GetMaterialValueType(Type), TEXT(""));
}

int32 FHLSLMaterialTranslator::VirtualTextureUniform(FName ParameterName, int32 TextureIndex, int32 VectorIndex, UE::Shader::EValueType Type)
{
	FMaterialParameterInfo ParameterInfo = GetParameterAssociationInfo();
	ParameterInfo.Name = ParameterName;

	return AddUniformExpression(new FMaterialUniformExpressionRuntimeVirtualTextureUniform(ParameterInfo, TextureIndex, VectorIndex), GetMaterialValueType(Type), TEXT(""));
}

int32 FHLSLMaterialTranslator::VirtualTextureWorldToUV(int32 WorldPositionIndex, int32 P0, int32 P1, int32 P2)
{
	if (!UseVirtualTexturing(FeatureLevel, TargetPlatform) || WorldPositionIndex == INDEX_NONE || P0 == INDEX_NONE || P1 == INDEX_NONE || P2 == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const EDerivativeStatus WorldPositionDerivStatus = GetDerivativeStatus(WorldPositionIndex);

	FString CodeFinite = FString::Printf(TEXT("VirtualTextureWorldToUV(%s, %s, %s, %s)"), *CoerceParameter(WorldPositionIndex, MCT_LWCVector3), *GetParameterCode(P0), *GetParameterCode(P1), *GetParameterCode(P2));
	if (IsAnalyticDerivEnabled() && IsDerivativeValid(WorldPositionDerivStatus))
	{
		if (WorldPositionDerivStatus == EDerivativeStatus::Valid)
		{
			WorldPositionIndex = ValidCast(WorldPositionIndex, MCT_LWCVector3);
			FString WorldPositionDeriv = GetParameterCodeDeriv(WorldPositionIndex, CompiledPDV_Analytic);
			FString CodeAnalytic = FString::Printf(TEXT("VirtualTextureWorldToUVDeriv(%s, %s, %s, %s)"), *WorldPositionDeriv, *GetParameterCode(P0), *GetParameterCode(P1), *GetParameterCode(P2));
			return AddCodeChunkInnerDeriv(*CodeFinite, *CodeAnalytic, MCT_Float2, false, EDerivativeStatus::Valid);
		}
		else
		{
			return AddInlinedCodeChunkZeroDeriv(MCT_Float2, *CodeFinite);
		}
	}
	else
	{
		return AddInlinedCodeChunk(MCT_Float2, *CodeFinite);
	}
}

int32 FHLSLMaterialTranslator::VirtualTextureUnpack(int32 CodeIndex0, int32 CodeIndex1, int32 CodeIndex2, int32 P0, EVirtualTextureUnpackType UnpackType)
{
	if (UnpackType == EVirtualTextureUnpackType::BaseColorYCoCg)
	{
		FString	SampleCode(TEXT("VirtualTextureUnpackBaseColorYCoCg(%s)"));
		return CodeIndex0 == INDEX_NONE ? INDEX_NONE : AddCodeChunk(MCT_Float3, *SampleCode, *GetParameterCode(CodeIndex0));
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC3)
	{
		FString	SampleCode(TEXT("VirtualTextureUnpackNormalBC3(%s)"));
		return CodeIndex1 == INDEX_NONE ? INDEX_NONE : AddCodeChunk(MCT_Float3, *SampleCode, *GetParameterCode(CodeIndex1));
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC5)
	{
		FString	SampleCode(TEXT("VirtualTextureUnpackNormalBC5(%s)"));
		return CodeIndex1 == INDEX_NONE ? INDEX_NONE : AddCodeChunk(MCT_Float3, *SampleCode, *GetParameterCode(CodeIndex1));
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC3BC3)
	{
		FString	SampleCode(TEXT("VirtualTextureUnpackNormalBC3BC3(%s, %s)"));
		return CodeIndex0 == INDEX_NONE || CodeIndex1 == INDEX_NONE ? INDEX_NONE : AddCodeChunk(MCT_Float3, *SampleCode, *GetParameterCode(CodeIndex0), *GetParameterCode(CodeIndex1));
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBC5BC1)
	{
		FString	SampleCode(TEXT("VirtualTextureUnpackNormalBC5BC1(%s, %s)"));
		return CodeIndex0 == INDEX_NONE || CodeIndex1 == INDEX_NONE ? INDEX_NONE : AddCodeChunk(MCT_Float3, *SampleCode, *GetParameterCode(CodeIndex1), *GetParameterCode(CodeIndex2));
	}
	else if (UnpackType == EVirtualTextureUnpackType::HeightR16)
	{
		FString	SampleCode(TEXT("VirtualTextureUnpackHeight(%s, %s)"));
		return CodeIndex0 == INDEX_NONE ? INDEX_NONE : AddCodeChunk(MCT_Float, *SampleCode, *GetParameterCode(CodeIndex0), *GetParameterCode(P0));
	}
	else if (UnpackType == EVirtualTextureUnpackType::NormalBGR565)
	{
		FString	SampleCode(TEXT("VirtualTextureUnpackNormalBGR565(%s)"));
		return CodeIndex0 == INDEX_NONE || CodeIndex1 == INDEX_NONE ? INDEX_NONE : AddCodeChunk(MCT_Float3, *SampleCode, *GetParameterCode(CodeIndex1));
	}
	if (UnpackType == EVirtualTextureUnpackType::BaseColorSRGB)
	{
		FString	SampleCode(TEXT("VirtualTextureUnpackBaseColorSRGB(%s)"));
		return CodeIndex0 == INDEX_NONE ? INDEX_NONE : AddCodeChunk(MCT_Float3, *SampleCode, *GetParameterCode(CodeIndex0));
	}
	return CodeIndex0;
}

int32 FHLSLMaterialTranslator::ExternalTexture(const FGuid& ExternalTextureGuid)
{
	bool bOnlyInPixelShader = GetFeatureLevel() < ERHIFeatureLevel::SM5;

	if (bOnlyInPixelShader && ShaderFrequency != SF_Pixel)
	{
		return NonPixelShaderExpressionError();
	}

	return AddUniformExpression(new FMaterialUniformExpressionExternalTexture(ExternalTextureGuid), MCT_TextureExternal, TEXT(""));
}

int32 FHLSLMaterialTranslator::ExternalTexture(UTexture* InTexture, int32& TextureReferenceIndex)
{
	bool bOnlyInPixelShader = GetFeatureLevel() < ERHIFeatureLevel::SM5;

	if (bOnlyInPixelShader && ShaderFrequency != SF_Pixel)
	{
		return NonPixelShaderExpressionError();
	}

	TextureReferenceIndex = Material->GetReferencedTextures().Find(InTexture);
	checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->ExternalTexture() without implementing UMaterialExpression::GetReferencedTexture properly"));

	return AddUniformExpression(new FMaterialUniformExpressionExternalTexture(TextureReferenceIndex), MCT_TextureExternal, TEXT(""));
}

int32 FHLSLMaterialTranslator::ExternalTextureParameter(FName ParameterName, UTexture* DefaultValue, int32& TextureReferenceIndex)
{
	bool bOnlyInPixelShader = GetFeatureLevel() < ERHIFeatureLevel::SM5;

	if (bOnlyInPixelShader && ShaderFrequency != SF_Pixel)
	{
		return NonPixelShaderExpressionError();
	}

	TextureReferenceIndex = Material->GetReferencedTextures().Find(DefaultValue);
	checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->ExternalTextureParameter() without implementing UMaterialExpression::GetReferencedTexture properly"));
	return AddUniformExpression(new FMaterialUniformExpressionExternalTextureParameter(ParameterName, TextureReferenceIndex), MCT_TextureExternal, TEXT(""));
}

int32 FHLSLMaterialTranslator::ExternalTextureCoordinateScaleRotation(int32 TextureReferenceIndex, TOptional<FName> ParameterName)
{
	return AddUniformExpression(new FMaterialUniformExpressionExternalTextureCoordinateScaleRotation(TextureReferenceIndex, ParameterName), MCT_Float4, TEXT(""));
}
int32 FHLSLMaterialTranslator::ExternalTextureCoordinateScaleRotation(const FGuid& ExternalTextureGuid)
{
	return AddUniformExpression(new FMaterialUniformExpressionExternalTextureCoordinateScaleRotation(ExternalTextureGuid), MCT_Float4, TEXT(""));
}
int32 FHLSLMaterialTranslator::ExternalTextureCoordinateOffset(int32 TextureReferenceIndex, TOptional<FName> ParameterName)
{
	return AddUniformExpression(new FMaterialUniformExpressionExternalTextureCoordinateOffset(TextureReferenceIndex, ParameterName), MCT_Float2, TEXT(""));
}
int32 FHLSLMaterialTranslator::ExternalTextureCoordinateOffset(const FGuid& ExternalTextureGuid)
{
	return AddUniformExpression(new FMaterialUniformExpressionExternalTextureCoordinateOffset(ExternalTextureGuid), MCT_Float2, TEXT(""));
}

UObject* FHLSLMaterialTranslator::GetReferencedTexture(int32 Index)
{
	return Material->GetReferencedTextures()[Index];
}

int32 FHLSLMaterialTranslator::StaticBool(bool bValue)
{
	return AddInlinedCodeChunk(MCT_StaticBool,(bValue ? TEXT("true") : TEXT("false")));
}

int32 FHLSLMaterialTranslator::DynamicBoolParameter(FName ParameterName, bool bDefaultValue)
{
	// Look up the value we are compiling with for this static parameter.
	bool bValue = bDefaultValue;

	FMaterialParameterInfo ParameterInfo = GetParameterAssociationInfo();
	ParameterInfo.Name = ParameterName;

	const UE::Shader::EValueType ValueType = GetShaderValueType(EMaterialParameterType::StaticSwitch);
	UE::Shader::FValue DefaultValue(bValue);

	const uint32* PrevDefaultOffset = DefaultUniformValues.Find(DefaultValue);
	uint32 DefaultOffset;
	if (PrevDefaultOffset)
	{
		DefaultOffset = *PrevDefaultOffset;
	}
	else
	{
		DefaultOffset = MaterialCompilationOutput.UniformExpressionSet.AddDefaultParameterValue(DefaultValue);
		DefaultUniformValues.Add(DefaultValue, DefaultOffset);
	}

	const int32 ParameterIndex = MaterialCompilationOutput.UniformExpressionSet.FindOrAddNumericParameter(EMaterialParameterType::StaticSwitch, ParameterInfo, DefaultOffset);
	return AddUniformExpression(new FMaterialUniformExpressionStaticBoolParameter(ParameterInfo, ParameterIndex), MCT_Bool, TEXT(""));
}

int32 FHLSLMaterialTranslator::StaticBoolParameter(FName ParameterName,bool bDefaultValue)
{
	// Look up the value we are compiling with for this static parameter.
	bool bValue = bDefaultValue;

	FMaterialParameterInfo ParameterInfo = GetParameterAssociationInfo();
	ParameterInfo.Name = ParameterName;

	for (const FStaticSwitchParameter& Parameter : StaticParameters.StaticSwitchParameters)
	{
		if (Parameter.ParameterInfo == ParameterInfo)
		{
			bValue = Parameter.Value;
			break;
		}
	}

	return StaticBool(bValue);
}
	
int32 FHLSLMaterialTranslator::StaticComponentMask(int32 Vector,FName ParameterName,bool bDefaultR,bool bDefaultG,bool bDefaultB,bool bDefaultA)
{
	// Look up the value we are compiling with for this static parameter.
	bool bValueR = bDefaultR;
	bool bValueG = bDefaultG;
	bool bValueB = bDefaultB;
	bool bValueA = bDefaultA;

	FMaterialParameterInfo ParameterInfo = GetParameterAssociationInfo();
	ParameterInfo.Name = ParameterName;

	for (const FStaticComponentMaskParameter& Parameter : StaticParameters.EditorOnly.StaticComponentMaskParameters)
	{
		if (Parameter.ParameterInfo == ParameterInfo)
		{
			bValueR = Parameter.R;
			bValueG = Parameter.G;
			bValueB = Parameter.B;
			bValueA = Parameter.A;
			break;
		}
	}

	return ComponentMask(Vector,bValueR,bValueG,bValueB,bValueA);
}

const FMaterialLayersFunctions* FHLSLMaterialTranslator::GetMaterialLayers()
{
	return StaticParameters.bHasMaterialLayers ? &CachedMaterialLayers : nullptr;
}

bool FHLSLMaterialTranslator::GetStaticBoolValue(int32 BoolIndex, bool& bSucceeded)
{
	bSucceeded = true;
	if (BoolIndex == INDEX_NONE)
	{
		bSucceeded = false;
		return false;
	}

	if (GetParameterType(BoolIndex) != MCT_StaticBool)
	{
		Errorf(TEXT("Failed to cast %s input to static bool type"), DescribeType(GetParameterType(BoolIndex)));
		bSucceeded = false;
		return false;
	}

	if (GetParameterCode(BoolIndex).Contains(TEXT("true")))
	{
		return true;
	}
	return false;
}

int32 FHLSLMaterialTranslator::StaticTerrainLayerWeight(FName LayerName,int32 Default)
{
	// Look up the weight-map index for this static parameter.
	int32 WeightmapIndex = INDEX_NONE;
	bool bFoundParameter = false;

	for(int32 ParameterIndex = 0;ParameterIndex < StaticParameters.EditorOnly.TerrainLayerWeightParameters.Num(); ++ParameterIndex)
	{
		const FStaticTerrainLayerWeightParameter& Parameter = StaticParameters.EditorOnly.TerrainLayerWeightParameters[ParameterIndex];
		if(Parameter.LayerName == LayerName)
		{
			WeightmapIndex = Parameter.WeightmapIndex;
			bFoundParameter = true;
		}
	}

	if(!bFoundParameter && Material->IsPreview())
	{
		return Default;
	}
	else if(WeightmapIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	else
	{			
		constexpr EMaterialSamplerType SamplerType = SAMPLERTYPE_Masks;
		FString WeightmapName = FString::Printf(TEXT("Weightmap%d"),WeightmapIndex);
		int32 TextureReferenceIndex = INDEX_NONE;
		int32 TextureCodeIndex = TextureParameter(FName(*WeightmapName), GEngine->WeightMapPlaceholderTexture, TextureReferenceIndex, SamplerType);
		int32 WeightmapCode = TextureSample(TextureCodeIndex, TextureCoordinate(3, false, false), SamplerType, /*MipValue0Index = */INDEX_NONE, /*MipValue1Index = */INDEX_NONE, /*MipValueMode = */TMVM_None, /*SamplerSource = */SSM_TerrainWeightmapGroupSettings);

		FString LayerMaskName = FString::Printf(TEXT("LayerMask_%s"),*LayerName.ToString());
		return Dot(WeightmapCode,VectorParameter(FName(*LayerMaskName), FLinearColor(1.f,0.f,0.f,0.f)));
	}
}

int32 FHLSLMaterialTranslator::VertexColor()
{
	bUsesVertexColor |= (ShaderFrequency != SF_Vertex);

	FString FiniteCode = TEXT("Parameters.VertexColor");
	if (IsAnalyticDerivEnabled())
	{
		FString AnalyticCode = DerivativeAutogen.ConstructDeriv(FiniteCode, TEXT("Parameters.VertexColor_DDX"), TEXT("Parameters.VertexColor_DDY"), EDerivativeType::Float4);
		return AddCodeChunkInnerDeriv(*FiniteCode, *AnalyticCode, MCT_Float4, false, EDerivativeStatus::Valid);
	}
	else
	{
		return AddInlinedCodeChunk(MCT_Float4, *FiniteCode);
	}
}

int32 FHLSLMaterialTranslator::PreSkinnedPosition()
{
	if (ShaderFrequency != SF_Vertex)
	{
		return Errorf(TEXT("Pre-skinned position is only available in the vertex shader, pass through custom interpolators if needed."));
	}

	return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.PreSkinnedPosition"));
}

int32 FHLSLMaterialTranslator::PreSkinnedNormal()
{
	if (ShaderFrequency != SF_Vertex)
	{
		return Errorf(TEXT("Pre-skinned normal is only available in the vertex shader, pass through custom interpolators if needed."));
	}

	return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.PreSkinnedNormal"));
}

int32 FHLSLMaterialTranslator::VertexInterpolator(uint32 InterpolatorIndex)
{
	if (ShaderFrequency != SF_Pixel)
	{
		return Errorf(TEXT("Custom interpolator outputs only available in pixel shaders."));
	}

	UMaterialExpressionVertexInterpolator** InterpolatorPtr = CustomVertexInterpolators.FindByPredicate([InterpolatorIndex](const UMaterialExpressionVertexInterpolator* Item) { return Item && Item->InterpolatorIndex == InterpolatorIndex; });
	if (InterpolatorPtr == nullptr)
	{
		return Errorf(TEXT("Invalid custom interpolator index."));
	}

	MaterialCompilationOutput.bUsesVertexInterpolator = true;

	UMaterialExpressionVertexInterpolator* Interpolator = *InterpolatorPtr;
	check(Interpolator->InterpolatorIndex == InterpolatorIndex);
	check(Interpolator->InterpolatedType & MCT_Float);

	// Assign interpolator offset and accumulate size
	int32 InterpolatorSize = 0;
	switch (Interpolator->InterpolatedType)
	{
	case MCT_Float4:	InterpolatorSize = 4; break;
	case MCT_Float3:	InterpolatorSize = 3; break;
	case MCT_Float2:	InterpolatorSize = 2; break;
	default:			InterpolatorSize = 1;
	};

	if (Interpolator->InterpolatorOffset == INDEX_NONE)
	{
		Interpolator->InterpolatorOffset = CurrentCustomVertexInterpolatorOffset;
		CurrentCustomVertexInterpolatorOffset += InterpolatorSize;
	}
	check(CurrentCustomVertexInterpolatorOffset != INDEX_NONE && Interpolator->InterpolatorOffset < CurrentCustomVertexInterpolatorOffset);

	// Copy interpolated data from pixel parameters to local
	const TCHAR* TypeName = HLSLTypeString(Interpolator->InterpolatedType);
	const TCHAR* Swizzle[2] = { TEXT("x"), TEXT("y") };
	const int32 Offset = Interpolator->InterpolatorOffset;
	
	// Note: We reference the UV define directly to avoid having to pre-accumulate UV counts before property translation
	FString ValueCode	= FString::Printf(TEXT("%s(Parameters.TexCoords[VERTEX_INTERPOLATOR_%i_TEXCOORDS_X].%s"),		TypeName, InterpolatorIndex, Swizzle[Offset%2]);
	FString DDXCode		= FString::Printf(TEXT("%s(Parameters.TexCoords_DDX[VERTEX_INTERPOLATOR_%i_TEXCOORDS_X].%s"),	TypeName, InterpolatorIndex, Swizzle[Offset%2]);
	FString DDYCode		= FString::Printf(TEXT("%s(Parameters.TexCoords_DDY[VERTEX_INTERPOLATOR_%i_TEXCOORDS_X].%s"),	TypeName, InterpolatorIndex, Swizzle[Offset%2]);
	if (InterpolatorSize >= 2)
	{
		ValueCode	+= FString::Printf(TEXT(", Parameters.TexCoords[VERTEX_INTERPOLATOR_%i_TEXCOORDS_Y].%s"),		InterpolatorIndex, Swizzle[(Offset+1)%2]);
		DDXCode		+= FString::Printf(TEXT(", Parameters.TexCoords_DDX[VERTEX_INTERPOLATOR_%i_TEXCOORDS_Y].%s"),	InterpolatorIndex, Swizzle[(Offset+1)%2]);
		DDYCode		+= FString::Printf(TEXT(", Parameters.TexCoords_DDY[VERTEX_INTERPOLATOR_%i_TEXCOORDS_Y].%s"),	InterpolatorIndex, Swizzle[(Offset+1)%2]);

		if (InterpolatorSize >= 3)
		{
			ValueCode	+= FString::Printf(TEXT(", Parameters.TexCoords[VERTEX_INTERPOLATOR_%i_TEXCOORDS_Z].%s"),		InterpolatorIndex, Swizzle[(Offset+2)%2]);
			DDXCode		+= FString::Printf(TEXT(", Parameters.TexCoords_DDX[VERTEX_INTERPOLATOR_%i_TEXCOORDS_Z].%s"),	InterpolatorIndex, Swizzle[(Offset+2)%2]);
			DDYCode		+= FString::Printf(TEXT(", Parameters.TexCoords_DDY[VERTEX_INTERPOLATOR_%i_TEXCOORDS_Z].%s"),	InterpolatorIndex, Swizzle[(Offset+2)%2]);

			if (InterpolatorSize >= 4)
			{
				check(InterpolatorSize == 4);
				ValueCode	+= FString::Printf(TEXT(", Parameters.TexCoords[VERTEX_INTERPOLATOR_%i_TEXCOORDS_W].%s"),		InterpolatorIndex, Swizzle[(Offset+3)%2]);
				DDXCode		+= FString::Printf(TEXT(", Parameters.TexCoords_DDX[VERTEX_INTERPOLATOR_%i_TEXCOORDS_W].%s"),	InterpolatorIndex, Swizzle[(Offset+3)%2]);
				DDYCode		+= FString::Printf(TEXT(", Parameters.TexCoords_DDY[VERTEX_INTERPOLATOR_%i_TEXCOORDS_W].%s"),	InterpolatorIndex, Swizzle[(Offset+3)%2]);
			}
		}
	}

	ValueCode.Append(TEXT(")"));
	DDXCode.Append(TEXT(")"));
	DDYCode.Append(TEXT(")"));

	if (IsAnalyticDerivEnabled())
	{
		FString AnalyticCode = DerivativeAutogen.ConstructDeriv(*ValueCode, *DDXCode, *DDYCode, GetDerivType(Interpolator->InterpolatedType));
		return AddCodeChunkInnerDeriv(*ValueCode, *AnalyticCode, Interpolator->InterpolatedType, false, EDerivativeStatus::Valid);
	}
	else
	{
		return AddCodeChunk(Interpolator->InterpolatedType, *ValueCode);
	}

}

int32 FHLSLMaterialTranslator::Add(int32 A, int32 B)
{
	if (A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const EMaterialValueType ResultType = GetArithmeticResultType(A, B);
	if (GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
	{
		return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A), GetParameterUniformExpression(B), FMO_Add), ResultType, TEXT("(%s + %s)"), *GetParameterCode(A), *GetParameterCode(B));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Add, A, B);
		}
		else
		{
			if (ResultType & MCT_LWCType)
			{
				return AddCodeChunk(ResultType, TEXT("LWCAdd(%s, %s)"), *GetParameterCode(A), *GetParameterCode(B));
			}
			else
			{
				return AddCodeChunk(ResultType, TEXT("(%s + %s)"), *GetParameterCode(A), *GetParameterCode(B));
			}
		}
	}
}

int32 FHLSLMaterialTranslator::Sub(int32 A, int32 B)
{
	if (A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const EMaterialValueType ResultType = GetArithmeticResultType(A, B);
	if (GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
	{
		return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A), GetParameterUniformExpression(B), FMO_Sub), ResultType, TEXT("(%s - %s)"), *GetParameterCode(A), *GetParameterCode(B));
	}
	else
	{
		int32 ResultCode = INDEX_NONE;

		if (IsAnalyticDerivEnabled())
		{
			ResultCode = DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Sub, A, B);
		}
		else
		{
			if (ResultType & MCT_LWCType)
			{
				ResultCode = AddCodeChunk(ResultType, TEXT("LWCSubtract(%s, %s)"), *GetParameterCode(A), *GetParameterCode(B));
			}
			else
			{
				ResultCode = AddCodeChunk(ResultType, TEXT("(%s - %s)"), *GetParameterCode(A), *GetParameterCode(B));
			}
		}

		// If both sides are LWC and we are subtracting, assume relative space going forward and truncate to single precision float.
		const EMaterialValueType AType = GetParameterType(A);
		const EMaterialValueType BType = GetParameterType(B);

		const bool bTruncateToFloat = (AType == BType) && IsLWCType(AType) && IsLWCType(BType);
		if (bTruncateToFloat)
		{
			const bool bAllowLWCTruncate = GetLWCTruncateMode() == 2; // Allow automatic truncate?
			if (bAllowLWCTruncate)
			{
				return TruncateLWC(ResultCode);
			}
		}

		return ResultCode;
	}
}

int32 FHLSLMaterialTranslator::Mul(int32 A,int32 B)
{
	if(A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
	{
		return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Mul),GetArithmeticResultType(A,B),TEXT("(%s * %s)"),*GetParameterCode(A),*GetParameterCode(B));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc2(*this,FMaterialDerivativeAutogen::EFunc2::Mul,A,B);
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A,B),TEXT("(%s * %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
	}
}

int32 FHLSLMaterialTranslator::Div(int32 A, int32 B)
{
	if (A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	FMaterialUniformExpression* UniformA = GetParameterUniformExpression(A);
	FMaterialUniformExpression* UniformB = GetParameterUniformExpression(B);
	if (UniformA && UniformB)
	{
		return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(UniformA, UniformB, FMO_Div), GetArithmeticResultType(A, B), TEXT("(%s / %s)"), *GetParameterCode(A), *GetParameterCode(B));
	}
	else if (UniformB && !UniformB->IsConstant())
	{
		// Division is often optimized as multiplication by reciprocal
		// If the divisor is a uniform expression, we can fold the reciprocal into the preshader
		const int32 RcpB = AddUniformExpression(new FMaterialUniformExpressionRcp(UniformB), GetParameterType(B), TEXT("rcp(%s)"), *GetParameterCode(B));
		return Mul(A, RcpB);
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Div, A, B);
		}
		else
		{
			return AddCodeChunk(GetArithmeticResultType(A, B), TEXT("(%s / %s)"), *GetParameterCode(A), *GetParameterCode(B));
		}
	}
}

int32 FHLSLMaterialTranslator::Dot(int32 A,int32 B)
{
	if(A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	FMaterialUniformExpression* ExpressionA = GetParameterUniformExpression(A);
	FMaterialUniformExpression* ExpressionB = GetParameterUniformExpression(B);

	EMaterialValueType TypeA = GetParameterType(A);
	EMaterialValueType TypeB = GetParameterType(B);
	if(ExpressionA && ExpressionB)
	{
		if (TypeA == MCT_Float && TypeB == MCT_Float)
		{
			return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Mul),MCT_Float,TEXT("(%s * %s)"),*GetParameterCode(A),*GetParameterCode(B));
		}
		else
		{
			if (TypeA == TypeB)
			{
				return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Dot,TypeA),MCT_Float,TEXT("dot(%s,%s)"),*GetParameterCode(A),*GetParameterCode(B));
			}
			else
			{
				// Promote scalar (or truncate the bigger type)
				if (TypeA == MCT_Float || (TypeB != MCT_Float && GetNumComponents(TypeA) > GetNumComponents(TypeB)))
				{
					return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Dot,TypeB),MCT_Float,TEXT("dot(%s,%s)"),*CoerceParameter(A, TypeB),*GetParameterCode(B));
				}
				else
				{
					return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(ExpressionA,ExpressionB,FMO_Dot,TypeA),MCT_Float,TEXT("dot(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B, TypeA));
				}
			}
		}
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Dot, A, B);
		}
		else
		{
		// Promote scalar (or truncate the bigger type)
		if (TypeA == MCT_Float || (TypeB != MCT_Float && GetNumComponents(TypeA) > GetNumComponents(TypeB)))
		{
			return AddCodeChunk(MCT_Float,TEXT("dot(%s, %s)"), *CoerceParameter(A, TypeB), *GetParameterCode(B));
		}
		else
		{
			return AddCodeChunk(MCT_Float,TEXT("dot(%s, %s)"), *GetParameterCode(A), *CoerceParameter(B, TypeA));
		}
	}
}
}

int32 FHLSLMaterialTranslator::Cross(int32 A,int32 B)
{
	if(A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
	{		
		EMaterialValueType ResultType = GetArithmeticResultType(A,B);
		if (ResultType == MCT_Float2 || (ResultType & MCT_Float) == 0)
		{
			return Errorf(TEXT("Cross product requires 3-component vector input."));
		}
		return AddUniformExpression(new FMaterialUniformExpressionFoldedMath(GetParameterUniformExpression(A),GetParameterUniformExpression(B),FMO_Cross,ResultType),MCT_Float3,TEXT("cross(%s,%s)"),*GetParameterCode(A),*GetParameterCode(B));
	}
	else
	{
		if(IsAnalyticDerivEnabled())
			return DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Cross, A, B);
		else
		return AddCodeChunk(MCT_Float3,TEXT("cross(%s,%s)"),*CoerceParameter(A,MCT_Float3),*CoerceParameter(B,MCT_Float3));
	}
}

int32 FHLSLMaterialTranslator::Power(int32 Base,int32 Exponent)
{
	if(Base == INDEX_NONE || Exponent == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (IsAnalyticDerivEnabled())
	{
		return DerivativeAutogen.GenerateExpressionFunc2(*this,FMaterialDerivativeAutogen::EFunc2::PowPositiveClamped,Base,Exponent);
	}
	else
	{
	// Clamp Pow input to >= 0 to help avoid common NaN cases
	return AddCodeChunk(GetParameterType(Base),TEXT("PositiveClampedPow(%s,%s)"),*GetParameterCode(Base),*CoerceParameter(Exponent,MCT_Float));
}
}
	
int32 FHLSLMaterialTranslator::Logarithm2(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}
		
	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionLogarithm2(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("log2(%s)"),*GetParameterCode(X));
	}
	else if (IsAnalyticDerivEnabled())
	{
		return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Log2, X);
	}
	else
	{
		return AddCodeChunk(GetParameterType(X), TEXT("log2(%s)"), *GetParameterCode(X));
	}
}

int32 FHLSLMaterialTranslator::Logarithm10(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}
		
	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionLogarithm10(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("log10(%s)"),*GetParameterCode(X));
	}
	else if (IsAnalyticDerivEnabled())
	{
		return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Log10, X);
	}
	else
	{
		return AddCodeChunk(GetParameterType(X), TEXT("log10(%s)"), *GetParameterCode(X));
	}
}

int32 FHLSLMaterialTranslator::SquareRoot(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionSquareRoot(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("sqrt(%s)"),*GetParameterCode(X));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this,FMaterialDerivativeAutogen::EFunc1::Sqrt,X);
		}
		else
		{
		return AddCodeChunk(GetParameterType(X),TEXT("sqrt(%s)"),*GetParameterCode(X));
	}
}
}

int32 FHLSLMaterialTranslator::Length(int32 X)
{
	if (X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionLength(GetParameterUniformExpression(X), GetParameterType(X)), MCT_Float, TEXT("length(%s)"), *GetParameterCode(X));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Length, X);
		}
		else
		{
			return AddCodeChunk(MCT_Float, TEXT("length(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Normalize(int32 X)
{
	if (X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const EMaterialValueType ResultType = MakeNonLWCType(GetParameterType(X));

	if (GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionNormalize(GetParameterUniformExpression(X)), ResultType, TEXT("normalize(%s)"), *GetParameterCode(X));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this, FMaterialDerivativeAutogen::EFunc1::Normalize, X);
		}
		else
		{
			return AddCodeChunk(ResultType, TEXT("normalize(%s)"), *GetParameterCode(X));
		}
	}
}

int32 FHLSLMaterialTranslator::Step(int32 Y, int32 X)
{
	if (X == INDEX_NONE || Y == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	FMaterialUniformExpression* ExpressionX = GetParameterUniformExpression(X);
	FMaterialUniformExpression* ExpressionY = GetParameterUniformExpression(Y);

	EMaterialValueType ResultType = GetArithmeticResultType(X, Y);

	//Constant folding.
	if (ExpressionX && ExpressionY)
	{
		// when x == y return 1.0f
		if (ExpressionX == ExpressionY)
		{
			const int32 EqualResult = 1.0f;
			if (ResultType == MCT_Float || ResultType == MCT_Float1)
			{
				return Constant(EqualResult);
			}
			if (ResultType == MCT_Float2)
			{
				return Constant2(EqualResult, EqualResult);
			}
			if (ResultType == MCT_Float3)
			{
				return Constant3(EqualResult, EqualResult, EqualResult);
			}
			if (ResultType == MCT_Float4)
			{
				return Constant4(EqualResult, EqualResult, EqualResult, EqualResult);
			}
		}

		if (ExpressionX->IsConstant() && ExpressionY->IsConstant())
		{
			FLinearColor ValueX, ValueY;
			FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
			ExpressionX->GetNumberValue(DummyContext, ValueX);
			ExpressionY->GetNumberValue(DummyContext, ValueY);

			float Red = ValueX.R >= ValueY.R ? 1 : 0;
			if (ResultType == MCT_Float || ResultType == MCT_Float1)
			{
				return Constant(Red);
			}

			float Green = ValueX.G >= ValueY.G ? 1 : 0;
			if (ResultType == MCT_Float2)
			{
				return Constant2(Red, Green);
			}

			float Blue = ValueX.B >= ValueY.B ? 1 : 0;
			if (ResultType == MCT_Float3)
			{
				return Constant3(Red, Green, Blue);
			}

			float Alpha = ValueX.A >= ValueY.A ? 1 : 0;
			if (ResultType == MCT_Float4)
			{
				return Constant4(Red, Green, Blue, Alpha);
			}
		}
	}

	if (IsLWCType(ResultType))
	{
		return AddCodeChunkZeroDeriv(MakeNonLWCType(ResultType), TEXT("LWCStep(%s,%s)"), *CoerceParameter(Y, ResultType), *CoerceParameter(X, ResultType));
	}
	else
	{
		return AddCodeChunkZeroDeriv(ResultType, TEXT("step(%s,%s)"), *CoerceParameter(Y, ResultType), *CoerceParameter(X, ResultType));
	}
}

int32 FHLSLMaterialTranslator::SmoothStep(int32 X, int32 Y, int32 A)
{
	if (X == INDEX_NONE || Y == INDEX_NONE || A == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	FMaterialUniformExpression* ExpressionX = GetParameterUniformExpression(X);
	FMaterialUniformExpression* ExpressionY = GetParameterUniformExpression(Y);
	FMaterialUniformExpression* ExpressionA = GetParameterUniformExpression(A);
	bool bExpressionsAreEqual = false;

	// According to https://docs.microsoft.com/en-us/windows/win32/direct3dhlsl/dx-graphics-hlsl-smoothstep
	// Smoothstep's min and max and return result in the same size as the alpha.
	// Therefore the result type (and each input) should be GetParameterType(A);

	// However, for usability reasons, we will use the ArithmiticType of the three.
	// This is important to do, because it allows a user to input a vector into the min or max
	// and get a vector result, without putting inputs into the other two constants.
	// This is not exactly the behavior of raw HLSL, but it is a more intuitive experience
	// and mimics more closely the LinearInterpolate node.
	// Incompatible inputs will be caught by the CoerceParameters below.

	EMaterialValueType ResultType = GetArithmeticResultType(X, Y);
	ResultType = GetArithmeticResultType(ResultType, GetParameterType(A));


	// Skip over interpolations where inputs are equal

	float EqualResult = 0.0f;
	// smoothstep( x, y, y ) == 1.0
	if (Y == A)
	{
		bExpressionsAreEqual = true;
		EqualResult = 1.0f;
	}

	// smoothstep( x, y, x ) == 0.0
	if (X == A)
	{
		bExpressionsAreEqual = true;
		EqualResult = 0.0f;
	}

	if (bExpressionsAreEqual)
	{
		if (ResultType == MCT_Float || ResultType == MCT_Float1)
		{
			return Constant(EqualResult);
		}
		if (ResultType == MCT_Float2)
		{
			return Constant2(EqualResult, EqualResult);
		}
		if (ResultType == MCT_Float3)
		{
			return Constant3(EqualResult, EqualResult, EqualResult);
		}
		if (ResultType == MCT_Float4)
		{
			return Constant4(EqualResult, EqualResult, EqualResult, EqualResult);
		}
	}

	// smoothstep( x, x, a ) could create a div by zero depending on implementation.
	// The common implementation is to treat smoothstep as a step in these situations.
	if (X == Y)
	{
		bExpressionsAreEqual = true;
	}
	else if (ExpressionX && ExpressionY)
	{
		if (ExpressionX->IsConstant() && ExpressionY->IsConstant() && (*CurrentScopeChunks)[X].Type == (*CurrentScopeChunks)[Y].Type)
		{
			FLinearColor ValueX, ValueY;
			FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
			ExpressionX->GetNumberValue(DummyContext, ValueX);
			ExpressionY->GetNumberValue(DummyContext, ValueY);

			if (ValueX == ValueY)
			{
				bExpressionsAreEqual = true;
			}
		}
	}

	if (bExpressionsAreEqual)
	{
		return Step(X, A);
	}

	//When all inputs are constant, we can precompile the operation.
	if (ExpressionX && ExpressionY && ExpressionA && ExpressionX->IsConstant() && ExpressionY->IsConstant() && ExpressionA->IsConstant())
	{
		FLinearColor ValueX, ValueY, ValueA;
		FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
		ExpressionX->GetNumberValue(DummyContext, ValueX);
		ExpressionY->GetNumberValue(DummyContext, ValueY);
		ExpressionA->GetNumberValue(DummyContext, ValueA);

		float Red = FMath::SmoothStep(ValueX.R, ValueY.R, ValueA.R);
		if (ResultType == MCT_Float || ResultType == MCT_Float1)
		{
			return Constant(Red);
		}

		float Green = FMath::SmoothStep(ValueX.G, ValueY.G, ValueA.G);
		if (ResultType == MCT_Float2)
		{
			return Constant2(Red, Green);
		}

		float Blue = FMath::SmoothStep(ValueX.B, ValueY.B, ValueA.B);
		if (ResultType == MCT_Float3)
		{
			return Constant3(Red, Green, Blue);
		}

		float Alpha = FMath::SmoothStep(ValueX.A, ValueY.A, ValueA.A);
		if (ResultType == MCT_Float4)
		{
			return Constant4(Red, Green, Blue, Alpha);
		}
	}

	if (IsLWCType(ResultType))
	{
		return AddCodeChunk(MakeNonLWCType(ResultType), TEXT("LWCSmoothStep(%s,%s,%s)"), *CoerceParameter(X, ResultType), *CoerceParameter(Y, ResultType), *CoerceParameter(A, ResultType));
	}
	else
	{
		return AddCodeChunk(ResultType, TEXT("smoothstep(%s,%s,%s)"), *CoerceParameter(X, ResultType), *CoerceParameter(Y, ResultType), *CoerceParameter(A, ResultType));
	}
}

int32 FHLSLMaterialTranslator::InvLerp(int32 X, int32 Y, int32 A)
{
	if (X == INDEX_NONE || Y == INDEX_NONE || A == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	FMaterialUniformExpression* ExpressionX = GetParameterUniformExpression(X);
	FMaterialUniformExpression* ExpressionY = GetParameterUniformExpression(Y);
	FMaterialUniformExpression* ExpressionA = GetParameterUniformExpression(A);
	bool bExpressionsAreEqual = false;

	EMaterialValueType ResultType = GetParameterType(A);


	// Skip over interpolations where inputs are equal.

	float EqualResult = 0.0f;
	// (y-x)/(y-x) == 1.0
	if (Y == A)
	{
		bExpressionsAreEqual = true;
		EqualResult = 1.0;
	}

	// (x-x)/(y-x) == 0.0
	if (X == A)
	{
		bExpressionsAreEqual = true;
		EqualResult = 0.0f;
	}

	if (bExpressionsAreEqual)
	{
		if (ResultType == MCT_Float || ResultType == MCT_Float1)
		{
			return Constant(EqualResult);
		}
		if (ResultType == MCT_Float2)
		{
			return Constant2(EqualResult, EqualResult);
		}
		if (ResultType == MCT_Float3)
		{
			return Constant3(EqualResult, EqualResult, EqualResult);
		}
		if (ResultType == MCT_Float4)
		{
			return Constant4(EqualResult, EqualResult, EqualResult, EqualResult);
		}
	}

	// (a-x)/(x-x) will create a div by zero.
	if (X == Y)
	{
		bExpressionsAreEqual = true;
	}
	else if (ExpressionX && ExpressionY)
	{
		if (ExpressionX->IsConstant() && ExpressionY->IsConstant() && (*CurrentScopeChunks)[X].Type == (*CurrentScopeChunks)[Y].Type)
		{
			FLinearColor ValueX, ValueY;
			FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
			ExpressionX->GetNumberValue(DummyContext, ValueX);
			ExpressionY->GetNumberValue(DummyContext, ValueY);

			if (ValueX == ValueY)
			{
				bExpressionsAreEqual = true;
			}
		}
	}

	if (bExpressionsAreEqual)
	{
		Error(TEXT("Div by Zero: InvLerp A == B."));
	}

	//When all inputs are constant, we can precompile the operation.
	if (ExpressionX && ExpressionY && ExpressionA && ExpressionX->IsConstant() && ExpressionY->IsConstant() && ExpressionA->IsConstant())
	{
		FLinearColor ValueX, ValueY, ValueA;
		FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
		ExpressionX->GetNumberValue(DummyContext, ValueX);
		ExpressionY->GetNumberValue(DummyContext, ValueY);
		ExpressionA->GetNumberValue(DummyContext, ValueA);

		float Red = FMath::GetRangePct(ValueX.R, ValueY.R, ValueA.R);
		if (ResultType == MCT_Float || ResultType == MCT_Float1)
		{
			return Constant(Red);
		}

		float Green = FMath::GetRangePct(ValueX.G, ValueY.G, ValueA.G);
		if (ResultType == MCT_Float2)
		{
			return Constant2(Red, Green);
		}

		float Blue = FMath::GetRangePct(ValueX.B, ValueY.B, ValueA.B);
		if (ResultType == MCT_Float3)
		{
			return Constant3(Red, Green, Blue);
		}

		float Alpha = FMath::GetRangePct(ValueX.A, ValueY.A, ValueA.A);
		if (ResultType == MCT_Float4)
		{
			return Constant4(Red, Green, Blue, Alpha);
		}
	}

	int32 Numerator = Sub(A, X);
	int32 Denominator = Sub(Y, X);

	return Div(Numerator, Denominator);
}

int32 FHLSLMaterialTranslator::Lerp(int32 X,int32 Y,int32 A)
{
	if(X == INDEX_NONE || Y == INDEX_NONE || A == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	FMaterialUniformExpression* ExpressionX = GetParameterUniformExpression(X);
	FMaterialUniformExpression* ExpressionY = GetParameterUniformExpression(Y);
	FMaterialUniformExpression* ExpressionA = GetParameterUniformExpression(A);
	bool bExpressionsAreEqual = false;

	// Skip over interpolations where inputs are equal
	if (X == Y)
	{
		bExpressionsAreEqual = true;
	}
	else if (ExpressionX && ExpressionY)
	{
		if (ExpressionX->IsConstant() && ExpressionY->IsConstant() && (*CurrentScopeChunks)[X].Type == (*CurrentScopeChunks)[Y].Type)
		{
			FLinearColor ValueX, ValueY;
			FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
			ExpressionX->GetNumberValue(DummyContext, ValueX);
			ExpressionY->GetNumberValue(DummyContext, ValueY);

			if (ValueX == ValueY)
			{
				bExpressionsAreEqual = true;
			}
		}
	}

	if (bExpressionsAreEqual)
	{
		return X;
	}

	EMaterialValueType ResultType = GetArithmeticResultType(X,Y);
	EMaterialValueType AlphaType = ResultType == (*CurrentScopeChunks)[A].Type ? ResultType : MCT_Float1;

	if (AlphaType == MCT_Float1 && ExpressionA && ExpressionA->IsConstant())
	{
		// Skip over interpolations that explicitly select an input
		FLinearColor Value;
		FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
		ExpressionA->GetNumberValue(DummyContext, Value);

		if (Value.R == 0.0f)
		{
			return X;
		}
		else if (Value.R == 1.f)
		{
			return Y;
		}
	}

	if (IsAnalyticDerivEnabled())
	{
		return DerivativeAutogen.GenerateLerpFunc(*this, X, Y, A);
	}
	else
	{
		return AddCodeChunk(ResultType,TEXT("lerp(%s, %s, %s)"),*CoerceParameter(X,ResultType),*CoerceParameter(Y,ResultType),*CoerceParameter(A,AlphaType));
	}
}

int32 FHLSLMaterialTranslator::Min(int32 A,int32 B)
{
	if(A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
	{
		return AddUniformExpression(new FMaterialUniformExpressionMin(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(A),TEXT("min(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc2(*this,FMaterialDerivativeAutogen::EFunc2::Min,A,B);
		}
		else
		{
		return AddCodeChunk(GetParameterType(A),TEXT("min(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
	}
}
}

int32 FHLSLMaterialTranslator::Max(int32 A,int32 B)
{
	if(A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
	{
		return AddUniformExpression(new FMaterialUniformExpressionMax(GetParameterUniformExpression(A),GetParameterUniformExpression(B)),GetParameterType(A),TEXT("max(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc2(*this,FMaterialDerivativeAutogen::EFunc2::Max,A,B);
		}
		else
		{
		return AddCodeChunk(GetParameterType(A),TEXT("max(%s,%s)"),*GetParameterCode(A),*CoerceParameter(B,GetParameterType(A)));
	}
}
}

int32 FHLSLMaterialTranslator::Clamp(int32 X, int32 A, int32 B)
{
	if (X == INDEX_NONE || A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	FMaterialUniformExpression* UniformX = GetParameterUniformExpression(X);
	FMaterialUniformExpression* UniformA = GetParameterUniformExpression(A);
	FMaterialUniformExpression* UniformB = GetParameterUniformExpression(B);

	if (UniformA && UniformB && UniformA->IsConstant() && UniformB->IsConstant())
	{
		// Check to see if we're clamping between 0-1, in that case we can use Saturate, which has some additional optimizations
		FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
		FLinearColor ValueA;
		FLinearColor ValueB;
		UniformA->GetNumberValue(DummyContext, ValueA);
		UniformB->GetNumberValue(DummyContext, ValueB);
		if (ValueA == FLinearColor(0.0f, 0.0f, 0.0f, 0.0f) && ValueB == FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
		{
			return Saturate(X);
		}
	}
	
	if (UniformX && UniformA && UniformB)
	{
		return AddUniformExpression(new FMaterialUniformExpressionClamp(UniformX, UniformA, UniformB), GetParameterType(X), TEXT("min(max(%s,%s),%s)"), *GetParameterCode(X), *CoerceParameter(A, GetParameterType(X)), *CoerceParameter(B, GetParameterType(X)));
	}

	if (IsAnalyticDerivEnabled())
	{
		// Make sure 'X' is given as first parameter, to ensure proper type coercion
		// LWC_TODO - should handle the case where an LWC value is clamped between 2 scalars, could use optimized clamping function in that case
		int32 MaxXA = DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Max, X, A);
		int32 Result = DerivativeAutogen.GenerateExpressionFunc2(*this, FMaterialDerivativeAutogen::EFunc2::Min, MaxXA, B);
		return Result;
	}
	else
	{
		return AddCodeChunk(GetParameterType(X), TEXT("min(max(%s,%s),%s)"), *GetParameterCode(X), *CoerceParameter(A, GetParameterType(X)), *CoerceParameter(B, GetParameterType(X)));
	}
}

int32 FHLSLMaterialTranslator::Saturate(int32 X)
{
	if(X == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if(GetParameterUniformExpression(X))
	{
		return AddUniformExpression(new FMaterialUniformExpressionSaturate(GetParameterUniformExpression(X)),GetParameterType(X),TEXT("saturate(%s)"),*GetParameterCode(X));
	}
	else
	{
		if (IsAnalyticDerivEnabled())
		{
			return DerivativeAutogen.GenerateExpressionFunc1(*this,FMaterialDerivativeAutogen::EFunc1::Saturate,X);
		}
		else
		{
		return AddCodeChunk(GetParameterType(X),TEXT("saturate(%s)"),*GetParameterCode(X));
	}
}
}

static FString ComponentMaskLWC(const FString& SourceString, EMaterialValueType SourceType, EMaterialValueType ResultType, bool R, bool G, bool B, bool A)
{
	const TCHAR* ComponentAccess[] =
	{
		TEXT("LWCGetX"),
		TEXT("LWCGetY"),
		TEXT("LWCGetZ"),
		TEXT("LWCGetW"),
	};

	bool bNeedClosingParen = false;
	bool bNeedComma = false;
	FString Result;
	if (ResultType != MCT_LWCScalar)
	{
		Result = TEXT("MakeLWCVector(");
		bNeedClosingParen = true;
	}
	if (R)
	{
		Result += FString::Printf(TEXT("%s(%s)"), ComponentAccess[0], *SourceString);
		bNeedComma = true;
	}
	if (G)
	{
		if (bNeedComma) Result += TEXT(", ");
		Result += FString::Printf(TEXT("%s(%s)"), ComponentAccess[SourceType == MCT_LWCScalar ? 0 : 1], *SourceString);
		bNeedComma = true;
	}
	if (B)
	{
		if (bNeedComma) Result += TEXT(", ");
		Result += FString::Printf(TEXT("%s(%s)"), ComponentAccess[SourceType == MCT_LWCScalar ? 0 : 2], *SourceString);
		bNeedComma = true;
	}
	if (A)
	{
		if (bNeedComma) Result += TEXT(", ");
		Result += FString::Printf(TEXT("%s(%s)"), ComponentAccess[SourceType == MCT_LWCScalar ? 0 : 3], *SourceString);
		bNeedComma = true;
	}
	if (bNeedClosingParen)
	{
		Result += TEXT(")");
	}
	return Result;
}

int32 FHLSLMaterialTranslator::ComponentMask(int32 Vector,bool R,bool G,bool B,bool A)
{
	if(Vector == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	EMaterialValueType VectorType = GetParameterType(Vector);
	uint32 NumValidComponents = 0u;
	if (VectorType & MCT_Float4) NumValidComponents = 4u;
	else if (VectorType & MCT_Float3) NumValidComponents = 3u;
	else if (VectorType & MCT_Float2) NumValidComponents = 2u;
	else if (VectorType & MCT_Float1) NumValidComponents = 1u;
	else if (VectorType == MCT_LWCScalar) NumValidComponents = 4u; // Allow .gba mask on scalar values, will use .rrrr as needed
	else if (VectorType == MCT_LWCVector2) NumValidComponents = 2u;
	else if (VectorType == MCT_LWCVector3) NumValidComponents = 3u;
	else if (VectorType == MCT_LWCVector4) NumValidComponents = 4u;

	if(	(A && NumValidComponents < 4u) ||
		(B && NumValidComponents < 3u) ||
		(G && NumValidComponents < 2u) ||
		(R && NumValidComponents < 1u))
	{
		return Errorf(TEXT("Not enough components in (%s: %s) for component mask %u%u%u%u"),*GetParameterCode(Vector),DescribeType(VectorType),R,G,B,A);
	}

	const bool bIsLWC = (VectorType & MCT_LWCType);
	EMaterialValueType ResultType;
	switch((R ? 1 : 0) + (G ? 1 : 0) + (B ? 1 : 0) + (A ? 1 : 0))
	{
	case 1: ResultType = bIsLWC ? MCT_LWCScalar : MCT_Float; break;
	case 2: ResultType = bIsLWC ? MCT_LWCVector2 : MCT_Float2; break;
	case 3: ResultType = bIsLWC ? MCT_LWCVector3 : MCT_Float3; break;
	case 4: ResultType = bIsLWC ? MCT_LWCVector4 : MCT_Float4; break;
	default: 
		return Errorf(TEXT("Couldn't determine result type of component mask %u%u%u%u"),R,G,B,A);
	};

	FString MaskString = FString::Printf(TEXT("%s%s%s%s"),
		R ? TEXT("r") : TEXT(""),
		// If VectorType is set to MCT_Float/MCT_LWCScalar which means it could be any of the float types, assume it is a float1
		G ? ((VectorType == MCT_Float || VectorType == MCT_LWCScalar) ? TEXT("r") : TEXT("g")) : TEXT(""),
		B ? ((VectorType == MCT_Float || VectorType == MCT_LWCScalar) ? TEXT("r") : TEXT("b")) : TEXT(""),
		A ? ((VectorType == MCT_Float || VectorType == MCT_LWCScalar) ? TEXT("r") : TEXT("a")) : TEXT("")
		);

	FString SourceString = GetParameterCode(Vector);
	auto* Expression = GetParameterUniformExpression(Vector);
	if (Expression)
	{
		int8 Mask[4] = {-1, -1, -1, -1};
		for (int32 Index = 0; Index < MaskString.Len(); ++Index)
		{
			Mask[Index] = SwizzleComponentToIndex(MaskString[Index]);
		}
		return AddUniformExpression(
			new FMaterialUniformExpressionComponentSwizzle(Expression, Mask[0], Mask[1], Mask[2], Mask[3]),
			ResultType,
			TEXT("%s.%s"),
			*SourceString,
			*MaskString
			);
	}

	const EDerivativeStatus VectorDerivStatus = GetDerivativeStatus(Vector);
	FString CodeFinite;
	if (bIsLWC)
	{
		CodeFinite = ComponentMaskLWC(SourceString, VectorType, ResultType, R, G, B, A);
	}
	else
	{
		CodeFinite = FString::Printf(TEXT("%s.%s"), *SourceString, *MaskString);
	}

	if (IsAnalyticDerivEnabled() && IsDerivativeValid(VectorDerivStatus))
	{
		const FString VectorDeriv = GetParameterCodeDeriv(Vector, CompiledPDV_Analytic);

		FString CodeAnalytic;
		if (VectorDerivStatus == EDerivativeStatus::Valid)
		{
			FString ValueDeriv;
			if (bIsLWC)
			{
				ValueDeriv = ComponentMaskLWC(VectorDeriv + TEXT(".Value"), VectorType, ResultType, R, G, B, A);
			}
			else
			{
				ValueDeriv = FString::Printf(TEXT("%s.Value.%s"), *VectorDeriv, *MaskString);
			}

			CodeAnalytic = DerivativeAutogen.ConstructDeriv(
				ValueDeriv,
				FString::Printf(TEXT("%s.Ddx.%s"), *VectorDeriv, *MaskString),
				FString::Printf(TEXT("%s.Ddy.%s"), *VectorDeriv, *MaskString),
				GetDerivType(ResultType));

			return AddCodeChunkInnerDeriv(*CodeFinite, *CodeAnalytic, ResultType, false, EDerivativeStatus::Valid);
		}
		else
		{
			return AddInlinedCodeChunkZeroDeriv(ResultType, *CodeFinite);
		}
	}
	else
	{
		return AddInlinedCodeChunk(ResultType, *CodeFinite);
	}
}

int32 FHLSLMaterialTranslator::AppendVector(int32 A,int32 B)
{
	if(A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const EMaterialValueType TypeA = GetParameterType(A);
	const EMaterialValueType TypeB = GetParameterType(B);
	const bool bIsLWC = IsLWCType(TypeA) || IsLWCType(TypeB);
	const int32 NumComponentsA = GetNumComponents(TypeA);
	const int32 NumComponentsB = GetNumComponents(TypeB);
	const int32 NumResultComponents = NumComponentsA + NumComponentsB;
	if (NumResultComponents > 4)
	{
		return Errorf(TEXT("Can't append %s to %s"), DescribeType(TypeA), DescribeType(TypeB));
	}

	const EMaterialValueType ResultType = bIsLWC ? GetLWCVectorType(NumResultComponents) : GetVectorType(NumResultComponents);

	if(GetParameterUniformExpression(A) && GetParameterUniformExpression(B))
	{
		return AddUniformExpression(new FMaterialUniformExpressionAppendVector(GetParameterUniformExpression(A),GetParameterUniformExpression(B),GetNumComponents(GetParameterType(A))),ResultType,TEXT("MaterialFloat%u(%s,%s)"),NumResultComponents,*GetParameterCode(A),*GetParameterCode(B));
	}
	else
	{
		FString FiniteCode;
		if (bIsLWC)
		{
			FiniteCode = FString::Printf(TEXT("MakeLWCVector(LWCPromote(%s),LWCPromote(%s))"), *GetParameterCode(A), *GetParameterCode(B));
		}
		else
		{
			FiniteCode = FString::Printf(TEXT("MaterialFloat%u(%s,%s)"), NumResultComponents, *GetParameterCode(A), *GetParameterCode(B));
		}
		
		const EDerivativeStatus ADerivativeStatus = GetDerivativeStatus(A);
		const EDerivativeStatus BDerivativeStatus = GetDerivativeStatus(B);
		if (IsAnalyticDerivEnabled() && IsDerivativeValid(ADerivativeStatus) && IsDerivativeValid(BDerivativeStatus))
		{
			if (ADerivativeStatus == EDerivativeStatus::Zero && BDerivativeStatus == EDerivativeStatus::Zero)
			{
				return AddInlinedCodeChunkZeroDeriv(ResultType, *FiniteCode);
			}
			else
			{
				FString A_DDX = GetFloatZeroVector(NumComponentsA);
				FString A_DDY = GetFloatZeroVector(NumComponentsA);
				FString B_DDX = GetFloatZeroVector(NumComponentsB);
				FString B_DDY = GetFloatZeroVector(NumComponentsB);

				if (ADerivativeStatus == EDerivativeStatus::Valid)
				{
					FString Deriv = *GetParameterCodeDeriv(A, CompiledPDV_Analytic);
					A_DDX = Deriv + TEXT(".Ddx");
					A_DDY = Deriv + TEXT(".Ddy");
				}

				if (BDerivativeStatus == EDerivativeStatus::Valid)
				{
					FString Deriv = *GetParameterCodeDeriv(B, CompiledPDV_Analytic);
					B_DDX = Deriv + TEXT(".Ddx");
					B_DDY = Deriv + TEXT(".Ddy");
				}

				FString DDXCode = FString::Printf(TEXT("MaterialFloat%u(%s, %s)"), NumResultComponents, *A_DDX, *B_DDX);
				FString DDYCode = FString::Printf(TEXT("MaterialFloat%u(%s, %s)"), NumResultComponents, *A_DDY, *B_DDY);
				FString AnalyticCode = DerivativeAutogen.ConstructDeriv(FiniteCode, *DDXCode, *DDYCode, GetDerivType(ResultType));
				return AddCodeChunkInnerDeriv(*FiniteCode, *AnalyticCode, ResultType, false, EDerivativeStatus::Valid);
			}
		}
		else
		{
			return AddInlinedCodeChunk(ResultType, *FiniteCode);
		}
	}
}

static FString MultiplyMatrix(const TCHAR* Vector, const TCHAR* Matrix, int AWComponent)
{
	if (AWComponent)
	{
		return FString::Printf(TEXT("mul(MaterialFloat4(%s, 1.0f), %s).xyz"), Vector, Matrix);
	}
	else
	{
		return FString::Printf(TEXT("mul(%s, (MaterialFloat3x3)(%s))"), Vector, Matrix);
	}
}

static FString MultiplyTransposeMatrix(const TCHAR* Matrix, const TCHAR* Vector, int AWComponent)
{
	if (AWComponent)
	{
		return FString::Printf(TEXT("mul(%s, MaterialFloat4(%s, 1.0f)).xyz"), Matrix, Vector);
	}
	else
	{
		return FString::Printf(TEXT("mul((MaterialFloat3x3)(%s), %s)"), Matrix, Vector);
	}
}

static FString LWCMultiplyMatrix(const TCHAR* Vector, const TCHAR* Matrix, int AWComponent)
{
	if (AWComponent)
	{
		return FString::Printf(TEXT("LWCMultiply(%s, %s)"), Vector, Matrix);
	}
	else
	{
		return FString::Printf(TEXT("LWCMultiplyVector(%s, %s)"), Vector, Matrix);
	}
}

int32 FHLSLMaterialTranslator::TransformBase(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A, int AWComponent)
{
	if (A == INDEX_NONE)
	{
		// unable to compile
		return INDEX_NONE;
	}

	const EMaterialValueType SourceType = GetParameterType(A);
		
	{ // validation
		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex)
		{
			return NonPixelShaderExpressionError();
		}

		if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute && ShaderFrequency != SF_Vertex)
		{
			if ((SourceCoordBasis == MCB_Local || DestCoordBasis == MCB_Local))
			{
				return Errorf(TEXT("Local space is only supported for vertex, compute or pixel shader"));
			}
		}

		if (AWComponent != 0 && (SourceCoordBasis == MCB_Tangent || DestCoordBasis == MCB_Tangent))
		{
			return Errorf(TEXT("Tangent basis not available for position transformations"));
		}
		
		// Construct float3(0,0,x) out of the input if it is a scalar
		// This way artists can plug in a scalar and it will be treated as height, or a vector displacement
		const uint32 NumInputComponents = GetNumComponents(SourceType);
		if (NumInputComponents == 1u && SourceCoordBasis == MCB_Tangent)
		{
			A = AppendVector(Constant2(0, 0), A);
		}
		else if (NumInputComponents < 3u)
		{
			return Errorf(TEXT("input must be a 3-component vector (current: %s: %s) or a scalar (if source is Tangent)"), *GetParameterCode(A), DescribeType(SourceType));
		}
	}
		
	if (SourceCoordBasis == DestCoordBasis)
	{
		// no transformation needed
		return A;
	}
		
	FString CodeStr;
	FString CodeDerivStr;
	EMaterialCommonBasis IntermediaryBasis = MCB_World;

	switch (SourceCoordBasis)
	{
		case MCB_Tangent:
		{
			check(AWComponent == 0);
			if (DestCoordBasis == MCB_World)
			{
				CodeStr = TEXT("mul(<A>, Parameters.TangentToWorld)");
				CodeDerivStr = TEXT("mul(<A>, Parameters.TangentToWorld)");
			}
			// else use MCB_World as intermediary basis
			break;
		}
		case MCB_Local:
		{
			if (DestCoordBasis == MCB_World)
			{
				CodeStr = TEXT("TransformLocal<TO><PREV>World(Parameters, <A>)");
				CodeDerivStr = TEXT("TransformLocal<TO><PREV>World(Parameters, <A>)");
			}
			// else use MCB_World as intermediary basis
			break;
		}
		case MCB_TranslatedWorld:
		{
			if (DestCoordBasis == MCB_World)
			{
				if (AWComponent)
				{
					CodeStr = TEXT("LWCSubtract(<A>, ResolvedView.<PREV>PreViewTranslation)");
				}
				else
				{
					CodeStr = TEXT("<A>");
				}
				CodeDerivStr = TEXT("<A>");
			}
			else if (DestCoordBasis == MCB_Camera)
			{
				CodeStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>TranslatedWorldToCameraView"), AWComponent);
				CodeDerivStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>TranslatedWorldToCameraView"), 0);
			}
			else if (DestCoordBasis == MCB_View)
			{
				CodeStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>TranslatedWorldToView"), AWComponent);
				CodeDerivStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>TranslatedWorldToView"), 0);
			}
			// else use MCB_World as intermediary basis
			break;
		}
		case MCB_World:
		{
			if (DestCoordBasis == MCB_Tangent)
			{
				CodeStr = MultiplyTransposeMatrix(TEXT("Parameters.TangentToWorld"), TEXT("<A>"), AWComponent);
				CodeDerivStr = MultiplyTransposeMatrix(TEXT("Parameters.TangentToWorld"), TEXT("<A>"), 0);
			}
			else if (DestCoordBasis == MCB_Local)
			{
				const EMaterialDomain Domain = (const EMaterialDomain)Material->GetMaterialDomain();

				if(Domain != MD_Surface && Domain != MD_Volume)
				{
					// TODO: for decals we could support it
					Errorf(TEXT("This transformation is only supported in the 'Surface' material domain."));
					return INDEX_NONE;
				}

				// TODO: inconsistent with TransformLocal<TO>World with instancing
				CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("GetPrimitiveData(Parameters).<PREVIOUS>WorldToLocal"), AWComponent);
				CodeDerivStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("GetPrimitiveData(Parameters).<PREVIOUS>WorldToLocal"), 0);
			}
			else if (DestCoordBasis == MCB_TranslatedWorld)
			{
				if (AWComponent)
				{
					CodeStr = TEXT("LWCToFloat(LWCAdd(<A>, ResolvedView.<PREV>PreViewTranslation))");
				}
				else
				{
					CodeStr = TEXT("<A>");
				}
				CodeDerivStr = TEXT("<A>");
			}
			else if (DestCoordBasis == MCB_MeshParticle)
			{
				CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("Parameters.Particle.WorldToParticle"), AWComponent);
				CodeDerivStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("Parameters.Particle.WorldToParticle"), 0);
				bUsesParticleWorldToLocal = true;
			}
			else if (DestCoordBasis == MCB_Instance)
			{
				CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("GetWorldToInstance(Parameters)"), AWComponent);
				CodeDerivStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("GetWorldToInstance(Parameters)"), 0);
				bUsesInstanceWorldToLocalPS |= ShaderFrequency == SF_Pixel;
			}

			// else use MCB_TranslatedWorld as intermediary basis
			IntermediaryBasis = MCB_TranslatedWorld;
			break;
		}
		case MCB_Camera:
		{
			if (DestCoordBasis == MCB_TranslatedWorld)
			{
				CodeStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>CameraViewToTranslatedWorld"), AWComponent);
				CodeDerivStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>CameraViewToTranslatedWorld"), 0);
			}
			// else use MCB_TranslatedWorld as intermediary basis
			IntermediaryBasis = MCB_TranslatedWorld;
			break;
		}
		case MCB_View:
		{
			if (DestCoordBasis == MCB_TranslatedWorld)
			{
				CodeStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>ViewToTranslatedWorld"), AWComponent);
				CodeDerivStr = MultiplyMatrix(TEXT("<A>"), TEXT("ResolvedView.<PREV>ViewToTranslatedWorld"), 0);
			}
			// else use MCB_TranslatedWorld as intermediary basis
			IntermediaryBasis = MCB_TranslatedWorld;
			break;
		}
		case MCB_MeshParticle:
		{
			if (DestCoordBasis == MCB_World)
			{
				CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("Parameters.Particle.ParticleToWorld"), AWComponent);
				CodeDerivStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("Parameters.Particle.ParticleToWorld"), 0);
				bUsesParticleLocalToWorld = true;
			}
			// use World as an intermediary base
			break;
		}
		case MCB_Instance:
		{
			if (DestCoordBasis == MCB_World)
			{
				CodeStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("Get<PREV>InstanceToWorld(Parameters)"), AWComponent);
				CodeDerivStr = LWCMultiplyMatrix(TEXT("<A>"), TEXT("Get<PREV>InstanceToWorld(Parameters)"), 0);
				bUsesInstanceLocalToWorldPS |= ShaderFrequency == SF_Pixel;
			}
			// use World as an intermediary base
			break;
		}

		default:
			check(0);
			break;
	}

	if (CodeStr.IsEmpty())
	{
		// check intermediary basis so we don't have infinite recursion
		check(IntermediaryBasis != SourceCoordBasis);
		check(IntermediaryBasis != DestCoordBasis);

		// use intermediary basis
		const int32 IntermediaryA = TransformBase(SourceCoordBasis, IntermediaryBasis, A, AWComponent);

		return TransformBase(IntermediaryBasis, DestCoordBasis, IntermediaryA, AWComponent);
	}
		
	if (AWComponent != 0)
	{
		CodeStr.ReplaceInline(TEXT("<TO>"),TEXT("PositionTo"));
	}
	else
	{
		CodeStr.ReplaceInline(TEXT("<TO>"),TEXT("VectorTo"));
	}
	CodeDerivStr.ReplaceInline(TEXT("<TO>"), TEXT("VectorTo"));
		
	if (bCompilingPreviousFrame)
	{
		CodeStr.ReplaceInline(TEXT("<PREV>"),TEXT("Prev"));
		CodeStr.ReplaceInline(TEXT("<PREVIOUS>"), TEXT("Previous"));
		CodeDerivStr.ReplaceInline(TEXT("<PREV>"), TEXT("Prev"));
		CodeDerivStr.ReplaceInline(TEXT("<PREVIOUS>"), TEXT("Previous"));
	}
	else
	{
		CodeStr.ReplaceInline(TEXT("<PREV>"),TEXT(""));
		CodeStr.ReplaceInline(TEXT("<PREVIOUS>"), TEXT(""));
		CodeDerivStr.ReplaceInline(TEXT("<PREV>"), TEXT(""));
		CodeDerivStr.ReplaceInline(TEXT("<PREVIOUS>"), TEXT(""));
	}

	int32 CastA = A;
	if (SourceCoordBasis == MCB_World && AWComponent)
	{
		CastA = ValidCast(CastA, MCT_LWCVector3);
	}
	else
	{
		CastA = ValidCast(CastA, MCT_Float3);
	}

	CodeStr.ReplaceInline(TEXT("<A>"), *GetParameterCode(CastA));

	if (ShaderFrequency != SF_Vertex && (DestCoordBasis == MCB_Tangent || SourceCoordBasis == MCB_Tangent))
	{
		bUsesTransformVector = true;
	}

	const EMaterialValueType ResultType = (DestCoordBasis == MCB_World && AWComponent) ? MCT_LWCVector3 : MCT_Float3;
	const EDerivativeStatus ADerivStatus = GetDerivativeStatus(CastA);

	int32 Result;
	if (IsAnalyticDerivEnabled() && IsDerivativeValid(ADerivStatus))
	{
		if (ADerivStatus == EDerivativeStatus::Valid)
		{
			FString CastADeriv = GetParameterCodeDeriv(CastA, CompiledPDV_Analytic);
			FString CodeAnalytic = DerivativeAutogen.ConstructDeriv(
				CodeStr,
				CodeDerivStr.Replace(TEXT("<A>"), *(CastADeriv + TEXT(".Ddx"))),
				CodeDerivStr.Replace(TEXT("<A>"), *(CastADeriv + TEXT(".Ddy"))), GetDerivType(ResultType));
			Result = AddCodeChunkInnerDeriv(*CodeStr, *CodeAnalytic, ResultType, false, EDerivativeStatus::Valid);
		}
		else
		{
			Result = AddCodeChunkZeroDeriv(ResultType, *CodeStr);
		}
	}
	else
	{
		Result = AddCodeChunk(ResultType, *CodeStr);
	}
	
	return CastToNonLWCIfDisabled(Result);
}

int32 FHLSLMaterialTranslator::TransformVector(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A)
{
	return TransformBase(SourceCoordBasis, DestCoordBasis, A, 0);
}

int32 FHLSLMaterialTranslator::TransformPosition(EMaterialCommonBasis SourceCoordBasis, EMaterialCommonBasis DestCoordBasis, int32 A)
{
	return TransformBase(SourceCoordBasis, DestCoordBasis, A, 1);
}

int32 FHLSLMaterialTranslator::TransformNormalFromRequestedBasisToWorld(int32 NormalCodeChunk)
{
	if (IsTangentSpaceNormal())
	{
		// See TransformTangentNormalToWorld definitions in MaterialTemplate.ush
		return AddCodeChunk(MCT_Float3, TEXT("TransformTangentNormalToWorld(Parameters.TangentToWorld, %s)"), *GetParameterCode(NormalCodeChunk));
	}
	return NormalCodeChunk;
}

int32 FHLSLMaterialTranslator::DynamicParameter(FLinearColor& DefaultValue, uint32 ParameterIndex)
{
	if (ShaderFrequency != SF_Vertex && ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonVertexOrPixelShaderExpressionError();
	}

	DynamicParticleParameterMask |= (1 << ParameterIndex);

	int32 Default = Constant4(DefaultValue.R, DefaultValue.G, DefaultValue.B, DefaultValue.A);
	return AddInlinedCodeChunkZeroDeriv(
		MCT_Float4,
		TEXT("GetDynamicParameter(Parameters.Particle, %s, %u)"),
		*GetParameterCode(Default),
		ParameterIndex
		);
}

int32 FHLSLMaterialTranslator::LightmapUVs()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}

	if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	bUsesLightmapUVs = true;

	const TCHAR* FiniteCode = TEXT("GetLightmapUVs(Parameters)");
	if (IsAnalyticDerivEnabled())
	{
		FString AnalyticCode = DerivativeAutogen.ConstructDeriv(
			FiniteCode,
			TEXT("GetLightmapUVs_DDX(Parameters)"),
			TEXT("GetLightmapUVs_DDY(Parameters)"), EDerivativeType::Float2);
		return AddCodeChunkInnerDeriv(FiniteCode, *AnalyticCode, MCT_Float2, false, EDerivativeStatus::Valid);
	}
	else
	{
		return AddCodeChunk(MCT_Float2, FiniteCode);
	}
}

int32 FHLSLMaterialTranslator::PrecomputedAOMask()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}

	if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	bUsesAOMaterialMask = true;

	int32 ResultIdx = INDEX_NONE;
	FString CodeChunk = FString::Printf(TEXT("Parameters.AOMaterialMask"));
	ResultIdx = AddCodeChunk(
		MCT_Float,
		*CodeChunk
		);
	return ResultIdx;
}

int32 FHLSLMaterialTranslator::GenericSwitch(const TCHAR* SwitchExpressionText, int32 IfTrue, int32 IfFalse)
{
	if (IfTrue == INDEX_NONE || IfFalse == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// exactly the same inputs on both sides - no need to generate anything extra
	if (IfTrue == IfFalse)
	{
		return IfTrue;
	}

	FMaterialUniformExpression* IfTrueExpression = GetParameterUniformExpression(IfTrue);
	FMaterialUniformExpression* IfFalseExpression = GetParameterUniformExpression(IfFalse);
	if (IfTrueExpression &&
		IfFalseExpression &&
		IfTrueExpression->IsConstant() &&
		IfFalseExpression->IsConstant())
	{
		FMaterialRenderContext DummyContext(nullptr, *Material, nullptr);
		FLinearColor IfTrueValue;
		FLinearColor IfFalseValue;
		IfTrueExpression->GetNumberValue(DummyContext, IfTrueValue);
		IfFalseExpression->GetNumberValue(DummyContext, IfFalseValue);
		if (IfTrueValue == IfFalseValue)
		{
			// If both inputs are wired to == constant values, avoid adding the runtime switch
			// This will avoid breaking various offline checks for constant values
			return IfTrue;
		}
	}

	// Both branches of '?:' need to be the same type
	const EMaterialValueType ResultType = GetArithmeticResultType(IfTrue, IfFalse);
	const FString IfTrueCode = CoerceParameter(IfTrue, ResultType);
	const FString IfFalseCode = CoerceParameter(IfFalse, ResultType);
	
	if (IsLWCType(ResultType))
	{
		return AddCodeChunk(ResultType, TEXT("LWCSelect(%s, %s, %s)"), SwitchExpressionText, *IfTrueCode, *IfFalseCode);
	}
	else
	{
		return AddCodeChunk(ResultType, TEXT("(%s ? (%s) : (%s))"), SwitchExpressionText, *IfTrueCode, *IfFalseCode);
	}
}

int32 FHLSLMaterialTranslator::GIReplace(int32 Direct, int32 StaticIndirect, int32 DynamicIndirect)
{ 
	return GenericSwitch(TEXT("GetGIReplaceState()"), DynamicIndirect, Direct);
}

int32 FHLSLMaterialTranslator::ShadowReplace(int32 Default, int32 Shadow)
{
	return GenericSwitch(TEXT("GetShadowReplaceState()"), Shadow, Default);
}

int32 FHLSLMaterialTranslator::NaniteReplace(int32 Default, int32 Nanite)
{
	return GenericSwitch(TEXT("GetNaniteReplaceState()"), Nanite, Default);
}

int32 FHLSLMaterialTranslator::ReflectionCapturePassSwitch(int32 Default, int32 Reflection)
{
	return GenericSwitch(TEXT("GetReflectionCapturePassSwitchState()"), Reflection, Default);
}

int32 FHLSLMaterialTranslator::RayTracingQualitySwitchReplace(int32 Normal, int32 RayTraced)
{
	return GenericSwitch(TEXT("GetRayTracingQualitySwitch()"), RayTraced, Normal);
}

int32 FHLSLMaterialTranslator::PathTracingQualitySwitchReplace(int32 Normal, int32 PathTraced)
{
	return GenericSwitch(TEXT("GetPathTracingQualitySwitch()"), PathTraced, Normal);
}

int32 FHLSLMaterialTranslator::PathTracingRayTypeSwitch(int32 Main, int32 Shadow, int32 IndirectDiffuse, int32 IndirectSpecular, int32 IndirectVolume)
{
	// Generate a sequential series of switches so we can easily account for type promotion across the ports
	// Any input port that does not have anything connected to it (INDEX_NONE) defaults back to Main
	int32 TmpA = GenericSwitch(TEXT("GetPathTracingIsShadow()")          , Shadow           == INDEX_NONE ? Main : Shadow          , Main);
	int32 TmpB = GenericSwitch(TEXT("GetPathTracingIsIndirectDiffuse()") , IndirectDiffuse  == INDEX_NONE ? Main : IndirectDiffuse , TmpA);
	int32 TmpC = GenericSwitch(TEXT("GetPathTracingIsIndirectSpecular()"), IndirectSpecular == INDEX_NONE ? Main : IndirectSpecular, TmpB);
	int32 TmpD = GenericSwitch(TEXT("GetPathTracingIsIndirectVolume()")  , IndirectVolume   == INDEX_NONE ? Main : IndirectVolume  , TmpC);
	return TmpD;
}

int32 FHLSLMaterialTranslator::LightmassReplace(int32 Realtime, int32 Lightmass)
{
	return GenericSwitch(TEXT("GetLightmassReplaceState()"), Lightmass, Realtime);
}

int32 FHLSLMaterialTranslator::VirtualTextureOutputReplace(int32 Default, int32 VirtualTexture)
{
	return GenericSwitch(TEXT("GetRuntimeVirtualTextureOutputSwitch()"), VirtualTexture, Default);
}

int32 FHLSLMaterialTranslator::ObjectOrientation()
{ 
	return AddInlinedCodeChunkZeroDeriv(MCT_Float3,TEXT("GetObjectOrientation(Parameters)"));
}

int32 FHLSLMaterialTranslator::RotateAboutAxis(int32 NormalizedRotationAxisAndAngleIndex, int32 PositionOnAxisIndex, int32 PositionIndex)
{
	if (NormalizedRotationAxisAndAngleIndex == INDEX_NONE
		|| PositionOnAxisIndex == INDEX_NONE
		|| PositionIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	else
	{
		const EMaterialValueType PositionOnAxisType = GetParameterType(PositionOnAxisIndex);
		const EMaterialValueType PositionType = GetParameterType(PositionIndex);
		const EMaterialValueType InputType = IsLWCType(PositionOnAxisType) || IsLWCType(PositionType) ? MCT_LWCVector3 : MCT_Float3;
		return AddCodeChunk(
			MCT_Float3,
			TEXT("RotateAboutAxis(%s,%s,%s)"),
			*CoerceParameter(NormalizedRotationAxisAndAngleIndex,MCT_Float4),
			*CoerceParameter(PositionOnAxisIndex, InputType),
			*CoerceParameter(PositionIndex, InputType)
			);	
	}
}

int32 FHLSLMaterialTranslator::TwoSidedSign()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}
	return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("Parameters.TwoSidedSign"));
}

int32 FHLSLMaterialTranslator::VertexNormal()
{
	if (ShaderFrequency != SF_Vertex)
	{
		bUsesTransformVector = true;
	}
	return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.TangentToWorld[2]"));	
}

int32 FHLSLMaterialTranslator::VertexTangent()
{
	if (ShaderFrequency != SF_Vertex)
	{
		bUsesTransformVector = true;
	}
	return AddInlinedCodeChunk(MCT_Float3, TEXT("Parameters.TangentToWorld[0]"));
}

int32 FHLSLMaterialTranslator::PixelNormalWS()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Compute)
	{
		return NonPixelShaderExpressionError();
	}
	if(MaterialProperty == MP_Normal)
	{
		return Errorf(TEXT("Invalid node PixelNormalWS used for Normal input."));
	}
	if (ShaderFrequency != SF_Vertex)
	{
		bUsesTransformVector = true;
	}
	return AddInlinedCodeChunk(MCT_Float3,TEXT("Parameters.WorldNormal"));	
}

int32 FHLSLMaterialTranslator::Derivative(int32 A, const TCHAR* Component)
{
	if (A == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (ShaderFrequency == SF_Compute)
	{
		// running a material in a compute shader pass (e.g. when using SVOGI)
		return AddInlinedCodeChunk(MCT_Float, TEXT("0"));
	}

	if (ShaderFrequency != SF_Pixel)
	{
		return NonPixelShaderExpressionError();
	}

	const FDerivInfo ADerivInfo = GetDerivInfo(A);
	const EMaterialValueType ResultType = MakeNonLWCType(ADerivInfo.Type);
	const TCHAR* FunctionName = IsLWCType(ADerivInfo.Type) ? TEXT("LWCDd") : TEXT("dd");
    
    // Spirv expects DDX/Y to have 32 bit width, so need to ensure we use float and not half
    const EMaterialValueType ParameterType = GetParameterType(A);
    const TCHAR* CastType = TEXT("");
    
    switch (ParameterType)
    {
    	case MCT_Float1: case MCT_Float: CastType = TEXT("(float)"); break;
	    case MCT_Float2: CastType = TEXT("(float2)"); break;
	    case MCT_Float3: CastType = TEXT("(float3)"); break;
    	case MCT_Float4: CastType = TEXT("(float4)"); break;
    }
            
	const FString FiniteCode = FString::Printf(TEXT("%s%s(%s%s)"), FunctionName, Component, CastType, *GetParameterCode(A));

	if (IsAnalyticDerivEnabled() && ADerivInfo.DerivativeStatus == EDerivativeStatus::Valid)
	{
		FString ADeriv = GetParameterCodeDeriv(A, CompiledPDV_Analytic);

		FString AnalyticCode = DerivativeAutogen.ConstructDeriv(
			FString::Printf(TEXT("%s.Dd%s"), *ADeriv, Component),
			FString::Printf(TEXT("dd%s(%s.Ddx)"), Component, *ADeriv),
			FString::Printf(TEXT("dd%s(%s.Ddy)"), Component, *ADeriv),
			MakeNonLWCType(ADerivInfo.DerivType)
		);

		return AddCodeChunkInnerDeriv(*FiniteCode, *AnalyticCode, ResultType, false, EDerivativeStatus::Valid);
	}
	else
	{
		return AddCodeChunkInnerDeriv(*FiniteCode, ResultType, false, (ADerivInfo.DerivativeStatus == EDerivativeStatus::Zero) ? EDerivativeStatus::Zero : EDerivativeStatus::NotValid);
	}
}

int32 FHLSLMaterialTranslator::DDX( int32 A )
{
	return Derivative(A, TEXT("x"));
}

int32 FHLSLMaterialTranslator::DDY( int32 A )
{
	return Derivative(A, TEXT("y"));
}

int32 FHLSLMaterialTranslator::AntialiasedTextureMask(int32 Tex, int32 UV, float Threshold, uint8 Channel)
{
	if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (Tex == INDEX_NONE || UV == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	int32 ThresholdConst = Constant(Threshold);
	int32 ChannelConst = Constant(Channel);
	FString TextureName = CoerceParameter(Tex, GetParameterType(Tex));

	return AddCodeChunk(MCT_Float, 
		TEXT("AntialiasedTextureMask(%s,%sSampler,%s,%s,%s)"), 
		*GetParameterCode(Tex),
		*TextureName,
		*GetParameterCode(UV),
		*GetParameterCode(ThresholdConst),
		*GetParameterCode(ChannelConst));
}

int32 FHLSLMaterialTranslator::DepthOfFieldFunction(int32 Depth, int32 FunctionValueIndex)
{
	if (Depth == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	return AddCodeChunk(MCT_Float, 
		TEXT("MaterialExpressionDepthOfFieldFunction(%s, %d)"), 
		*GetParameterCode(Depth), FunctionValueIndex);
}

int32 FHLSLMaterialTranslator::Sobol(int32 Cell, int32 Index, int32 Seed)
{
	AddEstimatedTextureSample(2);

	return AddCodeChunk(MCT_Float2,
		TEXT("floor(%s) + float2(SobolIndex(SobolPixel(uint2(%s)), uint(%s)) ^ uint2(%s * 0x10000) & 0xffff) / 0x10000"),
		*GetParameterCode(Cell),
		*GetParameterCode(Cell),
		*GetParameterCode(Index),
		*GetParameterCode(Seed));
}

int32 FHLSLMaterialTranslator::TemporalSobol(int32 Index, int32 Seed)
{
	AddEstimatedTextureSample(2);

	return AddCodeChunk(MCT_Float2,
		TEXT("float2(SobolIndex(SobolPixel(uint2(Parameters.SvPosition.xy)), uint(View.StateFrameIndexMod8 + 8 * %s)) ^ uint2(%s * 0x10000) & 0xffff) / 0x10000"),
		*GetParameterCode(Index),
		*GetParameterCode(Seed));
}

int32 FHLSLMaterialTranslator::Noise(int32 Position, float Scale, int32 Quality, uint8 NoiseFunction, bool bTurbulence, int32 Levels, float OutputMin, float OutputMax, float LevelScale, int32 FilterWidth, bool bTiling, uint32 RepeatSize)
{
	if(Position == INDEX_NONE || FilterWidth == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (NoiseFunction == NOISEFUNCTION_SimplexTex ||
		NoiseFunction == NOISEFUNCTION_GradientTex ||
		NoiseFunction == NOISEFUNCTION_GradientTex3D)
	{
		AddEstimatedTextureSample();
	}

	// to limit performance problems due to values outside reasonable range
	Levels = FMath::Clamp(Levels, 1, 10);

	int32 ScaleConst = Constant(Scale);
	int32 QualityConst = Constant(Quality);
	int32 NoiseFunctionConst = Constant(NoiseFunction);
	int32 TurbulenceConst = Constant(bTurbulence);
	int32 LevelsConst = Constant(Levels);
	int32 OutputMinConst = Constant(OutputMin);
	int32 OutputMaxConst = Constant(OutputMax);
	int32 LevelScaleConst = Constant(LevelScale);
	int32 TilingConst = Constant(bTiling);
	int32 RepeatSizeConst = Constant(RepeatSize);

	const EMaterialValueType PositionType = GetParameterType(Position);
	if (IsLWCType(PositionType))
	{
		// If Noise is driven by a LWC position, just take the offset within the current tile
		// Will generate discontinuity in noise at tile boudaries
		// Could potentially add noise functions that operate directly on LWC values, but that would be very expensive
		Position = AddCodeChunk(MCT_Float3, TEXT("LWCNormalizeTile(%s).Offset"), *CoerceParameter(Position, MCT_LWCVector3));
	}

	return AddCodeChunk(MCT_Float, 
		TEXT("MaterialExpressionNoise(%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s)"), 
		*CoerceParameter(Position, MCT_Float3),
		*GetParameterCode(ScaleConst),
		*GetParameterCode(QualityConst),
		*GetParameterCode(NoiseFunctionConst),
		*GetParameterCode(TurbulenceConst),
		*GetParameterCode(LevelsConst),
		*GetParameterCode(OutputMinConst),
		*GetParameterCode(OutputMaxConst),
		*GetParameterCode(LevelScaleConst),
		*GetParameterCode(FilterWidth),
		*GetParameterCode(TilingConst),
		*GetParameterCode(RepeatSizeConst));
}

int32 FHLSLMaterialTranslator::VectorNoise(int32 Position, int32 Quality, uint8 NoiseFunction, bool bTiling, uint32 TileSize)
{
	if (Position == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	int32 QualityConst = Constant(Quality);
	int32 NoiseFunctionConst = Constant(NoiseFunction);
	int32 TilingConst = Constant(bTiling);
	int32 TileSizeConst = Constant(TileSize);

	if (NoiseFunction == VNF_GradientALU || NoiseFunction == VNF_VoronoiALU)
	{
		return AddCodeChunk(MCT_Float4,
			TEXT("MaterialExpressionVectorNoise(%s,%s,%s,%s,%s)"),
			*CoerceParameter(Position, MCT_Float3), // LWC_TODO - maybe possible/useful to add LWC-aware noise functions
			*GetParameterCode(QualityConst),
			*GetParameterCode(NoiseFunctionConst),
			*GetParameterCode(TilingConst),
			*GetParameterCode(TileSizeConst));
	}
	else
	{
		return AddCodeChunk(MCT_Float3,
			TEXT("MaterialExpressionVectorNoise(%s,%s,%s,%s,%s).xyz"),
			*CoerceParameter(Position, MCT_Float3),
			*GetParameterCode(QualityConst),
			*GetParameterCode(NoiseFunctionConst),
			*GetParameterCode(TilingConst),
			*GetParameterCode(TileSizeConst));
	}
}

int32 FHLSLMaterialTranslator::BlackBody( int32 Temp )
{
	if( Temp == INDEX_NONE )
	{
		return INDEX_NONE;
	}

	return AddCodeChunk( MCT_Float3, TEXT("MaterialExpressionBlackBody(%s)"), *GetParameterCode(Temp) );
}

int32 FHLSLMaterialTranslator::GetHairUV()
{
	return AddCodeChunk(MCT_Float2, TEXT("MaterialExpressionGetHairUV(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairDimensions()
{
	return AddCodeChunk(MCT_Float2, TEXT("MaterialExpressionGetHairDimensions(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairSeed()
{
	return AddCodeChunk(MCT_Float1, TEXT("MaterialExpressionGetHairSeed(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairClumpID()
{
	return AddCodeChunk(MCT_Float1, TEXT("MaterialExpressionGetHairClumpID(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairTangent(bool bUseTangentSpace)
{
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionGetHairTangent(Parameters, %s)"), bUseTangentSpace ? TEXT("true") : TEXT("false"));
}

int32 FHLSLMaterialTranslator::GetHairRootUV()
{
	return AddCodeChunk(MCT_Float2, TEXT("MaterialExpressionGetHairRootUV(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairBaseColor()
{
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionGetHairBaseColor(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairRoughness()
{
	return AddCodeChunk(MCT_Float1, TEXT("MaterialExpressionGetHairRoughness(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairAO()
{
	return AddCodeChunk(MCT_Float1, TEXT("MaterialExpressionGetHairAO(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairDepth()
{
	return AddCodeChunk(MCT_Float1, TEXT("MaterialExpressionGetHairDepth(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairCoverage()
{
	return AddCodeChunk(MCT_Float1, TEXT("MaterialExpressionGetHairCoverage(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairAuxilaryData()
{
	return AddCodeChunk(MCT_Float4, TEXT("MaterialExpressionGetHairAuxilaryData(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairAtlasUVs()
{
	return AddCodeChunk(MCT_Float2, TEXT("MaterialExpressionGetAtlasUVs(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairGroupIndex()
{
	return AddCodeChunk(MCT_Float1, TEXT("MaterialExpressionGetHairGroupIndex(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetHairColorFromMelanin(int32 Melanin, int32 Redness, int32 DyeColor)
{
	if (Melanin == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (Redness == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (DyeColor == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionGetHairColorFromMelanin(%s, %s, %s)"), *GetParameterCode(Melanin), *GetParameterCode(Redness), *GetParameterCode(DyeColor));
}

int32 FHLSLMaterialTranslator::DistanceToNearestSurface(int32 PositionArg)
{
	if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (PositionArg == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	MaterialCompilationOutput.bUsesGlobalDistanceField = true;

	return AddCodeChunk(MCT_Float, TEXT("GetDistanceToNearestSurfaceGlobal(%s)"), *CoerceParameter(PositionArg, MCT_LWCVector3));
}

int32 FHLSLMaterialTranslator::DistanceFieldGradient(int32 PositionArg)
{
	if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (PositionArg == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	MaterialCompilationOutput.bUsesGlobalDistanceField = true;

	return AddCodeChunk(MCT_Float3, TEXT("GetDistanceFieldGradientGlobal(%s)"), *CoerceParameter(PositionArg, MCT_LWCVector3));
}

int32 FHLSLMaterialTranslator::DistanceFieldApproxAO(int32 PositionArg, int32 NormalArg, int32 BaseDistanceArg, int32 RadiusArg, uint32 NumSteps, float StepScale)
{
	if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (PositionArg == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (NormalArg == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (BaseDistanceArg == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (RadiusArg == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	MaterialCompilationOutput.bUsesGlobalDistanceField = true;

	NumSteps = FMath::Clamp(NumSteps, 1, 4);
	StepScale = FMath::Max(StepScale, 1.0f);

	const int32 NumStepsConst = Constant(NumSteps);
	const int32 NumStepsMinus1Const = Constant(NumSteps - 1);
	const int32 StepScaleConst = Constant(StepScale);

	int32 StepDistance;
	int32 DistanceBias;
	int32 MaxDistance;

	if (NumSteps == 1)
	{
		StepDistance = Constant(0);
		DistanceBias = BaseDistanceArg;
		MaxDistance = BaseDistanceArg;
	}
	else
	{
		StepDistance = Div(Sub(RadiusArg, BaseDistanceArg), Sub(Power(StepScaleConst, NumStepsMinus1Const), Constant(1)));
		DistanceBias = Sub(BaseDistanceArg, StepDistance);
		MaxDistance = RadiusArg;
	}

	return AddCodeChunk(MCT_Float,
		TEXT("CalculateDistanceFieldApproxAO(%s, %s, %s, %s, %s, %s, %s)"),
		*CoerceParameter(PositionArg, MCT_LWCVector3),
		*CoerceParameter(NormalArg, MCT_Float3),
		*GetParameterCode(NumStepsConst),
		*CoerceParameter(StepDistance, MCT_Float),
		*GetParameterCode(StepScaleConst),
		*CoerceParameter(DistanceBias, MCT_Float),
		*CoerceParameter(MaxDistance, MCT_Float));
}

int32 FHLSLMaterialTranslator::SamplePhysicsField(int32 PositionArg, const int32 OutputType, const int32 TargetIndex)
{
	if (ErrorUnlessFeatureLevelSupported(ERHIFeatureLevel::SM5) == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (PositionArg == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// LWC_TODO: LWC aware physics field
	if (TargetIndex != INDEX_NONE)
	{
		if (OutputType == EFieldOutputType::Field_Output_Vector)
		{
			return AddCodeChunk(MCT_Float3, TEXT("MatPhysicsField_SamplePhysicsVectorField(%s,%d)"), *CoerceParameter(PositionArg, MCT_Float3), static_cast<uint8>(TargetIndex));
		}
		else if (OutputType == EFieldOutputType::Field_Output_Scalar)
		{
			return AddCodeChunk(MCT_Float, TEXT("MatPhysicsField_SamplePhysicsScalarField(%s,%d)"), *CoerceParameter(PositionArg, MCT_Float3), static_cast<uint8>(TargetIndex));
		}
		else if (OutputType == EFieldOutputType::Field_Output_Integer)
		{
			return AddCodeChunk(MCT_Float, TEXT("MatPhysicsField_SamplePhysicsIntegerField(%s,%d)"), *CoerceParameter(PositionArg, MCT_Float3), static_cast<uint8>(TargetIndex));
		}
		else
		{
			return INDEX_NONE;
		}
	}
	else
	{
		return INDEX_NONE;
	}
}

int32 FHLSLMaterialTranslator::AtmosphericFogColor( int32 WorldPosition )
{
	return SkyAtmosphereAerialPerspective(WorldPosition);
}

int32 FHLSLMaterialTranslator::AtmosphericLightVector()
{
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionAtmosphericLightVector(Parameters)"));
}

int32 FHLSLMaterialTranslator::AtmosphericLightColor()
{
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionAtmosphericLightColor(Parameters)"));
}

int32 FHLSLMaterialTranslator::SkyAtmosphereLightIlluminance(int32 WorldPosition, int32 LightIndex)
{
	bUsesSkyAtmosphere = true;
	FString WorldPosCode = WorldPosition == INDEX_NONE ? FString(TEXT("Parameters.AbsoluteWorldPosition")) : CoerceParameter(WorldPosition, MCT_LWCVector3);
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionSkyAtmosphereLightIlluminance(Parameters, %s, %d)"), *WorldPosCode, LightIndex);
}

int32 FHLSLMaterialTranslator::SkyAtmosphereLightDirection(int32 LightIndex)
{
	bUsesSkyAtmosphere = true;
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionSkyAtmosphereLightDirection(Parameters, %d)"), LightIndex);
}

int32 FHLSLMaterialTranslator::SkyAtmosphereLightDiskLuminance(int32 LightIndex, int32 OverrideAtmosphereLightDiscCosHalfApexAngle)
{
	bUsesSkyAtmosphere = true;
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionSkyAtmosphereLightDiskLuminance(Parameters, %d, %s)"), LightIndex, 
		OverrideAtmosphereLightDiscCosHalfApexAngle == INDEX_NONE ? *GetParameterCode(Constant(-1.0f)) : *GetParameterCode(OverrideAtmosphereLightDiscCosHalfApexAngle));
}

int32 FHLSLMaterialTranslator::SkyAtmosphereViewLuminance()
{
	bUsesSkyAtmosphere = true;
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionSkyAtmosphereViewLuminance(Parameters)"));
}

int32 FHLSLMaterialTranslator::SkyAtmosphereAerialPerspective(int32 WorldPosition)
{
	bUsesSkyAtmosphere = true;
	FString WorldPosCode = WorldPosition == INDEX_NONE ? FString(TEXT("Parameters.AbsoluteWorldPosition")) : CoerceParameter(WorldPosition, MCT_LWCVector3);
	return AddCodeChunk(MCT_Float4, TEXT("MaterialExpressionSkyAtmosphereAerialPerspective(Parameters, %s)"), *WorldPosCode);
}

int32 FHLSLMaterialTranslator::SkyAtmosphereDistantLightScatteredLuminance()
{
	bUsesSkyAtmosphere = true;
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionSkyAtmosphereDistantLightScatteredLuminance(Parameters)"));
}

int32 FHLSLMaterialTranslator::SkyLightEnvMapSample(int32 DirectionCodeChunk, int32 RoughnessCodeChunk)
{
	if (DirectionCodeChunk == INDEX_NONE || RoughnessCodeChunk == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (Material->GetMaterialDomain() != MD_Surface)
	{
		return Errorf(TEXT("The SkyLightEnvMapSample node can only be used when material Domain is set to Surface."));
	}

	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionSkyLightEnvMapSample(%s, %s)"), *GetParameterCode(DirectionCodeChunk), *GetParameterCode(RoughnessCodeChunk));
}

int32 FHLSLMaterialTranslator::SceneDepthWithoutWater(int32 Offset, int32 ViewportUV, bool bUseOffset, float FallbackDepth)
{
	if (ShaderFrequency == SF_Vertex)
	{
		// Mobile currently does not support this, we need to read a separate copy of the depth, we must disable framebuffer fetch and force scene texture reads.
		// (Texture bindings are not setup properly for any platform so we're disallowing usage in vertex shader altogether now)
		return Errorf(TEXT("Cannot read scene depth without water from the vertex shader."));
	}

	const EMaterialDomain MaterialDomain = Material->GetMaterialDomain();

	if (MaterialDomain != MD_PostProcess)
	{
		if (!Material->GetShadingModels().HasShadingModel(MSM_SingleLayerWater))
		{
			return Errorf(TEXT("Can only read scene depth below water when material Shading Model is Single Layer Water or when material Domain is PostProcess."));
		}

		if (MaterialDomain != MD_Surface)
		{
			return Errorf(TEXT("Can only read scene depth below water when material Domain is set to Surface or PostProcess."));
		}

		if (IsTranslucentBlendMode(Material->GetBlendMode()))
		{
			return Errorf(TEXT("Can only read scene depth below water when material Blend Mode isn't translucent."));
		}
	}

	if (Offset == INDEX_NONE && bUseOffset)
	{
		return INDEX_NONE;
	}

	AddEstimatedTextureSample();

	const FString UserDepthCode(TEXT("MaterialExpressionSceneDepthWithoutWater(%s, %s)"));
	const FString FallbackString(FString::SanitizeFloat(FallbackDepth));
	const int32 TexCoordCode = GetScreenAlignedUV(Offset, ViewportUV, bUseOffset);

	// add the code string
	return AddCodeChunk(
		MCT_Float,
		*UserDepthCode,
		*GetParameterCode(TexCoordCode),
		*FallbackString
	);
}

int32 FHLSLMaterialTranslator::GetCloudSampleAltitude()
{
	return AddCodeChunk(MCT_Float, TEXT("MaterialExpressionCloudSampleAltitude(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetCloudSampleAltitudeInLayer()
{
	return AddCodeChunk(MCT_Float, TEXT("MaterialExpressionCloudSampleAltitudeInLayer(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetCloudSampleNormAltitudeInLayer()
{
	return AddCodeChunk(MCT_Float, TEXT("MaterialExpressionCloudSampleNormAltitudeInLayer(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetCloudSampleShadowSampleDistance()
{
	return AddCodeChunk(MCT_Float, TEXT("MaterialExpressionVolumeSampleShadowSampleDistance(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetVolumeSampleConservativeDensity()
{
	return AddCodeChunk(MCT_Float4, TEXT("MaterialExpressionVolumeSampleConservativeDensity(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetCloudEmptySpaceSkippingSphereCenterWorldPosition()
{
	return AddCodeChunk(MCT_Float3, TEXT("MaterialExpressionCloudEmptySpaceSkippingSphereCenterWorldPosition(Parameters)"));
}

int32 FHLSLMaterialTranslator::GetCloudEmptySpaceSkippingSphereRadius()
{
	return AddCodeChunk(MCT_Float1, TEXT("MaterialExpressionCloudEmptySpaceSkippingSphereRadius(Parameters)"));
}

int32 FHLSLMaterialTranslator::CustomPrimitiveData(int32 OutputIndex, EMaterialValueType Type)
{
	check(OutputIndex < FCustomPrimitiveData::NumCustomPrimitiveDataFloats);

	const int32 NumComponents = GetNumComponents(Type);

	FString HlslCode;
			
	// Only float2, float3 and float4 need this
	if (NumComponents > 1)
	{
		HlslCode.Append(FString::Printf(TEXT("float%d("), NumComponents));
	}

	for (int i = 0; i < NumComponents; i++)
	{
		const int32 CurrentOutputIndex = OutputIndex + i;

		// Check if we are accessing inside the array, otherwise default to 0
		if (CurrentOutputIndex < FCustomPrimitiveData::NumCustomPrimitiveDataFloats)
		{
			const int32 CustomDataIndex = CurrentOutputIndex / 4;
			const int32 ElementIndex = CurrentOutputIndex % 4; // Index x, y, z or w

			HlslCode.Append(FString::Printf(TEXT("GetPrimitiveData(Parameters).CustomPrimitiveData[%d][%d]"), CustomDataIndex, ElementIndex));
		}
		else
		{
			HlslCode.Append(TEXT("0.0f"));
		}

		if (i+1 < NumComponents)
		{
			HlslCode.Append(", ");
		}
	}

	// This is the matching parenthesis to the first append
	if (NumComponents > 1)
	{
		HlslCode.AppendChar(')');
	}

	return AddCodeChunk(Type, TEXT("%s"), *HlslCode);
}

int32 FHLSLMaterialTranslator::ShadingModel(EMaterialShadingModel InSelectedShadingModel)
{
	// If the shading model is masked out, fallback to default shading model
	uint32 PlatformShadingModelsMask = GetPlatformShadingModelsMask(Platform);
	if ((PlatformShadingModelsMask & (1u << (uint32)InSelectedShadingModel)) == 0)
	{
		InSelectedShadingModel = MSM_DefaultLit;
	}
	
	if (InSelectedShadingModel < MSM_NUM)
	{
		ShadingModelsFromCompilation.AddShadingModel(InSelectedShadingModel);
	}
	return AddInlinedCodeChunk(MCT_ShadingModel, TEXT("%d"), InSelectedShadingModel);
}

int32 FHLSLMaterialTranslator::DefaultMaterialAttributes()
{
	return AddInlinedCodeChunk(MCT_MaterialAttributes, TEXT("DefaultMaterialAttributes"));
}

int32 FHLSLMaterialTranslator::SetMaterialAttribute(int32 MaterialAttributes, int32 Value, const FGuid& AttributeID)
{
	const EMaterialProperty Property = FMaterialAttributeDefinitionMap::GetProperty(AttributeID);
	const FString PropertyName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
	const EMaterialValueType PropertyType = FMaterialAttributeDefinitionMap::GetValueType(AttributeID);
	const EShaderFrequency Frequency = FMaterialAttributeDefinitionMap::GetShaderFrequency(AttributeID);
	
	if (MaterialAttributes == INDEX_NONE ||
		GetParameterType(MaterialAttributes) != MCT_MaterialAttributes)
	{
		return Error(TEXT("Expected MaterialAttributes"));
	}

	if (Frequency != ShaderFrequency)
	{
		return Errorf(TEXT("Can't set material attribute %s from shader stage %d"), *PropertyName, ShaderFrequency);
	}

	const int32 CastValue = ValidCast(Value, PropertyType);
	FMaterialUniformExpression* UniformExpression = GetParameterUniformExpression(CastValue);
	bool bSetDefaultValue = false;
	if (UniformExpression && UniformExpression->IsConstant())
	{
		FMaterialRenderContext Context(nullptr, *Material, nullptr);
		FLinearColor ConstantValue(ForceInitToZero);
		UniformExpression->GetNumberValue(Context, ConstantValue);
		const FVector4f DefaultValue = FMaterialAttributeDefinitionMap::GetDefaultValue(AttributeID);
		bSetDefaultValue = (ConstantValue == FLinearColor(DefaultValue));
	}

	uint64 AttributeMask = GetParameterMaterialAttributeMask(MaterialAttributes);
	if (bSetDefaultValue)
	{
		if (!(AttributeMask & (1ull << Property)))
		{
			// Setting default value to an input that already has the default value set, this is a NOP
			return MaterialAttributes;
		}
		// Otherwise, explicitly set the value back to default
		AttributeMask &= ~(1ull << Property);
	}
	else
	{
		// Setting a non-default value
		AttributeMask |= (1ull << Property);
	}

	const int32 Result = AddCodeChunk(MCT_MaterialAttributes, TEXT("FMaterialAttributes_Set%s(%s, %s)"),
		*PropertyName,
		*GetParameterCode(MaterialAttributes),
		*GetParameterCode(CastValue));

	SetParameterMaterialAttributes(Result, AttributeMask);

	return Result;
}

int32 FHLSLMaterialTranslator::BeginScope()
{
	const int32 Result = AddCodeChunk(MCT_VoidStatement, TEXT(""));
	ScopeStack.Add(Result);
	return Result;
}

int32 FHLSLMaterialTranslator::BeginScope_If(int32 Condition)
{
	const int32 ConditionAsFloat = ForceCast(Condition, MCT_Float1);
	if (ConditionAsFloat == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const int32 Result = AddCodeChunk(MCT_VoidStatement, TEXT("if (%s != 0.0f)"), *GetParameterCode(ConditionAsFloat));
	ScopeStack.Add(Result);

	return Result;
}

int32 FHLSLMaterialTranslator::BeginScope_Else()
{
	const int32 Result = AddCodeChunk(MCT_VoidStatement, TEXT("else"));
	ScopeStack.Add(Result);

	return Result;
}

int32 FHLSLMaterialTranslator::BeginScope_For(const UMaterialExpression* Expression, int32 StartIndex, int32 EndIndex, int32 IndexStep)
{
	const int32 StartIndexAsFloat = ForceCast(StartIndex, MCT_Float1);
	const int32 EndIndexAsFloat = ForceCast(EndIndex, MCT_Float1);
	const int32 IndexStepAsFloat = ForceCast(IndexStep, MCT_Float1);
	if (StartIndexAsFloat == INDEX_NONE || EndIndexAsFloat == INDEX_NONE || IndexStepAsFloat == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const int32 LoopIndex = NumForLoops[ShaderFrequency]++;
	check(!ForLoopMap[ShaderFrequency].Contains(Expression));
	ForLoopMap[ShaderFrequency].Add(Expression, LoopIndex);

	const int32 Result = AddCodeChunk(MCT_VoidStatement, TEXT("for (float ForLoopCounter%d = %s; ForLoopCounter%d < %s; ForLoopCounter%d += %s)"),
		LoopIndex, *GetParameterCode(StartIndexAsFloat),
		LoopIndex, *GetParameterCode(EndIndexAsFloat),
		LoopIndex, *GetParameterCode(IndexStepAsFloat));
	ScopeStack.Add(Result);
	return Result;
}

int32 FHLSLMaterialTranslator::EndScope()
{
	return ScopeStack.Pop(false);
}

int32 FHLSLMaterialTranslator::ForLoopIndex(const UMaterialExpression* Expression)
{
	const int32* Result = ForLoopMap[ShaderFrequency].Find(Expression);
	if (Result)
	{
		return AddInlinedCodeChunk(MCT_Float1, TEXT("ForLoopCounter%d"), *Result);
	}
	return Error(TEXT("Expression is not a for-loop"));
}

int32 FHLSLMaterialTranslator::ReturnMaterialAttributes(int32 MaterialAttributes)
{
	if (MaterialAttributes == INDEX_NONE ||
		GetParameterType(MaterialAttributes) != MCT_MaterialAttributes)
	{
		return Error(TEXT("Expected MaterialAttributes"));
	}

	const uint64 AttributeMask = GetParameterMaterialAttributeMask(MaterialAttributes);
	MaterialAttributesReturned[ShaderFrequency] |= AttributeMask;

	return AddCodeChunk(MCT_VoidStatement, TEXT("return %s;"), *GetParameterCode(MaterialAttributes));
}

int32 FHLSLMaterialTranslator::SetLocal(const FName& LocalName, int32 Value)
{
	const int32 ValueAsFloat = ForceCast(Value, MCT_Float1);
	if (ValueAsFloat == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	FMaterialLocalVariableEntry& Entry = LocalVariables[ShaderFrequency].FindOrAdd(LocalName);
	if (Entry.DeclarationCodeIndex == INDEX_NONE)
	{
		Entry.Name = LocalName.ToString();
		Entry.DeclarationCodeIndex = AddCodeChunk(MCT_VoidStatement, TEXT("float %s = %s;"), *Entry.Name, *GetParameterCode(ValueAsFloat));
		return Entry.DeclarationCodeIndex;
	}
	else
	{
		// ensure the declaration is visible in the current scope
		AddCodeChunkToCurrentScope(Entry.DeclarationCodeIndex);
		return AddCodeChunk(MCT_VoidStatement, TEXT("%s = %s;"), *Entry.Name, *GetParameterCode(ValueAsFloat));
	}
}

int32 FHLSLMaterialTranslator::GetLocal(const FName& LocalName)
{
	const FMaterialLocalVariableEntry* Entry = LocalVariables[ShaderFrequency].Find(LocalName);
	if (!Entry)
	{
		return Errorf(TEXT("Local %s used before being set"), *LocalName.ToString());
	}

	// ensure the declaration is visible in the current scope
	AddCodeChunkToCurrentScope(Entry->DeclarationCodeIndex);
	return AddInlinedCodeChunk(MCT_Float1, TEXT("%s"), *Entry->Name);
}

FStrataOperator& FHLSLMaterialTranslator::StrataCompilationRegisterOperator(int32 OperatorType, FGuid StrataExpressionGuid, UMaterialExpression* Parent, FGuid StrataParentExpressionGuid, bool bUseParameterBlending)
{
	if (OperatorType == STRATA_OPERATOR_BSDF_LEGACY)
	{
		// We register the fact that a legacy material conversion is used and register a simple BSDF
		bStrataUsesConversionFromLegacy = true;
		OperatorType = STRATA_OPERATOR_BSDF;
	}

	static FStrataOperator DefaultOperatorOnError = FStrataOperator();

	if (StrataMaterialExpressionToOperatorIndex.Find(StrataExpressionGuid))
	{
		// It is not possible to register/use a Strata BSDF multiple times with this same exact graph path. (that would break the strata tree code generation)
		Errorf(TEXT("Material %s: It is not possible to uses a Strata BSDF (or any ouput of type StrataData) multiple times within a Strata material topology with the same graph path GUID (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
		return DefaultOperatorOnError;
	}

	const uint32 NewOperatorIndex = StrataMaterialExpressionToOperatorIndex.Num();
	if (NewOperatorIndex >= STRATA_MAX_OPERATOR_COUNT)
	{
		Errorf(TEXT("Material %s have too many Strata Operators (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
		return DefaultOperatorOnError;
	}

	int32* ParentOperatorIndex = StrataMaterialExpressionToOperatorIndex.Find(StrataParentExpressionGuid);
	if (Parent!=nullptr && ParentOperatorIndex == nullptr)
	{
		Errorf(TEXT("Material %s tries to register unknown operator parents (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
		return DefaultOperatorOnError;
	}

	StrataMaterialExpressionToOperatorIndex.Add(StrataExpressionGuid, NewOperatorIndex);

	FStrataOperator& NewOperator = StrataMaterialExpressionRegisteredOperators.AddDefaulted_GetRef();

	NewOperator.OperatorType = OperatorType;
	NewOperator.bNodeRequestParameterBlending = bUseParameterBlending;
	NewOperator.Index = NewOperatorIndex;
	NewOperator.ParentIndex = ParentOperatorIndex != nullptr ? *ParentOperatorIndex : INDEX_NONE;
	NewOperator.LeftIndex   = INDEX_NONE;
	NewOperator.RightIndex   = INDEX_NONE;

	NewOperator.BSDFIndex = INDEX_NONE;	// Allocated later to be albedo account for inline

	NewOperator.MaxDistanceFromLeaves = 0;
	NewOperator.bIsBottom = false;
	NewOperator.bIsTop = false;

	return NewOperator;
}

FStrataOperator& FHLSLMaterialTranslator::StrataCompilationGetOperator(FGuid StrataExpressionGuid)
{
	auto* OperatorIndex = StrataMaterialExpressionToOperatorIndex.Find(StrataExpressionGuid);
	if (!(OperatorIndex && *OperatorIndex >= 0 && *OperatorIndex < STRATA_MAX_OPERATOR_COUNT))
	{
		static FStrataOperator DefaultOperatorOnError = FStrataOperator();
		return DefaultOperatorOnError;
	};
	return StrataMaterialExpressionRegisteredOperators[*OperatorIndex];
}

FStrataOperator* FHLSLMaterialTranslator::StrataCompilationGetOperatorFromIndex(int32 OperatorIndex)
{
	int32 OperatorCount = StrataMaterialExpressionRegisteredOperators.Num();
	if (OperatorIndex < 0 || OperatorIndex >= OperatorCount)
	{
		Errorf(TEXT("SubstrateCompilationGetOperatorFromIndex - OperatorIndex out of range %s (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
		return nullptr;
	};
	return &StrataMaterialExpressionRegisteredOperators[OperatorIndex];
}

void FHLSLMaterialTranslator::StrataEvaluateSharedLocalBases(
	uint8& OutUsedSharedLocalBasesCount,
	uint8& OutRequestedSharedLocalBasesCount,
	FString* OutStrataPixelNormalInitializerValues, 
	FShaderCompilerEnvironment* OutEnvironment) const
{
	/*
	* The final output code/workflow for shared tangent basis should look like
	*
	* #define SHAREDLOCALBASIS_INDEX_0 0		// default, unused
	* #define SHAREDLOCALBASIS_INDEX_1 0		// default, unused
	* #define SHAREDLOCALBASIS_INDEX_2 0
	*
	* FStrataData BSDF0 = GetStrataSlabBSDF(... SHAREDLOCALBASIS_0, NormalCode0 ...)
	* FStrataData BSDF1 = GetStrataSlabBSDF(... SHAREDLOCALBASIS_1, NormalCode1 ...)
	*
	* float3 NormalCode2 = lerp(NormalCode0, NormalCode1, mix)
	* FStrataData BSDF2 = StrataHorizontalMixingParameterBlending(BSDF0, BSDF1, mix, NormalCode2, SHAREDLOCALBASIS_INDEX_2) // will internally create NormalCode2
	*
	* tParameters.SharedLocalBases.Normals[SHAREDLOCALBASIS_INDEX_2] = NormalCode2;
	* #if MATERIAL_TANGENTSPACENORMAL
	* Parameters.SharedLocalBases.Normals[SHAREDLOCALBASIS_INDEX_2] *= Parameters.TwoSidedSign;
	* #endif
	*/

	FStrataRegisteredSharedLocalBasis UsedSharedLocalBasesInfo[STRATA_MAX_SHAREDLOCALBASES_REGISTERS];
	OutUsedSharedLocalBasesCount = 0;
	OutRequestedSharedLocalBasesCount = 0;

	for (int32 OpIt = 0; OpIt < StrataMaterialExpressionRegisteredOperators.Num(); ++OpIt)
	{
		const FStrataOperator& BSDFOperator = StrataMaterialExpressionRegisteredOperators[OpIt];
		if (BSDFOperator.BSDFIndex == INDEX_NONE || BSDFOperator.IsDiscarded())
		{
			continue;	// not a BSDF or if discarded (i.e. not the root of a parameter blending subtree), then there is no local basis to register
		}

		if (BSDFOperator.BSDFRegisteredSharedLocalBasis.NormalCodeChunk == INDEX_NONE && BSDFOperator.BSDFRegisteredSharedLocalBasis.TangentCodeChunk == INDEX_NONE)
		{
			continue;	// We skip null normal on certain BSDF, for instance unlit.
		}
		const FStrataSharedLocalBasesInfo& StrataSharedLocalBasesInfo = StrataCompilationInfoGetMatchingSharedLocalBasisInfo(BSDFOperator.BSDFRegisteredSharedLocalBasis);

		// First, we check that the normal/tangent has not already written out (avoid 2 BSDFs sharing the same normal to note generate the same code twice)
		bool bAlreadyProcessed = false;
		for (uint8 i = 0; i < OutUsedSharedLocalBasesCount; ++i)
		{
			if (UsedSharedLocalBasesInfo[i].NormalCodeChunkHash == StrataSharedLocalBasesInfo.SharedData.NormalCodeChunkHash &&
				(UsedSharedLocalBasesInfo[i].TangentCodeChunkHash == StrataSharedLocalBasesInfo.SharedData.TangentCodeChunkHash || BSDFOperator.BSDFRegisteredSharedLocalBasis.TangentCodeChunk == INDEX_NONE))
			{
				bAlreadyProcessed = true;
				break;
			}
		}
		if (bAlreadyProcessed)
		{
			continue;
		}

		++OutRequestedSharedLocalBasesCount;
		if (OutUsedSharedLocalBasesCount >= STRATA_MAX_SHAREDLOCALBASES_REGISTERS)
		{
			continue;
		}

		const uint8 FinalSharedLocalBasisIndex = OutUsedSharedLocalBasesCount++;
		UsedSharedLocalBasesInfo[FinalSharedLocalBasisIndex] = StrataSharedLocalBasesInfo.SharedData;

		if (OutStrataPixelNormalInitializerValues)
		{
			// Write out normals
			*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\tParameters.SharedLocalBases.Normals[%u] = %s;\n"), FinalSharedLocalBasisIndex, *StrataSharedLocalBasesInfo.NormalCode);
			*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\t#if MATERIAL_TANGENTSPACENORMAL\n"));
			*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\tParameters.SharedLocalBases.Normals[%u] *= Parameters.TwoSidedSign;\n"), FinalSharedLocalBasisIndex);
			*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\t#endif\n"));

			// Write out tangents
			if (StrataSharedLocalBasesInfo.SharedData.TangentCodeChunk != INDEX_NONE)
			{
				*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\tParameters.SharedLocalBases.Tangents[%u] = %s;\n"), FinalSharedLocalBasisIndex, *StrataSharedLocalBasesInfo.TangentCode);
			}
			else
			{
				*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\tParameters.SharedLocalBases.Tangents[%u] = Parameters.TangentToWorld[0];\n"), FinalSharedLocalBasisIndex);
			}
			*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\t#if MATERIAL_TANGENTSPACENORMAL\n"));
			*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\tParameters.SharedLocalBases.Tangents[%u] *= Parameters.TwoSidedSign;\n"), FinalSharedLocalBasisIndex);
			*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\t#endif\n"));
		}
	}
	if (OutStrataPixelNormalInitializerValues)
	{
		*OutStrataPixelNormalInitializerValues += FString::Printf(TEXT("\tParameters.SharedLocalBases.Count = %u;\n"), FinalUsedSharedLocalBasesCount);
	}

	if (OutEnvironment)
	{
		// Now write out all the macros, them mapping from the BSDF to the effective position/index in the shared local basis array they should write to.
		for (TMultiMap<uint64, FStrataSharedLocalBasesInfo>::TConstIterator It(CodeChunkToStrataSharedLocalBasis); It; ++It)
		{
			// The default linear output index will be 0 by default, and different if in fact the shared local basis points to one that is effectively in used in the array of shared local bases.
			uint8 LinearIndex = 0;
			for (uint8 i = 0; i < FinalUsedSharedLocalBasesCount; ++i)
			{
				if (UsedSharedLocalBasesInfo[i].NormalCodeChunkHash == It->Value.SharedData.NormalCodeChunkHash &&
					(UsedSharedLocalBasesInfo[i].TangentCodeChunkHash == It->Value.SharedData.TangentCodeChunkHash || It->Value.SharedData.TangentCodeChunk == INDEX_NONE))
				{
					LinearIndex = i;
					break;
				}
			}

			OutEnvironment->SetDefine(*GetStrataSharedLocalBasisIndexMacro(It->Value.SharedData), LinearIndex);
		}
	}
}

bool FHLSLMaterialTranslator::StrataGenerateDerivedMaterialOperatorData()
{
	if (StrataMaterialExpressionRegisteredOperators.IsEmpty())
	{
		Errorf(TEXT("Could not find any strata operators or BSDFs in Material %s (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
		return false;
	}

	//
	// Evaluate the one and only root node
	//
	int32 RootIndex = INDEX_NONE;
	for (auto& It : StrataMaterialExpressionRegisteredOperators)
	{
		if (It.ParentIndex == INDEX_NONE)
		{
			check(RootIndex == INDEX_NONE);	// There can only be one
			RootIndex = It.Index;
		}
	}
	StrataMaterialRootOperator = &StrataMaterialExpressionRegisteredOperators[RootIndex];
	if (!StrataMaterialRootOperator)
	{
		Errorf(TEXT("Cannot find the root of the Strata Tree for Material %s (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
		return false;
	}

	//
	// Make sure each and every path of the strata tree have valid children and path.
	//
	for (auto& It : StrataMaterialExpressionRegisteredOperators)
	{
		if (It.IsDiscarded())
		{
			continue; // ignore discarded operations in sub tree using parameter blending
		}

		bool bMustHaveLeftChild = false;
		bool bMustHaveRightChild = false;
		switch (It.OperatorType)
		{
		// Operators without any child
		case STRATA_OPERATOR_BSDF:
		break;

		// Operators with two children
		case STRATA_OPERATOR_HORIZONTAL:
		case STRATA_OPERATOR_VERTICAL:
		case STRATA_OPERATOR_ADD:
		{
			bMustHaveLeftChild = true;
			bMustHaveRightChild = true;
		}
		break;

		// Operators with a single child
		case STRATA_OPERATOR_WEIGHT:
		{
			bMustHaveLeftChild = true;
		}
		break;
		}

		if (bMustHaveLeftChild && It.LeftIndex == INDEX_NONE)
		{
			Errorf(TEXT("A Strata Operator %s node is missing its first input from material %s (asset: %s).\r\n"), GetStrataOperatorStr(It.OperatorType), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
			return false;
		}
		if (bMustHaveRightChild && It.RightIndex == INDEX_NONE)
		{
			Errorf(TEXT("A Strata Operator %s node is missing its second input from material %s (asset: %s).\r\n"), GetStrataOperatorStr(It.OperatorType), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
			return false;
		}
	}

	StrataSimplificationStatus.bRunFullSimplification = StrataCompilationConfig.bFullSimplify && !bStrataUsesConversionFromLegacy;

	const uint32 StrataBytePerPixel = Strata::GetBytePerPixel(Platform);
	do 
	{
		// Reset some data for simplifiucation iteration
		StrataMaterialBSDFCount = 0;

		//
		// Parse the tree and mark nodes that are the root of a subtree using parameter blending, while other nodes in that tree are forced to use parameter blending.
		// Allocate BSDFIndex at the same time.
		// Check BSDF that should be unique.
		//
		{
			bool bHasUnlit = false;
			bool bHasVFogCloud = false;
			bool bHasHair = false;
			bool bHasEye = false;
			bool bHasSLW = false;
			bool bHasSlab = false;

			int VOpTopBranchCountTaken = 0;
			int VOpBottomBranchCountTaken = 0;
			bool bStrataUsesVerticalLayering = false;
			bool bOperatorEncountered = false;

			std::function<void(FStrataOperator&, bool)> WalkOperators = [&](FStrataOperator& CurrentOperator, bool bInsideParameterBlendingSubTree) -> void
			{
				const bool bCurrentOpRequestParameterBlending	= CurrentOperator.bNodeRequestParameterBlending || StrataSimplificationStatus.bRunFullSimplification;
				const bool bRootOfParameterBlendingSubTree		= bCurrentOpRequestParameterBlending && !bInsideParameterBlendingSubTree;
				const bool bUseParameterBlending				= bCurrentOpRequestParameterBlending || bInsideParameterBlendingSubTree;

				CurrentOperator.bUseParameterBlending			= bUseParameterBlending;
				CurrentOperator.bRootOfParameterBlendingSubTree	= bRootOfParameterBlendingSubTree;

				switch (CurrentOperator.OperatorType)
				{
				case STRATA_OPERATOR_VERTICAL:
				{
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.LeftIndex], bUseParameterBlending);
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.RightIndex], bUseParameterBlending);
					bOperatorEncountered = true;
					break;
				}
				case STRATA_OPERATOR_HORIZONTAL:
				case STRATA_OPERATOR_ADD:
				{
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.LeftIndex], bUseParameterBlending);
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.RightIndex], bUseParameterBlending);
					bOperatorEncountered = true;
					break;
				}
				case STRATA_OPERATOR_WEIGHT:
				{
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.LeftIndex], bUseParameterBlending);
					bOperatorEncountered = true;
					break;
				}
				case STRATA_OPERATOR_BSDF:
				{
					if (!bInsideParameterBlendingSubTree)
					{
						CurrentOperator.BSDFIndex = StrataMaterialBSDFCount++;
					}

					bHasUnlit |= CurrentOperator.BSDFType == STRATA_BSDF_TYPE_UNLIT;
					bHasVFogCloud |= CurrentOperator.BSDFType == STRATA_BSDF_TYPE_VOLUMETRICFOGCLOUD;
					bHasHair |= CurrentOperator.BSDFType == STRATA_BSDF_TYPE_HAIR;
					bHasEye |= CurrentOperator.BSDFType == STRATA_BSDF_TYPE_EYE;
					bHasSLW |= CurrentOperator.BSDFType == STRATA_BSDF_TYPE_SINGLELAYERWATER;
					bHasSlab |= CurrentOperator.BSDFType == STRATA_BSDF_TYPE_SLAB;
					break;
				}
				}

				// We mark the top of a parameter blending tree as a BSDF now to allocate a slot for it that can then be used next for non-parameter blending operations.
				// Intermediate parameter blending BSDF and operation will be done inline and stored in FStrataData.
				if (bRootOfParameterBlendingSubTree)
				{
					CurrentOperator.OperatorType = STRATA_OPERATOR_BSDF;
					CurrentOperator.BSDFIndex = StrataMaterialBSDFCount++;
					// We do not reset LeftIndex and RightIndex because those are needed to recover local tangent basis information needed with parameter blending.
				}

				// When at least one vertical operator exists that is not parameter blending, we can enabled writing to opaque rough refraction buffer.
				bStrataUsesVerticalLayering = bStrataUsesVerticalLayering || (!CurrentOperator.bUseParameterBlending && CurrentOperator.OperatorType == STRATA_OPERATOR_VERTICAL);
			};

			WalkOperators(*StrataMaterialRootOperator, false);

			const bool bIsOpaqueOrMasked = IsOpaqueOrMaskedBlendMode(*Material);
			bStrataOutputsOpaqueRoughRefractions = bStrataUsesVerticalLayering && bIsOpaqueOrMasked;
			bStrataMaterialIsUnlitNode = bHasUnlit;

			if ((bHasUnlit || bHasVFogCloud || bHasHair || bHasEye || bHasSLW) && StrataMaterialBSDFCount > 1)
			{
				Errorf(TEXT("Unlit, Fog/Cloud, Hair or SingleLayerWater must be used in isolation. See %s (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
				// Even though we could support Unlit with slab.
			}

			if ((bHasUnlit || bHasVFogCloud || bHasHair || bHasEye || bHasSLW) && bOperatorEncountered)
			{
				Errorf(TEXT("Unlit, Fog/Cloud, Hair or SingleLayerWater cannot be used with operators. See %s (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
				// This is because it will results in simpler lighting loops focusin on slab.
			}

			if (StrataMaterialBSDFCount > STRATA_MAX_BSDF_COUNT)
			{
				Errorf(TEXT("Material tries to register more BSDF than can be supproted (%d > %d). See %s (asset: %s).\r\n"), StrataMaterialBSDFCount, STRATA_MAX_BSDF_COUNT, *Material->GetDebugName(), *Material->GetAssetPath().ToString());
			}
		}

		// STRATA_TODO: operation using parameter blending are not actually discarded so they still occupy a spot in the operation array in the compiler and in the shader also.
		// Even thought the compiler will remove unused operators from the shader code, we will still be limited to STRATA_MAX_OPERATOR_COUNT even with parameter blending.
		// This can be fixed by remapping all operation index and make sure this is the index that is always used in the compiler and specified to the shader.

		//
		// Make sure all the types have valid children operator indices
		//
		for (auto& It : StrataMaterialExpressionRegisteredOperators)
		{
			if (It.IsDiscarded())
			{
				continue; // ignore discarded operations in sub tree using parameter blending
			}

			check(It.Index != INDEX_NONE);

			switch (It.OperatorType)
			{
			// Operators without any child
			case STRATA_OPERATOR_BSDF:
			{
				if (!It.bUseParameterBlending)
				{
					// When using parameter blending, we need to keep indices to be able to recover local basis information for normal blending.
					check(It.LeftIndex == INDEX_NONE && It.RightIndex == INDEX_NONE && It.BSDFIndex != INDEX_NONE);
				}
				break;
			}

			// Operators with two children
			case STRATA_OPERATOR_HORIZONTAL:
			case STRATA_OPERATOR_VERTICAL:
			case STRATA_OPERATOR_ADD:
			{
				check(It.RightIndex != INDEX_NONE);
			}
			// Fallthrough

			// Operators with a single child
			case STRATA_OPERATOR_WEIGHT:
			{
				check(It.LeftIndex != INDEX_NONE);
			}
			// Fallthrough
			}
		}

		//
		// Compute the maximum depth from the BSDF node for each operator
		//
		{
			std::function<void(FStrataOperator&)>  WalkOperatorsToRoot = [&](FStrataOperator& CurrentOperator) -> void
			{
				switch (CurrentOperator.OperatorType)
				{
				case STRATA_OPERATOR_WEIGHT:
				{
					CurrentOperator.MaxDistanceFromLeaves = StrataMaterialExpressionRegisteredOperators[CurrentOperator.LeftIndex].MaxDistanceFromLeaves + 1;
					break;
				}
				case STRATA_OPERATOR_VERTICAL:
				case STRATA_OPERATOR_HORIZONTAL:
				case STRATA_OPERATOR_ADD:
				{
					CurrentOperator.MaxDistanceFromLeaves = FMath::Max(
						StrataMaterialExpressionRegisteredOperators[CurrentOperator.LeftIndex].MaxDistanceFromLeaves,
						StrataMaterialExpressionRegisteredOperators[CurrentOperator.RightIndex].MaxDistanceFromLeaves) + 1;
					break;
				}
				case STRATA_OPERATOR_BSDF:
				{
					CurrentOperator.MaxDistanceFromLeaves = 0;
					break;
				}
				}

				if (CurrentOperator.ParentIndex != INDEX_NONE)
				{
					WalkOperatorsToRoot(StrataMaterialExpressionRegisteredOperators[CurrentOperator.ParentIndex]);
				}
			};

			for (auto& It : StrataMaterialExpressionRegisteredOperators)
			{
				if (It.IsDiscarded())
				{
					continue; // ignore discarded operations in sub tree using parameter blending
				}

				if (It.OperatorType == STRATA_OPERATOR_BSDF)
				{
					// Recursively parse all node from BSDF to the root node and update the necessary properties.
					WalkOperatorsToRoot(It);
				}
			}
		}

		//
		// Compute IsTop or IsBottom layer using a depth first tree visit while counting vertical right and left branches taken.
		//
		{
			int VOpTopBranchCountTaken = 0;
			int VOpBottomBranchCountTaken = 0;

			std::function<void(FStrataOperator&)> WalkOperators = [&](FStrataOperator& CurrentOperator) -> void
			{
				switch (CurrentOperator.OperatorType)
				{
				case STRATA_OPERATOR_VERTICAL:
				{
					VOpTopBranchCountTaken++;
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.LeftIndex]);
					VOpTopBranchCountTaken--;
					VOpBottomBranchCountTaken++;
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.RightIndex]);
					VOpBottomBranchCountTaken--;
					break;
				}
				case STRATA_OPERATOR_HORIZONTAL:
				case STRATA_OPERATOR_ADD:
				{
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.LeftIndex]);
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.RightIndex]);
					break;
				}
				case STRATA_OPERATOR_WEIGHT:
				{
					WalkOperators(StrataMaterialExpressionRegisteredOperators[CurrentOperator.LeftIndex]);
					break;
				}
				case STRATA_OPERATOR_BSDF:
				{
					const int32 VopCount = VOpTopBranchCountTaken + VOpBottomBranchCountTaken;
					CurrentOperator.LayerDepth = VOpBottomBranchCountTaken;
					CurrentOperator.bIsTop = VopCount == 0 || VOpTopBranchCountTaken == VopCount;
					CurrentOperator.bIsBottom = VopCount == 0 || VOpBottomBranchCountTaken == VopCount;
					break;
				}
				}
			};

			WalkOperators(*StrataMaterialRootOperator);
		}

		//
		// Compute the size of the material
		//
		{
			// Compute the shared local basis count only
			// But we cannot use StrataEvaluateSharedLocalBases here, becaise the material has not been compiled yet so all the bases would just default to the same.
			// STRATA_TODO: can we do that material generation in two passes? 
			//		1- A first one to evaluate the normal/tangent code
			//		2- Operators are processed and simplification computed based on memory budget
			//		3- Material is finally compiled for with operator updated to fit in memory budget.
			uint8 UsedSharedLocalBasesCount = StrataMaterialBSDFCount;

			const uint32 UintByteSize = sizeof(uint32);
			StrataMaterialRequestedSizeByte = 0;

			// 1. Evaluate simple/single BSDF
			bStrataMaterialIsSimple = StrataMaterialBSDFCount == 1;
			bStrataMaterialIsSingle = StrataMaterialBSDFCount == 1;
			bool bIsFastWaterPath = false;
			bool bCustomEncoding = false;
			for (auto& It : StrataMaterialExpressionRegisteredOperators)
			{
				if (It.IsDiscarded())
				{
					continue; // ignore discarded operations in sub tree using parameter blending
				}

				switch (It.OperatorType)
				{
				case STRATA_OPERATOR_BSDF:
				{
					// From the compiler side, we can only assume the top layer has gray scale luminance weight.
					const bool bMayHaveColoredWeight = !It.bIsTop;

					switch (It.BSDFType)
					{
					case STRATA_BSDF_TYPE_SLAB:
					{
						bStrataMaterialIsSimple = bStrataMaterialIsSimple && !bMayHaveColoredWeight && !It.bBSDFHasAnisotropy && !It.bBSDFHasEdgeColor && !It.bBSDFHasFuzz && !It.bBSDFHasSecondRoughnessOrSimpleClearCoat && !It.bBSDFHasMFPPluggedIn && !It.bBSDFHasSSS;
						bStrataMaterialIsSingle = bStrataMaterialIsSingle && !bMayHaveColoredWeight && !It.bBSDFHasAnisotropy;
						break;
					}
					case STRATA_BSDF_TYPE_HAIR:
					{
						bStrataMaterialIsSimple = false;
						bStrataMaterialIsSingle = false;
						bCustomEncoding			= true;
						break;
					}
					case STRATA_BSDF_TYPE_EYE:
					{
						bStrataMaterialIsSimple = false;
						bStrataMaterialIsSingle = false;
						bCustomEncoding			= true;
						break;
					}
					case STRATA_BSDF_TYPE_SINGLELAYERWATER:
					{
						bStrataMaterialIsSimple = false;
						bStrataMaterialIsSingle = false;
						bIsFastWaterPath		= true;
						break;
					}
					}
					break;
				}
				case STRATA_OPERATOR_WEIGHT:
				{
					// If a BSDF modified by a weight operator, its weight will be < 1.0f, and it won't be a single material anymore
					bStrataMaterialIsSimple = false;
					bStrataMaterialIsSingle = false;
					break;
				}
				}
			}
			bStrataMaterialIsSingle = bStrataMaterialIsSingle && !bStrataMaterialIsSimple;

			// 2. Header

			if (!bStrataMaterialIsSimple && !bStrataMaterialIsSingle && !bCustomEncoding && !bIsFastWaterPath) // header written later, 
			{
				// Packed Header
				StrataMaterialRequestedSizeByte += UintByteSize;

				// Shared local bases between BSDFs
				StrataMaterialRequestedSizeByte += UsedSharedLocalBasesCount * STRATA_PACKED_SHAREDLOCALBASIS_STRIDE_BYTES;
			}
			// Note:
			//  - We do not need to account for the Top Normal texture when evaluating the material byte count for the optimisation algorithm.
			//  - This is because we only need to optimise for the Strata uint matertial buffer.


			// 2. The list of BSDFs
			for (auto& It : StrataMaterialExpressionRegisteredOperators)
			{
				if (It.IsDiscarded())
				{
					continue; // ignore discarded operations in sub tree using parameter blending
				}

				switch (It.OperatorType)
				{
				case STRATA_OPERATOR_BSDF:
				{
					// From the compiler side, we can only assume the top layer has gray scale luminance weight.
					const bool bMayHaveColoredWeight = !It.bIsTop;

					if (bStrataMaterialIsSimple)
					{
						// Header
						StrataMaterialRequestedSizeByte += UintByteSize;
						// Disney material
						StrataMaterialRequestedSizeByte += UintByteSize;
						break; // Stop here
					}
					else if (bStrataMaterialIsSingle)
					{
						// Header
						StrataMaterialRequestedSizeByte += UintByteSize;
					}
					else if (bCustomEncoding)
					{
						// Header
						StrataMaterialRequestedSizeByte += UintByteSize;
					}
					else if (bIsFastWaterPath)
					{
						// Header + Data
						StrataMaterialRequestedSizeByte += UintByteSize;
						// Data
						StrataMaterialRequestedSizeByte += UintByteSize;
						break; // Stop here
					}
					else if (bMayHaveColoredWeight)
					{
						// BSDF state
						StrataMaterialRequestedSizeByte += UintByteSize;
						// Color weight
						StrataMaterialRequestedSizeByte += UintByteSize;
						// Light transmittance weight
						StrataMaterialRequestedSizeByte += UintByteSize;
					}
					else
					{
						// BSDF state with gray scale weight
						StrataMaterialRequestedSizeByte += UintByteSize;
					}

					switch (It.BSDFType)
					{
					case STRATA_BSDF_TYPE_SLAB:
					{
						// Compute values closer to the reality for HasSSS and IsSimpleVolume, now that we know that we know the topology of the material.
						const bool bIsSimpleVolume = !It.bIsBottom && It.bBSDFHasMFPPluggedIn;
						const bool bHasSSS = It.bIsBottom && It.bBSDFHasSSS;

						StrataMaterialRequestedSizeByte += UintByteSize;
						StrataMaterialRequestedSizeByte += UintByteSize;

						if (It.bBSDFHasEdgeColor || It.bBSDFHasSecondRoughnessOrSimpleClearCoat)
						{
							StrataMaterialRequestedSizeByte += UintByteSize;
						}
						if (bHasSSS || bIsSimpleVolume)
						{
							StrataMaterialRequestedSizeByte += UintByteSize;
						}
						if (It.bBSDFHasFuzz)
						{
							StrataMaterialRequestedSizeByte += UintByteSize;
						}
						break;
					}
					case STRATA_BSDF_TYPE_HAIR:
					{
						// Custom encoding
						StrataMaterialRequestedSizeByte += UintByteSize;
						StrataMaterialRequestedSizeByte += UintByteSize;
						break;
					}
					case STRATA_BSDF_TYPE_EYE:
					{
						// Custom encoding
						StrataMaterialRequestedSizeByte += UintByteSize;
						StrataMaterialRequestedSizeByte += UintByteSize;
						break;
					}
					case STRATA_BSDF_TYPE_SINGLELAYERWATER:
					{
						Errorf(TEXT("Substrate error: single layer water should go through the its dedicated fast path in %s (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
						break;
					}
					case STRATA_BSDF_TYPE_UNLIT:
					{
						// Never stored, it goes directly into the scene as emitted luminance.
						break;
					}
					default:
					{
						Errorf(TEXT("Unkownd BSDF type encountered in %s (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
						break;
					}
					}
					break;
				} // case STRATA_OPERATOR_BSDF
				} // switch (It.OperatorType)
			}

			StrataSimplificationStatus.bMaterialFitsInMemoryBudget = StrataMaterialRequestedSizeByte <= StrataBytePerPixel;
			if (!StrataSimplificationStatus.bMaterialFitsInMemoryBudget && StrataSimplificationStatus.bRunFullSimplification)
			{
				// If we have already run the full simplification but the material still does not fit in memory, we must fail the material compilation.
				Errorf(TEXT("Material %s could not be simplified to fit in strata per pixel (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
				return false;
			}
			if (!StrataSimplificationStatus.bRunFullSimplification)
			{
				// Record the original requested byte size before simplification, only for the first pass.
				StrataSimplificationStatus.OriginalRequestedByteSize = StrataMaterialRequestedSizeByte;
			}
			StrataSimplificationStatus.bRunFullSimplification = !StrataSimplificationStatus.bMaterialFitsInMemoryBudget;

			const uint32 RequestedSizeInUint = FMath::DivideAndRoundUp(StrataMaterialRequestedSizeByte, 4u);
			check(RequestedSizeInUint < 256u);

			MaterialCompilationOutput.StrataMaterialType = bStrataMaterialIsSimple ? 0 : bStrataMaterialIsSingle? 1 : 2;
			MaterialCompilationOutput.StrataBSDFCount = StrataMaterialBSDFCount;
			MaterialCompilationOutput.StrataUintPerPixel = uint8(FMath::Clamp(RequestedSizeInUint, 0u, 0xFF));
		}
	}
	while (!StrataSimplificationStatus.bMaterialFitsInMemoryBudget);

	return true; // Success
}

FStrataRegisteredSharedLocalBasis FHLSLMaterialTranslator::StrataCompilationInfoRegisterSharedLocalBasis(int32 NormalCodeChunk)
{
	check(NormalCodeChunk != INDEX_NONE);
	check(NextFreeStrataShaderNormalIndex < 255);	// Out of shared local basis slots

	FStrataRegisteredSharedLocalBasis StrataRegisteredSharedLocalBasis;
	StrataRegisteredSharedLocalBasis.NormalCodeChunk		= NormalCodeChunk;
	StrataRegisteredSharedLocalBasis.NormalCodeChunkHash	= GetParameterHash(NormalCodeChunk);
	StrataRegisteredSharedLocalBasis.TangentCodeChunk		= INDEX_NONE;
	StrataRegisteredSharedLocalBasis.TangentCodeChunkHash	= GetParameterHash(INDEX_NONE);
	StrataRegisteredSharedLocalBasis.GraphSharedLocalBasisIndex = NextFreeStrataShaderNormalIndex++;

	// Find any basis which match the Normal code chunk
	// A normal can be duplicated when it is paired with different tangent, so find the first one which matches
	TArray<FStrataSharedLocalBasesInfo*> NormalInfos;
	CodeChunkToStrataSharedLocalBasis.MultiFindPointer(StrataRegisteredSharedLocalBasis.NormalCodeChunkHash, NormalInfos);
	if (NormalInfos.Num() == 0)
	{
		CodeChunkToStrataSharedLocalBasis.Add(StrataRegisteredSharedLocalBasis.NormalCodeChunkHash, { StrataRegisteredSharedLocalBasis, *GetParameterCode(StrataRegisteredSharedLocalBasis.NormalCodeChunk), FString() });
		return StrataRegisteredSharedLocalBasis;
	}
	// Return the first existing code chunk which match the normal chunk code
	return NormalInfos[0]->SharedData;
}

FStrataRegisteredSharedLocalBasis FHLSLMaterialTranslator::StrataCompilationInfoRegisterSharedLocalBasis(int32 NormalCodeChunk, int32 TangentCodeChunk)
{
	check(NormalCodeChunk != INDEX_NONE);
	check(NextFreeStrataShaderNormalIndex < 255);	// Out of shared local basis slots
	

	FStrataRegisteredSharedLocalBasis StrataRegisteredSharedLocalBasis;
	StrataRegisteredSharedLocalBasis.NormalCodeChunk		= NormalCodeChunk;
	StrataRegisteredSharedLocalBasis.NormalCodeChunkHash	= GetParameterHash(NormalCodeChunk);
	StrataRegisteredSharedLocalBasis.TangentCodeChunk		= TangentCodeChunk;
	StrataRegisteredSharedLocalBasis.TangentCodeChunkHash	= GetParameterHash(TangentCodeChunk);
	StrataRegisteredSharedLocalBasis.GraphSharedLocalBasisIndex = NextFreeStrataShaderNormalIndex++;

	// Find a basis which matches both the Normal & the Tangent code chunks
	TArray<FStrataSharedLocalBasesInfo*> NormalInfos;
	CodeChunkToStrataSharedLocalBasis.MultiFindPointer(StrataRegisteredSharedLocalBasis.NormalCodeChunkHash, NormalInfos);
	for (FStrataSharedLocalBasesInfo* NormalInfo : NormalInfos)
	{
		// * Either we find a perfect match (normal & tangent matches)
		// * Or we find a normal which doesn't have a tangent associated with, and we set the tangent for code
		if (StrataRegisteredSharedLocalBasis.TangentCodeChunkHash == NormalInfo->SharedData.TangentCodeChunk)
		{
			return NormalInfo->SharedData;
			break;
		}
		else if (NormalInfo->SharedData.TangentCodeChunk == INDEX_NONE)
		{
			NormalInfo->SharedData.TangentCodeChunk		= TangentCodeChunk;
			NormalInfo->SharedData.TangentCodeChunkHash = StrataRegisteredSharedLocalBasis.TangentCodeChunkHash;
			NormalInfo->TangentCode				= *GetParameterCode(TangentCodeChunk);
			return NormalInfo->SharedData;
			break;
		}
	}

	// Allocate a new slot for a new shared local basis
	CodeChunkToStrataSharedLocalBasis.Add(StrataRegisteredSharedLocalBasis.NormalCodeChunkHash, { StrataRegisteredSharedLocalBasis, *GetParameterCode(StrataRegisteredSharedLocalBasis.NormalCodeChunk), *GetParameterCode(StrataRegisteredSharedLocalBasis.TangentCodeChunk) });
	return StrataRegisteredSharedLocalBasis;
}

FHLSLMaterialTranslator::FStrataSharedLocalBasesInfo FHLSLMaterialTranslator::StrataCompilationInfoGetMatchingSharedLocalBasisInfo(const FStrataRegisteredSharedLocalBasis& SearchedSharedLocalBasis) const
{
	check(NextFreeStrataShaderNormalIndex < 255);	// Out of shared local basis slots

	// Find a basis which matches both the Normal & the Tangent code chunks
	TArray<const FStrataSharedLocalBasesInfo*> NormalInfos;
	CodeChunkToStrataSharedLocalBasis.MultiFindPointer(SearchedSharedLocalBasis.NormalCodeChunkHash, NormalInfos);

	// We first try to find a perfect match for normal and tangent from all the registered element.
	for (const FStrataSharedLocalBasesInfo* NormalInfo : NormalInfos)
	{
		if (SearchedSharedLocalBasis.TangentCodeChunk == INDEX_NONE ||											// We selected the first available normal if there is no tangent specified on the material.
			SearchedSharedLocalBasis.TangentCodeChunkHash == NormalInfo->SharedData.TangentCodeChunkHash)		// Otherwise we select the normal+tangent that exactly matches the request.
		{
			return *NormalInfo;
			break;
		}
	}

	check(0);	// When the compiler is querying, this is to get a result to generate code from a fully processed graph. No result means a bug happened during graph processing.
	return FStrataSharedLocalBasesInfo();
}

int32 FHLSLMaterialTranslator::StrataAddParameterBlendingBSDFCoverageToNormalMixCodeChunk(int32 ACodeChunk, int32 BCodeChunk)
{
	if (ACodeChunk == INDEX_NONE || BCodeChunk == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	return AddCodeChunk(MCT_Float, TEXT("AddParameterBlendingBSDFCoverageToNormalMix(%s, %s)"), *GetParameterCode(ACodeChunk), *GetParameterCode(BCodeChunk));
}

int32 FHLSLMaterialTranslator::StrataVerticalLayeringParameterBlendingBSDFCoverageToNormalMixCodeChunk(int32 TopCodeChunk)
{
	if (TopCodeChunk == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	return AddCodeChunk(MCT_Float, TEXT("VerticalLayeringParameterBlendingBSDFCoverageToNormalMix(%s)"), *GetParameterCode(TopCodeChunk));
}

int32 FHLSLMaterialTranslator::StrataHorizontalMixingParameterBlendingBSDFCoverageToNormalMixCodeChunk(int32 BackgroundCodeChunk, int32 ForegroundCodeChunk, int32 HorizontalMixCodeChunk)
{
	if (BackgroundCodeChunk == INDEX_NONE || ForegroundCodeChunk == INDEX_NONE || HorizontalMixCodeChunk == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	return AddCodeChunk(MCT_Float, TEXT("HorizontalMixingParameterBlendingBSDFCoverageToNormalMix(%s, %s, %s)"), *GetParameterCode(BackgroundCodeChunk), *GetParameterCode(ForegroundCodeChunk), *GetParameterCode(HorizontalMixCodeChunk));
}

int32 FHLSLMaterialTranslator::StrataCreateAndRegisterNullMaterial()
{
	int32 OutputCodeChunk = AddInlinedCodeChunk(MCT_Strata, TEXT("GetInitialisedStrataData()"));
	return OutputCodeChunk;
}

FString FHLSLMaterialTranslator::StrataGetCastParameterCode(int32 Index, EMaterialValueType DestType)
{
	int32 CastParameter = ForceCast(Index, DestType);
	return GetParameterCode(CastParameter);
}

int32 FHLSLMaterialTranslator::StrataSlabBSDF(
	int32 DiffuseAlbedo, int32 F0, int32 F90,
	int32 Roughness, int32 Anisotropy,
	int32 SSSProfileId, int32 SSSMFP, int32 SSSMFPScale, int32 SSSPhaseAniso, int32 UseSSSDiffusion,
	int32 EmissiveColor,
	int32 SecondRoughness, int32 SecondRoughnessWeight, int32 SecondRoughnessAsSimpleClearCoat,
	int32 FuzzAmount, int32 FuzzColor, int32 FuzzRoughness,
	int32 Thickness, 
	int32 Normal, int32 Tangent, const FString& SharedLocalBasisIndexMacro,
	FStrataOperator* PromoteToOperator)
{
	const FString NormalCode = GetParameterCode(Normal);
	const FString TangentCode = Tangent != INDEX_NONE ? *GetParameterCode(Tangent) : TEXT("NONE");
	const FString ThicknessCode = GetParameterCode(Thickness);
	const bool bIsThinSurface = Material->IsThinSurface();

	if (PromoteToOperator)
	{
		if (PromoteToOperator->Index == INDEX_NONE || PromoteToOperator->BSDFIndex == INDEX_NONE)
		{
			Errorf(TEXT("Invalid StrataSlabBSDF operator and BSDF indices during promotion in Material %s (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
			return INDEX_NONE;
		}
		return AddCodeChunk(
			MCT_Strata, TEXT("PromoteParameterBlendedBSDFToOperator(GetStrataSlabBSDF(Parameters.StrataPixelFootprint, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, Parameters.SharedLocalBases.Types) /* Normal = %s ; Tangent = %s ; Thickness = %s */, Parameters.StrataTree, %u, %u, %u, %u)"),
			*StrataGetCastParameterCode(DiffuseAlbedo,			MCT_Float3),
			*StrataGetCastParameterCode(F0,						MCT_Float3),
			*StrataGetCastParameterCode(F90,					MCT_Float3),
			*StrataGetCastParameterCode(Roughness,				MCT_Float),
			*StrataGetCastParameterCode(Anisotropy,				MCT_Float),
			*StrataGetCastParameterCode(SSSProfileId,			MCT_Float),
			*StrataGetCastParameterCode(SSSMFP,					MCT_Float3),
			*StrataGetCastParameterCode(SSSMFPScale,			MCT_Float),
			*StrataGetCastParameterCode(SSSPhaseAniso,			MCT_Float),
			*StrataGetCastParameterCode(UseSSSDiffusion,		MCT_Float),
			*StrataGetCastParameterCode(EmissiveColor,			MCT_Float3),
			*StrataGetCastParameterCode(SecondRoughness,		MCT_Float),
			*StrataGetCastParameterCode(SecondRoughnessWeight,	MCT_Float),
			*StrataGetCastParameterCode(SecondRoughnessAsSimpleClearCoat, MCT_Float),
			*StrataGetCastParameterCode(FuzzAmount,				MCT_Float),
			*StrataGetCastParameterCode(FuzzColor,				MCT_Float3),
			*StrataGetCastParameterCode(FuzzRoughness,			MCT_Float1),
			*StrataGetCastParameterCode(Thickness,				MCT_Float),
			bIsThinSurface ? TEXT("true") : TEXT("false"),
			*SharedLocalBasisIndexMacro,
			*NormalCode,
			*TangentCode,
			*ThicknessCode,
			PromoteToOperator->Index,
			PromoteToOperator->BSDFIndex,
			PromoteToOperator->LayerDepth,
			PromoteToOperator->bIsBottom ? 1 : 0
		);
	}
	
	return AddCodeChunk(
		MCT_Strata, TEXT("GetStrataSlabBSDF(Parameters.StrataPixelFootprint, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, Parameters.SharedLocalBases.Types) /* Normal = %s ; Tangent = %s ; Thickness = %s */"),
		*StrataGetCastParameterCode(DiffuseAlbedo,			MCT_Float3),
		*StrataGetCastParameterCode(F0,						MCT_Float3),
		*StrataGetCastParameterCode(F90,					MCT_Float3),
		*StrataGetCastParameterCode(Roughness,				MCT_Float),
		*StrataGetCastParameterCode(Anisotropy,				MCT_Float),
		*StrataGetCastParameterCode(SSSProfileId,			MCT_Float),
		*StrataGetCastParameterCode(SSSMFP,					MCT_Float3),
		*StrataGetCastParameterCode(SSSMFPScale,			MCT_Float),
		*StrataGetCastParameterCode(SSSPhaseAniso,			MCT_Float),
		*StrataGetCastParameterCode(UseSSSDiffusion,		MCT_Float),
		*StrataGetCastParameterCode(EmissiveColor,			MCT_Float3),
		*StrataGetCastParameterCode(SecondRoughness,		MCT_Float),
		*StrataGetCastParameterCode(SecondRoughnessWeight,	MCT_Float),
		*StrataGetCastParameterCode(SecondRoughnessAsSimpleClearCoat, MCT_Float),
		*StrataGetCastParameterCode(FuzzAmount,				MCT_Float),
		*StrataGetCastParameterCode(FuzzColor,				MCT_Float3),
		*StrataGetCastParameterCode(FuzzRoughness,			MCT_Float1),
		*StrataGetCastParameterCode(Thickness,				MCT_Float),
		bIsThinSurface ? TEXT("true") : TEXT("false"),
		*SharedLocalBasisIndexMacro,
		*NormalCode,
		*TangentCode,
		*ThicknessCode
	);
}

int32 FHLSLMaterialTranslator::StrataConversionFromLegacy(
	bool bHasDynamicShadingModels,
	int32 BaseColor, int32 Specular, int32 Metallic,
	int32 Roughness, int32 Anisotropy,
	int32 SubSurfaceColor, int32 SubSurfaceProfileId,
	int32 ClearCoat, int32 ClearCoatRoughness,
	int32 EmissiveColor,
	int32 Opacity,
	int32 TransmittanceColor,
	int32 WaterScatteringCoefficients, int32 WaterAbsorptionCoefficients, int32 WaterPhaseG, int32 ColorScaleBehindWater,
	int32 ShadingModel,
	int32 Normal, int32 Tangent, const FString& SharedLocalBasisIndexMacro,
	int32 ClearCoat_Normal, int32 ClearCoat_Tangent, const FString& ClearCoat_SharedLocalBasisIndexMacro,
	int32 CustomTangent_Tangent,
	FStrataOperator* PromoteToOperator)
{
	const FString NormalCode = GetParameterCode(Normal);
	const FString TangentCode = Tangent != INDEX_NONE ? *GetParameterCode(Tangent) : TEXT("NONE");
	const int32 RawTangent = Tangent != INDEX_NONE ? Tangent : Normal;

	const FString ClearCoat_NormalCode = GetParameterCode(ClearCoat_Normal);
	const FString ClearCoat_TangentCode = Tangent != INDEX_NONE ? *GetParameterCode(ClearCoat_Tangent) : TEXT("NONE");

	if (PromoteToOperator)
	{
		if (PromoteToOperator->Index == INDEX_NONE || PromoteToOperator->BSDFIndex == INDEX_NONE)
		{
			Errorf(TEXT("Invalid StrataSlabBSDF operator and BSDF indices during promotion in Material %s (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
			return INDEX_NONE;
		}

		return AddCodeChunk(
			MCT_Strata, TEXT("PromoteParameterBlendedBSDFToOperator(StrataConvertLegacyMaterial%s(Parameters.StrataPixelFootprint, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, Parameters.SharedLocalBases.Types, Parameters.StrataTree) /* Normal = %s ; Tangent = %s ; ClearCoat_Normal = %s ; ClearCoat_Tangent = %s */, Parameters.StrataTree, %u, %u, %u, %u)"),
			bHasDynamicShadingModels ? TEXT("Dynamic") : TEXT("Static"),
			*StrataGetCastParameterCode(BaseColor,						MCT_Float3),
			*StrataGetCastParameterCode(Specular,						MCT_Float),
			*StrataGetCastParameterCode(Metallic,						MCT_Float),
			*StrataGetCastParameterCode(Roughness,						MCT_Float),
			*StrataGetCastParameterCode(Anisotropy,						MCT_Float),
			*StrataGetCastParameterCode(SubSurfaceColor,				MCT_Float3),
			*StrataGetCastParameterCode(SubSurfaceProfileId,			MCT_Float),
			*StrataGetCastParameterCode(ClearCoat,						MCT_Float),
			*StrataGetCastParameterCode(ClearCoatRoughness,				MCT_Float),
			*StrataGetCastParameterCode(EmissiveColor,					MCT_Float3),
			*StrataGetCastParameterCode(Opacity,						MCT_Float),
			*StrataGetCastParameterCode(TransmittanceColor,				MCT_Float3),
			*StrataGetCastParameterCode(WaterScatteringCoefficients,	MCT_Float3),
			*StrataGetCastParameterCode(WaterAbsorptionCoefficients,	MCT_Float3),
			*StrataGetCastParameterCode(WaterPhaseG,					MCT_Float),
			*StrataGetCastParameterCode(ColorScaleBehindWater,			MCT_Float3),
			*GetParameterCode(ShadingModel),
			// Raw access to Normal/Tangent/ClearCoatNormal/CustomTangent for conversion purpose
			*StrataGetCastParameterCode(Normal,							MCT_Float3),
			*StrataGetCastParameterCode(RawTangent,						MCT_Float3),
			*StrataGetCastParameterCode(ClearCoat_Normal,				MCT_Float3),
			*StrataGetCastParameterCode(CustomTangent_Tangent,			MCT_Float3),
			*SharedLocalBasisIndexMacro,
			*ClearCoat_SharedLocalBasisIndexMacro,
			// Regular normal basis
			*NormalCode,
			*TangentCode,
			// Clear coat bottom layer normal basis
			*ClearCoat_NormalCode,
			*ClearCoat_TangentCode,
			PromoteToOperator->Index,
			PromoteToOperator->BSDFIndex,
			PromoteToOperator->LayerDepth,
			PromoteToOperator->bIsBottom ? 1 : 0
		);
	}

	return AddCodeChunk(
		MCT_Strata, TEXT("StrataConvertLegacyMaterial%s(Parameters.StrataPixelFootprint, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s, Parameters.SharedLocalBases.Types, Parameters.StrataTree) /* Normal = %s ; Tangent = %s ; ClearCoat_Normal = %s ; ClearCoat_Tangent = %s */"),
		bHasDynamicShadingModels ? TEXT("Dynamic") : TEXT("Static"),
		*StrataGetCastParameterCode(BaseColor,						MCT_Float3),
		*StrataGetCastParameterCode(Specular,						MCT_Float),
		*StrataGetCastParameterCode(Metallic,						MCT_Float),
		*StrataGetCastParameterCode(Roughness,						MCT_Float),
		*StrataGetCastParameterCode(Anisotropy,						MCT_Float),
		*StrataGetCastParameterCode(SubSurfaceColor,				MCT_Float3),
		*StrataGetCastParameterCode(SubSurfaceProfileId,			MCT_Float),
		*StrataGetCastParameterCode(ClearCoat,						MCT_Float),
		*StrataGetCastParameterCode(ClearCoatRoughness,				MCT_Float),
		*StrataGetCastParameterCode(EmissiveColor,					MCT_Float3),
		*StrataGetCastParameterCode(Opacity,						MCT_Float),
		*StrataGetCastParameterCode(TransmittanceColor,				MCT_Float3),
		*StrataGetCastParameterCode(WaterScatteringCoefficients,	MCT_Float3),
		*StrataGetCastParameterCode(WaterAbsorptionCoefficients,	MCT_Float3),
		*StrataGetCastParameterCode(WaterPhaseG,					MCT_Float),
		*StrataGetCastParameterCode(ColorScaleBehindWater,			MCT_Float3),
		*GetParameterCode(ShadingModel),
		// Raw access to Normal/Tangent/ClearCoatNormal/CustomTangent for conversion purpose
		*StrataGetCastParameterCode(Normal,							MCT_Float3),
		*StrataGetCastParameterCode(RawTangent,						MCT_Float3),
		*StrataGetCastParameterCode(ClearCoat_Normal,				MCT_Float3),
		*StrataGetCastParameterCode(CustomTangent_Tangent,			MCT_Float3),
		*SharedLocalBasisIndexMacro,
		*ClearCoat_SharedLocalBasisIndexMacro,
		// Regular normal basis
		*NormalCode,
		*TangentCode,
		// Clear coat bottom layer normal basis
		*ClearCoat_NormalCode,
		*ClearCoat_TangentCode
	);
}

int32 FHLSLMaterialTranslator::StrataVolumetricFogCloudBSDF(int32 Albedo, int32 Extinction, int32 EmissiveColor, int32 AmbientOcclusion)
{
	return AddCodeChunk(
		MCT_Strata, TEXT("GetStrataVolumeFogCloudBSDF(%s, %s, %s, %s)"),
		*StrataGetCastParameterCode(Albedo,				MCT_Float3),
		*StrataGetCastParameterCode(Extinction,			MCT_Float3),
		*StrataGetCastParameterCode(EmissiveColor,		MCT_Float3),
		*StrataGetCastParameterCode(AmbientOcclusion,	MCT_Float)
	);
}

int32 FHLSLMaterialTranslator::StrataUnlitBSDF(int32 EmissiveColor, int32 TransmittanceColor)
{
	return AddCodeChunk(
		MCT_Strata, TEXT("GetStrataUnlitBSDF(%s, %s)"),
		*StrataGetCastParameterCode(EmissiveColor,		MCT_Float3),
		*StrataGetCastParameterCode(TransmittanceColor,	MCT_Float3)
	);
}

int32 FHLSLMaterialTranslator::StrataHairBSDF(int32 BaseColor, int32 Scatter, int32 Specular, int32 Roughness, int32 Backlit, int32 EmissiveColor, int32 Tangent, const FString& SharedLocalBasisIndexMacro, FStrataOperator* PromoteToOperator)
{
	return AddCodeChunk(
		MCT_Strata, TEXT("PromoteParameterBlendedBSDFToOperator(GetStrataHairBSDF(%s, %s, %s, %s, %s, %s, %s), Parameters.StrataTree, %u, %u, %u, %u) /* Tangent:%s */"),
		*StrataGetCastParameterCode(BaseColor,			MCT_Float3),
		*StrataGetCastParameterCode(Scatter,			MCT_Float),
		*StrataGetCastParameterCode(Specular,			MCT_Float),
		*StrataGetCastParameterCode(Roughness,			MCT_Float),
		*StrataGetCastParameterCode(Backlit,			MCT_Float),
		*StrataGetCastParameterCode(EmissiveColor,		MCT_Float3),
		*SharedLocalBasisIndexMacro,
		PromoteToOperator->Index,
		PromoteToOperator->BSDFIndex,
		PromoteToOperator->LayerDepth,
		PromoteToOperator->bIsBottom ? 1 : 0,
		*GetParameterCode(Tangent)
	);
}

int32 FHLSLMaterialTranslator::StrataEyeBSDF(int32 DiffuseAlbedo, int32 Roughness, int32 IrisMask, int32 IrisDistance, int32 IrisNormal, int32 IrisPlaneNormal, int32 SSSProfileId, int32 EmissiveColor, int32 CorneaNormal, const FString& SharedLocalBasisIndexMacro, FStrataOperator* PromoteToOperator)
{
	return AddCodeChunk(
		MCT_Strata, TEXT("PromoteParameterBlendedBSDFToOperator(GetStrataEyeBSDF(%s, %s, %s, %s, %s, %s, %s, %s, %s), Parameters.StrataTree, %u, %u, %u, %u) /* Cornea:%s Iris:%s */"),
		*StrataGetCastParameterCode(DiffuseAlbedo, MCT_Float3),
		*StrataGetCastParameterCode(Roughness, MCT_Float),
		*StrataGetCastParameterCode(IrisMask, MCT_Float),
		*StrataGetCastParameterCode(IrisDistance, MCT_Float),
		*StrataGetCastParameterCode(IrisNormal, MCT_Float3),
		*StrataGetCastParameterCode(IrisPlaneNormal, MCT_Float3),
		*StrataGetCastParameterCode(SSSProfileId, MCT_Float),
		*StrataGetCastParameterCode(EmissiveColor, MCT_Float3),
		*SharedLocalBasisIndexMacro,
		PromoteToOperator->Index,
		PromoteToOperator->BSDFIndex,
		PromoteToOperator->LayerDepth,
		PromoteToOperator->bIsBottom ? 1 : 0,
		*GetParameterCode(CorneaNormal),
		*GetParameterCode(IrisNormal)
	);
}

int32 FHLSLMaterialTranslator::StrataSingleLayerWaterBSDF(
	int32 BaseColor, int32 Metallic, int32 Specular, int32 Roughness, int32 EmissiveColor, int32 TopMaterialOpacity,
	int32 WaterAlbedo, int32 WaterExtinction, int32 WaterPhaseG, int32 ColorScaleBehindWater, int32 Normal, const FString& SharedLocalBasisIndexMacro,
	FStrataOperator* PromoteToOperator)
{
	return AddCodeChunk(
		MCT_Strata, TEXT("PromoteParameterBlendedBSDFToOperator(GetStrataSingleLayerWaterBSDF(%s, %s, %s, %s, %s, %s, %s, %s, %s, %s, %s), Parameters.StrataTree, %u, %u, %u, %u) /* Normal:%s */"),
		*StrataGetCastParameterCode(BaseColor,				MCT_Float3),
		*StrataGetCastParameterCode(Metallic,				MCT_Float),
		*StrataGetCastParameterCode(Specular,				MCT_Float),
		*StrataGetCastParameterCode(Roughness,				MCT_Float),
		*StrataGetCastParameterCode(EmissiveColor,			MCT_Float3),
		*StrataGetCastParameterCode(TopMaterialOpacity,		MCT_Float),
		*StrataGetCastParameterCode(WaterAlbedo,			MCT_Float3),
		*StrataGetCastParameterCode(WaterExtinction,		MCT_Float3),
		*StrataGetCastParameterCode(WaterPhaseG,			MCT_Float),
		*StrataGetCastParameterCode(ColorScaleBehindWater,	MCT_Float3),
		*SharedLocalBasisIndexMacro,
		PromoteToOperator->Index,
		PromoteToOperator->BSDFIndex,
		PromoteToOperator->LayerDepth,
		PromoteToOperator->bIsBottom ? 1 : 0,
		*GetParameterCode(Normal)
	);
}

int32 FHLSLMaterialTranslator::StrataHorizontalMixing(int32 Background, int32 Foreground, int32 Mix, int OperatorIndex, uint32 MaxDistanceFromLeaves)
{
	if (Foreground == INDEX_NONE || Background == INDEX_NONE || Mix == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	return AddCodeChunk(
		MCT_Strata, TEXT("StrataHorizontalMixing(%s, %s, %s, Parameters.StrataTree, %u, %u)"),
		*GetParameterCode(Background),
		*GetParameterCode(Foreground),
		*GetParameterCode(Mix),
		OperatorIndex,
		MaxDistanceFromLeaves
	);
}

int32 FHLSLMaterialTranslator::StrataHorizontalMixingParameterBlending(
	int32 Background, int32 Foreground, int32 HorizontalMixCodeChunk, int32 NormalMixCodeChunk, const FString& SharedLocalBasisIndexMacro, FStrataOperator* PromoteToOperator)
{
	if (Foreground == INDEX_NONE || Background == INDEX_NONE || HorizontalMixCodeChunk == INDEX_NONE || NormalMixCodeChunk == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (PromoteToOperator)
	{
		check(PromoteToOperator->Index != INDEX_NONE);
		check(PromoteToOperator->BSDFIndex != INDEX_NONE);
		return AddCodeChunk(
			MCT_Strata, TEXT("PromoteParameterBlendedBSDFToOperator(StrataHorizontalMixingParameterBlending(%s, %s, %s, %s, %s), Parameters.StrataTree, %u, %u, %u, %u)"),
			*GetParameterCode(Background),
			*GetParameterCode(Foreground),
			*GetParameterCode(HorizontalMixCodeChunk),
			*GetParameterCode(NormalMixCodeChunk),
			*SharedLocalBasisIndexMacro,
			PromoteToOperator->Index,
			PromoteToOperator->BSDFIndex,
			PromoteToOperator->LayerDepth,
			PromoteToOperator->bIsBottom ? 1 : 0
		);
	}

	return AddCodeChunk(
		MCT_Strata, TEXT("StrataHorizontalMixingParameterBlending(%s, %s, %s, %s, %s)"),
		*GetParameterCode(Background),
		*GetParameterCode(Foreground),
		*GetParameterCode(HorizontalMixCodeChunk),
		*GetParameterCode(NormalMixCodeChunk),
		*SharedLocalBasisIndexMacro
	);
}

int32 FHLSLMaterialTranslator::StrataVerticalLayering(int32 Top, int32 Base, int32 Thickness, int OperatorIndex, uint32 MaxDistanceFromLeaves)
{
	if (Top == INDEX_NONE || Base == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	const FString ThicknessCode = Thickness != INDEX_NONE ? GetParameterCode(Thickness) : TEXT("NONE");;
	return AddCodeChunk(
		MCT_Strata, TEXT("StrataVerticalLayering(%s, %s, Parameters.StrataTree, %u, %u) /* Thickness = %s */"),
		*GetParameterCode(Top),
		*GetParameterCode(Base),
		OperatorIndex,
		MaxDistanceFromLeaves,
		*ThicknessCode
	);
}

int32 FHLSLMaterialTranslator::StrataVerticalLayeringParameterBlending(int32 Top, int32 Base, int32 Thickness, const FString& SharedLocalBasisIndexMacro, int32 TopBSDFNormalCodeChunk, FStrataOperator* PromoteToOperator)
{
	if (Top == INDEX_NONE || Base == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const FString ThicknessCode = Thickness != INDEX_NONE ? GetParameterCode(Thickness) : TEXT("NONE");

	if (PromoteToOperator)
	{
		check(PromoteToOperator->Index != INDEX_NONE);
		check(PromoteToOperator->BSDFIndex != INDEX_NONE);
		return AddCodeChunk(
			MCT_Strata, TEXT("PromoteParameterBlendedBSDFToOperator(StrataVerticalLayeringParameterBlending(%s, %s, %s, dot(%s, %s)), Parameters.StrataTree, %u, %u, %u, %u) /* Thickness = %s */"),
			*GetParameterCode(Top),
			*GetParameterCode(Base),
			*SharedLocalBasisIndexMacro,
			*GetParameterCode(TopBSDFNormalCodeChunk),
			*GetParameterCode(CameraVector()),
			PromoteToOperator->Index,
			PromoteToOperator->BSDFIndex,
			PromoteToOperator->LayerDepth,
			PromoteToOperator->bIsBottom ? 1 : 0,
			*ThicknessCode
		);
	}

	return AddCodeChunk(
		MCT_Strata, TEXT("StrataVerticalLayeringParameterBlending(%s, %s, %s, dot(%s, %s)) /* Thickness = %s */"),
		*GetParameterCode(Top),
		*GetParameterCode(Base),
		*SharedLocalBasisIndexMacro,
		*GetParameterCode(TopBSDFNormalCodeChunk),
		*GetParameterCode(CameraVector()),
		*ThicknessCode
	);
}

int32 FHLSLMaterialTranslator::StrataAdd(int32 A, int32 B, int OperatorIndex, uint32 MaxDistanceFromLeaves)
{
	if (A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	return AddCodeChunk(
		MCT_Strata, TEXT("StrataAdd(%s, %s, Parameters.StrataTree, %u, %u)"),
		*GetParameterCode(A),
		*GetParameterCode(B),
		OperatorIndex,
		MaxDistanceFromLeaves
	);
}

int32 FHLSLMaterialTranslator::StrataAddParameterBlending(int32 A, int32 B, int32 AMixWeight, const FString& SharedLocalBasisIndexMacro, FStrataOperator* PromoteToOperator)
{
	if (A == INDEX_NONE || B == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (PromoteToOperator)
	{
		check(PromoteToOperator->Index != INDEX_NONE);
		check(PromoteToOperator->BSDFIndex != INDEX_NONE);
		return AddCodeChunk(
			MCT_Strata, TEXT("PromoteParameterBlendedBSDFToOperator(StrataAddParameterBlending(%s, %s, %s, %s), Parameters.StrataTree, %u, %u, %u, %u)"),
			*GetParameterCode(A),
			*GetParameterCode(B),
			*GetParameterCode(AMixWeight),
			*SharedLocalBasisIndexMacro,
			PromoteToOperator->Index,
			PromoteToOperator->BSDFIndex,
			PromoteToOperator->LayerDepth,
			PromoteToOperator->bIsBottom ? 1 : 0
		);
	}

	return AddCodeChunk(
		MCT_Strata, TEXT("StrataAddParameterBlending(%s, %s, %s, %s)"),
		*GetParameterCode(A),
		*GetParameterCode(B),
		*GetParameterCode(AMixWeight),
		*SharedLocalBasisIndexMacro
	);
}

int32 FHLSLMaterialTranslator::StrataWeight(int32 A, int32 Weight, int OperatorIndex, uint32 MaxDistanceFromLeaves)
{
	if (A == INDEX_NONE || Weight == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	return AddCodeChunk(
		MCT_Strata, TEXT("StrataWeight(%s, %s, Parameters.StrataTree, %u, %u)"),
		*GetParameterCode(A),
		*GetParameterCode(Weight),
		OperatorIndex,
		MaxDistanceFromLeaves
	);
}

int32 FHLSLMaterialTranslator::StrataWeightParameterBlending(int32 A, int32 Weight, FStrataOperator* PromoteToOperator)
{
	if (A == INDEX_NONE || Weight == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	if (PromoteToOperator)
	{
		check(PromoteToOperator->Index != INDEX_NONE);
		check(PromoteToOperator->BSDFIndex != INDEX_NONE);
		return AddCodeChunk(
			MCT_Strata, TEXT("PromoteParameterBlendedBSDFToOperator(StrataWeightParameterBlending(%s, %s), Parameters.StrataTree, %u, %u, %u, %u)"),
			*GetParameterCode(A),
			*GetParameterCode(Weight),
			PromoteToOperator->Index,
			PromoteToOperator->BSDFIndex,
			PromoteToOperator->LayerDepth,
			PromoteToOperator->bIsBottom ? 1 : 0
		);
	}

	return AddCodeChunk(
		MCT_Strata, TEXT("StrataWeightParameterBlending(%s, %s)"),
		*GetParameterCode(A),
		*GetParameterCode(Weight)
	);
}

int32 FHLSLMaterialTranslator::StrataTransmittanceToMFP(int32 TransmittanceColor, int32 DesiredThickness, int32 OutputIndex)
{
	if (OutputIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const int32 DefaultThicknessCodechunk = AddInlinedCodeChunk(MCT_Float1, TEXT("%f"), STRATA_LAYER_DEFAULT_THICKNESS_CM);
	switch (OutputIndex)
	{
	case 0:
		return AddCodeChunk(MCT_Float3, 
			// For the math to be valid, input to TransmittanceToMeanFreePath must be in meter.
			// Then the output needs to be is converted to centimeters.
			TEXT("(TransmittanceToMeanFreePath(%s, %s * CENTIMETER_TO_METER) * METER_TO_CENTIMETER)"), 
			*GetParameterCode(TransmittanceColor),
			*GetParameterCode(DesiredThickness == INDEX_NONE ? DefaultThicknessCodechunk : DesiredThickness));
		break;
	case 1:
		// Thickness to be plugged into other nodes thickness input.
		// This matches the Slab node default using STRATA_LAYER_DEFAULT_THICKNESS_CM
		return DesiredThickness == INDEX_NONE ? DefaultThicknessCodechunk : DesiredThickness;
		break;
	}
	return INDEX_NONE;
}

int32 FHLSLMaterialTranslator::StrataMetalnessToDiffuseAlbedoF0(int32 BaseColor, int32 Specular, int32 Metallic, int32 OutputIndex)
{
	if (OutputIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	switch (OutputIndex)
	{
	case 0:
		return AddCodeChunk(MCT_Float3,
			TEXT("ComputeDiffuseAlbedo(%s, saturate(%s))"),
			*GetParameterCode(BaseColor),
			*GetParameterCode(Metallic));
		break;
	case 1:
		return AddCodeChunk(MCT_Float3,
			TEXT("ComputeF0(%s, %s, saturate(%s))"),
			*GetParameterCode(Specular),
			*GetParameterCode(BaseColor),
			*GetParameterCode(Metallic));
		break;
	}
	return INDEX_NONE;
}

int32 FHLSLMaterialTranslator::StrataHazinessToSecondaryRoughness(int32 BaseRoughness, int32 Haziness, int32 OutputIndex)
{
	if (OutputIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	switch (OutputIndex)
	{
	case 0:
		return AddCodeChunk(MCT_Float1,
			TEXT("StrataComputeHazeRoughness(saturate(%s))"),
			*GetParameterCode(BaseRoughness));
		break;
	case 1:
		return AddCodeChunk(MCT_Float1,
			TEXT("StrataComputeHazeWeight(saturate(%s), saturate(%s))"),
			*GetParameterCode(BaseRoughness),
			*GetParameterCode(Haziness));
		break;
	}
	return INDEX_NONE;
}

int32 FHLSLMaterialTranslator::StrataThinFilm(int32 NormalCodeChunk, int32 SpecularColorCodeChunk, int32 EdgeSpecularColorCodeChunk, int32 ThicknessCodeChunk, int32 IORCodeChunk, int32 OutputIndex)
{
	if (NormalCodeChunk == INDEX_NONE || SpecularColorCodeChunk == INDEX_NONE || EdgeSpecularColorCodeChunk == INDEX_NONE
		|| ThicknessCodeChunk == INDEX_NONE || IORCodeChunk == INDEX_NONE)
	{
		Errorf(TEXT("Substrate error: one of SubstrateThinFilm node input is invalid (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
		return INDEX_NONE;
	}
	if (OutputIndex < 0 || OutputIndex > 1)
	{
		Errorf(TEXT("Substrate error: SubstrateThinFilm output index is invalid (asset: %s).\r\n"), *Material->GetDebugName(), *Material->GetAssetPath().ToString());
		return INDEX_NONE;
	}
	return AddCodeChunk(
		MCT_Float3, TEXT("StrataGetThinFilmF0F90(%s, %s, %s, %s, %s)%s"),
		*GetParameterCode(Dot(NormalCodeChunk, CameraVector())),
		*GetParameterCode(SpecularColorCodeChunk),
		*GetParameterCode(EdgeSpecularColorCodeChunk),
		*GetParameterCode(ThicknessCodeChunk),
		*GetParameterCode(IORCodeChunk),
		OutputIndex == 0 ? TEXT(".F0") : TEXT(".F90")
	);
}

int32 FHLSLMaterialTranslator::StrataCompilePreview(int32 StrataDataCodeChunk)
{
	if (StrataDataCodeChunk == INDEX_NONE)
	{
		return INDEX_NONE;
	}
	// Instead of using a preview color, we might go with a sphere+lighting preview. 
	// The only problem we would have to solve is to process the strata tree before for the sub tree. 
	// Or have dedicated lighting preview functions for StrataData that have not be filled in by the Strata tree processing.
	int32 PreviewCodeChunk = AddCodeChunk(MCT_Float3, TEXT("%s.PreviewColor"), *GetParameterCode(StrataDataCodeChunk));
	return PreviewCodeChunk;
}

bool FHLSLMaterialTranslator::StrataSkipsOpacityEvaluation()
{
	return !IsTranslucentBlendMode(Material)
		&& Material->GetShadingModels().CountShadingModels() == 1
		&& !Material->GetShadingModels().HasShadingModel(MSM_SingleLayerWater)
		&& !Material->GetShadingModels().HasShadingModel(MSM_Subsurface)
		&& !Material->GetShadingModels().HasShadingModel(MSM_SubsurfaceProfile)
		&& !Material->GetShadingModels().HasShadingModel(MSM_TwoSidedFoliage)
		&& !Material->GetShadingModels().HasShadingModel(MSM_PreintegratedSkin);
}

FGuid FHLSLMaterialTranslator::StrataTreeStackPush(UMaterialExpression* Expression, uint32 InputIndex)
{
	// Create an md5 hash for the parent, its input pin index and current node to represent the path.
	uint32 IntputHashBuffer[9];
	FGuid PreviousNodeGuid = StrataTreeStackGetPathUniqueId();
	FGuid NodeGuid = Expression->MaterialExpressionGuid;
	IntputHashBuffer[0] = PreviousNodeGuid.A;
	IntputHashBuffer[1] = PreviousNodeGuid.B;
	IntputHashBuffer[2] = PreviousNodeGuid.C;
	IntputHashBuffer[3] = PreviousNodeGuid.D;
	IntputHashBuffer[4] = InputIndex;
	IntputHashBuffer[5] = NodeGuid.A;
	IntputHashBuffer[6] = NodeGuid.B;
	IntputHashBuffer[7] = NodeGuid.C;
	IntputHashBuffer[8] = NodeGuid.D;

	uint32 OutputHashBuffer[]{ 0, 0, 0, 0 };
	FMD5 IdentifierStringHash;
	IdentifierStringHash.Update((uint8*)IntputHashBuffer, sizeof(IntputHashBuffer));
	IdentifierStringHash.Final((uint8*)&OutputHashBuffer);

	StrataNodeIdentifierStack.Push(FGuid(OutputHashBuffer[0], OutputHashBuffer[1], OutputHashBuffer[2], OutputHashBuffer[3]));

#if DEBUG_STRATA_TREE_STACK
	UE_LOG(LogMaterial, Display, TEXT(" StrataTreeStack: Push (input %i of %s)"), InputIndex , *Expression->GetName());
	TStringBuilder<2048> GuidStack;
	for (auto& Entry : StrataNodeIdentifierStack)
	{
		GuidStack.Append(*Entry.ToString());
		GuidStack.Append(TEXT("  "));
	}
	UE_LOG(LogMaterial, Display, TEXT(" StrataTreeStack: %s."), *GuidStack);
#endif

	bStrataTreeOutOfStackDepthOccurred = bStrataTreeOutOfStackDepthOccurred || (StrataNodeIdentifierStack.Num() > STRATA_TREE_MAX_DEPTH);

	return StrataNodeIdentifierStack.Top();
}

FGuid FHLSLMaterialTranslator::StrataTreeStackGetPathUniqueId()
{
	return StrataNodeIdentifierStack.Top();
}

FGuid FHLSLMaterialTranslator::StrataTreeStackGetParentPathUniqueId()
{
	if (StrataNodeIdentifierStack.Num() < 2)
	{
		// return some default when strata tree stack unique guid cannot be found
		FGuid NullParent;
		return NullParent;
	}
	return StrataNodeIdentifierStack.Last(1);
}

void FHLSLMaterialTranslator::StrataTreeStackPop()
{
	check(StrataNodeIdentifierStack.Num() >= 2);// 2 because there must always be the root remaining.
	StrataNodeIdentifierStack.Pop();

#if DEBUG_STRATA_TREE_STACK
	TStringBuilder<2048> GuidStack;
	for (auto& Entry : StrataNodeIdentifierStack)
	{
		GuidStack.Append(*Entry.ToString());
		GuidStack.Append(TEXT("  "));
	}
	UE_LOG(LogMaterial, Display, TEXT(" StrataTreeStack: Pop %s."), *GuidStack);
#endif
}

bool FHLSLMaterialTranslator::GetStrataTreeOutOfStackDepthOccurred()
{
	return bStrataTreeOutOfStackDepthOccurred;
}

int32 FHLSLMaterialTranslator::StrataThicknessStackGetThicknessIndex()
{
	return StrataThicknessStack.Top();
}

int32 FHLSLMaterialTranslator::StrataThicknessStackGetThicknessCode(int32 Index)
{
	int32 OutCode = INDEX_NONE;
	if (Index == INDEX_NONE || Index >= StrataThicknessIndexToExpressionInput.Num())
	{
		UE_LOG(LogMaterial, Error, TEXT(" StrataThichkness: %i could not be found)"), Index);
	}
	else if (FExpressionInput* Input = StrataThicknessIndexToExpressionInput[Index])
	{
		OutCode = Input->GetTracedInput().Expression ? Input->Compile(this) : Constant(STRATA_LAYER_DEFAULT_THICKNESS_CM);
		EMaterialValueType Type = GetType(OutCode);
		if (IsLWCType(Type))
		{
			Type = MakeNonLWCType(Type);
			OutCode = ValidCast(OutCode, Type);
		}	
	}
	if (OutCode == INDEX_NONE)
	{
		OutCode = Constant(STRATA_LAYER_DEFAULT_THICKNESS_CM);
	}
	return OutCode;
}

int32 FHLSLMaterialTranslator::StrataThicknessStackPush(UMaterialExpression* Expression, FExpressionInput* Input)
{
	int32 Index = StrataThicknessIndexToExpressionInput.Num();
	StrataThicknessIndexToExpressionInput.Add(Input);
	StrataThicknessStack.Push(Index);
	return Index;	
}

void FHLSLMaterialTranslator::StrataThicknessStackPop()
{
	check(StrataThicknessStack.Num() >= 1);
	StrataThicknessStack.Pop();
}

int32 FHLSLMaterialTranslator::MapARPassthroughCameraUV(int32 UV)
{
	if (UV == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	int32 UVPair0 = AddInlinedCodeChunkZeroDeriv(MCT_Float4, TEXT("ResolvedView.XRPassthroughCameraUVs[0]"));
	int32 UVPair1 = AddInlinedCodeChunkZeroDeriv(MCT_Float4, TEXT("ResolvedView.XRPassthroughCameraUVs[1]"));

	int32 ULerp = Lerp(UVPair0, UVPair1, ComponentMask(UV, 1, 0, 0, 0));
	return Lerp(ComponentMask(ULerp, 1, 1, 0, 0), ComponentMask(ULerp, 0, 0, 1, 1), ComponentMask(UV, 0, 1, 0, 0));
}

int32 FHLSLMaterialTranslator::AccessMaterialAttribute(int32 CodeIndex, const FGuid& AttributeID)
{
	check(GetParameterType(CodeIndex) == MCT_MaterialAttributes);

	const FString AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName(AttributeID);
	const EMaterialValueType AttributeType = FMaterialAttributeDefinitionMap::GetValueType(AttributeID);
	return AddInlinedCodeChunk(
		AttributeType,
		TEXT("%s.%s"),
		*GetParameterCode(CodeIndex),
		*AttributeName);
}

static bool IsMaterialPathExemptFromRestrictiveMode(FStringBuilderBase& PathName)
{
	const FStringView PathNameView = PathName;

	// Anything currently being edited is NOT exempt from restrictive mode
	const FStringView EngineTransientPath = TEXT("/Engine/Transient.");
	if (PathNameView.StartsWith(EngineTransientPath))
	{
		PathName.RemoveAt(0, EngineTransientPath.Len());
		return false;
	}

	// Anything else in /Engine/ and /Game/ folders is exempt
	if (PathNameView.StartsWith(TEXT("/Engine/")) || PathNameView.StartsWith(TEXT("/Game/")))
	{
		return true;
	}

	return false;
}

static bool IsMaterialExpressionExemptFromRestrictiveMode(const UMaterialExpression* Expression, FStringBuilderBase& PathName)
{
	if (Expression)
	{
		if (Expression->Material)
		{
			Expression->Material->GetPathName(nullptr, PathName);
			return IsMaterialPathExemptFromRestrictiveMode(PathName);
		}
		if (Expression->Function)
		{
			Expression->Function->GetPathName(nullptr, PathName);
			return IsMaterialPathExemptFromRestrictiveMode(PathName);
		}
	}
	return false;
}

bool FHLSLMaterialTranslator::ValidateMaterialExpressionPermission(const UMaterialExpression* Expression)
{
	FNameBuilder MaterialPathName;
	if (!IsExpressionClassPermitted(Expression->GetClass()) && !IsMaterialExpressionExemptFromRestrictiveMode(Expression, MaterialPathName))
	{
		if (MaterialPathName.Len() > 0)
		{
			Errorf(TEXT("Material expression %s not permitted in %s"), *Expression->GetCreationName().ToString(), *MaterialPathName);
		}
		else
		{
			Errorf(TEXT("Material expression %s not permitted"), *Expression->GetCreationName().ToString());
		}
		return false;
	}
	return true;
}

int32 FHLSLMaterialTranslator::CustomExpression( class UMaterialExpressionCustom* Custom, int32 OutputIndex, TArray<int32>& CompiledInputs )
{
	if (!ValidateMaterialExpressionPermission(Custom))
	{
		return INDEX_NONE;
	}

	const FMaterialCustomExpressionEntry* CustomEntry = nullptr;
	for (const FMaterialCustomExpressionEntry& Entry : CustomExpressions)
	{
		if (Entry.Expression == Custom &&
			Entry.ScopeID == CurrentScopeID)
		{
			bool bInputsMatch = true;
			for(int32 InputIndex = 0; InputIndex < CompiledInputs.Num(); ++InputIndex)
			{
				const uint64 InputHash = GetParameterHash(CompiledInputs[InputIndex]);
				if (Entry.InputHash[InputIndex] != InputHash)
				{
					bInputsMatch = false;
					break;
				}
			}

			if (bInputsMatch)
			{
				CustomEntry = &Entry;
				break;
			}
		}
	}

	if (!CustomEntry)
	{
		FString OutputTypeString;
		EMaterialValueType OutputType;
		switch (Custom->OutputType)
		{
		case CMOT_Float2:
			OutputType = MCT_Float2;
			OutputTypeString = TEXT("MaterialFloat2");
			break;
		case CMOT_Float3:
			OutputType = MCT_Float3;
			OutputTypeString = TEXT("MaterialFloat3");
			break;
		case CMOT_Float4:
			OutputType = MCT_Float4;
			OutputTypeString = TEXT("MaterialFloat4");
			break;
		case CMOT_MaterialAttributes:
			OutputType = MCT_MaterialAttributes;
			OutputTypeString = TEXT("FMaterialAttributes");
			break;
		default:
			OutputType = MCT_Float;
			OutputTypeString = TEXT("MaterialFloat");
			break;
		}

		// Declare implementation function
		FString InputParamDecl;
		check(Custom->Inputs.Num() == CompiledInputs.Num());
		for (int32 i = 0; i < Custom->Inputs.Num(); i++)
		{
			// skip over unnamed inputs
			if (Custom->Inputs[i].InputName.IsNone())
			{
				continue;
			}
			InputParamDecl += TEXT(",");
			const FString InputNameStr = Custom->Inputs[i].InputName.ToString();
			switch (GetParameterType(CompiledInputs[i]))
			{
			case MCT_Float:
			case MCT_Float1:
				InputParamDecl += TEXT("MaterialFloat ");
				InputParamDecl += InputNameStr;
				break;
			case MCT_Float2:
				InputParamDecl += TEXT("MaterialFloat2 ");
				InputParamDecl += InputNameStr;
				break;
			case MCT_Float3:
				InputParamDecl += TEXT("MaterialFloat3 ");
				InputParamDecl += InputNameStr;
				break;
			case MCT_Float4:
				InputParamDecl += TEXT("MaterialFloat4 ");
				InputParamDecl += InputNameStr;
				break;
			case MCT_LWCScalar:
				InputParamDecl += TEXT("float ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", FLWCScalar LWC");
				InputParamDecl += InputNameStr;
				break;
			case MCT_LWCVector2:
				InputParamDecl += TEXT("float2 ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", FLWCVector2 LWC");
				InputParamDecl += InputNameStr;
				break;
			case MCT_LWCVector3:
				InputParamDecl += TEXT("float3 ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", FLWCVector3 LWC");
				InputParamDecl += InputNameStr;
				break;
			case MCT_LWCVector4:
				InputParamDecl += TEXT("float4 ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", FLWCVector4 LWC");
				InputParamDecl += InputNameStr;
				break;
			case MCT_Texture2D:
				InputParamDecl += TEXT("Texture2D ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT("Sampler ");
				break;
			case MCT_TextureCube:
				InputParamDecl += TEXT("TextureCube ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT("Sampler ");
				break;
			case MCT_Texture2DArray:
				InputParamDecl += TEXT("Texture2DArray ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT("Sampler ");
				break;
			case MCT_TextureCubeArray:
				InputParamDecl += TEXT("TextureCubeArray ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT("Sampler ");
				break;
			case MCT_TextureExternal:
				InputParamDecl += TEXT("TextureExternal ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT("Sampler ");
				break;
			case MCT_VolumeTexture:
				InputParamDecl += TEXT("Texture3D ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT(", SamplerState ");
				InputParamDecl += InputNameStr;
				InputParamDecl += TEXT("Sampler ");
				break;
			default:
				return Errorf(TEXT("Bad type %s for %s input %s"), DescribeType(GetParameterType(CompiledInputs[i])), *Custom->Description, *InputNameStr);
				break;
			}
		}

		for (const FCustomOutput& CustomOutput : Custom->AdditionalOutputs)
		{
			if (CustomOutput.OutputName.IsNone())
			{
				continue;
			}

			InputParamDecl += TEXT(", inout "); // use 'inout', so custom code may optionally avoid setting certain outputs (will default to 0)
			const FString OutputNameStr = CustomOutput.OutputName.ToString();
			switch (CustomOutput.OutputType)
			{
			case CMOT_Float1:
				InputParamDecl += TEXT("MaterialFloat ");
				InputParamDecl += OutputNameStr;
				break;
			case CMOT_Float2:
				InputParamDecl += TEXT("MaterialFloat2 ");
				InputParamDecl += OutputNameStr;
				break;
			case CMOT_Float3:
				InputParamDecl += TEXT("MaterialFloat3 ");
				InputParamDecl += OutputNameStr;
				break;
			case CMOT_Float4:
				InputParamDecl += TEXT("MaterialFloat4 ");
				InputParamDecl += OutputNameStr;
				break;
			case CMOT_MaterialAttributes:
				InputParamDecl += TEXT("FMaterialAttributes ");
				InputParamDecl += OutputNameStr;
				break;
			default:
				return Errorf(TEXT("Bad type %d for %s output %s"), static_cast<int32>(CustomOutput.OutputType.GetValue()), *Custom->Description, *OutputNameStr);
				break;
			}
		}

		int32 CustomExpressionIndex = CustomExpressions.Num();
		FString Code = Custom->Code;
		if (!Code.Contains(TEXT("return")))
		{
			Code = FString(TEXT("return ")) + Code + TEXT(";");
		}
		Code.ReplaceInline(TEXT("\n"), TEXT("\r\n"), ESearchCase::CaseSensitive);

		FString ParametersType = ShaderFrequency == SF_Vertex ? TEXT("Vertex") : TEXT("Pixel");

		FMaterialCustomExpressionEntry& Entry = CustomExpressions.AddDefaulted_GetRef();
		CustomEntry = &Entry;
		Entry.Expression = Custom;
		Entry.ScopeID = CurrentScopeID;
		Entry.InputHash.Empty(CompiledInputs.Num());
		for (int32 InputIndex = 0; InputIndex < CompiledInputs.Num(); ++InputIndex)
		{
			const uint64 InputHash = GetParameterHash(CompiledInputs[InputIndex]);
			Entry.InputHash.Add(InputHash);
		}

		for (FCustomDefine DefineEntry : Custom->AdditionalDefines)
		{
			FString DefineStatement = TEXT("#ifndef ") + DefineEntry.DefineName + LINE_TERMINATOR;
			DefineStatement += TEXT("#define ") + DefineEntry.DefineName + TEXT(" ") + DefineEntry.DefineValue + LINE_TERMINATOR;
			DefineStatement += TEXT("#endif//") + DefineEntry.DefineName + LINE_TERMINATOR;

			Entry.Implementation += DefineStatement;
		}

		for (FString IncludeFile : Custom->IncludeFilePaths)
		{
			FString IncludeStatement = TEXT("#include ");
			IncludeStatement += TEXT("\"");
			IncludeStatement += IncludeFile;
			IncludeStatement += TEXT("\"");
			IncludeStatement += LINE_TERMINATOR;

			Entry.Implementation += IncludeStatement;
		}

		Entry.Implementation += FString::Printf(TEXT("%s CustomExpression%d(FMaterial%sParameters Parameters%s)\r\n{\r\n%s\r\n}\r\n"), *OutputTypeString, CustomExpressionIndex, *ParametersType, *InputParamDecl, *Code);
		const uint64 ImplementationHash = CityHash64((char*)*Entry.Implementation, Entry.Implementation.Len() * sizeof(TCHAR));

		Entry.OutputCodeIndex.Empty(Custom->AdditionalOutputs.Num() + 1);
		Entry.OutputCodeIndex.Add(INDEX_NONE); // Output0 will hold the return value for the custom expression function, patch it in later

		// Create local temp variables to hold results of additional outputs
		for (const FCustomOutput& CustomOutput : Custom->AdditionalOutputs)
		{
			if (CustomOutput.OutputName.IsNone())
			{
				continue;
			}

			// We're creating 0-initialized values to be filled in by the custom expression, so generate hashes based on code/name of the output
			const FString OutputName = CustomOutput.OutputName.ToString();
			const uint64 BaseHash = CityHash64WithSeed((char*)*OutputName, OutputName.Len() * sizeof(TCHAR), ImplementationHash);

			int32 OutputCode = INDEX_NONE;
			switch (CustomOutput.OutputType)
			{
			case CMOT_Float1: OutputCode = AddCodeChunkWithHash(BaseHash, MCT_Float, TEXT("0.0f")); break;
			case CMOT_Float2: OutputCode = AddCodeChunkWithHash(BaseHash, MCT_Float2, TEXT("MaterialFloat2(0.0f, 0.0f)")); break;
			case CMOT_Float3: OutputCode = AddCodeChunkWithHash(BaseHash, MCT_Float3, TEXT("MaterialFloat3(0.0f, 0.0f, 0.0f)")); break;
			case CMOT_Float4: OutputCode = AddCodeChunkWithHash(BaseHash, MCT_Float4, TEXT("MaterialFloat4(0.0f, 0.0f, 0.0f, 0.0f)")); break;
			case CMOT_MaterialAttributes: OutputCode = AddCodeChunkWithHash(BaseHash, MCT_MaterialAttributes, TEXT("(FMaterialAttributes)0.0f")); break;
			default: checkNoEntry(); break;
			}
			Entry.OutputCodeIndex.Add(OutputCode);
		}

		// Add call to implementation function
		FString CodeChunk = FString::Printf(TEXT("CustomExpression%d(Parameters"), CustomExpressionIndex);
		for (int32 i = 0; i < CompiledInputs.Num(); i++)
		{
			// skip over unnamed inputs
			if (Custom->Inputs[i].InputName.IsNone())
			{
				continue;
			}

			FString ParamCode = GetParameterCode(CompiledInputs[i]);
			EMaterialValueType ParamType = GetParameterType(CompiledInputs[i]);

			CodeChunk += TEXT(",");
			if (ParamType & MCT_LWCType)
			{
				// LWC types get two values, first the value converted to float, then the raw LWC value
				// This way legacy custom expressions can continue to operate on regular float values
				CodeChunk += TEXT("LWCToFloat(");
				CodeChunk += *ParamCode;
				CodeChunk += TEXT("),");
			}
			CodeChunk += *ParamCode;
			if (ParamType == MCT_Texture2D || ParamType == MCT_TextureCube || ParamType == MCT_TextureCubeArray || ParamType == MCT_Texture2DArray || ParamType == MCT_TextureExternal || ParamType == MCT_VolumeTexture)
			{
				CodeChunk += TEXT(",");
				CodeChunk += *ParamCode;
				CodeChunk += TEXT("Sampler");
			}
		}
		// Pass 'out' parameters
		for (int32 i = 1; i < Entry.OutputCodeIndex.Num(); ++i)
		{
			FString ParamCode = GetParameterCode(Entry.OutputCodeIndex[i]);
			CodeChunk += TEXT(",");
			CodeChunk += *ParamCode;
		}

		CodeChunk += TEXT(")");

		// Save result of function as first output
		Entry.OutputCodeIndex[0] = AddCodeChunk(
			OutputType,
			*CodeChunk
		);
	}

	check(CustomEntry);
	if (!CustomEntry->OutputCodeIndex.IsValidIndex(OutputIndex))
	{
		return Errorf(TEXT("Invalid custom expression OutputIndex %d"), OutputIndex);
	}

	int32 Result = CustomEntry->OutputCodeIndex[OutputIndex];
	if (Custom->IsResultMaterialAttributes(OutputIndex))
	{
		Result = AccessMaterialAttribute(Result, GetMaterialAttribute());
	}
	return Result;
}

int32 FHLSLMaterialTranslator::CustomOutput(class UMaterialExpressionCustomOutput* Custom, int32 OutputIndex, int32 OutputCode)
{
	if (MaterialProperty != MP_MAX)
	{
		return Errorf(TEXT("A Custom Output node should not be attached to the %s material property"), *FMaterialAttributeDefinitionMap::GetAttributeName(MaterialProperty));
	}

	if (OutputCode == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	EMaterialValueType OutputType = GetParameterType(OutputCode);
	if (!IsFloatNumericType(OutputType))
	{
		return Errorf(TEXT("Bad type %s for %s"), DescribeType(OutputType), *Custom->GetDescription());
	}

	FString OutputTypeString = HLSLTypeString(OutputType);
	FString Definitions;
	FString Body;

	// For now just grab the finite differences version, and use both for finite and analytic. Should fix later.
	if ((*CurrentScopeChunks)[OutputCode].UniformExpression && !(*CurrentScopeChunks)[OutputCode].UniformExpression->IsConstant())
	{
		Body = GetParameterCode(OutputCode);
	}
	else
	{
		GetFixedParameterCode(OutputCode, *CurrentScopeChunks, Definitions, Body, CompiledPDV_FiniteDifferences);
	}

	const FString FunctionNameBase = FString::Printf(TEXT("%s%d"), *Custom->GetFunctionName(), OutputIndex);

	// Primary function will have _LWC suffix if it returns an LWC type
	// We also define a pre-processor symbol to indicate the custom output function is available so that
	// shaders can implement some fallback behavior for cases where the custom output has not been added
	FString ImplementationCodeFinite = FString::Printf(TEXT("#define HAVE_%s 1\r\n%s %s%s(%s FMaterial%sParameters Parameters)\r\n{\r\n%s return %s;\r\n}\r\n"),
		*FunctionNameBase,
		*OutputTypeString,
		*FunctionNameBase,
		IsLWCType(OutputType) ? TEXT("_LWC") : TEXT(""),
		ShaderFrequency == SF_Vertex ? TEXT("const") : TEXT("inout"), // don't use 'inout' for Vertex parameters, to work around potential compiler bug with FXC
		ShaderFrequency == SF_Vertex ? TEXT("Vertex") : TEXT("Pixel"),
		*Definitions,
		*Body);
	if (IsLWCType(OutputType))
	{
		// Add a wrapper with no suffix to return a non-LWC type
		ImplementationCodeFinite += FString::Printf(TEXT("%s %s(%s FMaterial%sParameters Parameters) { return LWCToFloat(%s_LWC(Parameters)); }\r\n"),
			HLSLTypeString(MakeNonLWCType(OutputType)),
			*FunctionNameBase,
			ShaderFrequency == SF_Vertex ? TEXT("const") : TEXT("inout"),
			ShaderFrequency == SF_Vertex ? TEXT("Vertex") : TEXT("Pixel"),
			*FunctionNameBase);
	}
	else
	{
		// Add a wrapper with LWC suffix to return a LWC type
		ImplementationCodeFinite += FString::Printf(TEXT("%s %s_LWC(%s FMaterial%sParameters Parameters) { return LWCPromote(%s(Parameters)); }\r\n"),
			HLSLTypeString(MakeLWCType(OutputType)),
			*FunctionNameBase,
			ShaderFrequency == SF_Vertex ? TEXT("const") : TEXT("inout"),
			ShaderFrequency == SF_Vertex ? TEXT("Vertex") : TEXT("Pixel"),
			*FunctionNameBase);
	}

	DerivativeVariations[CompiledPDV_FiniteDifferences].CustomOutputImplementations.Add(ImplementationCodeFinite);

	// FIXEM: for now just skipping these, as they create invalid code.
	//FString ImplementationCodeAnalytic = FString::Printf(TEXT("%s %s%d_Analytic(FMaterial%sParameters Parameters)\r\n{\r\n%s return %s;\r\n}\r\n"), *OutputTypeString, *Custom->GetFunctionName(), OutputIndex, ShaderFrequency == SF_Vertex ? TEXT("Vertex") : TEXT("Pixel"), *Definitions, *Body);
	//DerivativeVariations[CompiledPDV_Analytic].CustomOutputImplementations.Add(ImplementationCodeAnalytic);

	// return value is not used
	return INDEX_NONE;
}

int32 FHLSLMaterialTranslator::VirtualTextureOutput(uint8 AttributeMask)
{
	MaterialCompilationOutput.bHasRuntimeVirtualTextureOutputNode |= AttributeMask != 0;
	MaterialCompilationOutput.RuntimeVirtualTextureOutputAttributeMask |= AttributeMask;

	// return value is not used
	return INDEX_NONE;
}

#if HANDLE_CUSTOM_OUTPUTS_AS_MATERIAL_ATTRIBUTES
/** Used to translate code for custom output attributes such as ClearCoatBottomNormal */
void FHLSLMaterialTranslator::GenerateCustomAttributeCode(int32 OutputIndex, int32 OutputCode, EMaterialValueType OutputType, FString& DisplayName)
{
	check(MaterialProperty == MP_CustomOutput);
	check(OutputIndex >= 0 && OutputCode != INDEX_NONE);

	FString OutputTypeString;
	switch (OutputType)
	{
		case MCT_Float:
		case MCT_Float1:
			OutputTypeString = TEXT("MaterialFloat");
			break;
		case MCT_Float2:
			OutputTypeString += TEXT("MaterialFloat2");
			break;
		case MCT_Float3:
			OutputTypeString += TEXT("MaterialFloat3");
			break;
		case MCT_Float4:
			OutputTypeString += TEXT("MaterialFloat4");
			break;
		default:
			check(0);
	}

	FString Definitions;
	FString Body;

	if ((*CurrentScopeChunks)[OutputCode].UniformExpression && !(*CurrentScopeChunks)[OutputCode].UniformExpression->IsConstant())
	{
		Body = GetParameterCode(OutputCode);
	}
	else
	{
		GetFixedParameterCode(OutputCode, *CurrentScopeChunks, Definitions, Body);
	}

	FString ImplementationCode = FString::Printf(TEXT("%s %s%d(FMaterial%sParameters Parameters)\r\n{\r\n%s return %s;\r\n}\r\n"), *OutputTypeString, *DisplayName, OutputIndex, ShaderFrequency == SF_Vertex ? TEXT("Vertex") : TEXT("Pixel"), *Definitions, *Body);
	CustomOutputImplementations.Add(ImplementationCode);
}
#endif

/**
	* Adds code to return a random value shared by all geometry for any given instanced static mesh
	*
	* @return	Code index
	*/
int32 FHLSLMaterialTranslator::PerInstanceRandom()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Vertex && ShaderFrequency != SF_RayHitGroup)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	else
	{
		MaterialCompilationOutput.bUsesPerInstanceRandom = true;
		return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("GetPerInstanceRandom(Parameters)"));
	}
}

/**
	* Returns a mask that either enables or disables selection on a per-instance basis when instancing
	*
	* @return	Code index
	*/
int32 FHLSLMaterialTranslator::PerInstanceFadeAmount()
{
	if (ShaderFrequency != SF_Pixel && ShaderFrequency != SF_Vertex)
	{
		return NonVertexOrPixelShaderExpressionError();
	}
	else
	{
		return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("GetPerInstanceFadeAmount(Parameters)"));
	}
}

/**
 *	Returns a custom data on a per-instance basis when instancing
 *	@DataIndex - index in array that represents custom data
 *
 *	@return	Code index
 */
int32 FHLSLMaterialTranslator::PerInstanceCustomData(int32 DataIndex, int32 DefaultValueIndex)
{
	if (DefaultValueIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// The case where ShaderFrequency is not SF_Vertex only works with Nanite - TODO: Edge case the error
	MaterialCompilationOutput.bUsesPerInstanceCustomData = true;
	return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("GetPerInstanceCustomData(Parameters, %d, %s)"), DataIndex, *GetParameterCode(DefaultValueIndex));
}

/**
 *	Returns a custom data on a per-instance basis when instancing
 *	@DataIndex - index in array that represents custom data
 *
 *	@return	Code index
 */
int32 FHLSLMaterialTranslator::PerInstanceCustomData3Vector(int32 DataIndex, int32 DefaultValueIndex)
{
	if (DefaultValueIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	// The case when ShaderFrequency is not SF_Vertex only works with Nanite - TODO: Edge case the error
	MaterialCompilationOutput.bUsesPerInstanceCustomData = true;
	return AddInlinedCodeChunkZeroDeriv(MCT_Float3, TEXT("GetPerInstanceCustomData3Vector(Parameters, %d, %s)"), DataIndex, *GetParameterCode(DefaultValueIndex));
}

/**
	* Returns a float2 texture coordinate after 2x2 transform and offset applied
	*
	* @return	Code index
	*/
int32 FHLSLMaterialTranslator::RotateScaleOffsetTexCoords(int32 TexCoordCodeIndex, int32 RotationScale, int32 Offset)
{
	if (IsAnalyticDerivEnabled())
	{
		return DerivativeAutogen.GenerateRotateScaleOffsetTexCoordsFunc(*this, TexCoordCodeIndex, RotationScale, Offset);
	}
	else
	{
	return AddCodeChunk(MCT_Float2,
		TEXT("RotateScaleOffsetTexCoords(%s, %s, %s.xy)"),
		*GetParameterCode(TexCoordCodeIndex),
		*GetParameterCode(RotationScale),
		*GetParameterCode(Offset));
}
}

/**
* Handles SpeedTree vertex animation (wind, smooth LOD)
*
* @return	Code index
*/
int32 FHLSLMaterialTranslator::SpeedTree(int32 GeometryArg, int32 WindArg, int32 LODArg, float BillboardThreshold, bool bAccurateWindVelocities, bool bExtraBend, int32 ExtraBendArg)
{ 
	if (Material && Material->IsUsedWithSkeletalMesh())
	{
		return Error(TEXT("SpeedTree node not currently supported for Skeletal Meshes, please disable usage flag."));
	}

	if (ShaderFrequency != SF_Vertex)
	{
		return NonVertexShaderExpressionError();
	}
	else
	{
		bUsesSpeedTree = true;

		AllocateSlot(AllocatedUserVertexTexCoords, 2, 6);

		// Only generate previous frame's computations if required and opted-in
		const bool bEnablePreviousFrameInformation = bCompilingPreviousFrame && bAccurateWindVelocities;
		return AddCodeChunk(MCT_Float3, TEXT("GetSpeedTreeVertexOffset(Parameters, %s, %s, %s, %g, %s, %s, %s)"), *GetParameterCode(GeometryArg), *GetParameterCode(WindArg), *GetParameterCode(LODArg), BillboardThreshold, bEnablePreviousFrameInformation ? TEXT("true") : TEXT("false"), bExtraBend ? TEXT("true") : TEXT("false"), *GetParameterCode(ExtraBendArg, TEXT("float3(0,0,0)")));
	}
}

/**Experimental access to the EyeAdaptation RT for Post Process materials. Can be one frame behind depending on the value of BlendableLocation. */
int32 FHLSLMaterialTranslator::EyeAdaptation()
{
	if( ShaderFrequency != SF_Pixel )
	{
		return NonPixelShaderExpressionError();
	}

	MaterialCompilationOutput.bUsesEyeAdaptation = true;

	return AddInlinedCodeChunkZeroDeriv(MCT_Float, TEXT("EyeAdaptationLookup()"));
}

/**Experimental access to the EyeAdaptation RT for applying an inverse. */
int32 FHLSLMaterialTranslator::EyeAdaptationInverse(int32 LightValueArg, int32 AlphaArg)
{
	if (ShaderFrequency != SF_Pixel)
	{
		return NonPixelShaderExpressionError();
	}

	if (LightValueArg == INDEX_NONE || AlphaArg == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	const EMaterialValueType LightValueType = GetParameterType(LightValueArg);
	const EMaterialValueType ResultType = LightValueType;

	if (!IsFloatNumericType(LightValueType))
	{
		Errorf(TEXT("EyeAdaptationInverse expects a float numeric type for LightValue"));
		return INDEX_NONE;
	}

	if (GetParameterType(AlphaArg) != MCT_Float)
	{
		Errorf(TEXT("EyeAdaptationInverse expects a float type for Alpha"));
		return INDEX_NONE;
	}

	MaterialCompilationOutput.bUsesEyeAdaptation = true;

	int32 Multiplier = AddInlinedCodeChunk(MCT_Float, TEXT("EyeAdaptationInverseLookup(%s)"), *GetParameterCode(AlphaArg));

	// return LightValue scaled by inverse eye adaptation
	return Mul(LightValueArg, Multiplier);
}

// to only have one piece of code dealing with error handling if the Primitive constant buffer is not used.
// @param Name e.g. TEXT("ObjectWorldPositionAndRadius.w")
int32 FHLSLMaterialTranslator::GetPrimitiveProperty(EMaterialValueType Type, const TCHAR* ExpressionName, const TCHAR* HLSLName)
{
	const EMaterialDomain Domain = (const EMaterialDomain)Material->GetMaterialDomain();

	if(Domain != MD_Surface && Domain != MD_Volume)
	{
		Errorf(TEXT("The material expression '%s' is only supported in the 'Surface' or 'Volume' material domain."), ExpressionName);
		return INDEX_NONE;
	}

	return AddInlinedCodeChunkZeroDeriv(Type, TEXT("GetPrimitiveData(Parameters).%s"), HLSLName);
}

// The compiler can run in a different state and this affects caching of sub expression, Expressions are different (e.g. View.PrevWorldViewOrigin) when using previous frame's values
bool FHLSLMaterialTranslator::IsCurrentlyCompilingForPreviousFrame() const
{
	return bCompilingPreviousFrame;
}

bool FHLSLMaterialTranslator::IsDevelopmentFeatureEnabled(const FName& FeatureName) const
{
	if (FeatureName == NAME_SelectionColor)
	{
		// This is an editor-only feature (see FDefaultMaterialInstance::GetVectorValue).

		// Determine if we're sure the editor will never run using the target shader platform.
		// The list below may not be comprehensive enough, but it definitely includes platforms which won't use selection color for sure.
		const bool bEditorMayUseTargetShaderPlatform = IsPCPlatform(Platform);
		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.CompileShadersForDevelopment"));
		const bool bCompileShadersForDevelopment = (CVar && CVar->GetValueOnAnyThread() != 0);

		return
			// Does the material explicitly forbid development features?
			Material->GetAllowDevelopmentShaderCompile()
			// Can the editor run using the current shader platform?
			&& bEditorMayUseTargetShaderPlatform
			// Are shader development features globally disabled?
			&& bCompileShadersForDevelopment;
	}

	return true;
}

int32 FHLSLMaterialTranslator::SparseVolumeTexture(USparseVolumeTexture* Texture, int32 SubTextureId, int32& TextureReferenceIndex, EMaterialSamplerType SamplerType)
{
	TextureReferenceIndex = Material->GetReferencedTextures().Find(Texture);
	checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->SparseVolumeTexture() without implementing UMaterialExpression::GetReferencedTexture properly"));

	return AddUniformExpression(new FMaterialUniformExpressionTexture(TextureReferenceIndex, SubTextureId, SamplerType), MCT_VolumeTexture, TEXT(""));
}

int32 FHLSLMaterialTranslator::SparseVolumeTextureParameter(FName ParameterName, USparseVolumeTexture* InDefaultTexture, int32 SubTextureId, int32& TextureReferenceIndex, EMaterialSamplerType SamplerType)
{
	USparseVolumeTexture* DefaultTexture = InDefaultTexture;

	// If we're compiling a function, give the function a chance to override the default parameter value
	FMaterialParameterMetadata Meta;
	if (GetParameterOverrideValueForCurrentFunction(EMaterialParameterType::SparseVolumeTexture, ParameterName, Meta))
	{
		DefaultTexture = Meta.Value.SparseVolumeTexture;
	}

	TextureReferenceIndex = Material->GetReferencedTextures().Find(DefaultTexture);
	checkf(TextureReferenceIndex != INDEX_NONE, TEXT("Material expression called Compiler->SparseVolumeTextureParameter() without implementing UMaterialExpression::GetReferencedTexture properly"));

	FMaterialParameterInfo ParameterInfo = GetParameterAssociationInfo();
	ParameterInfo.Name = ParameterName;

	return AddUniformExpression(new FMaterialUniformExpressionTextureParameter(ParameterInfo, TextureReferenceIndex, SubTextureId, SamplerType), MCT_VolumeTexture, TEXT(""));
}

int32 FHLSLMaterialTranslator::SparseVolumeTextureGetVoxelCoord(int32 PackedPhysicalTileCoord, int32 TileSize, int32 CoordPageTable, int32 CoordVolume)
{
	FString SampleCode = FString::Printf(TEXT("SparseVolumeTextureGetVoxelCoord(%s, %s, %s, %s)"),
		*GetParameterCode(PackedPhysicalTileCoord), *GetParameterCode(TileSize), *GetParameterCode(CoordPageTable), *GetParameterCode(CoordVolume));
	return AddCodeChunk(MCT_UInt3, *SampleCode);
}

int32 FHLSLMaterialTranslator::SparseVolumeTextureSample(int32 TextureIndex, int32 CoordinateIndex)
{
	if (TextureIndex == INDEX_NONE || CoordinateIndex == INDEX_NONE)
	{
		return INDEX_NONE;
	}

	EMaterialValueType TextureType = GetParameterType(TextureIndex);

	int32 SamplingCodeIndex = INDEX_NONE;
	FString TextureName;
	if (TextureType == MCT_TextureCube)
	{
		return Errorf(TEXT("FHLSLMaterialTranslator::SparseVolumeTextureSample MCT_TextureCube ERROR."));
	}
	else if (TextureType == MCT_Texture2DArray)
	{
		return Errorf(TEXT("FHLSLMaterialTranslator::SparseVolumeTextureSample MCT_Texture2DArray ERROR."));
	}
	else if (TextureType == MCT_TextureCubeArray)
	{
		return Errorf(TEXT("FHLSLMaterialTranslator::SparseVolumeTextureSample MCT_TextureCubeArray ERROR."));
	}
	else if (TextureType == MCT_VolumeTexture)
	{
		TextureName = CoerceParameter(TextureIndex, MCT_VolumeTexture);		// TODO this hsould be some texture reserved for sub textures


		FMaterialUniformExpression* UniformExpression = GetParameterUniformExpression(TextureIndex);
		if (UniformExpression == nullptr)
		{
			return Errorf(TEXT("Unable to find expression for Sparse Volume Texture."));
		}
		FMaterialUniformExpressionTexture* TextureUniformExpression = UniformExpression->GetTextureUniformExpression();
		if (TextureUniformExpression == nullptr || TextureUniformExpression->GetPageTableLayerIndex() != INDEX_NONE || TextureUniformExpression->GetTextureLayerIndex() == INDEX_NONE)
		{
			// Sparse volume texture do not use PageTableLayerIndex but only TextureLayerIndex.
			return Errorf(TEXT("The provided uniform expression is cannot be identified as a Sparse Volume Texture."));
		}

		int32 VirtualTextureIndex = UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Volume].Find(TextureUniformExpression);
		check(UniformTextureExpressions[(uint32)EMaterialTextureParameterType::Volume].IsValidIndex(VirtualTextureIndex));

		const int32 TextureLayerIndex = TextureUniformExpression->GetTextureLayerIndex();
		check(TextureLayerIndex != INDEX_NONE);
		if (TextureLayerIndex == 0)
		{
			FString SampleCode = FString::Printf(TEXT("%s.Load(int4(int3(%s), 0)).x"), *TextureName, *GetParameterCode(CoordinateIndex));
			AddEstimatedTextureSample();
			// We ignore derivative for now, see IsAnalyticDerivEnabled() and IsDerivativeValid(UvDerivativeStatus)
			SamplingCodeIndex = AddCodeChunk(MCT_UInt1, *SampleCode);
		}
		else if (TextureLayerIndex == 1 || TextureLayerIndex == 2)
		{
			FString SampleCode = FString::Printf(TEXT("%s.Load(int4(int3(%s), 0))"), *TextureName, *GetParameterCode(CoordinateIndex));
			AddEstimatedTextureSample();
			// We ignore derivative for now, see IsAnalyticDerivEnabled() and IsDerivativeValid(UvDerivativeStatus)
			SamplingCodeIndex = AddCodeChunk(MCT_Float4, *SampleCode);
		}
		else
		{
			return Errorf(TEXT("Sparse volume texture cannot be interpreted."));
		}

	}
	else if (TextureType == MCT_TextureExternal)
	{
		return Errorf(TEXT("FHLSLMaterialTranslator::SparseVolumeTextureSample MCT_TextureExternal ERROR."));
	}
	else // MCT_Texture2D
	{
		return Errorf(TEXT("FHLSLMaterialTranslator::SparseVolumeTextureSample MCT_Texture2D ERROR."));
	}

	return SamplingCodeIndex;
}

int32 FHLSLMaterialTranslator::SparseVolumeTextureUniform(int32 TextureIndex, int32 VectorIndex, UE::Shader::EValueType Type)
{
	return AddUniformExpression(new FMaterialUniformExpressionSparseVolumeTextureUniform(TextureIndex, VectorIndex), GetMaterialValueType(Type), TEXT(""));
}

int32 FHLSLMaterialTranslator::SparseVolumeTextureUniformParameter(FName ParameterName, int32 TextureIndex, int32 VectorIndex, UE::Shader::EValueType Type)
{
	FMaterialParameterInfo ParameterInfo = GetParameterAssociationInfo();
	ParameterInfo.Name = ParameterName;
	return AddUniformExpression(new FMaterialUniformExpressionSparseVolumeTextureUniform(ParameterInfo, TextureIndex, VectorIndex), GetMaterialValueType(Type), TEXT(""));
}


#endif // WITH_EDITORONLY_DATA
