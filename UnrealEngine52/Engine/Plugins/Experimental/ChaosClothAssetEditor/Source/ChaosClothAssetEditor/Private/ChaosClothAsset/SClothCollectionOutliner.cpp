// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosClothAsset/SClothCollectionOutliner.h"
#include "ChaosClothAsset/ClothCollection.h"
#include "Templates/EnableIf.h"

#define LOCTEXT_NAMESPACE "ClothCollectionOutliner"

const FName FClothCollectionHeaderData::ColumnZeroName = FName("ID");

void SClothCollectionOutlinerRow::Construct(const FArguments& InArgs,
	TSharedRef<STableViewBase> OwnerTableView,
	const TSharedPtr<const FClothCollectionHeaderData>& InHeaderData,
	const TSharedPtr<const FClothCollectionItem>& InItemToEdit)
{
	HeaderData = InHeaderData;
	Item = InItemToEdit;

	SMultiColumnTableRow<TSharedPtr<const FClothCollectionItem>>::Construct(
		FSuperRowType::FArguments()
		.Style(&FAppStyle().GetWidgetStyle<FTableRowStyle>("TableView.AlternatingRow"))
		, OwnerTableView);
}


TSharedRef<SWidget> SClothCollectionOutlinerRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	int32 FoundIndex;
	if (HeaderData->AttributeNames.Find(ColumnName, FoundIndex))
	{
		const FString& AttrValue = Item->AttributeValues[FoundIndex];

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(AttrValue))
			];
	}

	return SNullWidget::NullWidget;
}


void SClothCollectionOutliner::Construct(const FArguments& InArgs)
{
	ClothCollection = InArgs._ClothCollection;
	SelectedGroupName = InArgs._SelectedGroupName;

	HeaderRowWidget =
		SNew(SHeaderRow)
		.Visibility(EVisibility::Visible);

	if (ClothCollection.IsValid())
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
				SAssignNew(ListView, SListView<TSharedPtr<const FClothCollectionItem>>)
				.SelectionMode(ESelectionMode::None)
				.ListItemsSource(&ListItems)
				.OnGenerateRow(this, &SClothCollectionOutliner::GenerateRow)
				.HeaderRow(HeaderRowWidget)
			]
		]
	];
}

void SClothCollectionOutliner::SetClothCollection(TWeakPtr<UE::Chaos::ClothAsset::FClothCollection> InClothCollection)
{
	ClothCollection = InClothCollection;
}

void SClothCollectionOutliner::SetSelectedGroupName(const FName& InSelectedGroupName)
{
	SelectedGroupName = InSelectedGroupName;

	RegenerateHeader();
	RepopulateListView();
}

const FName& SClothCollectionOutliner::GetSelectedGroupName() const
{
	return SelectedGroupName;
}

void SClothCollectionOutliner::RegenerateHeader()
{
	constexpr float CustomFillWidth = 2.0f;

	TSharedPtr<UE::Chaos::ClothAsset::FClothCollection> PinnedClothCollection = ClothCollection.Pin();

	if (!PinnedClothCollection.IsValid())
	{
		return;
	}

	HeaderRowWidget->ClearColumns();

	HeaderData = MakeShared<FClothCollectionHeaderData>();
	HeaderData->AttributeNames.Add(FClothCollectionHeaderData::ColumnZeroName);
	HeaderData->AttributeNames.Append(PinnedClothCollection->AttributeNames(SelectedGroupName));

	for (int32 AttributeNameIndex = 0; AttributeNameIndex < HeaderData->AttributeNames.Num(); ++AttributeNameIndex)
	{
		const FName& AttrName = HeaderData->AttributeNames[AttributeNameIndex];

		HeaderRowWidget->AddColumn(
			SHeaderRow::Column(AttrName)
			.DefaultLabel(FText::FromName(AttrName))
			.FillWidth(CustomFillWidth)
		);
	}
}

namespace ClothCollectionOutlinerHelpers
{
	template<typename T>
	FString AttributeValueToString(const T& Value)
	{
		return Value.ToString();
	}

	FString AttributeValueToString(float Value)
	{
		return FString::SanitizeFloat(Value);
	}

	FString AttributeValueToString(int32 Value)
	{
		return FString::FromInt(Value);
	}

	template<typename T>
	FString AttributeValueToString(const TArray<T>& Array)
	{
		FString Out;
		for ( int32 ArrayIndex = 0; ArrayIndex < Array.Num(); ++ArrayIndex )
		{
			Out += AttributeValueToString(Array[ArrayIndex]);

			if (ArrayIndex != Array.Num() - 1)
			{
				Out += "; ";
			}
		}
		return Out;
	}

