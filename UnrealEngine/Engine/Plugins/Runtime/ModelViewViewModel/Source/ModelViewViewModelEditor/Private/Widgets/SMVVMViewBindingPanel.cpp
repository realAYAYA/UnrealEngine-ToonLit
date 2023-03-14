// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMVVMViewBindingPanel.h"

#include "WidgetBlueprintEditor.h"
#include "WidgetBlueprintToolMenuContext.h"
#include "MVVMBlueprintView.h"
#include "MVVMWidgetBlueprintExtension_View.h"
#include "Customizations/MVVMConversionPathCustomization.h"
#include "Customizations/MVVMPropertyPathCustomization.h"

#include "Styling/AppStyle.h"
#include "IStructureDetailsView.h"
#include "PropertyEditorModule.h"
#include "StatusBarSubsystem.h"
#include "ToolMenuContext.h"
#include "ToolMenus.h"
#include "Tabs/MVVMBindingSummoner.h"
#include "Tabs/MVVMViewModelSummoner.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SSimpleButton.h"
#include "Styling/MVVMEditorStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/SMVVMManageViewModelsWidget.h"
#include "Widgets/SMVVMViewModelContextListWidget.h"
#include "Widgets/SMVVMViewModelPanel.h"
#include "Widgets/SMVVMViewBindingListView.h"
#include "Widgets/SNullWidget.h"
#include "SPositiveActionButton.h"


#define LOCTEXT_NAMESPACE "BindingPanel"

namespace UE::MVVM
{

/** */
void SBindingsPanel::Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> WidgetBlueprintEditor, bool bInIsDrawerTab)
{
	WeakBlueprintEditor = WidgetBlueprintEditor;
	bIsDrawerTab = bInIsDrawerTab;

	UWidgetBlueprint* WidgetBlueprint = WidgetBlueprintEditor->GetWidgetBlueprintObj();
	check(WidgetBlueprint);

	UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = UMVVMWidgetBlueprintExtension_View::GetExtension<UMVVMWidgetBlueprintExtension_View>(WidgetBlueprint);
	MVVMExtension = MVVMExtensionPtr;
	if (MVVMExtensionPtr)
	{
		BlueprintViewChangedDelegateHandle = MVVMExtensionPtr->OnBlueprintViewChangedDelegate().AddSP(this, &SBindingsPanel::HandleBlueprintViewChangedDelegate);
	}
	else
	{
		WidgetBlueprint->OnExtensionAdded.AddRaw(this, &SBindingsPanel::HandleExtensionAdded);
	}

	{
		// Connection Settings
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

		FDetailsViewArgs DetailsViewArgs;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bShowPropertyMatrixButton = false;
		DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
		DetailsViewArgs.ViewIdentifier = NAME_None;
		DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
		
		FStructureDetailsViewArgs StructureDetailsViewArgs;
		StructDetailsView = PropertyEditorModule.CreateStructureDetailView(DetailsViewArgs, StructureDetailsViewArgs, TSharedPtr<FStructOnScope>());
		StructDetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout(FMVVMBlueprintPropertyPath::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&UE::MVVM::FPropertyPathCustomization::MakeInstance, WidgetBlueprint));
		StructDetailsView->GetDetailsView()->RegisterInstancedCustomPropertyTypeLayout(FMVVMBlueprintViewConversionPath::StaticStruct()->GetFName(), FOnGetPropertyTypeCustomizationInstance::CreateStatic(&UE::MVVM::FConversionPathCustomization::MakeInstance, WidgetBlueprint));
	}

	HandleBlueprintViewChangedDelegate();
}


SBindingsPanel::~SBindingsPanel()
{
	if (UMVVMWidgetBlueprintExtension_View* Extension = MVVMExtension.Get())
	{
		Extension->OnBlueprintViewChangedDelegate().Remove(BlueprintViewChangedDelegateHandle);
	}
	if (TSharedPtr<FWidgetBlueprintEditor> WidgetEditor = WeakBlueprintEditor.Pin())
	{
		UWidgetBlueprint* WidgetBlueprint = WidgetEditor->GetWidgetBlueprintObj();
		if (WidgetBlueprint)
		{
			WidgetBlueprint->OnExtensionAdded.RemoveAll(this);
		}
	}
}

