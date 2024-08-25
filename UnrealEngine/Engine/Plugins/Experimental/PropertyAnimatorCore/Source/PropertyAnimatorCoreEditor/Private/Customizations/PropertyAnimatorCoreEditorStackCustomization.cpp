// Copyright Epic Games, Inc. All Rights Reserved.

#include "PropertyAnimatorCoreEditorStackCustomization.h"

#include "Components/PropertyAnimatorCoreComponent.h"
#include "Contexts/OperatorStackEditorMenuContext.h"
#include "Animators/PropertyAnimatorCoreBase.h"
#include "Framework/Commands/GenericCommands.h"
#include "Items/OperatorStackEditorObjectItem.h"
#include "Menus/PropertyAnimatorCoreEditorMenu.h"
#include "Styling/SlateIconFinder.h"
#include "Subsystems/PropertyAnimatorCoreEditorSubsystem.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "Widgets/PropertyAnimatorCoreEditorEditPanelOptions.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorEditorStackCustomization"

UPropertyAnimatorCoreEditorStackCustomization::UPropertyAnimatorCoreEditorStackCustomization()
	: UOperatorStackEditorStackCustomization(
		TEXT("Animators")
		, LOCTEXT("CustomizationLabel", "Animators")
		, 0
	)
{
	RegisterCustomizationFor(UPropertyAnimatorCoreBase::StaticClass());
	RegisterCustomizationFor(UPropertyAnimatorCoreComponent::StaticClass());
}

bool UPropertyAnimatorCoreEditorStackCustomization::TransformContextItem(const FOperatorStackEditorItemPtr& InItem, TArray<FOperatorStackEditorItemPtr>& OutTransformedItems) const
{
	// If we have an actor then display controllers
	if (InItem->IsA<AActor>())
	{
		const AActor* Actor = InItem->Get<AActor>();

		if (UActorComponent* ControllerComponent = Actor->FindComponentByClass(UPropertyAnimatorCoreComponent::StaticClass()))
		{
			OutTransformedItems.Add(MakeShared<FOperatorStackEditorObjectItem>(ControllerComponent));
		}

		return true;
	}
	// If we have a property controller component then display stack
	else if (InItem->IsA<UPropertyAnimatorCoreComponent>())
	{
		const UPropertyAnimatorCoreComponent* ControllerComponent = InItem->Get<UPropertyAnimatorCoreComponent>();

		for (UPropertyAnimatorCoreBase* Controller : ControllerComponent->GetAnimators())
		{
			if (Controller && Controller->IsA<UPropertyAnimatorCoreBase>())
			{
				OutTransformedItems.Add(MakeShared<FOperatorStackEditorObjectItem>(Controller));
			}
		}

		return true;
	}

	return Super::TransformContextItem(InItem, OutTransformedItems);
}

void UPropertyAnimatorCoreEditorStackCustomization::CustomizeStackHeader(const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorHeaderBuilder& InHeaderBuilder)
{
	if (!InItemTree.GetContext().GetItems().IsEmpty())
	{
		static const FName AddAnimatorMenuName = TEXT("AddAnimatorMenu");
		if (!UToolMenus::Get()->IsMenuRegistered(AddAnimatorMenuName))
		{
			UToolMenu* const AddControllerMenu = UToolMenus::Get()->RegisterMenu(AddAnimatorMenuName, NAME_None, EMultiBoxType::Menu);
			AddControllerMenu->AddDynamicSection(TEXT("FillAddAnimatorMenuSection"), FNewToolMenuDelegate::CreateUObject(this, &UPropertyAnimatorCoreEditorStackCustomization::FillAddAnimatorMenuSection));
		}

		// Pinned search keywords
		TSet<FString> PinnedControllerNames;
		for (const FOperatorStackEditorItemPtr& SupportedItem : InItemTree.GetAllItems())
		{
			if (const UPropertyAnimatorCoreBase* Controller = SupportedItem->Get<UPropertyAnimatorCoreBase>())
			{
				PinnedControllerNames.Add(Controller->GetAnimatorOriginalName().ToString());
			}
		}

		InHeaderBuilder
			.SetToolMenu(
				AddAnimatorMenuName
				, LOCTEXT("AddControllersMenu", "Add Animators")
				, FAppStyle::GetBrush(TEXT("Icons.PlusCircle")))
			.SetSearchAllowed(true)
			.SetSearchPinnedKeywords(PinnedControllerNames);
	}

	Super::CustomizeStackHeader(InItemTree, InHeaderBuilder);
}

