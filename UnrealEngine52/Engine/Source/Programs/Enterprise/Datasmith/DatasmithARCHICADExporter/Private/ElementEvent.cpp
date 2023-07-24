// Copyright Epic Games, Inc. All Rights Reserved.

#include "ElementEvent.h"
#include "Utils/ElementTools.h"
#include "Utils/TAssValueName.h"
#include "Synchronizer.h"
#include "Commander.h"

BEGIN_NAMESPACE_UE_AC

template <>
FAssValueName::SAssValueName TAssEnumName< API_ElementDBEventID >::AssEnumName[] = {
	ValueName(APINotifyElement_BeginEvents),
	ValueName(APINotifyElement_EndEvents),

	ValueName(APINotifyElement_New),
	ValueName(APINotifyElement_Copy),
	ValueName(APINotifyElement_Change),
	ValueName(APINotifyElement_Edit),
	ValueName(APINotifyElement_Delete),

	ValueName(APINotifyElement_Undo_Created),
	ValueName(APINotifyElement_Undo_Modified),
	ValueName(APINotifyElement_Undo_Deleted),
	ValueName(APINotifyElement_Redo_Created),
	ValueName(APINotifyElement_Redo_Modified),
	ValueName(APINotifyElement_Redo_Deleted),
	ValueName(APINotifyElement_PropertyValueChange),

	EnumEnd(-1)};

static GSErrCode ElementEventCB(const API_NotifyElementType* ElemType)
{
	try
	{
		return FElementEvent::Event(*ElemType);
	}
	catch (std::exception& e)
	{
		UE_AC_DebugF("ElementEventCB - Exception \"%s\"\n", e.what());
		return APIERR_NOTSUPPORTED;
	}
	catch (...)
	{
		UE_AC_DebugF("ElementEventCB - Unknown exception\n");
		return APIERR_NOTSUPPORTED;
	}
}

GSErrCode FElementEvent::Initialize()
{
	GSErrCode GSErr = ACAPI_Notify_CatchNewElement(nullptr, ElementEventCB);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FElementEvent::Initialize - ACAPI_Notify_CatchNewElement error=%s\n", GetErrorName(GSErr));
	}

	GSErr = ACAPI_Notify_InstallElementObserver(ElementEventCB);
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FElementEvent::Initialize - ACAPI_Notify_InstallElementObserver error=%s\n", GetErrorName(GSErr));
	}

	return GSErr;
}

GSErrCode FElementEvent::Event(const API_NotifyElementType& ElemType)
{
	UE_AC_TraceF("-> FElementEvent::Event(%s)\n", TAssEnumName< API_ElementDBEventID >::GetName(ElemType.notifID));
	GS::UniString ElemTypeName;
#if AC_VERSION < 26
	if (ACAPI_Goodies(APIAny_GetElemTypeNameID, (void*)(size_t)ElemType.elemHead.typeID, &ElemTypeName) != NoError)
	{
		ElemTypeName = GS::UniString::Printf("ElementType=%d", ElemType.elemHead.typeID);
	}
#else
	const API_ElemTypeID ElemTypeID = ElemType.elemHead.type.typeID;
	if (API_FirstElemType <= ElemTypeID && ElemTypeID <= API_LastElemType)
	{
		ElemTypeName = GS::UniString::Printf("ElementType=%s", FElementTools::TypeName(ElemTypeID));
	}
	else
	{
		ElemTypeName = GS::UniString::Printf("ElementType=%d", ElemTypeID);
	}
#endif

	GS::UniString ElemUUID(APIGuidToString(ElemType.elemHead.guid));
	switch (ElemType.notifID)
	{
		case APINotifyElement_BeginEvents:
			UE_AC_TraceF("Element event: Begin events for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_EndEvents:
			UE_AC_TraceF("Element event: End events for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_New:
			UE_AC_TraceF("Element event: New for %s, %s\n", ElemTypeName.ToCStr().Get(), ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Copy:
			UE_AC_TraceF("Element event: Copy for %s, %s\n", ElemTypeName.ToCStr().Get(), ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Change:
			UE_AC_TraceF("Element event: Change for %s, %s\n", ElemTypeName.ToCStr().Get(), ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Edit:
			UE_AC_TraceF("Element event: Edit for %s, %s\n", ElemTypeName.ToCStr().Get(), ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Delete:
			UE_AC_TraceF("Element event: Delete for %s, %s\n", ElemTypeName.ToCStr().Get(), ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Undo_Created:
			UE_AC_TraceF("Element event: Undo_Created for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Undo_Modified:
			UE_AC_TraceF("Element event: Undo_Modified for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Undo_Deleted:
			UE_AC_TraceF("Element event: Undo_Deleted for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Redo_Created:
			UE_AC_TraceF("Element event: Redo_Created for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Redo_Modified:
			UE_AC_TraceF("Element event: Redo_Modified for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_Redo_Deleted:
			UE_AC_TraceF("Element event: Redo_Deleted for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_PropertyValueChange:
			UE_AC_TraceF("Element event: PropertyValueChange for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
		case APINotifyElement_ClassificationChange:
			UE_AC_TraceF("Element event: ClassificationChange for %s, %s\n", ElemTypeName.ToCStr().Get(),
						 ElemUUID.ToCStr().Get());
			break;
	}
	if (FCommander::IsAutoSyncEnabled())
	{
		FSynchronizer::PostDoSnapshot("Element modified");
	}

	UE_AC_TraceF("<- FElementEvent::Event\n");
	return NoError;
}

END_NAMESPACE_UE_AC
