// Copyright Epic Games, Inc. All Rights Reserved.

#include "ImgMediaSource.h"

#include "IImgMediaModule.h"
#include "ImgMediaGlobalCache.h"
#include "ImgMediaMipMapInfo.h"
#include "ImgMediaPrivate.h"

#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UObject/UE5MainStreamObjectVersion.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ImgMediaSource)


/* UImgMediaSource structors
 *****************************************************************************/

UImgMediaSource::UImgMediaSource()
	: IsPathRelativeToProjectRoot_DEPRECATED(false)
	, FrameRateOverride(0, 0)
	, bFillGapsInSequence(true)
	, MipMapInfo(MakeShared<FImgMediaMipMapInfo, ESPMode::ThreadSafe>())
{
}


/* UImgMediaSource interface
 *****************************************************************************/

void UImgMediaSource::GetProxies(TArray<FString>& OutProxies) const
{
	IFileManager::Get().FindFiles(OutProxies, *FPaths::Combine(GetFullPath(), TEXT("*")), false, true);
}

const FString UImgMediaSource::GetSequencePath() const
{
	return ExpandSequencePathTokens(SequencePath.Path);
}

void UImgMediaSource::SetSequencePath(const FString& Path)
{
	const FString SanitizedPath = FPaths::GetPath(Path);

	if (SanitizedPath.IsEmpty() || SanitizedPath.StartsWith(TEXT(".")))
	{
		SequencePath.Path = SanitizedPath;
	}
	else
	{
		FString FullPath = FPaths::ConvertRelativePathToFull(SanitizedPath);
		const FString RelativeDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

		if (FullPath.StartsWith(RelativeDir))
		{
			FPaths::MakePathRelativeTo(FullPath, *RelativeDir);
			FullPath = FString(TEXT("./")) + FullPath;
		}

		SequencePath.Path = FullPath;
	}
}

void UImgMediaSource::SetTokenizedSequencePath(const FString& Path)
{
	SequencePath.Path = SanitizeTokenizedSequencePath(Path);
}

FString UImgMediaSource::ExpandSequencePathTokens(const FString& InPath)
{
	return InPath
		.Replace(TEXT("{engine_dir}"), *FPaths::ConvertRelativePathToFull(FPaths::EngineDir()))
		.Replace(TEXT("{project_dir}"), *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()))
		;
}

FString UImgMediaSource::SanitizeTokenizedSequencePath(const FString& InPath)
{
	FString SanitizedPickedPath = InPath.TrimStartAndEnd().Replace(TEXT("\""), TEXT(""));

	const FString ProjectAbsolutePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());

	// Replace supported tokens
	FString ExpandedPath = UImgMediaSource::ExpandSequencePathTokens(SanitizedPickedPath);

	// Relative paths are always w.r.t. the project root.
	if (FPaths::IsRelative(ExpandedPath))
	{
		ExpandedPath = FPaths::Combine(ProjectAbsolutePath, SanitizedPickedPath);
	}

	// Chop trailing file path, in case the user picked a file instead of a folder
	if (FPaths::FileExists(ExpandedPath))
	{
		ExpandedPath = FPaths::GetPath(ExpandedPath);
		SanitizedPickedPath = FPaths::GetPath(SanitizedPickedPath);
	}

	// If the user picked the absolute path of a directory that is inside the project, use relative path.
	// Unless the user has a token in the beginning.
	if (!InPath.Len() || InPath[0] != '{') // '{' indicates that the path begins with a token
	{
		FString PathRelativeToProject;

		if (IsPathUnderBasePath(ExpandedPath, ProjectAbsolutePath, PathRelativeToProject))
		{
			SanitizedPickedPath = PathRelativeToProject;
		}
	}

	return SanitizedPickedPath;
}

void UImgMediaSource::AddTargetObject(AActor* InActor)
{
	MipMapInfo->AddObject(InActor);
}

void UImgMediaSource::AddTargetObject(AActor* InActor, float Width)
{
	AddTargetObject(InActor);
}


