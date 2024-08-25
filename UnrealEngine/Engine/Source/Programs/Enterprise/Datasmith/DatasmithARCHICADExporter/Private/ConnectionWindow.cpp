// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConnectionWindow.h"
#include "ResourcesIDs.h"
#include "Synchronizer.h"
#include "Commander.h"
#include "Menus.h"
#include "Utils/ShellOpenDocument.h"
#include "Utils/TaskCalledFromEventLoop.h"

#undef TicksPerSecond

#include "DirectLinkEndpoint.h"
#include "DatasmithDirectLink.h"

BEGIN_NAMESPACE_UE_AC

static class FConnectionDialog* ConnectionDialog = nullptr;

template < class Key, class Element >
bool Equals(const TMap< Key, Element >& InMap1, const TMap< Key, Element >& InMap2);

template < class Element > bool Equals(const TArray< Element >& InArray1, const TArray< Element >& InArray2);

inline bool operator==(const DirectLink::FRawInfo::FDataPointId& InDataPointId1,
					   const DirectLink::FRawInfo::FDataPointId& InDataPointId2)
{
	return InDataPointId1.Name == InDataPointId2.Name && InDataPointId1.Id == InDataPointId2.Id &&
		   InDataPointId1.bIsPublic == InDataPointId2.bIsPublic;
}

inline bool operator==(const DirectLink::FRawInfo::FEndpointInfo& InEndpointInfo1,
					   const DirectLink::FRawInfo::FEndpointInfo& InEndpointInfo2)
{
	return InEndpointInfo1.Name == InEndpointInfo2.Name &&
		   InEndpointInfo1.Version.ExactMatch(InEndpointInfo2.Version) &&
		   Equals(InEndpointInfo1.Destinations, InEndpointInfo2.Destinations) &&
		   Equals(InEndpointInfo1.Sources, InEndpointInfo2.Sources) &&
		   InEndpointInfo1.UserName == InEndpointInfo2.UserName &&
		   InEndpointInfo1.ExecutableName == InEndpointInfo2.ExecutableName &&
		   InEndpointInfo1.ComputerName == InEndpointInfo2.ComputerName &&
		   InEndpointInfo1.bIsLocal == InEndpointInfo2.bIsLocal &&
		   InEndpointInfo1.ProcessId == InEndpointInfo2.ProcessId;
}

inline bool operator==(const DirectLink::FRawInfo::FDataPointInfo& InDataPointInfo1,
					   const DirectLink::FRawInfo::FDataPointInfo& InDataPointInfo2)
{
	return InDataPointInfo1.EndpointAddress == InDataPointInfo2.EndpointAddress &&
		   InDataPointInfo1.Name == InDataPointInfo2.Name && InDataPointInfo1.bIsSource == InDataPointInfo2.bIsSource &&
		   InDataPointInfo1.bIsPublic == InDataPointInfo2.bIsPublic;
}

inline bool operator==(const DirectLink::FCommunicationStatus& InCommunicationStatus1,
					   const DirectLink::FCommunicationStatus& InCommunicationStatus2)
{
	return InCommunicationStatus1.bIsSending == InCommunicationStatus2.bIsSending &&
		   InCommunicationStatus1.bIsReceiving == InCommunicationStatus2.bIsReceiving &&
		   InCommunicationStatus1.TaskTotal == InCommunicationStatus2.TaskTotal &&
		   InCommunicationStatus1.TaskCompleted == InCommunicationStatus2.TaskCompleted;
}

inline bool operator==(const DirectLink::FRawInfo::FStreamInfo& InStreamInfo1,
					   const DirectLink::FRawInfo::FStreamInfo& InStreamInfo2)
{
	return InStreamInfo1.StreamId == InStreamInfo2.StreamId && InStreamInfo1.Source == InStreamInfo2.Source &&
		   InStreamInfo1.Destination == InStreamInfo2.Destination &&
		   InStreamInfo1.ConnectionState == InStreamInfo2.ConnectionState &&
		   InStreamInfo1.CommunicationStatus == InStreamInfo2.CommunicationStatus;
}

