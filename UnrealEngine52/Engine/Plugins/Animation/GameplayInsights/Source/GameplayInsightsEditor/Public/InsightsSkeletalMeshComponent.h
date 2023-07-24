// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SkeletalMeshComponent.h"
#include "InsightsSkeletalMeshComponent.generated.h"

class IAnimationProvider;
struct FSkeletalMeshPoseMessage;
struct FSkeletalMeshInfo;

UCLASS(Hidden)
class GAMEPLAYINSIGHTSEDITOR_API UInsightsSkeletalMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:
	// Set this component up from a provider & message
	void SetPoseFromProvider(const IAnimationProvider& InProvider, const FSkeletalMeshPoseMessage& InMessage, const FSkeletalMeshInfo& SkeletalMeshInfo);

	// USkeletalMeshComponent interface
	virtual void InitAnim(bool bForceReInit) override;
};
