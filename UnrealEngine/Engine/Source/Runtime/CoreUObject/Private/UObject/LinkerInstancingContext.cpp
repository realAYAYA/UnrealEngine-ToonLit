// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/LinkerInstancingContext.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/NameTypes.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Templates/Tuple.h"
#include "Misc/AutomationTest.h"

LLM_DEFINE_TAG(Loading_LinkerInstancingContext);

class FLinkerInstancingContext::FSharedLinkerInstancingContextData
{
	/** The shared content needs to be protected as it could be modified by the caller as well as the loading thread */
	mutable FRWLock Lock;
	/** Map of original package name to their instance counterpart. */
	FLinkerInstancedPackageMap InstancedPackageMap;
	/** Optional function to map original package name to their instance counterpart. The result of this function should be immutable, as it will be cached. */
	TFunction<FName(FName)> InstancedPackageMapFunc;
	/** Map of original top level asset path to their instance counterpart. */
	TMap<FTopLevelAssetPath, FTopLevelAssetPath> PathMapping;
	/** Tags can be used to determine some loading behavior. */
	TSet<FName> Tags;
	/** Remap soft object paths */
	std::atomic<bool> bSoftObjectPathRemappingEnabled { true };

public:
	FSharedLinkerInstancingContextData() = default;
	explicit FSharedLinkerInstancingContextData(TSet<FName> InTags)
		: Tags(InTags)
	{
	}

	explicit FSharedLinkerInstancingContextData(bool bInSoftObjectPathRemappingEnabled)
		: bSoftObjectPathRemappingEnabled(bInSoftObjectPathRemappingEnabled)
	{
	}

	explicit FSharedLinkerInstancingContextData(const FSharedLinkerInstancingContextData& Other)
		: InstancedPackageMap(Other.InstancedPackageMap)
		, InstancedPackageMapFunc(Other.InstancedPackageMapFunc)
		, PathMapping(Other.PathMapping)
		, Tags(Other.Tags)
		, bSoftObjectPathRemappingEnabled(Other.GetSoftObjectPathRemappingEnabled())
	{
	}
		
	void AddPackageMapping(FName Original, FName Instanced)
	{
		FWriteScopeLock ScopeLock(Lock);
		LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
		InstancedPackageMap.AddPackageMapping(Original, Instanced);
	}

	bool FindPackageMapping(FName Original, FName& Instanced) const
	{
		FReadScopeLock ScopeLock(Lock);
		LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
		if (const FName* InstancedPtr = InstancedPackageMap.InstancedPackageMapping.Find(Original))
		{
			Instanced = *InstancedPtr;
			return true;
		}
		return false;
	}

	void FixupSoftObjectPath(FSoftObjectPath& InOutSoftObjectPath) const
	{
		FReadScopeLock ScopeLock(Lock);
		LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
		InstancedPackageMap.FixupSoftObjectPath(InOutSoftObjectPath);
	}

	FSoftObjectPath RemapPath(const FSoftObjectPath& Path) const
	{
		FReadScopeLock ScopeLock(Lock);
		if (const FTopLevelAssetPath* Remapped = PathMapping.Find(Path.GetAssetPath()))
		{
			return FSoftObjectPath(*Remapped, Path.GetSubPathString());
		}
		return Path;
	}

	bool IsInstanced() const
	{
		FReadScopeLock ScopeLock(Lock);
		return InstancedPackageMap.IsInstanced() || InstancedPackageMapFunc || PathMapping.Num() > 0;
	}

	void EnableAutomationTest()
	{
		FWriteScopeLock ScopeLock(Lock);
		InstancedPackageMap.EnableAutomationTest();
	}

	void BuildPackageMapping(FName Original, FName Instanced, bool bInSoftObjectPathRemappingEnabled)
	{
		FWriteScopeLock ScopeLock(Lock);
		LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
		InstancedPackageMap.BuildPackageMapping(Original, Instanced, bInSoftObjectPathRemappingEnabled);
	}

	void AddTag(FName NewTag)
	{
		FWriteScopeLock ScopeLock(Lock);
		LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
		Tags.Add(NewTag);
	}

	void AppendTags(const TSet<FName>& NewTags)
	{
		FWriteScopeLock ScopeLock(Lock);
		LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
		Tags.Append(NewTags);
	}

