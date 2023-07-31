// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "CommonUIInputTypes.h"
#include "CommonInputBaseTypes.h"
#include "Misc/Passkey.h"
#include "Containers/Ticker.h"

#include "CommonUIActionRouterBase.generated.h"

class AHUD;
class UCanvas;
class UWidget;
class UCommonUserWidget;
class UCommonActivatableWidget;
class UCommonInputSubsystem;
class UInputComponent;
class UPlayerInput;
class FCommonAnalogCursor;
class IInputProcessor;

enum class EProcessHoldActionResult;
class FActivatableTreeNode;
class UCommonInputSubsystem;
class UCommonInputActionDomain;
struct FUIActionBinding;
class FDebugDisplayInfo;
struct FAutoCompleteCommand;
using FActivatableTreeNodePtr = TSharedPtr<FActivatableTreeNode>;
using FActivatableTreeNodeRef = TSharedRef<FActivatableTreeNode>;

class FActivatableTreeRoot;
using FActivatableTreeRootPtr = TSharedPtr<FActivatableTreeRoot>;
using FActivatableTreeRootRef = TSharedRef<FActivatableTreeRoot>;

enum class ERouteUIInputResult : uint8
{
	Handled,
	BlockGameInput,
	Unhandled
};

/**
 * The nucleus of the CommonUI input routing system
 * @todo DanH: Explain what that means more fully
 */
UCLASS()
class COMMONUI_API UCommonUIActionRouterBase : public ULocalPlayerSubsystem
{
	GENERATED_BODY()

public:
	static UCommonUIActionRouterBase* Get(const UWidget& ContextWidget);

	/** searches up the SWidget tree until it finds the nearest UCommonActivatableWidget */
	static UCommonActivatableWidget* FindOwningActivatable(TSharedPtr<SWidget> Widget, ULocalPlayer* OwningLocalPlayer);

	UCommonUIActionRouterBase();
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	/** Sets whether the underlying activatable tree system is enabled - when disabled, all we really do is process Persistent input actions */
	virtual void SetIsActivatableTreeEnabled(bool bInIsTreeEnabled);

	virtual FUIActionBindingHandle RegisterUIActionBinding(const UWidget& Widget, const FBindUIActionArgs& BindActionArgs);
	bool RegisterLinkedPreprocessor(const UWidget& Widget, const TSharedRef<IInputProcessor>& InputPreprocessor, int32 DesiredIndex = INDEX_NONE);

	DECLARE_EVENT_OneParam(UCommonUIActionRouterBase, FOnActiveInputModeChanged, ECommonInputMode);
	FOnActiveInputModeChanged& OnActiveInputModeChanged() const { return OnActiveInputModeChangedEvent; }
	ECommonInputMode GetActiveInputMode(ECommonInputMode DefaultInputMode = ECommonInputMode::All) const;
	EMouseCaptureMode GetActiveMouseCaptureMode(EMouseCaptureMode DefaultMouseCapture = EMouseCaptureMode::NoCapture) const;

	DECLARE_EVENT_OneParam(UCommonUIActionRouterBase, FOnCameraConfigChanged, FUICameraConfig);
	FOnCameraConfigChanged& OnCameraConfigChanged() const { return OnCameraConfigChangedEvent; }

	void RegisterScrollRecipient(const UWidget& ScrollableWidget);
	void UnregisterScrollRecipient(const UWidget& ScrollableWidget);
	TArray<const UWidget*> GatherActiveAnalogScrollRecipients() const;

	TArray<FUIActionBindingHandle> GatherActiveBindings() const;
	FSimpleMulticastDelegate& OnBoundActionsUpdated() const { return OnBoundActionsUpdatedEvent; }

	UCommonInputSubsystem& GetInputSubsystem() const;

	//@todo DanH: VERY TEMP! The event we want from the game viewport lives at the level atm
	ERouteUIInputResult ProcessInput(FKey Key, EInputEvent InputEvent) const;
	bool CanProcessNormalGameInput() const;

	bool IsPendingTreeChange() const;

	TSharedPtr<FCommonAnalogCursor> GetCommonAnalogCursor() const { return AnalogCursor; }

	void FlushInput();

	bool IsWidgetInActiveRoot(const UCommonActivatableWidget* Widget) const;

	void SetActiveUIInputConfig(const FUIInputConfig& NewConfig);

//COMMONUI_SCOPE:
public:
	//@todo DanH: Not loving this bit of coupling, it really should to be possible for any widget to accomplish this (and the CommonUserWidget just does it automatically)
	void NotifyUserWidgetConstructed(const UCommonUserWidget& Widget);
	void NotifyUserWidgetDestructed(const UCommonUserWidget& Widget);
	
	void AddBinding(FUIActionBindingHandle Binding);
	void RemoveBinding(FUIActionBindingHandle Binding);

	bool IsVirtualAcceptPressedBound() const;

	int32 GetLocalPlayerIndex() const;

	void RefreshActiveRootFocus();
	void RefreshUIInputConfig();

	bool ShouldAlwaysShowCursor() const;

protected:
	virtual TSharedRef<FCommonAnalogCursor> MakeAnalogCursor() const;
	virtual void PostAnalogCursorCreate();
	void RegisterAnalogCursorTick();

	virtual void SetActiveRoot(FActivatableTreeRootPtr NewActiveRoot);
	void SetForceResetActiveRoot(bool bInForceResetActiveRoot);

