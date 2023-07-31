// Copyright Epic Games, Inc. All Rights Reserved.

#include "SLiveLinkXRSourceFactory.h"
#include "LiveLinkXR.h"
#include "LiveLinkXRSourceSettings.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"

#if WITH_EDITOR
#include "DetailLayoutBuilder.h"
#endif //WITH_EDITOR

#define LOCTEXT_NAMESPACE "SLiveLinkXRSourceFactory"

void SLiveLinkXRSourceFactory::Construct(const FArguments& Args)
{
#if WITH_EDITOR
	OnConnectionSettingsAccepted = Args._OnConnectionSettingsAccepted;

	FStructureDetailsViewArgs StructureViewArgs;
	FDetailsViewArgs DetailArgs;
	DetailArgs.bAllowSearch = false;
	DetailArgs.bShowScrollBar = false;

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	StructOnScope = MakeShared<FStructOnScope>(FLiveLinkXRConnectionSettings::StaticStruct());
	CastChecked<UScriptStruct>(StructOnScope->GetStruct())->CopyScriptStruct(StructOnScope->GetStructMemory(), &ConnectionSettings);
	StructureDetailsView = PropertyEditor.CreateStructureDetailView(DetailArgs, StructureViewArgs, StructOnScope);

	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			StructureDetailsView->GetWidget().ToSharedRef()
		]
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Right)
		.AutoHeight()
		[
			SNew(SButton)
			.OnClicked(this, &SLiveLinkXRSourceFactory::OnSettingsAccepted)
			.Text(LOCTEXT("AddSource", "Add"))
		]
	];
#endif //WITH_EDITOR
}

FReply SLiveLinkXRSourceFactory::OnSettingsAccepted()
{
#if WITH_EDITOR
	CastChecked<UScriptStruct>(StructOnScope->GetStruct())->CopyScriptStruct(&ConnectionSettings, StructOnScope->GetStructMemory());
	OnConnectionSettingsAccepted.ExecuteIfBound(ConnectionSettings);
#endif //WITH_EDITOR

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE