// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMEditor.h"
#include "AssetThumbnail.h"
#include "AssetToolsModule.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialStageExpressions/DMMSETextureSample.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "Components/PrimitiveComponent.h"
#include "DetailLayoutBuilder.h"
#include "DMBlueprintFunctionLibrary.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorCommands.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorSettings.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Engine/World.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/Commands/InputChord.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IDetailTreeNode.h"
#include "IDocumentation.h"
#include "IPropertyRowGenerator.h"
#include "MaterialDomain.h"
#include "Menus/DMToolBarMenus.h"
#include "Misc/CoreDelegates.h"
#include "Model/DynamicMaterialModel.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "SEnumCombo.h"
#include "Slate/Properties/Editors/SDMPropertyEditOpacity.h"
#include "Slate/Properties/SDMBlendMode.h"
#include "Slate/Properties/SDMDomain.h"
#include "Slate/Properties/SDMMaterialParameters.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMComponentEdit.h"
#include "Slate/SDMToolBar.h"
#include "SlateOptMacros.h"
#include "Styling/StyleColors.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SConstraintCanvas.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SDMEditor"

TSharedPtr<FAssetThumbnailPool> SDMEditor::ThumbnailPool = nullptr;
TMap<const SWidget*, TArray<FDMPropertyHandle>> SDMEditor::PropertyHandleMap;

TSharedRef<FAssetThumbnailPool> SDMEditor::GetThumbnailPool()
{
	if (TSharedPtr<FAssetThumbnailPool> SharedPool = UThumbnailManager::Get().GetSharedThumbnailPool())
	{
		return SharedPool.ToSharedRef();
	}

	if (!ThumbnailPool.IsValid())
	{
		ThumbnailPool = MakeShared<FAssetThumbnailPool>(1024);

		FCoreDelegates::OnEnginePreExit.AddLambda([]()
			{
				ThumbnailPool.Reset();
			});
	}

	return ThumbnailPool.ToSharedRef();
}

void SDMEditor::Construct(const FArguments& InArgs, TWeakObjectPtr<UDynamicMaterialModel> InModelWeak)
{
	SetCanTick(true);

	ActiveSlotIndex = 0;

	BindCommands();
	
	ChildSlot
	.HAlign(HAlign_Fill)
	.VAlign(VAlign_Fill)
	[
		SAssignNew(Container, SBox)
		[
			CreateMainLayout()
		]
	];

	SetMaterialModel(InModelWeak.Get());

	UDynamicMaterialEditorSettings::Get()->OnSettingsChanged.AddSP(this, &SDMEditor::OnSettingsChanged);

	static bool bAddedEnginePreExitDelegate = false;

	if (!bAddedEnginePreExitDelegate)
	{
		bAddedEnginePreExitDelegate = true;

		FCoreDelegates::OnEnginePreExit.AddLambda(
			[]
			{
				PropertyHandleMap.Reset();
			}
		);
	}
}

void SDMEditor::PrivateRegisterAttributes(struct FSlateAttributeDescriptor::FInitializer&)
{
	
}

SDMEditor::~SDMEditor()
{
	if (FDynamicMaterialModule::AreUObjectsSafe())
	{
		if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
		{
			if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModel))
			{
				ModelEditorOnlyData->GetOnMaterialBuiltDelegate().RemoveAll(this);
				ModelEditorOnlyData->GetOnValueListUpdateDelegate().RemoveAll(this);
				ModelEditorOnlyData->GetOnSlotListUpdateDelegate().RemoveAll(this);
			}
		}
	}
}

void SDMEditor::ClearEditor()
{
	SetMaterialModel(nullptr);
}

void SDMEditor::SetActiveSlotIndex(int InSlotIndex)
{
	if (InSlotIndex != ActiveSlotIndex && SlotWidgets.IsValidIndex(InSlotIndex))
	{
		ActiveSlotIndex = InSlotIndex;
		SlotWidgets[InSlotIndex]->ClearSelection();
		RefreshSlotsList();
	}
}

FDMPropertyHandle SDMEditor::GetPropertyHandle(const SWidget* InOwner, UDMMaterialComponent* InComponent, const FName& InPropertyName)
{
	TArray<FDMPropertyHandle>& PropertyHandles = PropertyHandleMap.FindOrAdd(InOwner);

	for (const FDMPropertyHandle& ExistingHandle : PropertyHandles)
	{
		if (ExistingHandle.PropertyHandle->GetProperty()->GetFName() == InPropertyName)
		{
			TArray<UObject*> Outers;
			ExistingHandle.PropertyHandle->GetOuterObjects(Outers);

			if (Outers.IsEmpty() == false && Outers[0] == InComponent)
			{
				return ExistingHandle;
			}
		}
	}

	FDMPropertyHandle NewHandle = CreatePropertyHandle(InOwner, InComponent, InPropertyName);
	PropertyHandles.Add(NewHandle);

	return NewHandle;
}

void SDMEditor::ClearPropertyHandles(const SWidget* InOwner)
{
	PropertyHandleMap.Remove(InOwner);
}

