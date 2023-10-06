// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dataflow/CollectionSpreadSheetWidget.h"
#include "Widgets/Input/SButton.h"
#include "Styling/StarshipCoreStyle.h"
#include "Widgets/Layout/SScrollBox.h"


const FName FCollectionSpreadSheetHeader::IndexColumnName = FName("Index");

namespace CollectionSpreadSheetHelpers
{
	template<typename T>
	FString AttributeValueToString(const T& Value)
	{
		return Value.ToString();
	}

	FString AttributeValueToString(float Value)
	{
		return FString::SanitizeFloat(Value, 2);
	}

	FString AttributeValueToString(int32 Value)
	{
		return FString::FromInt(Value);
	}

	FString AttributeValueToString(FString Value)
	{
		return Value;
	}

	FString AttributeValueToString(FLinearColor Value)
	{
		return FString::Printf(TEXT("(R=%.2f G=%.2f B=%.2f A=%.2f)"), Value.R, Value.G, Value.B, Value.A);
	}

	FString AttributeValueToString(FVector Value)
	{
		return FString::Printf(TEXT("(X=%.2f Y=%.2f Z=%.2f)"), Value.X, Value.Y, Value.Z);
	}

	FString AttributeValueToString(bool Value)
	{
		return Value ? FString("true") : FString("false");
	}

	FString AttributeValueToString(const FConstBitReference& Value)
	{
		return Value ? FString("true") : FString("false");
	}

	FString AttributeValueToString(TSet<int32> Value)
	{
		TArray<int32> Array = Value.Array();
		FString Out;

		for (int32 Idx = 0; Idx < Array.Num(); ++Idx)
		{
			Out += AttributeValueToString(Array[Idx]);

			if (Idx != Array.Num() - 1)
			{
				Out += " ";
			}
		}

		return Out;
	}

	template<typename T>
	FString AttributeValueToString(const TArray<T>& Array)
	{
		FString Out;

		for (int32 Idx = 0; Idx < Array.Num(); ++Idx)
		{
			Out += AttributeValueToString(Array[Idx]);

			if (Idx != Array.Num() - 1)
			{
				Out += "; ";
			}
		}

		return Out;
	}

	FString AttributeValueToString(FTransform Value)
	{
		const FVector Translation = Value.GetTranslation();
		const FVector Rotation = Value.GetRotation().Euler();
		const FVector Scale = Value.GetScale3D();

		return FString::Printf(TEXT("T:(%s) R:(%s) S:(%s)"), *Translation.ToString(), *Rotation.ToString(), *Scale.ToString());
	}

	FString AttributeValueToString(FBox Value)
	{
		return FString::Printf(TEXT("Min:(%s) Max:(%s)"), *Value.Min.ToString(), *Value.Max.ToString());
	}

	FString AttributeValueToString(FIntVector Value)
	{
		return FString::Printf(TEXT("%d %d %d"), Value.X, Value.Y, Value.Z);
	}

	template<typename T>
	FString AttributeValueToString(const FManagedArrayCollection& InCollection, const FName& InAttributeName, const FName& InGroupName, int32 InIdxColumn)
	{
		const TManagedArray<T>* const Array = InCollection.FindAttributeTyped<T>(InAttributeName, InGroupName);
		if (Array == nullptr)
		{
			return FString("<Unknown Attribute>");
		}

		return AttributeValueToString((*Array)[InIdxColumn]);
	}

	FString AttributeValueToString(const FManagedArrayCollection& InCollection, const FName& InAttributeName, const FName& InGroupName, int32 InIdxColumn)
	{
		const FManagedArrayCollection::EArrayType ArrayType = InCollection.GetAttributeType(InAttributeName, InGroupName);

		FString ValueAsString;

		switch (ArrayType)
		{
		case FManagedArrayCollection::EArrayType::FFloatType:
			ValueAsString = AttributeValueToString<float>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FInt32Type:
			ValueAsString = AttributeValueToString<int32>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FBoolType:
			ValueAsString = AttributeValueToString<bool>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FStringType:
			ValueAsString = AttributeValueToString<FString>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FLinearColorType:
			ValueAsString = AttributeValueToString<FLinearColor>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FVectorType:
			ValueAsString = AttributeValueToString<FVector3f>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FVector2DType:
			ValueAsString = AttributeValueToString<FVector2f>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FVector3dType:
			ValueAsString = AttributeValueToString<FVector2f>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FIntVectorType:
			ValueAsString = AttributeValueToString<FIntVector>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FTransformType:
			ValueAsString = AttributeValueToString<FTransform>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FVector2DArrayType:
			ValueAsString = AttributeValueToString<TArray<FVector2f>>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FIntArrayType:
			ValueAsString = AttributeValueToString<TSet<int32>>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FInt32ArrayType:
			ValueAsString = AttributeValueToString<TArray<int32>>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FFloatArrayType:
			ValueAsString = AttributeValueToString<TArray<float>>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		case FManagedArrayCollection::EArrayType::FBoxType:
			ValueAsString = AttributeValueToString<FBox>(InCollection, InAttributeName, InGroupName, InIdxColumn);
			break;

		default:
			//ensure(false);
			ValueAsString = "<Unknown Data Type>";
		}

		return ValueAsString;
	}
}


