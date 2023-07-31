// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Insights/Table/ViewModels/TableCellValue.h"

namespace Insights
{

class FBaseTreeNode;
class FTableColumn;

////////////////////////////////////////////////////////////////////////////////////////////////////

class ITableCellValueGetter
{
public:
	virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const = 0;
	virtual uint64 GetValueId(const FTableColumn& Column, const FBaseTreeNode& Node) const = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTableCellValueGetter : public ITableCellValueGetter
{
public:
	FTableCellValueGetter() {}
	virtual ~FTableCellValueGetter() {}

	virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const override { return TOptional<FTableCellValue>(); }
	virtual uint64 GetValueId(const FTableColumn& Column, const FBaseTreeNode& Node) const override { return 0; }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FNameValueGetter : public FTableCellValueGetter
{
public:
	virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FDisplayNameValueGetter : public FTableCellValueGetter
{
public:
	virtual const TOptional<FTableCellValue> GetValue(const FTableColumn& Column, const FBaseTreeNode& Node) const override;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
