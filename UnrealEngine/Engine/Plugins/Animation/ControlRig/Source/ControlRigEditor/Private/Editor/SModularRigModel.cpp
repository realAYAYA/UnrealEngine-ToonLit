// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/SModularRigModel.h"
#include "Widgets/Input/SComboButton.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSearchBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ScopedTransaction.h"
#include "Editor/ControlRigEditor.h"
#include "BlueprintActionDatabase.h"
#include "BlueprintVariableNodeSpawner.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "K2Node_VariableGet.h"
#include "RigVMBlueprintUtils.h"
#include "ControlRigModularRigCommands.h"
#include "ControlRigBlueprint.h"
#include "Graph/ControlRigGraph.h"
#include "Graph/ControlRigGraphNode.h"
#include "Graph/ControlRigGraphSchema.h"
#include "GraphEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AnimationRuntime.h"
#include "ClassViewerFilter.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Application/SlateApplication.h"
#include "Editor/EditorEngine.h"
#include "HelperUtil.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "ControlRig.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformTime.h"
#include "Dialogs/Dialogs.h"
#include "IPersonaToolkit.h"
#include "SKismetInspector.h"
#include "Types/WidgetActiveTimerDelegate.h"
#include "Dialog/SCustomDialog.h"
#include "EditMode/ControlRigEditMode.h"
#include "ToolMenus.h"
#include "Editor/ControlRigContextMenuContext.h"
#include "Editor/SRigSpacePickerWidget.h"
#include "Settings/ControlRigSettings.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Styling/AppStyle.h"
#include "ControlRigSkeletalMeshComponent.h"
#include "Sequencer/ControlRigLayerInstance.h"
#include "Algo/MinElement.h"
#include "Algo/MaxElement.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "Kismet2/SClassPickerDialog.h"
#include "RigVMFunctions/Math/RigVMMathLibrary.h"
#include "Preferences/PersonaOptions.h"

#define LOCTEXT_NAMESPACE "SModularRigModel"

///////////////////////////////////////////////////////////

const FName SModularRigModel::ContextMenuName = TEXT("ControlRigEditor.ModularRigModel.ContextMenu");

SModularRigModel::~SModularRigModel()
{
	const FControlRigEditor* Editor = ControlRigEditor.IsValid() ? ControlRigEditor.Pin().Get() : nullptr;
	OnEditorClose(Editor, ControlRigBlueprint.Get());
}

