// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Profile/IMediaProfileManager.h"
#include "UObject/GCObject.h"

class UMediaProfile;
class UProxyMediaSource;
class UProxyMediaOutput;

class FMediaProfileManager : public IMediaProfileManager, public FGCObject
{
public:
	FMediaProfileManager();
	virtual ~FMediaProfileManager();

	//~ Begin IMediaProfileManager Interface
	virtual UMediaProfile* GetCurrentMediaProfile() const override;
	virtual void SetCurrentMediaProfile(UMediaProfile* InMediaProfile) override;
	TArray<UProxyMediaSource*> GetAllMediaSourceProxy() const override;
	TArray<UProxyMediaOutput*> GetAllMediaOutputProxy() const override;
	virtual FOnMediaProfileChanged& OnMediaProfileChanged() override;
	//~ End IMediaProfileManager Interface

	//~ Begin FGCObject Interface
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FMediaProfileManager");
	}
	//~End FGCObject Interface

private:
#if WITH_EDITOR
	void OnMediaProxiesChanged();
#endif

	TArray<UProxyMediaSource*> MediaSourceProxies;
	TArray<UProxyMediaOutput*> MediaOutputProxies;
	UMediaProfile* CurrentMediaProfile;
	FOnMediaProfileChanged MediaProfileChangedDelegate;
};
