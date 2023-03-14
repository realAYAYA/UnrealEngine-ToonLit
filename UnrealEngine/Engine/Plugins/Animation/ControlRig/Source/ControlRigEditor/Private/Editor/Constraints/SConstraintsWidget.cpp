// Copyright Epic Games, Inc. All Rights Reserved.

#include "SConstraintsWidget.h"

#include "ActorPickerMode.h"
#include "ConstraintChannelHelper.h"
#include "ConstraintsActor.h"
#include "ControlRigEditorStyle.h"
#include "DetailLayoutBuilder.h"
#include "LevelEditorActions.h"
#include "LevelEditorViewport.h"
#include "PropertyCustomizationHelpers.h"
#include "Selection.h"
#include "SlateOptMacros.h"
#include "LevelEditor.h"
#include "SSocketChooser.h"
#include "TransformConstraint.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Input/SButton.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "PropertyEditorModule.h"
#include "DetailsViewArgs.h"
#include "Tools/ConstraintBaker.h"
#include "ScopedTransaction.h"
#include "ISequencer.h"
#include "Tools/BakingHelper.h"
#include "MovieSceneToolHelpers.h"

#define LOCTEXT_NAMESPACE "SConstraintsWidget"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TArray< SConstraintsCreationWidget::ItemSharedPtr > SConstraintsCreationWidget::ListItems({
	FDroppableConstraintItem::Make(ETransformConstraintType::Translation),
	FDroppableConstraintItem::Make(ETransformConstraintType::Rotation),
	FDroppableConstraintItem::Make(ETransformConstraintType::Scale),
	FDroppableConstraintItem::Make(ETransformConstraintType::Parent),
	FDroppableConstraintItem::Make(ETransformConstraintType::LookAt)
});

const TArray< const FSlateBrush* >& FConstraintInfo::GetBrushes()
{
	static const TArray< const FSlateBrush* > Brushes({
		FAppStyle::Get().GetBrush("EditorViewport.TranslateMode"),
		FAppStyle::Get().GetBrush("EditorViewport.RotateMode"),
		FAppStyle::Get().GetBrush("EditorViewport.ScaleMode"),
		FAppStyle::Get().GetBrush("Icons.ConstraintManager.ParentHierarchy"),
		FAppStyle::Get().GetBrush("Icons.ConstraintManager.LookAt")
		});
	return Brushes;
}

const TMap< UClass*, ETransformConstraintType >& FConstraintInfo::GetConstraintToType()
{
	static const TMap< UClass*, ETransformConstraintType > ConstraintToType({
		{UTickableTranslationConstraint::StaticClass(), ETransformConstraintType::Translation},
		{UTickableRotationConstraint::StaticClass(), ETransformConstraintType::Rotation},
		{UTickableScaleConstraint::StaticClass(), ETransformConstraintType::Scale},
		{UTickableParentConstraint::StaticClass(), ETransformConstraintType::Parent},
		{UTickableLookAtConstraint::StaticClass(), ETransformConstraintType::LookAt}
		});
	return ConstraintToType;
}

const FSlateBrush* FConstraintInfo::GetBrush(uint8 InType)
{
	static const UEnum* ETransformConstraintTypeEnum = StaticEnum<ETransformConstraintType>();
	if (ETransformConstraintTypeEnum->IsValidEnumValue(InType))
	{
		return GetBrushes()[InType];
	}

	return FAppStyle::Get().GetDefaultBrush();
}

int8 FConstraintInfo::GetType(UClass* InClass)
{
	if (const ETransformConstraintType* TransformConstraint = GetConstraintToType().Find(InClass))
	{
		return static_cast<int8>(*TransformConstraint); 
	}
	return -1;
}

