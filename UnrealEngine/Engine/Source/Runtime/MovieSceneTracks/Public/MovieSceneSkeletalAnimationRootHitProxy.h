// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HitProxies.h"
#include "UObject/WeakObjectPtr.h"

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "MovieSceneTrack.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"
#endif

class UMovieSceneSkeletalAnimationSection;
class  USkeletalMeshComponent;

struct HMovieSceneSkeletalAnimationRootHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( MOVIESCENETRACKS_API );

	TWeakObjectPtr<UMovieSceneSkeletalAnimationSection> AnimSection;
	TWeakObjectPtr<USkeletalMeshComponent> SkelMeshComp;

	MOVIESCENETRACKS_API HMovieSceneSkeletalAnimationRootHitProxy(UMovieSceneSkeletalAnimationSection* InSection, USkeletalMeshComponent* InSkelMeshComp);

	virtual EMouseCursor::Type GetMouseCursor() override;
};

