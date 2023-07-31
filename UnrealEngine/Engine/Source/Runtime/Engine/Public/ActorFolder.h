// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "Folder.h"

#include "ActorFolder.generated.h"

class ULevel;
struct FActorFolderDesc;

UCLASS(Within = Level)
class ENGINE_API UActorFolder : public UObject
{
	GENERATED_BODY()

#if WITH_EDITOR
public:
	// Helper that creates a new UActorFolder
	static UActorFolder* Create(ULevel* InOuter, const FString& InFolderLabel, UActorFolder* InParent);

	static EPackageFlags GetExternalPackageFlags() { return (PKG_EditorOnly | PKG_ContainsMapData | PKG_NewlyCreated); }
	
	// Returns actor folder info stored in its package
	static FActorFolderDesc GetAssetRegistryInfoFromPackage(FName PackageName);
	
	//~ Begin UObject
	virtual bool IsAsset() const override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject

	void SetParent(UActorFolder* InParent);
	UActorFolder* GetParent(bool bSkipDeleted = true) const;

	void SetLabel(const FString& InFolderLabel);
	const FString& GetLabel() const { return FolderLabel; }

	// Control whether the folder is initially expanded or not
	void SetIsInitiallyExpanded(bool bInFolderInitiallyExpanded);
	bool IsInitiallyExpanded() const { return bFolderInitiallyExpanded; }

	const FGuid& GetGuid() const { return FolderGuid; }
	FName GetPath() const;
	FString GetDisplayName() const;

	void MarkAsDeleted();
	bool IsMarkedAsDeleted() const { return bIsDeleted; }
	
	// Checks if folder is valid (if it's not deleted)
	bool IsValid() const { return !IsMarkedAsDeleted(); }

	// Remaps parent folder to the first parent folder not marked as deleted
	void Fixup();
	// Detects and clears invalid parent folder
	void FixupParentFolder();

	FFolder GetFolder() const;

	/**
	 * Set the folder packaging mode.
	* @param bExternal will set the folder packaging mode to external if true, to internal otherwise
	* @param bShouldDirty should dirty or not the level package
	*/
	void SetPackageExternal(bool bExternal, bool bShouldDirty = true);

private:
	FName GetPathInternal(UActorFolder* InSkipFolder) const;
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