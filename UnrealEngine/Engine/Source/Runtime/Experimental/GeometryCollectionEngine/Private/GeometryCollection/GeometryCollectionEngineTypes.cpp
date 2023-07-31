// Copyright Epic Games, Inc. All Rights Reserved.

#include "GeometryCollection/GeometryCollectionEngineTypes.h"


FName GetCollectionAttributeName(ECollectionAttributeEnum TypeIn)
{
	switch (TypeIn)
	{
	case ECollectionAttributeEnum::Chaos_Active:
		return FGeometryDynamicCollection::ActiveAttribute;
	case ECollectionAttributeEnum::Chaos_DynamicState:
		return FGeometryDynamicCollection::DynamicStateAttribute;
	case ECollectionAttributeEnum::Chaos_CollisionGroup:
		return FGeometryDynamicCollection::CollisionGroupAttribute;
	}
	check(false);
	return FName("");
}


FName GetCollectionGroupName(ECollectionGroupEnum TypeIn)
{
	switch (TypeIn)
	{
	case ECollectionGroupEnum::Chaos_Traansform:
		return FGeometryCollection::TransformGroup;
	}
	check(false);
	return FName("");
}
