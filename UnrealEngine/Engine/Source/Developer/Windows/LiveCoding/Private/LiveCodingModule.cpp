// Copyright Epic Games, Inc. All Rights Reserved.

#include "LiveCodingModule.h"
#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CoreDelegates.h"
#include "LiveCodingLog.h"
#include "External/LC_Commands.h"
#include "External/LC_EntryPoint.h"
#include "External/LC_API.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/MessageDialog.h"
#include "Misc/ScopedSlowTask.h"
#include "LiveCodingSettings.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "Windows/WindowsHWrapper.h"
#include "Algo/Sort.h"
#include "Algo/BinarySearch.h"
#include "HAL/LowLevelMemTracker.h"
#if WITH_EDITOR
	#include "Editor.h"
	#include "Kismet2/ReloadUtilities.h"
	#include "Widgets/Notifications/SNotificationList.h"
	#include "Framework/Notifications/NotificationManager.h"
#else
	#include "UObject/Reload.h"
#endif
#if WITH_ENGINE
	#include "Engine/Engine.h"
	#include "UObject/UObjectIterator.h"
	#include "UObject/StrongObjectPtr.h"
#endif

#include "Windows/AllowWindowsPlatformTypes.h"
#include <psapi.h> // for GetModuleFileNameEx
#ifdef NT_SUCCESS
#undef NT_SUCCESS
#endif
#include <subauth.h> // for UNICODE_STRING

IMPLEMENT_MODULE(FLiveCodingModule, LiveCoding)

#define LOCTEXT_NAMESPACE "LiveCodingModule"

LLM_DEFINE_TAG(LiveCoding);

bool GIsCompileActive = false;
bool GTriggerReload = false;
bool GHasLoadedPatch = false;
commands::PostCompileResult GPostCompileResult = commands::PostCompileResult::Success;
FString GLiveCodingConsolePath;
FString GLiveCodingConsoleArguments;
FLiveCodingModule* GLiveCodingModule = nullptr;

#if IS_MONOLITHIC
extern const TCHAR* GLiveCodingEngineDir;
extern const TCHAR* GLiveCodingProject;
#endif

#if !WITH_EDITOR
class FNullReload : public IReload
{
public:
	FNullReload(FLiveCodingModule& InLiveCodingModule)
		: LiveCodingModule(InLiveCodingModule)
	{
		BeginReload(EActiveReloadType::LiveCoding, *this);
	}

	~FNullReload()
	{
		EndReload();
	}

	virtual EActiveReloadType GetType() const
	{
		return EActiveReloadType::LiveCoding;
	}


	virtual const TCHAR* GetPrefix() const
	{
		return TEXT("LIVECODING");
	}

	virtual void NotifyFunctionRemap(FNativeFuncPtr NewFunctionPointer, FNativeFuncPtr OldFunctionPointer)
	{
	}

	virtual void NotifyChange(UClass* New, UClass* Old) override
	{
	}

	virtual void NotifyChange(UEnum* New, UEnum* Old) override
	{
	}

	virtual void NotifyChange(UScriptStruct* New, UScriptStruct* Old) override
	{
	}

	virtual void NotifyChange(UPackage* New, UPackage* Old) override
	{
	}

	virtual bool GetEnableReinstancing(bool bHasChanged) const
	{
		if (bHasChanged && !bEnabledMessage)
		{
			bEnabledMessage = true;
			bHasReinstancingOccurred = true;
			static const TCHAR* Message = TEXT("Object structure changes detected.  LiveCoding re-instancing isn't supported in builds without the editor");
			UE_LOG(LogLiveCoding, Error, TEXT("%s"), Message);
#if WITH_ENGINE
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(uint64(uintptr_t(&LiveCodingModule)), 5.f, FColor::Red, Message);
			}
#endif
		}
		return false;
	}

	virtual void Reinstance()
	{
	}

	bool HasReinstancingOccurred() const
	{
		return bHasReinstancingOccurred;
	}

	void Reset()
	{
		bHasReinstancingOccurred = false;
	}

private:
	FLiveCodingModule& LiveCodingModule;
	mutable bool bEnabledMessage = false;
	mutable bool bHasReinstancingOccurred = false;
};
#endif

// Helper structure to load the NTDLL library and work with an API
struct FNtDllFunction
{
	FARPROC Addr;

	FNtDllFunction(const char* Name)
	{
		HMODULE NtDll = LoadLibraryW(L"ntdll.dll");
		check(NtDll);
		Addr = GetProcAddress(NtDll, Name);
	}

	template <typename... ArgTypes>
	unsigned int operator () (ArgTypes... Args)
	{
		typedef unsigned int (NTAPI* Prototype)(ArgTypes...);
		return (Prototype((void*)Addr))(Args...);
	}
};

FLiveCodingModule::FLiveCodingModule()
	: FullEngineDir(FPaths::ConvertRelativePathToFull(FPaths::EngineDir()))
	, FullEnginePluginsDir(FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir()))
	, FullProjectDir(FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()))
	, FullProjectPluginsDir(FPaths::ConvertRelativePathToFull(FPaths::ProjectPluginsDir()))
{
	GLiveCodingModule = this;
}

