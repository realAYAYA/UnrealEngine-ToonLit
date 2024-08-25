// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorConfigBase.h"
#include "NiagaraAssetBrowserConfig.generated.h"

USTRUCT()
struct FNiagaraAssetBrowserConfiguration
{
	GENERATED_BODY()
	
	UPROPERTY()
	TArray<FName> MainFilterSelection;

	UPROPERTY()
	bool bShouldDisplayViewport = false;
};

UCLASS(EditorConfig="NiagaraAssetBrowser")
class NIAGARAEDITOR_API UNiagaraAssetBrowserConfig : public UEditorConfigBase
{
	GENERATED_BODY()
	
public:		
	static void Initialize();
	static UNiagaraAssetBrowserConfig* Get() { return Instance; }
	
	UPROPERTY(meta=(EditorConfig))
	TMap<FName, FNiagaraAssetBrowserConfiguration> MainFilterSelection;
	
private:
	static TObjectPtr<UNiagaraAssetBrowserConfig> Instance;
};
