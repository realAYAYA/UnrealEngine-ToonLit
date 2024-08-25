// Copyright Epic Games, Inc. All Rights Reserved.

#include "LocalizationDescriptor.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "JsonUtils/JsonObjectArrayUpdater.h"

#define LOCTEXT_NAMESPACE "LocalizationDescriptor"

namespace LocalizationDescriptor
{
	FString GetDescriptorKey(const FLocalizationTargetDescriptor& Descriptor)
	{
		return Descriptor.Name;
	}

	bool TryGetDescriptorJsonObjectKey(const FJsonObject& JsonObject, FString& OutKey)
	{
		return JsonObject.TryGetStringField(TEXT("Name"), OutKey);
	}

	void UpdateDescriptorJsonObject(const FLocalizationTargetDescriptor& Descriptor, FJsonObject& JsonObject)
	{
		Descriptor.UpdateJson(JsonObject);
	}
}

ELocalizationTargetDescriptorLoadingPolicy::Type ELocalizationTargetDescriptorLoadingPolicy::FromString(const TCHAR *String)
{
	ELocalizationTargetDescriptorLoadingPolicy::Type TestType = (ELocalizationTargetDescriptorLoadingPolicy::Type)0;
	for(; TestType < ELocalizationTargetDescriptorLoadingPolicy::Max; TestType = (ELocalizationTargetDescriptorLoadingPolicy::Type)(TestType + 1))
	{
		const TCHAR *TestString = ToString(TestType);
		if (FCString::Stricmp(String, TestString) == 0)
		{
			break;
		}
	}
	return TestType;
}

const TCHAR* ELocalizationTargetDescriptorLoadingPolicy::ToString(const ELocalizationTargetDescriptorLoadingPolicy::Type Value)
{
	switch (Value)
	{
	case Never:
		return TEXT("Never");

	case Always:
		return TEXT("Always");

	case Editor:
		return TEXT("Editor");

	case Game:
		return TEXT("Game");

	case PropertyNames:
		return TEXT("PropertyNames");

	case ToolTips:
		return TEXT("ToolTips");

	default:
		ensureMsgf(false, TEXT("ELocalizationTargetDescriptorLoadingPolicy::ToString - Unrecognized ELocalizationTargetDescriptorLoadingPolicy value: %i"), Value);
		return nullptr;
	}
}

ELocalizationConfigGenerationPolicy::Type ELocalizationConfigGenerationPolicy::FromString(const TCHAR* String)
{
	ELocalizationConfigGenerationPolicy ::Type TestType = (ELocalizationConfigGenerationPolicy::Type)0;
	for (; TestType < ELocalizationConfigGenerationPolicy::Max; TestType = (ELocalizationConfigGenerationPolicy::Type)(TestType + 1))
	{
		const TCHAR* TestString = ToString(TestType);
		if (FCString::Stricmp(String, TestString) == 0)
		{
			break;
		}
	}
	return TestType;
}

const TCHAR* ELocalizationConfigGenerationPolicy::ToString(const ELocalizationConfigGenerationPolicy::Type Value)
{
	switch (Value)
	{
	case Never:
		return TEXT("Never");

	case Auto:
		return TEXT("Auto");

	case User:
		return TEXT("User");

	default:
		ensureMsgf(false, TEXT("ELocalizationTargetDescriptorGenerationPolicy ::ToString - Unrecognized ELocalizationTargetDescriptorGenerationPolicy value: %i"), Value);
		return nullptr;
	}
}

FLocalizationTargetDescriptor::FLocalizationTargetDescriptor(FString InName, ELocalizationTargetDescriptorLoadingPolicy::Type InLoadingPolicy, ELocalizationConfigGenerationPolicy::Type InGenerationPolicy)
	: Name(MoveTemp(InName))
	, LoadingPolicy(InLoadingPolicy)
	, ConfigGenerationPolicy(InGenerationPolicy)
{
}