FLiveCodingModule::~FLiveCodingModule()
{
	GLiveCodingModule = nullptr;
}

void FLiveCodingModule::StartupModule()
{
	LLM_SCOPE_BYTAG(LiveCoding);

	// Register with NT to get dll nitrifications
	FNtDllFunction RegisterFunc("LdrRegisterDllNotification");
	RegisterFunc(0, OnDllNotification, this, &CallbackCookie);

	// Get the creating process main executable name.  We want to skip this file when iterating through the dll list
	FString MainModuleName;
	{
		TCHAR Scratch[MAX_PATH];
		int Length = GetModuleFileNameEx(::GetCurrentProcess(), NULL, Scratch, MAX_PATH);
		MainModuleName = FString(Length, Scratch);
	}

	// https://docs.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-peb_ldr_data
	typedef struct _PEB_LDR_DATA {
		BYTE       Reserved1[8];
		PVOID      Reserved2[3];
		LIST_ENTRY InMemoryOrderModuleList;
	} PEB_LDR_DATA, * PPEB_LDR_DATA;

	// https://docs.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-peb
	typedef struct _PEB {
		BYTE                          Reserved1[2];
		BYTE                          BeingDebugged;
		BYTE                          Reserved2[1];
		PVOID                         Reserved3[2];
		PPEB_LDR_DATA                 Ldr;
		//...
	} PEB, * PPEB;

	// https://docs.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-teb
	typedef struct _TEB {
		PVOID Reserved1[12];
		PPEB  ProcessEnvironmentBlock;
		//...
	} TEB, * PTEB;

	// https://docs.microsoft.com/en-us/windows/win32/api/winternl/ns-winternl-peb_ldr_data
	typedef struct _LDR_DATA_TABLE_ENTRY {
		PVOID Reserved1[2];
		LIST_ENTRY InMemoryOrderLinks;
		PVOID Reserved2[2];
		PVOID DllBase;
		PVOID EntryPoint;
		PVOID Reserved3;
		UNICODE_STRING FullDllName;
		//...
	} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;

	// Enumerate already loaded modules.
	const TEB* ThreadEnvBlock = reinterpret_cast<TEB*>(NtCurrentTeb());
	const PEB* ProcessEnvBlock = ThreadEnvBlock->ProcessEnvironmentBlock;
	const LIST_ENTRY* ModuleIter = ProcessEnvBlock->Ldr->InMemoryOrderModuleList.Flink;
	const LIST_ENTRY* ModuleIterEnd = ModuleIter->Blink;
	do
	{
		const auto& ModuleData = *(LDR_DATA_TABLE_ENTRY*)(ModuleIter - 1);
		if (ModuleData.DllBase == 0)
		{
			break;
		}

		FString FullPath(ModuleData.FullDllName.Length / sizeof(ModuleData.FullDllName.Buffer[0]), ModuleData.FullDllName.Buffer);
		if (!FullPath.Equals(MainModuleName, ESearchCase::IgnoreCase))
		{
			FPaths::NormalizeFilename(FullPath);
			OnDllLoaded(FullPath);
		}
		ModuleIter = ModuleIter->Flink;
	} while (ModuleIter != ModuleIterEnd);

	Settings = GetMutableDefault<ULiveCodingSettings>();

	IConsoleManager& ConsoleManager = IConsoleManager::Get();

	EnableCommand = ConsoleManager.RegisterConsoleCommand(
		TEXT("LiveCoding"),
		TEXT("Enables live coding support"),
		FConsoleCommandWithOutputDeviceDelegate::CreateRaw(this, &FLiveCodingModule::EnableConsoleCommand),
		ECVF_Cheat
	);

	CompileCommand = ConsoleManager.RegisterConsoleCommand(
		TEXT("LiveCoding.Compile"),
		TEXT("Initiates a live coding compile"),
		FConsoleCommandDelegate::CreateLambda([this] { Compile(ELiveCodingCompileFlags::None, nullptr); }),
		ECVF_Cheat
	);

#if IS_MONOLITHIC
	FString DefaultEngineDir = GLiveCodingEngineDir;
#else
	FString DefaultEngineDir = FPaths::EngineDir();
#endif
#if USE_DEBUG_LIVE_CODING_CONSOLE
	static const TCHAR* DefaultConsolePath = TEXT("Binaries/Win64/LiveCodingConsole-Win64-Debug.exe");
#else
	static const TCHAR* DefaultConsolePath = TEXT("Binaries/Win64/LiveCodingConsole.exe");
#endif 
	ConsolePathVariable = ConsoleManager.RegisterConsoleVariable(
		TEXT("LiveCoding.ConsolePath"),
		FPaths::ConvertRelativePathToFull(DefaultEngineDir / DefaultConsolePath),
		TEXT("Path to the live coding console application"),
		ECVF_Cheat
	);

#if IS_MONOLITHIC
	FString SourceProject = (GLiveCodingProject != nullptr)? GLiveCodingProject : TEXT("");
#else
	FString SourceProject = FPaths::IsProjectFilePathSet() ? FPaths::GetProjectFilePath() : TEXT("");
#endif
	if (SourceProject.Len() > 0)
	{
		SourceProject = FPaths::ConvertRelativePathToFull(SourceProject);
	}
	SourceProjectVariable = ConsoleManager.RegisterConsoleVariable(
		TEXT("LiveCoding.SourceProject"),
		SourceProject,
		TEXT("Path to the project that this target was built from"),
		ECVF_Cheat
	);

	EndFrameDelegateHandle = FCoreDelegates::OnEndFrame.AddRaw(this, &FLiveCodingModule::Tick);

	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	if (SettingsModule != nullptr)
	{
		SettingsSection = SettingsModule->RegisterSettings("Editor", "General", "Live Coding",
			LOCTEXT("LiveCodingSettingsName", "Live Coding"),
			LOCTEXT("LiveCodintSettingsDescription", "Settings for recompiling C++ code while the engine is running."),
			GetMutableDefault<ULiveCodingSettings>()
		);
	}

	LppStartup();

	if (Settings->bEnabled && !FApp::IsUnattended())
	{
		if(Settings->Startup == ELiveCodingStartupMode::Automatic)
		{
			StartLiveCoding();
			ShowConsole();
		}
		else if(Settings->Startup == ELiveCodingStartupMode::AutomaticButHidden)
		{
			GLiveCodingConsoleArguments = L"-Hidden";
			StartLiveCoding();
		}
	}

	if(FParse::Param(FCommandLine::Get(), TEXT("LiveCoding")))
	{
		StartLiveCoding();
	}

	bEnabledLastTick = Settings->bEnabled;
	bEnableReinstancingLastTick = IsReinstancingEnabled();
}

