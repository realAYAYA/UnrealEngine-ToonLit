// Copyright Epic Games, Inc. All Rights Reserved.
#include "Materials/MaterialHLSLEmitter.h"

#if WITH_EDITOR

#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionVolumetricAdvancedMaterialOutput.h"
#include "Materials/Material.h"
#include "MaterialDomain.h"
#include "MaterialHLSLTree.h"
#include "MaterialCachedHLSLTree.h"
#include "MaterialShared.h"
#include "HLSLTree/HLSLTreeEmit.h"
#include "Containers/LazyPrintf.h"
#include "RenderUtils.h"
#include "DataDrivenShaderPlatformInfo.h"

static bool SharedPixelProperties[CompiledMP_MAX];

static const TCHAR* HLSLTypeString(EMaterialValueType Type)
{
	switch (Type)
	{
	case MCT_Float1:				return TEXT("MaterialFloat");
	case MCT_Float2:				return TEXT("MaterialFloat2");
	case MCT_Float3:				return TEXT("MaterialFloat3");
	case MCT_Float4:				return TEXT("MaterialFloat4");
	case MCT_Float:					return TEXT("MaterialFloat");
	case MCT_Texture2D:				return TEXT("texture2D");
	case MCT_TextureCube:			return TEXT("textureCube");
	case MCT_Texture2DArray:		return TEXT("texture2DArray");
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
	default:						return TEXT("unknown");
	};
}

