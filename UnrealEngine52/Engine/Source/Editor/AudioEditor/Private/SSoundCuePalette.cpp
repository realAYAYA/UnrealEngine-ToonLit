// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSoundCuePalette.h"

#include "EdGraph/EdGraphSchema.h"
#include "SoundCueGraph/SoundCueGraphSchema.h"
#include "UObject/UObjectGlobals.h"

void SSoundCuePalette::Construct(const FArguments& InArgs)
{
	// Auto expand the palette as there's so few nodes
	SGraphPalette::Construct(SGraphPalette::FArguments().AutoExpandActionMenu(true));
}

void SSoundCuePalette::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	const USoundCueGraphSchema* Schema = GetDefault<USoundCueGraphSchema>();

	FGraphActionMenuBuilder ActionMenuBuilder;

	// Determine all possible actions
	Schema->GetPaletteActions(ActionMenuBuilder);

	//@TODO: Avoid this copy
	OutAllActions.Append(ActionMenuBuilder);
}
