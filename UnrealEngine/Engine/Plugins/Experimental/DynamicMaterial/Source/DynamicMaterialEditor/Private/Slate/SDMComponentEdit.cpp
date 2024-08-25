// Copyright Epic Games, Inc. All Rights Reserved.

#include "Slate/SDMComponentEdit.h"
#include "Components/DMMaterialEffect.h"
#include "Components/DMMaterialEffectStack.h"
#include "Components/DMMaterialLayer.h"
#include "Components/DMMaterialProperty.h"
#include "Components/DMMaterialSlot.h"
#include "Components/DMMaterialStage.h"
#include "Components/DMMaterialStageInput.h"
#include "Components/DMMaterialStageSource.h"
#include "Components/DMMaterialStageThroughput.h"
#include "Components/DMMaterialValue.h"
#include "Components/DMTextureUV.h"
#include "CustomDetailsViewArgs.h"
#include "CustomDetailsViewModule.h"
#include "CustomDetailsViewSequencer.h"
#include "DetailLayoutBuilder.h"
#include "DMEDefs.h"
#include "DMWorldSubsystem.h"
#include "DynamicMaterialEditorModule.h"
#include "DynamicMaterialEditorStyle.h"
#include "DynamicMaterialModule.h"
#include "Engine/World.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ICustomDetailsView.h"
#include "IDetailKeyframeHandler.h"
#include "IDetailTreeNode.h"
#include "ISinglePropertyView.h"
#include "Items/CustomDetailsViewItemId.h"
#include "Items/ICustomDetailsViewCustomItem.h"
#include "Items/ICustomDetailsViewItem.h"
#include "Menus/DMMaterialStageSourceMenus.h"
#include "Misc/CoreDelegates.h"
#include "Model/DynamicMaterialModelEditorOnlyData.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "Slate/Properties/Editors/SDMPropertyEditBool.h"
#include "Slate/Properties/Editors/SDMPropertyEditColor.h"
#include "Slate/Properties/Editors/SDMPropertyEditEnum.h"
#include "Slate/Properties/Editors/SDMPropertyEditFloat.h"
#include "Slate/Properties/Editors/SDMPropertyEditObject.h"
#include "Slate/Properties/Editors/SDMPropertyEditVector.h"
#include "Slate/SDMSlot.h"
#include "Slate/SDMStage.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateTypes.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Text/STextBlock.h"

static const float RowPadding = 2.0f;

#define LOCTEXT_NAMESPACE "SDMComponentEdit"

ECheckBoxState SDMComponentEdit::IsInputPerChannelMapped(UDMMaterialStageThroughput* InThroughput, int32 InInputIdx)
{
	// TODO Implement per channel mapping
	return ECheckBoxState::Unchecked;
}

