// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/NamePermissionList.h"

#include "ToolMenuBase.generated.h"

class FMultiBox;

UENUM()
enum class ECustomizedToolMenuVisibility
{
	None,
	Visible,
	Hidden
};

USTRUCT()
struct FCustomizedToolMenuEntry
{
	GENERATED_BODY()

	FCustomizedToolMenuEntry() :
		Visibility(ECustomizedToolMenuVisibility::None)
	{
	}

	UPROPERTY()
	ECustomizedToolMenuVisibility Visibility;
};

USTRUCT()
struct FCustomizedToolMenuSection
{
	GENERATED_BODY()

	FCustomizedToolMenuSection() :
		Visibility(ECustomizedToolMenuVisibility::None)
	{
	}

	UPROPERTY()
	ECustomizedToolMenuVisibility Visibility;
};

USTRUCT()
struct FCustomizedToolMenuNameArray
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<FName> Names;
};

USTRUCT()
struct FCustomizedToolMenu
{
	GENERATED_BODY()

	UPROPERTY()
	FName Name;

	UPROPERTY()
	TMap<FName, FCustomizedToolMenuEntry> Entries;

	UPROPERTY()
	TMap<FName, FCustomizedToolMenuSection> Sections;

	UPROPERTY()
	TMap<FName, FCustomizedToolMenuNameArray> EntryOrder;

	UPROPERTY()
	TArray<FName> SectionOrder;

	UPROPERTY()
	TArray<FName> SuppressExtenders;

	FNamePermissionList MenuPermissions;

	SLATE_API FCustomizedToolMenuEntry* FindEntry(const FName InEntryName);
	SLATE_API const FCustomizedToolMenuEntry* FindEntry(const FName InEntryName) const;
	SLATE_API FCustomizedToolMenuEntry* AddEntry(const FName InEntryName);
	SLATE_API ECustomizedToolMenuVisibility GetEntryVisiblity(const FName InSectionName) const;
	SLATE_API bool IsEntryHidden(const FName InEntryName) const;
	SLATE_API FName GetEntrySectionName(const FName InEntryName) const;

	SLATE_API FCustomizedToolMenuSection* FindSection(const FName InSectionName);
	SLATE_API const FCustomizedToolMenuSection* FindSection(const FName InSectionName) const;
	SLATE_API FCustomizedToolMenuSection* AddSection(const FName InSectionName);
	SLATE_API ECustomizedToolMenuVisibility GetSectionVisiblity(const FName InSectionName) const;
	SLATE_API bool IsSectionHidden(const FName InSectionName) const;

	SLATE_API void SetSuppressExtenders(const FName InOwnerName, const bool bInSuppress);
	SLATE_API bool IsSuppressExtenders() const;
};

struct FCustomizedToolMenuHierarchy
{
	SLATE_API FName GetEntrySectionName(const FName InEntryName) const;
	SLATE_API bool IsEntryHidden(const FName InEntryName) const;

	SLATE_API bool IsSectionHidden(const FName InSectionName) const;

	SLATE_API bool IsSuppressExtenders() const;

	SLATE_API FCustomizedToolMenu GenerateFlattened() const;

	TArray<const FCustomizedToolMenu*> Hierarchy;
	TArray<const FCustomizedToolMenu*> RuntimeHierarchy;
};

UCLASS(Abstract, MinimalAPI)
class UToolMenuBase : public UObject
{
	GENERATED_BODY()

public:

	virtual bool IsEditing() const { return false; }
	virtual FName GetSectionName(const FName InEntryName) const { return NAME_None; }

	virtual bool ContainsSection(const FName InName) const { return false; }
	virtual bool ContainsEntry(const FName InName) const { return false; }

	virtual FCustomizedToolMenu* FindMenuCustomization() const { return nullptr; }
	virtual FCustomizedToolMenu* AddMenuCustomization() const { return nullptr; }
	virtual FCustomizedToolMenuHierarchy GetMenuCustomizationHierarchy() const { return FCustomizedToolMenuHierarchy(); }
	virtual void UpdateMenuCustomizationFromMultibox(const TSharedRef<const FMultiBox>& InMultiBox) {}
	virtual void OnMenuDestroyed() {}
};

