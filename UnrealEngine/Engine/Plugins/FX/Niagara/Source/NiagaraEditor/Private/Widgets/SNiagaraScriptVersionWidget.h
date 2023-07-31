// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NiagaraActions.h"
#include "SNiagaraVersionWidget.h"

class NIAGARAEDITOR_API SNiagaraScriptVersionWidget : public SNiagaraVersionWidget
{
public:
	void Construct(const FArguments& InArgs, UNiagaraScript* InScript, UNiagaraVersionMetaData* InMetadata, const FString& BasePackageName);

protected:
	virtual FText GetInfoHeaderText() const override;
	virtual void ExecuteSaveAsAssetAction(FNiagaraAssetVersion AssetVersion) override;

private:
	UNiagaraScript* Script = nullptr;
	FString BasePackageName;
};