void UPropertyAnimatorCoreEditorStackCustomization::CustomizeItemHeader(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorHeaderBuilder& InHeaderBuilder)
{
	// Customize component header
	if (InItem->IsA<UPropertyAnimatorCoreComponent>())
	{
		FBoolProperty* EnableProperty = FindFProperty<FBoolProperty>(UPropertyAnimatorCoreComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreComponent, bAnimatorsEnabled));

		const FSlateIcon ClassIcon = FSlateIconFinder::FindIconForClass(UPropertyAnimatorCoreComponent::StaticClass());

		// Action menu available in header in slim toolbar
		static const FName HeaderComponentMenuName = TEXT("HeaderComponentMenu");
		if (!UToolMenus::Get()->IsMenuRegistered(HeaderComponentMenuName))
		{
			UToolMenu* const HeaderAnimatorMenu = UToolMenus::Get()->RegisterMenu(HeaderComponentMenuName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
			HeaderAnimatorMenu->AddDynamicSection(TEXT("FillHeaderComponentMenu"), FNewToolMenuDelegate::CreateUObject(this, &UPropertyAnimatorCoreEditorStackCustomization::FillComponentHeaderActionMenu));
		}

		FString HeaderLabel = TEXT("Animators");

		// Add actor name next to label when we customize multiple items
		if (InItemTree.GetRootItems().Num() > 1)
		{
			if (const UPropertyAnimatorCoreComponent* ControllerComponent = InItem->Get<UPropertyAnimatorCoreComponent>())
			{
				HeaderLabel += + TEXT(" (") + ControllerComponent->GetOwner()->GetActorNameOrLabel() + TEXT(")");
			}
		}

		InHeaderBuilder
			.SetProperty(EnableProperty)
			.SetToolbarMenu(HeaderComponentMenuName)
			.SetIcon(ClassIcon.GetIcon())
			.SetLabel(FText::FromString(HeaderLabel));
	}
	// Customize controller header
	else if (InItem->IsA<UPropertyAnimatorCoreBase>())
	{
		UPropertyAnimatorCoreBase* Animator = InItem->Get<UPropertyAnimatorCoreBase>();

		FBoolProperty* EnableProperty = FindFProperty<FBoolProperty>(UPropertyAnimatorCoreBase::StaticClass(), GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreBase, bAnimatorEnabled));

		const FSlateIcon ClassIcon = FSlateIconFinder::FindIconForClass(Animator->GetClass());

		// Commands for item on key events
		const TSharedPtr<FUICommandList> Commands = CreateAnimatorCommands(Animator);

		// Action menu available in header in slim toolbar
		static const FName HeaderModifierMenuName = TEXT("HeaderControllerMenu");
		if (!UToolMenus::Get()->IsMenuRegistered(HeaderModifierMenuName))
		{
			UToolMenu* const HeaderModifierMenu = UToolMenus::Get()->RegisterMenu(HeaderModifierMenuName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
			HeaderModifierMenu->AddDynamicSection(TEXT("FillHeaderControllerMenu"), FNewToolMenuDelegate::CreateUObject(this, &UPropertyAnimatorCoreEditorStackCustomization::FillAnimatorHeaderActionMenu));
		}

		// Context menu available when right clicking on item
		static const FName ContextModifierMenuName = TEXT("ContextControllerMenu");
		if (!UToolMenus::Get()->IsMenuRegistered(ContextModifierMenuName))
		{
			UToolMenu* const ContextModifierMenu = UToolMenus::Get()->RegisterMenu(ContextModifierMenuName, NAME_None, EMultiBoxType::Menu);
			ContextModifierMenu->AddDynamicSection(TEXT("FillContextControllerMenu"), FNewToolMenuDelegate::CreateUObject(this, &UPropertyAnimatorCoreEditorStackCustomization::FillAnimatorContextActionMenu));
		}

		const TSet<FString> SearchKeywords
		{
			Animator->GetAnimatorOriginalName().ToString(),
			Animator->GetAnimatorDisplayName()
		};

		/** Show last execution error messages if failed execution */
		TAttribute<EOperatorStackEditorMessageType> MessageType = EOperatorStackEditorMessageType::None;
		TAttribute<FText> MessageText = FText::GetEmpty();

		TWeakObjectPtr<UPropertyAnimatorCoreBase> AnimatorWeak(Animator);

		MessageType = TAttribute<EOperatorStackEditorMessageType>::CreateLambda([AnimatorWeak]()
		{
			if (const UPropertyAnimatorCoreBase* Animator = AnimatorWeak.Get())
			{
				return Animator->GetLinkedPropertiesCount() > 0
					? EOperatorStackEditorMessageType::None
					: EOperatorStackEditorMessageType::Info;
			}

			return EOperatorStackEditorMessageType::None;
		});

		MessageText = TAttribute<FText>::CreateLambda([AnimatorWeak]()
		{
			if (AnimatorWeak.IsValid())
			{
				return LOCTEXT("NoPropertiesLinked", "No properties are currently linked to this animator");
			}

			return FText::GetEmpty();
		});

		static const FLinearColor AnimatorColor = FLinearColor(FColor::Orange).Desaturate(0.25);

		InHeaderBuilder
			.SetBorderColor(AnimatorColor)
			.SetSearchAllowed(true)
			.SetSearchKeywords(SearchKeywords)
			.SetExpandable(true)
			.SetIcon(ClassIcon.GetIcon())
			.SetLabel(FText::FromString(Animator->GetAnimatorDisplayName()))
			.SetProperty(EnableProperty)
			.SetCommandList(Commands)
			.SetToolbarMenu(HeaderModifierMenuName)
			.SetContextMenu(ContextModifierMenuName)
			.SetMessageBox(MessageType, MessageText);
	}

	Super::CustomizeItemHeader(InItem, InItemTree, InHeaderBuilder);
}

