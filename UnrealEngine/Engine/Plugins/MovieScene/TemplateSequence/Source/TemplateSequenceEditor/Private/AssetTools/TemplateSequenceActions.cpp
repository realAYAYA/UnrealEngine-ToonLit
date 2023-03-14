// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/TemplateSequenceActions.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "TemplateSequence.h"
#include "TemplateSequenceEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FTemplateSequenceActions::FTemplateSequenceActions(const TSharedRef<ISlateStyle>& InStyle, const EAssetTypeCategories::Type InAssetCategory)
	: Style(InStyle)
	, AssetCategory(InAssetCategory)
{ }

uint32 FTemplateSequenceActions::GetCategories()
{
	return AssetCategory;
}

FText FTemplateSequenceActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_TemplateSequence", "Template Sequence");
}

UClass* FTemplateSequenceActions::GetSupportedClass() const
{
	return UTemplateSequence::StaticClass();
}

FColor FTemplateSequenceActions::GetTypeColor() const
{
	return FColor(200, 80, 80);
}

void FTemplateSequenceActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	UWorld* WorldContext = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::Editor)
		{
			WorldContext = Context.World();
			break;
		}
	}

	if (!ensure(WorldContext))
	{
		return;
	}

	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UTemplateSequence* TemplateSequence = Cast<UTemplateSequence>(*ObjIt);

		if (TemplateSequence != nullptr)
		{
			TSharedRef<FTemplateSequenceEditorToolkit> Toolkit = MakeShareable(new FTemplateSequenceEditorToolkit(Style));
			FTemplateSequenceToolkitParams ToolkitParams;
			InitializeToolkitParams(ToolkitParams);
			Toolkit->Initialize(Mode, EditWithinLevelEditor, TemplateSequence, ToolkitParams);
		}
	}
}

void FTemplateSequenceActions::InitializeToolkitParams(FTemplateSequenceToolkitParams& ToolkitParams) const
{
}

bool FTemplateSequenceActions::ShouldForceWorldCentric()
{
	// @todo sequencer: Hack to force world-centric mode for Sequencer
	return true;
}


#undef LOCTEXT_NAMESPACE