void SModularRigModel::Construct(const FArguments& InArgs, TSharedRef<FControlRigEditor> InControlRigEditor)
{
	ControlRigEditor = InControlRigEditor;

	ControlRigBlueprint = ControlRigEditor.Pin()->GetControlRigBlueprint();

	ControlRigBlueprint->OnRefreshEditor().AddRaw(this, &SModularRigModel::HandleRefreshEditorFromBlueprint);
	ControlRigBlueprint->OnSetObjectBeingDebugged().AddRaw(this, &SModularRigModel::HandleSetObjectBeingDebugged);
	ControlRigBlueprint->OnModularRigPreCompiled().AddRaw(this, &SModularRigModel::HandlePreCompileModularRigs);
	ControlRigBlueprint->OnModularRigCompiled().AddRaw(this, &SModularRigModel::HandlePostCompileModularRigs);

	// for deleting, renaming, dragging
	CommandList = MakeShared<FUICommandList>();

	UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
	if (Editor != nullptr)
	{
		Editor->RegisterForUndo(this);
	}

	BindCommands();

	// setup all delegates for the modular rig model widget
	FModularRigTreeDelegates Delegates;
	Delegates.OnGetModularRig = FOnGetModularRigTreeRig::CreateSP(this, &SModularRigModel::GetModularRigForTreeView);
	Delegates.OnContextMenuOpening = FOnContextMenuOpening::CreateSP(this, &SModularRigModel::CreateContextMenuWidget);
	Delegates.OnDragDetected = FOnDragDetected::CreateSP(this, &SModularRigModel::OnDragDetected);
	Delegates.OnCanAcceptDrop = FOnModularRigTreeCanAcceptDrop::CreateSP(this, &SModularRigModel::OnCanAcceptDrop);
	Delegates.OnAcceptDrop = FOnModularRigTreeAcceptDrop::CreateSP(this, &SModularRigModel::OnAcceptDrop);
	Delegates.OnMouseButtonClick = FOnModularRigTreeMouseButtonClick::CreateSP(this, &SModularRigModel::OnItemClicked);
	Delegates.OnMouseButtonDoubleClick = FOnModularRigTreeMouseButtonClick::CreateSP(this, &SModularRigModel::OnItemDoubleClicked);
	Delegates.OnRequestDetailsInspection = FOnModularRigTreeRequestDetailsInspection::CreateSP(this, &SModularRigModel::OnRequestDetailsInspection);
	Delegates.OnRenameElement = FOnModularRigTreeRenameElement::CreateSP(this, &SModularRigModel::HandleRenameModule);
	Delegates.OnVerifyModuleNameChanged = FOnModularRigTreeVerifyElementNameChanged::CreateSP(this, &SModularRigModel::HandleVerifyNameChanged);
	Delegates.OnResolveConnector = FOnModularRigTreeResolveConnector::CreateSP(this, &SModularRigModel::HandleConnectorResolved);
	Delegates.OnDisconnectConnector = FOnModularRigTreeDisconnectConnector::CreateSP(this, &SModularRigModel::HandleConnectorDisconnect);

	HeaderRowWidget = SNew(SHeaderRow)
		.Visibility(EVisibility::Visible);

	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SModularRigTreeView::Column_Module)
		.DefaultLabel(FText::FromName(SModularRigTreeView::Column_Module))
		.HAlignCell(HAlign_Left)
		.HAlignHeader(HAlign_Left)
		.VAlignCell(VAlign_Fill)
	);
	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SModularRigTreeView::Column_Connector)
		.DefaultLabel(FText::FromName(SModularRigTreeView::Column_Connector))
		.HAlignCell(HAlign_Left)
		.HAlignHeader(HAlign_Left)
		.VAlignCell(VAlign_Center)
	);
	HeaderRowWidget->AddColumn(
		SHeaderRow::Column(SModularRigTreeView::Column_Buttons)
		.DefaultLabel(FText::FromName(SModularRigTreeView::Column_Buttons))
		.ManualWidth(60)
		.HAlignCell(HAlign_Left)
		.HAlignHeader(HAlign_Left)
		.VAlignCell(VAlign_Center)
	);
	
	ChildSlot
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.Padding(0.0f, 0.0f)
		[
			SNew(SBorder)
			.Padding(0.0f)
			.ShowEffectWhenDisabled(false)
			[
				SNew(SBorder)
				.Padding(2.0f)
				.BorderImage(FAppStyle::GetBrush("SCSEditor.TreePanel"))
				[
					SAssignNew(TreeView, SModularRigTreeView)
					.HeaderRow(HeaderRowWidget)
					.RigTreeDelegates(Delegates)
					.AutoScrollEnabled(true)
				]
			]
		]
	];

	RefreshTreeView();

	if (ControlRigEditor.IsValid())
	{
		ControlRigEditor.Pin()->GetKeyDownDelegate().BindLambda([&](const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)->FReply {
			return OnKeyDown(MyGeometry, InKeyEvent);
		});
		ControlRigEditor.Pin()->OnGetViewportContextMenu().BindSP(this, &SModularRigModel::GetContextMenu);
		ControlRigEditor.Pin()->OnViewportContextMenuCommands().BindSP(this, &SModularRigModel::GetContextMenuCommands);
		ControlRigEditor.Pin()->OnEditorClosed().AddSP(this, &SModularRigModel::OnEditorClose);
	}
	
	CreateContextMenu();

	if(const UModularRig* Rig = GetModularRigForTreeView())
	{
		if(URigHierarchy* Hierarchy = Rig->GetHierarchy())
		{
			Hierarchy->OnModified().AddSP(this, &SModularRigModel::OnHierarchyModified);
		}
	}
}

void SModularRigModel::OnEditorClose(const FRigVMEditor* InEditor, URigVMBlueprint* InBlueprint)
{
	if (InEditor)
	{
		FControlRigEditor* Editor = (FControlRigEditor*)InEditor;  
		Editor->OnGetViewportContextMenu().Unbind();
		Editor->OnViewportContextMenuCommands().Unbind();
	}

	if (UControlRigBlueprint* BP = Cast<UControlRigBlueprint>(InBlueprint))
	{
		InBlueprint->OnRefreshEditor().RemoveAll(this);
		InBlueprint->OnSetObjectBeingDebugged().RemoveAll(this);
		BP->OnModularRigPreCompiled().RemoveAll(this);
		BP->OnModularRigCompiled().RemoveAll(this);
	}

	if(const UModularRig* Rig = GetModularRigForTreeView())
	{
		if(URigHierarchy* Hierarchy = Rig->GetHierarchy())
		{
			Hierarchy->OnModified().RemoveAll(this);
		}
	}

	ControlRigEditor.Reset();
	ControlRigBlueprint.Reset();
}

void SModularRigModel::BindCommands()
{
	// create new command
	const FControlRigModularRigCommands& Commands = FControlRigModularRigCommands::Get();

	CommandList->MapAction(Commands.AddModuleItem,
		FExecuteAction::CreateSP(this, &SModularRigModel::HandleNewItem),
		FCanExecuteAction());

	CommandList->MapAction(Commands.RenameModuleItem,
		FExecuteAction::CreateSP(this, &SModularRigModel::HandleRenameModule),
		FCanExecuteAction());

	CommandList->MapAction(Commands.DeleteModuleItem,
		FExecuteAction::CreateSP(this, &SModularRigModel::HandleDeleteModules),
		FCanExecuteAction());

	CommandList->MapAction(Commands.MirrorModuleItem,
		FExecuteAction::CreateSP(this, &SModularRigModel::HandleMirrorModules),
		FCanExecuteAction());

	CommandList->MapAction(Commands.ReresolveModuleItem,
		FExecuteAction::CreateSP(this, &SModularRigModel::HandleReresolveModules),
		FCanExecuteAction());
}

