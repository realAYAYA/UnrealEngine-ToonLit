// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

class UNiagaraDataInterface;
class UNiagaraEditorDataBase;
class UNiagaraEditorParametersAdapterBase;
class UNiagaraMessageDataBase;
class UNiagaraScriptSourceBase;
class UNiagaraSystem;
struct FNiagaraSystemStateData;

/** Defines utility methods for creating editor only data which is stored on runtime objects. */
class INiagaraEditorOnlyDataUtilities
{
public:
	virtual UNiagaraScriptSourceBase* CreateDefaultScriptSource(UObject* InOuter) const = 0;

	virtual UNiagaraEditorDataBase* CreateDefaultEditorData(UObject* InOuter) const = 0;

	virtual UNiagaraEditorParametersAdapterBase* CreateDefaultEditorParameters(UObject* InOuter) const = 0;

	virtual UObject::FAssetRegistryTag CreateClassUsageAssetRegistryTag(const UObject* SourceObject) const = 0;

	virtual UNiagaraMessageDataBase* CreateErrorMessage(UObject* InOuter, FText InMessageShort, FText InMessageLong, FName InTopicName, bool bInAllowDismissal = false) const = 0;

	virtual UNiagaraMessageDataBase* CreateWarningMessage(UObject* InOuter, FText InMessageShort, FText InMessageLong, FName InTopicName, bool bInAllowDismissal = false) const = 0;

	virtual bool IsEditorDataInterfaceInstance(const UNiagaraDataInterface* DataInterface) const = 0;

	virtual UNiagaraDataInterface* GetResolvedRuntimeInstanceForEditorDataInterfaceInstance(const UNiagaraSystem& OwningSystem, UNiagaraDataInterface& EditorDataInterfaceInstance) const = 0;

	virtual TOptional<FNiagaraSystemStateData> TryGetSystemStateData(const UNiagaraSystem& System) const = 0;
};
