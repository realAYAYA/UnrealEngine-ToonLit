// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modifiers/Customizations/ActorModifierCoreEditorStackCustomization.h"

#include "Framework/Commands/GenericCommands.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "JsonObjectConverter.h"
#include "Modifiers/ActorModifierCoreBase.h"
#include "Modifiers/ActorModifierCoreComponent.h"
#include "Modifiers/ActorModifierCoreStack.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/SlateIconFinder.h"
#include "Subsystems/ActorModifierCoreEditorSubsystem.h"
#include "Subsystems/ActorModifierCoreSubsystem.h"
#include "ActorModifierCoreEditorStyle.h"
#include "Contexts/OperatorStackEditorMenuContext.h"
#include "Items/OperatorStackEditorObjectItem.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "ActorModifierCoreEditorStackCustomization"

DEFINE_LOG_CATEGORY_STATIC(LogActorModifierCoreEditorPropertiesWrapper, Log, All);

UActorModifierCoreEditorStackCustomization::UActorModifierCoreEditorStackCustomization()
	: UOperatorStackEditorStackCustomization(
		TEXT("Modifiers")
		, LOCTEXT("CustomizationLabel", "Modifiers")
		, 1
	)
{
	// for stack and modifiers
	RegisterCustomizationFor(UActorModifierCoreBase::StaticClass());
}

bool UActorModifierCoreEditorStackCustomization::TransformContextItem(const FOperatorStackEditorItemPtr& InItem, TArray<FOperatorStackEditorItemPtr>& OutTransformedItems) const
{
	// If we have an actor then display stack
	if (InItem->IsA<AActor>())
	{
		const UActorModifierCoreSubsystem* ModifierSubsystem = UActorModifierCoreSubsystem::Get();

		if (UActorModifierCoreStack* Stack = ModifierSubsystem->GetActorModifierStack(InItem->Get<AActor>()))
		{
			OutTransformedItems.Add(MakeShared<FOperatorStackEditorObjectItem>(Stack));

			return true;
		}
	}
	// If we have a modifier component then display stack
	else if (InItem->IsA<UActorModifierCoreComponent>())
	{
		const UActorModifierCoreComponent* ModifierComponent = InItem->Get<UActorModifierCoreComponent>();

		OutTransformedItems.Add(MakeShared<FOperatorStackEditorObjectItem>(ModifierComponent->GetModifierStack()));

		return true;
	}
	// If we have a stack then display modifiers inside
	else if (InItem->IsA<UActorModifierCoreStack>())
	{
		const UActorModifierCoreStack* ModifierStack = InItem->Get<UActorModifierCoreStack>();

		for (UActorModifierCoreBase* Modifier : ModifierStack->GetModifiers())
		{
			OutTransformedItems.Add(MakeShared<FOperatorStackEditorObjectItem>(Modifier));
		}

		return true;
	}

	return Super::TransformContextItem(InItem, OutTransformedItems);
}

void UActorModifierCoreEditorStackCustomization::CustomizeStackHeader(const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorHeaderBuilder& InHeaderBuilder)
{
	if (!InItemTree.GetContext().GetItems().IsEmpty())
	{
		static const FName AddModifierMenuName = TEXT("AddModifierMenu");
		if (!UToolMenus::Get()->IsMenuRegistered(AddModifierMenuName))
		{
			UToolMenu* const AddModifierMenu = UToolMenus::Get()->RegisterMenu(AddModifierMenuName, NAME_None, EMultiBoxType::Menu);
			AddModifierMenu->AddDynamicSection(TEXT("PopulateAddModifierMenu"), FNewToolMenuDelegate::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::FillStackHeaderMenu));
		}

		// Pin used categories
		TSet<FString> PinnedKeywords;
		for (FOperatorStackEditorItemPtr Item : InItemTree.GetAllItems())
		{
			if (Item.IsValid() && Item->IsA<UActorModifierCoreBase>())
			{
				const UActorModifierCoreBase* Modifier = Item->Get<UActorModifierCoreBase>();

				if (Modifier && !Modifier->IsModifierStack())
				{
					PinnedKeywords.Add(Modifier->GetModifierCategory().ToString());
				}
			}
		}

		InHeaderBuilder
			.SetToolMenu(
				AddModifierMenuName
				, LOCTEXT("AddModifiersMenu", "Add Modifiers")
				, FAppStyle::GetBrush(TEXT("Icons.PlusCircle"))
			)
			.SetSearchAllowed(true)
			.SetSearchPinnedKeywords(PinnedKeywords);
	}

	Super::CustomizeStackHeader(InItemTree, InHeaderBuilder);
}

