// Copyright Epic Games, Inc. All Rights Reserved.

#include "MaterialShader.h"
#include "DerivedDataCache.h"
#include "DerivedDataRequestOwner.h"
#include "RenderUtils.h"
#include "Stats/StatsMisc.h"
#include "Materials/MaterialAttributeDefinitionMap.h"
#include "Materials/MaterialInterface.h"
#include "ProfilingDebugging/DiagnosticTable.h"
#include "MeshMaterialShaderType.h"
#include "MaterialDomain.h"
#include "MaterialShaderMapLayout.h"
#include "SceneInterface.h"
#include "ShaderCompiler.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "ProfilingDebugging/CookStats.h"
#include "Stats/StatsTrace.h"
#include "UObject/ReleaseObjectVersion.h"
#include "UObject/EditorObjectVersion.h"
#include "ProfilingDebugging/CountersTrace.h"
#include "ProfilingDebugging/LoadTimeTracker.h"
#include "Misc/PathViews.h"
#include "SceneTexturesConfig.h"
#include "ShaderCodeLibrary.h"
#include "UObject/UE5ReleaseStreamObjectVersion.h"
#include "Materials/Material.h"

int32 GMaterialExcludeNonPipelinedShaders = 1;
static FAutoConsoleVariableRef CVarMaterialExcludeNonPipelinedShaders(
	TEXT("r.Material.ExcludeNonPipelinedShaders"),
	GMaterialExcludeNonPipelinedShaders,
	TEXT("if != 0, standalone shaders that are also part of FShaderPipeline will not be compiled (default)."),
	ECVF_ReadOnly
);

static TAutoConsoleVariable<FString> CVarMaterialShaderMapDump(
	TEXT("r.Material.ShaderMapDump"),
	"",
	TEXT("Outputs a textual dump of all shader maps found for the given named material (specified by path).\n")
	TEXT("Note that this will include any instances of said material created by a MaterialInstance.\n")
	TEXT("Files (.txt extension) will be dumped to Saved\\MaterialShaderMaps named with the DDC key hash.\n"),
	ECVF_ReadOnly);

#if ENABLE_COOK_STATS
namespace MaterialShaderCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static int32 ShadersCompiled = 0;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("MaterialShader.Usage"), TEXT(""));
		AddStat(TEXT("MaterialShader.Misc"), FCookStatsManager::CreateKeyValueArray(
			TEXT("ShadersCompiled"), ShadersCompiled
			));
	});
}
#endif

//
// Globals
//
FCriticalSection FMaterialShaderMap::GIdToMaterialShaderMapCS;
TMap<FMaterialShaderMapId,FMaterialShaderMap*> FMaterialShaderMap::GIdToMaterialShaderMap[SP_NumPlatforms];
#if ALLOW_SHADERMAP_DEBUG_DATA
TArray<FMaterialShaderMap*> FMaterialShaderMap::AllMaterialShaderMaps;
FCriticalSection FMaterialShaderMap::AllMaterialShaderMapsGuard;
#endif
// defined in the same module (Material.cpp)
bool PoolSpecialMaterialsCompileJobs();

/** 
 * Tracks material resources and their shader maps that are being compiled.
 * Uses a TRefCountPtr as this will be the only reference to a shader map while it is being compiled.
 */
//TMap<TRefCountPtr<FMaterialShaderMap>, TArray<FMaterial*> > FMaterialShaderMap::ShaderMapsBeingCompiled;

static inline bool ShouldCacheMaterialShader(const FMaterialShaderType* ShaderType, EShaderPlatform Platform, EShaderPermutationFlags PermutationFlags, const FMaterial* Material, int32 PermutationId)
{
	return ShaderType->ShouldCompilePermutation(Platform, Material, PermutationId, PermutationFlags) && Material->ShouldCache(Platform, ShaderType, nullptr);
}


/** Converts an EMaterialShadingModel to a string description. */
FString GetShadingModelString(EMaterialShadingModel ShadingModel)
{
	FString ShadingModelName;
	switch(ShadingModel)
	{
		case MSM_Unlit:				ShadingModelName = TEXT("MSM_Unlit"); break;
		case MSM_DefaultLit:		ShadingModelName = TEXT("MSM_DefaultLit"); break;
		case MSM_Subsurface:		ShadingModelName = TEXT("MSM_Subsurface"); break;
		case MSM_PreintegratedSkin:	ShadingModelName = TEXT("MSM_PreintegratedSkin"); break;
		case MSM_ClearCoat:			ShadingModelName = TEXT("MSM_ClearCoat"); break;
		case MSM_SubsurfaceProfile:	ShadingModelName = TEXT("MSM_SubsurfaceProfile"); break;
		case MSM_TwoSidedFoliage:	ShadingModelName = TEXT("MSM_TwoSidedFoliage"); break;
		case MSM_Hair:				ShadingModelName = TEXT("MSM_Hair"); break;
		case MSM_Cloth:				ShadingModelName = TEXT("MSM_Cloth"); break;
		case MSM_Eye:				ShadingModelName = TEXT("MSM_Eye"); break;
		case MSM_SingleLayerWater:	ShadingModelName = TEXT("MSM_SingleLayerWater"); break;
		case MSM_ThinTranslucent:	ShadingModelName = TEXT("MSM_ThinTranslucent"); break;
		default: ShadingModelName = TEXT("Unknown"); break;
	}
	return ShadingModelName;
}

/** Helpers class to identify and remove shader types that are going to be used by the pipelines. 
    Standalone shaders of that type should no longer be required, but removing them from the shadermap layout is a bigger endeavour */
class FPipelinedShaderFilter
{
	TSet<const FShaderType*>	PipelinedShaderTypes;
	bool						bAnyTypesExcluded = false;

public:

	FPipelinedShaderFilter(EShaderPlatform ShaderPlatform, TArray<FShaderPipelineType*> Pipelines)
	{
		if (ExcludeNonPipelinedShaderTypes(ShaderPlatform))
		{
			for (FShaderPipelineType* Pipeline : Pipelines)
			{
				if (Pipeline->ShouldOptimizeUnusedOutputs(ShaderPlatform))
				{
					PipelinedShaderTypes.Append(Pipeline->GetStages());
				}
			}

			bAnyTypesExcluded = PipelinedShaderTypes.Num() != 0;
		}
	}

	inline bool IsPipelinedType(const FShaderType* Type) const
	{
		return bAnyTypesExcluded ? PipelinedShaderTypes.Contains(Type) : false;
	}
};

/** Converts an FMaterialShadingModelField to a string description containing all the shading models present, delimited by "|" */
FString GetShadingModelFieldString(FMaterialShadingModelField ShadingModels, const FShadingModelToStringDelegate& Delegate, const FString& Delimiter)
{
	FString ShadingModelsName;
	uint32 TempShadingModels = (uint32)ShadingModels.GetShadingModelField();

	while (TempShadingModels)
	{
		uint32 BitIndex = FMath::CountTrailingZeros(TempShadingModels); // Find index of first set bit
		TempShadingModels &= ~(1 << BitIndex); // Flip first set bit to 0
		ShadingModelsName += Delegate.Execute((EMaterialShadingModel)BitIndex); // Add the name of the shading model corresponding to that bit

		// If there are more bits left, add a pipe limiter to the string 
		if (TempShadingModels)
		{
			ShadingModelsName.Append(Delimiter);
		}
	}

	return ShadingModelsName;
}

/** Converts an FMaterialShadingModelField to a string description containing all the shading models present, delimited by "|" */
FString GetShadingModelFieldString(FMaterialShadingModelField ShadingModels)
{
	return GetShadingModelFieldString(ShadingModels, FShadingModelToStringDelegate::CreateStatic(&GetShadingModelString), TEXT("|"));
}

/** Converts an EBlendMode to a string description. */
FString GetBlendModeString(EBlendMode BlendMode)
{
	return FString(UMaterial::GetBlendModeString(BlendMode));
}

#if WITH_EDITOR
/** Creates a string key for the derived data cache given a shader map id. */
static FString GetMaterialShaderMapKeyString(const FMaterialShaderMapId& ShaderMapId, EShaderPlatform Platform)
{
	FName Format = LegacyShaderPlatformToShaderFormat(Platform);
	FString ShaderMapKeyString;
	ShaderMapKeyString.Reserve(16384);
	
	ShaderMapKeyString.Append(TEXTVIEW("MATSM_"));
	ShaderMapKeyString.Append(GetMaterialShaderMapDDCKey());
	ShaderMapKeyString.AppendChar('_');

	Format.AppendString(ShaderMapKeyString);
	ShaderMapKeyString.AppendChar('_');
	ShaderMapKeyString.AppendInt(GetTargetPlatformManagerRef().ShaderFormatVersion(Format));
	ShaderMapKeyString.AppendChar('_');

	ShaderMapAppendKeyString(Platform, ShaderMapKeyString);
	ShaderMapId.AppendKeyString(ShaderMapKeyString);
	FMaterialAttributeDefinitionMap::AppendDDCKeyString(ShaderMapKeyString);
	FShaderCompileUtilities::AppendGBufferDDCKeyString(Platform, ShaderMapKeyString);

	return ShaderMapKeyString;
}

static UE::DerivedData::FCacheKey GetMaterialShaderMapKey(const FStringView MaterialShaderMapKey)
{
	static UE::DerivedData::FCacheBucket Bucket("MaterialShaderMap");
	return {Bucket, FIoHash::HashBuffer(MakeMemoryView(FTCHARToUTF8(MaterialShaderMapKey)))};
}

static UE::DerivedData::FSharedString GetMaterialShaderMapName(const FStringView MaterialPath, const FMaterialShaderMapId& ShaderMapId, const EShaderPlatform Platform)
{
	FName FeatureLevelName;
	GetFeatureLevelName(ShaderMapId.FeatureLevel, FeatureLevelName);
	return UE::DerivedData::FSharedString(WriteToString<256>(MaterialPath,
		TEXTVIEW(" ["), FDataDrivenShaderPlatformInfo::GetName(Platform),
		TEXTVIEW(", "), FeatureLevelName,
		TEXTVIEW(", "), LexToString(ShaderMapId.QualityLevel),
		TEXTVIEW("]")));
}

#endif // WITH_EDITOR

/** Called for every material shader to update the appropriate stats. */
void UpdateMaterialShaderCompilingStats(const FMaterial* Material)
{
	INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTotalMaterialShaders,1);

	switch(Material->GetBlendMode())
	{
		case BLEND_Opaque: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumOpaqueMaterialShaders,1); break;
		case BLEND_Masked: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumMaskedMaterialShaders,1); break;
		default: INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumTransparentMaterialShaders,1); break;
	}

	FMaterialShadingModelField ShadingModels = Material->GetShadingModels();
	
	if (ShadingModels.HasOnlyShadingModel(MSM_Unlit))
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumUnlitMaterialShaders, 1);
	}
	else if (ShadingModels.HasAnyShadingModel({ MSM_DefaultLit, MSM_Subsurface, MSM_PreintegratedSkin, MSM_ClearCoat, MSM_Cloth, MSM_SubsurfaceProfile, MSM_TwoSidedFoliage, MSM_SingleLayerWater, MSM_ThinTranslucent }))
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumLitMaterialShaders, 1);
	}


	if (Material->IsSpecialEngineMaterial())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumSpecialMaterialShaders,1);
	}
	if (Material->IsUsedWithParticleSystem())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumParticleMaterialShaders,1);
	}
	if (Material->IsUsedWithSkeletalMesh())
	{
		INC_DWORD_STAT_BY(STAT_ShaderCompiling_NumSkinnedMaterialShaders,1);
	}
}

FStaticParameterSet::FStaticParameterSet(const FStaticParameterSet& InValue) = default;

FStaticParameterSet& FStaticParameterSet::operator=(const FStaticParameterSet& InValue)
{
	StaticSwitchParameters = InValue.StaticSwitchParameters;
#if WITH_EDITORONLY_DATA
	EditorOnly.StaticComponentMaskParameters = InValue.EditorOnly.StaticComponentMaskParameters;
	EditorOnly.TerrainLayerWeightParameters = InValue.EditorOnly.TerrainLayerWeightParameters;
#endif // WITH_EDITORONLY_DATA
	MaterialLayers = InValue.MaterialLayers;
	bHasMaterialLayers = InValue.bHasMaterialLayers;
	if (bHasMaterialLayers)
	{
		MaterialLayers = InValue.MaterialLayers;
#if WITH_EDITORONLY_DATA
		EditorOnly.MaterialLayers = InValue.EditorOnly.MaterialLayers;
#endif
	}
	return *this;
}

