// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/PropertyAnimatorCoreEditorSubsystem.h"

#include "Components/PropertyAnimatorCoreComponent.h"
#include "DetailRowMenuContext.h"
#include "Editor.h"
#include "Framework/Application/SlateApplication.h"
#include "IDetailTreeNode.h"
#include "Menus/PropertyAnimatorCoreEditorMenu.h"
#include "Menus/PropertyAnimatorCoreEditorMenuContext.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "Styles/PropertyAnimatorCoreEditorStyle.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "Widgets/SPropertyAnimatorCoreEditorEditPanel.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreEditorSubsystem"

FPropertyAnimatorCoreEditorEditPanelOptions& UPropertyAnimatorCoreEditorSubsystem::OpenPropertyControlWindow()
{
	TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> PropertyControlPanel = PropertyControllerPanelWeak.Pin();

	if (!PropertyControllerPanelWeak.IsValid())
	{
		PropertyControlPanel = SPropertyAnimatorCoreEditorEditPanel::OpenWindow();
		PropertyControllerPanelWeak = PropertyControlPanel;
	}

	PropertyControlPanel->FocusWindow();

	return PropertyControlPanel->GetOptions();
}

void UPropertyAnimatorCoreEditorSubsystem::ClosePropertyControlWindow() const
{
	if (const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> PropertyControllerPanel = PropertyControllerPanelWeak.Pin())
	{
		PropertyControllerPanel->CloseWindow();
	}
}

bool UPropertyAnimatorCoreEditorSubsystem::IsPropertyControlWindowOpened() const
{
	return PropertyControllerPanelWeak.IsValid();
}

