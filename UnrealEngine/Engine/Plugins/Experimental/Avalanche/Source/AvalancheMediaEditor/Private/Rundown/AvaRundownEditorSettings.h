// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "Rundown/AvaRundownEditorDefines.h"
#include "AvaRundownEditorSettings.generated.h"

class UAvaRundownMacroCollection;

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Rundown"))
class UAvaRundownEditorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UAvaRundownEditorSettings();

	static const UAvaRundownEditorSettings* Get();
	static UAvaRundownEditorSettings* GetMutable();

	/**
	 * Configuring the default page action when closing the editor.
	 * By default it will stop the pages.
	 */
	UPROPERTY(Config, EditAnywhere, Category="Rundown")
	bool bShouldStopPagesOnClose = true;
	
	/** Shows the checker board in preview */
	UPROPERTY(Config, EditAnywhere, Category="Preview")
	bool bPreviewCheckerBoard = false;

	/** Current macro collection used by the rundown editor. */
	UPROPERTY(Config, EditAnywhere, Category="Macros")
	TSoftObjectPtr<UAvaRundownMacroCollection> MacroCollection;
	
	UPROPERTY(Config, EditAnywhere, Category="Page Actions")
	EAvaRundownPageSet PreviewContinueActionPageSet = EAvaRundownPageSet::SelectedOrPlaying;

	UPROPERTY(Config, EditAnywhere, Category="Page Actions")
	EAvaRundownPageSet PreviewOutActionPageSet = EAvaRundownPageSet::SelectedOrPlaying;
	
	UPROPERTY(Config, EditAnywhere, Category="Page Actions")
	EAvaRundownPageSet ContinueActionPageSet = EAvaRundownPageSet::SelectedOrPlaying;

	UPROPERTY(Config, EditAnywhere, Category="Page Actions")
	EAvaRundownPageSet TakeOutActionPageSet = EAvaRundownPageSet::SelectedOrPlaying;

	UPROPERTY(Config, EditAnywhere, Category="Page Actions")
	EAvaRundownPageSet UpdateValuesActionPageSet = EAvaRundownPageSet::SelectedOrPlaying;

	/** Whether rundown server is started automatically. */
	UPROPERTY(Config, EditAnywhere, Category="Server")
	bool bAutoStartRundownServer = false;

	/** Name given to the rundown server. If empty, the server name will be the host name. */
	UPROPERTY(Config, EditAnywhere, Category="Server")
	FString RundownServerName;
};
