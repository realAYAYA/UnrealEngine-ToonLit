// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/Extensions/DynamicExtensionContainer.h"
#include "MVVM/Extensions/ICurveEditorTreeItemExtension.h"
#include "CurveDataAbstraction.h"
#include "CurveEditorTypes.h"
#include "Tree/ICurveEditorTreeItem.h"

class ISequencer;
class IPropertyTypeCustomization;
class SCurveEditorTree;
struct FTimeSliderArgs;

namespace UE
{
namespace Sequencer
{

class FSequencerEditorViewModel;

class SEQUENCER_API FCurveEditorExtension : public IDynamicExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FCurveEditorExtension)

	FCurveEditorExtension();

	virtual void OnCreated(TSharedRef<FViewModel> InWeakOwner) override;

	/** Creates the curve editor view-model and widget */
	void CreateCurveEditor(const FTimeSliderArgs& TimeSliderArgs);

	/** Gets the curve editor view-model */
	TSharedPtr<FCurveEditor> GetCurveEditor() const { return CurveEditorModel; }

	/** Opens the curve editor */
	void OpenCurveEditor();
	/** Returns whether the curve editor is open */
	bool IsCurveEditorOpen() const;
	/** Closes the curve editor */
	void CloseCurveEditor();

	/**
	 * Synchronize curve editor selection with sequencer outliner selection on the next update.
	 */
	void RequestSyncSelection();

public:

	static const FName CurveEditorTabName;

private:

	/** Get the default key attributes to apply to newly created keys on the curve editor */
	FKeyAttributes GetDefaultKeyAttributes() const;

	/** Syncs the selection of the curve editor with that of the sequencer */
	void SyncSelection();

private:

	/** The sequencer editor we are extending with a curve editor */
	TWeakPtr<FSequencerEditorViewModel> WeakOwnerModel;

	/** Curve editor */
	TSharedPtr<FCurveEditor> CurveEditorModel;
	/** Curve editor tree widget */
	TSharedPtr<SCurveEditorTree> CurveEditorTreeView;
	/** The search widget for filtering curves in the Curve Editor tree. */
	TSharedPtr<SWidget> CurveEditorSearchBox;
	/** The curve editor panel. This is created and updated even if it is not currently visible. */
	TSharedPtr<SWidget> CurveEditorPanel;

	friend class FCurveEditorIntegrationExtension;
};

} // namespace Sequencer
} // namespace UE