FText SDMComponentEdit::GetInputChannelMapDescription(UDMMaterialStageThroughput* InThroughput, int32 InInputIdx, int32 InChannelIdx)
{
	if (!IsValid(InThroughput) || !InThroughput->IsComponentValid())
	{
		return FText::GetEmpty();
	}
	
	UDMMaterialStage* Stage = InThroughput->GetStage();
	
	if (!IsValid(Stage) || !Stage->IsComponentValid())
	{
		return FText::GetEmpty();
	}

	const TArray<FDMMaterialStageConnection>& InputConnectionMap = Stage->GetInputConnectionMap();

	if (!InputConnectionMap.IsValidIndex(InInputIdx))
	{
		return FText::GetEmpty();
	}

	const FDMMaterialStageConnection& Connection = InputConnectionMap[InInputIdx];

	if (!Connection.Channels.IsValidIndex(InChannelIdx))
	{
		return FText::GetEmpty();
	}

	FText SourceName = LOCTEXT("?", "?");
	EDMValueType OutputType = EDMValueType::VT_None;

	const FDMMaterialStageConnectorChannel& Channel = Connection.Channels[InChannelIdx];

	if (Channel.SourceIndex == FDMMaterialStageConnectorChannel::PREVIOUS_STAGE)
	{
		if (Stage)
		{
			UDMMaterialLayerObject* Layer = Stage->GetLayer();

			if (!IsValid(Layer))
			{
				return FText::GetEmpty();
			}

			UDMMaterialSlot* Slot = Layer->GetSlot();

			if (!IsValid(Slot) || !Slot->IsComponentValid())
			{
				return FText::GetEmpty();
			}

			UDynamicMaterialModelEditorOnlyData* ModelEditorOnlyData = Slot->GetMaterialModelEditorOnlyData();

			if (!IsValid(ModelEditorOnlyData))
			{
				return FText::GetEmpty();
			}

			UDMMaterialProperty* PropertyObj = ModelEditorOnlyData->GetMaterialProperty(Channel.MaterialProperty);

			if (!IsValid(PropertyObj) || !PropertyObj->IsComponentValid())
			{
				return FText::GetEmpty();
			}

			SourceName = FText::Format(
				LOCTEXT("PrevStageFormat", "Prev Stage {0}"),
				PropertyObj->GetDescription()
			);

			if (const UDMMaterialLayerObject* PreviousStage = Layer->GetPreviousLayer(Channel.MaterialProperty, EDMMaterialLayerStage::Base))
			{
				if (UDMMaterialStage* PreviousMask = PreviousStage->GetStage(EDMMaterialLayerStage::Mask))
				{
					if (UDMMaterialStageSource* PreviousStageSource = PreviousMask->GetSource())
					{
						const TArray<FDMMaterialStageConnector>& PreviousStageOutputs = PreviousStageSource->GetOutputConnectors();

						if (PreviousStageOutputs.IsValidIndex(Channel.OutputIndex))
						{
							static const FText StageOutputFormat = LOCTEXT("StageOutputFormat", "{0}: {1}");

							SourceName = FText::Format(
								StageOutputFormat,
								SourceName,
								PreviousStageOutputs[Channel.OutputIndex].Name
							);

							OutputType = PreviousStageOutputs[Channel.OutputIndex].Type;
						}
					}
				}
			}
		}
	}
	else
	{
		const TArray<UDMMaterialStageInput*>& StageInputs = Stage->GetInputs();
		const int32 StageInputIdx = Channel.SourceIndex - FDMMaterialStageConnectorChannel::FIRST_STAGE_INPUT;

		if (StageInputs.IsValidIndex(StageInputIdx))
		{
			SourceName = StageInputs[StageInputIdx]->GetChannelDescription(Channel);

			const TArray<FDMMaterialStageConnector>& StageInputOutputConnectors = StageInputs[StageInputIdx]->GetOutputConnectors();

			if (StageInputOutputConnectors.IsValidIndex(Channel.OutputIndex))
			{
				if (StageInputOutputConnectors.Num() > 1)
				{
					static const FText InputOutputFormat = LOCTEXT("InputOutputFormat", "{0}: {1}");

					SourceName = FText::Format(
						InputOutputFormat,
						SourceName,
						StageInputOutputConnectors[Channel.OutputIndex].Name
					);
				}

				OutputType = StageInputOutputConnectors[Channel.OutputIndex].Type;
			}
		}
	}

	if (Channel.OutputChannel == FDMMaterialStageConnectorChannel::WHOLE_CHANNEL || OutputType == EDMValueType::VT_None)
	{
		static const FText WholeChannelFormat = LOCTEXT("WholeChannelFormat", "{0}");
		return FText::Format(WholeChannelFormat, SourceName);
	}

	// Assume RGBA.
	TArray<FText> Channels;

	if (Channel.OutputChannel & FDMMaterialStageConnectorChannel::FIRST_CHANNEL)
	{
		Channels.Add(UDMValueDefinitionLibrary::GetValueDefinition(OutputType).GetChannelName(1));
	}

	if (Channel.OutputChannel & FDMMaterialStageConnectorChannel::SECOND_CHANNEL)
	{
		Channels.Add(UDMValueDefinitionLibrary::GetValueDefinition(OutputType).GetChannelName(2));
	}

	if (Channel.OutputChannel & FDMMaterialStageConnectorChannel::THIRD_CHANNEL)
	{
		Channels.Add(UDMValueDefinitionLibrary::GetValueDefinition(OutputType).GetChannelName(3));
	}

	if (Channel.OutputChannel & FDMMaterialStageConnectorChannel::FOURTH_CHANNEL)
	{
		Channels.Add(UDMValueDefinitionLibrary::GetValueDefinition(OutputType).GetChannelName(4));
	}

	static const FText Separator = LOCTEXT("ChannelSeparator", ",");
	static const FText MaskedChannelFormat = LOCTEXT("MaskedChannelFormat", "{0}: {1}");
	const FText ChannelName = FText::Join(Separator, Channels);

	return FText::Format(MaskedChannelFormat, SourceName, ChannelName);
}

