// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemTest.h"
#include "ReplicationSystemTestPlugin/ReplicationSystemTestPlugin.h"
#include "RequiredProgramMainCPPInclude.h"
#include "Styling/UMGCoreStyle.h"
#include "AutomationTestRunner.h"
#include "NullTestRunner.h"
#include "Net/Core/Trace/Private/NetTraceInternal.h"
#include "Materials/Material.h"

#if WITH_AUTOMATION_WORKER
namespace UE::Net
{
	class FAutomationTestRunner;
}

typedef UE::Net::FAutomationTestRunner TestRunner;
#else
typedef FNullTestRunner TestRunner;
#endif

DEFINE_LOG_CATEGORY_STATIC(LogReplicationSystemTest, Log, All);

IMPLEMENT_APPLICATION(ReplicationSystemTest, "");

static void PreInit();
static void LoadModules();
static void PostInit();
static void TearDown();

#if defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE
int TestMain()
#else
INT32_MAIN_INT32_ARGC_TCHAR_ARGV()
#endif
{
	FString OriginalCmdLine;

#if !(defined(PLATFORM_XBOXONE) && PLATFORM_XBOXONE)
	// Parse original cmdline if there is one
	OriginalCmdLine = FCommandLine::BuildFromArgV(nullptr, ArgC, ArgV, nullptr);	
#endif

	// Due to some code not respecting nullrhi etc.
	PRIVATE_GIsRunningCommandlet = true;
	PRIVATE_GAllowCommandletRendering = false;
	PRIVATE_GAllowCommandletAudio = false;

	// Init cmd line used for test
	{
		FString CmdLineOverride(TEXT("-nullrhi -log -NoAsyncLoadingThread -NoAsyncPostLoad -noedl -unattended -ENGINEINI=Engine.ini -UseIrisReplication=1"));
		FString NetTraceVerbosity;
		if(FParse::Value(ToCStr(OriginalCmdLine), TEXT("NetTrace="), NetTraceVerbosity))
		{
			CmdLineOverride.Appendf(TEXT(" -trace=net,log,cpu -nettrace=%s"), ToCStr(NetTraceVerbosity));
		}

		FCommandLine::Set(ToCStr(CmdLineOverride));
	}

	PreInit();
	LoadModules();
	PostInit();

	UE_LOG(LogReplicationSystemTest, Display, TEXT("ReplicationSystemTest"));

	{
		TUniquePtr<TestRunner> Runner(new TestRunner());

		FString TestFilter;
		if (FParse::Value(ToCStr(OriginalCmdLine), TEXT("TestFilter="), TestFilter))
		{
			Runner->RunTests(ToCStr(TestFilter));
		}
		else
		{
			Runner->RunTests();
		}
		
	}

	TearDown();

	return 0;
}

static void TearDown()
{
	RequestEngineExit(TEXT("Shutting down ReplicationSystemTest"));

	FPlatformApplicationMisc::TearDown();
	FPlatformMisc::PlatformTearDown();

	if (GLog)
	{
		GLog->TearDown();
	}

	FCoreDelegates::OnExit.Broadcast();
	FModuleManager::Get().UnloadModulesAtShutdown();

#if STATS
	FThreadStats::StopThread();
#endif

	FTaskGraphInterface::Shutdown();

	if (GConfig)
	{
		GConfig->Exit();
		delete GConfig;
		GConfig = nullptr;
	}

	FTraceAuxiliary::Shutdown();
}

