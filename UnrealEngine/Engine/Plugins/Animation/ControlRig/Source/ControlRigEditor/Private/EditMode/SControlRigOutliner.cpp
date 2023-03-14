// Copyright Epic Games, Inc. All Rights Reserved.

#include "EditMode/SControlRigOutliner.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "AssetRegistry/AssetData.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "ScopedTransaction.h"
#include "ControlRig.h"
#include "UnrealEdGlobals.h"
#include "EditMode/ControlRigEditMode.h"
#include "EditorModeManager.h"
#include "ISequencer.h"
#include "LevelSequence.h"
#include "Selection.h"
#include "Editor.h"
#include "LevelEditor.h"
#include "Widgets/Text/STextBlock.h"
#include "EditMode/ControlRigControlsProxy.h"
#include "EditMode/ControlRigEditModeSettings.h"
#include "TimerManager.h"
#include "Editor/SRigHierarchyTreeView.h"
#include "Rigs/RigHierarchyController.h"
#include "ControlRigObjectBinding.h"
#include "ControlRigEditorStyle.h"
#include "Settings/ControlRigSettings.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Views/SListPanel.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"
#include "HelperUtil.h"
#include "ScopedTransaction.h"
#include "Rigs/FKControlRig.h"

#define LOCTEXT_NAMESPACE "ControlRigOutliner"

FRigTreeDisplaySettings FMultiRigTreeDelegates::DefaultDisplaySettings;


//////////////////////////////////////////////////////////////
/// FMultiRigData
///////////////////////////////////////////////////////////

uint32 GetTypeHash(const FMultiRigData& Data)
{
	return GetTypeHash(TTuple<const UControlRig*, FRigElementKey>(Data.ControlRig.Get(), (Data.Key.IsSet() ? Data.Key.GetValue() : FRigElementKey())));
}

FText FMultiRigData::GetName() const
{
	if (Key.IsSet())
	{
		return FText::FromName(Key.GetValue().Name);
	}
	else if (ControlRig.IsValid())
	{
		FString ControlRigName = ControlRig.Get()->GetName();
		FString BoundObjectName;
		if (TSharedPtr<IControlRigObjectBinding> ObjectBinding = ControlRig.Get()->GetObjectBinding())
		{
			if (ObjectBinding->GetBoundObject())
			{
				AActor* Actor = ObjectBinding->GetBoundObject()->GetTypedOuter<AActor>();
				if (Actor)
				{
					BoundObjectName = Actor->GetActorLabel();
				}
			}
		}
		FText AreaTitle = FText::Format(LOCTEXT("ControlTitle", "{0}  ({1})"), FText::AsCultureInvariant(ControlRigName), FText::AsCultureInvariant((BoundObjectName)));
		return AreaTitle;
	}
	FName None = NAME_None;
	return FText::FromName(None);
}

FText FMultiRigData::GetDisplayName() const
{
	if (Key.IsSet())
	{
		if(ControlRig.IsValid())
		{
			if(const FRigControlElement* ControlElement = ControlRig->GetHierarchy()->Find<FRigControlElement>(Key.GetValue()))
			{
				if(!ControlElement->Settings.DisplayName.IsNone())
				{
					return FText::FromName(ControlElement->Settings.DisplayName);
				}
			}
		}
	}
	return GetName();
}

bool FMultiRigData::operator== (const FMultiRigData & Other) const
{
	if (ControlRig.IsValid() == false && Other.ControlRig.IsValid() == false)
	{
		return true;
	}
	else if (ControlRig.IsValid() == true && Other.ControlRig.IsValid() == true)
	{
		if (ControlRig.Get() == Other.ControlRig.Get())
		{
			if (Key.IsSet() == false && Other.Key.IsSet() == false)
			{
				return true;
			}
			else if (Key.IsSet() == true && Other.Key.IsSet() == true)
			{
				return Key.GetValue() == Other.Key.GetValue();
			}
		}
	}
	return false;
}

bool FMultiRigData::IsValid() const
{
	return (ControlRig.IsValid() && (Key.IsSet() == false || Key.GetValue().IsValid()));
}

URigHierarchy* FMultiRigData::GetHierarchy() const
{
	if (ControlRig.IsValid())
	{
		return ControlRig.Get()->GetHierarchy();
	}
	return nullptr;
}

//////////////////////////////////////////////////////////////
/// FMultiRigTreeElement
///////////////////////////////////////////////////////////

FMultiRigTreeElement::FMultiRigTreeElement(const FMultiRigData& InData, TWeakPtr<SMultiRigHierarchyTreeView> InTreeView, ERigTreeFilterResult InFilterResult)
{
	Data = InData;
	FilterResult = InFilterResult;

	if (InTreeView.IsValid() && Data.ControlRig.IsValid())
	{
		if (const URigHierarchy* Hierarchy = Data.GetHierarchy())
		{
			const FRigTreeDisplaySettings& Settings = InTreeView.Pin()->GetTreeDelegates().GetDisplaySettings();
			RefreshDisplaySettings(Hierarchy, Settings);
		}
	}
}