inline bool operator==(const DirectLink::FRawInfo& InState1, const DirectLink::FRawInfo& InState2)
{
	return InState1.ThisEndpointAddress == InState2.ThisEndpointAddress &&
		   Equals(InState1.EndpointsInfo, InState2.EndpointsInfo) &&
		   Equals(InState1.DataPointsInfo, InState2.DataPointsInfo) &&
		   Equals(InState1.StreamsInfo, InState2.StreamsInfo);
}

template < class Key, class Element >
bool Equals(const TMap< Key, Element >& InMap1, const TMap< Key, Element >& InMap2)
{
	int32 NbElements = InMap1.Num();
	if (NbElements != InMap2.Num())
	{
		return false;
	}
	for (const auto& Pair : InMap1)
	{
		const Element* Value = InMap2.Find(Pair.Key);
		if (Value == nullptr || !(*Value == Pair.Value))
		{
			return false;
		}
	}
	return true;
}

template < class Element > bool Equals(const TArray< Element >& InArray1, const TArray< Element >& InArray2)
{
	int32 NbElements = InArray1.Num();
	if (NbElements != InArray2.Num())
	{
		return false;
	}
	for (int32 Index = 0; Index < NbElements; ++Index)
	{
		if (!(InArray1[Index] == InArray2[Index]))
		{
			return false;
		}
	}
	return true;
}

// Class that observe endpoint state change
class FEndpointObserver : public DirectLink::IEndpointObserver
{
  public:
	FEndpointObserver()
		: CV(AccessControl)
		, Endpoint(FDatasmithDirectLink::GetEnpoint())
	{
		Endpoint->AddEndpointObserver(this);
	}

	~FEndpointObserver()
	{
		Endpoint->RemoveEndpointObserver(this);
		bChanged = false;
	}

	bool GetStatus(DirectLink::FRawInfo* OutStatus)
	{
		GS::Guard< GS::Lock > lck(AccessControl);
		if (!bChanged)
		{
			return false;
		}
		*OutStatus = LastRawInfo;
		bChanged = false;
		return true;
	}

  private:
	virtual void OnStateChanged(const DirectLink::FRawInfo& InRawInfo) override
	{
		GS::Guard< GS::Lock > lck(AccessControl);
		if (!(InRawInfo == LastRawInfo))
		{
			LastRawInfo = InRawInfo;
			bChanged = true;
		}
	}

	TSharedRef< DirectLink::FEndpoint, ESPMode::ThreadSafe > Endpoint;

	DirectLink::FRawInfo LastRawInfo;
	bool				 bChanged = false;

	// Control access on this object (for queue operations)
	GS::Lock AccessControl;

	// Condition variable
	GS::Condition CV;
};

class FToolTipText : public GS::Object
{
public:
    FToolTipText(const GS::UniString& InString)
    : ToolTipText(InString)
    {}

    GS::UniString   ToolTipText;
};

