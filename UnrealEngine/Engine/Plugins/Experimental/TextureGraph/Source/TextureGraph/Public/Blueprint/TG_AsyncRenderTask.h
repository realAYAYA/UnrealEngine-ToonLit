// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TG_SystemTypes.h"
#include "TextureGraph.h"
#include "TG_HelperFunctions.h"
#include "Data/Blob.h"
#include "Data/TiledBlob.h"
#include "TG_AsyncTask.h"
#include "TG_AsyncRenderTask.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTSRenderTaskDelegate, const TArray<UTextureRenderTarget2D*>&, OutputRts);

UCLASS()
class TEXTUREGRAPH_API UTG_AsyncRenderTask : public UTG_AsyncTask
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "TextureGraph" , meta=(DisplayName="Texture Graph Render (Async)" , BlueprintInternalUseOnly = "true"))
	static UTG_AsyncRenderTask* TG_AsyncRenderTask(UTextureGraph* InTextureGraph);

	virtual void Activate() override;

	virtual void FinishDestroy() override;

	UPROPERTY(BlueprintAssignable, Category = "TextureGraph")
	FTSRenderTaskDelegate OnDone;

private:
	AsyncBool FinalizeAllOutputBlobs();

	AsyncBool GetRenderTextures();

	TArray<UTextureRenderTarget2D*> OutputRts;

	TArray<BlobPtr> OutputBlobs;

	UTextureGraph* OriginalTextureGraphPtr;
	UTextureGraph* TextureGraphPtr;
};