namespace
{

UWorld* GetCurrentWorld()
{
	return GCurrentLevelEditingViewportClient ? GCurrentLevelEditingViewportClient->GetWorld() : nullptr;
}
	
// NOTE we use this function to get the current selection as control actors are Temporary Editor Actors so won't be
// pushed added to the selection list.
TArray<AActor*> GetCurrentSelection()
{
	const UWorld* World = GetCurrentWorld();
	const ULevel* CurrentLevel = IsValid(World) ? World->GetCurrentLevel() : nullptr;
	if (!CurrentLevel)
	{
		static const TArray<AActor*> DummyArray;
		return DummyArray;
	}

	return CurrentLevel->Actors.FilterByPredicate( [](const AActor* Actor)
	{
		return IsValid(Actor) && Actor->IsSelected();
	});	
}
	
}

/**
 * SConstraintItem
 */

void SDroppableConstraintItem::Construct(
		const FArguments& InArgs,
		const TSharedPtr<const FDroppableConstraintItem>& InItem,
		TSharedPtr<SConstraintsCreationWidget> InConstraintsWidget)
{
	ConstraintItem = InItem;
	ConstraintType = InItem->Type;
	ConstraintsWidget = InConstraintsWidget;
	
	const FButtonStyle& ButtonStyle = FAppStyle::GetWidgetStyle<FButtonStyle>( "PlacementBrowser.Asset" );

	// enum to string
	const UEnum* ETransformConstraintTypeEnum = StaticEnum<ETransformConstraintType>();
	// const uint8 ConstraintType = static_cast<uint8>(InItem->Type);
	const FString TypeStr = ETransformConstraintTypeEnum->GetNameStringByValue((uint8)ConstraintType);

	// tooltip
	const FString ToolTipStr = FString::Printf(TEXT("Create new %s constraint."), *TypeStr);
	const TSharedPtr<IToolTip> ToolTip = FSlateApplicationBase::Get().MakeToolTip(FText::FromString(ToolTipStr));
	
	ChildSlot
	.Padding(FMargin(8.f, 2.f, 12.f, 2.f))
	[
		SNew(SOverlay)

		+SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage( FAppStyle::Get().GetBrush("PlacementBrowser.Asset.Background"))
			.Cursor( EMouseCursor::GrabHand )
			.ToolTip(ToolTip)
			.Padding(0)
			[
				SNew( SHorizontalBox )

				+ SHorizontalBox::Slot()
				.Padding(8.0f, 4.f)
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew( SBox )
					.WidthOverride(40)
					.HeightOverride(40)
					[
					 	SNew(SImage)
					 	.DesiredSizeOverride(FVector2D(16, 16))
						.Image(FConstraintInfo::GetBrush((uint8)ConstraintType))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Fill)
				.Padding(0)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush("PlacementBrowser.Asset.LabelBack"))
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.Padding(9, 0, 0, 1)
						.VAlign(VAlign_Center)
						[
							SNew( STextBlock )
							.TextStyle( FAppStyle::Get(), "PlacementBrowser.Asset.Name" )
							.Text_Lambda( [TypeStr]
							{
								return FText::FromString(TypeStr);
							} )
						]
					]
				]
			]
		]

		+SOverlay::Slot()
		[
			SNew(SBorder)
			.BorderImage_Lambda( [this, &ButtonStyle]
			{
				if (bIsPressed)
				{
					return &ButtonStyle.Pressed;
				}
				
				if (IsHovered())
				{
					return &ButtonStyle.Hovered;
				}

				return &ButtonStyle.Normal;
			})
			.Cursor( EMouseCursor::GrabHand )
			.ToolTip( ToolTip )
		]
	];
}

FReply SDroppableConstraintItem::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsPressed = true;
		// return FReply::Handled().DetectDrag( SharedThis( this ), MouseEvent.GetEffectingButton() );
		return CreateSelectionPicker();
	}

	return FReply::Unhandled();
}

FReply SDroppableConstraintItem::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = false;
	}

	return FReply::Unhandled();
}

FReply SDroppableConstraintItem::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsPressed = false;
	
	// if (MouseEvent.IsMouseButtonDown( EKeys::LeftMouseButton ))
	// {
	// 	return CreateSelectionPicker();
	// }
	
	return FReply::Handled();
}