class FConnectionDialog : public DG::Palette,
						  public DG::PanelObserver,
						  public DG::ListBoxObserver,
						  public DG::ButtonItemObserver,
						  public DG::CompoundItemObserver
{
	enum
	{
		kConnectionsSingleSelListId = 1,
		kNoConnectionTextId,
		kCacheFolderTextId,
		kGotoCacheFolderButtonId,
		kChooseCacheFolderButtonId
	};
	enum
	{
		kSourceColumn = 1,
		kDestinationColumn,
		kNbColumns = kDestinationColumn
	};

	DG::SingleSelListBox ConnectionsListBox;
	DG::LeftText		 NoConnectionText;
	DG::LeftText		 CacheFolderText;
	DG::IconButton		 GotoCacheFolderButton;
	DG::IconButton		 ChooseCacheFolderButton;
	FEndpointObserver	 EndpointObserver;
	DirectLink::FRawInfo CurrentStatus;

	static GS::Guid PaletteGuid;

  public:
	FConnectionDialog();
	~FConnectionDialog();

	virtual void PanelOpened(const DG::PanelOpenEvent& /*ev*/) override
	{
		// SetClientSize(GetOriginalClientWidth(), GetOriginalClientHeight());
	}

	virtual void PanelClosed(const DG::PanelCloseEvent& /* ev */) override {}

	virtual void PanelCloseRequested(const DG::PanelCloseRequestEvent& /* ev */, bool* bAccepted) override
	{
		*bAccepted = true;
		Hide();
		FTaskCalledFromEventLoop::CallFunctorFromEventLoop([]() { FConnectionWindow::DeleteWindow(); });
	}

	virtual void PanelIdle(const DG::PanelIdleEvent& /* ev */) override
	{
		if (EndpointObserver.GetStatus(&CurrentStatus))
		{
			short ItemsCount = ConnectionsListBox.GetItemCount();
			while (ItemsCount > 0)
			{
				ConnectionsListBox.DeleteItem(ItemsCount--);
			}

			// Grab the sources
			TMap< FGuid, const DirectLink::FRawInfo::FDataPointId* > SourcesMap;
			const DirectLink::FRawInfo::FEndpointInfo*				 SourceEndpointInfo =
				CurrentStatus.EndpointsInfo.Find(CurrentStatus.ThisEndpointAddress);
			if (SourceEndpointInfo != nullptr)
			{
				// Grab all the sources
				SourcesMap.Reserve(SourceEndpointInfo->Sources.Num());
				for (const DirectLink::FRawInfo::FDataPointId& NamedId : SourceEndpointInfo->Sources)
				{
					SourcesMap.Add(NamedId.Id, &NamedId);
				}
			}

			// Grab all the potential destinations
			class FDestination
			{
			  public:
				const DirectLink::FRawInfo::FEndpointInfo* EndpointInfo;
				const DirectLink::FRawInfo::FDataPointId*  DataPointId;
			};
			TMap< FGuid, FDestination > DestinationsMap;
			for (const TPair< FMessageAddress, DirectLink::FRawInfo::FEndpointInfo >& EnpointsInfoPair :
				 CurrentStatus.EndpointsInfo)
			{
				const DirectLink::FRawInfo::FEndpointInfo& EndpointInfo = EnpointsInfoPair.Value;
				DestinationsMap.Reserve(EndpointInfo.Destinations.Num());
				for (const DirectLink::FRawInfo::FDataPointId& NamedId : EndpointInfo.Destinations)
				{
					DestinationsMap.Add(NamedId.Id, {&EndpointInfo, &NamedId});
				}
			}

			// Collect all active connections between our EndPoint and destinations
			FString ThisText;
			for (const DirectLink::FRawInfo::FStreamInfo& StreamInfo : CurrentStatus.StreamsInfo)
			{
				if (StreamInfo.ConnectionState == DirectLink::EStreamConnectionState::Active)
				{
					const DirectLink::FRawInfo::FDataPointId** SourceDataPointId = SourcesMap.Find(StreamInfo.Source);
					const FDestination* DestinationDataPointId = DestinationsMap.Find(StreamInfo.Destination);

					if (SourceDataPointId != nullptr && DestinationDataPointId != nullptr)
					{
						ConnectionsListBox.InsertItem(++ItemsCount);
						ConnectionsListBox.SetTabItemText(ItemsCount, kSourceColumn,
														  UEToGSString(*(*SourceDataPointId)->Name));
						FString DestinationName(DestinationDataPointId->EndpointInfo->Name + TEXT(" : ") +
												DestinationDataPointId->DataPointId->Name);
						ConnectionsListBox.SetTabItemText(ItemsCount, kDestinationColumn,
														  UEToGSString(*DestinationName));

                        /* kName_FmtTooltip "User : %T\nComputer Name : %T\nProgram Name: %T\nEndpoint Name : %T" */
                        GS::UniString ToolTipString(GS::UniString::Printf(GetGSName(kName_FmtTooltip),
                                                                          UEToGSString(*DestinationDataPointId->EndpointInfo->UserName).ToPrintf(),
                                                                          UEToGSString(*DestinationDataPointId->EndpointInfo->ComputerName).ToPrintf(),
                                                                          UEToGSString(*DestinationDataPointId->EndpointInfo->ExecutableName).ToPrintf(),
                                                                          UEToGSString(*DestinationDataPointId->EndpointInfo->Name).ToPrintf()));
                        ConnectionsListBox.SetItemObjectData(ItemsCount, new FToolTipText(ToolTipString));
					}
				}
			}

			if (ItemsCount > 0)
			{
				ConnectionsListBox.Show();
				NoConnectionText.Hide();
			}
			else
			{
				ConnectionsListBox.Hide();
				NoConnectionText.Show();
			}

			UE_AC_TraceF("Connection status changed\n");
		}
	}

	virtual void PanelResized(const DG::PanelResizeEvent& ev) override
	{
		if (ev.GetSource() == this)
		{
			ConnectionsListBox.MoveAndResize(0, 0, ev.GetHorizontalChange(), ev.GetVerticalChange());
			const short ListBoxSize = ConnectionsListBox.GetItemWidth();
			const short SourceColumnSize = ListBoxSize / 3;
			const short DestinationColumnSize = ListBoxSize - SourceColumnSize;

			ConnectionsListBox.SetHeaderItemSize(kSourceColumn, SourceColumnSize);
			ConnectionsListBox.SetHeaderItemSize(kDestinationColumn, DestinationColumnSize);

			ConnectionsListBox.SetTabFieldProperties(kSourceColumn, 0, SourceColumnSize, DG::ListBox::Left,
													 DG::ListBox::MiddleTruncate, false);
			ConnectionsListBox.SetTabFieldProperties(kDestinationColumn, SourceColumnSize,
													 SourceColumnSize + DestinationColumnSize, DG::ListBox::Left,
													 DG::ListBox::MiddleTruncate, false);

			NoConnectionText.MoveAndResize(0, 0, ev.GetHorizontalChange(), ev.GetVerticalChange());

			CacheFolderText.MoveAndResize(0, ev.GetVerticalChange(), ev.GetHorizontalChange(), 0);
			GotoCacheFolderButton.MoveAndResize(ev.GetHorizontalChange(), ev.GetVerticalChange(), 0, 0);
			ChooseCacheFolderButton.MoveAndResize(ev.GetHorizontalChange(), ev.GetVerticalChange(), 0, 0);
		}
	}

	virtual void ButtonClicked(const DG::ButtonClickEvent& ev) override
	{
		if (ev.GetSource() == &ChooseCacheFolderButton)
		{
			GS::UniString CachePath(FSyncDatabase::GetCachePath());
			IO::Location  NewFolderLocation;
			IO::Location  CurrentFolderLocation(CachePath);
			if (DGBrowseForFolder(&NewFolderLocation, &CurrentFolderLocation, GS::UniString("Choose a cache folder")))
			{
				GSErrCode GSErr = NewFolderLocation.ToPath(&CachePath);
				if (GSErr == NoError)
				{
					UE_AC_TraceF("FConnectionDialog::ButtonClicked - New cache folder is \"%s\"\n", CachePath.ToUtf8());
					FSyncDatabase::SetCachePath(CachePath);
					UpdateCacheDirectoryText();
				}
				else
				{
					UE_AC_DebugF("FConnectionDialog::ButtonClicked - NewFolderLocation.ToPath return error %s\n",
								 GetErrorName(GSErr));
				}
			}
		}
		else if (ev.GetSource() == &GotoCacheFolderButton)
		{
			UE_AC::ShellOpenDocument(FSyncDatabase::GetCachePath().ToUtf8());
		}
	}
	
	void UpdateCacheDirectoryText()
	{
		CacheFolderText.SetText(GetGSName(kName_CacheDirectory) + FSyncDatabase::GetCachePath());
	}

#if PLATFORM_MAC & AC_VERSION > 25
	virtual void ItemMouseExited(const DG::ItemMouseMoveEvent& /*ev*/) override {}
	virtual void ItemMouseEntered(const DG::ItemMouseMoveEvent& /*ev*/) override {}
	virtual short SpecMouseExited(const DG::ItemMouseMoveEvent& /*ev*/) override { return 0; }
	virtual short SpecMouseEntered(const DG::ItemMouseMoveEvent& /*ev*/) override { return 0; }
#endif
};

