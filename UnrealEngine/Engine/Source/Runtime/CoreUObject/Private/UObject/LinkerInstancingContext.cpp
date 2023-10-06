// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/LinkerInstancingContext.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/NameTypes.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Templates/Tuple.h"
#include "Misc/AutomationTest.h"

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
			InstancedPackageMap.FixupSoftObjectPath(InOutSoftObjectPath);
		}
	}
}

void FLinkerInstancedPackageMap::AddPackageMapping(FName Original, FName Instanced)
{
	if (InstanceMappingDirection == EInstanceMappingDirection::OriginalToInstanced)
	{
		InstancedPackageMapping.Add(Original, Instanced);
	}
	else
	{
		check(InstanceMappingDirection == EInstanceMappingDirection::InstancedToOriginal);
		InstancedPackageMapping.Add(Instanced, Original);
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

		const int32 Index = InstancedView.Find(OriginalView);

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
			if (const int32 GeneratedFolderStartIndex = OriginalView.Find(GeneratedFolderName); GeneratedFolderStartIndex != INDEX_NONE)
			{
				// ... and is that generated folder immediately preceding the package name?
				const int32 GeneratedFolderEndIndex = GeneratedFolderStartIndex + GeneratedFolderName.Len();
				if (const int32 ExtraSlashIndex = OriginalView.Find(TEXTVIEW("/"), GeneratedFolderEndIndex); ExtraSlashIndex == INDEX_NONE)
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
			if (const int32 GeneratedFolderIndex = TmpSoftObjectPathView.Find(GeneratedPackagesFolder); GeneratedFolderIndex != INDEX_NONE)
			{
				check(GeneratedFolderIndex == 0 || (TmpSoftObjectPathView.StartsWith(InstancedPackagePrefix) && GeneratedFolderIndex == InstancedPackagePrefix.Len()));

				// ... and is that generated folder path immediately preceding the package name?
				if (const int32 ExtraSlashIndex = TmpSoftObjectPathView.Find(TEXTVIEW("/"), InstancedPackagePrefix.Len() + GeneratedPackagesFolder.Len()); ExtraSlashIndex == INDEX_NONE)
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

