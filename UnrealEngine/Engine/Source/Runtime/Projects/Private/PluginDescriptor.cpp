// Copyright Epic Games, Inc. All Rights Reserved.

#include "PluginDescriptor.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProjectDescriptor.h"

#define LOCTEXT_NAMESPACE "PluginDescriptor"

namespace PluginDescriptor
{
	bool ReadFile(const TCHAR* FileName, FString& Text, FText* OutFailReason = nullptr)
	{
		if (!FFileHelper::LoadFileToString(Text, FileName))
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailedToLoadDescriptorFile", "Failed to open descriptor file '{0}'"), FText::FromStringView(FileName));
			}
			return false;
		}
		return true;
	}

	bool WriteFile(const TCHAR* FileName, const FString& Text, FText* OutFailReason = nullptr)
	{
		if (!FFileHelper::SaveStringToFile(Text, FileName))
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailedToWriteDescriptorFile", "Failed to write plugin descriptor file '{0}'. Perhaps the file is Read-Only?"), FText::FromStringView(FileName));
			}
			return false;
		}
		return true;
	}

	TSharedPtr<FJsonObject> DeserializeJson(const FString& Text, FText* OutFailReason = nullptr)
	{
		TSharedPtr<FJsonObject> JsonObject;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("FailedToReadDescriptorFile", "Failed to read file. {0}"), FText::FromString(Reader->GetErrorMessage()));
			}
			return TSharedPtr<FJsonObject>();
		}
		return JsonObject;
	}
}

/**
 * Version numbers for plugin descriptors. These version numbers are not generally needed; serialization from JSON attempts to be tolerant of missing/added fields.
 */ 
enum class EPluginDescriptorVersion : uint8
{
	Invalid = 0,
	Initial = 1,
	NameHash = 2,
	ProjectPluginUnification = 3,
	// !!!!!!!!!! IMPORTANT: Remember to also update LatestPluginDescriptorFileVersion in Plugins.cs (and Plugin system documentation) when this changes!!!!!!!!!!!
	// -----<new versions can be added before this line>-------------------------------------------------
	// - this needs to be the last line (see note below)
	LatestPlusOne,
	Latest = LatestPlusOne - 1
};

const FString& FPluginDescriptor::GetFileExtension()
{
	static const FString Extension(TEXT(".uplugin"));
	return Extension;
}

FPluginDescriptor::FPluginDescriptor()
	: Version(0)
	, VerseScope(EVerseScope::PublicUser)
	, EnabledByDefault(EPluginEnabledByDefault::Unspecified)
	, bCanContainContent(false)
	, bCanContainVerse(false)
	, bIsBetaVersion(false)
	, bIsExperimentalVersion(false)
	, bInstalled(false)
	, bRequiresBuildPlatform(false)
	, bIsHidden(false)
	, bIsSealed(false)
	, bNoCode(false)
	, bExplicitlyLoaded(false)
	, bHasExplicitPlatforms(false)
	, bIsPluginExtension(false)
{
}

bool FPluginDescriptor::Load(const TCHAR* FileName, FText* OutFailReason /*= nullptr*/)
{
#if WITH_EDITOR
	CachedJson.Reset();
	AdditionalFieldsToWrite.Reset();
#endif // WITH_EDITOR

	FString Text;
	if (PluginDescriptor::ReadFile(FileName, Text, OutFailReason))
	{
		return Read(Text, OutFailReason);
	}
	return false;
}

bool FPluginDescriptor::Load(const FString& FileName, FText* OutFailReason /*= nullptr*/)
{
	return Load(*FileName, OutFailReason);
}

bool FPluginDescriptor::Load(const FString& FileName, FText& OutFailReason)
{
	return Load(*FileName, &OutFailReason);
}

