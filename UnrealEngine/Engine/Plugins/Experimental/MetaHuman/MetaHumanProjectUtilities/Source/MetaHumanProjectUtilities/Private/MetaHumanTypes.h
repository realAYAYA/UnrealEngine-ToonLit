// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Misc/Paths.h"

// Common data types used in various parts of the MetaHumanProjectUtilities module

struct FMetaHumanAssetImportDescription;

enum class EQualityLevel: int
{
	Low,
	Medium,
	High
};

struct FMetaHumanAssetVersion
{
	int32 Major;
	int32 Minor;

	// Comparison operators
	friend bool operator <(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
	{
		return Left.Major < Right.Major || (Left.Major == Right.Major && Left.Minor < Right.Minor);
	}

	friend bool operator>(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right) { return Right < Left; }
	friend bool operator<=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right) { return !(Left > Right); }
	friend bool operator>=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right) { return !(Left < Right); }

	friend bool operator ==(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right)
	{
		return Right.Major == Left.Major && Right.Minor == Left.Minor;
	}

	friend bool operator!=(const FMetaHumanAssetVersion& Left, const FMetaHumanAssetVersion& Right) { return !(Left == Right); }

	static FMetaHumanAssetVersion FromString(const FString& String)
	{
		FString MajorPart;
		FString MinorPart;
		String.Split(TEXT("."), &MajorPart, &MinorPart);
		return FMetaHumanAssetVersion{FCString::Atoi(*MajorPart), FCString::Atoi(*MinorPart)};
	}

	FString AsString() const
	{
		return FString::Format(TEXT("{0}.{1}"), {Major, Minor});
	}
};

// Reason for performing an update (currently only version difference, but this could be extended).
struct FAssetUpdateReason
{
	FMetaHumanAssetVersion OldVersion;
	FMetaHumanAssetVersion NewVersion;

	// Whether or not the update is a breaking change (change in major version number)
	bool IsBreakingChange() const
	{
		return NewVersion.Major != OldVersion.Major;
	}
};

// List of relative asset paths to be Added, Replaced etc. as part of the current import action
struct FAssetOperationPaths
{
	TArray<FString> Add;
	TArray<FString> Replace;
	TArray<FString> Skip;
	TArray<FString> Update;
	TArray<FAssetUpdateReason> UpdateReasons;
};

// Helper structure to simplify management of file and asset paths. All paths are absolute and explicit as to whether
// they are a file path or an asset path.
struct FImportPaths
{
	const FString MetaHumansFolderName = TEXT("MetaHumans");
	const FString CommonFolderName = TEXT("Common");

	explicit FImportPaths(FMetaHumanAssetImportDescription ImportDescription);

	static FString FilenameToAssetName(const FString& Filename)
	{
		return FString::Format(TEXT("{0}.{0}"), {FPaths::GetBaseFilename(Filename)});
	}

	static FString AssetNameToFilename(const FString& AssetName)
	{
		return FString::Format(TEXT("{0}.uasset"), {AssetName});
	}

	FString CharacterNameToBlueprintAssetPath(const FString& CharacterName) const
	{
		return FPaths::Combine(DestinationMetaHumansAssetPath, CharacterName, FString::Format(TEXT("BP_{0}.BP_{0}"), {CharacterName}));
	}

	FString GetSourceFile(const FString& RelativeFilePath) const
	{
		return FPaths::Combine(SourceRootFilePath, RelativeFilePath);
	}

	FString GetDestinationFile(const FString& RelativeFilePath) const
	{
		return FPaths::Combine(DestinationRootFilePath, RelativeFilePath);
	}

	FString GetDestinationAsset(const FString& RelativeFilePath) const
	{
		return FPaths::Combine(DestinationRootAssetPath, FPaths::GetPath(RelativeFilePath), FilenameToAssetName(RelativeFilePath));
	}

	FString SourceRootFilePath;
	FString SourceMetaHumansFilePath;
	FString SourceCharacterFilePath;
	FString SourceCommonFilePath;

	FString DestinationRootFilePath;
	FString DestinationMetaHumansFilePath;
	FString DestinationCharacterFilePath;
	FString DestinationCommonFilePath;

	FString DestinationRootAssetPath;
	FString DestinationMetaHumansAssetPath;
	FString DestinationCharacterAssetPath;
	FString DestinationCommonAssetPath;
};


