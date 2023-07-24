// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/Extensions/DynamicExtensionContainer.h"
#include "MVVM/Extensions/ICurveEditorTreeItemExtension.h"
#include "CurveEditorTypes.h"
#include "Tree/ICurveEditorTreeItem.h"

namespace UE
{
namespace Sequencer
{

class FSequenceModel;
class FCurveEditorExtension;

/**
 * Extension for managing integration between outliner items and the curve editor.
 *
 * It relies on the following:
 *
 * - Extension owner (generally the root view-model) implements ICurveEditorExtension, to get
 *   access to the curve editor itself.
 *
 * - Outliner items implementing ICurveEditorTreeItemExtension (or its default shim) if they 
 *   want to show up in the curve editor.
 */
class SEQUENCER_API FCurveEditorIntegrationExtension : public IDynamicExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FCurveEditorIntegrationExtension)

	FCurveEditorIntegrationExtension();

	virtual void OnCreated(TSharedRef<FViewModel> InWeakOwner) override;

	/** 
	 * Keeps the curve editor items up-to-date with the sequencer outliner by adding/removing 
	 * entries as needed.
	 */
	void UpdateCurveEditor();

	/**
	 * Clears the curve editor of all contents.
	 */
	void ResetCurveEditor();

private:

	/** Update curve editor items when sequencer outliner items change */
	void OnHierarchyChanged();

	/** Adds the given view-model to the curve editor */
	FCurveEditorTreeItemID AddToCurveEditor(TViewModelPtr<ICurveEditorTreeItemExtension> InViewModel, TSharedPtr<FCurveEditor> InCurveEditor);

	/** Finds the curve editor extension on the top-level sequencer editor view-model */
	FCurveEditorExtension* GetCurveEditorExtension();

private:

	TWeakPtr<FSequenceModel> WeakOwnerModel;
	TMap<TWeakPtr<FViewModel>, FCurveEditorTreeItemID> ViewModelToTreeItemIDMap;
};

} // namespace Sequencer
} // namespace UE

