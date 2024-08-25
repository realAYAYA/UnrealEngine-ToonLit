// Copyright Epic Games, Inc. All Rights Reserved.

#include "ModuleDescriptor.h"
#include "Misc/App.h"
#include "Misc/ScopedSlowTask.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "JsonUtils/JsonObjectArrayUpdater.h"
#include "Modules/ModuleManager.h"
#include "JsonExtensions.h"

#define LOCTEXT_NAMESPACE "ModuleDescriptor"

namespace ModuleDescriptor
{
	FString GetModuleKey(const FModuleDescriptor& Module)
	{
		return Module.Name.ToString();
	}

	bool TryGetModuleJsonObjectKey(const FJsonObject& JsonObject, FString& OutKey)
	{
		return JsonObject.TryGetStringField(TEXT("Name"), OutKey);
	}

	void UpdateModuleJsonObject(const FModuleDescriptor& Module, FJsonObject& JsonObject)
	{
		Module.UpdateJson(JsonObject);
	}
}

ELoadingPhase::Type ELoadingPhase::FromString( const TCHAR *String )
{
	ELoadingPhase::Type TestType = (ELoadingPhase::Type)0;
	for(; TestType < ELoadingPhase::Max; TestType = (ELoadingPhase::Type)(TestType + 1))
	{
		const TCHAR *TestString = ToString(TestType);
		if(FCString::Stricmp(String, TestString) == 0)
		{
			break;
		}
	}
	return TestType;
}

const TCHAR* ELoadingPhase::ToString( const ELoadingPhase::Type Value )
{
	switch( Value )
	{
	case Default:
		return TEXT( "Default" );

	case PostDefault:
		return TEXT( "PostDefault" );

	case PreDefault:
		return TEXT( "PreDefault" );

	case PostConfigInit:
		return TEXT( "PostConfigInit" );

	case PostSplashScreen:
		return TEXT("PostSplashScreen");

	case PreEarlyLoadingScreen:
		return TEXT("PreEarlyLoadingScreen");

	case PreLoadingScreen:
		return TEXT( "PreLoadingScreen" );

	case PostEngineInit:
		return TEXT( "PostEngineInit" );

	case EarliestPossible:
		return TEXT("EarliestPossible");

	case None:
		return TEXT( "None" );

	default:
		ensureMsgf( false, TEXT( "Unrecognized ELoadingPhase value: %i" ), Value );
		return NULL;
	}
}

EHostType::Type EHostType::FromString( const TCHAR *String )
{
	EHostType::Type TestType = (EHostType::Type)0;
	for(; TestType < EHostType::Max; TestType = (EHostType::Type)(TestType + 1))
	{
		const TCHAR *TestString = ToString(TestType);
		if(FCString::Stricmp(String, TestString) == 0)
		{
			break;
		}
	}
	return TestType;
}

const TCHAR* EHostType::ToString( const EHostType::Type Value )
{
	switch( Value )
	{
		case Runtime:
			return TEXT( "Runtime" );

		case RuntimeNoCommandlet:
			return TEXT( "RuntimeNoCommandlet" );

		case RuntimeAndProgram:
			return TEXT( "RuntimeAndProgram" );

		case CookedOnly:
			return TEXT( "CookedOnly" );

		case UncookedOnly:
			return TEXT( "UncookedOnly" );

		case Developer:
			return TEXT( "Developer" );

		case DeveloperTool:
			return TEXT( "DeveloperTool" );

		case Editor:
			return TEXT( "Editor" );

		case EditorNoCommandlet:
			return TEXT( "EditorNoCommandlet" );

		case EditorAndProgram:
			return TEXT( "EditorAndProgram" );

		case Program:
			return TEXT("Program");

		case ServerOnly:
			return TEXT("ServerOnly");

		case ClientOnly:
			return TEXT("ClientOnly");

		case ClientOnlyNoCommandlet:
			return TEXT("ClientOnlyNoCommandlet");

		default:
			ensureMsgf( false, TEXT( "Unrecognized EModuleType value: %i" ), Value );
			return NULL;
	}
}

