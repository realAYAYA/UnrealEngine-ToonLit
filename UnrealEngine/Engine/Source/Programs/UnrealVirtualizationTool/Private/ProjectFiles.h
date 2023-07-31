// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"

namespace UE::Virtualization
{

struct FPlugin
{
	FString PluginFilePath;

	TArray<FString> PackagePaths;
};

class FProject
{
public:
	FProject(FString&& ProjectFilePath);
	~FProject() = default;

	void AddFile(const FString& FilePath);
	void AddPluginFile(const FString& FilePath, FString&& PluginFilePath);
	
	FStringView GetProjectFilePath() const;
	FStringView GetProjectName() const;
	FStringView GetProjectRoot() const;

	TArray<FString> GetAllPackages() const;

	void RegisterMountPoints() const;
	void UnRegisterMountPoints() const;

	bool TryLoadConfig(FConfigFile& OutConfig) const;

private:
	FString ProjectFilePath;

	TArray<FPlugin> Plugins;
	TArray<FString> PackagePaths;
};

} //namespace UE::Virtualization