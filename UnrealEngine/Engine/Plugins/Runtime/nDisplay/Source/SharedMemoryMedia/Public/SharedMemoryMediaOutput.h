// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"

#include "MediaIOCoreDefinitions.h"
#include "UObject/ObjectMacros.h"

#include "SharedMemoryMediaOutput.generated.h"


/**
 * Output information for a SharedMemory media capture.
 */
UCLASS(BlueprintType)
class SHAREDMEMORYMEDIA_API USharedMemoryMediaOutput : public UMediaOutput
{
	GENERATED_BODY()

public:

	/** Shared memory will be opened by using this name. Should be unique per media output. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	FString UniqueName = TEXT("UniqueName");

public:
	
	//~ Begin UMediaOutput interface
	virtual bool Validate(FString& FailureReason) const override;
	virtual FIntPoint GetRequestedSize() const override;
	virtual EPixelFormat GetRequestedPixelFormat() const override;
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const override;

protected:
	virtual UMediaCapture* CreateMediaCaptureImpl() override;
	//~ End UMediaOutput interface

};
