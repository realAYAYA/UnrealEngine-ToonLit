// Copyright Epic Games, Inc. All Rights Reserved.

#include "Selection/RestorableObjectSelection.h"

#include "Selection/AddedAndRemovedComponentInfo.h"
#include "Selection/PropertySelectionMap.h"
#include "Selection/PropertySelection.h"

const FPropertySelection* UE::LevelSnapshots::FRestorableObjectSelection::GetPropertySelection() const
{
	return Owner.EditorWorldObjectToSelectedProperties.Find(ObjectPath);
}

const UE::LevelSnapshots::FAddedAndRemovedComponentInfo* UE::LevelSnapshots::FRestorableObjectSelection::GetComponentSelection() const
{
	return Owner.EditorActorToComponentSelection.Find(ObjectPath);
}

const UE::LevelSnapshots::FCustomSubobjectRestorationInfo* UE::LevelSnapshots::FRestorableObjectSelection::GetCustomSubobjectSelection() const
{
	return Owner.EditorWorldObjectToCustomSubobjectSelection.Find(ObjectPath);
}