FDMPropertyHandle SDMEditor::CreatePropertyHandle(const void* InOwner, UDMMaterialComponent* InComponent,
	const FName& InPropertyName)
{
	FDMPropertyHandle PropertyHandle;

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FPropertyRowGeneratorArgs RowGeneratorArgs;
	RowGeneratorArgs.NotifyHook = InComponent;

	PropertyHandle.PropertyRowGenerator = PropertyEditor.CreatePropertyRowGenerator(RowGeneratorArgs);
	PropertyHandle.PropertyRowGenerator->SetObjects({InComponent});

	for (const TSharedRef<IDetailTreeNode>& CategoryNode : PropertyHandle.PropertyRowGenerator->GetRootTreeNodes())
	{
		if (CategoryNode->GetNodeName() != TEXT("Material Designer"))
		{
			continue;
		}

		TArray<TSharedRef<IDetailTreeNode>> ChildNodes;
		CategoryNode->GetChildren(ChildNodes);

		for (const TSharedRef<IDetailTreeNode>& ChildNode : ChildNodes)
		{
			if (ChildNode->GetNodeType() != EDetailNodeType::Item)
			{
				continue;
			}

			if (ChildNode->GetNodeName() != InPropertyName)
			{
				continue;
			}

			PropertyHandle.DetailTreeNode = ChildNode;
			PropertyHandle.PropertyHandle = ChildNode->CreatePropertyHandle();
			return PropertyHandle;
		}
	}

	return PropertyHandle;
}

void SDMEditor::SetMaterialModel(UDynamicMaterialModel* InMaterialModel)
{
	MaterialModelWeak = InMaterialModel;

	SlotsContainer.Reset();
	SlotWidgets.Empty();

	Toolbar->SetMaterialModel(InMaterialModel);

	if (!InMaterialModel)
	{
		Container->SetContent(SDMEditor::GetEmptyContent());
		return;
	}

	Container->SetContent(CreateMainLayout());

	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(InMaterialModel))
	{
		ModelEditorOnlyData->GetOnMaterialBuiltDelegate().AddSP(this, &SDMEditor::OnMaterialBuilt);
		ModelEditorOnlyData->GetOnValueListUpdateDelegate().AddSP(this, &SDMEditor::OnValuesUpdated);
		ModelEditorOnlyData->GetOnSlotListUpdateDelegate().AddSP(this, &SDMEditor::OnSlotsUpdated);
	}
}

void SDMEditor::SetMaterialObjectProperty(const FDMObjectMaterialProperty& InObjectProperty)
{
	UDynamicMaterialModel* MaterialModel = InObjectProperty.GetMaterialModel();

	if (IsValid(MaterialModel))
	{
		ObjectProperty = InObjectProperty;
		SetMaterialModel(MaterialModel);
	}
	else
	{
		ObjectProperty.Reset();
	}
}

void SDMEditor::SetMaterialActor(AActor* InActor)
{
	if (UDynamicMaterialEditorSettings* Settings = UDynamicMaterialEditorSettings::Get())
	{
		if (!Settings->bFollowSelection)
		{
			return;
		}
	}

	if (!Container.IsValid())
	{
		return;
	}

	if (!IsValid(InActor))
	{
		Container->SetContent(SDMEditor::GetEmptyContent());
		return;
	}

	Toolbar->SetMaterialActor(InActor);

	Container->SetContent(
		SNew(SBox)
		.HAlign(HAlign_Center)
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		[
			CreateActorMaterialSlotSelector(InActor)
		]
	);
}

