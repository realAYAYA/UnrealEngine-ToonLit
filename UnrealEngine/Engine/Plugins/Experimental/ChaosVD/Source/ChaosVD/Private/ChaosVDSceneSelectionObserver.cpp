// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDSceneSelectionObserver.h"

#include "Elements/Framework/TypedElementSelectionSet.h"

FChaosVDSceneSelectionObserver::FChaosVDSceneSelectionObserver()
{
}

FChaosVDSceneSelectionObserver::~FChaosVDSceneSelectionObserver()
{
	if (ObservedSelection.IsValid())
	{
		ObservedSelection->OnPreChange().RemoveAll(this);
		ObservedSelection->OnChanged().RemoveAll(this);
	}
}

void FChaosVDSceneSelectionObserver::RegisterSelectionSetObject(UTypedElementSelectionSet* SelectionSetObject)
{
	if (ObservedSelection.IsValid())
	{
		ObservedSelection->OnPreChange().RemoveAll(this);
		ObservedSelection->OnChanged().RemoveAll(this);
	}

	if (SelectionSetObject)
	{
		ObservedSelection = TWeakObjectPtr<UTypedElementSelectionSet>(SelectionSetObject);
		ObservedSelection->OnPreChange().AddRaw(this, &FChaosVDSceneSelectionObserver::HandlePreSelectionChange);
		ObservedSelection->OnChanged().AddRaw(this, &FChaosVDSceneSelectionObserver::HandlePostSelectionChange);
	}
}