FModuleDescriptor::FModuleDescriptor(const FName InName, EHostType::Type InType, ELoadingPhase::Type InLoadingPhase)
	: Name(InName)
	, Type(InType)
	, LoadingPhase(InLoadingPhase)
	, bHasExplicitPlatforms(false)
{
}

bool FModuleDescriptor::Read(const FJsonObject& Object, FText* OutFailReason /*= nullptr*/)
{
	// Read the module name
	TSharedPtr<FJsonValue> NameValue = Object.TryGetField(TEXT("Name"));
	if(!NameValue.IsValid() || NameValue->Type != EJson::String)
	{
		if (OutFailReason)
		{
			*OutFailReason = LOCTEXT("ModuleWithoutAName", "Found a 'Module' entry with a missing 'Name' field");
		}
		return false;
	}
	Name = FName(*NameValue->AsString());

	// Read the module type
	TSharedPtr<FJsonValue> TypeValue = Object.TryGetField(TEXT("Type"));
	if(!TypeValue.IsValid() || TypeValue->Type != EJson::String)
	{
		if (OutFailReason)
		{
			*OutFailReason = FText::Format( LOCTEXT( "ModuleWithoutAType", "Found Module entry '{0}' with a missing 'Type' field" ), FText::FromName(Name) );
		}
		return false;
	}
	Type = EHostType::FromString(*TypeValue->AsString());
	if(Type == EHostType::Max)
	{
		if (OutFailReason)
		{
			*OutFailReason = FText::Format( LOCTEXT( "ModuleWithInvalidType", "Module entry '{0}' specified an unrecognized module Type '{1}'" ), FText::FromName(Name), FText::FromString(TypeValue->AsString()) );
		}
		return false;
	}

	// Read the loading phase
	TSharedPtr<FJsonValue> LoadingPhaseValue = Object.TryGetField(TEXT("LoadingPhase"));
	if(LoadingPhaseValue.IsValid() && LoadingPhaseValue->Type == EJson::String)
	{
		LoadingPhase = ELoadingPhase::FromString(*LoadingPhaseValue->AsString());
		if(LoadingPhase == ELoadingPhase::Max)
		{
			if (OutFailReason)
			{
				*OutFailReason = FText::Format( LOCTEXT( "ModuleWithInvalidLoadingPhase", "Module entry '{0}' specified an unrecognized module LoadingPhase '{1}'" ), FText::FromName(Name), FText::FromString(LoadingPhaseValue->AsString()) );
			}
			return false;
		}
	}

	// Read the allow/deny lists for platforms
	JsonExtensions::TryGetStringArrayFieldWithDeprecatedFallback(Object, TEXT("PlatformAllowList"), TEXT("WhitelistPlatforms"), /*out*/ PlatformAllowList);
	JsonExtensions::TryGetStringArrayFieldWithDeprecatedFallback(Object, TEXT("PlatformDenyList"), TEXT("BlacklistPlatforms"), /*out*/ PlatformDenyList);
	Object.TryGetBoolField(TEXT("HasExplicitPlatforms"), bHasExplicitPlatforms);

	// Read the allow/deny lists for targets
	JsonExtensions::TryGetEnumArrayFieldWithDeprecatedFallback(Object, TEXT("TargetAllowList"), TEXT("WhitelistTargets"), /*out*/ TargetAllowList);
	JsonExtensions::TryGetEnumArrayFieldWithDeprecatedFallback(Object, TEXT("TargetDenyList"), TEXT("BlacklistTargets"), /*out*/ TargetDenyList);

	// Read the allow/deny lists for target configurations
	JsonExtensions::TryGetEnumArrayFieldWithDeprecatedFallback(Object, TEXT("TargetConfigurationAllowList"), TEXT("WhitelistTargetConfigurations"), /*out*/ TargetConfigurationAllowList);
	JsonExtensions::TryGetEnumArrayFieldWithDeprecatedFallback(Object, TEXT("TargetConfigurationDenyList"), TEXT("BlacklistTargetConfigurations"), /*out*/ TargetConfigurationDenyList);

	// Read the allow/deny lists for programs
	JsonExtensions::TryGetStringArrayFieldWithDeprecatedFallback(Object, TEXT("ProgramAllowList"), TEXT("WhitelistPrograms"), /*out*/ ProgramAllowList);
	JsonExtensions::TryGetStringArrayFieldWithDeprecatedFallback(Object, TEXT("ProgramDenyList"), TEXT("BlacklistPrograms"), /*out*/ ProgramDenyList);

	// Read the additional dependencies
	Object.TryGetStringArrayField(TEXT("AdditionalDependencies"), AdditionalDependencies);

	return true;
}

