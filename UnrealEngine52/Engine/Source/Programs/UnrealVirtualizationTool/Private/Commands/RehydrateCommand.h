// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Commands/CommandBase.h"

namespace UE::Virtualization
{

class FRehydrateCommand final : public FCommand
{
public:
	FRehydrateCommand(FStringView CommandName);
	virtual ~FRehydrateCommand() = default;

	static void PrintCmdLineHelp();
private:
	virtual bool Initialize(const TCHAR* CmdLine) override;

	virtual bool Run(const TArray<FProject>& Projects) override;

	virtual const TArray<FString>& GetPackages() const override;

private:

	FString ClientSpecName;

	TArray<FString> Packages;

	bool bShouldCheckout = false;
};

} // namespace UE::Virtualization