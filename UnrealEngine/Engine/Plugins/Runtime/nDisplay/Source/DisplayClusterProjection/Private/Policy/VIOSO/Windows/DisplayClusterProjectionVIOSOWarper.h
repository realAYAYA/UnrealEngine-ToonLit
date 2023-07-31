// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Policy/VIOSO/ViosoPolicyConfiguration.h"

//-------------------------------------------------------------------------------------------------
// Vioso headers
//-------------------------------------------------------------------------------------------------
THIRD_PARTY_INCLUDES_START
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include "VWBTypes.h"
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_END


/**
 * Warper api for VIOSO
 */
class FViosoWarper
{
public:
	bool IsValid();

	bool Initialize(void* pDxDevice, const FViosoPolicyConfiguration &InConfigData);
	void Release();

	bool CalculateViewProjection(IDisplayClusterViewport* InViewport, const uint32 InContextNum, FVector& InOutViewLocation, FRotator& InOutViewRotation, FMatrix& OutProjMatrix, const float WorldToMeters, const float NCP, const float FCP);
	bool Render(VWB_param RenderParam, VWB_uint StateMask);

private:
	FVector ToViosoLocation(const FVector& InUnrealLocation, const float WorldToMeters) const;
	FVector FromViosoLocation(const FVector& InViosoLocation, const float WorldToMeters) const;
	FVector  ToViosoEulerRotation(const FRotator& InRotation) const;
	FRotator FromViosoEulerRotation(const FVector& InEulerRotation) const;

	bool IsViewClipValid() const;

	FVector GetViosoViewRotationEulerR_LHT() const;
	FVector GetViosoViewRotationEulerR_RH() const;
	FVector GetViosoViewLocation() const;
	FMatrix GetProjMatrix(IDisplayClusterViewport* InViewport, const uint32 InContextNum, const float NCP, const float FCP) const;

protected:
	// Internal VIOSO data
	struct VWB_Warper* pWarper = nullptr;

private:
	union {
		struct {
			VWB_float _left, _bottom, _right, _top;
			VWB_float _near, _far;
		};
		VWB_float ViewClip[6];
	};

	union {
		struct {
			VWB_float        _11, _12, _13, _14;
			VWB_float        _21, _22, _23, _24;
			VWB_float        _31, _32, _33, _34;
			VWB_float        _41, _42, _43, _44;
		};
		VWB_float ViewMatrix[16];
	};
};
