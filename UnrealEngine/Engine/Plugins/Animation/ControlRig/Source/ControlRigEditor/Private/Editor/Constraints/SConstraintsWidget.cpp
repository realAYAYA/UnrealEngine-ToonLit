// Copyright Epic Games, Inc. All Rights Reserved.

#include "SConstraintsWidget.h"

#include "ActorPickerMode.h"
#include "Constraints/MovieSceneConstraintChannelHelper.h"
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
#include "ConstraintsManager.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/Input/NumericTypeInterface.h"
#include "FrameNumberDetailsCustomization.h"

#define LOCTEXT_NAMESPACE "SConstraintsWidget"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

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

const TArray< UTickableTransformConstraint* >& FConstraintInfo::GetMutableDefaults()
{
	static TArray< UTickableTransformConstraint* > MutableDefaults({
		GetMutableDefault<UTickableTranslationConstraint>(),
		GetMutableDefault<UTickableRotationConstraint>(),
		GetMutableDefault<UTickableScaleConstraint>(),
		GetMutableDefault<UTickableParentConstraint>(),
		GetMutableDefault<UTickableLookAtConstraint>()});
	return MutableDefaults;
}

const TArray< TFunction<UTickableTransformConstraint*()> >& FConstraintInfo::GetConfigurableConstraints()
{
	static TArray< TFunction<UTickableTransformConstraint*()> > ConfigurableConstraints({
		[](){ return NewObject<UTickableTranslationConstraint>(GetTransientPackage(), NAME_None, RF_Transactional, GetMutableDefault<UTickableTranslationConstraint>(), true); },
		[](){ return NewObject<UTickableRotationConstraint>(GetTransientPackage(), NAME_None, RF_Transactional, GetMutableDefault<UTickableRotationConstraint>(), true); },
		[](){ return NewObject<UTickableScaleConstraint>(GetTransientPackage(), NAME_None, RF_Transactional, GetMutableDefault<UTickableScaleConstraint>(), true); },
		[](){ return NewObject<UTickableParentConstraint>(GetTransientPackage(), NAME_None, RF_Transactional, GetMutableDefault<UTickableParentConstraint>(), true); },
		[](){ return NewObject<UTickableLookAtConstraint>(GetTransientPackage(), NAME_None, RF_Transactional, GetMutableDefault<UTickableLookAtConstraint>(), true); },
	});
	return ConfigurableConstraints;
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

UTickableTransformConstraint* FConstraintInfo::GetMutable(ETransformConstraintType InType)
{
	static const UEnum* ETransformConstraintTypeEnum = StaticEnum<ETransformConstraintType>();
	const uint8 ConstraintType = static_cast<uint8>(InType);
	if (ETransformConstraintTypeEnum->IsValidEnumValue(ConstraintType))
	{
		return GetMutableDefaults()[ConstraintType];
	}
	return nullptr;
}

UTickableTransformConstraint* FConstraintInfo::GetConfigurable(ETransformConstraintType InType)
{
	static const UEnum* ETransformConstraintTypeEnum = StaticEnum<ETransformConstraintType>();
	const uint8 ConstraintType = static_cast<uint8>(InType);
	if (ETransformConstraintTypeEnum->IsValidEnumValue(ConstraintType))
	{
		return GetConfigurableConstraints()[ConstraintType]();
	}
	return nullptr;
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

	return ObjectPtrDecay(CurrentLevel->Actors).FilterByPredicate( [](const AActor* Actor)
	{
		return IsValid(Actor) && Actor->IsSelected();
	});	
}

static TWeakPtr<ISequencer> GetSequencerChecked()
{
	const TWeakPtr<ISequencer> WeakSequencer = FBakingHelper::GetSequencer();
	if (!WeakSequencer.IsValid() || !WeakSequencer.Pin()->GetFocusedMovieSceneSequence())
	{
		return nullptr;
	}
	
	const UMovieScene* MovieScene = WeakSequencer.Pin()->GetFocusedMovieSceneSequence()->GetMovieScene();
	if (!MovieScene)
	{
		return nullptr;
	}

	return WeakSequencer;
}
	
}

/**
 * SConstraintItem
 */

void SConstraintMenuEntry::Construct(
		const FArguments& InArgs,
		const ETransformConstraintType& InType)
{
	ConstraintType = InType;
	OnConstraintCreated = InArgs._OnConstraintCreated;
	
	const FButtonStyle& ButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>( "Menu.Button" );

	// enum to string
	const UEnum* ETransformConstraintTypeEnum = StaticEnum<ETransformConstraintType>();
	const FString TypeStr = ETransformConstraintTypeEnum->GetNameStringByValue((uint8)ConstraintType);

	// tooltip
	const FString ToolTipStr = FString::Printf(TEXT("Create new %s constraint."), *TypeStr);
	const TSharedPtr<IToolTip> ToolTip = FSlateApplicationBase::Get().MakeToolTip(FText::FromString(ToolTipStr));

	const float MenuIconSize = FAppStyle::Get().GetFloat("Menu.MenuIconSize");

	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew( SHorizontalBox )

		+ SHorizontalBox::Slot()
		[
			SNew(SBorder)
			.BorderImage_Lambda( [this, &ButtonStyle]
			{
				if (bIsPressed) { return &ButtonStyle.Pressed; }
				if (IsHovered()) { return &ButtonStyle.Hovered; }
				return &ButtonStyle.Normal;
			})
			.ForegroundColor(FLinearColor::White)
			.ToolTip( ToolTip )
			[
				SNew( SHorizontalBox )

				+ SHorizontalBox::Slot()
				.Padding(14.0f, 0.f, 10.f, 0.0f)
				.AutoWidth()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(MenuIconSize)
					.HeightOverride(MenuIconSize)
					[
						SNew(SImage)
						.Image(FConstraintInfo::GetBrush((uint8)ConstraintType))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.Padding(1.f, 0.f, 25.f, 0.f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew( STextBlock )
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Text_Lambda( [TypeStr] { return FText::FromString(TypeStr); } )
				]
			]
		]
		
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Right)
		.Padding(FMargin(0.f, 1.f, 0.f, 1.f))
		.AutoWidth()
		[
			SNew(SComboButton)
			.ComboButtonStyle(FControlRigEditorStyle::Get(), "ConstraintManager.ComboButton")
			.MenuContent()
			[
				GenerateConstraintDefaultWidget()
			]
		]
	];
}

