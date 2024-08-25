// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "AvaTransitionTreeDebugInstance.h"
#include "Delegates/Delegate.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"

class FAvaTransitionEditorViewModel;
class IAvaTransitionDebuggableExtension;
class UStateTree;
struct FStateTreeDebugger;
struct FStateTreeInstanceDebugId;

class FAvaTransitionDebugger : public TSharedFromThis<FAvaTransitionDebugger>
{
public:
	FAvaTransitionDebugger();

	~FAvaTransitionDebugger();

	void Initialize(const TSharedRef<FAvaTransitionEditorViewModel>& InEditorViewModel);

	void Start();

	void Stop();

	bool IsActive() const;

	void OnTreeInstanceStarted(UStateTree& InStateTree, const FStateTreeInstanceDebugId& InInstanceDebugId, const FString& InInstanceName);

	void OnTreeInstanceStopped(const FStateTreeInstanceDebugId& InInstanceDebugId);

	void OnNodeEntered(const FGuid& InNodeId, const FStateTreeInstanceDebugId& InInstanceDebugId);

	void OnNodeExited(const FGuid& InNodeId, const FStateTreeInstanceDebugId& InInstanceDebugId);

private:
	TSharedPtr<IAvaTransitionDebuggableExtension> FindDebuggable(const FGuid& InNodeId) const;

	TWeakPtr<FAvaTransitionEditorViewModel> EditorViewModelWeak;

	TSharedPtr<FStateTreeDebugger> Debugger;

	TArray<FAvaTransitionTreeDebugInstance> TreeDebugInstances;
};

#endif // WITH_STATETREE_DEBUGGER