void FLiveCodingModule::ShutdownModule()
{
	LppShutdown();

	FCoreDelegates::OnEndFrame.Remove(EndFrameDelegateHandle);

	IConsoleManager& ConsoleManager = IConsoleManager::Get();
	ConsoleManager.UnregisterConsoleObject(SourceProjectVariable);
	ConsoleManager.UnregisterConsoleObject(ConsolePathVariable);
	ConsoleManager.UnregisterConsoleObject(CompileCommand);
	ConsoleManager.UnregisterConsoleObject(EnableCommand);

	// Unregister from the dll notifications
	if (CallbackCookie)
	{
		FNtDllFunction UnregisterFunc("LdrUnregisterDllNotification");
		UnregisterFunc(CallbackCookie);
	}
}

void FLiveCodingModule::EnableByDefault(bool bEnable)
{
	if(Settings->bEnabled != bEnable)
	{
		Settings->bEnabled = bEnable;
		if(SettingsSection.IsValid())
		{
			SettingsSection->Save();
		}
	}
	EnableForSession(bEnable);
}

bool FLiveCodingModule::IsEnabledByDefault() const
{
	return Settings->bEnabled;
}

void FLiveCodingModule::EnableConsoleCommand(FOutputDevice& out)
{
	EnableForSession(true);

	// For packaged builds, by default it is unlikely that UE_LOG messages will be seen in the console.
	// So log any error directly.
	if (!EnableErrorText.IsEmpty())
	{
		out.Log(EnableErrorText);
	}
}

void FLiveCodingModule::EnableForSession(bool bEnable)
{
	if (bEnable)
	{
		EnableErrorText = FText::GetEmpty();
		if(!bStarted)
		{
			StartLiveCoding();
			ShowConsole();
		}
		else
		{
			bEnabledForSession = true;
			ShowConsole();
		}
	}
	else 
	{
		if(bStarted)
		{
			UE_LOG(LogLiveCoding, Display, TEXT("Console will be hidden but remain running in the background. Restart to disable completely."));
			LppSetActive(false);
			LppSetVisible(false);
			bEnabledForSession = false;
		}
	}
}

bool FLiveCodingModule::IsEnabledForSession() const
{
	return bEnabledForSession;
}

const FText& FLiveCodingModule::GetEnableErrorText() const
{
	return EnableErrorText;
}

bool FLiveCodingModule::CanEnableForSession() const
{
#if !IS_MONOLITHIC
	FModuleManager& ModuleManager = FModuleManager::Get();
	if(ModuleManager.HasAnyOverridenModuleFilename())
	{
		return false;
	}
#endif
	return true;
}

bool FLiveCodingModule::HasStarted() const
{
	return bStarted;
}

void FLiveCodingModule::ShowConsole()
{
	if (bStarted)
	{
		LppSetVisible(true);
		LppSetActive(true);
		LppShowConsole();
	}
}

void FLiveCodingModule::Compile()
{
	Compile(ELiveCodingCompileFlags::None, nullptr);
}

inline bool ReturnResults(ELiveCodingCompileResult InResult, ELiveCodingCompileResult* OutResult)
{
	if (OutResult != nullptr)
	{
		*OutResult = InResult;
	}
	return InResult == ELiveCodingCompileResult::Success || InResult == ELiveCodingCompileResult::NoChanges || InResult == ELiveCodingCompileResult::InProgress;
}