TSharedRef<SWidget> SDMEditor::CreateActorMaterialSlotSelector(const AActor* InActor)
{
	TArray<TSharedPtr<FDMObjectMaterialProperty>> MaterialProperties = Toolbar->GetMaterialProperties();
	if (MaterialProperties.IsEmpty())
	{
		return 
			SNew(STextBlock)
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
			.Text(LOCTEXT("NoMaterialSlot", "\n\nThe selected actor contains no primitive components with material slots."));
	}

	TSharedRef<SVerticalBox> ListOuter = 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Center)
		.Padding(0.0f, 20.0f, 0.0f, 20.0f)
		[
			SNew(STextBlock)
			.TextStyle(FDynamicMaterialEditorStyle::Get(), "ActorNameBig")
			.Text(Toolbar.Get(), &SDMToolBar::GetSlotActorDisplayName)
		];

	const UObject* CurrentOuter = nullptr;

	for (const TSharedPtr<FDMObjectMaterialProperty>& MaterialSlot : MaterialProperties)
	{
		if (!MaterialSlot.IsValid())
		{
			continue;
		}

		// Only show material slots on the selector
		if (MaterialSlot->Property)
		{
			continue;
		}

		const UObject* Outer = MaterialSlot->OuterWeak.Get();
		if (!IsValid(Outer))
		{
			continue;
		}

		UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(MaterialSlot->OuterWeak.Get());

		if (!PrimComponent)
		{
			continue;
		}

		if (Outer != CurrentOuter)
		{
			ListOuter->AddSlot()
				.AutoHeight()
				.Padding(0.f, CurrentOuter == nullptr ? 0.f : 10.f, 0.f, 5.f)
				[
					SNew(STextBlock)
					.TextStyle(FDynamicMaterialEditorStyle::Get(), "ComponentNameBig")
					.Text(FText::FromString(Outer->GetName()))
				];

			CurrentOuter = Outer;
		}

		TWeakPtr<FDMObjectMaterialProperty> MaterialSlotWeak = MaterialSlot;

		constexpr int32 ThumbnailSize = 48;
		TSharedRef<FAssetThumbnail> Thumbnail = MakeShared<FAssetThumbnail>(PrimComponent->GetMaterial(MaterialSlot->Index), ThumbnailSize, ThumbnailSize, UThumbnailManager::Get().GetSharedThumbnailPool());

		FAssetThumbnailConfig ThumbnailConfig;
		ThumbnailConfig.GenericThumbnailSize = ThumbnailSize;

		ListOuter->AddSlot()
			.AutoHeight()
			.HAlign(EHorizontalAlignment::HAlign_Left)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 5.f, 5.f, 5.f)
				.VAlign(EVerticalAlignment::VAlign_Center)
				[
					Thumbnail->MakeThumbnailWidget(ThumbnailConfig)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.f, 5.f, 0.5, 5.f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 5.f, 0.f, 5.f)
					[
						SNew(STextBlock)
						.Text(MaterialSlot->GetPropertyName(true))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.f, 0.f, 0.f, 5.f)
					[
						SNew(SButton)
						.ContentPadding(FMargin(2.f, 2.f, 2.f, 2.f))
						.OnClicked(this, &SDMEditor::OnCreateMaterialButtonClicked, MaterialSlotWeak)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("CreateMaterial", "Create Material"))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
				]
			];
	}

	return SNew(SScrollBox)
		.Orientation(EOrientation::Orient_Vertical)
		+ SScrollBox::Slot()
		[
			ListOuter
		];
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SDMEditor::GetEmptyContent()
{
	return SNew(SBox)
		.HAlign(HAlign_Center)
		.Padding(5.0f, 5.0f, 5.0f, 5.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("NoActiveMaterial", "No active Material Designer Instance."))
		];
}

