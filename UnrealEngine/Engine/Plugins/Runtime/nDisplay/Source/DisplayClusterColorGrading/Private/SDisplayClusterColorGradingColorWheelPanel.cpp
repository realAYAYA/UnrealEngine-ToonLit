// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDisplayClusterColorGradingColorWheelPanel.h"

#include "DisplayClusterColorGradingCommands.h"
#include "SDisplayClusterColorGradingColorWheel.h"
#include "DetailView/SDisplayClusterColorGradingDetailView.h"
#include "Drawer/DisplayClusterColorGradingDrawerState.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Modules/ModuleManager.h"
#include "PropertyHandle.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SSplitter.h"
 
#define LOCTEXT_NAMESPACE "DisplayClusterColorGrading"

SDisplayClusterColorGradingColorWheelPanel::~SDisplayClusterColorGradingColorWheelPanel()
{
	if (ColorGradingDataModel)
	{
		ColorGradingDataModel->OnColorGradingGroupSelectionChanged().RemoveAll(this);
		ColorGradingDataModel->OnColorGradingElementSelectionChanged().RemoveAll(this);
	}
}

void SDisplayClusterColorGradingColorWheelPanel::Construct(const FArguments& InArgs)
{
	ColorGradingDataModel = InArgs._ColorGradingDataModelSource;

	if (ColorGradingDataModel)
	{
		ColorGradingDataModel->OnColorGradingGroupSelectionChanged().AddSP(this, &SDisplayClusterColorGradingColorWheelPanel::OnColorGradingGroupSelectionChanged);
		ColorGradingDataModel->OnColorGradingElementSelectionChanged().AddSP(this, &SDisplayClusterColorGradingColorWheelPanel::OnColorGradingElementSelectionChanged);
	}

	CommandList = MakeShared<FUICommandList>();
	BindCommands();

	ColorWheelOrientation = EOrientation::Orient_Vertical;

	ColorWheels.AddDefaulted(NumColorWheels);
	HiddenColorWheels.AddZeroed(NumColorWheels);

	for (int32 Index = 0; Index < NumColorWheels; ++Index)
    ChildSlot
	[
		SNew(SSplitter)
		.Orientation(Orient_Horizontal)
		.PhysicalSplitterHandleSize(1.0f)
		.HitDetectionSplitterHandleSize(5.0f)
		.Style(FAppStyle::Get(), "DetailsView.Splitter")

		+ SSplitter::Slot()
		.Value(0.8f)
		[
			SNew(SVerticalBox)
			.Visibility(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorWheelPanelVisibility)

			// Toolbar slot
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(6, 4)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ColorGradingGroupPropertyBox, SBox)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(ColorGradingElementsToolBarBox, SHorizontalBox)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSpacer)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					MakeColorDisplayModeCheckbox()
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(5.0f, 0.0f, 0.0f, 0.0f)
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				[
					SNew(SComboButton)
					.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
					.OnGetMenuContent(this, &SDisplayClusterColorGradingColorWheelPanel::MakeSettingsMenu)
					.HasDownArrow(false)
					.ContentPadding(FMargin(1.0f, 0.0f))
					.ButtonContent()
					[
						SNew(SImage)
						.ColorAndOpacity(FSlateColor::UseForeground())
						.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(6, 4)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Fill)
				.Padding(2, 0)
				[
					SAssignNew(ColorWheels[0], SDisplayClusterColorGradingColorWheel)
					.ColorDisplayMode(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayMode)
					.Orientation(ColorWheelOrientation)
					.Visibility(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorWheelVisibility, 0)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Fill)
				.Padding(2, 0)
				[
					SAssignNew(ColorWheels[1], SDisplayClusterColorGradingColorWheel)
					.ColorDisplayMode(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayMode)
					.Orientation(ColorWheelOrientation)
					.Visibility(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorWheelVisibility, 1)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Fill)
				.Padding(2, 0)
				[
					SAssignNew(ColorWheels[2], SDisplayClusterColorGradingColorWheel)
					.ColorDisplayMode(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayMode)
					.Orientation(ColorWheelOrientation)
					.Visibility(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorWheelVisibility, 2)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Fill)
				.Padding(2, 0)
				[
					SAssignNew(ColorWheels[3], SDisplayClusterColorGradingColorWheel)
					.ColorDisplayMode(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayMode)
					.Orientation(ColorWheelOrientation)
					.Visibility(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorWheelVisibility, 3)
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.HAlign(HAlign_Fill)
				.Padding(2, 0)
				[
					SAssignNew(ColorWheels[4], SDisplayClusterColorGradingColorWheel)
					.ColorDisplayMode(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayMode)
					.Orientation(ColorWheelOrientation)
					.Visibility(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorWheelVisibility, 4)
				]
			]
		]

		+ SSplitter::Slot()
		.Value(0.2f)
		[
			SAssignNew(DetailView, SDisplayClusterColorGradingDetailView)
			.PropertyRowGeneratorSource(ColorGradingDataModel->GetPropertyRowGenerator())
			.OnFilterDetailTreeNode(this, &SDisplayClusterColorGradingColorWheelPanel::FilterDetailTreeNode)
		]
	];
}