void FStaticParameterSet::PostSerialize(const FArchive& Ar)
{
#if WITH_EDITORONLY_DATA
	if (StaticSwitchParameters_DEPRECATED.Num() > 0)
	{
		EditorOnly.StaticSwitchParameters_DEPRECATED = MoveTemp(StaticSwitchParameters_DEPRECATED);
	}
	if (EditorOnly.StaticSwitchParameters_DEPRECATED.Num() > 0)
	{
		StaticSwitchParameters = MoveTemp(EditorOnly.StaticSwitchParameters_DEPRECATED);
	}
	if (StaticComponentMaskParameters_DEPRECATED.Num() > 0)
	{
		EditorOnly.StaticComponentMaskParameters = MoveTemp(StaticComponentMaskParameters_DEPRECATED);
	}
	if (TerrainLayerWeightParameters_DEPRECATED.Num() > 0)
	{
		EditorOnly.TerrainLayerWeightParameters = MoveTemp(TerrainLayerWeightParameters_DEPRECATED);
	}
	// If we serialized a legacy 'MaterialLayers' property from a FMaterialLayersFunctions property, capture the editor-only portion here
	if (MaterialLayers.LegacySerializedEditorOnlyData)
	{
		EditorOnly.MaterialLayers = MoveTemp(*MaterialLayers.LegacySerializedEditorOnlyData);
		MaterialLayers.LegacySerializedEditorOnlyData.Reset();
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void FStaticParameterSet::SerializeLegacy(FArchive& Ar)
{
	// Old UMaterialInstances may use this path to serialize their 'StaticParameters' (newer assets will use automatic tagged serialization)
	// Even older UMaterialInstances may serialize FMaterialShaderMapId, which will potentially use this path as well (newer FMaterialShaderMapId do not serialize FStaticParameterSet directly)
	// In both cases, the data will be loaded from uasset, so backwards compatibility is required
	// New assets should *not* use this path, so this doesn't need to handle future version changes

	Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);

	Ar << EditorOnly.StaticSwitchParameters_DEPRECATED;
	Ar << EditorOnly.StaticComponentMaskParameters;
	Ar << EditorOnly.TerrainLayerWeightParameters;

	if (Ar.CustomVer(FReleaseObjectVersion::GUID) >= FReleaseObjectVersion::MaterialLayersParameterSerializationRefactor)
	{
		if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::MaterialLayerStacksAreNotParameters)
		{
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			Ar << MaterialLayersParameters_DEPRECATED;
			if (MaterialLayersParameters_DEPRECATED.Num() > 0)
			{
				bHasMaterialLayers = true;
				MaterialLayers = MoveTemp(MaterialLayersParameters_DEPRECATED[0].Value.GetRuntime());
				EditorOnly.MaterialLayers = MoveTemp(MaterialLayersParameters_DEPRECATED[0].Value.EditorOnly);
				MaterialLayersParameters_DEPRECATED.Empty();
			}
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}
}

void FStaticParameterSet::UpdateLegacyTerrainLayerWeightData()
{
	int32 ParameterIndex = 0;
	while (ParameterIndex < EditorOnly.TerrainLayerWeightParameters.Num())
	{
		FStaticTerrainLayerWeightParameter& TerrainParameter = EditorOnly.TerrainLayerWeightParameters[ParameterIndex];
		if (TerrainParameter.bOverride_DEPRECATED)
		{
			TerrainParameter.LayerName = TerrainParameter.ParameterInfo_DEPRECATED.Name;
			ParameterIndex++;
		}
		else
		{
			// Remove any parameters that didn't have bOverride set
			EditorOnly.TerrainLayerWeightParameters.RemoveAt(ParameterIndex);
		}
	}
}

void FStaticParameterSet::UpdateLegacyMaterialLayersData()
{
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (MaterialLayersParameters_DEPRECATED.Num() > 0)
	{
		bHasMaterialLayers = true;
		MaterialLayers = MoveTemp(MaterialLayersParameters_DEPRECATED[0].Value.GetRuntime());
		EditorOnly.MaterialLayers = MoveTemp(MaterialLayersParameters_DEPRECATED[0].Value.EditorOnly);
		MaterialLayersParameters_DEPRECATED.Empty();
	}
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

bool FStaticParameterSet::GetMaterialLayers(FMaterialLayersFunctions& OutMaterialLayers) const
{
	if (bHasMaterialLayers)
	{
		OutMaterialLayers.GetRuntime() = MaterialLayers;
		OutMaterialLayers.EditorOnly = EditorOnly.MaterialLayers;
		return true;
	}
	return false;
}

void FStaticParameterSet::Validate(const FStaticParameterSetRuntimeData& Runtime, const FStaticParameterSetEditorOnlyData& EditorOnly)
{
	FMaterialLayersFunctions::Validate(Runtime.MaterialLayers, EditorOnly.MaterialLayers);
}

#endif // WITH_EDITOR

bool FStaticParameterSet::operator==(const FStaticParameterSet& ReferenceSet) const
{
	if (bHasMaterialLayers != ReferenceSet.bHasMaterialLayers)
	{
		return false;
	}

	if(StaticSwitchParameters.Num() != ReferenceSet.StaticSwitchParameters.Num())
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (EditorOnly.StaticComponentMaskParameters.Num() != ReferenceSet.EditorOnly.StaticComponentMaskParameters.Num()
		|| EditorOnly.TerrainLayerWeightParameters.Num() != ReferenceSet.EditorOnly.TerrainLayerWeightParameters.Num())
	{
		return false;
	}

	if (EditorOnly.StaticComponentMaskParameters != ReferenceSet.EditorOnly.StaticComponentMaskParameters)
	{
		return false;
	}

	if (EditorOnly.TerrainLayerWeightParameters != ReferenceSet.EditorOnly.TerrainLayerWeightParameters)
	{
		return false;
	}
#endif // WITH_EDITORONLY_DATA

	if (StaticSwitchParameters != ReferenceSet.StaticSwitchParameters)
	{
		return false;
	}

	if (bHasMaterialLayers)
	{
		if (MaterialLayers != ReferenceSet.MaterialLayers)
		{
			return false;
		}
#if WITH_EDITORONLY_DATA
		if (EditorOnly.MaterialLayers != ReferenceSet.EditorOnly.MaterialLayers)
		{
			return false;
		}
#endif // WITH_EDITORONLY_DATA
	}

	return true;
}

void FStaticParameterSet::SortForEquivalent()
{
	StaticSwitchParameters.Sort([](const FStaticSwitchParameter& A, const FStaticSwitchParameter& B) { return B.ExpressionGUID < A.ExpressionGUID; });
#if WITH_EDITORONLY_DATA
	EditorOnly.StaticComponentMaskParameters.Sort([](const FStaticComponentMaskParameter& A, const FStaticComponentMaskParameter& B) { return B.ExpressionGUID < A.ExpressionGUID; });
	EditorOnly.TerrainLayerWeightParameters.Sort([](const FStaticTerrainLayerWeightParameter& A, const FStaticTerrainLayerWeightParameter& B) { return B.LayerName.LexicalLess(A.LayerName); });
#endif // WITH_EDITORONLY_DATA
}

bool FStaticParameterSet::Equivalent(const FStaticParameterSet& ReferenceSet) const
{
	if (bHasMaterialLayers != ReferenceSet.bHasMaterialLayers)
	{
		return false;
	}

	if(StaticSwitchParameters.Num() != ReferenceSet.StaticSwitchParameters.Num())
	{
		return false;
	}

#if WITH_EDITORONLY_DATA
	if (EditorOnly.StaticComponentMaskParameters.Num() != ReferenceSet.EditorOnly.StaticComponentMaskParameters.Num()
		|| EditorOnly.TerrainLayerWeightParameters.Num() != ReferenceSet.EditorOnly.TerrainLayerWeightParameters.Num())
	{
		return false;
	}
#endif // WITH_EDITORONLY_DATA

	// this is not ideal, but it is easy to code up
	FStaticParameterSet Temp1 = *this;
	FStaticParameterSet Temp2 = ReferenceSet;
	Temp1.SortForEquivalent();
	Temp2.SortForEquivalent();
	bool bResult = (Temp1 == Temp2);
	ensure(!bResult || (*this) == ReferenceSet); // if this never fires, then we really didn't need to sort did we?
	return bResult;
}

#if WITH_EDITORONLY_DATA
void FStaticParameterSet::SetParameterValue(const FMaterialParameterInfo& ParameterInfo, const FMaterialParameterMetadata& Meta, EMaterialSetParameterValueFlags Flags)
{
	const FMaterialParameterValue& Value = Meta.Value;
	switch (Value.Type)
	{
	case EMaterialParameterType::StaticSwitch: SetStaticSwitchParameterValue(ParameterInfo, Meta.ExpressionGuid, Value.AsStaticSwitch()); break;
	case EMaterialParameterType::StaticComponentMask: SetStaticComponentMaskParameterValue(ParameterInfo, Meta.ExpressionGuid, Value.Bool[0], Value.Bool[1], Value.Bool[2], Value.Bool[3]); break;
	default: checkNoEntry();
	}
}

void FStaticParameterSet::AddParametersOfType(EMaterialParameterType Type, const TMap<FMaterialParameterInfo, FMaterialParameterMetadata>& Values)
{
	switch (Type)
	{
	case EMaterialParameterType::StaticSwitch:
		StaticSwitchParameters.Empty(Values.Num());
		for (const auto& It : Values)
		{
			const FMaterialParameterMetadata& Meta = It.Value;
			check(Meta.Value.Type == Type);
			if(!Meta.bDynamicSwitchParameter)
			{
				StaticSwitchParameters.Emplace(It.Key, Meta.Value.AsStaticSwitch(), Meta.bOverride, Meta.ExpressionGuid);
			}
		}
		break;
	case EMaterialParameterType::StaticComponentMask:
		EditorOnly.StaticComponentMaskParameters.Empty(Values.Num());
		for (const auto& It : Values)
		{
			const FMaterialParameterMetadata& Meta = It.Value;
			check(Meta.Value.Type == Type);
			EditorOnly.StaticComponentMaskParameters.Emplace(It.Key,
				Meta.Value.Bool[0],
				Meta.Value.Bool[1],
				Meta.Value.Bool[2],
				Meta.Value.Bool[3],
				Meta.bOverride,
				Meta.ExpressionGuid);
		}
		break;
	default: checkNoEntry();
	}
}

void FStaticParameterSet::SetStaticSwitchParameterValue(const FMaterialParameterInfo& ParameterInfo, const FGuid& ExpressionGuid, bool Value)
{
	for (FStaticSwitchParameter& Parameter : StaticSwitchParameters)
	{
		if (Parameter.ParameterInfo == ParameterInfo)
		{
			Parameter.bOverride = true;
			Parameter.Value = Value;
			return;
		}
	}

	new(StaticSwitchParameters) FStaticSwitchParameter(ParameterInfo, Value, true, ExpressionGuid);
}

void FStaticParameterSet::SetStaticComponentMaskParameterValue(const FMaterialParameterInfo& ParameterInfo, const FGuid& ExpressionGuid, bool R, bool G, bool B, bool A)
{
	for (FStaticComponentMaskParameter& Parameter : EditorOnly.StaticComponentMaskParameters)
	{
		if (Parameter.ParameterInfo == ParameterInfo)
		{
			Parameter.bOverride = true;
			Parameter.R = R;
			Parameter.G = G;
			Parameter.B = B;
			Parameter.A = A;
			return;
		}
	}

	new(EditorOnly.StaticComponentMaskParameters) FStaticComponentMaskParameter(ParameterInfo, R, G, B, A, true, ExpressionGuid);
}
#endif // WITH_EDITORONLY_DATA



#if WITH_EDITOR
FString FStrataCompilationConfig::GetShaderMapKeyString() const
{
	if (bFullSimplify)
	{
		return TEXT("_STRTFS");
	}
	return TEXT("");
}

void FStrataCompilationConfig::UpdateHash(FSHA1& Hasher) const
{
	Hasher.Update((const uint8*)(&bFullSimplify), sizeof(bFullSimplify));
}

void FStrataCompilationConfig::Serialize(FArchive& Ar)
{
	Ar << bFullSimplify;
}
#endif

void FMaterialShaderMapId::Serialize(FArchive& Ar, bool bLoadedByCookedMaterial)
{
	SCOPED_LOADTIMER(FMaterialShaderMapId_Serialize);

	// Note: FMaterialShaderMapId is saved both in packages (legacy UMaterialInstance) and the DDC (FMaterialShaderMap)
	// Backwards compatibility only works with FMaterialShaderMapId's stored in packages.  
	// Only serialized in legacy packages if UEVer() < VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS
	// You must bump MATERIALSHADERMAP_DERIVEDDATA_VER as well if changing the serialization of FMaterialShaderMapId.
	Ar.UsingCustomVersion(FEditorObjectVersion::GUID);
	Ar.UsingCustomVersion(FReleaseObjectVersion::GUID);
	Ar.UsingCustomVersion(FUE5ReleaseStreamObjectVersion::GUID);

	const bool bIsLegacyPackage = Ar.UEVer() < VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS;

	// Ensure saved content is correct
	check(!Ar.IsSaving() || IsContentValid());

#if WITH_EDITOR
	const bool bIsSavingCooked = Ar.IsSaving() && Ar.IsCooking();
	bIsCookedId = bLoadedByCookedMaterial;

	if (!bIsSavingCooked && !bLoadedByCookedMaterial)
	{
		uint32 UsageInt = Usage;
		Ar << UsageInt;
		Usage = (EMaterialShaderMapUsage::Type)UsageInt;

		if (Usage == EMaterialShaderMapUsage::MaterialExportCustomOutput)
		{
			Ar << UsageCustomOutput;
		}

		Ar << BaseMaterialId;
	}
#endif

	if (!bIsLegacyPackage)
	{
		static_assert(sizeof(QualityLevel) == 1, "If you change the size of QualityLevel, you must adjust this serialization code and bump MATERIALSHADERMAP_DERIVEDDATA_VER");
		Ar << (uint8&)QualityLevel;
		Ar << (int32&)FeatureLevel;
	}
	else
	{
		uint8 LegacyQualityLevel;
		Ar << LegacyQualityLevel;
	}

#if WITH_EDITOR
	if (!bIsSavingCooked && !bLoadedByCookedMaterial)
	{
		Ar.UsingCustomVersion(FRenderingObjectVersion::GUID);

		if (Ar.CustomVer(FRenderingObjectVersion::GUID) < FRenderingObjectVersion::MaterialShaderMapIdSerialization)
		{
			// Serialize using old path
			FStaticParameterSet ParameterSet;
			ParameterSet.SerializeLegacy(Ar);
			UpdateFromParameterSet(ParameterSet);
		}
		else
		{
			Ar << StaticSwitchParameters;
			Ar << StaticComponentMaskParameters;
			Ar << TerrainLayerWeightParameters;
			if (Ar.CustomVer(FUE5ReleaseStreamObjectVersion::GUID) < FUE5ReleaseStreamObjectVersion::MaterialLayerStacksAreNotParameters)
			{
				PRAGMA_DISABLE_DEPRECATION_WARNINGS
				TArray<FStaticMaterialLayersParameter::ID> MaterialLayersParameterIDs;
				Ar << MaterialLayersParameterIDs;
				if (MaterialLayersParameterIDs.Num() > 0)
				{
					MaterialLayersId = MoveTemp(MaterialLayersParameterIDs[0].Functions);
				}
				PRAGMA_ENABLE_DEPRECATION_WARNINGS
			}
			else
			{
				Ar << MaterialLayersId;
			}
		}

		Ar << ReferencedFunctions;

		if (Ar.UEVer() >= VER_UE4_COLLECTIONS_IN_SHADERMAPID)
		{
			Ar << ReferencedParameterCollections;
		}

		if (Ar.CustomVer(FEditorObjectVersion::GUID) >= FEditorObjectVersion::AddedMaterialSharedInputs &&
			Ar.CustomVer(FReleaseObjectVersion::GUID) < FReleaseObjectVersion::RemovedMaterialSharedInputCollection)
		{
			TArray<FGuid> Deprecated;
			Ar << Deprecated;
		}

		Ar << ShaderTypeDependencies;
		if (!bIsLegacyPackage)
		{
			Ar << ShaderPipelineTypeDependencies;
		}
		Ar << VertexFactoryTypeDependencies;

		if (!bIsLegacyPackage)
		{
			Ar << TextureReferencesHash;
		}
		else
		{
			FSHAHash LegacyHash;
			Ar << LegacyHash;
		}

		if (Ar.UEVer() >= VER_UE4_MATERIAL_INSTANCE_BASE_PROPERTY_OVERRIDES)
		{
			Ar << BasePropertyOverridesHash;
		}

		if (!bIsLegacyPackage)
		{
			Ar << bUsingNewHLSLGenerator;
		}
		else
		{
			bUsingNewHLSLGenerator = false;
		}

		// STRATA_TODO We do not need to serialize FStrataCompilationConfig for now since this is only used when debugging in the editor.
		// However we might want to do that when compilation config will change between raster and path tracing for instance.
		// So currently, the shader map DDC key string won't be changing, but if the user toggles simplification on via the Material Editor it will cache a new map. 
		// In other words we only cache the simplified shader map version of the material in the editor (and not during cooks).
	}
	else
	{
		if (bIsSavingCooked)
		{
			// Saving cooked data, this should be valid
			GetMaterialHash(CookedShaderMapIdHash);
			checkf(CookedShaderMapIdHash != FSHAHash(), TEXT("Tried to save an invalid shadermap id hash during cook"));
		}
		
		Ar << CookedShaderMapIdHash;
	}
#else
	// Cooked so can assume this is valid
	Ar << CookedShaderMapIdHash;
	checkf(CookedShaderMapIdHash != FSHAHash(), TEXT("Loaded an invalid cooked shadermap id hash"));
#endif // WITH_EDITOR

	if (!bIsLegacyPackage)
	{
		Ar << LayoutParams;
	}
	else
	{
		LayoutParams.InitializeForCurrent();
	}

	// Ensure loaded content is correct
	check(!Ar.IsLoading() || IsContentValid());
}

#if WITH_EDITOR
/** Hashes the material-specific part of this shader map Id. */
void FMaterialShaderMapId::GetMaterialHash(FSHAHash& OutHash, bool bWithStaticParameters) const
{
	check(IsContentValid());
	FSHA1 HashState;

	HashState.Update((const uint8*)&Usage, sizeof(Usage));
	if (Usage == EMaterialShaderMapUsage::MaterialExportCustomOutput)
	{
		HashState.UpdateWithString(*UsageCustomOutput, UsageCustomOutput.Len());
	}

	HashState.Update((const uint8*)&BaseMaterialId, sizeof(BaseMaterialId));

	FString QualityLevelString;
	GetMaterialQualityLevelName(QualityLevel, QualityLevelString);
	HashState.UpdateWithString(*QualityLevelString, QualityLevelString.Len());

	HashState.Update((const uint8*)&FeatureLevel, sizeof(FeatureLevel));


	//Hash the static parameters
	if (bWithStaticParameters)
	{
		for (const FStaticSwitchParameter& StaticSwitchParameter : StaticSwitchParameters)
		{
			StaticSwitchParameter.UpdateHash(HashState);
		}
	}
	for (const FStaticComponentMaskParameter& StaticComponentMaskParameter : StaticComponentMaskParameters)
	{
		StaticComponentMaskParameter.UpdateHash(HashState);
	}
	for (const FStaticTerrainLayerWeightParameter& StaticTerrainLayerWeightParameter : TerrainLayerWeightParameters)
	{
		StaticTerrainLayerWeightParameter.UpdateHash(HashState);
	}
	if (MaterialLayersId)
	{
		MaterialLayersId->UpdateHash(HashState);
	}

	for (int32 FunctionIndex = 0; FunctionIndex < ReferencedFunctions.Num(); FunctionIndex++)
	{
		HashState.Update((const uint8*)&ReferencedFunctions[FunctionIndex], sizeof(ReferencedFunctions[FunctionIndex]));
	}

	for (int32 CollectionIndex = 0; CollectionIndex < ReferencedParameterCollections.Num(); CollectionIndex++)
	{
		HashState.Update((const uint8*)&ReferencedParameterCollections[CollectionIndex], sizeof(ReferencedParameterCollections[CollectionIndex]));
	}

	for (int32 VertexFactoryIndex = 0; VertexFactoryIndex < VertexFactoryTypeDependencies.Num(); VertexFactoryIndex++)
	{
		HashState.Update((const uint8*)&VertexFactoryTypeDependencies[VertexFactoryIndex].VFSourceHash, sizeof(VertexFactoryTypeDependencies[VertexFactoryIndex].VFSourceHash));
	}

	HashState.Update((const uint8*)&TextureReferencesHash, sizeof(TextureReferencesHash));

	HashState.Update((const uint8*)&BasePropertyOverridesHash, sizeof(BasePropertyOverridesHash));

	HashState.Update((const uint8*)&bUsingNewHLSLGenerator, sizeof(bUsingNewHLSLGenerator));

	StrataCompilationConfig.UpdateHash(HashState);

	HashState.Final();
	HashState.GetHash(&OutHash.Hash[0]);
}
#endif // WITH_EDITOR

/** 
* Tests this set against another for equality.
* 
* @param ReferenceSet	The set to compare against
* @return				true if the sets are equal
*/
bool FMaterialShaderMapId::Equals(const FMaterialShaderMapId& ReferenceSet, bool bWithStaticParameters) const
{
	// Ensure data is in valid state for comparison
	check(IsContentValid() && ReferenceSet.IsContentValid());

#if WITH_EDITOR
	if (IsCookedId() != ReferenceSet.IsCookedId())
	{
		return false;
	}

	if (bUsingNewHLSLGenerator != ReferenceSet.bUsingNewHLSLGenerator)
	{
		return false;
	}

	if (StrataCompilationConfig != ReferenceSet.StrataCompilationConfig)
	{
		return false;
	}

	if (!IsCookedId())
	{
		if (BaseMaterialId != ReferenceSet.BaseMaterialId)
		{
			return false;
		}
	}
	else
#endif
	{
		if (CookedShaderMapIdHash != ReferenceSet.CookedShaderMapIdHash)
		{
			return false;
		}
	}

#if WITH_EDITOR
	if (Usage != ReferenceSet.Usage
		|| UsageCustomOutput != ReferenceSet.UsageCustomOutput)
	{
		return false;
	}
#endif

	if (QualityLevel != ReferenceSet.QualityLevel
		|| FeatureLevel != ReferenceSet.FeatureLevel)
	{
		return false;
	}

	if (LayoutParams != ReferenceSet.LayoutParams)
	{
		return false;
	}

#if WITH_EDITOR
	if (!IsCookedId())
	{
		if ((bWithStaticParameters && StaticSwitchParameters.Num() != ReferenceSet.StaticSwitchParameters.Num())
			|| StaticComponentMaskParameters.Num() != ReferenceSet.StaticComponentMaskParameters.Num()
			|| TerrainLayerWeightParameters.Num() != ReferenceSet.TerrainLayerWeightParameters.Num()
			|| ReferencedFunctions.Num() != ReferenceSet.ReferencedFunctions.Num()
			|| ReferencedParameterCollections.Num() != ReferenceSet.ReferencedParameterCollections.Num()
			|| ShaderTypeDependencies.Num() != ReferenceSet.ShaderTypeDependencies.Num()
			|| ShaderPipelineTypeDependencies.Num() != ReferenceSet.ShaderPipelineTypeDependencies.Num()
			|| VertexFactoryTypeDependencies.Num() != ReferenceSet.VertexFactoryTypeDependencies.Num())
		{
			return false;
		}

		if ((bWithStaticParameters && StaticSwitchParameters != ReferenceSet.StaticSwitchParameters)
			|| StaticComponentMaskParameters != ReferenceSet.StaticComponentMaskParameters
			|| TerrainLayerWeightParameters != ReferenceSet.TerrainLayerWeightParameters
			|| MaterialLayersId != ReferenceSet.MaterialLayersId)
		{
			return false;
		}

		for (int32 RefFunctionIndex = 0; RefFunctionIndex < ReferenceSet.ReferencedFunctions.Num(); RefFunctionIndex++)
		{
			const FGuid& ReferenceGuid = ReferenceSet.ReferencedFunctions[RefFunctionIndex];

			if (ReferencedFunctions[RefFunctionIndex] != ReferenceGuid)
			{
				return false;
			}
		}

		for (int32 RefCollectionIndex = 0; RefCollectionIndex < ReferenceSet.ReferencedParameterCollections.Num(); RefCollectionIndex++)
		{
			const FGuid& ReferenceGuid = ReferenceSet.ReferencedParameterCollections[RefCollectionIndex];

			if (ReferencedParameterCollections[RefCollectionIndex] != ReferenceGuid)
			{
				return false;
			}
		}

		for (int32 ShaderIndex = 0; ShaderIndex < ShaderTypeDependencies.Num(); ShaderIndex++)
		{
			const FShaderTypeDependency& ShaderTypeDependency = ShaderTypeDependencies[ShaderIndex];

			if (ShaderTypeDependency != ReferenceSet.ShaderTypeDependencies[ShaderIndex])
			{
				return false;
			}
		}

		for (int32 ShaderPipelineIndex = 0; ShaderPipelineIndex < ShaderPipelineTypeDependencies.Num(); ShaderPipelineIndex++)
		{
			const FShaderPipelineTypeDependency& ShaderPipelineTypeDependency = ShaderPipelineTypeDependencies[ShaderPipelineIndex];

			if (ShaderPipelineTypeDependency != ReferenceSet.ShaderPipelineTypeDependencies[ShaderPipelineIndex])
			{
				return false;
			}
		}

		for (int32 VFIndex = 0; VFIndex < VertexFactoryTypeDependencies.Num(); VFIndex++)
		{
			const FVertexFactoryTypeDependency& VFDependency = VertexFactoryTypeDependencies[VFIndex];

			if (VFDependency != ReferenceSet.VertexFactoryTypeDependencies[VFIndex])
			{
				return false;
			}
		}

		if (TextureReferencesHash != ReferenceSet.TextureReferencesHash)
		{
			return false;
		}

		if( BasePropertyOverridesHash != ReferenceSet.BasePropertyOverridesHash )
		{
			return false;
		}
	}
#endif // WITH_EDITOR

	return true;
}

/** Ensure content is valid - for example overrides are set deterministically for serialization and sorting */
bool FMaterialShaderMapId::IsContentValid() const
{
#if WITH_EDITOR
	if (!LayoutParams.IsInitialized())
	{
		return false;
	}

	//We expect overrides to be set to false
	for (const FStaticSwitchParameter& StaticSwitchParameter : StaticSwitchParameters)
	{
		if (StaticSwitchParameter.bOverride != false)
		{
			return false;
		}
	}
	for (const FStaticComponentMaskParameter& StaticComponentMaskParameter : StaticComponentMaskParameters)
	{
		if (StaticComponentMaskParameter.bOverride != false)
		{
			return false;
		}
	}
#endif // WITH_EDITOR
	return true;
}


#if WITH_EDITOR
void FMaterialShaderMapId::UpdateFromParameterSet(const FStaticParameterSet& StaticParameters)
{
	struct FStaticParameterCompare
	{
		bool operator()(const FStaticParameterBase& Lhs, const FStaticParameterBase& Rhs) const
		{
			if (Lhs.ParameterInfo.Association != Rhs.ParameterInfo.Association) return Lhs.ParameterInfo.Association < Rhs.ParameterInfo.Association;
			else if (Lhs.ParameterInfo.Index != Rhs.ParameterInfo.Index) return Lhs.ParameterInfo.Index < Rhs.ParameterInfo.Index;
			else return Lhs.ParameterInfo.Name.LexicalLess(Rhs.ParameterInfo.Name);
		}
	};

	struct FStaticTerrainLayerWeightParameterCompare
	{
		bool operator()(const FStaticTerrainLayerWeightParameter& Lhs, const FStaticTerrainLayerWeightParameter& Rhs) const
		{
			return Lhs.LayerName.LexicalLess(Rhs.LayerName);
		}
	};

	StaticSwitchParameters = StaticParameters.StaticSwitchParameters;
	StaticComponentMaskParameters = StaticParameters.EditorOnly.StaticComponentMaskParameters;
	TerrainLayerWeightParameters = StaticParameters.EditorOnly.TerrainLayerWeightParameters;
	if (StaticParameters.bHasMaterialLayers)
	{
		MaterialLayersId = StaticParameters.MaterialLayers.GetID(StaticParameters.EditorOnly.MaterialLayers);
	}

	// Sort the arrays by parameter name, ensure the ID is not influenced by the order
	StaticSwitchParameters.Sort(FStaticParameterCompare());
	StaticComponentMaskParameters.Sort(FStaticParameterCompare());
	TerrainLayerWeightParameters.Sort(FStaticTerrainLayerWeightParameterCompare());

	//since bOverrides aren't used to check id matches, make sure they're consistently set to false in the static parameter set as part of the id.
	//this ensures deterministic cook results, rather than allowing bOverride to be set in the shader map's copy of the id based on the first id used.
	for (FStaticSwitchParameter& StaticSwitchParameter : StaticSwitchParameters)
	{
		StaticSwitchParameter.bOverride = false;
	}
	for (FStaticComponentMaskParameter& StaticComponentMaskParameter : StaticComponentMaskParameters)
	{
		StaticComponentMaskParameter.bOverride = false;
	}
}	

uint32 GetTypeHash(FPlatformTypeLayoutParameters Params) { return HashCombine(Params.Flags, Params.MaxFieldAlignment); }

void FMaterialShaderMapId::AppendStaticParametersString(FString& ParamsString) const
{
	for (const FStaticSwitchParameter& StaticSwitchParameter : StaticSwitchParameters)
	{
		StaticSwitchParameter.AppendKeyString(ParamsString);
	}
	for (const FStaticComponentMaskParameter& StaticComponentMaskParameter : StaticComponentMaskParameters)
	{
		StaticComponentMaskParameter.AppendKeyString(ParamsString);
	}
	for (const FStaticTerrainLayerWeightParameter& StaticTerrainLayerWeightParameter : TerrainLayerWeightParameters)
	{
		StaticTerrainLayerWeightParameter.AppendKeyString(ParamsString);
	}
}

void FMaterialShaderMapId::AppendKeyString(FString& KeyString, bool bIncludeSourceAndMaterialState) const
{
	check(IsContentValid());
	if (bIncludeSourceAndMaterialState)
	{
		BaseMaterialId.AppendString(KeyString);
		KeyString.AppendChar('_');
	}

	GetMaterialQualityLevelFName(QualityLevel).AppendString(KeyString);
	KeyString.AppendChar('_');

	FName FeatureLevelName;
	GetFeatureLevelName(FeatureLevel, FeatureLevelName);
	FeatureLevelName.AppendString(KeyString);
	KeyString.AppendChar('_');

	LayoutParams.AppendKeyString(KeyString);

	AppendStaticParametersString(KeyString);

	if (MaterialLayersId)
	{
		MaterialLayersId->AppendKeyString(KeyString);
	}

	KeyString.AppendChar('_');
	KeyString.AppendInt(Usage);
	KeyString.AppendChar('_');

	if (Usage == EMaterialShaderMapUsage::MaterialExportCustomOutput)
	{
		KeyString += UsageCustomOutput;
		KeyString.AppendChar('_');
	}

	if (bIncludeSourceAndMaterialState)
	{
		// Add any referenced functions to the key so that we will recompile when they are changed
		for (int32 FunctionIndex = 0; FunctionIndex < ReferencedFunctions.Num(); FunctionIndex++)
		{
			ReferencedFunctions[FunctionIndex].AppendString(KeyString);
		}
	}

	{
		const FSHAHash LayoutHash = GetShaderTypeLayoutHash(StaticGetTypeLayoutDesc<FMaterialShaderMapContent>(), LayoutParams);
		KeyString.AppendChar('_');
		LayoutHash.AppendString(KeyString);
		KeyString.AppendChar('_');
	}

	KeyString.AppendChar('_');

	if (bIncludeSourceAndMaterialState)
	{
		for (int32 CollectionIndex = 0; CollectionIndex < ReferencedParameterCollections.Num(); CollectionIndex++)
		{
			ReferencedParameterCollections[CollectionIndex].AppendString(KeyString);
		}
	}

	// Add the inputs for any shaders that are stored inline in the shader map
	AppendKeyStringShaderDependencies(
		MakeArrayView(ShaderTypeDependencies),
		MakeArrayView(ShaderPipelineTypeDependencies),
		MakeArrayView(VertexFactoryTypeDependencies),
		LayoutParams, 
		KeyString,
		bIncludeSourceAndMaterialState);

	BytesToHex(&TextureReferencesHash.Hash[0], sizeof(TextureReferencesHash.Hash), KeyString);

	BytesToHex(&BasePropertyOverridesHash.Hash[0], sizeof(BasePropertyOverridesHash.Hash), KeyString);

	if (bUsingNewHLSLGenerator)
	{
		KeyString += TEXT("_NewHLSL");
	}

	KeyString += StrataCompilationConfig.GetShaderMapKeyString();
}

void FMaterialShaderMapId::SetShaderDependencies(const TArray<FShaderType*>& ShaderTypes, const TArray<const FShaderPipelineType*>& ShaderPipelineTypes, const TArray<FVertexFactoryType*>& VFTypes, EShaderPlatform ShaderPlatform)
{
	if (!FPlatformProperties::RequiresCookedData() && AllowShaderCompiling())
	{
		for (int32 ShaderTypeIndex = 0; ShaderTypeIndex < ShaderTypes.Num(); ShaderTypeIndex++)
		{
			FShaderTypeDependency Dependency;
			Dependency.ShaderTypeName = ShaderTypes[ShaderTypeIndex]->GetHashedName();
			Dependency.SourceHash = ShaderTypes[ShaderTypeIndex]->GetSourceHash(ShaderPlatform);
			ShaderTypeDependencies.Add(Dependency);
		}

		for (int32 VFTypeIndex = 0; VFTypeIndex < VFTypes.Num(); VFTypeIndex++)
		{
			FVertexFactoryTypeDependency Dependency;
			Dependency.VertexFactoryTypeName = VFTypes[VFTypeIndex]->GetHashedName();
			Dependency.VFSourceHash = VFTypes[VFTypeIndex]->GetSourceHash(ShaderPlatform);
			VertexFactoryTypeDependencies.Add(Dependency);
		}

		for (int32 TypeIndex = 0; TypeIndex < ShaderPipelineTypes.Num(); TypeIndex++)
		{
			const FShaderPipelineType* Pipeline = ShaderPipelineTypes[TypeIndex];
			FShaderPipelineTypeDependency Dependency;
			Dependency.ShaderPipelineTypeName = Pipeline->GetHashedName();
			Dependency.StagesSourceHash = Pipeline->GetSourceHash(ShaderPlatform);
			ShaderPipelineTypeDependencies.Add(Dependency);
		}
	}
}

static void PrepareMaterialShaderCompileJob(EShaderPlatform Platform,
	EShaderPermutationFlags PermutationFlags,
	const FMaterial* Material,
	const FMaterialShaderMapId& ShaderMapId,
	FSharedShaderCompilerEnvironment* MaterialEnvironment,
	const FShaderPipelineType* ShaderPipeline,
	FString DebugDescription,
	FString DebugExtension,
	FShaderCompileJob* NewJob)
{
	const FShaderCompileJobKey& Key = NewJob->Key;
	const FMaterialShaderType* ShaderType = Key.ShaderType->AsMaterialShaderType();

	NewJob->Input.SharedEnvironment = MaterialEnvironment;
	FShaderCompilerEnvironment& ShaderEnvironment = NewJob->Input.Environment;

	UE_LOG(LogShaders, Verbose, TEXT("			%s"), ShaderType->GetName());
	COOK_STAT(MaterialShaderCookStats::ShadersCompiled++);

	//update material shader stats
	UpdateMaterialShaderCompilingStats(Material);

	Material->SetupExtaCompilationSettings(Platform, NewJob->Input.ExtraSettings);

	// Allow the shader type to modify the compile environment.
	ShaderType->SetupCompileEnvironment(Platform, Material, Key.PermutationId, PermutationFlags, ShaderEnvironment);

	// Compile the shader environment passed in with the shader type's source code.
	::GlobalBeginCompileShader(
		Material->GetUniqueAssetName(Platform, ShaderMapId) / LexToString(Material->GetQualityLevel()),
		nullptr,
		ShaderType,
		ShaderPipeline,
		Key.PermutationId,
		ShaderType->GetShaderFilename(),
		ShaderType->GetFunctionName(),
		FShaderTarget(ShaderType->GetFrequency(), Platform),
		NewJob->Input,
		true,
		DebugDescription,
		DebugExtension
	);
}

/**
 * Enqueues a compilation for a new shader of this type.
 * @param Material - The material to link the shader with.
 */
void FMaterialShaderType::BeginCompileShader(
	EShaderCompileJobPriority Priority,
	uint32 ShaderMapJobId,
	int32 PermutationId,
	const FMaterial* Material,
	const FMaterialShaderMapId& ShaderMapId,
	FSharedShaderCompilerEnvironment* MaterialEnvironment,
	EShaderPlatform Platform,
	EShaderPermutationFlags PermutationFlags,
	TArray<FShaderCommonCompileJobPtr>& NewJobs,
	const TCHAR* DebugDescription,
	const TCHAR* DebugExtension
	) const
{
	FShaderCompileJob* NewJob = GShaderCompilingManager->PrepareShaderCompileJob(ShaderMapJobId, FShaderCompileJobKey(this, nullptr, PermutationId), Priority);
	if (NewJob)
	{
		PrepareMaterialShaderCompileJob(Platform, PermutationFlags, Material, ShaderMapId, MaterialEnvironment, nullptr, DebugDescription, DebugExtension, NewJob);
		NewJobs.Add(FShaderCommonCompileJobPtr(NewJob));
	}
}

void FMaterialShaderType::BeginCompileShaderPipeline(
	EShaderCompileJobPriority Priority,
	uint32 ShaderMapJobId,
	EShaderPlatform Platform,
	EShaderPermutationFlags PermutationFlags,
	const FMaterial* Material,
	const FMaterialShaderMapId& ShaderMapId,
	FSharedShaderCompilerEnvironment* MaterialEnvironment,
	const FShaderPipelineType* ShaderPipeline,
	TArray<FShaderCommonCompileJobPtr>& NewJobs,
	const TCHAR* DebugDescription,
	const TCHAR* DebugExtension)
{
	check(ShaderPipeline);
	UE_LOG(LogShaders, Verbose, TEXT("	Pipeline: %s"), ShaderPipeline->GetName());

	// Add all the jobs as individual first, then add the dependencies into a pipeline job
	auto* NewPipelineJob = GShaderCompilingManager->PreparePipelineCompileJob(ShaderMapJobId, FShaderPipelineCompileJobKey(ShaderPipeline, nullptr, kUniqueShaderPermutationId), Priority);
	if (NewPipelineJob)
	{
		for (FShaderCompileJob* StageJob : NewPipelineJob->StageJobs)
		{
			PrepareMaterialShaderCompileJob(Platform, PermutationFlags, Material, ShaderMapId, MaterialEnvironment, ShaderPipeline, DebugDescription, DebugExtension, StageJob);
		}
		NewJobs.Add(FShaderCommonCompileJobPtr(NewPipelineJob));
	}
}

/**
 * Either creates a new instance of this type or returns an equivalent existing shader.
 * @param Material - The material to link the shader with.
 * @param CurrentJob - Compile job that was enqueued by BeginCompileShader.
 */
FShader* FMaterialShaderType::FinishCompileShader(
	const FUniformExpressionSet& UniformExpressionSet,
	const FSHAHash& MaterialShaderMapHash,
	const FShaderCompileJob& CurrentJob,
	const FShaderPipelineType* ShaderPipelineType,
	const FString& InDebugDescription
	) const
{
	check(CurrentJob.bSucceeded);

	if (ShaderPipelineType && !ShaderPipelineType->ShouldOptimizeUnusedOutputs(CurrentJob.Input.Target.GetPlatform()))
	{
		// If sharing shaders in this pipeline, remove it from the type/id so it uses the one in the shared shadermap list
		ShaderPipelineType = nullptr;
	}

	FShader* Shader = ConstructCompiled(CompiledShaderInitializerType(this, CurrentJob.Key.PermutationId, CurrentJob.Output, UniformExpressionSet, MaterialShaderMapHash, ShaderPipelineType, nullptr, InDebugDescription));
	CurrentJob.Output.ParameterMap.VerifyBindingsAreComplete(GetName(), CurrentJob.Output.Target, CurrentJob.Key.VFType);

	return Shader;
}
#endif // WITH_EDITOR

bool FMaterialShaderType::ShouldCompilePermutation(EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, int32 PermutationId, EShaderPermutationFlags Flags) const
{
	return FShaderType::ShouldCompilePermutation(FMaterialShaderPermutationParameters(Platform, MaterialParameters, PermutationId, Flags));
}

bool FMaterialShaderType::ShouldCompilePipeline(const FShaderPipelineType* ShaderPipelineType, EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, EShaderPermutationFlags Flags)
{
	const FMaterialShaderPermutationParameters Parameters(Platform, MaterialParameters, kUniqueShaderPermutationId, Flags);
	for (const FShaderType* ShaderType : ShaderPipelineType->GetStages())
	{
		checkSlow(ShaderType->GetMaterialShaderType());
		if (!ShaderType->ShouldCompilePermutation(Parameters))
		{
			return false;
		}
	}
	return true;
}

#if WITH_EDITOR
void FMaterialShaderType::SetupCompileEnvironment(EShaderPlatform Platform, const FMaterialShaderParameters& MaterialParameters, int32 PermutationId, EShaderPermutationFlags PermutationFlags, FShaderCompilerEnvironment& Environment) const
{
	// Allow the shader type to modify its compile environment.
	ModifyCompilationEnvironment(FMaterialShaderPermutationParameters(Platform, MaterialParameters, PermutationId, PermutationFlags), Environment);
}
#endif // WITH_EDITOR

/**
* Finds the shader map for a material.
* @param StaticParameterSet - The static parameter set identifying the shader map
* @param Platform - The platform to lookup for
* @return NULL if no cached shader map was found.
*/
TRefCountPtr<FMaterialShaderMap> FMaterialShaderMap::FindId(const FMaterialShaderMapId& ShaderMapId, EShaderPlatform InPlatform)
{
	FScopeLock ScopeLock(&GIdToMaterialShaderMapCS);
	check(ShaderMapId.IsValid());
	TRefCountPtr<FMaterialShaderMap> Result = GIdToMaterialShaderMap[InPlatform].FindRef(ShaderMapId);
	check(Result == nullptr || (!Result->bDeletedThroughDeferredCleanup && Result->bRegistered) );
	return Result;
}

#if WITH_EDITOR
void FMaterialShaderMap::GetAllOutdatedTypes(TArray<const FShaderType*>& OutdatedShaderTypes, TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes)
{
#if ALLOW_SHADERMAP_DEBUG_DATA
	FScopeLock AllMatSMAccess(&AllMaterialShaderMapsGuard);
	for (const FMaterialShaderMap* ShaderMap : AllMaterialShaderMaps)
	{
		ShaderMap->GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
	}
#endif
}

TRACE_DECLARE_INT_COUNTER(Shaders_FMaterialShaderMapDDCRequests, TEXT("Shaders/FMaterialShaderMap/DDCRequests"));
TRACE_DECLARE_INT_COUNTER(Shaders_FMaterialShaderMapDDCHits, TEXT("Shaders/FMaterialShaderMap/DDCHits"));
TRACE_DECLARE_MEMORY_COUNTER(Shaders_FMaterialShaderMapDDCBytesReceived, TEXT("Shaders/FMaterialShaderMap/DDCBytesRecieved"));
TRACE_DECLARE_MEMORY_COUNTER(Shaders_FMaterialShaderMapDDCBytesSent, TEXT("Shaders/FMaterialShaderMap/DDCBytesSent"));

void FMaterialShaderMap::LoadFromDerivedDataCache(const FMaterial* Material, const FMaterialShaderMapId& ShaderMapId, EShaderPlatform InPlatform, const ITargetPlatform* TargetPlatform, TRefCountPtr<FMaterialShaderMap>& InOutShaderMap, FString& OutDDCKeyDesc)
{
	InOutShaderMap = BeginLoadFromDerivedDataCache(Material, ShaderMapId, InPlatform, TargetPlatform, InOutShaderMap, OutDDCKeyDesc)->Get();
}

TSharedRef<FMaterialShaderMap::FAsyncLoadContext> FMaterialShaderMap::BeginLoadFromDerivedDataCache(const FMaterial* Material, const FMaterialShaderMapId& ShaderMapId, EShaderPlatform InPlatform, const ITargetPlatform* TargetPlatform, TRefCountPtr<FMaterialShaderMap>& InShaderMap, FString& OutDDCKeyDesc)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialShaderMap::BeginLoadFromDerivedDataCache);
	using namespace UE::DerivedData;

	struct FMaterialShaderMapAsyncLoadContext : public FMaterialShaderMap::FAsyncLoadContext
	{
		FString	        DataKey;
		FSharedBuffer   CachedData;
		EShaderPlatform Platform;
		FString         AssetName;
		FRequestOwner   RequestOwner{UE::DerivedData::EPriority::Normal};
		TRefCountPtr<FMaterialShaderMap> ShaderMap;

		bool IsReady() const override
		{
			return RequestOwner.Poll();
		}

		TRefCountPtr<FMaterialShaderMap> Get() override
		{
			// Make sure the async work is complete
			if (!RequestOwner.Poll())
			{
				COOK_STAT(auto Timer = MaterialShaderCookStats::UsageStats.TimeAsyncWait());
				COOK_STAT(Timer.TrackCyclesOnly());
				RequestOwner.Wait();
			}

			TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialShaderMap::FinishLoadFromDerivedDataCache);
			COOK_STAT(auto Timer = MaterialShaderCookStats::UsageStats.TimeSyncWork());

			if (CachedData)
			{
				TRACE_COUNTER_INCREMENT(Shaders_FMaterialShaderMapDDCHits);
				TRACE_COUNTER_ADD(Shaders_FMaterialShaderMapDDCBytesReceived, int64(CachedData.GetSize()));
				COOK_STAT(Timer.AddHit(int64(CachedData.GetSize())));
				ShaderMap = new FMaterialShaderMap();
				FMemoryReaderView Ar(CachedData, /*bIsPersistent*/ true);

				// Deserialize from the cached data
				ShaderMap->Serialize(Ar);
				//InOutShaderMap->RegisterSerializedShaders(false);

				const FString InDataKey = GetMaterialShaderMapKeyString(ShaderMap->GetShaderMapId(), Platform);
				checkf(InDataKey == DataKey, TEXT("Data deserialized from the DDC would need a different DDC key!"));
				//checkf(InOutShaderMap->GetShaderMapId() == Context.ShaderMapId, TEXT("Shadermap data deserialized from the DDC does not match the ID we used to build the key!"));

				// Register in the global map
				ShaderMap->Register(Platform);

				GShaderCompilerStats->AddDDCHit(1);

				FCacheKey DdcKey = GetMaterialShaderMapKey(DataKey);
				UE_LOG(LogMaterial, Verbose, TEXT("Loaded shaders for %s from DDC (key hash: %s)"), *AssetName, *LexToString(DdcKey.Hash));
			}
			else
			{
				TRACE_COUNTER_INCREMENT(Shaders_FMaterialShaderMapDDCRequests);
				// We should be build the data later, and we can track that the resource was built there when we push it to the DDC.
				COOK_STAT(Timer.TrackCyclesOnly());

				GShaderCompilerStats->AddDDCMiss(1);
			}

			return ShaderMap;
		}
	};

	TSharedRef<FMaterialShaderMapAsyncLoadContext> Result = MakeShared<FMaterialShaderMapAsyncLoadContext>();

	if (InShaderMap != nullptr)
	{
		check(InShaderMap->GetShaderPlatform() == InPlatform);
		// If the shader map was non-NULL then it was found in memory but is incomplete, attempt to load the missing entries from memory
		InShaderMap->LoadMissingShadersFromMemory(Material);

		Result->ShaderMap = InShaderMap;
	}
	else
	{
		// Shader map was not found in memory, try to load it from the DDC
		STAT(double MaterialDDCTime = 0);
		{
			SCOPE_SECONDS_COUNTER(MaterialDDCTime);
			COOK_STAT(auto Timer = MaterialShaderCookStats::UsageStats.TimeSyncWork());
			COOK_STAT(Timer.TrackCyclesOnly());

			Result->DataKey = GetMaterialShaderMapKeyString(ShaderMapId, InPlatform);
			FCacheKey CacheKey = GetMaterialShaderMapKey(Result->DataKey);
			OutDDCKeyDesc = LexToString(CacheKey.Hash);

			if (UNLIKELY(ShouldDumpShaderDDCKeys()))
			{
				const FString FileName = FString::Printf(TEXT("%s-%s-%s.txt"), *Material->GetAssetName(), *LexToString(ShaderMapId.FeatureLevel), *LexToString(ShaderMapId.QualityLevel));
				DumpShaderDDCKeyToFile(InPlatform, ShaderMapId.LayoutParams.WithEditorOnly(), FileName, Result->DataKey);
			}

			if (Material->IsDefaultMaterial() && Material->GetMaterialDomain() == EMaterialDomain::MD_Surface)
			{
				FString SpecialEngineDDCKey = Result->DataKey;
				SpecialEngineDDCKey.RemoveFromEnd(TEXT("\n"));
				UE_LOG(LogMaterial, Log, TEXT("%s-%s-%s: %s"), *Material->GetAssetName(), *LexToString(ShaderMapId.FeatureLevel), *LexToString(ShaderMapId.QualityLevel), *SpecialEngineDDCKey);
			}

			bool bCheckCache = true;

			// If NoShaderDDC then don't check for a material the first time we encounter it to simulate
			// a cold DDC
			static bool bNoShaderDDC =
				FParse::Param(FCommandLine::Get(), TEXT("noshaderddc")) || 
				FParse::Param(FCommandLine::Get(), TEXT("nomaterialshaderddc"));

			if (bNoShaderDDC)
			{
				static TSet<uint32> SeenKeys;

				const uint32 KeyHash = GetTypeHash(Result->DataKey);

				if (!SeenKeys.Contains(KeyHash))
				{
					bCheckCache = false;
					SeenKeys.Add(KeyHash);
				}
			}

			// Do not check the DDC if the material isn't persistent, because
			//   - this results in a lot of DDC requests when editing materials which are almost always going to return nothing.
			//   - since the get call is synchronous, this can cause a hitch if there's network latency
			if (bCheckCache && Material->IsPersistent())
			{
				FCacheGetValueRequest Request;
				Request.Name = GetMaterialShaderMapName(Material->GetFullPath(), ShaderMapId, InPlatform);
				Request.Key = GetMaterialShaderMapKey(Result->DataKey);
				Result->AssetName = Material->GetAssetName();
				Result->Platform = InPlatform;

				GetCache().GetValue({Request}, Result->RequestOwner, [Result](FCacheGetValueResponse&& Response)
				{
					Result->CachedData = Response.Value.GetData().Decompress();
					// This callback might hold the last reference to Result, which owns RequestOwner, so
					// we must not cancel in the Owner's destructor; Canceling in a callback will deadlock.
					Result->RequestOwner.KeepAlive();
				});
			}
		}
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_DDCLoading,(float)MaterialDDCTime);
	}

	return Result;
}

