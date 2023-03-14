// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataInterface_Object.h"

bool UDataInterface_Object_Asset::GetDataImpl(const UE::DataInterface::FContext& Context) const
{
	Context.SetResult(TObjectPtr<UObject>(Asset));
	return true;
}