FReply SConstraintMenuEntry::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsPressed = true;
		static constexpr bool bUseMutableDefault = false;
		return CreateSelectionPicker(bUseMutableDefault);
	}

	return FReply::Unhandled();
}

TSharedRef<SWidget> SConstraintMenuEntry::GenerateConstraintDefaultWidget()
{
	// static constexpr bool CloseAfterSelection = true;
	// FMenuBuilder MenuBuilder(CloseAfterSelection, nullptr);

	static FDetailsViewArgs DetailsViewArgs;
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
		DetailsViewArgs.NotifyHook = this;
	}
	
	TSharedRef<IDetailsView> DetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);

	ConfigurableConstraint = FConstraintInfo::GetConfigurable(ConstraintType);
	
	// manage properties visibility
	const bool bIsLookAtConstraint = ConfigurableConstraint->GetClass() == UTickableLookAtConstraint::StaticClass();
	const auto PropertyVisibility = FIsPropertyVisible::CreateLambda(
		[bIsLookAtConstraint](const FPropertyAndParent& InPropertyAndParent)
		{
			const FName PropertyName = InPropertyAndParent.Property.GetFName();
			
			// hide active property
			static const FName ActivePropName = GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, Active);
			if (PropertyName == ActivePropName)
			{
				return false;
			}

			// hide offset properties for look at constraints
			if (bIsLookAtConstraint)
			{
				static const FName MaintainOffsetPropName = GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, bMaintainOffset);
				static const FName DynamicOffsetPropName = GET_MEMBER_NAME_CHECKED(UTickableTransformConstraint, bDynamicOffset);
				if (PropertyName == MaintainOffsetPropName || PropertyName == DynamicOffsetPropName)
				{
					return false;
				}
			}

			return true;
		});
	DetailsView->SetIsPropertyVisibleDelegate(PropertyVisibility);
		
	DetailsView->SetObject(ConfigurableConstraint);

	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
			.AutoHeight()
			.VAlign( VAlign_Center )
			[
				DetailsView
			]
			
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding( 0.0f, 1.0f )
			.VAlign( VAlign_Center )
			[
				SNew( SHorizontalBox )

				+ SHorizontalBox::Slot()
				.FillWidth(1.f)
				.HAlign(HAlign_Fill)
				[
					SNew(SSpacer)
				]
		
				+SHorizontalBox::Slot()
				.Padding(16.f, 1.f)
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Text(LOCTEXT("CreateButtonLabel", "Create"))
					.OnClicked_Lambda([this]()
					{
						static constexpr bool bUseMutableDefault = true;
						return CreateSelectionPicker(bUseMutableDefault);
					})
				]
			]
		];
}

FReply SConstraintMenuEntry::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if ( MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		bIsPressed = false;
	}

	return FReply::Unhandled();
}

FReply SConstraintMenuEntry::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsPressed = false;
	return FReply::Handled();
}

void SConstraintMenuEntry::NotifyPostChange(const FPropertyChangedEvent& InPropertyChangedEvent, FProperty* InPropertyThatChanged)
{
	if (!InPropertyThatChanged || InPropertyChangedEvent.ChangeType != EPropertyChangeType::ValueSet)
	{
		return;
	}

	UTickableTransformConstraint* MutableConstraint = FConstraintInfo::GetMutable(ConstraintType);
	if (!IsValid(ConfigurableConstraint) || !IsValid(MutableConstraint))
	{
		return;
	}

	const UClass* ConstraintClass = MutableConstraint->GetClass();
	
	if (const FProperty* CDOProperty = ConstraintClass->FindPropertyByName(InPropertyChangedEvent.GetPropertyName()))
	{
		// copy the property
		CDOProperty->CopyCompleteValue_InContainer(MutableConstraint, ConfigurableConstraint.Get());
	}
	else if (InPropertyChangedEvent.MemberProperty)
	{
		// copy the member property instead
		if (const FProperty* CDOMemberProperty = ConstraintClass->FindPropertyByName(InPropertyChangedEvent.GetMemberPropertyName()))
		{
			CDOMemberProperty->CopyCompleteValue_InContainer(MutableConstraint, ConfigurableConstraint.Get());
		}
	}
}

