// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "LocTextHelper.h"
#include "Templates/SharedPointer.h"

class FText;

class LOCALIZATION_API FLocalizationSCC
{
public:
	FLocalizationSCC();
	~FLocalizationSCC();

	bool CheckOutFile(const FString& InFile, FText& OutError);
	bool CheckinFiles(const FText& InChangeDescription, FText& OutError);
	bool CleanUp(FText& OutError);
	bool RevertFile(const FString& InFile, FText& OutError);
	bool IsReady(FText& OutError);

private:
	TArray<FString> CheckedOutFiles;
};

class LOCALIZATION_API FLocFileSCCNotifies : public ILocFileNotifies
{
public:
	FLocFileSCCNotifies(const TSharedPtr<FLocalizationSCC>& InSourceControlInfo)
		: SourceControlInfo(InSourceControlInfo)
	{
	}

	/** Virtual destructor */
	virtual ~FLocFileSCCNotifies() {}

	//~ ILocFileNotifies interface
	virtual void PreFileRead(const FString& InFilename) override {}
	virtual void PostFileRead(const FString& InFilename) override {}
	virtual void PreFileWrite(const FString& InFilename) override;
	virtual void PostFileWrite(const FString& InFilename) override;

private:
	TSharedPtr<FLocalizationSCC> SourceControlInfo;
};