FReply SDroppableConstraintItem::CreateSelectionPicker() const
{
	// FIXME temp approach for selecting the parent
	FSlateApplication::Get().DismissAllMenus();
	
	static const FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

	TSharedPtr<SConstraintsCreationWidget> ConstraintsCreationWidget = this->ConstraintsWidget.Pin();
	ETransformConstraintType ConstraintTypeCopy = this->ConstraintType;
	ActorPickerMode.BeginActorPickingMode(
		FOnGetAllowedClasses(), 
		FOnShouldFilterActor(), 
		FOnActorSelected::CreateLambda([ConstraintsCreationWidget, ConstraintTypeCopy](AActor* InActor)
		{
			const FOnConstraintCreated CreationDelegate = ConstraintsCreationWidget.IsValid() ?
				ConstraintsCreationWidget->OnConstraintCreated : FOnConstraintCreated();
			SDroppableConstraintItem::CreateConstraint(InActor, CreationDelegate, ConstraintTypeCopy);
		}) );

	
	return FReply::Handled();
}

void SDroppableConstraintItem::CreateConstraint(
	AActor* InParent,
	FOnConstraintCreated InCreationDelegate,
	const ETransformConstraintType InConstraintType)
{
	if (!InParent)
	{
		return;	
	}

	const UEnum* ETransformConstraintTypeEnum = StaticEnum<ETransformConstraintType>();
	if (!ETransformConstraintTypeEnum->IsValidEnumValue(static_cast<int64>(InConstraintType)))
	{
		return;
	}
	
	// get selected actors
	const TArray<AActor*> Selection = GetCurrentSelection();
	if (Selection.IsEmpty())
	{
		return;	
	}

	// has socket?
	USceneComponent* ComponentWithSockets = nullptr;
	if(USceneComponent* ParentComponent = InParent->GetRootComponent())
	{
		if (ParentComponent->HasAnySockets())
		{
			ComponentWithSockets = ParentComponent;
		}
	}

	// create constraints
	auto CreateConstraint = [InCreationDelegate, InConstraintType](
		const TArray<AActor*>& Selection, AActor* InParent, const FName& InSocketName)
	{
		UWorld* World = GetCurrentWorld();
		if (!IsValid(World))
		{
			return;
		}

		bool bCreated = false;
		for (AActor* Child: Selection)
		{
			if (Child != InParent)
			{
				FScopedTransaction Transaction(LOCTEXT("CreateConstraintKey", "Create Constraint Key"));
				UTickableTransformConstraint* Constraint =
					FTransformConstraintUtils::CreateAndAddFromActors(World, InParent, InSocketName, Child, InConstraintType);
				if (Constraint)
				{
					FConstraintChannelHelper::SmartConstraintKey(Constraint);
					bCreated = true;
				}
			}
		}
	
		// update list
		if (bCreated && InCreationDelegate.IsBound())
		{
			InCreationDelegate.Execute();
		}
	};
	
	// Show socket chooser if we have sockets to select
	if (ComponentWithSockets != nullptr)
	{		
		const FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		const TSharedPtr< ILevelEditor > LevelEditor = LevelEditorModule.GetFirstLevelEditor();

		FVector2D SummonLocation = FSlateApplication::Get().GetCursorPos();
		SummonLocation.Y += 4 * FSlateApplication::Get().GetCursorSize().Y;
		
		// Create as context menu
		FSlateApplication::Get().PushMenu(
			LevelEditor.ToSharedRef(),
			FWidgetPath(),
			SNew(SSocketChooserPopup)
			.SceneComponent( ComponentWithSockets )
			.OnSocketChosen_Lambda([CreateConstraint, Selection, InParent](FName InSocketName)
			{
				CreateConstraint(Selection, InParent, InSocketName);
			}),
			SummonLocation,
			FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu )
			);
	}
	else
	{
		CreateConstraint(Selection, InParent, NAME_None);
	}
}

/**
 * SConstraintCreationWidget
 */

void SConstraintsCreationWidget::Construct(const FArguments& InArgs)
{
	OnConstraintCreated = InArgs._OnConstraintCreated;
	
	ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 3.f))
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SAssignNew(ListView, ConstraintItemListView)
				.SelectionMode(ESelectionMode::None)
				.ListItemsSource( &ListItems )
				.OnGenerateRow(this, &SConstraintsCreationWidget::OnGenerateWidgetForItem)
			]
		]
	];
}