void UActorModifierCoreEditorStackCustomization::CustomizeItemHeader(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorHeaderBuilder& InHeaderBuilder)
{
	const UActorModifierCoreSubsystem* ModifierSubsystem = UActorModifierCoreSubsystem::Get();

	// Customize stack and modifier header
	if (InItem->IsA<UActorModifierCoreBase>())
	{
		UActorModifierCoreBase* Modifier = InItem->Get<UActorModifierCoreBase>();

		FBoolProperty* ModifierEnableProperty = FindFProperty<FBoolProperty>(UActorModifierCoreBase::StaticClass(), GET_MEMBER_NAME_CHECKED(UActorModifierCoreBase, bModifierEnabled));

		const bool bIsStack = Modifier->IsModifierStack();

		// Commands for item on key events
		const TSharedPtr<FUICommandList> Commands = CreateModifierCommands(Modifier);

		// Action menu available in header in slim toolbar
		static const FName HeaderModifierMenuName = TEXT("HeaderModifierMenu");
		if (!UToolMenus::Get()->IsMenuRegistered(HeaderModifierMenuName))
		{
			UToolMenu* const HeaderModifierMenu = UToolMenus::Get()->RegisterMenu(HeaderModifierMenuName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
			HeaderModifierMenu->AddDynamicSection(TEXT("FillHeaderModifierMenu"), FNewToolMenuDelegate::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::FillItemHeaderActionMenu));
		}

		// Context menu available when right clicking on item
		static const FName ContextModifierMenuName = TEXT("ContextModifierMenu");
		if (!UToolMenus::Get()->IsMenuRegistered(ContextModifierMenuName))
		{
			UToolMenu* const ContextModifierMenu = UToolMenus::Get()->RegisterMenu(ContextModifierMenuName, NAME_None, EMultiBoxType::Menu);
			ContextModifierMenu->AddDynamicSection(TEXT("FillContextModifierMenu"), FNewToolMenuDelegate::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::FillItemContextActionMenu));
		}

		// Item keyword for search
		const TSet<FString> SearchKeywords
		{
			Modifier->GetModifierName().ToString(),
			Modifier->GetModifierCategory().ToString()
		};

		FSlateIcon ModifierIcon = FSlateIconFinder::FindIconForClass(UActorModifierCoreBase::StaticClass());
		FLinearColor ModifierColor = FLinearColor::Transparent;

		ModifierSubsystem->ProcessModifierMetadata(Modifier->GetModifierName(), [&ModifierIcon, &ModifierColor](const FActorModifierCoreMetadata& InMetadata)
		{
			ModifierIcon = InMetadata.GetIcon();
			ModifierColor = InMetadata.GetColor();
			return true;
		});

		FString HeaderLabel = Modifier->GetModifierName().ToString();

		// Add actor name next to stack label when we customize multiple items
		if (bIsStack && InItemTree.GetRootItems().Num() > 1)
		{
			if (const AActor* ModifiedActor = Modifier->GetModifiedActor())
			{
				HeaderLabel += + TEXT(" (") + ModifiedActor->GetActorNameOrLabel() + TEXT(")");
			}
		}

		/** Show last execution error messages if failed execution */
		TAttribute<EOperatorStackEditorMessageType> MessageType = EOperatorStackEditorMessageType::None;
		TAttribute<FText> MessageText = FText::GetEmpty();

		if (!bIsStack)
		{
			TWeakObjectPtr<UActorModifierCoreBase> ModifierWeak(Modifier);

			MessageType = TAttribute<EOperatorStackEditorMessageType>::CreateLambda([ModifierWeak]()
			{
				if (const UActorModifierCoreBase* Modifier = ModifierWeak.Get())
				{
					if (Modifier->GetModifierLastStatus().GetStatus() == EActorModifierCoreStatus::Warning)
					{
						return EOperatorStackEditorMessageType::Warning;
					}

					if (Modifier->GetModifierLastStatus().GetStatus() == EActorModifierCoreStatus::Error)
					{
						return EOperatorStackEditorMessageType::Error;
					}

					if (!Modifier->IsModifierEnabled())
					{
						return EOperatorStackEditorMessageType::Warning;
					}
				}

				return EOperatorStackEditorMessageType::None;
			});

			MessageText = TAttribute<FText>::CreateLambda([ModifierWeak]()
			{
				if (const UActorModifierCoreBase* Modifier = ModifierWeak.Get())
				{
					if (!Modifier->IsModifierEnabled())
					{
						return LOCTEXT("ModifierDisabled", "Modifier disabled");
					}

					return Modifier->GetModifierLastStatus().GetStatusMessage();
				}

				return FText::GetEmpty();
			});
		}

		InHeaderBuilder
			.SetSearchAllowed(true)
			.SetSearchKeywords(SearchKeywords)
			.SetExpandable(!bIsStack)
			.SetIcon(ModifierIcon.GetIcon())
			.SetLabel(FText::FromString(HeaderLabel))
			.SetBorderColor(ModifierColor)
			.SetProperty(ModifierEnableProperty)
			.SetCommandList(Commands)
			.SetToolbarMenu(HeaderModifierMenuName)
			.SetContextMenu(ContextModifierMenuName)
			.SetMessageBox(MessageType, MessageText);
	}

	Super::CustomizeItemHeader(InItem, InItemTree, InHeaderBuilder);
}