	template<typename T>
	FString AttributeValueToString(const UE::Chaos::ClothAsset::FClothCollection& ClothCollection, const FName& AttributeName, const FName& GroupName, int32 AttributeArrayIndex)
	{
		const TManagedArray<T>* const Array = ClothCollection.FindAttributeTyped<T>(AttributeName, GroupName);
		if (Array == nullptr)
		{
			return FString("(Unknown Attribute)");
		}

		return AttributeValueToString((*Array)[AttributeArrayIndex]);
	}

	FString AttributeValueToString(const UE::Chaos::ClothAsset::FClothCollection& ClothCollection, const FName& AttributeName, const FName& GroupName, int32 AttributeArrayIndex)
	{
		const FManagedArrayCollection::EArrayType ArrayType = ClothCollection.GetAttributeType(AttributeName, GroupName);

		FString ValueAsString;

		switch (ArrayType)
		{
		case FManagedArrayCollection::EArrayType::FVectorType:
			ValueAsString = AttributeValueToString<FVector3f>(ClothCollection, AttributeName, GroupName, AttributeArrayIndex);
			break;
		case FManagedArrayCollection::EArrayType::FVector2DType:
			ValueAsString = AttributeValueToString<FVector2f>(ClothCollection, AttributeName, GroupName, AttributeArrayIndex);
			break;
		case FManagedArrayCollection::EArrayType::FFloatType:
			ValueAsString = AttributeValueToString<float>(ClothCollection, AttributeName, GroupName, AttributeArrayIndex);
			break;
		case FManagedArrayCollection::EArrayType::FIntVectorType:
			ValueAsString = AttributeValueToString<FIntVector>(ClothCollection, AttributeName, GroupName, AttributeArrayIndex);
			break;
		case FManagedArrayCollection::EArrayType::FVector2DArrayType:
			ValueAsString = AttributeValueToString<TArray<FVector2f>>(ClothCollection, AttributeName, GroupName, AttributeArrayIndex);
			break;
		case FManagedArrayCollection::EArrayType::FLinearColorType:
			ValueAsString = AttributeValueToString<FLinearColor>(ClothCollection, AttributeName, GroupName, AttributeArrayIndex);
			break;
		case FManagedArrayCollection::EArrayType::FInt32Type:
			ValueAsString = AttributeValueToString<int32>(ClothCollection, AttributeName, GroupName, AttributeArrayIndex);
			break;
		case FManagedArrayCollection::EArrayType::FInt32ArrayType:
			ValueAsString = AttributeValueToString<TArray<int32>>(ClothCollection, AttributeName, GroupName, AttributeArrayIndex);
			break;
		case FManagedArrayCollection::EArrayType::FFloatArrayType:
			ValueAsString = AttributeValueToString<TArray<float>>(ClothCollection, AttributeName, GroupName, AttributeArrayIndex);
			break;
		default:
			ensure(false);
			ValueAsString = "(Unknown Data Type)";
		}

		return ValueAsString;
	}
}

void SClothCollectionOutliner::RepopulateListView()
{
	ListItems.Empty();

	const TSharedPtr<const UE::Chaos::ClothAsset::FClothCollection> PinnedClothCollection = ClothCollection.Pin();

	if (!PinnedClothCollection.IsValid())
	{
		return;
	}

	const int32 NumElements = PinnedClothCollection->NumElements(SelectedGroupName);

	for (int32 ElementIndex = 0; ElementIndex < NumElements; ++ElementIndex)
	{
		const TSharedPtr<FClothCollectionItem> NewItem = MakeShared<FClothCollectionItem>();
		NewItem->AttributeValues.SetNum(HeaderData->AttributeNames.Num());

		for (int32 AttributeNameIndex = 0; AttributeNameIndex < HeaderData->AttributeNames.Num(); ++AttributeNameIndex)
		{
			const FName& AttributeName = HeaderData->AttributeNames[AttributeNameIndex];
			if (AttributeName == FClothCollectionHeaderData::ColumnZeroName)
			{
				NewItem->AttributeValues[AttributeNameIndex] = FString::FromInt(ElementIndex);
			}
			else
			{
				NewItem->AttributeValues[AttributeNameIndex] = ClothCollectionOutlinerHelpers::AttributeValueToString(*PinnedClothCollection, AttributeName, SelectedGroupName, ElementIndex);
			}
		}

		ListItems.Add(NewItem);
	}

	ListView->RequestListRefresh();
}

TSharedRef<ITableRow> SClothCollectionOutliner::GenerateRow(TSharedPtr<const FClothCollectionItem> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(SClothCollectionOutlinerRow, OwnerTable, this->HeaderData, InItem);
}


#undef LOCTEXT_NAMESPACE