bool UPropertyAnimatorCoreEditorSubsystem::FillAnimatorMenu(UToolMenu* InMenu, const FPropertyAnimatorCoreEditorMenuContext& InContext, const FPropertyAnimatorCoreEditorMenuOptions& InOptions)
{
	if (!InMenu || InContext.IsEmpty())
	{
		return false;
	}

	LastMenuData = MakeShared<FPropertyAnimatorCoreEditorMenuData>(InContext, InOptions);

	FToolMenuSection* AnimatorSection = nullptr;
	if (InOptions.ShouldCreateSubMenu())
	{
		static const FName AnimatorSectionName("ContextAnimatorActions");

		AnimatorSection = InMenu->FindSection(AnimatorSectionName);
		if (!AnimatorSection)
		{
			AnimatorSection = &InMenu->AddSection(AnimatorSectionName
				, LOCTEXT("ContextAnimatorActions", "Animators Actions")
				, FToolMenuInsert(NAME_None, EToolMenuInsertType::First));
		}
	}

	if (InOptions.IsMenuType(EPropertyAnimatorCoreEditorMenuType::Edit))
	{
		if (InOptions.ShouldCreateSubMenu())
		{
			AnimatorSection->AddSubMenu(
				TEXT("EditAnimatorMenu"),
				LOCTEXT("EditAnimatorMenu.Label", "Edit Animators"),
				LOCTEXT("EditAnimatorMenu.Tooltip", "Edit Animators"),
				FNewToolMenuDelegate::CreateLambda(&UE::PropertyAnimatorCoreEditor::Menu::FillEditAnimatorSection, LastMenuData.ToSharedRef()));
		}
		else
		{
			UE::PropertyAnimatorCoreEditor::Menu::FillEditAnimatorSection(InMenu, LastMenuData.ToSharedRef());
		}
	}

	if (InOptions.IsMenuType(EPropertyAnimatorCoreEditorMenuType::New) && InContext.ContainsAnyActor())
	{
		if (InOptions.ShouldCreateSubMenu())
		{
			AnimatorSection->AddSubMenu(
				TEXT("NewAnimatorMenu"),
				LOCTEXT("NewAnimatorMenu.Label", "Add Animators"),
				LOCTEXT("NewAnimatorMenu.Tooltip", "Add animators to the selection"),
				FNewToolMenuDelegate::CreateLambda(&UE::PropertyAnimatorCoreEditor::Menu::FillNewAnimatorSection, LastMenuData.ToSharedRef()));
		}
		else
		{
			UE::PropertyAnimatorCoreEditor::Menu::FillNewAnimatorSection(InMenu, LastMenuData.ToSharedRef());
		}
	}

	if (InOptions.IsMenuType(EPropertyAnimatorCoreEditorMenuType::Existing) && InContext.ContainsAnyProperty())
	{
		if (InOptions.ShouldCreateSubMenu())
		{
			AnimatorSection->AddSubMenu(
				TEXT("ExistingAnimatorMenu"),
				LOCTEXT("ExistingAnimatorMenu.Label", "Existing Animators"),
				LOCTEXT("ExistingAnimatorMenu.Tooltip", "Link or unlink selection to/from existing animators"),
				FNewToolMenuDelegate::CreateLambda(&UE::PropertyAnimatorCoreEditor::Menu::FillExistingAnimatorSection, LastMenuData.ToSharedRef()));
		}
		else
		{
			UE::PropertyAnimatorCoreEditor::Menu::FillExistingAnimatorSection(InMenu, LastMenuData.ToSharedRef());
		}
	}

	if (InOptions.IsMenuType(EPropertyAnimatorCoreEditorMenuType::Link) && InContext.ContainsAnyAnimator())
	{
		if (InOptions.ShouldCreateSubMenu())
		{
			AnimatorSection->AddSubMenu(
				TEXT("LinkAnimatorMenu"),
				LOCTEXT("LinkAnimatorMenu.Label", "Link Animators"),
				LOCTEXT("LinkAnimatorMenu.Tooltip", "Link selection to/from animators"),
				FNewToolMenuDelegate::CreateLambda(&UE::PropertyAnimatorCoreEditor::Menu::FillLinkAnimatorSection, LastMenuData.ToSharedRef()));
		}
		else
		{
			UE::PropertyAnimatorCoreEditor::Menu::FillLinkAnimatorSection(InMenu, LastMenuData.ToSharedRef());
		}
	}

	if (InOptions.IsMenuType(EPropertyAnimatorCoreEditorMenuType::Enable) && InContext.ContainsAnyDisabledAnimator())
	{
		if (InOptions.ShouldCreateSubMenu())
		{
			AnimatorSection->AddSubMenu(
				TEXT("EnableAnimatorMenu"),
				LOCTEXT("EnableAnimatorMenu.Label", "Enable Animators"),
				LOCTEXT("EnableAnimatorMenu.Tooltip", "Enable selected animators"),
				FNewToolMenuDelegate::CreateLambda(&UE::PropertyAnimatorCoreEditor::Menu::FillEnableAnimatorSection, LastMenuData.ToSharedRef()));
		}
		else
		{
			UE::PropertyAnimatorCoreEditor::Menu::FillEnableAnimatorSection(InMenu, LastMenuData.ToSharedRef());
		}
	}

	if (InOptions.IsMenuType(EPropertyAnimatorCoreEditorMenuType::Disable) && InContext.ContainsAnyEnabledAnimator())
	{
		if (InOptions.ShouldCreateSubMenu())
		{
			AnimatorSection->AddSubMenu(
				TEXT("DisableAnimatorMenu"),
				LOCTEXT("DisableAnimatorMenu.Label", "Disable Animators"),
				LOCTEXT("DisableAnimatorMenu.Tooltip", "Disable selected animators"),
				FNewToolMenuDelegate::CreateLambda(&UE::PropertyAnimatorCoreEditor::Menu::FillDisableAnimatorSection, LastMenuData.ToSharedRef()));
		}
		else
		{
			UE::PropertyAnimatorCoreEditor::Menu::FillDisableAnimatorSection(InMenu, LastMenuData.ToSharedRef());
		}
	}

	if (InOptions.IsMenuType(EPropertyAnimatorCoreEditorMenuType::Delete) && InContext.ContainsAnyComponentAnimator())
	{
		if (InOptions.ShouldCreateSubMenu())
		{
			AnimatorSection->AddSubMenu(
				TEXT("DeleteAnimatorMenu"),
				LOCTEXT("DeleteAnimatorMenu.Label", "Delete Animators"),
				LOCTEXT("DeleteAnimatorMenu.Tooltip", "Delete selected animators"),
				FNewToolMenuDelegate::CreateLambda(&UE::PropertyAnimatorCoreEditor::Menu::FillDeleteAnimatorSection, LastMenuData.ToSharedRef()));
		}
		else
		{
			UE::PropertyAnimatorCoreEditor::Menu::FillDeleteAnimatorSection(InMenu, LastMenuData.ToSharedRef());
		}
	}

	return false;
}

UPropertyAnimatorCoreEditorSubsystem* UPropertyAnimatorCoreEditorSubsystem::Get()
{
	if (GEditor)
	{
		return GEditor->GetEditorSubsystem<UPropertyAnimatorCoreEditorSubsystem>();
	}
	return nullptr;
}