	bool HasTag(FName Tag) const
	{
		FReadScopeLock ScopeLock(Lock);
		return Tags.Contains(Tag);
	}

	void SetSoftObjectPathRemappingEnabled(bool bInSoftObjectPathRemappingEnabled)
	{
		bSoftObjectPathRemappingEnabled = bInSoftObjectPathRemappingEnabled;
	}

	bool GetSoftObjectPathRemappingEnabled() const
	{
		return bSoftObjectPathRemappingEnabled;
	}

	void AddPathMapping(FSoftObjectPath Original, FSoftObjectPath Instanced)
	{
		FWriteScopeLock ScopeLock(Lock);
		LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
		PathMapping.Emplace(Original.GetAssetPath(), Instanced.GetAssetPath());
	}

	void SetPackageMappingFunc(TFunction<FName(FName)> InInstancedPackageMapFunc)
	{
		FWriteScopeLock ScopeLock(Lock);
		InstancedPackageMapFunc = MoveTemp(InInstancedPackageMapFunc);
	}

	FName RemapPackage(const FName& PackageName)
	{
		bool bAddPackageMapping = false;

		FName RemappedPackageName;
		{
			FReadScopeLock ScopeLock(Lock);
			RemappedPackageName = InstancedPackageMap.RemapPackage(PackageName);

			if ((RemappedPackageName == PackageName) && InstancedPackageMapFunc)
			{
				RemappedPackageName = InstancedPackageMapFunc(PackageName);

				if (RemappedPackageName != PackageName)
				{
					bAddPackageMapping = true; 
				}
			}
		}

		if (bAddPackageMapping)
		{
			FWriteScopeLock ScopeLock(Lock);
			LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
			InstancedPackageMap.AddPackageMapping(PackageName, RemappedPackageName);
		}

		return RemappedPackageName;
	}
};

FLinkerInstancingContext::FLinkerInstancingContext()
{
	LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
	SharedData = MakeShared<FSharedLinkerInstancingContextData>();
}

FLinkerInstancingContext::FLinkerInstancingContext(TSet<FName> InTags)
{
	LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
	SharedData = MakeShared<FSharedLinkerInstancingContextData>(MoveTemp(InTags));
}

FLinkerInstancingContext::FLinkerInstancingContext(bool bInSoftObjectPathRemappingEnabled)
{
	LLM_SCOPE_BYTAG(Loading_LinkerInstancingContext);
	SharedData = MakeShared<FSharedLinkerInstancingContextData>(bInSoftObjectPathRemappingEnabled);
}

FLinkerInstancingContext FLinkerInstancingContext::DuplicateContext(const FLinkerInstancingContext& InLinkerInstancingContext)
{
	FLinkerInstancingContext OutLinkerInstancingContext = InLinkerInstancingContext;
	OutLinkerInstancingContext.SharedData = MakeShared<FSharedLinkerInstancingContextData>(*OutLinkerInstancingContext.SharedData.Get());
	return OutLinkerInstancingContext;
}

void FLinkerInstancingContext::EnableAutomationTest() 
{ 
	SharedData->EnableAutomationTest();
}

void FLinkerInstancingContext::BuildPackageMapping(FName Original, FName Instanced)
{
	SharedData->BuildPackageMapping(Original, Instanced, GetSoftObjectPathRemappingEnabled());
}

bool FLinkerInstancingContext::FindPackageMapping(FName Original, FName& Instanced) const
{
	return SharedData->FindPackageMapping(Original, Instanced);
}

bool FLinkerInstancingContext::IsInstanced() const
{
	return SharedData->IsInstanced();
}

/** Remap the package name from the import table to its instanced counterpart, otherwise return the name unmodified. */
FName FLinkerInstancingContext::RemapPackage(const FName& PackageName) const
{
	return SharedData->RemapPackage(PackageName);
}

/**
 * Remap the top level asset part of the path name to its instanced counterpart, otherwise return the name unmodified.
 * i.e. remaps /Path/To/Package.AssetName:Inner to /NewPath/To/NewPackage.NewAssetName:Inner
 */
FSoftObjectPath FLinkerInstancingContext::RemapPath(const FSoftObjectPath& Path) const
{
	return SharedData->RemapPath(Path);
}

