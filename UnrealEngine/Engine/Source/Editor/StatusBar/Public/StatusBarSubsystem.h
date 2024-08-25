// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "Framework/Notifications/NotificationManager.h"
#include "GlobalStatusBarExtension.h"
#include "StatusBarSubsystem.generated.h"

class SStatusBar;
class SWindow;
class SWidget;
class SDockTab;
struct FWidgetDrawerConfig;

template<typename ObjectType> 
class TAttribute;

struct FStatusBarMessageHandle
{
	friend class UStatusBarSubsystem;

	FStatusBarMessageHandle()
		: Id(INDEX_NONE)
	{}

	bool IsValid()
	{
		return Id != INDEX_NONE;
	}

	void Reset()
	{
		Id = INDEX_NONE;
	}

	bool operator==(const FStatusBarMessageHandle& OtherHandle) const
	{
		return Id == OtherHandle.Id;
	}
private:
	FStatusBarMessageHandle(int32 InId)
		: Id(InId)
	{}

	int32 Id;
};

struct FStatusBarData
{
	TWeakPtr<SStatusBar> StatusBarWidget;
	TSharedPtr<SWidget> ConsoleEditBox;
};

UCLASS()
class STATUSBAR_API UStatusBarSubsystem : public UEditorSubsystem, public IProgressNotificationHandler
{
	GENERATED_BODY()

public:

	/**
	 *	Prepares for use
	 */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override final;

	/**
	 *	Internal cleanup
	 */
	virtual void Deinitialize() override final;

	/**
	 * Focuses the debug console or opens the output log drawer on the status bar for status bar residing in the passed in parent window 
	 *
	 * @param ParentWindow			The parent window of the status bar 
	 * @param bAlwaysToggleDrawer	If true, the output log drawer will be toggled without focusing the debug console first
	 * @return true if a status bar debug console was found and focused 
	 */
	bool ToggleDebugConsole(TSharedRef<SWindow> ParentWindow, bool bAlwaysToggleDrawer=false);

	/**
	 * Opens the content browser drawer for a status bar residing in the active window 
	 * 
	 * @return true if the content browser was opened or false if no status bar in the active window was found
	 */
	bool OpenContentBrowserDrawer();

	/**
	 * Opens the output log drawer for a status bar residing in the active window
	 *
	 * @return true if the output log was opened or false if no status bar in the active window was found
	 */
	bool OpenOutputLogDrawer();

	/**
	 * Tries to toggle the given drawer
	 */
	bool TryToggleDrawer(const FName DrawerId);

	/**
	 * Forces all drawers to dismiss. Usually it dismisses with focus. Only call this if there is some reason an open drawer would be invalid for the current state of the editor.
	 */
	bool ForceDismissDrawer();

	/** 
	 * Creates a new instance of a status bar widget
	 *
	 * @param StatusBarName	The name of the status bar for updating it later. This name must be unique. You can use the instance number of an fname to generate a unique fname for comparison but a non-unique one for serialization of status bar data. I.E all asset editors of a specific type will be saved the same but at runtime will be indentified uniquely. This is usally what you want.
	 * @param ParentTab	Parent tab of the status bar.
	 */
	TSharedRef<SWidget> MakeStatusBarWidget(FName UniqueStatusBarName, const TSharedRef<SDockTab>& InParentTab);

	/**
	 * @return true if a status bar was found for the active window
	 */
	bool ActiveWindowHasStatusBar() const;

	/**
	 * @return true if a status bar was found for the active window immediately behind a Notification Window
	*/
	bool ActiveWindowBehindNotificationHasStatusBar();

	/**
	 * Creates a new instance of a status bar widget
	 *
	 * @param StatusBarName	The name of the status bar to add the drawer to
	 * @param Drawer		The drawer to add to the status bar
	 * @param SlotIndex		The position at which to add the new drawer
	 */
	void RegisterDrawer(FName StatusBarName, FWidgetDrawerConfig&& Drawer, int32 SlotIndex = INDEX_NONE);

	/**
	 * Unregisters and destroys the drawer with the given DrawerId 
	 *
	 * @param StatusBarName	The name of the status bar to unregister the drawer from
	 * @param DrawerId		the unique name id of the drawer to unregister
	 */
	void UnregisterDrawer(FName StatusBarName, FName DrawerId);

