// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

enum class ECheckBoxState : uint8;
class UDMXControlConsoleEditorModel;
class UDMXEntityFixturePatch;


namespace UE::DMX::Private
{
	/** Model for a row in the control console fixture patch list */
	class FDMXControlConsoleFixturePatchListRowModel
		: public TSharedFromThis<FDMXControlConsoleFixturePatchListRowModel>
	{
	public:
		FDMXControlConsoleFixturePatchListRowModel(const TWeakObjectPtr<UDMXEntityFixturePatch> InWeakFixturePatch, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		/** Returns true if the row widget is enabled */
		bool IsRowEnabled() const;

		/** Returns the fader group enable state. Checked means it is enabled. */
		ECheckBoxState GetFaderGroupEnabledState() const;

		/** Sets if the fader group is enabled. */
		void SetFaderGroupEnabled(bool bEnable);

	private:
		/** The fixture patch of this row */
		TWeakObjectPtr<UDMXEntityFixturePatch> WeakFixturePatch;

		/** Weak reference to the Control Console edior model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
	};
}
