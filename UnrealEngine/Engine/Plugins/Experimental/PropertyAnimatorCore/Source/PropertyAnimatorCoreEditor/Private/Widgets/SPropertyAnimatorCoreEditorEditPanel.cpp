// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SPropertyAnimatorCoreEditorEditPanel.h"

#include "DragDropOps/PropertyAnimatorCoreEditorViewDragDropOp.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/GenericCommands.h"
#include "Menus/PropertyAnimatorCoreEditorMenu.h"
#include "Menus/PropertyAnimatorCoreEditorMenuContext.h"
#include "Styles/PropertyAnimatorCoreEditorStyle.h"
#include "Styling/ToolBarStyle.h"
#include "Subsystems/PropertyAnimatorCoreEditorSubsystem.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Views/SPropertyAnimatorCoreEditorControllersView.h"
#include "Views/SPropertyAnimatorCoreEditorPropertiesView.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreEditorEditPanel"

SPropertyAnimatorCoreEditorEditPanel::~SPropertyAnimatorCoreEditorEditPanel()
{
	UPropertyAnimatorCoreBase::OnAnimatorCreatedDelegate.RemoveAll(this);
	UPropertyAnimatorCoreBase::OnAnimatorRemovedDelegate.RemoveAll(this);
	UPropertyAnimatorCoreBase::OnAnimatorRenamedDelegate.RemoveAll(this);
	UPropertyAnimatorCoreBase::OnAnimatorPropertyLinkedDelegate.RemoveAll(this);
	UPropertyAnimatorCoreBase::OnAnimatorPropertyUnlinkedDelegate.RemoveAll(this);
}

TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> SPropertyAnimatorCoreEditorEditPanel::OpenWindow()
{
	TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> PropertyControllerPanel = SNew(SPropertyAnimatorCoreEditorEditPanel);

	const TAttribute<FText> WindowTitle = TAttribute<FText>::CreateSP(PropertyControllerPanel.Get(), &SPropertyAnimatorCoreEditorEditPanel::GetTitle);

	const TSharedRef<SWindow> PropertyControlWindow =
		SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(FVector2f(600.0f, 600.0f))
		.SizingRule(ESizingRule::FixedSize)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.FocusWhenFirstShown(true)
		.Content()
		[
			PropertyControllerPanel.ToSharedRef()
		];

	FSlateApplication::Get().AddWindow(PropertyControlWindow, true);

	return PropertyControllerPanel;
}

void SPropertyAnimatorCoreEditorEditPanel::CloseWindow() const
{
	if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
	{
		Window->RequestDestroyWindow();
	}
}

void SPropertyAnimatorCoreEditorEditPanel::FocusWindow() const
{
	if (const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared()))
	{
		Window->BringToFront();
	}
}