bool FLiveCodingModule::Compile(ELiveCodingCompileFlags CompileFlags, ELiveCodingCompileResult* Result)
{
	if (GIsCompileActive)
	{
		return ReturnResults(ELiveCodingCompileResult::CompileStillActive, Result);
	}

	EnableForSession(true);
	if (!bStarted)
	{
		return ReturnResults(ELiveCodingCompileResult::NotStarted, Result);
	}

	// Need to do this immediately rather than waiting until next tick
	UpdateModules(); 

	// Trigger the recompile
	GIsCompileActive = true;
	LastResults = ELiveCodingCompileResult::Failure;
	LppTriggerRecompile();

	// If we aren't waiting, just return now
	if (!EnumHasAnyFlags(CompileFlags, ELiveCodingCompileFlags::WaitForCompletion))
	{
		return ReturnResults(ELiveCodingCompileResult::InProgress, Result);
	}

	// Wait until we are no longer compiling.  Cancellation is handled via other mechanisms and
	// need not be detected in this loop.  GIsCompileActive will be cleared.
	FText StatusUpdate = LOCTEXT("CompileStatusMessage", "Compiling...");
	FScopedSlowTask SlowTask(0, StatusUpdate, GIsSlowTask);
	SlowTask.MakeDialog();

	// Wait until the compile completes
	while (GIsCompileActive)
	{
		SlowTask.EnterProgressFrame(0.0f);
		AttemptSyncLivePatching();
		FPlatformProcess::Sleep(0.01f);
	}

	// A final sync to get the result and complete the process
	AttemptSyncLivePatching();

	return ReturnResults(LastResults, Result);
}

bool FLiveCodingModule::IsCompiling() const
{
	return GIsCompileActive;
}

void FLiveCodingModule::Tick()
{
	if (LppWantsRestart())
	{
		LppRestart(lpp::LPP_RESTART_BEHAVIOR_REQUEST_EXIT, 0);
	}

	if (Settings->bEnabled != bEnabledLastTick && Settings->Startup != ELiveCodingStartupMode::Manual)
	{
		EnableForSession(Settings->bEnabled);
		bEnabledLastTick = Settings->bEnabled;
		if (IsEnabledByDefault() && !IsEnabledForSession())
		{
			FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("NoEnableLiveCodingAfterHotReload", "Live Coding cannot be enabled while hot-reloaded modules are active. Please close the editor and build from your IDE before restarting."));
		}
	}
	else if (IsEnabledForSession() && IsReinstancingEnabled() != bEnableReinstancingLastTick)
	{
		bEnableReinstancingLastTick = IsReinstancingEnabled();
		LppSetReinstancingFlow(bEnableReinstancingLastTick);
	}

	if (bUpdateModulesInTick)
	{
		UpdateModules();
		bUpdateModulesInTick = false;
	}

	AttemptSyncLivePatching();
}

