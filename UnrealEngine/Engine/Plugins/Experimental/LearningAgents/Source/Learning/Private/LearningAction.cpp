// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAction.h"

#include "LearningRandom.h"

#include "NNERuntimeBasicCpuBuilder.h"

namespace UE::Learning::Action
{
	namespace Private
	{
		static inline bool ContainsDuplicates(const TArrayView<const FName> ElementNames)
		{
			TSet<FName, DefaultKeyFuncs<FName>, TInlineSetAllocator<32>> ElementNameSet;
			ElementNameSet.Append(ElementNames);
			return ElementNames.Num() != ElementNameSet.Num();
		}

		static inline bool CheckAllValid(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			for (const FSchemaElement SubElement : Elements)
			{
				if (!Schema.IsValid(SubElement)) { return false; }
			}
			return true;
		}

		static inline int32 GetMaxActionVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size = FMath::Max(Size, Schema.GetActionVectorSize(SubElement));
			}
			return Size;
		}

		static inline int32 GetTotalActionVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size += Schema.GetActionVectorSize(SubElement);
			}
			return Size;
		}

		static inline int32 GetTotalEncodedActionVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size += Schema.GetEncodedVectorSize(SubElement);
			}
			return Size;
		}

		static inline int32 GetTotalActionDistributionVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size += Schema.GetActionDistributionVectorSize(SubElement);
			}
			return Size;
		}

		static inline bool CheckAllValid(const FObject& Object, const TArrayView<const FObjectElement> Elements)
		{
			for (const FObjectElement SubElement : Elements)
			{
				if (!Object.IsValid(SubElement)) { return false; }
			}
			return true;
		}

		static inline bool CheckPriorProbabilitiesExclusive(const TArrayView<const float> PriorProbabilities, const float Epsilon = UE_KINDA_SMALL_NUMBER)
		{
			if (PriorProbabilities.Num() == 0) { return true; }

			for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
			{
				if (PriorProbabilities[Idx] < 0.0f || PriorProbabilities[Idx] > 1.0f)
				{
					return false;
				}
			}

			float Total = 0.0f;
			for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
			{
				Total += PriorProbabilities[Idx];
			}

			return FMath::Abs(Total - 1.0f) < Epsilon;
		}

		static inline bool CheckPriorProbabilitiesInclusive(const TArrayView<const float> PriorProbabilities)
		{
			if (PriorProbabilities.Num() == 0) { return true; }

			for (int32 Idx = 0; Idx < PriorProbabilities.Num(); Idx++)
			{
				if (PriorProbabilities[Idx] < 0.0f || PriorProbabilities[Idx] > 1.0f)
				{
					return false;
				}
			}

			return true;
		}

		static inline float Logit(const float X)
		{
			return FMath::Loge(FMath::Max(X / FMath::Max(1.0f - X, FLT_MIN), FLT_MIN));
		}
	}

	FSchemaElement FSchema::CreateNull(const FName Tag)
	{
		const int32 Index = Types.Add(EType::Null);
		Tags.Add(Tag);
		EncodedVectorSizes.Add(0);
		ActionVectorSizes.Add(0);
		ActionDistributionVectorSizes.Add(0);
		TypeDataIndices.Add(INDEX_NONE);

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateContinuous(const FSchemaContinuousParameters Parameters, const FName Tag)
	{
		FContinuousData ElementData;
		ElementData.Num = Parameters.Num;

		const int32 Index = Types.Add(EType::Continuous);
		Tags.Add(Tag);
		EncodedVectorSizes.Add(2 * Parameters.Num);
		ActionVectorSizes.Add(Parameters.Num);
		ActionDistributionVectorSizes.Add(2 * Parameters.Num);
		TypeDataIndices.Add(ContinuousData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateDiscreteExclusive(const FSchemaDiscreteExclusiveParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.PriorProbabilities.Num() == Parameters.Num);
		UE_LEARNING_CHECK(Private::CheckPriorProbabilitiesExclusive(Parameters.PriorProbabilities));

		FDiscreteExclusiveData ElementData;
		ElementData.Num = Parameters.Num;
		ElementData.PriorProbabilitiesOffset = PriorProbabilities.Num();

		PriorProbabilities.Append(Parameters.PriorProbabilities);

		const int32 Index = Types.Add(EType::DiscreteExclusive);
		Tags.Add(Tag);
		EncodedVectorSizes.Add(Parameters.Num);
		ActionVectorSizes.Add(Parameters.Num);
		ActionDistributionVectorSizes.Add(Parameters.Num);
		TypeDataIndices.Add(DiscreteExclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateDiscreteInclusive(const FSchemaDiscreteInclusiveParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.PriorProbabilities.Num() == Parameters.Num);
		UE_LEARNING_CHECK(Private::CheckPriorProbabilitiesInclusive(Parameters.PriorProbabilities));

		FDiscreteInclusiveData ElementData;
		ElementData.Num = Parameters.Num;
		ElementData.PriorProbabilitiesOffset = PriorProbabilities.Num();

		PriorProbabilities.Append(Parameters.PriorProbabilities);

		const int32 Index = Types.Add(EType::DiscreteInclusive);
		Tags.Add(Tag);
		EncodedVectorSizes.Add(Parameters.Num);
		ActionVectorSizes.Add(Parameters.Num);
		ActionDistributionVectorSizes.Add(Parameters.Num);
		TypeDataIndices.Add(DiscreteInclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateAnd(const FSchemaAndParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		UE_LEARNING_CHECK(!Private::ContainsDuplicates(Parameters.ElementNames));
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));

		FAndData ElementData;
		ElementData.Num = Parameters.Elements.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		const int32 Index = Types.Add(EType::And);
		Tags.Add(Tag);
		EncodedVectorSizes.Add(Private::GetTotalEncodedActionVectorSize(*this, Parameters.Elements));
		ActionVectorSizes.Add(Private::GetTotalActionVectorSize(*this, Parameters.Elements));
		ActionDistributionVectorSizes.Add(Private::GetTotalActionDistributionVectorSize(*this, Parameters.Elements));
		TypeDataIndices.Add(AndData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateOrExclusive(const FSchemaOrExclusiveParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		UE_LEARNING_CHECK(!Private::ContainsDuplicates(Parameters.ElementNames));
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));
		UE_LEARNING_CHECK(Parameters.PriorProbabilities.Num() == Parameters.Elements.Num());
		UE_LEARNING_CHECK(Private::CheckPriorProbabilitiesExclusive(Parameters.PriorProbabilities));

		FOrExclusiveData ElementData;
		ElementData.Num = Parameters.Elements.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();
		ElementData.PriorProbabilitiesOffset = PriorProbabilities.Num();

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);
		PriorProbabilities.Append(Parameters.PriorProbabilities);

		const int32 Index = Types.Add(EType::OrExclusive);
		Tags.Add(Tag);
		EncodedVectorSizes.Add(Private::GetTotalEncodedActionVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		ActionVectorSizes.Add(Private::GetMaxActionVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		ActionDistributionVectorSizes.Add(Private::GetTotalActionDistributionVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		TypeDataIndices.Add(OrExclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateOrInclusive(const FSchemaOrInclusiveParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		UE_LEARNING_CHECK(!Private::ContainsDuplicates(Parameters.ElementNames));
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));
		UE_LEARNING_CHECK(Parameters.PriorProbabilities.Num() == Parameters.Elements.Num());
		UE_LEARNING_CHECK(Private::CheckPriorProbabilitiesInclusive(Parameters.PriorProbabilities));

		FOrInclusiveData ElementData;
		ElementData.Num = Parameters.Elements.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();
		ElementData.PriorProbabilitiesOffset = PriorProbabilities.Num();

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);
		PriorProbabilities.Append(Parameters.PriorProbabilities);

		const int32 Index = Types.Add(EType::OrInclusive);
		Tags.Add(Tag);
		EncodedVectorSizes.Add(Private::GetTotalEncodedActionVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		ActionVectorSizes.Add(Private::GetTotalActionVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		ActionDistributionVectorSizes.Add(Private::GetTotalActionDistributionVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		TypeDataIndices.Add(OrInclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateArray(const FSchemaArrayParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(IsValid(Parameters.Element));
		UE_LEARNING_CHECK(Parameters.Num >= 0);

		FArrayData ElementData;
		ElementData.Num = Parameters.Num;
		ElementData.ElementIndex = SubElementObjects.Num();

		SubElementNames.Add(NAME_None);
		SubElementObjects.Add(Parameters.Element);

		const int32 Index = Types.Add(EType::Array);
		Tags.Add(Tag);
		EncodedVectorSizes.Add(GetEncodedVectorSize(Parameters.Element) * Parameters.Num);
		ActionVectorSizes.Add(GetActionVectorSize(Parameters.Element) * Parameters.Num);
		ActionDistributionVectorSizes.Add(GetActionDistributionVectorSize(Parameters.Element) * Parameters.Num);
		TypeDataIndices.Add(ArrayData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateEncoding(const FSchemaEncodingParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(IsValid(Parameters.Element));

		FEncodingData ElementData;
		ElementData.EncodingSize = Parameters.EncodingSize;
		ElementData.LayerNum = Parameters.LayerNum;
		ElementData.ActivationFunction = Parameters.ActivationFunction;
		ElementData.ElementIndex = SubElementObjects.Num();

		SubElementNames.Add(NAME_None);
		SubElementObjects.Add(Parameters.Element);

		const int32 Index = Types.Add(EType::Encoding);
		Tags.Add(Tag);
		EncodedVectorSizes.Add(ElementData.EncodingSize);
		ActionVectorSizes.Add(GetActionVectorSize(Parameters.Element));
		ActionDistributionVectorSizes.Add(GetActionDistributionVectorSize(Parameters.Element));
		TypeDataIndices.Add(EncodingData.Add(ElementData));

		return { Index, Generation };
	}

	bool FSchema::IsValid(const FSchemaElement Element) const
	{
		return Element.Generation == Generation && Element.Index != INDEX_NONE;
	}

	EType FSchema::GetType(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element));
		return Types[Element.Index];
	}

	FName FSchema::GetTag(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element));
		return Tags[Element.Index];
	}

	int32 FSchema::GetEncodedVectorSize(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element));
		return EncodedVectorSizes[Element.Index];
	}

	int32 FSchema::GetActionVectorSize(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element));
		return ActionVectorSizes[Element.Index];
	}

	int32 FSchema::GetActionDistributionVectorSize(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element));
		return ActionDistributionVectorSizes[Element.Index];
	}

	FSchemaContinuousParameters FSchema::GetContinuous(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Continuous);

		FSchemaContinuousParameters Parameters;
		Parameters.Num = ContinuousData[TypeDataIndices[Element.Index]].Num;
		return Parameters;
	}

	FSchemaDiscreteExclusiveParameters FSchema::GetDiscreteExclusive(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::DiscreteExclusive);
		const FDiscreteExclusiveData& ElementData = DiscreteExclusiveData[TypeDataIndices[Element.Index]];

		FSchemaDiscreteExclusiveParameters Parameters;
		Parameters.Num = ElementData.Num;
		Parameters.PriorProbabilities = TArrayView<const float>(PriorProbabilities.GetData() + ElementData.PriorProbabilitiesOffset, ElementData.Num);
		return Parameters;
	}

	FSchemaDiscreteInclusiveParameters FSchema::GetDiscreteInclusive(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::DiscreteInclusive);
		const FDiscreteInclusiveData& ElementData = DiscreteInclusiveData[TypeDataIndices[Element.Index]];

		FSchemaDiscreteInclusiveParameters Parameters;
		Parameters.Num = ElementData.Num;
		Parameters.PriorProbabilities = TArrayView<const float>(PriorProbabilities.GetData() + ElementData.PriorProbabilitiesOffset, ElementData.Num);
		return Parameters;
	}

	FSchemaAndParameters FSchema::GetAnd(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::And);
		const FAndData& ElementData = AndData[TypeDataIndices[Element.Index]];

		FSchemaAndParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.Elements = TArrayView<const FSchemaElement>(SubElementObjects.GetData() + ElementData.ElementsOffset, ElementData.Num);
		return Parameters;
	}

	FSchemaOrExclusiveParameters FSchema::GetOrExclusive(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::OrExclusive);
		const FOrExclusiveData& ElementData = OrExclusiveData[TypeDataIndices[Element.Index]];

		FSchemaOrExclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.Elements = TArrayView<const FSchemaElement>(SubElementObjects.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.PriorProbabilities = TArrayView<const float>(PriorProbabilities.GetData() + ElementData.PriorProbabilitiesOffset, ElementData.Num);
		return Parameters;
	}

	FSchemaOrInclusiveParameters FSchema::GetOrInclusive(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::OrInclusive);
		const FOrInclusiveData& ElementData = OrInclusiveData[TypeDataIndices[Element.Index]];

		FSchemaOrInclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.Elements = TArrayView<const FSchemaElement>(SubElementObjects.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.PriorProbabilities = TArrayView<const float>(PriorProbabilities.GetData() + ElementData.PriorProbabilitiesOffset, ElementData.Num);
		return Parameters;
	}

	FSchemaArrayParameters FSchema::GetArray(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Array);
		const FArrayData& ElementData = ArrayData[TypeDataIndices[Element.Index]];

		FSchemaArrayParameters Parameters;
		Parameters.Num = ElementData.Num;
		Parameters.Element = SubElementObjects[ElementData.ElementIndex];
		return Parameters;
	}

	FSchemaEncodingParameters FSchema::GetEncoding(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Encoding);
		const FEncodingData& ElementData = EncodingData[TypeDataIndices[Element.Index]];

		FSchemaEncodingParameters Parameters;
		Parameters.Element = SubElementObjects[ElementData.ElementIndex];
		Parameters.EncodingSize = ElementData.EncodingSize;
		Parameters.LayerNum = ElementData.LayerNum;
		Parameters.ActivationFunction = ElementData.ActivationFunction;
		return Parameters;
	}

	uint32 FSchema::GetGeneration() const
	{
		return Generation;
	}

	void FSchema::Empty()
	{
		Types.Empty();
		Tags.Empty();
		EncodedVectorSizes.Empty();
		ActionVectorSizes.Empty();
		ActionDistributionVectorSizes.Empty();
		TypeDataIndices.Empty();

		ContinuousData.Empty();
		DiscreteExclusiveData.Empty();
		DiscreteInclusiveData.Empty();
		AndData.Empty();
		OrExclusiveData.Empty();
		OrInclusiveData.Empty();
		ArrayData.Empty();
		EncodingData.Empty();

		SubElementNames.Empty();
		SubElementObjects.Empty();
		PriorProbabilities.Empty();

		Generation++;
	}

	void FSchema::Reset()
	{
		Types.Reset();
		Tags.Reset();
		EncodedVectorSizes.Reset();
		ActionVectorSizes.Reset();
		ActionDistributionVectorSizes.Reset();
		TypeDataIndices.Reset();

		ContinuousData.Reset();
		DiscreteExclusiveData.Reset();
		DiscreteInclusiveData.Reset();
		AndData.Reset();
		OrExclusiveData.Reset();
		OrInclusiveData.Reset();
		ArrayData.Reset();
		EncodingData.Reset();

		SubElementNames.Reset();
		SubElementObjects.Reset();
		PriorProbabilities.Reset();

		Generation++;
	}

	FObjectElement FObject::CreateNull(const FName Tag)
	{
		const int32 Index = Types.Add(EType::Null);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		ElementDataOffsets.Add(SubElementObjects.Num());
		ElementDataNums.Add(0);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateContinuous(const FObjectContinuousParameters Parameters, const FName Tag)
	{
		const int32 Index = Types.Add(EType::Continuous);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(Parameters.Values.Num());

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		ElementDataOffsets.Add(SubElementObjects.Num());
		ElementDataNums.Add(0);

		ContinuousValues.Append(Parameters.Values);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateDiscreteExclusive(const FObjectDiscreteExclusiveParameters Parameters, const FName Tag)
	{
		const int32 Index = Types.Add(EType::DiscreteExclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(1);

		ElementDataOffsets.Add(SubElementObjects.Num());
		ElementDataNums.Add(0);

		DiscreteValues.Add(Parameters.DiscreteIndex);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateDiscreteInclusive(const FObjectDiscreteInclusiveParameters Parameters, const FName Tag)
	{
		const int32 Index = Types.Add(EType::DiscreteInclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(Parameters.DiscreteIndices.Num());

		ElementDataOffsets.Add(SubElementObjects.Num());
		ElementDataNums.Add(0);

		DiscreteValues.Append(Parameters.DiscreteIndices);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateAnd(const FObjectAndParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		UE_LEARNING_CHECK(!Private::ContainsDuplicates(Parameters.ElementNames));
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));

		const int32 Index = Types.Add(EType::And);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		ElementDataOffsets.Add(SubElementObjects.Num());
		ElementDataNums.Add(Parameters.Elements.Num());

		SubElementObjects.Append(Parameters.Elements);
		SubElementNames.Append(Parameters.ElementNames);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateOrExclusive(const FObjectOrExclusiveParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(IsValid(Parameters.Element));

		const int32 Index = Types.Add(EType::OrExclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		ElementDataOffsets.Add(SubElementObjects.Num());
		ElementDataNums.Add(1);

		SubElementObjects.Add(Parameters.Element);
		SubElementNames.Add(Parameters.ElementName);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateOrInclusive(const FObjectOrInclusiveParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		UE_LEARNING_CHECK(!Private::ContainsDuplicates(Parameters.ElementNames));
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));

		const int32 Index = Types.Add(EType::OrInclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		ElementDataOffsets.Add(SubElementObjects.Num());
		ElementDataNums.Add(Parameters.Elements.Num());

		SubElementObjects.Append(Parameters.Elements);
		SubElementNames.Append(Parameters.ElementNames);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateArray(const FObjectArrayParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));

		const int32 Index = Types.Add(EType::Array);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		ElementDataOffsets.Add(SubElementObjects.Num());
		ElementDataNums.Add(Parameters.Elements.Num());

		for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
		{
			SubElementNames.Add(NAME_None);
		}
		SubElementObjects.Append(Parameters.Elements);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateEncoding(const FObjectEncodingParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(IsValid(Parameters.Element));

		const int32 Index = Types.Add(EType::Encoding);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		DiscreteDataOffsets.Add(DiscreteValues.Num());
		DiscreteDataNums.Add(0);

		ElementDataOffsets.Add(SubElementObjects.Num());
		ElementDataNums.Add(1);

		SubElementNames.Add(NAME_None);
		SubElementObjects.Add(Parameters.Element);

		return { Index, Generation };
	}

	bool FObject::IsValid(const FObjectElement Element) const
	{
		return Element.Generation == Generation && Element.Index != INDEX_NONE;
	}

	EType FObject::GetType(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element));
		return Types[Element.Index];
	}
	
	FName FObject::GetTag(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element));
		return Tags[Element.Index];
	}

	FObjectContinuousParameters FObject::GetContinuous(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Continuous);

		FObjectContinuousParameters Parameters;
		Parameters.Values = TArrayView<const float>(ContinuousValues.GetData() + ContinuousDataOffsets[Element.Index], ContinuousDataNums[Element.Index]);
		return Parameters;
	}

	FObjectDiscreteExclusiveParameters FObject::GetDiscreteExclusive(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::DiscreteExclusive);

		FObjectDiscreteExclusiveParameters Parameters;
		Parameters.DiscreteIndex = DiscreteValues[DiscreteDataOffsets[Element.Index]];
		return Parameters;
	}

	FObjectDiscreteInclusiveParameters FObject::GetDiscreteInclusive(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::DiscreteInclusive);

		FObjectDiscreteInclusiveParameters Parameters;
		Parameters.DiscreteIndices = TArrayView<const int32>(DiscreteValues.GetData() + DiscreteDataOffsets[Element.Index], DiscreteDataNums[Element.Index]);
		return Parameters;
	}

	FObjectAndParameters FObject::GetAnd(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::And);

		FObjectAndParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementDataOffsets[Element.Index], ElementDataNums[Element.Index]);
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + ElementDataOffsets[Element.Index], ElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectOrExclusiveParameters FObject::GetOrExclusive(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::OrExclusive);

		FObjectOrExclusiveParameters Parameters;
		Parameters.ElementName = SubElementNames[ElementDataOffsets[Element.Index]];
		Parameters.Element = SubElementObjects[ElementDataOffsets[Element.Index]];
		return Parameters;
	}

	FObjectOrInclusiveParameters FObject::GetOrInclusive(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::OrInclusive);

		FObjectOrInclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementDataOffsets[Element.Index], ElementDataNums[Element.Index]);
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + ElementDataOffsets[Element.Index], ElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectArrayParameters FObject::GetArray(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Array);
		
		FObjectArrayParameters Parameters;
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + ElementDataOffsets[Element.Index], ElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectEncodingParameters FObject::GetEncoding(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Encoding);

		FObjectEncodingParameters Parameters;
		Parameters.Element = SubElementObjects[ElementDataOffsets[Element.Index]];
		return Parameters;
	}

	uint32 FObject::GetGeneration() const
	{
		return Generation;
	}

	void FObject::Empty()
	{
		Types.Empty();
		Tags.Empty();
		ContinuousDataOffsets.Empty();
		ContinuousDataNums.Empty();
		DiscreteDataOffsets.Empty();
		DiscreteDataNums.Empty();
		ElementDataOffsets.Empty();
		ElementDataNums.Empty();

		ContinuousValues.Empty();
		DiscreteValues.Empty();
		SubElementObjects.Empty();
		SubElementNames.Empty();

		Generation++;
	}

	void FObject::Reset()
	{
		Types.Reset();
		Tags.Reset();
		ContinuousDataOffsets.Reset();
		ContinuousDataNums.Reset();
		DiscreteDataOffsets.Reset();
		DiscreteDataNums.Reset();
		ElementDataOffsets.Reset();
		ElementDataNums.Reset();

		ContinuousValues.Reset();
		DiscreteValues.Reset();
		SubElementObjects.Reset();
		SubElementNames.Reset();

		Generation++;
	}

	namespace Private
	{
		static inline NNE::RuntimeBasic::FModelBuilder::EActivationFunction GetNNEActivationFunction(const EEncodingActivationFunction ActivationFunction)
		{
			switch (ActivationFunction)
			{
			case EEncodingActivationFunction::ReLU: return NNE::RuntimeBasic::FModelBuilder::EActivationFunction::ReLU;
			case EEncodingActivationFunction::ELU: return NNE::RuntimeBasic::FModelBuilder::EActivationFunction::ELU;
			case EEncodingActivationFunction::TanH: return NNE::RuntimeBasic::FModelBuilder::EActivationFunction::TanH;
			default: UE_LEARNING_NOT_IMPLEMENTED(); return NNE::RuntimeBasic::FModelBuilder::EActivationFunction::ReLU;
			}
		}

		NNE::RuntimeBasic::FModelBuilderElement MakeDecoderNetworkFromSchema(
			NNE::RuntimeBasic::FModelBuilder& Builder,
			const FSchema& Schema,
			const FSchemaElement SchemaElement)
		{
			const EType SchemaElementType = Schema.GetType(SchemaElement);

			NNE::RuntimeBasic::FModelBuilderElement ReturnElement;

			switch (SchemaElementType)
			{

			case EType::Null:
			{
				ReturnElement = Builder.MakeCopy(0);
				break;
			}

			case EType::Continuous:
			{
				const int32 ValueNum = Schema.GetContinuous(SchemaElement).Num * 2;

				ReturnElement = Builder.MakeDenormalize(
					ValueNum,
					Builder.MakeWeightsZero(ValueNum),
					Builder.MakeWeightsConstant(ValueNum, 1.0f));
				break;
			}

			case EType::DiscreteExclusive:
			{
				const FSchemaDiscreteExclusiveParameters Parameters = Schema.GetDiscreteExclusive(SchemaElement);

				TArray<float, TInlineAllocator<16>> LogPriorProbabilities;
				LogPriorProbabilities.Append(Parameters.PriorProbabilities);
				for (int32 Idx = 0; Idx < Parameters.Num; Idx++)
				{
					// Clamp zero probabilities to the smallest (positive) float. This is approximately equal to a probability of 1:1e38
					LogPriorProbabilities[Idx] = FMath::Loge(FMath::Max(LogPriorProbabilities[Idx], FLT_MIN));
				}

				ReturnElement = Builder.MakeDenormalize(
					Parameters.Num,
					Builder.MakeWeightsCopy(LogPriorProbabilities),
					Builder.MakeWeightsConstant(Parameters.Num, 1.0f));

				break;
			}

			case EType::DiscreteInclusive:
			{
				const FSchemaDiscreteInclusiveParameters Parameters = Schema.GetDiscreteInclusive(SchemaElement);

				TArray<float, TInlineAllocator<16>> LogPriorProbabilities;
				LogPriorProbabilities.Append(Parameters.PriorProbabilities);
				for (int32 Idx = 0; Idx < Parameters.Num; Idx++)
				{
					LogPriorProbabilities[Idx] = Private::Logit(LogPriorProbabilities[Idx]);
				}

				ReturnElement = Builder.MakeDenormalize(Parameters.Num,
					Builder.MakeWeightsCopy(LogPriorProbabilities),
					Builder.MakeWeightsConstant(Parameters.Num, 1.0f));
				break;
			}
					
			case EType::And:
			{
				const FSchemaAndParameters Parameters = Schema.GetAnd(SchemaElement);

				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderLayers;
				BuilderLayers.Reserve(Parameters.Elements.Num());
				for (const FSchemaElement SubElement : Parameters.Elements)
				{
					BuilderLayers.Emplace(MakeDecoderNetworkFromSchema(Builder, Schema, SubElement));
				}

				ReturnElement = Builder.MakeConcat(BuilderLayers);
				break;
			}

			case EType::OrExclusive:
			{
				const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);

				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderLayers;
				BuilderLayers.Reserve(Parameters.Elements.Num() + 1);
				for (const FSchemaElement SubElement : Parameters.Elements)
				{
					BuilderLayers.Emplace(MakeDecoderNetworkFromSchema(Builder, Schema, SubElement));
				}

				TArray<float, TInlineAllocator<16>> LogPriorProbabilities;
				LogPriorProbabilities.Append(Parameters.PriorProbabilities);
				for (int32 Idx = 0; Idx < Parameters.PriorProbabilities.Num(); Idx++)
				{
					// Clamp zero probabilities to the smallest (positive) float. This is approximately equal to a probability of 1:1e38
					LogPriorProbabilities[Idx] = FMath::Loge(FMath::Max(LogPriorProbabilities[Idx], FLT_MIN));
				}

				BuilderLayers.Emplace(Builder.MakeDenormalize(
					LogPriorProbabilities.Num(),
					Builder.MakeWeightsCopy(LogPriorProbabilities),
					Builder.MakeWeightsConstant(LogPriorProbabilities.Num(), 1.0f)));

				ReturnElement = Builder.MakeConcat(BuilderLayers);
				break;
			}

			case EType::OrInclusive:
			{
				const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);

				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderLayers;
				BuilderLayers.Reserve(Parameters.Elements.Num() + 1);
				for (const FSchemaElement SubElement : Parameters.Elements)
				{
					BuilderLayers.Emplace(MakeDecoderNetworkFromSchema(Builder, Schema, SubElement));
				}

				TArray<float, TInlineAllocator<16>> LogPriorProbabilities;
				LogPriorProbabilities.Append(Parameters.PriorProbabilities);
				for (int32 Idx = 0; Idx < Parameters.PriorProbabilities.Num(); Idx++)
				{
					LogPriorProbabilities[Idx] = Private::Logit(LogPriorProbabilities[Idx]);
				}

				BuilderLayers.Emplace(Builder.MakeDenormalize(
					LogPriorProbabilities.Num(),
					Builder.MakeWeightsCopy(LogPriorProbabilities),
					Builder.MakeWeightsConstant(LogPriorProbabilities.Num(), 1.0f)));

				ReturnElement = Builder.MakeConcat(BuilderLayers);
				break;
			}

			case EType::Array:
			{
				const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);

				ReturnElement = Builder.MakeArray(Parameters.Num, MakeDecoderNetworkFromSchema(Builder, Schema, Parameters.Element));
				break;
			}

			case EType::Encoding:
			{
				const FSchemaEncodingParameters Parameters = Schema.GetEncoding(SchemaElement);

				const int32 SubElementEncodedSize = Schema.GetEncodedVectorSize(Parameters.Element);

				ReturnElement = Builder.MakeSequence({
					Builder.MakeActivation(Parameters.EncodingSize, GetNNEActivationFunction(Parameters.ActivationFunction)),
					Builder.MakeMLPWithRandomKaimingWeights(
						Parameters.EncodingSize, 
						SubElementEncodedSize, 
						Parameters.EncodingSize, 
						Parameters.LayerNum + 1,  // Add 1 to account for input layer 
						GetNNEActivationFunction(Parameters.ActivationFunction),
						false),
					MakeDecoderNetworkFromSchema(Builder, Schema, Parameters.Element),
				});
				break;
			}

			default:
			{
				UE_LEARNING_NOT_IMPLEMENTED();
			}
			}

			UE_LEARNING_CHECKF(ReturnElement.GetInputSize() == Schema.GetEncodedVectorSize(SchemaElement),
				TEXT("Decoder Network Input unexpected size. Got %i, expected %i according to Schema."),
				ReturnElement.GetInputSize(), Schema.GetEncodedVectorSize(SchemaElement));

			UE_LEARNING_CHECKF(ReturnElement.GetOutputSize() == Schema.GetActionDistributionVectorSize(SchemaElement),
				TEXT("Decoder Network Output unexpected size. Got %i, expected %i according to Schema."),
				ReturnElement.GetOutputSize(), Schema.GetActionDistributionVectorSize(SchemaElement));

			return ReturnElement;
		}

		static inline int32 HashFNameStable(const FName Name)
		{
			const FString NameString = Name.ToString().ToLower();
			return (int32)CityHash32(
				(const char*)NameString.GetCharArray().GetData(),
				NameString.GetCharArray().GetTypeSize() *
				NameString.GetCharArray().Num());
		}

		static inline int32 HashInt(const int32 Int)
		{
			return (int32)CityHash32((const char*)&Int, sizeof(int32));
		}

		static inline int32 HashCombine(const TArrayView<const int32> Hashes)
		{
			return (int32)CityHash32((const char*)Hashes.GetData(), Hashes.Num() * Hashes.GetTypeSize());
		}

		static inline int32 HashElements(
			const FSchema& Schema,
			const TArrayView<const FName> SchemaElementNames,
			const TArrayView<const FSchemaElement> SchemaElements,
			const int32 Salt)
		{
			// Note: Here we xor all entries together. 
			// This makes the hash in invariant to the ordering of pairs of names and elements 
			// which is actually what we want since these two arrays are representing a map-like 
			// structure and it is fine to pass keys and values in a different order.

			int32 Hash = 0x5b3bbe4d;
			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaElements.Num(); SchemaElementIdx++)
			{
				Hash ^= HashCombine({ HashFNameStable(SchemaElementNames[SchemaElementIdx]), GetSchemaObjectsCompatibilityHash(Schema, SchemaElements[SchemaElementIdx], Salt) });
			}

			return Hash;
		}
	}

	int32 GetSchemaObjectsCompatibilityHash(
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const int32 Salt)
	{
		UE_LEARNING_CHECK(Schema.IsValid(SchemaElement));
		const EType SchemaElementType = Schema.GetType(SchemaElement);

		const int32 Hash = Private::HashCombine({ Salt, Private::HashInt((int32)SchemaElementType) });

		switch (SchemaElementType)
		{
		case EType::Null: return Hash;

		case EType::Continuous: return Private::HashCombine({ Hash, Private::HashInt(Schema.GetContinuous(SchemaElement).Num) });

		case EType::DiscreteExclusive: return Private::HashCombine({ Hash, Private::HashInt(Schema.GetDiscreteExclusive(SchemaElement).Num) });

		case EType::DiscreteInclusive: return Private::HashCombine({ Hash, Private::HashInt(Schema.GetDiscreteInclusive(SchemaElement).Num) });

		case EType::And:
		{
			const FSchemaAndParameters Parameters = Schema.GetAnd(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashElements(Schema, Parameters.ElementNames, Parameters.Elements, Salt) });
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashElements(Schema, Parameters.ElementNames, Parameters.Elements, Salt) });
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashElements(Schema, Parameters.ElementNames, Parameters.Elements, Salt) });
		}

		case EType::Array:
		{
			const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashInt(Parameters.Num), GetSchemaObjectsCompatibilityHash(Schema, Parameters.Element, Salt) });
		}

		case EType::Encoding:
		{
			const FSchemaEncodingParameters Parameters = Schema.GetEncoding(SchemaElement);
			return GetSchemaObjectsCompatibilityHash(Schema, Parameters.Element, Salt);
		}

		default:
		{
			UE_LEARNING_NOT_IMPLEMENTED();
			return 0;
		}

		}
	}

	bool AreSchemaObjectsCompatible(
		const FSchema& SchemaA,
		const FSchemaElement SchemaElementA,
		const FSchema& SchemaB,
		const FSchemaElement SchemaElementB)
	{
		UE_LEARNING_CHECK(SchemaA.IsValid(SchemaElementA));
		UE_LEARNING_CHECK(SchemaB.IsValid(SchemaElementB));

		const EType SchemaElementTypeA = SchemaA.GetType(SchemaElementA);
		const EType SchemaElementTypeB = SchemaB.GetType(SchemaElementB);

		// If any element is an encoding element we forward the comparison to the sub-element since encoding elements don't affect compatibility
		if (SchemaElementTypeA == EType::Encoding) { return AreSchemaObjectsCompatible(SchemaA, SchemaA.GetEncoding(SchemaElementA).Element, SchemaB, SchemaElementB); }
		if (SchemaElementTypeB == EType::Encoding) { return AreSchemaObjectsCompatible(SchemaA, SchemaElementA, SchemaB, SchemaB.GetEncoding(SchemaElementB).Element); }

		// Otherwise if types don't match we immediately know elements are incompatible
		if (SchemaElementTypeA != SchemaElementTypeB) { return false; }

		// This is an early-out since if the input sizes are different we are definitely incompatible
		if (SchemaA.GetActionVectorSize(SchemaElementA) != SchemaB.GetActionVectorSize(SchemaElementB)) { return false; }

		switch (SchemaElementTypeA)
		{
		case EType::Null: return true;

		case EType::Continuous: return SchemaA.GetContinuous(SchemaElementA).Num == SchemaB.GetContinuous(SchemaElementB).Num;

		case EType::DiscreteExclusive: return SchemaA.GetDiscreteExclusive(SchemaElementA).Num == SchemaB.GetDiscreteExclusive(SchemaElementB).Num;

		case EType::DiscreteInclusive: return SchemaA.GetDiscreteInclusive(SchemaElementA).Num == SchemaB.GetDiscreteInclusive(SchemaElementB).Num;

		case EType::And:
		{
			const FSchemaAndParameters ParametersA = SchemaA.GetAnd(SchemaElementA);
			const FSchemaAndParameters ParametersB = SchemaB.GetAnd(SchemaElementB);

			if (ParametersA.Elements.Num() != ParametersB.Elements.Num()) { return false; }

			for (int32 SchemaElementAIdx = 0; SchemaElementAIdx < ParametersA.Elements.Num(); SchemaElementAIdx++)
			{
				const int32 SchemaElementBIdx = ParametersB.ElementNames.Find(ParametersA.ElementNames[SchemaElementAIdx]);
				if (SchemaElementBIdx == INDEX_NONE) { return false; }
				if (!AreSchemaObjectsCompatible(SchemaA, ParametersA.Elements[SchemaElementAIdx], SchemaB, ParametersB.Elements[SchemaElementBIdx])) { return false; }
			}

			return true;
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters ParametersA = SchemaA.GetOrExclusive(SchemaElementA);
			const FSchemaOrExclusiveParameters ParametersB = SchemaB.GetOrExclusive(SchemaElementB);

			if (ParametersA.Elements.Num() != ParametersB.Elements.Num()) { return false; }

			for (int32 SchemaElementAIdx = 0; SchemaElementAIdx < ParametersA.Elements.Num(); SchemaElementAIdx++)
			{
				const int32 SchemaElementBIdx = ParametersB.ElementNames.Find(ParametersA.ElementNames[SchemaElementAIdx]);
				if (SchemaElementBIdx == INDEX_NONE) { return false; }
				if (!AreSchemaObjectsCompatible(SchemaA, ParametersA.Elements[SchemaElementAIdx], SchemaB, ParametersB.Elements[SchemaElementBIdx])) { return false; }
			}

			return true;
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters ParametersA = SchemaA.GetOrInclusive(SchemaElementA);
			const FSchemaOrInclusiveParameters ParametersB = SchemaB.GetOrInclusive(SchemaElementB);

			if (ParametersA.Elements.Num() != ParametersB.Elements.Num()) { return false; }

			for (int32 SchemaElementAIdx = 0; SchemaElementAIdx < ParametersA.Elements.Num(); SchemaElementAIdx++)
			{
				const int32 SchemaElementBIdx = ParametersB.ElementNames.Find(ParametersA.ElementNames[SchemaElementAIdx]);
				if (SchemaElementBIdx == INDEX_NONE) { return false; }
				if (!AreSchemaObjectsCompatible(SchemaA, ParametersA.Elements[SchemaElementAIdx], SchemaB, ParametersB.Elements[SchemaElementBIdx])) { return false; }
			}

			return true;
		}

		case EType::Array:
		{
			const FSchemaArrayParameters ParametersA = SchemaA.GetArray(SchemaElementA);
			const FSchemaArrayParameters ParametersB = SchemaB.GetArray(SchemaElementB);

			return (ParametersA.Num == ParametersB.Num) && AreSchemaObjectsCompatible(SchemaA, ParametersA.Element, SchemaB, ParametersB.Element);
		}

		case EType::Encoding:
		{
			UE_LEARNING_CHECKF(false, TEXT("Encoding elements should always be forwarded..."));
			return false;
		}

		default:
		{
			UE_LEARNING_NOT_IMPLEMENTED();
			return false;
		}

		}
	}

	LEARNING_API void GenerateDecoderNetworkFileDataFromSchema(
		TArray<uint8>& OutFileData,
		uint32& OutInputSize,
		uint32& OutOutputSize,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const uint32 Seed)
	{
		UE_LEARNING_CHECK(Schema.IsValid(SchemaElement));

		NNE::RuntimeBasic::FModelBuilder Builder(Seed);
		Builder.WriteFileDataAndReset(OutFileData, OutInputSize, OutOutputSize, Private::MakeDecoderNetworkFromSchema(Builder, Schema, SchemaElement));
	}

	void SampleVectorFromDistributionVector(
		uint32& InOutSeed,
		TLearningArrayView<1, float> OutActionVector,
		const TLearningArrayView<1, const float> ActionDistributionVector,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const float ActionNoiseScale)
	{
		UE_LEARNING_CHECK(Schema.IsValid(SchemaElement));

		const EType SchemaElementType = Schema.GetType(SchemaElement);

		switch (SchemaElementType)
		{
		case EType::Null: break;

		case EType::Continuous:
		{
			const int32 ValueNum = Schema.GetContinuous(SchemaElement).Num;
			UE_LEARNING_CHECK(ValueNum == OutActionVector.Num());
			UE_LEARNING_CHECK(ValueNum * 2 == ActionDistributionVector.Num());

			Random::SampleDistributionIndependantNormal(
				OutActionVector,
				InOutSeed,
				ActionDistributionVector.Slice(0, ValueNum),
				ActionDistributionVector.Slice(ValueNum, ValueNum),
				ActionNoiseScale);

			break;
		}

		case EType::DiscreteExclusive:
		{
			const int32 ValueNum = Schema.GetDiscreteExclusive(SchemaElement).Num;
			UE_LEARNING_CHECK(ValueNum == OutActionVector.Num());
			UE_LEARNING_CHECK(ValueNum == ActionDistributionVector.Num());

			Random::SampleDistributionMultinoulli(
				OutActionVector,
				InOutSeed,
				ActionDistributionVector,
				ActionNoiseScale);

			break;
		}

		case EType::DiscreteInclusive:
		{
			const int32 ValueNum = Schema.GetDiscreteInclusive(SchemaElement).Num;
			UE_LEARNING_CHECK(ValueNum == OutActionVector.Num());
			UE_LEARNING_CHECK(ValueNum == ActionDistributionVector.Num());

			Random::SampleDistributionBernoulli(
				OutActionVector,
				InOutSeed,
				ActionDistributionVector,
				ActionNoiseScale);

			break;
		}

		case EType::And:
		{
			const FSchemaAndParameters Parameters = Schema.GetAnd(SchemaElement);

			int32 SubElementActionVectorOffset = 0;
			int32 SubElementActionDistributionVectorOffset = 0;

			for (const FSchemaElement SubElement : Parameters.Elements)
			{
				const int32 SubElementActionVectorSize = Schema.GetActionVectorSize(SubElement);
				const int32 SubElementActionDistributionVectorSize = Schema.GetActionDistributionVectorSize(SubElement);

				SampleVectorFromDistributionVector(
					InOutSeed,
					OutActionVector.Slice(SubElementActionVectorOffset, SubElementActionVectorSize),
					ActionDistributionVector.Slice(SubElementActionDistributionVectorOffset, SubElementActionDistributionVectorSize),
					Schema,
					SubElement,
					ActionNoiseScale);

				SubElementActionVectorOffset += SubElementActionVectorSize;
				SubElementActionDistributionVectorOffset += SubElementActionDistributionVectorSize;
			}

			UE_LEARNING_CHECK(SubElementActionVectorOffset == OutActionVector.Num());
			UE_LEARNING_CHECK(SubElementActionDistributionVectorOffset == ActionDistributionVector.Num());

			break;
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);

			const int32 SubElementActionVectorMax = Private::GetMaxActionVectorSize(Schema, Parameters.Elements);
			const int32 SubElementActionDistributionVectorTotal = Private::GetTotalActionDistributionVectorSize(Schema, Parameters.Elements);

			// Zero main part of vector
			Array::Zero(OutActionVector.Slice(0, SubElementActionVectorMax));

			// Sample which sub-element to generate
			Random::SampleDistributionMultinoulli(
				OutActionVector.Slice(SubElementActionVectorMax, Parameters.Elements.Num()),
				InOutSeed,
				ActionDistributionVector.Slice(SubElementActionDistributionVectorTotal, Parameters.Elements.Num()),
				ActionNoiseScale);

			int32 SubElementActionDistributionVectorOffset = 0;

			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				if (OutActionVector[SubElementActionVectorMax + SubElementIdx])
				{
					const FSchemaElement SubElement = Parameters.Elements[SubElementIdx];
					const int32 SubElementActionVectorSize = Schema.GetActionVectorSize(SubElement);
					const int32 SubElementActionDistributionVectorSize = Schema.GetActionDistributionVectorSize(SubElement);

					// Sample Sub-Element
					SampleVectorFromDistributionVector(
						InOutSeed,
						OutActionVector.Slice(0, SubElementActionVectorSize),
						ActionDistributionVector.Slice(SubElementActionDistributionVectorOffset, SubElementActionDistributionVectorSize),
						Schema,
						SubElement,
						ActionNoiseScale);

					SubElementActionDistributionVectorOffset += SubElementActionDistributionVectorSize;

					// We can break early because this exclusively samples one action
					break;
				}
			}

			break;
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);

			const int32 SubElementActionVectorTotal = Private::GetTotalActionVectorSize(Schema, Parameters.Elements);
			const int32 SubElementActionDistributionVectorTotal = Private::GetTotalActionDistributionVectorSize(Schema, Parameters.Elements);

			// Zero main part of vector
			Array::Zero(OutActionVector.Slice(0, SubElementActionVectorTotal));

			// Sample which sub-elements to generate
			Random::SampleDistributionBernoulli(
				OutActionVector.Slice(SubElementActionVectorTotal, Parameters.Elements.Num()),
				InOutSeed,
				ActionDistributionVector.Slice(SubElementActionDistributionVectorTotal, Parameters.Elements.Num()),
				ActionNoiseScale);

			int32 SubElementActionVectorOffset = 0;
			int32 SubElementActionDistributionVectorOffset = 0;

			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				if (OutActionVector[SubElementActionVectorTotal + SubElementIdx])
				{
					const FSchemaElement SubElement = Parameters.Elements[SubElementIdx];
					const int32 SubElementActionVectorSize = Schema.GetActionVectorSize(SubElement);
					const int32 SubElementActionDistributionVectorSize = Schema.GetActionDistributionVectorSize(SubElement);

					// Sample sub-elements
					SampleVectorFromDistributionVector(
						InOutSeed,
						OutActionVector.Slice(SubElementActionVectorOffset, SubElementActionVectorSize),
						ActionDistributionVector.Slice(SubElementActionDistributionVectorOffset, SubElementActionDistributionVectorSize),
						Schema,
						SubElement,
						ActionNoiseScale);

					SubElementActionVectorOffset += SubElementActionVectorSize;
					SubElementActionDistributionVectorOffset += SubElementActionDistributionVectorSize;
				}
			}

			break;
		}

		case EType::Array:
		{
			const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);

			const int32 SubElementActionVectorSize = Schema.GetActionVectorSize(Parameters.Element);
			const int32 SubElementActionDistributionVectorSize = Schema.GetActionDistributionVectorSize(Parameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < Parameters.Num; ElementIdx++)
			{
				SampleVectorFromDistributionVector(
					InOutSeed,
					OutActionVector.Slice(ElementIdx * SubElementActionVectorSize, SubElementActionVectorSize),
					ActionDistributionVector.Slice(ElementIdx * SubElementActionDistributionVectorSize, SubElementActionDistributionVectorSize),
					Schema,
					Parameters.Element,
					ActionNoiseScale);
			}

			break;
		}

		case EType::Encoding:
		{
			const FSchemaEncodingParameters Parameters = Schema.GetEncoding(SchemaElement);

			SampleVectorFromDistributionVector(
				InOutSeed,
				OutActionVector,
				ActionDistributionVector,
				Schema,
				Parameters.Element,
				ActionNoiseScale);

			break;
		}

		}
	}
		
	void SetVectorFromObject(
		TLearningArrayView<1, float> OutActionVector,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const FObject& Object,
		const FObjectElement ObjectElement)
	{
		UE_LEARNING_CHECK(Schema.IsValid(SchemaElement));
		UE_LEARNING_CHECK(Object.IsValid(ObjectElement));
		UE_LEARNING_CHECK(OutActionVector.Num() == Schema.GetActionVectorSize(SchemaElement));

		// Check that the types match

		const EType SchemaElementType = Schema.GetType(SchemaElement);
		const EType ObjectElementType = Object.GetType(ObjectElement);
		UE_LEARNING_CHECK(ObjectElementType == SchemaElementType);

		// Zero Action Vector

		Array::Zero(OutActionVector);

		// Logic for each specific element type

		switch (SchemaElementType)
		{

		case EType::Null: return;

		case EType::Continuous:
		{
			// Check the input sizes match

			TArrayView<const float> ActionValues = Object.GetContinuous(ObjectElement).Values;
			UE_LEARNING_CHECK(Schema.GetActionVectorSize(SchemaElement) == ActionValues.Num());
			UE_LEARNING_CHECK(Schema.GetActionVectorSize(SchemaElement) == OutActionVector.Num());

			// Copy in the values from the action object

			Array::Copy<1, float>(OutActionVector, ActionValues);
			return;
		}

		case EType::DiscreteExclusive:
		{
			int32 ActionValue = Object.GetDiscreteExclusive(ObjectElement).DiscreteIndex;
			UE_LEARNING_CHECK(Schema.GetActionVectorSize(SchemaElement) > ActionValue && ActionValue >= 0);
			UE_LEARNING_CHECK(Schema.GetActionVectorSize(SchemaElement) == OutActionVector.Num());

			// Set the single value in the action vector

			OutActionVector[ActionValue] = 1.0f;
			return;
		}

		case EType::DiscreteInclusive:
		{
			TArrayView<const int32> ActionValues = Object.GetDiscreteInclusive(ObjectElement).DiscreteIndices;
			UE_LEARNING_CHECK(Schema.GetActionVectorSize(SchemaElement) >= ActionValues.Num());
			UE_LEARNING_CHECK(Schema.GetActionVectorSize(SchemaElement) == OutActionVector.Num());

			// Set values in the action vector

			for (int32 ActionValueIdx = 0; ActionValueIdx < ActionValues.Num(); ActionValueIdx++)
			{
				UE_LEARNING_CHECK(Schema.GetActionVectorSize(SchemaElement) > ActionValues[ActionValueIdx] && ActionValues[ActionValueIdx] >= 0);
				OutActionVector[ActionValues[ActionValueIdx]] = 1.0f;
			}

			return;
		}

		case EType::And:
		{
			// Check the number of sub-elements match

			const FSchemaAndParameters SchemaParameters = Schema.GetAnd(SchemaElement);
			const FObjectAndParameters ObjectParameters = Object.GetAnd(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.Elements.Num() == ObjectParameters.Elements.Num());

			// Set the Sub-elements

			int32 SubElementOffset = 0;

			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaParameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 ObjectElementIndex = ObjectParameters.ElementNames.Find(SchemaParameters.ElementNames[SchemaElementIdx]);
				UE_LEARNING_CHECK(ObjectElementIndex != INDEX_NONE);

				const int32 SubElementSize = Schema.GetActionVectorSize(SchemaParameters.Elements[SchemaElementIdx]);

				SetVectorFromObject(
					OutActionVector.Slice(SubElementOffset, SubElementSize),
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Object,
					ObjectParameters.Elements[ObjectElementIndex]);

				SubElementOffset += SubElementSize;
			}

			UE_LEARNING_CHECK(SubElementOffset == OutActionVector.Num());
			return;
		}

		case EType::OrExclusive:
		{
			// Check only one sub-element is given and index is valid

			const FSchemaOrExclusiveParameters SchemaParameters = Schema.GetOrExclusive(SchemaElement);
			const FObjectOrExclusiveParameters ObjectParameters = Object.GetOrExclusive(ObjectElement);

			const int32 SchemaElementIndex = SchemaParameters.ElementNames.Find(ObjectParameters.ElementName);
			UE_LEARNING_CHECK(SchemaElementIndex != INDEX_NONE);

			// Set the sub-element

			const int32 SubElementSize = Schema.GetActionVectorSize(SchemaParameters.Elements[SchemaElementIndex]);

			SetVectorFromObject(
				OutActionVector.Slice(0, SubElementSize),
				Schema,
				SchemaParameters.Elements[SchemaElementIndex],
				Object,
				ObjectParameters.Element);

			// Set Mask

			const int32 MaxSubElementSize = Private::GetMaxActionVectorSize(Schema, SchemaParameters.Elements);

			OutActionVector[MaxSubElementSize + SchemaElementIndex] = 1.0f;

			UE_LEARNING_CHECK(OutActionVector.Num() == MaxSubElementSize + SchemaParameters.Elements.Num());
			return;
		}

		case EType::OrInclusive:
		{
			// Check all indices are in range

			const FSchemaOrInclusiveParameters SchemaParameters = Schema.GetOrInclusive(SchemaElement);
			const FObjectOrInclusiveParameters ObjectParameters = Object.GetOrInclusive(ObjectElement);
			UE_LEARNING_CHECK(ObjectParameters.Elements.Num() <= SchemaParameters.Elements.Num());

			// Update sub-elements

			int32 SubElementOffset = 0;

			for (int32 ObjectElementIdx = 0; ObjectElementIdx < ObjectParameters.Elements.Num(); ObjectElementIdx++)
			{
				const int32 SchemaElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementNames[ObjectElementIdx]);
				UE_LEARNING_CHECK(SchemaElementIdx != INDEX_NONE);

				const int32 SubElementSize = Schema.GetActionVectorSize(SchemaParameters.Elements[SchemaElementIdx]);

				SetVectorFromObject(
					OutActionVector.Slice(SubElementOffset, SubElementSize),
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Object,
					ObjectParameters.Elements[ObjectElementIdx]);

				SubElementOffset += SubElementSize;
			}

			// Set Mask

			UE_LEARNING_CHECK(SubElementOffset + SchemaParameters.Elements.Num() == OutActionVector.Num());

			for (int32 ObjectElementIdx = 0; ObjectElementIdx < ObjectParameters.Elements.Num(); ObjectElementIdx++)
			{
				const int32 SchemaElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementNames[ObjectElementIdx]);
				UE_LEARNING_CHECK(SchemaElementIdx != INDEX_NONE);

				OutActionVector[SubElementOffset + SchemaElementIdx] = 1.0f;
			}

			return;
		}

		case EType::Array:
		{
			// Check number of array elements is correct

			const FSchemaArrayParameters SchemaParameters = Schema.GetArray(SchemaElement);
			const FObjectArrayParameters ObjectParameters = Object.GetArray(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.Num == ObjectParameters.Elements.Num());

			// Update sub-elements

			const int32 SubElementSize = Schema.GetActionVectorSize(SchemaParameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < SchemaParameters.Num; ElementIdx++)
			{
				SetVectorFromObject(
					OutActionVector.Slice(ElementIdx * SubElementSize, SubElementSize),
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx]);
			}

			return;
		}

		case EType::Encoding:
		{
			const FSchemaEncodingParameters SchemaParameters = Schema.GetEncoding(SchemaElement);
			const FObjectEncodingParameters ObjectParameters = Object.GetEncoding(ObjectElement);

			SetVectorFromObject(
				OutActionVector,
				Schema,
				SchemaParameters.Element,
				Object,
				ObjectParameters.Element);

			return;
		}

		default:
		{
			UE_LEARNING_NOT_IMPLEMENTED();
			return;
		}

		}
	}

	void GetObjectFromVector(
		FObject& OutObject,
		FObjectElement& OutObjectElement,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const TLearningArrayView<1, const float> ActionVector)
	{
		UE_LEARNING_CHECK(Schema.IsValid(SchemaElement));

		// Check that the types match

		const EType SchemaElementType = Schema.GetType(SchemaElement);
		const FName SchemaElementTag = Schema.GetTag(SchemaElement);

		// Get Action Vector Size

		const int32 ActionVectorSize = ActionVector.Num();
		UE_LEARNING_CHECK(ActionVectorSize == Schema.GetActionVectorSize(SchemaElement));

		// Logic for each specific element type

		switch (SchemaElementType)
		{

		case EType::Null:
		{
			OutObjectElement = OutObject.CreateNull(SchemaElementTag);
			return;
		}

		case EType::Continuous:
		{
			UE_LEARNING_CHECK(ActionVectorSize == Schema.GetContinuous(SchemaElement).Num);

			OutObjectElement = OutObject.CreateContinuous({ MakeArrayView(ActionVector.GetData(), ActionVector.Num()) }, SchemaElementTag);
			return;
		}

		case EType::DiscreteExclusive:
		{
			UE_LEARNING_CHECK(ActionVectorSize == Schema.GetDiscreteExclusive(SchemaElement).Num);

			// Find Index
			int32 ExclusiveIndex = INDEX_NONE;
			for (int32 Idx = 0; Idx < ActionVectorSize; Idx++)
			{
				UE_LEARNING_CHECK(ActionVector[Idx] == 0.0f || ActionVector[Idx] == 1.0f);
				if (ActionVector[Idx])
				{
					ExclusiveIndex = Idx;
					break;
				}
			}
			UE_LEARNING_CHECK(ExclusiveIndex != INDEX_NONE);

			OutObjectElement = OutObject.CreateDiscreteExclusive({ ExclusiveIndex }, SchemaElementTag);
			return;
		}

		case EType::DiscreteInclusive:
		{
			UE_LEARNING_CHECK(ActionVectorSize == Schema.GetDiscreteInclusive(SchemaElement).Num);

			// Find Indices
			TArray<int32, TInlineAllocator<8>> InclusiveIndices;
			InclusiveIndices.Reserve(ActionVectorSize);
			for (int32 Idx = 0; Idx < ActionVectorSize; Idx++)
			{
				UE_LEARNING_CHECK(ActionVector[Idx] == 0.0f || ActionVector[Idx] == 1.0f);
				if (ActionVector[Idx])
				{
					InclusiveIndices.Add(Idx);
				}
			}

			OutObjectElement = OutObject.CreateDiscreteInclusive({ InclusiveIndices }, SchemaElementTag);
			return;
		}

		case EType::And:
		{
			const FSchemaAndParameters Parameters = Schema.GetAnd(SchemaElement);
			
			// Create Sub-elements

			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElements.SetNumUninitialized(Parameters.Elements.Num());

			int32 SubElementOffset = 0;
			for (int32 SchemaElementIdx = 0; SchemaElementIdx < Parameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 SubElementSize = Schema.GetActionVectorSize(Parameters.Elements[SchemaElementIdx]);

				GetObjectFromVector(
					OutObject,
					SubElements[SchemaElementIdx],
					Schema,
					Parameters.Elements[SchemaElementIdx],
					ActionVector.Slice(SubElementOffset, SubElementSize));

				SubElementOffset += SubElementSize;
			}
			UE_LEARNING_CHECK(SubElementOffset == ActionVectorSize);

			OutObjectElement = OutObject.CreateAnd({ Parameters.ElementNames, SubElements }, SchemaElementTag);
			return;
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);

			// Find active element

			const int32 MaxSubElementSize = Private::GetMaxActionVectorSize(Schema, Parameters.Elements);

			int32 SchemaElementIndex = INDEX_NONE;
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				UE_LEARNING_CHECK(ActionVector[MaxSubElementSize + SubElementIdx] == 0.0f || ActionVector[MaxSubElementSize + SubElementIdx] == 1.0f);
				if (ActionVector[MaxSubElementSize + SubElementIdx])
				{
					SchemaElementIndex = SubElementIdx;
					break;
				}
			}
			UE_LEARNING_CHECK(SchemaElementIndex != INDEX_NONE);

			// Create sub-element

			const int32 SubElementSize = Schema.GetActionVectorSize(Parameters.Elements[SchemaElementIndex]);

			FObjectElement SubElement;
			GetObjectFromVector(
				OutObject,
				SubElement,
				Schema,
				Parameters.Elements[SchemaElementIndex],
				ActionVector.Slice(0, SubElementSize));

			OutObjectElement = OutObject.CreateOrExclusive({ Parameters.ElementNames[SchemaElementIndex], SubElement }, SchemaElementTag);
			return;
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);

			// Find total sub-element size

			const int32 TotalSubElementSize = Private::GetTotalActionVectorSize(Schema, Parameters.Elements);

			// Create sub-elements

			TArray<FName, TInlineAllocator<8>> SubElementNames;
			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElementNames.Reserve(Parameters.Elements.Num());
			SubElements.Reserve(Parameters.Elements.Num());

			int32 SubElementOffset = 0;
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				const int32 SubElementSize = Schema.GetActionVectorSize(Parameters.Elements[SubElementIdx]);

				UE_LEARNING_CHECK(ActionVector[TotalSubElementSize + SubElementIdx] == 0.0f || ActionVector[TotalSubElementSize + SubElementIdx] == 1.0f);
				if (ActionVector[TotalSubElementSize + SubElementIdx])
				{
					FObjectElement SubElement;
					GetObjectFromVector(
						OutObject,
						SubElement,
						Schema,
						Parameters.Elements[SubElementIdx],
						ActionVector.Slice(SubElementOffset, SubElementSize));

					SubElementNames.Add(Parameters.ElementNames[SubElementIdx]);
					SubElements.Add(SubElement);
				}

				SubElementOffset += SubElementSize;
			}
			UE_LEARNING_CHECK(SubElementOffset + Parameters.Elements.Num() == ActionVectorSize);

			OutObjectElement = OutObject.CreateOrInclusive({ SubElementNames, SubElements }, SchemaElementTag);
			return;
		}

		case EType::Array:
		{
			const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);

			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElements.SetNumUninitialized(Parameters.Num);

			// Create sub-elements

			const int32 SubElementSize = Schema.GetActionVectorSize(Parameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < Parameters.Num; ElementIdx++)
			{
				GetObjectFromVector(
					OutObject,
					SubElements[ElementIdx],
					Schema,
					Parameters.Element,
					ActionVector.Slice(ElementIdx * SubElementSize, SubElementSize));
			}

			OutObjectElement = OutObject.CreateArray({ SubElements }, SchemaElementTag);
			return;
		}

		case EType::Encoding:
		{
			const FSchemaEncodingParameters Parameters = Schema.GetEncoding(SchemaElement);

			FObjectElement SubElement;
			GetObjectFromVector(
				OutObject,
				SubElement,
				Schema,
				Parameters.Element,
				ActionVector);

			OutObjectElement = OutObject.CreateEncoding({ SubElement }, SchemaElementTag);
			return;
		}

		default:
		{
			UE_LEARNING_NOT_IMPLEMENTED();
			OutObjectElement = FObjectElement();
			return;
		}
		}
	}
}