static void PreInit()
{
	FGenericPlatformOutputDevices::SetupOutputDevices();

	GError = FPlatformApplicationMisc::GetErrorOutputDevice();
	GWarn = FPlatformApplicationMisc::GetFeedbackContext();

	FPlatformMisc::PlatformInit();
#if WITH_APPLICATION_CORE
	FPlatformApplicationMisc::Init();
#endif
	FPlatformMemory::Init();

	// Initialize trace
	FString Parameter;
	if (FParse::Value(FCommandLine::Get(), TEXT("-trace="), Parameter, false))
	{
		FTraceAuxiliary::Initialize(FCommandLine::Get());
		FTraceAuxiliary::TryAutoConnect();
	}

#if UE_NET_TRACE_ENABLED
	uint32 NetTraceVerbosity;
	if(FParse::Value(FCommandLine::Get(), TEXT("NetTrace="), NetTraceVerbosity))
	{
		FNetTrace::SetTraceVerbosity(NetTraceVerbosity);
	}
#endif

	if (IPlatformFile* WrapperFile = FPlatformFileManager::Get().GetPlatformFile(TEXT("ReplicationSystemTestFile")))
	{
		IPlatformFile* CurrentPlatformFile = &FPlatformFileManager::Get().GetPlatformFile();
		WrapperFile->Initialize(CurrentPlatformFile, TEXT(""));
		FPlatformFileManager::Get().SetPlatformFile(*WrapperFile);
	}

#if WITH_COREUOBJECT
	// Initialize the PackageResourceManager, which is needed to load any (non-script) Packages. It is first used in ProcessNewlyLoadedObjects (due to the loading of asset references in Class Default Objects)
	// It has to be intialized after the AssetRegistryModule; the editor implementations of PackageResourceManager relies on it
	IPackageResourceManager::Initialize();
#endif

	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::FileSystemReady);

	FConfigCacheIni::InitializeConfigSystem();

	// Config overrides
	GConfig->SetInt(TEXT("/Script/Engine.GarbageCollectionSettings"), TEXT("gc.MaxObjectsNotConsideredByGC"), 0, GEngineIni);
	GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("AIControllerClassName"), TEXT("/Script/AIModule.AIController"), GEngineIni);
	GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("DefaultMaterialName"), TEXT("/Engine/Transient.MockDefaultMaterial"), GEngineIni);
	GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("DefaultLightFunctionMaterialName"), TEXT("/Engine/Transient.MockDefaultMaterial"), GEngineIni);
	GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("DefaultDeferredDecalMaterialName"), TEXT("/Engine/Transient.MockDefaultMaterial"), GEngineIni);
	GConfig->SetString(TEXT("/Script/Engine.Engine"), TEXT("DefaultPostProcessMaterialName"), TEXT("/Engine/Transient.MockDefaultMaterial"), GEngineIni);

	// Console commands
	IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Net.IsPushModelEnabled 1"), *GLog, nullptr);

	// Console commands
	IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Log LogIrisBridge Verbose"), *GLog, nullptr);


	GGameThreadId = FPlatformTLS::GetCurrentThreadId();
	FTaskGraphInterface::Startup(FPlatformMisc::NumberOfCores());
	FTaskGraphInterface::Get().AttachToThread(ENamedThreads::GameThread);

	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::TaskGraphSystemReady);

#if STATS
	FThreadStats::StartThread();
#endif

	FDelayedAutoRegisterHelper::RunAndClearDelayedAutoRegisterDelegates(EDelayedRegisterRunPhase::StatSystemReady);

	FCoreStyle::ResetToDefault();
	FUMGCoreStyle::ResetToDefault();
}

static void LoadModules()
{
	// Always attempt to load CoreUObject. It requires additional pre-init which is called from its module's StartupModule method.
#if WITH_COREUOBJECT
#if USE_PER_MODULE_UOBJECT_BOOTSTRAP // otherwise do it later
	FModuleManager::Get().OnProcessLoadedObjectsCallback().AddStatic(ProcessNewlyLoadedUObjects);
#endif
	FModuleManager::Get().LoadModule(TEXT("CoreUObject"));

	FCoreDelegates::OnInit.Broadcast();
#endif

	// Create a mock default material to keep the material system happy
	UMaterial* MockMaterial = NewObject<UMaterial>(GetTransientPackage(), UMaterial::StaticClass(), TEXT("MockDefaultMaterial"), RF_Transient | RF_MarkAsRootSet);

	// ChaosEngineSolvers requires ChaosSolvers. We're not able to call ProcessNewlyLoadedObjects before this module is loaded.
	FModuleManager::Get().LoadModule(TEXT("ChaosSolvers"));
	ProcessNewlyLoadedUObjects();

	FModuleManager::Get().LoadModule(TEXT("IrisCore"));

	FModuleManager::Get().LoadModule(TEXT("ReplicationSystemTestPlugin"));
}

static void PostInit()
{
#if WITH_COREUOBJECT
	// Required for GC to be allowed.
	if (GUObjectArray.IsOpenForDisregardForGC())
	{
		GUObjectArray.CloseDisregardForGC();
	}
#endif

	// Disable ini file operations
	GConfig->DisableFileOperations();
}
