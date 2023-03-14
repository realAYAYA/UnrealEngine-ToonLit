// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraTypeCustomizations.h"

#include "CoreMinimal.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "IPropertyUtilities.h"
#include "NiagaraConstants.h"
#include "NiagaraDataInterfaceRW.h"
#include "NiagaraEditorModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraParameterDefinitions.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraScriptSource.h"
#include "NiagaraScriptVariable.h"
#include "NiagaraSimulationStageBase.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "SGraphActionMenu.h"
#include "DeviceProfiles/DeviceProfileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/TNiagaraViewModelManager.h"
#include "Widgets/SNiagaraParameterName.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "IDetailGroup.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraTypeCustomizations)

#define LOCTEXT_NAMESPACE "FNiagaraVariableAttributeBindingCustomization"
#define ALLOW_LIBRARY_TO_LIBRARY_DEFAULT_BINDING 0

void FNiagaraNumericCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyHandle> ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(ValueHandle.IsValid() ? 125.f : 200.f)
		[
			// Some Niagara numeric types have no value so in that case just display their type name
			ValueHandle.IsValid()
			? ValueHandle->CreatePropertyValueWidget()
			: SNew(STextBlock)
			  .Text(FText::FromString(FName::NameToDisplayString(CastField<FStructProperty>(PropertyHandle->GetProperty())->Struct->GetName(), false)))
			  .Font(IDetailLayoutBuilder::GetDetailFont())
		];
}


void FNiagaraBoolCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	ValueHandle = PropertyHandle->GetChildHandle(TEXT("Value"));

	static const FName DefaultForegroundName("DefaultForeground");

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.OnCheckStateChanged(this, &FNiagaraBoolCustomization::OnCheckStateChanged)
			.IsChecked(this, &FNiagaraBoolCustomization::OnGetCheckState)
			.ForegroundColor(FAppStyle::GetSlateColor(DefaultForegroundName))
			.Padding(0.0f)
		];
}

ECheckBoxState FNiagaraBoolCustomization::OnGetCheckState() const
{
	ECheckBoxState CheckState = ECheckBoxState::Undetermined;
	int32 Value;
	FPropertyAccess::Result Result = ValueHandle->GetValue(Value);
	if (Result == FPropertyAccess::Success)
	{
		CheckState = Value == FNiagaraBool::True ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}

	return CheckState;
}

void FNiagaraBoolCustomization::OnCheckStateChanged(ECheckBoxState InNewState)
{
	if (InNewState == ECheckBoxState::Checked)
	{
		ValueHandle->SetValue(FNiagaraBool::True);
	}
	else
	{
		ValueHandle->SetValue(FNiagaraBool::False);
	}
}

void FNiagaraMatrixCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	uint32 NumChildren = 0;
	PropertyHandle->GetNumChildren(NumChildren);

	for (uint32 ChildNum = 0; ChildNum < NumChildren; ++ChildNum)
	{
		ChildBuilder.AddProperty(PropertyHandle->GetChildHandle(ChildNum).ToSharedRef());
	}
}

TArray<FNiagaraVariableBase> FNiagaraStackAssetAction_VarBind::FindVariables(const FVersionedNiagaraEmitter& InEmitter, bool bSystem, bool bEmitter, bool bParticles, bool bUser, bool bAllowStatic)
{
	TArray<FNiagaraVariableBase> Bindings;
	TArray<FNiagaraParameterMapHistory> Histories;

	UNiagaraEmitter* Emitter = InEmitter.Emitter;
	UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(InEmitter.GetEmitterData()->GraphSource);
	if (Source)
	{
		Histories.Append(UNiagaraNodeParameterMapBase::GetParameterMaps(Source->NodeGraph));
	}

	if (bSystem || bEmitter)
	{
		if (UNiagaraSystem* Sys = Emitter->GetTypedOuter<UNiagaraSystem>())
		{
			Source = Cast<UNiagaraScriptSource>(Sys->GetSystemUpdateScript()->GetLatestSource()); 
			if (Source)
			{
				Histories.Append(UNiagaraNodeParameterMapBase::GetParameterMaps(Source->NodeGraph));
			}
		}
	}
	


	for (const FNiagaraParameterMapHistory& History : Histories)
	{
		for (const FNiagaraVariable& Var : History.Variables)
		{
			if (Var.GetType().IsStatic() && !bAllowStatic)
				continue;

			if (FNiagaraParameterMapHistory::IsAttribute(Var) && bParticles)
			{
				Bindings.AddUnique(Var);
			}
			else if (FNiagaraParameterMapHistory::IsSystemParameter(Var) && bSystem)
			{
				Bindings.AddUnique(Var);
			}
			else if (Var.IsInNameSpace(Emitter->GetUniqueEmitterName()) && bEmitter)
			{
				Bindings.AddUnique(FNiagaraUtilities::ResolveAliases(Var, FNiagaraAliasContext()
					.ChangeEmitterNameToEmitter(Emitter->GetUniqueEmitterName())));
			}
			else if (FNiagaraParameterMapHistory::IsAliasedEmitterParameter(Var) && bEmitter)
			{
				Bindings.AddUnique(Var);
			}
			else if (Var.IsInNameSpace(FNiagaraConstants::EmitterNamespaceString) && bEmitter)
			{
				Bindings.AddUnique(Var);
			}
			else if (FNiagaraParameterMapHistory::IsUserParameter(Var) && bUser)
			{
				Bindings.AddUnique(Var);
			}
		}
	}

	if (bUser)
	{
		if (UNiagaraSystem* Sys = Emitter->GetTypedOuter<UNiagaraSystem>())
		{
			for (const FNiagaraVariable Var : Sys->GetExposedParameters().ReadParameterVariables())
			{
				Bindings.AddUnique(Var);
			}
		}
	}
	return Bindings;
}


FName FNiagaraVariableAttributeBindingCustomization::GetVariableName() const
{
	if (BaseEmitter.Emitter && TargetVariableBinding)
	{
		return (TargetVariableBinding->GetName());
	}
	return FName();
}

FText FNiagaraVariableAttributeBindingCustomization::GetCurrentText() const
{
	if (BaseEmitter.Emitter && TargetVariableBinding)
	{
		return FText::FromName(TargetVariableBinding->GetName());
	}
	return FText::FromString(TEXT("Missing"));
}

FText FNiagaraVariableAttributeBindingCustomization::GetTooltipText() const
{
	if (BaseEmitter.Emitter && TargetVariableBinding)
	{
		FString DefaultValueStr = TargetVariableBinding->GetDefaultValueString();

		FText TooltipDesc = FText::Format(LOCTEXT("AttributeBindingTooltip", "Use the variable \"{0}\" if it exists, otherwise use the default \"{1}\" "), FText::FromName(TargetVariableBinding->GetName()),
			FText::FromString(DefaultValueStr));
		return TooltipDesc;
	}
	return FText::FromString(TEXT("Missing"));
}

TSharedRef<SWidget> FNiagaraVariableAttributeBindingCustomization::OnGetMenuContent() const
{
	FGraphActionMenuBuilder MenuBuilder;

	TSharedPtr<SGraphActionMenu> GraphActionMenu;
	
	TSharedPtr<SWidget> Widget = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			[
				SAssignNew(GraphActionMenu, SGraphActionMenu)
				.OnActionSelected(const_cast<FNiagaraVariableAttributeBindingCustomization*>(this), &FNiagaraVariableAttributeBindingCustomization::OnActionSelected)
				.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(const_cast<FNiagaraVariableAttributeBindingCustomization*>(this), &FNiagaraVariableAttributeBindingCustomization::OnCreateWidgetForAction))
				.OnCollectAllActions(const_cast<FNiagaraVariableAttributeBindingCustomization*>(this), &FNiagaraVariableAttributeBindingCustomization::CollectAllActions)
				.AutoExpandActionMenu(false)
				.bAllowPreselectedItemActivation(true)
				.ShowFilterTextBox(true)
			]
		];

	// the widget to focus is retrieved after this function is called via delegate, so setting it here works
	TSharedPtr<SWidget> SearchBoxToFocus = GraphActionMenu->GetFilterTextBox();
	ComboButton->SetMenuContentWidgetToFocus(SearchBoxToFocus);
	
	return Widget.ToSharedRef(); 
}

TArray<FName> FNiagaraVariableAttributeBindingCustomization::GetNames(const FVersionedNiagaraEmitter& InEmitter) const
{
	TArray<FName> Names;
	if (!PropertyHandle.IsValid() || !PropertyHandle->GetProperty() || !TargetVariableBinding)
	{
		return Names;
	}

	const bool bSystem = true;
	const bool bEmitter = true;
	const bool bParticles = true;
	const bool bUser = true;
	const bool bAllowStatic = true;
	TArray<FNiagaraVariableBase> Vars = FNiagaraStackAssetAction_VarBind::FindVariables(InEmitter, bSystem, bEmitter, bParticles, bUser, bAllowStatic);

	for (const FNiagaraVariableBase& Var : Vars)
	{
		if (!bAllowStatic && Var.GetType() != TargetVariableBinding->GetType() )
		{
			continue;
		}
		else if (bAllowStatic && !Var.GetType().IsSameBaseDefinition(TargetVariableBinding->GetType()))
		{
			continue;
		}

		if ( RenderProps && RenderProps->IsSupportedVariableForBinding(Var, *PropertyHandle->GetProperty()->GetName()) )
		{
			Names.AddUnique(Var.GetName());
		}
		else if ( SimulationStage )
		{
			// Unless we have an explicit "particles." binding we are not in the particle namespace
			const bool IsParticleNamespace = !TargetVariableBinding->GetName().IsNone() && TargetVariableBinding->IsParticleBinding();
			if ( IsParticleNamespace == Var.IsInNameSpace(FNiagaraConstants::ParticleAttributeNamespace) )
			{
				Names.AddUnique(Var.GetName());
			}
		}
	}

	return Names;
}