FReply SModularRigModel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

void SModularRigModel::RefreshTreeView(bool bRebuildContent)
{
	bool bDummySuspensionFlag = false;
	bool* SuspensionFlagPtr = &bDummySuspensionFlag;
	if (ControlRigEditor.IsValid())
	{
		SuspensionFlagPtr = &ControlRigEditor.Pin()->GetSuspendDetailsPanelRefreshFlag();
	}
	TGuardValue<bool> SuspendDetailsPanelRefreshGuard(*SuspensionFlagPtr, true);

	TreeView->RefreshTreeView(bRebuildContent);
}

TArray<FString> SModularRigModel::GetSelectedKeys() const
{
	TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
	
	TArray<FString> SelectedKeys;
	for (const TSharedPtr<FModularRigTreeElement>& SelectedItem : SelectedItems)
	{
		if (SelectedItem.IsValid())
		{
			if(!SelectedItem->Key.IsEmpty())
			{
				SelectedKeys.AddUnique(SelectedItem->Key);
			}
		}
	}

	return SelectedKeys;
}


void SModularRigModel::HandlePreCompileModularRigs(URigVMBlueprint* InBlueprint)
{
}

void SModularRigModel::HandlePostCompileModularRigs(URigVMBlueprint* InBlueprint)
{
	RefreshTreeView();
	if (ControlRigEditor.IsValid())
	{
		TArray<TSharedPtr<FModularRigTreeElement>> SelectedElements;
		Algo::Transform(ControlRigEditor.Pin()->ModulesSelected, SelectedElements, [this](const FString& Path)
		{
			return TreeView->FindElement(Path);
		});
		TreeView->SetSelection(SelectedElements);
		ControlRigEditor.Pin()->RefreshDetailView();
	}
}

void SModularRigModel::HandleRefreshEditorFromBlueprint(URigVMBlueprint* InBlueprint)
{
	RefreshTreeView();
}

void SModularRigModel::HandleSetObjectBeingDebugged(UObject* InObject)
{
	if(ControlRigBeingDebuggedPtr.Get() == InObject)
	{
		return;
	}

	if(ControlRigBeingDebuggedPtr.IsValid())
	{
		if(URigHierarchy* Hierarchy = ControlRigBeingDebuggedPtr->GetHierarchy())
		{
			Hierarchy->OnModified().RemoveAll(this);
		}
	}

	ControlRigBeingDebuggedPtr.Reset();

	if(UModularRig* ControlRig = Cast<UModularRig>(InObject))
	{
		ControlRigBeingDebuggedPtr = ControlRig;

		if(URigHierarchy* Hierarchy = ControlRig->GetHierarchy())
		{
			Hierarchy->OnModified().AddSP(this, &SModularRigModel::OnHierarchyModified);
		}
	}

	RefreshTreeView();
}

TSharedPtr< SWidget > SModularRigModel::CreateContextMenuWidget()
{
	UToolMenus* ToolMenus = UToolMenus::Get();

	if (UToolMenu* Menu = GetContextMenu())
	{
		return ToolMenus->GenerateWidget(Menu);
	}
	
	return SNullWidget::NullWidget;
}

void SModularRigModel::OnItemClicked(TSharedPtr<FModularRigTreeElement> InItem)
{
	UModularRig* Rig = GetModularRig();
	check(Rig);

	if (ControlRigEditor.IsValid() && InItem.IsValid())
	{
		ControlRigEditor.Pin()->SetDetailViewForRigModules(TreeView->GetSelectedKeys());
	}
}

void SModularRigModel::OnItemDoubleClicked(TSharedPtr<FModularRigTreeElement> InItem)
{

}