void FMaterialShaderMap::SaveToDerivedDataCache()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialShaderMap::SaveToDerivedDataCache);
	COOK_STAT(auto Timer = MaterialShaderCookStats::UsageStats.TimeSyncWork());
	TArray64<uint8> SaveData;
	FMemoryWriter64 Ar(SaveData, true);
	Serialize(Ar);

	TRACE_COUNTER_ADD(Shaders_FMaterialShaderMapDDCBytesSent, SaveData.Num());
	COOK_STAT(Timer.AddMiss(SaveData.Num()));

	const FString DataKey = GetMaterialShaderMapKeyString(ShaderMapId, GetShaderPlatform());

	using namespace UE::DerivedData;
	FCachePutValueRequest Request;
	Request.Name = GetMaterialShaderMapName(GetMaterialPath(), ShaderMapId, GetShaderPlatform());
	Request.Key = GetMaterialShaderMapKey(DataKey);

	UE_LOG(LogMaterial, Verbose, TEXT("Saved shaders for %s to DDC (key hash: %s)"), *Request.Name, *LexToString(Request.Key.Hash));
	UE_LOG(LogMaterial, VeryVerbose, TEXT("Full DDC data key for %s: %s"), *Request.Name, *DataKey);

	if (!CVarMaterialShaderMapDump->GetString().IsEmpty() && CVarMaterialShaderMapDump->GetString() == GetMaterialPath())
	{
		TStringBuilder<256> Path;
		FPathViews::Append(Path, FPaths::ProjectSavedDir(), TEXT("MaterialShaderMaps"), TEXT(""));
		Path << Request.Key.Hash << ".txt";
		FArchive* DumpAr = IFileManager::Get().CreateFileWriter(*Path, FILEWRITE_Silent);
		if (DumpAr)
		{
			FTCHARToUTF8 Converter(*ToString());
			DumpAr->Serialize(const_cast<UTF8CHAR*>(reinterpret_cast<const UTF8CHAR*>(Converter.Get())), Converter.Length());
		}
	}

	Request.Value = FValue::Compress(MakeSharedBufferFromArray(MoveTemp(SaveData)));
	FRequestOwner AsyncOwner(EPriority::Normal);
	FRequestBarrier AsyncBarrier(AsyncOwner);
	GetCache().PutValue({Request}, AsyncOwner);
	AsyncOwner.KeepAlive();
}
#endif // WITH_EDITOR