void FNiagaraVariableAttributeBindingCustomization::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	TArray<FName> EventNames = GetNames(BaseEmitter);
	for (FName EventName : EventNames)
	{
		FText CategoryName = FText();
		FString DisplayNameString = FName::NameToDisplayString(EventName.ToString(), false);
		const FText NameText = FText::FromString(DisplayNameString);
		const FText TooltipDesc = FText::Format(LOCTEXT("SetFunctionPopupTooltip", "Use the variable \"{0}\" "), FText::FromString(DisplayNameString));
		TSharedPtr<FNiagaraStackAssetAction_VarBind> NewNodeAction(new FNiagaraStackAssetAction_VarBind(EventName, CategoryName, NameText,
			TooltipDesc, 0, FText()));
		OutAllActions.AddAction(NewNodeAction);
	}
}

TSharedRef<SWidget> FNiagaraVariableAttributeBindingCustomization::OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SNiagaraParameterName)
			.ParameterName(((FNiagaraStackAssetAction_VarBind* const)InCreateData->Action.Get())->VarName)
			.IsReadOnly(true)
			//SNew(STextBlock)
			//.Text(InCreateData->Action->GetMenuDescription())
			.ToolTipText(InCreateData->Action->GetTooltipDescription())
		];
}


void FNiagaraVariableAttributeBindingCustomization::OnActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedActions, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedActions.Num() == 0)
	{
		for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
		{
			TSharedPtr<FEdGraphSchemaAction> CurrentAction = SelectedActions[ActionIndex];

			if (CurrentAction.IsValid())
			{
				FSlateApplication::Get().DismissAllMenus();
				FNiagaraStackAssetAction_VarBind* EventSourceAction = (FNiagaraStackAssetAction_VarBind*)CurrentAction.Get();
				ChangeSource(EventSourceAction->VarName);
			}
		}
	}
}

void FNiagaraVariableAttributeBindingCustomization::ChangeSource(FName InVarName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("ChangeVariableSource", " Change Variable Source to \"{0}\" "), FText::FromName(InVarName)));
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	for (UObject* Obj : Objects)
	{
		Obj->Modify();
	}
	check(BaseEmitter.Emitter);
	check(RenderProps || SimulationStage);

	PropertyHandle->NotifyPreChange();
	const ENiagaraRendererSourceDataMode BindingSourceMode = RenderProps ? RenderProps->GetCurrentSourceMode() : ENiagaraRendererSourceDataMode::Emitter;
	TargetVariableBinding->SetValue(InVarName, BaseEmitter, BindingSourceMode);
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}

void FNiagaraVariableAttributeBindingCustomization::ResetToDefault()
{
	UE_LOG(LogNiagaraEditor, Warning, TEXT("Reset to default!"));
}

EVisibility FNiagaraVariableAttributeBindingCustomization::IsResetToDefaultsVisible() const
{
	check(BaseEmitter.Emitter);
	check(RenderProps || SimulationStage);
	check(TargetVariableBinding);
	check(DefaultVariableBinding);

	const ENiagaraRendererSourceDataMode BindingSourceMode = RenderProps ? RenderProps->GetCurrentSourceMode() : ENiagaraRendererSourceDataMode::Emitter;
	return TargetVariableBinding->MatchesDefault(*DefaultVariableBinding, BindingSourceMode) ? EVisibility::Hidden : EVisibility::Visible;
}

FReply FNiagaraVariableAttributeBindingCustomization::OnResetToDefaultsClicked()
{
	FScopedTransaction Transaction(LOCTEXT("ResetBindingParam", "Reset binding"));
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	for (UObject* Obj : Objects)
	{
		Obj->Modify();
	}
	check(BaseEmitter.Emitter);
	check(RenderProps || SimulationStage);
	check(TargetVariableBinding);
	check(DefaultVariableBinding);

	PropertyHandle->NotifyPreChange();
	const ENiagaraRendererSourceDataMode BindingSourceMode = RenderProps ? RenderProps->GetCurrentSourceMode() : ENiagaraRendererSourceDataMode::Emitter;
	TargetVariableBinding->ResetToDefault(*DefaultVariableBinding, BaseEmitter, BindingSourceMode);
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
	return FReply::Handled();
}

void FNiagaraVariableAttributeBindingCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	RenderProps = nullptr;
	SimulationStage = nullptr;
	BaseEmitter = FVersionedNiagaraEmitter();
	PropertyHandle = InPropertyHandle;
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	bool bAddDefault = true;

	InPropertyHandle->SetOnPropertyResetToDefault(FSimpleDelegate::CreateLambda([this]() { ResetToDefault(); }));
	//InPropertyHandle->ExecuteCustomResetToDefault

	/*FResetToDefaultOverride ResetOverride = FResetToDefaultOverride::Create(
		FIsResetToDefaultVisible::CreateSP(this, &FMotionControllerDetails::IsSourceValueModified),
		FResetToDefaultHandler::CreateSP(this, &FMotionControllerDetails::OnResetSourceValue)
	);

	PropertyRow.OverrideResetToDefault(ResetOverride); */
	InPropertyHandle->MarkResetToDefaultCustomized(true);
	
	if (Objects.Num() == 1)
	{
		RenderProps = Cast<UNiagaraRendererProperties>(Objects[0]);
		if (RenderProps)
		{
			BaseEmitter = RenderProps->GetOuterEmitter();
		}

		SimulationStage = Cast<UNiagaraSimulationStageBase>(Objects[0]);
		if ( SimulationStage )
		{
			BaseEmitter = SimulationStage->GetOuterEmitter();
		}

		if (BaseEmitter.Emitter)
		{
			TargetVariableBinding = (FNiagaraVariableAttributeBinding*)PropertyHandle->GetValueBaseAddress((uint8*)Objects[0]);
			DefaultVariableBinding = (FNiagaraVariableAttributeBinding*)PropertyHandle->GetValueBaseAddress((uint8*)Objects[0]->GetClass()->GetDefaultObject());
				
			HeaderRow
				.NameContent()
				[
					PropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(0.0f, 0.0f, 4.0f, 0.0f)
					[
						SAssignNew(ComboButton, SComboButton)					
						.OnGetMenuContent(this, &FNiagaraVariableAttributeBindingCustomization::OnGetMenuContent)
						.ContentPadding(1)
						.ToolTipText(this, &FNiagaraVariableAttributeBindingCustomization::GetTooltipText)
						.ButtonContent()
						[
							/*SNew(STextBlock)
							.Text(this, &FNiagaraVariableAttributeBindingCustomization::GetCurrentText)
							.Font(IDetailLayoutBuilder::GetDetailFont())*/
							SNew(SNiagaraParameterName)
							.ParameterName(this, &FNiagaraVariableAttributeBindingCustomization::GetVariableName)
							.IsReadOnly(true)
							//SNew(STextBlock)
							//.Text(InCreateData->Action->GetMenuDescription())
						]
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
						.Visibility(this, &FNiagaraVariableAttributeBindingCustomization::IsResetToDefaultsVisible)
						.OnClicked(this, &FNiagaraVariableAttributeBindingCustomization::OnResetToDefaultsClicked)
						.Content()
						[
							SNew(SImage)
							.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
							.ColorAndOpacity(FSlateColor::UseForeground())
						]
					]
				];
			bAddDefault = false;
		}
	}
	
	if (bAddDefault)
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
	}
}

//////////////////////////////////////////////////////////////////////////

FName FNiagaraUserParameterBindingCustomization::GetVariableName() const
{
	if (IsValid() && TargetUserParameterBinding)
	{
		return (TargetUserParameterBinding->Parameter.GetName());
	}
	return FName();
}

FText FNiagaraUserParameterBindingCustomization::GetCurrentText() const
{
	if (IsValid() && TargetUserParameterBinding)
	{
		return FText::FromName(TargetUserParameterBinding->Parameter.GetName());
	}
	return FText::FromString(TEXT("Missing"));
}

FText FNiagaraUserParameterBindingCustomization::GetTooltipText() const
{
	if (IsValid() && TargetUserParameterBinding && TargetUserParameterBinding->Parameter.IsValid())
	{
		FText TooltipDesc = FText::Format(LOCTEXT("UserParameterBindingTooltip", "Bound to the user parameter \"{0}\""), FText::FromName(TargetUserParameterBinding->Parameter.GetName()));
		return TooltipDesc;
	}
	return FText::FromString(TEXT("Missing"));
}

TSharedRef<SWidget> FNiagaraUserParameterBindingCustomization::OnGetMenuContent() const
{
	FGraphActionMenuBuilder MenuBuilder;

	return SNew(SBorder)
		.Padding(5)
		[
			SNew(SBox)
			[
				SNew(SGraphActionMenu)
				.OnActionSelected(const_cast<FNiagaraUserParameterBindingCustomization*>(this), &FNiagaraUserParameterBindingCustomization::OnActionSelected)
				.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(const_cast<FNiagaraUserParameterBindingCustomization*>(this), &FNiagaraUserParameterBindingCustomization::OnCreateWidgetForAction))
				.OnCollectAllActions(const_cast<FNiagaraUserParameterBindingCustomization*>(this), &FNiagaraUserParameterBindingCustomization::CollectAllActions)
				.AutoExpandActionMenu(false)
				.ShowFilterTextBox(true)
			]
		];
}

TArray<FName> FNiagaraUserParameterBindingCustomization::GetNames() const
{
	TArray<FName> Names;

	if (IsValid() && TargetUserParameterBinding)
	{
		UNiagaraSystem* BaseSystem = BaseSystemWeakPtr.Get();
		for (const FNiagaraVariable Var : BaseSystem->GetExposedParameters().ReadParameterVariables())
		{
			if (FNiagaraParameterMapHistory::IsUserParameter(Var) && Var.GetType() == TargetUserParameterBinding->Parameter.GetType())
			{
				Names.AddUnique(Var.GetName());
			}
		}
	}
	
	return Names;
}

