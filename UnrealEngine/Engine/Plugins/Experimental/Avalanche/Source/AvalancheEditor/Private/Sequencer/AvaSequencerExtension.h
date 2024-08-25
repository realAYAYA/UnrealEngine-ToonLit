// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaSequencerArgs.h"
#include "IAvaEditorExtension.h"
#include "IAvaSequencerProvider.h"
#include "Misc/EngineVersionComparison.h"

class FText;
class IAvaSequencer;
class ISequencer;
class SWidget;
class UObject;

UE_AVA_TYPE_EXTERNAL(IAvaSequencerProvider);

class FAvaSequencerExtension: public FAvaEditorExtension, public IAvaSequencerProvider
{
public:
	UE_AVA_INHERITS(FAvaSequencerExtension, FAvaEditorExtension, IAvaSequencerProvider);

	FAvaSequencerExtension();

	explicit FAvaSequencerExtension(FName InSequencerTabId, bool bInSupportsDrawerWidget);

	TSharedPtr<IAvaSequencer> GetAvaSequencer() const { return AvaSequencer; }

	TSharedPtr<ISequencer> GetSequencer() const;

	void OnObjectRenamed(UObject* InRenamedObject, const FText& InDisplayNameText);

	FName GetSequencerTabId() const { return SequencerTabId; }

	virtual FAvaSequencerArgs MakeSequencerArgs() const;

	//~ Begin IAvaEditorExtension
	virtual void Activate() override;
	virtual void Deactivate() override;
	virtual void Cleanup() override;
	virtual void RegisterTabSpawners(const TSharedRef<IAvaEditor>& InEditor) const override;
	virtual void ExtendLevelEditorLayout(FLayoutExtender& InExtender) const override;
	virtual void NotifyOnSelectionChanged(const FAvaEditorSelection& InSelection) override;
	virtual void OnCopyActors(FString& OutExtensionData, TConstArrayView<AActor*> InActorsToCopy) override;
	virtual void OnPasteActors(FStringView InPastedData, TConstArrayView<FAvaEditorPastedActor> InPastedActors) override;
	//~ End IAvaEditorExtension

	//~ Begin IAvaSequencerProvider
	virtual IAvaSequenceProvider* GetSequenceProvider() const override;
	virtual FEditorModeTools* GetSequencerModeTools() const override;
	virtual IAvaSequencePlaybackObject* GetPlaybackObject() const override;
	virtual TSharedPtr<IToolkitHost> GetSequencerToolkitHost() const override;
	virtual UObject* GetPlaybackContext() const override;
	virtual bool CanEditOrPlaySequences() const override;
	virtual bool CanExportSequences() const override { return true; }
	virtual void ExportSequences(TConstArrayView<UAvaSequence*> InSequencesToExport);
	//~ End IAvaSequencerProvider

protected:
	TSharedPtr<IAvaSequencer> AvaSequencer;

private:
	void BindDelegates();

	void UnbindDelegates();

	TSharedRef<SWidget> GetSequenceDrawerWidget();

	void RegisterSequenceDrawerWidget();

	void UnregisterSequenceDrawerWidget();

	/** Temporary fix ensuring that any Director Blueprint that got renamed to the incorrect outer gets fixed to the correct outer*/
	void ValidateDirectorBlueprints();

	/** The Drawer Widget for the Animations */
	TSharedPtr<SWidget> SequencerDrawerWidget;

	const FName SequencerTabId;

	const bool bSupportsDrawerWidget;
};
