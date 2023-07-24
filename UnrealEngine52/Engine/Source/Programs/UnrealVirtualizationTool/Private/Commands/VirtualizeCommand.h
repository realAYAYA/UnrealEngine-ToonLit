// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/CommandBase.h"

namespace UE::Virtualization
{

class FVirtualizeCommand : public FCommand
{
public:
	FVirtualizeCommand(FStringView CommandName);
	virtual ~FVirtualizeCommand() = default;

	static void PrintCmdLineHelp();

private:
	virtual bool Initialize(const TCHAR* CmdLine) override;

	virtual bool Run(const TArray<FProject>& Projects) override;

	virtual const TArray<FString>& GetPackages() const override;

	bool TrySubmitChangelist(const FSourceControlChangelistPtr& ChangelistToSubmit, const TArray<FString>& DescriptionTags);
	
protected:

	bool TryParsePackageList(const FString& PackageListPath, TArray<FString>& OutPackages);	

protected:
	FString ClientSpecName;

	TArray<FString> AllPackages;

	FSourceControlChangelistPtr SourceChangelist;
	FString SourceChangelistNumber;

	bool bShouldSubmitChangelist = false;
	bool bShouldCheckout = false;
};

/** Used to convert the legacy command line -Mode=Changelist to the new style of command */
class FVirtualizeLegacyChangeListCommand final : public FVirtualizeCommand
{
public:
	FVirtualizeLegacyChangeListCommand(FStringView CommandName);
	virtual ~FVirtualizeLegacyChangeListCommand() = default;

private:
	virtual bool Initialize(const TCHAR* CmdLine) override;
};

/** Used to convert the legacy command line -Mode=Packagelist to the new style of command */
class FVirtualizeLegacyPackageListCommand final : public FVirtualizeCommand
{
public:
	FVirtualizeLegacyPackageListCommand(FStringView CommandName);
	virtual ~FVirtualizeLegacyPackageListCommand() = default;

private:
	virtual bool Initialize(const TCHAR* CmdLine) override;
};



} // namespace UE::Virtualization
