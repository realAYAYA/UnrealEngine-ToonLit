// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

struct FAssetData;
class UObject;
class FString;

namespace PCGEditorUtils
{
	bool IsAssetPCGBlueprint(const FAssetData& InAssetData);

	/** 
	* From an object, get its parent package and a unique name 
	* For example, if you want to create a new asset next to the original object, it will return the parent package of the original package
	* and a unique name for the new asset.
	*/
	void GetParentPackagePathAndUniqueName(const UObject* OriginalObject, const FString& NewAssetTentativeName, FString& OutPackagePath, FString& OutUniqueName);

}