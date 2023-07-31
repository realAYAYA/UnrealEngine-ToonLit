// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionSimulationCoreTypes.h"
#include "Templates/SharedPointer.h"

namespace GeometryCollection 
{
	namespace SizeSpecific
	{
		//
		//  Given a volume, return the first index of the size specific data that matches this volume. 
		//
		int32 CHAOS_API FindIndexForVolume(const TArray<FSharedSimulationSizeSpecificData>& SizeSpecificData, Chaos::FReal Volume);

		//
		//  Given a FBox, return the first index of the size specific data that matches this volume. 
		//
		int32 CHAOS_API FindIndexForVolume(const TArray<FSharedSimulationSizeSpecificData>& SizeSpecificData, const FBox& Bounds);

	}
}