void UActorModifierCoreEditorStackCustomization::CustomizeItemBody(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorBodyBuilder& InBodyBuilder)
{
	// Customize stack and modifier body
	if (InItem->IsA<UActorModifierCoreBase>())
	{
		FBoolProperty* ModifierEnableProperty = FindFProperty<FBoolProperty>(UActorModifierCoreBase::StaticClass(), GET_MEMBER_NAME_CHECKED(UActorModifierCoreBase, bModifierEnabled));
		FBoolProperty* ProfilingEnableProperty = FindFProperty<FBoolProperty>(UActorModifierCoreStack::StaticClass(), GET_MEMBER_NAME_CHECKED(UActorModifierCoreStack, bModifierProfiling));

		const bool bIsStack = InItem->IsA<UActorModifierCoreStack>();

		InBodyBuilder
			.SetShowDetailsView(!bIsStack)
			.DisallowProperty(ModifierEnableProperty)
			.DisallowProperty(ProfilingEnableProperty);
	}

	Super::CustomizeItemBody(InItem, InItemTree, InBodyBuilder);
}

void UActorModifierCoreEditorStackCustomization::CustomizeItemFooter(const FOperatorStackEditorItemPtr& InItem, const FOperatorStackEditorTree& InItemTree, FOperatorStackEditorFooterBuilder& InFooterBuilder)
{
	// Customize stack and modifier footer
	if (InItem->IsA<UActorModifierCoreBase>())
	{
		const UActorModifierCoreBase* Modifier = InItem->Get<UActorModifierCoreBase>();
		const TSharedPtr<FActorModifierCoreProfiler> Profiler = Modifier->GetProfiler();
		UActorModifierCoreEditorSubsystem* ExtensionSubsystem = UActorModifierCoreEditorSubsystem::Get();

		if (Profiler.IsValid())
		{
			const TSharedPtr<SWidget> Widget = ExtensionSubsystem->CreateProfilerWidget(Profiler);

			InFooterBuilder
				.SetCustomWidget(Widget);
		}
	}

	Super::CustomizeItemFooter(InItem, InItemTree, InFooterBuilder);
}

bool UActorModifierCoreEditorStackCustomization::OnIsItemDraggable(const FOperatorStackEditorItemPtr& InDragItem)
{
	if (InDragItem->IsA<UActorModifierCoreBase>())
	{
		const bool bIsStack = InDragItem->IsA<UActorModifierCoreStack>();
		return !bIsStack;
	}

	return Super::OnIsItemDraggable(InDragItem);
}

TOptional<EItemDropZone> UActorModifierCoreEditorStackCustomization::OnItemCanAcceptDrop(const TArray<FOperatorStackEditorItemPtr>& InDraggedItems, const FOperatorStackEditorItemPtr& InDropZoneItem, EItemDropZone InZone)
{
	const UActorModifierCoreSubsystem* ModifierSubsystem = UActorModifierCoreSubsystem::Get();

	TSet<UActorModifierCoreBase*> DraggedModifiers;
	for (const FOperatorStackEditorItemPtr& Item : InDraggedItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		if (!Item->IsA<UActorModifierCoreBase>())
		{
			continue;
		}

		if (UActorModifierCoreBase* Modifier = Item->Get<UActorModifierCoreBase>())
		{
			if (!Modifier->IsModifierStack())
			{
				DraggedModifiers.Add(Modifier);
			}
		}
	}

	if (InDropZoneItem->IsA<UActorModifierCoreBase>())
	{
		UActorModifierCoreBase* DropModifier = InDropZoneItem->Get<UActorModifierCoreBase>();

		TArray<UActorModifierCoreBase*> MoveModifiers;
		TArray<UActorModifierCoreBase*> CloneModifiers;

		const EActorModifierCoreStackPosition Position = InZone == EItemDropZone::AboveItem ? EActorModifierCoreStackPosition::Before : EActorModifierCoreStackPosition::After;
		ModifierSubsystem->GetSortedModifiers(DraggedModifiers, DropModifier->GetModifiedActor(), DropModifier, Position, MoveModifiers, CloneModifiers);

		if (!MoveModifiers.IsEmpty())
		{
			return InZone;
		}
	}

	return Super::OnItemCanAcceptDrop(InDraggedItems, InDropZoneItem, InZone);
}

