// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Containers/UnrealString.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

class ISourceControlProvider;
class ISourceControlChangelist;

typedef TSharedPtr<ISourceControlChangelist, ESPMode::ThreadSafe> FSourceControlChangelistPtr;

namespace UE::Virtualization
{

class FProject;

/** The base class to derive new commands from */
class FCommand
{
public:
	FCommand(FStringView InCommandName);
	virtual ~FCommand();

	virtual bool Initialize(const TCHAR* CmdLine) = 0;

	virtual bool Run(const TArray<FProject>& Projects) = 0;

	virtual const TArray<FString>& GetPackages() const = 0;

	const FString& GetName() const
	{
		return CommandName;
	}

protected: // Common commandline parsing code
	
	static void ParseCommandLine(const TCHAR* CmdLine, TArray<FString>& Tokens, TArray<FString>& Switches);

	enum EPathResult : int8
	{
		/** The switch was a valid package/path but parsing it resulted in an error */
		Error = -1,
		/** The switch was not a valid package/path */
		NotFound = 0,
		/** The switch was a valid package/path and was successfully parsed */
		Success = 1
	};

	static EPathResult ParseSwitchForPaths(const FString& Switch, TArray<FString>& OutPackages);

protected: // Common SourceControl Code
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
	
	bool TryParseChangelist(FStringView ClientSpecName, FStringView ChangelistNumber, TArray<FString>& OutPackages, FSourceControlChangelistPtr* OutChangelist);

	FString FindClientSpecForChangelist(FStringView ChangelistNumber);
private:
	FString CommandName;

protected:
	TUniquePtr<ISourceControlProvider> SCCProvider;
};

} // namespace UE::Virtualization