TSharedRef<ITableRow> SConstraintsCreationWidget::OnGenerateWidgetForItem(
	ItemSharedPtr InItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<ItemSharedPtr>, OwnerTable)
	.Style(&FAppStyle::Get(), "PlacementBrowser.PlaceableItemRow")
	[
		SNew(SDroppableConstraintItem, InItem.ToSharedRef(), SharedThis(this))
	];
}

/**
 * SEditableConstraintItem
 */

void SEditableConstraintItem::Construct(
	const FArguments& InArgs,
	const TSharedPtr<FEditableConstraintItem>& InItem,
	TSharedPtr<SConstraintsEditionWidget> InConstraintsWidget)
{
	ConstraintItem = InItem;
	ConstraintsWidget = InConstraintsWidget;

	static const FSlateBrush* RoundedBoxBrush = FControlRigEditorStyle::Get().GetBrush(TEXT("ControlRig.SpacePicker.RoundedRect"));
	
	// enum to string
	const uint8 ConstraintType = static_cast<uint8>(InItem->Type);

	const FSimpleDelegate OnConstraintRemoved = FSimpleDelegate::CreateLambda([this]()
	{
		if(ConstraintsWidget.IsValid())
		{
			ConstraintsWidget.Pin()->RemoveItem(ConstraintItem);
		}
	});

	// constraint
	auto GetConstraint = [this]() -> UTickableConstraint*
	{
		UWorld* World = GetCurrentWorld();
		if (!IsValid(World))
		{
			return nullptr;
		}
		
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		return Controller.GetConstraint(ConstraintItem->Name);
	};
	UTickableConstraint* Constraint = GetConstraint();

	// labels
	FString ParentLabel(TEXT("undefined")), ChildLabel(TEXT("undefined"));
	if (!InItem->Label.IsEmpty())
	{
		ChildLabel = InItem->Label;
		InItem->Label.Split(TEXT("."), &ParentLabel, &ChildLabel);
	}
	
	FString ParentFullLabel = ParentLabel, ChildFullLabel = ChildLabel;
	if (IsValid(Constraint))
	{
		Constraint->GetFullLabel().Split(TEXT("."), &ParentFullLabel, &ChildFullLabel);
	}

	// widgets
	ChildSlot
	.Padding(FMargin(8.f, 2.f, 12.f, 2.f))
	[
		SNew(SHorizontalBox)
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Fill)
		.Padding(0)
		[
			SNew(SBorder)
			.Padding(FMargin(5.0, 2.0, 5.0, 2.0))
			.BorderImage(RoundedBoxBrush)
			.BorderBackgroundColor_Lambda([Constraint]()
			{
				if (!IsValid(Constraint))
				{
					return FStyleColors::Transparent;
				}
				return Constraint->IsFullyActive() ? FStyleColors::Select : FStyleColors::Transparent;
			})
			.Content()
			[
				SNew(SHorizontalBox)

				// constraint icon
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(FMargin(0.f, 0.f, 3.f, 0.f))
				[
					SNew(SImage)
					.Image(FConstraintInfo::GetBrush(ConstraintType))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]

				// constraint name
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(0)
				[
					SNew( STextBlock )
					.Text_Lambda( [ParentLabel]()
					{
						return FText::FromString(ParentLabel);
					})
					.Font_Lambda([Constraint]()
					{
						if (!IsValid(Constraint))
						{
							return IDetailLayoutBuilder::GetDetailFont();
						}
						return Constraint->Active ? IDetailLayoutBuilder::GetDetailFont() : IDetailLayoutBuilder::GetDetailFontItalic();
					})
					.ToolTipText_Lambda( [Constraint, ParentFullLabel, ChildFullLabel]()
					{
						if (!IsValid(Constraint))
						{
							return FText();
						}

						static constexpr TCHAR ToolTipFormat[] = TEXT("%s constraint between parent '%s' and child '%s'.");
						const FString TypeLabel = Constraint->GetTypeLabel();
						const FString FullLabel = FString::Printf(ToolTipFormat, *TypeLabel, *ParentFullLabel, *ChildFullLabel);
						return FText::FromString(FullLabel);
					})
				]
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.f)
		[
			SNew(SSpacer)
		]

		// add key
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(0)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
			.ContentPadding(0)
			.OnClicked_Lambda(	[Constraint]()
			{
				if (!IsValid(Constraint))
				{
					return FReply::Handled();
				}
				if (UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint))
				{
					FScopedTransaction Transaction(LOCTEXT("CreateConstraintKey", "Create Constraint Key"));
					FConstraintChannelHelper::SmartConstraintKey(TransformConstraint);
				}
				return FReply::Handled();
			})
			.IsEnabled_Lambda([]()
			{
				return FConstraintChannelHelper::IsKeyframingAvailable();
			})
			.Visibility_Lambda([]()
			{
				const bool bIsKeyframingAvailable = FConstraintChannelHelper::IsKeyframingAvailable();
				return bIsKeyframingAvailable ? EVisibility::Visible : EVisibility::Hidden;
			})
			.ToolTipText(LOCTEXT("KeyConstraintToolTip", "Add an active keyframe for that constraint."))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Sequencer.AddKey.Details"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
		
		// move up
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(0)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
			.ContentPadding(0)
			.OnClicked_Lambda(	[this, InItem]()
			{
				if (ConstraintsWidget.IsValid())
				{
					ConstraintsWidget.Pin()->MoveItemUp(InItem);
				}
				return FReply::Handled();
			})
			.IsEnabled_Lambda([this]()
			{
				return ConstraintsWidget.IsValid() ? ConstraintsWidget.Pin()->CanMoveUp(ConstraintItem) : false;
			})
			.ToolTipText(LOCTEXT("MoveConstraintUp", "Move this constraint up in the list."))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.ChevronUp"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]

		// move down
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(0)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), TEXT("SimpleButton"))
			.ContentPadding(0)
			.OnClicked_Lambda([this, InItem]()
			{
				if (ConstraintsWidget.IsValid())
				{
					ConstraintsWidget.Pin()->MoveItemDown(InItem);
				}	
				return FReply::Handled();
			})
			.IsEnabled_Lambda([this]()
			{
				return ConstraintsWidget.IsValid() ? ConstraintsWidget.Pin()->CanMoveDown(ConstraintItem) : false;
			})
			.ToolTipText(LOCTEXT("MoveConstraintDown", "Move this constraint down in the list."))
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("Icons.ChevronDown"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]

		// deletion
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Left)
		.Padding(0)
		[
			 PropertyCustomizationHelpers::MakeClearButton(OnConstraintRemoved, LOCTEXT("DeleteConstraint", "Remove this constraint."), true)
		]
	];
}

