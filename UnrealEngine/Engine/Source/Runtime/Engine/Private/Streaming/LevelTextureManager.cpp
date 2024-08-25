// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	LevelTextureManager.cpp: Implementation of content streaming classes.
=============================================================================*/

#include "Streaming/LevelTextureManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "SceneInterface.h"
#include "Streaming/DynamicTextureInstanceManager.h"
#include "Streaming/StaticTextureInstanceManager.h"

FLevelRenderAssetManager::FLevelRenderAssetManager(ULevel* InLevel, RenderAssetInstanceTask::FDoWorkTask& AsyncTask)
	: Level(InLevel)
	, bIsInitialized(false)
	, bHasBeenReferencedToStreamedTextures(false)
	, StaticInstances(AsyncTask)
	, BuildStep(EStaticBuildStep::BuildTextureLookUpMap)
{
	if (Level)
	{
		Level->bStaticComponentsRegisteredInStreamingManager = false;
	}
}


void FLevelRenderAssetManager::Remove(FRemovedRenderAssetArray* RemovedRenderAssets)
{ 
	TArray<const UPrimitiveComponent*> ReferencedComponents;
	StaticInstances.GetReferencedComponents(ReferencedComponents);
	ReferencedComponents.Append(UnprocessedComponents);
	ReferencedComponents.Append(PendingComponents);
	for (const UPrimitiveComponent* Component : ReferencedComponents)
	{
		if (Component)
		{
			check(Component->IsValidLowLevelFast()); // Check that this component was not already destroyed.
			// Don't check as there can be duplicates in PendingComponents
			// check(Component->bAttachedToStreamingManagerAsStatic);  

			// A component can only be referenced in one level, so if it was here, we can clear the flag
			Component->bAttachedToStreamingManagerAsStatic = false;
		}
	}

	// Mark all static textures/meshes for removal.
	if (RemovedRenderAssets)
	{
		for (auto It = StaticInstances.GetRenderAssetIterator(); It; ++It)
		{
			RemovedRenderAssets->Push(*It);
		}
	}

	BuildStep = EStaticBuildStep::BuildTextureLookUpMap;
	UnprocessedComponents.Empty();
	PendingComponents.Empty();
	TextureGuidToLevelIndex.Empty();
	bIsInitialized = false;

	if (Level)
	{
		Level->bStaticComponentsRegisteredInStreamingManager = false;
	}
}

float FLevelRenderAssetManager::GetWorldTime() const
{
	if (Level)
	{
		UWorld* World = Level->GetWorld();

		// When paused, updating the world time sometimes break visibility logic.
		if (World && !World->IsPaused())
		{
			// In the editor, we only return a time for the PIE world.
			// TODO: figure out why there are more than one PIE world
			if (!GIsEditor || (World->IsPlayInEditor() && World->Scene && World->Scene->GetFrameNumber()))
			{
				return World->GetTimeSeconds();
			}
		}
	}
	return 0;
}

void FLevelRenderAssetManager::SetAsStatic(FDynamicRenderAssetInstanceManager& DynamicComponentManager, const UPrimitiveComponent* Primitive)
{
	check(Primitive);
	Primitive->bAttachedToStreamingManagerAsStatic = true;
	if (Primitive->bHandledByStreamingManagerAsDynamic)
	{
		DynamicComponentManager.Remove(Primitive, nullptr);
		Primitive->bHandledByStreamingManagerAsDynamic = false;
	}
}


void FLevelRenderAssetManager::SetAsDynamic(FDynamicRenderAssetInstanceManager& DynamicComponentManager, FStreamingTextureLevelContext& LevelContext, const UPrimitiveComponent* Primitive)
{
	check(Primitive);
	Primitive->bAttachedToStreamingManagerAsStatic = false;
	if (!Primitive->bHandledByStreamingManagerAsDynamic)
	{
		DynamicComponentManager.Add(Primitive, LevelContext);
	}
}

