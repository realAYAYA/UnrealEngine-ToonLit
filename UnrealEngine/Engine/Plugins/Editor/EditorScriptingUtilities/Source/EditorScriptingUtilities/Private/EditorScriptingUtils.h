// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AssetRegistry/AssetData.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEditorScripting, Log, All);

namespace EditorScriptingUtils
{
	/*
	 * Check if the editor is in a valid state to run a command.
	 */
	bool IsInEditorAndNotPlaying();

	UE_DEPRECATED(4.27, "Use EditorScriptingUtils::IsInEditorAndNotPlaying()")
	bool CheckIfInEditorAndPIE();

	/*
	 * Have any flag that are not supported for Editor Utilities (PIE, Map, MapData).
	 */
	bool IsPackageFlagsSupportedForAssetLibrary(uint32 PackageFlags);

	/*
	 * Check if the object has an asset and has a package in the ContentBrowser
	 */
	bool IsAContentBrowserAsset(UObject* Object, FString& OutFailureReason);

	/*
	 * Check if the Path is a valid ContentBrowser Path
	 */
	bool IsAValidPath(const FString& Path, const TCHAR* InvalidChar, FString& OutFailureReason);

	/*
	 * Check if the AssetPath can be used to create a new asset
	 */
	bool IsAValidPathForCreateNewAsset(const FString& ObjectPath, FString& OutFailureReason);

	/*
	 * Check if the Path have a valid root
	 */
	bool HasValidRoot(const FString& ObjectPath);

	/*
	 * From "AssetClass'/Game/Folder/Package.Asset'", "AssetClass /Game/Folder/Package.Asset", "/Game/Folder/Package.Asset", "/Game/Folder/MyAsset" "/Game/Folder/Package.Asset:InnerAsset.2ndInnerAsset"
	 * and convert to "/Game/Folder/Package.Asset"
	 * @note: Object name is inferred from package name when missing
	 */
	FString ConvertAnyPathToObjectPath(const FString& AssetPath, FString& OutFailureReason);

	/*
	 * From "AssetClass'/Game/Folder/MyAsset.MyAsset', "AssetClass /Game/Folder/MyAsset.MyAsset, "/Game/Folder/MyAsset.MyAsset", "/Game/Folder/", "/Game/Folder" "/Game/Folder/MyAsset.MyAsset:InnerAsset.2ndInnerAsset"
	 * and convert to "/Game/Folder"
	 */
	FString ConvertAnyPathToLongPackagePath(const FString& Path, FString& OutFailureReason);

	/*
	 * From "AssetClass'/Game/Folder/MyAsset.MyAsset', "/Game/Folder/MyAsset.MyAsset", "/Game/Folder/", "/Game/Folder" "/Game/Folder/MyAsset.MyAsset:InnerAsset.2ndInnerAsset"
	 * and find the AssetData
	 */
	FAssetData FindAssetDataFromAnyPath(const FString& AssetPath, FString& OutFailureReason);

	/*
	 * Get the list of all the assets in a folder
	 * Valid inputs: "/Game/MyFolder/", "/Game/MyFolder", "/Game/", "/Game"
	 */
	bool GetAssetsInPath(const FString& LongPackagePath, bool bRecursive, TArray<FAssetData>& OutAssetDatas, TArray<FAssetData>& OutMapAssetDatas, FString& OutFailureReason);

	/*
	 * Get the list of all the assets in a folder
	 * Valid inputs: "/Game/MyFolder/", "/Game/MyFolder", "/Game/", "/Game"
	 */
	bool GetAssetsInPath(const FString& LongPackagePath, bool bRecursive, TArray<UObject*>& OutAssets, TArray<FAssetData>& OutCouldNotLoadAssetData, TArray<FString>& OutFailureReasons);

	/*
	 * Load the asset from a FAssetData. Will return the blueprint class if it's a blueprint object. Need to be a valid asset from the ContentBrowser
	 * Normally we don't want to load Map assets because they can have side effect with file operations.
	 */
	UObject* LoadAsset(const FAssetData& AssetData, bool bAllowMapAsset, FString& OutFailureReason);

	/*
	 * Delete the directory on the disk only if it's empty
	 */
	bool DeleteEmptyDirectoryFromDisk(const FString& LongPackagePath);
}