FConnectionDialog::FConnectionDialog()
	: DG::Palette(ACAPI_GetOwnResModule(), LocalizeResId(kDlgConnections), ACAPI_GetOwnResModule(), PaletteGuid)
	, ConnectionsListBox(GetReference(), kConnectionsSingleSelListId)
	, NoConnectionText(GetReference(), kNoConnectionTextId)
	, CacheFolderText(GetReference(), kCacheFolderTextId)
	, GotoCacheFolderButton(GetReference(), kGotoCacheFolderButtonId)
	, ChooseCacheFolderButton(GetReference(), kChooseCacheFolderButtonId)
{
	// initialize the listbox
	ConnectionsListBox.SetTabFieldCount(kNbColumns);
	ConnectionsListBox.SetHeaderSynchronState(false);

	const short ListBoxSize = ConnectionsListBox.GetItemWidth();
	const short SourceColumnSize = ListBoxSize / 3;
	const short DestinationColumnSize = ListBoxSize - SourceColumnSize;

	ConnectionsListBox.SetHeaderItemSize(kSourceColumn, SourceColumnSize);
	ConnectionsListBox.SetHeaderItemSize(kDestinationColumn, DestinationColumnSize);

	ConnectionsListBox.SetTabFieldProperties(kSourceColumn, 0, SourceColumnSize, DG::ListBox::Left,
											 DG::ListBox::MiddleTruncate, false);
	ConnectionsListBox.SetTabFieldProperties(kDestinationColumn, SourceColumnSize,
											 SourceColumnSize + DestinationColumnSize, DG::ListBox::Left,
											 DG::ListBox::MiddleTruncate, false);

	ConnectionsListBox.SetHeaderItemText(kSourceColumn, GS::UniString("Source"));
	ConnectionsListBox.SetHeaderItemText(kDestinationColumn, GS::UniString("Destination"));
    ConnectionsListBox.SetHelpStyle(ConnectionsListBox.HSForItem);

    ConnectionsListBox.Hide();
#if AC_VERSION >= 24
    ConnectionsListBox.onToolTipRequested += [this](const DG::Item& inItem, DG::HelpEventArg& EventArgs) -> short
    {
        if (&inItem != &ConnectionsListBox)
        {
            UE_AC_DebugF("ConnectionsListBox.onToolTipRequested - Item isn't ConnectionsListBox\n");
            return 0;
        }
        if (EventArgs.subMessage != DG_HSM_TOOLTIP || EventArgs.listItem == 0)
        {
            return 0;
        }
        const GS::Object* Object = ConnectionsListBox.GetItemObjectData(EventArgs.listItem);
        if (Object == nullptr)
        {
            UE_AC_DebugF("ConnectionsListBox.onToolTipRequested - Object is null\n");
            return 0;
        }
        EventArgs.toolTipText += static_cast<const FToolTipText&>(*Object).ToolTipText;
        return 1;
    };
#endif
	Attach(*this);
	AttachToAllItems(*this);

	UpdateCacheDirectoryText();

	bool SendForInactiveApp = false;
	EnableIdleEvent(SendForInactiveApp);
	ConnectionDialog = this;
	BeginEventProcessing();
}

FConnectionDialog::~FConnectionDialog()
{
	EndEventProcessing();
	DetachFromAllItems(*this);
	Detach(*this);

	if (ConnectionDialog == this)
	{
		ConnectionDialog = nullptr;
	}
}

GS::Guid FConnectionDialog::PaletteGuid("5E23E6F0-7F6A-4BA4-91EC-2F7BA22D4CB3");

void FConnectionWindow::ShowWindow()
{
	if (ConnectionDialog == nullptr)
	{
		new FConnectionDialog();
	}
	if (ConnectionDialog != nullptr)
	{
		ConnectionDialog->Show();
		ConnectionDialog->BringToFront();
	}
}

void FConnectionWindow::DeleteWindow()
{
	if (ConnectionDialog != nullptr)
	{
		delete ConnectionDialog;
		ConnectionDialog = nullptr;
	}
}

END_NAMESPACE_UE_AC
