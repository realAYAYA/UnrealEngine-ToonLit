// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseCustomObjectProxyArchive.h"

namespace UE::LevelSnapshots::CustomSerialization
{
	class FLoadCustomObjectProxyArchive : public FBaseCustomObjectProxyArchive
	{
		const TArray<FSoftObjectPath>& TargetArray;
	public:

		FLoadCustomObjectProxyArchive(FArchive& InInnerArchive, const TArray<FSoftObjectPath>& TargetArray);

		virtual FArchive& operator<<(UObject*& Obj) override;
		virtual FArchive& operator<<(FSoftObjectPath& Value) override;
	};
}
