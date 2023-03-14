// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HitProxies.h"
#include "MovieSceneTrack.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sections/MovieSceneSkeletalAnimationSection.h"

class UMovieSceneSkeletalAnimationSection;
class  USkeletalMeshComponent;

struct HMovieSceneSkeletalAnimationRootHitProxy : public HHitProxy
{
	DECLARE_HIT_PROXY( MOVIESCENETRACKS_API );

	TWeakObjectPtr<UMovieSceneSkeletalAnimationSection> AnimSection;
	
	TWeakObjectPtr<USkeletalMeshComponent> SkelMeshComp;

	HMovieSceneSkeletalAnimationRootHitProxy(UMovieSceneSkeletalAnimationSection* InSection, USkeletalMeshComponent* InSkelMeshComp) :
		HHitProxy(HPP_UI),
		AnimSection(InSection),
		SkelMeshComp(InSkelMeshComp)
	{}

	virtual EMouseCursor::Type GetMouseCursor() override
	{
		return EMouseCursor::CardinalCross;
	}
};

