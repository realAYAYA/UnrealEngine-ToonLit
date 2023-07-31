// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCPanelExposedField.h"

#include "Algo/Transform.h"
#include "EditorFontGlyphs.h"
#include "IDetailTreeNode.h"
#include "IRCProtocolBindingList.h"
#include "IRemoteControlProtocolModule.h"
#include "IRemoteControlProtocolWidgetsModule.h"
#include "IRemoteControlUIModule.h"
#include "Layout/Visibility.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RCPanelWidgetRegistry.h"
#include "RemoteControlField.h"
#include "RemoteControlPanelStyle.h"
#include "RemoteControlPreset.h"
#include "RemoteControlSettings.h"
#include "SRCPanelTreeNode.h"
#include "SResetToDefaultPropertyEditor.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/RemoteControlStyles.h"
#include "UObject/Object.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Masks/SRCProtocolMask.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SNullWidget.h"

#define LOCTEXT_NAMESPACE "RemoteControlPanel"

namespace ExposedFieldUtils
{
	TSharedRef<SWidget> CreateNodeValueWidget(const TSharedPtr<IDetailTreeNode>& Node)
	{
		FNodeWidgets NodeWidgets = Node->CreateNodeWidgets();

		TSharedRef<SHorizontalBox> FieldWidget = SNew(SHorizontalBox);

		if (NodeWidgets.ValueWidget)
		{
			FieldWidget->AddSlot()
				.Padding(FMargin(3.0f, 2.0f))
				.HAlign(HAlign_Left)
				[
					NodeWidgets.ValueWidget.ToSharedRef()
				];
		}
		else if (NodeWidgets.WholeRowWidget)
		{
			FieldWidget->AddSlot()
				.Padding(FMargin(3.0f, 2.0f))
				[
					NodeWidgets.WholeRowWidget.ToSharedRef()
				];
		}

		return FieldWidget;
	}
}

TSharedPtr<SRCPanelTreeNode> SRCPanelExposedField::MakeInstance(const FGenerateWidgetArgs& Args)
{
	return SNew(SRCPanelExposedField, StaticCastSharedPtr<FRemoteControlField>(Args.Entity), Args.ColumnSizeData, Args.WidgetRegistry).Preset(Args.Preset).LiveMode(Args.bIsInLiveMode).HighlightText(Args.HighlightText);
}

void SRCPanelExposedField::Construct(const FArguments& InArgs, TWeakPtr<FRemoteControlField> InField, FRCColumnSizeData InColumnSizeData, TWeakPtr<FRCPanelWidgetRegistry> InWidgetRegistry)
{
	WeakField = MoveTemp(InField);

	ColumnSizeData = MoveTemp(InColumnSizeData);
	WidgetRegistry = MoveTemp(InWidgetRegistry);
	
	HighlightText = InArgs._HighlightText;

	ResetButtonWidget = SNullWidget::NullWidget;

	if (TSharedPtr<FRemoteControlField> FieldPtr = WeakField.Pin())
	{
		Initialize(FieldPtr->GetId(), InArgs._Preset.Get(), InArgs._LiveMode);
	
		CachedLabel = FieldPtr->GetLabel();
		EntityId = FieldPtr->GetId();
		
		if (FieldPtr->FieldType == EExposedFieldType::Property)
		{
			ConstructPropertyWidget();
		}
		else
		{
			ConstructFunctionWidget();
		}
	}
}

void SRCPanelExposedField::GetNodeChildren(TArray<TSharedPtr<SRCPanelTreeNode>>& OutChildren) const
{
	OutChildren.Append(ChildWidgets);
}

SRCPanelTreeNode::ENodeType SRCPanelExposedField::GetRCType() const
{
	return SRCPanelTreeNode::Field;
}

bool SRCPanelExposedField::HasChildren() const
{
	return ChildWidgets.Num() > 0;
}

FName SRCPanelExposedField::GetFieldLabel() const
{
	return CachedLabel;
}

EExposedFieldType SRCPanelExposedField::GetFieldType() const
{
	if (TSharedPtr<FRemoteControlField> Field = WeakField.Pin())
	{
		return Field->FieldType;
	}
	
	return EExposedFieldType::Invalid;
}

void SRCPanelExposedField::SetIsHovered(bool bInIsHovered)
{
	bIsHovered = bInIsHovered;
}