TSharedRef<ITableRow> FMultiRigTreeElement::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FMultiRigTreeElement> InRigTreeElement, TSharedPtr<SMultiRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned)
{
	return SNew(SMultiRigHierarchyItem, InOwnerTable, InRigTreeElement, InTreeView, InSettings, bPinned);

}

void FMultiRigTreeElement::RefreshDisplaySettings(const URigHierarchy* InHierarchy, const FRigTreeDisplaySettings& InSettings)
{
	const TPair<const FSlateBrush*, FSlateColor> Result = SMultiRigHierarchyItem::GetBrushForElementType(InHierarchy, Data);

	IconBrush = Result.Key;
	IconColor = Result.Value;
	if (IconColor.IsColorSpecified() && InSettings.bShowIconColors)
	{
		IconColor = FilterResult == ERigTreeFilterResult::Shown ? Result.Value : FSlateColor(Result.Value.GetSpecifiedColor() * 0.5f);
	}
	else
	{
		IconColor = FilterResult == ERigTreeFilterResult::Shown ? FSlateColor::UseForeground() : FSlateColor(FLinearColor::Gray * 0.5f);
	}
	TextColor = FilterResult == ERigTreeFilterResult::Shown ? FSlateColor::UseForeground() : FSlateColor(FLinearColor::Gray * 0.5f);

}
//////////////////////////////////////////////////////////////
/// SMultiRigHierarchyItem
///////////////////////////////////////////////////////////
void SMultiRigHierarchyItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FMultiRigTreeElement> InRigTreeElement, TSharedPtr<SMultiRigHierarchyTreeView> InTreeView, const FRigTreeDisplaySettings& InSettings, bool bPinned)
{
	WeakRigTreeElement = InRigTreeElement;
	Delegates = InTreeView->GetTreeDelegates();

	
	if (InRigTreeElement->Data.ControlRig.IsValid() == false || (InRigTreeElement->Data.Key.IsSet() &&
		InRigTreeElement->Data.Key.GetValue().IsValid() == false))
	{
		STableRow<TSharedPtr<FMultiRigTreeElement>>::Construct(
			STableRow<TSharedPtr<FMultiRigTreeElement>>::FArguments()
			.ShowSelection(false)
			.Content()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
			.FillHeight(200.f)
			[
				SNew(SSpacer)
			]
			], OwnerTable);
		return;
	}

	TSharedPtr< SInlineEditableTextBlock > InlineWidget;

	STableRow<TSharedPtr<FMultiRigTreeElement>>::Construct(
		STableRow<TSharedPtr<FMultiRigTreeElement>>::FArguments()
		.ShowWires(true)
		.Content()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		.MaxWidth(18)
		.FillWidth(1.0)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
		[


			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.OnClicked(this, &SMultiRigHierarchyItem::OnGetSelectedClicked)
			[
				SNew(SImage)
				.Image_Lambda([this]() -> const FSlateBrush*
					{
						//if no key is set then it's the control rig so we get that based upon it's state
						if (WeakRigTreeElement.Pin()->Data.Key.IsSet() == false)
						{
							if (UControlRig* ControlRig = WeakRigTreeElement.Pin()->Data.ControlRig.Get())
							{
								if (ControlRig->GetControlsVisible())
								{
									return (FAppStyle::GetBrush("Level.VisibleIcon16x"));
								}
								else
								{
									return  (FAppStyle::GetBrush("Level.NotVisibleIcon16x"));
								}
							}

						}
						return WeakRigTreeElement.Pin()->IconBrush;
					})
					.ColorAndOpacity_Lambda([this]()
					{
						return WeakRigTreeElement.Pin()->IconColor;
					})
			]
		]
		+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineWidget, SInlineEditableTextBlock)
				.Text(this, &SMultiRigHierarchyItem::GetDisplayName)
			.MultiLine(false)
			.ColorAndOpacity_Lambda([this]()
				{
					if (WeakRigTreeElement.IsValid())
					{
						return WeakRigTreeElement.Pin()->TextColor;
					}
					return FSlateColor::UseForeground();
				})
			]
			], OwnerTable);

}

FReply SMultiRigHierarchyItem::OnGetSelectedClicked()
{
	if (FMultiRigTreeElement* Element = WeakRigTreeElement.Pin().Get())
	{
		if (Element->Data.Key.IsSet() == false)
		{
			if (UControlRig* ControlRig = Element->Data.ControlRig.Get())
			{
				FScopedTransaction ScopedTransaction(LOCTEXT("ToggleControlsVisibility", "Toggle Controls Visibility"), !GIsTransacting);
				ControlRig->Modify();
				ControlRig->ToggleControlsVisible();
				return FReply::Handled();
			}
		}
	}
	return FReply::Unhandled();  //should flow to selection instead
}

