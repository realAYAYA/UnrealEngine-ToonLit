// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaOutput.h"

#include "ProxyMediaOutput.generated.h"


/**
 * A media output that redirect to another media output.
 */
UCLASS(BlueprintType)
class MEDIAFRAMEWORKUTILITIES_API UProxyMediaOutput
	: public UMediaOutput
{
	GENERATED_BODY()

	UProxyMediaOutput();

private:

	/** Dynamic media output proxy. */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<UMediaOutput> DynamicProxy;

	/** Media output proxy. */
	UPROPERTY(EditAnywhere, Category = "Media Proxy")
	TObjectPtr<UMediaOutput> Proxy;

public:

	/**
	 * Get the media output proxy.
	 *
	 * @return The media output, or nullptr if not set.
	 */
	UMediaOutput* GetMediaOutput() const;

	/**
	 * Get the last element of the media output chain that is not a proxy media output.
	 *
	 * @return The media output, or nullptr if not set.
	 */
	UMediaOutput* GetLeafMediaOutput() const;

	/**
	 * Is the media proxy has a valid proxy.
	 *
	 * @return true if the proxy is valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Media Proxy")
	bool IsProxyValid() const;

	/**
	 * Set the dynamic media output proxy.
	 *
	 * @param InProxy The proxy to use.
	 */
	void SetDynamicMediaOutput(UMediaOutput* InProxy);


#if WITH_EDITOR
	/**
	 * Set the media output proxy.
	 *
	 * @param InProxy The proxy to use.
	 */
	void SetMediaOutput(UMediaOutput* InProxy);
#endif

public:

	//~ UMediaOutput interface
	virtual bool Validate(FString& OutFailureReason) const override;
	virtual FIntPoint GetRequestedSize() const override;
	virtual EPixelFormat GetRequestedPixelFormat() const override;
	virtual EMediaCaptureConversionOperation GetConversionOperation(EMediaCaptureSourceType InSourceType) const override;

protected:

	virtual UMediaCapture* CreateMediaCaptureImpl() override;

private:

	mutable bool bLeafMediaOutput;
	mutable bool bValidateGuard;
	mutable bool bRequestedSizeGuard;
	mutable bool bRequestedPixelFormatGuard;
	mutable bool bCreateMediaCaptureImplGuard;
};
