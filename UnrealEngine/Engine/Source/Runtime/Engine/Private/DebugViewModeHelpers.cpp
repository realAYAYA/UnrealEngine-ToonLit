// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	DebugViewModeHelpers.cpp: debug view shader helpers.
=============================================================================*/
#include "DebugViewModeHelpers.h"
#include "DebugViewModeInterface.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "MaterialShaderType.h"
#include "MeshMaterialShader.h"
#include "ShaderCompiler.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/FeedbackContext.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "ActorEditorUtils.h"

#define LOCTEXT_NAMESPACE "LogDebugViewMode"

const TCHAR* DebugViewShaderModeToString(EDebugViewShaderMode InShaderMode)
{
	switch (InShaderMode)
	{
	case DVSM_None:
		return TEXT("DVSM_None");
	case DVSM_ShaderComplexity:
		return TEXT("DVSM_ShaderComplexity");
	case DVSM_ShaderComplexityContainedQuadOverhead:
		return TEXT("DVSM_ShaderComplexityContainedQuadOverhead");
	case DVSM_ShaderComplexityBleedingQuadOverhead:
		return TEXT("DVSM_ShaderComplexityBleedingQuadOverhead");
	case DVSM_QuadComplexity:
		return TEXT("DVSM_QuadComplexity");
	case DVSM_PrimitiveDistanceAccuracy:
		return TEXT("DVSM_PrimitiveDistanceAccuracy");
	case DVSM_MeshUVDensityAccuracy:
		return TEXT("DVSM_MeshUVDensityAccuracy");
	case DVSM_MaterialTextureScaleAccuracy:
		return TEXT("DVSM_MaterialTextureScaleAccuracy");
	case DVSM_OutputMaterialTextureScales:
		return TEXT("DVSM_OutputMaterialTextureScales");
	case DVSM_RequiredTextureResolution:
		return TEXT("DVSM_RequiredTextureResolution");
	case DVSM_VirtualTexturePendingMips:
		return TEXT("DVSM_VirtualTexturePendingMips");
	case DVSM_RayTracingDebug:
		return TEXT("DVSM_RayTracingDebug");
	case DVSM_LODColoration:
		return TEXT("DVSM_LODColoration");
	case DVSM_VisualizeGPUSkinCache:
		return TEXT("DVSM_VisualizeGPUSkinCache");
	default:
		return TEXT("DVSM_None");
	}
}

#if WITH_DEBUG_VIEW_MODES

static bool PlatformSupportsDebugViewShaders(EShaderPlatform Platform)
{
	// List of platforms that have been tested and proved functional.
	return FDataDrivenShaderPlatformInfo::GetSupportsDebugViewShaders(Platform);
}

bool AllowDebugViewVSDSHS(EShaderPlatform Platform)
{
	return IsPCPlatform(Platform); 
}

bool AllowDebugViewShaderMode(EDebugViewShaderMode ShaderMode, EShaderPlatform Platform, ERHIFeatureLevel::Type FeatureLevel)
{
#if WITH_EDITOR
	// Those options are used to test compilation on specific platforms
	static const bool bForceQuadOverdraw = FParse::Param(FCommandLine::Get(), TEXT("quadoverdraw"));
	static const bool bForceStreamingAccuracy = FParse::Param(FCommandLine::Get(), TEXT("streamingaccuracy"));
	static const bool bForceTextureStreamingBuild = FParse::Param(FCommandLine::Get(), TEXT("streamingbuild"));

	switch (ShaderMode)
	{
	case DVSM_None:
		return false;
	case DVSM_ShaderComplexity:
	case DVSM_LODColoration:
		return IsPCPlatform(Platform);
	case DVSM_ShaderComplexityContainedQuadOverhead:
	case DVSM_ShaderComplexityBleedingQuadOverhead:
	case DVSM_QuadComplexity:
		return FeatureLevel >= ERHIFeatureLevel::SM5 && (bForceQuadOverdraw || (PlatformSupportsDebugViewShaders(Platform) && !IsMetalPlatform(Platform))); // Last one to fix for Metal then remove this Metal check.
	case DVSM_PrimitiveDistanceAccuracy:
	case DVSM_MeshUVDensityAccuracy:
		return FeatureLevel >= ERHIFeatureLevel::SM5 && (bForceStreamingAccuracy || PlatformSupportsDebugViewShaders(Platform));
	case DVSM_MaterialTextureScaleAccuracy:
	case DVSM_RequiredTextureResolution:
	case DVSM_OutputMaterialTextureScales:
	case DVSM_VirtualTexturePendingMips:
		return FeatureLevel >= ERHIFeatureLevel::SM5 && (bForceTextureStreamingBuild || PlatformSupportsDebugViewShaders(Platform));
	case DVSM_RayTracingDebug:
		return FeatureLevel >= ERHIFeatureLevel::SM5 ;
	case DVSM_VisualizeGPUSkinCache:
		return PlatformSupportsDebugViewShaders(Platform);
	default:
		return false;
	}
#else
	return ShaderMode == DVSM_ShaderComplexity;
#endif
}

