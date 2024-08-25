// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterBindingCustomization.h"

#include "CoreMinimal.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailGroup.h"
#include "IPropertyUtilities.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "SGraphActionMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SComboButton.h"

#include "INiagaraEditorTypeUtilities.h"
#include "NiagaraEditorModule.h"
#include "NiagaraEmitter.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSystem.h"
#include "NiagaraVariableMetaData.h"
#include "NiagaraParameterBinding.h"
#include "SNiagaraParameterEditor.h"
#include "Customizations/NiagaraTypeCustomizations.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SNiagaraParameterName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraParameterBindingCustomization)

#define LOCTEXT_NAMESPACE "NiagaraParameterBindingCustomization"

FNiagaraParameterBindingAction::FNiagaraParameterBindingAction()
	: FEdGraphSchemaAction(FText(), FText(), FText(), 0)
{
}

FNiagaraParameterBindingAction::FNiagaraParameterBindingAction(FText InName, FText InTooltip)
	: FEdGraphSchemaAction(FText(), InName, InTooltip, 0)
{
}

TSharedRef<FNiagaraParameterBindingAction> FNiagaraParameterBindingAction::MakeNone()
{
	TSharedRef<FNiagaraParameterBindingAction> Action = MakeShared<FNiagaraParameterBindingAction>(
		LOCTEXT("None", "None"),
		LOCTEXT("NoneTooltip", "Not bound to any parameter.")
	);
	Action->Grouping = 1;
	return Action;
}

TSharedRef<FNiagaraParameterBindingAction> FNiagaraParameterBindingAction::MakeConstant(const FNiagaraTypeDefinition& TypeDef, const uint8* ValueData)
{
	TSharedRef<FNiagaraParameterBindingAction> Action = MakeShared<FNiagaraParameterBindingAction>(
		FText::Format(LOCTEXT("ConstantFormat", "Constant Value \"{0}\""), FText::FromString(TypeDef.ToString(ValueData))),
		LOCTEXT("ConstantTooltip", "Use user defined constant value, not bound to any parameter.")
	);
	Action->Grouping = 1;
	return Action;
}

TSharedRef<FNiagaraParameterBindingAction> FNiagaraParameterBindingAction::MakeParameter(FNiagaraVariableBase InAliasedParameter, FNiagaraVariableBase InResolvedParameter)
{
	TSharedRef<FNiagaraParameterBindingAction> Action = MakeShared<FNiagaraParameterBindingAction>(
		FText::FromName(InAliasedParameter.GetName()),
		FText::Format(LOCTEXT("VariableBindActionTooltip", "Bind to variable \"{0}\""), FText::FromName(InAliasedParameter.GetName()))
	);
	Action->Grouping = 0;
	Action->bIsValidParameter = true;
	Action->AliasedParameter = InAliasedParameter;
	Action->ResolvedParameter = InResolvedParameter;
	return Action;
}

TSharedRef<IPropertyTypeCustomization> FNiagaraParameterBindingCustomization::MakeInstance()
{
	return MakeShared<FNiagaraParameterBindingCustomization>();
}

void FNiagaraParameterBindingCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	EmitterWeakPtr.Reset();
	SystemWeakPtr.Reset();

	// We only handle single object
	UObject* OwnerObject = nullptr;
	{
		TArray<UObject*> OuterObjects;
		PropertyHandle->GetOuterObjects(OuterObjects);
		OwnerObject = OuterObjects.Num() == 1 ? OuterObjects[0] : nullptr;
		if (OwnerObject == nullptr)
		{
			HeaderRow
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MaxDesiredWidth(200.f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FName::NameToDisplayString(CastField<FStructProperty>(PropertyHandle->GetProperty())->Struct->GetName(), false)))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];

			return;
		}
	}

	InPropertyHandle->SetOnPropertyResetToDefault(FSimpleDelegate::CreateLambda([]() { }));
	PropertyHandle->MarkResetToDefaultCustomized(true);

	OwnerWeakPtr = OwnerObject;
	EmitterWeakPtr = OwnerObject->GetTypedOuter<UNiagaraEmitter>();
	SystemWeakPtr = OwnerObject->GetTypedOuter<UNiagaraSystem>();

	TSharedPtr<SHorizontalBox> ParameterPanel = SNew(SHorizontalBox);

	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	if (ParameterBinding && ParameterBinding->HasDefaultValueEditorOnly())
	{
		const FNiagaraVariableBase& DefaultParameter = ParameterBinding->GetDefaultAliasedParameter();
		TConstArrayView<uint8> DefaultValue = ParameterBinding->GetDefaultValueEditorOnly();
		DefaultValueStructOnScope = MakeShared<FStructOnScope>(DefaultParameter.GetType().GetStruct());
		FMemory::Memcpy(DefaultValueStructOnScope->GetStructMemory(), DefaultValue.GetData(), DefaultValue.Num());

		FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::GetModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
		TSharedPtr<INiagaraEditorTypeUtilities, ESPMode::ThreadSafe> TypeEditorUtilities = NiagaraEditorModule.GetTypeUtilities(DefaultParameter.GetType());
		if (TypeEditorUtilities.IsValid())
		{
			FNiagaraInputParameterCustomization CustomizationOptions;
			CustomizationOptions.bBroadcastValueChangesOnCommitOnly = true; // each broadcast usually forces a recompile, so we only want to do it on commits
			DefaultValueParameterEditor = TypeEditorUtilities->CreateParameterEditor(ParameterBinding->ResolvedParameter.GetType(), EUnit::Unspecified, CustomizationOptions);
			DefaultValueParameterEditor->UpdateInternalValueFromStruct(DefaultValueStructOnScope.ToSharedRef());
			DefaultValueParameterEditor->SetOnValueChanged(SNiagaraParameterEditor::FOnValueChange::CreateSP(this, &FNiagaraParameterBindingCustomization::OnValueChanged));

			ParameterPanel->AddSlot()
			.AutoWidth()
			.Padding(0, 0, 0, 3)
			[
				SNew(SBox)
				.HAlign(DefaultValueParameterEditor->GetHorizontalAlignment())
				.VAlign(DefaultValueParameterEditor->GetVerticalAlignment())
				.IsEnabled(this, &FNiagaraParameterBindingCustomization::IsConstantEnabled)
				.Visibility(this, &FNiagaraParameterBindingCustomization::IsConstantVisibile)
				[
					DefaultValueParameterEditor.ToSharedRef()
				]
			];
		}
	}

	ParameterPanel->AddSlot()
	.AutoWidth()
	[
		SNew(SNiagaraParameterName)
		.ParameterName(this, &FNiagaraParameterBindingCustomization::GetVariableName)
		.IsReadOnly(true)
		.IsEnabled(this, &FNiagaraParameterBindingCustomization::IsBindingEnabled)
		.Visibility(this, &FNiagaraParameterBindingCustomization::IsBindingVisibile)
		.ToolTipText(this, &FNiagaraParameterBindingCustomization::GetTooltipText)
	];

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			ParameterPanel.ToSharedRef()
		]
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.HAlign(HAlign_Right)
		.Padding(3, 0, 0, 0)
		[
			SNew(SComboButton)					
			.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
			.ForegroundColor(FSlateColor::UseForeground())
			.OnGetMenuContent(this, &FNiagaraParameterBindingCustomization::OnGetMenuContent)
			.ContentPadding(2)
			.MenuPlacement(MenuPlacement_BelowRightAnchor)
			.ToolTipText(this, &FNiagaraParameterBindingCustomization::GetTooltipText)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(2, 1)
		[
			SNew(SButton)
			.IsFocusable(false)
			.ToolTipText(LOCTEXT("ResetToDefaultToolTip", "Reset to Default"))
			.ButtonStyle(FAppStyle::Get(), "NoBorder")
			.ContentPadding(0)
			.Visibility(this, &FNiagaraParameterBindingCustomization::IsResetToDefaultsVisible)
			.OnClicked(this, &FNiagaraParameterBindingCustomization::OnResetToDefaultsClicked)
			.Content()
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
	];
}

