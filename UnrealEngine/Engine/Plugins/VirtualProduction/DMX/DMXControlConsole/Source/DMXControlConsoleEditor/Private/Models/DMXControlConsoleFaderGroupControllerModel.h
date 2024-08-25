// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/Controllers/DMXControlConsoleFaderGroupController.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class UDMXControlConsoleElementController;
class UDMXControlConsoleFaderGroup;
class UDMXControlConsoleFaderGroupController;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	/** Model for a fader group controller in a Control Console */
	class FDMXControlConsoleFaderGroupControllerModel
		: public TSharedFromThis<FDMXControlConsoleFaderGroupControllerModel>
	{
	public:
		/** Constructor */
		FDMXControlConsoleFaderGroupControllerModel(const TWeakObjectPtr<UDMXControlConsoleFaderGroupController> InWeakFaderGroupController, const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		/** Gets the Fader Group Controller this model is based on */
		UDMXControlConsoleFaderGroupController* GetFaderGroupController() const;

		/** Gets the first available Fader Group in the Fader Group Controller, if valid */
		UDMXControlConsoleFaderGroup* GetFirstAvailableFaderGroup() const;

		/** Gets an array with only Element Controllers that match the current filter */
		TArray<UDMXControlConsoleElementController*> GetMatchingFilterElementControllersOnly() const;

		/** Gets the name of the Fader Group Controller, relative to the contained Fader Groups */
		FString GetRelativeControllerName() const;

		/** True if the Controller has just one Fader Group */
		bool HasSingleFaderGroup() const;

		/** True if a new Fader Group Controller can be added next to this */
		bool CanAddFaderGroupController() const;

		/** True if a new Fader Group Controller can be added on next row */
		bool CanAddFaderGroupControllerOnNewRow() const;

		/** True if a new Element Controller can be added */
		bool CanAddElementController() const;

		/** True if all the Fader Groups in the Controller are locked */
		bool IsLocked() const;

	private:
		/** Weak reference to the Fader Group Controller this model is based on */
		TWeakObjectPtr<UDMXControlConsoleFaderGroupController> WeakFaderGroupController;

		/** Weak reference to the Control Console edior model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
	};
}