/**
 * SConstraintsEditionWidget
 */

void SConstraintsEditionWidget::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew( SVerticalBox )

		+ SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 3.f))
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SAssignNew(ListView, ConstraintItemListView)
				.SelectionMode(ESelectionMode::Single)
				.ListItemsSource( &ListItems )
				.OnGenerateRow(this, &SConstraintsEditionWidget::OnGenerateWidgetForItem)
				.OnContextMenuOpening(this, &SConstraintsEditionWidget::CreateContextMenu)
				.OnMouseButtonDoubleClick(this, &SConstraintsEditionWidget::OnItemDoubleClicked)
			]
		]
	];

	RefreshConstraintList();
	RegisterSelectionChanged();
}

SConstraintsEditionWidget::~SConstraintsEditionWidget()
{
	UnregisterSelectionChanged();
}

void SConstraintsEditionWidget::RegisterSelectionChanged()
{
	FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	FLevelEditorModule::FActorSelectionChangedEvent& ActorSelectionChangedEvent = LevelEditor.OnActorSelectionChanged();

	// unregister previous one
	if (OnSelectionChangedHandle.IsValid())
	{
		ActorSelectionChangedEvent.Remove(OnSelectionChangedHandle);
		OnSelectionChangedHandle.Reset();
	}

	// register
	ActorSelectionChangedEvent.AddRaw( this, &SConstraintsEditionWidget::OnActorSelectionChanged );
}

