// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

namespace UE
{
namespace Geometry
{

class FBoxFilter
{
private:
	float Radius = 0.5f;
	
public:
	FBoxFilter(const float RadiusIn)
		: Radius(RadiusIn)
	{		
	}

	/** @return the filter weight given a 2D distance vector. */
	float GetWeight(const FVector2d& Dist) const
	{
		// [-Radius, Radius)
		return Dist.X >= -Radius && Dist.X < Radius && Dist.Y >= -Radius && Dist.Y < Radius; 
	}
};

} // end namespace UE::Geometry
} // end namespace UE