// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SModularRigTreeView.h"
#include "Styling/AppStyle.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"
#include "HelperUtil.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ControlRig.h"
#include "ControlRigEditorStyle.h"
#include "ModularRig.h"
#include "ModularRigRuleManager.h"
#include "SRigHierarchyTreeView.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Editor/SModularRigModel.h"
#include "Fonts/FontMeasure.h"
#include "Settings/ControlRigSettings.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Rigs/AdditiveControlRig.h"
#include "Rigs/RigHierarchyController.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SButton.h"
#include "ScopedTransaction.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "SModularRigTreeView"

TMap<FSoftObjectPath, TSharedPtr<FSlateBrush>> FModularRigTreeElement::IconPathToBrush;

//////////////////////////////////////////////////////////////
/// FModularRigTreeElement
///////////////////////////////////////////////////////////
FModularRigTreeElement::FModularRigTreeElement(const FString& InKey, TWeakPtr<SModularRigTreeView> InTreeView, bool bInIsPrimary)
{
	Key = InKey;
	bIsPrimary = bInIsPrimary;

	FString ShortNameStr = Key;
	(void)URigHierarchy::SplitNameSpace(ShortNameStr, &ModulePath, &ShortNameStr);
	if (bIsPrimary)
	{
		ModulePath = Key;
		if(const UModularRig* ModularRig = InTreeView.Pin()->GetRigTreeDelegates().GetModularRig())
		{
			if (const FRigModuleInstance* Module = ModularRig->FindModule(ModulePath))
			{
				if (const UControlRig* Rig = Module->GetRig())
				{
					if (const FRigModuleConnector* PrimaryConnector = Rig->GetRigModuleSettings().FindPrimaryConnector())
					{
						ConnectorName = PrimaryConnector->Name;
					}
				}
			}
		}
	}
	else
	{
		ConnectorName = ShortNameStr;
	}
	
	ShortName = *ShortNameStr;
	
	if(InTreeView.IsValid())
	{
		if(const UModularRig* ModularRig = InTreeView.Pin()->GetRigTreeDelegates().GetModularRig())
		{
			RefreshDisplaySettings(ModularRig);
		}
	}
}

void FModularRigTreeElement::RefreshDisplaySettings(const UModularRig* InModularRig)
{
	const TPair<const FSlateBrush*, FSlateColor> Result = GetBrushAndColor(InModularRig);

	IconBrush = Result.Key;
	IconColor = Result.Value;
	TextColor = FSlateColor::UseForeground();
}

TSharedRef<ITableRow> FModularRigTreeElement::MakeTreeRowWidget(const TSharedRef<STableViewBase>& InOwnerTable, TSharedRef<FModularRigTreeElement> InRigTreeElement, TSharedPtr<SModularRigTreeView> InTreeView, bool bPinned)
{
	return SNew(SModularRigModelItem, InOwnerTable, InRigTreeElement, InTreeView, bPinned);
}

void FModularRigTreeElement::RequestRename()
{
	OnRenameRequested.ExecuteIfBound();
}

//////////////////////////////////////////////////////////////
/// SModularRigModelItem
///////////////////////////////////////////////////////////
void SModularRigModelItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& OwnerTable, TSharedRef<FModularRigTreeElement> InRigTreeElement, TSharedPtr<SModularRigTreeView> InTreeView, bool bPinned)
{
	WeakRigTreeElement = InRigTreeElement;
	Delegates = InTreeView->GetRigTreeDelegates();

	if (InRigTreeElement->Key.IsEmpty())
	{
		SMultiColumnTableRow<TSharedPtr<FModularRigTreeElement>>::Construct(
			SMultiColumnTableRow<TSharedPtr<FModularRigTreeElement>>::FArguments()
			.ShowSelection(false)
			.OnCanAcceptDrop(Delegates.OnCanAcceptDrop)
			.OnAcceptDrop(Delegates.OnAcceptDrop)
			, OwnerTable);
		return;
	}

	const FString& ModulePath = InRigTreeElement->ModulePath;
	FString ConnectorPath = FString::Printf(TEXT("%s:%s"), *ModulePath, *InRigTreeElement->ConnectorName);
	ConnectorKey = FRigElementKey(*ConnectorPath, ERigElementType::Connector);

	SMultiColumnTableRow<TSharedPtr<FModularRigTreeElement>>::Construct(
		SMultiColumnTableRow<TSharedPtr<FModularRigTreeElement>>::FArguments()
		.OnDragDetected(Delegates.OnDragDetected)
		.OnCanAcceptDrop(Delegates.OnCanAcceptDrop)
		.OnAcceptDrop(Delegates.OnAcceptDrop)
		.ShowWires(true), OwnerTable);
}


