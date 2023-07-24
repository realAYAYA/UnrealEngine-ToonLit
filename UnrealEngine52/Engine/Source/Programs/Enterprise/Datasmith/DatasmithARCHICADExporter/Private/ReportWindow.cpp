// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReportWindow.h"
#include "ResourcesIDs.h"
#include "Synchronizer.h"
#include "Commander.h"
#include "Menus.h"
#include "Utils/Pasteboard.h"
#include "Utils/Error.h"
#include "Utils/TaskCalledFromEventLoop.h"

BEGIN_NAMESPACE_UE_AC

class FReportDialog : public DG::Palette,
					  public DG::PanelObserver,
					  public DG::ButtonItemObserver,
					  public DG::CompoundItemObserver
{
	enum
	{
		kCloseButtonId = 1,
		kClearButtonId,
		kMessagesTextEditId,
		kCopyAllButtonId,
		kCopySelectionButtonId
	};

	DG::Button		  CloseButton;
	DG::Button		  ClearButton;
	DG::MultiLineEdit MessagesTextEdit;
	DG::Button		  CopyAllButton;
	DG::Button		  CopySelectionButton;
	size_t			  LastSize = 0;

	static GS::Guid PaletteGuid;

  public:
	FReportDialog();
	~FReportDialog();

	virtual void PanelClosed(const DG::PanelCloseEvent& /* ev */) override {}

	virtual void PanelIdle(const DG::PanelIdleEvent& /* ev */) override { Update(); }

	virtual void PanelCloseRequested(const DG::PanelCloseRequestEvent& /* ev */, bool* /* accepted */) override
	{
		Hide();
	}

	virtual void PanelResized(const DG::PanelResizeEvent& ev) override
	{
		if (ev.GetSource() == this)
		{
			DG::Point Position = CloseButton.GetPosition();
			Position.Set(Position.GetX() + ev.GetHorizontalChange(), Position.GetY() + ev.GetVerticalChange());
			CloseButton.SetPosition(Position);

			Position = ClearButton.GetPosition();
			Position.SetY(Position.GetY() + ev.GetVerticalChange());
			ClearButton.SetPosition(Position);

			Position = CopyAllButton.GetPosition();
			Position.SetY(Position.GetY() + ev.GetVerticalChange());
			CopyAllButton.SetPosition(Position);

			Position = CopySelectionButton.GetPosition();
			Position.SetY(Position.GetY() + ev.GetVerticalChange());
			CopySelectionButton.SetPosition(Position);

			MessagesTextEdit.SetSize(MessagesTextEdit.GetWidth() + ev.GetHorizontalChange(),
									 MessagesTextEdit.GetHeight() + ev.GetVerticalChange());
		}
	}

	virtual void ButtonClicked(const DG::ButtonClickEvent& ev) override
	{
		if (ev.GetSource() == &CloseButton)
		{
			SendCloseRequest();
		}
		else if (ev.GetSource() == &ClearButton)
		{
            FTraceListener::Get().Clear();
			MessagesTextEdit.SetText(GS::UniString(""));
		}
		else if (ev.GetSource() == &CopyAllButton)
		{
			SetPasteboardWithString(MessagesTextEdit.GetText().ToUtf8());
		}
		else if (ev.GetSource() == &CopySelectionButton)
		{
			DG::CharRange Selection(MessagesTextEdit.GetSelection());
			GS::UniString SelectedText(
				MessagesTextEdit.GetText().GetSubstring(Selection.GetMin(), Selection.GetLength()));
			SetPasteboardWithString(SelectedText.ToUtf8());
		}
	}

	// Update the text content with the collected traces
	void Update()
	{
        if (FTraceListener::Get().HasUpdate())
        {
            GS::UniString Traces = FTraceListener::Get().GetTraces();
            DG::CharRange Selection(MessagesTextEdit.GetSelection());
            
            MessagesTextEdit.SetText(Traces);
            
            // On empty selection, we set selection to the end, otherwise we restore previous one
            if (Selection.GetLength() == 0)
            {
                Selection.SetWithLength(Traces.GetLength(), 0);
            }
            MessagesTextEdit.SetSelection(Selection);
        }
	}

#if PLATFORM_MAC & AC_VERSION > 25
	virtual void ItemMouseExited(const DG::ItemMouseMoveEvent& /*ev*/) override {}
	virtual void ItemMouseEntered(const DG::ItemMouseMoveEvent& /*ev*/) override {}
	virtual short SpecMouseExited(const DG::ItemMouseMoveEvent& /*ev*/) override { return 0; }
	virtual short SpecMouseEntered(const DG::ItemMouseMoveEvent& /*ev*/) override { return 0; }
#endif
					  };

FReportDialog::FReportDialog()
	: DG::Palette(ACAPI_GetOwnResModule(), LocalizeResId(kDlgReport), ACAPI_GetOwnResModule(), PaletteGuid)
	, CloseButton(GetReference(), kCloseButtonId)
	, ClearButton(GetReference(), kClearButtonId)
	, MessagesTextEdit(GetReference(), kMessagesTextEditId)
	, CopyAllButton(GetReference(), kCopyAllButtonId)
	, CopySelectionButton(GetReference(), kCopySelectionButtonId)
{
	Attach(*this);
	AttachToAllItems(*this);
	bool SendForInactiveApp = false;
	EnableIdleEvent(SendForInactiveApp);
    BeginEventProcessing();
    Show();
}

FReportDialog::~FReportDialog()
{
	DetachFromAllItems(*this);
	Detach(*this);
}

GS::Guid FReportDialog::PaletteGuid("CA0A0905-1FDA-401B-97F7-B00EEB3254C6");

static FReportDialog* ReportDialog = nullptr;

void FReportWindow::Create()
{
	if (ReportDialog == nullptr)
	{
        ReportDialog = new FReportDialog();
	}
    ReportDialog->Show();
    ReportDialog->BringToFront();
}

void FReportWindow::Delete()
{
	if (ReportDialog != nullptr)
	{
        delete ReportDialog;
        ReportDialog = nullptr;
	}
}

static FTraceListener* TraceListener;

FTraceListener& FTraceListener::Get()
{
	if (TraceListener == nullptr)
	{
		TraceListener = new FTraceListener();
	}
	return *TraceListener;
}

void FTraceListener::Delete()
{
    delete TraceListener;
    TraceListener = nullptr;
}

GS::UniString FTraceListener::GetTraces()
{
    GS::Guard< GS::Lock > lck(FTraceListener::Get().AccessControl);
    bHasUpdate = false;
    return GS::UniString(Traces.c_str(), CC_UTF8);
}

void FTraceListener::Clear()
{
    GS::Guard< GS::Lock > lck(FTraceListener::Get().AccessControl);
    Traces.clear();
    bHasUpdate = false;
}

FTraceListener::FTraceListener()
	: CV(AccessControl)
{
	Traces.reserve(100 * 1024);
	AddTraceListener(this);
}

FTraceListener::~FTraceListener()
{
    RemoveTraceListener(this);
}

void FTraceListener::NewTrace(EP2DB InTraceLevel, const utf8_string& InMsg)
{
#ifdef DEBUG
	const EP2DB MessageLevel = kP2DB_Trace;
#else
	const EP2DB MessageLevel = kP2DB_Debug; // Put kP2DB_Report for final release
#endif

	if (InTraceLevel <= MessageLevel)
    {
        GS::Guard< GS::Lock > lck(AccessControl);
        
        if (InTraceLevel != kP2DB_Report)
        {
            Traces.append("* ");
        }
        Traces.append(InMsg);
        bHasUpdate = true;
    }
}

END_NAMESPACE_UE_AC
