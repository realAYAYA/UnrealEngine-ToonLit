// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
GeometryCollectionProxyData.cpp: 
=============================================================================*/

#include "GeometryCollectionProxyData.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/TransformCollection.h"

/*
* FTransformDynamicCollection (FManagedArrayCollection)
*/

FTransformDynamicCollection::FTransformDynamicCollection()
	: FManagedArrayCollection()
{
	Construct();
}


void FTransformDynamicCollection::Construct()
{
	FManagedArrayCollection::FConstructionParameters TransformDependency(FTransformCollection::TransformGroup);

	// Transform Group
	AddExternalAttribute<FTransform>(FTransformCollection::TransformAttribute, FTransformCollection::TransformGroup, Transform);
	AddExternalAttribute<int32>(FTransformCollection::ParentAttribute, FTransformCollection::TransformGroup, Parent);
	AddExternalAttribute<TSet<int32>>(FTransformCollection::ChildrenAttribute, FTransformCollection::TransformGroup, Children);
	AddExternalAttribute<int32>(FGeometryCollection::SimulationTypeAttribute, FTransformCollection::TransformGroup, SimulationType);
	AddExternalAttribute<int32>(FGeometryCollection::StatusFlagsAttribute, FTransformCollection::TransformGroup, StatusFlags);
}

/*
* FGeometryDynamicCollection (FTransformDynamicCollection)
*/

const FName FGeometryDynamicCollection::ActiveAttribute("Active");
const FName FGeometryDynamicCollection::CollisionGroupAttribute("CollisionGroup");
const FName FGeometryDynamicCollection::CollisionMaskAttribute("CollisionMask");
const FName FGeometryDynamicCollection::DynamicStateAttribute("DynamicState");
const FName FGeometryDynamicCollection::ImplicitsAttribute("Implicits");
const FName FGeometryDynamicCollection::ShapesQueryDataAttribute("ShapesQueryData");
const FName FGeometryDynamicCollection::ShapesSimDataAttribute("ShapesSimData");
const FName FGeometryDynamicCollection::SimplicialsAttribute("CollisionParticles");
const FName FGeometryDynamicCollection::SimulatableParticlesAttribute("SimulatableParticlesAttribute");
const FName FGeometryDynamicCollection::SharedImplicitsAttribute("SharedImplicits");

FGeometryDynamicCollection::FGeometryDynamicCollection()
	: FTransformDynamicCollection()
{
	// Transform Group
	AddExternalAttribute<bool>(FGeometryDynamicCollection::ActiveAttribute, FTransformCollection::TransformGroup, Active);
	AddExternalAttribute<int32>(FGeometryDynamicCollection::CollisionGroupAttribute, FTransformCollection::TransformGroup, CollisionGroup);
	AddExternalAttribute<int32>(FGeometryDynamicCollection::CollisionMaskAttribute, FTransformCollection::TransformGroup, CollisionMask);
	AddExternalAttribute("CollisionStructureID", FTransformCollection::TransformGroup, CollisionStructureID);
	AddExternalAttribute<int32>(FGeometryDynamicCollection::DynamicStateAttribute, FTransformCollection::TransformGroup, DynamicState);
	AddExternalAttribute(ImplicitsAttribute, FTransformCollection::TransformGroup, Implicits);
	AddExternalAttribute("InitialAngularVelocity", FTransformCollection::TransformGroup, InitialAngularVelocity);
	AddExternalAttribute("InitialLinearVelocity", FTransformCollection::TransformGroup, InitialLinearVelocity);
	AddExternalAttribute("MassToLocal", FTransformCollection::TransformGroup, MassToLocal);
	//AddExternalAttribute(ShapesQueryDataAttribute, FTransformCollection::TransformGroup, ShapeQueryData);
	//AddExternalAttribute(ShapesSimDataAttribute, FTransformCollection::TransformGroup, ShapeSimData);
	AddExternalAttribute(SimplicialsAttribute, FTransformCollection::TransformGroup, Simplicials);
	AddExternalAttribute(SimulatableParticlesAttribute, FGeometryCollection::TransformGroup, SimulatableParticles);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FGeometryCollectionDynamicStateFacade::FGeometryCollectionDynamicStateFacade(FManagedArrayCollection& InCollection)
	: ActiveAttribute(InCollection, FGeometryDynamicCollection::ActiveAttribute,  FTransformCollection::TransformGroup)
	, DynamicStateAttribute(InCollection, FGeometryDynamicCollection::DynamicStateAttribute,  FTransformCollection::TransformGroup)
	, ChildrenAttribute(InCollection, "Children",  FTransformCollection::TransformGroup)
	, ParentAttribute(InCollection, "Parent",  FTransformCollection::TransformGroup)
	, InternalClusterParentTypeAttribute(InCollection, "InternalClusterParentTypeArray", FGeometryCollection::TransformGroup)
{
}

bool FGeometryCollectionDynamicStateFacade::IsValid() const
{
	return ActiveAttribute.IsValid()
		&& DynamicStateAttribute.IsValid()
		&& ChildrenAttribute.IsValid()
		&& ParentAttribute.IsValid()
		&& InternalClusterParentTypeAttribute.IsValid()
		;
}

bool FGeometryCollectionDynamicStateFacade::IsDynamicOrSleeping(int32 TransformIndex) const
{
	const int32 State = DynamicStateAttribute.Get()[TransformIndex];
	return (State == (int)EObjectStateTypeEnum::Chaos_Object_Sleeping) || (State == (int)EObjectStateTypeEnum::Chaos_Object_Dynamic);
}

bool FGeometryCollectionDynamicStateFacade::IsSleeping(int32 TransformIndex) const
{
	const int32 State = DynamicStateAttribute.Get()[TransformIndex];
	return (State == (int)EObjectStateTypeEnum::Chaos_Object_Sleeping);
}

bool FGeometryCollectionDynamicStateFacade::HasChildren(int32 TransformIndex) const
{
	return (ChildrenAttribute.Get()[TransformIndex].Num() > 0);
}

bool FGeometryCollectionDynamicStateFacade::HasBrokenOff(int32 TransformIndex) const
{
	const bool IsActive = ActiveAttribute.Get()[TransformIndex];
	const bool HasParent = (ParentAttribute.Get()[TransformIndex] != INDEX_NONE);
	return IsActive && (!HasParent) && IsDynamicOrSleeping(TransformIndex);
}

bool FGeometryCollectionDynamicStateFacade::HasInternalClusterParent(int32 TransformIndex) const
{
	const uint8 InternalParenttype = InternalClusterParentTypeAttribute.Get()[TransformIndex];
	return InternalParenttype != (uint8)Chaos::EInternalClusterType::None;
}

bool FGeometryCollectionDynamicStateFacade::HasDynamicInternalClusterParent(int32 TransformIndex) const
{
	const uint8 InternalParenttype = InternalClusterParentTypeAttribute.Get()[TransformIndex];
	return InternalParenttype == (uint8)Chaos::EInternalClusterType::Dynamic;
}