// Copyright Epic Games, Inc. All Rights Reserved.

#include "SMassQuery.h"
#include "SMassBitSet.h"
#include "MassEntityTypes.h"
#include "MassDebuggerModel.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "SMassDebugger"


static_assert(EMassAccessOperation::Read == uint32(EMassBitSetDisplayMode::ReadOnly) && EMassAccessOperation::Write == uint32(EMassBitSetDisplayMode::ReadWrite)
	, "EMassAccessOperation and EMassBitSetDisplayMode need to be kept in sync");

namespace UE::Mass::Debugger::UI::Private
{
	template<typename TBitSet>
	void AddBitSetPair(TSharedRef<SVerticalBox>& Box, const TMassExecutionAccess<TBitSet>& BitSetAccess, const FString& Label)
	{
		if (BitSetAccess.IsEmpty() == false)
		{
			static const FSlateBrush* RequiredAccessBrushes[] = {
				FMassDebuggerStyle::GetBrush("MassDebug.Fragment.ReadOnly")
				, FMassDebuggerStyle::GetBrush("MassDebug.Fragment.ReadWrite")
			};

			Box->AddSlot()
			.AutoHeight()
			[
				SNew(SMassBitSet<TBitSet>, Label, BitSetAccess.AsArrayView(), RequiredAccessBrushes)
				.SlotPadding(5)
			];
		}
	}
} // namespace UE::Mass::Debugger::UI::Private


void SMassQuery::Construct(const FArguments& InArgs, TSharedPtr<FMassDebuggerQueryData> InQueryData)
{
	using UE::Mass::Debugger::UI::Private::AddBitSetPair;
	using UE::Mass::Debugger::UI::AddBitSet;

	const FSlateColor ReadOnlyColor = FLinearColor(0.4f, 0.4f, 0.4f, 0.9f);
	const FSlateColor ReadWriteColor = FLinearColor(0.1f, 0.8f, 0.1f, 0.9f);

	QueryData = InQueryData;

	FMassExecutionRequirements& ExecutionRequirements = InQueryData->ExecutionRequirements;

	TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
	static const FSlateBrush* ReadOnlyBrush = FMassDebuggerStyle::GetBrush("MassDebug.Fragment.ReadOnly");

	Box->AddSlot()
	.AutoHeight()
	[
		SNew(SRichTextBlock)
		.Text(InQueryData->Label)
		.DecoratorStyleSet(&FAppStyle::Get())
		.TextStyle(FAppStyle::Get(), "LargeText")
	];

	AddBitSetPair(Box, ExecutionRequirements.Fragments, TEXT("Fragments"));
	AddBitSet(Box, ExecutionRequirements.RequiredAllTags, TEXT("RequiredAllTags"), ReadOnlyBrush);
	AddBitSet(Box, ExecutionRequirements.RequiredAnyTags, TEXT("RequiredAnyTags"), ReadOnlyBrush);
	AddBitSet(Box, ExecutionRequirements.RequiredNoneTags,TEXT("RequiredNoneTags"), ReadOnlyBrush);
	AddBitSetPair(Box, ExecutionRequirements.ChunkFragments, TEXT("Chunk Fragments"));
	AddBitSetPair(Box, ExecutionRequirements.SharedFragments, TEXT("Shared Fragments"));
	AddBitSetPair(Box, ExecutionRequirements.RequiredSubsystems, TEXT("Required Subsystems"));

	ChildSlot
	[
		SNew(SBorder)
		.Padding(10.0f)
		[
			Box
		]
	];
}

#undef LOCTEXT_NAMESPACE

