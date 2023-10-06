// Copyright Epic Games, Inc. All Rights Reserved.

#include "SSkeletalMeshEditorToolbox.h"

#include "ISkeletalMeshEditor.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SUniformWrapPanel.h"

void SSkeletalMeshEditorToolbox::Construct(
	const FArguments& InArgs, 
	const TSharedRef<ISkeletalMeshEditor>& InOwningEditor
	)
{
	ChildSlot
	[
		SAssignNew(InlineContentHolder, SBorder)
		.BorderImage( FAppStyle::GetBrush( "ToolPanel.GroupBorder" ) )
	];
}

void SSkeletalMeshEditorToolbox::AttachToolkit(const TSharedRef<IToolkit>& InToolkit)
{
	TSharedPtr<SWidget> Content = InToolkit->GetInlineContent();
	InlineContentHolder->SetContent(Content.ToSharedRef());
}

void SSkeletalMeshEditorToolbox::DetachToolkit(const TSharedRef<IToolkit>& InToolkit)
{
	InlineContentHolder->SetContent(SNullWidget::NullWidget);
}
