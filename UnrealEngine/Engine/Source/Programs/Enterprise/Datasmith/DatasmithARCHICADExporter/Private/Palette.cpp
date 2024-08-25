// Copyright Epic Games, Inc. All Rights Reserved.

#include "Palette.h"
#include "Synchronizer.h"
#include "Utils/APIEnvir.h"
#include "Utils/TAssValueName.h"
#include "ResourcesIDs.h"
#include "Utils/Error.h"
//#include "CSynchronizer.hpp"
#include "Commander.h"
#include "Menus.h"

#define PALETTE_4_ALL_VIEW 1
#define TRACE_PALETTE 0

BEGIN_NAMESPACE_UE_AC

enum
{
	kDial_Snapshot = 1,
#if AUTO_SYNC
	kDial_AutoSync,
#endif
	kDial_Connections,
	kDial_Export3D,
	kDial_Messages,

	kDial_Information,
	kDial_ZapModelDB
};

static bool bPaletteRegistered = false;

class FPaletteWindow : public DG::Palette,
					   public DG::PanelObserver,
					   public DG::ButtonItemObserver,
					   public DG::CheckItemObserver,
					   public DG::CompoundItemObserver
{
  public:
	DG::IconButton IconSnapshot;
#if AUTO_SYNC
	DG::IconPushCheck IconAutoSync;
#endif
	DG::IconButton IconConnections;
	DG::IconButton IconExport3D;
	DG::IconButton IconMessages;
	DG::IconButton IconInfo2Clipboard;
	DG::IconButton IconZapDB;

	FPaletteWindow()
		: DG::Palette(ACAPI_GetOwnResModule(), LocalizeResId(kDlgPalette), ACAPI_GetOwnResModule(),
					  FPalette::PaletteGuid)
		, IconSnapshot(GetReference(), kDial_Snapshot)
#if AUTO_SYNC
		, IconAutoSync(GetReference(), kDial_AutoSync)
#endif
		, IconConnections(GetReference(), kDial_Connections)
		, IconExport3D(GetReference(), kDial_Export3D)
		, IconMessages(GetReference(), kDial_Messages)
		, IconInfo2Clipboard(GetReference(), kDial_Information)
		, IconZapDB(GetReference(), kDial_ZapModelDB)
	{
		this->Attach(*this);
		AttachToAllItems(*this);
		this->BeginEventProcessing();

		bool SendForInactiveApp = true;
		EnableIdleEvent(SendForInactiveApp);

		Show();
	}

	~FPaletteWindow()
	{
		this->EndEventProcessing();
		this->Detach(*this);
		DetachFromAllItems(*this);
	}

#if PLATFORM_MAC & AC_VERSION > 25
	virtual void ItemMouseExited(const DG::ItemMouseMoveEvent& /*ev*/) override {}
	virtual void ItemMouseEntered(const DG::ItemMouseMoveEvent& /*ev*/) override {}
	virtual short SpecMouseExited(const DG::ItemMouseMoveEvent& /*ev*/) override { return 0; }
	virtual short SpecMouseEntered(const DG::ItemMouseMoveEvent& /*ev*/) override { return 0; }
#endif

  private:
	virtual void PanelOpened(const DG::PanelOpenEvent& /*ev*/) override
	{
		SetClientSize(GetOriginalClientWidth(), GetOriginalClientHeight());
	}

	virtual void PanelCloseRequested(const DG::PanelCloseRequestEvent& /* ev */, bool* /* accepted */) override
	{
		Hide();
	}

	virtual void PanelIdle(const DG::PanelIdleEvent& /* ev */) override
	{
		const int  Delay = 10;
		static int Count = Delay;
		if (Is3DCurrenWindow())
		{
			static DG::NativeUnit NativeXPos;
			static DG::NativeUnit NativeYPos;
			DGMousePosData		  PositionData;
			short				  Result = DGGetMousePosition(0, &PositionData);
			(void)Result;
			if (PositionData.nativeXPos != NativeXPos || PositionData.nativeYPos != NativeYPos)
			{
				NativeXPos = PositionData.nativeXPos;
				NativeYPos = PositionData.nativeYPos;
				Count = Delay;
			}
			if (--Count == 0)
			{
				Count = Delay;
				TryFunctionCatchAndLog("AutoSync - Check View State", []() -> GSErrCode {
					FSynchronizer* Synchronizer = FSynchronizer::GetCurrent();
					if (Synchronizer != nullptr)
					{
						Synchronizer->DoIdle(&Count);
					}
					return NoError;
				});
			}
		}
		else
		{
			Count = Delay;
		}
	}

	virtual void ButtonClicked(const DG::ButtonClickEvent& ev) override
	{
		TryFunctionCatchAndAlert("FPaletteWindow - ButtonClicked", [this, &ev]() -> GSErrCode {
			if (ev.GetSource() == &IconSnapshot)
			{
				FCommander::DoSnapshot();
			}
			else if (ev.GetSource() == &IconConnections)
			{
				FCommander::ShowConnectionsDialog();
			}
			else if (ev.GetSource() == &IconExport3D)
			{
				FCommander::Export3DToFile();
			}
			else if (ev.GetSource() == &IconMessages)
			{
				FCommander::ShowMessagesDialog();
			}
			else if (ev.GetSource() == &IconInfo2Clipboard)
			{
				FCommander::CopySelection2Clipboard();
			}
			else if (ev.GetSource() == &IconZapDB)
			{
				FCommander::ZapDB();
			}
			else
			{
				UE_AC_DebugF("FPaletteWindow::ButtonClicked - Unknown event source\n");
			}
			return NoError;
		});
	}

	virtual void CheckItemChanged(const DG::CheckItemChangeEvent& ev) override
	{
#if AUTO_SYNC
		if (ev.GetSource() == &IconAutoSync)
		{
			FCommander::ToggleAutoSync();
		}
		else
#else
		(void)ev; // Remove warning
#endif
		{
			UE_AC_DebugF("FPaletteWindow::CheckItemChanged - Unknown event source\n");
		}
	}
};

