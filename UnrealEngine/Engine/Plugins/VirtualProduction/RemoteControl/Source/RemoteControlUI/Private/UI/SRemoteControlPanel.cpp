// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRemoteControlPanel.h"

#include "Action/SRCActionPanel.h"
#include "ActorEditorUtils.h"
#include "Behaviour/Builtin/Bind/RCBehaviourBind.h"
#include "Behaviour/SRCBehaviourPanel.h"
#include "ClassViewerFilter.h"
#include "ClassViewerModule.h"
#include "Commands/RemoteControlCommands.h"
#include "Controller/SRCControllerPanel.h"
#include "Editor.h"
#include "Editor/EditorPerformanceSettings.h"
#include "EditorFontGlyphs.h"
#include "Engine/Selection.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "IRemoteControlModule.h"
#include "IRemoteControlProtocolWidgetsModule.h"
#include "IRemoteControlUIModule.h"
#include "ISettingsModule.h"
#include "IStructureDetailsView.h"
#include "Input/Reply.h"
#include "Interfaces/IMainFrameModule.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Layout/Visibility.h"
#include "Materials/Material.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "RCPanelWidgetRegistry.h"
#include "RemoteControlActor.h"
#include "RemoteControlEntity.h"
#include "RemoteControlField.h"
#include "RemoteControlLogger.h"
#include "RemoteControlPanelStyle.h"
#include "RemoteControlPreset.h"
#include "RemoteControlSettings.h"
#include "RemoteControlUIModule.h"
#include "SClassViewer.h"
#include "SRCLogger.h"
#include "SRCModeSwitcher.h"
#include "SRCPanelExposedActor.h"
#include "SRCPanelExposedEntitiesList.h"
#include "SRCPanelExposedField.h"
#include "SRCPanelFunctionPicker.h"
#include "SRCPanelTreeNode.h"
#include "SWarningOrErrorBox.h"
#include "SceneOutlinerModule.h"
#include "SceneOutlinerPublicTypes.h"
#include "ScopedTransaction.h"
#include "Styling/RemoteControlStyles.h"
#include "Styling/ToolBarStyle.h"
#include "Subsystems/Subsystem.h"
#include "Templates/SharedPointer.h"
#include "Templates/SubclassOf.h"
#include "Templates/UnrealTypeTraits.h"
#include "ToolMenus.h"
#include "Toolkits/IToolkitHost.h"
#include "UI/BaseLogicUI/RCLogicModeBase.h"
#include "UI/BaseLogicUI/SRCLogicPanelListBase.h"
#include "UI/Drawers/SRCPanelDrawer.h"
#include "UI/Filters/SRCPanelFilter.h"
#include "UI/Panels/SRCDockPanel.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SLayeredImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "RemoteControlPanel"

const FName SRemoteControlPanel::DefaultRemoteControlPanelToolBarName("RemoteControlPanel.DefaultToolBar");
const FName SRemoteControlPanel::AuxiliaryRemoteControlPanelToolBarName("RemoteControlPanel.AuxiliaryToolBar");
const float SRemoteControlPanel::MinimumPanelWidth = 640.f;

TSharedRef<SBox> SRemoteControlPanel::CreateNoneSelectedWidget()
{
	return SNew(SBox)
		.Padding(0.f)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoneSelected", "Select an entity to view details."))
			.TextStyle(&FAppStyle::GetWidgetStyle<FTextBlockStyle>("NormalText"))
			.Justification(ETextJustify::Center)
		];
}

void SRemoteControlPanel::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObjects(LogicClipboardItems);
}

namespace RemoteControlPanelUtils
{
	bool IsExposableActor(AActor* Actor)
	{
		return Actor->IsEditable()
            && Actor->IsListedInSceneOutliner()						// Only add actors that are allowed to be selected and drawn in editor
            && !Actor->IsTemplate()									// Should never happen, but we never want CDOs
            && !Actor->HasAnyFlags(RF_Transient)					// Don't add transient actors in non-play worlds
            && !FActorEditorUtils::IsABuilderBrush(Actor)			// Don't add the builder brush
            && !Actor->IsA(AWorldSettings::StaticClass());	// Don't add the WorldSettings actor, even though it is technically editable
	};

	TSharedPtr<FStructOnScope> GetEntityOnScope(const TSharedPtr<FRemoteControlEntity>& Entity, const UScriptStruct* EntityType)
	{
		if (ensure(Entity && EntityType))
		{
			check(EntityType->IsChildOf(FRemoteControlEntity::StaticStruct()));
			return MakeShared<FStructOnScope>(EntityType, reinterpret_cast<uint8*>(Entity.Get()));
		}

		return nullptr;
	}
}

class FRemoteControlPanelInputProcessor : public IInputProcessor
{
public:
	FRemoteControlPanelInputProcessor(TSharedPtr<SRemoteControlPanel> InOwner)
	{
		Owner = InOwner;
	}

	void SetOwner(TSharedPtr<SRemoteControlPanel> InOwner)
	{
		Owner = InOwner;
	}

	virtual ~FRemoteControlPanelInputProcessor() = default;

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
	{
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		if (const TSharedPtr<SRemoteControlPanel> PinnedOwner = Owner.Pin())
		{
			if (PinnedOwner->IsInLiveMode())
			{
				return false;
			}

			if (!SlateApp.HasFocusedDescendants(PinnedOwner.ToSharedRef()))
			{
				return false;
			}

			if (InKeyEvent.GetKey() == EKeys::Delete)
			{
				PinnedOwner->DeleteEntity();
				return true;
			}

			if (InKeyEvent.GetKey() == EKeys::F2)
			{
				PinnedOwner->RenameEntity();
				return true;
			}
		}

		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("RemoteControlInputInterceptor"); }

private:
	TWeakPtr<SRemoteControlPanel> Owner;
};

/**
 * UI representation of a auto resizing button.
 */
class SAutoResizeButton : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SAutoResizeButton)
		: _ForceSmallIcons(false)
		, _UICommand(nullptr)
		, _IconOverride()
	{}

		SLATE_ATTRIBUTE(bool, ForceSmallIcons)

		/** UI Command must be mapped to MainFrame Command List. */
		SLATE_ARGUMENT(TSharedPtr<const FUICommandInfo>, UICommand)

		SLATE_ARGUMENT(TAttribute<FSlateIcon>, IconOverride)

	SLATE_END_ARGS()

public:

	void Construct(const FArguments& InArgs)
	{
		bForceSmallIcons = InArgs._ForceSmallIcons;

		UICommand = InArgs._UICommand;

		IconOverride = InArgs._IconOverride;

		// Mimic Toolbar button style
		const FToolBarStyle& ToolBarStyle = FAppStyle::Get().GetWidgetStyle<FToolBarStyle>("AssetEditorToolbar");

		// Get the label & tooltip from the UI Command.
		const TAttribute<FText> ActualLabel = UICommand.IsValid() ? UICommand->GetLabel() : FText::GetEmpty();
		const TAttribute<FText> ActualToolTip = UICommand.IsValid() ? UICommand->GetDescription() : FText::GetEmpty();

		// If we were supplied an image than go ahead and use that, otherwise we use a null widget
		TSharedRef<SLayeredImage> IconWidget = SNew(SLayeredImage)
			.ColorAndOpacity(this, &SAutoResizeButton::GetIconForegroundColor)
			.Visibility(EVisibility::HitTestInvisible)
			.Image(this, &SAutoResizeButton::GetIconBrush);

		ChildSlot
		.Padding(ToolBarStyle.ButtonPadding.Left, 0.f, ToolBarStyle.ButtonPadding.Right, 0.f)
			[
				SNew(SCheckBox)
				.Padding(ToolBarStyle.CheckBoxPadding)
				.Style(&ToolBarStyle.ToggleButton)
				.IsChecked(this , &SAutoResizeButton::HandleIsChecked)
				.OnCheckStateChanged(this, &SAutoResizeButton::OnCheckStateChanged)
				.ToolTipText(ActualToolTip)
				.Content()
				[
					SNew(SHorizontalBox)

					// Icon Widget
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.Padding(2.f, 4.f)
					[
						IconWidget
					]

					// Label Text
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(ToolBarStyle.LabelPadding)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Visibility(this, &SAutoResizeButton::GetLabelVisibility)
						.Text(ActualLabel)
						.TextStyle(&ToolBarStyle.LabelStyle)	// Smaller font for tool tip labels
					]
				]
			];
	}

private:

	/** Called by Slate to determine whether labels are visible */
	EVisibility GetLabelVisibility() const
	{
		const bool bUseSmallIcons = bForceSmallIcons.IsSet() ? bForceSmallIcons.Get() : false;

		return bUseSmallIcons ? EVisibility::Collapsed : EVisibility::Visible;
	}

	/** Gets the icon brush for the toolbar block widget */
	const FSlateBrush* GetIconBrush() const
	{
		const FSlateIcon ActionIcon = UICommand.IsValid() ?  UICommand->GetIcon() : FSlateIcon();
		const FSlateIcon& ActualIcon = IconOverride.IsSet() ? IconOverride.Get() : ActionIcon;

		return ActualIcon.GetIcon();
	}

	/** Retrieves the color used by icon brush. */
	FSlateColor GetIconForegroundColor() const
	{
		// If any brush has a tint, don't assume it should be subdued
		const FSlateBrush* Brush = GetIconBrush();
		if (Brush && Brush->TintColor != FLinearColor::White)
		{
			return FLinearColor::White;
		}

		return FSlateColor::UseForeground();
	}

	/**
	 * Called by Slate to check whether this toolbar button is checked or not.
	 */
	ECheckBoxState HandleIsChecked() const
	{
		IMainFrameModule& MainFrame = FModuleManager::Get().LoadModuleChecked<IMainFrameModule>("MainFrame");

		TSharedPtr<FUICommandList> ActionList = MainFrame.GetMainFrameCommandBindings();

		if (ActionList.IsValid() && UICommand.IsValid())
		{
			return ActionList->GetCheckState(UICommand.ToSharedRef());
		}

		return ECheckBoxState::Undetermined;
	}

	/**
	 * Called by Slate when this tool bar button's button is clicked
	 */
	void OnCheckStateChanged(ECheckBoxState NewState)
	{
		IMainFrameModule& MainFrame = FModuleManager::Get().LoadModuleChecked<IMainFrameModule>("MainFrame");

		TSharedPtr<FUICommandList> ActionList = MainFrame.GetMainFrameCommandBindings();

		if (ActionList.IsValid() && UICommand.IsValid())
		{
			ActionList->TryExecuteAction(UICommand.ToSharedRef());
		}
	}

private:

	/** Should we use small icons or not. */
	TAttribute<bool> bForceSmallIcons;

	/** Holds the UI Command information of this button. */
	TSharedPtr<const FUICommandInfo> UICommand;

	/** Overriden icon to be used instead of default one. */
	TAttribute<FSlateIcon> IconOverride;
};

