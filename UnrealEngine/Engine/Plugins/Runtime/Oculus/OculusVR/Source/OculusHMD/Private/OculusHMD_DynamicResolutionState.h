// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_Settings.h"
#include "DynamicResolutionState.h"

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FDynamicResolutionState
//-------------------------------------------------------------------------------------------------

class FDynamicResolutionState : public IDynamicResolutionState
{
public:
	FDynamicResolutionState(const OculusHMD::FSettingsPtr InSettings);

	// ISceneViewFamilyScreenPercentage
	virtual void ResetHistory() override;
	virtual bool IsSupported() const override;
	virtual void SetupMainViewFamily(class FSceneViewFamily& ViewFamily) override;

protected:
	virtual DynamicRenderScaling::TMap<float> GetResolutionFractionsApproximation() const override;
	virtual DynamicRenderScaling::TMap<float> GetResolutionFractionsUpperBound() const override;
	virtual void SetEnabled(bool bEnable) override;
	virtual bool IsEnabled() const override;
	virtual void ProcessEvent(EDynamicResolutionStateEvent Event) override;

private:
	const OculusHMD::FSettingsPtr Settings;
	float ResolutionFraction;
	float ResolutionFractionUpperBound;
};

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