void SModularRigModel::CreateContextMenu()
{
	static bool bCreatedMenu = false;
	if(bCreatedMenu)
	{
		return;
	}
	bCreatedMenu = true;
	
	const FName MenuName = ContextMenuName;

	UToolMenus* ToolMenus = UToolMenus::Get();
	
	if (!ensure(ToolMenus))
	{
		return;
	}

	if (UToolMenu* Menu = ToolMenus->ExtendMenu(MenuName))
	{
		Menu->AddDynamicSection(NAME_None, FNewToolMenuDelegate::CreateLambda([](UToolMenu* InMenu)
			{
				UControlRigContextMenuContext* MainContext = InMenu->FindContext<UControlRigContextMenuContext>();
				
				if (SModularRigModel* ModelPanel = MainContext->GetModularRigModelPanel())
				{
					const FControlRigModularRigCommands& Commands = FControlRigModularRigCommands::Get(); 
				
					FToolMenuSection& ModulesSection = InMenu->AddSection(TEXT("Modules"), LOCTEXT("ModulesHeader", "Modules"));
					ModulesSection.AddSubMenu(TEXT("New"), LOCTEXT("New", "New"), LOCTEXT("New_ToolTip", "Create New Modules"),
						FNewToolMenuDelegate::CreateLambda([Commands, ModelPanel](UToolMenu* InSubMenu)
						{
							FToolMenuSection& DefaultSection = InSubMenu->AddSection(NAME_None);
							DefaultSection.AddMenuEntry(Commands.AddModuleItem);
						})
					);
					ModulesSection.AddMenuEntry(Commands.RenameModuleItem);
					ModulesSection.AddMenuEntry(Commands.DeleteModuleItem);
					ModulesSection.AddMenuEntry(Commands.MirrorModuleItem);
					ModulesSection.AddMenuEntry(Commands.ReresolveModuleItem);
				}
			})
		);
	}
}

UToolMenu* SModularRigModel::GetContextMenu()
{
	const FName MenuName = ContextMenuName;
	UToolMenus* ToolMenus = UToolMenus::Get();

	if(!ensure(ToolMenus))
	{
		return nullptr;
	}

	// individual entries in this menu can access members of this context, particularly useful for editor scripting
	UControlRigContextMenuContext* ContextMenuContext = NewObject<UControlRigContextMenuContext>();
	FControlRigMenuSpecificContext MenuSpecificContext;
	MenuSpecificContext.ModularRigModelPanel = SharedThis(this);
	ContextMenuContext->Init(ControlRigEditor, MenuSpecificContext);

	FToolMenuContext MenuContext(CommandList);
	MenuContext.AddObject(ContextMenuContext);

	UToolMenu* Menu = ToolMenus->GenerateMenu(MenuName, MenuContext);

	return Menu;
}

TSharedPtr<FUICommandList> SModularRigModel::GetContextMenuCommands() const
{
	return CommandList;
}

bool SModularRigModel::IsSingleSelected() const
{
	if(GetSelectedKeys().Num() == 1)
	{
		return true;
	}
	return false;
}

/** Filter class to show only RigModules. */
class FClassViewerRigModulesFilter : public IClassViewerFilter
{
public:
	FClassViewerRigModulesFilter()
		: AssetRegistry(FModuleManager::GetModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get())
	{}
	
	virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		if(InClass)
		{
			const bool bChildOfObjectClass = InClass->IsChildOf(UControlRig::StaticClass());
			const bool bMatchesFlags = !InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
			const bool bNotNative = !InClass->IsNative();

			// Allow any class contained in the extra picker common classes array
			if (InInitOptions.ExtraPickerCommonClasses.Contains(InClass))
			{
				return true;
			}
			
			if (bChildOfObjectClass && bMatchesFlags && bNotNative)
			{
				const FAssetData AssetData(InClass);
				return MatchesFilter(AssetData);
			}
		}
		return false;
	}

	virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef< const IUnloadedBlueprintData > InUnloadedClassData, TSharedRef< FClassViewerFilterFuncs > InFilterFuncs) override
	{
		const bool bChildOfObjectClass = InUnloadedClassData->IsChildOf(UControlRig::StaticClass());
		const bool bMatchesFlags = !InUnloadedClassData->HasAnyClassFlags(CLASS_Hidden | CLASS_HideDropDown | CLASS_Deprecated | CLASS_Abstract);
		if (bChildOfObjectClass && bMatchesFlags)
		{
			const FString GeneratedClassPathString = InUnloadedClassData->GetClassPathName().ToString();
			const FString BlueprintPath = GeneratedClassPathString.LeftChop(2); // Chop off _C
			const FAssetData AssetData = AssetRegistry.GetAssetByObjectPath(FSoftObjectPath(BlueprintPath));
			return MatchesFilter(AssetData);

		}
		return false;
	}

private:
	bool MatchesFilter(const FAssetData& AssetData)
	{
		static const UEnum* ControlTypeEnum = StaticEnum<EControlRigType>();
		const FString ControlRigTypeStr = AssetData.GetTagValueRef<FString>(TEXT("ControlRigType"));
		if (ControlRigTypeStr.IsEmpty())
		{
			return false;
		}

		const EControlRigType ControlRigType = (EControlRigType)(ControlTypeEnum->GetValueByName(*ControlRigTypeStr));
		return ControlRigType == EControlRigType::RigModule;
	}

	const IAssetRegistry& AssetRegistry;
};

