// Copyright Epic Games, Inc. All Rights Reserved.

#include "CameraAnimationSequenceActions.h"
#include "CameraAnimationSequence.h"
#include "CineCameraActor.h"
#include "TemplateSequenceEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FCameraAnimationSequenceActions::FCameraAnimationSequenceActions(const TSharedRef<ISlateStyle>& InStyle, const EAssetTypeCategories::Type InAssetCategory)
    : FTemplateSequenceActions(InStyle, InAssetCategory)
{
}

FText FCameraAnimationSequenceActions::GetName() const
{
    return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_CameraAnimationSequence", "Camera Animation Sequence");
}

UClass* FCameraAnimationSequenceActions::GetSupportedClass() const
{
    return UCameraAnimationSequence::StaticClass();
}

void FCameraAnimationSequenceActions::InitializeToolkitParams(FTemplateSequenceToolkitParams& ToolkitParams) const
{
    FTemplateSequenceActions::InitializeToolkitParams(ToolkitParams);

    ToolkitParams.bCanChangeBinding = false;
}

#undef LOCTEXT_NAMESPACE