bool FPluginDescriptor::Read(const FString& Text, FText* OutFailReason /*= nullptr*/)
{
#if WITH_EDITOR
	CachedJson.Reset();
	AdditionalFieldsToWrite.Reset();
#endif // WITH_EDITOR

	// Deserialize a JSON object from the string
	TSharedPtr<FJsonObject> JsonObject = PluginDescriptor::DeserializeJson(Text, OutFailReason);
	if (JsonObject.IsValid())
	{
		// Parse it as a plug-in descriptor
		if (Read(*JsonObject.Get(), OutFailReason))
		{
#if WITH_EDITOR
			CachedJson = JsonObject;
			AdditionalFieldsToWrite.Reset();
#endif // WITH_EDITOR
			return true;
		}
	}
	return false;
}

bool FPluginDescriptor::Read(const FString& Text, FText& OutFailReason)
{
	return Read(Text, &OutFailReason);
}

bool FPluginDescriptor::Read(const FJsonObject& Object, FText* OutFailReason /*= nullptr*/)
{
	// Read the file version
	int32 FileVersionInt32;
	if(!Object.TryGetNumberField(TEXT("FileVersion"), FileVersionInt32))
	{
		if(!Object.TryGetNumberField(TEXT("PluginFileVersion"), FileVersionInt32))
		{
			if (OutFailReason)
			{
				*OutFailReason = LOCTEXT("InvalidProjectFileVersion", "File does not have a valid 'FileVersion' number.");
			}
			return false;
		}
	}

	// Check that it's within range
	EPluginDescriptorVersion PluginFileVersion = (EPluginDescriptorVersion)FileVersionInt32;
	if ((PluginFileVersion <= EPluginDescriptorVersion::Invalid) || (PluginFileVersion > EPluginDescriptorVersion::Latest))
	{
		if (OutFailReason)
		{
			const FText ReadVersionText = FText::FromString(FString::Printf(TEXT("%d"), (int32)PluginFileVersion));
			const FText LatestVersionText = FText::FromString(FString::Printf(TEXT("%d"), (int32)EPluginDescriptorVersion::Latest));
			*OutFailReason = FText::Format(LOCTEXT("ProjectFileVersionTooLarge", "File appears to be in a newer version ({0}) of the file format that we can load (max version: {1})."), ReadVersionText, LatestVersionText);
		}
		return false;
	}

	// Read the other fields
	Object.TryGetNumberField(TEXT("Version"), Version);
	Object.TryGetStringField(TEXT("VersionName"), VersionName);
	Object.TryGetStringField(TEXT("FriendlyName"), FriendlyName);
	Object.TryGetStringField(TEXT("Description"), Description);

	if (!Object.TryGetStringField(TEXT("Category"), Category))
	{
		// Category used to be called CategoryPath in .uplugin files
		Object.TryGetStringField(TEXT("CategoryPath"), Category);
	}
        
	// Due to a difference in command line parsing between Windows and Mac, we shipped a few Mac samples containing
	// a category name with escaped quotes. Remove them here to make sure we can list them in the right category.
	if (Category.Len() >= 2 && Category.StartsWith(TEXT("\""), ESearchCase::CaseSensitive) && Category.EndsWith(TEXT("\""), ESearchCase::CaseSensitive))
	{
		Category.MidInline(1, Category.Len() - 2, EAllowShrinking::No);
	}

	Object.TryGetStringField(TEXT("CreatedBy"), CreatedBy);
	Object.TryGetStringField(TEXT("CreatedByURL"), CreatedByURL);
	Object.TryGetStringField(TEXT("DocsURL"), DocsURL);
	Object.TryGetStringField(TEXT("MarketplaceURL"), MarketplaceURL);
	Object.TryGetStringField(TEXT("SupportURL"), SupportURL);
	Object.TryGetStringField(TEXT("EngineVersion"), EngineVersion);
	Object.TryGetStringField(TEXT("EditorCustomVirtualPath"), EditorCustomVirtualPath);
	Object.TryGetStringArrayField(TEXT("SupportedTargetPlatforms"), SupportedTargetPlatforms);
	Object.TryGetStringArrayField(TEXT("SupportedPrograms"), SupportedPrograms);
	Object.TryGetBoolField(TEXT("bIsPluginExtension"), bIsPluginExtension);

	if (!FModuleDescriptor::ReadArray(Object, TEXT("Modules"), Modules, OutFailReason))
	{
		return false;
	}

	if (!FLocalizationTargetDescriptor::ReadArray(Object, TEXT("LocalizationTargets"), LocalizationTargets, OutFailReason))
	{
		return false;
	}

	Object.TryGetStringField(TEXT("VersePath"), VersePath);

	// Read the Verse scope
	TSharedPtr<FJsonValue> VerseScopeValue = Object.TryGetField(TEXT("VerseScope"));
	if (VerseScopeValue.IsValid() && VerseScopeValue->Type == EJson::String)
	{
		if (TOptional<EVerseScope::Type> MaybeVerseScope = EVerseScope::FromString(*VerseScopeValue->AsString()))
		{
			VerseScope = *MaybeVerseScope;
		}
		else
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("PluginWithInvalidVerseScope", "Plugin entry 'VerseScope' specified an unrecognized value '{1}'"), FText::FromString(VerseScopeValue->AsString()));
			}
			return false;
		}
	}

	// Read the Verse version.
	TSharedPtr<FJsonValue> VerseVersionValue = Object.TryGetField(TEXT("VerseVersion"));
	if (VerseVersionValue.IsValid())
	{
		uint32 PluginVerseVersion;
		if (VerseVersionValue->TryGetNumber(PluginVerseVersion))
		{
			VerseVersion = PluginVerseVersion;
		}
		else
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("PluginWithInvalidVerseVersion", "Plugin entry 'VerseVersion' specified an unrecognized value '{1}'"), FText::FromString(VerseVersionValue->AsString()));
			}
			return false;
		}
	}

	Object.TryGetBoolField(TEXT("EnableVerseAssetReflection"), bEnableVerseAssetReflection);

	bool bEnabledByDefault;
	if(Object.TryGetBoolField(TEXT("EnabledByDefault"), bEnabledByDefault))
	{
		EnabledByDefault = bEnabledByDefault ? EPluginEnabledByDefault::Enabled : EPluginEnabledByDefault::Disabled;
	}

	Object.TryGetBoolField(TEXT("CanContainContent"), bCanContainContent);
	Object.TryGetBoolField(TEXT("CanContainVerse"), bCanContainVerse);
	Object.TryGetBoolField(TEXT("NoCode"), bNoCode);
	Object.TryGetBoolField(TEXT("IsBetaVersion"), bIsBetaVersion);
	Object.TryGetBoolField(TEXT("IsExperimentalVersion"), bIsExperimentalVersion);
	Object.TryGetBoolField(TEXT("Installed"), bInstalled);
	Object.TryGetBoolField(TEXT("RequiresBuildPlatform"), bRequiresBuildPlatform);
	Object.TryGetBoolField(TEXT("Hidden"), bIsHidden);
	Object.TryGetBoolField(TEXT("Sealed"), bIsSealed);
	Object.TryGetBoolField(TEXT("ExplicitlyLoaded"), bExplicitlyLoaded);
	Object.TryGetBoolField(TEXT("HasExplicitPlatforms"), bHasExplicitPlatforms);

	bool bCanBeUsedWithUnrealHeaderTool;
	if(Object.TryGetBoolField(TEXT("CanBeUsedWithUnrealHeaderTool"), bCanBeUsedWithUnrealHeaderTool) && bCanBeUsedWithUnrealHeaderTool)
	{
		SupportedPrograms.Add(TEXT("UnrealHeaderTool"));
	}

	PreBuildSteps.Read(Object, TEXT("PreBuildSteps"));
	PostBuildSteps.Read(Object, TEXT("PostBuildSteps"));

	if (!FPluginReferenceDescriptor::ReadArray(Object, TEXT("Plugins"), Plugins, OutFailReason))
	{
		return false;
	}

	// Backwards compatibility support
	TArray<FString> DisallowedPluginNameStrings;
	if (Object.TryGetStringArrayField(TEXT("DisallowedPlugins"), DisallowedPluginNameStrings))
	{
		DisallowedPlugins.Reserve(DisallowedPluginNameStrings.Num());
		for (int32 Index = 0; Index < DisallowedPluginNameStrings.Num(); ++Index)
		{
			FPluginDisallowedDescriptor& PluginDisallowedDescriptor = DisallowedPlugins.AddDefaulted_GetRef();
			PluginDisallowedDescriptor.Name = DisallowedPluginNameStrings[Index];
		}
	}
	else if (!FPluginDisallowedDescriptor::ReadArray(Object, TEXT("DisallowedPlugins"), DisallowedPlugins, OutFailReason))
	{
		return false;
	}

	return true;
}