bool FModuleDescriptor::Read(const FJsonObject& Object, FText& OutFailReason)
{
	return Read(Object, &OutFailReason);
}

bool FModuleDescriptor::ReadArray(const FJsonObject& Object, const TCHAR* Name, TArray<FModuleDescriptor>& OutModules, FText* OutFailReason /*= nullptr*/)
{
	bool bResult = true;

	TSharedPtr<FJsonValue> ModulesArrayValue = Object.TryGetField(Name);
	if(ModulesArrayValue.IsValid() && ModulesArrayValue->Type == EJson::Array)
	{
		const TArray< TSharedPtr< FJsonValue > >& ModulesArray = ModulesArrayValue->AsArray();
		for(int Idx = 0; Idx < ModulesArray.Num(); Idx++)
		{
			const TSharedPtr<FJsonValue>& ModuleValue = ModulesArray[Idx];
			if(ModuleValue.IsValid() && ModuleValue->Type == EJson::Object)
			{
				FModuleDescriptor Descriptor;
				if(Descriptor.Read(*ModuleValue->AsObject().Get(), OutFailReason))
				{
					OutModules.Add(Descriptor);
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
					*OutFailReason = LOCTEXT( "ModuleWithInvalidModulesArray", "The 'Modules' array has invalid contents and was not able to be loaded." );
				}
				bResult = false;
			}
		}
	}
	
	return bResult;
}

bool FModuleDescriptor::ReadArray(const FJsonObject& Object, const TCHAR* Name, TArray<FModuleDescriptor>& OutModules, FText& OutFailReason)
{
	return ReadArray(Object, Name, OutModules, &OutFailReason);
}

void FModuleDescriptor::Write(TJsonWriter<>& Writer) const
{
	TSharedRef<FJsonObject> ModuleJsonObject = MakeShared<FJsonObject>();
	UpdateJson(*ModuleJsonObject);

	FJsonSerializer::Serialize(ModuleJsonObject, Writer);
}