void FNiagaraUserParameterBindingCustomization::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	TArray<FName> UserParamNames = GetNames();
	for (FName UserParamName : UserParamNames)
	{
		FText CategoryName = FText();
		FString DisplayNameString = FName::NameToDisplayString(UserParamName.ToString(), false);
		const FText NameText = FText::FromString(DisplayNameString);
		const FText TooltipDesc = FText::Format(LOCTEXT("BindToUserParameter", "Bind to the User Parameter \"{0}\" "), FText::FromString(DisplayNameString));
		TSharedPtr<FNiagaraStackAssetAction_VarBind> NewNodeAction(new FNiagaraStackAssetAction_VarBind(UserParamName, CategoryName, NameText,
			TooltipDesc, 0, FText()));
		OutAllActions.AddAction(NewNodeAction);
	}
}

TSharedRef<SWidget> FNiagaraUserParameterBindingCustomization::OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SNiagaraParameterName)
			.ParameterName(((FNiagaraStackAssetAction_VarBind* const)InCreateData->Action.Get())->VarName)
			.IsReadOnly(true)
			//SNew(STextBlock)
			//.Text(InCreateData->Action->GetMenuDescription())
			.ToolTipText(InCreateData->Action->GetTooltipDescription())
		];
}


void FNiagaraUserParameterBindingCustomization::OnActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedActions, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedActions.Num() == 0)
	{
		for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
		{
			TSharedPtr<FEdGraphSchemaAction> CurrentAction = SelectedActions[ActionIndex];

			if (CurrentAction.IsValid())
			{
				FSlateApplication::Get().DismissAllMenus();
				FNiagaraStackAssetAction_VarBind* EventSourceAction = (FNiagaraStackAssetAction_VarBind*)CurrentAction.Get();
				ChangeSource(EventSourceAction->VarName);
			}
		}
	}
}

void FNiagaraUserParameterBindingCustomization::ChangeSource(FName InVarName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("ChangeUserParameterSource", " Change User Parameter Source to \"{0}\" "), FText::FromName(InVarName)));
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	for (UObject* Obj : Objects)
	{
		Obj->Modify();
	}

	PropertyHandle->NotifyPreChange();
	TargetUserParameterBinding->Parameter.SetName(InVarName);
	//TargetUserParameterBinding->Parameter.SetType(FNiagaraTypeDefinition::GetUObjectDef()); Do not override the type here!
	//TargetVariableBinding->DataSetVariable = FNiagaraConstants::GetAttributeAsDataSetKey(TargetVariableBinding->BoundVariable);
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}

void FNiagaraUserParameterBindingCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	bool bAddDefault = true;

	ObjectCustomizingWeakPtr.Reset();
	BaseSystemWeakPtr.Reset();

	if (Objects.Num() == 1)
	{
		UNiagaraSystem* BaseSystem = Objects[0]->GetTypedOuter<UNiagaraSystem>();
		if (BaseSystem == nullptr)
		{
			if (UNiagaraDataInterface* ObjectAsDataInterface = Cast<UNiagaraDataInterface>(Objects[0]))
			{
				FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
				FVersionedNiagaraEmitter Emitter;
				NiagaraEditorModule.GetTargetSystemAndEmitterForDataInterface(ObjectAsDataInterface, BaseSystem, Emitter);
			}
		}
		
		if (BaseSystem)
		{
			ObjectCustomizingWeakPtr = Objects[0];
			BaseSystemWeakPtr = BaseSystem;

			TargetUserParameterBinding = (FNiagaraUserParameterBinding*)PropertyHandle->GetValueBaseAddress((uint8*)Objects[0]);

			HeaderRow
				.NameContent()
				[
					PropertyHandle->CreatePropertyNameWidget()
				]
				.ValueContent()
				.MaxDesiredWidth(200.f)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &FNiagaraUserParameterBindingCustomization::OnGetMenuContent)
					.ContentPadding(1)
					.ToolTipText(this, &FNiagaraUserParameterBindingCustomization::GetTooltipText)
					.ButtonContent()
					[

						SNew(SNiagaraParameterName)
						.ParameterName(this, &FNiagaraUserParameterBindingCustomization::GetVariableName)
						.IsReadOnly(true)
					]
				];

			bAddDefault = false;
		}
	}

	if (bAddDefault)
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
	}
}

//////////////////////////////////////////////////////////////////////////
FName FNiagaraMaterialAttributeBindingCustomization::GetNiagaraVariableName() const
{
	if (BaseSystem && TargetParameterBinding)
	{
		return TargetParameterBinding->NiagaraVariable.GetName();
	}
	return FName();
}

FName FNiagaraMaterialAttributeBindingCustomization::GetNiagaraChildVariableName() const
{
	if (BaseSystem && TargetParameterBinding)
	{
		return TargetParameterBinding->NiagaraChildVariable.GetName();
	}
	return FName();
}

FText FNiagaraMaterialAttributeBindingCustomization::GetNiagaraCurrentText() const
{
	if (BaseSystem && TargetParameterBinding)
	{
		return MakeCurrentText(TargetParameterBinding->NiagaraVariable, TargetParameterBinding->NiagaraChildVariable);
	}
	return FText::FromString(TEXT("Missing"));
}


FText FNiagaraMaterialAttributeBindingCustomization::MakeCurrentText(const FNiagaraVariableBase& BaseVar, const FNiagaraVariableBase& ChildVar) 
{
	if (BaseVar.GetName().IsNone())
	{
		return FText::FromName(NAME_None);
	}

	FString DisplayNameString = FName::NameToDisplayString(BaseVar.GetName().ToString(), false);
	FNiagaraTypeDefinition TargetType = BaseVar.GetType();
	if (ChildVar.GetName() != NAME_None)
	{
		DisplayNameString += TEXT(" \"") + FName::NameToDisplayString(ChildVar.GetName().ToString(), false) + TEXT("\"");
		TargetType = ChildVar.GetType();
	}

	DisplayNameString += TEXT(" (") + FName::NameToDisplayString(TargetType.GetFName().ToString(), false) + TEXT(")");

	const FText NameText = FText::FromString(DisplayNameString);
	return NameText;
}

FText FNiagaraMaterialAttributeBindingCustomization::GetNiagaraTooltipText() const
{
	if (BaseSystem && TargetParameterBinding && TargetParameterBinding->NiagaraVariable.IsValid())
	{
		FText TooltipDesc = FText::Format(LOCTEXT("MaterialAttributeBindingTooltip", "Bound to the parameter \"{0}\""), MakeCurrentText(TargetParameterBinding->NiagaraVariable, TargetParameterBinding->NiagaraChildVariable));
		return TooltipDesc;
	}
	return FText::FromString(TEXT("Missing"));
}

TSharedRef<SWidget> FNiagaraMaterialAttributeBindingCustomization::OnGetNiagaraMenuContent() const
{
	FGraphActionMenuBuilder MenuBuilder;

	TSharedRef<SGraphActionMenu> GraphActionMenu = SNew(SGraphActionMenu)
		.OnActionSelected(const_cast<FNiagaraMaterialAttributeBindingCustomization*>(this), &FNiagaraMaterialAttributeBindingCustomization::OnNiagaraActionSelected)
		.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(const_cast<FNiagaraMaterialAttributeBindingCustomization*>(this), &FNiagaraMaterialAttributeBindingCustomization::OnCreateWidgetForNiagaraAction))
		.OnCollectAllActions(const_cast<FNiagaraMaterialAttributeBindingCustomization*>(this), &FNiagaraMaterialAttributeBindingCustomization::CollectAllNiagaraActions)
		.AutoExpandActionMenu(false)
		.ShowFilterTextBox(true);
	
	TSharedRef<SWidget> MenuContent =  SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			[
				GraphActionMenu
			]
		];

	NiagaraParameterButton->SetMenuContentWidgetToFocus(GraphActionMenu->GetFilterTextBox());
	return MenuContent;
}


bool FNiagaraMaterialAttributeBindingCustomization::IsCompatibleNiagaraVariable(const FNiagaraVariable& InVar) const
{
	{

		if (InVar.GetType() == FNiagaraTypeDefinition::GetFloatDef() ||
			InVar.GetType() == FNiagaraTypeDefinition::GetVec4Def() ||
			InVar.GetType() == FNiagaraTypeDefinition::GetColorDef() ||
			InVar.GetType() == FNiagaraTypeDefinition::GetVec2Def() ||
			InVar.GetType() == FNiagaraTypeDefinition::GetVec3Def() ||
			InVar.GetType() == FNiagaraTypeDefinition::GetPositionDef() ||
			InVar.GetType() == FNiagaraTypeDefinition::GetUObjectDef() ||
			InVar.GetType() == FNiagaraTypeDefinition::GetUTextureDef() ||
			InVar.GetType() == FNiagaraTypeDefinition::GetUTextureRenderTargetDef() )
		{
			return true;
		}
		else if (InVar.GetType().IsDataInterface())
		{
			return true;
		}
	}
	return false;
}