SDMComponentEdit::~SDMComponentEdit()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	SDMEditor::ClearPropertyHandles(this);

	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ComponentWeak.Get()))
	{
		Stage->GetOnUpdate().RemoveAll(this);
	}
	else if (UDMMaterialEffect* Effect = Cast<UDMMaterialEffect>(ComponentWeak.Get()))
	{
		Effect->GetOnUpdate().RemoveAll(this);
	}
}

void SDMComponentEdit::Construct(const FArguments& InArgs, UDMMaterialComponent* InComponent, const TWeakPtr<SDMSlot>& InSlotWidget)
{
	ComponentWeak = InComponent;
	SlotWidgetWeak = InSlotWidget;

	KeyframeHandler = nullptr;

	if (ensure(IsValid(InComponent)))
	{
		if (const UWorld* const World = InComponent->GetWorld())
		{
			if (const UDMWorldSubsystem* const WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
			{
				KeyframeHandler = WorldSubsystem->GetKeyframeHandler();
			}
		}

		ChildSlot
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			[
				SAssignNew(Container, SBox)
				[
					CreateEditWidget()
				]
			];
	}
}

TSharedRef<SWidget> SDMComponentEdit::CreateEditWidget()
{
	SDMEditor::ClearPropertyHandles(this);

	FCustomDetailsViewArgs Args;
	Args.KeyframeHandler = KeyframeHandler;
	Args.bAllowGlobalExtensions = true;
	Args.bAllowResetToDefault = true;
	Args.bShowCategories = false;

	TSharedRef<ICustomDetailsView> DetailsView = ICustomDetailsViewModule::Get().CreateCustomDetailsView(Args);
	FCustomDetailsViewItemId RootId = DetailsView->GetRootItem()->GetItemId();

	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ComponentWeak.Get()))
	{
		TSharedPtr<ICustomDetailsViewCustomItem> TypeSelectorItem = DetailsView->CreateCustomItem(
			FName(TEXT("SamplerType")),
			LOCTEXT("SamplerType", "Type"),
			LOCTEXT("SamplerTypeTooltip", "Source Function")
		);

		if (TypeSelectorItem.IsValid())
		{
			TypeSelectorItem->SetValueWidget(CreateSourceTypeEditWidget());

			DetailsView->ExtendTree(
				RootId,
				ECustomDetailsTreeInsertPosition::FirstChild,
				TypeSelectorItem->AsItem()
			);
		}
	}

	TArray<FDMPropertyHandle> EditRows = GetEditRows();

	for (const FDMPropertyHandle& EditRow : EditRows)
	{
		ECustomDetailsTreeInsertPosition Position = ECustomDetailsTreeInsertPosition::Child;

		if (EditRow.DetailTreeNode->CreatePropertyHandle()->HasMetaData("HighPriority"))
		{
			Position = ECustomDetailsTreeInsertPosition::FirstChild;
		}
		else if (EditRow.DetailTreeNode->CreatePropertyHandle()->HasMetaData("LowPriority"))
		{
			Position = ECustomDetailsTreeInsertPosition::LastChild;
		}

		TSharedRef<ICustomDetailsViewItem> Item = DetailsView->CreateDetailTreeItem(EditRow.DetailTreeNode.ToSharedRef());

		if (EditRow.NameOverride.IsSet())
		{
			Item->SetOverrideWidget(
				ECustomDetailsViewWidgetType::Name,
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(EditRow.NameOverride.GetValue())
				.ToolTipText(EditRow.NameToolTipOverride.Get(FText::GetEmpty()))
			);
		}

		if (EditRow.DetailTreeNode->CreatePropertyHandle()->HasMetaData("NotKeyframeable"))
		{
			Item->SetKeyframeEnabled(false);
		}

		if (EditRow.ResetToDefaultOverride.IsSet())
		{
			Item->SetResetToDefaultOverride(EditRow.ResetToDefaultOverride.GetValue());
		}

		DetailsView->ExtendTree(RootId, Position, Item);
	}

	DetailsView->RebuildTree(ECustomDetailsViewBuildType::InstantBuild);

	return DetailsView;
}