void FModuleDescriptor::UpdateJson(FJsonObject& JsonObject) const
{
	JsonObject.SetStringField(TEXT("Name"), Name.ToString());
	JsonObject.SetStringField(TEXT("Type"), FString(EHostType::ToString(Type)));
	JsonObject.SetStringField(TEXT("LoadingPhase"), FString(ELoadingPhase::ToString(LoadingPhase)));

	if (PlatformAllowList.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> PlatformAllowListValues;
		for (const FString& Entry : PlatformAllowList)
		{
			PlatformAllowListValues.Add(MakeShareable(new FJsonValueString(Entry)));
		}
		JsonObject.SetArrayField(TEXT("PlatformAllowList"), PlatformAllowListValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("PlatformAllowList"));
	}

	if (PlatformDenyList.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> PlatformDenyListValues;
		for (const FString& Entry : PlatformDenyList)
		{
			PlatformDenyListValues.Add(MakeShareable(new FJsonValueString(Entry)));
		}
		JsonObject.SetArrayField(TEXT("PlatformDenyList"), PlatformDenyListValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("PlatformDenyList"));
	}

	if (TargetAllowList.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TargetAllowListValues;
		for (EBuildTargetType Target : TargetAllowList)
		{
			TargetAllowListValues.Add(MakeShareable(new FJsonValueString(LexToString(Target))));
		}
		JsonObject.SetArrayField(TEXT("TargetAllowList"), TargetAllowListValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("TargetAllowList"));
	}

	if (TargetDenyList.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TargetDenyListValues;
		for (EBuildTargetType Target : TargetDenyList)
		{
			TargetDenyListValues.Add(MakeShareable(new FJsonValueString(LexToString(Target))));
		}
		JsonObject.SetArrayField(TEXT("TargetDenyList"), TargetDenyListValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("TargetDenyList"));
	}

	if (TargetConfigurationAllowList.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TargetConfigurationAllowListValues;
		for (EBuildConfiguration Config : TargetConfigurationAllowList)
		{
			TargetConfigurationAllowListValues.Add(MakeShareable(new FJsonValueString(LexToString(Config))));
		}
		JsonObject.SetArrayField(TEXT("TargetConfigurationAllowList"), TargetConfigurationAllowListValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("TargetConfigurationAllowList"));
	}

	if (TargetConfigurationDenyList.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> TargetConfigurationDenyListValues;
		for (EBuildConfiguration Config : TargetConfigurationDenyList)
		{
			TargetConfigurationDenyListValues.Add(MakeShareable(new FJsonValueString(LexToString(Config))));
		}
		JsonObject.SetArrayField(TEXT("TargetConfigurationDenyList"), TargetConfigurationDenyListValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("TargetConfigurationDenyList"));
	}

	if (ProgramAllowList.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ProgramAllowListValues;
		for (const FString& Program : ProgramAllowList)
		{
			ProgramAllowListValues.Add(MakeShareable(new FJsonValueString(Program)));
		}
		JsonObject.SetArrayField(TEXT("ProgramAllowList"), ProgramAllowListValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("ProgramAllowList"));
	}

	if (ProgramDenyList.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> ProgramDenyListValues;
		for (const FString& Program : ProgramDenyList)
		{
			ProgramDenyListValues.Add(MakeShareable(new FJsonValueString(Program)));
		}
		JsonObject.SetArrayField(TEXT("ProgramDenyList"), ProgramDenyListValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("ProgramDenyList"));
	}

	if (AdditionalDependencies.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> AdditionalDependencyValues;
		for (const FString& AdditionalDependency : AdditionalDependencies)
		{
			AdditionalDependencyValues.Add(MakeShareable(new FJsonValueString(AdditionalDependency)));
		}
		JsonObject.SetArrayField(TEXT("AdditionalDependencies"), AdditionalDependencyValues);
	}
	else
	{
		JsonObject.RemoveField(TEXT("AdditionalDependencies"));
	}

	if (bHasExplicitPlatforms)
	{
		JsonObject.SetBoolField(TEXT("HasExplicitPlatforms"), bHasExplicitPlatforms);
	}
	else
	{
		JsonObject.RemoveField(TEXT("HasExplicitPlatforms"));
	}

	// Clear away deprecated fields
	JsonObject.RemoveField(TEXT("WhitelistPlatforms"));
	JsonObject.RemoveField(TEXT("BlacklistPlatforms"));
	JsonObject.RemoveField(TEXT("WhitelistTargets"));
	JsonObject.RemoveField(TEXT("BlacklistTargets"));
	JsonObject.RemoveField(TEXT("WhitelistTargetConfigurations"));
	JsonObject.RemoveField(TEXT("BlacklistTargetConfigurations"));
	JsonObject.RemoveField(TEXT("WhitelistPrograms"));
	JsonObject.RemoveField(TEXT("BlacklistPrograms"));
}