void UPropertyAnimatorCoreEditorSubsystem::RegisterDetailPanelCustomization()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	OnGetGlobalRowExtensionHandle = PropertyEditor.GetGlobalRowExtensionDelegate().AddUObject(this, &UPropertyAnimatorCoreEditorSubsystem::OnGetGlobalRowExtension);
}

void UPropertyAnimatorCoreEditorSubsystem::UnregisterDetailPanelCustomization()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.GetGlobalRowExtensionDelegate().Remove(OnGetGlobalRowExtensionHandle);
	OnGetGlobalRowExtensionHandle.Reset();
}

void UPropertyAnimatorCoreEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Initialize here, to setup ClassIcons if no other code uses it
	FPropertyAnimatorCoreEditorStyle::Get();

	RegisterDetailPanelCustomization();
}

void UPropertyAnimatorCoreEditorSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UnregisterDetailPanelCustomization();
}

void UPropertyAnimatorCoreEditorSubsystem::OnGetGlobalRowExtension(const FOnGenerateGlobalRowExtensionArgs& InArgs, TArray<FPropertyRowExtensionButton>& OutExtensions)
{
	TSharedPtr<IPropertyHandle> PropertyHandle = InArgs.PropertyHandle;
	if (!PropertyHandle.IsValid())
	{
		return;
	}

	// Extend context row menu
	ExtendPropertyRowContextMenu();

	TWeakPtr<IDetailTreeNode> OwnerTreeNodeWeak = InArgs.OwnerTreeNode;

	const FText Label = LOCTEXT("PropertyAnimatorCoreEditorExtension.Label", "Edit Animators");
	const FText Tooltip = LOCTEXT("PropertyAnimatorCoreEditorExtension.Tooltip", "Edit animators for this property");

	// Set custom icon when linked or not
	const TAttribute<FSlateIcon> Icon = MakeAttributeLambda([this, OwnerTreeNodeWeak, PropertyHandle]()
	{
		static const FSlateIcon ControlPropertyIcon(FPropertyAnimatorCoreEditorStyle::Get().GetStyleSetName(), "PropertyControlIcon.Default");
		static const FSlateIcon LinkedControlPropertyIcon(FPropertyAnimatorCoreEditorStyle::Get().GetStyleSetName(), "PropertyControlIcon.Linked");

		return IsControlPropertyLinked(OwnerTreeNodeWeak, PropertyHandle) ? LinkedControlPropertyIcon : ControlPropertyIcon;
	});

	const FUIAction Action = FUIAction(
		FExecuteAction::CreateUObject(this, &UPropertyAnimatorCoreEditorSubsystem::OnControlPropertyClicked, OwnerTreeNodeWeak, PropertyHandle),
		FCanExecuteAction::CreateUObject(this, &UPropertyAnimatorCoreEditorSubsystem::IsControlPropertySupported, OwnerTreeNodeWeak, PropertyHandle),
		FGetActionCheckState(),
		FIsActionButtonVisible::CreateUObject(this, &UPropertyAnimatorCoreEditorSubsystem::IsControlPropertyVisible, OwnerTreeNodeWeak, PropertyHandle)
	);

	FPropertyRowExtensionButton& AnimatePropertyButton = OutExtensions.AddDefaulted_GetRef();
	AnimatePropertyButton.Label = Label;
	AnimatePropertyButton.ToolTip = Tooltip;
	AnimatePropertyButton.Icon = Icon;
	AnimatePropertyButton.UIAction = Action;
}

void UPropertyAnimatorCoreEditorSubsystem::OnControlPropertyClicked(TWeakPtr<IDetailTreeNode> InOwnerTreeNode, TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	TOptional<FPropertyAnimatorCoreData> PropertyData = GetPropertyData(InPropertyHandle);
	if (!PropertyData.IsSet())
	{
		return;
	}

	const TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
	if (!ActiveWindow)
	{
		return;
	}

	// Open context menu
	const FVector2D MenuLocation = FSlateApplication::Get().GetCursorPos() + FVector2D(0, 16);

	FSlateApplication::Get().PushMenu(
		ActiveWindow.ToSharedRef(),
		FWidgetPath(),
		GenerateContextMenuWidget(PropertyData.GetValue()),
		MenuLocation,
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
	);
}