void FPalette::Register()
{
	if (bPaletteRegistered)
	{
		return;
	}
#if PALETTE_4_ALL_VIEW
	GS::GSFlags Flags = API_PalEnabled_FloorPlan | API_PalEnabled_Section | API_PalEnabled_3D | API_PalEnabled_Detail |
						API_PalEnabled_Layout | API_PalEnabled_Worksheet | API_PalEnabled_Elevation |
						API_PalEnabled_InteriorElevation | API_PalEnabled_DocumentFrom3D;
#else
	GS::GSFlags Flags = API_PalEnabled_3D;
#endif
	GSErrCode GSErr = ACAPI_RegisterModelessWindow(FPalette::PaletteRefId(), APIPaletteControlCallBack, Flags,
												   GSGuid2APIGuid(PaletteGuid));
	if (GSErr != NoError)
	{
		UE_AC_DebugF("FPalette::Register - ACAPI_RegisterModelessWindow failed err(%d)\n", GSErr);
	}
	else
	{
		bPaletteRegistered = true;
	}
}

void FPalette::Unregister()
{
	if (bPaletteRegistered)
	{
		GSErrCode GSErr = ACAPI_UnregisterModelessWindow(FPalette::PaletteRefId());
		if (GSErr != NoError)
		{
			UE_AC_DebugF("FPalette::Unregister - ACAPI_UnregisterModelessWindow failed err(%d)\n", GSErr);
		}
		bPaletteRegistered = false;
	}
}

// Constructor
FPalette::FPalette()
{
	CurrentPalette = this;
	Window = new FPaletteWindow();
}

// Destructor
FPalette::~FPalette()
{
	delete Window;
	Window = nullptr;
	CurrentPalette = nullptr;
}

// Toggle visibility of palette
void FPalette::ShowFromUser()
{
	// No palette, we create one
	if (CurrentPalette == nullptr)
	{
		new FPalette();
		WindowChanged();
	}
	else
	{
		if (CurrentPalette->Window->IsVisible())
		{
			CurrentPalette->Window->Hide();
		}
		else
		{
			CurrentPalette->Window->Show();
		}
	}
}