TArray<uint8>* FMaterialShaderMap::BackupShadersToMemory()
{
	TArray<uint8>* SavedShaderData = new TArray<uint8>();
#if 0
	FMemoryWriter Ar(*SavedShaderData);
	
	for (FMeshMaterialShaderMap* MeshShaderMap : Content->OrderedMeshShaderMaps)
	{
		if (MeshShaderMap)
		{
			// Serialize data needed to handle shader key changes in between the save and the load of the FShaders
			const bool bHandleShaderKeyChanges = true;
			MeshShaderMap->SerializeInline(Ar, true, bHandleShaderKeyChanges, false);
			MeshShaderMap->Empty();
		}
	}

	Content->SerializeInline(Ar, true, true, false);
	Content->Empty();
#endif
	check(false);
	return SavedShaderData;
}

void FMaterialShaderMap::RestoreShadersFromMemory(const TArray<uint8>& ShaderData)
{
	check(false);
#if 0
	FMemoryReader Ar(ShaderData);

	for(FMeshMaterialShaderMap* MeshShaderMap : Content->OrderedMeshShaderMaps)
	{
		if (MeshShaderMap)
		{
			// Use the serialized shader key data to detect when the saved shader is no longer valid and skip it
			const bool bHandleShaderKeyChanges = true;
			MeshShaderMap->SerializeInline(Ar, true, bHandleShaderKeyChanges, false);
			//MeshShaderMaps[Index].RegisterSerializedShaders(false);
		}
	}

	Content->SerializeInline(Ar, true, true, false);
	//RegisterSerializedShaders(false);
#endif
}

void FMaterialShaderMap::SaveForRemoteRecompile(FArchive& Ar, const TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap> > >& CompiledShaderMaps)
{
	// now we serialize a map (for each material), but without inline the resources, since they are above
	int32 MapSize = CompiledShaderMaps.Num();
	Ar << MapSize;

	for (TMap<FString, TArray<TRefCountPtr<FMaterialShaderMap> > >::TConstIterator It(CompiledShaderMaps); It; ++It)
	{
		const TArray<TRefCountPtr<FMaterialShaderMap> >& ShaderMapArray = It.Value();

		FString MaterialName = It.Key();
		Ar << MaterialName;

		int32 NumShaderMaps = ShaderMapArray.Num();
		Ar << NumShaderMaps;

		for (int32 Index = 0; Index < ShaderMapArray.Num(); Index++)
		{
			FMaterialShaderMap* ShaderMap = ShaderMapArray[Index];
			if (ShaderMap)
			{
				uint8 bIsValid = 1;
				Ar << bIsValid;
				ShaderMap->Serialize(Ar, false);
			}
			else
			{
				uint8 bIsValid = 0;
				Ar << bIsValid;
			}
		}
	}
}

void FMaterialShaderMap::LoadForRemoteRecompile(FArchive& Ar, EShaderPlatform ShaderPlatform, TArray<UMaterialInterface*>& OutLoadedMaterials)
{
	FString MaterialName;
	TArray<TRefCountPtr<FMaterialShaderMap>> LoadedShaderMaps;

	int32 MapSize;
	Ar << MapSize;

	for (int32 MaterialIndex = 0; MaterialIndex < MapSize; MaterialIndex++)
	{
		MaterialName.Reset();
		Ar << MaterialName;

		int32 NumShaderMaps = 0;
		Ar << NumShaderMaps;

		LoadedShaderMaps.Reset();
		for (int32 ShaderMapIndex = 0; ShaderMapIndex < NumShaderMaps; ShaderMapIndex++)
		{
			uint8 bIsValid = 0;
			Ar << bIsValid;

			if (bIsValid)
			{
				FMaterialShaderMap* ShaderMap = new FMaterialShaderMap();

				// serialize the id and the material shader map
				ShaderMap->Serialize(Ar, false);

				// Register in the global map
				ShaderMap->Register(ShaderPlatform);

				LoadedShaderMaps.Add(ShaderMap);
			}
		}

		UMaterialInterface* MatchingMaterial = FindObject<UMaterialInterface>(nullptr, *MaterialName);
		if (!MatchingMaterial)
		{
			continue;
		}
		OutLoadedMaterials.Add(MatchingMaterial);

		// Assign in two passes: first pass for shader maps with unspecified quality levels,
		// Second pass for shader maps with a specific quality level
		for (int32 PassIndex = 0; PassIndex < 2; PassIndex++)
		{
			for (int32 ShaderMapIndex = 0; ShaderMapIndex < LoadedShaderMaps.Num(); ShaderMapIndex++)
			{
				FMaterialShaderMap* LoadedShaderMap = LoadedShaderMaps[ShaderMapIndex];
	
				if (LoadedShaderMap->GetShaderPlatform() == ShaderPlatform 
					&& LoadedShaderMap->GetShaderMapId().FeatureLevel == GetMaxSupportedFeatureLevel(ShaderPlatform))
				{
					EMaterialQualityLevel::Type LoadedQualityLevel = LoadedShaderMap->GetShaderMapId().QualityLevel;

					for (int32 QualityLevelIndex = 0; QualityLevelIndex < EMaterialQualityLevel::Num; QualityLevelIndex++)
					{
						// First pass: assign shader maps with unspecified quality levels to all material resources
						if ((PassIndex == 0 && LoadedQualityLevel == EMaterialQualityLevel::Num)
							// Second pass: assign shader maps with a specified quality level to only the appropriate material resource
							|| (PassIndex == 1 && QualityLevelIndex == LoadedQualityLevel))
						{
							FMaterialResource* MaterialResource = MatchingMaterial->GetMaterialResource(GetMaxSupportedFeatureLevel(ShaderPlatform), (EMaterialQualityLevel::Type)QualityLevelIndex);
							MaterialResource->SetGameThreadShaderMap(LoadedShaderMap);
						}
					}
				}
			}
		}
	}
}

