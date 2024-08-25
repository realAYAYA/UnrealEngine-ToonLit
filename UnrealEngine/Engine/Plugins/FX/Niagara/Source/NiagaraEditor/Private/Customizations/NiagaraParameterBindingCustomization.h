// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"
#include "Layout/Visibility.h"
#include "UObject/StructOnScope.h"

#include "NiagaraCommon.h"
#include "NiagaraParameterBindingCustomization.generated.h"

class FDetailWidgetRow;
class IDetailChildrenBuilder;
struct FEdGraphSchemaAction;
struct FGraphActionListBuilderBase;
class IPropetyHandle;
class IPropertyTypeCustomizationUtils;
class SWidget;

class UNiagaraEmitter;
class UNiagaraSystem;
struct FNiagaraParameterBinding;
class SNiagaraParameterEditor;

USTRUCT()
struct FNiagaraParameterBindingAction : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FNiagaraParameterBindingAction();
	explicit FNiagaraParameterBindingAction(FText InName, FText InTooltip);

	static TSharedRef<FNiagaraParameterBindingAction> MakeNone();
	static TSharedRef<FNiagaraParameterBindingAction> MakeConstant(const FNiagaraTypeDefinition& TypeDef, const uint8* ValueData);
	static TSharedRef<FNiagaraParameterBindingAction> MakeParameter(FNiagaraVariableBase InAliasedParameter, FNiagaraVariableBase InResolvedParameter);

	bool bIsValidParameter = false;
	FNiagaraVariableBase AliasedParameter;
	FNiagaraVariableBase ResolvedParameter;
};

class FNiagaraParameterBindingCustomization : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	/** IPropertyTypeCustomization interface begin */
	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override {}
	/** IPropertyTypeCustomization interface end */

	bool IsValid() const;
	FNiagaraParameterBinding* GetParameterBinding() const;
	FNiagaraParameterBinding* GetDefaultParameterBinding() const;

	TSharedRef<SWidget> OnGetMenuContent() const;
	bool ForEachBindableVariable(TFunction<bool(FNiagaraVariableBase, FNiagaraVariableBase)> Delegate) const;
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) const;
	TSharedRef<SWidget> OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData) const;
	void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType) const;
	void OnValueChanged() const;

	bool IsBindingValid(FNiagaraParameterBinding* ParameterBinding) const;
	EVisibility IsBindingVisibile() const;
	EVisibility IsConstantVisibile() const;
	bool IsBindingEnabled() const;
	bool IsConstantEnabled() const;

	FText GetTooltipText() const;
	FName GetVariableName() const;

	EVisibility IsResetToDefaultsVisible() const;
	FReply OnResetToDefaultsClicked();

protected:
	TSharedPtr<IPropertyHandle>		PropertyHandle;
	TWeakObjectPtr<UObject>			OwnerWeakPtr;
	TWeakObjectPtr<UNiagaraEmitter>	EmitterWeakPtr;
	TWeakObjectPtr<UNiagaraSystem>	SystemWeakPtr;

	TSharedPtr<SNiagaraParameterEditor>	DefaultValueParameterEditor;
	TSharedPtr<FStructOnScope>			DefaultValueStructOnScope;
};
