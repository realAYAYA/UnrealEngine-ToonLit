/*
Copyright 2019 Valve Corporation under https://opensource.org/licenses/BSD-3-Clause
This code includes modifications by Epic Games.  Modifications (c) Epic Games, Inc.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
   may be used to endorse or promote products derived from this software
   without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNodeBase.h"
#include "SteamVRInputDeviceFunctionLibrary.h"
#include "SteamVRSkeletonDefinition.h"
#include "AnimNode_SteamVRSetWristTransform.generated.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS

/**
* Custom animation node that sets the wrist transform of a target pose from a reference pose
*/
USTRUCT(BlueprintInternalUseOnly, meta = (Deprecated = "5.1"))
struct STEAMVRINPUTDEVICE_API FAnimNode_SteamVRSetWristTransform : public FAnimNode_Base
{
	GENERATED_USTRUCT_BODY()

	/** The pose from where we will get the root and/or wrist transform from */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links, meta = (DeprecatedProperty))
	FPoseLink ReferencePose;

	/** What kind of skeleton is used in the reference pose */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (AlwaysAsPin, DeprecatedProperty))
	EHandSkeleton HandSkeleton = EHandSkeleton::VR_SteamVRHandSkeleton;

	/** The pose to apply the wrist transform to */
	UE_DEPRECATED(5.1, "SteamVR plugin is deprecated; please use the OpenXR plugin.")
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Links, meta = (DeprecatedProperty))
	FPoseLink TargetPose;

public:

	// FAnimNode_Base interface
	virtual void Initialize_AnyThread(const FAnimationInitializeContext& Context) override;
	virtual void CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) override;
	virtual void Update_AnyThread(const FAnimationUpdateContext& Context) override;
	virtual void Evaluate_AnyThread(FPoseContext& Output) override;
	// End of FAnimNode_Base interface

	FAnimNode_SteamVRSetWristTransform();

private:
	/** The root bone index of the SteamVR & UE4 Skeletons */
	FCompactPoseBoneIndex RootBoneIndex = FCompactPoseBoneIndex(0);

	/** The wrist bone index of the SteamVR Skeleton */
	FCompactPoseBoneIndex SteamVRWristBoneIndex = FCompactPoseBoneIndex(1);

};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