void SDisplayClusterColorGradingColorWheelPanel::Refresh()
{
	if (ColorGradingDataModel)
	{
		if (FDisplayClusterColorGradingDataModel::FColorGradingGroup* ColorGradingGroup = ColorGradingDataModel->GetSelectedColorGradingGroup())
		{
			FillColorGradingGroupProperty(*ColorGradingGroup);
			FillColorGradingElementsToolBar(ColorGradingGroup->ColorGradingElements);

			if (const FDisplayClusterColorGradingDataModel::FColorGradingElement* ColorGradingElement = ColorGradingDataModel->GetSelectedColorGradingElement())
			{
				FillColorWheels(*ColorGradingElement);
			}
			else
			{
				ClearColorWheels();
			}
		}
		else
		{
			ClearColorGradingGroupProperty();
			ClearColorGradingElementsToolBar();
			ClearColorWheels();
		}

		DetailView->Refresh();
	}
}

void SDisplayClusterColorGradingColorWheelPanel::GetDrawerState(FDisplayClusterColorGradingDrawerState& OutDrawerState)
{
	OutDrawerState.HiddenColorWheels = HiddenColorWheels;
	OutDrawerState.ColorDisplayMode = ColorDisplayMode;
	OutDrawerState.ColorWheelOrientation = ColorWheelOrientation;
}

void SDisplayClusterColorGradingColorWheelPanel::SetDrawerState(const FDisplayClusterColorGradingDrawerState& InDrawerState)
{
	// TODO: These could also be output to a config file to be stored between runs
	HiddenColorWheels = InDrawerState.HiddenColorWheels;
	ColorDisplayMode = InDrawerState.ColorDisplayMode;
	SetColorWheelOrientation(InDrawerState.ColorWheelOrientation);
}