static FString GenerateMaterialTemplateHLSL(EShaderPlatform ShaderPlatform,
	const FMaterial& Material,
	const UE::HLSLTree::FEmitContext& EmitContext,
	const TCHAR* DeclarationsCode,
	const TCHAR* SharedShaderCode,
	const TCHAR* VertexShaderCode,
	const TCHAR* PixelShaderCodePhase0[2],
	const TCHAR* PixelShaderCodePhase1[2],
	FMaterialCompilationOutput& OutCompilationOutput)
{
	using namespace UE::HLSLTree;

	const Material::FEmitData& EmitMaterialData = EmitContext.FindData<Material::FEmitData>();

	FString MaterialTemplateSource;
	LoadShaderSourceFileChecked(TEXT("/Engine/Private/MaterialTemplate.ush"), ShaderPlatform, MaterialTemplateSource);

	// Find the string index of the '#line' statement in MaterialTemplate.usf
	const int32 LineIndex = MaterialTemplateSource.Find(TEXT("#line"), ESearchCase::CaseSensitive);
	check(LineIndex != INDEX_NONE);

	// Count line endings before the '#line' statement
	int32 MaterialTemplateLineNumber = INDEX_NONE;
	int32 StartPosition = LineIndex + 1;
	do
	{
		MaterialTemplateLineNumber++;
		// Using \n instead of LINE_TERMINATOR as not all of the lines are terminated consistently
		// Subtract one from the last found line ending index to make sure we skip over it
		StartPosition = MaterialTemplateSource.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, StartPosition - 1);
	} while (StartPosition != INDEX_NONE);
	check(MaterialTemplateLineNumber != INDEX_NONE);
	// At this point MaterialTemplateLineNumber is one less than the line number of the '#line' statement
	// For some reason we have to add 2 more to the #line value to get correct error line numbers from D3DXCompileShader
	MaterialTemplateLineNumber += 3;

	FLazyPrintf LazyPrintf(*MaterialTemplateSource);

	uint32 NumVertexTexCoords = 0u;
	uint32 NumPixelTexCoords = 0u;
	for (int32 TexCoordIndex = 0; TexCoordIndex < Material::MaxNumTexCoords; ++TexCoordIndex)
	{
		const Material::EExternalInput TexCoordInput = Material::MakeInputTexCoord(TexCoordIndex);
		if (EmitMaterialData.IsExternalInputUsed(SF_Vertex, TexCoordInput))
		{
			NumVertexTexCoords = TexCoordIndex + 1;
		}
		if (EmitMaterialData.IsExternalInputUsed(SF_Pixel, TexCoordInput))
		{
			NumPixelTexCoords = TexCoordIndex + 1;
		}
	}

	NumVertexTexCoords = FMath::Max(NumVertexTexCoords, NumPixelTexCoords);

	const uint32 NumCustomVectors = (EmitMaterialData.NumInterpolatorComponents + 1) / 2;
	const uint32 NumTexCoordVectors = NumPixelTexCoords + NumCustomVectors;

	OutCompilationOutput.NumUsedUVScalars = NumPixelTexCoords * 2;
	OutCompilationOutput.NumUsedCustomInterpolatorScalars = EmitMaterialData.NumInterpolatorComponents;

	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"), NumVertexTexCoords));
	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"), NumPixelTexCoords));
	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"), NumCustomVectors));
	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"), NumTexCoordVectors));

	FString VertexInterpolatorsOffsetsDefinition;
	LazyPrintf.PushParam(*VertexInterpolatorsOffsetsDefinition);

	FString MaterialAttributesDeclaration;
	FString MaterialAttributesUtilities;
	FString MaterialAttributesDefault;

	const EMaterialShadingModel DefaultShadingModel = Material.GetShadingModels().GetFirstShadingModel();

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
		case MCT_ShadingModel: case MCT_UInt:  case MCT_UInt1:  HLSLType = TEXT("uint"); break;
		case MCT_UInt2:  HLSLType = TEXT("uint2"); break;
		case MCT_UInt3:  HLSLType = TEXT("uint3"); break;
		case MCT_UInt4:  HLSLType = TEXT("uint4"); break;
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

			// Standard value type
			switch (PropertyType)
			{
			case MCT_Float: case MCT_Float1: MaterialAttributesDefault += FString::Printf(TEXT("\tResult.%s = %0.8f;") LINE_TERMINATOR, *PropertyName, DefaultValue.X); break;
			case MCT_Float2: MaterialAttributesDefault += FString::Printf(TEXT("\tResult.%s = MaterialFloat2(%0.8f,%0.8f);") LINE_TERMINATOR, *PropertyName, DefaultValue.X, DefaultValue.Y); break;
			case MCT_Float3: MaterialAttributesDefault += FString::Printf(TEXT("\tResult.%s = MaterialFloat3(%0.8f,%0.8f,%0.8f);") LINE_TERMINATOR, *PropertyName, DefaultValue.X, DefaultValue.Y, DefaultValue.Z); break;
			case MCT_Float4: MaterialAttributesDefault += FString::Printf(TEXT("\tResult.%s = MaterialFloat4(%0.8f,%0.8f,%0.8f,%0.8f);") LINE_TERMINATOR, *PropertyName, DefaultValue.X, DefaultValue.Y, DefaultValue.Z, DefaultValue.W); break;
			case MCT_ShadingModel: MaterialAttributesDefault += FString::Printf(TEXT("\tResult.%s = %d;") LINE_TERMINATOR, *PropertyName, (int32)DefaultShadingModel); break;
			case MCT_Strata: MaterialAttributesDefault += FString::Printf(TEXT("\tResult.%s = GetInitialisedStrataData();") LINE_TERMINATOR, *PropertyName); break; // TODO
			default: checkNoEntry(); break;
			}
		}
	}

	//LazyPrintf.PushParam(*MaterialAttributesDeclaration);
	//LazyPrintf.PushParam(*MaterialAttributesUtilities);

	LazyPrintf.PushParam(DeclarationsCode);
	LazyPrintf.PushParam(TEXT(""));

	// Stores the shared shader results member declarations
	// PixelMembersDeclaration should be the same for all variations, but might change in the future. There are cases where work is shared
	// between the pixel and vertex shader, but with Nanite all work has to be moved into the pixel shader, which means we will want
	// different inputs. But for now, we are keeping them the same.
	FString PixelMembersDeclaration;

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

		PixelMembersDeclaration += FString::Printf(TEXT("\t%s %s;\n"), HLSLTypeString(Type), *PropertyName);
	}

	//for (int32 Index = 0; Index < CompiledPDV_MAX; Index++)
	//{
	//	GetSharedInputsMaterialCode(PixelMembersDeclaration[Index], NormalAssignment[Index], PixelMembersSetupAndAssignments[Index], (ECompiledPartialDerivativeVariation)Index);
	//}

	LazyPrintf.PushParam(*PixelMembersDeclaration);

	LazyPrintf.PushParam(SharedShaderCode);

	// Anything used bye the GenerationFunctionCode() like WorldPositionOffset shouldn't be using texures, right?
	// Let those use the standard finite differences textures, since they should be the same. If we actually want
	// those to handle texture reads properly, we'll have to make extra versions.
	ECompiledPartialDerivativeVariation BaseDerivativeVariation = CompiledPDV_FiniteDifferences;

	//if (bCompileForComputeShader)
	//{
	//	LazyPrintf.PushParam(*GenerateFunctionCode(CompiledMP_EmissiveColorCS, BaseDerivativeVariation));
	//}
	//else
	{
		LazyPrintf.PushParam(TEXT("return 0"));
	}

	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material.GetTranslucencyDirectionalLightingIntensity()));

	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material.GetTranslucentShadowDensityScale()));
	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material.GetTranslucentSelfShadowDensityScale()));
	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material.GetTranslucentSelfShadowSecondDensityScale()));
	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material.GetTranslucentSelfShadowSecondOpacity()));
	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material.GetTranslucentBackscatteringExponent()));

	{
		FLinearColor Extinction = Material.GetTranslucentMultipleScatteringExtinction();

		LazyPrintf.PushParam(*FString::Printf(TEXT("return MaterialFloat3(%.5f, %.5f, %.5f)"), Extinction.R, Extinction.G, Extinction.B));
	}

	LazyPrintf.PushParam(*FString::Printf(TEXT("return %.5f"), Material.GetOpacityMaskClipValue()));

	LazyPrintf.PushParam(TEXT("return Parameters.MaterialAttributes.WorldPositionOffset"));
	LazyPrintf.PushParam(TEXT("return Parameters.MaterialAttributes.PrevWorldPositionOffset"));
	// CustomData0/1 are named ClearCoat/ClearCoatRoughness
	LazyPrintf.PushParam(TEXT("return Parameters.MaterialAttributes.ClearCoat"));
	LazyPrintf.PushParam(TEXT("return Parameters.MaterialAttributes.ClearCoatRoughness"));

	// Print custom texture coordinate assignments, should be fine with regular derivatives
	{
		FString CustomUVAssignments;
		for (uint32 CustomUVIndex = 0; CustomUVIndex < NumPixelTexCoords; CustomUVIndex++)
		{
			const FString AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName((EMaterialProperty)(MP_CustomizedUVs0 + CustomUVIndex));
			CustomUVAssignments += FString::Printf(TEXT("\tOutTexCoords[%u] = Parameters.MaterialAttributes.%s;") LINE_TERMINATOR, CustomUVIndex, *AttributeName);
		}
		LazyPrintf.PushParam(*CustomUVAssignments);
	}

	// Print custom vertex shader interpolator assignments
	{
		FString CustomInterpolatorAssignments;
		for (uint32 InterpolatorIndex = 0; InterpolatorIndex < NumCustomVectors; ++InterpolatorIndex)
		{
			CustomInterpolatorAssignments += FString::Printf(TEXT("\tOutTexCoords[NUM_MATERIAL_TEXCOORDS + %u] = Parameters.CustomInterpolators[%u];") LINE_TERMINATOR, InterpolatorIndex, InterpolatorIndex);
		}
		LazyPrintf.PushParam(*CustomInterpolatorAssignments);
	}

	//LazyPrintf.PushParam(*MaterialAttributesDefault);

	//if (bEnableExecutionFlow)
	{
		FString EvaluateMaterialDeclaration;

		EvaluateMaterialDeclaration += TEXT("void EvaluateVertexMaterialAttributesInternal(in out FMaterialVertexParameters Parameters)" LINE_TERMINATOR);
		EvaluateMaterialDeclaration += TEXT("{" LINE_TERMINATOR);
		EvaluateMaterialDeclaration += VertexShaderCode;
		EvaluateMaterialDeclaration += TEXT("}" LINE_TERMINATOR);

		EvaluateMaterialDeclaration += TEXT("void EvaluateVertexMaterialAttributes(in out FMaterialVertexParameters Parameters)" LINE_TERMINATOR);
		EvaluateMaterialDeclaration += TEXT("{" LINE_TERMINATOR);
		EvaluateMaterialDeclaration += TEXT("    EvaluateVertexMaterialAttributesInternal(Parameters);" LINE_TERMINATOR);
		for (uint32 TexCoordIndex = 0; TexCoordIndex < NumPixelTexCoords; ++TexCoordIndex)
		{
			const Material::EExternalInput TexCoordInput = Material::MakeInputTexCoord(TexCoordIndex);

			if (EmitMaterialData.IsExternalInputUsed(SF_Pixel, TexCoordInput) && !EmitMaterialData.IsExternalInputUsed(SF_Vertex, TexCoordInput))
			{
				const FString AttributeName = FMaterialAttributeDefinitionMap::GetAttributeName((EMaterialProperty)(MP_CustomizedUVs0 + TexCoordIndex));
				EvaluateMaterialDeclaration += FString::Printf(TEXT("    Parameters.MaterialAttributes.%s = Parameters.TexCoords[%d];") LINE_TERMINATOR, *AttributeName, TexCoordIndex);
			}
		}
		EvaluateMaterialDeclaration += TEXT("}" LINE_TERMINATOR);

		FString EvaluateMaterialAttributesPhase0[2];
		FString EvaluateMaterialAttributesPhase1[2];
		for (int32 DerivativeIndex = 0; DerivativeIndex < 2; ++DerivativeIndex)
		{
			const TCHAR* DerivativeName = (DerivativeIndex == 0) ? TEXT("HWDerivative") : TEXT("AnalyticDerivative");

			EvaluateMaterialAttributesPhase0[DerivativeIndex] =  FString::Printf(TEXT("    FMaterialAttributes MaterialAttributesPhase0;" LINE_TERMINATOR), DerivativeName);
			EvaluateMaterialAttributesPhase0[DerivativeIndex] += FString::Printf(TEXT("    EvaluatePixelMaterialAttributesPhase0_%s(Parameters, MaterialAttributesPhase0);" LINE_TERMINATOR), DerivativeName);

			EvaluateMaterialDeclaration += FString::Printf(TEXT("void EvaluatePixelMaterialAttributesPhase0_%s(in out FMaterialPixelParameters Parameters, out FMaterialAttributes OutResult)" LINE_TERMINATOR), DerivativeName);
			EvaluateMaterialDeclaration += TEXT("{" LINE_TERMINATOR);
			EvaluateMaterialDeclaration += PixelShaderCodePhase0[DerivativeIndex];
			EvaluateMaterialDeclaration += TEXT("}" LINE_TERMINATOR);

			if (PixelShaderCodePhase1[DerivativeIndex])
			{
				EvaluateMaterialDeclaration += FString::Printf(TEXT("void EvaluatePixelMaterialAttributesPhase1_%s(in out FMaterialPixelParameters Parameters, out FMaterialAttributes OutResult)" LINE_TERMINATOR), DerivativeName);
				EvaluateMaterialDeclaration += TEXT("{" LINE_TERMINATOR);
				EvaluateMaterialDeclaration += PixelShaderCodePhase1[DerivativeIndex];
				EvaluateMaterialDeclaration += TEXT("}" LINE_TERMINATOR);

				EvaluateMaterialAttributesPhase1[DerivativeIndex] =  FString::Printf(TEXT("    FMaterialAttributes MaterialAttributesPhase1;" LINE_TERMINATOR), DerivativeName);
				EvaluateMaterialAttributesPhase1[DerivativeIndex] += FString::Printf(TEXT("    EvaluatePixelMaterialAttributesPhase1_%s(Parameters, MaterialAttributesPhase1);" LINE_TERMINATOR), DerivativeName);
				EvaluateMaterialAttributesPhase1[DerivativeIndex] += TEXT("    Parameters.MaterialAttributes = MaterialAttributesPhase1;" LINE_TERMINATOR);
			}
			else
			{
				EvaluateMaterialAttributesPhase0[DerivativeIndex] += TEXT("    Parameters.MaterialAttributes = MaterialAttributesPhase0;" LINE_TERMINATOR);
			}
		}

		LazyPrintf.PushParam(*EvaluateMaterialDeclaration);

		for (int32 DerivativeIndex = 0; DerivativeIndex < 2; ++DerivativeIndex)
		{
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

				// 'Normal' is always evaluated in Phase0
				// If there is a Phase1, it will contain all other attributes
				const int32 EvaluatePhase = (Property == MP_Normal || !PixelShaderCodePhase1[DerivativeIndex]) ? 0 : 1;
				FString& EvaluateMaterialAttributesCode = (EvaluatePhase == 0) ? EvaluateMaterialAttributesPhase0[DerivativeIndex] : EvaluateMaterialAttributesPhase1[DerivativeIndex];

				if (PropertyIndex == MP_FrontMaterial)
				{
					EvaluateMaterialAttributesCode += FString::Printf("    PixelMaterialInputs.%s = GetInitialisedStrataData();" LINE_TERMINATOR, *PropertyName);
				}
				else if (PropertyIndex == MP_SubsurfaceColor)
				{
					// TODO - properly handle subsurface profile
					EvaluateMaterialAttributesCode += FString::Printf("    PixelMaterialInputs.Subsurface = float4(MaterialAttributesPhase%d.%s, 0.0f);" LINE_TERMINATOR, EvaluatePhase, *PropertyName);
				}
				else
				{
					EvaluateMaterialAttributesCode += FString::Printf("    PixelMaterialInputs.%s = MaterialAttributesPhase%d.%s;" LINE_TERMINATOR, *PropertyName, EvaluatePhase, *PropertyName);
				}
			}

			LazyPrintf.PushParam(TEXT(""));
			LazyPrintf.PushParam(*EvaluateMaterialAttributesPhase0[DerivativeIndex]);
			LazyPrintf.PushParam(*EvaluateMaterialAttributesPhase1[DerivativeIndex]);
		}
	}

	LazyPrintf.PushParam(*FString::Printf(TEXT("%u"), MaterialTemplateLineNumber));

	return LazyPrintf.GetResultString();
}

