// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Input/UIActionBindingHandle.h"
#include "Input/UIActionBinding.h"
#include "Input/CommonUIInputSettings.h"

// Note: Everything in here should be considered completely private to each other and CommonUIActionRouter.
//		They were all originally defined directly in CommonUIActionRouter.cpp, but it was annoying having to scroll around so much.

class SWidget;
class UWidget;
class UCommonUIActionRouterBase;
class UCommonActivatableWidget;

class IInputProcessor;
struct FBindUIActionArgs;
enum class ECommonInputType : uint8;

class FActivatableTreeNode;
struct FCommonInputActionDataBase;
class UCommonInputSubsystem;
using FActivatableTreeNodePtr = TSharedPtr<FActivatableTreeNode>;
using FActivatableTreeNodeRef = TSharedRef<FActivatableTreeNode>;

class FActivatableTreeRoot;
using FActivatableTreeRootPtr = TSharedPtr<FActivatableTreeRoot>;
using FActivatableTreeRootRef = TSharedRef<FActivatableTreeRoot>;

class FActionRouterBindingCollection;

DECLARE_LOG_CATEGORY_EXTERN(LogUIActionRouter, Log, All);

//////////////////////////////////////////////////////////////////////////
// FActionRouterBindingCollection
//////////////////////////////////////////////////////////////////////////

class COMMONUI_API FActionRouterBindingCollection : public TSharedFromThis<FActionRouterBindingCollection>
{
public:
	virtual ~FActionRouterBindingCollection() {}

	virtual EProcessHoldActionResult ProcessHoldInput(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent) const;
	virtual bool ProcessNormalInput(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent) const;
	virtual bool IsReceivingInput() const { return true; }

	void AddBinding(FUIActionBinding& Binding);
	
	void RemoveBindings(const TArray<FUIActionBindingHandle>& WidgetBindings);
	void RemoveBinding(FUIActionBindingHandle ActionHandle);

	bool HasHoldBindings() const { return HoldBindingsCount > 0; }

	const TArray<FUIActionBindingHandle>& GetActionBindings() const { return ActionBindings; }
	
protected:
	FActionRouterBindingCollection(UCommonUIActionRouterBase& OwningRouter);
	virtual bool IsWidgetReachableForInput(const UWidget* Widget) const;

	int32 GetOwnerUserIndex() const;
	int32 GetOwnerControllerId() const;
	UCommonUIActionRouterBase& GetActionRouter() const { check(ActionRouterPtr.IsValid()); return *ActionRouterPtr; }
	
	void DebugDumpActionBindings(FString& OutputStr, int32 IndentSpaces) const;

	/** The set of action bindings contained within this collection */
	TArray<FUIActionBindingHandle> ActionBindings;
	
	/**
	 * Treat this as guaranteed to be valid and access via GetActionRouter()
	 * Only kept as a WeakObjectPtr so we can reliably assert in the case it somehow becomes invalid.
	 */
	TWeakObjectPtr<UCommonUIActionRouterBase> ActionRouterPtr;

private:
	//Slate application sends repeat actions only for the last pressed key, so we have to keep track of this last held binding and clear it when we get a new key to hold
	mutable FUIActionBindingHandle CurrentlyHeldBinding;

	int32 HoldBindingsCount = 0;
};

//////////////////////////////////////////////////////////////////////////
// FActivatableTreeNode
//////////////////////////////////////////////////////////////////////////

class COMMONUI_API FActivatableTreeNode : public FActionRouterBindingCollection
{
public:
	virtual ~FActivatableTreeNode();
	
	virtual EProcessHoldActionResult ProcessHoldInput(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent) const override;
	virtual bool ProcessActionDomainHoldInput(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent, EProcessHoldActionResult& OutHoldActionResult) const;
	virtual bool ProcessNormalInput(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent) const override;
	virtual bool ProcessActionDomainNormalInput(ECommonInputMode ActiveInputMode, FKey Key, EInputEvent InputEvent) const;
	virtual bool IsReceivingInput() const override { return bCanReceiveInput && IsWidgetActivated(); }

	bool IsWidgetValid() const;
	bool IsWidgetActivated() const;
	bool DoesWidgetSupportActivationFocus() const;
	void AppendAllActiveActions(TArray<FUIActionBindingHandle>& BoundActions) const;

	UCommonActivatableWidget* GetWidget() { return RepresentedWidget.Get(); }
	const UCommonActivatableWidget* GetWidget() const { return RepresentedWidget.Get(); }

	TArray<FActivatableTreeNodeRef>& GetChildren() { return Children; }
	const TArray<FActivatableTreeNodeRef>& GetChildren() const { return Children; }
	
	FActivatableTreeNodePtr GetParentNode() const { return Parent.Pin(); }
	
