// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreLoadScreen.h"

#include "CoreMinimal.h"

#include "Widgets/SVirtualWindow.h"
#include "Widgets/SWindow.h"

#include "TickableObjectRenderThread.h"

#include "Containers/Ticker.h"

#include "PreLoadSlateThreading.h"


// Class that handles storing all registered PreLoadScreens and Playing/Stopping them
class PRELOADSCREEN_API FPreLoadScreenManager
{
public:
    //Gets the single instance of this settings object. Also creates it if needed
    static FPreLoadScreenManager* Get();
    static void Create();
    static void Destroy();

    void Initialize(FSlateRenderer& InSlateRenderer);

    void RegisterPreLoadScreen(const TSharedPtr<IPreLoadScreen>& PreLoadScreen);
    void UnRegisterPreLoadScreen(const TSharedPtr<IPreLoadScreen>& PreLoadScreen);

    /**
	 * Plays the first found PreLoadScreen that matches the bEarlyPreLoadScreen setting passed in.
	 * @returns false if no PreLoadScreen with that type has been registered.
	 */
    bool PlayFirstPreLoadScreen(EPreLoadScreenTypes PreLoadScreenTypeToPlay);

	/**
	 * Plays the PreLoadScreen with a tag that matches InTag
	 * @returns false if no PreLoadScreen with that tag has been registered.
	 */
	bool PlayPreLoadScreenWithTag(FName InTag);
    
	void StopPreLoadScreen();
	void WaitForEngineLoadingScreenToFinish();

    void PassPreLoadScreenWindowBackToGame() const;

    bool IsUsingMainWindow() const;

    TSharedPtr<SWindow> GetRenderWindow();

    bool HasRegisteredPreLoadScreenType(EPreLoadScreenTypes PreLoadScreenTypeToCheck) const;
    bool HasActivePreLoadScreenType(EPreLoadScreenTypes PreLoadScreenTypeToCheck) const;
    bool HasValidActivePreLoadScreen() const;


    void SetEngineLoadingComplete(bool IsEngineLoadingFinished = true);
    bool IsEngineLoadingComplete() const { return bIsEngineLoadingComplete; }

    static void EnableRendering(bool bEnabled);
    static bool ArePreLoadScreensEnabled();
    
    // Callback for handling cleaning up any resources you would like to remove after the PreLoadScreenManager cleans up
    // Not needed for PreLoadScreens as those have a separate CleanUp method called.
    DECLARE_MULTICAST_DELEGATE(FOnPreLoadScreenManagerCleanUp);
    FOnPreLoadScreenManagerCleanUp OnPreLoadScreenManagerCleanUp;

	DECLARE_MULTICAST_DELEGATE_OneParam(FIsPreloadScreenResponsibleForRenderingMultiDelegate, bool);
	FIsPreloadScreenResponsibleForRenderingMultiDelegate IsResponsibleForRenderingDelegate;

protected:
    //Default constructor. We don't want other classes to make these. Should just rely on Get()
    FPreLoadScreenManager();

	~FPreLoadScreenManager() = default;

	void PlayPreLoadScreenAtIndex(int32 Index);

    void BeginPlay();

    /*** These functions describe the flow for an EarlyPreLoadScreen where everything is blocking waiting on a call to StopPreLoadScreen ***/
    void HandleEarlyStartupPlay();
    //Separate tick that handles 
    void EarlyPlayFrameTick();
    void GameLogicFrameTick();
    void EarlyPlayRenderFrameTick();
	bool HasActivePreLoadScreenTypeForEarlyStartup() const;

	void PlatformSpecificGameLogicFrameTick();

	//Creates a tick on the Render Thread that we run every
	static void StaticRenderTick_RenderThread();
	void RenderTick_RenderThread();

    /*** These functions describe how everything is handled during an non-Early PreLoadPlay. Everything is handled asynchronously in this case with a standalone renderer ***/
	void HandleEngineLoadingPlay();

	/*** These functions describe the flow for showing an CustomSplashScreen ***/
	void HandleCustomSplashScreenPlay();

    IPreLoadScreen* GetActivePreLoadScreen();
    const IPreLoadScreen* GetActivePreLoadScreen() const;

	void HandleStopPreLoadScreen();
	void CleanUpResources();

	//Helpers that setup and clean-up delegates that only need to be active while we are playing an EarlyStartup PreLoadScreen
	void RegisterDelegatesForEarlyStartupPlay();
	void CleanUpDelegatesForEarlyStartupPlay();

	void HandleFlushRenderingCommandsStart();
	void HandleFlushRenderingCommandsEnd();

	//Singleton Instance
	struct FPreLoadScreenManagerDelete
	{
		void operator()(FPreLoadScreenManager* Ptr) const
		{
			delete Ptr;
		}
	};
	static TUniquePtr<FPreLoadScreenManager, FPreLoadScreenManagerDelete> Instance;

	static FCriticalSection AcquireCriticalSection;
	static TAtomic<bool> bRenderingEnabled;

	TArray<TSharedPtr<IPreLoadScreen>> PreLoadScreens;

    int32 ActivePreLoadScreenIndex;
    double LastTickTime;

    /** Widget renderer used to tick and paint windows in a thread safe way */
    TSharedPtr<FPreLoadSlateWidgetRenderer, ESPMode::ThreadSafe> WidgetRenderer;

    /** The window that the loading screen resides in */
    TWeakPtr<class SWindow> MainWindow;

    /** Virtual window that we render to instead of the main slate window (for thread safety).  Shares only the same backbuffer as the main window */
    TSharedPtr<class SVirtualWindow> VirtualRenderWindow;

    bool bInitialized;

    /** The threading mechanism with which we handle running slate on another thread */
	FPreLoadScreenSlateSynchMechanism* SyncMechanism;
	friend FPreLoadScreenSlateSynchMechanism;

	bool bIsResponsibleForRendering;
	bool bHasRenderPreLoadScreenFrame_RenderThread;

    double LastRenderTickTime;

    float OriginalSlateSleepVariableValue;
    bool bIsEngineLoadingComplete;

private:
#if PLATFORM_ANDROID
	void Android_PlatformSpecificGameLogicFrameTick();
#endif //PLATFORM_ANDROID

#if PLATFORM_IOS
	void IOS_PlatformSpecificGameLogicFrameTick();
#endif //PLATFORM_IOS
};