void SConstraintsEditionWidget::UnregisterSelectionChanged()
{
	if (OnSelectionChangedHandle.IsValid())
	{
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnActorSelectionChanged().Remove(OnSelectionChangedHandle);
		OnSelectionChangedHandle.Reset();
	}
}

void SConstraintsEditionWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if(bNeedsRefresh)
	{
		RefreshConstraintList();
		bNeedsRefresh = false;
	}
}

TSharedRef<ITableRow> SConstraintsEditionWidget::OnGenerateWidgetForItem(
	ItemSharedPtr InItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<ItemSharedPtr>, OwnerTable)
	.Style(&FAppStyle::Get(), "PlacementBrowser.PlaceableItemRow")
	[
		SNew(SEditableConstraintItem, InItem.ToSharedRef(), SharedThis(this))
	];
}

bool SConstraintsEditionWidget::CanMoveUp(const TSharedPtr<FEditableConstraintItem>& Item) const
{
	const int32 NumItems = ListItems.Num();
	if (NumItems < 2)
	{
		return false;
	}
	
	return ListItems[0] != Item; 
}

bool SConstraintsEditionWidget::CanMoveDown(const TSharedPtr<FEditableConstraintItem>& Item) const
{
	const int32 NumItems = ListItems.Num();
	if (NumItems < 2)
	{
		return false;
	}
	
	return ListItems.Last() != Item;
}

void SConstraintsEditionWidget::MoveItemUp(const TSharedPtr<FEditableConstraintItem>& Item)
{
	if (!CanMoveUp(Item))
	{
		return;
	}
	
	const int32 Index = ListItems.IndexOfByKey(Item);
	if (Index > 0 && ListItems.IsValidIndex(Index))
	{
		UWorld* World = GetCurrentWorld();
		if (!IsValid(World))
		{
			return;
		}
		
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);

		// the current item needs to tick before the previous item 
		const FName BeforeName(ListItems[Index]->Name.ToString());
		const FName AfterName(ListItems[Index-1]->Name.ToString());
		Controller.SetConstraintsDependencies(BeforeName, AfterName);
		
		RefreshConstraintList();
	}
}

void SConstraintsEditionWidget::MoveItemDown(const TSharedPtr<FEditableConstraintItem>& Item)
{
	if (!CanMoveDown(Item))
	{
		return;
	}
	
	const int32 Index = ListItems.IndexOfByKey(Item);
	if (ListItems.IsValidIndex(Index))
	{
		UWorld* World = GetCurrentWorld();
		if (!IsValid(World))
		{
			return;
		}
		
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);

		// the current item needs to tick after the next item 
		const FName BeforeName(ListItems[Index+1]->Name.ToString());
		const FName AfterName(ListItems[Index]->Name.ToString());
		Controller.SetConstraintsDependencies(BeforeName, AfterName);
		
		RefreshConstraintList();
	}
}

void SConstraintsEditionWidget::RemoveItem(const TSharedPtr<FEditableConstraintItem>& Item)
{
	UWorld* World = GetCurrentWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	FScopedTransaction Transaction(LOCTEXT("RemoveConstraint", "Remove Constraint"));
	
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
	
	const FName ConstraintName(Item->Name.ToString());
	Controller.RemoveConstraint(ConstraintName);

	RefreshConstraintList();
}

void SConstraintsEditionWidget::PostUndo(bool bSuccess)
{
	InvalidateConstraintList();
}

void SConstraintsEditionWidget::PostRedo(bool bSuccess)
{
	InvalidateConstraintList();
}

void SConstraintsEditionWidget::InvalidateConstraintList()
{
	bNeedsRefresh = true;

	// NOTE: this is a hack to disable the picker if the selection changes. The picker should trigger the escape key
	// but this is an editor wide change
	static const FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");
	if (ActorPickerMode.IsInActorPickingMode())
	{
		ActorPickerMode.EndActorPickingMode();
	}
}