void SCollectionSpreadSheetRow::Construct(const FArguments& InArgs, TSharedRef<STableViewBase> OwnerTableView, const TSharedPtr<const FCollectionSpreadSheetHeader>& InHeader, const TSharedPtr<const FCollectionSpreadSheetItem>& InItem)
{
	Header = InHeader;
	Item = InItem;

	SMultiColumnTableRow<TSharedPtr<const FCollectionSpreadSheetItem>>::Construct(
		FSuperRowType::FArguments()
		.Style(&FStarshipCoreStyle::GetCoreStyle().GetWidgetStyle<FTableRowStyle>("TableView.AlternatingRow")),
		OwnerTableView);
}


TSharedRef<SWidget> SCollectionSpreadSheetRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	int32 FoundIndex;
	if (Header->ColumnNames.Find(ColumnName, FoundIndex))
	{
		const FString& AttrValue = Item->Values[FoundIndex];

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(AttrValue))
				.ShadowColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 1.f))
				.Visibility(EVisibility::Visible)
			];
	}

	return SNullWidget::NullWidget;
}

//
// ----------------------------------------------------------------------------
//

void SCollectionSpreadSheet::Construct(const FArguments& InArgs)
{
	SelectedOutput = InArgs._SelectedOutput;

	SpreadSheetVerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical)
		.Thickness(FVector2D(12.0f, 12.0f));

	HeaderRowWidget = SNew(SHeaderRow)
		.Visibility(EVisibility::Visible);

	if (!CollectionInfoMap.IsEmpty())
	{
		RegenerateHeader();
		RepopulateListView();
	}

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 3.f))
		[
			SNew(SOverlay)
			+ SOverlay::Slot()
			[
				SAssignNew(ListView, SListView<TSharedPtr<const FCollectionSpreadSheetItem>>)
//				SAssignNew(ListView, STreeView<TSharedPtr<const FCollectionSpreadSheetItem>>)
				.SelectionMode(ESelectionMode::Multi)
				.ListItemsSource(&ListItems)
//				.TreeItemsSource(&ListItems)
				.OnGenerateRow(this, &SCollectionSpreadSheet::GenerateRow)
				.HeaderRow(HeaderRowWidget)
				.ExternalScrollbar(SpreadSheetVerticalScrollBar)
			]
		]
	];

	AttrTypeWidthMap.Add("Transform", 600);
	AttrTypeWidthMap.Add("String", 200);
	AttrTypeWidthMap.Add("LinearColor", 250);
	AttrTypeWidthMap.Add("int32", 100);
	AttrTypeWidthMap.Add("IntArray", 200);
	AttrTypeWidthMap.Add("Vector", 300);
	AttrTypeWidthMap.Add("Vector2D", 160);
	AttrTypeWidthMap.Add("Float", 150);
	AttrTypeWidthMap.Add("IntVector", 150);
	AttrTypeWidthMap.Add("Bool", 75);
	AttrTypeWidthMap.Add("Box", 550);
	AttrTypeWidthMap.Add("MeshSection", 0);
	AttrTypeWidthMap.Add("UInt8", 0);
}


void SCollectionSpreadSheet::SetSelectedOutput(const FName& InSelectedOutput)
{
	SelectedOutput = InSelectedOutput;

	RegenerateHeader();
	RepopulateListView();
}


const FName& SCollectionSpreadSheet::GetSelectedOutput() const
{
	return SelectedOutput;
}


void SCollectionSpreadSheet::SetSelectedGroup(const FName& InSelectedGroup)
{
	SelectedGroup = InSelectedGroup;

	RegenerateHeader();
	RepopulateListView();
}


const FName& SCollectionSpreadSheet::GetSelectedGroup() const
{
	return SelectedGroup;
}