void UActorModifierCoreEditorStackCustomization::OnDropItem(const TArray<FOperatorStackEditorItemPtr>& InDraggedItems, const FOperatorStackEditorItemPtr& InDropZoneItem, EItemDropZone InZone)
{
	const UActorModifierCoreSubsystem* const ModifierSubsystem = UActorModifierCoreSubsystem::Get();

	TSet<UActorModifierCoreBase*> Modifiers;
	for (const FOperatorStackEditorItemPtr& Item : InDraggedItems)
	{
		if (!Item.IsValid())
		{
			continue;
		}

		if (!Item->IsA<UActorModifierCoreBase>())
		{
			continue;
		}

		if (UActorModifierCoreBase* Modifier = Item->Get<UActorModifierCoreBase>())
		{
			if (!Modifier->IsModifierStack())
			{
				Modifiers.Add(Modifier);
			}
		}
	}

	if (InDropZoneItem->IsA<UActorModifierCoreBase>())
	{
		UActorModifierCoreBase* DropModifier = InDropZoneItem->Get<UActorModifierCoreBase>();

		UActorModifierCoreStack* const TargetStack = DropModifier->GetModifierStack();

		if (!IsValid(ModifierSubsystem) || !IsValid(TargetStack))
		{
			return;
		}

		AActor* const TargetActor = TargetStack->GetModifiedActor();

		TArray<UActorModifierCoreBase*> MoveModifiers;
		TArray<UActorModifierCoreBase*> CloneModifiers;

		const EActorModifierCoreStackPosition Position = InZone == EItemDropZone::AboveItem ? EActorModifierCoreStackPosition::Before : EActorModifierCoreStackPosition::After;
		ModifierSubsystem->GetSortedModifiers(Modifiers, TargetActor, DropModifier, Position, MoveModifiers, CloneModifiers);

		if (MoveModifiers.IsEmpty())
		{
			return;
		}

		FText FailReason;
		FActorModifierCoreStackMoveOp MoveOp;
		MoveOp.bShouldTransact = true;
		MoveOp.FailReason = &FailReason;
		MoveOp.MovePosition = Position;
		MoveOp.MovePositionContext = DropModifier;

		ModifierSubsystem->MoveModifiers(MoveModifiers, TargetStack, MoveOp);

		if (!FailReason.IsEmpty())
		{
			FNotificationInfo NotificationInfo(FailReason);
			NotificationInfo.ExpireDuration = 3.f;
			NotificationInfo.bFireAndForget = true;
			FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		}
	}

	Super::OnDropItem(InDraggedItems, InDropZoneItem, InZone);
}

void UActorModifierCoreEditorStackCustomization::FillStackHeaderMenu(UToolMenu* InToolMenu) const
{
	if (!InToolMenu)
	{
		return;
	}

	UOperatorStackEditorMenuContext* const AddModifierContext = InToolMenu->FindContext<UOperatorStackEditorMenuContext>();
	if (!AddModifierContext)
	{
		return;
	}

	FOperatorStackEditorContextPtr Context = AddModifierContext->GetContext();
	if (!Context)
	{
		return;
	}

	const UActorModifierCoreEditorSubsystem* ModifierExtensionSubsystem = UActorModifierCoreEditorSubsystem::Get();
	if (!IsValid(ModifierExtensionSubsystem))
	{
		return;
	}

	TSet<TWeakObjectPtr<UObject>> ContextObjects;
	for (const FOperatorStackEditorItemPtr& ContextItem : Context->GetItems())
	{
		if (ContextItem->IsA<UObject>())
		{
			ContextObjects.Add(ContextItem->Get<UObject>());
		}
	}

	const FActorModifierCoreEditorMenuContext MenuContext(ContextObjects);
	FActorModifierCoreEditorMenuOptions MenuOptions(EActorModifierCoreEditorMenuType::Add);
	MenuOptions.CreateSubMenu(false);
	ModifierExtensionSubsystem->FillModifierMenu(InToolMenu, MenuContext, MenuOptions);
}