void SRemoteControlPanel::Construct(const FArguments& InArgs, URemoteControlPreset* InPreset, TSharedPtr<IToolkitHost> InToolkitHost)
{
	OnLiveModeChange = InArgs._OnLiveModeChange;
	Preset = TStrongObjectPtr<URemoteControlPreset>(InPreset);
	WidgetRegistry = MakeShared<FRCPanelWidgetRegistry>();
	ToolkitHost = InToolkitHost;

	ActivePanel = ERCPanels::RCP_Properties;

	RCPanelStyle = &FRemoteControlPanelStyle::Get()->GetWidgetStyle<FRCPanelStyle>("RemoteControlPanel.MinorPanel");
	
	const URemoteControlSettings* RemoteControlSettings = GetMutableDefault<URemoteControlSettings>();
	bIsLogicPanelEnabled = RemoteControlSettings->bLogicPanelVisibility;

	TArray<TSharedRef<SWidget>> ExtensionWidgets;
	FRemoteControlUIModule::Get().GetExtensionGenerators().Broadcast(ExtensionWidgets);

	BindRemoteControlCommands();

	GenerateAuxiliaryToolbar();

	AddToolbarWidget(AuxiliaryToolbarWidgetContent.ToSharedRef());

	// Settings
	AddToolbarWidget(SNew(SButton)
			.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
			.ContentPadding(2.0f)
			.TextStyle(FRemoteControlPanelStyle::Get(), "RemoteControlPanel.Button.TextStyle")
			.OnClicked(this, &SRemoteControlPanel::OnClickSettingsButton)
			.ToolTipText(LOCTEXT("OpenRemoteControlSettings", "Open Remote Control settings."))
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush("Icons.Toolbar.Settings"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]);

	// Show Log
	AddToolbarWidget(SNew(SCheckBox)
		.Style(&RCPanelStyle->ToggleButtonStyle)
		.ToolTipText(LOCTEXT("ShowLogTooltip", "Show/Hide remote control log."))
		.IsChecked_Lambda([this]() { return (FRemoteControlLogger::Get().IsEnabled() && !bIsInLiveMode) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
		.OnCheckStateChanged(this, &SRemoteControlPanel::OnLogCheckboxToggle)
		.Padding(4.f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ShowLogLabel", "Log"))
			.Justification(ETextJustify::Center)
			.TextStyle(&RCPanelStyle->PanelTextStyle)
		]
	);

	// Extension Generators
	for (const TSharedRef<SWidget>& Widget : ExtensionWidgets)
	{
		AddToolbarWidget(Widget);
	}

	GenerateToolbar();
	UpdateRebindButtonVisibility();

	EntityProtocolDetails = SNew(SBox);

	EntityList = SNew(SRCPanelExposedEntitiesList, Preset.Get(), WidgetRegistry)
		.ExposeActorsComboButton(CreateExposeActorsButton())
		.ExposeFunctionsComboButton(CreateExposeFunctionsButton())
		.OnEntityListUpdated_Lambda([this] ()
			{
				UpdateEntityDetailsView(EntityList->GetSelectedEntity());
				UpdateRebindButtonVisibility();
				CachedExposedPropertyArgs.Reset();
			}
		)
		.LiveMode_Lambda([this]() {return bIsInLiveMode; })
		.ProtocolsMode_Lambda([this]() { return IsInProtocolsMode() && ActivePanel == ERCPanels::RCP_Protocols; });

	EntityList->OnSelectionChange().AddSP(this, &SRemoteControlPanel::UpdateEntityDetailsView);

	const TAttribute<float> TreeBindingSplitRatioTop = TAttribute<float>::Create(
		TAttribute<float>::FGetter::CreateLambda([]()
		{
			URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();
			return Settings->TreeBindingSplitRatio;
		}));

	const TAttribute<float> TreeBindingSplitRatioBottom = TAttribute<float>::Create(
		TAttribute<float>::FGetter::CreateLambda([]()
		{
			URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();
			return 1.0f - Settings->TreeBindingSplitRatio;
		}));

	TSharedRef<SWidget> HeaderPanel = SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.Padding(3.f, 0.f)
		.AutoHeight()
		[
			ToolbarWidgetContent.ToSharedRef()
		];

	TSharedRef<SWidget> FooterPanel = SNew(SVerticalBox)
		// Use less CPU Warning
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(5.f, 8.f, 5.f, 5.f))
		[
			CreateCPUThrottleWarning()
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(5.f, 8.f, 5.f, 5.f))
		[
			CreateProtectedIgnoredWarning()
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(5.f, 8.f, 5.f, 5.f))
		[
			CreateGetterSetterIgnoredWarning()
		];

	// Output Log Dock panel
	TSharedRef<SRCMinorPanel> OutputLogDockPanel = SNew(SRCMinorPanel)
		.HeaderLabel(LOCTEXT("LogPanelLabel", "Log"))
		[
			SNew(SRCLogger)
		];

	TSharedRef<SWidget> OutputLogIcon = SNew(SImage)
		.Image(FAppStyle::Get().GetBrush("MessageLog.TabIcon"))
		.ColorAndOpacity(FSlateColor::UseForeground());

	OutputLogDockPanel->AddHeaderToolbarItem(EToolbar::Left, OutputLogIcon);

	// Make 3 Columns with Controllers + Behaviours + Actions
	TSharedRef<SRCMajorPanel> LogicPanel = SNew(SRCMajorPanel)
		.EnableHeader(false)
		.EnableFooter(false);

	// Controllers with Behaviours panel
	TSharedRef<SRCMajorPanel> ControllersAndBehavioursPanel = SNew(SRCMajorPanel)
		.EnableHeader(false)
		.EnableFooter(false);

	ControllerPanel = SNew(SRCControllerPanel, SharedThis(this))
		.LiveMode_Lambda([this]() { return bIsInLiveMode; })
		.Visibility_Lambda([this] { return (bIsLogicPanelEnabled || bIsInLiveMode) && (ActivePanel == ERCPanels::RCP_Properties || ActivePanel == ERCPanels::RCP_Live || ActivePanel == ERCPanels::RCP_None) ? EVisibility::Visible : EVisibility::Collapsed; });

	BehaviourPanel = SNew(SRCBehaviourPanel, SharedThis(this))
		.Visibility_Lambda([this] { return !bIsInLiveMode && bIsLogicPanelEnabled && (ActivePanel == ERCPanels::RCP_Properties || ActivePanel == ERCPanels::RCP_None) ? EVisibility::Visible : EVisibility::Collapsed; });

	ControllersAndBehavioursPanel->AddPanel(ControllerPanel.ToSharedRef(), 0.8f);

	constexpr bool bResizable = false;
	ControllersAndBehavioursPanel->AddPanel(BehaviourPanel.ToSharedRef(), 0.f, bResizable);

	// Actions Panel.
	ActionPanel = SNew(SRCActionPanel, SharedThis(this))
		.Visibility_Lambda([this] { return !bIsInLiveMode && bIsLogicPanelEnabled && (ActivePanel == ERCPanels::RCP_Properties || ActivePanel == ERCPanels::RCP_None) ? EVisibility::Visible : EVisibility::Collapsed; });

	// Retrieve Action Panel Split ratio from RC Settings.
	// We can't just get the value, since it will update in real time as the user resizes the slot: this value needs to update in real time
	const TAttribute<float> ActionPanelSplitRatioAttribute = TAttribute<float>::Create(
		TAttribute<float>::FGetter::CreateLambda([]()
		{
			const URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();
			return Settings->ActionPanelSplitRatio;
		}));
	
	int32 ActionPanelSlotIndex = LogicPanel->AddPanel(ControllersAndBehavioursPanel, ActionPanelSplitRatioAttribute, true);
	LogicPanel->AddPanel(ActionPanel.ToSharedRef(), 1.0 - ActionPanelSplitRatioAttribute.Get(), true);

	if (ActionPanelSlotIndex >= 0)
	{
		const TWeakPtr<SRCMajorPanel> LogicPanelWeak = LogicPanel.ToWeakPtr();

		// We setup a lambda to fire whenever the user is done resizing the splitter.
		// This lambda will record the current splitter position in Remote Control Settings
		LogicPanel->OnSplitterFinishedResizing().BindLambda([ActionPanelSlotIndex, LogicPanelWeak]()
		{
			if (LogicPanelWeak.IsValid())
			{
				if (const TSharedPtr<SRCMajorPanel> LogicPanelShared = LogicPanelWeak.Pin())
				{
					const SSplitter::FSlot& Slot = LogicPanelShared->GetSplitterSlotAt(ActionPanelSlotIndex);		
					URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();
					Settings->ActionPanelSplitRatio = Slot.GetSizeValue();
					Settings->PostEditChange();
					Settings->SaveConfig();
				}
			}
		});
	}

	// Make 2 Columns with Panel Drawer + Main Panel
	TSharedRef<SSplitter> ContentPanel = SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		.HitDetectionSplitterHandleSize(RCPanelStyle->SplitterHandleSize)
		.PhysicalSplitterHandleSize(RCPanelStyle->SplitterHandleSize)

		// Panel Drawer
		+SSplitter::Slot()
		.Value(0.05f)
		.Resizable(false)
		.SizeRule(SSplitter::ESizeRule::SizeToContent)
		[
			SAssignNew(PanelDrawer, SRCPanelDrawer)
		]

		// Main Panel
		+SSplitter::Slot()
		.Value(0.35f)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([this](){ return (int32)ActivePanel; })

			// 1. Nothing to be shown as all panels might be hidden (Index : 0)
			// Since this is widget switcher we should show NullWidget in such case.
			+ SWidgetSwitcher::Slot()
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return !bIsLogicPanelEnabled ? 0 : 1; })

				+ SWidgetSwitcher::Slot()
				[
					SNullWidget::NullWidget
				]

				+ SWidgetSwitcher::Slot()
				[
					LogicPanel
				]
			]

			// 2. Exposed entities List with Logic Editor (Index : 1).
			+ SWidgetSwitcher::Slot()
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return !bIsLogicPanelEnabled ? 0 : 1; })

				+ SWidgetSwitcher::Slot()
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return !bIsInLiveMode ? 0 : 1; })

					+ SWidgetSwitcher::Slot()
                    [
						EntityList.ToSharedRef()
					]

					+ SWidgetSwitcher::Slot()
					[
						SNullWidget::NullWidget
					]
				]

				+ SWidgetSwitcher::Slot()
				[
					SNew(SWidgetSwitcher)
					.WidgetIndex_Lambda([this]() { return bIsInLiveMode ? 0 : 1; })

					+ SWidgetSwitcher::Slot()
					[
						LogicPanel
					]

					+ SWidgetSwitcher::Slot()
					[
						SNew(SSplitter)
						.Orientation(Orient_Horizontal)

						+SSplitter::Slot()
						.Value(0.4)
						[
							EntityList.ToSharedRef()
						]

						+SSplitter::Slot()
						.Value(0.6)
						[
							LogicPanel
						]
					]
				]
			]

			// 3. Properties with Entity Details (Index : 2).
			+ SWidgetSwitcher::Slot()
			[
				SNew(SSplitter)
				.Orientation(EOrientation::Orient_Vertical)

				// Exposed entities List
				+ SSplitter::Slot()
				.Value(TreeBindingSplitRatioTop)
				.OnSlotResized(SSplitter::FOnSlotResized::CreateLambda([](float InNewSize)
				{
					URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();
					Settings->TreeBindingSplitRatio = InNewSize;
					Settings->PostEditChange();
					Settings->SaveConfig();
				}))
				[
					EntityList.ToSharedRef()
				]

				// Entity Details
				+ SSplitter::Slot()
				.Value(TreeBindingSplitRatioBottom)
				[
					CreateEntityDetailsView()
				]
			]

			// 4. Properties with Protocols (Index : 3).
			+ SWidgetSwitcher::Slot()
			[
				SNew(SSplitter)
				.Orientation(EOrientation::Orient_Vertical)

				// Exposed entities List
				+ SSplitter::Slot()
				.Value(TreeBindingSplitRatioTop)
				.OnSlotResized(SSplitter::FOnSlotResized::CreateLambda([](float InNewSize)
				{
					URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();
					Settings->TreeBindingSplitRatio = InNewSize;
					Settings->PostEditChange();
					Settings->SaveConfig();
				}))
				[
					EntityList.ToSharedRef()
				]

				// Protocol Details
				+ SSplitter::Slot()
				.Value(TreeBindingSplitRatioBottom)
				[
					EntityProtocolDetails.ToSharedRef()
				]
			]

			// 5. Output Log (Index : 4).
			+ SWidgetSwitcher::Slot()
			[
				OutputLogDockPanel
			]

			// 6. Live (Index : 5).
			+ SWidgetSwitcher::Slot()
			[
				SNew(SSplitter)
				.Orientation(EOrientation::Orient_Horizontal)

				// Exposed entities List
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					EntityList.ToSharedRef()
				]

				// Logic Panel
				+ SSplitter::Slot()
				.Value(0.5f)
				[
					LogicPanel
				]
			]
		];

	ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				HeaderPanel
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				// Separator
				SNew(SSeparator)
				.SeparatorImage(FAppStyle::Get().GetBrush("Separator"))
				.Thickness(5.f)
				.Orientation(EOrientation::Orient_Horizontal)
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.FillHeight(1.f)
			[
				ContentPanel
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				// Separator
				SNew(SSeparator)
				.SeparatorImage(FAppStyle::Get().GetBrush("Separator"))
				.Thickness(5.f)
				.Orientation(EOrientation::Orient_Horizontal)
			]
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				FooterPanel
			]
		];


	InputProcessor = MakeShared<FRemoteControlPanelInputProcessor>(SharedThis(this));
	FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);

	RegisterEvents();
	RegisterPanels();
	CacheLevelClasses();
	Refresh();
	LoadSettings(InPreset->GetPresetId());
}