FReply SConstraintMenuEntry::CreateSelectionPicker(const bool bUseDefault) const
{
	// FIXME temp approach for selecting the parent
	FSlateApplication::Get().DismissAllMenus();
	
	static const FActorPickerModeModule& ActorPickerMode = FModuleManager::Get().GetModuleChecked<FActorPickerModeModule>("ActorPickerMode");

	ActorPickerMode.BeginActorPickingMode(
		FOnGetAllowedClasses(), 
		FOnShouldFilterActor(), 
		FOnActorSelected::CreateLambda([CreationDelegate = OnConstraintCreated, Type = ConstraintType, bUseDefault](AActor* InActor)
		{
			CreateConstraint(InActor, CreationDelegate, Type, bUseDefault);
		}) );
	
	return FReply::Handled();
}

void SConstraintMenuEntry::CreateConstraint(
	AActor* InParent,
	FOnConstraintCreated InCreationDelegate,
	const ETransformConstraintType InConstraintType,
	const bool bUseDefault)
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

	// gather sub components with sockets
	const TInlineComponentArray<USceneComponent*> Components(InParent);
	const TArray<USceneComponent*> ComponentsWithSockets = Components.FilterByPredicate([](const USceneComponent* Component)
	{
		return Component->HasAnySockets();
	});

	const TWeakPtr<ISequencer> WeakSequencer = GetSequencerChecked();
	
	// create constraints
	auto CreateConstraint = [InCreationDelegate, InConstraintType, WeakSequencer, bUseDefault](
		const TArray<AActor*>& Selection, UObject* InParent, const FName& InSocketName)
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
					FTransformConstraintUtils::CreateAndAddFromObjects(World, InParent, InSocketName, Child, NAME_None, InConstraintType, true, bUseDefault);
				if (Constraint)
				{
					bCreated = true;
					if (WeakSequencer.IsValid())
					{
						if (bUseDefault)
						{
							Constraint->Evaluate();
						}
						FMovieSceneConstraintChannelHelper::AddConstraintToSequencer(WeakSequencer.Pin(), Constraint);
					}
					else
					{
						FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
						Controller.StaticConstraintCreated(World, Constraint);
					}		
				}
			}
		}
	
		// update list
		if (bCreated && InCreationDelegate.IsBound())
		{
			InCreationDelegate.Execute();
		}
	};

	const int32 NumComponentsWithSockets = ComponentsWithSockets.Num();

	// if no component socket available then constrain the whole actor 
	if (NumComponentsWithSockets == 0)
	{
		return CreateConstraint(Selection, InParent, NAME_None);
	}

	// creates a menu encapsulating InContent  
	auto CreateMenu = [](const TSharedRef<SWidget>& InContent, const FVector2D& InLocation)
	{
		const FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		const TSharedPtr< ILevelEditor > LevelEditor = LevelEditorModule.GetFirstLevelEditor();
		FSlateApplication::Get().PushMenu(
			LevelEditor.ToSharedRef(),
			FWidgetPath(),
			InContent,
		InLocation,
		FPopupTransitionEffect( FPopupTransitionEffect::ContextMenu ) );
	};

	// creates a new socket chooser popup widget
	auto GetSocketChooserWidget = [CreateConstraint, Selection](USceneComponent* Component)
	{
		return SNew(SSocketChooserPopup)
		.SceneComponent( Component )
		.OnSocketChosen_Lambda([CreateConstraint, Selection, Component](FName InSocketName)
		{
			CreateConstraint(Selection, Component, InSocketName);
		});
	};

	// decal the menu 
	FVector2D SummonLocation = FSlateApplication::Get().GetCursorPos();
	SummonLocation.Y += 4 * FSlateApplication::Get().GetCursorSize().Y;
	
	// if one component with sockets then constrain the component selecting the socket
	if (NumComponentsWithSockets == 1)
	{
		return CreateMenu( GetSocketChooserWidget(ComponentsWithSockets[0]), SummonLocation);
	}

	// if there are several of them, then build a component chooser first then the socket chooser
	static constexpr bool CloseAfterSelection = true;
	FMenuBuilder MenuBuilder(CloseAfterSelection, nullptr);
	MenuBuilder.BeginSection("ChooseComp", LOCTEXT("ChooseComponentSection", "Choose Component"));
	{
		for (USceneComponent* Component: ComponentsWithSockets)
		{
			MenuBuilder.AddMenuEntry(FText::FromName(Component->GetFName()),
			FText(),
				FSlateIconFinder::FindIconForClass(Component->GetClass(), TEXT("SCS.Component")),
				FUIAction(FExecuteAction::CreateLambda([CreateMenu, GetSocketChooserWidget, Component]()
				{
					CreateMenu(GetSocketChooserWidget(Component), FSlateApplication::Get().GetCursorPos());
				})),
				NAME_None,
			   EUserInterfaceActionType::Button);
		}
	}
	MenuBuilder.EndSection();
	CreateMenu(MenuBuilder.MakeWidget(), SummonLocation);
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
		UTickableConstraint* Constraint = ConstraintItem->Constraint.Get();
		int32 Index = Controller.GetConstraintsArray().Find(Constraint);
		if (Index == INDEX_NONE)
		{
			return nullptr;
		}
		return Constraint;
	};
	TWeakObjectPtr<UTickableConstraint> Constraint = GetConstraint();

	UWorld* World = GetCurrentWorld();

	// sequencer
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerChecked();

	//manager for static check
	UConstraintsManager* Manager = UConstraintsManager::Find(World);

	// labels
	FString ParentLabel(TEXT("undefined")), ChildLabel(TEXT("undefined"));
	FString ParentFullLabel = ParentLabel, ChildFullLabel = ChildLabel;
	if (IsValid(Constraint.Get()))
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
			.BorderBackgroundColor_Lambda([Manager, World,Constraint]()
			{
				if (!Constraint.IsValid() || !IsValid(Constraint.Get()))
				{
					return FStyleColors::Transparent;
				}
				if (Manager && Manager->IsStaticConstraint(Constraint.Get()))
				{
					return FSlateColor(FLinearColor::Green * 0.5);
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
					.Text_Lambda( [Constraint, ParentLabel, ChildLabel,World]()
					{
							FString CurrentParentLabel = ParentLabel, CurrentChildLabel = ChildLabel;
							if (Constraint.IsValid() && IsValid(Constraint.Get()) && Constraint->IsValid())
							{
								FString Label = Constraint->GetLabel();
								Label.Split(TEXT("."), &CurrentParentLabel, &CurrentChildLabel);
							}
							static constexpr TCHAR LabelFormat[] = TEXT("%s <-- %s");
							const FString Label = FString::Printf(LabelFormat, *CurrentParentLabel, *CurrentChildLabel);
							return FText::FromString(Label);
					})
					.Font_Lambda([Constraint]()
					{
						if (!Constraint.IsValid() || !IsValid(Constraint.Get()))
						{
							return IDetailLayoutBuilder::GetDetailFont();
						}
						return Constraint->Active ? IDetailLayoutBuilder::GetDetailFont() : IDetailLayoutBuilder::GetDetailFontItalic();
					})
					.ToolTipText_Lambda( [Constraint, ParentFullLabel, ChildFullLabel]()
					{
						if (!Constraint.IsValid() || !IsValid(Constraint.Get()))
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
			.OnClicked_Lambda(	[Constraint, WeakSequencer]()
			{
				if (!Constraint.IsValid() || !IsValid(Constraint.Get()))
				{
					return FReply::Handled();
				}

				if (!WeakSequencer.IsValid())
				{
					return FReply::Handled();
				}
				
				if (UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint))
				{
					FScopedTransaction Transaction(LOCTEXT("CreateConstraintKey", "Create Constraint Key"));
					FMovieSceneConstraintChannelHelper::SmartConstraintKey(WeakSequencer.Pin(), TransformConstraint,TOptional<bool>(), TOptional<FFrameNumber>());
				}
				return FReply::Handled();
			})
			.IsEnabled_Lambda([WeakSequencer]()
			{
				return WeakSequencer.IsValid();
			})
			.Visibility_Lambda([WeakSequencer]()
			{
				const bool bIsKeyframingAvailable = WeakSequencer.IsValid();
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

//////////////////////////////////////////////////////////////
/// FBaseConstraintListWidget
///////////////////////////////////////////////////////////


FBaseConstraintListWidget::~FBaseConstraintListWidget()
{
	UnregisterSelectionChanged();
}

void FBaseConstraintListWidget::RegisterSelectionChanged()
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
	OnSelectionChangedHandle = ActorSelectionChangedEvent.AddRaw(this, &FBaseConstraintListWidget::OnActorSelectionChanged);
}

void FBaseConstraintListWidget::UnregisterSelectionChanged()
{
	if (OnSelectionChangedHandle.IsValid())
	{
		FLevelEditorModule& LevelEditor = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
		LevelEditor.OnActorSelectionChanged().Remove(OnSelectionChangedHandle);
		OnSelectionChangedHandle.Reset();
	}
}

void FBaseConstraintListWidget::PostUndo(bool bSuccess)
{
	InvalidateConstraintList();
}

void FBaseConstraintListWidget::PostRedo(bool bSuccess)
{
	InvalidateConstraintList();
}

void FBaseConstraintListWidget::InvalidateConstraintList()
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

FBaseConstraintListWidget::EShowConstraints FBaseConstraintListWidget::ShowConstraints = FBaseConstraintListWidget::EShowConstraints::ShowSelected;

FText FBaseConstraintListWidget::GetShowConstraintsText(EShowConstraints Index) const
{
	FText Text = FText::GetEmpty();
	switch (Index)
	{
		case EShowConstraints::ShowSelected:
		{
			Text = FText(LOCTEXT("Selected", "Selected"));
		}
		break;
		case EShowConstraints::ShowLevelSequence:
		{
			Text = FText(LOCTEXT("LevelSequence", "Level Sequence"));
		}
		break;
		case EShowConstraints::ShowValid:
		{
			Text = FText(LOCTEXT("Current", "Current"));
		}
		break;
		case EShowConstraints::ShowAll:
		{
			Text = FText(LOCTEXT("All", "All"));
		}
		break;
	}
	return Text;
}

FText FBaseConstraintListWidget::GetShowConstraintsTooltip(EShowConstraints Index) const
{
	FText Text = FText::GetEmpty();
	switch (Index)
	{
		case EShowConstraints::ShowSelected:
		{
			Text = FText(LOCTEXT("SelectedTooltip", "Show constraints on selected"));
		}
		break;
		case EShowConstraints::ShowLevelSequence:
		{
			Text = FText(LOCTEXT("LevelSequenceTooltip", "Show constraints on active level sequence"));
		}
		break;
		case EShowConstraints::ShowValid:
		{
			Text = FText(LOCTEXT("CurrentTooltip", "Show constraints that are current in level sequences or static"));
		}
		break;
		case EShowConstraints::ShowAll:
		{
			Text = FText(LOCTEXT("AllTooltip", "Show all constraints"));
		}
		break;
	}
	return Text;
}

int32 FBaseConstraintListWidget::RefreshConstraintList()
{
	// get constraints
	UWorld* World = GetCurrentWorld();
	if (!IsValid(World))
	{
		return 0;
	}

	const TArray<AActor*> Selection = GetCurrentSelection();

	const bool bIsConstraintsActor = Selection.Num() == 1 && Selection[0]->IsA<AConstraintsActor>();

	TArray< TWeakObjectPtr<UTickableConstraint> > Constraints;
	if (ShowConstraints == EShowConstraints::ShowSelected)
	{
		if (bIsConstraintsActor)
		{
			const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
			static constexpr bool bSorted = true;
			TArray< TObjectPtr<UTickableConstraint> > StaticConstraints;

			StaticConstraints = Controller.GetStaticConstraints(bSorted);
			for (TObjectPtr<UTickableConstraint>& Constraint : StaticConstraints)
			{
				Constraints.Add(Constraint);
			}
		}
		else
		{
			for (const AActor* Actor : Selection)
			{
				FTransformConstraintUtils::GetParentConstraints(World, Actor, Constraints);
			}
			//remove if not active...
			for (int32 Index = Constraints.Num() - 1; Index >= 0; --Index)
			{
				if (Constraints[Index].Get() == nullptr || Constraints[Index].Get()->IsValid() == false)
				{
					Constraints.RemoveAt(Index);
				}
			}
		}
	}
	else
	{
		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		static constexpr bool bSorted = true;
		Constraints = Controller.GetAllConstraints(bSorted);
		if (ShowConstraints == EShowConstraints::ShowLevelSequence)
		{
			TWeakPtr<ISequencer> WeakSequencer = GetSequencerChecked();
			if (WeakSequencer.IsValid())
			{
				UMovieScene* MovieScene = WeakSequencer.Pin()->GetFocusedMovieSceneSequence()->GetMovieScene();
				for (int32 Index = Constraints.Num() - 1; Index >= 0; --Index)
				{
					UMovieScene* OuterMovieScene = Constraints[Index].Get() ? Constraints[Index]->GetTypedOuter<UMovieScene>() : nullptr;
					if (OuterMovieScene != MovieScene)
					{
						Constraints.RemoveAt(Index);
					}
				}
			}
			else
			{
				Constraints.Empty();
			}
		}
		else if (ShowConstraints == EShowConstraints::ShowValid)
		{
			for (int32 Index = Constraints.Num() - 1; Index >= 0; --Index)
			{
				if (Constraints[Index].Get() == nullptr || Constraints[Index].Get()->IsValid() == false)
				{
					Constraints.RemoveAt(Index);
				}
			}
		}
	}

	// rebuild item list
	ListItems.Empty();

	const UEnum* ETransformConstraintTypeEnum = StaticEnum<ETransformConstraintType>();
	for (const TWeakObjectPtr<UTickableConstraint>& Constraint : Constraints)
	{
		const int8 Type = FConstraintInfo::GetType(Constraint->GetClass());
		if (ETransformConstraintTypeEnum->IsValidEnumValue(Type))
		{
			const ETransformConstraintType ConstraintType = static_cast<ETransformConstraintType>(Type);
			ListItems.Emplace(FEditableConstraintItem::Make(Constraint.Get(), ConstraintType));
		}
	}

	// refresh tree
	ListView->RequestListRefresh();

	return ListItems.Num();
}

void FBaseConstraintListWidget::OnActorSelectionChanged(const TArray<UObject*>& NewSelection, bool bForceRefresh)
{
	// NOTE we use this delegate to trigger an tree update, however, control actors are not selected as they are 
	// Temporary Editor Actors so NewSelection won't contain the controls
	InvalidateConstraintList();
}


//////////////////////////////////////////////////////////////
/// SConstraintsEditionWidget
///////////////////////////////////////////////////////////


void SConstraintsEditionWidget::Construct(const FArguments& InArgs)
{
	WeakSequencer = GetSequencerChecked();

	ChildSlot
	[
		SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[

			SNew( SVerticalBox )

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 0.f))
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
	
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 0.f))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.HAlign(HAlign_Fill)
					[
						SNew(SSpacer)
					]
		
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Right)
					.Padding(1.f)
					[
						SNew(SButton)
						.ButtonStyle(FAppStyle::Get(), "FlatButton.Default")
						.Text(LOCTEXT("BakeButton", "Bake..."))
						.OnClicked(this, &SConstraintsEditionWidget::OnBakeClicked)
						.IsEnabled_Lambda([this]()
						{
							return (ListItems.Num() > 0);
						})
						.ToolTipText(LOCTEXT("BakeConstraintButtonToolTip", "Bake selected contraints"))
					]
				]
		]
	];

	RefreshConstraintList();
	RegisterSelectionChanged();
}