/** Add a mapping from a package name to a new package name. There should be no separators (. or :) in these strings. */
void FLinkerInstancingContext::AddPackageMapping(FName Original, FName Instanced)
{
	SharedData->AddPackageMapping(Original, Instanced);
}

/** Add a mapping function from a package name to a new package name. This function should be thread-safe, as it can be invoked from ALT. */
void FLinkerInstancingContext::AddPackageMappingFunc(TFunction<FName(FName)> InInstancedPackageMapFunc)
{
	SharedData->SetPackageMappingFunc(InInstancedPackageMapFunc);
}

/** Add a mapping from a top level asset path (/Path/To/Package.AssetName) to another. */
void FLinkerInstancingContext::AddPathMapping(FSoftObjectPath Original, FSoftObjectPath Instanced)
{
	ensureAlwaysMsgf(Original.GetSubPathString().IsEmpty(),
		TEXT("Linker instance remap paths should be top-level assets only: %s->"), *Original.ToString());
	ensureAlwaysMsgf(Instanced.GetSubPathString().IsEmpty(),
		TEXT("Linker instance remap paths should be top-level assets only: ->%s"), *Instanced.ToString());

	SharedData->AddPathMapping(Original, Instanced);
}

void FLinkerInstancingContext::AddTag(FName NewTag)
{
	SharedData->AddTag(NewTag);
}

void FLinkerInstancingContext::AppendTags(const TSet<FName>& NewTags)
{
	SharedData->AppendTags(NewTags);
}

bool FLinkerInstancingContext::HasTag(FName Tag) const
{
	return SharedData->HasTag(Tag);
}

void FLinkerInstancingContext::SetSoftObjectPathRemappingEnabled(bool bInSoftObjectPathRemappingEnabled)
{
	SharedData->SetSoftObjectPathRemappingEnabled(bInSoftObjectPathRemappingEnabled);
}

bool FLinkerInstancingContext::GetSoftObjectPathRemappingEnabled() const
{
	return SharedData->GetSoftObjectPathRemappingEnabled();
}

void FLinkerInstancingContext::FixupSoftObjectPath(FSoftObjectPath& InOutSoftObjectPath) const
{
	if (IsInstanced() && GetSoftObjectPathRemappingEnabled())
	{
		// Try remapping AssetPathName before remapping LongPackageName
		if (FSoftObjectPath RemappedAssetPath = RemapPath(InOutSoftObjectPath); RemappedAssetPath != InOutSoftObjectPath)
		{
			InOutSoftObjectPath = RemappedAssetPath;
		}
		else
		{
			SharedData->FixupSoftObjectPath(InOutSoftObjectPath);
		}
	}
}

void FLinkerInstancedPackageMap::AddPackageMapping(FName Original, FName Instanced)
{
	if (InstanceMappingDirection == EInstanceMappingDirection::OriginalToInstanced)
	{
		if (!InstancedPackageMapping.Contains(Original))
		{
			InstancedPackageMapping.Add(Original, Instanced);
			bIsInstanced |= !Instanced.IsNone();
		}
	}
	else
	{
		check(InstanceMappingDirection == EInstanceMappingDirection::InstancedToOriginal);
		if (!InstancedPackageMapping.Contains(Instanced))
		{
			InstancedPackageMapping.Add(Instanced, Original);
			bIsInstanced |= !Original.IsNone();
		}
	}
}

