// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "GeometryCollection/GeometryCollectionProximityUtility.h"

class FGeometryCollection;

class CHAOS_API FGeometryCollectionProximityPropertiesInterface : public FManagedArrayInterface
{
public :
	typedef FManagedArrayInterface Super;
	using FManagedArrayInterface::ManagedCollection;

	// Proximity Properties Group Name
	static const FName ProximityPropertiesGroup;
	
	// Attribute: Method to determine proximity
	static const FName ProximityDetectionMethod;
	// Attribute: For convex hull proximity, what separation distance can still be considered as 'proximity'
	static const FName ProximityDistanceThreshold;
	// Attribute: Whether to use the computed proximity graph as a connection graph
	static const FName ProximityAsConnectionGraph;
	// Attribute: If greater than zero, filter proximity connections by requiring an amount of 'contact' as computed by the Contact Method
	// This is a second filter applied after initial proximity connections were determined by any Proximity Detection Method,
	// and can be used to reduce unsupported or spurious/glancing proximities.
	static const FName ProximityRequireContactAmount;
	// Attribute: How to define 'contact' for the above Require Contact Amount.
	static const FName ProximityContactMethod;

	struct FProximityProperties
	{
		EProximityMethod Method = EProximityMethod::Precise;
		float DistanceThreshold = 1.0f;
		float RequireContactAmount = 0.0f;
		EProximityContactMethod ContactMethod = EProximityContactMethod::MinOverlapInProjectionToMajorAxes;
		bool bUseAsConnectionGraph = false;
	};

	FGeometryCollectionProximityPropertiesInterface(FGeometryCollection* InGeometryCollection);

	void InitializeInterface() override;

	void CleanInterfaceForCook() override;
	
	void RemoveInterfaceAttributes() override;

	FProximityProperties GetProximityProperties() const;
	void SetProximityProperties(const FProximityProperties&);
};