bool FLocalizationTargetDescriptor::Read(const FJsonObject& InObject, FText* OutFailReason /*= nullptr*/)
{
	// Read the target name
	TSharedPtr<FJsonValue> NameValue = InObject.TryGetField(TEXT("Name"));
	if (!NameValue.IsValid() || NameValue->Type != EJson::String)
	{
		if (OutFailReason)
		{
			*OutFailReason = LOCTEXT("TargetWithoutAName", "Found a 'Localization Target' entry with a missing 'Name' field");
		}
		return false;
	}
	Name = NameValue->AsString();

	// Read the target loading policy
	TSharedPtr<FJsonValue> LoadingPolicyValue = InObject.TryGetField(TEXT("LoadingPolicy"));
	if (LoadingPolicyValue.IsValid() && LoadingPolicyValue->Type == EJson::String)
	{
		LoadingPolicy = ELocalizationTargetDescriptorLoadingPolicy::FromString(*LoadingPolicyValue->AsString());
		if (LoadingPolicy == ELocalizationTargetDescriptorLoadingPolicy::Max)
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("TargetWithInvalidLoadingPolicy", "Localization Target entry '{0}' specified an unrecognized target LoadingPolicy '{1}'"), FText::FromString(Name), FText::FromString(LoadingPolicyValue->AsString()));
			}
			return false;
		}
	}

		// Read the target generation policy
	TSharedPtr<FJsonValue> ConfigGenerationPolicyValue = InObject.TryGetField(TEXT("ConfigGenerationPolicy"));
	if (ConfigGenerationPolicyValue.IsValid() && ConfigGenerationPolicyValue->Type == EJson::String)
	{
		ConfigGenerationPolicy = ELocalizationConfigGenerationPolicy::FromString(*ConfigGenerationPolicyValue->AsString());
		if (ConfigGenerationPolicy == ELocalizationConfigGenerationPolicy::Max)
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format(LOCTEXT("TargetWithInvalidGenerationPolicy", "Localization Target entry '{0}' specified an unrecognized target GenerationPolicy'{1}'"), FText::FromString(Name), FText::FromString(ConfigGenerationPolicyValue->AsString()));
			}
			return false;
		}
	}
	// If we can't find the value, GenerationPolicy already defaults to Never so it's ok 

	return true;
}

bool FLocalizationTargetDescriptor::Read(const FJsonObject& InObject, FText& OutFailReason)
{
	return Read(InObject, &OutFailReason);
}

bool FLocalizationTargetDescriptor::ReadArray(const FJsonObject& InObject, const TCHAR* InName, TArray<FLocalizationTargetDescriptor>& OutTargets, FText* OutFailReason /*= nullptr*/)
{
	bool bResult = true;

	TSharedPtr<FJsonValue> TargetsArrayValue = InObject.TryGetField(InName);
	if (TargetsArrayValue.IsValid() && TargetsArrayValue->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& TargetsArray = TargetsArrayValue->AsArray();
		for (const TSharedPtr<FJsonValue>& TargetValue : TargetsArray)
		{
			if (TargetValue.IsValid() && TargetValue->Type == EJson::Object)
			{
				FLocalizationTargetDescriptor Descriptor;
				if (Descriptor.Read(*TargetValue->AsObject().Get(), OutFailReason))
				{
					OutTargets.Add(Descriptor);
				}
				else
				{
					bResult = false;
				}
			}
			else
			{
				if (OutFailReason)
				{
					*OutFailReason = LOCTEXT("TargetWithInvalidTargetsArray", "The 'Localization Targets' array has invalid contents and was not able to be loaded.");
				}
				bResult = false;
			}
		}
	}

	return bResult;
}

bool FLocalizationTargetDescriptor::ReadArray(const FJsonObject& InObject, const TCHAR* InName, TArray<FLocalizationTargetDescriptor>& OutTargets, FText& OutFailReason)
{
	return ReadArray(InObject, InName, OutTargets, &OutFailReason);
}