TArray<FDMPropertyHandle> SDMComponentEdit::GetEditRows()
{
	TArray<FDMPropertyHandle> PropertyRows;
	TSet<UDMMaterialComponent*> ProcessedObjects;

	FDynamicMaterialEditorModule::GeneratorComponentPropertyRows(SharedThis(this), ComponentWeak.Get(), PropertyRows, ProcessedObjects);

	return PropertyRows;
}

TSharedRef<SWidget> SDMComponentEdit::CreateExtensionButtons(const TSharedPtr<SDMComponentEdit>& InComponentEditWidget,
	UDMMaterialComponent* InComponent, const FName& InPropertyName, bool bInAllowKeyframe)
{
	return CreateExtensionButtons(
		InComponentEditWidget, 
		InComponent, 
		InPropertyName, 
		bInAllowKeyframe,
		FSimpleDelegate::CreateLambda([ComponentEditWidgetWeak = InComponentEditWidget.ToWeakPtr()]()
			{
				if (TSharedPtr<SDMComponentEdit> ComponentEditWidget = ComponentEditWidgetWeak.Pin())
				{
					if (TSharedPtr<SDMSlot> SlotWidget = ComponentEditWidget->GetSlotWidget())
					{
						SlotWidget->InvalidateComponentEditWidget();
					}
				}
			})
	);
}