void UActorModifierCoreEditorStackCustomization::FillItemHeaderActionMenu(UToolMenu* InToolMenu) const
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

	UActorModifierCoreBase* Modifier = ItemContext->Get<UActorModifierCoreBase>();
	if (!IsValid(Modifier))
	{
		return;
	}

	// Add profiling stat toggle entry
	if (UActorModifierCoreStack* ModifierStack = Cast<UActorModifierCoreStack>(Modifier))
	{
		const FToolMenuEntry EnableProfilingModifierAction = FToolMenuEntry::InitToolBarButton(
			TEXT("EnableProfilingModifierMenuEntry")
			, FUIAction(
				FExecuteAction::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::ToggleModifierProfilingAction, ModifierStack)
				, FCanExecuteAction()
				, FIsActionChecked::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::IsModifierProfiling, ModifierStack))
			, FText::GetEmpty()
			, FText::GetEmpty()
			, FSlateIcon(FActorModifierCoreEditorStyle::Get().GetStyleSetName(), "Profiling")
			, EUserInterfaceActionType::ToggleButton
		);

		InToolMenu->AddMenuEntry(EnableProfilingModifierAction.Name, EnableProfilingModifierAction);
	}

	// Add remove modifier entry
	const FToolMenuEntry RemoveModifierAction = FToolMenuEntry::InitToolBarButton(
		TEXT("RemoveModifierMenuEntry")
		, FExecuteAction::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::RemoveModifierAction, Modifier)
		, FText::GetEmpty()
		, FText::GetEmpty()
		, FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Delete")
	);

	InToolMenu->AddMenuEntry(RemoveModifierAction.Name, RemoveModifierAction);
}

void UActorModifierCoreEditorStackCustomization::FillItemContextActionMenu(UToolMenu* InToolMenu) const
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

	UActorModifierCoreBase* Modifier = ItemContext->Get<UActorModifierCoreBase>();
	if (!IsValid(Modifier))
	{
		return;
	}

	if (UActorModifierCoreStack* ModifierStack = Cast<UActorModifierCoreStack>(Modifier))
	{
		const FToolMenuEntry EnableProfilingModifierAction = FToolMenuEntry::InitMenuEntry(
			TEXT("EnableProfilingModifierMenuEntry")
			, LOCTEXT("EnableProfilingModifier", "Toggle profiling")
			, FText::GetEmpty()
			, FSlateIcon(FActorModifierCoreEditorStyle::Get().GetStyleSetName(), "Profiling")
			, FUIAction(
				FExecuteAction::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::ToggleModifierProfilingAction, ModifierStack)
				, FCanExecuteAction()
				, FIsActionChecked::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::IsModifierProfiling, ModifierStack))
			, EUserInterfaceActionType::ToggleButton);

		InToolMenu->AddMenuEntry(EnableProfilingModifierAction.Name, EnableProfilingModifierAction);
	}

	// Link menu entry to commands list for delete
	{
		TSharedPtr<const FUICommandList> Commands;
		const TSharedPtr<FUICommandInfo> Command = FGenericCommands::Get().Delete;
		InToolMenu->Context.GetActionForCommand(Command, Commands);
		const FToolMenuEntry RemoveModifierMenuEntry = FToolMenuEntry::InitMenuEntryWithCommandList(Command, Commands);
		InToolMenu->AddMenuEntry(RemoveModifierMenuEntry.Name, RemoveModifierMenuEntry);
	}

	// Link menu entry to commands list for copy
	{
		TSharedPtr<const FUICommandList> Commands;
		const TSharedPtr<FUICommandInfo> Command = FGenericCommands::Get().Copy;
		InToolMenu->Context.GetActionForCommand(Command, Commands);
		const FToolMenuEntry CopyModifierMenuEntry = FToolMenuEntry::InitMenuEntryWithCommandList(Command, Commands);
		InToolMenu->AddMenuEntry(CopyModifierMenuEntry.Name, CopyModifierMenuEntry);
	}

	// Link menu entry to commands list for paste
	{
		TSharedPtr<const FUICommandList> Commands;
		const TSharedPtr<FUICommandInfo> Command = FGenericCommands::Get().Paste;
		InToolMenu->Context.GetActionForCommand(Command, Commands);
		const FToolMenuEntry PasteModifierMenuEntry = FToolMenuEntry::InitMenuEntryWithCommandList(Command, Commands);
		InToolMenu->AddMenuEntry(PasteModifierMenuEntry.Name, PasteModifierMenuEntry);
	}
}