bool FNiagaraParameterBindingCustomization::IsValid() const
{
	return OwnerWeakPtr.IsValid();
}

FNiagaraParameterBinding* FNiagaraParameterBindingCustomization::GetParameterBinding() const
{
	UObject* OwnerObject = OwnerWeakPtr.Get();
	return OwnerObject ? (FNiagaraParameterBinding*)PropertyHandle->GetValueBaseAddress((uint8*)OwnerObject) : nullptr;
}

FNiagaraParameterBinding* FNiagaraParameterBindingCustomization::GetDefaultParameterBinding() const
{
	UObject* OwnerObject = OwnerWeakPtr.Get();
	return OwnerObject ? (FNiagaraParameterBinding*)PropertyHandle->GetValueBaseAddress((uint8*)OwnerObject->GetClass()->GetDefaultObject()) : nullptr;
}

TSharedRef<SWidget> FNiagaraParameterBindingCustomization::OnGetMenuContent() const
{
	FGraphActionMenuBuilder MenuBuilder;

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			[
				SNew(SGraphActionMenu)
				.OnActionSelected(this, &FNiagaraParameterBindingCustomization::OnActionSelected)
				.OnCreateWidgetForAction(this, &FNiagaraParameterBindingCustomization::OnCreateWidgetForAction)
				.OnCollectAllActions(this, &FNiagaraParameterBindingCustomization::CollectAllActions)
				.AutoExpandActionMenu(false)
				.ShowFilterTextBox(true)
			]
		];
}

bool FNiagaraParameterBindingCustomization::ForEachBindableVariable(TFunction<bool(FNiagaraVariableBase, FNiagaraVariableBase)> Delegate) const
{
	// Add bindings from system
	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	UNiagaraSystem* NiagaraSystem = SystemWeakPtr.Get();
	UNiagaraEmitter* NiagaraEmitter = EmitterWeakPtr.Get();
	if (ParameterBinding == nullptr || NiagaraSystem == nullptr)
	{
		return true;
	}

	TArray<FNiagaraParameterMapHistory, TInlineAllocator<2>> Histories;
	if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(NiagaraSystem->GetSystemUpdateScript()->GetLatestSource()))
	{
		Histories.Append(UNiagaraNodeParameterMapBase::GetParameterMaps(Source->NodeGraph));
	}

	if (NiagaraEmitter)
	{
		// This is lame but currently the only way to get the version data
		if (UObject* Owner = OwnerWeakPtr.Get())
		{
			if (UNiagaraSimulationStageBase* SimulationStage = Cast<UNiagaraSimulationStageBase>(Owner))
			{
				if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(SimulationStage->GetEmitterData()->GraphSource))
				{
					Histories.Append(UNiagaraNodeParameterMapBase::GetParameterMaps(Source->NodeGraph));
				}
			}
			else if (UNiagaraRendererProperties* RendererProperties = Cast<UNiagaraRendererProperties>(Owner))
			{
				if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(RendererProperties->GetEmitterData()->GraphSource))
				{
					Histories.Append(UNiagaraNodeParameterMapBase::GetParameterMaps(Source->NodeGraph));
				}
			}
		}
	}

	TArray<FNiagaraVariable> VisitedVariables;

	const FStringView NiagaraEmitterName = NiagaraEmitter ? NiagaraEmitter->GetUniqueEmitterName() : FStringView();
	for (const FNiagaraParameterMapHistory& History : Histories)
	{
		for (const FNiagaraVariable& Variable : History.Variables)
		{
			FNiagaraVariableBase AliasedParameter = Variable;
			if (ParameterBinding->CanBindTo(Variable, AliasedParameter, NiagaraEmitterName) && !VisitedVariables.Contains(Variable))
			{
				if (Delegate(AliasedParameter, Variable) == false)
				{
					return false;
				}
				VisitedVariables.Add(Variable);
			}
		}
	}

	if (ParameterBinding->AllowUserParameters())
	{
		for (const FNiagaraVariableBase& Variable : NiagaraSystem->GetExposedParameters().ReadParameterVariables())
		{
			if (ParameterBinding->CanBindTo(Variable.GetType()) && !VisitedVariables.Contains(Variable))
			{
				if (Delegate(Variable, Variable) == false)
				{
					return false;
				}
				VisitedVariables.Add(Variable);
			}
		}
	}
	return true;
}