void FLiveCodingModule::AttemptSyncLivePatching()
{

	// We use to wait for all commands to finish, but that causes a lock up if starting PIE after a compilation 
	// request caused another command to be sent to the live coding console.  For example, the registering of 
	// another lazy load module at PIE start would cause this problem.
	for (int Index = LppPendingTokens.Num(); Index-- > 0;)
	{
		if (LppTryWaitForToken(LppPendingTokens[Index]))
		{
			LppPendingTokens.RemoveAt(Index);
		}
	}

	// Needs to happen after updating modules, since "Quick Restart" functionality may try to install patch immediately
	extern void LppSyncPoint();
	LppSyncPoint();

	if ((!GIsCompileActive || GTriggerReload) && Reload.IsValid())
	{
		if (GHasLoadedPatch)
		{
#if WITH_COREUOBJECT && WITH_ENGINE

			// Collect the existing objects
			TArray<UObject*> StartingObjects;
			if (Reload->GetEnableReinstancing(false))
			{
				StartingObjects.Reserve(1024); // Arbitrary
				for (TObjectIterator<UObject> It(EObjectFlags::RF_NoFlags); It; ++It)
				{
					StartingObjects.Add(*It);
				}
				Algo::Sort(StartingObjects);
			}

			// During the module loading process, the list of changed classes will be recorded.  Invoking this method will 
			// result in the RegisterForReinstancing method being invoked which in turn records the classes in the ClassesToReinstance
			// member variable being populated.
			ProcessNewlyLoadedUObjects();

			// Complete the process of re-instancing without doing a GC
#if WITH_EDITOR
			Reload->Finalize(false);
#endif

			TArray<TStrongObjectPtr<UObject>> NewObjects;
			if (Reload->GetEnableReinstancing(false))
			{

				// Loop through the objects again looking for anything new that isn't associated with a
				// reinstanced class.
				for (TObjectIterator<UObject> It(EObjectFlags::RF_NoFlags); It; ++It)
				{
					if (Algo::BinarySearch(StartingObjects, *It) == INDEX_NONE)
					{
						if (!It->GetClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
						{
							NewObjects.Add(TStrongObjectPtr<UObject>(*It));
						}
					}
				}

				// Loop through all of the classes looking for classes that have been re-instanced.  Reset the CDO
				// to something that will never change.  Since these classes have been replaced, they should NEVER
				// have their CDos accessed again.  In the future we should try to figure out a better solution the issue
				// where the reinstanced crashes recreating the default object probably due to a mismatch between then
				// new constructor being invoked and the blueprint data associated with the old class.  With LC, the
				// old constructor has been replaced.
				static UObject* DummyDefaultObject = UObject::StaticClass()->ClassDefaultObject;
				for (TObjectIterator<UClass> It; It; ++It)
				{
					UClass* Class = *It;
					if (Class->GetName().StartsWith(TEXT("LIVECODING_")) ||
						Class->GetName().StartsWith(TEXT("REINST_")))
					{
						Class->ClassDefaultObject = DummyDefaultObject;
					}
				}
			}

			// Broadcast event prior to GC.  Otherwise some things are holding onto references
			FCoreUObjectDelegates::ReloadCompleteDelegate.Broadcast(EReloadCompleteReason::None);

			// Perform the GC to try and destruct all the objects which will be invoking the old destructors.
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
#endif

			// Second sync point to finish off the patching
			if (GTriggerReload)
			{
				LppSyncPoint();
			}

#if WITH_COREUOBJECT && WITH_ENGINE
			// Remove the reference to any new objects
			NewObjects.Empty();
#endif

			OnPatchCompleteDelegate.Broadcast();
			GHasLoadedPatch = false;

			bHasReinstancingOccurred |= Reload->HasReinstancingOccurred();
		}
		else if (GTriggerReload)
		{
			LppSyncPoint();
		}

		if (!GIsCompileActive)
		{
			static const FString Success("Live coding succeeded");

			// Reset this first so it does its logging first
			Reload.Reset();

			switch (GPostCompileResult)
			{
			case commands::PostCompileResult::Success:
				LastResults = ELiveCodingCompileResult::Success;
				if (bHasReinstancingOccurred)
				{
					if (!IsReinstancingEnabled())
					{
						UE_LOG(LogLiveCoding, Warning, TEXT("%s, %s"), *Success, TEXT("data type changes with re-instancing disabled is not supported and will likely lead to a crash"));
					}
					else
					{
#if WITH_EDITOR
						UE_LOG(LogLiveCoding, Warning, TEXT("%s, %s"), *Success, TEXT("data type changes may cause packaging to fail if assets reference the new or updated data types"));
#else
						UE_LOG(LogLiveCoding, Warning, TEXT("%s, %s"), *Success, TEXT("data type changes may cause unexpected failures"));
#endif
					}
				}
				else
				{
					UE_LOG(LogLiveCoding, Display, TEXT("%s"), *Success);
				}
				break;
			case commands::PostCompileResult::NoChanges:
				LastResults = ELiveCodingCompileResult::NoChanges;
				UE_LOG(LogLiveCoding, Display, TEXT("%s, %s"), *Success, TEXT("no code changes detected"));
				break;
			case commands::PostCompileResult::Cancelled:
				LastResults = ELiveCodingCompileResult::Cancelled;
				UE_LOG(LogLiveCoding, Error, TEXT("Live coding canceled"));
				break;
			case commands::PostCompileResult::Failure:
				LastResults = ELiveCodingCompileResult::Failure;
				UE_LOG(LogLiveCoding, Error, TEXT("Live coding failed, please see Live console for more information"));
				break;
			default:
				LastResults = ELiveCodingCompileResult::Failure;
				check(false);
			}

#if WITH_EDITOR
			static const FText SuccessText = LOCTEXT("Success", "Live coding succeeded");
			static const FText NoChangesText = LOCTEXT("NoChanges", "No code changes were detected.");
			static const FText FailureText = LOCTEXT("Failed", "Live coding failed");
			static const FText FailureDetailText = LOCTEXT("FailureDetail", "Please see Live Coding console for more information.");
			static const FText CancelledText = LOCTEXT("Cancelled", "Live coding cancelled");
			static const FText ReinstancingText = LOCTEXT("Reinstancing", "Data type changes may cause packaging to fail if assets reference the new or updated data types.");
			static const FText DisabledText = LOCTEXT("ReinstancingDisabled", "Data type changes with re-instancing disabled is not supported and will likely lead to a crash.");

			switch (GPostCompileResult)
			{
			case commands::PostCompileResult::Success:
				if (bHasReinstancingOccurred)
				{
					if (!IsReinstancingEnabled())
					{
						ShowNotification(true, SuccessText, &DisabledText);
					}
					else
					{
						ShowNotification(true, SuccessText, &ReinstancingText);
					}
				}
				else
				{
					ShowNotification(true, SuccessText, nullptr);
				}
				break;
			case commands::PostCompileResult::NoChanges:
				ShowNotification(true, SuccessText, &NoChangesText);
				break;
			case commands::PostCompileResult::Cancelled:
				ShowNotification(false, CancelledText, nullptr);
				break;
			case commands::PostCompileResult::Failure:
				ShowNotification(false, FailureText, &FailureDetailText);
				break;
			default:
				check(false);
			}
#endif
		}
		else
		{
			Reload->Reset();
		}
	}
	GTriggerReload = false;
}

#if WITH_EDITOR
void FLiveCodingModule::ShowNotification(bool Success, const FText& Title, const FText* SubText)
{
	FNotificationInfo Info(Title);
	Info.ExpireDuration = 5.0f;
	Info.bUseSuccessFailIcons = true;
	if (SubText)
	{
		Info.SubText = *SubText;
	}
	TSharedPtr<SNotificationItem> CompileNotification = FSlateNotificationManager::Get().AddNotification(Info);
	CompileNotification->SetCompletionState(Success ? SNotificationItem::CS_Success : SNotificationItem::CS_Fail);
}
#endif

ILiveCodingModule::FOnPatchCompleteDelegate& FLiveCodingModule::GetOnPatchCompleteDelegate()
{
	return OnPatchCompleteDelegate;
}

bool FLiveCodingModule::StartLiveCoding()
{
	EnableErrorText = FText::GetEmpty();
	if(!bStarted)
	{
		// Make sure there aren't any hot reload modules already active
		if (!CanEnableForSession())
		{
			EnableErrorText = LOCTEXT("NoLiveCodingCompileAfterHotReload", "Live Coding cannot be enabled while hot-reloaded modules are active. Please close the editor and build from your IDE before restarting.");
			UE_LOG(LogLiveCoding, Error, TEXT("Unable to start live coding session. Some modules have already been hot reloaded."));
			return false;
		}

		// Setup the console path
		GLiveCodingConsolePath = ConsolePathVariable->GetString();
		if (!FPaths::FileExists(GLiveCodingConsolePath))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("Executable"), FText::FromString(GLiveCodingConsolePath));
			const static FText FormatString = LOCTEXT("LiveCodingMissingExecutable", "Unable to start live coding session. Missing executable '{Executable}'. Use the LiveCoding.ConsolePath console variable to modify.");
			EnableErrorText = FText::Format(FormatString, Args);
			UE_LOG(LogLiveCoding, Error, TEXT("Unable to start live coding session. Missing executable '%s'. Use the LiveCoding.ConsolePath console variable to modify."), *GLiveCodingConsolePath);
			return false;
		}

		// Get the source project filename
		FString SourceProject = SourceProjectVariable->GetString();
		if (SourceProject.Len() > 0 && !FPaths::FileExists(SourceProject))
		{
			FFormatNamedArguments Args;
			Args.Add(TEXT("ProjectFile"), FText::FromString(SourceProject));
			const static FText FormatString = LOCTEXT("LiveCodingMissingProjectFile", "Unable to start live coding session. Unable to find source project file '{ProjectFile}'.");
			EnableErrorText = FText::Format(FormatString, Args);
			UE_LOG(LogLiveCoding, Error, TEXT("Unable to start live coding session. Unable to find source project file '%s'."), *SourceProject);
			return false;
		}

		UE_LOG(LogLiveCoding, Display, TEXT("Starting LiveCoding"));

		// Enable external build system
		LppUseExternalBuildSystem();

		// Enable the server
		FString ProjectPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()).ToLower();
		FString ProcessGroup = FString::Printf(TEXT("UE_%s_0x%08x"), FApp::GetProjectName(), GetTypeHash(ProjectPath));
		LppRegisterProcessGroup(TCHAR_TO_ANSI(*ProcessGroup));

		// Build the command line
		FString KnownTargetName = FPlatformMisc::GetUBTTargetName();
		FString Arguments = FString::Printf(TEXT("%s %s %s"),
			*KnownTargetName,
			FPlatformMisc::GetUBTPlatform(),
			LexToString(FApp::GetBuildConfiguration()));

		UE_LOG(LogLiveCoding, Display, TEXT("LiveCodingConsole Arguments: %s"), *Arguments);

		if(SourceProject.Len() > 0)
		{
			Arguments += FString::Printf(TEXT(" -Project=\"%s\""), *FPaths::ConvertRelativePathToFull(SourceProject));
		}
		LppSetBuildArguments(*Arguments);

