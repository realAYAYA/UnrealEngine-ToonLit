// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Factories/Factory.h"
#include "Profile/MediaProfile.h"
#include "Toolkits/SimpleAssetEditor.h"

class FExtender;

/** Viewer/editor for a MediaProfile */
class FMediaProfileEditorToolkit : public FSimpleAssetEditor
{

private:
	using Super = FSimpleAssetEditor;

public:
	static TSharedRef<FMediaProfileEditorToolkit> CreateEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UMediaProfile* InMediaProfile);

	virtual ~FMediaProfileEditorToolkit();

	/**
	 * Edits the specified table
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	InMediaProfile			The MediaProfile asset to edit
	 */
	void InitMediaProfileEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UMediaProfile* InMediaProfile);

	/**
	 * Binds our UI commands to delegates
	 */
	void BindCommands();

protected:

	virtual void SaveAsset_Execute() override;
	virtual void SaveAssetAs_Execute() override;
	virtual bool OnRequestClose() override;

private:

	/** Builds the toolbar widget for media profile editor */
	void ExtendToolBar();

	/** Applies changes to the media profile */
	void ApplyMediaProfile();

	/** Handle object property changes */
	void HandleCoreObjectPropertyChanged(UObject* Object, struct FPropertyChangedEvent& ChangedEvent);

	/** Handle pre object property changes */
	void HandleCorePreObjectPropertyChanged(UObject* Object, const class FEditPropertyChain& EditPropertyChain);

	/** Handle the project proxies changed */
	void HandleMediaProxiesChanged();

private:
	/** Details view */
	TSharedPtr<class IDetailsView> DetailsView;
	
	/** Whether the last modified object was a sub property of MediaProfile */
	bool bSubPropertyWasModified;
};