TSharedRef<SWidget> SDMComponentEdit::CreateExtensionButtons(const TSharedPtr<SWidget>& InPropertyOwner, UDMMaterialComponent* InComponent,
	const FName& InPropertyName, bool bInAllowKeyframe, FSimpleDelegate InOnResetDelegate)
{
	if (!IsValid(InComponent))
	{
		return SNew(SBox)
			.WidthOverride(42.f)
			.HeightOverride(18.f);
	}

	FProperty* Property = InComponent->GetClass()->FindPropertyByName(InPropertyName);

	if (!Property)
	{
		return SNew(SBox)
			.WidthOverride(42.f)
			.HeightOverride(18.f);
	}

	TSharedPtr<IPropertyHandle> PropertyHandle = nullptr;
	TSharedPtr<IDetailTreeNode> DetailTreeNode = nullptr;
	FString PropertyName = InPropertyName.ToString();

	if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(InComponent))
	{
		PropertyHandle = TextureUV->GetPropertyHandle(InPropertyName);
		DetailTreeNode = TextureUV->GetDetailTreeNode(InPropertyName);
		PropertyName = FString("TextureUV->") + PropertyName;
	}
	else if (UDMMaterialValue* Value = Cast<UDMMaterialValue>(InComponent))
	{
		PropertyHandle = Value->GetPropertyHandle();
		DetailTreeNode = Value->GetDetailTreeNode();
	}
	else
	{
		FDMPropertyHandle DMPropertyHandle = SDMEditor::GetPropertyHandle(InPropertyOwner.Get(), InComponent, InPropertyName);
		PropertyHandle = DMPropertyHandle.PropertyHandle;
		DetailTreeNode = DMPropertyHandle.DetailTreeNode;
	}

	if (PropertyHandle.IsValid())
	{
		ensure(PropertyHandle->GetProperty() == Property);
	}

	FOnGenerateGlobalRowExtensionArgs ExtensionRowArgs;
	ExtensionRowArgs.OwnerObject = InComponent;
	ExtensionRowArgs.Property = Property;
	ExtensionRowArgs.PropertyPath = PropertyName;
	ExtensionRowArgs.OwnerTreeNode = DetailTreeNode;
	ExtensionRowArgs.PropertyHandle = PropertyHandle;

	FSlimHorizontalToolBarBuilder ToolBarBuilder(TSharedPtr<FUICommandList>(), FMultiBoxCustomization::None);

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	TArray<FPropertyRowExtensionButton> ExtensionButtons;
	PropertyEditorModule.GetGlobalRowExtensionDelegate().Broadcast(ExtensionRowArgs, ExtensionButtons);

	TSharedRef<SWidget> ResetButtonWidget = PropertyCustomizationHelpers::MakeResetButton(
		FSimpleDelegate::CreateLambda([PropertyHandle, InOnResetDelegate]()
			{
				if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
				{
					TArray<UObject*> Outers;
					PropertyHandle->GetOuterObjects(Outers);

					if (Outers.IsEmpty() == false && IsValid(Outers[0]))
					{
						FScopedTransaction Transaction(LOCTEXT("ResetValue", "Reset Value to default."));

						if (FProperty* Property = PropertyHandle->GetProperty())
						{
							Outers[0]->Modify();

							if (UDMMaterialValue* Value = Cast<UDMMaterialValue>(Outers[0]))
							{
								Value->ApplyDefaultValue();
							}
							else
							{
								void* DefaultValue = Property->ContainerPtrToValuePtr<void>(Outers[0]->GetClass()->GetDefaultObject(true));

								if (DefaultValue != nullptr)
								{
									if (Property->HasSetter())
									{
										Property->CallSetter(Outers[0], DefaultValue);
									}
									else
									{
										Outers[0]->PreEditChange(Property);

										Property->SetValue_InContainer(Outers[0], DefaultValue);

										FPropertyChangedEvent PCE = FPropertyChangedEvent(Property, EPropertyChangeType::ValueSet);
										Outers[0]->PostEditChangeProperty(PCE);
									}
								}
							}
						}
						else
						{
							PropertyHandle->ResetToDefault();
						}

						InOnResetDelegate.ExecuteIfBound();
					}
				}
			})
	);
	ResetButtonWidget->SetVisibility(
		TAttribute<EVisibility>::CreateLambda([PropertyHandle]()->EVisibility
			{
				if (PropertyHandle.IsValid() && PropertyHandle->IsValidHandle())
				{
					if (FProperty* Property = PropertyHandle->GetProperty())
					{
						TArray<UObject*> Outers;
						PropertyHandle->GetOuterObjects(Outers);

						if (Outers.IsEmpty() == false && IsValid(Outers[0]))
						{
							if (UDMMaterialValue* Value = Cast<UDMMaterialValue>(Outers[0]))
							{
								return Value->IsDefaultValue()
									? EVisibility::Hidden
									: EVisibility::Visible;
							}

							return Property->Identical_InContainer(Outers[0], Outers[0]->GetClass()->GetDefaultObject(true))
								? EVisibility::Hidden
								: EVisibility::Visible;
						}
					}

					return PropertyHandle->CanResetToDefault() ? EVisibility::Visible : EVisibility::Hidden;
				}

				return EVisibility::Hidden;
			})
	);

	ToolBarBuilder.AddWidget(ResetButtonWidget);

	bool bAddedSequencerButtons = false;

	// Sequencer relies on getting the Keyframe Handler via the Details View of the IDetailTreeNode, but it's null since
	// there's no Details View here. Instead add it manually.
	if (bInAllowKeyframe && PropertyHandle.IsValid())
	{
		if (UWorld* World = InComponent->GetWorld())
		{
			if (UDMWorldSubsystem* DMWorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>())
			{
				if (const TSharedPtr<IDetailKeyframeHandler>& KeyframeHandler = DMWorldSubsystem->GetKeyframeHandler())
				{
					TArray<FPropertyRowExtensionButton> SequencerButtons;

					FCustomDetailsViewSequencerUtils::CreateSequencerExtensionButton(
						KeyframeHandler,
						PropertyHandle,
						SequencerButtons
					);

					for (const FPropertyRowExtensionButton& SequencerButton : SequencerButtons)
					{
						ToolBarBuilder.AddToolBarButton(
							SequencerButton.UIAction,
							NAME_None,
							TAttribute<FText>(),
							SequencerButton.ToolTip,
							SequencerButton.Icon
						);

						bAddedSequencerButtons = true;
					}
				}
			}
		}
	}

	if (!bAddedSequencerButtons)
	{
		// Maintain space
		ToolBarBuilder.AddWidget(
			SNew(SBox)
			.WidthOverride(20.f)
			.HeightOverride(18.f)
		);
	}

	if (!ExtensionButtons.IsEmpty())
	{
		// Build extension toolbar 
		ToolBarBuilder.SetLabelVisibility(EVisibility::Collapsed);
		ToolBarBuilder.SetStyle(&FAppStyle::Get(), "DetailsView.ExtensionToolBar");

		for (const FPropertyRowExtensionButton& Extension : ExtensionButtons)
		{
			ToolBarBuilder.AddToolBarButton(Extension.UIAction, NAME_None, Extension.Label, Extension.ToolTip, Extension.Icon);
		}
	}

	return ToolBarBuilder.MakeWidget();
}