bool ShouldCompileDebugViewModeShader(const FMeshMaterialShaderPermutationParameters& Parameters)
{
	if (!PlatformSupportsDebugViewShaders(Parameters.Platform))
	{
		return false;
	}

	if (Parameters.MaterialParameters.FeatureLevel < ERHIFeatureLevel::SM5)
	{
		return false;
	}

	if (!EnumHasAllFlags(Parameters.Flags, EShaderPermutationFlags::HasEditorOnlyData))
	{
		// Debug view shaders only in editor
		return false;
	}

	return true;
}

#endif // !(UE_BUILD_SHIPPING || UE_BUILD_TEST)


int32 GetNumActorsInWorld(UWorld* InWorld)
{
	int32 ActorCount = 0;
	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);
		if (!Level)
		{
			continue;
		}

		ActorCount += Level->Actors.Num();
	}
	return ActorCount;
}

bool WaitForShaderCompilation(const FText& Message, FSlowTask* ProgressTask)
{
	FlushRenderingCommands();

	const int32 NumShadersToBeCompiled = GShaderCompilingManager->GetNumRemainingJobs();
	int32 RemainingShaders = NumShadersToBeCompiled;
	if (NumShadersToBeCompiled > 0)
	{
		FScopedSlowTask SlowTask(1.f, Message);

		while (RemainingShaders > 0)
		{
			FPlatformProcess::Sleep(0.01f);
			GShaderCompilingManager->ProcessAsyncResults(false, true);

			const int32 RemainingShadersThisFrame = GShaderCompilingManager->GetNumRemainingJobs();
			if (RemainingShadersThisFrame > 0)
			{
				const int32 NumberOfShadersCompiledThisFrame = RemainingShaders - RemainingShadersThisFrame;

				const float FrameProgress = (float)NumberOfShadersCompiledThisFrame / (float)NumShadersToBeCompiled;
				if (ProgressTask)
				{
					ProgressTask->EnterProgressFrame(FrameProgress);
					SlowTask.EnterProgressFrame(FrameProgress);
					if (GWarn->ReceivedUserCancel())
					{
						return false;
					}
				}
			}
			RemainingShaders = RemainingShadersThisFrame;
		}
	}
	else if (ProgressTask)
	{
		ProgressTask->EnterProgressFrame();
		if (GWarn->ReceivedUserCancel())
		{
			return false;
		}
	}

	// Extra safety to make sure every shader map is updated
	GShaderCompilingManager->FinishAllCompilation();
	FlushRenderingCommands();

	return true;
}

/** Get the list of all material used in a world 
 *
 * @return true if the operation is a success, false if it was canceled.
 */