FReply SBindingsPanel::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	FReply Reply = FReply::Unhandled();
	if (TSharedPtr<FWidgetBlueprintEditor> WidgetEditor = WeakBlueprintEditor.Pin())
	{
		if (WidgetEditor->GetToolkitCommands()->ProcessCommandBindings(InKeyEvent))
		{
			Reply = FReply::Handled();
		}
	}

	return Reply;
}

bool SBindingsPanel::SupportsKeyboardFocus() const
{
	return true;
}

void SBindingsPanel::HandleBlueprintViewChangedDelegate()
{
	ChildSlot
	[
		GenerateEditViewWidget()
	];
}


void SBindingsPanel::OnBindingListSelectionChanged(TConstArrayView<FMVVMBlueprintViewBinding*> Selection)
{
	if (FMVVMBlueprintViewBinding* Binding = Selection.Num() > 0 ? Selection.Last() : nullptr)
	{
		TSharedRef<FStructOnScope> StructScope = MakeShared<FStructOnScope>(FMVVMBlueprintViewBinding::StaticStruct(), reinterpret_cast<uint8*>(Binding));
		StructDetailsView->SetStructureData(StructScope);
		DetailContainer->SetContent(StructDetailsView->GetWidget().ToSharedRef());
	}
	else
	{
		// show empty details view
		DetailsView->SetObject(nullptr);
		DetailContainer->SetContent(DetailsView.ToSharedRef());
	}
}


FReply SBindingsPanel::AddDefaultBinding()
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		if (UMVVMBlueprintView* EditorData = MVVMExtensionPtr->GetBlueprintView())
		{
			check(BindingsList);
			EditorData->AddDefaultBinding();
			BindingsList->Refresh();
		}
	}
	return FReply::Handled();
}

bool SBindingsPanel::CanAddBinding() const
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		if (UMVVMBlueprintView* BlueprintView = MVVMExtensionPtr->GetBlueprintView())
		{
			return BlueprintView->GetViewModels().Num() > 0;
		}
	}

	return false;
}

FText SBindingsPanel::GetAddBindingToolTip() const
{
	if (CanAddBinding())
	{
		return LOCTEXT("AddBindingTooltip", "Add an empty binding.");
	}
	else
	{
		return LOCTEXT("CannotAddBindingToolTip", "A viewmodel is required before adding bindings.");
	}
}

TSharedRef<SWidget> SBindingsPanel::CreateDrawerDockButton()
{
	if (bIsDrawerTab)
	{
		return SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), "SimpleButton")
			.ToolTipText(LOCTEXT("BindingDockInLayout_Tooltip", "Docks the binding drawer in tab."))
			.ContentPadding(FMargin(1.f, 0.f))
			.OnClicked(this, &SBindingsPanel::CreateDrawerDockButtonClicked)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0, 0.0f)
				[
					SNew(SImage)
					.ColorAndOpacity(FSlateColor::UseForeground())
					.Image(FAppStyle::Get().GetBrush("Icons.Layout"))
				]
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4.0, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("DockInLayout", "Dock in Layout"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			];
	}

	return SNullWidget::NullWidget;
}

FReply SBindingsPanel::CreateDrawerDockButtonClicked()
{
	if (TSharedPtr<FWidgetBlueprintEditor> WidgetEditor = WeakBlueprintEditor.Pin())
	{
		GEditor->GetEditorSubsystem<UStatusBarSubsystem>()->ForceDismissDrawer();

		if (TSharedPtr<SDockTab> ExistingTab = WidgetEditor->GetToolkitHost()->GetTabManager()->TryInvokeTab(FMVVMBindingSummoner::TabID))
		{
			ExistingTab->ActivateInParent(ETabActivationCause::SetDirectly); 
		}
	}

	return FReply::Handled();
}