TSharedPtr<SWidget> SDMComponentEdit::CreateSinglePropertyEditWidget(UDMMaterialComponent* InComponent, const FName& InPropertyName)
{
	if (!IsValid(InComponent))
	{
		return nullptr;
	}

	FProperty* ClassProperty = InComponent->GetClass()->FindPropertyByName(InPropertyName);

	if (!ClassProperty)
	{
		return nullptr;
	}

	FDMPropertyHandle PropertyHandle = SDMEditor::GetPropertyHandle(this, InComponent, InPropertyName);

	if (PropertyHandle.PropertyHandle.IsValid())
	{
		if (ClassProperty->IsA<FBoolProperty>())
		{
			return SNew(SDMPropertyEditBool, PropertyHandle.PropertyHandle)
				.ComponentEditWidget(SharedThis(this));
		}
		else if (ClassProperty->IsA<FEnumProperty>())
		{
			return SNew(SDMPropertyEditEnum, PropertyHandle.PropertyHandle)
				.ComponentEditWidget(SharedThis(this));
		}
		else if (ClassProperty->IsA<FFloatProperty>())
		{
			return SNew(SDMPropertyEditFloat, PropertyHandle.PropertyHandle)
				.ComponentEditWidget(SharedThis(this));
		}
		else if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(ClassProperty))
		{
			return SNew(SDMPropertyEditObject, PropertyHandle.PropertyHandle, ObjectProperty->PropertyClass)
				.ComponentEditWidget(SharedThis(this));
		}
		else if (FStructProperty* StructProperty = CastField<FStructProperty>(ClassProperty))
		{
			if (StructProperty->Struct == TBaseStructure<FVector2D>::Get())
			{
				return SNew(SDMPropertyEditVector, PropertyHandle.PropertyHandle, 2)
					.ComponentEditWidget(SharedThis(this));
			}
			else if (StructProperty->Struct == TBaseStructure<FVector>::Get()
				|| StructProperty->Struct == TBaseStructure<FRotator>::Get())
			{
				return SNew(SDMPropertyEditVector, PropertyHandle.PropertyHandle, 3)
					.ComponentEditWidget(SharedThis(this));
			}
			else if (StructProperty->Struct == TBaseStructure<FLinearColor>::Get())
			{
				return SNew(SDMPropertyEditColor, PropertyHandle.PropertyHandle)
					.ComponentEditWidget(SharedThis(this));
			}
		}

		return nullptr;
	}

	// This is a complete fallback that does not layout correctly.
	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));

	FSinglePropertyParams InitParams;
	InitParams.NamePlacement = EPropertyNamePlacement::Hidden;
	InitParams.NotifyHook = InComponent;

	return PropertyEditor.CreateSingleProperty(InComponent, InPropertyName, InitParams);
}