	FActivatableTreeNodeRef AddChildNode(UCommonActivatableWidget& InActivatableWidget);
	void CacheFocusRestorationTarget();
	TSharedPtr<SWidget> GetFocusFallbackTarget() const;

	bool IsExclusiveParentOfWidget(const TSharedPtr<SWidget>& SlateWidget) const;

	int32 GetLastPaintLayer() const;
	TOptional<FUIInputConfig> FindDesiredInputConfig() const;
	TOptional<FUIInputConfig> FindDesiredActionDomainInputConfig() const;
	FUICameraConfig FindDesiredCameraConfig() const;
	
	void SetCanReceiveInput(bool bInCanReceiveInput);
	
	void AddScrollRecipient(const UWidget& ScrollRecipient);
	void RemoveScrollRecipient(const UWidget& ScrollRecipient);
	void AddInputPreprocessor(const TSharedRef<IInputProcessor>& InputPreprocessor, int32 DesiredIndex);

	FSimpleDelegate OnActivated;
	FSimpleDelegate OnDeactivated;

protected:
	FActivatableTreeNode(UCommonUIActionRouterBase& OwningRouter, UCommonActivatableWidget& ActivatableWidget);
	FActivatableTreeNode(UCommonUIActionRouterBase& OwningRouter, UCommonActivatableWidget& ActivatableWidget, const FActivatableTreeNodeRef& InParent);

	virtual bool IsWidgetReachableForInput(const UWidget* Widget) const override;
	
	bool CanReceiveInput() const { return bCanReceiveInput; }
	virtual void Init();	
	FActivatableTreeRootRef GetRoot() const;

	void AppendValidScrollRecipients(TArray<const UWidget*>& AllScrollRecipients) const;
	void DebugDumpRecursive(FString& OutputStr, int32 Depth, bool bIncludeActions, bool bIncludeChildren, bool bIncludeInactive) const;
	
	bool IsParentOfWidget(const TSharedPtr<SWidget>& SlateWidget) const;
	
private:
	void HandleWidgetActivated();
	void HandleWidgetDeactivated();
	void HandleChildSlateReleased(UCommonActivatableWidget* ChildWidget);
	
	void RegisterPreprocessors();
	void UnregisterPreprocessors();

	bool DoesPathSupportActivationFocus() const;
	
#if !UE_BUILD_SHIPPING
	FString DebugWidgetName;
#endif

	TWeakObjectPtr<UCommonActivatableWidget> RepresentedWidget;
	TWeakPtr<FActivatableTreeNode> Parent;
	TArray<FActivatableTreeNodeRef> Children;
	TWeakPtr<SWidget> FocusRestorationTarget;

	bool bCanReceiveInput = false;

	struct FPreprocessorRegistration
	{
		int32 DesiredIndex = INDEX_NONE;
		TSharedRef<IInputProcessor> Preprocessor;
		FPreprocessorRegistration(int32 InDesiredIndex, const TSharedRef<IInputProcessor>& InPreprocessor)
			: DesiredIndex(InDesiredIndex), Preprocessor(InPreprocessor) 
		{}
	};
	TArray<FPreprocessorRegistration> RegisteredPreprocessors;

	// Mutable so we can keep it clean during normal use
	mutable TArray<TWeakObjectPtr<const UWidget>> ScrollRecipients;
};

//////////////////////////////////////////////////////////////////////////
// FActivatableTreeRoot
//////////////////////////////////////////////////////////////////////////

class COMMONUI_API FActivatableTreeRoot : public FActivatableTreeNode
{
public:
	static FActivatableTreeRootRef Create(UCommonUIActionRouterBase& OwningRouter, UCommonActivatableWidget& ActivatableWidget);
	
	void UpdateLeafNode();

	TArray<const UWidget*> GatherScrollRecipients() const;

	bool UpdateLeafmostActiveNode(FActivatableTreeNodePtr BaseCandidateNode, bool bInApplyConfig = true);

	void DebugDump(FString& OutputStr, bool bIncludeActions, bool bIncludeChildren, bool bIncludeInactive) const;

	FSimpleDelegate OnLeafmostActiveNodeChanged;

	void FocusLeafmostNode();

	void RefreshCachedRestorationTarget();

	void ApplyLeafmostNodeConfig();

protected:
	virtual void Init() override;

private:
	FActivatableTreeRoot(UCommonUIActionRouterBase& OwningRouter, UCommonActivatableWidget& ActivatableWidget)
		: FActivatableTreeNode(OwningRouter, ActivatableWidget)
	{}

	void HandleInputMethodChanged(ECommonInputType InputMethod);

	void HandleRequestRefreshLeafmostFocus();

	// WeakPtr because the root itself can be the primary active node - results in a circular ref leak using a full SharedPtr here
	TWeakPtr<FActivatableTreeNode> LeafmostActiveNode;
};