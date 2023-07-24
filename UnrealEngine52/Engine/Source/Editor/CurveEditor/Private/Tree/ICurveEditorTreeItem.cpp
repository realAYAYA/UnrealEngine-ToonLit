// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tree/ICurveEditorTreeItem.h"

const ICurveEditorTreeItem::FColumnNames ICurveEditorTreeItem::ColumnNames;

ICurveEditorTreeItem::FColumnNames::FColumnNames()
	: Label("Label")
	, SelectHeader("Select")
	, PinHeader("Pin")
{}