bool UActorModifierCoreEditorStackCustomization::CanRemoveModifier(UActorModifierCoreBase* InModifier) const
{
	if (!IsValid(InModifier) || !IsValid(InModifier->GetModifiedActor()))
	{
		return false;
	}

	return true;
}

void UActorModifierCoreEditorStackCustomization::RemoveModifierAction(UActorModifierCoreBase* InModifier) const
{
	if (!CanRemoveModifier(InModifier))
	{
		return;
	}

	const UActorModifierCoreSubsystem* ModifierSubsystem = UActorModifierCoreSubsystem::Get();

	if (!IsValid(ModifierSubsystem))
	{
		return;
	}

	if (InModifier->IsModifierStack())
	{
		TSet<AActor*> Actors
		{
			InModifier->GetModifiedActor()
		};

		ModifierSubsystem->RemoveActorsModifiers(Actors, true);
	}
	else
	{
		TSet<UActorModifierCoreBase*> Modifiers;
		Modifiers.Add(InModifier);

		FText OutFailReason;
		FActorModifierCoreStackRemoveOp RemoveOp;
		RemoveOp.bShouldTransact = true;
		RemoveOp.FailReason = &OutFailReason;

		if (!ModifierSubsystem->RemoveModifiers(Modifiers, RemoveOp))
		{
			FNotificationInfo NotificationInfo(OutFailReason);
			NotificationInfo.ExpireDuration = 3.0f;
			NotificationInfo.bFireAndForget = true;
			FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		}
	}
}

bool UActorModifierCoreEditorStackCustomization::CanCopyModifier(UActorModifierCoreBase* InModifier) const
{
	if (!IsValid(InModifier) || InModifier->IsModifierStack())
	{
		return false;
	}

	return true;
}

void UActorModifierCoreEditorStackCustomization::CopyModifierAction(UActorModifierCoreBase* InModifier) const
{
	if (!CanCopyModifier(InModifier))
	{
		return;
	}

	TMap<FName, FString> ModifierPropertiesHandlesMap;
	const bool bSuccess = CreatePropertiesHandlesMapFromModifier(InModifier, ModifierPropertiesHandlesMap);

	if (!bSuccess)
	{
		return;
	}

	FString SerializedString;

	const TSharedRef<FJsonObject> RootJsonObject = MakeShared<FJsonObject>();

	FActorModifierCoreEditorPropertiesWrapper ModifierPropertiesWrapper;
	ModifierPropertiesWrapper.ModifierName = InModifier->GetModifierName();
	ModifierPropertiesWrapper.PropertiesHandlesAsStringMap = ModifierPropertiesHandlesMap;

	TSharedRef<FJsonObject> PropertiesJsonObject = MakeShared<FJsonObject>();
	FJsonObjectConverter::UStructToJsonObject(FActorModifierCoreEditorPropertiesWrapper::StaticStruct(), &ModifierPropertiesWrapper, PropertiesJsonObject, 0 /* CheckFlags */, 0 /* SkipFlags */);
	const TSharedPtr<FJsonValue> PropertiesJsonValue = MakeShared<FJsonValueObject>(PropertiesJsonObject);

	RootJsonObject->SetField(PropertiesWrapperEntry, PropertiesJsonValue);

	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&SerializedString);

	FJsonSerializer::Serialize(RootJsonObject, Writer);

	// Add Prefix to quickly identify whether current clipboard is from Tag Collection Properties or not
	SerializedString = *FString::Printf(TEXT("%s%s")
		, *PropertiesWrapperPrefix
		, *SerializedString);

	FPlatformApplicationMisc::ClipboardCopy(*SerializedString);
}

bool UActorModifierCoreEditorStackCustomization::CanPasteModifier(UActorModifierCoreBase* InModifier) const
{
	if (!IsValid(InModifier))
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);
	return ClipboardContent.StartsWith(PropertiesWrapperPrefix);
}