SRemoteControlPanel::~SRemoteControlPanel()
{
	SaveSettings();

	UnregisterPanels();
	UnregisterEvents();

	FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);

	// Clear the log
	FRemoteControlLogger::Get().ClearLog();

	// Remove protocol bindings
	IRemoteControlProtocolWidgetsModule& ProtocolWidgetsModule = FModuleManager::LoadModuleChecked<IRemoteControlProtocolWidgetsModule>("RemoteControlProtocolWidgets");
	ProtocolWidgetsModule.ResetProtocolBindingList();

	// Unregister with UI module
	if (FModuleManager::Get().IsModuleLoaded("RemoteControlUI"))
	{
		FRemoteControlUIModule::Get().UnregisterRemoteControlPanel(this);
	}
}

void SRemoteControlPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (bMaterialsCompiledThisFrame)
	{
		TriggerMaterialCompiledRefresh();
		bMaterialsCompiledThisFrame = false;
	}
}

bool SRemoteControlPanel::IsExposed(const FRCExposesPropertyArgs& InPropertyArgs)
{
	if (!ensure(InPropertyArgs.IsValid()))
	{
		return false;
	}

	const FRCExposesPropertyArgs::EType ExtensionArgsType = InPropertyArgs.GetType();

	auto CheckCachedExposedArgs = [this, InPropertyArgs](const TArray<UObject*> InOwnerObjects, const FString& InPath, bool bIsCheckIsBoundByFullPath)
	{
		if (CachedExposedPropertyArgs.Contains(InPropertyArgs))
		{
			return true;
		}

		const bool bAllObjectsExposed = IsAllObjectsExposed(InOwnerObjects, InPath, bIsCheckIsBoundByFullPath);

		if (bAllObjectsExposed)
		{
			CachedExposedPropertyArgs.Emplace(InPropertyArgs);
		}

		return bAllObjectsExposed;
	};

	if (ExtensionArgsType == FRCExposesPropertyArgs::EType::E_Handle)
	{
		TArray<UObject*> OuterObjects;
		InPropertyArgs.PropertyHandle->GetOuterObjects(OuterObjects);
		const FString Path = InPropertyArgs.PropertyHandle->GeneratePathToProperty();

		constexpr bool bIsCheckIsBoundByFullPath = true;
		return CheckCachedExposedArgs({ OuterObjects }, Path, bIsCheckIsBoundByFullPath);
	}
	else if (ExtensionArgsType == FRCExposesPropertyArgs::EType::E_OwnerObject)
	{
		constexpr bool bIsCheckIsBoundByFullPath = false;
		return CheckCachedExposedArgs({ InPropertyArgs.OwnerObject.Get()}, InPropertyArgs.PropertyPath, bIsCheckIsBoundByFullPath);
	}

	// It never should hit this point
	ensure(false);

	return false;
}

bool SRemoteControlPanel::IsAllObjectsExposed(TArray<UObject*> InOuterObjects, const FString& InPath, bool bUsingDuplicatesInPath)
{
	TArray<TSharedPtr<FRemoteControlProperty>, TInlineAllocator<1>> PotentialMatches;
	for (const TWeakPtr<FRemoteControlProperty>& WeakProperty : Preset->GetExposedEntities<FRemoteControlProperty>())
	{
		if (TSharedPtr<FRemoteControlProperty> Property = WeakProperty.Pin())
		{
			// If that was exposed by property path it should be checked by the full path with duplicated like propertypath.propertypath[0]
			// If that was exposed by the owner object it should be without duplicated in the path, just propertypath[0]
			const bool Isbound = bUsingDuplicatesInPath ? Property->CheckIsBoundToPropertyPath(InPath) : Property->CheckIsBoundToString(InPath);

			if (Isbound)
			{
				PotentialMatches.Add(Property);
			}
		}
	}

	bool bAllObjectsExposed = true;

	for (UObject* OuterObject : InOuterObjects)
	{
		bool bFoundPropForObject = false;

		for (const TSharedPtr<FRemoteControlProperty>& Property : PotentialMatches)
		{
			if (Property->ContainsBoundObjects({ InOuterObjects } ))
			{
				bFoundPropForObject = true;
				break;
			}
		}

		bAllObjectsExposed &= bFoundPropForObject;
	}

	return bAllObjectsExposed;
}

void SRemoteControlPanel::ToggleProperty(const FRCExposesPropertyArgs& InPropertyArgs, FString InDesiredName)
{
	if (!ensure(InPropertyArgs.IsValid()))
	{
		return;
	}

	if (IsExposed(InPropertyArgs))
	{
		FScopedTransaction Transaction(LOCTEXT("UnexposeProperty", "Unexpose Property"));
		Preset->Modify();
		Unexpose(InPropertyArgs);
		return;
	}

	auto PostExpose = [this, InPropertyArgs]()
	{
		CachedExposedPropertyArgs.Emplace(InPropertyArgs);
	};

	const FRCExposesPropertyArgs::EType ExtensionArgsType = InPropertyArgs.GetType();
	if (ExtensionArgsType == FRCExposesPropertyArgs::EType::E_Handle)
	{
		TSet<UObject*> UniqueOuterObjects;
		{
			// Make sure properties are only being exposed once per object.
			TArray<UObject*> OuterObjects;
			InPropertyArgs.PropertyHandle->GetOuterObjects(OuterObjects);
			UniqueOuterObjects.Append(MoveTemp(OuterObjects));
		}

		if (UniqueOuterObjects.Num())
		{
			FScopedTransaction Transaction(LOCTEXT("ExposeProperty", "Expose Property"));
			Preset->Modify();

			for (UObject* Object : UniqueOuterObjects)
			{
				if (Object)
				{
					constexpr bool bCleanDuplicates = true; // GeneratePathToProperty duplicates container name (Array.Array[1], Set.Set[1], etc...)
					ExposeProperty(Object, FRCFieldPathInfo{ InPropertyArgs.PropertyHandle->GeneratePathToProperty(), bCleanDuplicates }, InDesiredName);
				}
			}

			PostExpose();
		}
	}
	else if (ExtensionArgsType == FRCExposesPropertyArgs::EType::E_OwnerObject)
	{
		FScopedTransaction Transaction(LOCTEXT("ExposeProperty", "Expose Property"));
		Preset->Modify();

		constexpr bool bCleanDuplicates = true; // GeneratePathToProperty duplicates container name (Array.Array[1], Set.Set[1], etc...)
		ExposeProperty(InPropertyArgs.OwnerObject.Get(), FRCFieldPathInfo{InPropertyArgs.PropertyPath, bCleanDuplicates});

		PostExpose();
	}
}

FGuid SRemoteControlPanel::GetSelectedGroup() const
{
	if (TSharedPtr<SRCPanelTreeNode> Node = EntityList->GetSelectedGroup())
	{
		return Node->GetRCId();
	}
	return FGuid();
}

FReply SRemoteControlPanel::OnClickDisableUseLessCPU() const
{
	UEditorPerformanceSettings* Settings = GetMutableDefault<UEditorPerformanceSettings>();
	Settings->bThrottleCPUWhenNotForeground = false;
	Settings->PostEditChange();
	Settings->SaveConfig();
	return FReply::Handled();
}

FReply SRemoteControlPanel::OnClickIgnoreWarnings() const
{
	URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();
	Settings->bIgnoreWarnings = true;
	Settings->PostEditChange();
	Settings->SaveConfig();
	return FReply::Handled();
}

TSharedRef<SWidget> SRemoteControlPanel::CreateCPUThrottleWarning() const
{
	FProperty* PerformanceThrottlingProperty = FindFieldChecked<FProperty>(UEditorPerformanceSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(UEditorPerformanceSettings, bThrottleCPUWhenNotForeground));
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("PropertyName"), PerformanceThrottlingProperty->GetDisplayNameText());
	FText PerformanceWarningText = FText::Format(LOCTEXT("RemoteControlPerformanceWarning", "Warning: The editor setting '{PropertyName}' is currently enabled\nThis will stop editor windows from updating in realtime while the editor is not in focus"), Arguments);

	return SNew(SWarningOrErrorBox)
		.Visibility_Lambda([]() { return GetDefault<UEditorPerformanceSettings>()->bThrottleCPUWhenNotForeground && !GetDefault<URemoteControlSettings>()->bIgnoreWarnings ? EVisibility::Visible : EVisibility::Collapsed; })
		.MessageStyle(EMessageStyle::Warning)
		.Message(PerformanceWarningText)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.OnClicked(this, &SRemoteControlPanel::OnClickDisableUseLessCPU)
				.TextStyle(FAppStyle::Get(), "DialogButtonText")
				.Text(LOCTEXT("RemoteControlPerformanceWarningDisable", "Disable"))
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SButton)
				.OnClicked(this, &SRemoteControlPanel::OnClickIgnoreWarnings)
				.TextStyle(FAppStyle::Get(), "DialogButtonText")
				.Text(LOCTEXT("RemoteControlIgnoreWarningEnable", "Ignore"))
			]
		];
}

TSharedRef<SWidget> SRemoteControlPanel::CreateProtectedIgnoredWarning() const
{
	FProperty* IgnoreProtectedCheckProperty = FindFieldChecked<FProperty>(URemoteControlSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(URemoteControlSettings, bIgnoreProtectedCheck));
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("PropertyName"), IgnoreProtectedCheckProperty->GetDisplayNameText());
	FText ProtectedIgnoredWarningText = FText::Format(LOCTEXT("RemoteControlProtectedIgnoredWarning", "Warning: The editor setting '{PropertyName}' is currently enabled\nThis will let properties with the protected flag be let through for certain checks"), Arguments);

	return SNew(SWarningOrErrorBox)
		.Visibility_Lambda([](){ return GetDefault<URemoteControlSettings>()->bIgnoreProtectedCheck && !GetDefault<URemoteControlSettings>()->bIgnoreWarnings ? EVisibility::Visible : EVisibility::Collapsed; })
		.MessageStyle(EMessageStyle::Warning)
		.Message(ProtectedIgnoredWarningText)
		[
			SNew(SButton)
			.OnClicked(this, &SRemoteControlPanel::OnClickIgnoreWarnings)
			.TextStyle(FAppStyle::Get(), "DialogButtonText")
			.Text(LOCTEXT("RemoteControlIgnoreWarningEnable", "Ignore"))
		];
}