void FLocalizationTargetDescriptor::Write(TJsonWriter<>& Writer) const
{
	TSharedRef<FJsonObject> DescriptorJsonObject = MakeShared<FJsonObject>();
	UpdateJson(*DescriptorJsonObject);

	FJsonSerializer::Serialize(DescriptorJsonObject, Writer);
}

void FLocalizationTargetDescriptor::UpdateJson(FJsonObject& JsonObject) const
{
	JsonObject.SetStringField(TEXT("Name"), Name);
	JsonObject.SetStringField(TEXT("LoadingPolicy"), FString(ELocalizationTargetDescriptorLoadingPolicy::ToString(LoadingPolicy)));
	JsonObject.SetStringField(TEXT("ConfigGenerationPolicy"), FString(ELocalizationConfigGenerationPolicy::ToString(ConfigGenerationPolicy)));
}

void FLocalizationTargetDescriptor::WriteArray(TJsonWriter<>& Writer, const TCHAR* ArrayName, const TArray<FLocalizationTargetDescriptor>& Descriptors)
{
	if (Descriptors.Num() > 0)
	{
		Writer.WriteArrayStart(ArrayName);
		for (const FLocalizationTargetDescriptor& Descriptor : Descriptors)
		{
			Descriptor.Write(Writer);
		}
		Writer.WriteArrayEnd();
	}
}

void FLocalizationTargetDescriptor::UpdateArray(FJsonObject& JsonObject, const TCHAR* ArrayName, const TArray<FLocalizationTargetDescriptor>& Descriptors)
{
	typedef FJsonObjectArrayUpdater<FLocalizationTargetDescriptor, FString> FLocTargetDescJsonArrayUpdater;

	FLocTargetDescJsonArrayUpdater::Execute(
		JsonObject, ArrayName, Descriptors,
		FLocTargetDescJsonArrayUpdater::FGetElementKey::CreateStatic(LocalizationDescriptor::GetDescriptorKey),
		FLocTargetDescJsonArrayUpdater::FTryGetJsonObjectKey::CreateStatic(LocalizationDescriptor::TryGetDescriptorJsonObjectKey),
		FLocTargetDescJsonArrayUpdater::FUpdateJsonObject::CreateStatic(LocalizationDescriptor::UpdateDescriptorJsonObject));
}

bool FLocalizationTargetDescriptor::ShouldLoadLocalizationTarget() const
{
	switch (LoadingPolicy)
	{
	case ELocalizationTargetDescriptorLoadingPolicy::Never:
		return false;

	case ELocalizationTargetDescriptorLoadingPolicy::Always:
		return true;

	case ELocalizationTargetDescriptorLoadingPolicy::Editor:
		return WITH_EDITOR;

	case ELocalizationTargetDescriptorLoadingPolicy::Game:
		return FApp::IsGame();

	case ELocalizationTargetDescriptorLoadingPolicy::PropertyNames:
#if WITH_EDITOR
		{
			bool bShouldUseLocalizedPropertyNames = false;
			if (!GConfig->GetBool(TEXT("Internationalization"), TEXT("ShouldUseLocalizedPropertyNames"), bShouldUseLocalizedPropertyNames, GEditorSettingsIni))
			{
				GConfig->GetBool(TEXT("Internationalization"), TEXT("ShouldUseLocalizedPropertyNames"), bShouldUseLocalizedPropertyNames, GEngineIni);
			}
			return bShouldUseLocalizedPropertyNames;
		}
#else	// WITH_EDITOR
		return false;
#endif	// WITH_EDITOR

	case ELocalizationTargetDescriptorLoadingPolicy::ToolTips:
		return WITH_EDITOR;

	default:
		ensureMsgf(false, TEXT("FLocalizationTargetDescriptor::ShouldLoadLocalizationTarget - Unrecognized ELocalizationTargetDescriptorLoadingPolicy value: %i"), LoadingPolicy);
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
