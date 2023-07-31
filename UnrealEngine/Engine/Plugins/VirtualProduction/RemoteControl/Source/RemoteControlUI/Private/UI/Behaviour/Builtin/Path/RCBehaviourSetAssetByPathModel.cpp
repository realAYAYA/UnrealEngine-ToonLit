// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCBehaviourSetAssetByPathModel.h"

#include "Controller/RCController.h"
#include "Editor.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Modules/ModuleManager.h"
#include "RCVirtualProperty.h"
#include "RCVirtualPropertyContainer.h"
#include "RemoteControlPreset.h"
#include "SRCBehaviourSetAssetByPath.h"
#include "Selection.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FRCSetAssetByPathBehaviourModel"

FRCSetAssetByPathBehaviourModel::FRCSetAssetByPathBehaviourModel(URCSetAssetByPathBehaviour* SetAssetByPathBehaviour)
	: FRCBehaviourModel(SetAssetByPathBehaviour)
	, SetAssetByPathBehaviourWeakPtr(SetAssetByPathBehaviour)
{
	FPropertyRowGeneratorArgs Args;
	Args.bShouldShowHiddenProperties = true;
	PropertyRowGenerator = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);
	PropertyRowGeneratorArray = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreatePropertyRowGenerator(Args);

	if (SetAssetByPathBehaviour)
	{
		PropertyRowGenerator->SetStructure(SetAssetByPathBehaviour->PropertyInContainer->CreateStructOnScope());
		DetailTreeNodeWeakPtr.Empty();
		for (const TSharedRef<IDetailTreeNode>& CategoryNode : PropertyRowGenerator->GetRootTreeNodes())
		{
			TArray<TSharedRef<IDetailTreeNode>> Children;
			CategoryNode->GetChildren(Children);
			for (const TSharedRef<IDetailTreeNode>& Child : Children)
			{
				DetailTreeNodeWeakPtr.Add(Child);
			}
		}

		// Secondary TArray Struct
		PropertyRowGeneratorArray->SetStructure(MakeShareable(new FStructOnScope(FRCSetAssetPath::StaticStruct(), (uint8*) &SetAssetByPathBehaviour->PathStruct)));
		PropertyRowGeneratorArray->OnFinishedChangingProperties().AddLambda([this](const FPropertyChangedEvent& InEvent)
		{
			if (InEvent.ChangeType != EPropertyChangeType::ValueSet)
			{
				return;
			}

			RefreshPathAndPreview();
		});
		PropertyRowGeneratorArray->OnRowsRefreshed().AddLambda([this]()
		{
			RefreshPathAndPreview();
		});
		DetailTreeNodeWeakPtrArray.Empty();
		for (const TSharedRef<IDetailTreeNode>& CategoryNode : PropertyRowGeneratorArray->GetRootTreeNodes())
		{
			TArray<TSharedRef<IDetailTreeNode>> Children;
			CategoryNode->GetChildren(Children);
			for (const TSharedRef<IDetailTreeNode>& Child : Children)
			{
				DetailTreeNodeWeakPtrArray.Add(Child);
			}
		}
	}

	PreviewPathWidget = SNew(STextBlock)
		.AutoWrapText(true);
	PathArrayWidget = SNew(SBox);

	Cast<URCSetAssetByPathBehaviour>(GetBehaviour())->UpdateTargetEntity();
	TWeakPtr<const FRemoteControlEntity> InitialSelected = Cast<URCSetAssetByPathBehaviour>(GetBehaviour())->GetTargetEntity();
	
	RefreshPathAndPreview();
	SelectorBox = GetSelectorWidget(InitialSelected);
}

TSharedRef<SWidget> FRCSetAssetByPathBehaviourModel::GetBehaviourDetailsWidget()
{
	return SNew(SRCBehaviourSetAssetByPath, SharedThis(this));
}