bool SConstraintsEditionWidget::SequencerTimeChanged() 
{
	if (WeakSequencer.IsValid())
	{
		TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
		const bool bTimeChanged = (SequencerLastTime.Rate != Sequencer->GetLocalTime().Rate ||
			SequencerLastTime.Time != Sequencer->GetLocalTime().Time);
		SequencerLastTime = Sequencer->GetLocalTime();
		return bTimeChanged;
	}
	return false;
}

void SConstraintsEditionWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if(bNeedsRefresh || SequencerTimeChanged())
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
		const FName BeforeName(ListItems[Index]->GetName().ToString());
		const FName AfterName(ListItems[Index-1]->GetName().ToString());
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
		const FName BeforeName(ListItems[Index+1]->GetName().ToString());
		const FName AfterName(ListItems[Index]->GetName().ToString());
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
	
	FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
	
	Controller.RemoveConstraint(Item->Constraint.Get());

	RefreshConstraintList();
}

void SConstraintsEditionWidget::InvalidateConstraintList()
{
	FBaseConstraintListWidget::InvalidateConstraintList();
	UpdateSequencer();
}

void SConstraintsEditionWidget::SequencerChanged(const TWeakPtr<ISequencer>& InNewSequencer)
{
	if (WeakSequencer != InNewSequencer)
	{
		WeakSequencer = InNewSequencer;
		InvalidateConstraintList();
	}
}