// Representation of a MetaHuman Version. This is a simple semantic-versioning style version number that is stored
// in a Json file at a specific location in the directory structure that MetaHumans use.
struct FMetaHumanVersion
{
	// Currently default initialisation == 0.0.0 which is not a valid version. This needs a bit more thought
	// TODO: refactor to use TOptional and avoid needing to represent invalid versions.
	FMetaHumanVersion() = default;

	explicit FMetaHumanVersion(const FString& VersionString)
	{
		TArray<FString> ParsedVersionString;
		const int32 NumSections = VersionString.ParseIntoArray(ParsedVersionString, TEXT("."));
		verify(NumSections == 3);
		if (NumSections == 3)
		{
			Major = FCString::Atoi(*ParsedVersionString[0]);
			Minor = FCString::Atoi(*ParsedVersionString[1]);
			Revision = FCString::Atoi(*ParsedVersionString[2]);
		}
	}

	explicit FMetaHumanVersion(const int Major, const int Minor, const int Revision)
		: Major(Major)
		, Minor(Minor)
		, Revision(Revision)
	{
	}

	// Comparison operators
	friend bool operator <(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right)
	{
		return Left.Major < Right.Major || (Left.Major == Right.Major && (Left.Minor < Right.Minor || (Left.Minor == Right.Minor && Left.Revision < Right.Revision)));
	}

	friend bool operator>(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right) { return Right < Left; }
	friend bool operator<=(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right) { return !(Left > Right); }
	friend bool operator>=(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right) { return !(Left < Right); }

	friend bool operator ==(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right)
	{
		return Right.Major == Left.Major && Right.Minor == Left.Minor && Right.Revision == Left.Revision;
	}

	friend bool operator!=(const FMetaHumanVersion& Left, const FMetaHumanVersion& Right) { return !(Left == Right); }

	// Hash function
	friend uint32 GetTypeHash(FMetaHumanVersion Version)
	{
		return (Version.Major << 20) + (Version.Minor << 10) + Version.Revision;
	}

	// Currently MetaHumans are compatible so long as they are from the same major version. In the future, compatibility
	// between versions may be more complex or require inspecting particular assets.
	bool IsCompatible(const FMetaHumanVersion& Other) const
	{
		return Major && Major == Other.Major;
	}

	FString AsString() const
	{
		return FString::Format(TEXT("{0}.{1}.{2}"), {Major, Minor, Revision});
	}

	static FMetaHumanVersion ReadFromFile(const FString& VersionFilePath);

	int32 Major = 0;
	int32 Minor = 0;
	int32 Revision = 0;
};


// Class that handles the layout on-disk of a MetaHuman being used as the source of an Import operation
// Gives us a single place to handle simple path operations, filenames etc.
class FSourceMetaHuman
{
public:
	FSourceMetaHuman(const FString& RootPath, const FString& Name)
		: RootPath(RootPath)
		, Name(Name)
	{
		const FString VersionFilePath = FPaths::Combine(RootPath, Name, TEXT("VersionInfo.txt"));
		Version = FMetaHumanVersion::ReadFromFile(VersionFilePath);
	}

	const FString& GetName() const
	{
		return Name;
	}

	const FMetaHumanVersion& GetVersion() const
	{
		return Version;
	}

	EQualityLevel GetQualityLevel() const
	{
		if (RootPath.Contains(TEXT("Tier0")))
		{
			return EQualityLevel::High;
		}
		if (RootPath.Contains(TEXT("Tier2")))
		{
			return EQualityLevel::Medium;
		}
		else
		{
			return EQualityLevel::Low;
		}
	}

private:
	FString RootPath;
	FString Name;
	FMetaHumanVersion Version;
};


// Class that handles the layout and filenames of a MetaHuman that has been added to a project.
class FInstalledMetaHuman
{
public:
	// For now, it is assumed that a MetaHuman has files in {MetaHumansFilePath}/{Name} and {MetaHumansFilePath}/Common.
	FInstalledMetaHuman(const FString& Name, const FString& MetaHumansFilePath);

	const FString& GetName() const
	{
		return Name;
	}

	FMetaHumanVersion GetVersion() const
	{
		const FString VersionFilePath = FPaths::Combine(MetaHumansFilePath, Name, TEXT("VersionInfo.txt"));
		return FMetaHumanVersion::ReadFromFile(VersionFilePath);
	}

	EQualityLevel GetQualityLevel() const;

	// Finds MetaHumans in the destination of a given import
	static TArray<FInstalledMetaHuman> GetInstalledMetaHumans(const FImportPaths& ImportPaths);

private:
	FString Name;
	FString MetaHumansFilePath;
	FString MetaHumansAssetPath;
};