TSharedRef<SWidget> FRCSetAssetByPathBehaviourModel::GetPropertyWidget()
{
	TSharedRef<SVerticalBox> FieldWidget = SNew(SVerticalBox);
	
	const TSharedPtr<IDetailTreeNode> PinnedNodeDefault = DetailTreeNodeWeakPtr[0];

	const FNodeWidgets NodeWidgetsDefault = PinnedNodeDefault->CreateNodeWidgets();
	if (NodeWidgetsDefault.ValueWidget)
	{
		FieldWidget->AddSlot()
			.Padding(FMargin(3.0f, 2.0f))
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SNew(STextBlock)
						.Text(FText::FromString("Target Exposed Property"))
				]
				+ SHorizontalBox::Slot()
				[
					NodeWidgetsDefault.NameWidget.ToSharedRef()
				]
			];
		
		FieldWidget->AddSlot()
			.Padding(FMargin(3.0f, 2.0f))
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				[
					SelectorBox.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				[
					NodeWidgetsDefault.ValueWidget.ToSharedRef()
				]
			];
	}
	else if (NodeWidgetsDefault.WholeRowWidget)
	{
		FieldWidget->AddSlot()
			.Padding(FMargin(3.0f, 2.0f))
			.AutoHeight()
			[
				NodeWidgetsDefault.WholeRowWidget.ToSharedRef()
			];
	}

	FieldWidget->AddSlot()
		.Padding(FMargin(3.f, 3.f))
		.AutoHeight()
		[
			SNew(SBox)
			.MaxDesiredHeight(150.f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					PathArrayWidget->AsShared()
				]
			]
		];

	FieldWidget->AddSlot()
		.Padding(FMargin(3.f, 3.f))
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString("Preview Path: "))
				]
			+ SHorizontalBox::Slot()
				[
					PreviewPathWidget->AsShared()
				]
		];

	
	return 	SNew(SHorizontalBox)
		.Clipping(EWidgetClipping::OnDemand)
		+ SHorizontalBox::Slot()
		.VAlign(VAlign_Center)
		.AutoWidth()
		.FillWidth(1.0f)
		[
			FieldWidget
		];
}

void FRCSetAssetByPathBehaviourModel::RegenerateWeakPtrInternal()
{
	DetailTreeNodeWeakPtrArray.Empty();
	for (const TSharedRef<IDetailTreeNode>& CategoryNode : PropertyRowGeneratorArray->GetRootTreeNodes())
	{
		TArray<TSharedRef<IDetailTreeNode>> Children;
		CategoryNode->GetChildren(Children);
		for (const TSharedRef<IDetailTreeNode>& Child : Children)
		{
			DetailTreeNodeWeakPtrArray.Add(Child);
		}
	}

	URCSetAssetByPathBehaviour* SetAssetByPathBehaviour = SetAssetByPathBehaviourWeakPtr.Get();
	if (!SetAssetByPathBehaviour)
	{
		return;
	}

	URCController* RCController = SetAssetByPathBehaviour->ControllerWeakPtr.Get();
	if (!RCController)
	{
		return;
	}

	URemoteControlPreset* RemoteControlPreset =  RCController->PresetWeakPtr.Get();
	if (!RemoteControlPreset)
	{
		return;
	}

	ExposedEntities = RemoteControlPreset->GetExposedEntities<const FRemoteControlEntity>();
}

void FRCSetAssetByPathBehaviourModel::RefreshPathAndPreview()
{
	const TSharedPtr<SVerticalBox> FieldArrayWidget = SNew(SVerticalBox);

	RegenerateWeakPtrInternal();
	if (URCSetAssetByPathBehaviour* Behaviour = Cast<URCSetAssetByPathBehaviour>(GetBehaviour()))
	{
		Behaviour->RefreshPathArray();
	}
	
	for (const TSharedPtr<IDetailTreeNode>& DetailTreeNodeArray : DetailTreeNodeWeakPtrArray)
	{
		const TSharedPtr<IDetailTreeNode> PinnedNode = DetailTreeNodeArray;

		TArray<TSharedRef<IDetailTreeNode>> Children;
		PinnedNode->GetChildren(Children);
		Children.Insert(PinnedNode.ToSharedRef(), 0);
		
		for (uint8 Counter = 0; Counter < Children.Num(); Counter++)
		{
			TSharedPtr<SWidget> Button = SNullWidget::NullWidget;
			if (Counter == 0)
			{
				Button = SNew(SBox)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.WidthOverride(22)
					.HeightOverride(22)
					[
						SNew(SButton)
						.ButtonStyle( FAppStyle::Get(), "SimpleButton" )
						.OnClicked_Lambda([this]()
						{
							if (URCSetAssetByPathBehaviour* Behaviour = Cast<URCSetAssetByPathBehaviour>(GetBehaviour()))
							{
								Behaviour->Modify();
								CreateAssetPathFromSelection();
								RefreshPathAndPreview();
							}

							return FReply::Handled();
						})
						.IsFocusable(false)
						.ContentPadding(0)
						.Visibility_Lambda([this]()
						{
							URCSetAssetByPathBehaviour* Behaviour = Cast<URCSetAssetByPathBehaviour>(GetBehaviour());
							if (!Behaviour)
							{
								return EVisibility::Collapsed;
							}

							return Behaviour->bInternal ? EVisibility::Visible : EVisibility::Collapsed;
						})
						.Content()
						[
							SNew(SImage)
							.Image( FAppStyle::GetBrush("Icons.Use") )
							.ColorAndOpacity( FSlateColor::UseForeground() )
						]
					];
			}
			
			const FNodeWidgets NodeWidgets = Children[Counter]->CreateNodeWidgets();
			if (NodeWidgets.ValueWidget)
			{
				FieldArrayWidget->AddSlot()
					.Padding(FMargin(3.0f, 2.0f))
					.AutoHeight()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						[
							NodeWidgets.NameWidget.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						[
							NodeWidgets.ValueWidget.ToSharedRef()
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							Button.ToSharedRef()
						]
					];
			}
			else if (NodeWidgets.WholeRowWidget)
			{
				FieldArrayWidget->AddSlot()
					.Padding(FMargin(3.0f, 2.0f))
					.AutoHeight()
					[
						NodeWidgets.WholeRowWidget.ToSharedRef()
					];
			}
		}
	}

	FString PreviewText = TEXT("Path is invalid!");
	if (URCSetAssetByPathBehaviour* SetAssetByPathBehaviour = SetAssetByPathBehaviourWeakPtr.Get())
	{
		PreviewText = SetAssetByPathBehaviour->GetCurrentPath();
	}
	
	PathArrayWidget->SetContent(FieldArrayWidget.ToSharedRef());
	PreviewPathWidget->SetText(FText::FromString(PreviewText));
}

