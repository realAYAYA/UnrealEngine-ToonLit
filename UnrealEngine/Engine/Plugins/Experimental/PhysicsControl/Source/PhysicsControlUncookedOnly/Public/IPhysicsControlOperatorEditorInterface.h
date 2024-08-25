// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

class IPhysicsControlOperatorEditorInterface : public IModularFeature
{
public:

	virtual void OpenOperatorNamesTab() = 0;
	virtual void CloseOperatorNamesTab() = 0;
	virtual void ToggleOperatorNamesTab() = 0;
	virtual bool IsOperatorNamesTabOpen() = 0;
	virtual void RequestRefresh() = 0;

};