/** Create Item */
void SModularRigModel::HandleNewItem()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	FString ParentPath;
	if (IsSingleSelected())
	{
		ParentPath = GetSelectedKeys()[0];
	}
	
	FClassViewerInitializationOptions Options;
	Options.bShowUnloadedBlueprints = true;
	Options.NameTypeToDisplay = EClassViewerNameTypeToDisplay::DisplayName;

	TSharedPtr<FClassViewerRigModulesFilter> ClassFilter = MakeShareable(new FClassViewerRigModulesFilter());
	Options.ClassFilters.Add(ClassFilter.ToSharedRef());
	Options.bShowNoneOption = false;
	
	UClass* ChosenClass;
	const FText TitleText = LOCTEXT("ModularRigModelPickModuleClass", "Pick Rig Module Class");
	const bool bPressedOk = SClassPickerDialog::PickClass(TitleText, Options, ChosenClass, UControlRig::StaticClass());
	if (bPressedOk)
	{
		HandleNewItem(ChosenClass, ParentPath);
	}
}

void SModularRigModel::HandleNewItem(UClass* InClass, const FString &InParentPath)
{
	UControlRig* ControlRig = InClass->GetDefaultObject<UControlRig>();
	if (!ControlRig)
	{
		return;
	}

	FSlateApplication::Get().DismissAllMenus();
	
	if (ControlRigBlueprint.IsValid())
	{
		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		
		FString ClassName = InClass->GetName();
		ClassName.RemoveFromEnd(TEXT("_C"));
		const FRigName Name = Controller->GetSafeNewName(InParentPath, FRigName(ClassName));
		const FString NewPath = Controller->AddModule(Name, InClass, InParentPath);
		TSharedPtr<FModularRigTreeElement> Element = TreeView->FindElement(NewPath);
		if (Element.IsValid())
		{
			TreeView->SetSelection({Element});
			TreeView->bRequestRenameSelected = true;
		}
	}
}

bool SModularRigModel::CanRenameModule() const
{
	return IsSingleSelected() && TreeView->FindElement(GetSelectedKeys()[0])->bIsPrimary;
}

void SModularRigModel::HandleRenameModule()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	if (!CanRenameModule())
	{
		return;
	}

	UModularRig* Rig = GetDefaultModularRig();
	if (Rig)
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigModelRenameSelected", "Rename selected module"));

		TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
		if (SelectedItems.Num() == 1)
		{
			SelectedItems[0]->RequestRename();
		}
	}

	return;
}

FName SModularRigModel::HandleRenameModule(const FString& InOldPath, const FName& InNewName)
{
	if (ControlRigBlueprint.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigModelRename", "Rename Module"));

		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		if (!Controller->RenameModule(InOldPath, InNewName).IsEmpty())
		{
			return InNewName;
		}
	}

	return NAME_None;
}

bool SModularRigModel::HandleVerifyNameChanged(const FString& InOldPath, const FName& InNewName, FText& OutErrorMessage)
{
	if (InNewName.IsNone())
	{
		return false;
	}

	FString ParentPath;
	FString OldName = InOldPath;
	(void)URigHierarchy::SplitNameSpace(InOldPath, &ParentPath, &OldName);

	if (InNewName == OldName)
	{
		return true;
	}
	
	if (ControlRigBlueprint.IsValid())
	{
		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		return Controller->CanRenameModule(InOldPath, InNewName, OutErrorMessage);
	}

	return false;
}

void SModularRigModel::HandleDeleteModules()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	UModularRig* Rig = GetDefaultModularRig();
	if (Rig)
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigModelDeleteSelected", "Delete selected modules"));

		TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
		TArray<FString> SelectedPaths;
		Algo::Transform(SelectedItems, SelectedPaths, [](const TSharedPtr<FModularRigTreeElement>& Element)
		{
			if (Element.IsValid())
			{
				return Element->ModulePath;
			}
			return FString();
		});
		HandleDeleteModules(SelectedPaths);
	}

	return;
}

void SModularRigModel::HandleDeleteModules(const TArray<FString>& InPaths)
{
	if (ControlRigBlueprint.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigModelDelete", "Delete Modules"));

		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		// Make sure we delete the modules from children to root
		TArray<FString> SortedPaths = Controller->Model->SortPaths(InPaths);
		Algo::Reverse(SortedPaths);
		for (const FString& Path : SortedPaths)
		{
			Controller->DeleteModule(Path);
		}
	}
}

void SModularRigModel::HandleReparentModules(const TArray<FString>& InPaths, const FString& InParentPath)
{
	if (ControlRigBlueprint.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigModelReparent", "Reparent Modules"));

		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		for (const FString& Path : InPaths)
		{
			Controller->ReparentModule(Path, InParentPath);
		}
	}
}

