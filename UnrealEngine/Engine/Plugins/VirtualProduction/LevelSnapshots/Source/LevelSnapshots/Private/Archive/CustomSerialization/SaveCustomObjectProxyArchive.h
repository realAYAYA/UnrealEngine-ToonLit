// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseCustomObjectProxyArchive.h"

namespace UE::LevelSnapshots::CustomSerialization
{
	class FSaveCustomObjectProxyArchive : public FBaseCustomObjectProxyArchive
	{
		TArray<FSoftObjectPath>& TargetArray;
	public:
		
		FSaveCustomObjectProxyArchive(FArchive& InInnerArchive, TArray<FSoftObjectPath>& TargetArray);
		
		virtual FArchive& operator<<(UObject*& Obj) override;
		virtual FArchive& operator<<(FSoftObjectPath& Value) override;
	};
}

