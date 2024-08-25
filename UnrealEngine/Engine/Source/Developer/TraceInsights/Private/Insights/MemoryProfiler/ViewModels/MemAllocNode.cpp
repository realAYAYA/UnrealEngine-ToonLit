// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemAllocNode.h"

// Insights
#include "Insights/MemoryProfiler/ViewModels/CallstackFormatting.h"

#define LOCTEXT_NAMESPACE "Insights::FMemAllocNode"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(FMemAllocNode)

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FMemAllocNode::GetAllocCallstackId() const
{
	return IsValidMemAlloc() ? GetMemAllocChecked().GetAllocCallstackId() : 0u;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

uint32 FMemAllocNode::GetFreeCallstackId() const
{
	return IsValidMemAlloc() ? GetMemAllocChecked().GetFreeCallstackId() : 0u;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetFullCallstack(ECallstackType InCallstackType) const
{
	return GetFullCallstackOrSourceFiles(InCallstackType, (uint8)EStackFrameFormatFlags::ModuleSymbolFileAndLine);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetFullCallstackSourceFiles(ECallstackType InCallstackType) const
{
	return GetFullCallstackOrSourceFiles(InCallstackType, (uint8)EStackFrameFormatFlags::File);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopFunction(ECallstackType InCallstackType) const
{
	return GetTopFunctionOrSourceFile(InCallstackType, (uint8)EStackFrameFormatFlags::ModuleAndSymbol);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopFunctionEx(ECallstackType InCallstackType) const
{
	return GetTopFunctionOrSourceFile(InCallstackType, (uint8)EStackFrameFormatFlags::ModuleSymbolFileAndLine);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopSourceFile(ECallstackType InCallstackType) const
{
	return GetTopFunctionOrSourceFile(InCallstackType, (uint8)EStackFrameFormatFlags::File);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopSourceFileEx(ECallstackType InCallstackType) const
{
	return GetTopFunctionOrSourceFile(InCallstackType, (uint8)EStackFrameFormatFlags::FileAndLine);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetFullCallstackOrSourceFiles(ECallstackType InCallstackType, uint8 Flags) const
{
	if (!IsValidMemAlloc())
	{
		return FText();
	}

	const Insights::FMemoryAlloc& Alloc = GetMemAllocChecked();
	const TraceServices::FCallstack* Callstack = 
		InCallstackType == ECallstackType::AllocCallstack
			? Alloc.GetAllocCallstack() 
			: Alloc.GetFreeCallstack();

	if (!Callstack)
	{
		return FText::FromString(GetCallstackNotAvailableString());
	}

	if (Callstack->Num() == 0)
	{
		return FText::FromString(GetEmptyCallstackString());
	}

	TStringBuilder<1024> Tooltip;
	const uint32 NumCallstackFrames = Callstack->Num();
	check(NumCallstackFrames <= 256); // see Callstack->Frame(uint8)
	for (uint32 FrameIndex = 0; FrameIndex < NumCallstackFrames; ++FrameIndex)
	{
		if (FrameIndex != 0)
		{
			Tooltip << TEXT("\n");
		}
		const TraceServices::FStackFrame* Frame = Callstack->Frame(static_cast<uint8>(FrameIndex));
		check(Frame != nullptr);
		FormatStackFrame(*Frame, Tooltip, (EStackFrameFormatFlags)Flags);
	}
	return FText::FromString(FString(Tooltip));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopFunctionOrSourceFile(ECallstackType InCallstackType, uint8 Flags) const
{
	if (!IsValidMemAlloc())
	{
		return FText();
	}

	const Insights::FMemoryAlloc& Alloc = GetMemAllocChecked();
	const TraceServices::FCallstack* Callstack = 
		InCallstackType == ECallstackType::AllocCallstack
			? Alloc.GetAllocCallstack() 
			: Alloc.GetFreeCallstack();

	if (!Callstack)
	{
		return FText::FromString(GetCallstackNotAvailableString());
	}

	const uint32 NumCallstackFrames = Callstack->Num();
	if (NumCallstackFrames == 0)
	{
		return FText::FromString(GetEmptyCallstackString());
	}
	check(NumCallstackFrames <= 256); // see Callstack->Frame(uint8)

	const TraceServices::FStackFrame* Frame = nullptr;
	for (uint32 FrameIndex = 0; FrameIndex < NumCallstackFrames; ++FrameIndex)
	{
		Frame = Callstack->Frame(static_cast<uint8>(FrameIndex));
		check(Frame != nullptr);

		if (Frame->Symbol &&
			Frame->Symbol->Name &&
			Frame->Symbol->FilterStatus != TraceServices::EResolvedSymbolFilterStatus::Filtered)
		{
			break;
		}
	}

	TStringBuilder<1024> Str;
	FormatStackFrame(*Frame, Str, (EStackFrameFormatFlags)Flags);
	return FText::FromString(FString(Str));
}

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights

#undef LOCTEXT_NAMESPACE