TSharedRef<SWidget> SRemoteControlPanel::CreateGetterSetterIgnoredWarning() const
{
	FProperty* IgnoreGetterSetterCheckProperty = FindFieldChecked<FProperty>(URemoteControlSettings::StaticClass(), GET_MEMBER_NAME_CHECKED(URemoteControlSettings, bIgnoreGetterSetterCheck));
	FFormatNamedArguments Arguments;
	Arguments.Add(TEXT("PropertyName"), IgnoreGetterSetterCheckProperty->GetDisplayNameText());
	FText ProtectedIgnoredWarningText = FText::Format(LOCTEXT("RemoteControlGetterSetterIgnoredWarning", "Warning: The editor setting '{PropertyName}' is currently enabled\nThis will let properties which have Getter/Setter let through for certain checks"), Arguments);

	return SNew(SWarningOrErrorBox)
		.Visibility_Lambda([](){ return GetDefault<URemoteControlSettings>()->bIgnoreGetterSetterCheck && !GetDefault<URemoteControlSettings>()->bIgnoreWarnings ? EVisibility::Visible : EVisibility::Collapsed; })
		.MessageStyle(EMessageStyle::Warning)
		.Message(ProtectedIgnoredWarningText)
		[
			SNew(SButton)
			.OnClicked(this, &SRemoteControlPanel::OnClickIgnoreWarnings)
			.TextStyle(FAppStyle::Get(), "DialogButtonText")
			.Text(LOCTEXT("RemoteControlIgnoreWarningEnable", "Ignore"))
		];
}

TSharedRef<SWidget> SRemoteControlPanel::CreateExposeFunctionsButton()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	SAssignNew(BlueprintPicker, SRCPanelFunctionPicker)
		.RemoteControlPanel(SharedThis(this))
		.AllowDefaultObjects(true)
		.Label(LOCTEXT("FunctionLibrariesLabel", "Function Libraries"))
		.ObjectClass(UBlueprintFunctionLibrary::StaticClass())
		.OnSelectFunction(this, &SRemoteControlPanel::ExposeFunction);

	SAssignNew(SubsystemFunctionPicker, SRCPanelFunctionPicker)
		.RemoteControlPanel(SharedThis(this))
		.Label(LOCTEXT("SubsystemFunctionLabel", "Subsystem Functions"))
		.ObjectClass(USubsystem::StaticClass())
		.OnSelectFunction(this, &SRemoteControlPanel::ExposeFunction);

	SAssignNew(ActorFunctionPicker, SRCPanelFunctionPicker)
		.RemoteControlPanel(SharedThis(this))
		.Label(LOCTEXT("ActorFunctionsLabel", "Actor Functions"))
		.ObjectClass(AActor::StaticClass())
		.OnSelectFunction(this, &SRemoteControlPanel::ExposeFunction);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ExposeHeader", "Expose"));
	{
		constexpr bool bNoIndent = true;
		constexpr bool bSearchable = false;

		auto CreatePickerSubMenu = [this, bNoIndent, bSearchable, &MenuBuilder] (const FText& Label, const FText& ToolTip, const TSharedRef<SWidget>& Widget)
		{
			MenuBuilder.AddSubMenu(
				Label,
				ToolTip,
				FNewMenuDelegate::CreateLambda(
					[this, bNoIndent, bSearchable, Widget](FMenuBuilder& MenuBuilder)
					{
						MenuBuilder.AddWidget(Widget, FText::GetEmpty(), bNoIndent, bSearchable);
						FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
					}
				)
			);
		};

		CreatePickerSubMenu(
			LOCTEXT("BlueprintFunctionLibraryFunctionSubMenu", "Blueprint Function Library Function"),
			LOCTEXT("FunctionLibraryFunctionSubMenuToolTip", "Expose a function from a blueprint function library."),
			BlueprintPicker.ToSharedRef()
		);

		CreatePickerSubMenu(
			LOCTEXT("SubsystemFunctionSubMenu", "Subsystem Function"),
			LOCTEXT("SubsystemFunctionSubMenuToolTip", "Expose a function from a subsytem."),
			SubsystemFunctionPicker.ToSharedRef()
		);

		CreatePickerSubMenu(
			LOCTEXT("ActorFunctionSubMenu", "Actor Function"),
			LOCTEXT("ActorFunctionSubMenuToolTip", "Expose an actor's function."),
			ActorFunctionPicker.ToSharedRef()
		);
	}

	MenuBuilder.EndSection();

	return SAssignNew(ExposeFunctionsComboButton, SComboButton)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Expose Functions")))
		.IsEnabled_Lambda([this]() { return !this->bIsInLiveMode; })
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
		.ForegroundColor(FSlateColor::UseForeground())
		.CollapseMenuOnParentFocus(true)
		.HasDownArrow(false)
		.ContentPadding(FMargin(4.f, 2.f))
		.ButtonContent()
		[
			SNew(SBox)
			.WidthOverride(RCPanelStyle->IconSize.X)
			.HeightOverride(RCPanelStyle->IconSize.Y)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::GetBrush("GraphEditor.Function_16x"))
			]
		]
		.MenuContent()
		[
			MenuBuilder.MakeWidget()
		];
}

TSharedRef<SWidget> SRemoteControlPanel::CreateExposeActorsButton()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ExposeHeader", "Expose"));
	{
		constexpr bool bNoIndent = true;
		constexpr bool bSearchable = false;

		auto CreatePickerSubMenu = [this, bNoIndent, bSearchable, &MenuBuilder] (const FText& Label, const FText& ToolTip, const TSharedRef<SWidget>& Widget)
		{
			MenuBuilder.AddSubMenu(
				Label,
				ToolTip,
				FNewMenuDelegate::CreateLambda(
					[this, bNoIndent, bSearchable, Widget](FMenuBuilder& MenuBuilder)
					{
						MenuBuilder.AddWidget(Widget, FText::GetEmpty(), bNoIndent, bSearchable);
						FSlateApplication::Get().SetKeyboardFocus(Widget, EFocusCause::Navigation);
					}
				)
			);
		};

		// SObjectPropertyEntryBox does not support non-level editor worlds.
		if (!Preset.IsValid() || !Preset->IsEmbeddedPreset())
		{
			MenuBuilder.AddWidget(
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(AActor::StaticClass())
				.OnObjectChanged(this, &SRemoteControlPanel::OnExposeActor)
				.AllowClear(false)
				.DisplayUseSelected(true)
				.DisplayBrowse(true)
				.NewAssetFactories(TArray<UFactory*>()),
				LOCTEXT("ActorEntry", "Actor"));
		}
		else
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("SelectActor", "Select Actor"),
				LOCTEXT("SelectActorTooltip", "Select an actor to Remote Control."),
				FNewMenuDelegate::CreateLambda(
					[this](FMenuBuilder& SubMenuBuilder)
					{
						FSceneOutlinerModule& SceneOutlinerModule = FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
						FSceneOutlinerInitializationOptions InitOptions;
						constexpr bool bAllowPIE = false;
						UWorld* PresetWorld = URemoteControlPreset::GetWorld(Preset.Get(), bAllowPIE);

						SubMenuBuilder.AddWidget(
							SceneOutlinerModule.CreateActorPicker(
								InitOptions,
								FOnActorPicked::CreateSP(this, &SRemoteControlPanel::ExposeActor),
								PresetWorld
							),
							FText::GetEmpty(), true, false
						);
					}
				)
			);
		}

		CreatePickerSubMenu(
			LOCTEXT("ClassPickerEntry", "Actors By Class"),
			LOCTEXT("ClassPickerEntrySubMenuToolTip", "Expose all actors of the chosen class."),
			CreateExposeByClassWidget()
		);
	}

	MenuBuilder.EndSection();

	return SAssignNew(ExposeActorsComboButton, SComboButton)
		.AddMetaData<FTagMetaData>(FTagMetaData(TEXT("Expose Actors")))
		.IsEnabled_Lambda([this]() { return !this->bIsInLiveMode; })
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
		.ForegroundColor(FSlateColor::UseForeground())
		.CollapseMenuOnParentFocus(true)
		.HasDownArrow(false)
		.ContentPadding(FMargin(4.f, 2.f))
		.ButtonContent()
		[
			SNew(SBox)
			.WidthOverride(RCPanelStyle->IconSize.X)
			.HeightOverride(RCPanelStyle->IconSize.Y)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::GetBrush("ClassIcon.Actor"))
			]
		]
		.MenuContent()
		[
			MenuBuilder.MakeWidget()
		];
}

TSharedRef<SWidget> SRemoteControlPanel::CreateExposeByClassWidget()
{
	class FActorClassInLevelFilter : public IClassViewerFilter
	{
	public:
		FActorClassInLevelFilter(const TSet<TWeakObjectPtr<const UClass>>& InClasses)
			: Classes(InClasses)
		{
		}

		virtual bool IsClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const UClass* InClass, TSharedRef<FClassViewerFilterFuncs> InFilterFuncs) override
		{
			return Classes.Contains(TWeakObjectPtr<const UClass>{InClass});
		}

		virtual bool IsUnloadedClassAllowed(const FClassViewerInitializationOptions& InInitOptions, const TSharedRef<const class IUnloadedBlueprintData> InUnloadedClassData, TSharedRef<class FClassViewerFilterFuncs> InFilterFuncs) override
		{
			return false;
		}

	public:
		const TSet<TWeakObjectPtr<const UClass>>& Classes;
	};

	TSharedPtr<FActorClassInLevelFilter> Filter = MakeShared<FActorClassInLevelFilter>(CachedClassesInLevel);

	FClassViewerInitializationOptions Options;
	{
		Options.ClassFilters.Add(Filter.ToSharedRef());
		Options.bIsPlaceableOnly = true;
		Options.Mode = EClassViewerMode::ClassPicker;
		Options.DisplayMode = EClassViewerDisplayMode::ListView;
		Options.bShowObjectRootClass = true;
		Options.bShowNoneOption = false;
		Options.bShowUnloadedBlueprints = false;
	}

	TSharedRef<SWidget> Widget = FModuleManager::LoadModuleChecked<FClassViewerModule>("ClassViewer").CreateClassViewer(Options, FOnClassPicked::CreateLambda(
		[this](UClass* ChosenClass)
		{
			constexpr bool bAllowPIE = false;

			if (UWorld* World = URemoteControlPreset::GetWorld(Preset.Get(), bAllowPIE))
			{
				for (TActorIterator<AActor> It(World, ChosenClass, EActorIteratorFlags::SkipPendingKill); It; ++It)
				{
					if (RemoteControlPanelUtils::IsExposableActor(*It))
					{
						ExposeActor(*It);
					}
				}
			}

			if (ExposeActorsComboButton)
			{
				ExposeActorsComboButton->SetIsOpen(false);
			}
		}));

	ClassPicker = StaticCastSharedRef<SClassViewer>(Widget);

	return SNew(SBox)
		.MinDesiredWidth(200.f)
		[
			Widget
		];
}

void SRemoteControlPanel::CacheLevelClasses()
{
	CachedClassesInLevel.Empty();
	constexpr bool bAllowPIE = false;

	if (UWorld* World = URemoteControlPreset::GetWorld(Preset.Get(), bAllowPIE))
	{
		for (TActorIterator<AActor> It(World, AActor::StaticClass(), EActorIteratorFlags::SkipPendingKill); It; ++It)
		{
			CacheActorClass(*It);
		}

		if (ClassPicker)
		{
			ClassPicker->Refresh();
		}
	}
}

void SRemoteControlPanel::OnActorAddedToLevel(AActor* Actor)
{
	if (Actor)
	{
		CacheActorClass(Actor);
		if (ClassPicker)
		{
			ClassPicker->Refresh();
		}

		UpdateActorFunctionPicker();
	}
}

void SRemoteControlPanel::OnLevelActorsRemoved(AActor* Actor)
{
	if (Actor)
	{
		if (ClassPicker)
		{
			ClassPicker->Refresh();
		}

		UpdateActorFunctionPicker();
	}
}

void SRemoteControlPanel::OnLevelActorListChanged()
{
	UpdateActorFunctionPicker();
}