namespace {
	inline FName GetArrayTypeString(FManagedArrayCollection::EArrayType ArrayType)
	{
		switch (ArrayType)
		{
#define MANAGED_ARRAY_TYPE(a,A)	case EManagedArrayType::F##A##Type:\
			return FName(#A);
#include "GeometryCollection/ManagedArrayTypeValues.inl"
#undef MANAGED_ARRAY_TYPE
		}
		return FName();
	}
}

void SCollectionSpreadSheet::RegenerateHeader()
{
	HeaderRowWidget->ClearColumns();

	Header = MakeShared<FCollectionSpreadSheetHeader>();
	TArray<FString> AttrTypes;

	if (CollectionInfoMap.Num() > 0 && !SelectedOutput.IsNone() && SelectedOutput.ToString() != "" &&
		!SelectedGroup.IsNone() && SelectedGroup.ToString() != "")
	{
		Header->ColumnNames.Add(FCollectionSpreadSheetHeader::IndexColumnName);

		for (FName Attr : CollectionInfoMap[SelectedOutput.ToString()].Collection.AttributeNames(SelectedGroup))
		{
			Header->ColumnNames.Add(Attr);
			AttrTypes.Add(GetArrayTypeString(CollectionInfoMap[SelectedOutput.ToString()].Collection.GetAttributeType(Attr, SelectedGroup)).ToString());
		}

	}

	for (int32 IdxAttr = 0; IdxAttr < Header->ColumnNames.Num(); ++IdxAttr)
	{
		const FName& ColumnName = Header->ColumnNames[IdxAttr];
		FName ToolTip;

		FString ColumnNameStr = ColumnName.ToString();
		FString AttrTypeStr;

		if (IdxAttr > 0)
		{
			ToolTip = FName(*FString::Printf(TEXT("Attr: %s\nType: %s"), *ColumnName.ToString(), *AttrTypes[IdxAttr - 1])); // IdxAttr needs to be adjusted because of the first Index column

			AttrTypeStr = AttrTypes[IdxAttr - 1];
		}
		else
		{
			ToolTip = FName("");
		}

		int32 ColumnWidth = 100;
		if (ColumnNameStr == "Index")
		{
			ColumnWidth = 100;
		}
		else
		{
			int32 ColumnNameStrLen = ColumnNameStr.Len() * 9;
			int32 AttrTypeWidth = 100;
			if (AttrTypeWidthMap.Contains(AttrTypeStr))
			{
				AttrTypeWidth = AttrTypeWidthMap[AttrTypeStr];
			}
			ColumnWidth = ColumnNameStrLen > AttrTypeWidth ? ColumnNameStrLen : AttrTypeWidth;
		}

		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(ColumnName)
			.DefaultLabel(FText::FromName(ColumnName))
			.DefaultTooltip(FText::FromName(ToolTip))
			.ManualWidth(ColumnWidth)
			//			.SortMode(EColumnSizeMode)
			.HAlignCell(HAlign_Center)
			.HAlignHeader(HAlign_Center)
			.VAlignCell(VAlign_Center)
		);
	}
}


void SCollectionSpreadSheet::RepopulateListView()
{
	ListItems.Empty();

	if (CollectionInfoMap.Num() > 0 &&
		!SelectedOutput.IsNone() && SelectedOutput.ToString() != "" &&
		!SelectedGroup.IsNone() && SelectedGroup.ToString() != "")
	{
		int32 NumElems = CollectionInfoMap[SelectedOutput.ToString()].Collection.NumElements(SelectedGroup);

		for (int32 IdxElem = 0; IdxElem < NumElems; ++IdxElem)
		{
			const TSharedPtr<FCollectionSpreadSheetItem> NewItem = MakeShared<FCollectionSpreadSheetItem>();
			NewItem->Values.SetNum(Header->ColumnNames.Num());

			for (int32 IdxColumn = 0; IdxColumn < Header->ColumnNames.Num(); ++IdxColumn)
			{
				const FName& ColumnName = Header->ColumnNames[IdxColumn];
				if (ColumnName == FCollectionSpreadSheetHeader::IndexColumnName)
				{
					NewItem->Values[IdxColumn] = FString::FromInt(IdxElem);
				}
				else
				{
					NewItem->Values[IdxColumn] = CollectionSpreadSheetHelpers::AttributeValueToString(CollectionInfoMap[SelectedOutput.ToString()].Collection, ColumnName, SelectedGroup, IdxElem);
				}
			}

			ListItems.Add(NewItem);
		}
	}

	ListView->RequestListRefresh();
}

