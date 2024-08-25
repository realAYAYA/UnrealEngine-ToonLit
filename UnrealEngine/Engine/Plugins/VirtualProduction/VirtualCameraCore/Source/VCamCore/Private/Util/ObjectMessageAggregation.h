// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "UObject/NameTypes.h"

class UObject;

namespace UE::VCamCore
{
	struct FAggregatedNotification
	{
		/** Messages are grouped by identifier. */
		FName Identifier;

		/**
		 * The title of the notification.
		 * It should be the same for all notifications with the same identifier for the same object, e.g. "Missing viewport 1" 
		 */
		FText Title;

		/** Specific data that is combined in the notification's subtext section. */
		FText Subtext;
	};

	/** Output provider message displayed when the target viewport is not currently open in the UI. */
	extern const FName NotificationKey_MissingTargetViewport;

	/**
	 * Adds a notification.
	 * If compiling against the editor, a notification is added to the bottom-right of the screen.
	 * A log is generated regardless of compile target.
	 *
	 * The notification is logged immediately. To avoid UI spam, at the end of the frame, all notifications are aggregated
	 * per notification identifier.
	 * 
	 * @param ContextObject The object for which this notification is generated. If multiple messages are received in the same frame, they are combined.
	 * @param Notification The notification about this object
	 */
	void AddAggregatedNotification(UObject& ContextObject, FAggregatedNotification Notification);
}
