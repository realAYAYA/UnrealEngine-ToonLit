// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Containers/UnrealString.h"
#include "Math/MathFwd.h"
#include "Subsystems/WorldSubsystem.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class AAvaViewportDataActor;
struct FAvaViewportGuideInfo;

struct AVALANCHEVIEWPORT_API FAvaViewportGuidePresetProvider
{
	bool SaveGuidePreset(const FString& InPresetName, const TArray<FAvaViewportGuideInfo>& InGuides, const FVector2f InViewportSize);

	bool LoadGuidePreset(const FString& InPresetName, TArray<FAvaViewportGuideInfo>& OutGuides, const FVector2f InViewportSize);

	bool RemoveGuidePreset(const FString& InPresetName);

	TArray<FString> GetGuidePresetNames();

	FString GetLastAccessedGuidePresetName() const;

protected:
	TWeakObjectPtr<AAvaViewportDataActor> DataActorWeak;

	FString LastAccessedGuidePresetName;
};