void SDMEditor::BindCommands()
{
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(
		FDynamicMaterialEditorCommands::Get().AddDefaultLayer,
		FExecuteAction::CreateSP(this, &SDMEditor::AddNewLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanAddNewLayer)
	);

	CommandList->MapAction(
		FDynamicMaterialEditorCommands::Get().InsertDefaultLayerAbove,
		FExecuteAction::CreateSP(this, &SDMEditor::InsertNewLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanInsertNewLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateSP(this, &SDMEditor::CopySelectedLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanCopySelectedLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateSP(this, &SDMEditor::CutSelectedLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanCutSelectedLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateSP(this, &SDMEditor::PasteLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanPasteLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateSP(this, &SDMEditor::DuplicateSelectedLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanDuplicateSelectedLayer)
	);

	CommandList->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateSP(this, &SDMEditor::DeleteSelectedLayer),
		FCanExecuteAction::CreateSP(this, &SDMEditor::CanDeleteSelectedLayer)
	);
}

TSharedRef<SWidget> SDMEditor::CreateMainLayout()
{
	return 
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SAssignNew(Toolbar, SDMToolBar)
			.MaterialModel(MaterialModelWeak.Get())
			.OnSlotChanged(this, &SDMEditor::OnToolBarPropertyChanged)
			.OnGetSettingsMenu(this, &SDMEditor::MakeToolBarSettingsMenu)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(GlobalOpacityContainer, SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				SNew(SBorder)
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Center)
				.Padding(3.0f, 3.0f, 3.0f, 3.0f)
				.BorderImage(FDynamicMaterialEditorStyle::GetBrush("Border.Bottom"))
				.BorderBackgroundColor(FLinearColor(1, 1, 1, 0.05f))
				[
					CreateMaterialSettingsRow()
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(GlobalOpacityContainer, SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				CreateGlobalOpacityWidget()
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			UE::DynamicMaterialEditor::bGlobalValuesEnabled && MaterialModelWeak.IsValid()
				? CreateParametersArea()
				: SNullWidget::NullWidget
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Top)
		[
			SAssignNew(SlotPickerContainer, SBox)
			[
				CreateSlotPickerWidget()
			]
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		[
			SAssignNew(SlotsContainer, SBox)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			[
				CreateSlotsWidget()
			]
		];
}

TSharedRef<SWidget> SDMEditor::CreateMaterialSettingsRow()
{
	return 
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(5.0f, 0.0f, 0.0f, 2.0f)
		[
			SNew(SDMDomain)
			.ToolTipText(LOCTEXT("MaterialDesignerInstanceDomainTooltip", "Change the Material Designer Instance Domain"))
			.SelectedItem(this, &SDMEditor::GetSelectedDomain)
			.OnSelectedItemChanged(this, &SDMEditor::OnDomainChanged)
			.IsEnabled(this, &SDMEditor::CanChangeDomain)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Center)
		.Padding(5.0f, 0.0f, 0.0f, 2.0f)
		[
			SNew(SDMBlendMode)
			.ToolTipText(LOCTEXT("MaterialDesignerInstanceBlendModeTooltip", "Change the Material Designer Instance Blend Mode"))
			.SelectedItem(this, &SDMEditor::GetSelectedBlendMode)
			.OnSelectedItemChanged(this, &SDMEditor::OnBlendModeChanged)
			.IsEnabled(this, &SDMEditor::CanChangeBlendType)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(10.0f, 0.0f, 0.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("MaterialDesignerInstanceTypeTooltip", "Enables the material's TSR pixel animation flag. Not available in translucent blend modes."))
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.Padding(0.f, 0.f, 5.f, 0.f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MaterialDesignerInstanceAnimaed", "Animated"))
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SDMEditor::IsMaterialAnimated)
				.IsEnabled(this, &SDMEditor::CanMaterialBeAnimated)
				.OnCheckStateChanged(this, &SDMEditor::OnMaterialAnimatedChanged)
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(10.f, 0.0f, 5.0f, 2.0f)
		[
			SNew(SHorizontalBox)
			.ToolTipText(LOCTEXT("MaterialDesignerInstanceUnlitTooltip", "Toggle between Default Lit and Unlit for this Material Designer Instance."))
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 5.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaterialDesignerInstanceUnlit", "Unlit"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.HAlign(HAlign_Left)
			.VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked(this, &SDMEditor::IsMaterialUnlit)
				.OnCheckStateChanged(this, &SDMEditor::OnMaterialUnlitChanged)
				.IsEnabled(this, &SDMEditor::CanChangeMaterialShadingModel)
			]
		];
}

TSharedRef<SWidget> SDMEditor::CreateGlobalOpacityWidget()
{
	UDMMaterialValueFloat1* OpacityValue = nullptr;

	if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		OpacityValue = MaterialModel->GetGlobalOpacityValue();
	}

	if (!OpacityValue)
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SDMPropertyEdit> GlobalOpacityWidget = SNew(SDMPropertyEditOpacity, SharedThis(this), OpacityValue);
	GlobalOpacityWidget->SetEnabled(TAttribute<bool>::CreateSP(this, &SDMEditor::IsGlobalOpacityEnabled));

	TSharedRef<SWidget> GlobalOpacityButtons = SDMComponentEdit::CreateExtensionButtons(SharedThis(this), OpacityValue, UDMMaterialValue::ValueName, true, FSimpleDelegate());
	GlobalOpacityButtons->SetEnabled(TAttribute<bool>::CreateSP(this, &SDMEditor::IsGlobalOpacityEnabled));

	TSharedPtr<SWidget> RowLabel;

	TSharedRef<SWidget> Row =
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(10.0f, 5.0f)
		[
			SAssignNew(RowLabel, SBox)
			.Padding(0.f, 0.f, 5.f, 0.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GlobalOpacity", "Global Opacity"))
			]
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Top)
		[
			GlobalOpacityWidget
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		[
			GlobalOpacityButtons
		];

	RowLabel->SetOnMouseButtonDown(FPointerEventHandler::CreateStatic(&SDMPropertyEdit::CreateRightClickDetailsMenu, GlobalOpacityWidget.ToWeakPtr()));

	return Row;
}

TSharedRef<SWidget> SDMEditor::CreateParametersArea()
{
	return 
		SNew(SExpandableArea)
		.InitiallyCollapsed(false)
		.HeaderPadding(FMargin(3.0f, 5.0f, 3.0f, 5.0f))
		.HeaderContent()
		[
			SNew(SBox)
			.VAlign(EVerticalAlignment::VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaterialParameters", "Material Parameters"))
				.Font(IDetailLayoutBuilder::GetDetailFontBold())
			]
		]
		.BodyContent()
		[
			SAssignNew(ParametersWidget, SDMMaterialParameters, MaterialModelWeak)
		];
}

TSharedRef<SWidget> SDMEditor::CreateSlotPickerWidget()
{
	TSharedRef<SHorizontalBox> SlotSelector = SNew(SHorizontalBox);

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);

	if (!ModelEditorOnlyData)
	{
		return SlotSelector;
	}

	const TArray<UDMMaterialSlot*>& Slots = ModelEditorOnlyData->GetSlots();

	if (Slots.IsEmpty())
	{
		return SlotSelector;
	}

	if (Slots.IsValidIndex(0))
	{
		SlotSelector->AddSlot()
			.Padding(3.f, 0.f, 3.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.IsChecked(this, &SDMEditor::GetRGBSlotCheckState_HasSlot)
				.OnCheckStateChanged(this, &SDMEditor::OnRGBSlotCheckStateChanged_HasSlot)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RGBSlot", "RGB"))
				]
			];		
	}
	else
	{
		SlotSelector->AddSlot()
			.Padding(3.f, 0.f, 3.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.IsChecked(this, &SDMEditor::GetRGBSlotCheckState_NoSlot)
				.OnCheckStateChanged(this, &SDMEditor::OnRGBSlotCheckStateChanged_NoSlot)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RGBSlot", "RGB"))
				]
			];
	}

	if (Slots.IsValidIndex(1))
	{
		SlotSelector->AddSlot()
			.Padding(0.f, 0.f, 3.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.IsEnabled(this, &SDMEditor::GetOpacityButtonEnabled_HasSlot)
				.IsChecked(this, &SDMEditor::GetOpacitySlotCheckState_HasSlot)
				.OnCheckStateChanged(this, &SDMEditor::OnOpacitySlotCheckStateChanged_HasSlot)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("OpacitySlot", "Opacity"))
				]
			];		
	}
	else
	{
		SlotSelector->AddSlot()
			.Padding(0.f, 0.f, 3.f, 0.f)
			[
				SNew(SCheckBox)
				.Style(FAppStyle::Get(), "DetailsView.SectionButton")
				.HAlign(EHorizontalAlignment::HAlign_Center)
				.IsEnabled(this, &SDMEditor::GetOpacityButtonEnabled_HasSlot)
				.IsChecked(this, &SDMEditor::GetOpacitySlotCheckState_NoSlot)
				.OnCheckStateChanged(this, &SDMEditor::OnOpacitySlotCheckStateChanged_NoSlot)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("AddOpacity", "Opacity"))
				]
			];
	}

	return SlotSelector;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedPtr<SDMSlot> SDMEditor::GetSlotWidget(UDMMaterialSlot* Slot) const
{
	if (ensure(IsValid(Slot)))
	{
		for (const TSharedPtr<SDMSlot>& SlotWidget : SlotWidgets)
		{
			if (SlotWidget.IsValid() && SlotWidget->GetSlot() == Slot)
			{
				return SlotWidget;
			}
		}
	}

	return nullptr;
}

