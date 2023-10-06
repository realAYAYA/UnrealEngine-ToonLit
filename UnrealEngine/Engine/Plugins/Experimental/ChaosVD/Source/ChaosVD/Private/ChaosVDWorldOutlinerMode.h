// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "ActorBrowsingMode.h"
#include "ChaosVDScene.h"
#include "ChaosVDSceneSelectionObserver.h"

class FChaosVDScene;

/**
 * Scene outliner mode used to represent a CVD (Chaos Visual Debugger) world
 * It has a more limited view compared to the normal outliner, hiding features we don't support,
 * and it is integrated with the CVD local selection system
 */
class FChaosVDWorldOutlinerMode : public FActorMode, public FChaosVDSceneSelectionObserver
{
public:

	FChaosVDWorldOutlinerMode(const FActorModeParams& InModeParams, TWeakPtr<FChaosVDScene> InScene);

	virtual ~FChaosVDWorldOutlinerMode() override;

	virtual void OnItemSelectionChanged(FSceneOutlinerTreeItemPtr Item, ESelectInfo::Type SelectionType, const FSceneOutlinerItemSelection& Selection) override;
	virtual void OnItemDoubleClick(FSceneOutlinerTreeItemPtr Item) override;

	virtual void CreateViewContent(FMenuBuilder& MenuBuilder) override
	{
		// Intentionally not implemented, as we don't support the built in menu to switch worlds
	}

	virtual bool ShouldShowFolders() const override { return true;};

private:

	void HandleActorLabelChanged(AActor* ChangedActor);

	virtual void HandlePostSelectionChange(const UTypedElementSelectionSet* ChangesSelectionSet) override;

	FDelegateHandle ActorLabelChangedDelegateHandle;

	TWeakPtr<FChaosVDScene> CVDScene;
};