void SModularRigModelItem::PopulateConnectorTargetList(const FRigElementKey InConnectorKey)
{
	if (!WeakRigTreeElement.IsValid())
	{
		return;
	}

	if (const UModularRig* ModularRig = Delegates.GetModularRig())
	{
		if (URigHierarchy* Hierarchy = ModularRig->GetHierarchy())
		{
			if (Cast<FRigConnectorElement>(Hierarchy->Find(InConnectorKey)))
			{
				ConnectorComboBox->GetTreeView()->RefreshTreeView(true);
			}
		}
	}
}

void SModularRigModelItem::PopulateConnectorCurrentTarget(TSharedPtr<SVerticalBox> InListBox, const FRigElementKey& InConnectorKey, const FRigElementKey& InTargetKey, const FSlateBrush* InBrush, const FSlateColor& InColor, const FText& InTitle)
{
	static const FSlateBrush* RoundedBoxBrush = FControlRigEditorStyle::Get().GetBrush(TEXT("ControlRig.SpacePicker.RoundedRect"));

	TAttribute<FSlateColor> TextColor = TAttribute<FSlateColor>::CreateLambda([this, InConnectorKey, InTargetKey]() -> FSlateColor
	{
		if(const UModularRig* ModularRig = Delegates.GetModularRig())
		{
			if(const URigHierarchy* Hierarchy = ModularRig->GetHierarchy())
			{
				if(!Hierarchy->Contains(InTargetKey))
				{
					if(!InTargetKey.IsValid())
					{
						if(const FRigConnectorElement* Connector = Hierarchy->Find<FRigConnectorElement>(InConnectorKey))
						{
							if(Connector->IsOptional())
							{
								return FSlateColor::UseForeground();
							}
						}
					}
					return FLinearColor::Red;
				}
			}
		}
		return FSlateColor::UseForeground();
	});
	
	TSharedPtr<SHorizontalBox> RowBox, ButtonBox;
	InListBox->AddSlot()
	.AutoHeight()
	.VAlign(VAlign_Top)
	.HAlign(HAlign_Left)
	.Padding(0.0, 0.0, 4.0, 0.0)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.Padding(0, 0, 0, 0)
		[
			SNew( SButton )
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ContentPadding(FMargin(0.0))
			.OnClicked_Lambda([this, InConnectorKey, InTargetKey]()
			{
				//Controller->ConnectConnectorToElement(InConnectorKey, InTargetKey, true, Info.GetModularRig()->GetModularRigSettings().bAutoResolve);
				PopulateConnectorTargetList(InConnectorKey);
				return FReply::Handled();
			})
			[
				SAssignNew(RowBox, SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.Padding(0)
				[
					SNew(SBorder)
					.Padding(FMargin(2.0, 2.0, 5.0, 2.0))
					.BorderImage(RoundedBoxBrush)
					.BorderBackgroundColor(FSlateColor(FLinearColor::Transparent))
					.Content()
					[
						SAssignNew(ButtonBox, SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
						[
							SNew(SImage)
							.Image(InBrush)
							.ColorAndOpacity(InColor)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Left)
						.Padding(0)
						[
							SNew( STextBlock )
							.Text( InTitle )
							.Font( IDetailLayoutBuilder::GetDetailFont() )
							.ColorAndOpacity(TextColor)
						]
					]
				]
			]
		]
	];
}

void SModularRigModelItem::OnConnectorTargetChanged(TSharedPtr<FRigTreeElement> Selection, ESelectInfo::Type SelectInfo, const FRigElementKey InConnectorKey)
{
	if (!Selection.IsValid() || SelectInfo == ESelectInfo::OnNavigation)
	{
		return;
	}
	
	FScopedTransaction Transaction(LOCTEXT("ModuleHierarchyResolveConnector", "Resolve Connector"));
	TSharedPtr<FRigTreeElement> NewSelection = Selection;
	Delegates.HandleResolveConnector(InConnectorKey, NewSelection->Key);
}

void SModularRigModelItem::OnNameCommitted(const FText& InText, ETextCommit::Type InCommitType) const
{
	// for now only allow enter
	// because it is important to keep the unique names per pose
	if (InCommitType == ETextCommit::OnEnter)
	{
		FString NewName = InText.ToString();
		const FString OldKey = WeakRigTreeElement.Pin()->ModulePath;

		Delegates.HandleRenameElement(OldKey, *NewName);
	}
}

bool SModularRigModelItem::OnVerifyNameChanged(const FText& InText, FText& OutErrorMessage)
{
	const FName NewName = *InText.ToString();
	const FString OldPath = WeakRigTreeElement.Pin()->ModulePath;
	return Delegates.HandleVerifyElementNameChanged(OldPath, NewName, OutErrorMessage);
}

TSharedRef<SWidget> SModularRigModelItem::GenerateWidgetForColumn(const FName& ColumnName)
{
	if(ColumnName == SModularRigTreeView::Column_Module)
	{
		TSharedPtr< SInlineEditableTextBlock > InlineWidget;

		TSharedRef<SWidget> Widget = SNew(SHorizontalBox)

		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(6, 0, 0, 0)
		.VAlign(VAlign_Fill)
		[
			SNew(SExpanderArrow, SharedThis(this))
			.IndentAmount(12)
			.ShouldDrawWires(true)
		]

		+SHorizontalBox::Slot()
		.MaxWidth(25)
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.MaxHeight(25)
			[
				SNew(SImage)
				.Image_Lambda([this]() -> const FSlateBrush*
				{
					if(WeakRigTreeElement.IsValid())
					{
						return WeakRigTreeElement.Pin()->IconBrush;
					}
					return nullptr;
				})
				.ColorAndOpacity_Lambda([this]()
				{
					if(WeakRigTreeElement.IsValid())
					{
						return WeakRigTreeElement.Pin()->IconColor;
					}
					return FSlateColor::UseForeground();
				})
				.DesiredSizeOverride(FVector2D(16, 16))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SAssignNew(InlineWidget, SInlineEditableTextBlock)
			.Text(this, &SModularRigModelItem::GetName, true)
			.OnVerifyTextChanged(this, &SModularRigModelItem::OnVerifyNameChanged)
			.OnTextCommitted(this, &SModularRigModelItem::OnNameCommitted)
			.ToolTipText(this, &SModularRigModelItem::GetItemTooltip)
			.MultiLine(false)
			//.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity_Lambda([this]()
			{
				if(WeakRigTreeElement.IsValid())
				{
					return WeakRigTreeElement.Pin()->TextColor;
				}
				return FSlateColor::UseForeground();
			})
		];

		if(WeakRigTreeElement.IsValid())
		{
			WeakRigTreeElement.Pin()->OnRenameRequested.BindSP(InlineWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode);
		}

		return Widget;
	}
	if(ColumnName == SModularRigTreeView::Column_Connector)
	{
		TSharedPtr<SVerticalBox> ComboButtonBox;

		FRigTreeDelegates TreeDelegates;
		TreeDelegates.OnGetHierarchy = FOnGetRigTreeHierarchy::CreateLambda([this]()
		{
			return Delegates.GetModularRig()->GetHierarchy();
		});
		TreeDelegates.OnRigTreeIsItemVisible = FOnRigTreeIsItemVisible::CreateLambda([this](const FRigElementKey& InTarget)
		{
			if(!ConnectorMatches.IsSet() && WeakRigTreeElement.IsValid())
			{
				if (const UModularRig* ModularRig = Delegates.GetModularRig())
				{
					if (const FRigModuleInstance* Module = ModularRig->FindModule(WeakRigTreeElement.Pin()->ModulePath))
					{
						if (URigHierarchy* Hierarchy = ModularRig->GetHierarchy())
						{
							if (const FRigConnectorElement* ConnectorElement = Cast<FRigConnectorElement>(Hierarchy->Find(ConnectorKey)))
							{
								const UModularRigRuleManager* RuleManager = ModularRig->GetHierarchy()->GetRuleManager();
								ConnectorMatches = RuleManager->FindMatches(ConnectorElement, Module, ModularRig->GetElementKeyRedirector());
							}
						}
					}
				}
			}

			if(ConnectorMatches.IsSet())
			{
				return ConnectorMatches.GetValue().ContainsMatch(InTarget); 
			}
			return true;
		});
		TreeDelegates.OnGetSelection.BindLambda([this]() -> TArray<FRigElementKey>
		{
			const FRigElementKeyRedirector Redirector = Delegates.GetModularRig()->GetElementKeyRedirector();
			if (const FRigElementKey* Key = Redirector.FindExternalKey(ConnectorKey))
			{
				return {*Key};
			}
			return {};
		});
		TreeDelegates.OnSelectionChanged.BindSP(this, &SModularRigModelItem::OnConnectorTargetChanged, ConnectorKey);

		TSharedRef<SWidget> Widget = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		[
			SNew( SComboButton )
			.ContentPadding(3)
			.MenuPlacement(MenuPlacement_BelowAnchor)
			.OnComboBoxOpened(this, &SModularRigModelItem::PopulateConnectorTargetList, ConnectorKey)
			.ButtonContent()
			[
				// Wrap in configurable box to restrain height/width of menu
				SNew(SBox)
				.MinDesiredWidth(200.0f)
				.Padding(0, 0, 0, 0)
				.HAlign(HAlign_Left)
				[
					SAssignNew(ComboButtonBox, SVerticalBox)
				]
			]
			.MenuContent()
			[
				SNew(SBorder)
				.Visibility(EVisibility::Visible)
				.BorderImage(FAppStyle::GetBrush("Menu.Background"))
				[
					SAssignNew(ConnectorComboBox, SSearchableRigHierarchyTreeView)
						.RigTreeDelegates(TreeDelegates)
						.MaxHeight(300)
				]
			]
		];

		const UModularRig* ModularRig = Delegates.GetModularRig();
		const FRigElementKeyRedirector& Redirector = ModularRig->GetElementKeyRedirector();
		FRigElementKey CurrentTargetKey;
		if (const FRigElementKey* Key = Redirector.FindExternalKey(ConnectorKey))
		{
			CurrentTargetKey = *Key;
		}
		TPair<const FSlateBrush*, FSlateColor> IconAndColor = SRigHierarchyItem::GetBrushForElementType(ModularRig->GetHierarchy(), CurrentTargetKey);
		PopulateConnectorCurrentTarget(ComboButtonBox, ConnectorKey, CurrentTargetKey, IconAndColor.Key, IconAndColor.Value, FText::FromName(CurrentTargetKey.Name));

		return Widget;
	}
	if(ColumnName == SModularRigTreeView::Column_Buttons)
	{
		return SNew(SHorizontalBox)

		// Reset button
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, 0.f, 0.f)
		[
			SAssignNew(ResetConnectorButton, SButton)
			.ButtonStyle( FAppStyle::Get(), "NoBorder" )
			.ButtonColorAndOpacity_Lambda([this]()
			{
				return ResetConnectorButton.IsValid() && ResetConnectorButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
			})
			.OnClicked_Lambda([this]()
			{
				Delegates.HandleDisconnectConnector(ConnectorKey);
				return FReply::Handled();
			})
			.ContentPadding(1.f)
			.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Reset_Connector", "Reset Connector"))
			[
				SNew(SImage)
				.ColorAndOpacity_Lambda( [this]()
				{
					return ResetConnectorButton.IsValid() && ResetConnectorButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
				})
				.Image(FSlateIcon(FAppStyle::Get().GetStyleSetName(), "PropertyWindow.DiffersFromDefault").GetIcon())
			]
		]

		// Use button
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, 0.f, 0.f)
		[
			SAssignNew(UseSelectedButton, SButton)
			.ButtonStyle( FAppStyle::Get(), "NoBorder" )
			.ButtonColorAndOpacity_Lambda([this]()
			{
				return UseSelectedButton.IsValid() && UseSelectedButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
			})
			.OnClicked_Lambda([this]()
			{
				if (const UModularRig* ModularRig = Delegates.GetModularRig())
				{
					const TArray<FRigElementKey>& Selected = ModularRig->GetHierarchy()->GetSelectedKeys();
					if (Selected.Num() > 0)
					{
						Delegates.HandleResolveConnector(ConnectorKey, Selected[0]);
					}
				}
				return FReply::Handled();
			})
			.ContentPadding(1.f)
			.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Use_Selected", "Use Selected"))
			[
				SNew(SImage)
				.ColorAndOpacity_Lambda( [this]()
				{
					return UseSelectedButton.IsValid() && UseSelectedButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
				})
				.Image(FAppStyle::GetBrush("Icons.CircleArrowLeft"))
			]
		]
		
		// Select in hierarchy button
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(0.f, 0.f, 0.f, 0.f)
		[
			SAssignNew(SelectElementButton, SButton)
			.ButtonStyle( FAppStyle::Get(), "NoBorder" )
			.ButtonColorAndOpacity_Lambda([this]()
			{
				return SelectElementButton.IsValid() && SelectElementButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
			})
			.OnClicked_Lambda([this]()
			{
				if (const UModularRig* ModularRig = Delegates.GetModularRig())
				{
					const FRigElementKeyRedirector& Redirector = ModularRig->GetElementKeyRedirector();
					if (const FRigElementKey* TargetKey = Redirector.FindExternalKey(ConnectorKey))
					{
						ModularRig->GetHierarchy()->GetController()->SelectElement(*TargetKey, true, true);
					}
				}
				return FReply::Handled();
			})
			.ContentPadding(1.f)
			.ToolTipText(NSLOCTEXT("ControlRigModuleDetails", "Select_Element", "Select Element"))
			[
				SNew(SImage)
				.ColorAndOpacity_Lambda( [this]()
				{
					return SelectElementButton.IsValid() && SelectElementButton->IsHovered()
					? FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.8))
					: FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.4));
				})
				.Image(FAppStyle::GetBrush("Icons.Search"))
			]
		];
	}
	return SNullWidget::NullWidget;
}