static void GetMaterialEnvironment(EShaderPlatform InPlatform,
	const FMaterial& InMaterial,
	const UE::HLSLTree::FEmitContext& EmitContext,
	const FMaterialCompilationOutput& MaterialCompilationOutput,
	FShaderCompilerEnvironment& OutEnvironment)
{
	using namespace UE::HLSLTree;

	const Material::FEmitData& EmitMaterialData = EmitContext.FindData<Material::FEmitData>();

	bool bMaterialRequestsDualSourceBlending = false;

	OutEnvironment.SetDefine(TEXT("ENABLE_NEW_HLSL_GENERATOR"), 1);

	const bool bNeedsParticlePosition = EmitMaterialData.IsExternalInputUsed(Material::EExternalInput::ParticleTranslatedWorldPosition) ||
		EmitMaterialData.IsExternalInputUsed(Material::EExternalInput::ParticleRadius);

	if (bNeedsParticlePosition)// || Material->ShouldGenerateSphericalParticleNormals() || bUsesSphericalParticleOpacity)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_POSITION"), 1);
	}

	if (false)//bNeedsParticleVelocity || Material->IsUsedWithNiagaraMeshParticles())
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_VELOCITY"), 1);
	}

	uint32 DynamicParticleParameterMask = 0u;
	if (DynamicParticleParameterMask)
	{
		OutEnvironment.SetDefine(TEXT("USE_DYNAMIC_PARAMETERS"), 1);
		OutEnvironment.SetDefine(TEXT("DYNAMIC_PARAMETERS_MASK"), DynamicParticleParameterMask);
	}

	if (false)//bNeedsParticleTime)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_TIME"), 1);
	}

	if (false)//bUsesParticleMotionBlur)
	{
		OutEnvironment.SetDefine(TEXT("USES_PARTICLE_MOTION_BLUR"), 1);
	}

	if (false)//bNeedsParticleRandom)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_RANDOM"), 1);
	}

	if (false)//bUsesSphericalParticleOpacity)
	{
		OutEnvironment.SetDefine(TEXT("SPHERICAL_PARTICLE_OPACITY"), TEXT("1"));
	}

	if (false)//bUsesParticleSubUVs)
	{
		OutEnvironment.SetDefine(TEXT("USE_PARTICLE_SUBUVS"), TEXT("1"));
	}

	if (EmitMaterialData.ExternalInputMask[SF_Pixel][(int32)Material::EExternalInput::LightmapTexCoord])
	{
		OutEnvironment.SetDefine(TEXT("LIGHTMAP_UV_ACCESS"), TEXT("1"));
	}

	if (false)//bUsesAOMaterialMask)
	{
		OutEnvironment.SetDefine(TEXT("USES_AO_MATERIAL_MASK"), TEXT("1"));
	}

	if (false)//bUsesSpeedTree)
	{
		OutEnvironment.SetDefine(TEXT("USES_SPEEDTREE"), TEXT("1"));
	}

	if (false)//bNeedsWorldPositionExcludingShaderOffsets)
	{
		OutEnvironment.SetDefine(TEXT("NEEDS_WORLD_POSITION_EXCLUDING_SHADER_OFFSETS"), TEXT("1"));
	}

	if (false)//bNeedsParticleSize)
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

	OutEnvironment.SetDefine(TEXT("USES_PER_INSTANCE_CUSTOM_DATA"), MaterialCompilationOutput.bUsesPerInstanceCustomData && InMaterial.IsUsedWithInstancedStaticMeshes());
	OutEnvironment.SetDefine(TEXT("USES_PER_INSTANCE_RANDOM"), MaterialCompilationOutput.bUsesPerInstanceRandom && InMaterial.IsUsedWithInstancedStaticMeshes());
	OutEnvironment.SetDefine(TEXT("USES_VERTEX_INTERPOLATOR"), MaterialCompilationOutput.bUsesVertexInterpolator);

	const bool bUsesVertexColor = EmitMaterialData.IsExternalInputUsed(SF_Vertex, Material::EExternalInput::VertexColor) ||
		EmitMaterialData.IsExternalInputUsed(SF_Vertex, Material::EExternalInput::VertexColor_Ddx) ||
		EmitMaterialData.IsExternalInputUsed(SF_Vertex, Material::EExternalInput::VertexColor_Ddy);

	const bool bUsesParticleColor = EmitMaterialData.IsExternalInputUsed(SF_Pixel, Material::EExternalInput::ParticleColor);


	// @todo MetalMRT: Remove this hack and implement proper atmospheric-fog solution for Metal MRT...
	OutEnvironment.SetDefine(TEXT("MATERIAL_ATMOSPHERIC_FOG"), false);// !IsMetalMRTPlatform(InPlatform) ? bUsesAtmosphericFog : 0);
	OutEnvironment.SetDefine(TEXT("MATERIAL_SKY_ATMOSPHERE"), false);// bUsesSkyAtmosphere);
	OutEnvironment.SetDefine(TEXT("INTERPOLATE_VERTEX_COLOR"), bUsesVertexColor);
	OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_COLOR"), bUsesParticleColor);
	OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_LOCAL_TO_WORLD"), false);// bUsesParticleLocalToWorld);
	OutEnvironment.SetDefine(TEXT("NEEDS_PARTICLE_WORLD_TO_LOCAL"), false);// bUsesParticleWorldToLocal);
	OutEnvironment.SetDefine(TEXT("USES_TRANSFORM_VECTOR"), false);// bUsesTransformVector);
	OutEnvironment.SetDefine(TEXT("WANT_PIXEL_DEPTH_OFFSET"), false);// bUsesPixelDepthOffset);
	OutEnvironment.SetDefine(TEXT("USES_WORLD_POSITION_OFFSET"), false);// bUsesWorldPositionOffset);
	OutEnvironment.SetDefine(TEXT("USES_EMISSIVE_COLOR"), false);// bUsesEmissiveColor);
	// Distortion uses tangent space transform 
	OutEnvironment.SetDefine(TEXT("USES_DISTORTION"), InMaterial.IsDistorted());

	OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_FOGGING"), InMaterial.ShouldApplyFogging());
	OutEnvironment.SetDefine(TEXT("MATERIAL_ENABLE_TRANSLUCENCY_CLOUD_FOGGING"), InMaterial.ShouldApplyCloudFogging());
	OutEnvironment.SetDefine(TEXT("MATERIAL_IS_SKY"), InMaterial.IsSky());
	OutEnvironment.SetDefine(TEXT("MATERIAL_COMPUTE_FOG_PER_PIXEL"), InMaterial.ComputeFogPerPixel());
	OutEnvironment.SetDefine(TEXT("MATERIAL_FULLY_ROUGH"), false);// bIsFullyRough || InMaterial.IsFullyRough());
	OutEnvironment.SetDefine(TEXT("MATERIAL_USES_ANISOTROPY"), false);// bUsesAnisotropy);

	// Count the number of VTStacks (each stack will allocate a feedback slot)
	OutEnvironment.SetDefine(TEXT("NUM_VIRTUALTEXTURE_SAMPLES"), EmitMaterialData.VTStacks.Num());

	// Setup defines to map each VT stack to either 1 or 2 page table textures, depending on how many layers it uses
	for (int i = 0; i < EmitMaterialData.VTStacks.Num(); ++i)
	{
		const FMaterialVirtualTextureStack& Stack = MaterialCompilationOutput.UniformExpressionSet.GetVTStack(i);

		TStringBuilder<256> PageTableName;
		PageTableName.Appendf(TEXT("VIRTUALTEXTURE_PAGETABLE_%d"), i);

		TStringBuilder<1024> PageTableValue;
		PageTableValue.Appendf(TEXT("Material.VirtualTexturePageTable0_%d"), i);

		if (Stack.GetNumLayers() > 4u)
		{
			PageTableValue.Appendf(TEXT(", Material.VirtualTexturePageTable1_%d"), i);
		}
		if (EmitMaterialData.VTStacks[i].bAdaptive)
		{
			PageTableValue.Appendf(TEXT(", Material.VirtualTexturePageTableIndirection_%d"), i);
		}
		OutEnvironment.SetDefine(PageTableName.ToString(), PageTableValue.ToString());
	}

	/*for (int32 CollectionIndex = 0; CollectionIndex < ParameterCollections.Num(); CollectionIndex++)
	{
		// Add uniform buffer declarations for any parameter collections referenced
		const FString CollectionName = FString::Printf(TEXT("MaterialCollection%u"), CollectionIndex);
		// This can potentially become an issue for MaterialCollection Uniform Buffers if they ever get non-numeric resources (eg Textures), as
		// OutEnvironment.ResourceTableMap has a map by name, and the N ParameterCollection Uniform Buffers ALL are names "MaterialCollection"
		// (and the hlsl cbuffers are named MaterialCollection0, etc, so the names don't match the layout)
		FShaderUniformBufferParameter::ModifyCompilationEnvironment(*CollectionName, ParameterCollections[CollectionIndex]->GetUniformBufferStruct(), InPlatform, OutEnvironment);
	}*/
	OutEnvironment.SetDefine(TEXT("IS_MATERIAL_SHADER"), true);

	// Set all the shading models for this material here 
	FMaterialShadingModelField ShadingModels = InMaterial.GetShadingModels();

	// If the material gets its shading model from the material expressions, then we use the result from the compilation (assuming it's valid).
	// This result will potentially be tighter than what GetShadingModels() returns, because it only picks up the shading models from the expressions that get compiled for a specific feature level and quality level
	// For example, the material might have shading models behind static switches. GetShadingModels() will return both the true and the false paths from that switch, whereas the shading model field from the compilation will only contain the actual shading model selected 
	if (InMaterial.IsShadingModelFromMaterialExpression() && EmitMaterialData.ShadingModelsFromCompilation.IsValid())
	{
		// Shading models fetched from the compilation of the expression graph
		ShadingModels = EmitMaterialData.ShadingModelsFromCompilation;
	}
	ensure(ShadingModels.IsValid());

	if (ShadingModels.IsLit())
	{
		int32 NumSetMaterials = 0;
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

		if (ShadingModels.HasShadingModel(MSM_SingleLayerWater) && FDataDrivenShaderPlatformInfo::GetRequiresDisableForwardLocalLights(InPlatform))
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

		if (NumSetMaterials == 1)
		{
			OutEnvironment.SetDefine(TEXT("MATERIAL_SINGLE_SHADINGMODEL"), TEXT("1"));
		}

		ensure(NumSetMaterials != 0);
		if (NumSetMaterials == 0)
		{
			// Should not really end up here
			UE_LOG(LogMaterial, Warning, TEXT("Unknown material shading model(s). Setting to MSM_DefaultLit"));
			OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_DEFAULT_LIT"), TEXT("1"));
		}
	}
	else
	{
		// Unlit shading model can only exist by itself
		OutEnvironment.SetDefine(TEXT("MATERIAL_SINGLE_SHADINGMODEL"), TEXT("1"));
		OutEnvironment.SetDefine(TEXT("MATERIAL_SHADINGMODEL_UNLIT"), TEXT("1"));
	}

	if (InMaterial.GetMaterialDomain() == MD_Volume) // && Material->HasN)
	{
		TArray<const UMaterialExpressionVolumetricAdvancedMaterialOutput*> VolumetricAdvancedExpressions;
		InMaterial.GetMaterialInterface()->GetMaterial()->GetAllExpressionsOfType(VolumetricAdvancedExpressions);
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
				InMaterial.HasAmbientOcclusionConnected() ? TEXT("1") : TEXT("0"));

			OutEnvironment.SetDefine(TEXT("MATERIAL_VOLUMETRIC_ADVANCED_GROUND_CONTRIBUTION"),
				VolumetricAdvancedNode->bGroundContribution ? TEXT("1") : TEXT("0"));
		}
	}

	OutEnvironment.SetDefine(TEXT("MATERIAL_IS_STRATA"), false);// bMaterialIsStrata ? TEXT("1") : TEXT("0"));

	// STRATA_TODO do not request DualSourceBlending if gray scale transmittance is selected.
	// bMaterialRequestsDualSourceBlending this base on limited set of blend mode: Opaque, Masked, TransmittanceCoverage, TransmittanceColored;
	//bMaterialRequestsDualSourceBlending |= bMaterialIsStrata;

	// if duals source blending (colored transmittance) is not supported on a platform, it will fall back to standard alpha blending (grey scale transmittance)
	OutEnvironment.SetDefine(TEXT("DUAL_SOURCE_COLOR_BLENDING_ENABLED"), false);// bMaterialRequestsDualSourceBlending&& Material->IsDualBlendingEnabled(Platform) ? TEXT("1") : TEXT("0"));

	// Translate() is called before getting here so we can create related define
	/*if (StrataMaterialAnalysis.RequestedBSDFCount > 0)
	{
		OutEnvironment.SetDefine(TEXT("STRATA_CLAMPED_LAYER_COUNT"), StrataMaterialAnalysis.ClampedLayerCount);
	}*/

	OutEnvironment.SetDefine(TEXT("TEXTURE_SAMPLE_DEBUG"), false);// IsDebugTextureSampleEnabled() ? TEXT("1") : TEXT("0"));
}