FText SMultiRigHierarchyItem::GetName() const
{
	return (WeakRigTreeElement.Pin()->Data.GetName());
}

FText SMultiRigHierarchyItem::GetDisplayName() const
{
	return (WeakRigTreeElement.Pin()->Data.GetDisplayName());
}

TPair<const FSlateBrush*, FSlateColor> SMultiRigHierarchyItem::GetBrushForElementType(const URigHierarchy* InHierarchy, const FMultiRigData& InData)
{
	const FSlateBrush* Brush = nullptr;
	FSlateColor Color = FSlateColor::UseForeground();
	if (InData.Key.IsSet())
	{
		const FRigElementKey Key = InData.Key.GetValue();
		return SRigHierarchyItem::GetBrushForElementType(InHierarchy, Key);
	}
	else
	{
		if (UControlRig* ControlRig = InData.ControlRig.Get())
		{
			if (ControlRig->GetControlsVisible())
			{
				Brush = FAppStyle::GetBrush("Level.VisibleIcon16x");
			}
			else
			{
				Brush = FAppStyle::GetBrush("Level.NotVisibleIcon16x");
			}
		}
		else
		{
			Brush = FAppStyle::GetBrush("Level.NotVisibleIcon16x");
		}
	}

	return TPair<const FSlateBrush*, FSlateColor>(Brush, Color);
}

//////////////////////////////////////////////////////////////
/// SRigHierarchyTreeView
///////////////////////////////////////////////////////////

void SMultiRigHierarchyTreeView::Construct(const FArguments& InArgs)
{
	Delegates = InArgs._RigTreeDelegates;

	STreeView<TSharedPtr<FMultiRigTreeElement>>::FArguments SuperArgs;
	SuperArgs.TreeItemsSource(&RootElements);
	SuperArgs.SelectionMode(ESelectionMode::Multi);
	SuperArgs.OnGenerateRow(this, &SMultiRigHierarchyTreeView::MakeTableRowWidget, false);
	SuperArgs.OnGetChildren(this, &SMultiRigHierarchyTreeView::HandleGetChildrenForTree);
	SuperArgs.OnSelectionChanged(FOnMultiRigTreeSelectionChanged::CreateRaw(&Delegates, &FMultiRigTreeDelegates::HandleSelectionChanged));
	SuperArgs.OnContextMenuOpening(Delegates.OnContextMenuOpening);
	SuperArgs.OnMouseButtonClick(Delegates.OnMouseButtonClick);
	SuperArgs.OnMouseButtonDoubleClick(Delegates.OnMouseButtonDoubleClick);
	SuperArgs.OnSetExpansionRecursive(Delegates.OnSetExpansionRecursive);
	SuperArgs.HighlightParentNodesForSelection(true);
	SuperArgs.ItemHeight(24);
	SuperArgs.AllowInvisibleItemSelection(true);  //without this we deselect everything when we filter or we collapse

	SuperArgs.ShouldStackHierarchyHeaders_Lambda([]() -> bool {
		return UControlRigEditorSettings::Get()->bShowStackedHierarchy;
		});
	SuperArgs.OnGeneratePinnedRow(this, &SMultiRigHierarchyTreeView::MakeTableRowWidget, true);
	SuperArgs.MaxPinnedItems_Lambda([]() -> int32
		{
			return FMath::Max<int32>(1, UControlRigEditorSettings::Get()->MaxStackSize);
		});

	STreeView<TSharedPtr<FMultiRigTreeElement>>::Construct(SuperArgs);
}

TSharedPtr<FMultiRigTreeElement> SMultiRigHierarchyTreeView::FindElement(const FMultiRigData& InElementData, TSharedPtr<FMultiRigTreeElement> CurrentItem)
{
	if (CurrentItem->Data == InElementData)
	{
		return CurrentItem;
	}

	for (int32 ChildIndex = 0; ChildIndex < CurrentItem->Children.Num(); ++ChildIndex)
	{
		TSharedPtr<FMultiRigTreeElement> Found = FindElement(InElementData, CurrentItem->Children[ChildIndex]);
		if (Found.IsValid())
		{
			return Found;
		}
	}

	return TSharedPtr<FMultiRigTreeElement>();
}

