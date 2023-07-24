// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CurveEditorTreeTraits.h"
#include "CurveEditorTypes.h"
#include "Input/Reply.h"
#include "Layout/Visibility.h"
#include "Templates/SharedPointer.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCurveEditor;
class ITableRow;
struct FSlateBrush;

class CURVEEDITOR_API SCurveEditorTreePin : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCurveEditorTreePin){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FCurveEditor> InCurveEditor, FCurveEditorTreeItemID InTreeItemID, const TSharedRef<ITableRow>& InTableRow);

private:

	FReply TogglePinned();
	const FSlateBrush* GetPinBrush() const;

	bool IsPinnedRecursive(FCurveEditorTreeItemID InTreeItem, FCurveEditor* CurveEditor) const;

	void PinRecursive(FCurveEditorTreeItemID InTreeItem, FCurveEditor* CurveEditor) const;

	void UnpinRecursive(FCurveEditorTreeItemID InTreeItem, FCurveEditor* CurveEditor, TArray<FCurveEditorTreeItemID>& OutUnpinnedItems) const;

	EVisibility GetPinVisibility() const;

private:

	TWeakPtr<ITableRow> WeakTableRow;
	TWeakPtr<FCurveEditor> WeakCurveEditor;
	FCurveEditorTreeItemID TreeItemID;
};