void UActorModifierCoreEditorStackCustomization::PasteModifierAction(UActorModifierCoreBase* InModifier) const
{
	if (!CanPasteModifier(InModifier))
	{
		return;
	}

	if (const UActorModifierCoreStack* ModifierStack = Cast<UActorModifierCoreStack>(InModifier))
	{
		TSet<AActor*> Actors { InModifier->GetModifiedActor() };
		if (!AddModifierFromClipboard(Actors))
		{
			return;
		}
		InModifier = ModifierStack->GetLastModifier();
	}

	FActorModifierCoreEditorPropertiesWrapper ModifierPropertiesWrapper;
	if (GetModifierPropertiesWrapperFromClipboard(ModifierPropertiesWrapper))
	{
		if (ModifierPropertiesWrapper.ModifierName == InModifier->GetModifierName())
		{
			FScopedTransaction Transaction(LOCTEXT("PasteModifierProperties", "Paste Modifier Properties"));
			InModifier->Modify();

			UpdateModifierFromPropertiesHandlesMap(InModifier, ModifierPropertiesWrapper.PropertiesHandlesAsStringMap);
		}
		else
		{
			const FString SourceModifierName = ModifierPropertiesWrapper.ModifierName.ToString();
			const FString DestinationModifierName = InModifier->GetModifierName().ToString();

			UE_LOG(LogActorModifierCoreEditorPropertiesWrapper, Warning, TEXT("Unable to copy properties from %s modifier to %s modifier"), *SourceModifierName, *DestinationModifierName);
		}
	}
}

bool UActorModifierCoreEditorStackCustomization::IsModifierProfiling(UActorModifierCoreStack* InStack) const
{
	if (!IsValid(InStack) || !IsValid(InStack->GetModifiedActor()))
	{
		return false;
	}

	return InStack->IsModifierProfiling();
}

void UActorModifierCoreEditorStackCustomization::ToggleModifierProfilingAction(UActorModifierCoreStack* InStack) const
{
	if (!IsValid(InStack) || !IsValid(InStack->GetModifiedActor()))
	{
		return;
	}

	InStack->SetModifierProfiling(!InStack->IsModifierProfiling());
}

TSharedRef<FUICommandList> UActorModifierCoreEditorStackCustomization::CreateModifierCommands(UActorModifierCoreBase* InModifier)
{
	TSharedRef<FUICommandList> Commands = MakeShared<FUICommandList>();

	Commands->MapAction(FGenericCommands::Get().Copy
		, FUIAction(
			FExecuteAction::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::CopyModifierAction, InModifier),
			FCanExecuteAction::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::CanCopyModifier, InModifier)
		)
	);

	Commands->MapAction(FGenericCommands::Get().Paste
		, FUIAction(
			FExecuteAction::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::PasteModifierAction, InModifier),
			FCanExecuteAction::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::CanPasteModifier, InModifier)
		)
	);

	Commands->MapAction(FGenericCommands::Get().Delete
		, FUIAction(
			FExecuteAction::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::RemoveModifierAction, InModifier),
			FCanExecuteAction::CreateUObject(this, &UActorModifierCoreEditorStackCustomization::CanRemoveModifier, InModifier)
		)
	);

	return Commands;
}

bool UActorModifierCoreEditorStackCustomization::CreatePropertiesHandlesMapFromModifier(UActorModifierCoreBase* InModifier, TMap<FName, FString>& OutModifierPropertiesHandlesMap) const
{
	if (!InModifier)
	{
		return false;
	}

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	// no need to keep the generator outside of this function, since we are converting handles to string
	const FPropertyRowGeneratorArgs RowGeneratorArgs;
	const TSharedRef<IPropertyRowGenerator> PropertyRowGenerator = PropertyEditor.CreatePropertyRowGenerator(RowGeneratorArgs);

	UObject* ModifierAsObj = Cast<UObject>(InModifier);
	check(ModifierAsObj);

	PropertyRowGenerator->SetObjects({ModifierAsObj});

	const TArray<TSharedRef<IDetailTreeNode>>& RootTreeNodes = PropertyRowGenerator->GetRootTreeNodes();

	for (const TSharedRef<IDetailTreeNode>& RootTreeNode : RootTreeNodes)
	{
		TArray<TSharedRef<IDetailTreeNode>> ChildrenNodes;
		RootTreeNode->GetChildren(ChildrenNodes);

		for (const TSharedRef<IDetailTreeNode>& Node : ChildrenNodes )
		{
			if (const TSharedPtr<IPropertyHandle>& PropertyHandle = Node->CreatePropertyHandle())
			{
				FName PropertyName = PropertyHandle->GetProperty()->NamePrivate;

				FString PropertyValueAsString;
				if (PropertyHandle->GetValueAsFormattedString(PropertyValueAsString, PPF_Copy) == FPropertyAccess::Success)
				{
					OutModifierPropertiesHandlesMap.Add(PropertyName, PropertyValueAsString);
				}
			}
		}
	}

	return true;
}