FText SModularRigModelItem::GetName(bool bUseShortName) const
{
	if(bUseShortName)
	{
		return (FText::FromName(WeakRigTreeElement.Pin()->ShortName));
	}
	return (FText::FromString(WeakRigTreeElement.Pin()->ModulePath));
}

FText SModularRigModelItem::GetItemTooltip() const
{
	const FText FullName = GetName(false);
	const FText ShortName = GetName(true);
	if(FullName.EqualTo(ShortName))
	{
		return FText();
	}
	return FullName;
}

//////////////////////////////////////////////////////////////
/// SModularRigTreeView
///////////////////////////////////////////////////////////

const FName SModularRigTreeView::Column_Module = TEXT("Module");
const FName SModularRigTreeView::Column_Connector = TEXT("Connector");
const FName SModularRigTreeView::Column_Buttons = TEXT("Actions");

void SModularRigTreeView::Construct(const FArguments& InArgs)
{
	Delegates = InArgs._RigTreeDelegates;
	bAutoScrollEnabled = InArgs._AutoScrollEnabled;

	STreeView<TSharedPtr<FModularRigTreeElement>>::FArguments SuperArgs;
	SuperArgs.HeaderRow(InArgs._HeaderRow);
	SuperArgs.TreeItemsSource(&RootElements);
	SuperArgs.SelectionMode(ESelectionMode::Multi);
	SuperArgs.OnGenerateRow(this, &SModularRigTreeView::MakeTableRowWidget, false);
	SuperArgs.OnGetChildren(this, &SModularRigTreeView::HandleGetChildrenForTree);
	SuperArgs.OnContextMenuOpening(Delegates.OnContextMenuOpening);
	SuperArgs.HighlightParentNodesForSelection(true);
	SuperArgs.ItemHeight(24);
	SuperArgs.AllowInvisibleItemSelection(true);  //without this we deselect everything when we filter or we collapse
	SuperArgs.OnMouseButtonClick(Delegates.OnMouseButtonClick);
	SuperArgs.OnMouseButtonDoubleClick(Delegates.OnMouseButtonDoubleClick);
	
	SuperArgs.ShouldStackHierarchyHeaders_Lambda([]() -> bool {
		return UControlRigEditorSettings::Get()->bShowStackedHierarchy;
	});
	SuperArgs.OnGeneratePinnedRow(this, &SModularRigTreeView::MakeTableRowWidget, true);
	SuperArgs.MaxPinnedItems_Lambda([]() -> int32
	{
		return FMath::Max<int32>(1, UControlRigEditorSettings::Get()->MaxStackSize);
	});

	STreeView<TSharedPtr<FModularRigTreeElement>>::Construct(SuperArgs);

	LastMousePosition = FVector2D::ZeroVector;
	TimeAtMousePosition = 0.0;
}

