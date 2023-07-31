// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeSourceFromText.h"
#include "ComputeFramework/ComputeFramework.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_EDITOR

void UComputeSourceFromText::PostLoad()
{
	Super::PostLoad();
	ReparseSourceText();
}

void UComputeSourceFromText::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
	PrevSourceFile = SourceFile;
}

void UComputeSourceFromText::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* ModifiedProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
	if (!ModifiedProperty)
	{
		return;
	}

	FName ModifiedPropName = ModifiedProperty->GetFName();

	if (ModifiedPropName == GET_MEMBER_NAME_CHECKED(UComputeSourceFromText, SourceFile))
	{
		ReparseSourceText();
	}
}

void UComputeSourceFromText::ReparseSourceText()
{
	if (SourceFile.FilePath.IsEmpty())
	{
		SourceText = FString();
		return;
	}

	FString FullPath = FPaths::ConvertRelativePathToFull(SourceFile.FilePath);

	IPlatformFile& PlatformFileSystem = IPlatformFile::GetPlatformPhysical();
	if (!PlatformFileSystem.FileExists(*FullPath))
	{
		UE_LOG(LogComputeFramework, Error, TEXT("Unable to find source file \"%s\""), *FullPath);

		SourceFile = PrevSourceFile;
		return;
	}

	if (!FFileHelper::LoadFileToString(SourceText, &PlatformFileSystem, *FullPath))
	{
		UE_LOG(LogComputeFramework, Error, TEXT("Unable to read source file \"%s\""), *FullPath);

		SourceFile = PrevSourceFile;
		return;
	}

	// todo[CF]: Notify graphs for recompilation
}

#endif