void SPropertyAnimatorCoreEditorEditPanel::Construct(const FArguments& InArgs)
{
	static const FSlateIcon ControllersIcon(FPropertyAnimatorCoreEditorStyle::Get().GetStyleSetName(), "PropertyControlIcon.Default");
	static const FSlateIcon PropertiesIcon(FPropertyAnimatorCoreEditorStyle::Get().GetStyleSetName(), "PropertyControlIcon.Default");

	static const FToolBarStyle& ToolBarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("SlimToolBar");
	static const FCheckBoxStyle* const CheckStyle = &ToolBarStyle.ToggleButton;

	TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> This = SharedThis(this);

	Options = FPropertyAnimatorCoreEditorEditPanelOptions(This);

	CommandList = MakeShared<FUICommandList>();

	const FGenericCommands& GenericCommands = FGenericCommands::Get();

	CommandList->MapAction(GenericCommands.Delete
		, FExecuteAction::CreateSP(this, &SPropertyAnimatorCoreEditorEditPanel::ExecuteDeleteCommand)
		, FCanExecuteAction::CreateSP(this, &SPropertyAnimatorCoreEditorEditPanel::CanExecuteDeleteCommand));

	CommandList->MapAction(GenericCommands.Rename
		, FExecuteAction::CreateSP(this, &SPropertyAnimatorCoreEditorEditPanel::ExecuteRenameCommand)
		, FCanExecuteAction::CreateSP(this, &SPropertyAnimatorCoreEditorEditPanel::CanExecuteRenameCommand));

	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SNew(SVerticalBox)
		// Toolbar to switch between different views
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(&ToolBarStyle.SeparatorBrush)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			.Padding(0.f, 5.f)
			[
				SAssignNew(ViewsToolbar, SScrollBox)
				.Orientation(EOrientation::Orient_Horizontal)
				.ScrollBarThickness(FVector2D(1.f))
				// Add "View by properties" button
				+ SScrollBox::Slot()
				.Padding(5.f, 0.f)
				[
					SNew(SCheckBox)
					.Style(CheckStyle)
					.ForegroundColor(FLinearColor::White)
					.OnCheckStateChanged(this, &SPropertyAnimatorCoreEditorEditPanel::OnViewButtonClicked, SPropertyAnimatorCoreEditorEditPanel::PropertiesViewIndex)
					.IsChecked(this, &SPropertyAnimatorCoreEditorEditPanel::IsViewButtonActive, SPropertyAnimatorCoreEditorEditPanel::PropertiesViewIndex)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(5.f)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.DesiredSizeOverride(FVector2D(16.f, 16.f))
							.Image(PropertiesIcon.GetIcon())
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.Padding(5.f)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("PropertiesView.Label", "View by properties"))
						]
					]
				]
				// Add "View by controllers" button
				+ SScrollBox::Slot()
				.Padding(5.f, 0.f)
				[
					SNew(SCheckBox)
					.Style(CheckStyle)
					.ForegroundColor(FLinearColor::White)
					.OnCheckStateChanged(this, &SPropertyAnimatorCoreEditorEditPanel::OnViewButtonClicked, SPropertyAnimatorCoreEditorEditPanel::ControllersViewIndex)
					.IsChecked(this, &SPropertyAnimatorCoreEditorEditPanel::IsViewButtonActive, SPropertyAnimatorCoreEditorEditPanel::ControllersViewIndex)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(5.f)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.DesiredSizeOverride(FVector2D(16.f, 16.f))
							.Image(ControllersIcon.GetIcon())
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.f)
						.Padding(5.f)
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("ControllersView.Label", "View by animators"))
						]
					]
				]
			]
		]
		// Widget for each views
		+ SVerticalBox::Slot()
		.FillHeight(1.f)
		[
			SAssignNew(ViewsSwitcher, SWidgetSwitcher)
			.WidgetIndex(ActiveViewIndex)
			+ SWidgetSwitcher::Slot()
			.Padding(5.f)
			[
				SAssignNew(PropertiesView, SPropertyAnimatorCoreEditorPropertiesView, This)
			]
			+ SWidgetSwitcher::Slot()
			.Padding(5.f)
			[
				SAssignNew(ControllersView, SPropertyAnimatorCoreEditorControllersView, This)
			]
		]
	];

	UPropertyAnimatorCoreBase::OnAnimatorCreatedDelegate.AddSP(this, &SPropertyAnimatorCoreEditorEditPanel::OnControllerUpdated);
	UPropertyAnimatorCoreBase::OnAnimatorRemovedDelegate.AddSP(this, &SPropertyAnimatorCoreEditorEditPanel::OnControllerUpdated);
	UPropertyAnimatorCoreBase::OnAnimatorRenamedDelegate.AddSP(this, &SPropertyAnimatorCoreEditorEditPanel::OnControllerUpdated);
	UPropertyAnimatorCoreBase::OnAnimatorPropertyLinkedDelegate.AddSP(this, &SPropertyAnimatorCoreEditorEditPanel::OnControllerPropertyUpdated);
	UPropertyAnimatorCoreBase::OnAnimatorPropertyUnlinkedDelegate.AddSP(this, &SPropertyAnimatorCoreEditorEditPanel::OnControllerPropertyUpdated);
}

