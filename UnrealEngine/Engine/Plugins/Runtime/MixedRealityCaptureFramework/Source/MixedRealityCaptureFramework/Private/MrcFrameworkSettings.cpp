// Copyright Epic Games, Inc. All Rights Reserved.

#include "MrcFrameworkSettings.h"

UMrcFrameworkSettings::UMrcFrameworkSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, DefaulVideoSource(TEXT("/MixedRealityCaptureFramework/MrcVideoSource"))
	, DefaultVideoProcessingMat(TEXT("/MixedRealityCaptureFramework/M_MrcVideoProcessing"))
	, DefaultRenderTarget(TEXT("/MixedRealityCaptureFramework/T_MrcRenderTarget"))
	, DefaultDistortionDisplacementMap(TEXT("/Engine/EngineResources/Black"))
	, DefaulGarbageMatteMesh(TEXT("/MixedRealityCaptureFramework/SM_GarbageMattePlane"))
	, DefaulGarbageMatteMaterial(TEXT("/MixedRealityCaptureFramework/M_GarbageMatte"))
	, DefaulGarbageMatteTarget(TEXT("/MixedRealityCaptureFramework/T_MrcGarbageMatteRenderTarget"))
{}
