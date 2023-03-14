// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraTypes.h"
#include "NiagaraEditorSettings.h"
#include "Misc/Guid.h"
#include "NiagaraScriptVariable.h"


struct FNiagaraParameterPanelItemBase
{
public:
	FNiagaraParameterPanelItemBase() = default;

	FNiagaraParameterPanelItemBase(
		const UNiagaraScriptVariable* InScriptVariable,
		const FNiagaraNamespaceMetadata& InNamespaceMetaData
	)
		: ScriptVariable(InScriptVariable)
		, NamespaceMetaData(InNamespaceMetaData)
	{};

	/* Equality operator to support AddUnique when gathering via parameter panel view models. */
	bool operator== (const FNiagaraParameterPanelItemBase & Other) const { return GetVariable() == Other.GetVariable(); };

	/* Simple getters to reduce clutter. */
	const FNiagaraVariable& GetVariable() const { return ScriptVariable->Variable; };
	const FNiagaraVariableMetaData& GetVariableMetaData() const { return ScriptVariable->Metadata; };

public:
	const UNiagaraScriptVariable* ScriptVariable;
	FNiagaraNamespaceMetadata NamespaceMetaData;
};

struct FNiagaraParameterPanelItem : public FNiagaraParameterPanelItemBase
{
public:
	DECLARE_DELEGATE(FOnRequestRename);
	DECLARE_DELEGATE(FOnRequestRenameNamespaceModifier);

	FNiagaraParameterPanelItem() = default;

public:
	/* NOTE: Const is a lie, but necessary evil to allow binding during CreateWidget type methods where this is passed as a const ref. */
	FOnRequestRename& GetOnRequestRename() const { return OnRequestRenameDelegate; };
	FOnRequestRenameNamespaceModifier& GetOnRequestRenameNamespaceModifier() const { return OnRequestRenameNamespaceModifierDelegate; };

	void RequestRename() const { check(OnRequestRenameDelegate.IsBound()); OnRequestRenameDelegate.ExecuteIfBound(); };

	void RequestRenameNamespaceModifier() const { check(OnRequestRenameNamespaceModifierDelegate.IsBound()); OnRequestRenameNamespaceModifierDelegate.ExecuteIfBound(); };

public:
	/* For script variables; if true, the variable is sourced from a script that is not owned by the emitter/system the parameter panel is referencing. */
	bool bExternallyReferenced;

	/* For script variables; if true, the variable is a member of a custom stack context for an emitter/system. */
	bool bSourcedFromCustomStackContext;

	/* Count of references to the variable in graphs viewed by a parameter panel view model. */
	int32 ReferenceCount;

	/* The relation of this parameter item to all parameter definitions it is matching. Whether the parameter item is subscribed to a definition is tracked by the UNiagaraScriptVariable's bSubscribedToParameterDefinitions member. */
	EParameterDefinitionMatchState DefinitionMatchState;

private:
	mutable FOnRequestRename OnRequestRenameDelegate;
	mutable FOnRequestRenameNamespaceModifier OnRequestRenameNamespaceModifierDelegate;
};

struct FNiagaraParameterPanelCategory
{
public:
	FNiagaraParameterPanelCategory() = default;

	FNiagaraParameterPanelCategory(const FNiagaraNamespaceMetadata& InNamespaceMetadata)
		:NamespaceMetaData(InNamespaceMetadata)
	{};

	bool operator== (const FNiagaraParameterPanelCategory& Other) const {return NamespaceMetaData == Other.NamespaceMetaData;};

public:
	const FNiagaraNamespaceMetadata NamespaceMetaData;
};

struct FNiagaraParameterDefinitionsPanelItem : public FNiagaraParameterPanelItemBase
{
public:
	FNiagaraParameterDefinitionsPanelItem() = default;

	FNiagaraParameterDefinitionsPanelItem(
		const UNiagaraScriptVariable* InScriptVariable,
		const FNiagaraNamespaceMetadata& InNamespaceMetaData,
		const FText& InParameterDefinitionsNameText,
		const FGuid& InParameterDefinitionsUniqueId
	)
		: FNiagaraParameterPanelItemBase(InScriptVariable, InNamespaceMetaData)
		, ParameterDefinitionsNameText(InParameterDefinitionsNameText)
		, ParameterDefinitionsUniqueId(InParameterDefinitionsUniqueId)
	{};

public:
	FText ParameterDefinitionsNameText;
	FGuid ParameterDefinitionsUniqueId;
};

struct FNiagaraParameterDefinitionsPanelCategory
{
public:
	FNiagaraParameterDefinitionsPanelCategory() = default;

	FNiagaraParameterDefinitionsPanelCategory(const FText& InParameterDefinitionsNameText, const FGuid& InParameterDefinitionsUniqueId)
		: ParameterDefinitionsNameText(InParameterDefinitionsNameText)
		, ParameterDefinitionsUniqueId(InParameterDefinitionsUniqueId)
	{};

	bool operator== (const FNiagaraParameterDefinitionsPanelCategory & Other) const { return ParameterDefinitionsUniqueId == Other.ParameterDefinitionsUniqueId; };

	FText ParameterDefinitionsNameText;
	FGuid ParameterDefinitionsUniqueId;
};
