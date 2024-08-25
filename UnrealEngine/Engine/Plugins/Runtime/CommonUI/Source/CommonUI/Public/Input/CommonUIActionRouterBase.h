// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonInputModeTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Containers/Ticker.h"
#include "Containers/CircularBuffer.h"

#include "Engine/EngineBaseTypes.h"
#include "Input/UIActionBindingHandle.h"
#include "InputCoreTypes.h"
#include "UObject/WeakObjectPtr.h"
#include "CommonUIActionRouterBase.generated.h"

class SWidget;
struct FBindUIActionArgs;

class AHUD;
class FWeakWidgetPath;
class FWidgetPath;
class UCanvas;
class UWidget;
class UCommonUserWidget;
class UCommonActivatableWidget;
class UCommonInputSubsystem;
class UInputComponent;
class UPlayerInput;
class FCommonAnalogCursor;
class IInputProcessor;
class UCommonInputActionDomainTable;

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
struct FFocusEvent;

enum class ERouteUIInputResult : uint8
{
	Handled,
	BlockGameInput,
	Unhandled
};

/**
 * The nucleus of the CommonUI input routing system. 
 * 
 * Gathers input from external sources such as game viewport client and forwards them to widgets 
 * via activatable tree node representation.
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

	DECLARE_EVENT_OneParam(UCommonUIActionRouterBase, FOnActivationMetadataChanged, FActivationMetadata);
	FOnActivationMetadataChanged& OnActivationMetadataChanged() const { return OnActivationMetadataChangedEvent; }

	void RegisterScrollRecipient(const UWidget& ScrollableWidget);
	void UnregisterScrollRecipient(const UWidget& ScrollableWidget);
	TArray<const UWidget*> GatherActiveAnalogScrollRecipients() const;

	TArray<FUIActionBindingHandle> GatherActiveBindings() const;
	FSimpleMulticastDelegate& OnBoundActionsUpdated() const { return OnBoundActionsUpdatedEvent; }

	UCommonInputSubsystem& GetInputSubsystem() const;

	virtual ERouteUIInputResult ProcessInput(FKey Key, EInputEvent InputEvent) const;
	bool CanProcessNormalGameInput() const;

	bool IsPendingTreeChange() const;

	TSharedPtr<FCommonAnalogCursor> GetCommonAnalogCursor() const { return AnalogCursor; }

	void FlushInput();

	bool IsWidgetInActiveRoot(const UCommonActivatableWidget* Widget) const;

	/** 
	 * Sets Input Config 
	 * 
	 * @param NewConfig config to set
	 * @param InConfigSource optional source of config. If exists, will be used to log input config source
	 */
	void SetActiveUIInputConfig(const FUIInputConfig& NewConfig, const UObject* InConfigSource = nullptr);

public:
	void NotifyUserWidgetConstructed(const UCommonUserWidget& Widget);
	void NotifyUserWidgetDestructed(const UCommonUserWidget& Widget);
	
	void AddBinding(FUIActionBindingHandle Binding);
	void RemoveBinding(FUIActionBindingHandle Binding);

	int32 GetLocalPlayerIndex() const;

	void RefreshActiveRootFocusRestorationTarget() const;
	void RefreshActiveRootFocus();
	void RefreshUIInputConfig();

	bool ShouldAlwaysShowCursor() const;

protected:
	virtual TSharedRef<FCommonAnalogCursor> MakeAnalogCursor() const;
	virtual void PostAnalogCursorCreate();
	void RegisterAnalogCursorTick();

	TWeakPtr<FActivatableTreeRoot> GetActiveRoot() const;
	virtual void SetActiveRoot(FActivatableTreeRootPtr NewActiveRoot);
	void SetForceResetActiveRoot(bool bInForceResetActiveRoot);

	virtual void ApplyUIInputConfig(const FUIInputConfig& NewConfig, bool bForceRefresh);
	void UpdateLeafNodeAndConfig(FActivatableTreeRootPtr DesiredRoot, FActivatableTreeNodePtr DesiredLeafNode);
	void FlushPressedKeys() const;

	void RefreshActionDomainLeafNodeConfig();

	bool bIsActivatableTreeEnabled = true;

	/** The currently applied UI input configuration */
	TOptional<FUIInputConfig> ActiveInputConfig;

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
	void SetActiveActivationMetadata(const FActivationMetadata& NewConfig);
	
	void HandleActivatableWidgetRebuilding(UCommonActivatableWidget& RebuildingWidget);
	void ProcessRebuiltWidgets();
	void AssembleTreeRecursive(const FActivatableTreeNodeRef& CurNode, TMap<UCommonActivatableWidget*, TArray<UCommonActivatableWidget*>>& WidgetsByDirectParent);

	void HandleRootWidgetSlateReleased(TWeakPtr<FActivatableTreeRoot> WeakRoot);
	void HandleRootNodeActivated(TWeakPtr<FActivatableTreeRoot> WeakActivatedRoot);
	void HandleRootNodeDeactivated(TWeakPtr<FActivatableTreeRoot> WeakDeactivatedRoot);
	void HandleLeafmostActiveNodeChanged();

	void HandleSlateFocusChanging(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldFocusedWidgetPath, const TSharedPtr<SWidget>& OldFocusedWidget, const FWidgetPath& NewFocusedWidgetPath, const TSharedPtr<SWidget>& NewFocusedWidget);

	void HandlePostGarbageCollect();

	const UCommonInputActionDomainTable* GetActionDomainTable() const;
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

	TCircularBuffer<FString> InputConfigSources = TCircularBuffer<FString>(5, "None");
	int32 InputConfigSourceIndex = 0;

	bool bForceResetActiveRoot = false;

	mutable FSimpleMulticastDelegate OnBoundActionsUpdatedEvent;
	mutable FOnActiveInputModeChanged OnActiveInputModeChangedEvent;
	mutable FOnActivationMetadataChanged OnActivationMetadataChangedEvent;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CommonInputBaseTypes.h"
#include "CommonUIInputTypes.h"
#include "Misc/Passkey.h"
#endif
