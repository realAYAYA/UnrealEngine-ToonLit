// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Misc/NotifyHook.h"
#include "EditorUndoClient.h"
#include "UObject/WeakObjectPtrTemplates.h"

struct FPropertyChangedEvent;

class ISequencer;
class FStructOnScope;
class UMovieSceneSection;
class IPropertyTypeCustomization;

struct FKeyEditData
{
	TSharedPtr<FStructOnScope> KeyStruct;
	TWeakObjectPtr<UMovieSceneSection> OwningSection;
};

/**
 * Widget that represents a details panel that refreshes on undo, and handles modification of the section on edit
 */
class MOVIESCENETOOLS_API SKeyEditInterface : public SCompoundWidget, public FEditorUndoClient, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SKeyEditInterface){}
		SLATE_ATTRIBUTE(FKeyEditData, EditData)
	SLATE_END_ARGS()

	~SKeyEditInterface();

	void Construct(const FArguments& InArgs, TSharedRef<ISequencer> InSequencer);

	/**
	 * (Re)Initialize this widget's details panel
	 */
	void Initialize();

private:

	/**
	 * Create a binding ID customization for the details panel
	 */
	TSharedRef<IPropertyTypeCustomization> CreateBindingIDCustomization();
	TSharedRef<IPropertyTypeCustomization> CreateFrameNumberCustomization();
	TSharedRef<IPropertyTypeCustomization> CreateEventCustomization();

	/**
	 * Called when a property has been changed on the UI
	 */
	void OnFinishedChangingProperties(const FPropertyChangedEvent& ChangeEvent, TSharedPtr<FStructOnScope> KeyStruct);

private:

	virtual FVector2D ComputeDesiredSize(float LayoutScaleMultiplier) const override;

	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	TAttribute<FKeyEditData> EditDataAttribute;
	TWeakObjectPtr<UMovieSceneSection> WeakSection;
	TWeakPtr<ISequencer> WeakSequencer;
};