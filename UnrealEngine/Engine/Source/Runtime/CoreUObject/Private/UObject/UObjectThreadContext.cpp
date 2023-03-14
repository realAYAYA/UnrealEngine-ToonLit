// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UObjectThreadContext.cpp: Unreal object globals
=============================================================================*/

#include "UObject/UObjectThreadContext.h"
#include "UObject/Object.h"
#include "UObject/LinkerLoad.h"

DEFINE_LOG_CATEGORY(LogUObjectThreadContext);

FUObjectThreadContext::FUObjectThreadContext()
: IsRoutingPostLoad(false)
, CurrentlyPostLoadedObjectByALT(nullptr)
, IsDeletingLinkers(false)
, IsInConstructor(0)
, ConstructedObject(nullptr)
, AsyncPackage(nullptr)
#if WITH_IOSTORE_IN_EDITOR
, AsyncPackageLoader(nullptr)
#endif
, SerializeContext(new FUObjectSerializeContext())
{}

FUObjectThreadContext::~FUObjectThreadContext()
{
}

FUObjectSerializeContext::FUObjectSerializeContext()
	: RefCount(0)
	, ImportCount(0)
	, ForcedExportCount(0)
	, ObjBeginLoadCount(0)
	, SerializedObject(nullptr)
	, SerializedPackageLinker(nullptr)
	, SerializedImportIndex(0)
	, SerializedImportLinker(nullptr)
	, SerializedExportIndex(0)
	, SerializedExportLinker(nullptr)
{}

FUObjectSerializeContext::~FUObjectSerializeContext()
{
	checkf(!HasLoadedObjects(), TEXT("FUObjectSerializeContext is being destroyed but it still has pending loaded objects in its ObjectsLoaded list."));
	check(AttachedLinkers.Num() == 0);
}

int32 FUObjectSerializeContext::IncrementBeginLoadCount()
{
	return ++ObjBeginLoadCount;
}
int32 FUObjectSerializeContext::DecrementBeginLoadCount()
{
	check(HasStartedLoading());
	return --ObjBeginLoadCount;
}

void FUObjectSerializeContext::AddUniqueLoadedObjects(const TArray<UObject*>& InObjects)
{
	for (UObject* NewLoadedObject : InObjects)
	{
		ObjectsLoaded.AddUnique(NewLoadedObject);
	}
	
}

void FUObjectSerializeContext::AddLoadedObject(UObject* InObject)
{
	ObjectsLoaded.Add(InObject);
}

bool FUObjectSerializeContext::PRIVATE_PatchNewObjectIntoExport(UObject* OldObject, UObject* NewObject)
{
	const int32 ObjLoadedIdx = ObjectsLoaded.Find(OldObject);
	if (ObjLoadedIdx != INDEX_NONE)
	{
		ObjectsLoaded[ObjLoadedIdx] = NewObject;
		return true;
	}
	else
	{
		return false;
	}
}
void FUObjectSerializeContext::AttachLinker(FLinkerLoad* InLinker)
{
	check(!GEventDrivenLoaderEnabled);
	AttachedLinkers.Add(InLinker);
}

void FUObjectSerializeContext::DetachLinker(FLinkerLoad* InLinker)
{
	AttachedLinkers.Remove(InLinker);
}

void FUObjectSerializeContext::DetachFromLinkers()
{
	check(!GEventDrivenLoaderEnabled);
	check(ObjectsLoaded.Num() == 0 || AttachedLinkers.Num() == 0);
	TArray<FLinkerLoad*> LinkersToDetach = AttachedLinkers.Array();
	for (FLinkerLoad* Linker : LinkersToDetach)
	{
		check(Linker->GetSerializeContext() == this);
		Linker->SetSerializeContext(nullptr);
	}
	check(AttachedLinkers.Num() == 0);
}
