// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/SListView.h"
#include "Materials/Material.h"
#include "IDetailTreeNode.h"
#include "IDetailPropertyRow.h"
#include "MaterialPropertyHelpers.h"

class UMaterialEditorPreviewParameters;

/** Data for a row in the list of custom primitive data entries*/
struct FCustomPrimitiveDataRowData
{
	FCustomPrimitiveDataRowData(int32 InSlot, FString InName, bool bInIsDuplicate = false) : Slot(InSlot), Name(InName), bIsDuplicate(bInIsDuplicate) {}

	int32 Slot = 0;
	FString Name = "";
	bool bIsDuplicate = false;
};

class SMaterialCustomPrimitiveDataPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialCustomPrimitiveDataPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UMaterialEditorPreviewParameters* InMaterialEditorInstance);

	void UpdateEditorInstance(UMaterialEditorPreviewParameters* InMaterialEditorInstance) { MaterialEditorInstance = InMaterialEditorInstance; Refresh(); }

private:

	void Refresh();

	/** Adds a new textbox with the string to the list */
	TSharedRef<ITableRow> OnGenerateRowForList(TSharedPtr<FCustomPrimitiveDataRowData> Item, const TSharedRef<STableViewBase>& OwnerTable);

	/** The list of strings */
	TArray<TSharedPtr<FCustomPrimitiveDataRowData>> Items;

	/** The actual UI list */
	TSharedPtr<SListView<TSharedPtr<FCustomPrimitiveDataRowData>>> ListViewWidget;

	/** The set of material parameters this is associated with */
	UMaterialEditorPreviewParameters* MaterialEditorInstance;
};