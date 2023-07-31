// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAnimCurvePicker.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "IEditableSkeleton.h"
#include "Animation/AnimationAsset.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "SListViewSelectorDropdownMenu.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SMenuOwner.h"
#include "Widgets/Layout/SBorder.h"

#define LOCTEXT_NAMESPACE "SAnimCurvePicker"

SAnimCurvePicker::~SAnimCurvePicker()
{
}

void SAnimCurvePicker::Construct(const FArguments& InArgs, const TSharedRef<IEditableSkeleton>& InEditableSkeleton)
{
	OnCurveNamePicked = InArgs._OnCurveNamePicked;
	IsCurveMarkedForExclusion = InArgs._IsCurveMarkedForExclusion;
	EditableSkeleton = InEditableSkeleton;

	SAssignNew(SearchBox, SSearchBox)
	.HintText(LOCTEXT("SearchBoxHint", "Search"))
	.OnTextChanged(this, &SAnimCurvePicker::HandleFilterTextChanged);

	SAssignNew(NameListView, SListView<TSharedPtr<FSmartName>>)
	.SelectionMode(ESelectionMode::Single)
	.ListItemsSource(&CurveNames)
	.OnSelectionChanged(this, &SAnimCurvePicker::HandleSelectionChanged)
	.OnGenerateRow(this, &SAnimCurvePicker::HandleGenerateRow);

	const float HorizontalPadding = 8.0f;
	const float VerticalPadding = 2.0f;
	const float WeightOverride = 300.0f;
	
	ChildSlot
	[
		SNew(SMenuOwner)
		[
			SNew(SListViewSelectorDropdownMenu<TSharedPtr<FSmartName>>, SearchBox, NameListView)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(HorizontalPadding, VerticalPadding)
				[
					SearchBox.ToSharedRef()
				]
				+SVerticalBox::Slot()
				.FillHeight(1.0f)
				.VAlign(VAlign_Fill)
				.Padding(HorizontalPadding, VerticalPadding)
				[
					SNew(SBox)
					.WidthOverride(WeightOverride)
					.HeightOverride(WeightOverride)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						[
							SNew(SBorder)
							.BorderImage(FAppStyle::GetBrush("Graph.StateNode.Body"))
							.BorderBackgroundColor(FAppStyle::Get().GetSlateColor("Colors.Input"))
						]
						+SOverlay::Slot()
						[
							SNew(SScrollBox)
							.Orientation(EOrientation::Orient_Vertical)
							+ SScrollBox::Slot()
							.HAlign(HAlign_Fill)
							.VAlign(VAlign_Fill)
							[
								NameListView.ToSharedRef()
							]
						]
					]
				]
			]
		]
	];

	RefreshListItems();
}

void SAnimCurvePicker::HandleSelectionChanged(TSharedPtr<FSmartName> InItem, ESelectInfo::Type InSelectionType)
{
	// When the user is navigating, do not act upon the selection change
	if (InSelectionType == ESelectInfo::OnNavigation)
	{
		return;
	}
	
	if (InItem.IsValid())
	{
		OnCurveNamePicked.ExecuteIfBound(*InItem);
	}
}

TSharedRef<ITableRow> SAnimCurvePicker::HandleGenerateRow(TSharedPtr<FSmartName> InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return 
		SNew(STableRow<TSharedPtr<FSmartName>>, InOwnerTable)
		.Padding(FMargin(8.0f, 0.0f))
		[
			SNew(SBox)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(InItem->DisplayName))
				.HighlightText_Lambda([this]() { return FText::FromString(FilterText); })
			]
		];
}

void SAnimCurvePicker::RefreshListItems()
{
	// Ensure valid skeleton exists
	const TSharedPtr<IEditableSkeleton> CurrentEditableSkeleton = EditableSkeleton.Pin();
	check(CurrentEditableSkeleton);
	const USkeleton & CurrentSkeleton = CurrentEditableSkeleton->GetSkeleton();

	// Get name container for all the curves stored by the skeleton
	const FSmartNameMapping* CurveNameMapping = CurrentSkeleton.GetSmartNameContainer(USkeleton::AnimCurveMappingName);
	CurveNameMapping->FillUidArray(CurveSmartNameUids);
	
	CurveNames.Reset();
	UniqueCurveNames.Reset();

	{
		// We use the asset registry to query all assets with the supplied skeleton, and accumulate their curve names
		const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		FARFilter Filter;
		Filter.bRecursiveClasses = true;
		Filter.ClassPaths.Add(UAnimationAsset::StaticClass()->GetClassPathName());
		Filter.TagsAndValues.Add(TEXT("Skeleton"), FAssetData(&CurrentSkeleton).GetExportTextName());

		TArray<FAssetData> FoundAssetData;
		AssetRegistryModule.Get().GetAssets(Filter, FoundAssetData);

		// Build set of unique curve smart names
		for (FAssetData& AssetData : FoundAssetData)
		{
			const FString TagValue = AssetData.GetTagValueRef<FString>(USkeleton::CurveNameTag);
			if (!TagValue.IsEmpty())
			{
				// parse notifies
				if (TagValue.ParseIntoArray(CurveNamesQueriedFromAssetRegistry, *USkeleton::CurveTagDelimiter, true) > 0)
				{
					for (const FString& CurveNameString : CurveNamesQueriedFromAssetRegistry)
					{
						FName CurveName = FName(*CurveNameString);
						FSmartName InCurveSmartName = {CurveName, CurveNameMapping->FindUID(CurveName)};
						
						if (IsCurveMarkedForExclusion.IsBound() && IsCurveMarkedForExclusion.Execute(InCurveSmartName))
						{
							continue;
						}
						
						UniqueCurveNames.Add(InCurveSmartName);
					}
				}
			}
		}
	}
	
	// Add unique curves that were not found using asset registry
	for (SmartName::UID_Type Id : CurveSmartNameUids)
	{
		FName OutCurveName;
		if (CurveNameMapping->GetName(Id, OutCurveName))
		{
			FSmartName InCurveSmartName(OutCurveName, Id);

			if (IsCurveMarkedForExclusion.IsBound() && IsCurveMarkedForExclusion.Execute(InCurveSmartName))
				continue;
			
			UniqueCurveNames.Add(InCurveSmartName);
		}
	}

	FilterAvailableCurves();
}

void SAnimCurvePicker::FilterAvailableCurves()
{
	CurveNames.Reset();
	
	// Exact filtering
	for (const FSmartName& UniqueCurveName : UniqueCurveNames)
	{
		if (FilterText.IsEmpty() || UniqueCurveName.DisplayName.ToString().Contains(FilterText))
		{
			CurveNames.Add(MakeShared<FSmartName>(UniqueCurveName));
		}
	}

	// Alphabetical sorting
	{
		struct FSmartNameSortItemSortOp
		{
			FORCEINLINE bool operator()( const TSharedPtr<FSmartName>& A, const TSharedPtr<FSmartName>& B ) const
			{
				return (A->DisplayName.Compare(B->DisplayName) < 0);
			}
		};
		CurveNames.Sort(FSmartNameSortItemSortOp());
	}

	// Rebuild list view
	NameListView->RequestListRefresh();
}

void SAnimCurvePicker::HandleFilterTextChanged(const FText& InFilterText)
{
	FilterText = InFilterText.ToString();

	FilterAvailableCurves();
}

#undef LOCTEXT_NAMESPACE