void FMaterialShaderMapContent::Finalize(const FShaderMapResourceCode* Code)
{
	FSHA1 Hasher;
	FShaderMapContent::Finalize(Code);
	UpdateHash(Hasher);

	for (FMeshMaterialShaderMap* MeshShaderMap : OrderedMeshShaderMaps)
	{
		MeshShaderMap->Finalize(Code);
		MeshShaderMap->UpdateHash(Hasher);
	}

	Hasher.Final();
	Hasher.GetHash(ShaderContentHash.Hash);
}

#if WITH_EDITOR

static TMap<uint32, TRefCountPtr<FMaterialShaderMap>>& GetCompilingShaderMapLookup()
{
	static TMap<uint32, TRefCountPtr<FMaterialShaderMap>> Map;
	return Map;
}

static FRWLock GCompilingShaderMapLock;

FMaterialShaderMap* FMaterialShaderMap::FindCompilingShaderMap(uint32 CompilingId)
{
	FReadScopeLock Locker(GCompilingShaderMapLock);
	return GetCompilingShaderMapLookup().FindRef(CompilingId);
}

uint32 FMaterialShaderMap::AcquireCompilingId(const TRefCountPtr<FSharedShaderCompilerEnvironment>& InMaterialEnvironment)
{
	check(IsInGameThread());
	if (CompilingId == 0u)
	{
		PendingCompilerEnvironment = InMaterialEnvironment;
		FWriteScopeLock Locker(GCompilingShaderMapLock);
		CompilingId = FShaderCommonCompileJob::GetNextJobId();
		GetCompilingShaderMapLookup().Add(CompilingId, this);
	}

	check(PendingCompilerEnvironment == InMaterialEnvironment);

	return CompilingId;
}

void FMaterialShaderMap::ReleaseCompilingId()
{
	check(IsInGameThread());
	if (CompilingId != 0u)
	{
		FWriteScopeLock Locker(GCompilingShaderMapLock);
		check(CompilingMaterialDependencies.Num() == 0);
		verify(GetCompilingShaderMapLookup().Remove(CompilingId) == 1);
		CompilingId = 0;
	}

	PendingCompilerEnvironment.SafeRelease();
}

void FMaterialShaderMap::AddCompilingDependency(FMaterial* Material)
{
	CompilingMaterialDependencies.AddUnique(Material);
	// if any of our dependencies is persistent, we're persistent
	bIsPersistent |= Material->IsPersistent();
}

void FMaterialShaderMap::RemoveCompilingDependency(FMaterial* Material)
{
	check(CompilingId != 0u);
	const int32 NumRemoved = CompilingMaterialDependencies.RemoveSingle(Material);
	check(NumRemoved == 1);
	checkSlow(!CompilingMaterialDependencies.Contains(Material));
	if (CompilingMaterialDependencies.Num() == 0)
	{
		const TArray<int32> CompilingIdsToCancel = { (int32)CompilingId };
		GShaderCompilingManager->CancelCompilation(GetFriendlyName(), CompilingIdsToCancel);

		ReleaseCompilingId();
	}
}

int32 FMaterialShaderMap::SubmitCompileJobs(uint32 CompilingShaderMapId,
	const FMaterial* Material,
	const TRefCountPtr<FSharedShaderCompilerEnvironment>& MaterialEnvironment,
	EShaderCompileJobPriority InPriority) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialShaderMap::SubmitCompileJobs);
	check(CompilingShaderMapId != 0u);
	check(MaterialEnvironment);

	TArray<FShaderCommonCompileJobPtr> CompileJobs;

	uint32 NumShaders = 0;
	uint32 NumVertexFactories = 0;

	const EShaderPlatform ShaderPlatform = GetShaderPlatform();
	const EShaderPermutationFlags LocalPermutationFlags = ShaderMapId.GetPermutationFlags();
	const FMaterialShaderParameters MaterialParameters(Material);
	const FMaterialShaderMapLayout& Layout = AcquireMaterialShaderMapLayout(ShaderPlatform, LocalPermutationFlags, MaterialParameters);

#if ALLOW_SHADERMAP_DEBUG_DATA
	FString DebugExtensionStr(TEXT(""));
	FString DebugDescriptionStr(TEXT(""));

	// DebugExtension and Description make the jobs unnecessarily different. Use them if the job cache is disabled or shader dev mode is on
	static IConsoleVariable* CVarCacheEnabled = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShaderCompiler.JobCache"));
	static IConsoleVariable* CVarShaderDevMode = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ShaderDevelopmentMode"));
	if ((CVarCacheEnabled && CVarCacheEnabled->GetInt() == 0) || (CVarShaderDevMode && CVarShaderDevMode->GetInt() != 0))
	{
		DebugExtensionStr = FString::Printf(TEXT("_%08x%08x"), ShaderMapId.BaseMaterialId.A, ShaderMapId.BaseMaterialId.B);
		DebugDescriptionStr = GetDebugDescription();
	}
	const TCHAR* DebugExtension = *DebugExtensionStr;
	const TCHAR* DebugDescription = *DebugDescriptionStr;

#else
	const TCHAR* DebugExtension = nullptr;
	const TCHAR* DebugDescription = nullptr;
#endif

	// Iterate over all vertex factory types.
	for (const FMeshMaterialShaderMapLayout& MeshLayout : Layout.MeshShaderMaps)
	{
		const FMeshMaterialShaderMap* MeshShaderMap = GetMeshShaderMap(MeshLayout.VertexFactoryType);

		uint32 NumShadersPerVF = 0;
		TSet<FString> ShaderTypeNames;

		// Do not submit jobs for the shader types that are included in some pipeline stages if that pipeline is optimzing unused outputs.
		// In such a case, these shaders should not be used runtime anymore
		FPipelinedShaderFilter PipelinedShaderFilter(ShaderPlatform, MeshLayout.ShaderPipelines);

		// Iterate over all mesh material shader types.
		TMap<TShaderTypePermutation<const FShaderType>, FShaderCompileJob*> SharedShaderJobs;
		for (const FShaderLayoutEntry& Shader : MeshLayout.Shaders)
		{
			FMeshMaterialShaderType* ShaderType = static_cast<FMeshMaterialShaderType*>(Shader.ShaderType);
			if (!Material->ShouldCache(ShaderPlatform, ShaderType, MeshLayout.VertexFactoryType))
			{
				continue;
			}

			// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
			check(ShaderMapId.ContainsVertexFactoryType(MeshLayout.VertexFactoryType));
			check(ShaderMapId.ContainsShaderType(ShaderType, kUniqueShaderPermutationId));

			NumShadersPerVF++;
			// only compile the shader if we don't already have it and it is not a pipelined one
			if (!PipelinedShaderFilter.IsPipelinedType(ShaderType) && (!MeshShaderMap || !MeshShaderMap->HasShader(ShaderType, Shader.PermutationId)))
			{
				// Compile this mesh material shader for this material and vertex factory type.
				ShaderType->BeginCompileShader(InPriority,
					CompilingShaderMapId,
					Shader.PermutationId,
					ShaderPlatform,
					LocalPermutationFlags,
					Material,
					ShaderMapId,
					MaterialEnvironment,
					MeshLayout.VertexFactoryType,
					CompileJobs,
					DebugDescription,
					DebugExtension
				);
				//TShaderTypePermutation<const FShaderType> ShaderTypePermutation(ShaderType, Shader.PermutationId);
				//check(!SharedShaderJobs.Find(ShaderTypePermutation));
				//SharedShaderJobs.Add(ShaderTypePermutation, Job);
			}
		}

		// Now the pipeline jobs; if it's a shareable pipeline, do not add duplicate jobs
		for (FShaderPipelineType* Pipeline : MeshLayout.ShaderPipelines)
		{
			if (!Material->ShouldCachePipeline(ShaderPlatform, Pipeline, MeshLayout.VertexFactoryType))
			{
				continue;
			}

			auto& StageTypes = Pipeline->GetStages();

			// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
			check(ShaderMapId.ContainsShaderPipelineType(Pipeline));
			check(ShaderMapId.ContainsVertexFactoryType(MeshLayout.VertexFactoryType));

			if (Pipeline->ShouldOptimizeUnusedOutputs(ShaderPlatform))
			{
				NumShadersPerVF += StageTypes.Num();

				for (auto* ShaderType : StageTypes)
				{
					// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
					check(ShaderMapId.ContainsShaderType(ShaderType, kUniqueShaderPermutationId));
				}

				// Make a pipeline job with all the stages
				FMeshMaterialShaderType::BeginCompileShaderPipeline(InPriority,
					CompilingShaderMapId,
					kUniqueShaderPermutationId,
					ShaderPlatform,
					LocalPermutationFlags,
					Material,
					ShaderMapId,
					MaterialEnvironment,
					MeshLayout.VertexFactoryType,
					Pipeline,
					CompileJobs,
					DebugDescription,
					DebugExtension);
			}
			else
			{
				// If sharing shaders amongst pipelines, add this pipeline as a dependency of an existing job
				for (const FShaderType* ShaderType : StageTypes)
				{
					TShaderTypePermutation<const FShaderType> ShaderTypePermutation(ShaderType, kUniqueShaderPermutationId);
					FShaderCompileJob** Job = SharedShaderJobs.Find(ShaderTypePermutation);
					checkf(Job, TEXT("Couldn't find existing shared job for mesh shader %s on pipeline %s!"), ShaderType->GetName(), Pipeline->GetName());
					auto* SingleJob = (*Job)->GetSingleShaderJob();
					auto& PipelinesToShare = SingleJob->SharingPipelines.FindOrAdd(MeshLayout.VertexFactoryType);
					check(!PipelinesToShare.Contains(Pipeline));
					PipelinesToShare.Add(Pipeline);
				}
			}
		}

		NumShaders += NumShadersPerVF;
		if (NumShadersPerVF > 0)
		{
			UE_LOG(LogShaders, Verbose, TEXT("			%s - %u shaders"), MeshLayout.VertexFactoryType->GetName(), NumShadersPerVF);
			NumVertexFactories++;
		}
	}

	// Do not submit jobs for the shader types that are included in some pipeline stages if that pipeline is optimzing unused outputs.
	// In such a case, these shaders should not be used runtime anymore
	FPipelinedShaderFilter PipelinedShaderFilter(ShaderPlatform, Layout.ShaderPipelines);

	// Iterate over all material shader types.
	TMap<TShaderTypePermutation<const FShaderType>, FShaderCompileJob*> SharedShaderJobs;
	for (const FShaderLayoutEntry& Shader : Layout.Shaders)
	{
		FMaterialShaderType* ShaderType = static_cast<FMaterialShaderType*>(Shader.ShaderType);
		if (!Material->ShouldCache(ShaderPlatform, ShaderType, nullptr))
		{
			continue;
		}

		// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
		check(ShaderMapId.ContainsShaderType(ShaderType, kUniqueShaderPermutationId));

		// Compile this material shader for this material.
		TArray<FString> ShaderErrors;

		// Only compile the shader if we don't already have it
		if (!PipelinedShaderFilter.IsPipelinedType(ShaderType) && !GetContent()->HasShader(ShaderType, Shader.PermutationId))
		{
			ShaderType->BeginCompileShader(InPriority,
				CompilingShaderMapId,
				Shader.PermutationId,
				Material,
				ShaderMapId,
				MaterialEnvironment,
				ShaderPlatform,
				LocalPermutationFlags,
				CompileJobs,
				DebugDescription,
				DebugExtension
			);

			//TShaderTypePermutation<const FShaderType> ShaderTypePermutation(ShaderType, Shader.PermutationId);
			//check(!SharedShaderJobs.Find(ShaderTypePermutation));
			//SharedShaderJobs.Add(ShaderTypePermutation, Job);
		}
		NumShaders++;
	}

	if (RHISupportsShaderPipelines(ShaderPlatform))
	{
		for (FShaderPipelineType* Pipeline : Layout.ShaderPipelines)
		{
			if (!Material->ShouldCachePipeline(ShaderPlatform, Pipeline, nullptr))
			{
				continue;
			}

			auto& StageTypes = Pipeline->GetStages();

			// Verify that the shader map Id contains inputs for any shaders that will be put into this shader map
			check(ShaderMapId.ContainsShaderPipelineType(Pipeline));

			if (Pipeline->ShouldOptimizeUnusedOutputs(ShaderPlatform))
			{
				NumShaders += StageTypes.Num();
				FMaterialShaderType::BeginCompileShaderPipeline(InPriority,
					CompilingShaderMapId,
					ShaderPlatform,
					LocalPermutationFlags,
					Material,
					ShaderMapId,
					MaterialEnvironment,
					Pipeline,
					CompileJobs,
					DebugDescription,
					DebugExtension);
			}
			else
			{
				// If sharing shaders amongst pipelines, add this pipeline as a dependency of an existing job
				for (const FShaderType* ShaderType : StageTypes)
				{
					TShaderTypePermutation<const FShaderType> ShaderTypePermutation(ShaderType, kUniqueShaderPermutationId);
					FShaderCompileJob** Job = SharedShaderJobs.Find(ShaderTypePermutation);
					checkf(Job, TEXT("Couldn't find existing shared job for material shader %s on pipeline %s!"), ShaderType->GetName(), Pipeline->GetName());
					auto* SingleJob = (*Job)->GetSingleShaderJob();
					auto& PipelinesToShare = SingleJob->SharingPipelines.FindOrAdd(nullptr);
					check(!PipelinesToShare.Contains(Pipeline));
					PipelinesToShare.Add(Pipeline);
				}
			}
		}
	}

	UE_LOG(LogShaders, Verbose, TEXT("		%u Shaders among %u VertexFactories"), NumShaders, NumVertexFactories);

	GShaderCompilingManager->SubmitJobs(CompileJobs, Material->GetBaseMaterialPathName(), GetDebugDescription());

	return CompileJobs.Num();
}

/**
* Compiles the shaders for a material and caches them in this shader map.
* @param Material - The material to compile shaders for.
* @param InShaderMapId - the set of static parameters to compile for
* @param Platform - The platform to compile to
*/
void FMaterialShaderMap::Compile(
	FMaterial* Material,
	const FMaterialShaderMapId& InShaderMapId,
	const TRefCountPtr<FSharedShaderCompilerEnvironment>& MaterialEnvironment,
	const FMaterialCompilationOutput& InMaterialCompilationOutput,
	EShaderPlatform InPlatform,
	EMaterialShaderPrecompileMode PrecompileMode)
{
	if (FPlatformProperties::RequiresCookedData())
	{
		UE_LOG(LogMaterial, Fatal, TEXT("Trying to compile %s at run-time, which is not supported on consoles!"), *Material->GetFriendlyName());
		return;
	}

	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialShaderMap::Compile);
	check(!Material->bContainsInlineShaders);

	// Assign a unique identifier so that shaders from this shader map can be associated with it after a deferred compile
	AcquireCompilingId(MaterialEnvironment);

#if DEBUG_INFINITESHADERCOMPILE
	UE_LOG(LogTemp, Display, TEXT("Added material ShaderMap 0x%08X%08X with Material 0x%08X%08X to ShaderMapsBeingCompiled"), (int)((int64)(this) >> 32), (int)((int64)(this)), (int)((int64)(Material) >> 32), (int)((int64)(Material)));
#endif  

	FMaterialShaderMapContent* NewContent = new FMaterialShaderMapContent(InPlatform);
	NewContent->MaterialCompilationOutput = InMaterialCompilationOutput;
	AssignContent(NewContent);

	ShaderMapId = InShaderMapId;
	bIsPersistent = Material->IsPersistent();

#if ALLOW_SHADERMAP_DEBUG_DATA
	// Store the material name for debugging purposes.
	// Note: Material instances with static parameters will have the same FriendlyName for their shader maps!
	NewContent->FriendlyName = Material->GetFriendlyName();
	NewContent->MaterialPath = Material->GetBaseMaterialPathName();

	// Log debug information about the material being compiled.
	const FString MaterialUsage = Material->GetMaterialUsageDescription();
	FString WorkingDebugDescription = FString::Printf(
		TEXT("Compiling %s: Platform=%s, Usage=%s"),
		*NewContent->FriendlyName,
		*FDataDrivenShaderPlatformInfo::GetName(InPlatform).ToString(),
		*MaterialUsage
	);
	for (int32 StaticSwitchIndex = 0; StaticSwitchIndex < ShaderMapId.GetStaticSwitchParameters().Num(); ++StaticSwitchIndex)
	{
		const FStaticSwitchParameter& StaticSwitchParameter = ShaderMapId.GetStaticSwitchParameters()[StaticSwitchIndex];
		WorkingDebugDescription += FString::Printf(
			TEXT(", StaticSwitch'%s'=%s"),
			*StaticSwitchParameter.ParameterInfo.ToString(),
			StaticSwitchParameter.Value ? TEXT("True") : TEXT("False")
		);
	}
	for (int32 StaticMaskIndex = 0; StaticMaskIndex < ShaderMapId.GetStaticComponentMaskParameters().Num(); ++StaticMaskIndex)
	{
		const FStaticComponentMaskParameter& StaticComponentMaskParameter = ShaderMapId.GetStaticComponentMaskParameters()[StaticMaskIndex];
		WorkingDebugDescription += FString::Printf(
			TEXT(", StaticMask'%s'=%s%s%s%s"),
			*StaticComponentMaskParameter.ParameterInfo.ToString(),
			StaticComponentMaskParameter.R ? TEXT("R") : TEXT(""),
			StaticComponentMaskParameter.G ? TEXT("G") : TEXT(""),
			StaticComponentMaskParameter.B ? TEXT("B") : TEXT(""),
			StaticComponentMaskParameter.A ? TEXT("A") : TEXT("")
		);
	}
	for (int32 StaticLayerIndex = 0; StaticLayerIndex < ShaderMapId.GetTerrainLayerWeightParameters().Num(); ++StaticLayerIndex)
	{
		const FStaticTerrainLayerWeightParameter& StaticTerrainLayerWeightParameter = ShaderMapId.GetTerrainLayerWeightParameters()[StaticLayerIndex];
		WorkingDebugDescription += FString::Printf(
			TEXT(", StaticTerrainLayer'%s'=%s"),
			*StaticTerrainLayerWeightParameter.LayerName.ToString(),
			*FString::Printf(TEXT("Weightmap%u"), StaticTerrainLayerWeightParameter.WeightmapIndex)
		);
	}

	if (ShaderMapId.GetMaterialLayersId())
	{
		const FMaterialLayersFunctions::ID& MaterialLayersId = *ShaderMapId.GetMaterialLayersId();
		WorkingDebugDescription += TEXT("Layers:");
		bool StartWithComma = false;
		for (const auto &Layer : MaterialLayersId.LayerIDs)
		{
			WorkingDebugDescription += (StartWithComma ? TEXT(", ") : TEXT("")) + Layer.ToString();
			StartWithComma = true;
		}
		WorkingDebugDescription += TEXT(", Blends:");
		StartWithComma = false;
		for (const auto &Blend : MaterialLayersId.BlendIDs)
		{
			WorkingDebugDescription += (StartWithComma ? TEXT(", ") : TEXT("")) + Blend.ToString();
			StartWithComma = true;
		}
		WorkingDebugDescription += TEXT(", LayerStates:");
		StartWithComma = false;
		for (bool State : MaterialLayersId.LayerStates)
		{
			WorkingDebugDescription += (StartWithComma ? TEXT(", ") : TEXT(""));
			WorkingDebugDescription += (State ? TEXT("1") : TEXT("0"));
			StartWithComma = true;
		}
	}

	// If we aren't actually compiling shaders don't print the debug message that we are compiling shaders.
	if (PrecompileMode != EMaterialShaderPrecompileMode::None)
	{
		UE_LOG(LogShaders, Verbose, TEXT("	%s"), *WorkingDebugDescription);
	}
	NewContent->DebugDescription = *WorkingDebugDescription;