TArray<TPair<FNiagaraVariableBase, FNiagaraVariableBase> > FNiagaraMaterialAttributeBindingCustomization::GetNiagaraNames() const
{
	TArray<TPair<FNiagaraVariableBase, FNiagaraVariableBase>> Names;
	TArray < FNiagaraVariableBase > BaseVars;

	if (BaseSystem && BaseEmitter.Emitter && TargetParameterBinding)
	{
		bool bSystem = true;
		bool bEmitter = true;
		bool bParticles = false;
		bool bUser = true;
		bool bStatic = false;
		BaseVars = FNiagaraStackAssetAction_VarBind::FindVariables(BaseEmitter, bSystem, bEmitter, bParticles, bUser, bStatic);

		TArray<UNiagaraScript*> Scripts;
		Scripts.Add(BaseSystem->GetSystemUpdateScript());
		Scripts.Add(BaseSystem->GetSystemSpawnScript());
		BaseEmitter.GetEmitterData()->GetScripts(Scripts, false);

		TMap<FString, FString> EmitterAlias;
		EmitterAlias.Emplace(FNiagaraConstants::EmitterNamespace.ToString(), BaseEmitter.Emitter->GetUniqueEmitterName());

		auto FindCachedDI = 
			[&](const FNiagaraVariableBase& BaseVariable) -> UNiagaraDataInterface*
			{
				FName VariableName = BaseVariable.GetName();
				if (BaseVariable.IsInNameSpace(FNiagaraConstants::EmitterNamespaceString))
				{
					VariableName = FNiagaraUtilities::ResolveAliases(BaseVariable, FNiagaraAliasContext()
						.ChangeEmitterToEmitterName(BaseEmitter.Emitter->GetUniqueEmitterName())).GetName();
				}

				for (UNiagaraScript* Script : Scripts)
				{
					TArray<FNiagaraScriptDataInterfaceInfo>& CachedDIs = Script->GetCachedDefaultDataInterfaces();
					for (const FNiagaraScriptDataInterfaceInfo& Info : CachedDIs)
					{
						if (Info.RegisteredParameterMapWrite == VariableName)
						{
							return Info.DataInterface;
						}
					}
				}

				return BaseVariable.GetType().GetClass()->GetDefaultObject<UNiagaraDataInterface>();
			};

		for (const FNiagaraVariableBase& BaseVar : BaseVars)
		{
			if (BaseVar.IsDataInterface())
			{
				UNiagaraDataInterface* DI = FindCachedDI(BaseVar);
				if (DI && DI->CanExposeVariables())
				{
					TArray<FNiagaraVariableBase> ChildVars;
					DI->GetExposedVariables(ChildVars);
					for (const FNiagaraVariableBase& ChildVar : ChildVars)
					{
						if (IsCompatibleNiagaraVariable(ChildVar))
						{
							Names.AddUnique(TPair<FNiagaraVariableBase, FNiagaraVariableBase>(BaseVar, ChildVar));
						}
					}
				}
			}
			else if (IsCompatibleNiagaraVariable(BaseVar))
			{
				if (RenderProps && TargetParameterBinding && RenderProps->IsSupportedVariableForBinding(BaseVar, TargetParameterBinding->MaterialParameterName))
				{
					Names.AddUnique(TPair<FNiagaraVariableBase, FNiagaraVariableBase>(BaseVar, FNiagaraVariableBase()));
				}
			}
		}
	}

	return Names;
}

void FNiagaraMaterialAttributeBindingCustomization::CollectAllNiagaraActions(FGraphActionListBuilderBase& OutAllActions)
{
	TArray<TPair<FNiagaraVariableBase, FNiagaraVariableBase>> ParamNames = GetNiagaraNames();
	for (TPair<FNiagaraVariableBase, FNiagaraVariableBase> ParamPair : ParamNames)
	{
		FText CategoryName = FText();
		const FText NameText = MakeCurrentText(ParamPair.Key, ParamPair.Value);
		const FText TooltipDesc = FText::Format(LOCTEXT("BindToNiagaraParameter", "Bind to the Niagara Variable \"{0}\" "), NameText);
		FNiagaraStackAssetAction_VarBind* VarBind = new FNiagaraStackAssetAction_VarBind(ParamPair.Key.GetName(), CategoryName, NameText,
			TooltipDesc, 0, FText());
		VarBind->BaseVar = ParamPair.Key;
		VarBind->ChildVar = ParamPair.Value;
		TSharedPtr<FNiagaraStackAssetAction_VarBind> NewNodeAction(VarBind);
		OutAllActions.AddAction(NewNodeAction);
	}
}


FText FNiagaraMaterialAttributeBindingCustomization::GetNiagaraChildVariableText() const
{

	FName ChildVarName = GetNiagaraChildVariableName();
	FText ChildVarNameText = ChildVarName.IsNone() == false ? FText::FromString(TEXT("| ") + ChildVarName.ToString()) : FText::GetEmpty();
	return ChildVarNameText;
}

EVisibility FNiagaraMaterialAttributeBindingCustomization::GetNiagaraChildVariableVisibility() const
{
	FName ChildVarName = GetNiagaraChildVariableName();
	return ChildVarName.IsNone() ? EVisibility::Collapsed : EVisibility::Visible;
}

TSharedRef<SWidget> FNiagaraMaterialAttributeBindingCustomization::OnCreateWidgetForNiagaraAction(struct FCreateWidgetForActionData* const InCreateData)
{
	FName ChildVarName = (((FNiagaraStackAssetAction_VarBind* const)InCreateData->Action.Get())->ChildVar.GetName());
	FText ChildVarNameText = ChildVarName.IsNone() == false ? FText::FromString(TEXT("| ") + ChildVarName.ToString()) : FText::GetEmpty();
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 0)
			[
				SNew(SNiagaraParameterName)
				.ParameterName(((FNiagaraStackAssetAction_VarBind* const)InCreateData->Action.Get())->VarName)
				.IsReadOnly(true)
				//SNew(STextBlock)
				//.Text(InCreateData->Action->GetMenuDescription())
				.ToolTipText(InCreateData->Action->GetTooltipDescription())
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(3, 0)
			[
				SNew(STextBlock)
				.Visibility(ChildVarName.IsNone() ? EVisibility::Collapsed : EVisibility::Visible)
				.Text(ChildVarNameText)
			]
		];
}


void FNiagaraMaterialAttributeBindingCustomization::OnNiagaraActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedActions, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedActions.Num() == 0)
	{
		for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
		{
			TSharedPtr<FEdGraphSchemaAction> CurrentAction = SelectedActions[ActionIndex];

			if (CurrentAction.IsValid())
			{
				FSlateApplication::Get().DismissAllMenus();
				FNiagaraStackAssetAction_VarBind* EventSourceAction = (FNiagaraStackAssetAction_VarBind*)CurrentAction.Get();
				ChangeNiagaraSource(EventSourceAction);
			}
		}
	}
}

void FNiagaraMaterialAttributeBindingCustomization::ChangeNiagaraSource(FNiagaraStackAssetAction_VarBind* InVar)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("ChangeParameterSource", " Change Parameter Source to \"{0}\" "), FText::FromName(InVar->VarName)));
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	for (UObject* Obj : Objects)
	{
		Obj->Modify();
	}

	PropertyHandle->NotifyPreChange();
	TargetParameterBinding->NiagaraVariable = InVar->BaseVar;
	TargetParameterBinding->NiagaraChildVariable = InVar->ChildVar;
	TargetParameterBinding->CacheValues(BaseEmitter.Emitter);
	//TargetParameterBinding->Parameter.SetType(FNiagaraTypeDefinition::GetUObjectDef()); Do not override the type here!
	//TargetVariableBinding->DataSetVariable = FNiagaraConstants::GetAttributeAsDataSetKey(TargetVariableBinding->BoundVariable);
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}

FText FNiagaraMaterialAttributeBindingCustomization::GetMaterialCurrentText() const
{
	if (BaseSystem && TargetParameterBinding)
	{
		return FText::FromName(TargetParameterBinding->MaterialParameterName);
	}
	return FText::FromString(TEXT("Missing"));
}

FText FNiagaraMaterialAttributeBindingCustomization::GetMaterialTooltipText() const
{
	if (BaseSystem && TargetParameterBinding && TargetParameterBinding->MaterialParameterName.IsValid())
	{
		FText TooltipDesc = FText::Format(LOCTEXT("MaterialParameterBindingTooltip", "Bound to the parameter \"{0}\""), FText::FromName(TargetParameterBinding->MaterialParameterName));
		return TooltipDesc;
	}
	return FText::FromString(TEXT("Missing"));
}

TSharedRef<SWidget> FNiagaraMaterialAttributeBindingCustomization::OnGetMaterialMenuContent() const
{
	FGraphActionMenuBuilder MenuBuilder;

	TSharedRef<SGraphActionMenu> GraphActionMenu = SNew(SGraphActionMenu)
		.OnActionSelected(const_cast<FNiagaraMaterialAttributeBindingCustomization*>(this), &FNiagaraMaterialAttributeBindingCustomization::OnMaterialActionSelected)
		.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(const_cast<FNiagaraMaterialAttributeBindingCustomization*>(this), &FNiagaraMaterialAttributeBindingCustomization::OnCreateWidgetForMaterialAction))
		.OnCollectAllActions(const_cast<FNiagaraMaterialAttributeBindingCustomization*>(this), &FNiagaraMaterialAttributeBindingCustomization::CollectAllMaterialActions)
		.AutoExpandActionMenu(false)
		.ShowFilterTextBox(true);

	TSharedRef<SWidget> MenuContent = SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			[
				GraphActionMenu
			]
		];

	MaterialParameterButton->SetMenuContentWidgetToFocus(GraphActionMenu->GetFilterTextBox());
	return MenuContent;
}

TArray<FName> FNiagaraMaterialAttributeBindingCustomization::GetMaterialNames() const
{
	TArray<FName> Names;

	if (BaseSystem && TargetParameterBinding && PropertyHandle)
	{
		TArray<UObject*> Objects;
		PropertyHandle->GetOuterObjects(Objects);

		if (Objects.Num() == 1)
		{
			UNiagaraRendererProperties* RendererProperties = Cast<UNiagaraRendererProperties>(Objects[0]);
			TArray<UMaterialInterface*> Materials;
			if (RendererProperties)
			{
				RendererProperties->GetUsedMaterials(nullptr, Materials);
			}

			TArray<FMaterialParameterInfo> ParameterInfo;
			for (UMaterialInterface* Material : Materials)
			{
				if (!Material)
				{
					continue;
				}

				{
					TArray<FMaterialParameterInfo> LocalParameterInfo;
					TArray<FGuid> ParameterIds;
					Material->GetAllTextureParameterInfo(LocalParameterInfo, ParameterIds);
					ParameterInfo.Append(LocalParameterInfo);
				}
				{
					TArray<FMaterialParameterInfo> LocalParameterInfo;
					TArray<FGuid> ParameterIds;
					Material->GetAllScalarParameterInfo(LocalParameterInfo, ParameterIds);
					ParameterInfo.Append(LocalParameterInfo);
				}
				{
					TArray<FMaterialParameterInfo> LocalParameterInfo;
					TArray<FGuid> ParameterIds;
					Material->GetAllVectorParameterInfo(LocalParameterInfo, ParameterIds);
					ParameterInfo.Append(LocalParameterInfo);
				}
			}

			for (const FMaterialParameterInfo& Var : ParameterInfo)
			{
				Names.AddUnique(Var.Name);
			}
		}
	}

	return Names;
}

