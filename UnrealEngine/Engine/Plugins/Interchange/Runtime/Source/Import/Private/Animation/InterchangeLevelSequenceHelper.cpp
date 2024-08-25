// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeLevelSequenceHelper.h"
#include "InterchangeAnimationDefinitions.h"

#include "Camera/CameraComponent.h"
#include "CineCameraComponent.h"
#include "Components/LocalLightComponent.h"

#include "MovieScene.h"
#include "MovieSceneSection.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneByteTrack.h"
#include "Tracks/MovieSceneColorTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneVisibilityTrack.h"

namespace UE::Interchange::Private
{
	FInterchangePropertyTracksHelper::FInterchangePropertyTracksHelper()
		: PropertyTracks{
			// Light
			{UE::Interchange::Animation::PropertyTracks::Light::Color, {UMovieSceneColorTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(ULocalLightComponent, LightColor), TEXT("Light Color")}},
			{UE::Interchange::Animation::PropertyTracks::Light::Intensity, {UMovieSceneFloatTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(ULightComponent, Intensity), TEXT("Intensity")}},
			{UE::Interchange::Animation::PropertyTracks::Light::IntensityUnits, {UMovieSceneByteTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(ULocalLightComponent, IntensityUnits), TEXT("Intensity Units"), StaticEnum<ELightUnits>()}},
			{UE::Interchange::Animation::PropertyTracks::Light::Temperature, {UMovieSceneFloatTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(ULocalLightComponent, Temperature), TEXT("Temperature")}},
			{UE::Interchange::Animation::PropertyTracks::Light::UseTemperature, {UMovieSceneBoolTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(ULocalLightComponent, bUseTemperature), TEXT("Use Temperature")}},
			{UE::Interchange::Animation::PropertyTracks::Visibility, {UMovieSceneVisibilityTrack::StaticClass()->GetName(), AActor::GetHiddenPropertyName().ToString(), TEXT("Visibility")}},

			// Camera
			{UE::Interchange::Animation::PropertyTracks::Camera::AspectRatioAxisConstraint, {UMovieSceneByteTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCameraComponent, AspectRatioAxisConstraint), TEXT("Aspect Ratio Axis Constraint"), StaticEnum<EAspectRatioAxisConstraint>()}},
			{UE::Interchange::Animation::PropertyTracks::Camera::ConstrainAspectRatio, {UMovieSceneBoolTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCameraComponent, bConstrainAspectRatio), TEXT("Constrain Aspect Ratio")}},
			{UE::Interchange::Animation::PropertyTracks::Camera::CurrentAperture, {UMovieSceneFloatTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCineCameraComponent, CurrentAperture), TEXT("Current Aperture")}},
			{UE::Interchange::Animation::PropertyTracks::Camera::CurrentFocalLength, {UMovieSceneFloatTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCineCameraComponent, CurrentFocalLength), TEXT("Current Focal Length")}},
			{UE::Interchange::Animation::PropertyTracks::Camera::CustomNearClippingPlane, {UMovieSceneFloatTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCineCameraComponent, CustomNearClippingPlane), TEXT("Custom Near Clipping Plane")}},
			{UE::Interchange::Animation::PropertyTracks::Camera::FieldOfView, {UMovieSceneFloatTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCameraComponent, FieldOfView), TEXT("Field of View")}},
			{UE::Interchange::Animation::PropertyTracks::Camera::OrthoFarClipPlane, {UMovieSceneFloatTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCameraComponent, OrthoFarClipPlane), TEXT("Ortho Far Clip Plane")}},
			{UE::Interchange::Animation::PropertyTracks::Camera::OrthoWidth, {UMovieSceneFloatTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCameraComponent, OrthoWidth), TEXT("Ortho Width")}},
			{UE::Interchange::Animation::PropertyTracks::Camera::PostProcessBlendWeight, {UMovieSceneFloatTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCameraComponent, PostProcessBlendWeight), TEXT("PostProcess Blend Weight")}},
			{UE::Interchange::Animation::PropertyTracks::Camera::ProjectionMode, {UMovieSceneByteTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCameraComponent, ProjectionMode), TEXT("Projection Mode"), StaticEnum<ECameraProjectionMode::Type>()}},
			{UE::Interchange::Animation::PropertyTracks::Camera::UseFieldOfViewForLOD, {UMovieSceneBoolTrack::StaticClass()->GetName(), GET_MEMBER_NAME_STRING_CHECKED(UCameraComponent, bUseFieldOfViewForLOD), TEXT("Use Field Of View For LOD")}},
		}
	{}

	FInterchangePropertyTracksHelper& FInterchangePropertyTracksHelper::GetInstance()
	{
		static FInterchangePropertyTracksHelper Instance;
		return Instance;
	}

	UMovieSceneSection* FInterchangePropertyTracksHelper::GetSection(UMovieScene* MovieScene, const UInterchangeAnimationTrackNode& AnimationTrackNode, const FGuid& ObjectBinding, const FName& Property) const
	{
		const FInterchangeProperty* InterchangePropertyTrack = PropertyTracks.Find(Property);

		if(!InterchangePropertyTrack)
		{
			return nullptr;
		}

		TSubclassOf<UMovieSceneTrack> TrackClass = FindObjectClass<UMovieSceneTrack>(*InterchangePropertyTrack->ClassType);

		UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(MovieScene->FindTrack(TrackClass, ObjectBinding, InterchangePropertyTrack->Name));

		if(!PropertyTrack)
		{
			PropertyTrack = Cast<UMovieScenePropertyTrack>(MovieScene->AddTrack(TrackClass, ObjectBinding));

			if(!PropertyTrack)
			{
				return nullptr;
			}

			PropertyTrack->SetPropertyNameAndPath(InterchangePropertyTrack->Name, InterchangePropertyTrack->Path);
		}
		else
		{
			PropertyTrack->RemoveAllAnimationData();
		}

		if(InterchangePropertyTrack->EnumClass)
		{
			if(UMovieSceneByteTrack* ByteTrack = Cast<UMovieSceneByteTrack>(PropertyTrack))
			{
				ByteTrack->SetEnum(InterchangePropertyTrack->EnumClass);
			}
		}

		bool bSectionAdded = false;

		UMovieSceneSection* Section = PropertyTrack->FindOrAddSection(0, bSectionAdded);

		if(!Section)
		{
			return nullptr;
		}

		if(bSectionAdded)
		{
			int32 CompletionMode;
			if(AnimationTrackNode.GetCustomCompletionMode(CompletionMode))
			{
				// Make sure EMovieSceneCompletionMode enum value are still between 0 and 2
				static_assert(0 == (uint32)EMovieSceneCompletionMode::KeepState, "ENUM_VALUE_HAS_CHANGED");
				static_assert(1 == (uint32)EMovieSceneCompletionMode::RestoreState, "ENUM_VALUE_HAS_CHANGED");
				static_assert(2 == (uint32)EMovieSceneCompletionMode::ProjectDefault, "ENUM_VALUE_HAS_CHANGED");

				Section->EvalOptions.CompletionMode = (EMovieSceneCompletionMode)CompletionMode;
			}
			// By default the completion mode is EMovieSceneCompletionMode::ProjectDefault
			else
			{
				Section->EvalOptions.CompletionMode = EMovieSceneCompletionMode::ProjectDefault;
			}

			Section->SetRange(TRange<FFrameNumber>::All());
		}

		return Section;
	}
}