void UImgMediaSource::RemoveTargetObject(AActor* InActor)
{
	MipMapInfo->RemoveObject(InActor);
}


/* IMediaOptions interface
 *****************************************************************************/

bool UImgMediaSource::GetMediaOption(const FName& Key, bool DefaultValue) const
{
	if (Key == ImgMedia::FillGapsInSequenceOption)
	{
		return bFillGapsInSequence;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

int64 UImgMediaSource::GetMediaOption(const FName& Key, int64 DefaultValue) const
{
	if (Key == ImgMedia::FrameRateOverrideDenonimatorOption)
	{
		return FrameRateOverride.Denominator;
	}

	if (Key == ImgMedia::FrameRateOverrideNumeratorOption)
	{
		return FrameRateOverride.Numerator;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}


FString UImgMediaSource::GetMediaOption(const FName& Key, const FString& DefaultValue) const
{
	if (Key == ImgMedia::ProxyOverrideOption)
	{
		return ProxyOverride;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

TSharedPtr<IMediaOptions::FDataContainer, ESPMode::ThreadSafe> UImgMediaSource::GetMediaOption(const FName& Key, const TSharedPtr<FDataContainer, ESPMode::ThreadSafe>& DefaultValue) const
{
	if (Key == ImgMedia::MipMapInfoOption)
	{
		return MipMapInfo;
	}

	return Super::GetMediaOption(Key, DefaultValue);
}

bool UImgMediaSource::HasMediaOption(const FName& Key) const
{
	if ((Key == ImgMedia::FillGapsInSequenceOption) ||
		(Key == ImgMedia::FrameRateOverrideDenonimatorOption) ||
		(Key == ImgMedia::FrameRateOverrideNumeratorOption) ||
		(Key == ImgMedia::ProxyOverrideOption) ||
		(Key == ImgMedia::MipMapInfoOption))
	{
		return true;
	}

	return Super::HasMediaOption(Key);
}


/* UMediaSource interface
 *****************************************************************************/

FString UImgMediaSource::GetUrl() const
{
	return FString(TEXT("img://")) + GetFullPath();
}


bool UImgMediaSource::Validate() const
{
	return FPaths::DirectoryExists(GetFullPath());
}

#if WITH_EDITOR

void UImgMediaSource::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// Has FillGapsInSequence changed?
	if (PropertyChangedEvent.GetPropertyName() == GET_MEMBER_NAME_CHECKED(ThisClass, bFillGapsInSequence))
	{
		// Clear the cache, as effectively the frames have changed.
		FImgMediaGlobalCache* GlobalCache = IImgMediaModule::GetGlobalCache();
		if (GlobalCache != nullptr)
		{
			GlobalCache->EmptyCache();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR

/* UFileMediaSource implementation
 *****************************************************************************/

FString UImgMediaSource::GetFullPath() const
{
	const FString ExpandedSequencePath = GetSequencePath();

	if (FPaths::IsRelative(ExpandedSequencePath))
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), ExpandedSequencePath));
	}
	else
	{
		return ExpandedSequencePath;
	}
}

void UImgMediaSource::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	Ar.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	if (Ar.CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::ImgMediaPathResolutionWithEngineOrProjectTokens)
	{
		if (Ar.IsLoading() && !IsPathRelativeToProjectRoot_DEPRECATED)
		{
			// This is an object that was saved with the old value (or before the property was added), so we need to convert the path accordingly

			IsPathRelativeToProjectRoot_DEPRECATED = true;

			if (FPaths::IsRelative(SequencePath.Path))
			{
				SequencePath.Path = FString::Printf(TEXT("Content/%s"), *SequencePath.Path);

				SequencePath.Path = UImgMediaSource::SanitizeTokenizedSequencePath(SequencePath.Path);
			}
		}
	}
#endif
}

bool UImgMediaSource::IsPathUnderBasePath(const FString& InPath, const FString& InBasePath, FString& OutRelativePath)
{
	OutRelativePath = InPath;

	return 
		FPaths::MakePathRelativeTo(OutRelativePath, *InBasePath) 
		&& !OutRelativePath.StartsWith(TEXT(".."));
}

