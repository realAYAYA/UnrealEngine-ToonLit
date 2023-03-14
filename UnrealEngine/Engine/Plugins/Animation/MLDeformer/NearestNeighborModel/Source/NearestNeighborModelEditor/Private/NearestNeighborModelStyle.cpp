// Copyright Epic Games, Inc. All Rights Reserved.

#include "NearestNeighborModelStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateTypes.h"
#include "Math/Color.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace UE::NearestNeighborModel
{
	#define MLDEFORMER_IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

	FNearestNeighborModelEditorStyle::FNearestNeighborModelEditorStyle() : FSlateStyleSet("NearestNeighborModelEditorStyle")
	{
		const FString ResourceDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("NearestNeighborModel"))->GetBaseDir(), TEXT("Resources"));
		SetContentRoot(ResourceDir);

		// Colors and sizes.
		Set("NearestNeighborModel.NearestNeighborActors.WireframeColor", FLinearColor(1.0f, 1.0f, 0.0f));
		Set("NearestNeighborModel.NearestNeighborActors.LabelColor", FLinearColor(1.0f, 1.0f, 0.0f));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	FNearestNeighborModelEditorStyle::~FNearestNeighborModelEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

	FNearestNeighborModelEditorStyle& FNearestNeighborModelEditorStyle::Get()
	{
		static FNearestNeighborModelEditorStyle Inst;
		return Inst;
	}
}	// namespace UE::NearestNeighborModel

#undef MLDEFORMER_IMAGE_BRUSH
