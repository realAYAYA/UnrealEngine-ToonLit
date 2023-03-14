// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraScriptHighlight.generated.h"

USTRUCT()
struct NIAGARA_API FNiagaraScriptHighlight
{
	GENERATED_BODY()

	FNiagaraScriptHighlight();

	bool IsValid() const;

	UPROPERTY(EditAnywhere, Category = Highlight)
	FLinearColor Color;

	UPROPERTY(EditAnywhere, Category = Highlight)
	FText DisplayName;

	bool operator==(const FNiagaraScriptHighlight& Other) const;

#if WITH_EDITORONLY_DATA
	static void ArrayToJson(const TArray<FNiagaraScriptHighlight>& InHighlights, FString& OutJson);
	static void JsonToArray(const FString& InJson, TArray<FNiagaraScriptHighlight>& OutHighlights);
#endif
};