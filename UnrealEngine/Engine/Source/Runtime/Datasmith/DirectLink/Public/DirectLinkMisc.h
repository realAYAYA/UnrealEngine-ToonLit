// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Templates/SharedPointer.h"

class IDatasmithElement;
class IDatasmithScene;

namespace DirectLink
{
class FSceneSnapshot;

DIRECTLINK_API const FString& GetDumpPath();

DIRECTLINK_API void DumpSceneSnapshot(FSceneSnapshot& SceneSnapshot, const FString& BaseFileName);
} // namespace DirectLink
