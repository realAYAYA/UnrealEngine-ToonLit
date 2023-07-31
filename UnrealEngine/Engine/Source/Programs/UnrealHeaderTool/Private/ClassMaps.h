// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StringUtils.h"
#include "UnrealSourceFile.h"
#include "UnrealTypeDefinitionInfo.h"
#include "UObject/Stack.h"

class UField;
class UClass;
class FProperty;
class UPackage;
class UEnum;
class FArchive;
struct FManifestModule;
class FUnrealSourceFile;
class FUnrealTypeDefinitionInfo;

// Helper class to support freezing of the container
struct FFreezableContainer
{
public:
	void Freeze()
	{
		bFrozen = true;
	}

protected:
	bool bFrozen = false;
};

// Wrapper class around TypeDefinition map so we can maintain a parallel by name map
struct FTypeDefinitionInfoMap
{
	void AddNameLookup(FUnrealObjectDefinitionInfo& Definition)
	{
		DefinitionsByName.Add(Definition.GetFName(), Definition.AsShared());
	}

	// Finding by name must be done on the stripped name for classes and script structs
	TSharedRef<FUnrealTypeDefinitionInfo>* FindByName(const TCHAR* Name)
	{
		FName SearchName(Name, EFindName::FNAME_Find);
		if (SearchName != NAME_None)
		{
			return DefinitionsByName.Find(SearchName);
		}
		return nullptr;
	}

	template<typename To>
	To* FindByName(const TCHAR* Name)
	{
		return UHTCast<To>(FindByName(Name));
	}

	FUnrealTypeDefinitionInfo& FindByNameChecked(const TCHAR* Name)
	{
		TSharedRef<FUnrealTypeDefinitionInfo>* TypeDef = FindByName(Name);
		check(TypeDef);
		return **TypeDef;
	}

	template <typename To>
	To& FindByNameChecked(const TCHAR* Name)
	{
		To* TypeDef = FindByName<To>(Name);
		check(TypeDef);
		return *TypeDef;
	}

	template <typename Lambda>
	void ForAllTypesByName(Lambda&& InLambda)
	{
		for (const TPair<FName, TSharedRef<FUnrealTypeDefinitionInfo>>& KVP : DefinitionsByName)
		{
			InLambda(*KVP.Value);
		}
	}

	template <typename Type, typename Lambda>
	Type* Find(Lambda&& InLambda)
	{
		for (const TPair<FName, TSharedRef<FUnrealTypeDefinitionInfo>>& KVP : DefinitionsByName)
		{
			if (Type* TypeDef = UHTCast<Type>(*KVP.Value))
			{
				if (InLambda(*TypeDef))
				{
					return TypeDef;
				}
			}
		}
		return nullptr;
	}

	void Reset()
	{
		DefinitionsByName.Reset();
	}

private:
	TMap<FName, TSharedRef<FUnrealTypeDefinitionInfo>> DefinitionsByName;
};

// Wrapper class around SourceFiles map so we can quickly get a list of source files for a given package
struct FUnrealSourceFiles : public FFreezableContainer
{
	TSharedPtr<FUnrealSourceFile> AddByHash(uint32 Hash, FString&& Filename, TSharedRef<FUnrealSourceFile> SourceFile)
	{
		check(!bFrozen);
		TSharedRef<FUnrealSourceFile>* Existing = SourceFilesByString.FindByHash(Hash, Filename);
		TSharedPtr<FUnrealSourceFile> Return(Existing != nullptr ? TSharedPtr<FUnrealSourceFile>(*Existing) : TSharedPtr<FUnrealSourceFile>());
		AllSourceFiles.Add(&SourceFile.Get());
		SourceFilesByString.AddByHash(Hash, MoveTemp(Filename), MoveTemp(SourceFile));
		return Return;
	}
	const TSharedRef<FUnrealSourceFile>* Find(const FString& Id) const 
	{
		check(bFrozen);
		return SourceFilesByString.Find(Id);
	}
	const TArray<FUnrealSourceFile*>& GetAllSourceFiles() const
	{
		check(bFrozen);
		return AllSourceFiles;
	}

	void Reset()
	{
		AllSourceFiles.Reset();
		SourceFilesByString.Reset();
	}

private:
	// A map of all source files indexed by string.
	TMap<FString, TSharedRef<FUnrealSourceFile>> SourceFilesByString;

	// Total collection of sources
	TArray<FUnrealSourceFile*> AllSourceFiles;
};

extern FUnrealSourceFiles GUnrealSourceFilesMap;
extern FTypeDefinitionInfoMap GTypeDefinitionInfoMap;
