// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class IDetailTreeNode;
class ITableRow;
class SAvaRundownPageRemoteControlProps;
class STableViewBase;
class URemoteControlPreset;
struct FAvaPageRemoteControlGenerateWidgetArgs;
struct FRemoteControlEntity;

class FAvaRundownRCPropertyItem : public TSharedFromThis<FAvaRundownRCPropertyItem>
{
public:
	FAvaRundownRCPropertyItem(TSharedRef<FRemoteControlEntity> InEntity, bool bInControlled);

	TSharedRef<ITableRow> CreateWidget(TSharedRef<SAvaRundownPageRemoteControlProps> InPropertyPanel, const TSharedRef<STableViewBase>& InOwnerTable) const;

	TSharedPtr<FRemoteControlEntity> GetEntity() const { return Entity.Pin(); }
	bool IsEntityControlled() const { return bEntityControlled; }

private:
	TWeakPtr<FRemoteControlEntity> Entity = nullptr;
	bool bEntityControlled = false;
};
