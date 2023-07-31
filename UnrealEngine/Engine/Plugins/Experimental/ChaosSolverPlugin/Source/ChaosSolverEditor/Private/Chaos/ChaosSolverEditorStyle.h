// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

class FChaosSolverEditorStyle final : public FSlateStyleSet
{
public:
	FChaosSolverEditorStyle() : FSlateStyleSet("ChaosSolverEditorStyle")
	{
		const FVector2D Icon16x16(16.f, 16.f);
		const FVector2D Icon64x64(64.f, 64.f);

#if !IS_MONOLITHIC
		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Experimental/ChaosSolverPlugin/Resources"));
#endif

		Set("ClassIcon.ChaosSolver", new FSlateVectorImageBrush(RootToContentDir(TEXT("ChaosSolver_16.svg")), Icon16x16));
		Set("ClassThumbnail.ChaosSolver", new FSlateVectorImageBrush(RootToContentDir(TEXT("ChaosSolver_64.svg")), Icon64x64));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	~FChaosSolverEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

public:

	static FChaosSolverEditorStyle& Get()
	{
		if (!Singleton.IsSet())
		{
			Singleton.Emplace();
		}
		return Singleton.GetValue();
	}

	static void Destroy()
	{
		Singleton.Reset();
	}

private:
	static TOptional<FChaosSolverEditorStyle> Singleton;
};

TOptional<FChaosSolverEditorStyle> FChaosSolverEditorStyle::Singleton;