int32 SConstraintsEditionWidget::RefreshConstraintList()
{
	// get constraints
	UWorld* World = GetCurrentWorld();
	if (!IsValid(World))
	{
		return 0;
	}
	
	const TArray<AActor*> Selection = GetCurrentSelection();
	
	const bool bIsConstraintsActor = Selection.Num() == 1 && Selection[0]->IsA<AConstraintsActor>();
	
	TArray< TObjectPtr<UTickableConstraint> > Constraints;
	if (bIsConstraintsActor)
	{
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		static constexpr bool bSorted = true;
		Constraints = Controller.GetAllConstraints(bSorted);
	}
	else
	{
		for (const AActor* Actor : Selection)
		{
			FTransformConstraintUtils::GetParentConstraints(World, Actor, Constraints);
		}
	}
	
	// rebuild item list
	ListItems.Empty();
	
	const UEnum* ETransformConstraintTypeEnum = StaticEnum<ETransformConstraintType>();
	for (const TObjectPtr<UTickableConstraint>& Constraint: Constraints)
	{
		const int8 Type = FConstraintInfo::GetType(Constraint->GetClass()); 
		if (ETransformConstraintTypeEnum->IsValidEnumValue(Type))
		{
			const ETransformConstraintType ConstraintType = static_cast<ETransformConstraintType>(Type);
			ListItems.Emplace( FEditableConstraintItem::Make(Constraint->GetFName(), ConstraintType, Constraint->GetLabel()) );
		}
	}

	// refresh tree
	ListView->RequestListRefresh();
	
	return ListItems.Num();
}

void SConstraintsEditionWidget::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	// NOTE we use this delegate to trigger an tree update, however, control actors are not selected as they are 
	// Temporary Editor Actors so NewSelection won't contain the controls
	InvalidateConstraintList();
}

