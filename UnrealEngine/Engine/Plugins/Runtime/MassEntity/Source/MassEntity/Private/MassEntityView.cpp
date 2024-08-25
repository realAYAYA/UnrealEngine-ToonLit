// Copyright Epic Games, Inc. All Rights Reserved.

#include "MassEntityView.h"
#include "MassEntityManager.h"
#include "MassArchetypeData.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MassEntityView)


//////////////////////////////////////////////////////////////////////
// FMassEntityView
FMassEntityView::FMassEntityView(const FMassArchetypeHandle& ArchetypeHandle, FMassEntityHandle InEntity)
{
	Entity = InEntity;
	Archetype = &FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	EntityDataHandle = Archetype->MakeEntityHandle(Entity);
}

FMassEntityView::FMassEntityView(const FMassEntityManager& EntityManager, FMassEntityHandle InEntity)
{
	Entity = InEntity;
	const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntity(Entity);
	Archetype = &FMassArchetypeHelper::ArchetypeDataFromHandleChecked(ArchetypeHandle);
	EntityDataHandle = Archetype->MakeEntityHandle(Entity);
}

FMassEntityView FMassEntityView::TryMakeView(const FMassEntityManager& EntityManager, FMassEntityHandle InEntity)
{
	const FMassArchetypeHandle ArchetypeHandle = EntityManager.GetArchetypeForEntity(InEntity);
	return ArchetypeHandle.IsValid() ? FMassEntityView(ArchetypeHandle, InEntity) : FMassEntityView();
}

void* FMassEntityView::GetFragmentPtr(const UScriptStruct& FragmentType) const
{
	checkSlow(Archetype && EntityDataHandle.IsValid());
	if (const int32* FragmentIndex = Archetype->GetFragmentIndex(&FragmentType))
	{
		// failing the below Find means given entity's archetype is missing given FragmentType
		return Archetype->GetFragmentData(*FragmentIndex, EntityDataHandle);
	}
	return nullptr;
}

void* FMassEntityView::GetFragmentPtrChecked(const UScriptStruct& FragmentType) const
{
	checkSlow(Archetype && EntityDataHandle.IsValid());
	const int32 FragmentIndex = Archetype->GetFragmentIndexChecked(&FragmentType);
	return Archetype->GetFragmentData(FragmentIndex, EntityDataHandle);
}

const void* FMassEntityView::GetConstSharedFragmentPtr(const UScriptStruct& FragmentType) const
{
	const FConstSharedStruct* SharedFragment = Archetype->GetSharedFragmentValues(Entity).GetConstSharedFragments().FindByPredicate(FStructTypeEqualOperator(&FragmentType));
	return (SharedFragment != nullptr) ? SharedFragment->GetMemory() : nullptr;
}

const void* FMassEntityView::GetConstSharedFragmentPtrChecked(const UScriptStruct& FragmentType) const
{
	const FConstSharedStruct* SharedFragment = Archetype->GetSharedFragmentValues(Entity).GetConstSharedFragments().FindByPredicate(FStructTypeEqualOperator(&FragmentType));
	check(SharedFragment != nullptr);
	return SharedFragment->GetMemory();
}

void* FMassEntityView::GetSharedFragmentPtr(const UScriptStruct& FragmentType) const
{
	const FSharedStruct* SharedFragment = Archetype->GetSharedFragmentValues(Entity).GetSharedFragments().FindByPredicate(FStructTypeEqualOperator(&FragmentType));
	// @todo: fix constness, should the getter be non-const too?
	return (SharedFragment != nullptr) ? const_cast<uint8*>(SharedFragment->GetMemory()) : nullptr;
}

void* FMassEntityView::GetSharedFragmentPtrChecked(const UScriptStruct& FragmentType) const
{
	const FSharedStruct* SharedFragment = Archetype->GetSharedFragmentValues(Entity).GetSharedFragments().FindByPredicate(FStructTypeEqualOperator(&FragmentType));
	check(SharedFragment != nullptr);
	// @todo: fix constness, should the getter be non-const too?
	return const_cast<uint8*>(SharedFragment->GetMemory());
}

bool FMassEntityView::HasTag(const UScriptStruct& TagType) const
{
	checkSlow(Archetype && EntityDataHandle.IsValid());
	return Archetype->HasTagType(&TagType);
}