bool FPluginDescriptor::Read(const FJsonObject& Object, FText& OutFailReason)
{
	return Read(Object, &OutFailReason);
}

bool FPluginDescriptor::Save(const TCHAR* FileName, FText* OutFailReason /*= nullptr*/) const
{
	// Write the descriptor to text
	FString Text;
	Write(Text);

	// Save it to a file
	return PluginDescriptor::WriteFile(FileName, Text, OutFailReason);
}

bool FPluginDescriptor::Save(const FString& FileName, FText* OutFailReason /*= nullptr*/) const
{
	return Save(*FileName, OutFailReason);
}

bool FPluginDescriptor::Save(const FString& FileName, FText& OutFailReason) const
{
	return Save(*FileName, &OutFailReason);
}

void FPluginDescriptor::Write(FString& Text) const
{
	// Write the contents of the descriptor to a string. Make sure the writer is destroyed so that the contents are flushed to the string.
	TSharedRef< TJsonWriter<> > WriterRef = TJsonWriterFactory<>::Create(&Text);
	TJsonWriter<>& Writer = WriterRef.Get();
	Write(Writer);
	Writer.Close();
}

void FPluginDescriptor::Write(TJsonWriter<>& Writer) const
{
	TSharedPtr<FJsonObject> PluginJsonObject = MakeShared<FJsonObject>();

#if WITH_EDITOR
	if (CachedJson.IsValid())
	{
		FJsonObject::Duplicate(/*Source=*/ CachedJson, /*Dest=*/ PluginJsonObject);
	}
#endif //if WITH_EDITOR

	UpdateJson(*PluginJsonObject);

	FJsonSerializer::Serialize(PluginJsonObject.ToSharedRef(), Writer);
}