TSharedRef<ITableRow> SCollectionSpreadSheet::GenerateRow(TSharedPtr<const FCollectionSpreadSheetItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedRef<SCollectionSpreadSheetRow> NewCollectionSpreadSheetRow = SNew(SCollectionSpreadSheetRow, OwnerTable, this->Header, InItem);

	return NewCollectionSpreadSheetRow;
}


//
// ----------------------------------------------------------------------------
//

void SCollectionSpreadSheetWidget::NodeOutputsComboBoxSelectionChanged(FName InSelectedOutput, ESelectInfo::Type InSelectInfo)
{
	if (CollectionTable)
	{
		if (CollectionTable->GetSelectedOutput() != InSelectedOutput)
		{
			CollectionTable->SetSelectedOutput(InSelectedOutput);

			NodeOutputsComboBoxLabel->SetText(FText::FromName(CollectionTable->GetSelectedOutput()));

			CollectionGroupsComboBox->RefreshOptions();
			CollectionGroupsComboBox->ClearSelection();

			UpdateCollectionGroups(InSelectedOutput);

			if (CollectionGroups.Num() > 0)
			{
				CollectionGroupsComboBox->SetSelectedItem(CollectionGroups[0]);
			}
		}
	}
}


void SCollectionSpreadSheetWidget::CollectionGroupsComboBoxSelectionChanged(FName InSelectedGroup, ESelectInfo::Type InSelectInfo)
{
	if (CollectionTable)
	{
		if (CollectionTable->GetSelectedGroup() != InSelectedGroup)
		{
			CollectionTable->SetSelectedGroup(InSelectedGroup);

			CollectionGroupsComboBoxLabel->SetText(FText::FromName(CollectionTable->GetSelectedGroup()));

			if (!InSelectedGroup.IsNone())
			{
				int32 NumElems = CollectionTable->GetCollectionInfoMap()[CollectionTable->GetSelectedOutput().ToString()].Collection.NumElements(InSelectedGroup);
				CollectionTable->SetNumItems(NumElems);
			}
		}
	}

	SetStatusText();
}


FText SCollectionSpreadSheetWidget::GetNoOutputText()
{
	return FText::FromString("No Output(s)");
}


FText SCollectionSpreadSheetWidget::GetNoGroupText()
{
	return FText::FromString("No Group(s)");
}


void SCollectionSpreadSheetWidget::Construct(const FArguments& InArgs)
{
	TSharedPtr<SScrollBar> SpreadSheetHorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal)
		.Thickness(FVector2D(12.0f, 12.0f));

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// Node: [ MakeBox_0                 ] [O]
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 10.0f, 0.0f, 5.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString("Node: "))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(10.0f, 10.0f, 10.0f, 5.0f)
			[
				SAssignNew(NodeNameTextBlock, STextBlock)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 10.0f, 4.0f, 5.0f)
			[
				SNew(SButton)
				.ToolTipText(FText::FromString("The button pins down the panel. When it pinned down it doesn't react to node selection change."))
				.ButtonStyle(FAppStyle::Get(), "Button")
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Content()
				[
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						if (!NodeNameTextBlock->GetText().IsEmpty())
						{
							return FText::FromString(bIsPinnedDown ? " X " : " O ");
						}
						else
						{
							return FText::FromString(" O ");
						}
					})
				]
				.OnClicked_Lambda([this]
				{
					if (!NodeNameTextBlock->GetText().IsEmpty())
					{
						bIsPinnedDown = !bIsPinnedDown;
		
						OnPinnedDownChangedDelegate.Broadcast(bIsPinnedDown);
					}

					return FReply::Handled();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 10.0f, 10.0f, 5.0f)
			[
				SNew(SButton)
				.ToolTipText(FText::FromString("The button locks the refresh of the values in the panel."))
				.ButtonStyle(FAppStyle::Get(), "Button")
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Content()
				[
					SNew(STextBlock)
					.Text_Lambda([this]
					{
						if (!NodeNameTextBlock->GetText().IsEmpty())
						{
							return FText::FromString(bIsRefreshLocked ? " L " : " U ");
						}
						else
						{
							return FText::FromString(" U ");
						}
					})
				]
				.OnClicked_Lambda([this]
				{
					if (!NodeNameTextBlock->GetText().IsEmpty())
					{
						bIsRefreshLocked = !bIsRefreshLocked;
		
						OnRefreshLockedChangedDelegate.Broadcast(bIsRefreshLocked);
					}
		
					return FReply::Handled();
				})
			]

		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// Output: [ TransformSelection        |V|]
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 0.0f, 10.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString("Output: "))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(10.0f, 0.0f, 10.0f, 10.0f)
			[
				SAssignNew(NodeOutputsComboBox, SComboBox<FName>)
				.ToolTipText(FText::FromString("Select a node output to see the output's data"))
				.OptionsSource(&NodeOutputs)
				.OnGenerateWidget(SComboBox<FName>::FOnGenerateWidget::CreateLambda([](FName Output)->TSharedRef<SWidget>
				{
					return SNew(STextBlock)
						.Text(FText::FromName(Output));
				}))
				.OnSelectionChanged(this, &SCollectionSpreadSheetWidget::NodeOutputsComboBoxSelectionChanged)
				[
					SAssignNew(NodeOutputsComboBoxLabel, STextBlock)
					.Text(GetNoOutputText())
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			// Group: [ TransformGroup        |V|]
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 0.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString("Group: "))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.Padding(10.0f, 0.0f, 10.0f, 2.0f)
			[
				SAssignNew(CollectionGroupsComboBox, SComboBox<FName>)
				.ToolTipText(FText::FromString("Select a group see the corresponding data"))
				.OptionsSource(&CollectionGroups)
				.OnGenerateWidget(SComboBox<FName>::FOnGenerateWidget::CreateLambda([](FName Group)->TSharedRef<SWidget>
				{
					return SNew(STextBlock)
						.Text(FText::FromName(Group));
				}))
				.OnSelectionChanged(this, &SCollectionSpreadSheetWidget::CollectionGroupsComboBoxSelectionChanged)
				[
					SAssignNew(CollectionGroupsComboBoxLabel, STextBlock)
						.Text(GetNoGroupText())
				]
			]
		]

		+SVerticalBox::Slot()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			[
				SNew(SScrollBox)
				.Orientation(Orient_Horizontal)
				.ExternalScrollbar(SpreadSheetHorizontalScrollBar)

				+ SScrollBox::Slot()
				[
					SAssignNew(CollectionTable, SCollectionSpreadSheet)
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				CollectionTable->SpreadSheetVerticalScrollBar.ToSharedRef()
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SpreadSheetHorizontalScrollBar.ToSharedRef()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10.0f, 10.0f, 0.0f, 5.0f)
			[
				SAssignNew(StatusTextBlock, STextBlock)
			]
		]
	];
}