void SBindingsPanel::HandleExtensionAdded(UBlueprintExtension* NewExtension)
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = Cast<UMVVMWidgetBlueprintExtension_View>(NewExtension))
	{
		UWidgetBlueprint* WidgetBlueprint = MVVMExtensionPtr->GetWidgetBlueprint();

		if (WidgetBlueprint)
		{
			WidgetBlueprint->OnExtensionAdded.RemoveAll(this);

			MVVMExtension = MVVMExtensionPtr;

			if (!BlueprintViewChangedDelegateHandle.IsValid())
			{
				BlueprintViewChangedDelegateHandle = MVVMExtensionPtr->OnBlueprintViewChangedDelegate().AddSP(this, &SBindingsPanel::HandleBlueprintViewChangedDelegate);
			}

			if (MVVMExtensionPtr->GetBlueprintView() == nullptr)
			{
				MVVMExtensionPtr->CreateBlueprintViewInstance();
			}

			HandleBlueprintViewChangedDelegate();
		}
	}
}

TSharedRef<SWidget> SBindingsPanel::GenerateSettingsMenu()
{
	UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get();
	DetailsView->SetObject(MVVMExtensionPtr && MVVMExtensionPtr->GetBlueprintView() ? MVVMExtensionPtr->GetBlueprintView() : nullptr);
	DetailContainer->SetContent(DetailsView.ToSharedRef());

	UWidgetBlueprintToolMenuContext* WidgetBlueprintMenuContext = NewObject<UWidgetBlueprintToolMenuContext>();
	WidgetBlueprintMenuContext->WidgetBlueprintEditor = WeakBlueprintEditor;

	FToolMenuContext MenuContext(WidgetBlueprintMenuContext);
	return UToolMenus::Get()->GenerateWidget("MVVMEditor.Panel.Toolbar.Settings", MenuContext);
}


void SBindingsPanel::RegisterSettingsMenu()
{
	UToolMenu* Menu = UToolMenus::Get()->RegisterMenu("MVVMEditor.Panel.Toolbar.Settings");
}


