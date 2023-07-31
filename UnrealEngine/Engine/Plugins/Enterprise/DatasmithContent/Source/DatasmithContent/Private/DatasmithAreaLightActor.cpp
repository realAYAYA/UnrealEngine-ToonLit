// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithAreaLightActor.h"


ADatasmithAreaLightActor::ADatasmithAreaLightActor()
	: Mobility( EComponentMobility::Static )
	, LightType( EDatasmithAreaLightActorType::Point )
	, LightShape( EDatasmithAreaLightActorShape::Rectangle )
	, Dimensions( 100.f, 100.f )
	, Intensity( 10.f )
	, IntensityUnits( ELightUnits::Candelas )
	, Color( FLinearColor::White )
	, Temperature( 6500.f )
	, IESTexture( nullptr )
	, Rotation( ForceInit )
	, SourceRadius( 0.f )
	, SourceLength( 0.f )
	, AttenuationRadius( 1000.f )
	, SpotlightInnerAngle(1.f)
	, SpotlightOuterAngle(44.f)
{
}