#if WITH_EDITOR
		if (IsReinstancingEnabled())
		{
			LppSetReinstancingFlow(true);
		}

		if (GEditor != nullptr)
		{
			LppDisableCompileFinishNotification();
		}
#endif

		// Create a mutex that allows UBT to detect that we shouldn't hot-reload into this executable. The handle to it will be released automatically when the process exits.
		FString ExecutablePath = FPaths::ConvertRelativePathToFull(FPlatformProcess::ExecutablePath());

		FString MutexName = TEXT("Global\\LiveCoding_");
		for (int Idx = 0; Idx < ExecutablePath.Len(); Idx++)
		{
			TCHAR Character = ExecutablePath[Idx];
			if (Character == '/' || Character == '\\' || Character == ':')
			{
				MutexName += '+';
			}
			else
			{
				MutexName += Character;
			}
		}

		ensure(CreateMutex(NULL, FALSE, *MutexName));

		// Configure all the current modules. For non-commandlets, schedule it to be done in the first Tick() so we can batch everything together.
		if (IsRunningCommandlet())
		{
			UpdateModules();
		}
		else
		{
			bUpdateModulesInTick = true;
		}

		// Mark it as started
		bStarted = true;
		bEnabledForSession = true;
	}
	return true;
}

void FLiveCodingModule::UpdateModules()
{
	TArray<ModuleChange> Changes;
	{
		FScopeLock lock(&ModuleChangeCs);
		Swap(Changes, ModuleChanges);
	}

	if (bEnabledForSession)
	{
#if IS_MONOLITHIC
		wchar_t FullFilePath[WINDOWS_MAX_PATH];
		verify(GetModuleFileName(hInstance, FullFilePath, UE_ARRAY_COUNT(FullFilePath)));
		LppEnableModule(FullFilePath);
#else
		// Collect the list of preloaded modules
		TSet<FName> PreloadedFileNames;
		{
			FModuleManager& Manager = FModuleManager::Get();
			for (FName moduleName : Settings->PreloadNamedModules)
			{
				FString FileName = Manager.GetModuleFilename(moduleName);
				if (!FileName.IsEmpty())
				{
					PreloadedFileNames.Add(FName(FPaths::GetBaseFilename(FileName, true)));
				}
			}
		}

		TArray<FString> EnableModules;
		for (const ModuleChange& Change : Changes)
		{
			if (Change.bLoaded)
			{
				ConfiguredModules.Add(Change.FullName);
				FString FullFilePath(Change.FullName.ToString());
				if (ShouldPreloadModule(PreloadedFileNames, FullFilePath))
				{
					EnableModules.Add(FullFilePath);
				}
				else
				{
					TRACE_CPUPROFILER_EVENT_SCOPE(LppEnableLazyLoadedModule);
					void* LppEnableLazyLoadedModuleToken = LppEnableLazyLoadedModule(*FullFilePath);
					LppPendingTokens.Add(LppEnableLazyLoadedModuleToken);
				}
			}
			else
			{
				ConfiguredModules.Remove(Change.FullName);
			}
		}

		if (EnableModules.Num() > 0)
		{
			TArray<const TCHAR*> EnableModuleFileNames;
			for (const FString& EnableModule : EnableModules)
			{
				EnableModuleFileNames.Add(*EnableModule);
			}

			{
				TRACE_CPUPROFILER_EVENT_SCOPE(LppEnableModules);
				void* LppEnableModulesToken = LppEnableModules(EnableModuleFileNames.GetData(), EnableModuleFileNames.Num());
				LppPendingTokens.Add(LppEnableModulesToken);
			}
		}
#endif
	}
}

