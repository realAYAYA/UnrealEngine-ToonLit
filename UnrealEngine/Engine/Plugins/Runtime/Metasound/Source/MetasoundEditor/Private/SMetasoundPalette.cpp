// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMetasoundPalette.h"
#include "MetasoundEditorGraphSchema.h"

void SMetasoundPalette::Construct(const FArguments& InArgs)
{
	SGraphPalette::Construct(SGraphPalette::FArguments().AutoExpandActionMenu(true));
}

void SMetasoundPalette::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	if (const UMetasoundEditorGraphSchema* Schema = GetDefault<UMetasoundEditorGraphSchema>())
	{
		FGraphActionMenuBuilder ActionMenuBuilder;
		Schema->GetPaletteActions(ActionMenuBuilder);
		OutAllActions.Append(ActionMenuBuilder);
	}
}
