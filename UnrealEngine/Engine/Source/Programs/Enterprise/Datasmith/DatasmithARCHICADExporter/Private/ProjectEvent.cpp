// Copyright Epic Games, Inc. All Rights Reserved.

#include "ProjectEvent.h"
#include "Synchronizer.h"
#include "Synchronizer.h"
#include "Palette.h"
#include "Utils/TAssValueName.h"

BEGIN_NAMESPACE_UE_AC

template <>
FAssValueName::SAssValueName TAssEnumName< API_NotifyEventID >::AssEnumName[] = {
	ValueName(APINotify_New),
	ValueName(APINotify_NewAndReset),
	ValueName(APINotify_Open),
	ValueName(APINotify_PreSave),
	ValueName(APINotify_Save),
	ValueName(APINotify_Close),
	ValueName(APINotify_Quit),
	ValueName(APINotify_TempSave),
#if AC_VERSION < 27
	ValueName(APINotify_ConvertUnId),
	ValueName(APINotify_ConvertGuid),
	ValueName(APINotify_ConvertLinkId),
	ValueName(APINotify_ConvertDrwGuid),
#endif
	ValueName(APINotify_SendChanges),
	ValueName(APINotify_ReceiveChanges),

	ValueName(APINotify_ChangeProjectDB),
	ValueName(APINotify_ChangeWindow),
	ValueName(APINotify_ChangeFloor),
	ValueName(APINotify_ChangeLibrary),

	ValueName(APINotify_AllInputFinished),

	ValueName(APINotify_UnitChanged),

	ValueName(APINotify_SideviewCreated),
	ValueName(APINotify_SideviewRebuilt),
#if AC_VERSION < 26
	ValueName(APINotify_PropertyDefinitionChanged),
	ValueName(APINotify_ClassificationItemChanged),
#endif
	ValueName(APINotify_PropertyVisibilityChanged),
	ValueName(APINotify_ClassificationVisibilityChanged),

	EnumEnd(-1)};

static GSErrCode ProjectEventNotificationHandler(API_NotifyEventID NotifID, Int32 Param)
{
	try
	{
		return FProjectEvent::Event(NotifID, Param);
	}
	catch (std::exception& e)
	{
		UE_AC_DebugF("FProjectEvent::Event - Exception \"%s\"\n", e.what());
		return APIERR_NOTSUPPORTED;
	}
	catch (...)
	{
		UE_AC_DebugF("FProjectEvent::Event - Unknown exception\n");
		return APIERR_NOTSUPPORTED;
	}
}

GSErrCode FProjectEvent::Initialize()
{
	GSErrCode GSErr = ACAPI_Notify_CatchProjectEvent(API_AllNotificationMask, ProjectEventNotificationHandler);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FProjectEvent::Initialize - ACAPI_Notify_CatchProjectEvent error=%s\n", GetErrorName(GSErr));
	}
	return GSErr;
}

GSErrCode FProjectEvent::Event(API_NotifyEventID NotifID, Int32 Param)
{
	(void)Param;
	UE_AC_VerboseF("-> FProjectEvent::Event(%s, %d)\n", TAssEnumName< API_NotifyEventID >::GetName(NotifID), Param);
	FSynchronizer* Synchronizer = FSynchronizer::GetCurrent();
	switch (NotifID)
	{
		case APINotify_New:
			if (Synchronizer != nullptr)
			{
				Synchronizer->Reset("APINotify_New");
			}
			break;
		case APINotify_NewAndReset:
			FSynchronizer::Get().ProjectOpen();
			break;
		case APINotify_Open:
			FSynchronizer::Get().ProjectOpen();
			break;
		case APINotify_Close:
			if (Synchronizer != nullptr)
			{
				Synchronizer->Reset("APINotify_Close");
			}
			break;
		case APINotify_Save:
			FSynchronizer::Get().ProjectSave();
			break;
		case APINotify_Quit:
			if (Synchronizer != nullptr)
			{
				Synchronizer->Reset("APINotify_Quit");
			}
			break;
		case APINotify_ChangeWindow:
			FPalette::WindowChanged();
			break;
		default:
			break;
	}

	UE_AC_VerboseF("<- FProjectEvent::Event\n");
	return NoError;
}

END_NAMESPACE_UE_AC
