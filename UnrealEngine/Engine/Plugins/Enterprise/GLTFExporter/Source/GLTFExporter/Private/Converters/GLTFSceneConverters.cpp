// Copyright Epic Games, Inc. All Rights Reserved.

#include "Converters/GLTFSceneConverters.h"
#include "Builders/GLTFContainerBuilder.h"
#include "LevelVariantSetsActor.h"
#include "LevelVariantSets.h"
#include "VariantSet.h"

FGLTFJsonScene* FGLTFSceneConverter::Convert(const UWorld* World)
{
	FGLTFJsonScene* Scene = Builder.AddScene();
	World->GetName(Scene->Name);

	const TArray<ULevel*>& Levels = World->GetLevels();
	if (Levels.Num() > 0)
	{
		for (const ULevel* Level : Levels)
		{
			if (Level == nullptr)
			{
				continue;
			}

			// TODO: add support for exporting Level->Model?

			TArray<AActor*> LevelActors(Level->Actors); // iterate using copy, in case new temporary actors are added during conversion

			if (Builder.ExportOptions->VariantSetsMode != EGLTFVariantSetsMode::None)
			{
				for (const AActor* Actor : LevelActors)
				{
					// TODO: should a LevelVariantSet be exported even if not selected for export?
					if (const ALevelVariantSetsActor *LevelVariantSetsActor = Cast<ALevelVariantSetsActor>(Actor))
					{
						const ULevelVariantSets* LevelVariantSets = const_cast<ALevelVariantSetsActor*>(LevelVariantSetsActor)->GetLevelVariantSets(true);

						if (Builder.ExportOptions->VariantSetsMode == EGLTFVariantSetsMode::Epic)
						{
							if (LevelVariantSets != nullptr)
							{
								FGLTFJsonEpicLevelVariantSets* JsonEpicLevelVariantSets = Builder.AddUniqueEpicLevelVariantSets(LevelVariantSets);
								if (JsonEpicLevelVariantSets != nullptr)
								{
									Scene->EpicLevelVariantSets.Add(JsonEpicLevelVariantSets);
								}
							}
						}
						else if (Builder.ExportOptions->VariantSetsMode == EGLTFVariantSetsMode::Khronos)
						{
							if (LevelVariantSets != nullptr)
							{
								for (const UVariantSet* VariantSet: LevelVariantSets->GetVariantSets())
								{
									for (const UVariant* Variant: VariantSet->GetVariants())
									{
										Builder.AddUniqueKhrMaterialVariant(Variant);
									}
								}
							}
						}
					}
				}
			}

			for (const AActor* Actor : LevelActors)
			{
				FGLTFJsonNode* JsonNode = Builder.AddUniqueNode(Actor);
				if (JsonNode != nullptr && Builder.IsRootActor(Actor))
				{
					// TODO: to avoid having to add irrelevant actors/components let GLTFComponentConverter decide and add root nodes to Scene->
					// This change may require node converters to support cyclic calls.
					Scene->Nodes.Add(JsonNode);
				}
			}
		}
	}
	else
	{
		Builder.LogWarning(
			FString::Printf(TEXT("World %s has no levels. Please make sure the world has been fully initialized"),
			*World->GetName()));
	}

	return Scene;
}
