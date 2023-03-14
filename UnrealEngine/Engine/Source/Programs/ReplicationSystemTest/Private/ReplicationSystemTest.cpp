// Copyright Epic Games, Inc. All Rights Reserved.

#include "ReplicationSystemTest.h"
#include "ReplicationSystemTestPlugin/ReplicationSystemTestPlugin.h"
#include "RequiredProgramMainCPPInclude.h"
#include "Styling/UMGCoreStyle.h"
#include "AutomationTestRunner.h"
#include "NullTestRunner.h"

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

bool GIsConsoleExecutable = true;

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
	// Due to some code not respecting nullrhi etc.
	PRIVATE_GIsRunningCommandlet = true;
	PRIVATE_GAllowCommandletRendering = false;
	PRIVATE_GAllowCommandletAudio = false;

	FCommandLine::Set(TEXT("-nullrhi -log -NoAsyncLoadingThread -NoAsyncPostLoad -noedl -unattended -ENGINEINI=Engine.ini"));

	PreInit();
	LoadModules();
	PostInit();

	UE_LOG(LogReplicationSystemTest, Display, TEXT("ReplicationSystemTest"));

	{
		TUniquePtr<TestRunner> Runner(new TestRunner());
		Runner->RunTests();
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

	// Console commands
	IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Net.IsPushModelEnabled 1"), *GLog, nullptr);

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