void FLevelRenderAssetManager::IncrementalBuild(FDynamicRenderAssetInstanceManager& DynamicComponentManager, FStreamingTextureLevelContext& LevelContext, bool bForceCompletion, int64& NumStepsLeft)
{
	check(Level);

	const float MaxTextureUVDensity = CVarStreamingMaxTextureUVDensity.GetValueOnAnyThread();

	switch (BuildStep)
	{
	case EStaticBuildStep::BuildTextureLookUpMap:
	{
		// Build the map to convert from a guid to the level index. 
		TextureGuidToLevelIndex.Reserve(Level->StreamingTextureGuids.Num());
		for (int32 TextureIndex = 0; TextureIndex < Level->StreamingTextureGuids.Num(); ++TextureIndex)
		{
			TextureGuidToLevelIndex.Add(Level->StreamingTextureGuids[TextureIndex], TextureIndex);
		}
		NumStepsLeft -= Level->StreamingTextureGuids.Num();
		BuildStep = EStaticBuildStep::ProcessActors;

		// Update the level context with the texture guid map. This is required in case the incremental build runs more steps.
		LevelContext.UpdateContext(EMaterialQualityLevel::Num, Level, &TextureGuidToLevelIndex);
		break;
	}
	case EStaticBuildStep::ProcessActors:
	{
		// All actors needs to be processed at once here because of the logic around bStaticComponentsRegisteredInStreamingManager.
		// All components must have either bHandledByStreamingManagerAsDynamic or bAttachedToStreamingManagerAsStatic set once bStaticComponentsRegisteredInStreamingManager gets set.
		// If any component gets created after, the logic in UPrimitiveComponent::CreateRenderState_Concurrent() will detect it as a new component and put it through the dynamic path.
		for (const AActor* Actor : Level->Actors)
		{
			if (!Actor) continue;

			const bool bIsStaticActor = Actor->IsRootComponentStatic();

			TInlineComponentArray<UPrimitiveComponent*> Primitives;
			Actor->GetComponents(Primitives);
			for (const UPrimitiveComponent* Primitive : Primitives)
			{
				check(Primitive);
				if (bIsStaticActor && Primitive->Mobility == EComponentMobility::Static)
				{
					SetAsStatic(DynamicComponentManager, Primitive);
					UnprocessedComponents.Push(Primitive);
				}
				else
				{
					SetAsDynamic(DynamicComponentManager, LevelContext, Primitive);
				}
			}
			
			NumStepsLeft -= (int64)FMath::Max<int32>(Primitives.Num(), 1);
		}

		NumStepsLeft -= (int64)FMath::Max<int32>(Level->Actors.Num(), 1);

		// Set a flag so that any further component added to the level gets handled as dynamic.
		Level->bStaticComponentsRegisteredInStreamingManager = true;

		BuildStep = EStaticBuildStep::ProcessComponents;
		break;
	}
	case EStaticBuildStep::ProcessComponents:
	{
		const bool bLevelIsVisible = Level->bIsVisible;

		while ((bForceCompletion || NumStepsLeft > 0) && UnprocessedComponents.Num())
		{
			const UPrimitiveComponent* Primitive = UnprocessedComponents.Pop(EAllowShrinking::No);

			const EAddComponentResult AddResult = StaticInstances.Add(Primitive, LevelContext, MaxTextureUVDensity);
			if (AddResult == EAddComponentResult::Fail && !bLevelIsVisible)
			{
				// Retry once the level becomes visible.
				PendingComponents.Add(Primitive);
			}
			else if (AddResult != EAddComponentResult::Success)
			{
				// Here we also handle the case for Fail_UIDensityConstraint.
				SetAsDynamic(DynamicComponentManager, LevelContext, Primitive);
			}

			--NumStepsLeft;
		}

		if (!UnprocessedComponents.Num())
		{
			UnprocessedComponents.Empty(); // Free the memory.
			BuildStep = EStaticBuildStep::NormalizeLightmapTexelFactors;
		}
		break;
	}
	case EStaticBuildStep::NormalizeLightmapTexelFactors:
	{
		// Unfortunately, PendingInsertionStaticPrimtivComponents won't be taken into account here.
		StaticInstances.NormalizeLightmapTexelFactor();
		BuildStep = EStaticBuildStep::CompileElements;
		break;
	}
	case EStaticBuildStep::CompileElements:
	{
		// Compile elements (to optimize runtime) for what is there.
		// PendingInsertionStaticPrimitives will be added after.
		NumStepsLeft -= (int64)StaticInstances.CompileElements();
		BuildStep = EStaticBuildStep::WaitForRegistration;
		break;
	}
	case EStaticBuildStep::WaitForRegistration:
		if (Level->bIsVisible)
		{
			// Remove unregistered component and resolve the bounds using the packed relative boxes.
			TArray<const UPrimitiveComponent*> RemovedPrimitives;
			NumStepsLeft -= (int64)StaticInstances.CheckRegistrationAndUnpackBounds(RemovedPrimitives);
			for (const UPrimitiveComponent* Primitive : RemovedPrimitives)
			{
				SetAsDynamic(DynamicComponentManager, LevelContext, Primitive);
			}

			NumStepsLeft -= (int64)PendingComponents.Num();

			// Reprocess the components that didn't have valid data.
			while (PendingComponents.Num())
			{
				const UPrimitiveComponent* Primitive = PendingComponents.Pop(EAllowShrinking::No);

				if (StaticInstances.Add(Primitive, LevelContext, MaxTextureUVDensity) != EAddComponentResult::Success)
				{
					SetAsDynamic(DynamicComponentManager, LevelContext, Primitive);
				}
			}

			PendingComponents.Empty(); // Free the memory.
			TextureGuidToLevelIndex.Empty();
			BuildStep = EStaticBuildStep::Done;
		}
		break;
	default:
		break;
	}
}