bool UActorModifierCoreEditorStackCustomization::GetModifierPropertiesWrapperFromClipboard(FActorModifierCoreEditorPropertiesWrapper& OutPropertiesWrapper) const
{
	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	// remove prefix, this is not part of the modifier properties json data
	ClipboardContent.RightChopInline(PropertiesWrapperPrefix.Len());

	TSharedPtr<FJsonObject> RootJsonObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ClipboardContent);

	if (!FJsonSerializer::Deserialize(Reader, RootJsonObject))
	{
		UE_LOG(LogActorModifierCoreEditorPropertiesWrapper, Warning, TEXT("Unable to serialize the pasted text into Json format"));
		return false;
	}

	const TSharedPtr<FJsonValue> PropertiesJsonValue = RootJsonObject->TryGetField(PropertiesWrapperEntry);

	if (!PropertiesJsonValue.IsValid())
	{
		UE_LOG(LogActorModifierCoreEditorPropertiesWrapper, Warning, TEXT("Missing %s entry field in pasted text"), *PropertiesWrapperEntry);
		return false;
	}

	if (PropertiesJsonValue->Type != EJson::Object)
	{
		UE_LOG(LogActorModifierCoreEditorPropertiesWrapper, Warning, TEXT("Invalid Modifier Properties Json Value. Not an object"));
		return false;
	}

	const TSharedPtr<FJsonObject>& PropertiesJsonObject = PropertiesJsonValue->AsObject();
	check(PropertiesJsonObject.IsValid());

	const bool bStructSuccess = FJsonObjectConverter::JsonObjectToUStruct(PropertiesJsonObject.ToSharedRef(), FActorModifierCoreEditorPropertiesWrapper::StaticStruct(), &OutPropertiesWrapper, 0 /* CheckFlags */, 0 /* SkipFlags */);

	return bStructSuccess;
}

bool UActorModifierCoreEditorStackCustomization::UpdateModifierFromPropertiesHandlesMap(UActorModifierCoreBase* InModifier, const TMap<FName, FString>& InModifierPropertiesHandlesMap) const
{
	if (!InModifier)
	{
		return false;
	}

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	// no need to keep the generator outside of this function, since we are converting handles to string
	const FPropertyRowGeneratorArgs RowGeneratorArgs;
	const TSharedRef<IPropertyRowGenerator> PropertyRowGenerator = PropertyEditor.CreatePropertyRowGenerator(RowGeneratorArgs);

	UObject* ModifierAsObj = Cast<UObject>(InModifier);
	check(ModifierAsObj);

	PropertyRowGenerator->SetObjects({ModifierAsObj});

	const TArray<TSharedRef<IDetailTreeNode>>& RootTreeNodes = PropertyRowGenerator->GetRootTreeNodes();

	for (const TSharedRef<IDetailTreeNode>& RootTreeNode : RootTreeNodes)
	{
		TArray<TSharedRef<IDetailTreeNode>> ChildrenNodes;
		RootTreeNode->GetChildren(ChildrenNodes);

		for (const TSharedRef<IDetailTreeNode>& Node : ChildrenNodes )
		{
			if (const TSharedPtr<IPropertyHandle>& PropertyHandle = Node->CreatePropertyHandle())
			{
				FName PropertyName = PropertyHandle->GetProperty()->NamePrivate;

				if (InModifierPropertiesHandlesMap.Contains(PropertyName))
				{
					FString PropertyValueAsString = InModifierPropertiesHandlesMap[PropertyName];
					PropertyHandle->SetValueFromFormattedString(PropertyValueAsString, EPropertyValueSetFlags::InstanceObjects);
				}
			}
		}
	}

	return true;
}

bool UActorModifierCoreEditorStackCustomization::AddModifierFromClipboard(TSet<AActor*>& InActors) const
{
	const UActorModifierCoreSubsystem* ModifierSubsystem = UActorModifierCoreSubsystem::Get();
	if (!IsValid(ModifierSubsystem))
	{
		return false;
	}

	FActorModifierCoreEditorPropertiesWrapper ModifierPropertiesWrapper;
	if (GetModifierPropertiesWrapperFromClipboard(ModifierPropertiesWrapper))
	{
		const FName ModifierName = ModifierPropertiesWrapper.ModifierName;

		FText OutFailReason;
		FActorModifierCoreStackInsertOp AddOp;
		AddOp.bShouldTransact = true;
		AddOp.FailReason = &OutFailReason;
		AddOp.NewModifierName = ModifierName;

		ModifierSubsystem->AddActorsModifiers(InActors, AddOp);

		if (!OutFailReason.IsEmpty())
		{
			FNotificationInfo NotificationInfo(OutFailReason);
			NotificationInfo.ExpireDuration = 3.0f;
			NotificationInfo.bFireAndForget = true;
			FSlateNotificationManager::Get().AddNotification(NotificationInfo);
		}

		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