TSharedRef<SWidget> SRCPanelExposedField::GetProtocolWidget(const FName ForColumnName, const FName InProtocolName)
{
	if (TSharedPtr<FRemoteControlField> RCField = WeakField.Pin())
	{
		if (const TSharedPtr<FRemoteControlProperty> RCProperty = StaticCastSharedPtr<FRemoteControlProperty>(RCField))
		{
			for (const FRemoteControlProtocolBinding& RCProtocolIter : RCProperty->ProtocolBindings)
			{
				if (RCProtocolIter.GetProtocolName() == InProtocolName)
				{
					if (TSharedPtr<TStructOnScope<FRemoteControlProtocolEntity>> RCProtocolEntityPtr = RCProtocolIter.GetRemoteControlProtocolEntityPtr())
					{
						if (ForColumnName == RemoteControlPresetColumns::BindingStatus)
						{
							TSharedPtr<SWidget> BindingStatusWidget = SNew(SButton)
								.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
								.ToolTipText(LOCTEXT("RecordingButtonToolTip", "Status of the protocol entity binding"))
								.ForegroundColor(FSlateColor::UseForeground())
								.OnClicked_Lambda([RCProtocolEntityPtr]()
									{
										const ERCBindingStatus BindingStatus = (*RCProtocolEntityPtr)->ToggleBindingStatus();

										IRemoteControlProtocolWidgetsModule& RCProtocolWidgetsModule = IRemoteControlProtocolWidgetsModule::Get();

										if (TSharedPtr<IRCProtocolBindingList> RCProtocolBindingList = RCProtocolWidgetsModule.GetProtocolBindingList())
										{
											if (BindingStatus == ERCBindingStatus::Awaiting)
											{
												RCProtocolBindingList->OnStartRecording(RCProtocolEntityPtr);
											}
											else if (BindingStatus == ERCBindingStatus::Bound)
											{
												RCProtocolBindingList->OnStopRecording(RCProtocolEntityPtr);
											}
											else
											{
												checkNoEntry();
											}
										}

										return FReply::Handled();
									}
								)
								.Content()
								[
									SNew(SImage)
									.ColorAndOpacity_Lambda([RCProtocolEntityPtr]()
										{
											const ERCBindingStatus ActiveBindingStatus = (*RCProtocolEntityPtr)->GetBindingStatus();

											switch (ActiveBindingStatus)
											{
												case ERCBindingStatus::Awaiting:
													return FLinearColor::Red;
												case ERCBindingStatus::Bound:
													return FLinearColor::Green;
												case ERCBindingStatus::Unassigned:
													return FLinearColor::Gray;
												default:
													checkNoEntry();
											}

											return FLinearColor::Black;
										}
									)
									.Image(FAppStyle::Get().GetBrush(TEXT("Icons.FilledCircle")))
								];

							return BindingStatusWidget.ToSharedRef();
						}
						else if (TSharedPtr<FRCPanelWidgetRegistry> Registry = WidgetRegistry.Pin())
						{
							if (TSharedPtr<IDetailTreeNode> Node = Registry->GetStructTreeNode(RCProtocolEntityPtr, (*RCProtocolEntityPtr)->GetPropertyName(ForColumnName).ToString(), ERCFindNodeMethod::Name))
							{
								FNodeWidgets NodeWidgets = Node->CreateNodeWidgets();

								if (NodeWidgets.ValueWidget)
								{
									return NodeWidgets.ValueWidget.ToSharedRef();
								}
							}
						}
					}
				}
			}
		}
	}
	
	return SRCPanelTreeNode::GetProtocolWidget(ForColumnName, InProtocolName);
}

const bool SRCPanelExposedField::HasProtocolExtension() const
{
	return GetFieldType() == EExposedFieldType::Property;
}

const bool SRCPanelExposedField::GetProtocolBindingsNum() const
{
	IRemoteControlProtocolModule& RCProtocolModule = IRemoteControlProtocolModule::Get();

	const TArray<FName>& AvailableProtocols = RCProtocolModule.GetProtocolNames();

	if (AvailableProtocols.Num() < 2 || AvailableProtocols.IsEmpty())
	{
		return false;
	}

	const TArray<FName>& SupportedProtocols = AvailableProtocols.FilterByPredicate([&](const FName& ThisProtocol) { return SupportsProtocol(ThisProtocol); });

	return SupportedProtocols.Num() >= 2;
}

