// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/DataflowCollectionSpreadSheet.h"
#include "Templates/EnableIf.h"
#include "Dataflow/DataflowSelection.h"


void FDataflowCollectionSpreadSheet::SetSupportedOutputTypes()
{
	GetSupportedOutputTypes().Empty();

	GetSupportedOutputTypes().Add("FManagedArrayCollection");
}

void FDataflowCollectionSpreadSheet::UpdateViewData()
{
	if (CollectionSpreadSheet)
	{
		CollectionSpreadSheet->GetCollectionTable()->GetCollectionInfoMap().Empty();

		if (GetSelectedNode())
		{
			if (GetSelectedNode()->IsBound())
			{
				if (TSharedPtr<FDataflowNode> DataflowNode = GetSelectedNode()->DataflowGraph->FindBaseNode(GetSelectedNode()->DataflowNodeGuid))
				{
					TArray<FDataflowOutput*> Outputs = DataflowNode->GetOutputs();

					for (FDataflowOutput* Output : Outputs)
					{
						FName Name = Output->GetName();
						FName Type = Output->GetType();

						if (Output->GetType() == "FManagedArrayCollection")
						{
							const FManagedArrayCollection& Value = Output->GetValue<FManagedArrayCollection>(*GetContext(), FManagedArrayCollection());

							CollectionSpreadSheet->GetCollectionTable()->GetCollectionInfoMap().Add(Name.ToString(), { Value });
						}
					}
				}
			}

			CollectionSpreadSheet->SetData(GetSelectedNode()->GetName());
		}
		else
		{
			CollectionSpreadSheet->SetData(FString());
		}

		CollectionSpreadSheet->RefreshWidget();
	}
}


void FDataflowCollectionSpreadSheet::SetCollectionSpreadSheet(TSharedPtr<SCollectionSpreadSheetWidget>& InCollectionSpreadSheet)
{
	ensure(!CollectionSpreadSheet);

	CollectionSpreadSheet = InCollectionSpreadSheet;

	if (CollectionSpreadSheet)
	{
		OnPinnedDownChangedDelegateHandle = CollectionSpreadSheet->GetOnPinnedDownChangedDelegate().AddRaw(this, &FDataflowCollectionSpreadSheet::OnPinnedDownChanged);
		OnRefreshLockedChangedDelegateHandle = CollectionSpreadSheet->GetOnRefreshLockedChangedDelegate().AddRaw(this, &FDataflowCollectionSpreadSheet::OnRefreshLockedChanged);
	}
}


FDataflowCollectionSpreadSheet::~FDataflowCollectionSpreadSheet()
{
	if (CollectionSpreadSheet)
	{
		CollectionSpreadSheet->GetOnPinnedDownChangedDelegate().Remove(OnPinnedDownChangedDelegateHandle);
	}
}