void FLiveCodingModule::OnModulesChanged(FName ModuleName, EModuleChangeReason Reason)
{
#if !IS_MONOLITHIC
	if (Reason == EModuleChangeReason::ModuleLoaded)
	{
		// Assume that Tick() won't be called if we're running a commandlet
		if (IsRunningCommandlet())
		{
			UpdateModules();
		}
		else
		{
			bUpdateModulesInTick = true;
		}
	}
#endif
}

bool FLiveCodingModule::ShouldPreloadModule(const TSet<FName>& PreloadedFileNames, const FString& FullFilePath) const
{

	// Perform some name based checks
	{
		FString FileName = FPaths::GetBaseFilename(FullFilePath, true);
		FName Name(FileName, FNAME_Find);
		if (Name != NAME_None && PreloadedFileNames.Contains(Name))
		{
			return true;
		}
	}

	if (FullFilePath.StartsWith(FullProjectDir, ESearchCase::IgnoreCase))
	{
		if (Settings->bPreloadProjectModules == Settings->bPreloadProjectPluginModules)
		{
			return Settings->bPreloadProjectModules;
		}

		if(FullFilePath.StartsWith(FullProjectPluginsDir, ESearchCase::IgnoreCase))
		{
			return Settings->bPreloadProjectPluginModules;
		}
		else
		{
			return Settings->bPreloadProjectModules;
		}
	}
	else
	{
		if (FApp::IsEngineInstalled())
		{
			return false;
		}

		if (Settings->bPreloadEngineModules == Settings->bPreloadEnginePluginModules)
		{
			return Settings->bPreloadEngineModules;
		}

		if(FullFilePath.StartsWith(FullEnginePluginsDir, ESearchCase::IgnoreCase))
		{
			return Settings->bPreloadEnginePluginModules;
		}
		else
		{
			return Settings->bPreloadEngineModules;
		}
	}
}