void FLinkerInstancedPackageMap::BuildPackageMapping(FName Original, FName Instanced, const bool bBuildWorldPartitionCellMapping)
{
	check(GeneratedPackagesFolder.IsEmpty() && InstancedPackageSuffix.IsEmpty() && InstancedPackagePrefix.IsEmpty());

	AddPackageMapping(Original, Instanced);

	if (bBuildWorldPartitionCellMapping && bEnableNonEditorPath)
	{
#if WITH_EDITOR
		// This code path should only be enabled while testing
		check(GIsAutomationTesting);
#endif

		FNameBuilder TmpOriginal(Original);
		FNameBuilder TmpInstanced(Instanced);
		FStringView OriginalView = TmpOriginal.ToView();
		FStringView InstancedView = TmpInstanced.ToView();

		const int32 Index = InstancedView.Find(OriginalView, 0, ESearchCase::IgnoreCase);

		// Stash the suffix used for this instance so we can also apply it to generated packages
		if (Index != INDEX_NONE)
		{
			InstancedPackagePrefix = InstancedView.Mid(0, Index);
			InstancedPackageSuffix = InstancedView.Mid(Index + OriginalView.Len());
		}

		// Is this a generated partitioned map package? If so, we'll also need to handle re-mapping paths to our persistent map package
		if (!InstancedPackagePrefix.IsEmpty() || !InstancedPackageSuffix.IsEmpty())
		{
			const FStringView GeneratedFolderName = TEXTVIEW("/_Generated_/");
			
			// Does this package path include the generated folder?
			if (const int32 GeneratedFolderStartIndex = OriginalView.Find(GeneratedFolderName, 0, ESearchCase::IgnoreCase); GeneratedFolderStartIndex != INDEX_NONE)
			{
				// ... and is that generated folder immediately preceding the package name?
				const int32 GeneratedFolderEndIndex = GeneratedFolderStartIndex + GeneratedFolderName.Len();
				if (const int32 ExtraSlashIndex = OriginalView.Find(TEXTVIEW("/"), GeneratedFolderEndIndex, ESearchCase::IgnoreCase); ExtraSlashIndex == INDEX_NONE)
				{
					GeneratedPackagesFolder = OriginalView.Left(GeneratedFolderEndIndex);
					const FString PersistentSourcePackage(OriginalView.Left(GeneratedFolderStartIndex));
									
					FNameBuilder PersistentPackageInstanceNameBuilder;
					PersistentPackageInstanceNameBuilder.Append(InstancedPackagePrefix);
					PersistentPackageInstanceNameBuilder.Append(PersistentSourcePackage);
					PersistentPackageInstanceNameBuilder.Append(InstancedPackageSuffix);

					const FName PersistentPackageInstanceName(PersistentPackageInstanceNameBuilder);

					AddPackageMapping(*PersistentSourcePackage, PersistentPackageInstanceName);
				}
			}

			if (GeneratedPackagesFolder.IsEmpty())
			{
				GeneratedPackagesFolder = OriginalView;
				GeneratedPackagesFolder += GeneratedFolderName;
			}
		}
	}
}

bool FLinkerInstancedPackageMap::FixupSoftObjectPath(FSoftObjectPath& InOutSoftObjectPath) const
{
	if (IsInstanced())
	{
		if (FName LongPackageName = InOutSoftObjectPath.GetLongPackageFName(), RemappedPackage = RemapPackage(LongPackageName); RemappedPackage != LongPackageName)
		{
			InOutSoftObjectPath = FSoftObjectPath(RemappedPackage, InOutSoftObjectPath.GetAssetFName(), InOutSoftObjectPath.GetSubPathString());
			return true;
		}
		else if (!GeneratedPackagesFolder.IsEmpty())
		{
#if WITH_EDITOR
			// This code path should only be enabled while testing
			check(GIsAutomationTesting);
#endif
			check(!InstancedPackagePrefix.IsEmpty() || !InstancedPackageSuffix.IsEmpty());

			FNameBuilder TmpSoftObjectPathBuilder;
			InOutSoftObjectPath.ToString(TmpSoftObjectPathBuilder);

			// Does this package path start with the generated folder path?
			FStringView TmpSoftObjectPathView = TmpSoftObjectPathBuilder.ToView();
			if (const int32 GeneratedFolderIndex = TmpSoftObjectPathView.Find(GeneratedPackagesFolder, 0, ESearchCase::IgnoreCase); GeneratedFolderIndex != INDEX_NONE)
			{
				check(GeneratedFolderIndex == 0 || (TmpSoftObjectPathView.StartsWith(InstancedPackagePrefix) && GeneratedFolderIndex == InstancedPackagePrefix.Len()));

				// ... and is that generated folder path immediately preceding the package name?
				if (const int32 ExtraSlashIndex = TmpSoftObjectPathView.Find(TEXTVIEW("/"), InstancedPackagePrefix.Len() + GeneratedPackagesFolder.Len(), ESearchCase::IgnoreCase); ExtraSlashIndex == INDEX_NONE)
				{
					FNameBuilder PackageNameBuilder;

					if (InstanceMappingDirection == EInstanceMappingDirection::OriginalToInstanced)
					{
						PackageNameBuilder.Append(InstancedPackagePrefix);
					}
					
					PackageNameBuilder.Append(InOutSoftObjectPath.GetLongPackageName().Mid(GeneratedFolderIndex));

					if (InstanceMappingDirection == EInstanceMappingDirection::OriginalToInstanced)
					{
						if (!PackageNameBuilder.ToView().EndsWith(InstancedPackageSuffix))
						{
							PackageNameBuilder.Append(InstancedPackageSuffix);
						}
					}
					else
					{
						check(InstanceMappingDirection == EInstanceMappingDirection::InstancedToOriginal);
						if (PackageNameBuilder.ToView().EndsWith(InstancedPackageSuffix))
						{
							PackageNameBuilder.RemoveSuffix(InstancedPackageSuffix.Len());
						}
					}
					FTopLevelAssetPath SuffixTopLevelAsset(FName(PackageNameBuilder), InOutSoftObjectPath.GetAssetFName());
					InOutSoftObjectPath = FSoftObjectPath(SuffixTopLevelAsset, InOutSoftObjectPath.GetSubPathString());
					return true;
				}
			}
		}
	}
	return false;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLinkerInstancingContextTests, "System.CoreUObject.LinkerInstancingContext", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter);