FReply SPropertyAnimatorCoreEditorEditPanel::OnDragStart(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent)
{
	if (!GlobalSelection.IsEmpty())
	{
		const TSharedRef<FPropertyAnimatorCoreEditorViewDragDropOp> DragDropOp = FPropertyAnimatorCoreEditorViewDragDropOp::New(GlobalSelection);
		return FReply::Handled().BeginDragDrop(DragDropOp);
	}

	return FReply::Unhandled();
}

FReply SPropertyAnimatorCoreEditorEditPanel::OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		// Open context menu
		const FVector2D MenuLocation = FSlateApplication::Get().GetCursorPos();

		FSlateApplication::Get().PushMenu(
			SharedThis(this),
			FWidgetPath(),
			GenerateContextMenuWidget(),
			MenuLocation,
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);

		return FReply::Handled();
	}

	return SCompoundWidget::OnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply SPropertyAnimatorCoreEditorEditPanel::OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid()
		&& CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return SCompoundWidget::OnKeyDown(InGeometry, InKeyEvent);
}

TSharedRef<SWidget> SPropertyAnimatorCoreEditorEditPanel::GenerateContextMenuWidget()
{
	UToolMenus* Menus = UToolMenus::Get();

	check(Menus);

	static const FName EditPanelContextMenuName = TEXT("AnimatorEditPanelContextMenu");

	if (!Menus->IsMenuRegistered(EditPanelContextMenuName))
	{
		UToolMenu* ContextMenu = Menus->RegisterMenu(EditPanelContextMenuName, NAME_None, EMultiBoxType::Menu);
		ContextMenu->AddDynamicSection(TEXT("FillAnimatorEditPanelContextSection"), FNewToolMenuDelegate::CreateSP(this, &SPropertyAnimatorCoreEditorEditPanel::FillAnimatorContextSection));

		const FGenericCommands& GenericCommands = FGenericCommands::Get();
		FToolMenuSection& GenericActionsSection = ContextMenu->FindOrAddSection(TEXT("GenericActions"), LOCTEXT("GenericActions.Label", "Generic Actions"));
		GenericActionsSection.AddMenuEntry(GenericCommands.Delete);
		GenericActionsSection.AddMenuEntry(GenericCommands.Rename);
	}

	UPropertyAnimatorCoreEditorMenuContext* MenuContext = nullptr;
	if (ActiveViewIndex == SPropertyAnimatorCoreEditorEditPanel::ControllersViewIndex && GlobalSelection.IsEmpty())
	{
		MenuContext = NewObject<UPropertyAnimatorCoreEditorMenuContext>();
		MenuContext->SetPropertyData(FPropertyAnimatorCoreData(GetOptions().GetContextActor(), nullptr, nullptr));
	}

	const FToolMenuContext ToolMenuContext(CommandList, nullptr, MenuContext);
	return Menus->GenerateWidget(EditPanelContextMenuName, ToolMenuContext);
}

void SPropertyAnimatorCoreEditorEditPanel::FillAnimatorContextSection(UToolMenu* InToolMenu) const
{
	if (!InToolMenu)
	{
		return;
	}

	const UPropertyAnimatorCoreEditorMenuContext* Context = InToolMenu->FindContext<UPropertyAnimatorCoreEditorMenuContext>();

	if (!Context)
	{
		return;
	}

	UPropertyAnimatorCoreEditorSubsystem* EditorSubsystem = UPropertyAnimatorCoreEditorSubsystem::Get();
	if (!EditorSubsystem)
	{
		return;
	}

	const FPropertyAnimatorCoreEditorMenuContext MenuContext({}, {Context->GetPropertyData()});
	const FPropertyAnimatorCoreEditorMenuOptions MenuOptions({EPropertyAnimatorCoreEditorMenuType::New});
	EditorSubsystem->FillAnimatorMenu(InToolMenu, MenuContext, MenuOptions);
}

