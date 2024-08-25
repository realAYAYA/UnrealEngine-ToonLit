// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningObservation.h"

#include "NNERuntimeBasicCpuBuilder.h"

namespace UE::Learning::Observation
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

		static inline int32 GetMaxObservationVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size = FMath::Max(Size, Schema.GetObservationVectorSize(SubElement));
			}
			return Size;
		}

		static inline int32 GetTotalObservationVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size += Schema.GetObservationVectorSize(SubElement);
			}
			return Size;
		}

		static inline int32 GetTotalEncodedObservationVectorSize(const FSchema& Schema, const TArrayView<const FSchemaElement> Elements)
		{
			int32 Size = 0;
			for (const FSchemaElement SubElement : Elements)
			{
				Size += Schema.GetEncodedVectorSize(SubElement);
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
	}

	FSchemaElement FSchema::CreateNull(const FName Tag)
	{
		const int32 Index = Types.Add(EType::Null);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(0);
		EncodedVectorSizes.Add(0);
		TypeDataIndices.Add(INDEX_NONE);

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateContinuous(const FSchemaContinuousParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.Num >= 0);

		FContinuousData ElementData;
		ElementData.Num = Parameters.Num;

		const int32 Index = Types.Add(EType::Continuous);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Parameters.Num);
		EncodedVectorSizes.Add(Parameters.Num);
		TypeDataIndices.Add(ContinuousData.Add(ElementData));

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
		ObservationVectorSizes.Add(Private::GetTotalObservationVectorSize(*this, Parameters.Elements));
		EncodedVectorSizes.Add(Private::GetTotalEncodedObservationVectorSize(*this, Parameters.Elements));
		TypeDataIndices.Add(AndData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateOrExclusive(const FSchemaOrExclusiveParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		UE_LEARNING_CHECK(!Private::ContainsDuplicates(Parameters.ElementNames));
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));

		FOrExclusiveData ElementData;
		ElementData.Num = Parameters.Elements.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();
		ElementData.EncodingSize = Parameters.EncodingSize;

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		const int32 Index = Types.Add(EType::OrExclusive);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Private::GetMaxObservationVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		EncodedVectorSizes.Add(Parameters.EncodingSize + Parameters.Elements.Num());
		TypeDataIndices.Add(OrExclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateOrInclusive(const FSchemaOrInclusiveParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.Elements.Num() == Parameters.ElementNames.Num());
		UE_LEARNING_CHECK(!Private::ContainsDuplicates(Parameters.ElementNames));
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));

		FOrInclusiveData ElementData;
		ElementData.Num = Parameters.Elements.Num();
		ElementData.ElementsOffset = SubElementObjects.Num();
		ElementData.AttentionEncodingSize = Parameters.AttentionEncodingSize;
		ElementData.AttentionHeadNum = Parameters.AttentionHeadNum;
		ElementData.ValueEncodingSize = Parameters.ValueEncodingSize;

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		const int32 Index = Types.Add(EType::OrInclusive);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(Private::GetTotalObservationVectorSize(*this, Parameters.Elements) + Parameters.Elements.Num());
		EncodedVectorSizes.Add(Parameters.AttentionHeadNum * Parameters.ValueEncodingSize + Parameters.Elements.Num());
		TypeDataIndices.Add(OrInclusiveData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateArray(const FSchemaArrayParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Parameters.Num >= 0);
		UE_LEARNING_CHECK(IsValid(Parameters.Element));

		FArrayData ElementData;
		ElementData.Num = Parameters.Num;
		ElementData.ElementIndex = SubElementObjects.Num();

		SubElementNames.Add(NAME_None);
		SubElementObjects.Add(Parameters.Element);

		const int32 Index = Types.Add(EType::Array);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(GetObservationVectorSize(Parameters.Element) * Parameters.Num);
		EncodedVectorSizes.Add(GetEncodedVectorSize(Parameters.Element) * Parameters.Num);
		TypeDataIndices.Add(ArrayData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateSet(const FSchemaSetParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(IsValid(Parameters.Element));

		FSetData ElementData;
		ElementData.MaxNum = Parameters.MaxNum;
		ElementData.ElementIndex = SubElementObjects.Num();
		ElementData.AttentionEncodingSize = Parameters.AttentionEncodingSize;
		ElementData.AttentionHeadNum = Parameters.AttentionHeadNum;
		ElementData.ValueEncodingSize = Parameters.ValueEncodingSize;

		SubElementNames.Add(NAME_None);
		SubElementObjects.Add(Parameters.Element);

		const int32 Index = Types.Add(EType::Set);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(GetObservationVectorSize(Parameters.Element) * Parameters.MaxNum + Parameters.MaxNum);
		EncodedVectorSizes.Add(Parameters.ValueEncodingSize * Parameters.AttentionHeadNum + 1);
		TypeDataIndices.Add(SetData.Add(ElementData));

		return { Index, Generation };
	}

	FSchemaElement FSchema::CreateEncoding(const FSchemaEncodingParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(IsValid(Parameters.Element));

		FEncodingData ElementData;
		ElementData.ElementIndex = SubElementObjects.Num();
		ElementData.EncodingSize = Parameters.EncodingSize;
		ElementData.LayerNum = Parameters.LayerNum;
		ElementData.ActivationFunction = Parameters.ActivationFunction;

		SubElementNames.Add(NAME_None);
		SubElementObjects.Add(Parameters.Element);

		const int32 Index = Types.Add(EType::Encoding);
		Tags.Add(Tag);
		ObservationVectorSizes.Add(GetObservationVectorSize(Parameters.Element));
		EncodedVectorSizes.Add(Parameters.EncodingSize);
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

	int32 FSchema::GetObservationVectorSize(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element));
		return ObservationVectorSizes[Element.Index];
	}

	int32 FSchema::GetEncodedVectorSize(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element));
		return EncodedVectorSizes[Element.Index];
	}

	FSchemaContinuousParameters FSchema::GetContinuous(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Continuous);

		FSchemaContinuousParameters Parameters;
		Parameters.Num = ContinuousData[TypeDataIndices[Element.Index]].Num;
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
		Parameters.EncodingSize = ElementData.EncodingSize;
		return Parameters;
	}

	FSchemaOrInclusiveParameters FSchema::GetOrInclusive(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::OrInclusive);
		const FOrInclusiveData& ElementData = OrInclusiveData[TypeDataIndices[Element.Index]];

		FSchemaOrInclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.Elements = TArrayView<const FSchemaElement>(SubElementObjects.GetData() + ElementData.ElementsOffset, ElementData.Num);
		Parameters.AttentionEncodingSize = ElementData.AttentionEncodingSize;
		Parameters.AttentionHeadNum = ElementData.AttentionHeadNum;
		Parameters.ValueEncodingSize = ElementData.ValueEncodingSize;
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

	FSchemaSetParameters FSchema::GetSet(const FSchemaElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Set);
		const FSetData& ElementData = SetData[TypeDataIndices[Element.Index]];

		FSchemaSetParameters Parameters;
		Parameters.MaxNum = ElementData.MaxNum;
		Parameters.Element = SubElementObjects[ElementData.ElementIndex];
		Parameters.AttentionEncodingSize = ElementData.AttentionEncodingSize;
		Parameters.AttentionHeadNum = ElementData.AttentionHeadNum;
		Parameters.ValueEncodingSize = ElementData.ValueEncodingSize;
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
		ObservationVectorSizes.Empty();
		EncodedVectorSizes.Empty();
		TypeDataIndices.Empty();

		ContinuousData.Empty();
		AndData.Empty();
		OrExclusiveData.Empty();
		OrInclusiveData.Empty();
		ArrayData.Empty();
		SetData.Empty();
		EncodingData.Empty();

		SubElementNames.Empty();
		SubElementObjects.Empty();

		Generation++;
	}

	void FSchema::Reset()
	{
		Types.Reset();
		Tags.Reset();
		ObservationVectorSizes.Reset();
		EncodedVectorSizes.Reset();
		TypeDataIndices.Reset();

		ContinuousData.Reset();
		AndData.Reset();
		OrExclusiveData.Reset();
		OrInclusiveData.Reset();
		ArrayData.Reset();
		SetData.Reset();
		EncodingData.Reset();

		SubElementNames.Reset();
		SubElementObjects.Reset();

		Generation++;
	}

	FObjectElement FObject::CreateNull(const FName Tag)
	{
		const int32 Index = Types.Add(EType::Null);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(0);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateContinuous(const FObjectContinuousParameters Parameters, const FName Tag)
	{
		const int32 Index = Types.Add(EType::Continuous);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(Parameters.Values.Num());

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(0);

		ContinuousValues.Append(Parameters.Values);

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

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(Parameters.Elements.Num());

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateOrExclusive(const FObjectOrExclusiveParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(IsValid(Parameters.Element));

		const int32 Index = Types.Add(EType::OrExclusive);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(1);

		SubElementNames.Add(Parameters.ElementName);
		SubElementObjects.Add(Parameters.Element);

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

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(Parameters.Elements.Num());

		SubElementNames.Append(Parameters.ElementNames);
		SubElementObjects.Append(Parameters.Elements);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateArray(const FObjectArrayParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));

		const int32 Index = Types.Add(EType::Array);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(Parameters.Elements.Num());

		for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
		{
			SubElementNames.Add(NAME_Name);
		}
		SubElementObjects.Append(Parameters.Elements);

		return { Index, Generation };
	}

	FObjectElement FObject::CreateSet(const FObjectSetParameters Parameters, const FName Tag)
	{
		UE_LEARNING_CHECK(Private::CheckAllValid(*this, Parameters.Elements));

		const int32 Index = Types.Add(EType::Set);
		Tags.Add(Tag);

		ContinuousDataOffsets.Add(ContinuousValues.Num());
		ContinuousDataNums.Add(0);

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(Parameters.Elements.Num());

		for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
		{
			SubElementNames.Add(NAME_Name);
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

		SubElementDataOffsets.Add(SubElementObjects.Num());
		SubElementDataNums.Add(1);

		SubElementNames.Add(NAME_Name);
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

	FObjectAndParameters FObject::GetAnd(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::And);

		FObjectAndParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectOrExclusiveParameters FObject::GetOrExclusive(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::OrExclusive);

		FObjectOrExclusiveParameters Parameters;
		Parameters.ElementName = SubElementNames[SubElementDataOffsets[Element.Index]];
		Parameters.Element = SubElementObjects[SubElementDataOffsets[Element.Index]];
		return Parameters;
	}

	FObjectOrInclusiveParameters FObject::GetOrInclusive(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::OrInclusive);

		FObjectOrInclusiveParameters Parameters;
		Parameters.ElementNames = TArrayView<const FName>(SubElementNames.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectArrayParameters FObject::GetArray(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Array);

		FObjectArrayParameters Parameters;
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectSetParameters FObject::GetSet(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Set);

		FObjectSetParameters Parameters;
		Parameters.Elements = TArrayView<const FObjectElement>(SubElementObjects.GetData() + SubElementDataOffsets[Element.Index], SubElementDataNums[Element.Index]);
		return Parameters;
	}

	FObjectEncodingParameters FObject::GetEncoding(const FObjectElement Element) const
	{
		UE_LEARNING_CHECK(IsValid(Element) && GetType(Element) == EType::Encoding);

		FObjectEncodingParameters Parameters;
		Parameters.Element = SubElementObjects[SubElementDataOffsets[Element.Index]];
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

		SubElementDataOffsets.Empty();
		SubElementDataNums.Empty();

		ContinuousValues.Empty();
		SubElementNames.Empty();
		SubElementObjects.Empty();

		Generation++;
	}

	void FObject::Reset()
	{
		Types.Reset();
		Tags.Reset();

		ContinuousDataOffsets.Reset();
		ContinuousDataNums.Reset();

		SubElementDataOffsets.Reset();
		SubElementDataNums.Reset();

		ContinuousValues.Reset();
		SubElementNames.Reset();
		SubElementObjects.Reset();

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

		NNE::RuntimeBasic::FModelBuilderElement MakeEncoderNetworkFromSchema(
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
				const int32 ValueNum = Schema.GetContinuous(SchemaElement).Num;

				ReturnElement = Builder.MakeDenormalize(
					ValueNum,
					Builder.MakeWeightsZero(ValueNum),
					Builder.MakeWeightsConstant(ValueNum, 1.0f));
				break;
			}

			case EType::And:
			{
				const FSchemaAndParameters Parameters = Schema.GetAnd(SchemaElement);

				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderLayers;
				BuilderLayers.Reserve(Parameters.Elements.Num());
				for (const FSchemaElement SubElement : Parameters.Elements)
				{
					BuilderLayers.Emplace(MakeEncoderNetworkFromSchema(Builder, Schema, SubElement));
				}

				ReturnElement = Builder.MakeConcat(BuilderLayers);
				break;
			}

			case EType::OrExclusive:
			{
				const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);

				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderSubLayers;
				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderEncoders;
				BuilderSubLayers.Reserve(Parameters.Elements.Num());
				BuilderEncoders.Reserve(Parameters.Elements.Num());
				for (const FSchemaElement SubElement : Parameters.Elements)
				{
					const int32 SubElementEncodedSize = Schema.GetEncodedVectorSize(SubElement);
					BuilderSubLayers.Emplace(MakeEncoderNetworkFromSchema(Builder, Schema, SubElement));
					BuilderEncoders.Emplace(Builder.MakeLinearWithRandomKaimingWeights(SubElementEncodedSize, Parameters.EncodingSize));
				}

				ReturnElement = Builder.MakeAggregateOrExclusive(Parameters.EncodingSize, BuilderSubLayers, BuilderEncoders);
				break;
			}

			case EType::OrInclusive:
			{
				const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);

				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderSubLayers;
				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderQueryLayers;
				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderKeyLayers;
				TArray<NNE::RuntimeBasic::FModelBuilderElement, TInlineAllocator<8>> BuilderValueLayers;
				BuilderSubLayers.Reserve(Parameters.Elements.Num());
				BuilderQueryLayers.Reserve(Parameters.Elements.Num());
				BuilderValueLayers.Reserve(Parameters.Elements.Num());
				for (const FSchemaElement SubElement : Parameters.Elements)
				{
					const int32 SubElementEncodedSize = Schema.GetEncodedVectorSize(SubElement);
					BuilderSubLayers.Emplace(MakeEncoderNetworkFromSchema(Builder, Schema, SubElement));
					BuilderQueryLayers.Emplace(Builder.MakeLinearWithRandomKaimingWeights(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.AttentionEncodingSize));
					BuilderKeyLayers.Emplace(Builder.MakeLinearWithRandomKaimingWeights(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.AttentionEncodingSize));
					BuilderValueLayers.Emplace(Builder.MakeLinearWithRandomKaimingWeights(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.ValueEncodingSize));
				}

				ReturnElement = Builder.MakeAggregateOrInclusive(
					Parameters.ValueEncodingSize,
					Parameters.AttentionEncodingSize,
					Parameters.AttentionHeadNum,
					BuilderSubLayers,
					BuilderQueryLayers,
					BuilderKeyLayers,
					BuilderValueLayers);

				break;
			}

			case EType::Array:
			{
				const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);

				ReturnElement = Builder.MakeArray(Parameters.Num, MakeEncoderNetworkFromSchema(Builder, Schema, Parameters.Element));
				break;
			}

			case EType::Set:
			{
				const FSchemaSetParameters Parameters = Schema.GetSet(SchemaElement);

				const int32 SubElementEncodedSize = Schema.GetEncodedVectorSize(Parameters.Element);

				ReturnElement = Builder.MakeAggregateSet(
					Parameters.MaxNum,
					Parameters.ValueEncodingSize,
					Parameters.AttentionEncodingSize,
					Parameters.AttentionHeadNum,
					MakeEncoderNetworkFromSchema(Builder, Schema, Parameters.Element),
					Builder.MakeLinearWithRandomKaimingWeights(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.AttentionEncodingSize),
					Builder.MakeLinearWithRandomKaimingWeights(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.AttentionEncodingSize),
					Builder.MakeLinearWithRandomKaimingWeights(SubElementEncodedSize, Parameters.AttentionHeadNum * Parameters.ValueEncodingSize));
				break;
			}

			case EType::Encoding:
			{
				const FSchemaEncodingParameters Parameters = Schema.GetEncoding(SchemaElement);

				const int32 SubElementEncodedSize = Schema.GetEncodedVectorSize(Parameters.Element);

				ReturnElement = Builder.MakeSequence({
					MakeEncoderNetworkFromSchema(Builder, Schema, Parameters.Element),
					Builder.MakeMLPWithRandomKaimingWeights(
						SubElementEncodedSize,
						Parameters.EncodingSize,
						Parameters.EncodingSize,
						Parameters.LayerNum + 1, // Add 1 to account for input layer
						GetNNEActivationFunction(Parameters.ActivationFunction),
						true)
					});
				break;
			}

			default:
			{
				UE_LEARNING_NOT_IMPLEMENTED();
			}
			}

			UE_LEARNING_CHECKF(ReturnElement.GetInputSize() == Schema.GetObservationVectorSize(SchemaElement),
				TEXT("Encoder Network Input unexpected size for %s. Got %i, expected %i according to Schema."),
				*Schema.GetTag(SchemaElement).ToString(), ReturnElement.GetInputSize(), Schema.GetObservationVectorSize(SchemaElement));

			UE_LEARNING_CHECKF(ReturnElement.GetOutputSize() == Schema.GetEncodedVectorSize(SchemaElement),
				TEXT("Encoder Network Output unexpected size for %s. Got %i, expected %i according to Schema."),
				*Schema.GetTag(SchemaElement).ToString(), ReturnElement.GetOutputSize(), Schema.GetEncodedVectorSize(SchemaElement));

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

		case EType::Set:
		{
			const FSchemaSetParameters Parameters = Schema.GetSet(SchemaElement);
			return Private::HashCombine({ Hash, Private::HashInt(Parameters.MaxNum), GetSchemaObjectsCompatibilityHash(Schema, Parameters.Element, Salt) });
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
		if (SchemaA.GetObservationVectorSize(SchemaElementA) != SchemaB.GetObservationVectorSize(SchemaElementB)) { return false; }

		switch (SchemaElementTypeA)
		{
		case EType::Null: return true;

		case EType::Continuous: return SchemaA.GetContinuous(SchemaElementA).Num == SchemaB.GetContinuous(SchemaElementB).Num;

		case EType::And:
		{
			const FSchemaAndParameters ParametersA = SchemaA.GetAnd(SchemaElementA);
			const FSchemaAndParameters ParametersB = SchemaA.GetAnd(SchemaElementB);

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
			const FSchemaOrExclusiveParameters ParametersB = SchemaA.GetOrExclusive(SchemaElementB);

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
			const FSchemaOrInclusiveParameters ParametersB = SchemaA.GetOrInclusive(SchemaElementB);

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
			const FSchemaArrayParameters ParametersB = SchemaA.GetArray (SchemaElementB);

			return (ParametersA.Num == ParametersB.Num) && AreSchemaObjectsCompatible(SchemaA, ParametersA.Element, SchemaB, ParametersB.Element);
		}

		case EType::Set:
		{
			const FSchemaSetParameters ParametersA = SchemaA.GetSet(SchemaElementA);
			const FSchemaSetParameters ParametersB = SchemaA.GetSet(SchemaElementB);

			return (ParametersA.MaxNum == ParametersB.MaxNum) && AreSchemaObjectsCompatible(SchemaA, ParametersA.Element, SchemaB, ParametersB.Element);
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

	void GenerateEncoderNetworkFileDataFromSchema(
		TArray<uint8>& OutFileData,
		uint32& OutInputSize,
		uint32& OutOutputSize,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const uint32 Seed)
	{
		UE_LEARNING_CHECK(Schema.IsValid(SchemaElement));

		NNE::RuntimeBasic::FModelBuilder Builder(Seed);
		Builder.WriteFileDataAndReset(OutFileData, OutInputSize, OutOutputSize, Private::MakeEncoderNetworkFromSchema(Builder, Schema, SchemaElement));
	}

	void SetVectorFromObject(
		TLearningArrayView<1, float> OutObservationVector,
		const FSchema& Schema,
		const FSchemaElement SchemaElement,
		const FObject& Object,
		const FObjectElement ObjectElement)
	{
		UE_LEARNING_CHECK(Schema.IsValid(SchemaElement));
		UE_LEARNING_CHECK(Object.IsValid(ObjectElement));
		UE_LEARNING_CHECK(OutObservationVector.Num() == Schema.GetObservationVectorSize(SchemaElement));

		// Check that the types match

		const EType SchemaElementType = Schema.GetType(SchemaElement);
		const EType ObjectElementType = Object.GetType(ObjectElement);
		UE_LEARNING_CHECK(ObjectElementType == SchemaElementType);

		// Zero Observation Vector

		Array::Zero(OutObservationVector);

		// Logic for each specific element type

		switch (SchemaElementType)
		{

		case EType::Null: return;

		case EType::Continuous:
		{
			// Check the input sizes match

			const TArrayView<const float> ObservationValues = Object.GetContinuous(ObjectElement).Values;
			UE_LEARNING_CHECK(Schema.GetObservationVectorSize(SchemaElement) == ObservationValues.Num());
			UE_LEARNING_CHECK(Schema.GetObservationVectorSize(SchemaElement) == OutObservationVector.Num());

			// Copy in the values from the observation object

			Array::Copy<1, float>(OutObservationVector, ObservationValues);
			return;
		}

		case EType::And:
		{
			// Check the number of sub-elements match

			const FSchemaAndParameters SchemaParameters = Schema.GetAnd(SchemaElement);
			const FObjectAndParameters ObjectParameters = Object.GetAnd(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.Elements.Num() == ObjectParameters.Elements.Num());

			// Update Sub-elements

			int32 SubElementOffset = 0;

			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaParameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 ObjectElementIndex = ObjectParameters.ElementNames.Find(SchemaParameters.ElementNames[SchemaElementIdx]);
				UE_LEARNING_CHECK(ObjectElementIndex != INDEX_NONE);

				const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Elements[SchemaElementIdx]);

				SetVectorFromObject(
					OutObservationVector.Slice(SubElementOffset, SubElementSize),
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Object,
					ObjectParameters.Elements[ObjectElementIndex]);

				SubElementOffset += SubElementSize;
			}

			UE_LEARNING_CHECK(SubElementOffset == OutObservationVector.Num());
			return;
		}

		case EType::OrExclusive:
		{
			// Check only one sub-element is given and index is valid

			const FSchemaOrExclusiveParameters SchemaParameters = Schema.GetOrExclusive(SchemaElement);
			const FObjectOrExclusiveParameters ObjectParameters = Object.GetOrExclusive(ObjectElement);

			const int32 SchemaElementIndex = SchemaParameters.ElementNames.Find(ObjectParameters.ElementName);
			UE_LEARNING_CHECK(SchemaElementIndex != INDEX_NONE);

			// Update sub-element

			const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Elements[SchemaElementIndex]);

			SetVectorFromObject(
				OutObservationVector.Slice(0, SubElementSize),
				Schema,
				SchemaParameters.Elements[SchemaElementIndex],
				Object,
				ObjectParameters.Element);

			// Set Mask

			const int32 MaxSubElementSize = Private::GetMaxObservationVectorSize(Schema, SchemaParameters.Elements);

			OutObservationVector[MaxSubElementSize + SchemaElementIndex] = 1.0f;

			UE_LEARNING_CHECK(OutObservationVector.Num() == MaxSubElementSize + SchemaParameters.Elements.Num());
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

			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaParameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Elements[SchemaElementIdx]);
				const int32 ObjectElementIdx = ObjectParameters.ElementNames.Find(SchemaParameters.ElementNames[SchemaElementIdx]);

				if (ObjectElementIdx != INDEX_NONE)
				{
					SetVectorFromObject(
						OutObservationVector.Slice(SubElementOffset, SubElementSize),
						Schema,
						SchemaParameters.Elements[SchemaElementIdx],
						Object,
						ObjectParameters.Elements[ObjectElementIdx]);
				}

				SubElementOffset += SubElementSize;
			}

			// Set Mask

			UE_LEARNING_CHECK(SubElementOffset + SchemaParameters.Elements.Num() == OutObservationVector.Num());

			for (int32 ObjectElementIdx = 0; ObjectElementIdx < ObjectParameters.Elements.Num(); ObjectElementIdx++)
			{
				const int32 SchemaElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementNames[ObjectElementIdx]);
				UE_LEARNING_CHECK(SchemaElementIdx != INDEX_NONE);

				OutObservationVector[SubElementOffset + SchemaElementIdx] = 1.0f;
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

			const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < SchemaParameters.Num; ElementIdx++)
			{
				SetVectorFromObject(
					OutObservationVector.Slice(ElementIdx * SubElementSize, SubElementSize),
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx]);
			}

			return;
		}

		case EType::Set:
		{
			// Check number of set elements is correct

			const FSchemaSetParameters SchemaParameters = Schema.GetSet(SchemaElement);
			const FObjectSetParameters ObjectParameters = Object.GetSet(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.MaxNum >= ObjectParameters.Elements.Num());

			// Update sub-elements

			int32 SubElementOffset = 0;

			const int32 SubElementSize = Schema.GetObservationVectorSize(SchemaParameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < ObjectParameters.Elements.Num(); ElementIdx++)
			{
				SetVectorFromObject(
					OutObservationVector.Slice(SubElementOffset, SubElementSize),
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx]);

				SubElementOffset += SubElementSize;
			}

			SubElementOffset = SubElementSize * SchemaParameters.MaxNum;

			// Set Mask

			Array::Set(OutObservationVector.Slice(SubElementOffset, ObjectParameters.Elements.Num()), 1.0f);

			UE_LEARNING_CHECK(SubElementOffset + SchemaParameters.MaxNum == OutObservationVector.Num());
			return;
		}

		case EType::Encoding:
		{
			const FSchemaEncodingParameters SchemaParameters = Schema.GetEncoding(SchemaElement);
			const FObjectEncodingParameters ObjectParameters = Object.GetEncoding(ObjectElement);

			SetVectorFromObject(
				OutObservationVector,
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
		const TLearningArrayView<1, const float> ObservationVector)
	{
		UE_LEARNING_CHECK(Schema.IsValid(SchemaElement));

		// Check that the types match

		const EType SchemaElementType = Schema.GetType(SchemaElement);
		const FName SchemaElementTag = Schema.GetTag(SchemaElement);

		// Get Observation Vector Size

		const int32 ObservationVectorSize = ObservationVector.Num();
		UE_LEARNING_CHECK(ObservationVectorSize == Schema.GetObservationVectorSize(SchemaElement));

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
			UE_LEARNING_CHECK(ObservationVectorSize == Schema.GetContinuous(SchemaElement).Num);

			OutObjectElement = OutObject.CreateContinuous({ MakeArrayView(ObservationVector.GetData(), ObservationVector.Num()) }, SchemaElementTag);
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
				const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Elements[SchemaElementIdx]);

				GetObjectFromVector(
					OutObject,
					SubElements[SchemaElementIdx],
					Schema,
					Parameters.Elements[SchemaElementIdx],
					ObservationVector.Slice(SubElementOffset, SubElementSize));

				SubElementOffset += SubElementSize;
			}
			UE_LEARNING_CHECK(SubElementOffset == ObservationVectorSize);

			OutObjectElement = OutObject.CreateAnd({ Parameters.ElementNames, SubElements }, SchemaElementTag);
			return;
		}

		case EType::OrExclusive:
		{
			const FSchemaOrExclusiveParameters Parameters = Schema.GetOrExclusive(SchemaElement);

			// Find active element

			const int32 MaxSubElementSize = Private::GetMaxObservationVectorSize(Schema, Parameters.Elements);

			int32 SchemaElementIndex = INDEX_NONE;
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				UE_LEARNING_CHECK(ObservationVector[MaxSubElementSize + SubElementIdx] == 0.0f || ObservationVector[MaxSubElementSize + SubElementIdx] == 1.0f);
				if (ObservationVector[MaxSubElementSize + SubElementIdx])
				{
					SchemaElementIndex = SubElementIdx;
					break;
				}
			}
			UE_LEARNING_CHECK(SchemaElementIndex != INDEX_NONE);

			// Create sub-element

			const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Elements[SchemaElementIndex]);

			FObjectElement SubElement;
			GetObjectFromVector(
				OutObject,
				SubElement,
				Schema,
				Parameters.Elements[SchemaElementIndex],
				ObservationVector.Slice(0, SubElementSize));

			OutObjectElement = OutObject.CreateOrExclusive({ Parameters.ElementNames[SchemaElementIndex], SubElement }, SchemaElementTag);
			return;
		}

		case EType::OrInclusive:
		{
			const FSchemaOrInclusiveParameters Parameters = Schema.GetOrInclusive(SchemaElement);

			// Find total sub-element size

			const int32 TotalSubElementSize = Private::GetTotalObservationVectorSize(Schema, Parameters.Elements);

			// Create sub-elements

			TArray<FName, TInlineAllocator<8>> SubElementNames;
			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElementNames.Reserve(Parameters.Elements.Num());
			SubElements.Reserve(Parameters.Elements.Num());

			int32 SubElementOffset = 0;
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Elements[SubElementIdx]);

				UE_LEARNING_CHECK(
					ObservationVector[TotalSubElementSize + SubElementIdx] == 0.0f || 
					ObservationVector[TotalSubElementSize + SubElementIdx] == 1.0f);

				if (ObservationVector[TotalSubElementSize + SubElementIdx] == 1.0f)
				{
					FObjectElement SubElement;
					GetObjectFromVector(
						OutObject,
						SubElement,
						Schema,
						Parameters.Elements[SubElementIdx],
						ObservationVector.Slice(SubElementOffset, SubElementSize));

					SubElementNames.Add(Parameters.ElementNames[SubElementIdx]);
					SubElements.Add(SubElement);
				}

				SubElementOffset += SubElementSize;
			}
			UE_LEARNING_CHECK(SubElementOffset + Parameters.Elements.Num() == ObservationVectorSize);

			OutObjectElement = OutObject.CreateOrInclusive({ SubElementNames, SubElements }, SchemaElementTag);
			return;
		}

		case EType::Array:
		{
			const FSchemaArrayParameters Parameters = Schema.GetArray(SchemaElement);

			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElements.SetNumUninitialized(Parameters.Num);

			// Create sub-elements

			const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Element);

			for (int32 ElementIdx = 0; ElementIdx < Parameters.Num; ElementIdx++)
			{
				GetObjectFromVector(
					OutObject,
					SubElements[ElementIdx],
					Schema,
					Parameters.Element,
					ObservationVector.Slice(ElementIdx * SubElementSize, SubElementSize));
			}

			OutObjectElement = OutObject.CreateArray({ SubElements }, SchemaElementTag);
			return;
		}

		case EType::Set:
		{
			const FSchemaSetParameters Parameters = Schema.GetSet(SchemaElement);

			const int32 SubElementSize = Schema.GetObservationVectorSize(Parameters.Element);

			// Create sub-elements

			TArray<FObjectElement, TInlineAllocator<8>> SubElements;
			SubElements.Reserve(Parameters.MaxNum);

			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.MaxNum; SubElementIdx++)
			{
				UE_LEARNING_CHECK(
					ObservationVector[SubElementSize * Parameters.MaxNum + SubElementIdx] == 0.0f || 
					ObservationVector[SubElementSize * Parameters.MaxNum + SubElementIdx] == 1.0f);

				if (ObservationVector[SubElementSize * Parameters.MaxNum + SubElementIdx] == 0.0f)
				{
					break;
				}

				FObjectElement SubElement;

				GetObjectFromVector(
					OutObject,
					SubElement,
					Schema,
					Parameters.Element,
					ObservationVector.Slice(SubElementIdx * SubElementSize, SubElementSize));

				SubElements.Add(SubElement);
			}

			OutObjectElement = OutObject.CreateSet({ SubElements }, SchemaElementTag);
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
				ObservationVector);

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