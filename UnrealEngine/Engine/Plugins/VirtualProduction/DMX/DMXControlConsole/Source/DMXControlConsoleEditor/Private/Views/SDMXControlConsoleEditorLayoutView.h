// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"

class SHorizontalBox;
class UDMXControlConsoleEditorGlobalLayoutBase;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	class SDMXControlConsoleEditorLayout;

	/** View for displaying layouts for the edited Control Console */
	class SDMXControlConsoleEditorLayoutView
		: public SCompoundWidget
		, public FSelfRegisteringEditorUndoClient
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorLayoutView)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel);

	protected:
		//~ Begin SWidget interface
		virtual bool SupportsKeyboardFocus() const override { return true; }
		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
		//~ End of SWidget interface

		//~ Begin FSelfRegisteringEditorUndoClient interface
		virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		//~ End FSelfRegisteringEditorUndoClient interface

	private:
		/** Updates Control Console layout, according to current layout mode */
		void UpdateLayout();

		/** True if the current layout widget's type name matches the given one */
		bool IsCurrentLayoutWidgetType(const FName& InWidgetTypeName) const;

		/** Called when the current Active Layout has changed */
		void OnActiveLayoutChanged(const UDMXControlConsoleEditorGlobalLayoutBase* ActiveLayout);

		/** Reference to the current layout widget */
		TSharedPtr<SDMXControlConsoleEditorLayout> Layout;

		/** Reference to the layout container box */
		TSharedPtr<SHorizontalBox> LayoutBox;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