void UPropertyAnimatorCoreEditorStackCustomization::CustomizeItemBody(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorBodyBuilder& InBodyBuilder)
{
	// Customize controller body
	if (InItem->IsA<UPropertyAnimatorCoreBase>())
	{
		FBoolProperty* EnableProperty = FindFProperty<FBoolProperty>(UPropertyAnimatorCoreBase::StaticClass(), GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreBase, bAnimatorEnabled));

		InBodyBuilder
			.SetShowDetailsView(true)
			.DisallowProperty(EnableProperty);
	}

	Super::CustomizeItemBody(InItem, InItemTree, InBodyBuilder);
}

void UPropertyAnimatorCoreEditorStackCustomization::CustomizeItemFooter(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorFooterBuilder& InFooterBuilder)
{
	// Customize component footer
	if (InItem->IsA<UPropertyAnimatorCoreComponent>())
	{
		FProperty* MagnitudeProperty = FindFProperty<FProperty>(UPropertyAnimatorCoreComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UPropertyAnimatorCoreComponent, AnimatorsMagnitude));

		InFooterBuilder
			.AllowProperty(MagnitudeProperty)
			.SetShowDetailsView(true);
	}

	Super::CustomizeItemFooter(InItem, InItemTree, InFooterBuilder);
}

bool UPropertyAnimatorCoreEditorStackCustomization::OnIsItemDraggable(const FOperatorStackEditorItemPtr& InItem)
{
	// Allow selection of item in listview for commands to work
	if (InItem->IsA<UPropertyAnimatorCoreBase>())
	{
		return true;
	}

	return Super::OnIsItemDraggable(InItem);
}

const FSlateBrush* UPropertyAnimatorCoreEditorStackCustomization::GetIcon() const
{
	return FSlateIconFinder::FindIconForClass(UPropertyAnimatorCoreBase::StaticClass()).GetIcon();
}

void UPropertyAnimatorCoreEditorStackCustomization::RemoveControllerAction(UPropertyAnimatorCoreBase* InAnimator) const
{
	if (!InAnimator)
	{
		return;
	}

	const UPropertyAnimatorCoreSubsystem* Subsystem = UPropertyAnimatorCoreSubsystem::Get();

	if (!Subsystem)
	{
		return;
	}

	Subsystem->RemoveAnimator(InAnimator, true);
}

void UPropertyAnimatorCoreEditorStackCustomization::RemoveControllersAction(UPropertyAnimatorCoreComponent* InComponent) const
{
	const UPropertyAnimatorCoreSubsystem* Subsystem = UPropertyAnimatorCoreSubsystem::Get();
	if (!InComponent || !Subsystem)
	{
		return;
	}

	TSet<UPropertyAnimatorCoreBase*> Animators;
	for (const TObjectPtr<UPropertyAnimatorCoreBase>& Animator : InComponent->GetAnimators())
	{
		Animators.Add(Animator);
	}

	Subsystem->RemoveAnimators(Animators, true);
}

void UPropertyAnimatorCoreEditorStackCustomization::FillAddAnimatorMenuSection(UToolMenu* InToolMenu) const
{
	if (!InToolMenu)
	{
		return;
	}

	const UOperatorStackEditorMenuContext* const AddAnimatorContext = InToolMenu->FindContext<UOperatorStackEditorMenuContext>();
	if (!AddAnimatorContext)
	{
		return;
	}

	const FOperatorStackEditorContextPtr Context = AddAnimatorContext->GetContext();
	if (!Context)
	{
		return;
	}

	UPropertyAnimatorCoreEditorSubsystem* AnimatorEditorSubsystem = UPropertyAnimatorCoreEditorSubsystem::Get();
	if (!AnimatorEditorSubsystem)
	{
		return;
	}

	TSet<UObject*> ContextObjects;
	for (const FOperatorStackEditorItemPtr& ContextItem : Context->GetItems())
	{
		if (ContextItem->IsA<UObject>())
		{
			ContextObjects.Add(ContextItem->Get<UObject>());
		}
	}

	const FPropertyAnimatorCoreEditorMenuContext MenuContext(ContextObjects, {});
	const FPropertyAnimatorCoreEditorMenuOptions MenuOptions({EPropertyAnimatorCoreEditorMenuType::New});
	AnimatorEditorSubsystem->FillAnimatorMenu(InToolMenu, MenuContext, MenuOptions);
}

