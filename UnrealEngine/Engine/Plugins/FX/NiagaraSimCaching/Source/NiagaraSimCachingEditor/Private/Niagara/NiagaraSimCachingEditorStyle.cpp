// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraSimCachingEditorStyle.h"

#include "Styling/SlateStyleRegistry.h"

FName FNiagaraSimCachingEditorStyle::StyleName("NiagaraSimCachingEditorStyle");

FNiagaraSimCachingEditorStyle::FNiagaraSimCachingEditorStyle()
	: FSlateStyleSet(StyleName)
{
	const FVector2D IconSize(20.0f, 20.0f);
	const FVector2D LabelIconSize(16.0f, 16.0f);

	FSlateStyleSet::SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FNiagaraSimCachingEditorStyle::~FNiagaraSimCachingEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

FNiagaraSimCachingEditorStyle& FNiagaraSimCachingEditorStyle::Get()
{
	static FNiagaraSimCachingEditorStyle Inst;
	return Inst;
}