bool SMultiRigHierarchyTreeView::AddElement(const FMultiRigData& InData, const FMultiRigData& InParentData)
{
	if (ElementMap.Contains(InData))
	{
		return false;
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();

	const FString FilteredString = Settings.FilterText.ToString();
	if (FilteredString.IsEmpty() || !InData.IsValid())
	{
		TSharedPtr<FMultiRigTreeElement> NewItem = MakeShared<FMultiRigTreeElement>(InData, SharedThis(this), ERigTreeFilterResult::Shown);

		if (InData.IsValid())
		{
			ElementMap.Add(InData, NewItem);
			if (InParentData.IsValid())
			{
				ParentMap.Add(InData, InParentData);
				TSharedPtr<FMultiRigTreeElement>* FoundItem = ElementMap.Find(InParentData);
				check(FoundItem);
				FoundItem->Get()->Children.Add(NewItem);
			}
			else
			{
				RootElements.Add(NewItem);
			}
		}
		else
		{
			RootElements.Add(NewItem);
		}
	}
	else
	{
		FString FilteredStringUnderScores = FilteredString.Replace(TEXT(" "), TEXT("_"));
		if (InData.GetName().ToString().Contains(FilteredString) || InData.GetName().ToString().Contains(FilteredStringUnderScores))
		{
			TSharedPtr<FMultiRigTreeElement> NewItem = MakeShared<FMultiRigTreeElement>(InData, SharedThis(this), ERigTreeFilterResult::Shown);
			ElementMap.Add(InData, NewItem);
			RootElements.Add(NewItem);

			if (!Settings.bFlattenHierarchyOnFilter && !Settings.bHideParentsOnFilter)
			{
				if (const URigHierarchy* Hierarchy = InData.GetHierarchy())
				{
					if (InData.Key.IsSet())
					{
						TSharedPtr<FMultiRigTreeElement> ChildItem = NewItem;
						FRigElementKey ParentKey = Hierarchy->GetFirstParent(InData.Key.GetValue());
						while (ParentKey.IsValid())
						{
							FMultiRigData ParentData(InData.ControlRig.Get(), ParentKey);
							if (!ElementMap.Contains(ParentData))
							{
								TSharedPtr<FMultiRigTreeElement> ParentItem = MakeShared<FMultiRigTreeElement>(ParentData, SharedThis(this), ERigTreeFilterResult::ShownDescendant);
								ElementMap.Add(ParentData, ParentItem);
								RootElements.Add(ParentItem);

								ReparentElement(ChildItem->Data, ParentData);

								ChildItem = ParentItem;
								ParentKey = Hierarchy->GetFirstParent(ParentKey);
							}
							else
							{
								ReparentElement(ChildItem->Data, ParentData);
								break;
							}
						}
					}
				}
			}
		}
	}

	return true;
}

bool SMultiRigHierarchyTreeView::AddElement(UControlRig* InControlRig, const FRigBaseElement* InElement)
{
	check(InControlRig);
	check(InElement);
	
	FMultiRigData Data(InControlRig, InElement->GetKey());

	if (ElementMap.Contains(Data))
	{
		return false;
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();

	auto IsElementShown = [Settings](const FRigBaseElement* InElement) -> bool
	{
		switch (InElement->GetType())
		{
			case ERigElementType::Bone:
			{
				if (!Settings.bShowBones)
				{
					return false;
				}

				const FRigBoneElement* BoneElement = CastChecked<FRigBoneElement>(InElement);
				if (!Settings.bShowImportedBones && BoneElement->BoneType == ERigBoneType::Imported)
				{
					return false;
				}
				break;
			}
			case ERigElementType::Null:
			{
				if (!Settings.bShowNulls)
				{
					return false;
				}
				break;
			}
			case ERigElementType::Control:
			{
				const FRigControlElement* ControlElement = CastChecked<FRigControlElement>(InElement);
				if (!Settings.bShowControls || ControlElement->Settings.AnimationType == ERigControlAnimationType::VisualCue)
				{
					return false;
				}
				break;
			}
			case ERigElementType::RigidBody:
			{
				if (!Settings.bShowRigidBodies)
				{
					return false;
				}
				break;
			}
			case ERigElementType::Reference:
			{
				if (!Settings.bShowReferences)
				{
					return false;
				}
				break;
			}
			case ERigElementType::Curve:
			{
				return false;
			}
			default:
			{
				break;
			}
		}
		return true;
	};

	if(!IsElementShown(InElement))
	{
		return false;
	}

	FMultiRigData ParentData;
	ParentData.ControlRig = InControlRig;

	if (!AddElement(Data,ParentData))
	{
		return false;
	}

	UFKControlRig* FKControlRig = Cast<UFKControlRig>(InControlRig);

	if (ElementMap.Contains(Data))
	{
		if (const URigHierarchy* Hierarchy = InControlRig->GetHierarchy())
		{
			FRigElementKey ParentKey = Hierarchy->GetFirstParent(InElement->GetKey());

			TArray<FRigElementWeight> ParentWeights = Hierarchy->GetParentWeightArray(InElement->GetKey());
			if (ParentWeights.Num() > 0)
			{
				TArray<FRigElementKey> ParentKeys = Hierarchy->GetParents(InElement->GetKey());
				check(ParentKeys.Num() == ParentWeights.Num());
				for (int32 ParentIndex = 0; ParentIndex < ParentKeys.Num(); ParentIndex++)
				{
					if (ParentWeights[ParentIndex].IsAlmostZero())
					{
						continue;
					}
					ParentKey = ParentKeys[ParentIndex];
					break;
				}
			}

			if (ParentKey.IsValid())
			{
				if(FKControlRig)
				{
					if(const FRigControlElement* ControlElement = Cast<FRigControlElement>(InElement))
					{
						if(ControlElement->Settings.AnimationType == ERigControlAnimationType::AnimationControl)
						{
							const FRigElementKey& ElementKey = InElement->GetKey();
							const FName BoneName = FKControlRig->GetControlTargetName(ElementKey.Name, ElementKey.Type);
							const FRigElementKey ParentBoneKey = Hierarchy->GetFirstParent(FRigElementKey(BoneName, ERigElementType::Bone));
							if(ParentBoneKey.IsValid())
							{
								ParentKey = FRigElementKey(FKControlRig->GetControlName(ParentBoneKey.Name, ElementKey.Type), ElementKey.Type);
							}
						}
					}
				}

				if (const FRigBaseElement* ParentElement = Hierarchy->Find(ParentKey))
				{
					if(ParentElement != nullptr)
					{
						AddElement(InControlRig,ParentElement);

						FMultiRigData NewParentData(InControlRig, ParentKey);

						if (ElementMap.Contains(NewParentData))
						{
							ReparentElement(Data, NewParentData);
						}
					}
				}
			}
		}
	}

	return true;
}


bool SMultiRigHierarchyTreeView::ReparentElement(const FMultiRigData& InData, const FMultiRigData& InParentData)
{
	if (!InData.IsValid() || InData == InParentData)
	{
		return false;
	}

	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();

	TSharedPtr<FMultiRigTreeElement>* FoundItem = ElementMap.Find(InData);
	if (FoundItem == nullptr)
	{
		return false;
	}

	if (!Settings.FilterText.IsEmpty() && Settings.bFlattenHierarchyOnFilter)
	{
		return false;
	}

	if (const FMultiRigData* ExistingParentKey = ParentMap.Find(InData))
	{
		if (*ExistingParentKey == InParentData)
		{
			return false;
		}

		if (TSharedPtr<FMultiRigTreeElement>* ExistingParent = ElementMap.Find(*ExistingParentKey))
		{
			(*ExistingParent)->Children.Remove(*FoundItem);
		}

		ParentMap.Remove(InData);
	}
	else
	{
		if (!InParentData.IsValid())
		{
			return false;
		}

		RootElements.Remove(*FoundItem);
	}

	if (InParentData.IsValid())
	{
		ParentMap.Add(InData, InParentData);

		TSharedPtr<FMultiRigTreeElement>* FoundParent = ElementMap.Find(InParentData);
		check(FoundParent);
		FoundParent->Get()->Children.Add(*FoundItem);
	}
	else
	{
		RootElements.Add(*FoundItem);
	}

	return true;
}

bool SMultiRigHierarchyTreeView::RemoveElement(const FMultiRigData& InData)
{
	TSharedPtr<FMultiRigTreeElement>* FoundItem = ElementMap.Find(InData);
	if (FoundItem == nullptr)
	{
		return false;
	}

	FMultiRigData EmptyParent(nullptr, FRigElementKey());
	ReparentElement(InData, EmptyParent);

	RootElements.Remove(*FoundItem);
	return ElementMap.Remove(InData) > 0;
}

void SMultiRigHierarchyTreeView::RefreshTreeView(bool bRebuildContent)
{
	TMap<FMultiRigData, bool> ExpansionState;

	if (bRebuildContent)
	{
		for (TPair<FMultiRigData, TSharedPtr<FMultiRigTreeElement>> Pair : ElementMap)
		{
			ExpansionState.FindOrAdd(Pair.Key) = IsItemExpanded(Pair.Value);
		}

		// internally save expansion states before rebuilding the tree, so the states can be restored later
		SaveAndClearSparseItemInfos();

		RootElements.Reset();
		ElementMap.Reset();
		ParentMap.Reset();
	}

	if (bRebuildContent)
	{
		for (const TWeakObjectPtr<UControlRig>& ControlRigPtr : ControlRigs)
		{
			if (UControlRig* ControlRig  = ControlRigPtr.Get())
			{
				FMultiRigData Empty(nullptr, FRigElementKey());
				FMultiRigData CRData;
				CRData.ControlRig = ControlRig; //leave key unset so it's valid

				AddElement(CRData, Empty);
				if (const URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
				{
					Hierarchy->Traverse([&](FRigBaseElement* Element, bool& bContinue)
						{
							AddElement(ControlRig,Element);
							bContinue = true;
						});

					// expand all elements upon the initial construction of the tree
					if (ExpansionState.Num() == 0)
					{
						for (TSharedPtr<FMultiRigTreeElement> RootElement : RootElements)
						{
							SetExpansionRecursive(RootElement, false, true);
						}
					}
					else if (ExpansionState.Num() < ElementMap.Num())
					{
						for (const TPair<FMultiRigData, TSharedPtr<FMultiRigTreeElement>>& Element : ElementMap)
						{
							if (!ExpansionState.Contains(Element.Key))
							{
								SetItemExpansion(Element.Value, true);
							}
						}
					}

					for (const auto& Pair : ElementMap)
					{
						RestoreSparseItemInfos(Pair.Value);
					}

				}
			}
		}
	}
	else
	{
		if (RootElements.Num() > 0)
		{
			// elements may be added at the end of the list after a spacer element
			// we need to remove the spacer element and re-add it at the end
			RootElements.RemoveAll([](TSharedPtr<FMultiRigTreeElement> InElement)
				{
					return (InElement.Get()->Data.ControlRig == nullptr && InElement.Get()->Data.Key == FRigElementKey());
				});
		}
	}

	RequestTreeRefresh();
	{
		ClearSelection();

		for (const TWeakObjectPtr<UControlRig>& ControlRigPtr : ControlRigs)
		{
			if (UControlRig* ControlRig = ControlRigPtr.Get())
			{
				if (const URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
				{

					TArray<FRigElementKey> Selection = Hierarchy->GetSelectedKeys();
					for (const FRigElementKey& Key : Selection)
					{
						for (int32 RootIndex = 0; RootIndex < RootElements.Num(); ++RootIndex)
						{
							FMultiRigData Data(ControlRig, Key);
							TSharedPtr<FMultiRigTreeElement> Found = FindElement(Data, RootElements[RootIndex]);
							if (Found.IsValid())
							{
								SetItemSelection(Found, true, ESelectInfo::OnNavigation);
							}
						}
					}
				}
			}
		}
	}
}

void SMultiRigHierarchyTreeView::SetExpansionRecursive(TSharedPtr<FMultiRigTreeElement> InElement, bool bTowardsParent,
	bool bShouldBeExpanded)
{
	SetItemExpansion(InElement, bShouldBeExpanded);

	if (bTowardsParent)
	{
		if (const FMultiRigData* ParentKey = ParentMap.Find(InElement->Data))
		{
			if (TSharedPtr<FMultiRigTreeElement>* ParentItem = ElementMap.Find(*ParentKey))
			{
				SetExpansionRecursive(*ParentItem, bTowardsParent, bShouldBeExpanded);
			}
		}
	}
	else
	{
		for (int32 ChildIndex = 0; ChildIndex < InElement->Children.Num(); ++ChildIndex)
		{
			SetExpansionRecursive(InElement->Children[ChildIndex], bTowardsParent, bShouldBeExpanded);
		}
	}
}

TSharedRef<ITableRow> SMultiRigHierarchyTreeView::MakeTableRowWidget(TSharedPtr<FMultiRigTreeElement> InItem,
	const TSharedRef<STableViewBase>& OwnerTable, bool bPinned)
{
	const FRigTreeDisplaySettings& Settings = Delegates.GetDisplaySettings();
	return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), SharedThis(this), Settings, bPinned);
}

void SMultiRigHierarchyTreeView::HandleGetChildrenForTree(TSharedPtr<FMultiRigTreeElement> InItem,
	TArray<TSharedPtr<FMultiRigTreeElement>>& OutChildren)
{
	OutChildren = InItem.Get()->Children;
}

TArray<FMultiRigData> SMultiRigHierarchyTreeView::GetSelectedData() const
{
	TArray<FMultiRigData> Keys;
	TArray<TSharedPtr<FMultiRigTreeElement>> SelectedElements = GetSelectedItems();
	for (const TSharedPtr<FMultiRigTreeElement>& SelectedElement : SelectedElements)
	{
		Keys.Add(SelectedElement->Data);
	}
	return Keys;
}

TSharedPtr<FMultiRigTreeElement> SMultiRigHierarchyTreeView::FindItemAtPosition(FVector2D InScreenSpacePosition) const
{
	if (ItemsPanel.IsValid() && ItemsSource != nullptr)
	{
		const FGeometry MyGeometry = ItemsPanel->GetCachedGeometry();
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		ItemsPanel->ArrangeChildren(MyGeometry, ArrangedChildren, true);

		const int32 Index = ItemsPanel->FindChildUnderPosition(ArrangedChildren, InScreenSpacePosition);
		if (ItemsSource->IsValidIndex(Index))
		{
			return ItemsSource->operator[](Index);
		}
	}
	return TSharedPtr<FMultiRigTreeElement>();
}

TArray<URigHierarchy*> SMultiRigHierarchyTreeView::GetHierarchy() const
{
	TArray<URigHierarchy*> RigHierarchy;
	for (const TWeakObjectPtr<UControlRig>& ControlRig : ControlRigs)
	{
		if (ControlRig.IsValid())
		{
			RigHierarchy.Add(ControlRig.Get()->GetHierarchy());
		}
	}
	return RigHierarchy;
}

void SMultiRigHierarchyTreeView::SetControlRigs(TArrayView < TWeakObjectPtr<UControlRig>>& InControlRigs)
{
	ControlRigs.SetNum(0);
	for (TWeakObjectPtr<UControlRig>& ControlRig : InControlRigs)
	{
		if (ControlRig.IsValid())
		{
			ControlRigs.Add(ControlRig.Get());
		}
	}
	RefreshTreeView(true);
}

//////////////////////////////////////////////////////////////
/// SSearchableMultiRigHierarchyTreeView
///////////////////////////////////////////////////////////

void SSearchableMultiRigHierarchyTreeView::Construct(const FArguments& InArgs)
{
	FMultiRigTreeDelegates TreeDelegates = InArgs._RigTreeDelegates;
	SuperGetRigTreeDisplaySettings = TreeDelegates.OnGetDisplaySettings;

	TreeDelegates.OnGetDisplaySettings.BindSP(this, &SSearchableMultiRigHierarchyTreeView::GetDisplaySettings);

	ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				.Padding(0.0f)
				[
					SNew(SSearchBox)
					.InitialText(InArgs._InitialFilterText)
				.OnTextChanged(this, &SSearchableMultiRigHierarchyTreeView::OnFilterTextChanged)
				]

			+ SVerticalBox::Slot()
				.AutoHeight()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Fill)
				.Padding(0.0f, 0.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SBorder)
						.Padding(2.0f)
						.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
						[
							SAssignNew(TreeView, SMultiRigHierarchyTreeView)
							.RigTreeDelegates(TreeDelegates)
						]
					]
				]
		];
}