void UPropertyAnimatorCoreEditorStackCustomization::FillComponentHeaderActionMenu(UToolMenu* InToolMenu)
{
	if (!InToolMenu)
	{
		return;
	}

	const UOperatorStackEditorMenuContext* const MenuContext = InToolMenu->FindContext<UOperatorStackEditorMenuContext>();
	if (!MenuContext)
	{
		return;
	}

	const FOperatorStackEditorItemPtr ItemContext = MenuContext->GetItem();
	if (!ItemContext)
	{
		return;
	}

	UPropertyAnimatorCoreComponent* ControllerComponent = ItemContext->Get<UPropertyAnimatorCoreComponent>();
	if (!IsValid(ControllerComponent))
	{
		return;
	}

	// Add remove controllers entry
	const FToolMenuEntry RemoveControllersAction = FToolMenuEntry::InitToolBarButton(
		TEXT("RemoveControllersMenuEntry")
		, FExecuteAction::CreateUObject(this, &UPropertyAnimatorCoreEditorStackCustomization::RemoveControllersAction, ControllerComponent)
		, FText::GetEmpty()
		, FText::GetEmpty()
		, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete")
	);

	InToolMenu->AddMenuEntry(RemoveControllersAction.Name, RemoveControllersAction);
}

void UPropertyAnimatorCoreEditorStackCustomization::FillAnimatorHeaderActionMenu(UToolMenu* InToolMenu)
{
	if (!InToolMenu)
	{
		return;
	}

	const UOperatorStackEditorMenuContext* const MenuContext = InToolMenu->FindContext<UOperatorStackEditorMenuContext>();
	if (!MenuContext)
	{
		return;
	}

	const FOperatorStackEditorItemPtr ItemContext = MenuContext->GetItem();
	if (!ItemContext)
	{
		return;
	}

	UPropertyAnimatorCoreBase* Controller = ItemContext->Get<UPropertyAnimatorCoreBase>();
	if (!IsValid(Controller))
	{
		return;
	}

	// Add remove controller entry
	const FToolMenuEntry RemoveControllerAction = FToolMenuEntry::InitToolBarButton(
		TEXT("RemoveControllerMenuEntry")
		, FExecuteAction::CreateUObject(this, &UPropertyAnimatorCoreEditorStackCustomization::RemoveControllerAction, Controller)
		, FText::GetEmpty()
		, FText::GetEmpty()
		, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete")
	);

	InToolMenu->AddMenuEntry(RemoveControllerAction.Name, RemoveControllerAction);
}

void UPropertyAnimatorCoreEditorStackCustomization::FillAnimatorContextActionMenu(UToolMenu* InToolMenu) const
{
	if (!InToolMenu)
	{
		return;
	}

	const UOperatorStackEditorMenuContext* const MenuContext = InToolMenu->FindContext<UOperatorStackEditorMenuContext>();
	if (!MenuContext)
	{
		return;
	}

	const FOperatorStackEditorItemPtr ItemContext = MenuContext->GetItem();
	if (!ItemContext)
	{
		return;
	}

	const UPropertyAnimatorCoreBase* Controller = ItemContext->Get<UPropertyAnimatorCoreBase>();
	if (!IsValid(Controller))
	{
		return;
	}

	// Lets get the commands list to bind the menu entry and execute it when clicked
	TSharedPtr<const FUICommandList> Commands;
	const TSharedPtr<FUICommandInfo> DeleteCommand = FGenericCommands::Get().Delete;
	InToolMenu->Context.GetActionForCommand(DeleteCommand, Commands);
	const FToolMenuEntry RemoveControllerMenuEntry = FToolMenuEntry::InitMenuEntryWithCommandList(DeleteCommand, Commands);

	InToolMenu->AddMenuEntry(RemoveControllerMenuEntry.Name, RemoveControllerMenuEntry);
}

TSharedRef<FUICommandList> UPropertyAnimatorCoreEditorStackCustomization::CreateAnimatorCommands(UPropertyAnimatorCoreBase* InAnimator)
{
	TSharedRef<FUICommandList> Commands = MakeShared<FUICommandList>();

	Commands->MapAction(FGenericCommands::Get().Delete,
		FUIAction(
			FExecuteAction::CreateUObject(this, &UPropertyAnimatorCoreEditorStackCustomization::RemoveControllerAction, InAnimator),
			FCanExecuteAction()
		)
	);

	return Commands;
}

#undef LOCTEXT_NAMESPACE
