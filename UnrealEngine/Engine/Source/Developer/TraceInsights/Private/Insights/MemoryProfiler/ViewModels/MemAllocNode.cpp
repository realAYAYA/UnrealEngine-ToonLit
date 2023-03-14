// Copyright Epic Games, Inc. All Rights Reserved.

#include "MemAllocNode.h"

// Insights
#include "Insights/MemoryProfiler/ViewModels/CallstackFormatting.h"

#define LOCTEXT_NAMESPACE "Insights::FMemAllocNode"

namespace Insights
{

INSIGHTS_IMPLEMENT_RTTI(FMemAllocNode)

////////////////////////////////////////////////////////////////////////////////////////////////////

uint64 FMemAllocNode::GetCallstackId() const
{
	return IsValidMemAlloc() ? uint64(GetMemAllocChecked().GetCallstack()) : 0ull;
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetFullCallstack() const
{
	return IsValidMemAlloc() ? GetMemAllocChecked().GetFullCallstack() : FText();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetFullCallstackSourceFiles() const
{
	return IsValidMemAlloc() ? GetMemAllocChecked().GetFullCallstackSourceFiles() : FText();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopFunction() const
{
	return GetTopFunctionOrSourceFile((uint8)EStackFrameFormatFlags::ModuleAndSymbol);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopFunctionEx() const
{
	return GetTopFunctionOrSourceFile((uint8)EStackFrameFormatFlags::ModuleSymbolFileAndLine);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopSourceFile() const
{
	return GetTopFunctionOrSourceFile((uint8)EStackFrameFormatFlags::File);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopSourceFileEx() const
{
	return GetTopFunctionOrSourceFile((uint8)EStackFrameFormatFlags::FileAndLine);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

FText FMemAllocNode::GetTopFunctionOrSourceFile(uint8 Flags) const
{
	if (!IsValidMemAlloc())
	{
		return FText();
	}

	const Insights::FMemoryAlloc& Alloc = GetMemAllocChecked();
	const TraceServices::FCallstack* Callstack = Alloc.GetCallstack();

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