#else
	FString DebugExtension = "";
#endif 

	Material->SetCompilingShaderMap(this);

	// Register this shader map in the global map with the material's ID.
	Register(InPlatform);

	// Mark the shader map as not having been finalized with ProcessCompilationResults
	bCompilationFinalized = false;

	// Mark as not having been compiled
	bCompiledSuccessfully = false;

	if (PrecompileMode != EMaterialShaderPrecompileMode::None)
	{
		EShaderCompileJobPriority CompilePriority = EShaderCompileJobPriority::Low;
		if (PrecompileMode == EMaterialShaderPrecompileMode::Synchronous)
		{
			CompilePriority = EShaderCompileJobPriority::High;
		}
		else if (!Material->IsPersistent())
		{
			// Note: using Material->IsPersistent() to detect whether this is a preview material which should have higher priority over background compiling
			CompilePriority = EShaderCompileJobPriority::Normal;
		}

		// Material can filter out all our shader types, essentially preventing the compilation from happening, which can make the shadermap stuck in
		// "always being compiled" mode. If we find out that we submitted 0 jobs, consider compilation finished.
		// Note that the shader map can be shared between FMaterials. Here we assume that they all will filter the shader types the same
		// (and have no good way to check this).
		if (SubmitCompileJobs(CompilingId, Material, MaterialEnvironment, CompilePriority) == 0)
		{
			RemoveCompilingDependency(Material);
			if (CompilingId == 0u)
			{
				bCompilationFinalized = true;
				bCompiledSuccessfully = true;
				// create resource code even if it's empty (needed during the serialization and possibly other places).
				GetResourceCode();

#if WITH_EDITOR
				if (bIsPersistent)
				{
					SaveToDerivedDataCache();
				}
#endif
			}
		}
	}

	// Compile the shaders for this shader map now if the material is not deferring and deferred compiles are not enabled globally
	// If we're early in the startup we can save some time by compiling all special/default materials asynchronously, even if normally they are synchronous
	if (CompilingId != 0u && PrecompileMode == EMaterialShaderPrecompileMode::Synchronous && !PoolSpecialMaterialsCompileJobs())
	{
		TArray<int32> CurrentShaderMapId;
		CurrentShaderMapId.Add(CompilingId);
		GShaderCompilingManager->FinishCompilation(
			GetFriendlyName(),
			CurrentShaderMapId);
	}
}

static FHashedName GetPreprocessedSourceKey(const FVertexFactoryType* VertexFactoryType, const FShaderType* ShaderType, int32 PermutationId)
{
	if (VertexFactoryType)
	{
		return FHashedName(FString::Printf(TEXT("%s/%s/%d"), VertexFactoryType->GetName(), ShaderType->GetName(), PermutationId));
	}
	else
	{
		return FHashedName(FString::Printf(TEXT("%s/%d"), ShaderType->GetName(), PermutationId));
	}
}

FShader* FMaterialShaderMap::ProcessCompilationResultsForSingleJob(FShaderCompileJob* SingleJob, const FShaderPipelineType* ShaderPipeline, const FSHAHash& MaterialShaderMapHash)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialShaderMap::ProcessCompilationResultsForSingleJob);

	check(SingleJob);
	const FShaderCompileJob& CurrentJob = *SingleJob;
	check(CurrentJob.Id == CompilingId);

	GetResourceCode()->AddShaderCompilerOutput(CurrentJob.Output, CurrentJob.Key.ToString());

#if ALLOW_SHADERMAP_DEBUG_DATA
	CompileTime += SingleJob->Output.CompileTime;
#endif
	FShader* Shader = nullptr;
	if (CurrentJob.Key.VFType)
	{
		const FVertexFactoryType* VertexFactoryType = CurrentJob.Key.VFType;
		check(VertexFactoryType->IsUsedWithMaterials());
		FMeshMaterialShaderMap* MeshShaderMap = AcquireMeshShaderMap(VertexFactoryType);

		check(MeshShaderMap);
		const FMeshMaterialShaderType* MeshMaterialShaderType = CurrentJob.Key.ShaderType->GetMeshMaterialShaderType();
		check(MeshMaterialShaderType);
		Shader = MeshMaterialShaderType->FinishCompileShader(GetContent()->MaterialCompilationOutput.UniformExpressionSet, MaterialShaderMapHash, CurrentJob, ShaderPipeline, GetFriendlyName());
		check(Shader);
		if (!ShaderPipeline)
		{
			//check(!MeshShaderMap->HasShader(MeshMaterialShaderType, CurrentJob.PermutationId));
			Shader = MeshShaderMap->FindOrAddShader(MeshMaterialShaderType->GetHashedName(), CurrentJob.Key.PermutationId, Shader);
		}
	}
	else
	{
		const FMaterialShaderType* MaterialShaderType = CurrentJob.Key.ShaderType->GetMaterialShaderType();
		check(MaterialShaderType);
		Shader = MaterialShaderType->FinishCompileShader(GetContent()->MaterialCompilationOutput.UniformExpressionSet, MaterialShaderMapHash, CurrentJob, ShaderPipeline, GetFriendlyName());

		check(Shader);
		if (!ShaderPipeline)
		{
			//check(!GetContent()->HasShader(MaterialShaderType, CurrentJob.PermutationId));
			Shader = GetMutableContent()->FindOrAddShader(MaterialShaderType->GetHashedName(), CurrentJob.Key.PermutationId, Shader);
		}
	}

	// add shader source
	{
		// Keep the preprocessed source list sorted by a name constructed from VF/ShaderType/PermutationId and deduplicate entries
		const FHashedName Key = GetPreprocessedSourceKey(CurrentJob.Key.VFType, CurrentJob.Key.ShaderType, CurrentJob.Key.PermutationId);

		const int32 Index = Algo::LowerBoundBy(GetMutableContent()->ShaderProcessedSource, Key, [](const FMaterialProcessedSource& Value) { return Value.Name; });
		if (Index >= GetMutableContent()->ShaderProcessedSource.Num() || GetMutableContent()->ShaderProcessedSource[Index].Name != Key)
		{
			GetMutableContent()->ShaderProcessedSource.EmplaceAt(Index, Key, *CurrentJob.Output.OptionalFinalShaderSource);
		}
	}

	return Shader;
}

void FMaterialShaderMap::ProcessCompilationResults(const TArray<FShaderCommonCompileJobPtr>& InCompilationResults, int32& InOutJobIndex, float& TimeBudget)
{
	check(!bCompilationFinalized);

	double StartTime = FPlatformTime::Seconds();

	FSHAHash MaterialShaderMapHash;
	ShaderMapId.GetMaterialHash(MaterialShaderMapHash);

	do
	{
		auto& BaseJob = InCompilationResults[InOutJobIndex++];
		FShaderCompileJob* SingleJob = BaseJob->GetSingleShaderJob();
		if (SingleJob)
		{
			FShader* Shader = ProcessCompilationResultsForSingleJob(SingleJob, nullptr, MaterialShaderMapHash);
			for (const auto& Pair : SingleJob->SharingPipelines)
			{
				const FVertexFactoryType* VFType = Pair.Key;
				FShaderMapContent* ShaderMapForPipeline = GetMutableContent();
				if (VFType)
				{
					ShaderMapForPipeline = AcquireMeshShaderMap(VFType->GetHashedName());
				}

				for (const FShaderPipelineType* PipelineType : Pair.Value)
				{
					FShaderPipeline* Pipeline = ShaderMapForPipeline->GetShaderPipeline(PipelineType);
					if (!Pipeline)
					{
						Pipeline = new FShaderPipeline(PipelineType);
						ShaderMapForPipeline->AddShaderPipeline(Pipeline);
					}
					Pipeline->AddShader(Shader, SingleJob->Key.PermutationId);
				}
			}
		}
		else
		{
			auto* PipelineJob = BaseJob->GetShaderPipelineJob();
			check(PipelineJob);

			const FShaderPipelineCompileJob& CurrentJob = *PipelineJob;
			check(CurrentJob.Id == CompilingId);

			const FVertexFactoryType* VertexFactoryType = CurrentJob.StageJobs[0]->GetSingleShaderJob()->Key.VFType;
			FShaderPipeline* ShaderPipeline = new FShaderPipeline(CurrentJob.Key.ShaderPipeline);
			if (VertexFactoryType)
			{
				check(VertexFactoryType->IsUsedWithMaterials());
				FMeshMaterialShaderMap* MeshShaderMap = AcquireMeshShaderMap(VertexFactoryType);
				check(MeshShaderMap);
				ShaderPipeline = MeshShaderMap->FindOrAddShaderPipeline(ShaderPipeline);
			}
			else
			{
				ShaderPipeline = GetMutableContent()->FindOrAddShaderPipeline(ShaderPipeline);
			}

			for (int32 Index = 0; Index < CurrentJob.StageJobs.Num(); ++Index)
			{
				SingleJob = CurrentJob.StageJobs[Index]->GetSingleShaderJob();
				FShader* Shader = ProcessCompilationResultsForSingleJob(SingleJob, PipelineJob->Key.ShaderPipeline, MaterialShaderMapHash);
				Shader = ShaderPipeline->FindOrAddShader(Shader, SingleJob->Key.PermutationId);
				check(VertexFactoryType == CurrentJob.StageJobs[Index]->GetSingleShaderJob()->Key.VFType);
			}
			ShaderPipeline->Validate(CurrentJob.Key.ShaderPipeline);
		}

		double NewStartTime = FPlatformTime::Seconds();
		TimeBudget -= NewStartTime - StartTime;
		StartTime = NewStartTime;
	}
	while ((TimeBudget > 0.0f) && InOutJobIndex < InCompilationResults.Num());
}

#endif // WITH_EDITOR

class FMaterialShaderMapLayoutCache
{
public:
	static FMaterialShaderMapLayoutCache& Get()
	{
		static FMaterialShaderMapLayoutCache Instance;
		return Instance;
	}

	const FMaterialShaderMapLayout& AcquireLayout(EShaderPlatform Platform, EShaderPermutationFlags Flags, const FMaterialShaderParameters& MaterialParameters)
	{
		const uint64 ParameterHash = CityHash64WithSeed((char*)&MaterialParameters, sizeof(MaterialParameters), (uint64)Platform | ((uint64)Flags << 32));

		FMaterialShaderMapLayout* Layout = nullptr;
		{
			FReadScopeLock Locker(LayoutLock);
			Layout = FindLayout(ParameterHash);
		}

		if (!Layout)
		{
			FWriteScopeLock Locker(LayoutLock);
			// Need to check for existing index again once we've taken the write-lock
			// TODO maybe better to create the layout without any lock, then discard if another thread created the same layout
			Layout = FindLayout(ParameterHash);
			if (!Layout)
			{
				Layout = new FMaterialShaderMapLayout();
				CreateLayout(*Layout, Platform, Flags, MaterialParameters);
				Layout->Platform = Platform;

				const int32 Index = ShaderMapLayouts.Add(TUniquePtr<FMaterialShaderMapLayout>(Layout));
				MaterialParameterHashes.Add(ParameterHash);
				MaterialShaderParameters.Add(MaterialParameters);
				ShaderMapHashTable.Add(ParameterHash, Index);

				check(MaterialParameterHashes.Num() == ShaderMapLayouts.Num());
				check(MaterialShaderParameters.Num() == ShaderMapLayouts.Num());
			}
		}

		check(Layout);
		return *Layout;
	}

private:
	FMaterialShaderMapLayout* FindLayout(uint64 ParameterHash)
	{
		for (int32 Index = ShaderMapHashTable.First(ParameterHash); ShaderMapHashTable.IsValid(Index); Index = ShaderMapHashTable.Next(Index))
		{
			if (MaterialParameterHashes[Index] == ParameterHash)
			{
				return ShaderMapLayouts[Index].Get();
			}
		}
		return nullptr;
	}

	void CreateLayout(FMaterialShaderMapLayout& Layout, EShaderPlatform Platform, EShaderPermutationFlags Flags, const FMaterialShaderParameters& MaterialParameters)
	{
		SCOPED_LOADTIMER(FMaterialShaderMapLayoutCache_CreateLayout);

		const TArray<FShaderType*>& SortedMaterialShaderTypes = FShaderType::GetSortedTypes(FShaderType::EShaderTypeForDynamicCast::Material);
		const TArray<FShaderType*>& SortedMeshMaterialShaderTypes = FShaderType::GetSortedTypes(FShaderType::EShaderTypeForDynamicCast::MeshMaterial);
		const TArray<FShaderPipelineType*>& SortedMaterialPipelineTypes = FShaderPipelineType::GetSortedTypes(FShaderType::EShaderTypeForDynamicCast::Material);
		const TArray<FShaderPipelineType*>& SortedMeshMaterialPipelineTypes = FShaderPipelineType::GetSortedTypes(FShaderType::EShaderTypeForDynamicCast::MeshMaterial);

		FSHA1 Hasher;

		for (FShaderType* BaseShaderType : SortedMaterialShaderTypes)
		{
			// Find this shader type in the material's shader map.
			FMaterialShaderType* ShaderType = static_cast<FMaterialShaderType*>(BaseShaderType);
			const int32 PermutationCount = ShaderType->GetPermutationCount();
			for (int32 PermutationId = 0; PermutationId < PermutationCount; ++PermutationId)
			{
				if (ShaderType->ShouldCompilePermutation(Platform, MaterialParameters, PermutationId, Flags))
				{
					Layout.Shaders.Add(FShaderLayoutEntry(ShaderType, PermutationId));

					const FHashedName& TypeName = ShaderType->GetHashedName();
					Hasher.Update((uint8*)&TypeName, sizeof(TypeName));
					Hasher.Update((uint8*)&PermutationId, sizeof(PermutationId));
				}
			}
		}

		if (RHISupportsShaderPipelines(Platform))
		{
			// Iterate over all pipeline types
			for (FShaderPipelineType* ShaderPipelineType : SortedMaterialPipelineTypes)
			{
				if (FMaterialShaderType::ShouldCompilePipeline(ShaderPipelineType, Platform, MaterialParameters, Flags))
				{
					Layout.ShaderPipelines.Add(ShaderPipelineType);

					const FHashedName& TypeName = ShaderPipelineType->GetHashedName();
					Hasher.Update((uint8*)&TypeName, sizeof(TypeName));
				}
			}
		}

		for (FVertexFactoryType* VertexFactoryType : FVertexFactoryType::GetSortedMaterialTypes())
		{
			FMeshMaterialShaderMapLayout* MeshLayout = nullptr;
			for (FShaderType* BaseShaderType : SortedMeshMaterialShaderTypes)
			{
				FMeshMaterialShaderType* ShaderType = static_cast<FMeshMaterialShaderType*>(BaseShaderType);

				if (!FMeshMaterialShaderType::ShouldCompileVertexFactoryPermutation(Platform, MaterialParameters, VertexFactoryType, ShaderType, Flags))
				{
					continue;
				}

				const int32 PermutationCount = ShaderType->GetPermutationCount();
				for (int32 PermutationId = 0; PermutationId < PermutationCount; ++PermutationId)
				{
					if (ShaderType->ShouldCompilePermutation(Platform, MaterialParameters, VertexFactoryType, PermutationId, Flags))
					{
						if (!MeshLayout)
						{
							MeshLayout = new(Layout.MeshShaderMaps) FMeshMaterialShaderMapLayout(VertexFactoryType);
						}
						MeshLayout->Shaders.Add(FShaderLayoutEntry(ShaderType, PermutationId));

						const FHashedName& TypeName = ShaderType->GetHashedName();
						Hasher.Update((uint8*)&TypeName, sizeof(TypeName));
						Hasher.Update((uint8*)&PermutationId, sizeof(PermutationId));
					}
				}
			}

			if (RHISupportsShaderPipelines(Platform))
			{
				for (FShaderPipelineType* ShaderPipelineType : SortedMeshMaterialPipelineTypes)
				{
					if (!FMeshMaterialShaderType::ShouldCompileVertexFactoryPipeline(ShaderPipelineType, Platform, MaterialParameters, VertexFactoryType, Flags))
					{
						continue;
					}

					if (FMeshMaterialShaderType::ShouldCompilePipeline(ShaderPipelineType, Platform, MaterialParameters, VertexFactoryType, Flags))
					{
						// Now check the completeness of the shader map
						if (!MeshLayout)
						{
							MeshLayout = new(Layout.MeshShaderMaps) FMeshMaterialShaderMapLayout(VertexFactoryType);
						}
						MeshLayout->ShaderPipelines.Add(ShaderPipelineType);

						const FHashedName& TypeName = ShaderPipelineType->GetHashedName();
						Hasher.Update((uint8*)&TypeName, sizeof(TypeName));
					}
				}
			}
		}

		Hasher.Final();
		Hasher.GetHash(Layout.ShaderMapHash.Hash);
	}