void SModularRigTreeView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	STreeView<TSharedPtr<FModularRigTreeElement, ESPMode::ThreadSafe>>::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const FGeometry PaintGeometry = GetPaintSpaceGeometry();
	const FVector2D MousePosition = FSlateApplication::Get().GetCursorPos();

	if(PaintGeometry.IsUnderLocation(MousePosition))
	{
		const FVector2D WidgetPosition = PaintGeometry.AbsoluteToLocal(MousePosition);

		static constexpr float SteadyMousePositionTolerance = 5.f;

		if(LastMousePosition.Equals(MousePosition, SteadyMousePositionTolerance))
		{
			TimeAtMousePosition += InDeltaTime;
		}
		else
		{
			LastMousePosition = MousePosition;
			TimeAtMousePosition = 0.0;
		}

		static constexpr float AutoScrollStartDuration = 0.5f; // in seconds
		static constexpr float AutoScrollDistance = 24.f; // in pixels
		static constexpr float AutoScrollSpeed = 150.f;

		if(TimeAtMousePosition > AutoScrollStartDuration && FSlateApplication::Get().IsDragDropping())
		{
			if((WidgetPosition.Y < AutoScrollDistance) || (WidgetPosition.Y > PaintGeometry.Size.Y - AutoScrollDistance))
			{
				if(bAutoScrollEnabled)
				{
					const bool bScrollUp = (WidgetPosition.Y < AutoScrollDistance);

					const float DeltaInSlateUnits = (bScrollUp ? -InDeltaTime : InDeltaTime) * AutoScrollSpeed; 
					ScrollBy(GetCachedGeometry(), DeltaInSlateUnits, EAllowOverscroll::No);
				}
			}
			else
			{
				const TSharedPtr<FModularRigTreeElement>* Item = FindItemAtPosition(MousePosition);
				if(Item && Item->IsValid())
				{
					if(!IsItemExpanded(*Item))
					{
						SetItemExpansion(*Item, true);
					}
				}
			}
		}
	}

	if (bRequestRenameSelected)
	{
		RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateLambda([this](double, float) {
			TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = GetSelectedItems();
			if (SelectedItems.Num() == 1)
			{
				SelectedItems[0]->RequestRename();
			}
			return EActiveTimerReturnType::Stop;
		}));
		bRequestRenameSelected = false;
	}
}