void SModularRigModel::HandleMirrorModules()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	UModularRig* Rig = GetDefaultModularRig();
	if (Rig)
	{
		TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
		TArray<FString> SelectedPaths;
		Algo::Transform(SelectedItems, SelectedPaths, [](const TSharedPtr<FModularRigTreeElement>& Element)
		{
			if (Element.IsValid())
			{
				return Element->ModulePath;
			}
			return FString();
		});
		HandleMirrorModules(SelectedPaths);
	}
}

void SModularRigModel::HandleMirrorModules(const TArray<FString>& InPaths)
{
	if (ControlRigBlueprint.IsValid())
	{
		FRigVMMirrorSettings Settings;
		TSharedPtr<FStructOnScope> StructToDisplay = MakeShareable(new FStructOnScope(FRigVMMirrorSettings::StaticStruct(), (uint8*)&Settings));
		
		TSharedRef<SKismetInspector> KismetInspector = SNew(SKismetInspector);
		KismetInspector->ShowSingleStruct(StructToDisplay);
		
		TSharedRef<SCustomDialog> MirrorDialog = SNew(SCustomDialog)
			.Title(FText(LOCTEXT("ControlModularModelMirror", "Mirror Selected Modules")))
			.Content()
			[
				KismetInspector
			]
			.Buttons({
				SCustomDialog::FButton(LOCTEXT("OK", "OK")),
				SCustomDialog::FButton(LOCTEXT("Cancel", "Cancel"))
		});

		if (MirrorDialog->ShowModal() == 0)
		{
			FScopedTransaction Transaction(LOCTEXT("ModularRigModelMirror", "Mirror Modules"));

			UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
			check(Controller);

			// Make sure we mirror the modules from root to children
			TArray<FString> SortedPaths = Controller->Model->SortPaths(InPaths);
			for (const FString& Path : SortedPaths)
			{
				Controller->MirrorModule(Path, Settings);
			}
		}
	}
}

void SModularRigModel::HandleReresolveModules()
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}

	UModularRig* Rig = GetDefaultModularRig();
	if (Rig)
	{
		TArray<TSharedPtr<FModularRigTreeElement>> SelectedItems = TreeView->GetSelectedItems();
		TArray<FString> SelectedPaths;
		Algo::Transform(SelectedItems, SelectedPaths, [](const TSharedPtr<FModularRigTreeElement>& Element)
		{
			if (Element.IsValid())
			{
				if(Element->ConnectorName.IsEmpty())
				{
					return Element->ModulePath;
				}
				return FString::Printf(TEXT("%s|%s"), *Element->ModulePath, *Element->ConnectorName);
			}
			return FString();
		});
		HandleReresolveModules(SelectedPaths);
	}
}

void SModularRigModel::HandleReresolveModules(const TArray<FString>& InPaths)
{
	if (ControlRigBlueprint.IsValid())
	{
		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		const UModularRig* Rig = GetDefaultModularRig();
		if (Rig == nullptr)
		{
			return;
		}

		const URigHierarchy* Hierarchy = Rig->GetHierarchy();
		if(Hierarchy == nullptr)
		{
			return;
		}

		TArray<FRigElementKey> ConnectorKeys;
		for (const FString& PathAndConnector : InPaths)
		{
			FString ModulePath = PathAndConnector;
			FString ConnectorName;
			(void)PathAndConnector.Split(TEXT("|"), &ModulePath, &ConnectorName, ESearchCase::CaseSensitive, ESearchDir::FromEnd);

			const FRigModuleReference* Module = Controller->Model->FindModule(ModulePath);
			if (!Module)
			{
				UE_LOG(LogControlRig, Error, TEXT("Could not find module %s"), *ModulePath);
				return;
			}

			if(!ConnectorName.IsEmpty())
			{
				// if we are executing this on a primary connector we want to re-resolve all secondaries
				const FRigConnectorElement* PrimaryConnector = Module->FindPrimaryConnector(Hierarchy);
				const FName DesiredName = Hierarchy->GetNameMetadata(PrimaryConnector->GetKey(), URigHierarchy::DesiredNameMetadataName, NAME_None);
				if(!DesiredName.IsNone() && DesiredName.ToString().Equals(ConnectorName, ESearchCase::CaseSensitive))
				{
					ConnectorName.Reset();
				}
			}

			const TArray<const FRigConnectorElement*> Connectors = Module->FindConnectors(Hierarchy);
			for(const FRigConnectorElement* Connector : Connectors)
			{
				if(Connector->IsSecondary())
				{
					if(ConnectorName.IsEmpty())
					{
						ConnectorKeys.AddUnique(Connector->GetKey());
					}
					else
					{
						const FName DesiredName = Hierarchy->GetNameMetadata(Connector->GetKey(), URigHierarchy::DesiredNameMetadataName, NAME_None);
						if(!DesiredName.IsNone() && DesiredName.ToString().Equals(ConnectorName, ESearchCase::CaseSensitive))
						{
							ConnectorKeys.AddUnique(Connector->GetKey());
							break;
						}
					}
				}
			}
		}

		Controller->AutoConnectSecondaryConnectors(ConnectorKeys, true, true);
	}
}

