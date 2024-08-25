// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorUndoClient.h"
#include "Widgets/SCompoundWidget.h"

class IDetailsView;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	class SDMXControlConsoleEditorFixturePatchVerticalBox;

	/** View for displaying the dmx library handler for the edited Control Console */
	class SDMXControlConsoleEditorDMXLibraryView
		: public SCompoundWidget
		, public FSelfRegisteringEditorUndoClient
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorDMXLibraryView)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel);

	protected:
		//~ Begin FSelfRegisteringEditorUndoClient interface
		virtual bool MatchesContext(const FTransactionContext& InContext, const TArray<TPair<UObject*, FTransactionObjectEvent>>& TransactionObjectContexts) const override;
		virtual void PostUndo(bool bSuccess) override;
		virtual void PostRedo(bool bSuccess) override;
		//~ End FSelfRegisteringEditorUndoClient interface

	private:
		/** Updates FixturePatchVerticalBox widget */
		void UpdateFixturePatchVerticalBox();

		/** Called when a Property has changed in current Control Console Data */
		void OnControlConsoleDataPropertyChanged(const FPropertyChangedEvent& PropertyChangedEvent);

		/** Called when the DMX Library in use has been changed */
		void OnDMXLibraryChanged();

		/** Called when the DMX Library in use has been reloaded */
		void OnDMXLibraryReloaded();

		/** Reference to FixturePatchRows widgets container */
		TSharedPtr<SDMXControlConsoleEditorFixturePatchVerticalBox> FixturePatchVerticalBox;

		/** Shows DMX Control Console Data's details */
		TSharedPtr<IDetailsView> ControlConsoleDataDetailsView;

		/** Weak reference to the Control Console Editor Model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
