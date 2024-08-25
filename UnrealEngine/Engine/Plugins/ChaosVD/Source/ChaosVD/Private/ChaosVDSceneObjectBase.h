// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Templates/SharedPointer.h"

class FChaosVDScene;

/** Base Class for any object that is owned by a Chaos VD Scene */
class FChaosVDSceneObjectBase
{
public:
	virtual ~FChaosVDSceneObjectBase() = default;

	virtual void SetScene(TWeakPtr<FChaosVDScene> InScene) { SceneWeakPtr = InScene; }
	
	TWeakPtr<FChaosVDScene> GetScene() const { return SceneWeakPtr; }
	
protected:
	TWeakPtr<FChaosVDScene> SceneWeakPtr;
};