TSharedPtr<FModularRigTreeElement> SModularRigTreeView::FindElement(const FString& InElementKey)
{
	for (TSharedPtr<FModularRigTreeElement> Root : RootElements)
	{
		if (TSharedPtr<FModularRigTreeElement> Found = FindElement(InElementKey, Root))
		{
			return Found;
		}
	}

	return TSharedPtr<FModularRigTreeElement>();
}

TSharedPtr<FModularRigTreeElement> SModularRigTreeView::FindElement(const FString& InElementKey, TSharedPtr<FModularRigTreeElement> CurrentItem)
{
	if (CurrentItem->Key == InElementKey)
	{
		return CurrentItem;
	}

	for (int32 ChildIndex = 0; ChildIndex < CurrentItem->Children.Num(); ++ChildIndex)
	{
		TSharedPtr<FModularRigTreeElement> Found = FindElement(InElementKey, CurrentItem->Children[ChildIndex]);
		if (Found.IsValid())
		{
			return Found;
		}
	}

	return TSharedPtr<FModularRigTreeElement>();
}

bool SModularRigTreeView::AddElement(FString InKey, FString InParentKey)
{
	if(ElementMap.Contains(InKey))
	{
		return false;
	}

	if (!InKey.IsEmpty())
	{
		TSharedPtr<FModularRigTreeElement> NewItem = MakeShared<FModularRigTreeElement>(InKey, SharedThis(this), true);

		ElementMap.Add(InKey, NewItem);
		if (!InParentKey.IsEmpty())
		{
			ParentMap.Add(InKey, InParentKey);

			TSharedPtr<FModularRigTreeElement>* FoundItem = ElementMap.Find(InParentKey);
			check(FoundItem);
			FoundItem->Get()->Children.Add(NewItem);
		}
		else
		{
			RootElements.Add(NewItem);
		}

		SetItemExpansion(NewItem, true);

		if (const UModularRig* ModularRig = Delegates.GetModularRig())
		{
			if (const FRigModuleInstance* Module = ModularRig->FindModule(NewItem->ModulePath))
			{
				if (const UControlRig* ModuleRig = Module->GetRig())
				{
					const UControlRig* CDO = ModuleRig->GetClass()->GetDefaultObject<UControlRig>();
					const TArray<FRigModuleConnector>& Connectors = CDO->GetRigModuleSettings().ExposedConnectors;
					for (const FRigModuleConnector& Connector : Connectors)
					{
						if (Connector.IsPrimary())
						{
							continue;
						}

						const FString Key = FString::Printf(TEXT("%s:%s"), *NewItem->ModulePath, *Connector.Name);
						TSharedPtr<FModularRigTreeElement> ConnectorItem = MakeShared<FModularRigTreeElement>(Key, SharedThis(this), false);
						NewItem.Get()->Children.Add(ConnectorItem);
						ElementMap.Add(Key, ConnectorItem);
						ParentMap.Add(Key, InKey);
					}
				}
			}
		}
	}

	return true;
}

