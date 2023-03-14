// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AssetRegistry/AssetData.h"

#include "EditorAssetLibrary.generated.h"

/**
 * Utility class to do most of the common functionalities with the ContentBrowser.
 * The AssetRegistryHelpers class has more complex utilities. Use FindAssetData to get a FAssetData from an Asset Path.
 * The Asset Path can be represented by 
 *		ie. (Reference/Text Path)	StaticMesh'/Game/MyFolder/MyAsset.MyAsset'
 *		ie. (Full Name)				StaticMesh /Game/MyFolder/MyAsset.MyAsset
 *		ie. (Path Name)				/Game/MyFolder/MyAsset.MyAsset
 *		ie. (Package Name)			/Game/MyFolder/MyAsset
 * The Directory Path can be represented by
 *		ie. /Game/MyNewFolder/
 *		ie. /Game/MyNewFolder
 * All operations can be slow. The editor should not be in play in editor mode. It will not work on assets of the type level.
 */
UCLASS()
class EDITORSCRIPTINGUTILITIES_API UEditorAssetLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Load an asset from the Content Browser. It will verify if the object is already loaded and only load it if it's necessary.
	 * @param	AssetPath		Asset Path of the asset (that is not a level).
	 * @return	Found or loaded asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static UObject* LoadAsset(const FString& AssetPath);

	/**
	 * Load a Blueprint asset from the Content Browser and return its generated class. It will verify if the object is already loaded and only load it if it's necessary.
	 * @param	AssetPath		Asset Path of the Blueprint asset.
	 * @return	Found or loaded class.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static UClass* LoadBlueprintClass(const FString& AssetPath);

	/**
	 * Return a valid AssetPath for a loaded asset. The asset need to be a valid asset in the Content Browser.
	 * Similar to GetPathName(). The format will be: /Game/MyFolder/MyAsset.MyAsset
	 * @param	LoadedAsset		Loaded Asset that exist in the Content Browser.
	 * @return	If valid, the asset Path of the loaded asset.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static FString GetPathNameForLoadedAsset(UObject* LoadedAsset);

	/**
	 * Return the AssetData for the Asset that can then be used with the more complex lib AssetRegistryHelpers.
	 * @param	AssetPath	Asset Path we are trying to find.
	 * @return	The AssetData found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static FAssetData FindAssetData(const FString& AssetPath);

	/**
	 * Check if the asset exists in the Content Browser.
	 * @param	AssetPath		Asset Path of the asset (that is not a level).
	 * @return	True if it does exist and it is valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool DoesAssetExist(const FString& AssetPath);

	/**
	 * Check if the assets exist in the Content Browser.
	 * @param	AssetPaths		Asset Path of the assets (that are not a level).
	 * @return	True if they exist and it is valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool DoAssetsExist(const TArray<FString>& AssetPaths);

public:
	/**
	 * Find Package Referencers for an asset. Only Soft and Hard dependencies would be looked for.
	 * Soft are dependencies which don't need to be loaded for the object to be used.
	 * Had are dependencies which are required for correct usage of the source asset and must be loaded at the same time.
	 * Other references may exist. The asset may be currently used in memory by another asset, by the editor or by code.
	 * Package dependencies are cached with the asset. False positive can happen until all the assets are loaded and re-saved.
	 * @param	AssetPath				Asset Path of the asset that we are looking for (that is not a level).
	 * @param	bLoadAssetsToConfirm	The asset and the referencers will be loaded (if not a level) to confirm the dependencies.
	 * @return	The package path of the referencers.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static TArray<FString> FindPackageReferencersForAsset(const FString& AssetPath, bool bLoadAssetsToConfirm = false);


	/**
	 * Consolidates an asset by replacing all references/uses of the provided AssetsToConsolidate with references to AssetToConsolidateTo.
	 * This is useful when you want all references of assets to be replaced by a single asset.
	 * The function first attempts to directly replace all relevant references located within objects that are already loaded and in memory.
	 * Next, it deletes the AssetsToConsolidate, leaving behind object redirectors to AssetToConsolidateTo.
	 * @param	AssetToConsolidateTo	Asset to which all references of the AssetsToConsolidate will instead refer to after this operation completes.
	 * @param	AssetsToConsolidate		All references to these assets will be modified to reference AssetToConsolidateTo instead.
	 * @note	The AssetsToConsolidate are DELETED by this function.
	 * @note	Modified objects will be saved if the operation succeeds.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool ConsolidateAssets(UObject* AssetToConsolidateTo, const TArray<UObject*>& AssetsToConsolidate);

public:
	/**
	 * Delete an asset from the Content Browser that is already loaded.
	 * This is a Force Delete. It doesn't check if the asset has references in other Levels or by Actors.
	 * It will close all the asset editors and may clear the Transaction buffer (Undo History).
	 * Will try to mark the file as deleted.
	 * @param	AssetToDelete			Asset that we want to delete.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool DeleteLoadedAsset(UObject* AssetToDelete);

	/**
	 * Delete assets from the Content Browser that are already loaded.
	 * This is a Force Delete. It doesn't check if the assets have references in other Levels or by Actors.
	 * It will close all the asset editors and may clear the Transaction buffer (Undo History).
	 * Will try to mark the files as deleted.
	 * @param	AssetsToDelete			Assets that we want to delete.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool DeleteLoadedAssets(const TArray<UObject*>& AssetsToDelete);

	/**
	 * Delete the package the assets live in. All objects that live in the package will be deleted.
	 * This is a Force Delete. It doesn't check if the asset has references in other Levels or by Actors.
	 * It will close all the asset editors and may clear the Transaction buffer (Undo History).
	 * Will try to mark the file as deleted. The Asset will be loaded before being deleted.
	 * @param	AssetPathToDelete		Asset Path of the asset that we want to delete.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool DeleteAsset(const FString& AssetPathToDelete);

	/**
	 * Delete the packages inside a directory. If the directory is then empty, delete the directory.
	 * This is a Force Delete. It doesn't check if the assets have references in other Levels or by Actors.
	 * It will close all the asset editors and may clear the Transaction buffer (Undo History).
	 * Will try to mark the file as deleted. Assets will be loaded before being deleted.
	 * The search is always recursive. It will try to delete the sub folders.
	 * @param	DirectoryPath		Directory that will be mark for delete and deleted.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool DeleteDirectory(const FString& DirectoryPath);

public:
	/**
	 * Duplicate an asset from the Content Browser that is already loaded. Will try to checkout the file.
	 * @param	SourceAsset				Asset that we want to copy from.
	 * @param	DestinationAssetPath	Asset Path of the duplicated asset.
	 * @return	The duplicated object if the operation succeeds
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static UObject* DuplicateLoadedAsset(UObject* SourceAsset, const FString& DestinationAssetPath);

	/**
	 * Duplicate an asset from the Content Browser. Will try to checkout the file. The Asset will be loaded before being duplicated.
	 * @param	SourceAssetPath			Asset Path of the asset that we want to copy from.
	 * @param	DestinationAssetPath	Asset Path of the duplicated asset.
	 * @return	The duplicated object if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static UObject* DuplicateAsset(const FString& SourceAssetPath, const FString& DestinationAssetPath);

	/**
	 * Duplicate asset from the Content Browser that are in the folder.
	 * Will try to checkout the files. The Assets will be loaded before being duplicated.
	 * @param	SourceDirectoryPath			Directory of the assets that we want to duplicate from.
	 * @param	DestinationDirectoryPath	Directory of the duplicated asset.
	 * @return	The duplicated object if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool DuplicateDirectory(const FString& SourceDirectoryPath, const FString& DestinationDirectoryPath);

public:
	/**
	 * Rename an asset from the Content Browser that is already loaded.
	 * Equivalent to a Move operation. Will try to checkout the files.
	 * @param	SourceAsset				Asset that we want to copy from.
	 * @param	DestinationAssetPath	Asset Path of the duplicated asset.
	 * @return	The if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool RenameLoadedAsset(UObject* SourceAsset, const FString& DestinationAssetPath);

	/**
	 * Rename an asset from the Content Browser. Equivalent to a Move operation.
	 * Will try to checkout the file. The Asset will be loaded before being renamed.
	 * @param	SourceAssetPath			Asset Path of the asset that we want to copy from.
	 * @param	DestinationAssetPath	Asset Path of the renamed asset.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool RenameAsset(const FString& SourceAssetPath, const FString& DestinationAssetPath);


	/**
	 * Rename assets from the Content Browser that are in the folder.
	 * Equivalent to a Move operation. Will try to checkout the files. The Assets will be loaded before being renamed.
	 * @param	SourceDirectoryPath			Directory of the assets that we want to rename from.
	 * @param	DestinationDirectoryPath	Directory of the renamed asset.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool RenameDirectory(const FString& SourceDirectoryPath, const FString& DestinationDirectoryPath);

public:
	/**
	 * Checkout the asset from the Content Browser.
	 * @param	AssetToCheckout		Asset to checkout.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool CheckoutLoadedAsset(UObject* AssetToCheckout);

	/**
	 * Checkout the assets from the Content Browser.
	 * @param	AssetToCheckout		Assets to checkout.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool CheckoutLoadedAssets(const TArray<UObject*>& AssetsToCheckout);

	/**
	 * Checkout the asset from the Content Browser.
	 * @param	AssetToCheckout		Asset Path of the asset that we want to checkout.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool CheckoutAsset(const FString& AssetToCheckout);

	/**
	 * Checkout assets from the Content Browser. It will load the assets if needed.
	 * All objects that are in the directory will be checkout. Assets will be loaded before being checkout.
	 * @param	DirectoryPath		Directory of the assets that to checkout.
	 * @param	bRecursive			If the AssetPath is a folder, the search will be recursive and will checkout the asset in the sub folders.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool CheckoutDirectory(const FString& DirectoryPath, bool bRecursive = true);

public:
	/**
	 * Save the packages the assets live in. All objects that live in the package will be saved. Will try to checkout the file.
	 * @param	AssetToSave			Asset that we want to save.
	 * @param	bOnlyIfIsDirty		Only checkout asset that are dirty.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool SaveLoadedAsset(UObject* AssetToSave, bool bOnlyIfIsDirty = true);

	/**
	 * Save the packages the assets live in. All objects that live in the package will be saved. Will try to checkout the files.
	 * @param	AssetToSaves		Assets that we want to save.
	 * @param	bOnlyIfIsDirty		Only checkout asset that are dirty.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool SaveLoadedAssets(const TArray<UObject*>& AssetsToSave, bool bOnlyIfIsDirty = true);

	/**
	 * Save the packages the assets live in. All objects that live in the package will be saved.
	 * Will try to checkout the file first. The Asset will be loaded before being saved.
	 * @param	AssetsToSave		Asset Path of the asset that we want to save.
	 * @param	bOnlyIfIsDirty		Only checkout/save the asset if it's dirty.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool SaveAsset(const FString& AssetToSave, bool bOnlyIfIsDirty = true);

	/**
	 * Save the packages the assets live in inside the directory. All objects that are in the directory will be saved.
	 * Will try to checkout the file first. Assets will be loaded before being saved.
	 * @param	DirectoryPath		Directory that will be checked out and saved.
	 * @param	bOnlyIfIsDirty		Only checkout asset that are dirty.
	 * @param	bRecursive			The search will be recursive and it will save the asset in the sub folders.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool SaveDirectory(const FString& DirectoryPath, bool bOnlyIfIsDirty = true, bool bRecursive = true);

public:
	/**
	* Check is the directory exist in the Content Browser.
	* @param	DirectoryPath		Long Path Name of the directory.
	* @return	True if it does exist and it is valid.
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool DoesDirectoryExist(const FString& DirectoryPath);

	/**
	 * Check if there any asset that exist in the directory.
	 * @param	DirectoryPath		Long Path Name of the directory.
	 * @return	True if there is any assets.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool DoesDirectoryHaveAssets(const FString& DirectoryPath, bool bRecursive = true);

	/**
	 * Create the directory on disk and in the Content Browser.
	 * @param	DirectoryPath		Long Path Name of the directory.
	 * @return	True if the operation succeeds.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static bool MakeDirectory(const FString& DirectoryPath);

public:
	/**
	 * Return the list of all the assets found in the DirectoryPath.
	 * @param	DirectoryPath		Directory path of the asset we want the list from.
	 * @param	bRecursive			The search will be recursive and will look in sub folders.
	 * @param	bIncludeFolder		The result will include folders name.
	 * @return	The list of asset found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static TArray<FString> ListAssets(const FString& DirectoryPath, bool bRecursive = true, bool bIncludeFolder = false);

	/**
	 * Return the list of all the assets that have the pair of Tag/Value.
	 * @param TagName	The tag associated with the assets requested.
	 * @param TagValue	The value associated with the assets requested.
	 * @return	The list of asset found.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static TArray<FString> ListAssetByTagValue(FName TagName, const FString& TagValue);

	/**
	 * Gets all TagValues (from Asset Registry) associated with an (unloaded) asset as strings value.
	 * @param	AssetPath		Asset Path we are trying to find.
	 * @return	The list of all TagName & TagValue.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset")
	static TMap<FName, FString> GetTagValues(const FString& AssetPath);

public:
	/**
	 * Get all tags/values of a loaded asset's metadata.
	 * @param	Object		The object from which to retrieve the metadata.
	 * @return	The list of all Tags and Values.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Metadata")
	static TMap<FName, FString> GetMetadataTagValues(UObject* Object);

	/**
	 * Get the value associated with the given tag of a loaded asset's metadata.
	 * @param	Object		The object from which to retrieve the metadata.
	 * @param	Tag			The tag to find in the metadata.
	 * @return	The string value associated with the tag.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Metadata")
	static FString GetMetadataTag(UObject* Object, FName Tag);

	/**
	 * Set the value associated with a given tag of a loaded asset's metadata.
	 * @param	Object		The object from which to retrieve the metadata.
	 * @param	Tag			The tag to set in the metadata.
	 * @param	Value		The string value to associate with the tag.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Metadata")
	static void SetMetadataTag(UObject* Object, FName Tag, const FString& Value);

	/**
	 * Remove the given tag from a loaded asset's metadata.
	 * @param	Object		The object from which to retrieve the metadata.
	 * @param	Tag			The tag to remove from the metadata.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Metadata")
	static void RemoveMetadataTag(UObject* Object, FName Tag);

public:
	/**
	 * Sync the Content Browser to the given asset(s)
	 * @param	AssetPaths	The list of asset paths to sync to in the Content Browser
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Content Browser")
	static void SyncBrowserToObjects(const TArray<FString>& AssetPaths);
};