// Switch to another window
void FPalette::WindowChanged()
{
	bool bIs3DView = Is3DCurrenWindow();

#if PALETTE_4_ALL_VIEW
	if (CurrentPalette != nullptr)
	{
		CurrentPalette->Window->IconSnapshot.SetStatus(bIs3DView);
		CurrentPalette->Window->IconAutoSync.SetStatus(bIs3DView);
	}
#endif
}

// AutoSync status changed
void FPalette::AutoSyncChanged()
{
	if (CurrentPalette != nullptr)
	{
		bool bAutoSyncEnabled = FCommander::IsAutoSyncEnabled();
#if AUTO_SYNC
		CurrentPalette->Window->IconAutoSync.SetState(bAutoSyncEnabled);
#else
		(void)bAutoSyncEnabled; // Remove warning
#endif
	}
}

// Delete palette
void FPalette::Delete()
{
	if (CurrentPalette)
	{
		delete CurrentPalette;
	}
}

// Save palette state to preferences
void FPalette::Save2Pref()
{
	/*	State.bIsDocked = Window->IsDocked();
		FPreferences* pref = FPreferences::Get();
		pref->Prefs.Palette = State;
		pref->Write();*/
}

// clang-format off
FAssValueName::SAssValueName Dg_Msg_Name[] = {
	ValueName(DG_MSG_NULL),
	ValueName(DG_MSG_INIT),
	ValueName(DG_MSG_CLOSEREQUEST),
	ValueName(DG_MSG_CLOSE),
	ValueName(DG_MSG_CLICK),
	ValueName(DG_MSG_DOUBLECLICK),
	ValueName(DG_MSG_CHANGE),
	ValueName(DG_MSG_TRACK),
	ValueName(DG_MSG_MOUSEMOVE),
	ValueName(DG_MSG_FOCUS),
	ValueName(DG_MSG_FILTERCHAR),
	ValueName(DG_MSG_HOTKEY),
	ValueName(DG_MSG_GROW),
	ValueName(DG_MSG_RESIZE),
	ValueName(DG_MSG_ACTIVATE),
	ValueName(DG_MSG_TOPSTATUSCHANGE),
	ValueName(DG_MSG_UPDATE),
	ValueName(DG_MSG_DRAGDROP),
	ValueName(DG_MSG_CONTEXTMENU),
	ValueName(DG_MSG_WHEELCLICK),
	ValueName(DG_MSG_WHEELTRACK),
	ValueName(DG_MSG_ITEMHELP),
	ValueName(DG_MSG_BACKGROUNDPAINT),
	ValueName(DG_MSG_LISTHEADERCLICK),
	ValueName(DG_MSG_LISTHEADERDRAG),
	ValueName(DG_MSG_LISTHEADERRESIZE),
	ValueName(DG_MSG_LISTHEADERBUTTONCLICK),
	ValueName(DG_MSG_SPLITTERDRAG),
	ValueName(DG_MSG_RESOLUTIONCHANGE),
	ValueName(DG_MSG_MOUSEDOWN),
	ValueName(DG_MSG_TREEITEMCLICK),
	ValueName(DG_MSG_TABBARITEMDRAG),
	ValueName(DG_MSG_SWITCHWND_BEGIN),
	ValueName(DG_MSG_SWITCHWND_NEXT),
	ValueName(DG_MSG_SWITCHWND_PREV),
	ValueName(DG_MSG_SWITCHWND_END),

	ValueName(DG_MSG_HOVER),
	ValueName(DG_MSG_PRESSED),
	ValueName(DG_MSG_UPDATEOVERLAY),
	ValueName(DG_MSG_CHANGEREQUEST),

	ValueName(DG_OF_MSG_FOLDERCHANGE),
	ValueName(DG_OF_MSG_SELCHANGE),
	ValueName(DG_OF_MSG_TYPECHANGE),

	EnumEnd(-1)};
// clang-format on

void FPalette::SetPaletteMenuTexts(bool PaletteIsOn, bool PaletteIsVisible)
{
	GS::UniString ItemStr(GetGSName(PaletteIsOn ? kName_HidePalette : kName_ShowPalette));
	FMenus::SetMenuItemText(kStrListMenuItemPalette, 1, ItemStr);

	FMenus::SetMenuItemStatus(kStrListMenuItemPalette, 1, !PaletteIsVisible, API_MenuItemDisabled);
}