void FModuleDescriptor::WriteArray(TJsonWriter<>& Writer, const TCHAR* ArrayName, const TArray<FModuleDescriptor>& Modules)
{
	if (Modules.Num() > 0)
	{
		Writer.WriteArrayStart(ArrayName);
		for(const FModuleDescriptor& Module : Modules)
		{
			Module.Write(Writer);
		}
		Writer.WriteArrayEnd();
	}
}

void FModuleDescriptor::UpdateArray(FJsonObject& JsonObject, const TCHAR* ArrayName, const TArray<FModuleDescriptor>& Modules)
{
	typedef FJsonObjectArrayUpdater<FModuleDescriptor, FString> FModuleJsonArrayUpdater;

	FModuleJsonArrayUpdater::Execute(
		JsonObject, ArrayName, Modules,
		FModuleJsonArrayUpdater::FGetElementKey::CreateStatic(ModuleDescriptor::GetModuleKey),
		FModuleJsonArrayUpdater::FTryGetJsonObjectKey::CreateStatic(ModuleDescriptor::TryGetModuleJsonObjectKey),
		FModuleJsonArrayUpdater::FUpdateJsonObject::CreateStatic(ModuleDescriptor::UpdateModuleJsonObject));
}

bool FModuleDescriptor::IsCompiledInConfiguration(const FString& Platform, EBuildConfiguration Configuration, const FString& TargetName, EBuildTargetType TargetType, bool bBuildDeveloperTools, bool bBuildRequiresCookedData) const
{
	// Check the platform is allowed
	if ((bHasExplicitPlatforms || PlatformAllowList.Num() > 0) && !PlatformAllowList.Contains(Platform))
	{
		return false;
	}

	// Check the platform is not denied
	if (PlatformDenyList.Contains(Platform))
	{
		return false;
	}

	// Check the target is allowed
	if (TargetAllowList.Num() > 0 && !TargetAllowList.Contains(TargetType))
	{
		return false;
	}

	// Check the target is not denied
	if (TargetDenyList.Contains(TargetType))
	{
		return false;
	}

	// Check the target configuration is allowed
	if (TargetConfigurationAllowList.Num() > 0 && !TargetConfigurationAllowList.Contains(Configuration))
	{
		return false;
	}

	// Check the target configuration is not denied
	if (TargetConfigurationDenyList.Contains(Configuration))
	{
		return false;
	}

	// Special checks just for programs
	if(TargetType == EBuildTargetType::Program)
	{
		// Check the program name is allowed. Note that this behavior is slightly different to other allow/deny checks; we will allow a module of any type if it's explicitly allowed for this program.
		if(ProgramAllowList.Num() > 0)
		{
			return ProgramAllowList.Contains(TargetName);
		}
				
		// Check the program name is not denied
		if(ProgramDenyList.Contains(TargetName))
		{
			return false;
		}
	}

	// Check the module is compatible with this target.
	switch (Type)
	{
	case EHostType::Runtime:
	case EHostType::RuntimeNoCommandlet:
        return TargetType != EBuildTargetType::Program;
	case EHostType::RuntimeAndProgram:
		return true;
	case EHostType::CookedOnly:
        return bBuildRequiresCookedData;
	case EHostType::UncookedOnly:
		return !bBuildRequiresCookedData;
	case EHostType::Developer:
		return TargetType == EBuildTargetType::Editor || TargetType == EBuildTargetType::Program;
	case EHostType::DeveloperTool:
		return bBuildDeveloperTools;
	case EHostType::Editor:
	case EHostType::EditorNoCommandlet:
		return TargetType == EBuildTargetType::Editor;
	case EHostType::EditorAndProgram:
		return TargetType == EBuildTargetType::Editor || TargetType == EBuildTargetType::Program;
	case EHostType::Program:
		return TargetType == EBuildTargetType::Program;
    case EHostType::ServerOnly:
        return TargetType != EBuildTargetType::Program && TargetType != EBuildTargetType::Client;
    case EHostType::ClientOnly:
	case EHostType::ClientOnlyNoCommandlet:
        return TargetType != EBuildTargetType::Program && TargetType != EBuildTargetType::Server;
    }

	return false;
}

