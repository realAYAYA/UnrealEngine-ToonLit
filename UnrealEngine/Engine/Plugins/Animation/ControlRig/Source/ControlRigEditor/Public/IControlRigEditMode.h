// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPersonaEditMode.h"

class ISequencer;

class IControlRigEditMode : public IPersonaEditMode
{
public:	
	virtual void SetObjects(const TWeakObjectPtr<>& InSelectedObject,  UObject* BindingObject, TWeakPtr<ISequencer> InSequencer) = 0;
};