const bool SRCPanelExposedField::SupportsProtocol(const FName& InProtocolName) const
{
	if (TSharedPtr<FRemoteControlField> RCField = WeakField.Pin())
	{
		if (const TSharedPtr<FRemoteControlProperty> RCProperty = StaticCastSharedPtr<FRemoteControlProperty>(RCField))
		{
			if (RCProperty->ProtocolBindings.Num())
			{
				for (TSet<FRemoteControlProtocolBinding>::TConstIterator RCProtocolIter(RCProperty->ProtocolBindings); RCProtocolIter; ++RCProtocolIter)
				{
					if (RCProtocolIter->GetProtocolName() == InProtocolName)
					{
						return true;
					}
				}
			}
		}
	}

	return false;
}

void SRCPanelExposedField::Refresh()
{
	if (TSharedPtr<FRemoteControlField> Field = WeakField.Pin())
	{
		CachedLabel = Field->GetLabel();
		
		if (Field->FieldType == EExposedFieldType::Property)
		{
			ConstructPropertyWidget();
		}
		else if (Field->FieldType == EExposedFieldType::Function)
		{
			ConstructFunctionWidget();
		}
	}
}

TSharedRef<SWidget> SRCPanelExposedField::GetWidget(const FName ForColumnName, const FName InActiveProtocol)
{
	if (HasProtocolExtension())
	{
		if (ForColumnName == RemoteControlPresetColumns::Mask)
		{
			return SNew(SRCProtocolMask, WeakField);
		}
		else if (ForColumnName == RemoteControlPresetColumns::Status)
		{
			return SNew(STextBlock)
				.Text(LOCTEXT("StatusText", "!"))
				.TextStyle(&RCPanelStyle->HeaderTextStyle)
				.ToolTipText_Lambda([this]() { return GetProtocolBindingsNum() ? LOCTEXT("StatusTooltip", "Entity is bound to two or more protocols.") : FText::GetEmpty(); })
				.Visibility_Lambda([this]() { return GetProtocolBindingsNum() ? EVisibility::Visible : EVisibility::Collapsed; });
		}
		else if (!DefaultColumns.Contains(ForColumnName))
		{
			return GetProtocolWidget(ForColumnName, InActiveProtocol);
		}
	}

	return SRCPanelTreeNode::GetWidget(ForColumnName, InActiveProtocol);
}

void SRCPanelExposedField::GetBoundObjects(TSet<UObject*>& OutBoundObjects) const
{
	if (TSharedPtr<FRemoteControlField> Field = WeakField.Pin())
	{
		OutBoundObjects.Append(Field->GetBoundObjects());
	}
}