void SDisplayClusterColorGradingColorWheelPanel::BindCommands()
{
	const FDisplayClusterColorGradingCommands& Commands = FDisplayClusterColorGradingCommands::Get();

	CommandList->MapAction(
		Commands.SaturationColorWheelVisibility,
		FExecuteAction::CreateSP(this, &SDisplayClusterColorGradingColorWheelPanel::ToggleColorWheelVisible, 3),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SDisplayClusterColorGradingColorWheelPanel::IsColorWheelVisible, 3)
	);

	CommandList->MapAction(
		Commands.ContrastColorWheelVisibility,
		FExecuteAction::CreateSP(this, &SDisplayClusterColorGradingColorWheelPanel::ToggleColorWheelVisible, 4),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SDisplayClusterColorGradingColorWheelPanel::IsColorWheelVisible, 4)
	);

	CommandList->MapAction(
		Commands.ColorWheelSliderOrientationHorizontal,
		FExecuteAction::CreateSP(this, &SDisplayClusterColorGradingColorWheelPanel::SetColorWheelOrientation, EOrientation::Orient_Horizontal),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SDisplayClusterColorGradingColorWheelPanel::IsColorWheelOrientationSelected, EOrientation::Orient_Horizontal)
	);

	CommandList->MapAction(
		Commands.ColorWheelSliderOrientationVertical,
		FExecuteAction::CreateSP(this, &SDisplayClusterColorGradingColorWheelPanel::SetColorWheelOrientation, EOrientation::Orient_Vertical),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &SDisplayClusterColorGradingColorWheelPanel::IsColorWheelOrientationSelected, EOrientation::Orient_Vertical)
	);
}

TSharedRef<SWidget> SDisplayClusterColorGradingColorWheelPanel::MakeColorDisplayModeCheckbox()
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.Type(ESlateCheckBoxType::ToggleButton)
			.IsChecked(this, &SDisplayClusterColorGradingColorWheelPanel::IsColorDisplayModeChecked, EColorDisplayMode::RGB)
			.OnCheckStateChanged(this, &SDisplayClusterColorGradingColorWheelPanel::OnColorDisplayModeCheckedChanged, EColorDisplayMode::RGB)
			.ToolTipText(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayModeToolTip, EColorDisplayMode::RGB)
			.Padding(4)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayModeLabel, EColorDisplayMode::RGB)
				.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)			
		.VAlign(VAlign_Top)
		.Padding(FMargin(0.0f, 0.0f, 3.0f, 0.0f))
		[
			SNew(SCheckBox)
			.Style(FAppStyle::Get(), "ToggleButtonCheckbox")
			.Type(ESlateCheckBoxType::ToggleButton)
			.IsChecked(this, &SDisplayClusterColorGradingColorWheelPanel::IsColorDisplayModeChecked, EColorDisplayMode::HSV)
			.OnCheckStateChanged(this, &SDisplayClusterColorGradingColorWheelPanel::OnColorDisplayModeCheckedChanged, EColorDisplayMode::HSV)
			.ToolTipText(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayModeToolTip, EColorDisplayMode::HSV)
			.Padding(4)
			.Content()
			[
				SNew(STextBlock)
				.Text(this, &SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayModeLabel, EColorDisplayMode::HSV)
				.Font(FAppStyle::GetFontStyle("StandardDialog.SmallFont"))
			]
		];
}