	void UpdateLeafNodeAndConfig(FActivatableTreeRootPtr DesiredRoot, FActivatableTreeNodePtr DesiredLeafNode);

	void RefreshActionDomainLeafNodeConfig();

	bool bIsActivatableTreeEnabled = true;

	TSharedPtr<FCommonAnalogCursor> AnalogCursor;
	FTSTicker::FDelegateHandle TickHandle;

private:
	bool Tick(float DeltaTime);

	void OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);
	void PopulateAutoCompleteEntries(TArray<FAutoCompleteCommand>& AutoCompleteList);

	void RegisterWidgetBindings(const FActivatableTreeNodePtr& TreeNode, const TArray<FUIActionBindingHandle>& BindingHandles);

	FActivatableTreeNodePtr FindNode(const UCommonActivatableWidget* Widget) const;
	FActivatableTreeNodePtr FindOwningNode(const UWidget& Widget) const;
	FActivatableTreeNodePtr FindNodeRecursive(const FActivatableTreeNodePtr& CurrentNode, const UCommonActivatableWidget& Widget) const;
	FActivatableTreeNodePtr FindNodeRecursive(const FActivatableTreeNodePtr& CurrentNode, const TSharedPtr<SWidget>& Widget) const;

	void ApplyUIInputConfig(const FUIInputConfig& NewConfig, bool bForceRefresh);
	void SetActiveUICameraConfig(const FUICameraConfig& NewConfig);
	
	void HandleActivatableWidgetRebuilding(UCommonActivatableWidget& RebuildingWidget);
	void ProcessRebuiltWidgets();
	void AssembleTreeRecursive(const FActivatableTreeNodeRef& CurNode, TMap<UCommonActivatableWidget*, TArray<UCommonActivatableWidget*>>& WidgetsByDirectParent);

	void HandleRootWidgetSlateReleased(TWeakPtr<FActivatableTreeRoot> WeakRoot);
	void HandleRootNodeActivated(TWeakPtr<FActivatableTreeRoot> WeakActivatedRoot);
	void HandleRootNodeDeactivated(TWeakPtr<FActivatableTreeRoot> WeakDeactivatedRoot);
	void HandleLeafmostActiveNodeChanged();

	void HandleSlateFocusChanging(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldFocusedWidgetPath, const TSharedPtr<SWidget>& OldFocusedWidget, const FWidgetPath& NewFocusedWidgetPath, const TSharedPtr<SWidget>& NewFocusedWidget);

	void HandlePostGarbageCollect();

	bool ProcessInputOnActionDomains(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent) const;
	EProcessHoldActionResult ProcessHoldInputOnActionDomains(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent) const;

	struct FPendingWidgetRegistration
	{
		TWeakObjectPtr<const UWidget> Widget;
		TArray<FUIActionBindingHandle> ActionBindings;
		bool bIsScrollRecipient = false;
		
		struct FPreprocessorRegistration
		{
			TSharedPtr<IInputProcessor> Preprocessor;
			int32 DesiredIdx = 0;
			bool operator==(const TSharedRef<IInputProcessor>& OtherPreprocessor) const { return Preprocessor == OtherPreprocessor; }
		};
		TArray<FPreprocessorRegistration> Preprocessors;

		bool operator==(const UWidget* OtherWidget) const { return OtherWidget == Widget.Get(); }
		bool operator==(const UWidget& OtherWidget) const { return &OtherWidget == Widget.Get(); }
	};
	FPendingWidgetRegistration& GetOrCreatePendingRegistration(const UWidget& Widget);
	TArray<FPendingWidgetRegistration> PendingWidgetRegistrations;
	
	TArray<TWeakObjectPtr<UCommonActivatableWidget>> RebuiltWidgetsPendingNodeAssignment;
	
	TArray<FActivatableTreeRootRef> RootNodes;
	FActivatableTreeRootPtr ActiveRootNode;

	// Note: Treat this as a TSharedRef - only reason it isn't is because TSharedRef doesn't play nice with forward declarations :(
	TSharedPtr<class FPersistentActionCollection> PersistentActions;

	/** The currently applied UI input configuration */
	TOptional<FUIInputConfig> ActiveInputConfig;

	bool bForceResetActiveRoot = false;

	mutable FSimpleMulticastDelegate OnBoundActionsUpdatedEvent;
	mutable FOnActiveInputModeChanged OnActiveInputModeChangedEvent;
	mutable FOnCameraConfigChanged OnCameraConfigChangedEvent;

	friend class FActionRouterBindingCollection;
	friend class FActivatableTreeNode;
	friend class FActivatableTreeRoot;
	friend class FActionRouterDebugUtils;

	mutable TArray<FKey> HeldKeys;

	/** A wrapper around TArray that keeps RootList sorted by PaintLayer during insertion */
	struct FActionDomainSortedRootList
	{
		TArray<FActivatableTreeRootRef> RootList;

		// Inserts RootNode into RootList based on its paint layer
		void Add(FActivatableTreeRootRef RootNode);

		// Trivial removal
		int32 Remove(FActivatableTreeRootRef RootNode);

		// Trivial Contains check
		bool Contains(FActivatableTreeRootRef RootNode) const;
	};

	TMap<TObjectPtr<UCommonInputActionDomain>, FActionDomainSortedRootList> ActionDomainRootNodes;
};