void FNiagaraParameterBindingCustomization::CollectAllActions(FGraphActionListBuilderBase& OutAllActions) const
{
	// Add default action
	bool bHasConstantValue = false;
	if (FNiagaraParameterBinding* ParameterBinding = GetParameterBinding())
	{
		UNiagaraSystem* NiagaraSystem = SystemWeakPtr.Get();
		UNiagaraEmitter* NiagaraEmitter = EmitterWeakPtr.Get();

		if (ParameterBinding->HasDefaultValueEditorOnly())
		{
			OutAllActions.AddAction(FNiagaraParameterBindingAction::MakeConstant(ParameterBinding->GetDefaultAliasedParameter().GetType(), ParameterBinding->GetDefaultValueEditorOnly().GetData()));
		}
		else
		{
			OutAllActions.AddAction(FNiagaraParameterBindingAction::MakeNone());
		}
	}
	else
	{
		OutAllActions.AddAction(FNiagaraParameterBindingAction::MakeNone());
	}

	// Add each variable
	ForEachBindableVariable(
		[&OutAllActions](FNiagaraVariableBase AliasedVariable, FNiagaraVariableBase ResolvedVariable)
		{
			OutAllActions.AddAction(FNiagaraParameterBindingAction::MakeParameter(AliasedVariable, ResolvedVariable));
			return true;
		}
	);
}

TSharedRef<SWidget> FNiagaraParameterBindingCustomization::OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData) const
{
	const FNiagaraParameterBindingAction* BindingAction = static_cast<const FNiagaraParameterBindingAction*>(InCreateData->Action.Get());
	check(BindingAction);

	if (BindingAction->bIsValidParameter)
	{
		return
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SNiagaraParameterName)
				.ParameterName(BindingAction->AliasedParameter.GetName())
				.IsReadOnly(true)
				.ToolTipText(InCreateData->Action->GetTooltipDescription())
			];
	}
	else
	{
		return
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(InCreateData->Action->GetMenuDescription())
				.ToolTipText(InCreateData->Action->GetTooltipDescription())
			];
	}
}

void FNiagaraParameterBindingCustomization::OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType) const
{
	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	if (ParameterBinding == nullptr || !PropertyHandle.IsValid() || (InSelectionType != ESelectInfo::OnMouseClick && InSelectionType != ESelectInfo::OnKeyPress))
	{
		return;
	}

	for (const TSharedPtr<FEdGraphSchemaAction>& EdAction : SelectedActions)
	{
		if (EdAction.IsValid())
		{
			FNiagaraParameterBindingAction* BindAction = static_cast<FNiagaraParameterBindingAction*>(EdAction.Get());
			FScopedTransaction Transaction(FText::Format(LOCTEXT("ChangeParameterBinding", " Change Parameter Binding to \"{0}\" "), FText::FromName(BindAction->ResolvedParameter.GetName())));
			FSlateApplication::Get().DismissAllMenus();

			TArray<UObject*> Objects;
			PropertyHandle->GetOuterObjects(Objects);
			for (UObject* Obj : Objects)
			{
				Obj->Modify();
			}
			PropertyHandle->NotifyPreChange();

			if (BindAction->bIsValidParameter)
			{
				ParameterBinding->AliasedParameter = BindAction->AliasedParameter;
				ParameterBinding->ResolvedParameter = BindAction->ResolvedParameter;
			}
			else
			{
				ParameterBinding->AliasedParameter = FNiagaraVariableBase();
				ParameterBinding->ResolvedParameter = FNiagaraVariableBase();
			}

			PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			PropertyHandle->NotifyFinishedChangingProperties();
		}
	}
}