void SDMEditor::RefreshGlobalOpacitySlider()
{
	if (GlobalOpacityContainer.IsValid())
	{
		GlobalOpacityContainer->SetContent(CreateGlobalOpacityWidget());
	}
}

void SDMEditor::RefreshParametersList()
{
	if (ParametersWidget.IsValid())
	{
		ParametersWidget->RefreshWidgets();
	}
}

void SDMEditor::RefreshSlotPickerList()
{
	if (SlotPickerContainer.IsValid())
	{
		SlotPickerContainer->SetContent(CreateSlotPickerWidget());
	}
}

void SDMEditor::RefreshSlotsList()
{
	if (SlotsContainer.IsValid())
	{
		SlotsContainer->SetContent(CreateSlotsWidget());
	}
}

void SDMEditor::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!MaterialModelWeak.IsValid())
	{
		return;
	}

	bool bValidModel = false;

	if (ObjectProperty.IsValid())
	{
		bValidModel = IsValid(ObjectProperty.GetMaterialModel());
	}
	else if (UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get())
	{
		auto CheckValidity = [MaterialModel]()
		{
			if (UWorld* World = MaterialModel->GetWorld())
			{
				if (UDMWorldSubsystem* WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
				{
					// ExecuteIfBound doesn't work with return values
					if (WorldSubsystem->GetIsValidDelegate().IsBound()
						&& WorldSubsystem->GetIsValidDelegate().Execute(MaterialModel) == false)
					{
						return false;
					}
				}
			}

			UActorComponent* ComponentOuter = MaterialModel->GetTypedOuter<UActorComponent>();
			if (ComponentOuter && !IsValid(ComponentOuter))
			{
				return false;
			}

			AActor* ActorOuter = MaterialModel->GetTypedOuter<AActor>();
			if (ActorOuter && !IsValid(ActorOuter))
			{
				return false;
			}

			UPackage* PackageOuter = MaterialModel->GetPackage();
			if (PackageOuter && !IsValid(PackageOuter))
			{
				return false;
			}

			return true;
		};

		bValidModel = CheckValidity();
	}

	if (!bValidModel)
	{
		ClearEditor();
	}
	else if (FDynamicMaterialModule::AreUObjectsSafe())
	{
		if (Toolbar.IsValid() && Toolbar->GetMaterialModel() != MaterialModelWeak)
		{
			Toolbar->SetMaterialModel(MaterialModelWeak.Get());
		}
		else
		{
			bool bHasInvalidSlotWidget = false;
			for (const TWeakPtr<SDMSlot> SlotWeak : SlotWidgets)
			{
				TSharedPtr<SDMSlot> Slot = SlotWeak.Pin();
				if (Slot.IsValid() && !Slot->CheckValidity())
				{
					bHasInvalidSlotWidget = true;
					break;
				}
			}
			if (bHasInvalidSlotWidget)
			{
				RefreshSlotsList();
			}
		}
	}
}

FReply SDMEditor::OnAddSlotButtonClicked()
{
	if (!MaterialModelWeak.IsValid())
	{
		return FReply::Unhandled();
	}

	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);

	if (!ModelEditorOnlyData)
	{
		return FReply::Unhandled();
	}

	ModelEditorOnlyData->AddSlot();

	return FReply::Handled();
}

