// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Widgets/SCompoundWidget.h"

class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{ 
	/** Base widget for Control Console layout */
	class SDMXControlConsoleEditorLayout
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorLayout)
		{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorGlobalLayoutBase* InLayout, UDMXControlConsoleEditorModel* InEditorModel);

		/** Gets the Layout this view is based on */
		UDMXControlConsoleEditorGlobalLayoutBase* GetEditorLayout() const { return EditorLayout.IsValid() ? EditorLayout.Get() : nullptr; }

		/** Requests this layout to be refreshed */
		virtual void RequestRefresh();

	protected:
		/** True if refreshing layout is allowed */
		virtual bool CanRefresh() const { return false; }

		/** Refreshes layout */
		virtual void Refresh();

		/** Called when an Element is added to this layout */
		virtual void OnLayoutElementAdded() = 0;

		/** Should be called when a Fader Group was deleted from the this view displays */
		virtual void OnLayoutElementRemoved() = 0;

		/** Timer handle in use while refreshing layout is requested but not carried out yet */
		FTimerHandle RefreshLayoutTimerHandle;

		/** Weak Reference to the Editor Layout */
		TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> EditorLayout;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
