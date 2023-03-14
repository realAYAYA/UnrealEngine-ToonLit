// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "StageMessages.h"

#include "StageMonitorUtils.generated.h"

UENUM()
enum class EStageMonitorNodeStatus
{
	Unknown,
	LoadingMap,
	Ready,
	HotReload,
	ShaderCompiling
};


/**
 * Message containing information about frame timings.
 * Sent at regular intervals
 */
USTRUCT()
struct STAGEMONITORCOMMON_API FFramePerformanceProviderMessage : public FStageProviderPeriodicMessage
{
	GENERATED_BODY()

public:

	FFramePerformanceProviderMessage() = default;

	UE_DEPRECATED(5.1, "FFramePerformanceProviderMessage constructor is deprecated, please use updated constructor.")
	FFramePerformanceProviderMessage(float GameThreadTime, float RenderThreadTime, float GPUTime, float IdleTime)
		: GameThreadMS(GameThreadTime), RenderThreadMS(RenderThreadTime), GPU_MS(GPUTime), IdleTimeMS(IdleTime), ShadersToCompile(0)
	{
		extern ENGINE_API float GAverageFPS;
		AverageFPS = GAverageFPS;
	}

	FFramePerformanceProviderMessage(EStageMonitorNodeStatus InStatus, float GameThreadTime, float RenderThreadTime, float GPUTime, float IdleTime, int32 InShadersToCompile)
	: Status(InStatus), GameThreadMS(GameThreadTime), RenderThreadMS(RenderThreadTime), GPU_MS(GPUTime), IdleTimeMS(IdleTime), ShadersToCompile(InShadersToCompile)
	{
		extern ENGINE_API float GAverageFPS;
		AverageFPS = GAverageFPS;
	}

	/** Average FrameRate read from GAverageFPS */
	UPROPERTY(VisibleAnywhere, Category = "Performance")
	EStageMonitorNodeStatus Status = EStageMonitorNodeStatus::Unknown;

	/** Full path name of the asset involved in the status. This will be empty for Ready, Shader Compiles, and Unknown states  */
	UPROPERTY(VisibleAnywhere, Category = "Performance")
	FString AssetInStatus;

	/** Average FrameRate read from GAverageFPS */
	UPROPERTY(VisibleAnywhere, Category = "Performance")
	float AverageFPS = 0.f;

	/** Current GameThread time read from GGameThreadTime in milliseconds */
	UPROPERTY(VisibleAnywhere, Category = "Performance", meta = (Unit = "ms"))
	float GameThreadMS = 0.f;

	/** Current RenderThread time read from GRenderThreadTime in milliseconds */
	UPROPERTY(VisibleAnywhere, Category = "Performance", meta = (Unit = "ms"))
	float RenderThreadMS = 0.f;

	/** Current GPU time read from GGPUFrameTime in milliseconds */
	UPROPERTY(VisibleAnywhere, Category = "Performance", meta = (Unit = "ms"))
	float GPU_MS = 0.f;

	/** Idle time (slept) in milliseconds during last frame */
	UPROPERTY(VisibleAnywhere, Category = "Performance", meta = (Unit = "ms"))
	float IdleTimeMS = 0.f;

	/** Number of shaders currently being compiled. */
	UPROPERTY(VisibleAnywhere, Category = "Performance")
	int32 ShadersToCompile = 0;
};


namespace StageMonitorUtils
{
	STAGEMONITORCOMMON_API FStageInstanceDescriptor GetInstanceDescriptor();
}
