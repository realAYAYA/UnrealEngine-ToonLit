// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/NiagaraScriptViewModel.h"

class FNiagaraSystemViewModel;
class FNiagaraScriptViewModel;
class UNiagaraSystem;

/** View model which manages the System script. */
class FNiagaraSystemScriptViewModel : public FNiagaraScriptViewModel
{
public:
	DECLARE_MULTICAST_DELEGATE(FOnSystemCompiled)

public:
	explicit FNiagaraSystemScriptViewModel(bool bInIsForDataProcessingOnly);

	void Initialize(UNiagaraSystem& InSystem);

	virtual ~FNiagaraSystemScriptViewModel() override;

	FOnSystemCompiled& OnSystemCompiled();

	void CompileSystem(bool bForce);
	
	virtual ENiagaraScriptCompileStatus GetLatestCompileStatus(FGuid VersionGuid = FGuid()) override;

private:
	void OnSystemVMCompiled(UNiagaraSystem* InSystem);

private:
	/** The System who's script is getting viewed and edited by this view model. */
	TWeakObjectPtr<UNiagaraSystem> System;

	FOnSystemCompiled OnSystemCompiledDelegate;
};