void SCollectionSpreadSheetWidget::SetData(const FString& InNodeName)
{
	NodeName = InNodeName;

	NodeOutputs.Empty();

	if (!NodeName.IsEmpty())
	{
		if (CollectionTable->GetCollectionInfoMap().Num() > 0)
		{
			for (auto& Info : CollectionTable->GetCollectionInfoMap())
			{
				NodeOutputs.Add(FName(*Info.Key));
			}
		}
	}

	CollectionGroups.Empty();
}


void SCollectionSpreadSheetWidget::RefreshWidget()
{
	NodeNameTextBlock->SetText(FText::FromString(NodeName));

	NodeOutputsComboBox->RefreshOptions();
	NodeOutputsComboBox->ClearSelection();

	if (NodeOutputs.Num() > 0)
	{
		NodeOutputsComboBox->SetSelectedItem(NodeOutputs[0]);
	}
	else
	{
		NodeOutputsComboBoxLabel->SetText(GetNoOutputText());
	}

	CollectionGroupsComboBox->RefreshOptions();
	CollectionGroupsComboBox->ClearSelection();

	if (NodeOutputs.Num() > 0)
	{
		UpdateCollectionGroups(NodeOutputs[0]);
	}

	if (CollectionGroups.Num() > 0)
	{
		CollectionGroupsComboBox->SetSelectedItem(CollectionGroups[0]);
	}
	else
	{
		CollectionGroupsComboBoxLabel->SetText(GetNoGroupText());
	}
}


void SCollectionSpreadSheetWidget::SetStatusText()
{
	if (!NodeName.IsEmpty())
	{
		FString Str = FString::Printf(TEXT("Group has %d elements"), CollectionTable->GetNumItems());
		StatusTextBlock->SetText(FText::FromString(Str));
	}
	else
	{
		StatusTextBlock->SetText(FText::FromString(" "));
	}
}


void SCollectionSpreadSheetWidget::UpdateCollectionGroups(const FName& InOutputName)
{
	if (!InOutputName.IsNone())
	{
		CollectionGroups.Empty();

		for (FName Group : GetCollectionTable()->GetCollectionInfoMap()[InOutputName.ToString()].Collection.GroupNames())
		{
			CollectionGroups.Add(Group);
		}
	}
}

