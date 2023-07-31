// Copyright Epic Games, Inc. All Rights Reserved.

#include "CommonAssetTypeActions_GenericInputActionDataTable.h"
#include "DataTableEditorModule.h"
#include "Input/CommonGenericInputActionDataTable.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FCommonAssetTypeActions_GenericInputActionDataTable::GetName() const
{
	return LOCTEXT("FCommonAssetTypeActions_GenericInputActionDataTable", "Common UI InputActionDataTable");
}

FColor FCommonAssetTypeActions_GenericInputActionDataTable::GetTypeColor() const
{
	// brown
	return FColor(139.f, 69.f, 19.f);
}

UClass* FCommonAssetTypeActions_GenericInputActionDataTable::GetSupportedClass() const
{
	return UCommonGenericInputActionDataTable::StaticClass();
}

void FCommonAssetTypeActions_GenericInputActionDataTable::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	//@TODO: DarenC - This is from FAssetTypeActions_DataTable - but we can't derive to private include.
	TArray<UDataTable*> DataTablesToOpen;
	TArray<UDataTable*> InvalidDataTables;

	for (UObject* Obj : InObjects)
	{
		UDataTable* Table = Cast<UDataTable>(Obj);
		if (Table)
		{
			if (Table->GetRowStruct())
			{
				DataTablesToOpen.Add(Table);
			}
			else
			{
				InvalidDataTables.Add(Table);
			}
		}
	}

	if (InvalidDataTables.Num() > 0)
	{
		FTextBuilder DataTablesListText;
		DataTablesListText.Indent();
		for (UDataTable* Table : InvalidDataTables)
		{
			const FTopLevelAssetPath ResolvedRowStructName = Table->GetRowStructPathName();
			DataTablesListText.AppendLineFormat(LOCTEXT("DataTable_MissingRowStructListEntry", "* {0} (Row Structure: {1})"), FText::FromString(Table->GetName()), FText::FromString(ResolvedRowStructName.ToString()));
		}

		FText Title = LOCTEXT("DataTable_MissingRowStructTitle", "Continue?");
		const EAppReturnType::Type DlgResult = FMessageDialog::Open(
			EAppMsgType::YesNoCancel,
			FText::Format(LOCTEXT("DataTable_MissingRowStructMsg", "The following Data Tables are missing their row structure and will not be editable.\n\n{0}\n\nDo you want to open these data tables?"), DataTablesListText.ToText()),
			&Title
		);

		switch (DlgResult)
		{
		case EAppReturnType::Yes:
			DataTablesToOpen.Append(InvalidDataTables);
			break;
		case EAppReturnType::Cancel:
			return;
		default:
			break;
		}
	}

	FDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FDataTableEditorModule>("DataTableEditor");
	for (UDataTable* Table : DataTablesToOpen)
	{
		DataTableEditorModule.CreateDataTableEditor(EToolkitMode::Standalone, EditWithinLevelEditor, Table);
	}
}

#undef LOCTEXT_NAMESPACE