class FStringBuilderMemstack : public TStringBuilderBase<TCHAR>
{
public:
	FStringBuilderMemstack(FMemStackBase& Allocator, int32 InSize) : TStringBuilderBase<TCHAR>((TCHAR*)Allocator.Alloc(sizeof(TCHAR)* InSize, alignof(TCHAR)), InSize) {}
};

class FMaterialHLSLErrorHandler : public UE::HLSLTree::FErrorHandlerInterface
{
public:
	explicit FMaterialHLSLErrorHandler(FMaterial& InOutMaterial)
		: Material(&InOutMaterial)
	{
		Material->CompileErrors.Reset();
		Material->ErrorExpressions.Reset();
	}

	virtual void AddErrorInternal(TConstArrayView<UObject*> InOwners, FStringView InError) override
	{
		for (UObject* Owner : InOwners)
		{
			UMaterialExpression* MaterialExpressionOwner = Cast<UMaterialExpression>(Owner);
			UMaterialExpression* ExpressionToError = nullptr;
			TStringBuilder<1024> FormattedError;

			if (MaterialExpressionOwner)
			{
				if (MaterialExpressionOwner->GetClass() != UMaterialExpressionMaterialFunctionCall::StaticClass()
					&& MaterialExpressionOwner->GetClass() != UMaterialExpressionFunctionInput::StaticClass()
					&& MaterialExpressionOwner->GetClass() != UMaterialExpressionFunctionOutput::StaticClass())
				{
					// Add the expression currently being compiled to ErrorExpressions so we can draw it differently
					ExpressionToError = MaterialExpressionOwner;

					const int32 ChopCount = FCString::Strlen(TEXT("MaterialExpression"));
					const FString ErrorClassName = MaterialExpressionOwner->GetClass()->GetName();

					// Add the node type to the error message
					FormattedError.Appendf(TEXT("(Node %s) "), *ErrorClassName.Right(ErrorClassName.Len() - ChopCount));
				}
			}

			FormattedError.Append(InError);
			const FString Error(FormattedError.ToView());

			// Standard error handling, immediately append one-off errors and signal failure
			Material->CompileErrors.AddUnique(Error);

			if (ExpressionToError)
			{
				Material->ErrorExpressions.Add(ExpressionToError);
				ExpressionToError->LastErrorText = Error;
			}
		}
	}

private:
	FMaterial* Material;
};