bool FModuleDescriptor::IsCompiledInCurrentConfiguration() const
{
	return IsCompiledInConfiguration(FPlatformMisc::GetUBTPlatform(), FApp::GetBuildConfiguration(), UE_APP_NAME, FApp::GetBuildTargetType(), !!WITH_UNREAL_DEVELOPER_TOOLS, FPlatformProperties::RequiresCookedData());
}

bool FModuleDescriptor::IsLoadedInCurrentConfiguration() const
{
	// Check that the module is built for this configuration
	if(!IsCompiledInCurrentConfiguration())
	{
		return false;
	}

	// Always respect the allow/deny lists for program targets
	EBuildTargetType TargetType = FApp::GetBuildTargetType();
	if(TargetType == EBuildTargetType::Program)
	{
		const FString TargetName = UE_APP_NAME;

		// Check the program name is allowed. Note that this behavior is slightly different to other allow/deny list checks; we will allow a module of any type if it's explicitly allowed for this program.
		if(ProgramAllowList.Num() > 0)
		{
			return ProgramAllowList.Contains(TargetName);
		}
				
		// Check the program name is not denied
		if(ProgramDenyList.Contains(TargetName))
		{
			return false;
		}
	}

	// Check that the runtime environment allows it to be loaded
	switch (Type)
	{
	case EHostType::RuntimeAndProgram:
		#if (WITH_ENGINE || WITH_PLUGIN_SUPPORT)
			return true;
		#endif
		break;

	case EHostType::Runtime:
		#if (WITH_ENGINE || WITH_PLUGIN_SUPPORT) && !IS_PROGRAM
			return true;
		#endif
		break;
	
	case EHostType::RuntimeNoCommandlet:
		#if (WITH_ENGINE || WITH_PLUGIN_SUPPORT)  && !IS_PROGRAM
			if(!IsRunningCommandlet()) return true;
		#endif
		break;

	case EHostType::CookedOnly:
		return FPlatformProperties::RequiresCookedData();

	case EHostType::UncookedOnly:
		return !FPlatformProperties::RequiresCookedData();

	case EHostType::Developer:
		#if WITH_EDITOR || IS_PROGRAM
			return true;
		#else
			return false;
		#endif

	case EHostType::DeveloperTool:
		#if WITH_UNREAL_DEVELOPER_TOOLS
			return true;
		#else
			return false;
		#endif

	case EHostType::Editor:
		#if WITH_EDITOR
			#if !UE_EDITOR
				// SetIsRunningAsCommandlet() may be set after AppInit() via "late commandlet token" path
				ensure(LoadingPhase != ELoadingPhase::PostConfigInit && LoadingPhase != ELoadingPhase::EarliestPossible);
			#endif
			if(GIsEditor) return true;
		#endif
		break;

	case EHostType::EditorNoCommandlet:
		#if WITH_EDITOR
			#if !UE_EDITOR
				// SetIsRunningAsCommandlet() may be set after AppInit() via "late commandlet token" path
				ensure(LoadingPhase != ELoadingPhase::PostConfigInit && LoadingPhase != ELoadingPhase::EarliestPossible);
			#endif
			if(GIsEditor && !IsRunningCommandlet()) return true;
		#endif
		break;

	case EHostType::EditorAndProgram:
		#if WITH_EDITOR
			#if !UE_EDITOR
				// SetIsRunningAsCommandlet() may be set after AppInit() via "late commandlet token" path
				ensure(LoadingPhase != ELoadingPhase::PostConfigInit && LoadingPhase != ELoadingPhase::EarliestPossible);
			#endif
			return GIsEditor;
		#elif IS_PROGRAM
			return true;
		#else
			return false;
		#endif

	case EHostType::Program:
		#if WITH_PLUGIN_SUPPORT && IS_PROGRAM
			return true;
		#endif
		break;

	case EHostType::ServerOnly:
		return !FPlatformProperties::IsClientOnly();

	case EHostType::ClientOnlyNoCommandlet:
#if (WITH_ENGINE || WITH_PLUGIN_SUPPORT)  && !IS_PROGRAM
		return (!IsRunningDedicatedServer()) && (!IsRunningCommandlet());
#endif
		// the fall in the case of not having defines listed above is intentional
	case EHostType::ClientOnly:
		return !IsRunningDedicatedServer();
	
	}
	return false;
}