TSharedRef<SWidget> SRCPanelExposedField::ConstructWidget()
{
	if (TSharedPtr<FRemoteControlField> Field = WeakField.Pin())
	{
		// For the moment, just use the first object.
		TArray<UObject*> Objects = Field->GetBoundObjects();
		if (GetFieldType() == EExposedFieldType::Property && Objects.Num() > 0)
		{
			if (TSharedPtr<FRCPanelWidgetRegistry> Registry = WidgetRegistry.Pin())
			{
				if (TSharedPtr<IDetailTreeNode> Node = Registry->GetObjectTreeNode(Objects[0], Field->FieldPathInfo.ToPathPropertyString(), ERCFindNodeMethod::Path))
				{
					TArray<TSharedRef<IDetailTreeNode>> ChildNodes;
					Node->GetChildren(ChildNodes);
					ChildWidgets.Reset(ChildNodes.Num());

					for (const TSharedRef<IDetailTreeNode>& ChildNode : ChildNodes)
					{
						ChildWidgets.Add(SNew(SRCPanelFieldChildNode, ChildNode, ColumnSizeData));
					}

					TSharedPtr<SHorizontalBox> FieldWidget = SNew(SHorizontalBox);
					FieldWidget->AddSlot()
					[
						ExposedFieldUtils::CreateNodeValueWidget(MoveTemp(Node))
					];

					TSharedPtr<IPropertyHandle> PropertyHandle = Node->CreatePropertyHandle();
					if (PropertyHandle && PropertyHandle->IsValidHandle() && (PropertyHandle->GetParentHandle()->GetPropertyDisplayName().ToString() == FString("Transform")
						|| PropertyHandle->GetOuterBaseClass() == UMaterialInstanceDynamic::StaticClass()))
					{
						// Set up a Zeroed DefaultValue, in case an ExposedEntity doesn't have a native Default. Needed for certain ResetToDefault cases.
						void* ValuePtr;
						if (PropertyHandle->GetValueData(ValuePtr) == FPropertyAccess::Result::Success)
						{
							DefaultValue.Reset(new uint8[PropertyHandle->GetProperty()->GetSize()]);
							PropertyHandle->GetProperty()->CopyCompleteValue(DefaultValue.Get(), ValuePtr);
							PropertyHandle->GetProperty()->ClearValue(DefaultValue.Get());
						}

						TWeakPtr<SRCPanelExposedField> WeakFieldPtr = SharedThis(this);
						auto IsVisible = [WeakFieldPtr, Node]()
						{
							TSharedPtr<SRCPanelExposedField> FieldPtr = WeakFieldPtr.Pin();
							TSharedPtr<IPropertyHandle> PropertyHandle = Node->CreatePropertyHandle();
							if (FieldPtr && PropertyHandle && PropertyHandle->IsValidHandle())
							{
								void* DataPtr;
								if (PropertyHandle->GetValueData(DataPtr) == FPropertyAccess::Result::Success)
								{
									FProperty* NodeProperty = PropertyHandle->GetProperty();
									bool bVisible = !NodeProperty->Identical(FieldPtr->DefaultValue.Get(), DataPtr);
									return bVisible ? EVisibility::Visible : EVisibility::Hidden;
								}
							}
							return EVisibility::Hidden;
						};
						
						ResetButtonWidget = SNew(SBox)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.Padding(2.f, 4.f)
							[
								SNew(SButton)
								.IsFocusable(false)
								.ButtonStyle(&RCPanelStyle->FlatButtonStyle)
								.ContentPadding(RCPanelStyle->PanelPadding)
								.Visibility_Lambda(IsVisible)
								.OnClicked_Lambda([Node]()
								{
									Node->CreatePropertyHandle()->ResetToDefault();
									return FReply::Handled();
								})
								.Content()
								[
									SNew(SImage)
									.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
									.ColorAndOpacity(FSlateColor::UseForeground())
								]
							];
					}
					else
					{
						ResetButtonWidget = SNew(SBox)
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Center)
							.Padding(2.f, 4.f)
							[
								SNew(SResetToDefaultPropertyEditor, PropertyHandle)
							];
					}

					return MakeFieldWidget(FieldWidget.ToSharedRef());
				}
			} 
		}

		return MakeFieldWidget(CreateInvalidWidget());
	}

	return MakeFieldWidget(SNullWidget::NullWidget);
}

TSharedRef<SWidget> SRCPanelExposedField::MakeFieldWidget(const TSharedRef<SWidget>& InWidget)
{
	FText WarningMessage;

	if (GetDefault<URemoteControlSettings>()->bDisplayInEditorOnlyWarnings)
	{
		bool bIsEditorOnly = false,
            bIsEditableInPackaged = true,
            bIsCallableInPackaged = true;
		
		if (const TSharedPtr<FRemoteControlField> RCField = GetRemoteControlField().Pin())
		{
			bIsEditorOnly = RCField->IsEditorOnly();
			if (RCField->FieldType == EExposedFieldType::Property)
			{
				bIsEditableInPackaged = StaticCastSharedPtr<FRemoteControlProperty>(RCField)->IsEditableInPackaged();
			}
			else
			{
				bIsCallableInPackaged = StaticCastSharedPtr<FRemoteControlFunction>(RCField)->IsCallableInPackaged();
			}
		}

		FTextBuilder Builder;
		if (bIsEditorOnly)
		{
			Builder.AppendLine(LOCTEXT("EditorOnlyWarning", "This field will be unavailable in packaged projects."));
		}

		if (!bIsEditableInPackaged)
		{
			Builder.AppendLine(LOCTEXT("NotEditableInPackagedWarning", "This property will not be editable in packaged projects."));
		}

		if (!bIsCallableInPackaged)
		{
			Builder.AppendLine(LOCTEXT("NotCallableInPackagedWarning", "This function will not be callable in packaged projects."));
		}
		
		WarningMessage = Builder.ToText();
	}
	
	return CreateEntityWidget(InWidget, ResetButtonWidget.ToSharedRef(), WarningMessage);
}

void SRCPanelExposedField::ConstructPropertyWidget()
{
	ChildSlot.AttachWidget(ConstructWidget());
}

