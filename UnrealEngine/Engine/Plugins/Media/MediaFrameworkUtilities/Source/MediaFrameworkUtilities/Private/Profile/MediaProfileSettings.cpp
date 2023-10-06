// Copyright Epic Games, Inc. All Rights Reserved.


#include "Profile/MediaProfileSettings.h"

#include "MediaAssets/ProxyMediaSource.h"
#include "MediaAssets/ProxyMediaOutput.h"
#include "Profile/MediaProfile.h"


TArray<UProxyMediaSource*> UMediaProfileSettings::LoadMediaSourceProxies() const
{
	TArray<UProxyMediaSource*> MediaSourceProxyPtr;
	MediaSourceProxyPtr.Reset(MediaSourceProxy.Num());

	for (const TSoftObjectPtr<UProxyMediaSource>& Proxy : MediaSourceProxy)
	{
		MediaSourceProxyPtr.Add(Proxy.LoadSynchronous());
	}
	return MediaSourceProxyPtr;
}


TArray<UProxyMediaOutput*> UMediaProfileSettings::LoadMediaOutputProxies() const
{
	TArray<UProxyMediaOutput*> MediaOutputProxyPtr;
	MediaOutputProxyPtr.Reset(MediaOutputProxy.Num());

	for (const TSoftObjectPtr<UProxyMediaOutput>& Proxy : MediaOutputProxy)
	{
		MediaOutputProxyPtr.Add(Proxy.LoadSynchronous());
	}
	return MediaOutputProxyPtr;
}


UMediaProfile* UMediaProfileSettings::GetStartupMediaProfile() const
{
	return StartupMediaProfile.LoadSynchronous();
}


#if WITH_EDITOR
void UMediaProfileSettings::SetMediaSourceProxy(const TArray<UProxyMediaSource*>& InProxies)
{
	MediaSourceProxy.Reset();
	for (UProxyMediaSource* Proxy : InProxies)
	{
		MediaSourceProxy.Add(Proxy);
	}
	OnMediaProxiesChanged.Broadcast();
}


void UMediaProfileSettings::SetMediaOutputProxy(const TArray<UProxyMediaOutput*>& InProxies)
{
	MediaOutputProxy.Reset();
	for (UProxyMediaOutput* Proxy : InProxies)
	{
		MediaOutputProxy.Add(Proxy);
	}
	OnMediaProxiesChanged.Broadcast();
}


void UMediaProfileSettings::PostEditChangeProperty(struct FPropertyChangedEvent& InPropertyChangedEvent)
{
	if (GET_MEMBER_NAME_CHECKED(ThisClass, MediaOutputProxy) == InPropertyChangedEvent.GetPropertyName()
		|| GET_MEMBER_NAME_CHECKED(ThisClass, MediaSourceProxy) == InPropertyChangedEvent.GetPropertyName())
	{
		// Recache the proxies ptr
		OnMediaProxiesChanged.Broadcast();
	}

	Super::PostEditChangeProperty(InPropertyChangedEvent);
}
#endif // WITH_EDITOR


UMediaProfileEditorSettings::UMediaProfileEditorSettings()
#if WITH_EDITOR
	: bDisplayInToolbar(true)
	, bDisplayInMainEditor(false)
#endif
{
}


UMediaProfile* UMediaProfileEditorSettings::GetUserMediaProfile() const
{
#if WITH_EDITOR
	return UserMediaProfile.LoadSynchronous();
#else
	return nullptr;
#endif
	
}


void UMediaProfileEditorSettings::SetUserMediaProfile(UMediaProfile* InMediaProfile)
{
#if WITH_EDITOR
	UserMediaProfile = InMediaProfile;
	SaveConfig();
#endif
}
