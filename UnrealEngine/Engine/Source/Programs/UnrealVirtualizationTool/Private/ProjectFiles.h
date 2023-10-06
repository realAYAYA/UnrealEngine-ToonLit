// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/StringFwd.h"
#include "Containers/UnrealString.h"
#include "Serialization/JsonSerializerMacros.h"

namespace UE::Virtualization
{

struct FPlugin : public FJsonSerializable
{
public:
	FString PluginFilePath;

	TArray<FString> PackagePaths;

public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("PluginPath", PluginFilePath);
		JSON_SERIALIZE_ARRAY("PackagePaths", PackagePaths);
	END_JSON_SERIALIZER
};

class FProject : public FJsonSerializable
{
public:
	FProject() = default;
	FProject(FString&& ProjectFilePath);
	~FProject() = default;

	void AddFile(const FString& FilePath);
	void AddPluginFile(const FString& FilePath, FString&& PluginFilePath);
	
	const FString& GetProjectFilePath() const;
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

public:
	BEGIN_JSON_SERIALIZER
		JSON_SERIALIZE("ProjectPath", ProjectFilePath);
		JSON_SERIALIZE_ARRAY_SERIALIZABLE("Plugins", Plugins, FPlugin);
		JSON_SERIALIZE_ARRAY("PackagePaths", PackagePaths);
	END_JSON_SERIALIZER
};

} //namespace UE::Virtualization
