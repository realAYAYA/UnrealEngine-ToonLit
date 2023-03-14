// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXChannelConnector.h"

#include "DMXEditor.h"
#include "DMXFixturePatchSharedData.h"
#include "Library/DMXLibrary.h"
#include "Library/DMXEntityFixturePatch.h"

#include "Widgets/SDMXChannel.h"


void SDMXChannelConnector::Construct(const FArguments& InArgs)
{
	check(InArgs._OnMouseButtonDownOnChannel.IsBound());
	check(InArgs._OnMouseButtonUpOnChannel.IsBound());
	check(InArgs._OnDragDetectedOnChannel.IsBound());
	check(InArgs._OnDragEnterChannel.IsBound());
	check(InArgs._OnDragLeaveChannel.IsBound());
	check(InArgs._OnDropOntoChannel.IsBound());

	OnMouseButtonDownOnChannel = InArgs._OnMouseButtonDownOnChannel;
	OnMouseButtonUpOnChannel = InArgs._OnMouseButtonUpOnChannel;
	OnDragDetectedOnChannel = InArgs._OnDragDetectedOnChannel;
	OnDragEnterChannel = InArgs._OnDragEnterChannel;
	OnDragLeaveChannel = InArgs._OnDragLeaveChannel;
	OnDropOntoChannel = InArgs._OnDropOntoChannel;

	ChannelID = InArgs._ChannelID;
	UniverseID = InArgs._UniverseID;

	DMXEditorPtr = InArgs._DMXEditor;

	ChildSlot
		[
			SAssignNew(ChannelValueWidget, SDMXChannel)
			.ChannelID(ChannelID)
			.Value(InArgs._Value)	
			.bShowChannelIDBottom(true)
		];
}

FReply SDMXChannelConnector::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnMouseButtonDownOnChannel.Execute(ChannelValueWidget->GetChannelID(), MouseEvent);
}

FReply SDMXChannelConnector::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnMouseButtonUpOnChannel.Execute(ChannelValueWidget->GetChannelID(), MouseEvent);
}

FReply SDMXChannelConnector::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	return OnDragDetectedOnChannel.Execute(ChannelValueWidget->GetChannelID(), MouseEvent);
}

void SDMXChannelConnector::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	OnDragEnterChannel.ExecuteIfBound(ChannelValueWidget->GetChannelID(), DragDropEvent);
}

void SDMXChannelConnector::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	OnDragLeaveChannel.ExecuteIfBound(ChannelValueWidget->GetChannelID(), DragDropEvent);
}

FReply SDMXChannelConnector::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	return OnDropOntoChannel.Execute(ChannelValueWidget->GetChannelID(), DragDropEvent);
}

UDMXLibrary* SDMXChannelConnector::GetDMXLibrary() const
{
	if (TSharedPtr<FDMXEditor> DMXEditor = DMXEditorPtr.Pin())
	{
		return DMXEditor->GetDMXLibrary();
	}
	return nullptr;
}
