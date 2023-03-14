// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MoviePipelineDeferredPasses.h"
#include "MovieRenderPipelineDataTypes.h"
#include "CinePrestreamingDebugRender.generated.h"

UCLASS(BlueprintType)
class UCinePrestreamingDebugRender : public UMoviePipelineDeferredPassBase
{
	GENERATED_BODY()

public:
	UCinePrestreamingDebugRender() : UMoviePipelineDeferredPassBase()
	{
		PassIdentifier = FMoviePipelinePassIdentifier("VirtualTexturePendingMips");
	}
#if WITH_EDITOR
	virtual FText GetDisplayText() const override { return NSLOCTEXT("MovieRenderPipeline", "DeferredBasePassSetting_DisplayName_VTPendingMips", "VT Pending Mips (Debug)"); }
#endif
	virtual void GetViewShowFlags(FEngineShowFlags& OutShowFlag, EViewModeIndex& OutViewModeIndex) const override
	{
		OutShowFlag = FEngineShowFlags(EShowFlagInitMode::ESFIM_Game);
		OutShowFlag.SetVirtualTexturePendingMips(true);
		OutViewModeIndex = EViewModeIndex::VMI_VirtualTexturePendingMips;
	}
	virtual int32 GetOutputFileSortingOrder() const override { return 2; }
};