void FNiagaraMaterialAttributeBindingCustomization::CollectAllMaterialActions(FGraphActionListBuilderBase& OutAllActions)
{
	TArray<FName> ParamNames = GetMaterialNames();
	for (FName ParamName : ParamNames)
	{
		FText CategoryName = FText();
		FString DisplayNameString = FName::NameToDisplayString(ParamName.ToString(), false);
		const FText NameText = FText::FromString(DisplayNameString);
		const FText TooltipDesc = FText::Format(LOCTEXT("BindToMaterialParameter", "Bind to the Material Variable \"{0}\" "), FText::FromString(DisplayNameString));
		TSharedPtr<FNiagaraStackAssetAction_VarBind> NewNodeAction(new FNiagaraStackAssetAction_VarBind(ParamName, CategoryName, NameText,
			TooltipDesc, 0, FText()));
		OutAllActions.AddAction(NewNodeAction);
	}
}

TSharedRef<SWidget> FNiagaraMaterialAttributeBindingCustomization::OnCreateWidgetForMaterialAction(struct FCreateWidgetForActionData* const InCreateData)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(InCreateData->Action->GetMenuDescription())
		.ToolTipText(InCreateData->Action->GetTooltipDescription())
		];
}


void FNiagaraMaterialAttributeBindingCustomization::OnMaterialActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedActions, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedActions.Num() == 0)
	{
		for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
		{
			TSharedPtr<FEdGraphSchemaAction> CurrentAction = SelectedActions[ActionIndex];

			if (CurrentAction.IsValid())
			{
				FSlateApplication::Get().DismissAllMenus();
				FNiagaraStackAssetAction_VarBind* EventSourceAction = (FNiagaraStackAssetAction_VarBind*)CurrentAction.Get();
				ChangeMaterialSource(EventSourceAction->VarName);
			}
		}
	}
}

void FNiagaraMaterialAttributeBindingCustomization::ChangeMaterialSource(FName InVarName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("ChangeParameterSource", " Change Parameter Source to \"{0}\" "), FText::FromName(InVarName)));
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	for (UObject* Obj : Objects)
	{
		Obj->Modify();
	}

	PropertyHandle->NotifyPreChange();
	TargetParameterBinding->MaterialParameterName = InVarName;
	//TargetParameterBinding->Parameter.SetType(FMaterialTypeDefinition::GetUObjectDef()); Do not override the type here!
	//TargetVariableBinding->DataSetVariable = FMaterialConstants::GetAttributeAsDataSetKey(TargetVariableBinding->BoundVariable);
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}

void FNiagaraMaterialAttributeBindingCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	bool bAddDefault = true;
	

	if (bAddDefault)
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
				.Text(LOCTEXT("ParamHeaderValue", "Binding"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			];
	}
}


void FNiagaraMaterialAttributeBindingCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	RenderProps = nullptr;
	BaseSystem =  nullptr;
	BaseEmitter = FVersionedNiagaraEmitter();
	  
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	if (Objects.Num() == 1)
	{
		RenderProps = Cast<UNiagaraRendererProperties>(Objects[0]);
		BaseSystem = Objects[0]->GetTypedOuter<UNiagaraSystem>();
		if (RenderProps)
		{
			BaseEmitter = RenderProps->GetOuterEmitter();
		}
		if (BaseSystem)
		{
			TargetParameterBinding = (FNiagaraMaterialAttributeBinding*)PropertyHandle->GetValueBaseAddress((uint8*)Objects[0]);

			TSharedPtr<IPropertyHandle> ChildPropertyHandle = StructPropertyHandle->GetChildHandle(0);
			FDetailWidgetRow& RowMaterial = ChildBuilder.AddCustomRow(FText::GetEmpty());
			RowMaterial
				.NameContent()
				[
					ChildPropertyHandle->CreatePropertyNameWidget()
				]
			.ValueContent()
				.MaxDesiredWidth(200.f)
				[
					SAssignNew(MaterialParameterButton, SComboButton)
					.OnGetMenuContent(this, &FNiagaraMaterialAttributeBindingCustomization::OnGetMaterialMenuContent)
					.ContentPadding(1)
					.ToolTipText(this, &FNiagaraMaterialAttributeBindingCustomization::GetMaterialTooltipText)
					.ButtonStyle(FAppStyle::Get(), "PropertyEditor.AssetComboStyle")
					.ForegroundColor(FAppStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(this, &FNiagaraMaterialAttributeBindingCustomization::GetMaterialCurrentText)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				];

			ChildPropertyHandle = StructPropertyHandle->GetChildHandle(1);
			FDetailWidgetRow& RowNiagara = ChildBuilder.AddCustomRow(FText::GetEmpty());
			RowNiagara
				.NameContent()
				[
					ChildPropertyHandle->CreatePropertyNameWidget()
				]
			.ValueContent()
				.MaxDesiredWidth(200.f)
				[
					SAssignNew(NiagaraParameterButton, SComboButton)
					.OnGetMenuContent(this, &FNiagaraMaterialAttributeBindingCustomization::OnGetNiagaraMenuContent)
					.ContentPadding(1)
					.ToolTipText(this, &FNiagaraMaterialAttributeBindingCustomization::GetNiagaraTooltipText)
					.ButtonStyle(FAppStyle::Get(), "PropertyEditor.AssetComboStyle")
					.ForegroundColor(FAppStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
					.ButtonContent()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(5, 0)
						[
							SNew(SNiagaraParameterName)
							.ParameterName(this, &FNiagaraMaterialAttributeBindingCustomization::GetNiagaraVariableName)
							.IsReadOnly(true)
						]
						+ SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.Padding(5, 0)
						[
							SNew(STextBlock)
							.Visibility(this, &FNiagaraMaterialAttributeBindingCustomization::GetNiagaraChildVariableVisibility)
							.Text(this, &FNiagaraMaterialAttributeBindingCustomization::GetNiagaraChildVariableText)
						]
					]
				];
		}
	}
}

//////////////////////////////////////////////////////////////////////////

FName FNiagaraDataInterfaceBindingCustomization::GetVariableName() const
{
	if (BaseStage && TargetDataInterfaceBinding)
	{
		return (TargetDataInterfaceBinding->BoundVariable.GetName());
	}
	return FName();
}

FText FNiagaraDataInterfaceBindingCustomization::GetCurrentText() const
{
	if (BaseStage && TargetDataInterfaceBinding)
	{
		return FText::FromName(TargetDataInterfaceBinding->BoundVariable.GetName());
	}
	return FText::FromString(TEXT("Missing"));
}

FText FNiagaraDataInterfaceBindingCustomization::GetTooltipText() const
{
	if (BaseStage && TargetDataInterfaceBinding && TargetDataInterfaceBinding->BoundVariable.IsValid())
	{
		FText TooltipDesc = FText::Format(LOCTEXT("DataInterfaceBindingTooltip", "Bound to the user parameter \"{0}\""), FText::FromName(TargetDataInterfaceBinding->BoundVariable.GetName()));
		return TooltipDesc;
	}
	return FText::FromString(TEXT("Missing"));
}

TSharedRef<SWidget> FNiagaraDataInterfaceBindingCustomization::OnGetMenuContent() const
{
	FGraphActionMenuBuilder MenuBuilder;

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			[
				SNew(SGraphActionMenu)
				.OnActionSelected(const_cast<FNiagaraDataInterfaceBindingCustomization*>(this), &FNiagaraDataInterfaceBindingCustomization::OnActionSelected)
				.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(const_cast<FNiagaraDataInterfaceBindingCustomization*>(this), &FNiagaraDataInterfaceBindingCustomization::OnCreateWidgetForAction))
				.OnCollectAllActions(const_cast<FNiagaraDataInterfaceBindingCustomization*>(this), &FNiagaraDataInterfaceBindingCustomization::CollectAllActions)
				.AutoExpandActionMenu(false)
				.ShowFilterTextBox(true)
			]
		];
}

TArray<FName> FNiagaraDataInterfaceBindingCustomization::GetNames() const
{
	TArray<FName> Names;

	if (BaseStage && TargetDataInterfaceBinding)
	{
		if (FVersionedNiagaraEmitterData* EmitterData = BaseStage->GetEmitterData())
		{
			// Find all used emitter and particle data interface variables that can be iterated upon.
			TArray<UNiagaraScript*> AllScripts;
			EmitterData->GetScripts(AllScripts, false);

			TArray<UNiagaraGraph*> Graphs;
			for (const UNiagaraScript* Script : AllScripts)
			{
				if (const UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(Script->GetLatestSource()))
				{
					Graphs.AddUnique(Source->NodeGraph);
				}
			}

			for (const UNiagaraGraph* Graph : Graphs)
			{
				TMap<FNiagaraVariable, FNiagaraGraphParameterReferenceCollection> ParameterReferenceMap = Graph->GetParameterReferenceMap();
				ParameterReferenceMap.KeySort([](const FNiagaraVariable& A, const FNiagaraVariable& B) { return A.GetName().LexicalLess(B.GetName()); });
				for (const auto& ParameterToReferences : ParameterReferenceMap)
				{
					const FNiagaraVariable& ParameterVariable = ParameterToReferences.Key;
					FNiagaraParameterHandle Handle(ParameterVariable.GetName());
					bool bIsValidNamespace = Handle.IsEmitterHandle() || Handle.IsSystemHandle() || Handle.IsOutputHandle() || Handle.IsParticleAttributeHandle() || Handle.IsTransientHandle() || Handle.IsStackContextHandle();
					if (ParameterVariable.IsDataInterface() && bIsValidNamespace)
					{
						const UClass* Class = ParameterVariable.GetType().GetClass();
						if (Class)
						{
							const UObject* DefaultObjDI = Class->GetDefaultObject();
							if (DefaultObjDI != nullptr && DefaultObjDI->IsA<UNiagaraDataInterfaceRWBase>())
							{
								Names.AddUnique(ParameterVariable.GetName());
							}
						}
					}
				}
			}
		}
	}

	return Names;
}