TSharedRef<SWidget> SBindingsPanel::GenerateEditViewWidget()
{
	FSlimHorizontalToolBarBuilder ToolbarBuilderGlobal(TSharedPtr<const FUICommandList>(), FMultiBoxCustomization::None);
	{
		ToolbarBuilderGlobal.BeginSection("Binding");
		ToolbarBuilderGlobal.AddWidget(
			SNew(SPositiveActionButton)
			.OnClicked(this, &SBindingsPanel::AddDefaultBinding)
			.Text(LOCTEXT("AddWidget", "Add Widget"))
			.ToolTipText(this, &SBindingsPanel::GetAddBindingToolTip)
			.IsEnabled(this, &SBindingsPanel::CanAddBinding)
		);
		ToolbarBuilderGlobal.EndSection();
	}

	{
		ToolbarBuilderGlobal.AddWidget(SNew(SSpacer), NAME_None, true, HAlign_Right);
	}

	{
		ToolbarBuilderGlobal.AddWidget(CreateDrawerDockButton());

		ToolbarBuilderGlobal.BeginSection("Options");

		ToolbarBuilderGlobal.AddToolBarButton(
			FUIAction(
				FExecuteAction::CreateSP(this, &SBindingsPanel::ToggleDetailsVisibility),
				FCanExecuteAction(),
				FGetActionCheckState::CreateSP(this, &SBindingsPanel::GetDetailsVisibleCheckState)
			),
			"ToggleDetails",
			LOCTEXT("Details", "Details"),
			LOCTEXT("DetailsToolTip", "Open Details View"),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "WorldBrowser.DetailsButtonBrush"),
			EUserInterfaceActionType::ToggleButton
		);
		ToolbarBuilderGlobal.EndSection();
	}

	BindingsList = nullptr;
	if (MVVMExtension.Get())
	{
		BindingsList = SNew(SBindingsList, StaticCastSharedRef<SBindingsPanel>(AsShared()), MVVMExtension.Get());
	}

	TSharedRef<SWidget> BindingWidget = SNew(SOverlay)
		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage(FMVVMEditorStyle::Get().GetBrush("BindingView.ViewModelWarning"))
			.Visibility_Lambda([this]()
				{
					if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
					{
						if (MVVMExtensionPtr->GetBlueprintView() != nullptr && 
							MVVMExtensionPtr->GetBlueprintView()->GetViewModels().Num() > 0)
						{
							return EVisibility::Collapsed;
						}
					}
					return EVisibility::Visible;
				})
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(20, 20, 12, 20)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.AutoWidth()
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Warning"))
				]
				+ SHorizontalBox::Slot()
				.Padding(0, 0, 8, 0)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MissingViewModel", "This editor requires a viewmodel that widgets can bind to, would you like to create a viewmodel now?"))
				]
				+ SHorizontalBox::Slot()
				.Padding(0, 0, 20, 0)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.OnClicked(this, &SBindingsPanel::HandleCreateViewModelClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CreateViewModel", "Create Viewmodel"))
					]
				]
			]
		]
		+ SOverlay::Slot()
		[
			SNew(SBox)
			.Visibility(this, &SBindingsPanel::GetVisibility, true)
			[
				BindingsList ? BindingsList.ToSharedRef() : SNullWidget::NullWidget
			]
		];

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(8.0f, 2.0f, 0.0f, 2.0f)
		[
			ToolbarBuilderGlobal.MakeWidget()
		]

		+SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SBorder)
			.BorderImage(FMVVMEditorStyle::Get().GetBrush("BindingView.Background"))
			[
				SNew(SSplitter)
				.Orientation(EOrientation::Orient_Horizontal)
	
				+ SSplitter::Slot()
				.Value(0.75f)
				[
					BindingWidget
				]
	
				+ SSplitter::Slot()
				.Value(0.25f)
				[
					SAssignNew(DetailContainer, SBorder)
					.Visibility(EVisibility::Collapsed)
					[
						DetailsView.ToSharedRef()
					]
				]
			]
		];
}

ECheckBoxState SBindingsPanel::GetDetailsVisibleCheckState() const
{
	if (DetailContainer.IsValid())
	{
		return DetailContainer->GetVisibility() == EVisibility::Visible ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void SBindingsPanel::ToggleDetailsVisibility() 
{
	if (DetailContainer.IsValid())
	{
		EVisibility NewVisibility = GetDetailsVisibleCheckState() == ECheckBoxState::Checked ? EVisibility::Collapsed : EVisibility::Visible;
		return DetailContainer->SetVisibility(NewVisibility);
	}
}

EVisibility SBindingsPanel::GetVisibility(bool bVisibleWithBindings) const
{
	if (UMVVMWidgetBlueprintExtension_View* MVVMExtensionPtr = MVVMExtension.Get())
	{
		if (MVVMExtensionPtr->GetBlueprintView() != nullptr &&
			MVVMExtensionPtr->GetBlueprintView()->GetNumBindings() > 0)
		{
			return bVisibleWithBindings ? EVisibility::Visible : EVisibility::Collapsed;
		}
	}
	return bVisibleWithBindings ? EVisibility::Collapsed : EVisibility::Visible;
}

FReply SBindingsPanel::HandleCreateViewModelClicked()
{
	if (TSharedPtr<FWidgetBlueprintEditor> BPEditor = WeakBlueprintEditor.Pin())
	{
		if (TSharedPtr<SDockTab> DockTab = BPEditor->GetTabManager()->TryInvokeTab(FTabId(FViewModelSummoner::TabID)))
		{
			TSharedPtr<SWidget> ViewModelPanel = DockTab->GetContent();
			if (ViewModelPanel.IsValid())
			{
				StaticCastSharedPtr<SMVVMViewModelPanel>(ViewModelPanel)->OpenAddViewModelMenu();
			}
		}
	}
	return FReply::Handled();
}

} // namespace UE::MVVM

#undef LOCTEXT_NAMESPACE