TSharedPtr<SWidget> SConstraintsEditionWidget::CreateContextMenu()
{
	const TArray<TSharedPtr<FEditableConstraintItem>> Selection = ListView->GetSelectedItems();
	if (Selection.IsEmpty())
	{
		return SNullWidget::NullWidget;
	}

	const int32 Index = ListItems.IndexOfByKey(Selection[0]);
	if (!ListItems.IsValidIndex(Index))
	{
		return SNullWidget::NullWidget;
	}

	UWorld* World = GetCurrentWorld();
	if (!IsValid(World))
	{
		return SNullWidget::NullWidget;
	}
	
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
	UTickableConstraint* Constraint = Controller.GetConstraint(ListItems[Index]->Name);
	if (!Constraint)
	{
		return SNullWidget::NullWidget;
	}

	static constexpr bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder(CloseAfterSelection, nullptr);

	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bCustomFilterAreaLocation = true;
		DetailsViewArgs.bCustomNameAreaLocation = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.ColumnWidth = 0.45f;
	}
	
	TSharedRef<IDetailsView> DetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);

	// manage properties visibility
	const bool bIsLookAtConstraint = Constraint->GetClass() == UTickableLookAtConstraint::StaticClass();
	FIsPropertyVisible PropertyVisibility = FIsPropertyVisible::CreateLambda(
		[bIsLookAtConstraint](const FPropertyAndParent& InPropertyAndParent)
		{
			if (!bIsLookAtConstraint)
			{
				return true;
			}

			// hide offset properties for look at constraints
			static const FName MaintainOffsetPropName = GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, bMaintainOffset);
			static const FName DynamicOffsetPropName = GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, bDynamicOffset);
						
			const FName PropertyName = InPropertyAndParent.Property.GetFName();
			if (PropertyName == MaintainOffsetPropName || PropertyName == DynamicOffsetPropName)
			{
				return false;
			}

			return true;
		});
	DetailsView->SetIsPropertyVisibleDelegate(PropertyVisibility);
	
	TArray<TWeakObjectPtr<UObject>> ConstrainsToEdit;
	ConstrainsToEdit.Add(Constraint);
	
	DetailsView->SetObjects(ConstrainsToEdit);

	// constraint details
	MenuBuilder.BeginSection("EditConstraint", LOCTEXT("EditConstraintHeader", "Edit Constraint"));
	{
		// this spacer is used to set a minimum size to the details builder so that the menu won't shrink when
		// collapsing the transform offset for instance
		static const FVector2D MinimumDetailsSize(300.0f, 0.0f);
		MenuBuilder.AddWidget(SNew(SSpacer).Size(MinimumDetailsSize), FText::GetEmpty(), false, false);
		
		MenuBuilder.AddWidget(DetailsView, FText::GetEmpty(), true);
	}
	MenuBuilder.EndSection();

	// transform constraint options
	if (UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint))
	{
		const FText ConstraintLabel = FText::FromName(ListItems[Index]->Name);
	
		// baking (note that this will probably be moved)
		MenuBuilder.BeginSection("BakeConstraint", LOCTEXT("BakeConstraintHeader", "Bake Constraint"));
		{
			MenuBuilder.AddMenuEntry(
			LOCTEXT("BakeConstraintLabel", "Bake"),
			FText::Format(LOCTEXT("BakeConstraintDoItTooltip", "Bake {0} transforms."), ConstraintLabel),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([Constraint, World]()
				{
					if (UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint))
					{
						const TWeakPtr<ISequencer> WeakSequencer = FBakingHelper::GetSequencer();
						if (!WeakSequencer.IsValid() || !WeakSequencer.Pin()->GetFocusedMovieSceneSequence())
						{
							return;
						}
						const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
						const UMovieScene* MovieScene = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene();
						if (!MovieScene)
						{
							return;
						}
						
						FScopedTransaction Transaction(LOCTEXT("BakeConstraint", "Bake Constraint"));
						FConstraintBaker::Bake(World, TransformConstraint, WeakSequencer.Pin(), TOptional<TArray<FFrameNumber>>());

					}
				})),
			NAME_None,
			EUserInterfaceActionType::Button);
		}
		MenuBuilder.EndSection();

		// keys section
		FCanExecuteAction IsCompensationEnabled = FCanExecuteAction::CreateLambda([TransformConstraint]()
		{
			return TransformConstraint->bDynamicOffset;
		});

		if (!bIsLookAtConstraint)
		{
			MenuBuilder.BeginSection("KeyConstraint", LOCTEXT("KeyConstraintHeader", "Keys"));
			{
				MenuBuilder.AddMenuEntry(
				LOCTEXT("CompensateKeyLabel", "Compensate Key"),
				FText::Format(LOCTEXT("CompensateKeyTooltip", "Compensate transform key for {0}."), ConstraintLabel),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([TransformConstraint]()
				{
					FConstraintChannelHelper::Compensate(TransformConstraint);
				}), IsCompensationEnabled),
				NAME_None,
				EUserInterfaceActionType::Button);

				MenuBuilder.AddMenuEntry(
				LOCTEXT("CompensateAllKeysLabel", "Compensate All Keys"),
				FText::Format(LOCTEXT("CompensateAllKeysTooltip", "Compensate all transform keys for {0}."), ConstraintLabel),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([TransformConstraint]()
				{
					FConstraintChannelHelper::Compensate(TransformConstraint, true);
				}), IsCompensationEnabled),
				NAME_None,
				EUserInterfaceActionType::Button);
			}
			MenuBuilder.EndSection();
		}
	}

	return MenuBuilder.MakeWidget();
}

void SConstraintsEditionWidget::OnItemDoubleClicked(ItemSharedPtr InItem)
{
	const int32 Index = ListItems.IndexOfByKey(InItem);
	if (!ListItems.IsValidIndex(Index))
	{
		return;
	}

	UWorld* World = GetCurrentWorld();
	if (!IsValid(World))
	{
		return;
	}
	
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
	UTickableConstraint* Constraint = Controller.GetConstraint(InItem->Name);
	if (!Constraint)
	{
		return;
	}

	Constraint->SetActive(!Constraint->Active);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
