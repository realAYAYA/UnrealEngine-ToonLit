// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

class UNiagaraScriptSourceBase;
class UNiagaraEditorParametersAdapterBase;

/** Defines utility methods for creating editor only data which is stored on runtime objects. */
class INiagaraEditorOnlyDataUtilities
{
public:
	virtual UNiagaraScriptSourceBase* CreateDefaultScriptSource(UObject* InOuter) const = 0;

	virtual UNiagaraEditorDataBase* CreateDefaultEditorData(UObject* InOuter) const = 0;

	virtual UNiagaraEditorParametersAdapterBase* CreateDefaultEditorParameters(UObject* InOuter) const = 0;
};