void FPluginDescriptor::UpdateJson(FJsonObject& JsonObject) const
{
	JsonObject.SetNumberField(TEXT("FileVersion"), EProjectDescriptorVersion::Latest);
	JsonObject.SetNumberField(TEXT("Version"), Version);
	JsonObject.SetStringField(TEXT("VersionName"), VersionName);
	JsonObject.SetStringField(TEXT("FriendlyName"), FriendlyName);
	JsonObject.SetStringField(TEXT("Description"), Description);
	JsonObject.SetStringField(TEXT("Category"), Category);
	JsonObject.SetStringField(TEXT("CreatedBy"), CreatedBy);
	JsonObject.SetStringField(TEXT("CreatedByURL"), CreatedByURL);
	JsonObject.SetStringField(TEXT("DocsURL"), DocsURL);
	JsonObject.SetStringField(TEXT("MarketplaceURL"), MarketplaceURL);
	JsonObject.SetStringField(TEXT("SupportURL"), SupportURL);

	if (EngineVersion.Len() > 0)
	{
		JsonObject.SetStringField(TEXT("EngineVersion"), EngineVersion);
	}
	else
	{
		JsonObject.RemoveField(TEXT("EngineVersion"));
	}

	if (EditorCustomVirtualPath.Len() > 0)
	{
		JsonObject.SetStringField(TEXT("EditorCustomVirtualPath"), EditorCustomVirtualPath);
	}
	else
	{
		JsonObject.RemoveField(TEXT("EditorCustomVirtualPath"));
	}

	if (!VersePath.IsEmpty())
	{
		JsonObject.SetStringField(TEXT("VersePath"), VersePath);
	}
	else
	{
		JsonObject.RemoveField(TEXT("VersePath"));
	}

	if (VerseScope != EVerseScope::PublicUser)
	{
		JsonObject.SetStringField(TEXT("VerseScope"), EVerseScope::ToString(VerseScope));
	}
	else
	{
		JsonObject.RemoveField(TEXT("VerseScope"));
	}

	if (VerseVersion.IsSet())
	{
		JsonObject.SetNumberField(TEXT("VerseVersion"), VerseVersion.GetValue());
	}
	else
	{
		JsonObject.RemoveField(TEXT("VerseVersion"));
	}

	if (bEnableVerseAssetReflection)
	{
		JsonObject.SetBoolField(TEXT("EnableVerseAssetReflection"), bEnableVerseAssetReflection);
	}
	else
	{
		JsonObject.RemoveField(TEXT("EnableVerseAssetReflection"));
	}

	if (EnabledByDefault != EPluginEnabledByDefault::Unspecified)
	{
		JsonObject.SetBoolField(TEXT("EnabledByDefault"), (EnabledByDefault == EPluginEnabledByDefault::Enabled));
	}
	else
	{
		JsonObject.RemoveField(TEXT("EnabledByDefault"));
	}

	JsonObject.SetBoolField(TEXT("CanContainContent"), bCanContainContent);
	if (bCanContainVerse)
	{
		JsonObject.SetBoolField(TEXT("CanContainVerse"), bCanContainVerse);
	}
	else
	{
		JsonObject.RemoveField(TEXT("CanContainVerse"));
	}

	if (bNoCode)
	{
		JsonObject.SetBoolField(TEXT("NoCode"), bNoCode);
	}
	JsonObject.SetBoolField(TEXT("IsBetaVersion"), bIsBetaVersion);
	JsonObject.SetBoolField(TEXT("IsExperimentalVersion"), bIsExperimentalVersion);
	JsonObject.SetBoolField(TEXT("Installed"), bInstalled);

	if (SupportedTargetPlatforms.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> SupportedTargetPlatformValues;
		for (const FString& SupportedTargetPlatform : SupportedTargetPlatforms)
		{
			SupportedTargetPlatformValues.Add(MakeShareable(new FJsonValueString(SupportedTargetPlatform)));
		}
		JsonObject.SetArrayField(TEXT("SupportedTargetPlatforms"), SupportedTargetPlatformValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("SupportedTargetPlatforms"));
	}

	if (SupportedPrograms.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> SupportedProgramValues;
		for (const FString& SupportedProgram : SupportedPrograms)
		{
			SupportedProgramValues.Add(MakeShareable(new FJsonValueString(SupportedProgram)));
		}
		JsonObject.SetArrayField(TEXT("SupportedPrograms"), SupportedProgramValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("SupportedPrograms"));
	}

	if (bIsPluginExtension)
	{
		JsonObject.SetBoolField(TEXT("bIsPluginExtension"), bIsPluginExtension);
	}
	else
	{
		JsonObject.RemoveField(TEXT("bIsPluginExtension"));
	}

	FModuleDescriptor::UpdateArray(JsonObject, TEXT("Modules"), Modules);

	FLocalizationTargetDescriptor::UpdateArray(JsonObject, TEXT("LocalizationTargets"), LocalizationTargets);

	if (bRequiresBuildPlatform)
	{
		JsonObject.SetBoolField(TEXT("RequiresBuildPlatform"), bRequiresBuildPlatform);
	}
	else
	{
		JsonObject.RemoveField(TEXT("RequiresBuildPlatform"));
	}

	if (bIsHidden)
	{
		JsonObject.SetBoolField(TEXT("Hidden"), bIsHidden);
	}
	else
	{
		JsonObject.RemoveField(TEXT("Hidden"));
	}

	if (bIsSealed)
	{
		JsonObject.SetBoolField(TEXT("Sealed"), bIsSealed);
	}
	else
	{
		JsonObject.RemoveField(TEXT("Sealed"));
	}

	if (bExplicitlyLoaded)
	{
		JsonObject.SetBoolField(TEXT("ExplicitlyLoaded"), bExplicitlyLoaded);
	}
	else
	{
		JsonObject.RemoveField(TEXT("ExplicitlyLoaded"));
	}

	if (bHasExplicitPlatforms)
	{
		JsonObject.SetBoolField(TEXT("HasExplicitPlatforms"), bHasExplicitPlatforms);
	}
	else
	{
		JsonObject.RemoveField(TEXT("HasExplicitPlatforms"));
	}

	PreBuildSteps.UpdateJson(JsonObject, TEXT("PreBuildSteps"));
	PostBuildSteps.UpdateJson(JsonObject, TEXT("PostBuildSteps"));

	// Removing the plugins field to force the array to be rebuilt in the same order as the Plugins array otherwise 
	// all new items are appended at the back.
	JsonObject.RemoveField(TEXT("Plugins"));

	FPluginReferenceDescriptor::UpdateArray(JsonObject, TEXT("Plugins"), Plugins);

	FPluginDisallowedDescriptor::UpdateArray(JsonObject, TEXT("DisallowedPlugins"), DisallowedPlugins);

#if WITH_EDITOR
	for (const auto& KVP : AdditionalFieldsToWrite)
	{
		JsonObject.SetField(KVP.Key, FJsonValue::Duplicate(KVP.Value));
	}
#endif //if WITH_EDITOR
}