void SModularRigModel::HandleConnectorResolved(const FRigElementKey& InConnector, const FRigElementKey& InTarget)
{
	if (ControlRigBlueprint.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigModelResolveConnector", "Resolve Connector"));

		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		if (ControlRigBeingDebuggedPtr.IsValid())
		{
			Controller->ConnectConnectorToElement(InConnector, InTarget, true, ControlRigBeingDebuggedPtr->GetModularRigSettings().bAutoResolve);
		}
	}
}

void SModularRigModel::HandleConnectorDisconnect(const FRigElementKey& InConnector)
{
	if (ControlRigBlueprint.IsValid())
	{
		FScopedTransaction Transaction(LOCTEXT("ModularRigModelDisconnectConnector", "Disconnect Connector"));

		UModularRigController* Controller = ControlRigBlueprint->GetModularRigController();
		check(Controller);

		Controller->DisconnectConnector(InConnector, false, true);
	}
}

void SModularRigModel::OnSelectionChanged(TSharedPtr<FModularRigTreeElement> Selection, ESelectInfo::Type SelectInfo)
{
}

void SModularRigModel::OnHierarchyModified(ERigHierarchyNotification InNotif, URigHierarchy* InHierarchy, const FRigBaseElement* InElement)
{
	if(!ControlRigBlueprint.IsValid())
	{
		return;
	}
	
	switch(InNotif)
	{
		case ERigHierarchyNotification::ElementSelected:
		case ERigHierarchyNotification::ElementDeselected:
		{
			const FRigConnectorElement* Connector = Cast<FRigConnectorElement>(InElement);
			if(Connector == nullptr)
			{
				for(const FModularRigSingleConnection& Connection : ControlRigBlueprint->ModularRigModel.Connections)
				{
					check(Connection.Connector.Type == ERigElementType::Connector);
					if(Connection.Target == InElement->GetKey())
					{
						if(const FRigConnectorElement* TargetConnector = InHierarchy->Find<FRigConnectorElement>(Connection.Connector))
						{
							OnHierarchyModified(InNotif, InHierarchy, TargetConnector);
						}
					}
				}
				return;
			}

			FString ModulePathOrConnectorName;
			if(Connector->IsPrimary())
			{
				ModulePathOrConnectorName = InHierarchy->GetModulePath(Connector->GetKey());
			}
			else
			{
				ModulePathOrConnectorName = Connector->GetName();
			}

			TSharedPtr<FModularRigTreeElement> Item = TreeView->FindElement(ModulePathOrConnectorName);
			if(Item.IsValid())
			{
				const bool bSelected = InNotif == ERigHierarchyNotification::ElementSelected;
				TreeView->SetItemSelection(Item, bSelected);
			}
		}
		default:
		{
			break;
		}
	}
}

class SModularRigModelPasteTransformsErrorPipe : public FOutputDevice
{
public:

	int32 NumErrors;

	SModularRigModelPasteTransformsErrorPipe()
		: FOutputDevice()
		, NumErrors(0)
	{
	}

	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category) override
	{
		UE_LOG(LogControlRig, Error, TEXT("Error importing transforms to Model: %s"), V);
		NumErrors++;
	}
};

UModularRig* SModularRigModel::GetModularRig() const
{
	if (ControlRigBlueprint.IsValid())
	{
		if (UControlRig* DebuggedRig = ControlRigBeingDebuggedPtr.Get())
		{
			return Cast<UModularRig>(DebuggedRig);
		}
	}
	if (ControlRigEditor.IsValid())
	{
		if (UControlRig* CurrentRig = ControlRigEditor.Pin()->GetControlRig())
		{
			return Cast<UModularRig>(CurrentRig);
		}
	}
	return nullptr;
}

UModularRig* SModularRigModel::GetDefaultModularRig() const
{
	if (ControlRigBlueprint.IsValid())
	{
		if (UControlRig* DebuggedRig = ControlRigBeingDebuggedPtr.Get())
		{
			return Cast<UModularRig>(DebuggedRig);
		}
	}
	return nullptr;
}


void SModularRigModel::OnRequestDetailsInspection(const FString& InKey)
{
	if(!ControlRigEditor.IsValid())
	{
		return;
	}
	ControlRigEditor.Pin()->SetDetailViewForRigModules({InKey});
}

void SModularRigModel::PostRedo(bool bSuccess) 
{
	if (bSuccess)
	{
		RefreshTreeView();
	}
}

void SModularRigModel::PostUndo(bool bSuccess) 
{
	if (bSuccess)
	{
		RefreshTreeView();
	}
}

