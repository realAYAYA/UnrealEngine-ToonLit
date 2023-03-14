// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "NiagaraActions.h"
#include "SNiagaraVersionWidget.h"



class NIAGARAEDITOR_API SNiagaraEmitterVersionWidget : public SNiagaraVersionWidget
{
public:
	void Construct(const FArguments& InArgs, UNiagaraEmitter* InEmitter, UNiagaraVersionMetaData* InMetadata, const FString& BasePackageName);

protected:
	virtual FText GetInfoHeaderText() const override;
	virtual void ExecuteSaveAsAssetAction(FNiagaraAssetVersion AssetVersion) override;

private:
	UNiagaraEmitter* Emitter = nullptr;
	FString BasePackageName;
};
