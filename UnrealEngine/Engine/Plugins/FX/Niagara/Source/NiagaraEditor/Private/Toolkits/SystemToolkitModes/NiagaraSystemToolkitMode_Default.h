// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraSystemToolkitModeBase.h"

class FNiagaraSystemToolkit;

class FNiagaraSystemToolkitMode_Default : public FNiagaraSystemToolkitModeBase
{
public:
	FNiagaraSystemToolkitMode_Default(TWeakPtr<FNiagaraSystemToolkit> InSystemToolkit);

	/** FApplicationMode interface */
	virtual void RegisterTabFactories(TSharedPtr<FTabManager> InTabManager) override;
	
	void ExtendToolbar();
	
	virtual void PostActivateMode() override;
};
