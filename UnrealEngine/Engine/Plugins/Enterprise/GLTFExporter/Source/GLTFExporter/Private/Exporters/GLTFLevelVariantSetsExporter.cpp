// Copyright Epic Games, Inc. All Rights Reserved.

#include "Exporters/GLTFLevelVariantSetsExporter.h"
#include "Exporters/GLTFExporterUtility.h"
#include "Builders/GLTFContainerBuilder.h"
#include "LevelVariantSets.h"

UGLTFLevelVariantSetsExporter::UGLTFLevelVariantSetsExporter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = ULevelVariantSets::StaticClass();
}

bool UGLTFLevelVariantSetsExporter::AddObject(FGLTFContainerBuilder& Builder, const UObject* Object)
{
	const ULevelVariantSets* LevelVariantSets = CastChecked<ULevelVariantSets>(Object);

	if (Builder.ExportOptions->VariantSetsMode == EGLTFVariantSetsMode::None)
	{
		Builder.LogError(
			FString::Printf(TEXT("Failed to export level variant sets %s because variant sets are disabled by export options"),
			*LevelVariantSets->GetName()));
		return false;
	}

	TArray<UWorld*> Worlds = FGLTFExporterUtility::GetAssociatedWorlds(LevelVariantSets);
	if (Worlds.Num() == 0)
	{
		Builder.LogError(
			FString::Printf(TEXT("Failed to export level variant sets %s because no associated level"),
			*LevelVariantSets->GetName()));
		return false;
	}

	if (Worlds.Num() > 1)
	{
		Builder.LogError(
			FString::Printf(TEXT("Failed to export level variant sets %s because more than one associated level"),
			*LevelVariantSets->GetName()));
		return false;
	}

	const UWorld* World = Worlds[0];
	FGLTFJsonScene* Scene = Builder.AddUniqueScene(World);
	if (Scene == nullptr)
	{
		Builder.LogError(
			FString::Printf(TEXT("Failed to export level %s for level variant sets %s"),
			*World->GetName(),
			*LevelVariantSets->GetName()));
		return false;
	}

	if (Builder.ExportOptions->VariantSetsMode == EGLTFVariantSetsMode::Epic)
	{
		FGLTFJsonEpicLevelVariantSets* EpicLevelVariantSets = Builder.AddUniqueEpicLevelVariantSets(LevelVariantSets);
		if (EpicLevelVariantSets == nullptr)
		{
			Builder.LogError(
				FString::Printf(TEXT("Failed to export level variant sets %s"),
				*LevelVariantSets->GetName()));
			return false;
		}

		Scene->EpicLevelVariantSets.AddUnique(EpicLevelVariantSets);
	}
	else if (Builder.ExportOptions->VariantSetsMode == EGLTFVariantSetsMode::Khronos)
	{
		if (Builder.GetRoot().KhrMaterialVariants.Num() < 1)
		{
			Builder.LogError(
				FString::Printf(TEXT("Failed to export any supported variant from level variant sets %s"),
				*LevelVariantSets->GetName()));
			return false;
		}
	}

	Builder.DefaultScene = Scene;
	return true;
}
