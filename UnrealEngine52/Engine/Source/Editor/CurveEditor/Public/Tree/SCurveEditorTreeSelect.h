// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

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

class CURVEEDITOR_API SCurveEditorTreeSelect : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCurveEditorTreeSelect){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr<FCurveEditor> InCurveEditor, FCurveEditorTreeItemID InTreeItemID, const TSharedRef<ITableRow>& InTableRow);

private:

	FReply SelectAll();

	const FSlateBrush* GetSelectBrush() const;

	EVisibility GetSelectVisibility() const;

private:

	TWeakPtr<ITableRow> WeakTableRow;
	TWeakPtr<FCurveEditor> WeakCurveEditor;
	FCurveEditorTreeItemID TreeItemID;
};