bool FPluginDescriptor::UpdatePluginFile(const FString& FileName, FText* OutFailReason /*= nullptr*/) const
{
	if (IFileManager::Get().FileExists(*FileName))
	{
		// Plugin file exists so we need to read it and update it.

		FString JsonText;
		if (!PluginDescriptor::ReadFile(*FileName, JsonText, OutFailReason))
		{
			return false;
		}

		TSharedPtr<FJsonObject> JsonObject = PluginDescriptor::DeserializeJson(JsonText, OutFailReason);
		if (!JsonObject.IsValid())
		{
			return false;
		}

		UpdateJson(*JsonObject.Get());

		{
			TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonText);
			if (!ensure(FJsonSerializer::Serialize(JsonObject.ToSharedRef(), JsonWriter)))
			{
				if (OutFailReason)
				{
					*OutFailReason = LOCTEXT("FailedToWriteDescriptor", "Failed to write plugin descriptor content");
				}
				return false;
			}
		}

#if WITH_EDITOR
		CachedJson = JsonObject;
#endif
		return PluginDescriptor::WriteFile(*FileName, JsonText, OutFailReason);
	}
	else
	{
		// Plugin file doesn't exist so just write it.
		return Save(FileName, OutFailReason);
	}
}

bool FPluginDescriptor::UpdatePluginFile(const FString& FileName, FText& OutFailReason) const
{
	return UpdatePluginFile(FileName, &OutFailReason);
}

bool FPluginDescriptor::SupportsTargetPlatform(const FString& Platform) const
{
	if (bHasExplicitPlatforms)
	{
		return SupportedTargetPlatforms.Contains(Platform);
	}
	else
	{
		return SupportedTargetPlatforms.Num() == 0 || SupportedTargetPlatforms.Contains(Platform);
	}
}

#undef LOCTEXT_NAMESPACE