bool SPropertyAnimatorCoreEditorEditPanel::CanExecuteDeleteCommand() const
{
	return !GlobalSelection.IsEmpty();
}

void SPropertyAnimatorCoreEditorEditPanel::ExecuteDeleteCommand()
{
	if (!CanExecuteDeleteCommand())
	{
		return;
	}

	const UPropertyAnimatorCoreSubsystem* ControllerSubsystem = UPropertyAnimatorCoreSubsystem::Get();
	for (const FPropertiesViewControllerItem& Item : GlobalSelection)
	{
		UPropertyAnimatorCoreBase* Controller = Item.ControllerWeak.Get();

		if (!Controller)
		{
			continue;
		}

		if (Item.Property.IsValid())
		{
			Controller->UnlinkProperty(*Item.Property);
		}
		else
		{
			ControllerSubsystem->RemoveAnimator(Controller);
		}
	}

	GlobalSelection.Empty();
}

bool SPropertyAnimatorCoreEditorEditPanel::CanExecuteRenameCommand() const
{
	if (GlobalSelection.Num() == 1 && ActiveViewIndex == SPropertyAnimatorCoreEditorEditPanel::ControllersViewIndex)
	{
		const FPropertiesViewControllerItem Item = GlobalSelection.Array()[0];

		if (Item.ControllerWeak.IsValid())
		{
			return true;
		}
	}

	return false;
}

void SPropertyAnimatorCoreEditorEditPanel::ExecuteRenameCommand()
{
	if (!CanExecuteRenameCommand())
	{
		return;
	}

	const FPropertiesViewControllerItem Item = GlobalSelection.Array()[0];
	UPropertyAnimatorCoreBase* Controller = Item.ControllerWeak.Get();

	if (!Controller)
	{
		return;
	}

	OnControllerRenameRequestedDelegate.Broadcast(Controller);
}

void SPropertyAnimatorCoreEditorEditPanel::Update() const
{
	if (PropertiesView.IsValid())
	{
		PropertiesView->Update();
	}

	if (ControllersView.IsValid())
    {
    	ControllersView->Update();
    }
}

FText SPropertyAnimatorCoreEditorEditPanel::GetTitle() const
{
	static const FText TitleText = LOCTEXT("EditAnimatorsWindow.Title", "Edit Animators ({0})");
	const AActor* OwningActor = GetOptions().GetContextActor();
	return FText::Format(TitleText, FText::FromString(OwningActor ? OwningActor->GetActorNameOrLabel() : TEXT("Invalid Actor")));
}

void SPropertyAnimatorCoreEditorEditPanel::OnViewButtonClicked(ECheckBoxState InCheckBoxState, int32 InViewIdx)
{
	ActiveViewIndex = InViewIdx;

	GlobalSelection.Empty();
	OnGlobalSelectionChangedDelegate.Broadcast();

	if (ViewsSwitcher.IsValid())
	{
		ViewsSwitcher->SetActiveWidgetIndex(ActiveViewIndex);
	}
}

ECheckBoxState SPropertyAnimatorCoreEditorEditPanel::IsViewButtonActive(int32 InViewIdx) const
{
	return InViewIdx == ActiveViewIndex
		? ECheckBoxState::Checked
		: ECheckBoxState::Unchecked;
}

void SPropertyAnimatorCoreEditorEditPanel::OnControllerUpdated(UPropertyAnimatorCoreBase* InController)
{
	if (InController && InController->GetAnimatorActor() == GetOptions().GetContextActor())
	{
		Update();
	}
}

void SPropertyAnimatorCoreEditorEditPanel::OnControllerPropertyUpdated(UPropertyAnimatorCoreBase* InController, const FPropertyAnimatorCoreData& InProperty)
{
	if (InController && InController->GetAnimatorActor() == GetOptions().GetContextActor())
	{
		Update();
	}
}

#undef LOCTEXT_NAMESPACE
