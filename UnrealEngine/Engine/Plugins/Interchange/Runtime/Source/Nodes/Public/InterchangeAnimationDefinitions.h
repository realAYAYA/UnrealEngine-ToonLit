// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"

namespace UE
{
	namespace Interchange
	{
		namespace Animation
		{
			namespace PropertyTracks
			{
				const FName Visibility = TEXT("Actor.bHidden");

				namespace Light
				{
					const FName Color = TEXT("Light.LightColor");
					const FName Intensity = TEXT("Light.Intensity");
					const FName IntensityUnits = TEXT("Light.IntensityUnits");
					const FName Temperature = TEXT("Light.Temperature");
					const FName UseTemperature = TEXT("Light.bUseTemperature");
				}

				namespace Camera
				{
					const FName AutoActivate = TEXT("Camera.bAutoActivate");
					const FName AspectRatioAxisConstraint = TEXT("Camera.AspectRatioAxisConstraint");
					const FName ConstrainAspectRatio = TEXT("Camera.bConstrainAspectRatio");
					const FName CurrentAperture = TEXT("Camera.CurrentAperture");
					const FName CurrentFocalLength = TEXT("Camera.CurrentFocalLength");
					const FName CustomNearClippingPlane = TEXT("Camera.CustomNearClippingPlane");
					const FName FieldOfView = TEXT("Camera.FieldOfView");
					const FName Mobility = TEXT("Camera.Mobility");
					const FName OrthoFarClipPlane = TEXT("Camera.OrthoFarClipPlane");
					const FName OrthoWidth = TEXT("Camera.OrthoWidth");
					const FName PostProcessBlendWeight = TEXT("Camera.PostProcessBlendWeight");
					const FName ProjectionMode = TEXT("Camera.ProjectionMode");
					const FName ShouldUpdatePhysicsVolume = TEXT("Camera.bShouldUpdatePhysicsVolume");
					const FName UseFieldOfViewForLOD = TEXT("Camera.bUseFieldOfViewForLOD");
				}
			}
		}
	}
}