GSErrCode FPalette::PaletteControlCallBack(Int32 ReferenceID, API_PaletteMessageID MessageID, GS::IntPtr Param)
{
	if (ReferenceID == PaletteRefId())
	{
		switch (MessageID)
		{
			case APIPalMsg_ClosePalette: // Called when quitting ArchiCAD
				Window->SendCloseRequest();
				break;

			case APIPalMsg_HidePalette_Begin:
				Window->Hide();
				// SetPaletteMenuTexts();
				break;

			case APIPalMsg_HidePalette_End:
				Window->Show();
				// SetPaletteMenuTexts();
				break;

			case APIPalMsg_DisableItems_Begin:
				Window->DisableItems();
				break;

			case APIPalMsg_DisableItems_End:
				Window->EnableItems();
				break;

			case APIPalMsg_IsPaletteVisible:
				(*reinterpret_cast< bool* >(Param)) = Window->IsVisible();
				break;
			default:
				break;
		}
	}
	return NoError;
}

// clang-format off
template <>
FAssValueName::SAssValueName TAssEnumName< API_PaletteMessageID >::AssEnumName[] =
{
	ValueName(APIPalMsg_ClosePalette),
	ValueName(APIPalMsg_HidePalette_Begin),
	ValueName(APIPalMsg_HidePalette_End),
	ValueName(APIPalMsg_DisableItems_Begin),
	ValueName(APIPalMsg_DisableItems_End),
	ValueName(APIPalMsg_OpenPalette),
	ValueName(APIPalMsg_IsPaletteVisible),
	EnumEnd(-1)
};
// clang-format on

GSErrCode __ACENV_CALL FPalette::APIPaletteControlCallBack(Int32 ReferenceID, API_PaletteMessageID MessageID,
														   GS::IntPtr Param)
{
	GSErrCode GSErr = APIERR_GENERAL;
	try
	{
#if TRACE_PALETTE
		UE_AC_TraceF("FPalette::APIPaletteControlCallBack - Ref=%d, Msg=%s, param=%llu\n", ReferenceID,
					 TAssEnumName< API_PaletteMessageID >::GetName(MessageID), Param);
#endif
		if (ReferenceID == FPalette::PaletteRefId())
		{
			if (CurrentPalette != nullptr)
			{
				return CurrentPalette->PaletteControlCallBack(ReferenceID, MessageID, Param);
			}
			else
			{
				if (MessageID == APIPalMsg_OpenPalette)
				{
					new FPalette();
				}
				else if (MessageID == APIPalMsg_IsPaletteVisible)
				{
					(*reinterpret_cast< bool* >(Param)) = false;
				}
				else
				{
				}
			}
		}
		GSErr = NoError;
	}
	catch (std::exception& e)
	{
		UE_AC_DebugF("FPalette::APIPaletteControlCallBack Ref(%d) Msg(%d) - Caught exception \"%s\"\n", ReferenceID,
					 MessageID, e.what());
	}
	catch (GS::GSException& gs)
	{
		UE_AC_DebugF("FPalette::APIPaletteControlCallBack Ref(%d) Msg(%d) - Caught exception \"%s\"\n", ReferenceID,
					 MessageID, gs.GetMessage().ToUtf8());
	}
	catch (...)
	{
		UE_AC_DebugF("FPalette::APIPaletteControlCallBack Ref(%d) Msg(%d) - Caught unknown exception\n", ReferenceID,
					 MessageID);
		ShowAlert("Unknown", "FPalette::APIPaletteControlCallBack");
	}

	return GSErr;
}

Int32 FPalette::PaletteRefId()
{
#if AC_VERSION < 24
	static Int32 RefId = GS::GenerateHashValue(PaletteGuid);
#else
	static Int32 RefId = PaletteGuid.GenerateHashValue();
#endif
	return RefId;
}

FPalette* FPalette::CurrentPalette = nullptr;
GS::Guid  FPalette::PaletteGuid("245C6E1B-6BBA-4908-9890-3879C1E0CD5A");

END_NAMESPACE_UE_AC
