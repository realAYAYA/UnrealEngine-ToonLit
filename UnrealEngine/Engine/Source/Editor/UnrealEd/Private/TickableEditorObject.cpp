// Copyright Epic Games, Inc. All Rights Reserved.

#include "TickableEditorObject.h"
#include "Tickable.h"

TArray<FTickableObjectBase::FTickableObjectEntry>& FTickableEditorObject::GetTickableObjects()
{
	static TTickableObjectsCollection TickableObjects;
	return TickableObjects;
}

TArray<FTickableEditorObject*>& FTickableEditorObject::GetPendingTickableObjects()
{
	static TArray<FTickableEditorObject*> PendingTickableObjects;
	return PendingTickableObjects;
}

TArray<FTickableObjectBase::FTickableObjectEntry>& FTickableCookObject::GetTickableObjects()
{
	static TTickableObjectsCollection TickableObjects;
	return TickableObjects;
}

TArray<FTickableCookObject*>& FTickableCookObject::GetPendingTickableObjects()
{
	static TArray<FTickableCookObject*> PendingTickableObjects;
	return PendingTickableObjects;
}

bool FTickableCookObject::bCollectionIntact = true;
bool FTickableCookObject::bIsTickingObjects = false;
FTickableCookObject* FTickableCookObject::ObjectBeingTicked = nullptr;
