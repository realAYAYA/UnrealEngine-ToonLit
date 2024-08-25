// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownRCPropertyItem.h"

#include "RemoteControlEntity.h"
#include "SAvaRundownRCPropertyItemRow.h"

FAvaRundownRCPropertyItem::FAvaRundownRCPropertyItem(TSharedRef<FRemoteControlEntity> InEntity, bool bInControlled)
{
	Entity = InEntity;
	bEntityControlled = bInControlled;
}

TSharedRef<ITableRow> FAvaRundownRCPropertyItem::CreateWidget(TSharedRef<SAvaRundownPageRemoteControlProps> InPropertyPanel, const TSharedRef<STableViewBase>& InOwnerTable) const
{
	return SNew(SAvaRundownRCPropertyItemRow, InPropertyPanel, InOwnerTable, SharedThis(this));
}
