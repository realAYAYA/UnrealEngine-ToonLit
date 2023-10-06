// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/Reply.h"
#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FCurveEditor;
class FTabManager;
class IDetailsView;
class SWindow;
class UClass;
class UCurveEditorFilterBase;

class CURVEEDITOR_API SCurveEditorFilterPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCurveEditorFilterPanel)
	{}
		
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FCurveEditor> InCurveEditor, UClass* DefaultFilterClass);
	void SetFilterClass(UClass* InClass);

public:
	/** Call this to request opening a window containing this panel. */
	static TSharedPtr<SCurveEditorFilterPanel> OpenDialog(TSharedPtr<SWindow> RootWindow, TSharedRef<FCurveEditor> InHostCurveEditor, TSubclassOf<UCurveEditorFilterBase> DefaultFilterClass);
	
	/** Closes the dialog if there is one open. */
	static void CloseDialog();

	/** The details view for filter class properties */
	TSharedPtr<class IDetailsView> GetDetailsView() const { return DetailsView; }

	/** Delegate for when the chosen filter class has changed */
	FSimpleDelegate OnFilterClassChanged;

protected:
	FReply OnApplyClicked();
	bool CanApplyFilter() const;
	FText GetCurrentFilterText() const;
private:
	/** Weak pointer to the curve editor which created this filter. */
	TWeakPtr<FCurveEditor> WeakCurveEditor;

	/** The Details View in our UI that we update each time they choose a class. */
	TSharedPtr<IDetailsView> DetailsView;

	/** Singleton for the pop-up window. */
	static TWeakPtr<SWindow> ExistingFilterWindow;
};