void FNiagaraDataInterfaceBindingCustomization::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	TArray<FName> UserParamNames = GetNames();
	for (FName UserParamName : UserParamNames)
	{
		FText CategoryName = FText();
		FString DisplayNameString = FName::NameToDisplayString(UserParamName.ToString(), false);
		const FText NameText = FText::FromString(DisplayNameString);
		const FText TooltipDesc = FText::Format(LOCTEXT("BindToDataInterface", "Bind to the User Parameter \"{0}\" "), FText::FromString(DisplayNameString));
		TSharedPtr<FNiagaraStackAssetAction_VarBind> NewNodeAction(new FNiagaraStackAssetAction_VarBind(UserParamName, CategoryName, NameText,
			TooltipDesc, 0, FText()));
		OutAllActions.AddAction(NewNodeAction);
	}
}

TSharedRef<SWidget> FNiagaraDataInterfaceBindingCustomization::OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SNiagaraParameterName)
			.ParameterName(((FNiagaraStackAssetAction_VarBind* const)InCreateData->Action.Get())->VarName)
			.IsReadOnly(true)
			//SNew(STextBlock)
			//.Text(InCreateData->Action->GetMenuDescription())
			.ToolTipText(InCreateData->Action->GetTooltipDescription())
		];
}


void FNiagaraDataInterfaceBindingCustomization::OnActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedActions, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedActions.Num() == 0)
	{
		for (int32 ActionIndex = 0; ActionIndex < SelectedActions.Num(); ActionIndex++)
		{
			TSharedPtr<FEdGraphSchemaAction> CurrentAction = SelectedActions[ActionIndex];

			if (CurrentAction.IsValid())
			{
				FSlateApplication::Get().DismissAllMenus();
				FNiagaraStackAssetAction_VarBind* EventSourceAction = (FNiagaraStackAssetAction_VarBind*)CurrentAction.Get();
				ChangeSource(EventSourceAction->VarName);
			}
		}
	}
}

void FNiagaraDataInterfaceBindingCustomization::ChangeSource(FName InVarName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("ChangeDataParameterSource", " Change Data Interface Source to \"{0}\" "), FText::FromName(InVarName)));
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	for (UObject* Obj : Objects)
	{
		Obj->Modify();
	}

	PropertyHandle->NotifyPreChange();
	TargetDataInterfaceBinding->BoundVariable.SetName(InVarName);
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}

void FNiagaraDataInterfaceBindingCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	PropertyHandle = InPropertyHandle;
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);

	PropertyHandle->MarkResetToDefaultCustomized(true);

	bool bAddDefault = true;
	if (Objects.Num() == 1)
	{
		BaseStage = Cast<UNiagaraSimulationStageBase>(Objects[0]);
		if (BaseStage)
		{
			TargetDataInterfaceBinding = (FNiagaraVariableDataInterfaceBinding*)PropertyHandle->GetValueBaseAddress((uint8*)Objects[0]);

			HeaderRow
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MaxDesiredWidth(250.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &FNiagaraDataInterfaceBindingCustomization::OnGetMenuContent)
					.ContentPadding(1)
					.ToolTipText(this, &FNiagaraDataInterfaceBindingCustomization::GetTooltipText)
					.ButtonStyle(FAppStyle::Get(), "PropertyEditor.AssetComboStyle")
					.ForegroundColor(FAppStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
					.ButtonContent()
					[
						SNew(SNiagaraParameterName)
						.ParameterName(this, &FNiagaraDataInterfaceBindingCustomization::GetVariableName)
						.IsReadOnly(true)
					]
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
					.Visibility(this, &FNiagaraDataInterfaceBindingCustomization::IsResetToDefaultsVisible)
					.OnClicked(this, &FNiagaraDataInterfaceBindingCustomization::OnResetToDefaultsClicked)
					.Content()
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			];
			bAddDefault = false;
		}
	}


	if (bAddDefault)
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
	}
}

EVisibility FNiagaraDataInterfaceBindingCustomization::IsResetToDefaultsVisible() const
{
	return TargetDataInterfaceBinding->BoundVariable.IsValid() ? EVisibility::Visible : EVisibility::Hidden;
}

FReply FNiagaraDataInterfaceBindingCustomization::OnResetToDefaultsClicked()
{
	ChangeSource(NAME_None);

	return FReply::Handled();
}

////////////////////////
FName FNiagaraScriptVariableBindingCustomization::GetVariableName() const
{
	if (TargetVariableBinding && TargetVariableBinding->IsValid())
	{
		return (TargetVariableBinding->Name);
	}
	return FName();
}

FText FNiagaraScriptVariableBindingCustomization::GetCurrentText() const
{
	if (TargetVariableBinding && TargetVariableBinding->IsValid())
	{
		return FText::FromName(TargetVariableBinding->Name);
	}
	return FText::FromString(TEXT("None"));
}

FText FNiagaraScriptVariableBindingCustomization::GetTooltipText() const
{
	if (TargetVariableBinding && TargetVariableBinding->IsValid())
	{
		FText TooltipDesc = FText::Format(LOCTEXT("BindingTooltip", "Use the variable \"{0}\" if it is defined, otherwise use the type's default value."), FText::FromName(TargetVariableBinding->Name));
		return TooltipDesc;
	}
	return FText::FromString(TEXT("There is no default binding selected."));
}

TSharedRef<SWidget> FNiagaraScriptVariableBindingCustomization::OnGetMenuContent() const
{
	FGraphActionMenuBuilder MenuBuilder; // TODO: Is this necessary? It's included in all the other implementations above, but it's never used. Spooky

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		.Padding(5)
		[
			SNew(SBox)
			[
				SNew(SGraphActionMenu)
				.OnActionSelected(const_cast<FNiagaraScriptVariableBindingCustomization*>(this), &FNiagaraScriptVariableBindingCustomization::OnActionSelected)
				.OnCreateWidgetForAction(SGraphActionMenu::FOnCreateWidgetForAction::CreateSP(const_cast<FNiagaraScriptVariableBindingCustomization*>(this), &FNiagaraScriptVariableBindingCustomization::OnCreateWidgetForAction))
				.OnCollectAllActions(const_cast<FNiagaraScriptVariableBindingCustomization*>(this), &FNiagaraScriptVariableBindingCustomization::CollectAllActions)
				.AutoExpandActionMenu(false)
				.ShowFilterTextBox(true)
			]
		];
}

TArray<TSharedPtr<FEdGraphSchemaAction>> FNiagaraScriptVariableBindingCustomization::GetGraphParameterBindingActions(const UNiagaraGraph* Graph)
{
	TArray<FName> Names;
	TArray<TSharedPtr<FEdGraphSchemaAction>> OutActions;

	for (const FNiagaraParameterMapHistory& History : UNiagaraNodeParameterMapBase::GetParameterMaps(Graph))
	{
		for (const FNiagaraVariable& Var : History.Variables)
		{
			FString Namespace = FNiagaraParameterMapHistory::GetNamespace(Var);
			if (Namespace == TEXT("Module."))
			{
				// TODO: Skip module inputs for now. Does it make sense to bind module inputs to module inputs?
				continue;
			}
			if (Var.GetType() == BaseScriptVariable->Variable.GetType())
			{
				Names.AddUnique(Var.GetName());
			}
		}
	}

	for (const auto& Var : Graph->GetParameterReferenceMap())
	{
		FString Namespace = FNiagaraParameterMapHistory::GetNamespace(Var.Key);
		if (Namespace == TEXT("Module."))
		{
			// TODO: Skip module inputs for now. Does it make sense to bind module inputs to module inputs?
			continue;
		}
		if (Var.Key.GetType() == BaseScriptVariable->Variable.GetType())
		{
			Names.AddUnique(Var.Key.GetName());
		}
	}
	
	for (const FName& Name : Names)
	{
		const FText NameText = FText::FromName(Name);
		const FText TooltipDesc = FText::Format(LOCTEXT("SetFunctionPopupTooltip", "Use the variable \"{0}\" "), NameText);
		TSharedPtr<FNiagaraStackAssetAction_VarBind> NewNodeAction(
			new FNiagaraStackAssetAction_VarBind(Name, FText(), NameText, TooltipDesc, 0, FText())
		);
		OutActions.Add(NewNodeAction);
	}

	return OutActions;
}

TArray<TSharedPtr<FEdGraphSchemaAction>> FNiagaraScriptVariableBindingCustomization::GetEngineConstantBindingActions()
{
	TArray<TSharedPtr<FEdGraphSchemaAction>> OutActions;

	for (const FNiagaraVariable& Parameter : FNiagaraConstants::GetEngineConstants())
	{
		if (BaseScriptVariable->Variable.GetType() != Parameter.GetType())
		{
			// types must match
			continue;
		}

		const FName& Name = Parameter.GetName();
		const FText NameText = FText::FromName(Name);
		const FText TooltipDesc = FText::Format(LOCTEXT("SetFunctionPopupTooltip", "Use the variable \"{0}\" "), NameText);

		TSharedPtr<FNiagaraStackAssetAction_VarBind> NewNodeAction(
			new FNiagaraStackAssetAction_VarBind(Name, FText(), NameText, TooltipDesc, 0, FText())
		);
		OutActions.Add(NewNodeAction);
	}

	return OutActions;
}