const FRigTreeDisplaySettings& SSearchableMultiRigHierarchyTreeView::GetDisplaySettings()
{
	if (SuperGetRigTreeDisplaySettings.IsBound())
	{
		Settings = SuperGetRigTreeDisplaySettings.Execute();
	}
	Settings.FilterText = FilterText;
	return Settings;
}

void SSearchableMultiRigHierarchyTreeView::OnFilterTextChanged(const FText& SearchText)
{
	FilterText = SearchText;
	GetTreeView()->RefreshTreeView();
}

//////////////////////////////////////////////////////////////
/// SControlRigOutliner
///////////////////////////////////////////////////////////

void SControlRigOutliner::Construct(const FArguments& InArgs, FControlRigEditMode& InEditMode)
{
	bIsChangingRigHierarchy = false;

	DisplaySettings.bShowBones = false;
	DisplaySettings.bShowControls = true;
	DisplaySettings.bShowNulls = false;
	DisplaySettings.bShowReferences = false;
	DisplaySettings.bShowRigidBodies = false;
	DisplaySettings.bHideParentsOnFilter = true;
	DisplaySettings.bFlattenHierarchyOnFilter = true;

	FMultiRigTreeDelegates RigTreeDelegates;
	RigTreeDelegates.OnGetDisplaySettings = FOnGetRigTreeDisplaySettings::CreateSP(this, &SControlRigOutliner::GetDisplaySettings);
	RigTreeDelegates.OnSelectionChanged = FOnMultiRigTreeSelectionChanged::CreateSP(this, &SControlRigOutliner::HandleSelectionChanged);

	
	ChildSlot
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(PickerExpander, SExpandableArea)
					.InitiallyCollapsed(false)
					//.AreaTitle(AreaTitle)
					//.AreaTitleFont(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.BorderBackgroundColor(FLinearColor(.6f, .6f, .6f))
					.BodyContent()
					[
						SAssignNew(HierarchyTreeView, SSearchableMultiRigHierarchyTreeView)
						.RigTreeDelegates(RigTreeDelegates)
					]
					
				]
		
			]
		];
	SetEditMode(InEditMode);
}