bool SModularRigTreeView::AddElement(const FRigModuleInstance* InElement)
{
	check(InElement);
	
	if (ElementMap.Contains(InElement->GetPath()))
	{
		return false;
	}

	const UModularRig* ModularRig = Delegates.GetModularRig();

	FString ElementPath = InElement->GetPath();
	if(!AddElement(ElementPath, FString()))
	{
		return false;
	}

	if (ElementMap.Contains(InElement->GetPath()))
	{
		if(ModularRig)
		{
			FString ParentPath = ModularRig->GetParentPath(ElementPath);
			if (!ParentPath.IsEmpty())
			{
				if(const FRigModuleInstance* ParentElement = ModularRig->FindModule(ParentPath))
				{
					AddElement(ParentElement);

					if(ElementMap.Contains(ParentPath))
					{
						ReparentElement(ElementPath, ParentPath);
					}
				}
			}
		}
	}

	return true;
}

void SModularRigTreeView::AddSpacerElement()
{
	AddElement(FString(), FString());
}

bool SModularRigTreeView::ReparentElement(const FString InKey, const FString InParentKey)
{
	if (InKey.IsEmpty() || InKey == InParentKey)
	{
		return false;
	}

	TSharedPtr<FModularRigTreeElement>* FoundItem = ElementMap.Find(InKey);
	if (FoundItem == nullptr)
	{
		return false;
	}

	if (const FString* ExistingParentKey = ParentMap.Find(InKey))
	{
		if (*ExistingParentKey == InParentKey)
		{
			return false;
		}

		if (TSharedPtr<FModularRigTreeElement>* ExistingParent = ElementMap.Find(*ExistingParentKey))
		{
			(*ExistingParent)->Children.Remove(*FoundItem);
		}

		ParentMap.Remove(InKey);
	}
	else
	{
		if (InParentKey.IsEmpty())
		{
			return false;
		}

		RootElements.Remove(*FoundItem);
	}

	if (!InParentKey.IsEmpty())
	{
		ParentMap.Add(InKey, InParentKey);

		TSharedPtr<FModularRigTreeElement>* FoundParent = ElementMap.Find(InParentKey);
		if(FoundParent)
		{
			FoundParent->Get()->Children.Add(*FoundItem);
		}
	}
	else
	{
		RootElements.Add(*FoundItem);
	}

	return true;
}