TSharedRef<SWidget> SDisplayClusterColorGradingColorWheelPanel::MakeSettingsMenu()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, CommandList);

	const FDisplayClusterColorGradingCommands& Commands = FDisplayClusterColorGradingCommands::Get();

	MenuBuilder.BeginSection(TEXT("ColorWheelVisibility"), LOCTEXT("ColorWheelPanel_ShowLabel", "Show"));
	{
		MenuBuilder.AddMenuEntry(Commands.SaturationColorWheelVisibility);
		MenuBuilder.AddMenuEntry(Commands.ContrastColorWheelVisibility);
	}
	MenuBuilder.EndSection();

	MenuBuilder.BeginSection(TEXT("ColorWheelSliders"), LOCTEXT("ColorWheelPanel_SlidersLabel", "Sliders"));
	{
		MenuBuilder.AddMenuEntry(Commands.ColorWheelSliderOrientationVertical);
		MenuBuilder.AddMenuEntry(Commands.ColorWheelSliderOrientationHorizontal);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SDisplayClusterColorGradingColorWheelPanel::FillColorGradingGroupProperty(const FDisplayClusterColorGradingDataModel::FColorGradingGroup& ColorGradingGroup)
{
	if (ColorGradingGroupPropertyBox.IsValid())
	{
		TSharedRef<SHorizontalBox> PropertyNameBox = SNew(SHorizontalBox);

		if (ColorGradingGroup.EditConditionPropertyHandle.IsValid())
		{
			if (TSharedPtr<IDetailTreeNode> EditConditionTreeNode = ColorGradingDataModel->GetPropertyRowGenerator()->FindTreeNode(ColorGradingGroup.EditConditionPropertyHandle))
			{
				FNodeWidgets EditConditionWidgets = EditConditionTreeNode->CreateNodeWidgets();

				if (EditConditionWidgets.ValueWidget.IsValid())
				{
					PropertyNameBox->AddSlot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						.Padding(2, 0, 4, 0)
						.AutoWidth()
						[
							EditConditionWidgets.ValueWidget.ToSharedRef()
						];
				}
			}
		}

		TSharedRef<SWidget> GroupHeaderWidget = ColorGradingGroup.GroupHeaderWidget.IsValid()
			? ColorGradingGroup.GroupHeaderWidget.ToSharedRef()
			: SNew(STextBlock).Text(ColorGradingGroup.DisplayName).Font(FAppStyle::Get().GetFontStyle("NormalFontBold"));

		PropertyNameBox->AddSlot()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(2, 0, 0, 0)
			[
				GroupHeaderWidget
			];

		ColorGradingGroupPropertyBox->SetContent(PropertyNameBox);
	}
}

void SDisplayClusterColorGradingColorWheelPanel::ClearColorGradingGroupProperty()
{
	ColorGradingGroupPropertyBox->SetContent(SNullWidget::NullWidget);
}

void SDisplayClusterColorGradingColorWheelPanel::FillColorGradingElementsToolBar(const TArray<FDisplayClusterColorGradingDataModel::FColorGradingElement>& ColorGradingElements)
{
	ColorGradingElementsToolBarBox->ClearChildren();

	for (const FDisplayClusterColorGradingDataModel::FColorGradingElement& Element : ColorGradingElements)
	{
		ColorGradingElementsToolBarBox->AddSlot()
			.AutoWidth()
			.Padding(2.0f, 0.0f, 2.0f, 0.0f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.OnCheckStateChanged(this, &SDisplayClusterColorGradingColorWheelPanel::OnColorGradingElementCheckedChanged, Element.DisplayName)
				.IsChecked(this, &SDisplayClusterColorGradingColorWheelPanel::IsColorGradingElementSelected, Element.DisplayName)
				[
					SNew(STextBlock)
					.TextStyle(FAppStyle::Get(), "SmallText")
					.Text(Element.DisplayName)
				]
			];
	}
}

void SDisplayClusterColorGradingColorWheelPanel::ClearColorGradingElementsToolBar()
{
	ColorGradingElementsToolBarBox->ClearChildren();
}

void SDisplayClusterColorGradingColorWheelPanel::FillColorWheels(const FDisplayClusterColorGradingDataModel::FColorGradingElement& ColorGradingElement)
{
	auto FillColorWheel = [this](const TSharedPtr<SDisplayClusterColorGradingColorWheel>& ColorWheel, const TSharedPtr<IPropertyHandle>& PropertyHandle)
	{
		if (ColorWheel.IsValid())
		{
			ColorWheel->SetColorPropertyHandle(PropertyHandle);
			ColorWheel->SetHeaderContent(CreateColorWheelHeaderWidget(PropertyHandle));
		}
	};

	FillColorWheel(ColorWheels[0], ColorGradingElement.OffsetPropertyHandle);
	FillColorWheel(ColorWheels[1], ColorGradingElement.GammaPropertyHandle);
	FillColorWheel(ColorWheels[2], ColorGradingElement.GainPropertyHandle);
	FillColorWheel(ColorWheels[3], ColorGradingElement.SaturationPropertyHandle);
	FillColorWheel(ColorWheels[4], ColorGradingElement.ContrastPropertyHandle);
}

void SDisplayClusterColorGradingColorWheelPanel::ClearColorWheels()
{
	for (const TSharedPtr<SDisplayClusterColorGradingColorWheel>& ColorWheel : ColorWheels)
	{
		if (ColorWheel.IsValid())
		{
			ColorWheel->SetColorPropertyHandle(nullptr);
			ColorWheel->SetHeaderContent(SNullWidget::NullWidget);
		}
	};
}

TSharedRef<SWidget> SDisplayClusterColorGradingColorWheelPanel::CreateColorWheelHeaderWidget(const TSharedPtr<IPropertyHandle>& ColorPropertyHandle)
{
	if (TSharedPtr<IDetailTreeNode> TreeNode = ColorGradingDataModel->GetPropertyRowGenerator()->FindTreeNode(ColorPropertyHandle))
	{
		FNodeWidgets NodeWidgets = TreeNode->CreateNodeWidgets();

		TSharedRef<SHorizontalBox> PropertyNameBox = SNew(SHorizontalBox);

		PropertyNameBox->AddSlot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			];

		if (NodeWidgets.EditConditionWidget.IsValid())
		{
			PropertyNameBox->AddSlot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				.Padding(2, 0, 0, 0)
				.AutoWidth()
				[
					NodeWidgets.EditConditionWidget.ToSharedRef()
				];
		}

		if (NodeWidgets.NameWidget.IsValid())
		{
			PropertyNameBox->AddSlot()
				.HAlign(NodeWidgets.NameWidgetLayoutData.HorizontalAlignment)
				.VAlign(NodeWidgets.NameWidgetLayoutData.VerticalAlignment)
				.Padding(2, 0, 0, 0)
				[
					NodeWidgets.NameWidget.ToSharedRef()
				];

			PropertyNameBox->AddSlot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.FillWidth(1.0f)
				[
					SNew(SBox)
					.WidthOverride(22.0f)
					[
						CreateColorPropertyExtensions(ColorPropertyHandle, TreeNode)
					]
				];
		}

		return PropertyNameBox;
	}

	return SNullWidget::NullWidget;
}

