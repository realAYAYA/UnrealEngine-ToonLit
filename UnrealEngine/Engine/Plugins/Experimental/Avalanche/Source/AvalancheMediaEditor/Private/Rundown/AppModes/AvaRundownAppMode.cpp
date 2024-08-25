// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownAppMode.h"

#include "Rundown/AvaRundownEditor.h"

#define LOCTEXT_NAMESPACE "AvaRundownAppMode"

const FName FAvaRundownAppMode::DefaultMode(TEXT("DefaultMode"));

FAvaRundownAppMode::FAvaRundownAppMode(const TSharedPtr<FAvaRundownEditor>& InRundownEditor, const FName& InModeName)
	: FApplicationMode(InModeName, FAvaRundownAppMode::GetLocalizedMode)
	, RundownEditorWeak(InRundownEditor)
{
}

void FAvaRundownAppMode::RegisterTabFactories(TSharedPtr<FTabManager> InTabManager)
{
	TSharedPtr<FAvaRundownEditor> PlaybackEditor = RundownEditorWeak.Pin();
	PlaybackEditor->PushTabFactories(TabFactories);
	FApplicationMode::RegisterTabFactories(InTabManager);
}

TSharedPtr<FDocumentTabFactory> FAvaRundownAppMode::GetDocumentTabFactory(const FName& InName) const
{
	if (const TSharedRef<FDocumentTabFactory>* DocFactory = DocumentTabFactories.Find(InName))
	{
		return *DocFactory;
	}

	return nullptr;
}

FText FAvaRundownAppMode::GetLocalizedMode(const FName InMode)
{
	static TMap<FName, FText> LocModes;

	if (LocModes.Num() == 0)
	{
		LocModes.Add(DefaultMode, LOCTEXT("Rundown_DefaultMode", "Default"));
	}

	check(InMode != NAME_None);
	const FText* OutDesc = LocModes.Find(InMode);
	check(OutDesc);
	
	return *OutDesc;
}

#undef LOCTEXT_NAMESPACE
