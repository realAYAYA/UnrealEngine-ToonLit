// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if NV_GEFORCENOW

#include "CoreMinimal.h"
#include "Layout/SlateRect.h"
#include "Containers/Ticker.h"

class SWidget;
class SWindow;

enum class ETrackedSlateWidgetOperations : uint8;

/**
 * Representation of a GeForce NOW action zone.
 */
class FWidgetGFNActionZone
{
public:

	FWidgetGFNActionZone(const SWidget* InWidget);

	/** Updates the action zone's values and notify the GeForce NOW API if the rect of the action zone changed or removes the action zone if it is no longer interactable. */
	void UpdateActionZone(TArray<TSharedRef<SWindow>>& SlateWindows);
	/** Tells the GeForce NOW API to remove this action zone. */
	void ClearActionZone();

	/** Returns the ID of this action zone. */
	unsigned int GetID() const;

private:

	const SWidget* Widget;
	FSlateRect ActionZoneRect;

	bool bWasInteractable = false;
	bool bHasActionZone = false;

public:

	inline bool operator==(const FWidgetGFNActionZone& OtherWidgetGFNActionZone) const
	{
		return Widget == OtherWidgetGFNActionZone.Widget;
	}

	inline bool operator==(const SWidget* OtherWidget) const
	{
		return Widget == OtherWidget;
	}

};

/**
 * Singleton that manages the Action Zones for GeForce NOW.
 * Action Zones are rects that overlay the game stream on the user's end that when pressed can trigger various GeForce NOW features like the Native Virtual Keyboard.
 */
class GEFORCENOWWRAPPER_API GeForceNOWActionZoneProcessor : public TSharedFromThis<GeForceNOWActionZoneProcessor>
{
public:

	/** Initializes the Action Zone Processor by hooking onto the Slate Widget Tracker and requesting the tracked widgets we are interested in. 
	 * Return true if initialization was successful.
	 */
	bool Initialize();
	/** Terminates the action Zone Processor by unhooking from the Slate Widget Tracker and clearing all action zones. */
	void Terminate();

private:

	/** Handles what happens when a widget is added or removed. */
	void HandleTrackedWidgetChanges(const SWidget* Widget, const FName& Tag, ETrackedSlateWidgetOperations Operation);

	/** Creates an action zone when a widget is added. */
	void HandleEditableTextWidgetRegistered(const SWidget* Widget);
	/** Removes the action zone of the removed widget. */
	void HandleEditableTextWidgetUnregistered(const SWidget* Widget);

	/** Calls an update on all the action zones the processor has. */
	bool ProcessGFNWidgetActionZones(float DeltaTime);
	/** Creates a ticker that calls ProcessGFNWidgetActionZones at an interval using GFNWidgetActionZonesProcessDelay. */
	void StartProcess();
	/** Removes the ticker that calls ProcessGFNWidgetActionZones. */
	void StopProcess();

	TArray<FWidgetGFNActionZone> GFNWidgetActionZones;

	FTSTicker::FDelegateHandle ProcessDelegateHandle;
};

#endif // NV_GEFORCENOW