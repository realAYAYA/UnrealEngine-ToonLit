// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetDefinition_CameraAnimationSequence.h"
#include "TemplateSequenceEditorToolkit.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void UAssetDefinition_CameraAnimationSequence::InitializeToolkitParams(FTemplateSequenceToolkitParams& ToolkitParams) const
{
    UAssetDefinition_TemplateSequence::InitializeToolkitParams(ToolkitParams);

    ToolkitParams.bCanChangeBinding = false;
}

#undef LOCTEXT_NAMESPACE