TSharedRef<SWidget> SDisplayClusterColorGradingColorWheelPanel::CreateColorPropertyExtensions(const TSharedPtr<IPropertyHandle>& ColorPropertyHandle, const TSharedPtr<IDetailTreeNode>& DetailTreeNode)
{
	TArray<FPropertyRowExtensionButton> ExtensionButtons;

	// Use a weak pointer to pass into delegates
	TWeakPtr<IPropertyHandle> WeakColorPropertyHandle = ColorPropertyHandle;

	FPropertyRowExtensionButton& ResetToDefaultButton = ExtensionButtons.AddDefaulted_GetRef();
	ResetToDefaultButton.Label = NSLOCTEXT("PropertyEditor", "ResetToDefault", "Reset to Default");
	ResetToDefaultButton.UIAction = FUIAction(
		FExecuteAction::CreateLambda([WeakColorPropertyHandle]
		{
			if (WeakColorPropertyHandle.IsValid())
			{
				WeakColorPropertyHandle.Pin()->ResetToDefault();
			}
		}),
		FCanExecuteAction::CreateLambda([WeakColorPropertyHandle]
		{
			const bool bIsEditable = WeakColorPropertyHandle.Pin()->IsEditable();
			return bIsEditable;
		}),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateLambda([WeakColorPropertyHandle]
		{
			bool bShowResetToDefaultButton = false;
			if (WeakColorPropertyHandle.IsValid())
			{
				if (!WeakColorPropertyHandle.Pin()->HasMetaData("NoResetToDefault") && !WeakColorPropertyHandle.Pin()->GetInstanceMetaData("NoResetToDefault"))
				{
					bShowResetToDefaultButton = WeakColorPropertyHandle.Pin()->CanResetToDefault();
				}
			}

			return bShowResetToDefaultButton;
		})
	);

	ResetToDefaultButton.Icon = FSlateIcon(FAppStyle::Get().GetStyleSetName(), "PropertyWindow.DiffersFromDefault");
	ResetToDefaultButton.ToolTip = NSLOCTEXT("PropertyEditor", "ResetToDefaultPropertyValueToolTip", "Reset this property to its default value.");

	// Add any global row extensions that are registered for the color property
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FOnGenerateGlobalRowExtensionArgs Args;
	Args.OwnerTreeNode = DetailTreeNode;
	Args.PropertyHandle = ColorPropertyHandle;

	PropertyEditorModule.GetGlobalRowExtensionDelegate().Broadcast(Args, ExtensionButtons);

	FSlimHorizontalToolBarBuilder ToolbarBuilder(TSharedPtr<FUICommandList>(), FMultiBoxCustomization::None);
	ToolbarBuilder.SetLabelVisibility(EVisibility::Collapsed);
	ToolbarBuilder.SetStyle(&FAppStyle::Get(), "DetailsView.ExtensionToolBar");
	ToolbarBuilder.SetIsFocusable(false);

	for (const FPropertyRowExtensionButton& Extension : ExtensionButtons)
	{
		ToolbarBuilder.AddToolBarButton(Extension.UIAction, NAME_None, Extension.Label, Extension.ToolTip, Extension.Icon);
	}

	return ToolbarBuilder.MakeWidget();
}