void FNiagaraParameterBindingCustomization::OnValueChanged() const
{
	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	if (ParameterBinding == nullptr || !PropertyHandle.IsValid())
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("ChangeParameterValue", " Change Parameter Value"));

	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	for (UObject* Obj : Objects)
	{
		Obj->Modify();
	}
	PropertyHandle->NotifyPreChange();

	DefaultValueParameterEditor->UpdateStructFromInternalValue(DefaultValueStructOnScope.ToSharedRef());
	ParameterBinding->SetDefaultValueEditorOnly(DefaultValueStructOnScope->GetStructMemory());

	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}

EVisibility FNiagaraParameterBindingCustomization::IsBindingVisibile() const
{
	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	if (ParameterBinding && ParameterBinding->HasDefaultValueEditorOnly())
	{
		return ParameterBinding->ResolvedParameter.GetName().IsNone() ? EVisibility::Collapsed : EVisibility::Visible;
	}
	return EVisibility::Visible;
}

EVisibility FNiagaraParameterBindingCustomization::IsConstantVisibile() const
{
	return IsBindingVisibile() == EVisibility::Visible ? EVisibility::Collapsed : EVisibility::Visible;
}

bool FNiagaraParameterBindingCustomization::IsBindingEnabled() const
{
	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	if (ParameterBinding && ParameterBinding->HasDefaultValueEditorOnly())
	{
		return ParameterBinding->ResolvedParameter.GetName().IsNone() == false;
	}
	return true;
}

bool FNiagaraParameterBindingCustomization::IsConstantEnabled() const
{
	return !IsBindingEnabled();
}

FText FNiagaraParameterBindingCustomization::GetTooltipText() const
{
	if ( FNiagaraParameterBinding* ParameterBinding = GetParameterBinding() )
	{
		if (ParameterBinding->AliasedParameter.GetName().IsNone())
		{
			if (ParameterBinding->HasDefaultValueEditorOnly())
			{
				const FNiagaraTypeDefinition ConstantType = ParameterBinding->GetDefaultAliasedParameter().GetType();
				return FText::Format(LOCTEXT("BindingTooltip_BoundConstant", "Bound to constant value \"{0}\" type \"{1}\""),
					FText::FromString(ConstantType.ToString(ParameterBinding->GetDefaultValueEditorOnly().GetData())),
					FText::FromString(ConstantType.GetName())
				);
			}
			else
			{
				return LOCTEXT("BindingTooltip_UnboundParameter", "Not bound to any parameter.");
			}
		}
		else
		{
			return FText::Format(LOCTEXT("BindingTooltip_BoundParameter", "Bound to parameter \"{0}\" type \"{1}\""),
				FText::FromName(ParameterBinding->AliasedParameter.GetName()),
				FText::FromString(ParameterBinding->AliasedParameter.GetType().GetName())
			);
		}
	}
	return FText::FromString(TEXT("Missing"));
}

FName FNiagaraParameterBindingCustomization::GetVariableName() const
{
	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	return ParameterBinding ? ParameterBinding->AliasedParameter.GetName() : FName();
}

EVisibility FNiagaraParameterBindingCustomization::IsResetToDefaultsVisible() const
{
	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	return ParameterBinding && ParameterBinding->IsSetoToDefault() ? EVisibility::Hidden : EVisibility::Visible;
}

FReply FNiagaraParameterBindingCustomization::OnResetToDefaultsClicked()
{
	if (FNiagaraParameterBinding* ParameterBinding = GetParameterBinding())
	{
		ParameterBinding->SetToDefault();

		if (DefaultValueStructOnScope.IsValid())
		{
			TConstArrayView<uint8> DefaultValue = ParameterBinding->GetDefaultValueEditorOnly();
			FMemory::Memcpy(DefaultValueStructOnScope->GetStructMemory(), DefaultValue.GetData(), DefaultValue.Num());
			DefaultValueParameterEditor->UpdateInternalValueFromStruct(DefaultValueStructOnScope.ToSharedRef());
		}

		PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		PropertyHandle->NotifyFinishedChangingProperties();
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