bool UPropertyAnimatorCoreEditorSubsystem::IsControlPropertySupported(TWeakPtr<IDetailTreeNode> InOwnerTreeNode, TSharedPtr<IPropertyHandle> InPropertyHandle) const
{
	TOptional<FPropertyAnimatorCoreData> PropertyData = GetPropertyData(InPropertyHandle);
	if (!PropertyData.IsSet())
	{
		return false;
	}

	const UPropertyAnimatorCoreSubsystem* Subsystem = UPropertyAnimatorCoreSubsystem::Get();
	if (!Subsystem)
	{
		return false;
	}

	return Subsystem->IsPropertySupported(PropertyData.GetValue());
}

bool UPropertyAnimatorCoreEditorSubsystem::IsControlPropertyVisible(TWeakPtr<IDetailTreeNode> InOwnerTreeNode, TSharedPtr<IPropertyHandle> InPropertyHandle) const
{
	return IsControlPropertyLinked(InOwnerTreeNode, InPropertyHandle);
}

bool UPropertyAnimatorCoreEditorSubsystem::IsControlPropertyLinked(TWeakPtr<IDetailTreeNode> InOwnerTreeNode, TSharedPtr<IPropertyHandle> InPropertyHandle) const
{
	TOptional<FPropertyAnimatorCoreData> PropertyData = GetPropertyData(InPropertyHandle);
	if (!PropertyData.IsSet())
	{
		return false;
	}

	const UPropertyAnimatorCoreSubsystem* Subsystem = UPropertyAnimatorCoreSubsystem::Get();
	if (!Subsystem)
	{
		return false;
	}

	for (const UPropertyAnimatorCoreBase* Controller : Subsystem->GetExistingAnimators(PropertyData.GetValue().GetOwningActor()))
	{
		if (Controller->IsPropertyLinked(PropertyData.GetValue()))
		{
			return true;
		}

		if (!Controller->GetInnerPropertiesLinked(PropertyData.GetValue()).IsEmpty())
		{
			return true;
		}
	}

	return false;
}

TSharedRef<SWidget> UPropertyAnimatorCoreEditorSubsystem::GenerateContextMenuWidget(const FPropertyAnimatorCoreData& InProperty)
{
	UToolMenus* Menus = UToolMenus::Get();

	check(Menus);

	static const FName AnimatorExtensionMenuName = TEXT("AnimatorExtensionMenu");

	if (!Menus->IsMenuRegistered(AnimatorExtensionMenuName))
	{
		UToolMenu* const DetailsViewExtensionMenu = Menus->RegisterMenu(AnimatorExtensionMenuName, NAME_None, EMultiBoxType::Menu);

		DetailsViewExtensionMenu->AddDynamicSection(
			TEXT("FillAnimatorExtensionSection")
			, FNewToolMenuDelegate::CreateUObject(this, &UPropertyAnimatorCoreEditorSubsystem::FillAnimatorExtensionSection)
		);
	}

	UPropertyAnimatorCoreEditorMenuContext* MenuContext = NewObject<UPropertyAnimatorCoreEditorMenuContext>();
	MenuContext->SetPropertyData(InProperty);

	const FToolMenuContext ToolMenuContext(MenuContext);
	return Menus->GenerateWidget(AnimatorExtensionMenuName, ToolMenuContext);
}

void UPropertyAnimatorCoreEditorSubsystem::ExtendPropertyRowContextMenu()
{
	UToolMenus* Menus = UToolMenus::Get();

	check(Menus);

	if (UToolMenu* ContextMenu = Menus->FindMenu(UE::PropertyEditor::RowContextMenuName))
	{
		ContextMenu->AddDynamicSection(
			TEXT("FillAnimatorRowContextSection")
			, FNewToolMenuDelegate::CreateUObject(this, &UPropertyAnimatorCoreEditorSubsystem::FillAnimatorRowContextSection));
	}
}

void UPropertyAnimatorCoreEditorSubsystem::FillAnimatorExtensionSection(UToolMenu* InToolMenu)
{
	if (!InToolMenu)
	{
		return;
	}

	const UPropertyAnimatorCoreEditorMenuContext* const Context = InToolMenu->FindContext<UPropertyAnimatorCoreEditorMenuContext>();

	if (!Context)
	{
		return;
	}

	const FPropertyAnimatorCoreEditorMenuContext MenuContext({}, {Context->GetPropertyData()});
	const FPropertyAnimatorCoreEditorMenuOptions MenuOptions({EPropertyAnimatorCoreEditorMenuType::Edit, EPropertyAnimatorCoreEditorMenuType::New, EPropertyAnimatorCoreEditorMenuType::Existing});
	FillAnimatorMenu(InToolMenu, MenuContext, MenuOptions);
}