	TArray<TUniquePtr<FMaterialShaderMapLayout>> ShaderMapLayouts;
	TArray<FMaterialShaderParameters> MaterialShaderParameters;
	TArray<uint64> MaterialParameterHashes;
	FHashTable ShaderMapHashTable;
	FRWLock LayoutLock;
};

const FMaterialShaderMapLayout& AcquireMaterialShaderMapLayout(EShaderPlatform Platform, EShaderPermutationFlags Flags, const FMaterialShaderParameters& MaterialParameters)
{
	return FMaterialShaderMapLayoutCache::Get().AcquireLayout(Platform, Flags, MaterialParameters);
}

bool FMaterialShaderMap::IsComplete(const FMaterial* Material, bool bSilent)
{
	SCOPED_LOADTIMER(FMaterialShaderMap_IsComplete);

	const FMaterialShaderMapContent* LocalContent = GetContent();
	const EShaderPlatform Platform = LocalContent->GetShaderPlatform();
	const FMaterialShaderParameters MaterialParameters(Material);

	const FMaterialShaderMapLayout& Layout = AcquireMaterialShaderMapLayout(Platform, ShaderMapId.GetPermutationFlags(), MaterialParameters);
	if (Layout.ShaderMapHash == LocalContent->ShaderContentHash)
	{
		return true;
	}

	// If our hash doesn't match the cached layout hash, shader map may still be complete
	// This can happen if FMaterial::ShouldCache is set to return false for any shaders that are included in the cached layout

	{
		// exclude shaders that are going to be uniquely used by the pipelines
		FPipelinedShaderFilter PipelinedShaderFilter(Platform, Layout.ShaderPipelines);
		for (const FShaderLayoutEntry& Shader : Layout.Shaders)
		{
			if (!LocalContent->HasShader(Shader.ShaderType, Shader.PermutationId))
			{
				if (!PipelinedShaderFilter.IsPipelinedType(Shader.ShaderType) && Material->ShouldCache(Platform, Shader.ShaderType, nullptr))
				{
					if (!bSilent)
					{
						UE_LOG(LogMaterial, Warning, TEXT("Incomplete material %s, missing FMaterialShader (%s, %d)."), *Material->GetFriendlyName(), Shader.ShaderType->GetName(), Shader.PermutationId);
					}
					return false;
				}
			}
		}
	}

	for (const FShaderPipelineType* Pipeline : Layout.ShaderPipelines)
	{
		if (!LocalContent->HasShaderPipeline(Pipeline))
		{
			if (Material->ShouldCachePipeline(Platform, Pipeline, nullptr))
			{
				if (!bSilent)
				{
					UE_LOG(LogMaterial, Warning, TEXT("Incomplete material %s, missing pipeline %s."), *Material->GetFriendlyName(), Pipeline->GetName());
				}
				return false;
			}
		}
	}

	for (const FMeshMaterialShaderMapLayout& MeshLayout : Layout.MeshShaderMaps)
	{
		FPipelinedShaderFilter PipelinedShaderFilter(Platform, MeshLayout.ShaderPipelines);
		const FMeshMaterialShaderMap* MeshShaderMap = LocalContent->GetMeshShaderMap(MeshLayout.VertexFactoryType->GetHashedName());

		for (const FShaderLayoutEntry& Shader : MeshLayout.Shaders)
		{
			if (Material->ShouldCache(Platform, Shader.ShaderType, MeshLayout.VertexFactoryType))
			{
				if (!PipelinedShaderFilter.IsPipelinedType(Shader.ShaderType) && (!MeshShaderMap || !MeshShaderMap->HasShader(Shader.ShaderType, Shader.PermutationId)))
				{
					if (!bSilent)
					{
						if (!MeshShaderMap)
						{
							UE_LOG(LogMaterial, Warning, TEXT("Incomplete material %s, missing Vertex Factory %s."), *Material->GetFriendlyName(), MeshLayout.VertexFactoryType->GetName());
						}
						else
						{
							UE_LOG(LogMaterial, Warning, TEXT("Incomplete material %s, missing (%s, %d) from %s."), *Material->GetFriendlyName(), Shader.ShaderType->GetName(), Shader.PermutationId, MeshLayout.VertexFactoryType->GetName());
						}
					}
					return false;
				}
			}
		}

		for (const FShaderPipelineType* Pipeline : MeshLayout.ShaderPipelines)
		{
			if (!MeshShaderMap || !MeshShaderMap->HasShaderPipeline(Pipeline))
			{
				if (Material->ShouldCachePipeline(Platform, Pipeline, MeshLayout.VertexFactoryType))
				{
					if (!bSilent)
					{
						if (!MeshShaderMap)
						{
							UE_LOG(LogMaterial, Warning, TEXT("Incomplete material %s, missing Vertex Factory %s."), *Material->GetFriendlyName(), MeshLayout.VertexFactoryType->GetName());
						}
						else
						{
							UE_LOG(LogMaterial, Warning, TEXT("Incomplete material %s, missing pipeline %s from %s."), *Material->GetFriendlyName(), Pipeline->GetName(), MeshLayout.VertexFactoryType->GetName());
						}
					}
					return false;
				}
			}
		}
	}

	// Was missing some shaders from the initial layout, but all of those shaders were explicitly exluced by our FMaterial::ShouldCache implementation
	return true;
}

FPSOPrecacheRequestResultArray FMaterialShaderMap::CollectPSOs(const FMaterialPSOPrecacheParams& PrecacheParams)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FMaterialShaderMap::CollectPSOs);

	// Shouldn't get here if the type doesn't support precaching
	check(PrecacheParams.VertexFactoryData.VertexFactoryType->SupportsPSOPrecaching());

	// Has data for this VF type
	const FMaterialShaderMapContent* LocalContent = GetContent();
	if (LocalContent == nullptr || !LocalContent->GetMeshShaderMap(PrecacheParams.VertexFactoryData.VertexFactoryType->GetHashedName()))
	{
		return FPSOPrecacheRequestResultArray();
	}

	// Only feature level is currently set as init settings - rest is default
	// (multiview & alpha channel not taken into account here)
	FSceneTexturesConfigInitSettings SceneTexturesConfigInitSettings;
	SceneTexturesConfigInitSettings.FeatureLevel = PrecacheParams.FeatureLevel;

	FSceneTexturesConfig SceneTexturesConfig;
	SceneTexturesConfig.Init(SceneTexturesConfigInitSettings);

	const EShadingPath ShadingPath = FSceneInterface::GetShadingPath(PrecacheParams.FeatureLevel);

	TArray<FPSOPrecacheData> PSOInitializers;
	PSOInitializers.Reserve(32);
	
	for (uint32 Index = 0; Index < FPSOCollectorCreateManager::MaxPSOCollectorCount; ++Index)
	{
		PSOCollectorCreateFunction CreateFunction = FPSOCollectorCreateManager::GetCreateFunction(ShadingPath, Index);
		if (CreateFunction)
		{
			IPSOCollector* PSOCollector = CreateFunction(PrecacheParams.FeatureLevel);
			if (PSOCollector != nullptr)
			{
				PSOCollector->CollectPSOInitializers(SceneTexturesConfig, *PrecacheParams.Material, PrecacheParams.VertexFactoryData, PrecacheParams.PrecachePSOParams, PSOInitializers);							
				delete PSOCollector;
			}
		}
	}

	return PrecachePSOs(PSOInitializers);
}

#if WITH_EDITOR
void FMaterialShaderMap::LoadMissingShadersFromMemory(const FMaterial* Material)
{
#if 0
	// Make sure we are operating on a referenced shader map or the below Find will cause this shader map to be deleted,
	// Since it creates a temporary ref counted pointer.
	check(NumRefs > 0);

	const TArray<FMaterial*>* CorrespondingMaterials = FMaterialShaderMap::ShaderMapsBeingCompiled.Find(this);

	if (CorrespondingMaterials)
	{
		check(!bCompilationFinalized);
		return;
	}

	FSHAHash MaterialShaderMapHash;
	ShaderMapId.GetMaterialHash(MaterialShaderMapHash);

	// Try to find necessary FMaterialShaderType's in memory
	for (TLinkedList<FShaderType*>::TIterator ShaderTypeIt(FShaderType::GetTypeList());ShaderTypeIt;ShaderTypeIt.Next())
	{
		FMaterialShaderType* ShaderType = ShaderTypeIt->GetMaterialShaderType();
		const int32 PermutationCount = ShaderType ? ShaderType->GetPermutationCount() : 0;
		for (int32 PermutationId = 0; PermutationId < PermutationCount; ++PermutationId)
		{
			if (ShouldCacheMaterialShader(ShaderType, GetShaderPlatform(), Material, PermutationId) && !HasShader(ShaderType, PermutationId))
			{
				FShaderKey ShaderKey(MaterialShaderMapHash, nullptr, nullptr, PermutationId, GetShaderPlatform());
				FShader* FoundShader = ShaderType->FindShaderByKey(ShaderKey);
				if (FoundShader)
				{
					AddShader(ShaderType, PermutationId, FoundShader);
				}
			}
		}
	}

	// Try to find necessary FShaderPipelineTypes in memory
	for (TLinkedList<FShaderPipelineType*>::TIterator ShaderPipelineIt(FShaderPipelineType::GetTypeList());ShaderPipelineIt;ShaderPipelineIt.Next())
	{
		const FShaderPipelineType* PipelineType = *ShaderPipelineIt;
		if (PipelineType && PipelineType->IsMaterialTypePipeline() && !HasShaderPipeline(PipelineType))
		{
			auto& Stages = PipelineType->GetStages();
			int32 NumShaders = 0;
			for (const FShaderType* Shader : Stages)
			{
				FMaterialShaderType* ShaderType = (FMaterialShaderType*)Shader->GetMaterialShaderType();
				if (ShaderType && ShouldCacheMaterialShader(ShaderType, GetShaderPlatform(), Material, kUniqueShaderPermutationId))
				{
					++NumShaders;
				}
			}

			if (NumShaders == Stages.Num())
			{
				TArray<FShader*> ShadersForPipeline;
				for (auto* Shader : Stages)
				{
					FMaterialShaderType* ShaderType = (FMaterialShaderType*)Shader->GetMaterialShaderType();
					if (!HasShader(ShaderType, kUniqueShaderPermutationId))
					{
						FShaderKey ShaderKey(MaterialShaderMapHash, PipelineType->ShouldOptimizeUnusedOutputs(GetShaderPlatform()) ? PipelineType : nullptr, nullptr, kUniqueShaderPermutationId, GetShaderPlatform());
						FShader* FoundShader = ShaderType->FindShaderByKey(ShaderKey);
						if (FoundShader)
						{
							AddShader(ShaderType, kUniqueShaderPermutationId, FoundShader);
							ShadersForPipeline.Add(FoundShader);
						}
					}
				}

				if (ShadersForPipeline.Num() == NumShaders && !HasShaderPipeline(PipelineType))
				{
					auto* Pipeline = new FShaderPipeline(PipelineType, ShadersForPipeline);
					AddShaderPipeline(PipelineType, Pipeline);
				}
			}
		}
	}

	// Try to find necessary FMeshMaterialShaderMap's in memory
	for(TLinkedList<FVertexFactoryType*>::TIterator VertexFactoryTypeIt(FVertexFactoryType::GetTypeList());VertexFactoryTypeIt;VertexFactoryTypeIt.Next())
	{
		FVertexFactoryType* VertexFactoryType = *VertexFactoryTypeIt;
		check(VertexFactoryType);

		if (VertexFactoryType->IsUsedWithMaterials())
		{
			FMeshMaterialShaderMap* MeshShaderMap = OrderedMeshShaderMaps[VertexFactoryType->GetId()];

			if (MeshShaderMap)
			{
				MeshShaderMap->LoadMissingShadersFromMemory(MaterialShaderMapHash, Material, GetShaderPlatform());
			}
		}
	}
#endif
}

const FMemoryImageString* FMaterialShaderMap::GetShaderSource(const FVertexFactoryType* VertexFactoryType, const FShaderType* ShaderType, int32 PermutationId) const
{
	FHashedName Key = GetPreprocessedSourceKey(VertexFactoryType, ShaderType, PermutationId);
	for (const FMaterialProcessedSource& Source : GetContent()->ShaderProcessedSource)
	{
		if (Source.Name == Key)
		{
			return &Source.Source;
		}
	}

	return nullptr;
}

const FMemoryImageString* FMaterialShaderMap::GetShaderSource(const FName VertexFactoryName, const FName ShaderTypeName) const
{
	return GetShaderSource(FindVertexFactoryType(VertexFactoryName), FindShaderTypeByName(ShaderTypeName), /* PermutationId */ 0);
}
#endif // WITH_EDITOR

void FMaterialShaderMap::GetShaderList(TMap<FShaderId, TShaderRef<FShader>>& OutShaders) const
{
	FSHAHash MaterialShaderMapHash;
#if WITH_EDITOR
	// TODO
	ShaderMapId.GetMaterialHash(MaterialShaderMapHash);
#endif

	GetContent()->GetShaderList(*this, MaterialShaderMapHash, OutShaders);
	for (FMeshMaterialShaderMap* MeshShaderMap : GetContent()->OrderedMeshShaderMaps)
	{
		if (MeshShaderMap)
		{
			MeshShaderMap->GetShaderList(*this, MaterialShaderMapHash, OutShaders);
		}
	}
}

void FMaterialShaderMap::GetShaderList(TMap<FHashedName, TShaderRef<FShader>>& OutShaders) const
{
	GetContent()->GetShaderList(*this, OutShaders);
	for (FMeshMaterialShaderMap* MeshShaderMap : GetContent()->OrderedMeshShaderMaps)
	{
		if (MeshShaderMap)
		{
			MeshShaderMap->GetShaderList(*this, OutShaders);
		}
	}
}

void FMaterialShaderMap::GetShaderPipelineList(TArray<FShaderPipelineRef>& OutShaderPipelines) const
{
	GetContent()->GetShaderPipelineList(*this, OutShaderPipelines, FShaderPipeline::EAll);
	for(FMeshMaterialShaderMap* MeshShaderMap : GetContent()->OrderedMeshShaderMaps)
	{
		if (MeshShaderMap)
		{
			MeshShaderMap->GetShaderPipelineList(*this, OutShaderPipelines, FShaderPipeline::EAll);
		}
	}
}

uint32 FMaterialShaderMap::GetShaderNum() const
{
	return GetContent()->GetNumShaders();
}

/**
 * Registers a material shader map in the global map so it can be used by materials.
 */
void FMaterialShaderMap::Register(EShaderPlatform InShaderPlatform)
{
	// Lazy initializer to bind OnSharedShaderMapResourceExplicitRelease to ShaderMapResourceExplicitRelease
	static struct FMaterialShaderMapInnerLazyInitializer
	{
		FMaterialShaderMapInnerLazyInitializer()
		{
			OnSharedShaderMapResourceExplicitRelease.BindStatic(&FMaterialShaderMap::ShaderMapResourceExplicitRelease);
		}
	} MaterialShaderMapInnerLazyInitializer;

	extern int32 GCreateShadersOnLoad;
	if (GCreateShadersOnLoad && GetShaderPlatform() == InShaderPlatform)
	{
		FShaderMapResource* ShaderResource = GetResource();
		if (ShaderResource)
		{
			ShaderResource->BeginCreateAllShaders();
		}
	}

	if (!bRegistered)
	{
		INC_DWORD_STAT(STAT_Shaders_NumShaderMaps);
	}

	{
		FScopeLock ScopeLock(&GIdToMaterialShaderMapCS);

		FMaterialShaderMap *CachedMap = GIdToMaterialShaderMap[GetShaderPlatform()].FindRef(ShaderMapId);

		// Only add new item if there's not already one in the map.
		// Items can possibly already be in the map because the GIdToMaterialShaderMapCS is not being locked between search & register lookups and new shader might be compiled
		if (CachedMap == nullptr)
		{
			GIdToMaterialShaderMap[GetShaderPlatform()].Add(ShaderMapId,this);
			bRegistered = true;
		}
		else
		{
			// Sanity check - We did not register so either bRegistered is false or this item is already in the map
			check((bRegistered == false && CachedMap != this) || (bRegistered == true && CachedMap == this));
		}
	}
}

void FMaterialShaderMap::AddRef()
{
	//#todo-mw: re-enable to try to find potential corruption of the global shader map ID array
	//check(IsInGameThread());
	FScopeLock ScopeLock(&GIdToMaterialShaderMapCS);
	check(!bDeletedThroughDeferredCleanup);
	++NumRefs;
}

void FMaterialShaderMap::Release()
{
	//#todo-mw: re-enable to try to find potential corruption of the global shader map ID array
	//check(IsInGameThread());

	{
		FScopeLock ScopeLock(&GIdToMaterialShaderMapCS);

		check(NumRefs > 0);
		if (--NumRefs == 0)
		{
			if (bRegistered)
			{
				bRegistered = false;
				DEC_DWORD_STAT(STAT_Shaders_NumShaderMaps);

				FMaterialShaderMap *CachedMap = GIdToMaterialShaderMap[GetShaderPlatform()].FindRef(ShaderMapId);

				// Map is marked as registered therefore we do expect it to be in the cache
				// If this does not happen there's bug in code causing ShaderMapID to be the same for two different objects.
				check(CachedMap == this);
				
				if (CachedMap == this)
				{
					GIdToMaterialShaderMap[GetShaderPlatform()].Remove(ShaderMapId);
				}
			}
			else
			{
				//sanity check - the map has not been registered and therefore should not appear in the cache
				check(GetShaderPlatform()>= EShaderPlatform::SP_NumPlatforms || GIdToMaterialShaderMap[GetShaderPlatform()].FindRef(ShaderMapId) != this);
			}

#if WITH_EDITOR
			FinalizedClone.SafeRelease();
#endif // WITH_EDITOR
			check(!bDeletedThroughDeferredCleanup);
			bDeletedThroughDeferredCleanup = true;
		}
	}
	if (bDeletedThroughDeferredCleanup)
	{
		BeginCleanup(this);
	}
}

