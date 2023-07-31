// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "Framework/PropertyViewer/PropertyPath.h"

namespace UE::PropertyViewer
{

/** */
class INotifyHook
{
public:
	virtual void OnPreValueChange(const FPropertyPath& Path) = 0;
	virtual void OnPostValueChange(const FPropertyPath& Path) = 0;
};

} //namespace
