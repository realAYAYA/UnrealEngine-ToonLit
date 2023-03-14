// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraSystem.h"
#include "ViewModels/TNiagaraViewModelManager.h"

class FNiagaraSystemViewModel;

class FNiagaraUserParameterPanelViewModel 
	: public TSharedFromThis<FNiagaraUserParameterPanelViewModel>
    , public TNiagaraViewModelManager<UNiagaraSystem, FNiagaraUserParameterPanelViewModel>
{
public:
    DECLARE_DELEGATE_OneParam(FOnParameterAdded, FNiagaraVariable);

	virtual ~FNiagaraUserParameterPanelViewModel();
	
	void Initialize(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel);
	
    FOnParameterAdded& OnParameterAdded() { return OnParameterAddedDelegate; }
private:
    FOnParameterAdded OnParameterAddedDelegate;
	TWeakPtr<FNiagaraSystemViewModel> SystemViewModelWeak;
    TNiagaraViewModelManager<UNiagaraSystem, FNiagaraUserParameterPanelViewModel>::Handle RegisteredHandle;
};