bool SDisplayClusterColorGradingColorWheelPanel::FilterDetailTreeNode(const TSharedRef<IDetailTreeNode>& InDetailTreeNode)
{
	if (ColorGradingDataModel)
	{
		if (FDisplayClusterColorGradingDataModel::FColorGradingGroup* ColorGradingGroup = ColorGradingDataModel->GetSelectedColorGradingGroup())
		{
			// Filter out any categories that are not configured by the data model to be displayed in the details section or subsection.
			// All other nodes (which will be any child of the category), should be displayed.
			if (InDetailTreeNode->GetNodeType() == EDetailNodeType::Category)
			{
				return ColorGradingGroup->DetailsViewCategories.Contains(InDetailTreeNode->GetNodeName());
			}
			else
			{
				return true;
			}
		}
	}

	return false;
}

void SDisplayClusterColorGradingColorWheelPanel::SetColorWheelOrientation(EOrientation NewOrientation)
{
	if (ColorWheelOrientation != NewOrientation)
	{
		ColorWheelOrientation = NewOrientation;

		for (const TSharedPtr<SDisplayClusterColorGradingColorWheel>& ColorWheel : ColorWheels)
		{
			if (ColorWheel.IsValid())
			{
				ColorWheel->SetOrientation(ColorWheelOrientation);
			}
		};
	}
}

bool SDisplayClusterColorGradingColorWheelPanel::IsColorWheelOrientationSelected(EOrientation Orientation) const
{
	return ColorWheelOrientation == Orientation;
}

void SDisplayClusterColorGradingColorWheelPanel::ToggleColorWheelVisible(int32 ColorWheelIndex)
{
	if (ColorWheelIndex >= 0 && ColorWheelIndex < HiddenColorWheels.Num())
	{
		HiddenColorWheels[ColorWheelIndex] = !HiddenColorWheels[ColorWheelIndex];
	}
}

bool SDisplayClusterColorGradingColorWheelPanel::IsColorWheelVisible(int32 ColorWheelIndex)
{
	if (ColorWheelIndex >= 0 && ColorWheelIndex < HiddenColorWheels.Num())
	{
		return !HiddenColorWheels[ColorWheelIndex];
	}

	return false;
}

void SDisplayClusterColorGradingColorWheelPanel::OnColorGradingGroupSelectionChanged()
{
	Refresh();
}