bool FLinkerInstancingContextTests::RunTest(const FString& Parameters)
{
	// Disabled SoftObjectPath Remapping
	{
		FName MappedSourcePackage(TEXT("/Game/PathA/PathB/PackageName"));
		FName UnmappedSourcePackage(TEXT("/Game/PathA/PathB/PackageNameOther"));
		FName InstancedPackage(TEXT("/Game/PathA/PathB/PackageName_Instance"));

		const FSoftObjectPath MappedSourcePackagePath(FTopLevelAssetPath(MappedSourcePackage, FName(TEXT("PackageName"))));
		const FSoftObjectPath MappedSourceObjectPath(FTopLevelAssetPath(MappedSourcePackage, FName(TEXT("PackageName"))), "PersistentLevel.Actor.Component");

		const bool bSoftObjectPathRemappingEnabled = false;
		FLinkerInstancingContext LinkerInstancingContext(bSoftObjectPathRemappingEnabled);
		LinkerInstancingContext.EnableAutomationTest();
		LinkerInstancingContext.BuildPackageMapping(MappedSourcePackage, InstancedPackage);

		TestEqual("RemapPackage with existing mapping - bSoftObjectPathRemappingEnabled(false)", LinkerInstancingContext.RemapPackage(MappedSourcePackage), InstancedPackage);
		TestEqual("RemapPackage without mapping - bSoftObjectPathRemappingEnabled(false)", LinkerInstancingContext.RemapPackage(UnmappedSourcePackage), UnmappedSourcePackage);

		FSoftObjectPath CopyMappedSourceObjectPackage = MappedSourcePackagePath;
		LinkerInstancingContext.FixupSoftObjectPath(CopyMappedSourceObjectPackage);
		TestEqual("FixupSoftObjectPath with no SubPathString - bSoftObjectPathRemappingEnabled(false)", CopyMappedSourceObjectPackage, MappedSourcePackagePath);

		FSoftObjectPath CopyMappedSourceObjectPath = MappedSourceObjectPath;
		LinkerInstancingContext.FixupSoftObjectPath(CopyMappedSourceObjectPath);
		TestEqual("FixupSoftObjectPath with SubPathString - bSoftObjectPathRemappingEnabled(false)", CopyMappedSourceObjectPath, MappedSourceObjectPath);
	}

	// Enabled SoftObjectPath Remapping
	{
		FName MappedSourcePackage(TEXT("/Game/PathA/PathB/PackageName"));
		FName UnmappedSourcePackage(TEXT("/Game/PathA/PathB/PackageNameOther"));
		FName InstancedPackage(TEXT("/Game/PathA/PathB/PackageName_Instance"));

		const FSoftObjectPath MappedSourcePackagePath(FTopLevelAssetPath(MappedSourcePackage, FName(TEXT("PackageName"))));
		const FSoftObjectPath RemappedPackagePath(FTopLevelAssetPath(InstancedPackage, MappedSourcePackagePath.GetAssetFName()));
		const FSoftObjectPath MappedSourceObjectPath(FTopLevelAssetPath(MappedSourcePackage, FName(TEXT("PackageName"))), "PersistentLevel.Actor.Component");
		const FSoftObjectPath RemappedObjectPath(FTopLevelAssetPath(InstancedPackage, MappedSourceObjectPath.GetAssetFName()), MappedSourceObjectPath.GetSubPathString());

		const bool bSoftObjectPathRemappingEnabled = true;
		FLinkerInstancingContext LinkerInstancingContext(bSoftObjectPathRemappingEnabled);
		LinkerInstancingContext.EnableAutomationTest();
		LinkerInstancingContext.BuildPackageMapping(MappedSourcePackage, InstancedPackage);

		TestEqual("RemapPackage with existing mapping - bSoftObjectPathRemappingEnabled(true)", LinkerInstancingContext.RemapPackage(MappedSourcePackage), InstancedPackage);
		TestEqual("RemapPackage without mapping - bSoftObjectPathRemappingEnabled(true)", LinkerInstancingContext.RemapPackage(UnmappedSourcePackage), UnmappedSourcePackage);

		FSoftObjectPath CopyMappedSourceObjectPackage = MappedSourcePackagePath;
		LinkerInstancingContext.FixupSoftObjectPath(CopyMappedSourceObjectPackage);
		TestEqual("FixupSoftObjectPath with no SubPathString - bSoftObjectPathRemappingEnabled(true)", CopyMappedSourceObjectPackage, RemappedPackagePath);

		FSoftObjectPath CopyMappedSourceObjectPath = MappedSourceObjectPath;
		LinkerInstancingContext.FixupSoftObjectPath(CopyMappedSourceObjectPath);
		TestEqual("FixupSoftObjectPath with SubPathString - bSoftObjectPathRemappingEnabled(true)", CopyMappedSourceObjectPath, RemappedObjectPath);
	}
	
	auto TestGeneratedPathFixup = [this](FName InSourceGeneratorPackage, FName InInstancedGeneratorPackage, FName InSourceGeneratedPackage, FName InInstancedGeneratedPackage, const FString& Description)
	{
		const bool bSoftObjectPathRemappingEnabled = true;
		FLinkerInstancingContext LinkerInstancingContext(bSoftObjectPathRemappingEnabled);
		LinkerInstancingContext.EnableAutomationTest();
		LinkerInstancingContext.BuildPackageMapping(InSourceGeneratedPackage, InInstancedGeneratedPackage);

		// Reverse mapping
		FLinkerInstancedPackageMap InstancingMap(FLinkerInstancedPackageMap::EInstanceMappingDirection::InstancedToOriginal);
		InstancingMap.EnableAutomationTest();
		InstancingMap.BuildPackageMapping(InSourceGeneratedPackage, InInstancedGeneratedPackage);

		TestEqual(FString::Format(TEXT("{0} - RemapPackage - _Generated_ Package with Suffix"), { Description }), LinkerInstancingContext.RemapPackage(InSourceGeneratedPackage), InInstancedGeneratedPackage);

		auto TestSoftObjectPathFixup = [this, &LinkerInstancingContext, &InstancingMap](const FSoftObjectPath& InSource, const FSoftObjectPath& InExpectedResult, const FString& Description)
		{
			FSoftObjectPath CopySource = InSource;
			LinkerInstancingContext.FixupSoftObjectPath(CopySource);
			TestEqual(FString::Format(TEXT("{0} - FixupSoftObjectPath"), { Description }), CopySource, InExpectedResult);

			LinkerInstancingContext.FixupSoftObjectPath(CopySource);
			TestEqual(FString::Format(TEXT("{0} - FixupSoftObjectPath already fixed up"), { Description }), CopySource, InExpectedResult);

			InstancingMap.FixupSoftObjectPath(CopySource);
			TestEqual(FString::Format(TEXT("{0} - Reverse FixupSoftObjectPath"), { Description }), CopySource, InSource);
		};

		const FSoftObjectPath SourcePackagePath(FTopLevelAssetPath(InSourceGeneratorPackage, FName(TEXT("PackageName"))));
		const FSoftObjectPath RemappedPackagePath(FTopLevelAssetPath(InInstancedGeneratorPackage, SourcePackagePath.GetAssetFName()));
		TestSoftObjectPathFixup(SourcePackagePath, RemappedPackagePath, Description);

		const FSoftObjectPath SourceObjectPath(FTopLevelAssetPath(InSourceGeneratorPackage, FName(TEXT("PackageName"))), "PersistentLevel.Actor.Component");
		const FSoftObjectPath RemappedSourceObjectPath(FTopLevelAssetPath(InInstancedGeneratorPackage, SourceObjectPath.GetAssetFName()), SourceObjectPath.GetSubPathString());
		TestSoftObjectPathFixup(SourceObjectPath, RemappedSourceObjectPath, Description);

		const FSoftObjectPath SourceGeneratedPackagePath(FTopLevelAssetPath(InSourceGeneratedPackage, FName(TEXT("PackageName"))));
		const FSoftObjectPath RemappedGeneratedPackagePath(FTopLevelAssetPath(InInstancedGeneratedPackage, SourceGeneratedPackagePath.GetAssetFName()));
		TestSoftObjectPathFixup(SourceGeneratedPackagePath, RemappedGeneratedPackagePath, Description);

		const FSoftObjectPath SourceGeneratedObjectPath(FTopLevelAssetPath(InSourceGeneratedPackage, FName(TEXT("PackageName"))), "PersistentLevel.Actor.Component");
		const FSoftObjectPath RemappedSourceGeneratedObjectPath(FTopLevelAssetPath(InInstancedGeneratedPackage, SourceGeneratedObjectPath.GetAssetFName()), SourceGeneratedObjectPath.GetSubPathString());
		TestSoftObjectPathFixup(SourceGeneratedObjectPath, RemappedSourceGeneratedObjectPath, Description);
	};

	{
		// Generated Package Mapping (Suffix)
		FName SourcePackage(TEXT("/Game/PathA/PathB/PackageName"));
		FName InstancedPackage(TEXT("/Game/PathA/PathB/PackageName_LevelInstance1"));
		FName SourceGeneratedPackage(TEXT("/Game/PathA/PathB/PackageName/_Generated_/1AEHCD0PRR98XVM12PU4N1D8X"));
		FName InstancedGeneratedPackage(TEXT("/Game/PathA/PathB/PackageName/_Generated_/1AEHCD0PRR98XVM12PU4N1D8X_LevelInstance1"));

		TestGeneratedPathFixup(SourcePackage, InstancedPackage, SourceGeneratedPackage, InstancedGeneratedPackage, TEXT("Suffix"));
	}

	{
		// Generated Package Mapping (Prefix)
		FName SourcePackage(TEXT("/Game/PathA/PathB/PackageName"));
		FName InstancedPackage(TEXT("/Temp/Game/PathA/PathB/PackageName"));
		FName SourceGeneratedPackage(TEXT("/Game/PathA/PathB/PackageName/_Generated_/1AEHCD0PRR98XVM12PU4N1D8X"));
		FName InstancedGeneratedPackage(TEXT("/Temp/Game/PathA/PathB/PackageName/_Generated_/1AEHCD0PRR98XVM12PU4N1D8X"));

		TestGeneratedPathFixup(SourcePackage, InstancedPackage, SourceGeneratedPackage, InstancedGeneratedPackage, TEXT("Prefix"));
	}

	{
		// Generated Package Mapping (Prefix + Suffix)
		FName SourcePackage(TEXT("/Game/PathA/PathB/PackageName"));
		FName InstancedPackage(TEXT("/Temp/Game/PathA/PathB/PackageName_LevelInstance1"));
		FName SourceGeneratedPackage(TEXT("/Game/PathA/PathB/PackageName/_Generated_/1AEHCD0PRR98XVM12PU4N1D8X"));
		FName InstancedGeneratedPackage(TEXT("/Temp/Game/PathA/PathB/PackageName/_Generated_/1AEHCD0PRR98XVM12PU4N1D8X_LevelInstance1"));

		TestGeneratedPathFixup(SourcePackage, InstancedPackage, SourceGeneratedPackage, InstancedGeneratedPackage, TEXT("Suffix+Prefix"));
	}

	return true;
}