bool MaterialEmitHLSL(const FMaterialCompileTargetParameters& InCompilerTarget,
	const FStaticParameterSet& InStaticParameters,
	FMaterial& InOutMaterial,
	FMaterialCompilationOutput& OutCompilationOutput,
	TRefCountPtr<FSharedShaderCompilerEnvironment>& OutMaterialEnvironment)
{
	using namespace UE::HLSLTree;
	using namespace UE::Shader;

	const TArray<FGuid>& OrderedVisibleAttributes = FMaterialAttributeDefinitionMap::GetOrderedVisibleAttributeList();
	FMemory::Memzero(SharedPixelProperties);
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

	bool bUsesWorldPositionOffset = false;
	bool bUsesPixelDepthOffset = false;

	const FTargetParameters TargetParameters(InCompilerTarget.ShaderPlatform, InCompilerTarget.FeatureLevel, InCompilerTarget.TargetPlatform);
	const FMaterialCachedHLSLTree* CachedTree = InOutMaterial.GetCachedHLSLTree();
	if (!CachedTree)
	{
		ensure(false);
		return false;
	}

	FMaterialHLSLErrorHandler ErrorHandler(InOutMaterial);
	FMemStackBase Allocator;
	FEmitContext EmitContext(Allocator, TargetParameters, ErrorHandler, CachedTree->GetTypeRegistry());
	EmitContext.Material = &InOutMaterial;
	EmitContext.MaterialCompilationOutput = &OutCompilationOutput;

	Material::FEmitData& EmitMaterialData = EmitContext.AcquireData<Material::FEmitData>();
	EmitMaterialData.VTPageTableResultType = CachedTree->GetVTPageTableResultType();
	EmitMaterialData.StaticParameters = &InStaticParameters;

	const FStructField* NormalField = CachedTree->GetMaterialAttributesType()->FindFieldByName(*FMaterialAttributeDefinitionMap::GetAttributeName(MP_Normal));
	check(NormalField);

	// Prepare pixel shader code
	FStringBuilderBase* PixelCodePhase0[2] = { nullptr };
	FStringBuilderBase* PixelCodePhase1[2] = { nullptr };

	for(int32 DerivativeIndex = 0; DerivativeIndex < 2; ++DerivativeIndex)
	{
		EmitContext.ShaderFrequency = SF_Pixel;
		EmitContext.bUseAnalyticDerivatives = (DerivativeIndex == 1);
		EmitContext.bMarkLiveValues = false;
		FEmitScope* EmitResultScope = EmitContext.PrepareScope(CachedTree->GetResultScope());

		// Prepare all fields *except* normal
		FRequestedType RequestedPixelAttributesType(CachedTree->GetMaterialAttributesType(), false);
		CachedTree->SetRequestedFields(SF_Pixel, RequestedPixelAttributesType);
		RequestedPixelAttributesType.SetFieldRequested(NormalField, false);

		const FPreparedType PixelResultType0 = EmitContext.PrepareExpression(CachedTree->GetResultExpression(), *EmitResultScope, RequestedPixelAttributesType);
		if (PixelResultType0.IsVoid())
		{
			return false;
		}

		EmitContext.bMarkLiveValues = true;
		EmitContext.PrepareExpression(CachedTree->GetResultExpression(), *EmitResultScope, RequestedPixelAttributesType);
		EmitContext.bMarkLiveValues = false;

		if (CachedTree->IsAttributeUsed(EmitContext, *EmitResultScope, PixelResultType0, MP_PixelDepthOffset))
		{
			bUsesPixelDepthOffset = true;
		}

		const bool bUseNormal = EmitMaterialData.IsExternalInputUsed(SF_Pixel, Material::EExternalInput::WorldNormal);
		const bool bUseReflection = EmitMaterialData.IsExternalInputUsed(SF_Pixel, Material::EExternalInput::WorldReflection);
		if (!bUseNormal && !bUseReflection)
		{
			// No access to material normal, can execute everything in phase0
			RequestedPixelAttributesType.SetFieldRequested(NormalField);
			const FPreparedType PixelResultType1 = EmitContext.PrepareExpression(CachedTree->GetResultExpression(), *EmitResultScope, RequestedPixelAttributesType);
			if (PixelResultType1.IsVoid())
			{
				return false;
			}

			EmitContext.bMarkLiveValues = true;
			EmitContext.PrepareExpression(CachedTree->GetResultExpression(), *EmitResultScope, RequestedPixelAttributesType);

			FEmitShaderExpression* EmitResultExpression = CachedTree->GetResultExpression()->GetValueShader(EmitContext, *EmitResultScope, RequestedPixelAttributesType, CachedTree->GetMaterialAttributesType());
			EmitContext.EmitStatement(*EmitResultScope, TEXT("OutResult = %;"), EmitResultExpression);

			PixelCodePhase0[DerivativeIndex] = new(Allocator) FStringBuilderMemstack(Allocator, 128 * 1024);
			CachedTree->GetTree().EmitShader(EmitContext, *PixelCodePhase0[DerivativeIndex]);
		}
		else
		{
			// Execute everything *except* normal in phase1
			FEmitShaderExpression* EmitResultExpression1 = CachedTree->GetResultExpression()->GetValueShader(EmitContext, *EmitResultScope, RequestedPixelAttributesType, CachedTree->GetMaterialAttributesType());
			EmitContext.EmitStatement(*EmitResultScope, TEXT("OutResult = %;"), EmitResultExpression1);

			PixelCodePhase1[DerivativeIndex] = new(Allocator) FStringBuilderMemstack(Allocator, 128 * 1024);
			CachedTree->GetTree().EmitShader(EmitContext, *PixelCodePhase1[DerivativeIndex]);

			EmitResultScope = EmitContext.PrepareScope(CachedTree->GetResultScope());

			// Prepare code for just the normal
			FRequestedType RequestedMaterialNormal(CachedTree->GetMaterialAttributesType(), false);
			RequestedMaterialNormal.SetFieldRequested(NormalField);
			const FPreparedType PixelResultType1 = EmitContext.PrepareExpression(CachedTree->GetResultExpression(), *EmitResultScope, RequestedMaterialNormal);
			if (PixelResultType1.IsVoid())
			{
				return false;
			}

			EmitContext.bMarkLiveValues = true;
			EmitContext.PrepareExpression(CachedTree->GetResultExpression(), *EmitResultScope, RequestedMaterialNormal);
			
			FEmitShaderExpression* EmitResultExpression0 = CachedTree->GetResultExpression()->GetValueShader(EmitContext, *EmitResultScope, RequestedMaterialNormal, CachedTree->GetMaterialAttributesType());
			EmitContext.EmitStatement(*EmitResultScope, TEXT("OutResult = %;"), EmitResultExpression0);

			// Execute the normal in phase0
			PixelCodePhase0[DerivativeIndex] = new(Allocator) FStringBuilderMemstack(Allocator, 128 * 1024);
			CachedTree->GetTree().EmitShader(EmitContext, *PixelCodePhase0[DerivativeIndex]);
		}
	}

	// Prepare vertex shader code
	FStringBuilderMemstack VertexCode(Allocator, 128 * 1024);
	{
		FRequestedType RequestedVertexAttributesType(CachedTree->GetMaterialAttributesType(), false);
		CachedTree->SetRequestedFields(SF_Vertex, RequestedVertexAttributesType);
		RequestedVertexAttributesType.SetFieldRequested(CachedTree->GetMaterialAttributesType()->FindFieldByName(TEXT("PrevWorldPositionOffset")));

		EmitContext.ShaderFrequency = SF_Vertex;
		EmitContext.bUseAnalyticDerivatives = false;
		EmitContext.bMarkLiveValues = false;
		FEmitScope* EmitResultScope = EmitContext.PrepareScope(CachedTree->GetResultScope());

		EmitMaterialData.PrepareInterpolators(EmitContext, *EmitResultScope);
		const FPreparedType VertexResultType = EmitContext.PrepareExpression(CachedTree->GetResultExpression(), *EmitResultScope, RequestedVertexAttributesType);
		if (VertexResultType.IsVoid())
		{
			return false;
		}

		EmitContext.bMarkLiveValues = true;
		EmitMaterialData.PrepareInterpolators(EmitContext, *EmitResultScope);
		EmitContext.PrepareExpression(CachedTree->GetResultExpression(), *EmitResultScope, RequestedVertexAttributesType);

		FEmitShaderExpression* EmitResultExpression = CachedTree->GetResultExpression()->GetValueShader(EmitContext, *EmitResultScope, RequestedVertexAttributesType, CachedTree->GetMaterialAttributesType());
		EmitContext.EmitStatement(*EmitResultScope, TEXT("Parameters.MaterialAttributes = %;"), EmitResultExpression);
		EmitMaterialData.EmitInterpolatorStatements(EmitContext, *EmitResultScope);

		bUsesWorldPositionOffset = CachedTree->IsAttributeUsed(EmitContext, *EmitResultScope, VertexResultType, MP_WorldPositionOffset);

		CachedTree->GetTree().EmitShader(EmitContext, VertexCode);
	}

	if (EmitContext.NumErrors > 0)
	{
		return false;
	}

	OutCompilationOutput.bModifiesMeshPosition = bUsesPixelDepthOffset || bUsesWorldPositionOffset;
	OutCompilationOutput.bUsesWorldPositionOffset = bUsesWorldPositionOffset;
	OutCompilationOutput.bUsesPixelDepthOffset = bUsesPixelDepthOffset;
	OutCompilationOutput.bUsesEyeAdaptation = EmitMaterialData.IsExternalInputUsed(SF_Pixel, Material::EExternalInput::EyeAdaptation);

	FStringBuilderMemstack Declarations(Allocator, 32 * 1024);
	CachedTree->GetTypeRegistry().EmitDeclarationsCode(Declarations);

	FStringBuilderMemstack SharedCode(Allocator, 32 * 1024);
	CachedTree->EmitSharedCode(SharedCode);
	EmitContext.EmitDeclarationsCode(SharedCode);
	EmitMaterialData.EmitInterpolatorShader(EmitContext, SharedCode);

	FString MaterialTemplateSource;
	{
		const TCHAR* InputPixelCodePhase0[2] =
		{
			PixelCodePhase0[0]->ToString(),
			PixelCodePhase0[1]->ToString(),
		};
		const TCHAR* InputPixelCodePhase1[2] =
		{
			PixelCodePhase1[0] ? PixelCodePhase1[0]->ToString() : nullptr,
			PixelCodePhase1[1] ? PixelCodePhase1[1]->ToString() : nullptr,
		};
		MaterialTemplateSource = GenerateMaterialTemplateHLSL(InCompilerTarget.ShaderPlatform,
			InOutMaterial,
			EmitContext,
			Declarations.ToString(),
			SharedCode.ToString(),
			VertexCode.ToString(),
			InputPixelCodePhase0,
			InputPixelCodePhase1,
			OutCompilationOutput);
	}

	OutMaterialEnvironment = new FSharedShaderCompilerEnvironment();
	OutMaterialEnvironment->TargetPlatform = InCompilerTarget.TargetPlatform;
	GetMaterialEnvironment(InCompilerTarget.ShaderPlatform, InOutMaterial, EmitContext, OutCompilationOutput, *OutMaterialEnvironment);
	OutMaterialEnvironment->IncludeVirtualPathToContentsMap.Add(TEXT("/Engine/Generated/Material.ush"), MoveTemp(MaterialTemplateSource));

	return true;
}

#endif // WITH_EDITOR
