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

#include "NiagaraEmitter.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSystem.h"
#include "NiagaraParameterBinding.h"
#include "Customizations/NiagaraTypeCustomizations.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SNiagaraParameterName.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraParameterBindingCustomization)

#define LOCTEXT_NAMESPACE "NiagaraParameterBindingCustomization"

FNiagaraParameterBindingAction::FNiagaraParameterBindingAction()
	: FEdGraphSchemaAction(
		FText(),
		LOCTEXT("None", "None"),
		LOCTEXT("NoneTooltip", "Not bound to any variable will use the default value"),
		1,
		FText()
	)
{
}

FNiagaraParameterBindingAction::FNiagaraParameterBindingAction(FNiagaraVariableBase InAliasedParameter, FNiagaraVariableBase InParameter)
	: FEdGraphSchemaAction(
		FText(),
		FText::FromName(InAliasedParameter.GetName()),
		FText::Format(LOCTEXT("VariableBindActionTooltip", "Bind to variable \"{0}\""), FText::FromName(InAliasedParameter.GetName())),
		0,
		FText()
	)
	, AliasedParameter(InAliasedParameter)
	, Parameter(InParameter)
{
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
	{
		TArray<UObject*> OuterObjects;
		PropertyHandle->GetOuterObjects(OuterObjects);
		UObject* OwnerObject = OuterObjects.Num() == 1 ? OuterObjects[0] : nullptr;
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

		OwnerWeakPtr = OwnerObject;
		EmitterWeakPtr = OwnerObject->GetTypedOuter<UNiagaraEmitter>();
		SystemWeakPtr = OwnerObject->GetTypedOuter<UNiagaraSystem>();
	}

	HeaderRow
	.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(0.0f, 0.0f, 4.0f, 0.0f)
		[
			SNew(SComboButton)					
			.OnGetMenuContent(this, &FNiagaraParameterBindingCustomization::OnGetMenuContent)
			.ContentPadding(1)
			.ToolTipText(this, &FNiagaraParameterBindingCustomization::GetTooltipText)
			.ButtonContent()
			[
				SNew(SNiagaraParameterName)
				.ParameterName(this, &FNiagaraParameterBindingCustomization::GetVariableName)
				.IsReadOnly(true)
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

void FNiagaraParameterBindingCustomization::CollectAllActions(FGraphActionListBuilderBase& OutAllActions) const
{
	// Add the unbound action this is where we will fallback to the value only
	OutAllActions.AddAction(MakeShared<FNiagaraParameterBindingAction>());

	// Add bindings from system
	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	UNiagaraSystem* NiagaraSystem = SystemWeakPtr.Get();
	UNiagaraEmitter* NiagaraEmitter = EmitterWeakPtr.Get();
	if (ParameterBinding == nullptr || NiagaraSystem == nullptr)
	{
		return;
	}

	TArray<FNiagaraParameterMapHistory> Histories;
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

	TArray<TPair<FNiagaraVariableBase, FNiagaraVariableBase>> AllowedVariables;

	const FStringView NiagaraEmitterName = NiagaraEmitter ? NiagaraEmitter->GetUniqueEmitterName() : FStringView();
	for (const FNiagaraParameterMapHistory& History : Histories)
	{
		for (const FNiagaraVariable& Variable : History.Variables)
		{
			FNiagaraVariableBase AliasedParameter = Variable;
			if (ParameterBinding->CanBindTo(Variable, AliasedParameter, NiagaraEmitterName))
			{
				AllowedVariables.AddUnique({ AliasedParameter, Variable });
				continue;
			}
		}
	}

	if (ParameterBinding->AllowUserParameters())
	{
		for (const FNiagaraVariableBase& Variable : NiagaraSystem->GetExposedParameters().ReadParameterVariables())
		{
			if (ParameterBinding->CanBindTo(Variable.GetType()))
			{
				AllowedVariables.AddUnique({ Variable, Variable });
			}
		}
	}

	for (const TPair<FNiagaraVariableBase, FNiagaraVariableBase>& AllowedVariable : AllowedVariables)
	{
		OutAllActions.AddAction(MakeShared<FNiagaraParameterBindingAction>(AllowedVariable.Key, AllowedVariable.Value));
	}
}

TSharedRef<SWidget> FNiagaraParameterBindingCustomization::OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData) const
{
	const FNiagaraParameterBindingAction* BindingAction = static_cast<const FNiagaraParameterBindingAction*>(InCreateData->Action.Get());
	check(BindingAction);

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
			FScopedTransaction Transaction(FText::Format(LOCTEXT("ChangeParameterBinding", " Change Parameter Binding to \"{0}\" "), FText::FromName(BindAction->Parameter.GetName())));
			FSlateApplication::Get().DismissAllMenus();

			TArray<UObject*> Objects;
			PropertyHandle->GetOuterObjects(Objects);
			for (UObject* Obj : Objects)
			{
				Obj->Modify();
			}
			PropertyHandle->NotifyPreChange();

			ParameterBinding->AliasedParameter = BindAction->AliasedParameter;
			ParameterBinding->Parameter = BindAction->Parameter;

			PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
			PropertyHandle->NotifyFinishedChangingProperties();
		}
	}
}

FText FNiagaraParameterBindingCustomization::GetTooltipText() const
{
	if ( FNiagaraParameterBinding* ParameterBinding = GetParameterBinding() )
	{
		const FName BoundParameterName = ParameterBinding->AliasedParameter.GetName();
		const FText BoundText =
			BoundParameterName.IsNone() ?
			LOCTEXT("BindingTooltip_UnboundParameter", "Not bound to any parameter.") :
			FText::Format(LOCTEXT("BindingTooltip_BoundParameter", "Bound to variable \"{0}\""), FText::FromName(BoundParameterName));

		const FText ValueText =
			//ParameterBinding->Parameter.IsValid() && ParameterBinding->Parameter.GetAllocatedSizeInBytes() > 0 ?
			//FText::Format(LOCTEXT("BindingTooltip_Value", "Default value \"{0}\""), FText::FromString(ParameterBinding->Parameter.ToString())) :
			LOCTEXT("BindingTooltip_NoValue", "No default value");

		return FText::Format(LOCTEXT("BindingTooltip", "{0}\n{1}"), BoundText, ValueText);
	}
	return FText::FromString(TEXT("Missing"));
}

FName FNiagaraParameterBindingCustomization::GetVariableName() const
{
	FNiagaraParameterBinding* ParameterBinding = GetParameterBinding();
	return ParameterBinding ? ParameterBinding->AliasedParameter.GetName() : FName();
}

void FNiagaraParameterBindingCustomization::ResetToDefault()
{

}

#undef LOCTEXT_NAMESPACE