void UPropertyAnimatorCoreEditorSubsystem::FillAnimatorRowContextSection(UToolMenu* InToolMenu)
{
	if (!InToolMenu)
	{
		return;
	}

	// For context menu in details view
	const UDetailRowMenuContext* Context = InToolMenu->FindContext<UDetailRowMenuContext>();

	if (!Context || Context->PropertyHandles.IsEmpty())
	{
		return;
	}

	TOptional<FPropertyAnimatorCoreData> PropertyData = GetPropertyData(Context->PropertyHandles[0], true);

	if (!PropertyData.IsSet())
	{
		return;
	}

	const FPropertyAnimatorCoreEditorMenuContext MenuContext({}, {PropertyData.GetValue()});
	const FPropertyAnimatorCoreEditorMenuOptions MenuOptions({EPropertyAnimatorCoreEditorMenuType::Edit, EPropertyAnimatorCoreEditorMenuType::New, EPropertyAnimatorCoreEditorMenuType::Existing});
	FillAnimatorMenu(InToolMenu, MenuContext, MenuOptions);
}

TOptional<FPropertyAnimatorCoreData> UPropertyAnimatorCoreEditorSubsystem::GetPropertyData(TSharedPtr<IPropertyHandle> InPropertyHandle, bool bInFindMemberProperty) const
{
	if (!InPropertyHandle.IsValid() || !InPropertyHandle->IsValidHandle())
	{
		return TOptional<FPropertyAnimatorCoreData>();
	}

	const FProperty* Property = InPropertyHandle->GetProperty();
	if (!Property)
	{
		return TOptional<FPropertyAnimatorCoreData>();
	}

	TArray<UObject*> OutOwners;
	InPropertyHandle->GetOuterObjects(OutOwners);

	if (OutOwners.Num() != 1)
	{
		return TOptional<FPropertyAnimatorCoreData>();
	}

	UObject* Owner = OutOwners[0];
	if (!IsValid(Owner))
	{
		return TOptional<FPropertyAnimatorCoreData>();
	}

	if (Owner->IsA<UPropertyAnimatorCoreComponent>() || Owner->GetTypedOuter<UPropertyAnimatorCoreComponent>())
	{
		return TOptional<FPropertyAnimatorCoreData>();
	}

	const FString OwnerPath = Owner->GetPathName();
	if (OwnerPath.IsEmpty())
	{
		return TOptional<FPropertyAnimatorCoreData>();
	}

	const FString PropertyPath = InPropertyHandle->GeneratePathToProperty();
	if (PropertyPath.IsEmpty())
	{
		return TOptional<FPropertyAnimatorCoreData>();
	}

	// Climbs up the tree to find a member property handle that is a direct child of an object property
	TFunction<TSharedPtr<IPropertyHandle>(TSharedPtr<IPropertyHandle>, bool)> FindMemberProperty = [&FindMemberProperty](TSharedPtr<IPropertyHandle> InHandle, bool bInRecurse)->TSharedPtr<IPropertyHandle>
	{
		if (!InHandle.IsValid() || !InHandle->IsValidHandle() || !InHandle->GetProperty())
		{
			return nullptr;
		}

		const TSharedPtr<IPropertyHandle> ParentHandle = InHandle->GetParentHandle();
		if (ParentHandle.IsValid() && ParentHandle->GetProperty() && !ParentHandle->GetProperty()->IsA<FObjectProperty>())
		{
			return bInRecurse ? FindMemberProperty(ParentHandle, bInRecurse) : nullptr;
		}

		return InHandle;
	};

	const TSharedPtr<IPropertyHandle> MemberPropertyHandle = FindMemberProperty(InPropertyHandle, bInFindMemberProperty);

	if (!MemberPropertyHandle.IsValid() || !MemberPropertyHandle->IsValidHandle())
	{
		return TOptional<FPropertyAnimatorCoreData>();
	}

	const FPropertyAnimatorCoreData PropertyData(Owner, MemberPropertyHandle->GetProperty(), nullptr);

	// We need a setter to control a property
	if (!PropertyData.HasSetter())
	{
		return TOptional<FPropertyAnimatorCoreData>();
	}

	return PropertyData;
}

#undef LOCTEXT_NAMESPACE
