// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/World.h"

class UStaticMesh;
class USkeletalMesh;
class UGeometryCollection;

/**
* The public interface to this module
*/
class GEOMETRYCOLLECTIONEDITOR_API FGeometryCollectionCommands
{
public:

	/**
	*  Command invoked from "GeometryCollection.ToString", uses the selected GeometryCollectionActor to output the RestCollection to the Log
	*  @param World
	*/
	static void ToString(UWorld * World);

	/**
	*  Command invoked from "GeometryCollection.WriteToHeaderFile", uses the selected GeometryCollectionActor to output the RestCollection to a header file
	*  @param World
	*/
	static void WriteToHeaderFile(const TArray<FString>& Args, UWorld * World);

	/**
	*  Command invoked from "GeometryCollection.WriteToOBJFile", uses the selected GeometryCollectionActor to output the RestCollection to an OBJ file
	*  @param World
	*/
	static void WriteToOBJFile(const TArray<FString>& Args, UWorld * World);

	/**
	*  Command invoked from "GeometryCollectionAlgo.PrintStatistics", uses the selected GeometryCollectionActor to output statistics
	*  @param World
	*/
	static void PrintStatistics(UWorld * World);

	/**
	*  Command invoked from "GeometryCollectionAlgo.PrintDetailedStatistics", uses the selected GeometryCollectionActor to output detailed statistics
	*  @param World
	*/
	static void PrintDetailedStatistics(UWorld * World);

	/**
	*  Command invoked from "GeometryCollectionAlgo.PrintDetailedStatisticsSummary", uses the selected GeometryCollectionActor to output detailed statistics
	*  @param World
	*/
	static void PrintDetailedStatisticsSummary(UWorld * World);

	/**
	*
	*  @param World
	*/
	static void DeleteCoincidentVertices(const TArray<FString>& Args, UWorld * World);


	/**
	*  Command to set attributes within a named group. 
	*
	*    GeometryCollection.SetNamedAttributeValues <type> <Attribute> <Group> <Value> [<Regex Attribute> <Regex>]
	* 
	*    Where type is in SupportedAttributeTypes (bool,int32,float)
	*      and <Attribute> exists within <Group>
	*      with optional regex matching against <RegexAttribute> within <Group>
	*
	*  For Example : 
	*     GeometryCollection.SetNamedAttributeValues bool SimulatableParticlesAttribute Transform 0 BoneName Cube_1_1
	*
	*     This will set the bool attribute SimulatableParticlesAttribute of the Transform group to false where BoneName equals Cube_1_1
	*
	*/
	static void SetNamedAttributeValues(const TArray<FString>& Args, UWorld* World);

	/**
	*
	*  @param World
	*/
	static void DeleteZeroAreaFaces(const TArray<FString>& Args, UWorld * World);

	/**
	*
	*  @param World
	*/
	static void DeleteHiddenFaces(UWorld * World);

	/**
	*
	*  @param World
	*/
	static void DeleteStaleVertices(UWorld * World);

	/*
	*  Split across xz-plane
	*/
	static void SplitAcrossYZPlane(UWorld * World);

	/**
	* Remove the selected geometry entry
	*/
	static void DeleteGeometry(const TArray<FString>& Args, UWorld* World);

	/**
	* Select all geometry in hierarchy
	*/
	static void SelectAllGeometry(const TArray<FString>& Args, UWorld* World);

	/**
	* Select no geometry in hierarchy
	*/
	static void SelectNone(const TArray<FString>& Args, UWorld* World);

	/**
	* Select no geometry in hierarchy
	*/
	static void SelectLessThenVolume(const TArray<FString>& Args, UWorld* World);

	/**
	* Select inverse of currently selected geometry in hierarchy
	*/
	static void SelectInverseGeometry(const TArray<FString>& Args, UWorld* World);

	/*
	*  Ensure single root.
	*/
	static int32 EnsureSingleRoot(UGeometryCollection* RestCollection);

	/*
	*  Build Proximity Database
	*/
	static void BuildProximityDatabase(const TArray<FString>& Args, UWorld * World);

	/*
	*  Test Bone Asset
	*/
	static void SetupNestedBoneAsset(UWorld * World);

	/*
	*  Setup two clustered cubes
	*/
	static void SetupTwoClusteredCubesAsset(UWorld * World);

	/*
*  Remove Holes
*/
	static void HealGeometry(UWorld * World);

};