FMaterialShaderMap::FMaterialShaderMap() :
	NumRefs(0),
	bDeletedThroughDeferredCleanup(false),
	bRegistered(false),
	bCompilationFinalized(true),
	bCompiledSuccessfully(true),
	bIsPersistent(false)
{
	checkSlow(IsInGameThread() || IsAsyncLoading());
#if ALLOW_SHADERMAP_DEBUG_DATA
	CompileTime = 0.f;
	{
		FScopeLock AllMatSMAccess(&AllMaterialShaderMapsGuard);
		AllMaterialShaderMaps.Add(this);
	}
#endif
}

FMaterialShaderMap::~FMaterialShaderMap()
{ 
	checkSlow(IsInGameThread() || IsAsyncLoading());
	check(bDeletedThroughDeferredCleanup);
	check(!bRegistered);
#if ALLOW_SHADERMAP_DEBUG_DATA
	if(GShaderCompilerStats != 0 && GetContent())
	{
		FString Path = GetMaterialPath();
		if (Path.IsEmpty())
		{
			Path = GetFriendlyName();
		}
		GShaderCompilerStats->RegisterCookedShaders(GetShaderNum(), CompileTime, GetShaderPlatform(), Path, GetDebugDescription());
	}
	{
		FScopeLock AllMatSMAccess(&AllMaterialShaderMapsGuard);
		AllMaterialShaderMaps.RemoveSwap(this);
	}
#endif

	// This is an unsavory hack to repair STAT_Shaders_NumShadersLoaded not being calculated right in the superclass because the Content class isn't allowed to have virtual functions atm.
	// A better way is tracked in UE-127112
#if STATS
	if (GetContent())
	{
		// Account for the extra shaders we missed because Content's GetNumShaders() isn't virtual and add them here. Note: Niagara needs the same
		uint32 BaseClassShaders = FShaderMapBase::GetContent()->GetNumShaders();
		uint32 TotalShadersIncludingBaseClass = GetContent()->GetNumShaders();
		uint32 OwnShaders = TotalShadersIncludingBaseClass - BaseClassShaders;
		DEC_DWORD_STAT_BY(STAT_Shaders_NumShadersLoaded, OwnShaders);
	}
#endif
}

#if WITH_EDITOR
FMaterialShaderMap* FMaterialShaderMap::AcquireFinalizedClone()
{
	checkSlow(IsInGameThread());

	const FMaterialShaderMapContent* LocalContent = GetContent();
	const FShaderMapResourceCode* LocalCode = GetResourceCode();

	checkf(LocalContent, TEXT("Can't clone shader map %s"), GetFriendlyName());
	checkf(LocalCode, TEXT("Can't clone shader map %s"), GetFriendlyName());

	if (GetFrozenContentSize() == 0u)
	{
		// If content isn't frozen yet, make sure to finalize it before making clone
		const_cast<FMaterialShaderMapContent*>(LocalContent)->Finalize(LocalCode);
	}

	LocalContent->Validate(*this);

	if (FinalizedClone && FinalizedClone->GetShaderContentHash() == GetShaderContentHash())
	{
		// Re-used existing clone if it's still valid
		return FinalizedClone;
	}

	FMaterialShaderMap* Clone = new FMaterialShaderMap();
	Clone->ShaderMapId = ShaderMapId;
	Clone->bCompilationFinalized = bCompilationFinalized;
	Clone->bCompiledSuccessfully = bCompiledSuccessfully;
	Clone->bIsPersistent = bIsPersistent;
	Clone->AssignCopy(*this);
	Clone->AssociateWithAssets(GetAssociatedAssets());

	FinalizedClone = Clone;
	return Clone;
}

FMaterialShaderMap* FMaterialShaderMap::GetFinalizedClone() const
{
	return FinalizedClone;
}
#endif // WITH_EDITOR

bool FMaterialShaderMap::Serialize(FArchive& Ar, bool bInlineShaderResources, bool bLoadedByCookedMaterial, bool bInlineShaderCode)
{
	SCOPED_LOADTIMER(FMaterialShaderMap_Serialize);
	// Note: This is saved to the DDC, not into packages (except when cooked)
	// Backwards compatibility therefore will not work based on the version of Ar
	// Instead, just bump MATERIALSHADERMAP_DERIVEDDATA_VER
	ShaderMapId.Serialize(Ar, bLoadedByCookedMaterial);
	bool bSerialized = Super::Serialize(Ar, bInlineShaderResources, bLoadedByCookedMaterial, bInlineShaderCode);
#if STATS
	// This is an unsavory hack to repair STAT_Shaders_NumShadersLoaded not being calculated right in the superclass because the Content class isn't allowed to have virtual functions atm.
	// A better way is tracked in UE-127112
	if (bSerialized && Ar.IsLoading())
	{
		// Account for the extra shaders we missed because Content's GetNumShaders() isn't virtual and add them here. Note: Niagara needs the same
		uint32 BaseClassShaders = FShaderMapBase::GetContent()->GetNumShaders();
		uint32 TotalShadersIncludingBaseClass = GetContent()->GetNumShaders();
		uint32 OwnShaders = TotalShadersIncludingBaseClass - BaseClassShaders;
		INC_DWORD_STAT_BY(STAT_Shaders_NumShadersLoaded, OwnShaders);
	}
#endif // STATS
	return bSerialized;
}

#if WITH_EDITOR
uint32 FMaterialShaderMap::GetMaxTextureSamplers() const
{
	uint32 MaxTextureSamplers = GetContent()->GetMaxTextureSamplersShaderMap(*this);

	for (int32 Index = 0;Index < GetContent()->OrderedMeshShaderMaps.Num();Index++)
	{
		const FMeshMaterialShaderMap* MeshShaderMap = GetContent()->OrderedMeshShaderMaps[Index];
		if (MeshShaderMap)
		{
			MaxTextureSamplers = FMath::Max(MaxTextureSamplers, MeshShaderMap->GetMaxTextureSamplersShaderMap(*this));
		}
	}

	return MaxTextureSamplers;
}
#endif // WITH_EDITOR

FMaterialShaderMapContent::~FMaterialShaderMapContent()
{
	int a = 0;
}

const FMeshMaterialShaderMap* FMaterialShaderMapContent::GetMeshShaderMap(const FHashedName& VertexFactoryTypeName) const
{
	const int32 Index = Algo::BinarySearchBy(OrderedMeshShaderMaps, VertexFactoryTypeName, FProjectMeshShaderMapToKey());
	if (Index != INDEX_NONE)
	{
		FMeshMaterialShaderMap* MeshShaderMap = OrderedMeshShaderMaps[Index].Get();
		checkSlow(MeshShaderMap->GetVertexFactoryTypeName() == VertexFactoryTypeName);
		return MeshShaderMap;
	}
	return nullptr;
}

FMeshMaterialShaderMap* FMaterialShaderMapContent::AcquireMeshShaderMap(const FHashedName& VertexFactoryTypeName)
{
	FMeshMaterialShaderMap* ShaderMap = const_cast<FMeshMaterialShaderMap*>(GetMeshShaderMap(VertexFactoryTypeName));
	if (!ShaderMap)
	{
		ShaderMap = new FMeshMaterialShaderMap(GetShaderPlatform(), VertexFactoryTypeName);
		AddMeshShaderMap(VertexFactoryTypeName, ShaderMap);
	}
	return ShaderMap;
}

void FMaterialShaderMapContent::AddMeshShaderMap(const FHashedName& VertexFactoryTypeName, FMeshMaterialShaderMap* MeshShaderMap)
{
	check(VertexFactoryTypeName == MeshShaderMap->GetVertexFactoryTypeName());
	checkSlow(GetMeshShaderMap(VertexFactoryTypeName) == nullptr);
	const int32 Index = Algo::LowerBoundBy(OrderedMeshShaderMaps, VertexFactoryTypeName, FProjectMeshShaderMapToKey());
	OrderedMeshShaderMaps.Insert(MeshShaderMap, Index);
}

void FMaterialShaderMapContent::RemoveMeshShaderMap(const FHashedName& VertexFactoryTypeName)
{
	const int32 Index = Algo::BinarySearchBy(OrderedMeshShaderMaps, VertexFactoryTypeName, FProjectMeshShaderMapToKey());
	if (Index != INDEX_NONE)
	{
		FMeshMaterialShaderMap* MeshShaderMap = OrderedMeshShaderMaps[Index].Get();
		delete MeshShaderMap;
		OrderedMeshShaderMaps.RemoveAt(Index);
	}
}

void FMaterialShaderMap::DumpDebugInfo(FOutputDevice& OutputDevice) const
{
	// Turn off as it makes diffing hard
	TGuardValue<ELogTimes::Type> DisableLogTimes(GPrintLogTimes, ELogTimes::None);

	OutputDevice.Logf(TEXT("Frequency, Target, VFType, ShaderType, SourceHash, VFSourceHash, OutputHash, IsShaderPipeline"));

	{
		TMap<FShaderId, TShaderRef<FShader>> ShadersL;
		GetShaderList(ShadersL);
		for (auto& KeyValue : ShadersL)
		{
			FShader* Shader = KeyValue.Value.GetShader();
			const FVertexFactoryType* VertexFactoryType = Shader->GetVertexFactoryType(GetPointerTable());
			OutputDevice.Logf(TEXT("%s, %s, %s, %s, %s, %s, %s, %s"),
				GetShaderFrequencyString(Shader->GetFrequency()),
				*LegacyShaderPlatformToShaderFormat(GetShaderPlatform()).ToString(),
				VertexFactoryType ? VertexFactoryType->GetName() : TEXT("null"),
				Shader->GetType(GetPointerTable())->GetName(),
				*Shader->GetHash().ToString(),
				*Shader->GetVertexFactoryHash().ToString(),
				*Shader->GetOutputHash().ToString(),
				TEXT("false")
				);
		}
	}

	{
		TArray<FShaderPipelineRef> ShaderPipelinesL;
		GetShaderPipelineList(ShaderPipelinesL);
		for (FShaderPipelineRef Value : ShaderPipelinesL)
		{
			FShaderPipeline* ShaderPipeline = Value.GetPipeline();
			TArray<TShaderRef<FShader>> ShadersL = ShaderPipeline->GetShaders(*this);
			for (TShaderRef<FShader>& Shader : ShadersL)
			{
				const FVertexFactoryType* VertexFactoryType = Shader->GetVertexFactoryType(GetPointerTable());
				OutputDevice.Logf(TEXT("%s, %s, %s, %s, %s, %s, %s, %s"),
					GetShaderFrequencyString(Shader->GetFrequency()),
					*LegacyShaderPlatformToShaderFormat(GetShaderPlatform()).ToString(),
					VertexFactoryType ? VertexFactoryType->GetName() : TEXT("null"),
					Shader->GetType(GetPointerTable())->GetName(),
					*Shader->GetHash().ToString(),
					*Shader->GetVertexFactoryHash().ToString(),
					*Shader->GetOutputHash().ToString(),
					TEXT("true")
					);
			}
		}
	}
}

#if WITH_EDITOR
void FMaterialShaderMap::InitalizeForODSC(EShaderPlatform TargetShaderPlatform, const FMaterialCompilationOutput& NewCompilationOutput)
{
	// Empty Content
	FMaterialShaderMapContent* NewContent = new FMaterialShaderMapContent(TargetShaderPlatform);
	NewContent->MaterialCompilationOutput = NewCompilationOutput;
	AssignContent(NewContent);

	// Empty Code
	GetResourceCode();
}
#endif // WITH_EDITOR

void FMaterialShaderMap::PostFinalizeContent()
{
	UniformBufferLayout.SafeRelease();
	if (GetContent())
	{
		UniformBufferLayout = RHICreateUniformBufferLayout(GetUniformExpressionSet().GetUniformBufferLayoutInitializer());
	}
}

#if WITH_EDITOR
void FMaterialShaderMap::GetOutdatedTypes(TArray<const FShaderType*>& OutdatedShaderTypes, TArray<const FShaderPipelineType*>& OutdatedShaderPipelineTypes, TArray<const FVertexFactoryType*>& OutdatedFactoryTypes) const
{
	FShaderMapBase::GetOutdatedTypes(OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);

	for (FMeshMaterialShaderMap* MeshShaderMap : GetContent()->OrderedMeshShaderMaps)
	{
		if (MeshShaderMap)
		{
			MeshShaderMap->GetOutdatedTypes(*this, OutdatedShaderTypes, OutdatedShaderPipelineTypes, OutdatedFactoryTypes);
		}
	}
}

void FMaterialShaderMap::SaveShaderStableKeys(EShaderPlatform TargetShaderPlatform, const FStableShaderKeyAndValue& SaveKeyVal)
{
	FShaderMapBase::SaveShaderStableKeys(TargetShaderPlatform, SaveKeyVal);
	for (FMeshMaterialShaderMap* MeshShaderMap : GetContent()->OrderedMeshShaderMaps)
	{
		if (MeshShaderMap)
		{
			MeshShaderMap->SaveShaderStableKeys(*this, TargetShaderPlatform, SaveKeyVal);
		}
	}
}
#endif // WITH_EDITOR

void FMaterialShaderMap::ShaderMapResourceExplicitRelease(const FShaderMapResource* ShaderMapResource)
{
	EShaderPlatform ShaderPlatform = ShaderMapResource->GetPlatform();

	// visit cached Material shader map and remove ones that have been released (possibly due to GC leak) to avoid use after free in RT
	FScopeLock ScopeLock(&GIdToMaterialShaderMapCS);
	for (auto MaterialMapIterator = GIdToMaterialShaderMap[ShaderPlatform].CreateIterator(); MaterialMapIterator; ++MaterialMapIterator)
	{
		if (MaterialMapIterator->Value->GetResource() == ShaderMapResource)
		{
			MaterialMapIterator.RemoveCurrent();
		}
	}
}

/**
 * Dump material stats for a given platform.
 * 
 * @param	Platform	Platform to dump stats for.
 */
void DumpMaterialStats(EShaderPlatform Platform)
{
#if ALLOW_DEBUG_FILES && ALLOW_SHADERMAP_DEBUG_DATA
	FDiagnosticTableViewer MaterialViewer(*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(TEXT("MaterialStats")));

	//#todo-rco: Pipelines

	// Mapping from friendly material name to shaders associated with it.
	TMultiMap<FString, TShaderRef<FShader>> MaterialToShaderMap;
	TMultiMap<FString, FShaderPipeline*> MaterialToShaderPipelineMap;

	// Set of material names.
	TSet<FString> MaterialNames;

	// Look at in-memory shader use.
	FScopeLock AllMatSMAccess(&FMaterialShaderMap::AllMaterialShaderMapsGuard);
	for (int32 ShaderMapIndex = 0; ShaderMapIndex < FMaterialShaderMap::AllMaterialShaderMaps.Num(); ShaderMapIndex++)
	{
		FMaterialShaderMap* MaterialShaderMap = FMaterialShaderMap::AllMaterialShaderMaps[ShaderMapIndex];
		TMap<FShaderId, TShaderRef<FShader>> Shaders;
		TArray<FShaderPipelineRef> ShaderPipelines;
		MaterialShaderMap->GetShaderList(Shaders);
		MaterialShaderMap->GetShaderPipelineList(ShaderPipelines);

		// Add friendly name to list of materials.
		FString FriendlyName = MaterialShaderMap->GetFriendlyName();
		MaterialNames.Add(FriendlyName);

		// Add shaders to mapping per friendly name as there might be multiple
		for (auto& KeyValue : Shaders)
		{
			MaterialToShaderMap.AddUnique(FriendlyName, KeyValue.Value);
		}

		for (const FShaderPipelineRef& Pipeline : ShaderPipelines)
		{
			for (const TShaderRef<FShader>& Shader : Pipeline.GetShaders())
			{
				MaterialToShaderMap.AddUnique(FriendlyName, Shader);
			}
			MaterialToShaderPipelineMap.AddUnique(FriendlyName, Pipeline.GetPipeline());
		}
	}

	// Write a row of headings for the table's columns.
	MaterialViewer.AddColumn(TEXT("Name"));
	MaterialViewer.AddColumn(TEXT("Shaders"));
	MaterialViewer.AddColumn(TEXT("Code Size"));
	MaterialViewer.AddColumn(TEXT("Pipelines"));
	MaterialViewer.CycleRow();

	// Iterate over all materials, gathering shader stats.
	int32 TotalCodeSize		= 0;
	int32 TotalShaderCount	= 0;
	int32 TotalShaderPipelineCount = 0;
	for( TSet<FString>::TConstIterator It(MaterialNames); It; ++It )
	{
		// Retrieve list of shaders in map.
		TArray<TShaderRef<FShader>> Shaders;
		MaterialToShaderMap.MultiFind( *It, Shaders );
		TArray<FShaderPipeline*> ShaderPipelines;
		MaterialToShaderPipelineMap.MultiFind(*It, ShaderPipelines);
		
		// Iterate over shaders and gather stats.
		int32 CodeSize = 0;
		for( int32 ShaderIndex=0; ShaderIndex<Shaders.Num(); ShaderIndex++ )
		{
			const TShaderRef<FShader>& Shader = Shaders[ShaderIndex];
			CodeSize += Shader->GetCodeSize();
		}

		TotalCodeSize += CodeSize;
		TotalShaderCount += Shaders.Num();
		TotalShaderPipelineCount += ShaderPipelines.Num();

		// Dump stats
		MaterialViewer.AddColumn(**It);
		MaterialViewer.AddColumn(TEXT("%u"),Shaders.Num());
		MaterialViewer.AddColumn(TEXT("%u"),CodeSize);
		MaterialViewer.AddColumn(TEXT("%u"), ShaderPipelines.Num());
		MaterialViewer.CycleRow();
	}

	// Add a total row.
	MaterialViewer.AddColumn(TEXT("Total"));
	MaterialViewer.AddColumn(TEXT("%u"),TotalShaderCount);
	MaterialViewer.AddColumn(TEXT("%u"),TotalCodeSize);
	MaterialViewer.AddColumn(TEXT("%u"), TotalShaderPipelineCount);
	MaterialViewer.CycleRow();
#endif // ALLOW_DEBUG_FILES && ALLOW_SHADERMAP_DEBUG_DATA
}

