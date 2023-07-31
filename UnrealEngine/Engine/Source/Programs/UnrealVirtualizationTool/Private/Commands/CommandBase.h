// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Templates/UniquePtr.h"

class ISourceControlProvider;

namespace UE::Virtualization
{

class FProject;

/** The base class to derive new commands from */
class FCommand
{
public:
	FCommand(FStringView InCommandName)
		: CommandName(InCommandName)
	{

	}
	virtual ~FCommand() = default;

	virtual bool Initialize(const TCHAR* CmdLine) = 0;

	virtual bool Run(const TArray<FProject>& Projects) = 0;

	const FString& GetName() const
	{
		return CommandName;
	}

	const TArray<FString>& GetPackages() const
	{
		return Packages;
	}

protected:

	/**
	 * Creates a new ISourceControlProvider that can be used to perforce source control operations. This provider
	 * will remain valid until reset or the command is completed.
	 * 
	 * @param ClientSpecName	The client spec to use when connecting. If left blank we will use a default
	 *							spec, which will likely be auto determined by the system.
	 * @return True if the provider was created, otherwise false.
	 */
	bool TryConnectToSourceControl(FStringView ClientSpecName);

	bool TryCheckOutFilesForProject(FStringView ClientSpecName, FStringView ProjectRoot, const TArray<FString>& ProjectPackages);
	
	bool TryParseChangelist(FStringView ClientSpecName, FStringView ChangelistNumber, TArray<FString>& OutPackages);
private:
	FString CommandName;

protected:
	TUniquePtr<ISourceControlProvider> SCCProvider;

	TArray<FString> Packages;

};

} // namespace UE::Virtualization