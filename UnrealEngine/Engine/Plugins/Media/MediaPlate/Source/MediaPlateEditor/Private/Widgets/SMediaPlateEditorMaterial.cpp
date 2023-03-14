// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaPlateEditorMaterial.h"

#include "MediaPlate.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SMediaPlateEditorMaterial"

/* SMediaPlateEditorMaterial interface
 *****************************************************************************/

void SMediaPlateEditorMaterial::Construct(const FArguments& InArgs, const TSharedRef<FMaterialItemView>& InMaterialItemView, UActorComponent* InCurrentComponent)
{
	TSharedRef<SVerticalBox> ResultWidget = SNew(SVerticalBox);

	// Get media plate.
	if (InCurrentComponent != nullptr)
	{
		MediaPlate = InCurrentComponent->GetOwner<AMediaPlate>();
		if (MediaPlate != nullptr)
		{
			// Add button for default material.
			ResultWidget->AddSlot()
				.AutoHeight()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
						.Text(LOCTEXT("ApplyDefaultMaterial", "Use Media Plate Default"))
						.ToolTipText(LOCTEXT("ApplyDefaultMaterialTooltip", "Set the material to the default Media Plate material."))
						.OnClicked(this, &SMediaPlateEditorMaterial::OnDefaultButtonClicked)
				];
		}
	}

	ChildSlot
	[
		ResultWidget
	];
}

FReply SMediaPlateEditorMaterial::OnDefaultButtonClicked() const
{
	if (MediaPlate != nullptr)
	{
		MediaPlate->UseDefaultMaterial();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