void SModularRigTreeView::RefreshTreeView(bool bRebuildContent)
{
	TMap<FString, bool> ExpansionState;

	if(bRebuildContent)
	{
		for (TPair<FString, TSharedPtr<FModularRigTreeElement>> Pair : ElementMap)
		{
			ExpansionState.FindOrAdd(Pair.Key) = IsItemExpanded(Pair.Value);
		}

		// internally save expansion states before rebuilding the tree, so the states can be restored later
		SaveAndClearSparseItemInfos();

		RootElements.Reset();
		ElementMap.Reset();
		ParentMap.Reset();
	}

	if(bRebuildContent)
	{
		const UModularRig* ModularRig = Delegates.GetModularRig();
		if(ModularRig)
		{
			ModularRig->ForEachModule([&](const FRigModuleInstance* Element)
			{
				AddElement(Element);
				return true;
			});

			// expand all elements upon the initial construction of the tree
			if (ExpansionState.Num() < ElementMap.Num())
			{
				for (const TPair<FString, TSharedPtr<FModularRigTreeElement>>& Element : ElementMap)
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

			if (RootElements.Num() > 0)
			{
				AddSpacerElement();
			}
		}
	}
	else
	{
		if (RootElements.Num()> 0)
		{
			// elements may be added at the end of the list after a spacer element
			// we need to remove the spacer element and re-add it at the end
			RootElements.RemoveAll([](TSharedPtr<FModularRigTreeElement> InElement)
			{
				return InElement.Get()->Key == FString();
			});
			AddSpacerElement();
		}
	}

	RequestTreeRefresh();
	{
		ClearSelection();
	}
}

TSharedRef<ITableRow> SModularRigTreeView::MakeTableRowWidget(TSharedPtr<FModularRigTreeElement> InItem,
	const TSharedRef<STableViewBase>& OwnerTable, bool bPinned)
{
	return InItem->MakeTreeRowWidget(OwnerTable, InItem.ToSharedRef(), SharedThis(this), bPinned);
}

void SModularRigTreeView::HandleGetChildrenForTree(TSharedPtr<FModularRigTreeElement> InItem,
	TArray<TSharedPtr<FModularRigTreeElement>>& OutChildren)
{
	OutChildren = InItem.Get()->Children;
}

TArray<FString> SModularRigTreeView::GetSelectedKeys() const
{
	TArray<FString> Keys;
	TArray<TSharedPtr<FModularRigTreeElement>> SelectedElements = GetSelectedItems();
	for(const TSharedPtr<FModularRigTreeElement>& SelectedElement : SelectedElements)
	{
		Keys.AddUnique(SelectedElement->ModulePath);
	}
	return Keys;
}

void SModularRigTreeView::SetSelection(const TArray<TSharedPtr<FModularRigTreeElement>>& InSelection) 
{
	ClearSelection();
	SetItemSelection(InSelection, true, ESelectInfo::Direct);
}

const TSharedPtr<FModularRigTreeElement>* SModularRigTreeView::FindItemAtPosition(FVector2D InScreenSpacePosition) const
{
	if (ItemsPanel.IsValid() && SListView<TSharedPtr<FModularRigTreeElement>>::HasValidItemsSource())
	{
		FArrangedChildren ArrangedChildren(EVisibility::Visible);
		const int32 Index = FindChildUnderPosition(ArrangedChildren, InScreenSpacePosition);
		if (ArrangedChildren.IsValidIndex(Index))
		{
			TSharedRef<SModularRigModelItem> ItemWidget = StaticCastSharedRef<SModularRigModelItem>(ArrangedChildren[Index].Widget);
			if (ItemWidget->WeakRigTreeElement.IsValid())
			{
				const FString Key = ItemWidget->WeakRigTreeElement.Pin()->Key;
				const TSharedPtr<FModularRigTreeElement>* ResultPtr = SListView<TSharedPtr<FModularRigTreeElement>>::GetItems().FindByPredicate([Key](const TSharedPtr<FModularRigTreeElement>& Item) -> bool
					{
						return Item->Key == Key;
					});

				if (ResultPtr)
				{
					return ResultPtr;
				}
			}
		}
	}
	return nullptr;
}

TPair<const FSlateBrush*, FSlateColor> FModularRigTreeElement::GetBrushAndColor(const UModularRig* InModularRig)
{
	const FSlateBrush* Brush = nullptr;
	FLinearColor Color = FSlateColor(EStyleColor::Foreground).GetColor(FWidgetStyle());
	float Opacity = 1.f;

	if (const FRigModuleInstance* ConnectorModule = InModularRig->FindModule(ModulePath))
	{
		const FModularRigModel& Model = InModularRig->GetModularRigModel();
		const FString ConnectorPath = FString::Printf(TEXT("%s:%s"), *ModulePath, *ConnectorName);
		bool bIsConnected = Model.Connections.HasConnection(FRigElementKey(*ConnectorPath, ERigElementType::Connector));
		bool bConnectionWarning = !bIsConnected;
		
		if (const UControlRig* ModuleRig = ConnectorModule->GetRig())
		{
			const FRigModuleConnector* Connector = ModuleRig->GetRigModuleSettings().ExposedConnectors.FindByPredicate([this](FRigModuleConnector& Connector)
			{
				return Connector.Name == ConnectorName;
			});
			if (Connector)
			{
				if (Connector->IsPrimary())
				{
					if (bIsConnected)
					{
						const FSoftObjectPath IconPath = ModuleRig->GetRigModuleSettings().Icon;
						const TSharedPtr<FSlateBrush>* ExistingBrush = IconPathToBrush.Find(IconPath);
						if(ExistingBrush && ExistingBrush->IsValid())
						{
							Brush = ExistingBrush->Get();
						}
						else
						{
							if(UTexture2D* Icon = Cast<UTexture2D>(IconPath.TryLoad()))
							{
								const TSharedPtr<FSlateBrush> NewBrush = MakeShareable(new FSlateBrush(UWidgetBlueprintLibrary::MakeBrushFromTexture(Icon, 16.0f, 16.0f)));
								IconPathToBrush.FindOrAdd(IconPath) = NewBrush;
								Brush = NewBrush.Get();
							}
						}
					}
					else
					{
						Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorWarning");
					}
				}
				else if (Connector->Settings.bOptional)
				{
					bConnectionWarning = false;
					if (!bIsConnected)
					{
						Opacity = 0.7;
						Color = FSlateColor(EStyleColor::Hover2).GetColor(FWidgetStyle());
					}
					Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorOptional");
				}
				else
				{
					Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.ConnectorSecondary");
				}
			}
		}

		if (bConnectionWarning)
		{
			Color = FSlateColor(EStyleColor::Warning).GetColor(FWidgetStyle());
		}
	}
	if (!Brush)
	{
		Brush = FControlRigEditorStyle::Get().GetBrush("ControlRig.Tree.RigidBody");
	}

	// Apply opacity
	Color = Color.CopyWithNewOpacity(Opacity);
	
	return TPair<const FSlateBrush*, FSlateColor>(Brush, Color);
}

//////////////////////////////////////////////////////////////
/// SSearchableModularRigTreeView
///////////////////////////////////////////////////////////

void SSearchableModularRigTreeView::Construct(const FArguments& InArgs)
{
	FModularRigTreeDelegates TreeDelegates = InArgs._RigTreeDelegates;
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
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
					SAssignNew(TreeView, SModularRigTreeView)
					.RigTreeDelegates(TreeDelegates)
				]
			]
		]
	];
}


#undef LOCTEXT_NAMESPACE
