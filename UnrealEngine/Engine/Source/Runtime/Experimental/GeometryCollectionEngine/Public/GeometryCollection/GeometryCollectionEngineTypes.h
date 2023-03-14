// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "UObject/ObjectMacros.h"
#include "GeometryCollectionProxyData.h"
#include "GeometryCollection/GeometryCollection.h"

UENUM()
enum class ECollectionAttributeEnum : uint8
{
	Chaos_Active           UMETA(DisplayName = "Active"),
	Chaos_DynamicState     UMETA(DisplayName = "DynamicState"),
	Chaos_CollisionGroup   UMETA(DisplayName = "CollisionGroup"),
	//
	Chaos_Max              UMETA(Hidden)
};

FName GetCollectionAttributeName(ECollectionAttributeEnum TypeIn);

UENUM()
enum class ECollectionGroupEnum : uint8
{
	Chaos_Traansform        UMETA(DisplayName = "Transform"),
	//
	Chaos_Max                UMETA(Hidden)
};


FName GetCollectionGroupName(ECollectionGroupEnum TypeIn);