	/** 
	 * Pushes a new status bar message
	 *
	 * @param StatusBarName	The name of the status bar to push messages to
	 * @param InMessage		The message to display
	 * @param InHintText	Optional hint text message.  This message will be highlighted to make it stand out
	 * @return	A handle to the message for clearing it later
	 */
	FStatusBarMessageHandle PushStatusBarMessage(FName StatusBarName, const TAttribute<FText>& InMessage, const TAttribute<FText>& InHintText);
	FStatusBarMessageHandle PushStatusBarMessage(FName StatusBarName, const TAttribute<FText>& InMessage);

	/**
	 * Removes a message from the status bar.  When messages are removed the previous message on the stack (if any) is displayed
	 *
	 * @param StatusBarName	The name of the status bar to remove from
	 * @param InHandle		Handle to the status bar message to remove
	 */
	void PopStatusBarMessage(FName StatusBarName, FStatusBarMessageHandle InHandle);

	/**
	 * Removes all messages from the status bar
	 *
	 * @param StatusBarName	The name of the status bar to remove from
	 */
	void ClearStatusBarMessages(FName StatusBarName);

	/**
	 * Registers a new global status bar extension.
	 *
	 * @param Extension The extension to register
	 * @return A reference to the extension interface, which can be later used to unregister the extension
	 */
	IGlobalStatusBarExtension& RegisterGlobalStatusBarExtension(TUniquePtr<IGlobalStatusBarExtension>&& Extension);

	/**
	 * Unregisters an existing status bar extension.
	 *
	 * @param Extension Reference returned by RegisterGlobalStatusBarExtension containing the extension to unregister
	 * @return An owned instance of the extension that was registered, or null if the extension was not found
	 */
	TUniquePtr<IGlobalStatusBarExtension> UnregisterGlobalStatusBarExtension(IGlobalStatusBarExtension* Extension);


private:
	/** IProgressNotificationHandler interface */
	virtual void StartProgressNotification(FProgressNotificationHandle Handle, FText DisplayText, int32 TotalWorkToDo) override;
	virtual void UpdateProgressNotification(FProgressNotificationHandle Handle, int32 TotalWorkDone, int32 UpdatedTotalWorkToDo, FText UpdatedDisplayText) override;
	virtual void CancelProgressNotification(FProgressNotificationHandle Handle) override;

	bool ToggleContentBrowser(TSharedRef<SWindow> ParentWindow);
	void OnDebugConsoleClosed(TWeakPtr<SStatusBar> OwningStatusBar);
	void CreateContentBrowserIfNeeded();
	void CreateAndShowNewUserTipIfNeeded(TSharedPtr<SWindow> ParentWindow, bool bIsRunningStartupDialog);
	const FString GetNewUserTipState() const;
	void CreateAndShowOneTimeIndustryQueryIfNeeded(TSharedPtr<SWindow> ParentWindow, bool bIsRunningStartupDialog);
	static const FString GetOneTimeStateWithFallback(const FString StoreId, const FString SectionName, const FString KeyName, const FString FallbackIniLocation, const FString FallbackIniKey);
	static void SetOneTimeStateWithFallback(const FString StoreId, const FString SectionName, const FString KeyName, const FString FallbackIniLocation, const FString FallbackIniKey);

	TSharedPtr<SStatusBar> GetStatusBar(FName StatusBarName) const;
	TSharedRef<SWidget> OnGetContentBrowser();
	void OnContentBrowserOpened(FName StatusBarWithDrawerName);
	void OnContentBrowserDismissed(const TSharedPtr<SWidget>& NewlyFocusedWidget);
	void HandleDeferredOpenContentBrowser(TSharedPtr<SWindow> ParentWindow);

	TSharedRef<SWidget> OnGetOutputLog();
	void OnOutputLogOpened(FName StatusBarWithDrawerName);
	void OnOutputLogDismised(const TSharedPtr<SWidget>& NewlyFocusedWidget);

	void OnDebugConsoleDrawerClosed();
private:
	TMap<FName, FStatusBarData> StatusBars;
	TWeakPtr<SWidget> PreviousKeyboardFocusedWidget;
	/** The floating content browser that is opened via the content browser button in the status bar */
	TSharedPtr<SWidget> StatusBarContentBrowser;
	TSharedPtr<SWidget> StatusBarOutputLog;
	static int32 MessageHandleCounter;
	TArray<TUniquePtr<IGlobalStatusBarExtension>> GlobalStatusBarExtensions;
};
