// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commander.h"

#include "ResourcesIDs.h"
#include "Utils/ShellOpenDocument.h"
#include "Utils/AutoChangeDatabase.h"
#include "Utils/Error.h"
#include "Utils/Element2String.h"
#include "Utils/Pasteboard.h"
#include "Palette.h"
#include "Menus.h"
#include "Synchronizer.h"
#include "Exporter.h"
#include "ReportWindow.h"
#if PLATFORM_MAC
	#include "ConnectionWindow.h"
#endif

#include "DatasmithDirectLink.h"
#include "IDirectLinkUI.h"
#include "IDatasmithExporterUIModule.h"

#include "exp.h"

BEGIN_NAMESPACE_UE_AC

void FCommander::DoSnapshot()
{
	DoSnapshotOrExport(nullptr);
}

static bool bAutoSyncEnabled = false;

void FCommander::ToggleAutoSync()
{
	bAutoSyncEnabled = !bAutoSyncEnabled;
	FMenus::AutoSyncChanged();
	FPalette::AutoSyncChanged();
	if (bAutoSyncEnabled)
	{
		DoSnapshot();
	}
}

bool FCommander::IsAutoSyncEnabled()
{
	return bAutoSyncEnabled;
}

void FCommander::CopySelection2Clipboard()
{
	API_SelectionInfo SelectionInfo;
	Zap(&SelectionInfo);

	GS::Array< API_Neig > SelectionNeigs;
	GSErrCode			  GSErr = ACAPI_Selection_Get(&SelectionInfo, &SelectionNeigs, false);

	BMKillHandle((GSHandle*)&SelectionInfo.marquee.coords);
	if (GSErr == APIERR_NOSEL)
	{
		GSErr = NoError;
	}
	if (GSErr != NoError)
	{
		UE_AC_TraceF("Selection2Clipboard - Error getting selection\n");
		return;
	}

	FAutoChangeDatabase AutoRestoreDB(APIWind_FloorPlanID);
	utf8_string			DumpSelected;

	if (SelectionInfo.typeID != API_SelEmpty)
	{
		// collect indexes of selected dimensions
		USize SelectionsCount = SelectionNeigs.GetSize();
		for (UInt32 Index = 0; Index < SelectionsCount; ++Index)
		{
			const API_Guid& ElemId = SelectionNeigs[Index].guid;
			DumpSelected += FElement2String::GetAllElementAsString(ElemId) + "\n";
		}
	}

	if (DumpSelected.size())
	{
		SetPasteboardWithString(DumpSelected.c_str());
		UE_AC_TraceF("Selection2Clipboard - Selected elements copied to clipboard\n");
	}
	else
	{
		UE_AC_TraceF("Selection2Clipboard - Nothind selected\n");
	}
}

void FCommander::ShowConnectionsDialog()
{
#if PLATFORM_WINDOWS
	IDatasmithExporterUIModule* DsExporterUIModule = IDatasmithExporterUIModule::Get();
	if (DsExporterUIModule != nullptr)
	{
		IDirectLinkUI* DLUI = DsExporterUIModule->GetDirectLinkExporterUI();
		if (DLUI != nullptr)
		{
			DLUI->OpenDirectLinkStreamWindow();
		}
	}
#else
	FConnectionWindow::ShowWindow();
#endif
}

void FCommander::Export3DToFile()
{
	IO::Location DestFile;
	GSErrCode	 GSErr = FExporter::DoChooseDestination(&DestFile);
	if (GSErr == NoError)
	{
		DoSnapshotOrExport(&DestFile);
	}
}

void FCommander::ShowMessagesDialog()
{
	FReportWindow::Create();
}

void FCommander::ShowHidePalette()
{
	FPalette::ShowFromUser();
}

// Dialog About...
enum
{
	kDlgAboutOfButtonOk = 1,
	kDlgAboutOfPictureAbout,
	kDlgAboutOfVersion
};

static short DGCALLBACK SetLicenseInfoTextCB(short InMessage, short InDialID, short InItemID, DGUserData InUserData,
											 DGMessageData /* MsgData */)
{
	const GS::UniString& AddonVersion = *(const GS::UniString*)InUserData;

	switch (InMessage)
	{
		case DG_MSG_INIT:
			short HorizontalSize;
			short VerticalSize;
			DGGetDialogClientSize(ACAPI_GetOwnResModule(), InDialID, DG_ORIGCLIENT, &HorizontalSize, &VerticalSize);
			DGSetDialogClientSize(InDialID, HorizontalSize, VerticalSize, DG_TOPLEFT, false);
			DGSetItemText(InDialID, kDlgAboutOfVersion, AddonVersion);
			break;
		default:
			break;
	}

	return InItemID;
}

void FCommander::ShowAboutOf()
{
	GS::UniString AddonVersion(GetAddonVersionsStr());

	short Result = DGModalDialog(ACAPI_GetOwnResModule(), LocalizeResId(kDlgAboutOf), ACAPI_GetOwnResModule(),
								 SetLicenseInfoTextCB, (DGUserData)&AddonVersion);
	(void)Result;
}

void FCommander::ZapDB()
{
	if (FSynchronizer::GetCurrent())
	{
		FSynchronizer::GetCurrent()->Reset("Zap database");
	}
	FReportWindow::Delete();
	FTraceListener::Get().Clear();
}

void FCommander::DoSnapshotOrExport(const IO::Location* InExportedFile)
{
	FAutoChangeDatabase AutoRestoreDB(APIWind_FloorPlanID);

	void* PreviousSight = nullptr;
	UE_AC_TestGSError(ACAPI_3D_SelectSight(nullptr, &PreviousSight));

	try
	{
		void* CurrentSight = nullptr;
		if (ACAPI_3D_GetCurrentWindowSight(&CurrentSight) != NoError)
		{
			UE_AC_DebugF("FCommander::DoSnapshotOrExport - Error : Current view isn't 3D\n");
			return;
		}
		Modeler::SightPtr		 SightPtr((Modeler::Sight*)CurrentSight);
		Modeler::ConstModel3DPtr Model3D(SightPtr->GetMainModelPtr());

		ModelerAPI::Model Model;
#if AC_VERSION < 26
		AttributeReader	  Reader; // deprecated constructor, temporary!
		UE_AC_TestGSError(EXPGetModel(Model3D, &Model, &Reader));
#else
		GS::Owner<Modeler::IAttributeReader> Reader(ACAPI_Attribute_GetCurrentAttributeSetReader());
		UE_AC_TestGSError(EXPGetModel(Model3D, &Model, Reader.Get()));
#endif
		if (InExportedFile)
		{
			FExporter().DoExport(Model, *InExportedFile);
		}
		else
		{
			FSynchronizer::Get().DoSnapshot(Model);
		}
	}
	catch (...)
	{
		GSErrCode GSErr = ACAPI_3D_SelectSight(PreviousSight, &PreviousSight);
		if (GSErr != NoError)
		{
			UE_AC_DebugF("FCommander::DoSnapshotOrExport - Error %d\n", GSErr);
		}
		throw;
	}

	UE_AC_TestGSError(ACAPI_3D_SelectSight(PreviousSight, &PreviousSight));
}

END_NAMESPACE_UE_AC