TArray<TSharedPtr<FEdGraphSchemaAction>> FNiagaraScriptVariableBindingCustomization::GetLibraryParameterBindingActions()
{
	TArray<TSharedPtr<FEdGraphSchemaAction>> OutActions;

	auto GetAvailableParameterDefinitions = [this]()->TArray<UNiagaraParameterDefinitions*> {
		const bool bSkipSubscribed = false;
		if (BaseGraph)
		{
			TSharedPtr<INiagaraParameterDefinitionsSubscriberViewModel> ParameterDefinitionsSubscriberViewModel = FNiagaraEditorUtilities::GetOwningLibrarySubscriberViewModelForGraph(BaseGraph);
			if (ParameterDefinitionsSubscriberViewModel.IsValid())
			{
				return ParameterDefinitionsSubscriberViewModel->GetAvailableParameterDefinitions(bSkipSubscribed);
			}
		}
		else if (BaseLibrary)
		{
			return BaseLibrary->GetAvailableParameterDefinitions(bSkipSubscribed);
		}
		checkf(false, TEXT("Script variable did not have a valid outer UNiagaraGraph or UNiagaraParameterDefinitions!"));
		return TArray<UNiagaraParameterDefinitions*>();
	};
	
	const TArray<UNiagaraParameterDefinitions*> AvailableParameterDefinitions = GetAvailableParameterDefinitions();
	for (UNiagaraParameterDefinitions* ParameterDefinitions : AvailableParameterDefinitions)
	{
		for(const UNiagaraScriptVariable* LibraryScriptVar : ParameterDefinitions->GetParametersConst())
		{ 
			if (BaseScriptVariable->Variable.GetType() != LibraryScriptVar->Variable.GetType())
			{
				// types must match
				continue;
			}
			else if (BaseScriptVariable->Metadata.GetVariableGuid() == LibraryScriptVar->Metadata.GetVariableGuid())
			{
				// do not bind to self
				continue;
			}

			const FName& Name = LibraryScriptVar->Variable.GetName();
			const FText NameText = FText::FromName(Name);
			const FText TooltipDesc = FText::Format(LOCTEXT("SetFunctionPopupTooltip", "Use the variable \"{0}\" "), NameText);

			FScriptVarBindingNameSubscriptionArgs SubscriptionArgs(ParameterDefinitions, LibraryScriptVar, BaseScriptVariable);

			TSharedPtr<FNiagaraStackAssetAction_VarBind> NewNodeAction(
				new FNiagaraStackAssetAction_VarBind(Name, FText(), NameText, TooltipDesc, 0, FText(), SubscriptionArgs)
			);
			OutActions.Add(NewNodeAction);
		}
	}

	return OutActions;
}

void FNiagaraScriptVariableBindingCustomization::CollectAllActions(FGraphActionListBuilderBase& OutAllActions)
{
	if (BaseGraph)
	{
		for (const TSharedPtr<FEdGraphSchemaAction>& Action : GetGraphParameterBindingActions(BaseGraph))
		{
			OutAllActions.AddAction(Action);
		}
		
	}

	for (const TSharedPtr<FEdGraphSchemaAction>& Action : GetEngineConstantBindingActions())
	{
		OutAllActions.AddAction(Action);
	}
	
	//@todo(ng) for now do not allow binding libraries to libraries as we do not handle fixing up the binding if the source library is removed.
	if(BaseGraph != nullptr || ALLOW_LIBRARY_TO_LIBRARY_DEFAULT_BINDING)
	{ 
		for (const TSharedPtr<FEdGraphSchemaAction>& Action : GetLibraryParameterBindingActions())
		{
			OutAllActions.AddAction(Action);
		}
	}
}

TSharedRef<SWidget> FNiagaraScriptVariableBindingCustomization::OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData)
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SNiagaraParameterName)
			.ParameterName(((FNiagaraStackAssetAction_VarBind* const)InCreateData->Action.Get())->VarName)
			.IsReadOnly(true)
			.ToolTipText(InCreateData->Action->GetTooltipDescription())
		];
}

void FNiagaraScriptVariableBindingCustomization::OnActionSelected(const TArray< TSharedPtr<FEdGraphSchemaAction> >& SelectedActions, ESelectInfo::Type InSelectionType)
{
	if (InSelectionType == ESelectInfo::OnMouseClick || InSelectionType == ESelectInfo::OnKeyPress || SelectedActions.Num() == 0)
	{
		for (auto& CurrentAction : SelectedActions)
		{
			if (CurrentAction.IsValid())
			{
				FSlateApplication::Get().DismissAllMenus();
				FNiagaraStackAssetAction_VarBind* EventSourceAction = (FNiagaraStackAssetAction_VarBind*)CurrentAction.Get();
				UNiagaraParameterDefinitions* SourceParameterDefinitions = EventSourceAction->LibraryNameSubscriptionArgs.SourceParameterDefinitions;
				const UNiagaraScriptVariable* SourceScriptVar = EventSourceAction->LibraryNameSubscriptionArgs.SourceScriptVar;

				ChangeSource(EventSourceAction->VarName);

				// If in a parameter definitions, consider the parameter definitions binding name subscriptions.
				if (BaseLibrary != nullptr)
				{
					// Force unsubscribe the current binding name.
					BaseLibrary->UnsubscribeBindingNameFromExternalParameterDefinitions(BaseScriptVariable->Metadata.GetVariableGuid());

					// If we are in a parameter definitions and the parameter definitions name binding args are set, set the base library to synchronize the binding name with the source library.
					//@todo(ng) re-enable
					if (ALLOW_LIBRARY_TO_LIBRARY_DEFAULT_BINDING && SourceParameterDefinitions != nullptr)
					{
						BaseLibrary->SubscribeBindingNameToExternalParameterDefinitions(
							SourceParameterDefinitions,
							SourceScriptVar->Metadata.GetVariableGuid(),
							BaseScriptVariable->Metadata.GetVariableGuid()
						);
					}
				}
				else if (BaseGraph != nullptr && SourceParameterDefinitions != nullptr)
				{
					// If we are in a script graph and the parameter definitions name binding args are set, add the parameter specified by the binding as it is not guaranteed to exist in the graph yet.
					if(BaseGraph->GetMetaData(EventSourceAction->LibraryNameSubscriptionArgs.SourceScriptVar->Variable).IsSet() == false)
					{
						BaseGraph->AddParameter(EventSourceAction->LibraryNameSubscriptionArgs.SourceScriptVar);

						TSharedPtr<INiagaraParameterDefinitionsSubscriberViewModel> ParameterDefinitionsSubscriberViewModel = FNiagaraEditorUtilities::GetOwningLibrarySubscriberViewModelForGraph(BaseGraph);
						if (ParameterDefinitionsSubscriberViewModel.IsValid() && ParameterDefinitionsSubscriberViewModel->GetSubscribedParameterDefinitions().Contains(SourceParameterDefinitions) == false)
						{
							ParameterDefinitionsSubscriberViewModel->SubscribeToParameterDefinitions(SourceParameterDefinitions);
						}
					}
				}
			}
		}
	}
}

void FNiagaraScriptVariableBindingCustomization::ChangeSource(FName InVarName)
{
	FScopedTransaction Transaction(FText::Format(LOCTEXT("ChangeBinding", " Change default binding to \"{0}\" "), FText::FromName(InVarName)));
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	for (UObject* Obj : Objects)
	{
		Obj->Modify();
	}

	PropertyHandle->NotifyPreChange();
	TargetVariableBinding->Name = InVarName;
	PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
	PropertyHandle->NotifyFinishedChangingProperties();
}

void FNiagaraScriptVariableBindingCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TSharedPtr<IPropertyUtilities> PropertyUtils = CustomizationUtils.GetPropertyUtilities();
	PropertyHandle = InPropertyHandle;
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	bool bAddDefault = true;
	if (Objects.Num() == 1)
	{
		BaseScriptVariable = Cast<UNiagaraScriptVariable>(Objects[0]);
		if (BaseScriptVariable)
		{
			BaseGraph = Cast<UNiagaraGraph>(BaseScriptVariable->GetOuter());
			BaseLibrary = Cast<UNiagaraParameterDefinitions>(BaseScriptVariable->GetOuter());

			TargetVariableBinding = (FNiagaraScriptVariableBinding*)PropertyHandle->GetValueBaseAddress((uint8*)Objects[0]);

			HeaderRow
			.IsEnabled(MakeAttributeLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }))
			.NameContent()
			[
				PropertyHandle->CreatePropertyNameWidget()
			]
			.ValueContent()
			.MaxDesiredWidth(200.f)
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &FNiagaraScriptVariableBindingCustomization::OnGetMenuContent)
				.ContentPadding(1)
				.ToolTipText(this, &FNiagaraScriptVariableBindingCustomization::GetTooltipText)
				.ButtonStyle(FAppStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(FAppStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.ButtonContent()
				[
					SNew(SNiagaraParameterName)
					.ParameterName(this, &FNiagaraScriptVariableBindingCustomization::GetVariableName)
					.IsReadOnly(true)
				]
			];
			bAddDefault = false;
		}
		else
		{
			BaseGraph = nullptr;
			BaseLibrary = nullptr;
		}
	}
	
	if (bAddDefault)
	{
		HeaderRow
		.IsEnabled(MakeAttributeLambda([=] { return !InPropertyHandle->IsEditConst() && PropertyUtils->IsPropertyEditingEnabled(); }))
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
	}
}

void FNiagaraVariableMetaDataCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	UNiagaraScriptVariable* OwningScriptVariable = nullptr;
	if (Objects.Num() == 1)
	{
		OwningScriptVariable = Cast<UNiagaraScriptVariable>(Objects[0]);
	}

	bool bOwningScriptVariableSubscribedToDefinition = false;
	bool bOwningScriptVariableOuteredToDefinitions = false;
	if (OwningScriptVariable != nullptr)
	{
		bOwningScriptVariableSubscribedToDefinition = OwningScriptVariable->GetIsSubscribedToParameterDefinitions();
		if (OwningScriptVariable->GetOuter()->IsA<UNiagaraParameterDefinitions>())
		{
			bOwningScriptVariableOuteredToDefinitions = true;
		}
	}

	uint32 NumChildren;
	PropertyHandle->GetNumChildren(NumChildren);
	TArray<TSharedRef<IPropertyHandle>> ChildPropertyHandles;
	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		ChildPropertyHandles.Add(PropertyHandle->GetChildHandle(ChildIndex).ToSharedRef());
	}

	for (const TSharedRef<IPropertyHandle>& ChildPropertyHandle : ChildPropertyHandles)
	{
		/** The customized flag is used for hiding properties.
		 *  The script variable's customization which contains meta data is hiding these properties already,
		 *  but the meta data customization would stomp these by readding them. Hence, we skip them here if they were marked as customized.
		*/
		if(ChildPropertyHandle->IsCustomized())
		{
			continue;
		}
		
		const FProperty* ChildProperty = ChildPropertyHandle->GetProperty();
		const FName ChildPropertyName = ChildProperty->GetFName();

		if (ChildPropertyName == GET_MEMBER_NAME_CHECKED(FNiagaraVariableMetaData, Description))
		{ 
			IDetailPropertyRow & DetailPropertyRow = ChildBuilder.AddProperty(ChildPropertyHandle);
			if (!bOwningScriptVariableOuteredToDefinitions && bOwningScriptVariableSubscribedToDefinition)
			{
				// Parameters linked to definitions but not part of the definitions asset themselves may not edit the description property.
				// NOTE: Parameters outered to definitions, when viewed via the SNiagaraParameterDefinitionsPanel, will not allow editing the definition due to the owning details view being disabled.
				DetailPropertyRow.IsEnabled(false);
			}
		}
		else if(bOwningScriptVariableOuteredToDefinitions)
		{
			// Parameters outered to definitions may only edit the description; all other properties are skipped for the child builder.
			continue;
		}
		else
		{
			ChildBuilder.AddProperty(ChildPropertyHandle);
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraSystemScalabilityOverrideCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	];
	HeaderRow.ValueContent()
	[
		InPropertyHandle->CreatePropertyValueWidget()
	];
}

void FNiagaraSystemScalabilityOverrideCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	auto AddOverrideProperties = [&](FName MainPropertyName, TArray<FName> OverrideProperties)
	{
		if (TSharedPtr<IPropertyHandle> MainProperty = PropertyHandle->GetChildHandle(MainPropertyName))
		{
			TAttribute<EVisibility> VisibilityAttribute = TAttribute<EVisibility>::CreateLambda([MainProperty]
			{
				bool bMainProp = false;
				if(MainProperty->GetValue(bMainProp) == FPropertyAccess::Success && bMainProp)
				{
					return EVisibility::Visible;
				}

				return EVisibility::Collapsed;
			});
			
			bool bMainProp;
			if (MainProperty->GetValue(bMainProp) == FPropertyAccess::Success)
			{
				IDetailGroup& PropGroup = ChildBuilder.AddGroup(MainPropertyName, MainProperty->GetPropertyDisplayName());
				PropGroup.HeaderProperty(MainProperty.ToSharedRef());
				for (FName OverridePropName : OverrideProperties)
				{
					if (TSharedPtr<IPropertyHandle> OverrideProp = PropertyHandle->GetChildHandle(OverridePropName))
					{
						PropGroup.AddPropertyRow(OverrideProp.ToSharedRef())
						.Visibility(VisibilityAttribute);
					}
				}
			}
			else
			{
				ChildBuilder.AddProperty(MainProperty.ToSharedRef());
			}
		}
	}; 
	
	TSharedPtr<IPropertyHandle> PlatformsProperty = PropertyHandle->GetChildHandle(TEXT("Platforms"));
	ChildBuilder.AddProperty(PlatformsProperty.ToSharedRef());

	AddOverrideProperties(TEXT("bOverrideDistanceSettings"), { TEXT("bCullByDistance"), TEXT("MaxDistance") });
	AddOverrideProperties(TEXT("bOverrideInstanceCountSettings"), { TEXT("bCullMaxInstanceCount"), TEXT("MaxInstances") });
	AddOverrideProperties(TEXT("bOverridePerSystemInstanceCountSettings"), { TEXT("bCullPerSystemMaxInstanceCount"), TEXT("MaxSystemInstances") });
	AddOverrideProperties(TEXT("bOverrideVisibilitySettings"), { TEXT("VisibilityCulling") });
	AddOverrideProperties(TEXT("bOverrideGlobalBudgetScalingSettings"), { TEXT("BudgetScaling") });
	AddOverrideProperties(TEXT("bOverrideCullProxySettings"), { TEXT("CullProxyMode"), TEXT("MaxSystemProxies") });
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraRendererMaterialParameterCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TArray<UObject*> Objects;
	PropertyHandle->GetOuterObjects(Objects);
	if (Objects.Num() > 0)
	{
		WeakRenderProperties = Cast<UNiagaraRendererProperties>(Objects[0]);
	}

	HeaderRow
		.NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		[
			PropertyHandle->CreatePropertyValueWidget()
		];
}

void FNiagaraRendererMaterialParameterCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	static const FName NAME_MaterialParameterName("MaterialParameterName");
	for (uint32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex);
		if ( ChildHandle->GetProperty()->GetFName() == NAME_MaterialParameterName )
		{
			MaterialBindingNameProperty = ChildHandle;

			ChildBuilder.AddCustomRow(ChildHandle->GetPropertyDisplayName())
			.NameWidget
			[
				ChildHandle->CreatePropertyNameWidget()
			]
			.ValueWidget
			[
				SNew(SComboButton)
				.OnGetMenuContent(this, &FNiagaraRendererMaterialParameterCustomization::OnGetMaterialBindingNameMenuContent)
				.ContentPadding(1)
				.ButtonStyle(FAppStyle::Get(), "PropertyEditor.AssetComboStyle")
				.ForegroundColor(FAppStyle::GetColor("PropertyEditor.AssetName.ColorAndOpacity"))
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(this, &FNiagaraRendererMaterialParameterCustomization::GetMaterialBindingNameText)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			];
		}
		else
		{
			ChildBuilder.AddProperty(ChildHandle.ToSharedRef());
		}
	}
}

FText FNiagaraRendererMaterialParameterCustomization::GetMaterialBindingNameText() const
{
	FName BindingName;
	if ( MaterialBindingNameProperty.IsValid() )
	{
		MaterialBindingNameProperty->GetValue(BindingName);
	}
	return FText::FromName(BindingName);
}

TSharedRef<SWidget> FNiagaraRendererMaterialParameterCustomization::OnGetMaterialBindingNameMenuContent() const
{
	FMenuBuilder MenuBuilder(true, NULL);

	UNiagaraRendererProperties* RenderProperties = WeakRenderProperties.Get();
	if (RenderProperties != nullptr && MaterialBindingNameProperty.IsValid())
	{
		TArray<UMaterialInterface*> UsedMaterials;
		RenderProperties->GetUsedMaterials(nullptr, UsedMaterials);

		TArray<FName> ValidBindings;
		for (UMaterialInterface* Material : UsedMaterials)
		{
			if (Material == nullptr)
			{
				continue;
			}

			GetMaterialBindingNames(Material, ValidBindings);
		}

		for (FName ValidBinding : ValidBindings)
		{
			MenuBuilder.AddMenuEntry(
				FText::FromName(ValidBinding),
				TAttribute<FText>(),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda(
						[ValidBinding, PropertyToSet=MaterialBindingNameProperty]()
						{
							PropertyToSet->SetValue(ValidBinding);
						}
					)
				)
			);
		}
	}

	return MenuBuilder.MakeWidget();
}

void FNiagaraRendererMaterialScalarParameterCustomization::GetMaterialBindingNames(UMaterialInterface* Material, TArray<FName>& OutBindings) const
{
	TArray<FName> ValidBindings;
	TArray<FMaterialParameterInfo> MaterialParameterInfos;
	TArray<FGuid> Guids;
	Material->GetAllScalarParameterInfo(MaterialParameterInfos, Guids);

	for (const FMaterialParameterInfo& MaterialParameterInfo : MaterialParameterInfos)
	{
		OutBindings.AddUnique(MaterialParameterInfo.Name);
	}
}

void FNiagaraRendererMaterialVectorParameterCustomization::GetMaterialBindingNames(UMaterialInterface* Material, TArray<FName>& OutBindings) const
{
	TArray<FMaterialParameterInfo> MaterialParameterInfos;
	TArray<FGuid> Guids;
	Material->GetAllVectorParameterInfo(MaterialParameterInfos, Guids);

	for (const FMaterialParameterInfo& MaterialParameterInfo : MaterialParameterInfos)
	{
		OutBindings.AddUnique(MaterialParameterInfo.Name);
	}
}

void FNiagaraRendererMaterialTextureParameterCustomization::GetMaterialBindingNames(UMaterialInterface* Material, TArray<FName>& OutBindings) const
{
	TArray<FMaterialParameterInfo> MaterialParameterInfos;
	TArray<FGuid> Guids;
	Material->GetAllTextureParameterInfo(MaterialParameterInfos, Guids);

	for (const FMaterialParameterInfo& MaterialParameterInfo : MaterialParameterInfos)
	{
		OutBindings.AddUnique(MaterialParameterInfo.Name);
	}
}

#undef LOCTEXT_NAMESPACE