void SRemoteControlPanel::CacheActorClass(AActor* Actor)
{
	if (RemoteControlPanelUtils::IsExposableActor(Actor))
	{
		UClass* Class = Actor->GetClass();
		do
		{
			CachedClassesInLevel.Emplace(Class);
			Class = Class->GetSuperClass();
		}
		while(Class != UObject::StaticClass() && Class != nullptr);
	}
}

void SRemoteControlPanel::OnMapChange(uint32)
{
	CacheLevelClasses();

	if (ClassPicker)
	{
		ClassPicker->Refresh();
	}

	UpdateRebindButtonVisibility();

	// Clear the widget cache on map change to make sure we don't keep widgets around pointing to potentially stale objects.
	WidgetRegistry->Clear();
	Refresh();
}

void SRemoteControlPanel::BindRemoteControlCommands()
{
	const FRemoteControlCommands& Commands = FRemoteControlCommands::Get();

	IMainFrameModule& MainFrame = FModuleManager::Get().LoadModuleChecked<IMainFrameModule>("MainFrame");

	FUICommandList& ActionList = *MainFrame.GetMainFrameCommandBindings();

	ActionList.MapAction(
		Commands.SavePreset,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::SaveAsset_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanSaveAsset));

	ActionList.MapAction(
		Commands.FindPresetInContentBrowser,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::FindInContentBrowser_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanFindInContentBrowser));

	ActionList.MapAction(
		Commands.ToggleProtocolMappings,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::ToggleProtocolMappings_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanToggleProtocolsMode),
		FIsActionChecked::CreateSP(this, &SRemoteControlPanel::IsInProtocolsMode),
		FIsActionButtonVisible::CreateSP(this, &SRemoteControlPanel::CanToggleProtocolsMode));

	ActionList.MapAction(
		Commands.ToggleLogicEditor,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::ToggleLogicEditor_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanToggleLogicPanel),
		FIsActionChecked::CreateSP(this, &SRemoteControlPanel::IsLogicPanelEnabled),
		FIsActionButtonVisible::CreateSP(this, &SRemoteControlPanel::CanToggleLogicPanel));

	ActionList.MapAction(
		Commands.DeleteEntity,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::DeleteEntity_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanDeleteEntity));

	ActionList.MapAction(
		Commands.RenameEntity,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::RenameEntity_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanRenameEntity));

	ActionList.MapAction(
		Commands.CopyItem,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::CopyItem_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanCopyItem));

	ActionList.MapAction(
		Commands.PasteItem,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::PasteItem_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanPasteItem));

	ActionList.MapAction(
		Commands.DuplicateItem,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::DuplicateItem_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanDuplicateItem));

	ActionList.MapAction(
		Commands.UpdateValue,
		FExecuteAction::CreateSP(this, &SRemoteControlPanel::UpdateValue_Execute),
		FCanExecuteAction::CreateSP(this, &SRemoteControlPanel::CanUpdateValue));
}

void SRemoteControlPanel::RegisterEvents()
{
	FEditorDelegates::MapChange.AddSP(this, &SRemoteControlPanel::OnMapChange);

	if (GEditor)
	{
		GEditor->OnBlueprintReinstanced().AddSP(this, &SRemoteControlPanel::OnBlueprintReinstanced);
	}

	if (GEngine)
	{
		GEngine->OnLevelActorAdded().AddSP(this, &SRemoteControlPanel::OnActorAddedToLevel);
		GEngine->OnLevelActorListChanged().AddSP(this, &SRemoteControlPanel::OnLevelActorListChanged);
		GEngine->OnLevelActorDeleted().AddSP(this, &SRemoteControlPanel::OnLevelActorsRemoved);
	}

	Preset->OnEntityExposed().AddSP(this, &SRemoteControlPanel::OnEntityExposed);
	Preset->OnEntityUnexposed().AddSP(this, &SRemoteControlPanel::OnEntityUnexposed);

	UMaterial::OnMaterialCompilationFinished().AddSP(this, &SRemoteControlPanel::OnMaterialCompiled);
}

void SRemoteControlPanel::UnregisterEvents()
{
	Preset->OnEntityExposed().RemoveAll(this);
	Preset->OnEntityUnexposed().RemoveAll(this);

	if (GEngine)
	{
		GEngine->OnLevelActorDeleted().RemoveAll(this);
		GEngine->OnLevelActorListChanged().RemoveAll(this);
		GEngine->OnLevelActorAdded().RemoveAll(this);
	}

	if (GEditor)
	{
		GEditor->OnBlueprintReinstanced().RemoveAll(this);
	}

	FEditorDelegates::MapChange.RemoveAll(this);

	UMaterial::OnMaterialCompilationFinished().RemoveAll(this);
}

void SRemoteControlPanel::RegisterPanels()
{
	if (PanelDrawer.IsValid())
	{
		PanelDrawer->OnRCPanelToggled().BindSP(this, &SRemoteControlPanel::OnRCPanelToggled);

		PanelDrawer->CanToggleRCPanel().BindLambda([this]()
			{
				return !bIsInLiveMode;
			}
		);

		{// Properties Panel
			TSharedRef<FRCPanelDrawerArgs> PropertiesPanel = MakeShared<FRCPanelDrawerArgs>(ERCPanels::RCP_Properties);

			PropertiesPanel->bDrawnByDefault = true;
			PropertiesPanel->bRotateIconBy90 = true;
			PropertiesPanel->Label = LOCTEXT("PropertiesPanelLabel", "Expose");
			PropertiesPanel->ToolTip = LOCTEXT("PropertiesPanelTooltip", "Open exposed properties panel.");
			PropertiesPanel->Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorPreferences.TabIcon");

			PanelDrawer->RegisterPanel(PropertiesPanel);

			RegisteredDrawers.Add(PropertiesPanel->GetPanelID(), PropertiesPanel);
		}

		{// Properties With Details Panel
			TSharedRef<FRCPanelDrawerArgs> EntityDetailsPanel = MakeShared<FRCPanelDrawerArgs>(ERCPanels::RCP_EntityDetails);

			EntityDetailsPanel->bDrawnByDefault = false;
			EntityDetailsPanel->bRotateIconBy90 = false;
			EntityDetailsPanel->Label = LOCTEXT("EntityDetailsPanelLabel", "Details");
			EntityDetailsPanel->ToolTip = LOCTEXT("EntityDetailsPanelTooltip", "Open entity details panel.");
			EntityDetailsPanel->Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Details");

			PanelDrawer->RegisterPanel(EntityDetailsPanel);

			RegisteredDrawers.Add(EntityDetailsPanel->GetPanelID(), EntityDetailsPanel);
		}

		{// Properties With Protocols Panel
			TSharedRef<FRCPanelDrawerArgs> EntityProtocolsPanel = MakeShared<FRCPanelDrawerArgs>(ERCPanels::RCP_Protocols);
			const FRemoteControlCommands& Commands = FRemoteControlCommands::Get();

			EntityProtocolsPanel->bDrawnByDefault = true;
			EntityProtocolsPanel->bRotateIconBy90 = false;
			EntityProtocolsPanel->DrawerVisibility = bIsInProtocolsMode ? EVisibility::Visible : EVisibility::Collapsed;
			EntityProtocolsPanel->Label = Commands.ToggleProtocolMappings->GetLabel();
			EntityProtocolsPanel->ToolTip = LOCTEXT("EntityProtocolsPanelTooltip", "Open entity protocols panel.");
			EntityProtocolsPanel->Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer");

			PanelDrawer->RegisterPanel(EntityProtocolsPanel);

			RegisteredDrawers.Add(EntityProtocolsPanel->GetPanelID(), EntityProtocolsPanel);
		}

		{// Output Log Panel
			TSharedRef<FRCPanelDrawerArgs> OutputLogPanel = MakeShared<FRCPanelDrawerArgs>(ERCPanels::RCP_OutputLog);

			OutputLogPanel->bDrawnByDefault = true;
			OutputLogPanel->bRotateIconBy90 = false;
			OutputLogPanel->DrawerVisibility = FRemoteControlLogger::Get().IsEnabled() ? EVisibility::Visible : EVisibility::Collapsed;
			OutputLogPanel->Label = LOCTEXT("OutputLogPanelPanelLabel", "Log");
			OutputLogPanel->ToolTip = LOCTEXT("OutputLogPanelPanelTooltip", "Open output log panel.");
			OutputLogPanel->Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MessageLog.TabIcon");

			PanelDrawer->RegisterPanel(OutputLogPanel);

			RegisteredDrawers.Add(OutputLogPanel->GetPanelID(), OutputLogPanel);
		}

		{// Live Panel
			TSharedRef<FRCPanelDrawerArgs> LivePanel = MakeShared<FRCPanelDrawerArgs>(ERCPanels::RCP_Live);

			LivePanel->bDrawnByDefault = true;
			LivePanel->bRotateIconBy90 = false;
			LivePanel->DrawerVisibility = bIsInLiveMode ? EVisibility::Visible : EVisibility::Collapsed;
			LivePanel->Label = LOCTEXT("LivePanelPanelLabel", "Live");
			LivePanel->ToolTip = LOCTEXT("LivePanelTooltip", "Open live panel.");
			LivePanel->Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer");

			PanelDrawer->RegisterPanel(LivePanel);

			RegisteredDrawers.Add(LivePanel->GetPanelID(), LivePanel);
		}
	}
}

void SRemoteControlPanel::UnregisterPanels()
{
	if (PanelDrawer.IsValid())
	{
		PanelDrawer->OnRCPanelToggled().Unbind();
		PanelDrawer->CanToggleRCPanel().Unbind();

		for (TMap<ERCPanels, TSharedRef<FRCPanelDrawerArgs>>::TIterator RegisteredDrawer = RegisteredDrawers.CreateIterator(); RegisteredDrawer; ++RegisteredDrawer)
		{
			PanelDrawer->UnregisterPanel(RegisteredDrawer->Value);

			RegisteredDrawer.RemoveCurrent();
		}
	}
}

void SRemoteControlPanel::Refresh()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(SRemoteControlPanel::Refresh);

	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SRemoteControlPanel::RefreshBlueprintPicker);
		BlueprintPicker->Refresh();
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SRemoteControlPanel::RefreshActorFunctionPicker);
		ActorFunctionPicker->Refresh();
	}
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(SRemoteControlPanel::RefreshSubsystemFunctionPicker);
		SubsystemFunctionPicker->Refresh();
	}

	EntityList->Refresh();
}

void SRemoteControlPanel::AddToolbarWidget(TSharedRef<SWidget> Widget)
{
	ToolbarWidgets.AddUnique(Widget);
}

void SRemoteControlPanel::RemoveAllToolbarWidgets()
{
	ToolbarWidgets.Empty();
}

void SRemoteControlPanel::Unexpose(const FRCExposesPropertyArgs& InPropertyArgs)
{
	if (!InPropertyArgs.IsValid())
	{
		return;
	}

	auto CheckAndUnexpose = [&](TArray<UObject*> InOuterObjects, const FString& InPath, bool bInUsingDuplicatesInPath)
	{
		// Find an exposed property with the same path.
		TArray<TSharedPtr<FRemoteControlProperty>, TInlineAllocator<1>> PotentialMatches;
		for (const TWeakPtr<FRemoteControlProperty>& WeakProperty : Preset->GetExposedEntities<FRemoteControlProperty>())
		{
			if (TSharedPtr<FRemoteControlProperty> Property = WeakProperty.Pin())
			{
				// If that was exposed by property path it should be checked by the full path with duplicated like propertypath.propertypath[0]
				// If that was exposed by the owner object it should be without duplicated in the path, just propertypath[0]
				const bool bIsbound = bInUsingDuplicatesInPath ? Property->CheckIsBoundToPropertyPath(InPath) : Property->CheckIsBoundToString(InPath);
				if (bIsbound)
				{
					PotentialMatches.Add(Property);
				}
			}
		}

		for (const TSharedPtr<FRemoteControlProperty>& Property : PotentialMatches)
		{
			if (Property->ContainsBoundObjects(InOuterObjects))
			{
				Preset->Unexpose(Property->GetId());
				break;
			}
		}
	};

	const FRCExposesPropertyArgs::EType ExtensionArgsType = InPropertyArgs.GetType();

	if (ExtensionArgsType == FRCExposesPropertyArgs::EType::E_Handle)
	{
		TArray<UObject*> OuterObjects;
		InPropertyArgs.PropertyHandle->GetOuterObjects(OuterObjects);

		constexpr bool bUsingDuplicatesInPath = true;
		CheckAndUnexpose(OuterObjects, InPropertyArgs.PropertyHandle->GeneratePathToProperty(), bUsingDuplicatesInPath);
	}
	else if (ExtensionArgsType == FRCExposesPropertyArgs::EType::E_OwnerObject)
	{
		constexpr bool bUsingDuplicatesInPath = false;
		CheckAndUnexpose({ InPropertyArgs.OwnerObject.Get()}, InPropertyArgs.PropertyPath, bUsingDuplicatesInPath);
	}
}