void SDMEditor::OnMaterialBuilt(UDynamicMaterialModel* InMaterialModel)
{
}

void SDMEditor::OnValuesUpdated(UDynamicMaterialModel* InMaterialModel)
{
	RefreshParametersList();
}

void SDMEditor::OnSlotsUpdated(UDynamicMaterialModel* InMaterialModel)
{
	RefreshSlotsList();
}

bool SDMEditor::IsGlobalOpacityEnabled() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->GetBlendMode() == BLEND_Translucent || ModelEditorOnlyData->GetBlendMode() == BLEND_Masked;
	}

	return false;
}

ECheckBoxState SDMEditor::GetRGBSlotCheckState_HasSlot() const
{
	return GetActiveSlotIndex() == 0 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SDMEditor::OnRGBSlotCheckStateChanged_HasSlot(ECheckBoxState InCheckState)
{
	SetActiveSlotIndex(0);
}

ECheckBoxState SDMEditor::GetRGBSlotCheckState_NoSlot() const
{
	return ECheckBoxState::Unchecked;
}

void SDMEditor::OnRGBSlotCheckStateChanged_NoSlot(ECheckBoxState InCheckState)
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		if (ModelEditorOnlyData->AddSlot())
		{
			RefreshSlotPickerList();
			SetActiveSlotIndex(0);
		}
	}
}

bool SDMEditor::GetOpacityButtonEnabled_NoSlot() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->GetDomain() != EMaterialDomain::MD_PostProcess
			&& ModelEditorOnlyData->GetBlendMode() != EBlendMode::BLEND_Opaque;
	}

	return false;
}

bool SDMEditor::GetOpacityButtonEnabled_HasSlot() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->GetBlendMode() != EBlendMode::BLEND_Opaque;
	}

	return false;
}

ECheckBoxState SDMEditor::GetOpacitySlotCheckState_HasSlot() const
{
	return GetActiveSlotIndex() == 1 ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SDMEditor::OnOpacitySlotCheckStateChanged_HasSlot(ECheckBoxState InCheckState)
{
	SetActiveSlotIndex(1);
}

ECheckBoxState SDMEditor::GetOpacitySlotCheckState_NoSlot() const
{
	return ECheckBoxState::Unchecked;
}

void SDMEditor::OnOpacitySlotCheckStateChanged_NoSlot(ECheckBoxState InCheckState)
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		if (UDMMaterialSlot* NewSlot = ModelEditorOnlyData->AddSlot())
		{
			if (UDMMaterialLayerObject* Layer = NewSlot->GetLayer(0))
			{
				FScopedTransaction Transaction(LOCTEXT("AddOpacitySlot", "Material Designer Add Opacity Slot"));
				Layer->Modify();

				switch (ModelEditorOnlyData->GetBlendMode())
				{
					case EBlendMode::BLEND_Masked:
						Layer->SetMaterialProperty(EDMMaterialPropertyType::OpacityMask);
						break;

					default:
						Layer->SetMaterialProperty(EDMMaterialPropertyType::Opacity);
						break;
				}
			}

			RefreshSlotPickerList();
			SetActiveSlotIndex(1);
		}
	}
}

FReply SDMEditor::OnCreateMaterialButtonClicked(TWeakPtr<FDMObjectMaterialProperty> InMaterialProperty)
{
	if (TSharedPtr<FDMObjectMaterialProperty> MaterialProperty = InMaterialProperty.Pin())
	{
		UDynamicMaterialModel* NewModel = UDMBlueprintFunctionLibrary::CreateDynamicMaterialInObject(*MaterialProperty);

		if (NewModel)
		{
			if (Toolbar.IsValid())
			{
				Toolbar->SetMaterialModel(NewModel);
			}
		}
	}

	return FReply::Handled();
}

void SDMEditor::OnToolBarPropertyChanged(TSharedPtr<FDMObjectMaterialProperty> InNewSelectedProperty)
{
	if (InNewSelectedProperty.IsValid())
	{
		SetMaterialModel(InNewSelectedProperty->GetMaterialModel());
	}
}

TSharedRef<SWidget> SDMEditor::MakeToolBarSettingsMenu()
{
	return FDMToolBarMenus::MakeEditorLayoutMenu(SharedThis(this));
}

void SDMEditor::OnSettingsChanged(const FPropertyChangedEvent& InPropertyChangedEvent)
{
	SetMaterialModel(MaterialModelWeak.Get());
}

bool SDMEditor::CanAddNewLayer() const
{
	return SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& MaterialModelWeak.IsValid();
}

void SDMEditor::AddNewLayer()
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex))
	{
		SlotWidgets[ActiveSlotIndex]->AddNewLayer_Expression(
			TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSample::StaticClass()),
			EDMMaterialLayerStage::All
		);

		SlotWidgets[ActiveSlotIndex]->InvalidateMainWidget();
	}
}

bool SDMEditor::CanInsertNewLayer() const
{
	return SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().Num() == 1
		&& MaterialModelWeak.IsValid();
}

