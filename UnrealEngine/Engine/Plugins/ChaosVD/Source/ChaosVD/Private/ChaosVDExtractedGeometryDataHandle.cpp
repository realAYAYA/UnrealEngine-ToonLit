// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDExtractedGeometryDataHandle.h"

#include "Chaos/ImplicitObject.h"

FName FChaosVDExtractedGeometryDataHandle::GetName() const
{
	using namespace Chaos;

	static FName InvalidName = TEXT("Invalid");

	return ImplicitObject ? GetImplicitObjectTypeName(GetInnerType(ImplicitObject->GetType())) : InvalidName;
}