void FModuleDescriptor::LoadModulesForPhase(ELoadingPhase::Type LoadingPhase, const TArray<FModuleDescriptor>& Modules, TMap<FName, EModuleLoadResult>& ModuleLoadErrors)
{
	FScopedSlowTask SlowTask((float)Modules.Num());
	for (int Idx = 0; Idx < Modules.Num(); Idx++)
	{
		SlowTask.EnterProgressFrame(1);
		const FModuleDescriptor& Descriptor = Modules[Idx];

		// Don't need to do anything if this module is already loaded
		if (!FModuleManager::Get().IsModuleLoaded(Descriptor.Name))
		{
			if (LoadingPhase == Descriptor.LoadingPhase && Descriptor.IsLoadedInCurrentConfiguration())
			{
				// @todo plugin: DLL search problems.  Plugins that statically depend on other modules within this plugin may not be found?  Need to test this.

				// NOTE: Loading this module may cause other modules to become loaded, both in the engine or game, or other modules 
				//       that are part of this project or plugin.  That's totally fine.
				EModuleLoadResult FailureReason;
				IModuleInterface* ModuleInterface = FModuleManager::Get().LoadModuleWithFailureReason(Descriptor.Name, FailureReason);
				if (ModuleInterface == nullptr)
				{
					// The module failed to load. Note this in the ModuleLoadErrors list.
					ModuleLoadErrors.Add(Descriptor.Name, FailureReason);
				}
			}
		}
	}
}

void FModuleDescriptor::UnloadModulesForPhase(ELoadingPhase::Type LoadingPhase, const TArray<FModuleDescriptor>& Modules, TMap<FName, EModuleUnloadResult>& OutErrors, bool bSkipUnload /*= false*/, bool bAllowUnloadCode /*= true*/)
{
	FScopedSlowTask SlowTask((float)Modules.Num());
	for (const FModuleDescriptor& Descriptor : Modules)
	{
		SlowTask.EnterProgressFrame();

		if (LoadingPhase != Descriptor.LoadingPhase)
		{
			continue;
		}

		IModuleInterface* Module = FModuleManager::Get().GetModule(Descriptor.Name);
		if (!Module)
		{
			continue;
		}

		if (!Module->SupportsDynamicReloading())
		{
			OutErrors.Add(Descriptor.Name, EModuleUnloadResult::UnloadNotSupported);
			continue;
		}

		if (bSkipUnload)
		{
			// Useful to gather errors without actually unloading
			continue;
		}

		Module->PreUnloadCallback();
		verify(FModuleManager::Get().UnloadModule(Descriptor.Name, false, bAllowUnloadCode));
	}
}

#if !IS_MONOLITHIC
bool FModuleDescriptor::CheckModuleCompatibility(const TArray<FModuleDescriptor>& Modules, TArray<FString>& OutIncompatibleFiles)
{
	FModuleManager& ModuleManager = FModuleManager::Get();

	bool bResult = true;
	for (const FModuleDescriptor& Module : Modules)
	{
		if (Module.IsCompiledInCurrentConfiguration() && !ModuleManager.IsModuleUpToDate(Module.Name))
		{
			OutIncompatibleFiles.Add(Module.Name.ToString());
			bResult = false;
		}
	}
	return bResult;
}
#endif

#undef LOCTEXT_NAMESPACE
