// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTools/MediaPlateActions.h"
#include "ContentBrowserModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IContentBrowserSingleton.h"
#include "MediaPlateComponent.h"
#include "MediaTexture.h"
#include "Misc/PackageName.h"
#include "Toolkits/MediaPlateEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

/* FMediaPlateActions constructors
 *****************************************************************************/

FMediaPlateActions::FMediaPlateActions(const TSharedRef<ISlateStyle>& InStyle)
	: Style(InStyle)
{
}

/* FAssetTypeActions_Base interface
 *****************************************************************************/

bool FMediaPlateActions::CanFilter()
{
	return true;
}

uint32 FMediaPlateActions::GetCategories()
{
	return EAssetTypeCategories::Media;
}

FText FMediaPlateActions::GetName() const
{
	return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_MediaPlate", "Media Plate");
}

UClass* FMediaPlateActions::GetSupportedClass() const
{
	return UMediaPlateComponent::StaticClass();
}

FColor FMediaPlateActions::GetTypeColor() const
{
	return FColor::Red;
}

void FMediaPlateActions::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid()
		? EToolkitMode::WorldCentric
		: EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UMediaPlateComponent* MediaPlate = Cast<UMediaPlateComponent>(*ObjIt);

		if (MediaPlate != nullptr)
		{
			TSharedRef<FMediaPlateEditorToolkit> EditorToolkit = MakeShareable(new FMediaPlateEditorToolkit(Style));
			EditorToolkit->Initialize(MediaPlate, Mode, EditWithinLevelEditor);
		}
	}
}

#undef LOCTEXT_NAMESPACE