bool FLevelRenderAssetManager::NeedsIncrementalBuild(int32 NumStepsLeftForIncrementalBuild) const
{
	check(Level);

	if (BuildStep == EStaticBuildStep::Done)
	{
		return false;
	}
	else if (Level->bIsVisible)
	{
		return true; // If visible, continue until done.
	}
	else // Otherwise, continue while there are incremental build steps available or we are waiting for visibility.
	{
		return BuildStep != EStaticBuildStep::WaitForRegistration && NumStepsLeftForIncrementalBuild > 0;
	}
}

void FLevelRenderAssetManager::IncrementalUpdate(
	FDynamicRenderAssetInstanceManager& DynamicComponentManager, 
	FRemovedRenderAssetArray& RemovedRenderAssets,
	int64& NumStepsLeftForIncrementalBuild, 
	float Percentage, 
	bool bUseDynamicStreaming) 
{
	check(Level);

	if (NeedsIncrementalBuild(NumStepsLeftForIncrementalBuild))
	{
		FStreamingTextureLevelContext LevelContext(EMaterialQualityLevel::Num, Level, &TextureGuidToLevelIndex);
		do
		{
			IncrementalBuild(DynamicComponentManager, LevelContext, Level->bIsVisible, NumStepsLeftForIncrementalBuild);
		}
		while (NeedsIncrementalBuild(NumStepsLeftForIncrementalBuild));
	}

	if (BuildStep == EStaticBuildStep::Done)
	{
		if (Level->bIsVisible)
		{
			bIsInitialized = true;
			// If the level is visible, update the bounds.
			StaticInstances.Refresh(Percentage);
		}
		else if (bIsInitialized)
		{
			// Mark all static textures for removal.
			for (auto It = StaticInstances.GetRenderAssetIterator(); It; ++It)
			{
				RemovedRenderAssets.Push(*It);
			}
			bIsInitialized = false;
		}
	}
}

void FLevelRenderAssetManager::NotifyLevelOffset(const FVector& Offset)
{
	if (BuildStep == EStaticBuildStep::Done)
	{
		// offset static primitives bounds
		StaticInstances.OffsetBounds(Offset);
	}
}

uint32 FLevelRenderAssetManager::GetAllocatedSize() const
{
	return StaticInstances.GetAllocatedSize() + 
		UnprocessedComponents.GetAllocatedSize() + 
		PendingComponents.GetAllocatedSize();
}