TSharedRef<SWidget> FRCSetAssetByPathBehaviourModel::GetSelectorWidget(TWeakPtr<const FRemoteControlEntity> InInitialSelected)
{
	return SNew(SComboBox<TWeakPtr<const FRemoteControlEntity>>)
		.OptionsSource(&ExposedEntities)
		.OnGenerateWidget_Lambda([this](TWeakPtr<const FRemoteControlEntity> InItem) -> TSharedRef<SWidget>
		{
			TSharedPtr<const FRemoteControlEntity> PinnedItem = InItem.Pin();
			if (!PinnedItem)
			{
				return SNullWidget::NullWidget;
			}

			return SNew(STextBlock).Text(FText::FromString(*PinnedItem->GetLabel().ToString()));
		})
		.OnComboBoxOpening_Lambda([this]()
		{
			const URCSetAssetByPathBehaviour* PathBehaviour = Cast<URCSetAssetByPathBehaviour>(GetBehaviour());
			if (!PathBehaviour)
			{
				return;
			}
			
			const URCController* Controller = PathBehaviour->ControllerWeakPtr.Get();
			if (!Controller)
			{
				return;
			}

			const URemoteControlPreset* Preset = Controller->PresetWeakPtr.Get();
			if (!Preset)
			{
				return;
			}
			
			ExposedEntities = Preset->GetExposedEntities<const FRemoteControlEntity>();
		})
		.OnSelectionChanged_Lambda([this](TWeakPtr<const FRemoteControlEntity> InItem, ESelectInfo::Type SelectType)
		{
			if (TSharedPtr<const FRemoteControlEntity> Entity = InItem.Pin())
			{
				if (URCSetAssetByPathBehaviour* PathBehaviour = Cast<URCSetAssetByPathBehaviour>(GetBehaviour()))
				{
					PathBehaviour->SetTargetEntity(Entity);
				}
			}
		})
		.InitiallySelectedItem(InInitialSelected)
		.Content()
		[
			SNew(STextBlock)
				.Text_Raw(this, &FRCSetAssetByPathBehaviourModel::GetSelectedEntityText)
		];
}

FText FRCSetAssetByPathBehaviourModel::GetSelectedEntityText() const
{
	FText ReturnText = FText::FromString(*FString("Nothing selected yet"));

	if (URCSetAssetByPathBehaviour* Behaviour = SetAssetByPathBehaviourWeakPtr.Get())
	{
		if (Behaviour->GetTargetEntity().Pin())
		{
			ReturnText = FText::FromString(*Behaviour->GetTargetEntity().Pin()->GetLabel().ToString());
		}
	}
	return ReturnText;
}


void FRCSetAssetByPathBehaviourModel::CreateAssetPathFromSelection() const
{
	URCSetAssetByPathBehaviour* SetAssetByPathBehaviour = Cast<URCSetAssetByPathBehaviour>(GetBehaviour());
	if (!SetAssetByPathBehaviour)
	{
		return;
	}
	
	TArray<FAssetData> AssetData;
	GEditor->GetContentBrowserSelections(AssetData);
	if (AssetData.Num() != 1)
	{
		// Only works with one selected Content
		return;
	}
	
	// Clear it, in case it is an already used one.
	TArray<FString>& PathArray = SetAssetByPathBehaviour->PathStruct.PathArray;
	PathArray.Empty();

	int32 IndexOfLast;
	const UObject* SelectedAsset = AssetData[0].GetAsset();
	FString PathString = SelectedAsset->GetPathName();

	// Remove the initial Game
	PathString.RemoveFromStart("/Game/");

	// Remove anything after the last /
	PathString.FindLastChar('/', IndexOfLast);
	PathString.RemoveAt(IndexOfLast, PathString.Len() - IndexOfLast);

	PathArray.Add(PathString);
}


#undef LOCTEXT_NAMESPACE