bool GetUsedMaterialsInWorld(UWorld* InWorld, OUT TSet<UMaterialInterface*>& OutMaterials, FSlowTask* ProgressTask)
{
#if WITH_EDITORONLY_DATA
	if (!InWorld)
	{
		return false;
	}

	const int32 NumActorsInWorld = GetNumActorsInWorld(InWorld);
	if (!NumActorsInWorld)
	{
		if (ProgressTask)
		{
			ProgressTask->EnterProgressFrame();
		}
		return true;
	}

	const float OneOverNumActorsInWorld = 1.f / (float)NumActorsInWorld;

	FScopedSlowTask SlowTask(1.f, (LOCTEXT("TextureStreamingBuild_GetTextureStreamingBuildMaterials", "Getting materials to rebuild")));

	for (int32 LevelIndex = 0; LevelIndex < InWorld->GetNumLevels(); LevelIndex++)
	{
		ULevel* Level = InWorld->GetLevel(LevelIndex);
		if (!Level)
		{
			continue;
		}

		for (AActor* Actor : Level->Actors)
		{
			if (ProgressTask)
			{
				ProgressTask->EnterProgressFrame(OneOverNumActorsInWorld);
				SlowTask.EnterProgressFrame(OneOverNumActorsInWorld);
				if (GWarn->ReceivedUserCancel())
				{
					return false;
				}
			}

			// Check the actor after incrementing the progress.
			if (!Actor || FActorEditorUtils::IsABuilderBrush(Actor))
			{
				continue;
			}

			TInlineComponentArray<UPrimitiveComponent*> Primitives;
			Actor->GetComponents(Primitives);

			for (UPrimitiveComponent* Primitive : Primitives)
			{
				if (!Primitive)
				{
					continue;
				}

				TArray<UMaterialInterface*> Materials;
				Primitive->GetUsedMaterials(Materials);

				for (UMaterialInterface* Material : Materials)
				{
					if (Material)
					{
						OutMaterials.Add(Material);
					}
				}
			}
		}
	}
	return OutMaterials.Num() != 0;
#else
	return false;
#endif
}

/**
 * Build Shaders to compute scales per texture.
 *
 * @param QualityLevel		The quality level for the shaders.
 * @param FeatureLevel		The feature level for the shaders.
 * @param Materials			The materials to update, the one that failed compilation will be removed (IN OUT).
 * @return true if the operation is a success, false if it was canceled.
 */
bool CompileDebugViewModeShaders(EDebugViewShaderMode ShaderMode, EMaterialQualityLevel::Type QualityLevel, ERHIFeatureLevel::Type FeatureLevel, TSet<UMaterialInterface*>& Materials, FSlowTask* ProgressTask)
{
#if WITH_EDITORONLY_DATA
	if (!GShaderCompilingManager || !Materials.Num())
	{
		return false;
	}

	const FDebugViewModeInterface* DebugViewModeInterface = FDebugViewModeInterface::GetInterface(ShaderMode);
	if (!DebugViewModeInterface)
	{
		return false;
	}

	const FVertexFactoryType* LocalVertexFactory = FindVertexFactoryType(TEXT("FLocalVertexFactory"));
	if (!LocalVertexFactory)
	{
		return false;
	}

	// remove materials incompatible with debug view modes (e.g. landscape materials can only be compiled with the landscape VF)
	for (TSet<UMaterialInterface*>::TIterator It(Materials); It; ++It)
	{
		UMaterialInterface* MaterialInterface = *It;
		check(MaterialInterface); // checked for null in GetTextureStreamingBuildMaterials
		const FMaterial* Material = MaterialInterface->GetMaterialResource(FeatureLevel, QualityLevel);

		if (Material && (Material->GetMaterialDomain() != MD_Surface || Material->IsUsedWithLandscape()))
		{
			It.RemoveCurrent();
		}
	}

	TSet<UMaterialInterface*> PendingMaterials = Materials;
	while (PendingMaterials.Num() > 0)
	{
		for(TSet<UMaterialInterface*>::TIterator It(PendingMaterials); It; ++It )
		{
			UMaterialInterface* MaterialInterface = *It;
			check(MaterialInterface); // checked for null in GetTextureStreamingBuildMaterials

			const FMaterial* Material = MaterialInterface->GetMaterialResource(FeatureLevel, QualityLevel);
			bool bMaterialFinished = true;
			if (Material && Material->GetGameThreadShaderMap())
			{
				FMaterialShaderTypes ShaderTypes;
				DebugViewModeInterface->AddShaderTypes(FeatureLevel, LocalVertexFactory, ShaderTypes);
				if (Material->ShouldCacheShaders(GetFeatureLevelShaderPlatform(FeatureLevel), ShaderTypes, LocalVertexFactory) && !Material->HasShaders(ShaderTypes, LocalVertexFactory))
				{
					bMaterialFinished = false;
				}
			}

			if (bMaterialFinished)
			{
				It.RemoveCurrent();
			}
		}

		if (PendingMaterials.Num() > 0)
		{
			FPlatformProcess::Sleep(0.1f);
			GShaderCompilingManager->ProcessAsyncResults(false, false);
			if (GWarn->ReceivedUserCancel())
			{
				break;
			}
		}
	}

	return PendingMaterials.Num() == 0;

#else
	return false;
#endif

}

#undef LOCTEXT_NAMESPACE