void SControlRigOutliner::OnObjectsReplaced(const TMap<UObject*, UObject*>& OldToNewInstanceMap)
{
	//if there's a control rig recreate the tree, controls may have changed
	bool bNewControlRig = false;
	for (const TPair<UObject*, UObject*>& Pair : OldToNewInstanceMap)
	{
		if(Pair.Key && Pair.Value)
		{
			if (Pair.Key->IsA<UControlRig>() && Pair.Value->IsA<UControlRig>())
			{
				bNewControlRig = false;
				break;
			}
		}
	}
	if (bNewControlRig)
	{
		HierarchyTreeView->GetTreeView()->RefreshTreeView(true);
	}
}

SControlRigOutliner::SControlRigOutliner()
{
	FCoreUObjectDelegates::OnObjectsReplaced.AddRaw(this, &SControlRigOutliner::OnObjectsReplaced);
}

SControlRigOutliner::~SControlRigOutliner()
{
	FCoreUObjectDelegates::OnObjectsReplaced.RemoveAll(this);
}

void SControlRigOutliner::HandleControlSelected(UControlRig* Subject, FRigControlElement* ControlElement, bool bSelected)
{
	FControlRigBaseDockableView::HandleControlSelected(Subject, ControlElement, bSelected);
	const FRigElementKey Key = ControlElement->GetKey();
	FMultiRigData Data(Subject, Key);
	for (int32 RootIndex = 0; RootIndex < HierarchyTreeView->GetTreeView()->GetRootElements().Num(); ++RootIndex)
	{
		TSharedPtr<FMultiRigTreeElement> Found = HierarchyTreeView->GetTreeView()->FindElement(Data, HierarchyTreeView->GetTreeView()->GetRootElements()[RootIndex]);
		if (Found.IsValid())
		{
			TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);

			HierarchyTreeView->GetTreeView()->SetItemSelection(Found, bSelected, ESelectInfo::OnNavigation);

			TArray<TSharedPtr<FMultiRigTreeElement>> SelectedItems = HierarchyTreeView->GetTreeView()->GetSelectedItems();
			for (TSharedPtr<FMultiRigTreeElement> SelectedItem : SelectedItems)
			{
				HierarchyTreeView->GetTreeView()->SetExpansionRecursive(SelectedItem, false, true);
			}

			if (SelectedItems.Num() > 0)
			{
				HierarchyTreeView->GetTreeView()->RequestScrollIntoView(SelectedItems.Last());
			}
		}
	}
}


