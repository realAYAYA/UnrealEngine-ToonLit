// Copyright Epic Games, Inc. All Rights Reserved.

#include "TreeViewBuilder.h"
#include "IFilterObject.h"

void FTreeViewDataBuilder::AddFilterObject(TSharedRef<IFilterObject> FilterObject)
{	
	FilterObjects.Add(FilterObject);
	FlatFilterObjects.Add(FilterObject);
}

void FTreeViewDataBuilder::AddChildFilterObject(TArray<TSharedPtr<IFilterObject>> InFilterObjects, TSharedRef<IFilterObject> ParentObject)
{
	for (TSharedPtr<IFilterObject> ChildFilter: InFilterObjects)
	{
		FlatFilterObjects.Add(ChildFilter);
	}

	ParentToChildren.FindOrAdd(ParentObject).Append(InFilterObjects);
}