void SDMEditor::InsertNewLayer()
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex))
	{
		if (UDMMaterialLayerObject* SelectedLayer = SlotWidgets[ActiveSlotIndex]->GetSelectedLayer())
		{
			if (UDMMaterialSlot* Slot = SelectedLayer->GetSlot())
			{
				// Added here because stuff is done after the layer is added
				FScopedTransaction Transaction(LOCTEXT("InsertNewLayer", "Material Designer Insert Layer"));
				Slot->Modify();

				SlotWidgets[ActiveSlotIndex]->AddNewLayer_Expression(
					TSubclassOf<UDMMaterialStageExpression>(UDMMaterialStageExpressionTextureSample::StaticClass()),
					EDMMaterialLayerStage::All
				);

				Slot->MoveLayerAfter(Slot->GetLayers().Last(), SelectedLayer);

				SlotWidgets[ActiveSlotIndex]->InvalidateMainWidget();
			}
		}
	}
}

bool SDMEditor::CanCopySelectedLayer() const
{
	return SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().Num() == 1
		&& MaterialModelWeak.IsValid();
}

void SDMEditor::CopySelectedLayer()
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().Num() == 1)
	{
		if (const UDMMaterialLayerObject* Layer = SlotWidgets[ActiveSlotIndex]->GetSelectedLayer())
		{
			FPlatformApplicationMisc::ClipboardCopy(*Layer->SerializeToString());
		}
	}
}

bool SDMEditor::CanCutSelectedLayer() const
{
	return CanCopySelectedLayer() && CanDeleteSelectedLayer();
}

void SDMEditor::CutSelectedLayer()
{
	CopySelectedLayer();
	DeleteSelectedLayer();
}

bool SDMEditor::CanPasteLayer() const
{
	if (!MaterialModelWeak.IsValid())
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return !ClipboardContent.IsEmpty();
}

void SDMEditor::PasteLayer()
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		if (UDMMaterialSlot* Slot = ModelEditorOnlyData->GetSlot(ActiveSlotIndex))
		{
			FString SerializeString;
			FPlatformApplicationMisc::ClipboardPaste(SerializeString);

			if (UDMMaterialLayerObject* PastedLayer = UDMMaterialLayerObject::DeserializeFromString(Slot, SerializeString))
			{
				FScopedTransaction Transaction(LOCTEXT("PasteLayer", "Material Designer Paste Layer"));
				Slot->Modify();
				Slot->PasteLayer(PastedLayer);

				SlotWidgets[ActiveSlotIndex]->InvalidateMainWidget();
			}
		}
	}
}

bool SDMEditor::CanDuplicateSelectedLayer() const
{
	// There's no "can add" check, so only copy is tested.
	return CanCopySelectedLayer();
}

void SDMEditor::DuplicateSelectedLayer()
{
	FString PastedText;
	FPlatformApplicationMisc::ClipboardPaste(PastedText);

	// Added here to set the transaction description
	FScopedTransaction Transaction(LOCTEXT("DuplicateLayer", "Material Designer Duplicate Layer"));

	CopySelectedLayer();
	PasteLayer();

	FPlatformApplicationMisc::ClipboardCopy(*PastedText);
}

bool SDMEditor::CanDeleteSelectedLayer() const
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().IsEmpty() == false)
	{
		return SlotWidgets[ActiveSlotIndex]->GetLayerRowsButtonsCanRemove();
	}

	return false;
}

void SDMEditor::DeleteSelectedLayer()
{
	if (SlotWidgets.IsValidIndex(ActiveSlotIndex)
		&& SlotWidgets[ActiveSlotIndex]->GetSelectedLayerIndices().IsEmpty() == false)
	{
		SlotWidgets[ActiveSlotIndex]->OnLayerRowButtonsRemoveClicked();
	}
}

