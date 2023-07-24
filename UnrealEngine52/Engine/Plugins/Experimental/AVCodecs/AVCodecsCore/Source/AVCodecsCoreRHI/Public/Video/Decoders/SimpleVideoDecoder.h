// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Video/SimpleVideo.h"
#include "Video/VideoDecoder.h"

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"

#include "SimpleVideoDecoder.generated.h"

UCLASS(Blueprintable)
class AVCODECSCORERHI_API USimpleVideoDecoder : public UObject, public FRunnable
{
	GENERATED_BODY()

private:
	TSharedPtr<TVideoDecoder<class FVideoResourceRHI>> Child;

	FRunnableThread* AsyncThread = nullptr;
	TQueue<FSimpleVideoPacket> AsyncQueue;

	virtual uint32 Run() override;
	virtual void Exit() override;

public:
	UFUNCTION(BlueprintCallable, Category = "Video")
	bool IsAsync() const;

	UFUNCTION(BlueprintCallable, Category = "Video")
	bool IsOpen() const;

	UFUNCTION(BlueprintCallable, Category = "Video")
	UPARAM(DisplayName = "Was Success") bool Open(ESimpleVideoCodec Codec, bool bAsynchronous);

	UFUNCTION(BlueprintCallable, Category = "Video")
	void Close();

	UFUNCTION(BlueprintCallable, Category = "Video")
	UPARAM(DisplayName = "Was Success") bool SendPacket(FSimpleVideoPacket const& Packet);

	UFUNCTION(BlueprintCallable, Category = "Video")
	UPARAM(DisplayName = "Was Success") bool ReceiveFrame(UTextureRenderTarget2D* Resource);

	// Configuration
public:
	UFUNCTION(BlueprintCallable, Category = "Video")
	ESimpleVideoCodec GetCodec() const;
};
