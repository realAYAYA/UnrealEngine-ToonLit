// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Folder.h"

#include "ActorFolder.generated.h"

class ULevel;
struct FActorFolderDesc;

UCLASS(Within = Level, MinimalAPI)
class UActorFolder : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
public:
	// Helper that creates a new UActorFolder
	static ENGINE_API UActorFolder* Create(ULevel* InOuter, const FString& InFolderLabel, UActorFolder* InParent);

	UE_DEPRECATED(5.4, "This function is deprecated.")
	static EPackageFlags GetExternalPackageFlags() { return (PKG_EditorOnly | PKG_ContainsMapData | PKG_NewlyCreated); }
	
	// Returns actor folder info stored in its package
	static ENGINE_API FActorFolderDesc GetAssetRegistryInfoFromPackage(FName PackageName);
	
	//~ Begin UObject
	ENGINE_API virtual bool IsAsset() const override;
	ENGINE_API virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	ENGINE_API virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	ENGINE_API virtual void PostLoad() override;
	//~ End UObject

	ENGINE_API void SetParent(UActorFolder* InParent);
	ENGINE_API UActorFolder* GetParent(bool bSkipDeleted = true) const;

	ENGINE_API void SetLabel(const FString& InFolderLabel);
	const FString& GetLabel() const { return FolderLabel; }

	// Control whether the folder is initially expanded or not
	ENGINE_API void SetIsInitiallyExpanded(bool bInFolderInitiallyExpanded);
	bool IsInitiallyExpanded() const { return bFolderInitiallyExpanded; }

	const FGuid& GetGuid() const { return FolderGuid; }
	ENGINE_API FName GetPath() const;
	ENGINE_API FString GetDisplayName() const;

	ENGINE_API void MarkAsDeleted();
	bool IsMarkedAsDeleted() const { return bIsDeleted; }
	
	// Checks if folder is valid (if it's not deleted)
	bool IsValid() const { return !IsMarkedAsDeleted(); }

	// Remaps parent folder to the first parent folder not marked as deleted
	ENGINE_API void Fixup();
	// Detects and clears invalid parent folder
	ENGINE_API void FixupParentFolder();

	ENGINE_API FFolder GetFolder() const;

	/**
	 * Set the folder packaging mode.
	* @param bExternal will set the folder packaging mode to external if true, to internal otherwise
	* @param bShouldDirty should dirty or not the level package
	*/
	ENGINE_API void SetPackageExternal(bool bExternal, bool bShouldDirty = true);

private:
	ENGINE_API FName GetPathInternal(UActorFolder* InSkipFolder) const;
#endif

#if WITH_EDITORONLY_DATA
private:
	UPROPERTY()
	FGuid ParentFolderGuid;

	UPROPERTY()
	FGuid FolderGuid;

	UPROPERTY()
	FString FolderLabel;

	UPROPERTY()
	bool bFolderInitiallyExpanded = true;

	UPROPERTY()
	bool bIsDeleted;
#endif
};