void SRemoteControlPanel::OnLogCheckboxToggle(ECheckBoxState State)
{
	const bool bIsLogEnabled = (State == ECheckBoxState::Checked) ? true : false;
	FRemoteControlLogger::Get().EnableLog(bIsLogEnabled);

	if (PanelDrawer.IsValid())
	{
		TSharedRef<FRCPanelDrawerArgs> OutputLogPanel = RegisteredDrawers.FindChecked(ERCPanels::RCP_OutputLog);

		OutputLogPanel->DrawerVisibility = bIsLogEnabled ? EVisibility::Visible : EVisibility::Collapsed;

		// When we are not enabling log collapse the drawer.
		PanelDrawer->TogglePanel(OutputLogPanel, !bIsLogEnabled);

		if (!bIsLogEnabled)
		{
			TSharedRef<FRCPanelDrawerArgs> PropertiesPanel = RegisteredDrawers.FindChecked(ERCPanels::RCP_Properties);

			PanelDrawer->TogglePanel(PropertiesPanel);
		}
	}
}

void SRemoteControlPanel::OnBlueprintReinstanced()
{
	Refresh();
}

void SRemoteControlPanel::ExposeProperty(UObject* Object, FRCFieldPathInfo Path, FString InDesiredName)
{
	if (Path.Resolve(Object))
	{
		FRemoteControlPresetExposeArgs Args;
		Args.Label = InDesiredName;
		Args.GroupId = GetSelectedGroup();
		Preset->ExposeProperty(Object, MoveTemp(Path), MoveTemp(Args));
	}
}

void SRemoteControlPanel::ExposeFunction(UObject* Object, UFunction* Function)
{
	if (ExposeFunctionsComboButton)
	{
		ExposeFunctionsComboButton->SetIsOpen(false);
	}

	FScopedTransaction Transaction(LOCTEXT("ExposeFunction", "ExposeFunction"));
	Preset->Modify();

	FRemoteControlPresetExposeArgs Args;
	Args.GroupId = GetSelectedGroup();
	Preset->ExposeFunction(Object, Function, MoveTemp(Args));
}

void SRemoteControlPanel::OnExposeActor(const FAssetData& AssetData)
{
	ExposeActor(Cast<AActor>(AssetData.GetAsset()));
}

void SRemoteControlPanel::ExposeActor(AActor* Actor)
{
	if (Actor)
	{
		FScopedTransaction Transaction(LOCTEXT("ExposeActor", "Expose Actor"));
		Preset->Modify();

		FRemoteControlPresetExposeArgs Args;
		Args.GroupId = GetSelectedGroup();

		Preset->ExposeActor(Actor, Args);
	}
}

TSharedRef<SWidget> SRemoteControlPanel::CreateEntityDetailsView()
{
	WrappedEntityDetailsView = SNew(SBorder)
		.BorderImage(&RCPanelStyle->ContentAreaBrush)
		.Padding(RCPanelStyle->PanelPadding);

	// Details Panel Dock Widget.
	TSharedRef<SRCMinorPanel> EntityDetailsDockPanel = SNew(SRCMinorPanel)
		.HeaderLabel(LOCTEXT("EntityDetailsLabel", "Details"))
		[
			WrappedEntityDetailsView.ToSharedRef()
		];

	// Details Panel Icon
	const TSharedRef<SWidget> DetailsIcon = SNew(SImage)
		.Image(FAppStyle::Get().GetBrush("LevelEditor.Tabs.Details"))
		.ColorAndOpacity(FSlateColor::UseForeground());

	EntityDetailsDockPanel->AddHeaderToolbarItem(EToolbar::Left, DetailsIcon);

	FDetailsViewArgs Args;
	Args.bShowOptions = false;
	Args.bAllowFavoriteSystem = false;
	Args.bAllowSearch = false;
	Args.bShowScrollBar = false;

	EntityDetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateStructureDetailView(MoveTemp(Args), FStructureDetailsViewArgs(), nullptr);

	UpdateEntityDetailsView(EntityList->GetSelectedEntity());

	const bool bCanShowDetailsView = LastSelectedEntity.IsValid() && (LastSelectedEntity->GetRCType() != SRCPanelTreeNode::Group) && (LastSelectedEntity->GetRCType() != SRCPanelTreeNode::FieldChild);

	if (bCanShowDetailsView)
	{
		if (ensure(EntityDetailsView && EntityDetailsView->GetWidget()))
		{
			WrappedEntityDetailsView->SetContent(EntityDetailsView->GetWidget().ToSharedRef());
		}
	}
	else
	{
		WrappedEntityDetailsView->SetContent(CreateNoneSelectedWidget());
	}

	return EntityDetailsDockPanel;
}

void SRemoteControlPanel::UpdateEntityDetailsView(const TSharedPtr<SRCPanelTreeNode>& SelectedNode)
{
	TSharedPtr<FStructOnScope> SelectedEntityPtr;

	LastSelectedEntity = SelectedNode;

	if (LastSelectedEntity)
	{
		if (LastSelectedEntity->GetRCType() != SRCPanelTreeNode::Group &&
			LastSelectedEntity->GetRCType() != SRCPanelTreeNode::FieldChild &&
			LastSelectedEntity->GetRCType() != SRCPanelTreeNode::FieldGroup) // Field Child does not contain entity ID, that is why it should not be processed
		{
			const TSharedPtr<FRemoteControlEntity> Entity = Preset->GetExposedEntity<FRemoteControlEntity>(LastSelectedEntity->GetRCId()).Pin();
			SelectedEntityPtr = RemoteControlPanelUtils::GetEntityOnScope(Entity, Preset->GetExposedEntityType(LastSelectedEntity->GetRCId()));
		}
	}

	const bool bCanShowDetailsView = LastSelectedEntity.IsValid() && (LastSelectedEntity->GetRCType() != SRCPanelTreeNode::Group) && (LastSelectedEntity->GetRCType() != SRCPanelTreeNode::FieldChild);

	if (bCanShowDetailsView)
	{
		if (EntityDetailsView)
		{
			EntityDetailsView->SetStructureData(SelectedEntityPtr);

			WrappedEntityDetailsView->SetContent(EntityDetailsView->GetWidget().ToSharedRef());
		}
	}
	else
	{
		WrappedEntityDetailsView->SetContent(CreateNoneSelectedWidget());
	}

	static const FName ProtocolWidgetsModuleName = "RemoteControlProtocolWidgets";
	if(LastSelectedEntity.IsValid() && FModuleManager::Get().IsModuleLoaded(ProtocolWidgetsModuleName) && ensure(Preset.IsValid()))
	{
		if (const TSharedPtr<FRemoteControlEntity> RCEntity = Preset->GetExposedEntity(LastSelectedEntity->GetRCId()).Pin())
		{
			if(RCEntity->IsBound())
			{
				IRemoteControlProtocolWidgetsModule& ProtocolWidgetsModule = FModuleManager::LoadModuleChecked<IRemoteControlProtocolWidgetsModule>(ProtocolWidgetsModuleName);
				EntityProtocolDetails->SetContent(ProtocolWidgetsModule.GenerateDetailsForEntity(Preset.Get(), RCEntity->GetId()));
			}
			else
			{
				EntityProtocolDetails->SetContent(CreateNoneSelectedWidget());
			}
		}
	}
	else
	{
		EntityProtocolDetails->SetContent(CreateNoneSelectedWidget());
	}

	// Trigger search to list the search results specific to selected group.
	if (EntityList.IsValid())
	{
		EntityList->UpdateSearch();
	}
}

void SRemoteControlPanel::UpdateRebindButtonVisibility()
{
	if (URemoteControlPreset* PresetPtr = Preset.Get())
	{
		for (TWeakPtr<FRemoteControlEntity> WeakEntity : PresetPtr->GetExposedEntities<FRemoteControlEntity>())
		{
			if (TSharedPtr<FRemoteControlEntity> Entity = WeakEntity.Pin())
			{
				if (!Entity->IsBound())
				{
					bShowRebindButton = true;
					return;
				}
			}
		}
	}

	bShowRebindButton = false;
}

FReply SRemoteControlPanel::OnClickRebindAllButton()
{
	if (URemoteControlPreset* PresetPtr = Preset.Get())
	{
		PresetPtr->RebindUnboundEntities();

		UpdateRebindButtonVisibility();
	}
	return FReply::Handled();
}

void SRemoteControlPanel::UpdateActorFunctionPicker()
{
	if (GEditor && ActorFunctionPicker && !NextTickTimerHandle.IsValid())
	{
		NextTickTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateLambda([WeakPanelPtr = TWeakPtr<SRemoteControlPanel>(StaticCastSharedRef<SRemoteControlPanel>(AsShared()))]()
		{
			if (TSharedPtr<SRemoteControlPanel> PanelPtr = WeakPanelPtr.Pin())
			{
				PanelPtr->ActorFunctionPicker->Refresh();
				PanelPtr->NextTickTimerHandle.Invalidate();
			}
		}));
	}
}

void SRemoteControlPanel::OnEntityExposed(URemoteControlPreset* InPreset, const FGuid& InEntityId)
{
	CachedExposedPropertyArgs.Empty();
}

void SRemoteControlPanel::OnEntityUnexposed(URemoteControlPreset* InPreset, const FGuid& InEntityId)
{
	CachedExposedPropertyArgs.Empty();
}

FReply SRemoteControlPanel::OnClickSettingsButton()
{
	FModuleManager::LoadModuleChecked<ISettingsModule>("Settings").ShowViewer("Project", "Plugins", "Remote Control");
	return FReply::Handled();
}

void SRemoteControlPanel::OnMaterialCompiled(UMaterialInterface* MaterialInterface)
{
	bMaterialsCompiledThisFrame = true;
}

void SRemoteControlPanel::TriggerMaterialCompiledRefresh()
{
	bool bTriggerRefresh = true;

	// Clear the widget cache on material compiled to make sure we have valid property nodes for IPropertyRowGenerator
	if (TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(SharedThis(this)))
	{
		const TArray<TSharedRef<SWindow>>& ChildWindows = Window->GetChildWindows();
		// Selecting a material from the RC window can trigger material recompiles,
		// If we refresh the widgets right away we might end up closing the material selection window while the user is
		// still selecting a material. Therefore we must wait until that window is closed before refreshing the panel.
		if (ChildWindows.Num())
		{
			bTriggerRefresh = false;

			if (!ChildWindows[0]->GetOnWindowClosedEvent().IsBound())
			{
				ChildWindows[0]->GetOnWindowClosedEvent().AddLambda([WeakThis = TWeakPtr<SRemoteControlPanel>(SharedThis(this))](const TSharedRef<SWindow>&)
					{
						if (TSharedPtr<SRemoteControlPanel> Panel = WeakThis.Pin())
						{
							Panel->WidgetRegistry->Clear();
							Panel->Refresh();
						}
					});
			}
		}
	}

	if (bTriggerRefresh)
	{
		WidgetRegistry->Clear();
		Refresh();
	}
}


