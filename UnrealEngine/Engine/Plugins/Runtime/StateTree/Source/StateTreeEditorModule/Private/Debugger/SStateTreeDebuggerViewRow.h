// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_STATETREE_DEBUGGER

#include "Debugger/StateTreeTraceTypes.h"
#include "Templates/SharedPointer.h"
#include "TraceServices/Model/Frames.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

class FStateTreeViewModel;

/** An item in the StateTreeDebugger trace event tree */
struct FStateTreeDebuggerEventTreeElement : TSharedFromThis<FStateTreeDebuggerEventTreeElement>
{
	explicit FStateTreeDebuggerEventTreeElement(const TraceServices::FFrame& Frame, const FStateTreeTraceEventVariantType& Event, const UStateTree* StateTree)
		: Frame(Frame), Event(Event), WeakStateTree(StateTree)
	{
	}

	TraceServices::FFrame Frame;
	FStateTreeTraceEventVariantType Event;
	TArray<TSharedPtr<FStateTreeDebuggerEventTreeElement>> Children;
	FString Description;
	TWeakObjectPtr<const UStateTree> WeakStateTree;
};


/**
 * Widget for row inside the StateTreeDebugger TreeView.
 */
class SStateTreeDebuggerViewRow : public STableRow<TSharedPtr<FStateTreeDebuggerEventTreeElement>>
{
public:
	void Construct(const FArguments& InArgs,
				   const TSharedPtr<STableViewBase>& InOwnerTableView,
				   const TSharedPtr<FStateTreeDebuggerEventTreeElement>& InElement);

private:
	TSharedPtr<SWidget> GenerateEventWidget() const;
	const FTextBlockStyle& GetEventTextStyle() const;
	FText GetEventDescription() const;
	FText GetEventTooltip() const;

	TSharedPtr<FStateTreeDebuggerEventTreeElement> Item;
};

#endif // WITH_STATETREE_DEBUGGER