TSharedRef<SWidget> SDMComponentEdit::MakeSourceTypeEditWidgetMenuContent()
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ComponentWeak.Get()))
	{
		if (TSharedPtr<SDMSlot> SlotWidget = GetSlotWidget())
		{
			if (TSharedPtr<SDMStage> StageWidget = SlotWidget->FindStageWidget(Stage))
			{
				return FDMMaterialStageSourceMenus::MakeChangeSourceMenu(SlotWidget, StageWidget);
			}
		}
	}

	return SNullWidget::NullWidget;
}

FText SDMComponentEdit::GetSourceTypeEditWidgetText() const
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ComponentWeak.Get()))
	{
		if (UDMMaterialStageSource* Source = Stage->GetSource())
		{
			return Source->GetStageDescription();
		}
	}

	return FText::GetEmpty();
}

void SDMComponentEdit::OnUndo()
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(ComponentWeak.Get()))
	{
		if (const UDMMaterialLayerObject* Layer = Stage->GetLayer())
		{
			if (Layer->GetStageType(Stage) == EDMMaterialLayerStage::Mask)
			{
				if (bCreatedWithLinkedUVs != Layer->IsTextureUVLinkEnabled())
				{
					if (TSharedPtr<SDMSlot> SlotWidget = SlotWidgetWeak.Pin())
					{
						SlotWidget->InvalidateComponentEditWidget();
					}
				}
			}
		}
	}
}

TSharedRef<SWidget> SDMComponentEdit::CreateSourceTypeEditWidget()
{
	TWeakPtr<SDMComponentEdit> ThisWeak = SharedThis(this);

	return SNew(SBox)
		.HAlign(EHorizontalAlignment::HAlign_Left)
		.VAlign(EVerticalAlignment::VAlign_Center)
		.Padding(0.f, 0.f, 0.f, 0.2f)
		[
			SNew(SComboButton)
				.HasDownArrow(false)
				.IsFocusable(true)
				.ContentPadding(4.0f)
				.ButtonStyle(FDynamicMaterialEditorStyle::Get(), "HoverHintOnly.Bordered.Dark")
				.ToolTipText(LOCTEXT("ChangeLayer", "Change Stage Type"))
				.OnGetMenuContent(this, &SDMComponentEdit::MakeSourceTypeEditWidgetMenuContent)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &SDMComponentEdit::GetSourceTypeEditWidgetText)
				]
		];
}

void SDMComponentEdit::CreateKeyFrame(TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	TArray<UObject*> Outers;
	InPropertyHandle->GetOuterObjects(Outers);

	if (Outers.IsEmpty() || !IsValid(Outers[0]))
	{
		return;
	}

	UDMMaterialComponent* Component = Cast<UDMMaterialComponent>(Outers[0]);

	if (!IsValid(Component))
	{
		return;
	}

	if (!InPropertyHandle.IsValid())
	{
		return;
	}

	const UWorld* const World = Component->GetWorld();
	if (!World)
	{
		return;
	}

	const UDMWorldSubsystem* const WorldSubsystem = World->GetSubsystem<UDMWorldSubsystem>();
	if (!WorldSubsystem)
	{
		return;
	}

	TSharedPtr<IDetailKeyframeHandler> KeyframeHandler = WorldSubsystem->GetKeyframeHandler();
	if (!KeyframeHandler.IsValid())
	{
		return;
	}

	if (!KeyframeHandler->IsPropertyKeyable(Component->GetClass(), *InPropertyHandle))
	{
		return;
	}

	KeyframeHandler->OnKeyPropertyClicked(*InPropertyHandle);
}

#undef LOCTEXT_NAMESPACE
