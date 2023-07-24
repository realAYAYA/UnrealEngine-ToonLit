// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EdGraph/EdGraphSchema.h"
#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

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

USTRUCT()
struct FNiagaraParameterBindingAction : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY();

	FNiagaraParameterBindingAction();
	FNiagaraParameterBindingAction(FNiagaraVariableBase InAliasedParameter, FNiagaraVariableBase InParameter);

	FNiagaraVariableBase AliasedParameter;
	FNiagaraVariableBase Parameter;
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
	void CollectAllActions(FGraphActionListBuilderBase& OutAllActions) const;
	TSharedRef<SWidget> OnCreateWidgetForAction(struct FCreateWidgetForActionData* const InCreateData) const;
	void OnActionSelected(const TArray<TSharedPtr<FEdGraphSchemaAction>>& SelectedActions, ESelectInfo::Type InSelectionType) const;

	FText GetTooltipText() const;
	FName GetVariableName() const;

	void ResetToDefault();

protected:
	TSharedPtr<IPropertyHandle>		PropertyHandle;
	TWeakObjectPtr<UObject>			OwnerWeakPtr;
	TWeakObjectPtr<UNiagaraEmitter>	EmitterWeakPtr;
	TWeakObjectPtr<UNiagaraSystem>	SystemWeakPtr;
};