void SConstraintsEditionWidget::UpdateSequencer()
{
	if (!WeakSequencer.IsValid())
	{
		WeakSequencer = GetSequencerChecked();
	}
}

FReply SConstraintsEditionWidget::OnBakeClicked()
{
	UpdateSequencer();
	if (!WeakSequencer.IsValid() || ListItems.Num() < 1)
	{
		return FReply::Unhandled();
	}
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	TSharedRef<SConstraintBakeWidget> BakeWidget =
		SNew(SConstraintBakeWidget)
		.Sequencer(Sequencer);

	return BakeWidget->OpenDialog(true);
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

	UpdateSequencer();
	
	const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
	UTickableConstraint* Constraint = ListItems[Index]->Constraint.Get();
	int32 ContIndex = Controller.GetConstraintsArray().Find(Constraint);
	if (ContIndex == INDEX_NONE)
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

	const UConstraintsManager* Manager = UConstraintsManager::Find(World);
	const bool bIsStatic = Manager ? Manager->IsStaticConstraint(Constraint) : false;
	auto IsPropertyReadOnly = [bIsStatic](const FProperty* InProperty)
	{
		if (InProperty && InProperty->HasAnyPropertyFlags(CPF_DisableEditOnInstance))
		{
			return !bIsStatic;
		}
		return false;
	};
	
	FIsPropertyReadOnly PropertyReadonly = FIsPropertyReadOnly::CreateLambda([IsPropertyReadOnly](const FPropertyAndParent& InPropertyAndParent)
	{
		if (IsPropertyReadOnly(&InPropertyAndParent.Property))
		{
			return true;
		}

		const bool bIsParentReadOnly = InPropertyAndParent.ParentProperties.ContainsByPredicate([IsPropertyReadOnly](const FProperty* Parent)
		{
			return IsPropertyReadOnly(Parent);
		});
		
		return bIsParentReadOnly;
	});
	DetailsView->SetIsPropertyReadOnlyDelegate(PropertyReadonly);
	
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
		const FText ConstraintLabel = FText::FromName(ListItems[Index]->GetName());
	
		// baking (note that this will probably be moved)
		MenuBuilder.BeginSection("BakeConstraint", LOCTEXT("BakeConstraintHeader", "Bake Constraint"));
		{
			MenuBuilder.AddMenuEntry(
			LOCTEXT("BakeConstraintLabel", "Bake"),
			FText::Format(LOCTEXT("BakeConstraintDoItTooltip", "Bake {0} transforms."), ConstraintLabel),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateLambda([this, Constraint, World]()
				{
					if (UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Constraint))
					{
						if (!WeakSequencer.IsValid())
						{
							return;
						}
						FScopedTransaction Transaction(LOCTEXT("BakeConstraint", "Bake Constraint"));
						FConstraintBaker::Bake(World, TransformConstraint, WeakSequencer.Pin(), TOptional< FBakingAnimationKeySettings>(), TOptional<TArray<FFrameNumber>>());

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

		if (!bIsLookAtConstraint && WeakSequencer.IsValid())
		{
			MenuBuilder.BeginSection("KeyConstraint", LOCTEXT("KeyConstraintHeader", "Keys"));
			{
				MenuBuilder.AddMenuEntry(
				LOCTEXT("CompensateKeyLabel", "Compensate Key"),
				FText::Format(LOCTEXT("CompensateKeyTooltip", "Compensate transform key for {0}."), ConstraintLabel),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([TransformConstraint, this]()
				{
					const TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
					const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
					const FFrameTime FrameTime = Sequencer->GetLocalTime().ConvertTo(TickResolution);
					const FFrameNumber Time = FrameTime.GetFrame();
					FMovieSceneConstraintChannelHelper::Compensate(WeakSequencer.Pin(), TransformConstraint, TOptional<FFrameNumber>(Time));
				}), IsCompensationEnabled),
				NAME_None,
				EUserInterfaceActionType::Button);

				MenuBuilder.AddMenuEntry(
				LOCTEXT("CompensateAllKeysLabel", "Compensate All Keys"),
				FText::Format(LOCTEXT("CompensateAllKeysTooltip", "Compensate all transform keys for {0}."), ConstraintLabel),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([TransformConstraint, this]()
				{
					FMovieSceneConstraintChannelHelper::Compensate(WeakSequencer.Pin(), TransformConstraint, TOptional<FFrameNumber>());
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
	UTickableConstraint* Constraint = InItem->Constraint.Get();
	int32 ContIndex = Controller.GetConstraintsArray().Find(Constraint);
	if (ContIndex != INDEX_NONE)
	{
		Constraint->SetActive(!Constraint->Active);
	}
}

//////////////////////////////////////////////////////////////
/// SConstraintBakeWidget
///////////////////////////////////////////////////////////

TOptional<FBakingAnimationKeySettings> SConstraintBakeWidget::BakeConstraintSettings;

void SConstraintBakeWidget::Construct(const FArguments& InArgs)
{
	check(InArgs._Sequencer);
	Sequencer = InArgs._Sequencer;

	if (BakeConstraintSettings.IsSet() == false)
	{
		BakeConstraintSettings = FBakingAnimationKeySettings();
		const FFrameRate TickResolution = Sequencer->GetFocusedTickResolution();
		const FFrameTime FrameTime = Sequencer->GetLocalTime().ConvertTo(TickResolution);
		FFrameNumber CurrentTime = FrameTime.GetFrame();

		TRange<FFrameNumber> Range = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->GetPlaybackRange();
		TArray<FFrameNumber> Keys;
		TArray < FKeyHandle> KeyHandles;

		BakeConstraintSettings.GetValue().StartFrame = Range.GetLowerBoundValue();
		BakeConstraintSettings.GetValue().EndFrame = Range.GetUpperBoundValue();
	}


	Settings = MakeShared<TStructOnScope<FBakingAnimationKeySettings>>();
	Settings->InitializeAs<FBakingAnimationKeySettings>(BakeConstraintSettings.GetValue());

	FStructureDetailsViewArgs StructureViewArgs;
	StructureViewArgs.bShowObjects = true;
	StructureViewArgs.bShowAssets = true;
	StructureViewArgs.bShowClasses = true;
	StructureViewArgs.bShowInterfaces = true;

	FDetailsViewArgs ViewArgs;
	ViewArgs.bAllowSearch = false;
	ViewArgs.bHideSelectionTip = false;
	ViewArgs.bShowObjectLabel = false;

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	DetailsView = PropertyEditor.CreateStructureDetailView(ViewArgs, StructureViewArgs, TSharedPtr<FStructOnScope>());
	TSharedPtr<INumericTypeInterface<double>> NumericTypeInterface = Sequencer->GetNumericTypeInterface();
	DetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout("FrameNumber",
		FOnGetPropertyTypeCustomizationInstance::CreateLambda([=]() {return MakeShared<FFrameNumberDetailsCustomization>(NumericTypeInterface); }));
	DetailsView->SetStructureData(Settings);


	ChildSlot
		[
			SNew(SBorder)
			.Visibility(EVisibility::Visible)
			[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(FMargin(0.0f, 0.f))
			[
				SAssignNew(ListView, ConstraintItemListView)
				.SelectionMode(ESelectionMode::Multi)
				.ListItemsSource(&ListItems)
				.OnGenerateRow(this, &SConstraintBakeWidget::OnGenerateWidgetForItem)

			]
			
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 8.f, 0.f, 0.f)
				[
					DetailsView->GetWidget().ToSharedRef()
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 16.f, 0.f, 8.f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					[
						SNew(SSpacer)
					]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(8.f, 0.f, 0.f, 0.f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
					.Text(LOCTEXT("OK", "OK"))
					.OnClicked_Lambda([this, InArgs]()
						{
							BakeSelected();
							CloseDialog();
							return FReply::Handled();

						})
						.IsEnabled_Lambda([this]()
						{
							return (ListItems.Num() > 0 && Settings.IsValid());
						})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(8.f, 0.f, 16.f, 0.f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.ContentPadding(FAppStyle::GetMargin("StandardDialog.ContentPadding"))
				.Text(LOCTEXT("Cancel", "Cancel"))
				.OnClicked_Lambda([this]()
					{
						CloseDialog();
					return FReply::Handled();
					})
				]
			]
		]
	];
	RefreshConstraintList();
	RegisterSelectionChanged();
}

TSharedRef<ITableRow> SConstraintBakeWidget::OnGenerateWidgetForItem(
	ItemSharedPtr InItem,
	const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<ItemSharedPtr>, OwnerTable)
		[
			SNew(SBakeConstraintItem, InItem.ToSharedRef(), SharedThis(this))
		];
		
}

void SConstraintBakeWidget::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bNeedsRefresh)
	{
		RefreshConstraintList();
		bNeedsRefresh = false;
	}
}

//for the bake we auto select averything when selection or something chagnes
int32 SConstraintBakeWidget::RefreshConstraintList()
{
	int32 Num = FBaseConstraintListWidget::RefreshConstraintList();
	ListView->SetItemSelection(ListItems, true, ESelectInfo::Direct);
	return Num;
}

void SConstraintBakeWidget::BakeSelected()
{
	TArray<TSharedPtr<FEditableConstraintItem>> Selection = ListView->GetSelectedItems();
	TArray<UTickableTransformConstraint*> Constraints;
	for (TSharedPtr<FEditableConstraintItem>& Item : Selection)
	{
		if (UTickableTransformConstraint* TransformConstraint = Cast<UTickableTransformConstraint>(Item->Constraint.Get()))
		{
			Constraints.Add(TransformConstraint);
		}
	}
	UWorld* World = GetCurrentWorld();

	FBakingAnimationKeySettings* BakeSettings = Settings->Get();
	
	FScopedTransaction Transaction(LOCTEXT("BakeConstraints", "Bake Constraints"));
	FConstraintBaker::BakeMultiple(World,Constraints, Sequencer,*BakeSettings );
	BakeConstraintSettings = *BakeSettings;
	InvalidateConstraintList();
}

class SConstraintBakedWidgetWindow : public SWindow
{
};



FReply SConstraintBakeWidget::OpenDialog(bool bModal)
{
	check(!DialogWindow.IsValid());

	const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();

	TSharedRef<SConstraintBakedWidgetWindow> Window = SNew(SConstraintBakedWidgetWindow)
		.Title(LOCTEXT("SConstraintsBakeWidgetTitle", "Bake Constraints"))
		.CreateTitleBar(true)
		.Type(EWindowType::Normal)
		.SizingRule(ESizingRule::Autosized)
		.ScreenPosition(CursorPos)
		.FocusWhenFirstShown(true)
		.ActivationPolicy(EWindowActivationPolicy::FirstShown)
		[
			AsShared()
		];

	Window->SetWidgetToFocusOnActivate(AsShared());

	DialogWindow = Window;

	Window->MoveWindowTo(CursorPos);

	if (bModal)
	{
		GEditor->EditorAddModalWindow(Window);
	}
	else
	{
		FSlateApplication::Get().AddWindow(Window);
	}

	return FReply::Handled();
}

void SConstraintBakeWidget::CloseDialog()
{
	if (DialogWindow.IsValid())
	{
		DialogWindow.Pin()->RequestDestroyWindow();
		DialogWindow.Reset();
	}
}

//////////////////////////////////////////////////////////////
/// SBakeConstraintItem
///////////////////////////////////////////////////////////


void SBakeConstraintItem::Construct(
	const FArguments& InArgs,
	const TSharedPtr<FEditableConstraintItem>& InItem,
	TSharedPtr<SConstraintBakeWidget> InConstraintsWidget)
{
	ConstraintItem = InItem;
	ConstraintsWidget = InConstraintsWidget;

	static const FSlateBrush* RoundedBoxBrush = FControlRigEditorStyle::Get().GetBrush(TEXT("ControlRig.SpacePicker.RoundedRect"));

	// enum to string
	const uint8 ConstraintType = static_cast<uint8>(InItem->Type);


	// constraint
	auto GetConstraint = [this]() -> UTickableConstraint*
	{
		UWorld* World = GetCurrentWorld();
		if (!IsValid(World))
		{
			return nullptr;
		}

		const FConstraintsManagerController& Controller = FConstraintsManagerController::Get(World);
		UTickableConstraint* Constraint = ConstraintItem->Constraint.Get();
		int32 Index = Controller.GetConstraintsArray().Find(Constraint);
		if (Index == INDEX_NONE)
		{
			return nullptr;
		}
		return Constraint;
	};
	TWeakObjectPtr<UTickableConstraint> Constraint = GetConstraint();

	// sequencer
	TWeakPtr<ISequencer> WeakSequencer = GetSequencerChecked();

	// labels
	FString ParentLabel(TEXT("undefined")), ChildLabel(TEXT("undefined"));
	

	FString ParentFullLabel = ParentLabel, ChildFullLabel = ChildLabel;
	if (IsValid(Constraint.Get()))
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
					return FStyleColors::Transparent;
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
						SNew(STextBlock)
						.Text_Lambda([Constraint, ParentLabel, ChildLabel]()
							{
								FString CurrentParentLabel = ParentLabel, CurrentChildLabel = ChildLabel;
								if (Constraint.IsValid() && IsValid(Constraint.Get()))
								{
									FString Label = Constraint->GetLabel();
									Label.Split(TEXT("."), &CurrentParentLabel, &CurrentChildLabel);
								}
								static constexpr TCHAR LabelFormat[] = TEXT("%s <-- %s");
								const FString Label = FString::Printf(LabelFormat, *CurrentParentLabel, *CurrentChildLabel);
								return FText::FromString(Label);
							})
					.Font_Lambda([Constraint]()
						{
							if (!Constraint.IsValid() || !IsValid(Constraint.Get()))
							{
								return IDetailLayoutBuilder::GetDetailFont();
							}
							return Constraint->Active ? IDetailLayoutBuilder::GetDetailFont() : IDetailLayoutBuilder::GetDetailFontItalic();
						})
								.ToolTipText_Lambda([Constraint, ParentFullLabel, ChildFullLabel]()
									{
										if (!Constraint.IsValid() || !IsValid(Constraint.Get()))
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

	
		];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