void SControlRigOutliner::HandleSelectionChanged(TSharedPtr<FMultiRigTreeElement> Selection, ESelectInfo::Type SelectInfo)
{
	if (bIsChangingRigHierarchy)
	{
		return;
	}
	const TArray<FMultiRigData> NewSelection = HierarchyTreeView->GetTreeView()->GetSelectedData();
	TMap<UControlRig*, TArray<FRigElementKey>> SelectedRigAndKeys;
	for (const FMultiRigData& Data : NewSelection)
	{
		if (Data.ControlRig.IsValid() && (Data.Key.IsSet() && Data.Key.GetValue() != FRigElementKey()))
		{
			if (SelectedRigAndKeys.Find(Data.ControlRig.Get()) == nullptr)
			{
				SelectedRigAndKeys.Add(Data.ControlRig.Get());
			}
			SelectedRigAndKeys[Data.ControlRig.Get()].Add(Data.Key.GetValue());
		}
	}
	TGuardValue<bool> GuardRigHierarchyChanges(bIsChangingRigHierarchy, true);
	FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(ModeTools->GetActiveMode(FControlRigEditMode::ModeName));
	bool bEndTransaction = false;
	if (GEditor && EditMode && EditMode->IsInLevelEditor())
	{
		GEditor->BeginTransaction(LOCTEXT("SelectControl", "Select Control"));
		bEndTransaction = true;
	}
	//due to how Sequencer Tree View will redo selection on next tick if we aren't keeping or toggling selection we need to clear it out
	if (FSlateApplication::Get().GetModifierKeys().IsShiftDown() == false || FSlateApplication::Get().GetModifierKeys().IsControlDown() == false)
	{
		if (EditMode)
		{		
			TMap<UControlRig*, TArray<FRigElementKey>> SelectedControls;
			EditMode->GetAllSelectedControls(SelectedControls);
			for (TPair<UControlRig*, TArray<FRigElementKey>>& CurrentSelection : SelectedControls)
			{
				if (CurrentSelection.Key)
				{
					CurrentSelection.Key->ClearControlSelection();
				}
			}
		}
	}

	for(TPair<UControlRig*, TArray<FRigElementKey>>& RigAndKeys: SelectedRigAndKeys)
	{ 
		const URigHierarchy* Hierarchy = RigAndKeys.Key->GetHierarchy();
		if (Hierarchy)
		{
			URigHierarchyController* Controller = ((URigHierarchy*)Hierarchy)->GetController(true);
			check(Controller);
			if (!Controller->SetSelection(RigAndKeys.Value))
			{
				if (bEndTransaction)
				{
					GEditor->EndTransaction();
				}
				return;
			}
		}
	}
	if (bEndTransaction)
	{
		GEditor->EndTransaction();
	}
}

void SControlRigOutliner::SetEditMode(FControlRigEditMode& InEditMode)
{
	FControlRigBaseDockableView::SetEditMode(InEditMode);
	ModeTools = InEditMode.GetModeManager();
	if (FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(ModeTools->GetActiveMode(FControlRigEditMode::ModeName)))
	{
		TArrayView<TWeakObjectPtr<UControlRig>> ControlRigs = EditMode->GetControlRigs();
		HierarchyTreeView->GetTreeView()->SetControlRigs(ControlRigs); //will refresh tree
	}
}

void SControlRigOutliner::HandleControlAdded(UControlRig* ControlRig, bool bIsAdded)
{
	FControlRigBaseDockableView::HandleControlAdded(ControlRig, bIsAdded);
	if (FControlRigEditMode* EditMode = static_cast<FControlRigEditMode*>(ModeTools->GetActiveMode(FControlRigEditMode::ModeName)))
	{
		TArrayView<TWeakObjectPtr<UControlRig>> ControlRigs = EditMode->GetControlRigs();
		HierarchyTreeView->GetTreeView()->SetControlRigs(ControlRigs); //will refresh tree
	}
}


#undef LOCTEXT_NAMESPACE