void SRCPanelExposedField::ConstructFunctionWidget()
{
	TSharedPtr<SRCPanelExposedField> ExposedFieldWidget;

	URemoteControlPreset* RCPreset = Preset.Get();
	if (!RCPreset)
	{
		return;
	}

	if (const TSharedPtr<FRemoteControlFunction> RCFunction = RCPreset->GetExposedEntity<FRemoteControlFunction>(EntityId).Pin())
	{
		if (RCFunction->GetFunction() && RCFunction->GetBoundObjects().Num())
		{
			if (TSharedPtr<FRCPanelWidgetRegistry> Registry = WidgetRegistry.Pin())
			{
				Registry->Refresh(RCFunction->FunctionArguments);

				TArray<TSharedPtr<SRCPanelFieldChildNode>> ChildNodes;
				for (TFieldIterator<FProperty> It(RCFunction->GetFunction()); It; ++It)
				{
					const bool Param = It->HasAnyPropertyFlags(CPF_Parm);
					const bool OutParam = It->HasAnyPropertyFlags(CPF_OutParm) && !It->HasAnyPropertyFlags(CPF_ConstParm);
					const bool ReturnParam = It->HasAnyPropertyFlags(CPF_ReturnParm);

					if (!Param || OutParam || ReturnParam)
					{
						continue;
					}

					if (TSharedPtr<IDetailTreeNode> PropertyNode = Registry->GetStructTreeNode(RCFunction->FunctionArguments, It->GetFName().ToString(), ERCFindNodeMethod::Name))
					{
						ChildNodes.Add(SNew(SRCPanelFieldChildNode, PropertyNode.ToSharedRef(), ColumnSizeData));
					}
				}

				ChildSlot.AttachWidget(MakeFieldWidget(ConstructCallFunctionButton()));
				ChildWidgets = ChildNodes;
			}
			

			return;
		}
	}

	ChildSlot.AttachWidget(ConstructWidget());
}

TSharedRef<SWidget> SRCPanelExposedField::ConstructCallFunctionButton(bool bIsEnabled)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.HAlign(HAlign_Left)
		.Padding(2.0f)
		.AutoHeight()
		[
			SNew(SButton)
			.IsEnabled(bIsEnabled)
			.VAlign(VAlign_Center)
			.OnClicked(this, &SRCPanelExposedField::OnClickFunctionButton)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CallFunctionLabel", "Call Function"))
			]
		];
}

FReply SRCPanelExposedField::OnClickFunctionButton()
{
	FScopedTransaction FunctionTransaction(LOCTEXT("CallExposedFunction", "Called a function through preset."));
	FEditorScriptExecutionGuard ScriptGuard;

	if (URemoteControlPreset* RCPreset = Preset.Get())
	{
		if (TSharedPtr<FRemoteControlFunction> Function = RCPreset->GetExposedEntity<FRemoteControlFunction>(EntityId).Pin())
		{
			for (UObject* Object : Function->GetBoundObjects())
			{
				if (Function->FunctionArguments && Function->FunctionArguments->IsValid())
				{
					Object->Modify();
					Object->ProcessEvent(Function->GetFunction(), Function->FunctionArguments->GetStructMemory());
				}
				else
				{
					ensureAlwaysMsgf(false, TEXT("Function default arguments could not be resolved."));
				}
			}
		}
	}

	return FReply::Handled();
}

void SRCPanelFieldChildNode::Construct(const FArguments& InArgs, const TSharedRef<IDetailTreeNode>& InNode, FRCColumnSizeData InColumnSizeData)
{
	TArray<TSharedRef<IDetailTreeNode>> ChildNodes;
	InNode->GetChildren(ChildNodes);

	Algo::Transform(ChildNodes, ChildrenNodes, [InColumnSizeData](const TSharedRef<IDetailTreeNode>& ChildNode) { return SNew(SRCPanelFieldChildNode, ChildNode, InColumnSizeData); });

	ColumnSizeData = InColumnSizeData;

	FNodeWidgets Widgets = InNode->CreateNodeWidgets();
	FMakeNodeWidgetArgs Args;
	Args.NameWidget = Widgets.NameWidget;
	Args.ValueWidget = ExposedFieldUtils::CreateNodeValueWidget(InNode);

	ChildSlot
	[
		MakeNodeWidget(Args)
	];
}

#undef LOCTEXT_NAMESPACE /*RemoteControlPanel*/