void SDisplayClusterColorGradingColorWheelPanel::OnColorGradingElementSelectionChanged()
{
	if (const FDisplayClusterColorGradingDataModel::FColorGradingElement* ColorGradingElement = ColorGradingDataModel->GetSelectedColorGradingElement())
	{
		FillColorWheels(*ColorGradingElement);
	}
	else
	{
		ClearColorWheels();
	}
}

void SDisplayClusterColorGradingColorWheelPanel::OnColorGradingElementCheckedChanged(ECheckBoxState State, FText ElementName)
{
	if (State == ECheckBoxState::Checked && ColorGradingDataModel)
	{
		if (FDisplayClusterColorGradingDataModel::FColorGradingGroup* ColorGradingGroup = ColorGradingDataModel->GetSelectedColorGradingGroup())
		{
			int32 ColorGradingElementIndex = ColorGradingGroup->ColorGradingElements.IndexOfByPredicate([=](const FDisplayClusterColorGradingDataModel::FColorGradingElement& Element)
			{
				return Element.DisplayName.CompareTo(ElementName) == 0;
			});

			ColorGradingDataModel->SetSelectedColorGradingElement(ColorGradingElementIndex);
		}
	}
}

ECheckBoxState SDisplayClusterColorGradingColorWheelPanel::IsColorGradingElementSelected(FText ElementName) const
{
	if (ColorGradingDataModel)
	{
		if (const FDisplayClusterColorGradingDataModel::FColorGradingElement* ColorGradingElement = ColorGradingDataModel->GetSelectedColorGradingElement())
		{
			if (ColorGradingElement->DisplayName.CompareTo(ElementName) == 0)
			{
				return ECheckBoxState::Checked;
			}
		}
	}

	return ECheckBoxState::Unchecked;
}

EVisibility SDisplayClusterColorGradingColorWheelPanel::GetColorWheelPanelVisibility() const
{
	bool bHasObjects = ColorGradingDataModel && ColorGradingDataModel->GetPropertyRowGenerator()->GetSelectedObjects().Num() > 0;
	return bHasObjects ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDisplayClusterColorGradingColorWheelPanel::GetColorWheelVisibility(int32 ColorWheelIndex) const
{
	bool bIsHidden = HiddenColorWheels[ColorWheelIndex];
	return bIsHidden ? EVisibility::Collapsed : EVisibility::Visible;
}

ECheckBoxState SDisplayClusterColorGradingColorWheelPanel::IsColorDisplayModeChecked(EColorDisplayMode InColorDisplayMode) const
{
	bool bIsModeSelected = InColorDisplayMode == ColorDisplayMode;
	return bIsModeSelected ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SDisplayClusterColorGradingColorWheelPanel::OnColorDisplayModeCheckedChanged(ECheckBoxState State, EColorDisplayMode InColorDisplayMode)
{
	if (State == ECheckBoxState::Checked)
	{
		ColorDisplayMode = InColorDisplayMode;
	}
}

FText SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayModeLabel(EColorDisplayMode InColorDisplayMode) const
{
	FText Text;

	switch (InColorDisplayMode)
	{
	case EColorDisplayMode::RGB: Text = LOCTEXT("ColorWheel_RGBColorDisplayModeLabel", "RGB"); break;
	case EColorDisplayMode::HSV: Text = LOCTEXT("ColorWheel_HSVColorDisplayModeLabel", "HSV"); break;
	}

	return Text;
}

FText SDisplayClusterColorGradingColorWheelPanel::GetColorDisplayModeToolTip(EColorDisplayMode InColorDisplayMode) const
{
	FText Text;

	switch (InColorDisplayMode)
	{
	case EColorDisplayMode::RGB: Text = LOCTEXT("ColorWheel_RGBColorDisplayModeToolTip", "Change to RGB color mode"); break;
	case EColorDisplayMode::HSV: Text = LOCTEXT("ColorWheel_HSVColorDisplayModeToolTip", "Change to HSV color mode"); break;
	}

	return Text;
}

#undef LOCTEXT_NAMESPACE