void FLiveCodingModule::BeginReload()
{
	if (GLiveCodingModule != nullptr)
	{
		if (!GLiveCodingModule->Reload.IsValid())
		{
			GLiveCodingModule->bHasReinstancingOccurred = false;
			GLiveCodingModule->bHasPatchBeenLoaded = false;
			GPostCompileResult = commands::PostCompileResult::Success;
#if WITH_EDITOR
			GLiveCodingModule->Reload.Reset(new FReload(EActiveReloadType::LiveCoding, TEXT("LIVECODING"), *GLog));
			GLiveCodingModule->Reload->SetEnableReinstancing(GLiveCodingModule->IsReinstancingEnabled());
			GLiveCodingModule->Reload->SetSendReloadCompleteNotification(false);
#else
			GLiveCodingModule->Reload.Reset(new FNullReload(*GLiveCodingModule));
#endif
		}
	}
}

bool FLiveCodingModule::IsReinstancingEnabled() const
{
#if WITH_EDITOR
	return Settings->bEnableReinstancing;
#else
	return false;
#endif
}

bool FLiveCodingModule::AutomaticallyCompileNewClasses() const
{
	return Settings->bAutomaticallyCompileNewClasses;
}

void FLiveCodingModule::OnDllNotification(unsigned int Reason, const void* DataPtr, void* Context)
{
	FLiveCodingModule* This = reinterpret_cast<FLiveCodingModule*>(Context);

	enum
	{
		LDR_DLL_NOTIFICATION_REASON_LOADED = 1,
		LDR_DLL_NOTIFICATION_REASON_UNLOADED = 2,
	};

	struct FNotificationData
	{
		uint32 Flags;
		const UNICODE_STRING& FullPath;
		const UNICODE_STRING& BaseName;
		UPTRINT	Base;
	};
	const auto& Data = *(FNotificationData*)DataPtr;
	FString FullPath(Data.FullPath.Length / sizeof(Data.FullPath.Buffer[0]), Data.FullPath.Buffer);
	FPaths::NormalizeFilename(FullPath);

	switch (Reason)
	{
	case LDR_DLL_NOTIFICATION_REASON_LOADED:
		This->OnDllLoaded(FullPath);
		break;

	case LDR_DLL_NOTIFICATION_REASON_UNLOADED:
		This->OnDllUnloaded(FullPath);
		break;
	}
}

void FLiveCodingModule::OnDllLoaded(const FString& FullPath)
{
	if (IsUEDll(FullPath))
	{
		FScopeLock lock(&ModuleChangeCs);
		ModuleChanges.Emplace(ModuleChange{ FName(FullPath), true });
	}
}

void FLiveCodingModule::OnDllUnloaded(const FString& FullPath)
{
	if (IsUEDll(FullPath))
	{
		FScopeLock lock(&ModuleChangeCs);
		ModuleChanges.Emplace(ModuleChange{ FName(FullPath), false });
	}
}


bool FLiveCodingModule::IsUEDll(const FString& FullPath)
{
	// Ignore patches.
	if (IsPatchDll(FullPath))
	{
		return false;
	}

	// Dll must be in the engine or project dir
	if (!FullPath.StartsWith(FullEngineDir, ESearchCase::IgnoreCase) &&
		!FullPath.StartsWith(FullEnginePluginsDir, ESearchCase::IgnoreCase) &&
		!FullPath.StartsWith(FullProjectDir, ESearchCase::IgnoreCase) &&
		!FullPath.StartsWith(FullProjectPluginsDir, ESearchCase::IgnoreCase))
	{
		return false;
	}
	return true;
}

bool FLiveCodingModule::IsPatchDll(const FString& FullPath)
{
	// If the Dll ends with ".patch_#.dll", then ignore it
	FString Name = FPaths::GetBaseFilename(FullPath, true);
	FString Extension = FPaths::GetExtension(Name);
	if (!Extension.StartsWith("patch_", ESearchCase::IgnoreCase))
	{
		return false;
	}

	for (int Index = 6; Index < Extension.Len(); ++Index)
	{
		if (!FChar::IsDigit(Extension[Index]))
		{
			return false;
		}
	}
	return true;
}

// Invoked from LC_ClientCommandActions
void LiveCodingBeginPatch()
{
	GHasLoadedPatch = true;
	// If we are beginning a patch from a restart from the console, we need to create the reload object
	FLiveCodingModule::BeginReload();
}

// Invoked from LC_ClientCommandActions
void LiveCodingEndCompile()
{
	GIsCompileActive = false;
}

// Invoked from LC_ClientCommandActions
void LiveCodingPreCompile()
{
	UE_LOG(LogLiveCoding, Display, TEXT("Starting Live Coding compile."));
	GIsCompileActive = true;
	if (GLiveCodingModule != nullptr)
	{
		GLiveCodingModule->BeginReload();
	}
}

// Invoked from LC_ClientCommandActions
void LiveCodingPostCompile(commands::PostCompileResult PostCompileResult)
{
	GPostCompileResult = PostCompileResult;
	GIsCompileActive = false;
}

// Invoked from LC_ClientCommandActions
void LiveCodingTriggerReload()
{
	GTriggerReload = true;
}

#undef LOCTEXT_NAMESPACE
