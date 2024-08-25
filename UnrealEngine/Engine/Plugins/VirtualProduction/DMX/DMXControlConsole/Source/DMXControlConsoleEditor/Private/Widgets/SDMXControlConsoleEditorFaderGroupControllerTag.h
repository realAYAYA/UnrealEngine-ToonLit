// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FSlateColor;


namespace UE::DMX::Private
{
	class FDMXControlConsoleFaderGroupControllerModel;

	/** A widget to display a color tag for the Fader Group Controller view */
	class SDMXControlConsoleEditorFaderGroupControllerTag
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupControllerTag)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TWeakPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel);

	private:
		/** Gets the fader group controller's editor color */
		FSlateColor GetFaderGroupControllerEditorColor() const;

		/** Gets the visibility state for the single slot tag */
		EVisibility GetSingleSlotTagVisibility() const;

		/** Gets the visibility state for the multiple slot tag */
		EVisibility GetMultiSlotTagVisibility() const;

		/** Weak Reference to the Fader Group Controller model */
		TWeakPtr<FDMXControlConsoleFaderGroupControllerModel> WeakFaderGroupControllerModel;
	};
}