void SRemoteControlPanel::RegisterDefaultToolBar()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus->IsMenuRegistered(DefaultRemoteControlPanelToolBarName))
	{
		UToolMenu* ToolbarBuilder = ToolMenus->RegisterMenu(DefaultRemoteControlPanelToolBarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
		ToolbarBuilder->StyleName = "ContentBrowser.ToolBar";

#if 0
		ToolbarBuilder->StyleName = "AssetEditorToolbar";
#endif
		{
			FToolMenuSection& AssetSection = ToolbarBuilder->AddSection("Asset");
			AssetSection.AddEntry(FToolMenuEntry::InitToolBarButton(FRemoteControlCommands::Get().SavePreset, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Save"))));
			AssetSection.AddEntry(FToolMenuEntry::InitToolBarButton(FRemoteControlCommands::Get().FindPresetInContentBrowser, LOCTEXT("FindInContentBrowserButton", "Browse"), TAttribute<FText>(), FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("LevelEditor.OpenContentBrowser"))));
			AssetSection.AddSeparator("Common");
		}
	}
}

void SRemoteControlPanel::GenerateToolbar()
{
	RegisterDefaultToolBar();

	ToolbarWidgetContent = SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			SNullWidget::NullWidget
		];

	UToolMenus* ToolMenus = UToolMenus::Get();
	IMainFrameModule& MainFrame = FModuleManager::Get().LoadModuleChecked<IMainFrameModule>("MainFrame");

	UToolMenu* GeneratedToolbar = ToolMenus->FindMenu(DefaultRemoteControlPanelToolBarName);

	GeneratedToolbar->Context = FToolMenuContext(MainFrame.GetMainFrameCommandBindings());

	TSharedRef<class SWidget> ToolBarWidget = ToolMenus->GenerateWidget(GeneratedToolbar);

	TSharedRef<SWidget> MiscWidgets = SNullWidget::NullWidget;

	if (ToolbarWidgets.Num() > 0)
	{
		TSharedRef<SHorizontalBox> MiscWidgetsHBox = SNew(SHorizontalBox);

		for (int32 WidgetIdx = 0; WidgetIdx < ToolbarWidgets.Num(); ++WidgetIdx)
		{
			MiscWidgetsHBox->AddSlot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				.AutoWidth()
				[
					SNew(SSeparator)
					.SeparatorImage(FAppStyle::Get().GetBrush("Separator"))
					.Thickness(1.5f)
					.Orientation(EOrientation::Orient_Vertical)
				];

			MiscWidgetsHBox->AddSlot()
				.Padding(5.f, 0.f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					ToolbarWidgets[WidgetIdx]
				];
		}

		MiscWidgets = SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				MiscWidgetsHBox
			];
	}

	const URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();
	const FName& DefaultPanelMode = Settings->DefaultPanelMode;

	Toolbar =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			ToolBarWidget
		]
		+ SHorizontalBox::Slot()
		.Padding(5.f, 0.f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(STextBlock)
			.Text(this, &SRemoteControlPanel::HandlePresetName)
		]
		+ SHorizontalBox::Slot()
		.Padding(5.f, 0.f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.FillWidth(1.f)
		[
			SNew(SSpacer)
		]
		+SHorizontalBox::Slot()
		.Padding(5.f, 0.f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SNew(SRCModeSwitcher)
			.DefaultMode(DefaultPanelMode)
			.OnModeSwitched_Lambda([this](const SRCModeSwitcher::FRCMode& NewMode)
				{
					if (NewMode.ModeId == TEXT("Operation"))
					{
						bIsInLiveMode = true;
						bIsLogicPanelEnabled = true;
					}
					else if (NewMode.ModeId == TEXT("Setup"))
					{
						bIsInLiveMode = false;
					}

					if (PanelDrawer.IsValid())
					{
						TSharedRef<FRCPanelDrawerArgs> LivePanel = RegisteredDrawers.FindChecked(ERCPanels::RCP_Live);

						LivePanel->DrawerVisibility = bIsInLiveMode ? EVisibility::Visible : EVisibility::Collapsed;

						// When we are not in Live Mode collapse the drawer.
						PanelDrawer->TogglePanel(LivePanel, !bIsInLiveMode);

						if (!bIsInLiveMode)
						{
							TSharedRef<FRCPanelDrawerArgs> PropertiesPanel = RegisteredDrawers.FindChecked(ERCPanels::RCP_Properties);

							PanelDrawer->TogglePanel(PropertiesPanel);
						}
					}

					OnLiveModeChange.ExecuteIfBound(SharedThis(this), bIsInLiveMode);

					URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();					
					Settings->DefaultPanelMode = NewMode.ModeId;
					Settings->PostEditChange();
					Settings->SaveConfig();
				}
			)

			+ SRCModeSwitcher::Mode("Setup")
			.DefaultLabel(FText::Format(LOCTEXT("SetupModeLabel", "{0}"), { FText::FromString("Setup") }))
			.DefaultTooltip(FText::Format(LOCTEXT("SetupModeTooltip", "Switch to {0} mode."), { FText::FromString("Setup") }))
			.HAlignCell(HAlign_Fill)
			.VAlignCell(VAlign_Fill)
			.FixedWidth(96.f)
			.OptionalIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.Outliner"))

			+ SRCModeSwitcher::Mode("Operation")
			.DefaultLabel(FText::Format(LOCTEXT("OpModeLabel", "{0}"), { FText::FromString("Operation") }))
			.DefaultTooltip(FText::Format(LOCTEXT("OpModeTooltip", "Switch to {0} mode."), { FText::FromString("Operation") }))
			.HAlignCell(HAlign_Fill)
			.VAlignCell(VAlign_Fill)
			.FixedWidth(96.f)
			.OptionalIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Blueprint"))
		]
		+ SHorizontalBox::Slot()
		.Padding(5.f, 0.f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.FillWidth(1.f)
		[
			SNew(SSpacer)
		]
		// Companion Separator (Rebind All)
		+SHorizontalBox::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Fill)
		.AutoWidth()
		[
			SNew(SSeparator)
			.SeparatorImage(FAppStyle::Get().GetBrush("Separator"))
			.Thickness(1.5f)
			.Orientation(EOrientation::Orient_Vertical)
			.Visibility_Lambda([this]() { return bShowRebindButton ? EVisibility::Visible : EVisibility::Collapsed; })
		]
		// Rebind All (Statically added here as we cannot probagagte visibility to its companion separator when dynamically added.)
		+ SHorizontalBox::Slot()
		.Padding(5.f, 0.f)
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
			.Visibility_Lambda([this]() { return bShowRebindButton ? EVisibility::Visible : EVisibility::Collapsed; })
			.OnClicked(this, &SRemoteControlPanel::OnClickRebindAllButton)
			.ContentPadding(2.f)
			[
				SNew(SHorizontalBox)

				+SHorizontalBox::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.AutoWidth()
				.Padding(2.f)
				[
					SNew(SImage)
					.Image(FAppStyle::Get().GetBrush("Icons.Link"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]

				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(4.f, 2.f)
				.FillWidth(1.f)
				[
					SNew(STextBlock)
					.ToolTipText(LOCTEXT("RebindButtonToolTip", "Attempt to rebind all unbound entites of the preset."))
					.Text(LOCTEXT("RebindButtonText", "Rebind All"))
					.TextStyle(&RCPanelStyle->PanelTextStyle)
				]
			]
		]
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Right)
		.AutoWidth()
		[
			MiscWidgets
		];

	if (ToolbarWidgetContent.IsValid())
	{
		ToolbarWidgetContent->SetContent(Toolbar.ToSharedRef());
	}
}

void SRemoteControlPanel::RegisterAuxiliaryToolBar()
{
	UToolMenus* ToolMenus = UToolMenus::Get();
	if (!ToolMenus->IsMenuRegistered(AuxiliaryRemoteControlPanelToolBarName))
	{
		UToolMenu* ToolbarBuilder = ToolMenus->RegisterMenu(AuxiliaryRemoteControlPanelToolBarName, NAME_None, EMultiBoxType::SlimHorizontalToolBar);
		ToolbarBuilder->StyleName = "ContentBrowser.ToolBar";

#if 0
		ToolbarBuilder->StyleName = "AssetEditorToolbar";
#endif
		{
			FToolMenuSection& ToolsSection = ToolbarBuilder->AddSection("Tools");

			const FRemoteControlCommands& Commands = FRemoteControlCommands::Get();

			ToolsSection.AddEntry(FToolMenuEntry::InitWidget("Logic"
			, SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SAutoResizeButton)
					.UICommand(FRemoteControlCommands::Get().ToggleLogicEditor)
					.ForceSmallIcons_Static(SRemoteControlPanel::ShouldForceSmallIcons)
					.IconOverride(FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("GraphEditor.StateMachine_16x")))
				]
				, Commands.ToggleLogicEditor->GetLabel()
			)
			);

			ToolsSection.AddEntry(FToolMenuEntry::InitWidget("Protocols"
			, SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SAutoResizeButton)
					.UICommand(FRemoteControlCommands::Get().ToggleProtocolMappings)
					.ForceSmallIcons_Static(SRemoteControlPanel::ShouldForceSmallIcons)
					.IconOverride(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Tabs.StatsViewer"))
				]
				, Commands.ToggleProtocolMappings->GetLabel()
			)
			);
		}
	}
}

void SRemoteControlPanel::GenerateAuxiliaryToolbar()
{
	RegisterAuxiliaryToolBar();

	AuxiliaryToolbarWidgetContent = SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush("Brushes.Panel"))
		[
			SNullWidget::NullWidget
		];

	UToolMenus* ToolMenus = UToolMenus::Get();
	IMainFrameModule& MainFrame = FModuleManager::Get().LoadModuleChecked<IMainFrameModule>("MainFrame");

	UToolMenu* GeneratedToolbar = ToolMenus->FindMenu(AuxiliaryRemoteControlPanelToolBarName);

	GeneratedToolbar->Context = FToolMenuContext(MainFrame.GetMainFrameCommandBindings());

	TSharedRef<class SWidget> ToolBarWidget = ToolMenus->GenerateWidget(GeneratedToolbar);

	AuxiliaryToolbar =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			ToolBarWidget
		];

	if (AuxiliaryToolbarWidgetContent.IsValid())
	{
		AuxiliaryToolbarWidgetContent->SetContent(AuxiliaryToolbar.ToSharedRef());
	}
}

FText SRemoteControlPanel::HandlePresetName() const
{
	if (Preset)
	{
		return FText::FromString(Preset->GetName());
	}

	return FText::GetEmpty();
}

bool SRemoteControlPanel::CanSaveAsset() const
{
	return Preset.IsValid();
}

void SRemoteControlPanel::SaveAsset_Execute() const
{
	if (Preset.IsValid())
	{
		TArray<UPackage*> PackagesToSave;

		if (!Preset->IsAsset())
		{
			// Log an invalid object but don't try to save it
			UE_LOG(LogRemoteControl, Log, TEXT("Invalid object to save: %s"), (Preset.IsValid()) ? *Preset->GetFullName() : TEXT("Null Object"));
		}
		else
		{
			PackagesToSave.Add(Preset->GetOutermost());
		}

		constexpr bool bCheckDirtyOnAssetSave = false;
		constexpr bool bPromptToSave = false;

		FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirtyOnAssetSave, bPromptToSave);
	}
}

bool SRemoteControlPanel::CanFindInContentBrowser() const
{
	return Preset.IsValid();
}

void SRemoteControlPanel::FindInContentBrowser_Execute() const
{
	if (Preset.IsValid())
	{
		TArray<UObject*> ObjectsToSyncTo;

		ObjectsToSyncTo.Add(Preset.Get());

		GEditor->SyncBrowserToObjects(ObjectsToSyncTo);
	}
}

