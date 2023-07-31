// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NiagaraActions.h"
#include "SlateFwd.h"
#include "SGraphActionMenu.h"
#include "EdGraph/EdGraphSchema.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

struct FCreateWidgetForActionData;

DECLARE_DELEGATE_RetVal_OneParam(bool, FOnActionClicked, TSharedPtr<FNiagaraMenuAction_Generic>)

struct FCreateNiagaraWidgetForActionData
{
	FCreateNiagaraWidgetForActionData(const TSharedPtr<FNiagaraMenuAction_Generic> InAction)
		: Action(InAction)
	{}
	const TSharedPtr<FNiagaraMenuAction_Generic> Action; 
	/** True if we want to use the mouse delegate */
	bool bHandleMouseButtonDown;
	/** The delegate to determine if the current action is selected in the row */
	FIsSelected IsRowSelectedDelegate;
	/** The text to highlight */
	TAttribute<FText> HighlightText;
	/** True if the widget should be read only - no renaming allowed */
	bool bIsReadOnly = true;
};

/** Custom widget for GraphActionMenu */
class NIAGARAEDITOR_API SNiagaraGraphActionWidget : public SCompoundWidget
{
	public:

	SLATE_BEGIN_ARGS( SNiagaraGraphActionWidget ) {}
	SLATE_ATTRIBUTE(FText, HighlightText)
SLATE_END_ARGS()

void Construct(const FArguments& InArgs, const FCreateWidgetForActionData* InCreateData);
	virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

	private:
	/** The item that we want to display with this widget */
	TWeakPtr<struct FEdGraphSchemaAction> ActionPtr;
	/** Delegate executed when mouse button goes down */
	FCreateWidgetMouseButtonDown MouseButtonDownDelegate;
};

/** Custom widget for GraphActionMenu */
class NIAGARAEDITOR_API SNiagaraActionWidget : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SNiagaraActionWidget )
		: _bShowTypeIfParameter(true)
	{} 
		SLATE_ARGUMENT(bool, bShowTypeIfParameter)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FCreateNiagaraWidgetForActionData& InCreateData);
	//virtual FReply OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent ) override;

private:
	/** The item that we want to display with this widget */
	TSharedPtr<struct FNiagaraMenuAction_Generic> ActionPtr;
	/** Delegate executed when mouse button goes down */
	FOnActionClicked MouseButtonDownDelegate;
	FNiagaraActionSourceData SourceData;
};
