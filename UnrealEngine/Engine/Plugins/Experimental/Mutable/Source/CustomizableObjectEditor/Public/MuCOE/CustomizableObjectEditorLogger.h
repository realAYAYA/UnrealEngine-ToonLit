// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/Attribute.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "Templates/SharedPointer.h"

class FCustomizableObjectEditorLogger;
class FName;
class SNotificationItem;

/** Notification categories. */
enum class ELoggerCategory
{
	General,
	GraphSearch,
	Compilation
};


/** Log parameters. Holds all parameters required to create a new CustomizableObjectEditorLogger log. */
class FLogParameters
{
	friend class FCustomizableObjectEditorLogger;

public:
	// Optional parameters setters
	FLogParameters& SubText(const FText& SubText);
	FLogParameters& SubText(FText&& SubText);
	
	FLogParameters& Category(ELoggerCategory Category);

	FLogParameters& Severity(EMessageSeverity::Type MessageSeverity);

	FLogParameters& Nodes(const TArray<const UCustomizableObjectNode*>& Nodes);

	FLogParameters& Node(const UCustomizableObjectNode& Node);

	FLogParameters& BaseObject(bool BaseObject = true);

	FLogParameters& CustomNotification(bool CustomNotification = true);
	
	FLogParameters& Notification(bool Notification = true);

	FLogParameters& FixNotification(bool FixNotification = true);
	
	/** Actually display the log. */
	void Log();
	
private:
	/** Constructor. Contains all required parameters. */
	FLogParameters(FCustomizableObjectEditorLogger& Logger, const FText& Text);
	FLogParameters(FCustomizableObjectEditorLogger& Logger, FText&& Text);
	
	FCustomizableObjectEditorLogger& Logger;

	// Required parameters
	const FText ParamText;

	// Optional parameters
	TAttribute<FText> ParamSubText;
	
	ELoggerCategory ParamCategory = ELoggerCategory::General;
	
	EMessageSeverity::Type ParamSeverity = EMessageSeverity::Info;
	
	TArray<const UCustomizableObjectNode*> ParamNodes;

	bool bParamBaseObject = false;

	bool bParamCustomNotification = false;
	
	bool bParamNotification = true;

	bool bParamFixNotification = false;
};


/** Customizable Object Editor Module Logger. Common log utilities.
 *
 * Avoids bombarding the user with many notifications. The Logger will only shows a notification per category.
 * Any new notification will override the previous one. */
class FCustomizableObjectEditorLogger
{
public:
	/** Create a new log. */
	static FLogParameters CreateLog(const FText& Text);
	static FLogParameters CreateLog(FText&& Text);

	/** Display the log. Equivalent to FLogParameters::Log(). */
	void Log(FLogParameters& LogParameters);

	/** Dismiss a fixed notification. */
	static void DismissNotification(ELoggerCategory Category);
	
private:
	/** Category data. */
	struct FCategoryData
	{
		/** Pointer to the last notification widget. */
		TWeakPtr<SNotificationItem> Notification;

		/** Number of new messages throw to this category since the notification was shown. */
		uint32 NumMessages = 0;
	};

	/** Data for each category. */
	TMap<ELoggerCategory, FCategoryData> CategoriesData;

	const static FName LOG_NAME;

	const static float NOTIFICATION_DURATION;

	/** Callback. Open the message log window. */
	void OpenMessageLog() const;
};