FReply SModularRigModel::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TArray<FString> DraggedElements = GetSelectedKeys();
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton) && DraggedElements.Num() > 0)
	{
		if (ControlRigEditor.IsValid())
		{
			TSharedRef<FModularRigModuleDragDropOp> DragDropOp = FModularRigModuleDragDropOp::New(MoveTemp(DraggedElements));
			return FReply::Handled().BeginDragDrop(DragDropOp);
		}
	}

	return FReply::Unhandled();
}

TOptional<EItemDropZone> SModularRigModel::OnCanAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FModularRigTreeElement> TargetItem)
{
	const TOptional<EItemDropZone> InvalidDropZone;
	TOptional<EItemDropZone> ReturnDropZone = DropZone;

	if(DropZone == EItemDropZone::BelowItem && !TargetItem.IsValid())
	{
		DropZone = EItemDropZone::OntoItem;
	}
	
	if(DropZone != EItemDropZone::OntoItem)
	{
		return InvalidDropZone;
	}

	TSharedPtr<FAssetDragDropOp> AssetDragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	TSharedPtr<FModularRigModuleDragDropOp> ModuleDragDropOperation = DragDropEvent.GetOperationAs<FModularRigModuleDragDropOp>();
	if (AssetDragDropOperation)
	{
		for (const FAssetData& AssetData : AssetDragDropOperation->GetAssets())
		{
			static const UEnum* ControlTypeEnum = StaticEnum<EControlRigType>();
			const FString ControlRigTypeStr = AssetData.GetTagValueRef<FString>(TEXT("ControlRigType"));
			if (ControlRigTypeStr.IsEmpty())
			{
				ReturnDropZone.Reset();
				break;
			}

			const EControlRigType ControlRigType = (EControlRigType)(ControlTypeEnum->GetValueByName(*ControlRigTypeStr));
			if (ControlRigType != EControlRigType::RigModule)
			{
				ReturnDropZone.Reset();
				break;
			}
		}
	}
	else if(ModuleDragDropOperation)
	{
		if(TargetItem.IsValid())
		{
			// we cannot drag a module onto itself
			if(ModuleDragDropOperation->GetElements().Contains(TargetItem->ModulePath))
			{
				return InvalidDropZone;
			}
		}
	}
	else
	{
		ReturnDropZone.Reset();
	}

	return ReturnDropZone;
}

FReply SModularRigModel::OnAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, TSharedPtr<FModularRigTreeElement> TargetItem)
{
	FString ParentPath;
	if (TargetItem.IsValid())
	{
		ParentPath = TargetItem->ModulePath;
	}

	TSharedPtr<FAssetDragDropOp> AssetDragDropOperation = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	TSharedPtr<FModularRigModuleDragDropOp> ModuleDragDropOperation = DragDropEvent.GetOperationAs<FModularRigModuleDragDropOp>();
	if (AssetDragDropOperation)
	{
		for (const FAssetData& AssetData : AssetDragDropOperation->GetAssets())
		{
			static const UEnum* ControlTypeEnum = StaticEnum<EControlRigType>();
			const FString ControlRigTypeStr = AssetData.GetTagValueRef<FString>(TEXT("ControlRigType"));
			if (ControlRigTypeStr.IsEmpty())
			{
				continue;
			}

			const EControlRigType ControlRigType = (EControlRigType)(ControlTypeEnum->GetValueByName(*ControlRigTypeStr));
			if (ControlRigType != EControlRigType::RigModule)
			{
				continue;
			}

			UClass* AssetClass = AssetData.GetClass();
			if (!AssetClass->IsChildOf(UControlRigBlueprint::StaticClass()))
			{
				continue;
			}

			if(UControlRigBlueprint* AssetBlueprint = Cast<UControlRigBlueprint>(AssetData.GetAsset()))
			{
				HandleNewItem(AssetBlueprint->GetControlRigClass(), ParentPath);
			}
		}

		FReply::Handled();
	}
	else if(ModuleDragDropOperation)
	{
		const TArray<FString> Paths = ModuleDragDropOperation->GetElements();
		HandleReparentModules(Paths, ParentPath);
	}
	
	return FReply::Unhandled();
}

FReply SModularRigModel::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	// only allow drops onto empty space of the widget (when there's no target item under the mouse)
	// when dropped onto an item SModularRigModel::OnAcceptDrop will deal with the event
	const TSharedPtr<FModularRigTreeElement>* ItemAtMouse = TreeView->FindItemAtPosition(DragDropEvent.GetScreenSpacePosition());
	FString ParentPath;
	if (ItemAtMouse && ItemAtMouse->IsValid())
	{
		return SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);
	}
	
	if (OnCanAcceptDrop(DragDropEvent, EItemDropZone::BelowItem, nullptr))
	{
		if (OnAcceptDrop(DragDropEvent, EItemDropZone::BelowItem, nullptr).IsEventHandled())
		{
			return FReply::Handled();
		}
	}
	return SCompoundWidget::OnDrop(MyGeometry, DragDropEvent);
}

#undef LOCTEXT_NAMESPACE