FReply SDMEditor::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList.IsValid())
	{
		if (CommandList->ProcessCommandBindings(InKeyEvent))
		{
			return FReply::Handled();
		}
	}

	// We accept the delete key bind, so we don't want this accidentally deleting actors and such.
	// Always return handled to stop the event bubbling.
	const TArray<TSharedRef<const FInputChord>> DeleteChords = {
		FGenericCommands::Get().Delete->GetActiveChord(EMultipleKeyBindingIndex::Primary),
		FGenericCommands::Get().Delete->GetActiveChord(EMultipleKeyBindingIndex::Secondary)
	};

	for (const TSharedRef<const FInputChord>& DeleteChord : DeleteChords)
	{
		if (DeleteChord->Key == InKeyEvent.GetKey())
		{
			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SDMEditor::PostUndo(bool bSuccess)
{
	OnUndo();
}

void SDMEditor::PostRedo(bool bSuccess)
{
	OnUndo();
}

TSharedRef<SWidget> SDMEditor::CreateSlotsWidget()
{
	SlotWidgets.Empty();

	auto CreateEmptySlotsContent = []() -> TSharedRef<SBox>
	{
		return 
			SNew(SBox)
			.HAlign(EHorizontalAlignment::HAlign_Center)
			.VAlign(EVerticalAlignment::VAlign_Center)
			.Padding(10.f, 5.f, 10.f, 5.f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SlotsContent", "Slots Content"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	};
	
	UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak);

	if (!ModelEditorOnlyData)
	{
		return CreateEmptySlotsContent();
	}

	const TArray<UDMMaterialSlot*>& Slots = ModelEditorOnlyData->GetSlots();

	if (Slots.IsEmpty())
	{
		return CreateEmptySlotsContent();
	}

	for (int32 SlotIdx = 0; SlotIdx < Slots.Num(); ++SlotIdx)
	{
		UDMMaterialSlot* Slot = Slots[SlotIdx];

		TSharedRef<SDMSlot> SlotWidget =
			SNew(SDMSlot, SharedThis(this), Slot)
			.SlotPreviewSize_Lambda([]() { return UDynamicMaterialEditorSettings::Get()->SlotPreviewSize; })
			.LayerPreviewSize_Lambda([]() { return UDynamicMaterialEditorSettings::Get()->LayerPreviewSize; });

		SlotWidgets.Add(SlotWidget);
	}

	return SlotWidgets.IsValidIndex(ActiveSlotIndex)
		? SlotWidgets[ActiveSlotIndex].ToSharedRef()
		: SNullWidget::NullWidget;
}

EMaterialDomain SDMEditor::GetSelectedDomain() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->GetDomain();
	}

	return EMaterialDomain::MD_Surface;
}

void SDMEditor::OnDomainChanged(const EMaterialDomain InDomain)
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		FScopedTransaction Transaction(LOCTEXT("ChangeDomain", "Material Designer Change Domain"));
		ModelEditorOnlyData->Modify();
		ModelEditorOnlyData->SetDomain(InDomain);

		RefreshSlotPickerList();

		if (InDomain == EMaterialDomain::MD_PostProcess)
		{
			SetActiveSlotIndex(0);
		}
	}
}

bool SDMEditor::CanChangeDomain() const
{
	return true;
}

EBlendMode SDMEditor::GetSelectedBlendMode() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->GetBlendMode();
	}

	return EBlendMode::BLEND_Opaque;
}

void SDMEditor::OnBlendModeChanged(const EBlendMode InBlendMode)
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		FScopedTransaction Transaction(LOCTEXT("ChangeBlendMode", "Material Designer Change Blend Mode"));
		ModelEditorOnlyData->Modify();
		ModelEditorOnlyData->SetBlendMode(InBlendMode);

		RefreshSlotPickerList();

		if (InBlendMode == EBlendMode::BLEND_Opaque)
		{
			SetActiveSlotIndex(0);
		}
	}
}

bool SDMEditor::CanChangeBlendType() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->GetDomain() != EMaterialDomain::MD_PostProcess;
	}

	return false;
}

ECheckBoxState SDMEditor::IsMaterialUnlit() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->GetShadingModel() == EDMMaterialShadingModel::Unlit
			? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

void SDMEditor::OnMaterialUnlitChanged(const ECheckBoxState InNewCheckState)
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		FScopedTransaction Transaction(LOCTEXT("ChangeShadingModel", "Material Designer Change Shading Model"));
		ModelEditorOnlyData->Modify();
		ModelEditorOnlyData->SetShadingModel(InNewCheckState == ECheckBoxState::Checked
			? EDMMaterialShadingModel::Unlit : EDMMaterialShadingModel::DefaultLit);
	}
}

bool SDMEditor::CanMaterialBeAnimated() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		switch (ModelEditorOnlyData->GetBlendMode())
		{
			case EBlendMode::BLEND_Masked:
			case EBlendMode::BLEND_Opaque:
				return true;

			default:
				return false;
		}
	}

	return false;
}

bool SDMEditor::CanChangeMaterialShadingModel() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->GetDomain() != EMaterialDomain::MD_PostProcess;
	}

	return false;
}

ECheckBoxState SDMEditor::IsMaterialAnimated() const
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		return ModelEditorOnlyData->IsPixelAnimationFlagSet()
			? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return ECheckBoxState::Unchecked;
}

void SDMEditor::OnMaterialAnimatedChanged(const ECheckBoxState InNewCheckState)
{
	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		FScopedTransaction Transaction(LOCTEXT("ToggleAnimated", "Material Designer Toggle Animated"));
		ModelEditorOnlyData->Modify();
		ModelEditorOnlyData->SetPixelAnimationFlag(InNewCheckState == ECheckBoxState::Checked);
	}
}

void SDMEditor::OnUndo()
{
	UDynamicMaterialModel* MaterialModel = MaterialModelWeak.Get();

	if (!IsValid(MaterialModel))
	{
		return;
	}

	if (UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = UDynamicMaterialModelEditorOnlyData::Get(MaterialModelWeak))
	{
		if (ModelEditorOnlyData->GetBlendMode() == BLEND_Opaque)
		{
			SetActiveSlotIndex(0);
		}
	}
}

#undef LOCTEXT_NAMESPACE
