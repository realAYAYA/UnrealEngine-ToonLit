// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "CoreTypes.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStateExtension.h"
#include "Evaluation/PreAnimatedState/MovieScenePreAnimatedStorageID.h"
#include "UObject/ObjectKey.h"

class UClass;
class UObject;

namespace UE
{
namespace MovieScene
{
struct FPreAnimatedStorageGroupHandle;
template <typename StorageType> struct TAutoRegisterPreAnimatedStorageID;

struct MOVIESCENE_API FPreAnimatedObjectGroupManager : TPreAnimatedStateGroupManager<FObjectKey>
{
	static TAutoRegisterPreAnimatedStorageID<FPreAnimatedObjectGroupManager> GroupManagerID;

	void OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap);

	void GetGroupsByClass(UClass* GeneratedClass, TArray<FPreAnimatedStorageGroupHandle>& OutGroupHandles);
};


} // namespace MovieScene
} // namespace UE
