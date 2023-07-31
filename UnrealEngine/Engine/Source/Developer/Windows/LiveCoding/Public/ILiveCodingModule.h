// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Delegates/Delegate.h"
#include "Modules/ModuleInterface.h"

#define LIVE_CODING_MODULE_NAME "LiveCoding"

class FText;

enum class ELiveCodingCompileFlags : uint8
{
	None = 0,
	WaitForCompletion = 1 << 0,
};
ENUM_CLASS_FLAGS(ELiveCodingCompileFlags)

enum class ELiveCodingCompileResult : uint8
{
	Success,			// Compile completed and there were changes 
	NoChanges,			// Compile completed and there were no changes
	InProgress,			// Compile started but wait for completion was not specified
	CompileStillActive,	// A prior compile request is still active
	NotStarted,			// Live coding monitor could not be started
	Failure,			// Complete completed but there was an error
	Cancelled,			// Compile was cancelled
};

class ILiveCodingModule : public IModuleInterface
{
public:
	virtual void EnableByDefault(bool bEnabled) = 0;
	virtual bool IsEnabledByDefault() const = 0;

	virtual void EnableForSession(bool bEnabled) = 0;
	virtual bool IsEnabledForSession() const = 0;
	virtual bool CanEnableForSession() const = 0;
	virtual const FText& GetEnableErrorText() const = 0;
	virtual bool AutomaticallyCompileNewClasses() const = 0;

	virtual bool HasStarted() const = 0;

	virtual void ShowConsole() = 0;
	virtual void Compile() = 0;
	virtual bool Compile(ELiveCodingCompileFlags CompileFlags, ELiveCodingCompileResult* Result) = 0;
	virtual bool IsCompiling() const = 0;
	virtual void Tick() = 0;

	DECLARE_MULTICAST_DELEGATE(FOnPatchCompleteDelegate);
	virtual FOnPatchCompleteDelegate& GetOnPatchCompleteDelegate() = 0;
};