bool SRemoteControlPanel::ShouldForceSmallIcons()
{
	// Find the DockTab that houses this RemoteControlPreset widget in it.
	FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FTabManager> EditorTabManager = LevelEditorModule.GetLevelEditorTabManager();

	if (TSharedPtr<SDockTab> Tab = EditorTabManager->FindExistingLiveTab(FRemoteControlUIModule::RemoteControlPanelTabName))
	{
		if (TSharedPtr<SWindow> Window = Tab->GetParentWindow())
		{
			const FVector2D& ActualWindowSize = Window->GetSizeInScreen() / Window->GetDPIScaleFactor();

			// Need not to check for less than the minimum value as user can never go beyond that limit while resizing the parent window.
			return ActualWindowSize.X == SRemoteControlPanel::MinimumPanelWidth ? true : false;
		}
	}

	return false;
}

void SRemoteControlPanel::ToggleProtocolMappings_Execute()
{
	bIsInProtocolsMode = !bIsInProtocolsMode;

	if (PanelDrawer.IsValid())
	{
		TSharedRef<FRCPanelDrawerArgs> ProtocolsPanel = RegisteredDrawers.FindChecked(ERCPanels::RCP_Protocols);

		ProtocolsPanel->DrawerVisibility = bIsInProtocolsMode ? EVisibility::Visible : EVisibility::Collapsed;

		// When we are not in Protocols Mode collapse the drawer.
		PanelDrawer->TogglePanel(ProtocolsPanel, !bIsInProtocolsMode);

		if (!bIsInProtocolsMode)
		{
			TSharedRef<FRCPanelDrawerArgs> PropertiesPanel = RegisteredDrawers.FindChecked(ERCPanels::RCP_Properties);

			PanelDrawer->TogglePanel(PropertiesPanel);
		}

		if (EntityList.IsValid())
		{
			const bool bToggleProtcolMode = IsInProtocolsMode() && ActivePanel == ERCPanels::RCP_Protocols;

			EntityList->RebuildListWithColumns(bToggleProtcolMode ? EEntitiesListMode::Protocols : EEntitiesListMode::Default);
		}
	}
}

bool SRemoteControlPanel::CanToggleProtocolsMode() const
{
	return !bIsInLiveMode;
}

bool SRemoteControlPanel::IsInProtocolsMode() const
{
	return bIsInProtocolsMode;
}

void SRemoteControlPanel::ToggleLogicEditor_Execute()
{
	bIsLogicPanelEnabled = !bIsLogicPanelEnabled;

	URemoteControlSettings* Settings = GetMutableDefault<URemoteControlSettings>();
	Settings->bLogicPanelVisibility = bIsLogicPanelEnabled;
	Settings->PostEditChange();
	Settings->SaveConfig();	

	if (PanelDrawer.IsValid() && (ActivePanel != ERCPanels::RCP_Properties))
	{
		TSharedRef<FRCPanelDrawerArgs> PropertiesPanel = RegisteredDrawers.FindChecked(ERCPanels::RCP_Properties);

		PanelDrawer->TogglePanel(PropertiesPanel);
	}
}

bool SRemoteControlPanel::CanToggleLogicPanel() const
{
	return true;
}

bool SRemoteControlPanel::IsLogicPanelEnabled() const
{
	return bIsLogicPanelEnabled;
}

void SRemoteControlPanel::OnRCPanelToggled(ERCPanels InPanelID)
{
	if (InPanelID != ActivePanel)
	{
		ActivePanel = InPanelID;

		if (EntityList.IsValid() && !bIsInLiveMode)
		{
			const bool bToggleProtcolMode = IsInProtocolsMode() && ActivePanel == ERCPanels::RCP_Protocols;

			EntityList->RebuildListWithColumns(bToggleProtcolMode ? EEntitiesListMode::Protocols : EEntitiesListMode::Default);
		}
	}
}

TSharedPtr<class SRCLogicPanelBase> SRemoteControlPanel::GetActiveLogicPanel() const
{
	if (ControllerPanel->IsListFocused())
	{
		return ControllerPanel;
	}
	else if (BehaviourPanel->IsListFocused())
	{
		return BehaviourPanel;
	}
	else if (ActionPanel->IsListFocused())
	{
		return ActionPanel;
	}

	return nullptr;
}

void SRemoteControlPanel::DeleteEntity_Execute()
{
	// Currently used  as common entry point of Delete UI command for both RC Entity and Logic Items.
	// This could potentially be moved out if the Logic panels are moved to a separate tab.

	// ~ Delete Logic Item ~
	//
	// If the user focus is currently active on a Logic panel then route the Delete command to it and return.
	if (TSharedPtr<SRCLogicPanelBase> ActiveLogicPanel = GetActiveLogicPanel())
	{
		ActiveLogicPanel->RequestDeleteSelectedItem();

		return; // handled
	}

	// ~ Delete Entity ~

	if (LastSelectedEntity->GetRCType() == SRCPanelTreeNode::FieldChild) // Field Child does not contain entity ID, that is why it should not be processed
	{
		return;
	}

	if (LastSelectedEntity->GetRCType() == SRCPanelTreeNode::Group)
	{
		FScopedTransaction Transaction(LOCTEXT("DeleteGroup", "Delete Group"));
		Preset->Modify();
		Preset->Layout.DeleteGroup(LastSelectedEntity->GetRCId());
	}
	else
	{
		FScopedTransaction Transaction(LOCTEXT("UnexposeFunction", "Unexpose remote control entity"));
		Preset->Modify();
		TArray<TSharedPtr<SRCPanelTreeNode>> SelectedEntities = EntityList->GetSelectedEntities();
		for (int32 Index = 0; Index < SelectedEntities.Num(); ++Index)
		{
			if (SelectedEntities[Index]->GetRCType() != SRCPanelTreeNode::FieldChild)
			{
				Preset->Unexpose(SelectedEntities[Index]->GetRCId());
			}
		}
	}

	EntityList->Refresh();
}

bool SRemoteControlPanel::CanDeleteEntity() const
{
	if (bIsInLiveMode)
	{
		return false;
	}

	if (const TSharedPtr<SRCLogicPanelBase> ActiveLogicPanel = GetActiveLogicPanel())
	{
		return !ActiveLogicPanel->GetSelectedLogicItems().IsEmpty(); // User has focus on a logic panel
	}

	if (LastSelectedEntity.IsValid() && Preset.IsValid())
	{
		// Do not allow default group to be deleted.
		return !Preset->Layout.IsDefaultGroup(LastSelectedEntity->GetRCId());
	}

	return false;
}

void SRemoteControlPanel::RenameEntity_Execute() const
{
	if (ControllerPanel->IsListFocused())
	{
		ControllerPanel->EnterRenameMode();
		return;
	}

	if (LastSelectedEntity->GetRCType() == SRCPanelTreeNode::FieldChild ||
		LastSelectedEntity->GetRCType() == SRCPanelTreeNode::FieldGroup) // Field Child/Group does not contain entity ID, that is why it should not be processed
	{
		return;
	}

	LastSelectedEntity->EnterRenameMode();
}

bool SRemoteControlPanel::CanRenameEntity() const
{
	if (bIsInLiveMode)
	{
		return false;
	}

	if (ControllerPanel->IsListFocused())
	{
		return true;
	}

	if (LastSelectedEntity.IsValid() && Preset.IsValid())
	{
		// Do not allow default group to be renamed.
		return !Preset->Layout.IsDefaultGroup(LastSelectedEntity->GetRCId());
	}

	return false;
}

void SRemoteControlPanel::SetLogicClipboardItems(const TArray<UObject*>& InItems, const TSharedPtr<SRCLogicPanelBase>& InSourcePanel)
{
	LogicClipboardItems = InItems;
	LogicClipboardItemSource = InSourcePanel;
}

void SRemoteControlPanel::CopyItem_Execute()
{
	if (const TSharedPtr<SRCLogicPanelBase> ActiveLogicPanel = GetActiveLogicPanel())
	{
		ActiveLogicPanel->CopySelectedPanelItems();
	}
}

bool SRemoteControlPanel::CanCopyItem() const
{
	if (bIsInLiveMode)
	{
		return false;
	}

	if (const TSharedPtr<SRCLogicPanelBase> ActiveLogicPanel = GetActiveLogicPanel())
	{
		return !ActiveLogicPanel->GetSelectedLogicItems().IsEmpty();
	}

	return false;
}

void SRemoteControlPanel::PasteItem_Execute()
{
	if (const TSharedPtr<SRCLogicPanelBase> ActiveLogicPanel = GetActiveLogicPanel())
	{
		ActiveLogicPanel->PasteItemsFromClipboard();
	}
}

bool SRemoteControlPanel::CanPasteItem() const
{
	if (bIsInLiveMode)
	{
		return false;
	}

	if(!LogicClipboardItems.IsEmpty())
	{
		if (const TSharedPtr<SRCLogicPanelBase>& ActiveLogicPanel = GetActiveLogicPanel())
		{
			// Currently we only support pasting items between panels of exactly the same type.
			if (LogicClipboardItemSource == ActiveLogicPanel)
			{
				return ActiveLogicPanel->CanPasteClipboardItems(LogicClipboardItems);
			}
		}
	}

	return false;
}

void SRemoteControlPanel::DuplicateItem_Execute()
{
	if (const TSharedPtr<SRCLogicPanelBase>& ActiveLogicPanel = GetActiveLogicPanel())
	{
		ActiveLogicPanel->DuplicateSelectedPanelItems();

		return;
	}
}

bool SRemoteControlPanel::CanDuplicateItem() const
{
	if (bIsInLiveMode)
	{
		return false;
	}

	if (const TSharedPtr<SRCLogicPanelBase>& ActiveLogicPanel = GetActiveLogicPanel())
	{
		return !ActiveLogicPanel->GetSelectedLogicItems().IsEmpty();
	}

	return false;
}

void SRemoteControlPanel::UpdateValue_Execute()
{
	if (const TSharedPtr<SRCLogicPanelBase>& ActiveLogicPanel = GetActiveLogicPanel())
	{
		ActiveLogicPanel->UpdateValue();
	}
}

bool SRemoteControlPanel::CanUpdateValue() const
{
	if (bIsInLiveMode)
	{
		return false;
	}
	
	if (const TSharedPtr<SRCLogicPanelBase> ActiveLogicPanel = GetActiveLogicPanel())
	{
		return ActiveLogicPanel->CanUpdateValue();
	}

	return false;
}

void SRemoteControlPanel::LoadSettings(const FGuid& InInstanceId) const
{
	const FString SettingsString = InInstanceId.ToString();

	// Load all our data using the settings string as a key in the user settings ini.
	const TSharedPtr<SRCPanelFilter> FilterPtr = EntityList->GetFilterPtr();
	if (FilterPtr.IsValid())
	{
		FilterPtr->LoadSettings(GEditorPerProjectIni, IRemoteControlUIModule::SettingsIniSection, SettingsString);
	}
}

void SRemoteControlPanel::SaveSettings()
{
	const TSharedPtr<SRCPanelFilter> FilterPtr = EntityList->GetFilterPtr();
	if (Preset.IsValid() && FilterPtr.IsValid())
	{
		const FString SettingsString = Preset->GetPresetId().ToString();

		// Save all our data using the settings string as a key in the user settings ini.
		FilterPtr->SaveSettings(GEditorPerProjectIni, IRemoteControlUIModule::SettingsIniSection, SettingsString);
	}
}

void SRemoteControlPanel::DeleteEntity()
{
	if (CanDeleteEntity())
	{
		DeleteEntity_Execute();
	}
}

void SRemoteControlPanel::RenameEntity()
{
	if (CanRenameEntity())
	{
		RenameEntity_Execute();
	}
}


#undef LOCTEXT_NAMESPACE /*RemoteControlPanel*/
