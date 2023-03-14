// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewEvent.h"
#include "TAssValueName.h"

BEGIN_NAMESPACE_UE_AC

// clang-format off
template <>
FAssValueName::SAssValueName TAssEnumName< API_NotifyViewEventID >::AssEnumName[] =
{
	ValueName(APINotifyView_Inserted),
	ValueName(APINotifyView_Modified),
	ValueName(APINotifyView_Deleted),
	ValueName(APINotifyView_Opened),
	ValueName(APINotifyView_Begin),
	ValueName(APINotifyView_End),

	EnumEnd(-1)
};

template <>
FAssValueName::SAssValueName TAssEnumName< API_NavigatorMapID >::AssEnumName[] =
{
	ValueName(API_UndefinedMap),
	ValueName(API_ProjectMap),
	ValueName(API_PublicViewMap),
	ValueName(API_MyViewMap),
	ValueName(API_LayoutMap),
	ValueName(API_PublisherSets),

	EnumEnd(-1)
};
// clang-format on

static GSErrCode ViewEventNotificationHandler(const API_NotifyViewEventType* ViewEvent)
{
	try
	{
		return FViewEvent::Event(*ViewEvent);
	}
	catch (std::exception& e)
	{
		UE_AC_DebugF("FViewEvent::Event - Exception \"%s\"\n", e.what());
		return APIERR_NOTSUPPORTED;
	}
	catch (...)
	{
		UE_AC_DebugF("FViewEvent::Event - Unknown exception\n");
		return APIERR_NOTSUPPORTED;
	}
}

GSErrCode FViewEvent::Initialize()
{
	GSErrCode GSErr =
		ACAPI_Notify_CatchViewEvent(API_AllViewNotificationMask, API_UndefinedMap, ViewEventNotificationHandler);
	if (GSErr == NoError)
	{
		GSErr = ACAPI_Notify_CatchViewEvent(API_AllViewNotificationMask, API_ProjectMap, ViewEventNotificationHandler);
	}
	if (GSErr == NoError)
	{
		GSErr =
			ACAPI_Notify_CatchViewEvent(API_AllViewNotificationMask, API_PublicViewMap, ViewEventNotificationHandler);
	}
	if (GSErr == NoError)
	{
		GSErr = ACAPI_Notify_CatchViewEvent(API_AllViewNotificationMask, API_MyViewMap, ViewEventNotificationHandler);
	}
	if (GSErr == NoError)
	{
		GSErr = ACAPI_Notify_CatchViewEvent(API_AllViewNotificationMask, API_LayoutMap, ViewEventNotificationHandler);
	}
	if (GSErr == NoError)
	{
		GSErr =
			ACAPI_Notify_CatchViewEvent(API_AllViewNotificationMask, API_PublisherSets, ViewEventNotificationHandler);
	}
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FViewEvent::Initialize - ACAPI_Notify_CatchViewEvent error=%s\n", GetErrorName(GSErr));
	}
	return GSErr;
}

GSErrCode FViewEvent::Event(const API_NotifyViewEventType& ViewEvent)
{
	UE_AC_VerboseF("--- FViewEvent::Event(%s), MapId=%s, ItemGuid={%s}\n",
				   TAssEnumName< API_NotifyViewEventID >::GetName(ViewEvent.notifID),
				   TAssEnumName< API_NavigatorMapID >::GetName(ViewEvent.mapId),
				   APIGuidToString(ViewEvent.itemGuid).ToUtf8());

	switch (ViewEvent.notifID)
	{
		case APINotifyView_Inserted:
		case APINotifyView_Modified:
		case APINotifyView_Deleted:
		case APINotifyView_Opened:
		case APINotifyView_Begin:
		case APINotifyView_End:
		default:
			break;
	}

	return NoError;
}

END_NAMESPACE_UE_AC
