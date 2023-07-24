// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IStereoLayers.h"
#include "IXRLoadingScreen.h"
#include "XRLoadingScreenBase.h"

class IStereoLayers;

/**
 * Default Loading Screen implementation based on the IStereoLayer interface.
 * It requires an XR tracking system with stereo rendering and stereo layers support.
 */

struct FSplashData {
	IXRLoadingScreen::FSplashDesc	Desc;
	uint32		LayerId;

	FSplashData(const IXRLoadingScreen::FSplashDesc& InDesc);
};

class HEADMOUNTEDDISPLAY_API FDefaultXRLoadingScreen : public TXRLoadingScreenBase<FSplashData>
{
public:
	FDefaultXRLoadingScreen(class IXRTrackingSystem* InTrackingSystem);

	/* IXRLoadingScreen interface */
	virtual void ShowLoadingScreen() override;
	virtual void HideLoadingScreen() override;

private:

	IStereoLayers* GetStereoLayers() const;

protected:
	virtual void DoShowSplash(FSplashData& Splash) override;
	virtual void DoHideSplash(FSplashData& Splash) override;
	virtual void DoAddSplash(FSplashData& Splash) override {}
	virtual void DoDeleteSplash(FSplashData& Splash) override;
	virtual void ApplyDeltaRotation(const FSplashData& Splash) override;

};