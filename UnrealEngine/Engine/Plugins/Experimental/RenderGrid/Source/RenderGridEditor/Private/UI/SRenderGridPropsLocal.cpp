// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SRenderGridPropsLocal.h"
#include "RenderGrid/RenderGrid.h"
#include "IRenderGridEditor.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "SRenderGridPropsLocal"


BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void UE::RenderGrid::Private::SRenderGridPropsLocal::Construct(const FArguments& InArgs, TSharedPtr<IRenderGridEditor> InBlueprintEditor, URenderGridPropsSourceLocal* InPropsSource)
{
	BlueprintEditorWeakPtr = InBlueprintEditor;
	PropsSource = InPropsSource;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION


#undef LOCTEXT_NAMESPACE
