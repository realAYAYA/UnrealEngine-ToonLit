// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningAgentsObservations.h"

#include "LearningAgentsManagerListener.h"
#include "LearningAgentsDebug.h"

#include "LearningArray.h"
#include "LearningLog.h"

#include "Containers/StaticArray.h"

#include "Runtime/Engine/Public/DrawDebugHelpers.h"
#include "Engine/World.h"
#include "Components/SplineComponent.h"

bool operator==(const FLearningAgentsObservationObjectElement& Lhs, const FLearningAgentsObservationObjectElement& Rhs)
{
	return Lhs.ObjectElement.Index == Rhs.ObjectElement.Index;
}

uint32 GetTypeHash(const FLearningAgentsObservationObjectElement& Element)
{
	return (uint32)Element.ObjectElement.Index;
}

namespace UE::Learning::Agents::Observation::Private
{
	static inline bool ContainsDuplicates(const TArrayView<const int32> Indices)
	{
		TSet<int32, DefaultKeyFuncs<int32>, TInlineSetAllocator<32>> IndicesSet;
		IndicesSet.Append(Indices);
		return Indices.Num() != IndicesSet.Num();
	}

	static inline bool ContainsDuplicates(const TArrayView<const FName> ElementNames)
	{
		TSet<FName, DefaultKeyFuncs<FName>, TInlineSetAllocator<32>> ElementNameSet;
		ElementNameSet.Append(ElementNames);
		return ElementNames.Num() != ElementNameSet.Num();
	}

	static inline const TCHAR* GetObservationTypeString(const Learning::Observation::EType ObservationType)
	{
		switch (ObservationType)
		{
		case  Learning::Observation::EType::Null: return TEXT("Null");
		case  Learning::Observation::EType::Continuous: return TEXT("Continuous");
		case  Learning::Observation::EType::And: return TEXT("Struct");
		case  Learning::Observation::EType::OrExclusive: return TEXT("ExclusiveUnion");
		case  Learning::Observation::EType::OrInclusive: return TEXT("InclusiveUnion");
		case  Learning::Observation::EType::Array: return TEXT("StaticArray");
		case  Learning::Observation::EType::Set: return TEXT("Set");
		case  Learning::Observation::EType::Encoding: return TEXT("Encoding");
		default:
			UE_LEARNING_NOT_IMPLEMENTED();
			return TEXT("Unimplemented");
		}
	}

	static bool ValidateObservationObjectMatchesSchema(
		const Learning::Observation::FSchema& Schema,
		const Learning::Observation::FSchemaElement SchemaElement,
		const Learning::Observation::FObject& Object,
		const Learning::Observation::FObjectElement ObjectElement)
	{
		// Check Elements are Valid

		if (!Schema.IsValid(SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Invalid Observation Schema Element."));
			return false;
		}

		if (!Object.IsValid(ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Invalid Observation Object Element."));
			return false;
		}

		// Check Names Match

		const FName ObservationSchemaElementTag = Schema.GetTag(SchemaElement);
		const FName ObservationObjectElementTag = Object.GetTag(ObjectElement);

		if (ObservationSchemaElementTag != ObservationObjectElementTag)
		{
			UE_LOG(LogLearning, Warning, TEXT("ValidateObservationObjectMatchesSchema: Observation tag does not match Schema. Expected '%s', got '%s'."),
				*ObservationSchemaElementTag.ToString(), *ObservationObjectElementTag.ToString());
		}

		// Check Types Match

		const Learning::Observation::EType ObservationSchemaElementType = Schema.GetType(SchemaElement);
		const Learning::Observation::EType ObservationObjectElementType = Object.GetType(ObjectElement);

		if (ObservationSchemaElementType != ObservationObjectElementType)
		{
			UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Observation '%s' type does not match Schema. Expected type '%s', got type '%s'."),
				*ObservationSchemaElementTag.ToString(),
				GetObservationTypeString(ObservationSchemaElementType),
				GetObservationTypeString(ObservationObjectElementType));
			return false;
		}

		// Type Specific Checks

		switch (ObservationSchemaElementType)
		{
		case Learning::Observation::EType::Null: return true;
		
		case Learning::Observation::EType::Continuous:
		{
			const int32 SchemaElementSize = Schema.GetContinuous(SchemaElement).Num;
			const int32 ObjectElementSize = Object.GetContinuous(ObjectElement).Values.Num();

			if (SchemaElementSize != ObjectElementSize)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Observation '%s' size does not match Schema. Expected '%i', got '%i'."),
					*ObservationSchemaElementTag.ToString(),
					SchemaElementSize,
					ObjectElementSize);
				return false;
			}

			return true;
		}

		case Learning::Observation::EType::And:
		{
			const Learning::Observation::FSchemaAndParameters SchemaParameters = Schema.GetAnd(SchemaElement);
			const Learning::Observation::FObjectAndParameters ObjectParameters = Object.GetAnd(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());
			UE_LEARNING_CHECK(ObjectParameters.Elements.Num() == ObjectParameters.ElementNames.Num());

			if (SchemaParameters.Elements.Num() != ObjectParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Observation '%s' number of sub-elements does not match Schema. Expected '%i', got '%i'."),
					*ObservationSchemaElementTag.ToString(),
					SchemaParameters.Elements.Num(),
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 SchemaElementIdx = 0; SchemaElementIdx < SchemaParameters.Elements.Num(); SchemaElementIdx++)
			{
				const int32 ObjectElementIdx = ObjectParameters.ElementNames.Find(SchemaParameters.ElementNames[SchemaElementIdx]);

				if (ObjectElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Observation '%s' does not include '%s' observation required by Schema."),
						*ObservationSchemaElementTag.ToString(),
						*SchemaParameters.ElementNames[SchemaElementIdx].ToString());
					return false;
				}

				if (!ValidateObservationObjectMatchesSchema(
					Schema,
					SchemaParameters.Elements[SchemaElementIdx],
					Object,
					ObjectParameters.Elements[ObjectElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Observation::EType::OrExclusive:
		{
			const Learning::Observation::FSchemaOrExclusiveParameters SchemaParameters = Schema.GetOrExclusive(SchemaElement);
			const Learning::Observation::FObjectOrExclusiveParameters ObjectParameters = Object.GetOrExclusive(ObjectElement);
			UE_LEARNING_CHECK(SchemaParameters.Elements.Num() == SchemaParameters.ElementNames.Num());

			const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementName);

			if (SchemaSubElementIdx == INDEX_NONE)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Observation '%s' Schema does not include '%s' observation."),
					*ObservationSchemaElementTag.ToString(),
					*ObjectParameters.ElementName.ToString());
				return false;
			}

			return ValidateObservationObjectMatchesSchema(
				Schema,
				SchemaParameters.Elements[SchemaSubElementIdx],
				Object,
				ObjectParameters.Element);
		}

		case Learning::Observation::EType::OrInclusive:
		{
			const Learning::Observation::FSchemaOrInclusiveParameters SchemaParameters = Schema.GetOrInclusive(SchemaElement);
			const Learning::Observation::FObjectOrInclusiveParameters ObjectParameters = Object.GetOrInclusive(ObjectElement);

			if (ObjectParameters.Elements.Num() > SchemaParameters.Elements.Num())
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Observation '%s' too many sub-observations provided. Expected at most '%i', got '%i'."),
					*ObservationSchemaElementTag.ToString(),
					SchemaParameters.Elements.Num(),
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 ObjectSubElementIdx = 0; ObjectSubElementIdx < ObjectParameters.Elements.Num(); ObjectSubElementIdx++)
			{
				const int32 SchemaSubElementIdx = SchemaParameters.ElementNames.Find(ObjectParameters.ElementNames[ObjectSubElementIdx]);

				if (SchemaSubElementIdx == INDEX_NONE)
				{
					UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Observation '%s' Schema does not include '%s' observation."),
						*ObservationSchemaElementTag.ToString(),
						*ObjectParameters.ElementNames[ObjectSubElementIdx].ToString());
					return false;
				}

				if (!ValidateObservationObjectMatchesSchema(
					Schema,
					SchemaParameters.Elements[SchemaSubElementIdx],
					Object,
					ObjectParameters.Elements[ObjectSubElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Observation::EType::Array:
		{
			const Learning::Observation::FSchemaArrayParameters SchemaParameters = Schema.GetArray(SchemaElement);
			const Learning::Observation::FObjectArrayParameters ObjectParameters = Object.GetArray(ObjectElement);

			if (ObjectParameters.Elements.Num() != SchemaParameters.Num)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Observation '%s' array incorrect size. Expected '%i' elements, got '%i'."),
					*ObservationSchemaElementTag.ToString(),
					SchemaParameters.Num,
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 ElementIdx = 0; ElementIdx < ObjectParameters.Elements.Num(); ElementIdx++)
			{
				if (!ValidateObservationObjectMatchesSchema(
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Observation::EType::Set:
		{
			const Learning::Observation::FSchemaSetParameters SchemaParameters = Schema.GetSet(SchemaElement);
			const Learning::Observation::FObjectSetParameters ObjectParameters = Object.GetSet(ObjectElement);

			if (ObjectParameters.Elements.Num() > SchemaParameters.MaxNum)
			{
				UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Observation '%s' set too large. Expected at most '%i' elements, got '%i'."),
					*ObservationSchemaElementTag.ToString(),
					SchemaParameters.MaxNum,
					ObjectParameters.Elements.Num());
				return false;
			}

			for (int32 ElementIdx = 0; ElementIdx < ObjectParameters.Elements.Num(); ElementIdx++)
			{
				if (!ValidateObservationObjectMatchesSchema(
					Schema,
					SchemaParameters.Element,
					Object,
					ObjectParameters.Elements[ElementIdx]))
				{
					return false;
				}
			}

			return true;
		}

		case Learning::Observation::EType::Encoding:
		{
			const Learning::Observation::FSchemaEncodingParameters SchemaParameters = Schema.GetEncoding(SchemaElement);
			const Learning::Observation::FObjectEncodingParameters ObjectParameters = Object.GetEncoding(ObjectElement);

			return ValidateObservationObjectMatchesSchema(
				Schema,
				SchemaParameters.Element,
				Object,
				ObjectParameters.Element);
		}

		default:
		{
			UE_LEARNING_NOT_IMPLEMENTED();
			return true;
		}
		}
	}

	static void LogObservation(
		const UE::Learning::Observation::FObject& Object, 
		const UE::Learning::Observation::FObjectElement ObjectElement,
		const FString& Indentation,
		const FString& Prefix)
	{
		if (!Object.IsValid(ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("LogObservation: Invalid Observation Object Element."));
			return;
		}

		const UE::Learning::Observation::EType Type = Object.GetType(ObjectElement);
		const FName Tag = Object.GetTag(ObjectElement);

		switch (Type)
		{
		case UE::Learning::Observation::EType::Null:
		{
			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetObservationTypeString(Type));
			return;
		}

		case UE::Learning::Observation::EType::Continuous:
		{
			const UE::Learning::Observation::FObjectContinuousParameters Parameters = Object.GetContinuous(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s) %s"), *Indentation, *Prefix, *Tag.ToString(), GetObservationTypeString(Type), *UE::Learning::Array::FormatFloat(Parameters.Values));
			return;
		}

		case UE::Learning::Observation::EType::And:
		{
			const UE::Learning::Observation::FObjectAndParameters Parameters = Object.GetAnd(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetObservationTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogObservation(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementNames[SubElementIdx].ToString()));
			}

			return;
		}

		case UE::Learning::Observation::EType::OrExclusive:
		{
			const UE::Learning::Observation::FObjectOrExclusiveParameters Parameters = Object.GetOrExclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetObservationTypeString(Type));
			LogObservation(Object, Parameters.Element, *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementName.ToString()));

			return;
		}

		case UE::Learning::Observation::EType::OrInclusive:
		{
			const UE::Learning::Observation::FObjectOrInclusiveParameters Parameters = Object.GetOrInclusive(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetObservationTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogObservation(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| \"%s\": "), *Parameters.ElementNames[SubElementIdx].ToString()));
			}

			return;
		}

		case UE::Learning::Observation::EType::Array:
		{
			const UE::Learning::Observation::FObjectArrayParameters Parameters = Object.GetArray(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetObservationTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogObservation(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| %3i:"), SubElementIdx));
			}

			return;
		}

		case UE::Learning::Observation::EType::Set:
		{
			const UE::Learning::Observation::FObjectSetParameters Parameters = Object.GetSet(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetObservationTypeString(Type));
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				LogObservation(Object, Parameters.Elements[SubElementIdx], *(Indentation + TEXT("    ")), FString::Printf(TEXT("| %3i:"), SubElementIdx));
			}

			return;
		}

		case UE::Learning::Observation::EType::Encoding:
		{
			const UE::Learning::Observation::FObjectEncodingParameters Parameters = Object.GetEncoding(ObjectElement);

			UE_LOG(LogLearning, Display, TEXT("%s%s \"%s\" (%s)"), *Indentation, *Prefix, *Tag.ToString(), GetObservationTypeString(Type));
			LogObservation(Object, Parameters.Element, *(Indentation + TEXT("    ")), TEXT("|"));

			return;
		}
		}
	}

	static inline FVector VectorLogSafe(const FVector V, const float Epsilon = UE_SMALL_NUMBER)
	{
		return FVector(
			FMath::Loge(FMath::Max(V.X, Epsilon)),
			FMath::Loge(FMath::Max(V.Y, Epsilon)),
			FMath::Loge(FMath::Max(V.Z, Epsilon)));
	}

	static inline FVector VectorExp(const FVector V)
	{
		return FVector(
			FMath::Exp(V.X),
			FMath::Exp(V.Y),
			FMath::Exp(V.Z));
	}

	static inline Learning::Observation::EEncodingActivationFunction GetEncodingActivationFunction(const ELearningAgentsActivationFunction ActivationFunction)
	{
		switch (ActivationFunction)
		{
		case ELearningAgentsActivationFunction::ReLU: return Learning::Observation::EEncodingActivationFunction::ReLU;
		case ELearningAgentsActivationFunction::ELU: return Learning::Observation::EEncodingActivationFunction::ELU;
		case ELearningAgentsActivationFunction::TanH: return Learning::Observation::EEncodingActivationFunction::TanH;
		default: UE_LEARNING_NOT_IMPLEMENTED(); return Learning::Observation::EEncodingActivationFunction::ReLU;
		}
	}
}

FTransform ULearningAgentsObservations::ProjectTransformOntoGroundPlane(const FTransform Transform, const FVector LocalForwardVector, const float GroundPlaneHeight)
{
	FVector Position = Transform.GetLocation();
	Position.Z = GroundPlaneHeight;

	const FVector Direction = (FVector(1.0f, 1.0f, 0.0f) * Transform.TransformVectorNoScale(LocalForwardVector)).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	return FTransform(FQuat::FindBetweenNormals(FVector::ForwardVector, Direction), Position, Transform.GetScale3D());
}

UEnum* ULearningAgentsObservations::FindEnumByName(const FString& Name)
{
	return FindObject<UEnum>(nullptr, *Name);
}

bool ULearningAgentsObservations::ValidateObservationObjectMatchesSchema(
	const ULearningAgentsObservationSchema* Schema,
	const FLearningAgentsObservationSchemaElement SchemaElement,
	const ULearningAgentsObservationObject* Object,
	const FLearningAgentsObservationObjectElement ObjectElement)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Schema is nullptr."));
		return false;
	}

	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("ValidateObservationObjectMatchesSchema: Object is nullptr."));
		return false;
	}

	return UE::Learning::Agents::Observation::Private::ValidateObservationObjectMatchesSchema(
		Schema->ObservationSchema,
		SchemaElement.SchemaElement,
		Object->ObservationObject,
		ObjectElement.ObjectElement);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyNullObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyNullObservation: Schema is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	return { Schema->ObservationSchema.CreateNull(Tag)};
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyContinuousObservation(ULearningAgentsObservationSchema* Schema, const int32 Size, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyContinuousObservation: Schema is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	if (Size < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyContinuousObservation: Invalid Continuous Observation Size '%i'."), Size);
		return FLearningAgentsObservationSchemaElement();
	}
	
	if (Size == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyContinuousObservation: Specifying zero-sized Continuous Observation."));
	}

	return { Schema->ObservationSchema.CreateContinuous({ Size }, Tag) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyExclusiveDiscreteObservation(ULearningAgentsObservationSchema* Schema, const int32 Size, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, Size, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyInclusiveDiscreteObservation(ULearningAgentsObservationSchema* Schema, const int32 Size, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, Size, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyCountObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, 1, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyStructObservation(ULearningAgentsObservationSchema* Schema, const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const FName Tag)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyStructObservation: Specifying zero-sized Struct Observation."));
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SubElements;
	SubElementIndices.Empty(Elements.Num());
	SubElementNames.Empty(Elements.Num());
	SubElements.Empty(Elements.Num());

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsObservationSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SortedSubElements;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
	}

	return SpecifyStructObservationFromArrayViews(Schema, SortedSubElementNames, SortedSubElements, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyStructObservationFromArrays(ULearningAgentsObservationSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const FName Tag)
{
	return SpecifyStructObservationFromArrayViews(Schema, ElementNames, Elements, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyStructObservationFromArrayViews(ULearningAgentsObservationSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStructObservationFromArrayViews: Schema is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyStructObservationFromArrayViews: Specifying zero-sized Struct Observation."));
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStructObservationFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationSchemaElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStructObservationFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsObservationSchemaElement();
	}

	TArray<UE::Learning::Observation::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationSchemaElement& Element : Elements)
	{
		if (!Schema->ObservationSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("SpecifyStructObservationFromArrayViews: Invalid Observation Object."));
			return FLearningAgentsObservationSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	return { Schema->ObservationSchema.CreateAnd({ ElementNames, SubElements }, Tag) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyExclusiveUnionObservation(ULearningAgentsObservationSchema* Schema, const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const int32 EncodingSize, const FName Tag)
{
	if (EncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionObservation: Invalid Observation EncodingSize '%i' - must be greater than zero."), EncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyExclusiveUnionObservation: Specifying zero-sized Exclusive Union Observation."));
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SubElements;
	SubElementIndices.Empty(Elements.Num());
	SubElementNames.Empty(Elements.Num());
	SubElements.Empty(Elements.Num());

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsObservationSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SortedSubElements;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
	}

	return SpecifyExclusiveUnionObservationFromArrayViews(Schema, SortedSubElementNames, SortedSubElements, EncodingSize, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyExclusiveUnionObservationFromArrays(ULearningAgentsObservationSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const int32 EncodingSize, const FName Tag)
{
	return SpecifyExclusiveUnionObservationFromArrayViews(Schema, ElementNames, Elements, EncodingSize, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyExclusiveUnionObservationFromArrayViews(ULearningAgentsObservationSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const int32 EncodingSize, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionObservationFromArrayViews: Schema is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	if (EncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionObservationFromArrayViews: Invalid Observation EncodingSize '%i' - must be greater than zero."), EncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyExclusiveUnionObservationFromArrayViews: Specifying zero-sized Exclusive Union Observation."));
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionObservationFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationSchemaElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionObservationFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsObservationSchemaElement();
	}

	TArray<UE::Learning::Observation::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationSchemaElement& Element : Elements)
	{
		if (!Schema->ObservationSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("SpecifyExclusiveUnionObservationFromArrayViews: Invalid Observation Object."));
			return FLearningAgentsObservationSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	return { Schema->ObservationSchema.CreateOrExclusive({ ElementNames, SubElements, EncodingSize }, Tag) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyInclusiveUnionObservation(ULearningAgentsObservationSchema* Schema, const TMap<FName, FLearningAgentsObservationSchemaElement>& Elements, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Tag)
{
	if (AttentionEncodingSize < 1 || AttentionHeadNum < 1 || ValueEncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionObservation: Invalid Observation Parameters: AttentionEncodingSize: %i, AttentionHeadNum: %i, ValueEncodingSize: %i - must be greater than zero."), AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyInclusiveUnionObservation: Specifying zero-sized Inclusive Union Observation."));
	}

	const int32 SubElementNum = Elements.Num();

	TArray<int32, TInlineAllocator<16>> SubElementIndices;
	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SubElements;
	SubElementIndices.Empty(SubElementNum);
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);

	int32 Index = 0;
	for (const TPair<FName, FLearningAgentsObservationSchemaElement>& Element : Elements)
	{
		SubElementIndices.Add(Index);
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
		Index++;
	}

	// Sort Elements According to FName

	SubElementIndices.Sort([SubElementNames](const int32 Lhs, const int32 Rhs)
	{
		return SubElementNames[Lhs].ToString().ToLower() < SubElementNames[Rhs].ToString().ToLower();
	});

	TArray<FName, TInlineAllocator<16>> SortedSubElementNames;
	TArray<FLearningAgentsObservationSchemaElement, TInlineAllocator<16>> SortedSubElements;
	SortedSubElementNames.SetNumUninitialized(SubElementNum);
	SortedSubElements.SetNumUninitialized(SubElementNum);
	for (int32 Idx = 0; Idx < SubElementNum; Idx++)
	{
		SortedSubElementNames[Idx] = SubElementNames[SubElementIndices[Idx]];
		SortedSubElements[Idx] = SubElements[SubElementIndices[Idx]];
	}

	return SpecifyInclusiveUnionObservationFromArrayViews(Schema, SortedSubElementNames, SortedSubElements, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyInclusiveUnionObservationFromArrays(ULearningAgentsObservationSchema* Schema, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationSchemaElement>& Elements, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Tag)
{
	return SpecifyInclusiveUnionObservationFromArrayViews(Schema, ElementNames, Elements, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyInclusiveUnionObservationFromArrayViews(ULearningAgentsObservationSchema* Schema, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationSchemaElement> Elements, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionObservationFromArrayViews: Schema is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	if (AttentionEncodingSize < 1 || AttentionHeadNum < 1 || ValueEncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionObservationFromArrayViews: Invalid Observation Parameters: AttentionEncodingSize: %i, AttentionHeadNum: %i, ValueEncodingSize: %i - must be greater than zero."), AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyInclusiveUnionObservationFromArrayViews: Specifying zero-sized Inclusive Union Observation."));
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionObservationFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationSchemaElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionObservationFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsObservationSchemaElement();
	}

	TArray<UE::Learning::Observation::FSchemaElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationSchemaElement& Element : Elements)
	{
		if (!Schema->ObservationSchema.IsValid(Element.SchemaElement))
		{
			UE_LOG(LogLearning, Error, TEXT("SpecifyInclusiveUnionObservationFromArrayViews: Invalid Observation Object."));
			return FLearningAgentsObservationSchemaElement();
		}

		SubElements.Add(Element.SchemaElement);
	}

	return { Schema->ObservationSchema.CreateOrInclusive({ ElementNames, SubElements, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize }, Tag) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyStaticArrayObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 Num, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStaticArrayObservation: Schema is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	if (Num < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStaticArrayObservation: Invalid Observation Array Num %i."), Num);
		return FLearningAgentsObservationSchemaElement();
	}

	if (Num == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifyStaticArrayObservation: Specifying zero-sized Static Array Observation."));
	}

	if (!Schema->ObservationSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyStaticArrayObservation: Invalid Observation Object."));
		return FLearningAgentsObservationSchemaElement();
	}

	return { Schema->ObservationSchema.CreateArray({ Element.SchemaElement, Num }, Tag) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifySetObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 MaxNum, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifySetObservation: Schema is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	if (MaxNum < 0)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifySetObservation: Invalid Observation Set MaxNum %i - must be greater than or equal to zero."), MaxNum);
		return FLearningAgentsObservationSchemaElement();
	}

	if (MaxNum == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("SpecifySetObservation: Specifying zero-sized Set Observation."));
	}

	if (AttentionEncodingSize < 1 || AttentionHeadNum < 1 || ValueEncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifySetObservation: Invalid Observation Parameters: AttentionEncodingSize: %i, AttentionHeadNum: %i, ValueEncodingSize: %i - must be greater than zero."), AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (!Schema->ObservationSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifySetObservation: Invalid Observation Object."));
		return FLearningAgentsObservationSchemaElement();
	}

	return { Schema->ObservationSchema.CreateSet({ Element.SchemaElement, MaxNum, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize }, Tag) };
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyPairObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element0, const FLearningAgentsObservationSchemaElement Element1, const FName Tag)
{
	return SpecifyStructObservationFromArrayViews(Schema, { TEXT("Key"), TEXT("Value") }, { Element0, Element1 }, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyArrayObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 MaxNum, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Tag)
{
	return SpecifySetObservation(Schema, SpecifyPairObservation(Schema, SpecifyCountObservation(Schema), Element), MaxNum, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyMapObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement KeyElement, const FLearningAgentsObservationSchemaElement ValueElement, const int32 MaxNum, const int32 AttentionEncodingSize, const int32 AttentionHeadNum, const int32 ValueEncodingSize, const FName Tag)
{
	return SpecifySetObservation(Schema, SpecifyPairObservation(Schema, KeyElement, ValueElement), MaxNum, AttentionEncodingSize, AttentionHeadNum, ValueEncodingSize, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyEnumObservation(ULearningAgentsObservationSchema* Schema, const UEnum* Enum, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEnumObservation: Enum is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	return SpecifyContinuousObservation(Schema, Enum->NumEnums() - 1, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyBitmaskObservation(ULearningAgentsObservationSchema* Schema, const UEnum* Enum, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyBitmaskObservation: Enum is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyBitmaskObservation: Too many values in Enum to use as Bitmask (%i)."), Enum->NumEnums() - 1);
		return FLearningAgentsObservationSchemaElement();
	}

	return SpecifyContinuousObservation(Schema, Enum->NumEnums() - 1, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyOptionalObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 EncodingSize, const FName Tag)
{
	return SpecifyExclusiveUnionObservationFromArrayViews(Schema, { TEXT("Null"), TEXT("Valid") }, { SpecifyNullObservation(Schema), Element }, EncodingSize, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyEitherObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement A, const FLearningAgentsObservationSchemaElement B, const int32 EncodingSize, const FName Tag)
{
	return SpecifyExclusiveUnionObservationFromArrayViews(Schema, { TEXT("A"), TEXT("B") }, { A, B }, EncodingSize, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyEncodingObservation(ULearningAgentsObservationSchema* Schema, const FLearningAgentsObservationSchemaElement Element, const int32 EncodingSize, const int32 HiddenLayerNum, const ELearningAgentsActivationFunction ActivationFunction, const FName Tag)
{
	if (!Schema)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEncodingObservation: Schema is nullptr."));
		return FLearningAgentsObservationSchemaElement();
	}

	if (EncodingSize < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEncodingObservation: Invalid Observation EncodingSize '%i' - must be greater than zero."), EncodingSize);
		return FLearningAgentsObservationSchemaElement();
	}

	if (HiddenLayerNum < 1)
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEncodingObservation: Invalid Observation HiddenLayerNum '%i' - must be greater than zero."), HiddenLayerNum);
		return FLearningAgentsObservationSchemaElement();
	}

	if (!Schema->ObservationSchema.IsValid(Element.SchemaElement))
	{
		UE_LOG(LogLearning, Error, TEXT("SpecifyEncodingObservation: Invalid Observation Object."));
		return FLearningAgentsObservationSchemaElement();
	}

	return { Schema->ObservationSchema.CreateEncoding({ Element.SchemaElement, EncodingSize, HiddenLayerNum, UE::Learning::Agents::Observation::Private::GetEncodingActivationFunction(ActivationFunction) }, Tag)};
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyBoolObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, 1, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyFloatObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, 1, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyLocationObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, 3, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyRotationObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, 6, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyScaleObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, 3, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyTransformObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyStructObservationFromArrayViews(Schema,
		{
			TEXT("Location"),
			TEXT("Rotation"),
			TEXT("Scale")
		},
		{
			SpecifyLocationObservation(Schema),
			SpecifyRotationObservation(Schema),
			SpecifyScaleObservation(Schema)
		}, 
		Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyAngleObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, 2, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyVelocityObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, 3, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyDirectionObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyContinuousObservation(Schema, 3, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyLocationAlongSplineObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyLocationObservation(Schema, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyProportionAlongSplineObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyExclusiveUnionObservationFromArrayViews(Schema,
		{
			TEXT("Angle"),
			TEXT("Proportion")
		},
		{
			SpecifyAngleObservation(Schema),
			SpecifyFloatObservation(Schema)
		},
		8,
		Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyDirectionAlongSplineObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyDirectionObservation(Schema, Tag);
}

FLearningAgentsObservationSchemaElement ULearningAgentsObservations::SpecifyProportionAlongRayObservation(ULearningAgentsObservationSchema* Schema, const FName Tag)
{
	return SpecifyFloatObservation(Schema, Tag);
}

void ULearningAgentsObservations::LogObservation(const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("LogObservation: Object is nullptr."));
		return;
	}

	UE::Learning::Agents::Observation::Private::LogObservation(Object->ObservationObject, Element.ObjectElement, TEXT(""), TEXT(""));
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeNullObservation(ULearningAgentsObservationObject* Object, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeNullObservation: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	return { Object->ObservationObject.CreateNull(Tag) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeContinuousObservation(
	ULearningAgentsObservationObject* Object, 
	const TArray<float>& Values, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	return MakeContinuousObservationFromArrayView(Object, Values, Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerLocation, VisualLoggerColor);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeContinuousObservationFromArrayView(
	ULearningAgentsObservationObject* Object, 
	const TArrayView<const float> Values, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeContinuousObservationFromArrayView: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (Values.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeContinuousObservationFromArrayView: Creating zero-sized Continuous Observation."));
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nValues: %s\nEncoded: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*UE::Learning::Array::FormatFloat(Values),
			*UE::Learning::Array::FormatFloat(Values)); // Encoded is identical to provided values
	}
#endif

	return { Object->ObservationObject.CreateContinuous({ Values }, Tag) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeExclusiveDiscreteObservation(
	ULearningAgentsObservationObject* Object, 
	const int32 DiscreteIndex, 
	const int32 Size, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (DiscreteIndex < 0 || DiscreteIndex >= Size)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveDiscreteObservation: Discrete index out of range: Got %i, expected <= %i."), DiscreteIndex, Size);
		return FLearningAgentsObservationObjectElement();
	}

	TArray<float, TInlineAllocator<32>> Values;
	Values.Init(0.0f, Size);
	Values[DiscreteIndex] = 1.0f;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nSize: [%i]\nIndex: [%i]\nEncoded: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Size,
			DiscreteIndex,
			*UE::Learning::Array::FormatFloat(Values, Size));
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, Values, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeInclusiveDiscreteObservation(
	ULearningAgentsObservationObject* Object, 
	const TArray<int32>& DiscreteIndices, 
	const int32 Size, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	return MakeInclusiveDiscreteObservationFromArrayView(Object, DiscreteIndices, Size, Tag, bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerLocation, VisualLoggerColor);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeInclusiveDiscreteObservationFromArrayView(
	ULearningAgentsObservationObject* Object, 
	const TArrayView<const int32> DiscreteIndices, 
	const int32 Size, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(DiscreteIndices))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveDiscreteObservationFromArrayView: Indices contain duplicates."));
		return FLearningAgentsObservationObjectElement();
	}

	TArray<float, TInlineAllocator<32>> Values;
	Values.Init(0.0f, Size);

	for (int32 Idx = 0; Idx < DiscreteIndices.Num(); Idx++)
	{
		if (DiscreteIndices[Idx] < 0 || DiscreteIndices[Idx] >= Size)
		{
			UE_LOG(LogLearning, Error, TEXT("MakeInclusiveDiscreteObservationFromArrayView: Discrete index out of range: Got %i, expected <= %i."), DiscreteIndices[Idx], Size);
			return FLearningAgentsObservationObjectElement();
		}

		Values[DiscreteIndices[Idx]] = 1.0f;
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nSize: [%i]\nIndices: %s\nEncoded: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Size,
			*UE::Learning::Array::FormatInt32(DiscreteIndices, Size),
			*UE::Learning::Array::FormatFloat(Values, Size));
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, Values, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeCountObservation(
	ULearningAgentsObservationObject* Object, 
	const int32 Num, 
	const int32 MaxNum, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (MaxNum == 0)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeCountObservation: MaxNum must not be zero for Count Observation."));
		return FLearningAgentsObservationObjectElement();
	}

	const float Encoded = (float)Num / (float)MaxNum;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nNum: [%i]\nMax Num: [%i]\nEncoded: [%6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Num,
			MaxNum,
			Encoded);
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, { Encoded }, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeStructObservation(ULearningAgentsObservationObject* Object, const TMap<FName, FLearningAgentsObservationObjectElement>& Elements, const FName Tag)
{
	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeStructObservation: Creating zero-sized Struct Observation."));
	}

	const int32 SubElementNum = Elements.Num();

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsObservationObjectElement>& Element : Elements)
	{
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
	}

	return MakeStructObservationFromArrayViews(Object, SubElementNames, SubElements, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeStructObservationFromArrays(ULearningAgentsObservationObject* Object, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Tag)
{
	return MakeStructObservationFromArrayViews(Object, ElementNames, Elements, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeStructObservationFromArrayViews(ULearningAgentsObservationObject* Object, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStructObservationFromArrayViews: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeStructObservationFromArrayViews: Creating zero-sized Struct Observation."));
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStructObservationFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationObjectElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStructObservationFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsObservationObjectElement();
	}

	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!Object->ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeStructObservationFromArrayViews: Invalid Observation Object."));
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { Object->ObservationObject.CreateAnd({ ElementNames, SubElements }, Tag)};
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeExclusiveUnionObservation(ULearningAgentsObservationObject* Object, const FName ElementName, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveUnionObservation: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeExclusiveUnionObservation: Invalid Observation Object."));
		return FLearningAgentsObservationObjectElement();
	}

	return { Object->ObservationObject.CreateOrExclusive({ ElementName, Element.ObjectElement }, Tag) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeInclusiveUnionObservation(ULearningAgentsObservationObject* Object, const TMap<FName, FLearningAgentsObservationObjectElement>& Elements, const FName Tag)
{
	const int32 SubElementNum = Elements.Num();

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.Empty(SubElementNum);
	SubElements.Empty(SubElementNum);

	for (const TPair<FName, FLearningAgentsObservationObjectElement>& Element : Elements)
	{
		SubElementNames.Add(Element.Key);
		SubElements.Add(Element.Value);
	}

	return MakeInclusiveUnionObservationFromArrayViews(Object, SubElementNames, SubElements, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeInclusiveUnionObservationFromArrays(ULearningAgentsObservationObject* Object, const TArray<FName>& ElementNames, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Tag)
{
	return MakeInclusiveUnionObservationFromArrayViews(Object, ElementNames, Elements, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeInclusiveUnionObservationFromArrayViews(ULearningAgentsObservationObject* Object, const TArrayView<const FName> ElementNames, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionObservationFromArrayViews: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (Elements.Num() != ElementNames.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionObservationFromArrayViews: Number of elements (%i) must match number of names (%i)."), Elements.Num(), ElementNames.Num());
		return FLearningAgentsObservationObjectElement();
	}

	if (UE::Learning::Agents::Observation::Private::ContainsDuplicates(ElementNames))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionObservationFromArrayViews: Element Names contain duplicates."));
		return FLearningAgentsObservationObjectElement();
	}

	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!Object->ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeInclusiveUnionObservationFromArrayViews: Invalid Observation Object."));
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { Object->ObservationObject.CreateOrInclusive({ ElementNames, SubElements }, Tag) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeStaticArrayObservation(ULearningAgentsObservationObject* Object, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Tag)
{
	return MakeStaticArrayObservationFromArrayView(Object, Elements, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeStaticArrayObservationFromArrayView(ULearningAgentsObservationObject* Object, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeStaticArrayObservationFromArrayView: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("MakeStaticArrayObservationFromArrayView: Creating zero-sized Static Array Observation."));
	}

	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!Object->ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeStaticArrayObservationFromArrayView: Invalid Observation Object."));
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { Object->ObservationObject.CreateArray({ SubElements }, Tag) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeSetObservation(ULearningAgentsObservationObject* Object, const TSet<FLearningAgentsObservationObjectElement>& Elements, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeSetObservation: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!Object->ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeSetObservation: Invalid Observation Object."));
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { Object->ObservationObject.CreateSet({ SubElements }, Tag) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeSetObservationFromArray(ULearningAgentsObservationObject* Object, const TArray<FLearningAgentsObservationObjectElement>& Elements, const FName Tag)
{
	return MakeSetObservationFromArrayView(Object, Elements, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeSetObservationFromArrayView(ULearningAgentsObservationObject* Object, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeSetObservationFromArrayView: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	TArray<UE::Learning::Observation::FObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (const FLearningAgentsObservationObjectElement& Element : Elements)
	{
		if (!Object->ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeSetObservationFromArrayView: Invalid Observation Object."));
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(Element.ObjectElement);
	}

	return { Object->ObservationObject.CreateSet({ SubElements }, Tag) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakePairObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Key, const FLearningAgentsObservationObjectElement Value, const FName Tag)
{
	return MakeStructObservationFromArrayViews(Object, { TEXT("Key"), TEXT("Value") }, { Key, Value }, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeArrayObservation(ULearningAgentsObservationObject* Object, const TArray<FLearningAgentsObservationObjectElement>& Elements, const int32 MaxNum, const FName Tag)
{
	return MakeArrayObservationFromArrayView(Object, Elements, MaxNum, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeArrayObservationFromArrayView(ULearningAgentsObservationObject* Object, const TArrayView<const FLearningAgentsObservationObjectElement> Elements, const int32 MaxNum, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeArrayObservationFromArrayView: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Elements.Num());

	for (int32 ElementIdx = 0; ElementIdx < Elements.Num(); ElementIdx++)
	{
		const FLearningAgentsObservationObjectElement Element = Elements[ElementIdx];

		if (!Object->ObservationObject.IsValid(Element.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeArrayObservationFromArrayView: Invalid Observation Object."));
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(MakePairObservation(Object, MakeCountObservation(Object, ElementIdx, MaxNum), Element));
	}

	return MakeSetObservationFromArrayView(Object, SubElements, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeMapObservation(ULearningAgentsObservationObject* Object, const TMap<FLearningAgentsObservationObjectElement, FLearningAgentsObservationObjectElement>& Map, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeMapObservation: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Map.Num());

	for (TPair<FLearningAgentsObservationObjectElement, FLearningAgentsObservationObjectElement> Item : Map)
	{
		if (!Object->ObservationObject.IsValid(Item.Key.ObjectElement) || !Object->ObservationObject.IsValid(Item.Value.ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeMapObservation: Invalid Observation Object."));
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(MakePairObservation(Object, Item.Key, Item.Value));
	}

	return MakeSetObservationFromArrayView(Object, SubElements, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeMapObservationFromArrays(ULearningAgentsObservationObject* Object, const TArray<FLearningAgentsObservationObjectElement>& Keys, const TArray<FLearningAgentsObservationObjectElement>& Values, const FName Tag)
{
	return MakeMapObservationFromArrayViews(Object, Keys, Values, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeMapObservationFromArrayViews(ULearningAgentsObservationObject* Object, const TArrayView<const FLearningAgentsObservationObjectElement> Keys, const TArrayView<const FLearningAgentsObservationObjectElement> Values, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeMapObservationFromArrayViews: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (Keys.Num() != Values.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("MakeMapObservationFromArrayViews: Number of keys (%i) must match number of values (%i)."), Keys.Num(), Values.Num());
		return FLearningAgentsObservationObjectElement();
	}

	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElements.Empty(Keys.Num());

	for (int32 ElementIdx = 0; ElementIdx < Keys.Num(); ElementIdx++)
	{
		if (!Object->ObservationObject.IsValid(Keys[ElementIdx].ObjectElement) || 
			!Object->ObservationObject.IsValid(Values[ElementIdx].ObjectElement))
		{
			UE_LOG(LogLearning, Error, TEXT("MakeMapObservationFromArrayViews: Invalid Observation Object."));
			return FLearningAgentsObservationObjectElement();
		}

		SubElements.Add(MakePairObservation(Object, Keys[ElementIdx], Values[ElementIdx]));
	}

	return MakeSetObservationFromArrayView(Object, SubElements, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeEnumObservation(
	ULearningAgentsObservationObject* Object, 
	const UEnum* Enum, 
	const uint8 EnumValue, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEnumObservation: Enum is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	const int32 EnumValueIndex = Enum->GetIndexByValue(EnumValue);

	if (EnumValueIndex == INDEX_NONE || EnumValueIndex < 0 || EnumValueIndex >= Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEnumObservation: EnumValue %i not valid for Enum '%s'."), EnumValue , *Enum->GetName());
		return FLearningAgentsObservationObjectElement();
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.Init(0.0f, Enum->NumEnums() - 1);
	OneHot[EnumValueIndex] = 1.0f;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nEnum: %s\nSize: [%i]\nValue: [%s]\nIndex: [%i]\nEncoded: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*Enum->GetName(),
			Enum->NumEnums() - 1,
			*Enum->GetDisplayNameTextByValue(EnumValue).ToString(),
			EnumValueIndex,
			*UE::Learning::Array::FormatFloat(OneHot, Enum->NumEnums() - 1));
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, OneHot, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeBitmaskObservation(
	ULearningAgentsObservationObject* Object, 
	const UEnum* Enum, 
	const int32 BitmaskValue, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeBitmaskObservation: Enum is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeBitmaskObservation: Too many values in Enum to use as Bitmask (%i)."), Enum->NumEnums() - 1);
		return FLearningAgentsObservationObjectElement();
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.Init(0.0f, Enum->NumEnums() - 1);

	for (int32 EnumIdx = 0; EnumIdx < Enum->NumEnums() - 1; EnumIdx++)
	{
		if (BitmaskValue & (1 << EnumIdx))
		{
			OneHot[EnumIdx] = 1.0f;
		}
	}

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		FString ValuesString;
		FString IndicesString;

		for (int32 EnumIdx = 0; EnumIdx < Enum->NumEnums() - 1; EnumIdx++)
		{
			if (BitmaskValue & (1 << EnumIdx))
			{
				ValuesString += Enum->GetDisplayNameTextByIndex(EnumIdx).ToString() + TEXT(" ");
				IndicesString += FString::FromInt(EnumIdx) + TEXT(" ");
			}
		}

		ValuesString = ValuesString.TrimEnd();
		IndicesString = IndicesString.TrimEnd();

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nEnum: %s\nSize: [%i]\nValues: [%s]\nIndices: [%s]\nEncoded: %s"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			*Enum->GetName(),
			Enum->NumEnums() - 1,
			*ValuesString,
			*IndicesString,
			*UE::Learning::Array::FormatFloat(OneHot, Enum->NumEnums() - 1));
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, OneHot, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeOptionalObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const ELearningAgentsOptionalObservation Option, const FName Tag)
{
	return MakeExclusiveUnionObservation(
		Object,
		Option == ELearningAgentsOptionalObservation::Null ? TEXT("Null") : TEXT("Valid"),
		Option == ELearningAgentsOptionalObservation::Null ? MakeNullObservation(Object) : Element,
		Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeOptionalNullObservation(ULearningAgentsObservationObject* Object, const FName Tag)
{
	return MakeExclusiveUnionObservation(Object, TEXT("Null"), MakeNullObservation(Object), Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeOptionalValidObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	return MakeExclusiveUnionObservation(Object, TEXT("Valid"), Element, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeEitherObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const ELearningAgentsEitherObservation Either, const FName Tag)
{
	return MakeExclusiveUnionObservation(Object, Either == ELearningAgentsEitherObservation::A ? TEXT("A") : TEXT("B"), Element, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeEitherAObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement A, const FName Tag)
{
	return MakeExclusiveUnionObservation(Object, TEXT("A"), A, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeEitherBObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement B, const FName Tag)
{
	return MakeExclusiveUnionObservation(Object, TEXT("B"), B, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeEncodingObservation(ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEncodingObservation: Object is nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("MakeEncodingObservation: Invalid Observation Object."));
		return FLearningAgentsObservationObjectElement();
	}

	return { Object->ObservationObject.CreateEncoding({ Element.ObjectElement }, Tag) };
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeBoolObservation(
	ULearningAgentsObservationObject* Object,
	const bool bValue,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const float EncodedBool = bValue ? 1.0f : -1.0f;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nValue: [%s]\nEncoded: [% 6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			bValue ? TEXT("true") : TEXT("false"),
			EncodedBool);
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, { EncodedBool }, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeFloatObservation(
	ULearningAgentsObservationObject* Object,
	const float Value,
	const float FloatScale,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const float EncodedValue = Value / FMath::Max(FloatScale, UE_SMALL_NUMBER);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nValue: [% 6.1f]\nScale: [% 6.2f]\nEncoded: [% 6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Value,
			FloatScale,
			EncodedValue);
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, { EncodedValue }, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeLocationObservation(
	ULearningAgentsObservationObject* Object,
	const FVector Location,
	const FTransform RelativeTransform,
	const float LocationScale,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const FVector LocalLocation = RelativeTransform.InverseTransformPosition(Location);
	const FVector EncodedLocation = FVector(
		LocalLocation.X / FMath::Max(LocationScale, UE_SMALL_NUMBER),
		LocalLocation.Y / FMath::Max(LocationScale, UE_SMALL_NUMBER),
		LocalLocation.Z / FMath::Max(LocationScale, UE_SMALL_NUMBER));

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_LOCATION(VisualLoggerObject, LogLearning, Display,
			Location,
			10,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_SEGMENT(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			Location,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			RelativeTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocation: [% 6.1f % 6.1f % 6.1f]\nLocal Location: [% 6.1f % 6.1f % 6.1f]\nScale: [% 6.2f]\nEncoded: [% 6.2f % 6.2f % 6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Location.X, Location.Y, Location.Z,
			LocalLocation.X, LocalLocation.Y, LocalLocation.Z,
			LocationScale,
			EncodedLocation.X, EncodedLocation.Y, EncodedLocation.Z);
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, {
		(float)EncodedLocation.X,
		(float)EncodedLocation.Y,
		(float)EncodedLocation.Z,
		}, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeRotationObservation(
	ULearningAgentsObservationObject* Object,
	const FRotator Rotation,
	const FRotator RelativeRotation,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerRotationLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	// Visual logging is handled in the MakeRotationObservationFromQuat

	return MakeRotationObservationFromQuat(
		Object,
		FQuat::MakeFromRotator(Rotation), FQuat::MakeFromRotator(RelativeRotation), Tag,
		bVisualLoggerEnabled, VisualLoggerListener, VisualLoggerAgentId, VisualLoggerRotationLocation, VisualLoggerLocation, VisualLoggerColor);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeRotationObservationFromQuat(
	ULearningAgentsObservationObject* Object,
	const FQuat Rotation,
	const FQuat RelativeRotation,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerRotationLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const FQuat LocalRotation = RelativeRotation.Inverse() * Rotation;
	const FVector LocalAxisForward = LocalRotation.GetForwardVector();
	const FVector LocalAxisRight = LocalRotation.GetRightVector();

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			VisualLoggerRotationLocation,
			LocalRotation.Rotator(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nRotation: [% 6.1f % 6.1f % 6.1f % 6.1f]\nLocal Rotation: [% 6.1f % 6.1f % 6.1f % 6.1f]\nEncoded Forward: [% 6.2f % 6.2f % 6.2f]\nEncoded Right: [% 6.2f % 6.2f % 6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Rotation.W, Rotation.X, Rotation.Y, Rotation.Z,
			LocalRotation.W, LocalRotation.X, LocalRotation.Y, LocalRotation.Z,
			LocalAxisForward.X, LocalAxisForward.Y, LocalAxisForward.Z,
			LocalAxisRight.X, LocalAxisRight.Y, LocalAxisRight.Z);
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, {
		(float)LocalAxisForward.X,
		(float)LocalAxisForward.Y,
		(float)LocalAxisForward.Z,
		(float)LocalAxisRight.X,
		(float)LocalAxisRight.Y,
		(float)LocalAxisRight.Z,
		}, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeScaleObservation(
	ULearningAgentsObservationObject* Object,
	const FVector Scale,
	const FVector RelativeScale,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerScaleLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const FVector LocalLogScale = 
		UE::Learning::Agents::Observation::Private::VectorLogSafe(Scale) - 
		UE::Learning::Agents::Observation::Private::VectorLogSafe(RelativeScale);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nScale: [% 6.1f % 6.1f % 6.1f]\nEncoded: [% 6.2f % 6.2f % 6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Scale.X, Scale.Y, Scale.Z,
			LocalLogScale.X, LocalLogScale.Y, LocalLogScale.Z);
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, {
		(float)LocalLogScale.X,
		(float)LocalLogScale.Y,
		(float)LocalLogScale.Z,
		}, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeTransformObservation(
	ULearningAgentsObservationObject* Object,
	const FTransform Transform,
	const FTransform RelativeTransform,
	const float LocationScale,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const FTransform LocalTransform = Transform * RelativeTransform.Inverse();
	
#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		const FVector LocalLocation = LocalTransform.GetLocation();
		const FRotator LocalRotation = LocalTransform.Rotator();
		const FVector LocalScale = LocalTransform.GetScale3D();

		const FVector Location = Transform.GetLocation();
		const FRotator Rotation = Transform.Rotator();
		const FVector Scale = Transform.GetScale3D();

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			Location,
			Rotation,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nLocation: [% 6.1f % 6.1f % 6.1f]\nLocal Location: [% 6.1f % 6.1f % 6.1f]\nRotation: [% 6.1f % 6.1f % 6.1f]\nLocal Rotation: [% 6.1f % 6.1f % 6.1f]\nScale: [% 6.1f % 6.1f % 6.1f]\nLocal Scale: [% 6.1f % 6.1f % 6.1f]\nLocation Scale: [% 6.1f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Location.X, Location.Y, Location.Z,
			LocalLocation.X, LocalLocation.Y, LocalLocation.Z,
			Rotation.Roll, Rotation.Pitch, Rotation.Yaw,
			LocalRotation.Roll, LocalRotation.Pitch, LocalRotation.Yaw,
			Scale.X, Scale.Y, Scale.Z,
			LocalScale.X, LocalScale.Y, LocalScale.Z,
			LocationScale);
	}
#endif

	return MakeStructObservationFromArrayViews(Object,
		{
			TEXT("Location"),
			TEXT("Rotation"),
			TEXT("Scale")
		},
		{
			MakeLocationObservation(Object, LocalTransform.GetLocation(), FTransform::Identity, LocationScale),
			MakeRotationObservationFromQuat(Object, LocalTransform.GetRotation(), FQuat::Identity),
			MakeScaleObservation(Object, LocalTransform.GetScale3D(), FVector::OneVector)
		},
		Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeAngleObservation(
	ULearningAgentsObservationObject* Object, 
	const float Angle, 
	const float RelativeAngle, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const float LocalAngle = FMath::FindDeltaAngleDegrees(RelativeAngle, Angle);
	const float EncodedX = FMath::Sin(FMath::DegreesToRadians(Angle));
	const float EncodedY = FMath::Cos(FMath::DegreesToRadians(Angle));

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_ANGLE_DEGREES(VisualLoggerObject, LogLearning, Display,
			Angle,
			RelativeAngle,
			VisualLoggerLocation,
			10.0f,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nAngle: [% 6.1f]\nRelative Angle: [% 6.1f]\nLocal Angle: [% 6.1f]\nEncoded: [% 6.2f % 6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Angle,
			RelativeAngle,
			LocalAngle,
			EncodedX, EncodedY);
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, { EncodedX, EncodedY }, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeAngleObservationRadians(
	ULearningAgentsObservationObject* Object, 
	const float Angle, 
	const float RelativeAngle, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	return MakeAngleObservation(
		Object, 
		FMath::RadiansToDegrees(Angle), 
		FMath::RadiansToDegrees(RelativeAngle), 
		Tag, 
		bVisualLoggerEnabled, 
		VisualLoggerListener, 
		VisualLoggerAgentId, 
		VisualLoggerLocation, 
		VisualLoggerColor);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeVelocityObservation(
	ULearningAgentsObservationObject* Object, 
	const FVector Velocity, 
	const FTransform RelativeTransform, 
	const float VelocityScale, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerVelocityLocation,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const FVector LocalVelocity = RelativeTransform.InverseTransformVectorNoScale(Velocity);
	const FVector EncodedVelocity = FVector(
		LocalVelocity.X / FMath::Max(VelocityScale, UE_SMALL_NUMBER),
		LocalVelocity.Y / FMath::Max(VelocityScale, UE_SMALL_NUMBER),
		LocalVelocity.Z / FMath::Max(VelocityScale, UE_SMALL_NUMBER));

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_ARROW(VisualLoggerObject, LogLearning, Display,
			VisualLoggerVelocityLocation,
			VisualLoggerVelocityLocation + Velocity,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			RelativeTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nVelocity: [% 6.1f % 6.1f % 6.1f]\nLocal Velocity: [% 6.1f % 6.1f % 6.1f]\nEncoded: [% 6.2f % 6.2f % 6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Velocity.X, Velocity.Y, Velocity.Z,
			LocalVelocity.X, LocalVelocity.Y, LocalVelocity.Z,
			EncodedVelocity.X, EncodedVelocity.Y, EncodedVelocity.Z);
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, { (float)EncodedVelocity.X, (float)EncodedVelocity.Y, (float)EncodedVelocity.Z }, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeDirectionObservation(
	ULearningAgentsObservationObject* Object,
	const FVector Direction,
	const FTransform RelativeTransform,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerDirectionLocation,
	const FVector VisualLoggerLocation,
	const float VisualLoggerArrowLength,
	const FLinearColor VisualLoggerColor)
{
	const FVector LocalDirection = RelativeTransform.InverseTransformVectorNoScale(Direction).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_ARROW(VisualLoggerObject, LogLearning, Display,
			VisualLoggerDirectionLocation,
			VisualLoggerDirectionLocation + VisualLoggerArrowLength * Direction,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RelativeTransform.GetTranslation(),
			RelativeTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nDirection: [% 6.1f % 6.1f % 6.1f]\nLocal Direction: [% 6.1f % 6.1f % 6.1f]\nEncoded: [% 6.2f % 6.2f % 6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			Direction.X, Direction.Y, Direction.Z,
			LocalDirection.X, LocalDirection.Y, LocalDirection.Z,
			LocalDirection.X, LocalDirection.Y, LocalDirection.Z); // We don't have a scaling so the encoded value is the same
	}
#endif

	return MakeContinuousObservationFromArrayView(Object, {
		(float)LocalDirection.X,
		(float)LocalDirection.Y,
		(float)LocalDirection.Z,
		}, Tag);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeLocationAlongSplineObservation(
	ULearningAgentsObservationObject* Object,
	const USplineComponent* SplineComponent,
	const float DistanceAlongSpline,
	const FTransform RelativeTransform,
	const float LocationScale,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeLocationAlongSplineObservation: SplineComponent was nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	const float TotalDistance = SplineComponent->GetSplineLength();
	const float LoopedDistance = SplineComponent->IsClosedLoop() ? FMath::Wrap(DistanceAlongSpline, 0.0f, TotalDistance) : FMath::Clamp(DistanceAlongSpline, 0.0f, TotalDistance);

	return MakeLocationObservation(
		Object, 
		SplineComponent->GetLocationAtDistanceAlongSpline(LoopedDistance, ESplineCoordinateSpace::World), 
		RelativeTransform, 
		LocationScale, 
		Tag, 
		bVisualLoggerEnabled, 
		VisualLoggerListener, 
		VisualLoggerAgentId, 
		VisualLoggerLocation, 
		VisualLoggerColor);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeProportionAlongSplineObservation(
	ULearningAgentsObservationObject* Object, 
	const USplineComponent* SplineComponent, 
	const float DistanceAlongSpline, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeProportionAlongSplineObservation: SplineComponent was nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	if (SplineComponent->IsClosedLoop())
	{
		const float TotalDistance = SplineComponent->GetSplineLength();
		const float WrapDistance = FMath::Wrap(DistanceAlongSpline, 0.0f, TotalDistance);
		const float Proportion = WrapDistance / FMath::Max(TotalDistance, UE_SMALL_NUMBER);
		const float Angle = FMath::Wrap(UE_TWO_PI * Proportion, -UE_PI, UE_PI);
		
		return MakeExclusiveUnionObservation(Object, TEXT("Angle"), MakeAngleObservationRadians(
			Object, 
			Angle,
			0.0f,
			TEXT("AngleObservation"),
			bVisualLoggerEnabled,
			VisualLoggerListener,
			VisualLoggerAgentId,
			VisualLoggerLocation,
			VisualLoggerColor), Tag);
	}
	else
	{
		const float Proportion = FMath::Clamp(DistanceAlongSpline / FMath::Max(SplineComponent->GetSplineLength(), UE_SMALL_NUMBER), 0.0f, 1.0f);

		return MakeExclusiveUnionObservation(Object, TEXT("Proportion"), MakeFloatObservation(
			Object, 
			Proportion, 
			1.0f, 
			TEXT("FloatObservation"),
			bVisualLoggerEnabled, 
			VisualLoggerListener, 
			VisualLoggerAgentId, 
			VisualLoggerLocation, 
			VisualLoggerColor), Tag);
	}
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeDirectionAlongSplineObservation(
	ULearningAgentsObservationObject* Object,
	const USplineComponent* SplineComponent,
	const float DistanceAlongSpline,
	const FTransform RelativeTransform,
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const float VisualLoggerArrowLength,
	const FLinearColor VisualLoggerColor)
{
	if (!SplineComponent)
	{
		UE_LOG(LogLearning, Error, TEXT("MakeDirectionAlongSplineObservation: SplineComponent was nullptr."));
		return FLearningAgentsObservationObjectElement();
	}

	const float TotalDistance = SplineComponent->GetSplineLength();
	const float LoopedDistance = SplineComponent->IsClosedLoop() ? FMath::Wrap(DistanceAlongSpline, 0.0f, TotalDistance) : FMath::Clamp(DistanceAlongSpline, 0.0f, TotalDistance);

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	const FVector VisualLoggerDirectionLocation = 
		(bVisualLoggerEnabled && VisualLoggerListener) ?
		SplineComponent->GetLocationAtDistanceAlongSpline(LoopedDistance, ESplineCoordinateSpace::World) :
		FVector::ZeroVector;
#else
	const FVector VisualLoggerDirectionLocation = FVector::ZeroVector;
#endif

	return MakeDirectionObservation(
		Object, 
		SplineComponent->GetDirectionAtDistanceAlongSpline(LoopedDistance, ESplineCoordinateSpace::World), 
		RelativeTransform, 
		Tag, 
		bVisualLoggerEnabled, 
		VisualLoggerListener, 
		VisualLoggerAgentId, 
		VisualLoggerDirectionLocation, 
		VisualLoggerLocation, 
		VisualLoggerArrowLength, 
		VisualLoggerColor);
}

FLearningAgentsObservationObjectElement ULearningAgentsObservations::MakeProportionAlongRayObservation(
	ULearningAgentsObservationObject* Object, 
	const FVector RayStart, 
	const FVector RayEnd, 
	const FTransform RayTransform, 
	const ECollisionChannel CollisionChannel, 
	const FName Tag,
	const bool bVisualLoggerEnabled,
	ULearningAgentsManagerListener* VisualLoggerListener,
	const int32 VisualLoggerAgentId,
	const FVector VisualLoggerLocation,
	const FLinearColor VisualLoggerColor)
{
	const FVector RayStartWorld = RayTransform.TransformPosition(RayStart);
	const FVector RayEndWorld = RayTransform.TransformPosition(RayEnd);
	const float RayDistance = FVector::Distance(RayStartWorld, RayEndWorld);

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(CollisionChannel);

	FHitResult TraceHit;
	const bool bHit = Object->GetWorld()->LineTraceSingleByObjectType(TraceHit, RayStartWorld, RayEndWorld, ObjectQueryParams);

	const float Encoded = bHit ? 1.0f - TraceHit.Time : 0.0f;

#if UE_LEARNING_AGENTS_ENABLE_VISUAL_LOG
	if (bVisualLoggerEnabled && VisualLoggerListener)
	{
		const ULearningAgentsVisualLoggerObject* VisualLoggerObject = VisualLoggerListener->GetOrAddVisualLoggerObject(Tag);

		UE_LEARNING_AGENTS_VLOG_SEGMENT(VisualLoggerObject, LogLearning, Display,
			RayStartWorld,
			RayEndWorld,
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		if (bHit)
		{
			UE_LEARNING_AGENTS_VLOG_LOCATION(VisualLoggerObject, LogLearning, Display,
				RayStartWorld + TraceHit.Time * (RayEndWorld - RayStartWorld),
				10.f,
				VisualLoggerColor.ToFColor(true),
				TEXT(""));
		}

		UE_LEARNING_AGENTS_VLOG_TRANSFORM(VisualLoggerObject, LogLearning, Display,
			RayTransform.GetTranslation(),
			RayTransform.GetRotation(),
			VisualLoggerColor.ToFColor(true),
			TEXT(""));

		UE_LEARNING_AGENTS_VLOG_STRING(VisualLoggerObject, LogLearning, Display, VisualLoggerLocation,
			VisualLoggerColor.ToFColor(true),
			TEXT("Listener: %s\nTag: %s\nAgent Id: % 3i\nRay Start: [% 6.1f % 6.1f % 6.1f]\nRay End: [% 6.1f % 6.1f % 6.1f]\nRay Start World: [% 6.1f % 6.1f % 6.1f]\nRay End World: [% 6.1f % 6.1f % 6.1f]\nCollision Channel: [%s]\nHit: [%s]\nEncoded: [% 6.2f]"),
			*VisualLoggerListener->GetName(),
			*Tag.ToString(),
			VisualLoggerAgentId,
			RayStart.X, RayStart.Y, RayStart.Z,
			RayEnd.X, RayEnd.Y, RayEnd.Z,
			RayStartWorld.X, RayStartWorld.Y, RayStartWorld.Z,
			RayEndWorld.X, RayEndWorld.Y, RayEndWorld.Z,
			*StaticEnum<ECollisionChannel>()->GetNameStringByValue(CollisionChannel),
			bHit ? TEXT("true") : TEXT("false"),
			Encoded);
	}
#endif

	return MakeFloatObservation(Object, Encoded, 1.0f, Tag);
}


bool ULearningAgentsObservations::GetNullObservation(const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNullObservation: Object is nullptr."));
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetNullObservation: Invalid Observation Object."));
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetNullObservation: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Null)
	{
		UE_LOG(LogLearning, Error, TEXT("GetNullObservation: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Null));
		return false;
	}

	return true;
}

bool ULearningAgentsObservations::GetContinuousObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousObservationNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousObservationNum: Invalid Observation Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetContinuousObservationNum: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Continuous)
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousObservationNum: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Continuous));
		OutNum = 0;
		return false;
	}

	OutNum = Object->ObservationObject.GetContinuous(Element.ObjectElement).Values.Num();
	return true;
}

bool ULearningAgentsObservations::GetContinuousObservation(TArray<float>& OutValues, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutValueNum = 0;
	if (!GetContinuousObservationNum(OutValueNum, Object, Element, Tag))
	{
		OutValues.Empty();
		return false;
	}

	OutValues.SetNumUninitialized(OutValueNum);

	if (!GetContinuousObservationToArrayView(OutValues, Object, Element, Tag))
	{
		OutValues.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservations::GetContinuousObservationToArrayView(TArrayView<float> OutValues, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousObservationToArrayView: Object is nullptr."));
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousObservationToArrayView: Invalid Observation Object."));
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetContinuousObservationToArrayView: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Continuous)
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousObservationToArrayView: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Continuous));
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	const TArrayView<const float> Values = Object->ObservationObject.GetContinuous(Element.ObjectElement).Values;

	if (Values.Num() != OutValues.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetContinuousObservationToArrayView: Observation '%s' size does not match. Observation is '%i' values but asked for '%i'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			Values.Num(), OutValues.Num());
		UE::Learning::Array::Zero<1, float>(OutValues);
		return false;
	}

	UE::Learning::Array::Copy<1, float>(OutValues, Values);
	return true;
}

bool ULearningAgentsObservations::GetExclusiveDiscreteObservation(int32& OutIndex, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 DiscreteValueNum;
	if (!GetContinuousObservationNum(DiscreteValueNum, Object, Element, Tag))
	{
		OutIndex = INDEX_NONE;
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.SetNumUninitialized(DiscreteValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Object, Element, Tag))
	{
		OutIndex = INDEX_NONE;
		return false;
	}

	OutIndex = INDEX_NONE;
	for (int32 Idx = 0; Idx < DiscreteValueNum; Idx++)
	{
		if (OneHot[Idx])
		{
			OutIndex = Idx;
			break;
		}
	}

	return OutIndex != INDEX_NONE;
}

bool ULearningAgentsObservations::GetInclusiveDiscreteObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 DiscreteValueNum;
	if (!GetContinuousObservationNum(DiscreteValueNum, Object, Element, Tag))
	{
		OutNum = 0;
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.SetNumUninitialized(DiscreteValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Object, Element, Tag))
	{
		OutNum = 0;
		return false;
	}

	OutNum = 0;
	for (int32 Idx = 0; Idx < DiscreteValueNum; Idx++)
	{
		if (OneHot[Idx]) { OutNum++; }
	}

	return true;
}

bool ULearningAgentsObservations::GetInclusiveDiscreteObservation(TArray<int32>& OutIndices, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutIndicesNum;
	if (!GetInclusiveDiscreteObservationNum(OutIndicesNum, Object, Element, Tag))
	{
		OutIndices.Empty();
		return false;
	}

	OutIndices.SetNumUninitialized(OutIndicesNum);
	return GetInclusiveDiscreteObservationToArrayView(OutIndices, Object, Element, Tag);
}

bool ULearningAgentsObservations::GetInclusiveDiscreteObservationToArrayView(TArrayView<int32> OutIndices, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveDiscreteObservationToArrayView: Object is nullptr."));
		return false;
	}

	int32 DiscreteValueNum;
	if (!GetContinuousObservationNum(DiscreteValueNum, Object, Element, Tag))
	{
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.SetNumUninitialized(DiscreteValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Object, Element, Tag))
	{
		return false;
	}

	int32 IndicesNum = 0;
	for (int32 Idx = 0; Idx < DiscreteValueNum; Idx++)
	{
		if (OneHot[Idx]) { IndicesNum++; }
	}

	if (IndicesNum != OutIndices.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveDiscreteObservationToArrayView: Observation '%s' size does not match. Observation is '%i' indices but asked for '%i'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			IndicesNum, OutIndices.Num());
		UE::Learning::Array::Zero<1, int32>(OutIndices);
		return false;
	}

	int32 Offset = 0;
	for (int32 Idx = 0; Idx < DiscreteValueNum; Idx++)
	{
		if (OneHot[Idx]) { OutIndices[Offset] = Idx; Offset++; }
	}

	return true;
}

bool ULearningAgentsObservations::GetCountObservation(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const int32 MaxNum, const FName Tag)
{
	float FloatNum = 0.0f;
	if (!GetContinuousObservationToArrayView(MakeArrayView(&FloatNum, 1), Object, Element, Tag))
	{
		OutNum = -1;
		return false;
	}

	OutNum = FMath::RoundToInt(FloatNum * MaxNum);
	return true;
}

bool ULearningAgentsObservations::GetStructObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationNum: Invalid Observation Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStructObservationNum: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::And)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationNum: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::And));
		OutNum = 0;
		return false;
	}

	const UE::Learning::Observation::FObjectAndParameters Parameters = Object->ObservationObject.GetAnd(Element.ObjectElement);

	OutNum = Parameters.Elements.Num();
	return true;
}

bool ULearningAgentsObservations::GetStructObservation(TMap<FName, FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetStructObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.SetNumUninitialized(OutElementNum);
	SubElements.SetNumUninitialized(OutElementNum);

	if (!GetStructObservationToArrayViews(SubElementNames, SubElements, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(OutElementNum);
	for (int32 ElementIdx = 0; ElementIdx < OutElementNum; ElementIdx++)
	{
		OutElements.Add(SubElementNames[ElementIdx], SubElements[ElementIdx]);
	}

	return true;
}

bool ULearningAgentsObservations::GetStructObservationToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetStructObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	OutElementNames.SetNumUninitialized(OutElementNum);
	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetStructObservationToArrayViews(OutElementNames, OutElements, Object, Element, Tag))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservations::GetStructObservationToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationToArrayViews: Object is nullptr."));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationToArrayViews: Invalid Observation Object."));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStructObservationToArrayViews: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::And)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationToArrayViews: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::And));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	const UE::Learning::Observation::FObjectAndParameters Parameters = Object->ObservationObject.GetAnd(Element.ObjectElement);

	if (Parameters.Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStructObservationToArrayViews: Getting zero-sized And Observation."));
	}

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationToArrayViews: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!Object->ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("GetStructObservationToArrayViews: Invalid Observation Object."));
			UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}

		OutElementNames[ElementIdx] = Parameters.ElementNames[ElementIdx];
		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsObservations::GetExclusiveUnionObservation(FName& OutElementName, FLearningAgentsObservationObjectElement& OutElement, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationToArrayViews: Object is nullptr."));
		OutElementName = NAME_None;
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationToArrayViews: Invalid Observation Object."));
		OutElementName = NAME_None;
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStructObservationToArrayViews: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::OrExclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStructObservationToArrayViews: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::OrExclusive));
		OutElementName = NAME_None;
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	const UE::Learning::Observation::FObjectOrExclusiveParameters Parameters = Object->ObservationObject.GetOrExclusive(Element.ObjectElement);
	OutElementName = Parameters.ElementName;
	OutElement = { Parameters.Element };
	return true;
}

bool ULearningAgentsObservations::GetInclusiveUnionObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionObservationNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionObservationNum: Invalid Observation Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetInclusiveUnionObservationNum: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::OrInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionObservationNum: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::OrInclusive));
		OutNum = 0;
		return false;
	}

	const UE::Learning::Observation::FObjectOrInclusiveParameters Parameters = Object->ObservationObject.GetOrInclusive(Element.ObjectElement);

	OutNum = Parameters.Elements.Num();
	return true;
}

bool ULearningAgentsObservations::GetInclusiveUnionObservation(TMap<FName, FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetInclusiveUnionObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FName, TInlineAllocator<16>> SubElementNames;
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> SubElements;
	SubElementNames.SetNumUninitialized(OutElementNum);
	SubElements.SetNumUninitialized(OutElementNum);

	if (!GetInclusiveUnionObservationToArrayViews(SubElementNames, SubElements, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(OutElementNum);
	for (int32 ElementIdx = 0; ElementIdx < OutElementNum; ElementIdx++)
	{
		OutElements.Add(SubElementNames[ElementIdx], SubElements[ElementIdx]);
	}

	return true;
}

bool ULearningAgentsObservations::GetInclusiveUnionObservationToArrays(TArray<FName>& OutElementNames, TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetInclusiveUnionObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	OutElementNames.SetNumUninitialized(OutElementNum);
	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetInclusiveUnionObservationToArrayViews(OutElementNames, OutElements, Object, Element, Tag))
	{
		OutElementNames.Empty();
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservations::GetInclusiveUnionObservationToArrayViews(TArrayView<FName> OutElementNames, TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionObservationToArrayViews: Object is nullptr."));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionObservationToArrayViews: Invalid Observation Object."));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetInclusiveUnionObservationToArrayViews: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::OrInclusive)
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionObservationToArrayViews: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::OrInclusive));
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	const UE::Learning::Observation::FObjectOrInclusiveParameters Parameters = Object->ObservationObject.GetOrInclusive(Element.ObjectElement);

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionObservationToArrayViews: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!Object->ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("GetInclusiveUnionObservationToArrayViews: Invalid Observation Object."));
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			UE::Learning::Array::Set<1, FName>(OutElementNames, NAME_None);
			return false;
		}

		OutElementNames[ElementIdx] = Parameters.ElementNames[ElementIdx];
		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsObservations::GetStaticArrayObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayObservationNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayObservationNum: Invalid Observation Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStaticArrayObservationNum: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Array)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayObservationNum: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Array));
		OutNum = 0;
		return false;
	}

	OutNum = Object->ObservationObject.GetArray(Element.ObjectElement).Elements.Num();
	return true;
}

bool ULearningAgentsObservations::GetStaticArrayObservation(TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetStaticArrayObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetStaticArrayObservationToArrayView(OutElements, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservations::GetStaticArrayObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayObservationNum: Object is nullptr."));
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayObservationToArrayView: Invalid Observation Object."));
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStaticArrayObservationToArrayView: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Array)
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayObservationToArrayView: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Array));
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	const UE::Learning::Observation::FObjectArrayParameters Parameters = Object->ObservationObject.GetArray(Element.ObjectElement);

	if (Parameters.Elements.Num() == 0)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetStaticArrayObservationToArrayView: Getting zero-sized Static Array Observation."));
	}

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetStaticArrayObservationToArrayView: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!Object->ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("GetStaticArrayObservationToArrayView: Invalid Observation Object."));
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}

		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsObservations::GetSetObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservationNum: Object is nullptr."));
		OutNum = 0;
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservationNum: Invalid Observation Object."));
		OutNum = 0;
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetSetObservationNum: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Set)
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservationNum: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Set));
		OutNum = 0;
		return false;
	}

	OutNum = Object->ObservationObject.GetSet(Element.ObjectElement).Elements.Num();
	return true;
}

bool ULearningAgentsObservations::GetSetObservation(TSet<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetSetObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservation: Object is nullptr."));
		OutElements.Empty();
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservation: Invalid Observation Object."));
		OutElements.Empty();
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetSetObservation: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Set)
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservation: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Set));
		OutElements.Empty();
		return false;
	}

	const UE::Learning::Observation::FObjectSetParameters Parameters = Object->ObservationObject.GetSet(Element.ObjectElement);

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservation: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(Parameters.Elements.Num());
	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!Object->ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("GetSetObservation: Invalid Observation Object."));
			OutElements.Empty();
			return false;
		}

		OutElements.Add({ Parameters.Elements[ElementIdx] });
	}

	return true;
}

bool ULearningAgentsObservations::GetSetObservationToArray(TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetSetObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetSetObservationToArrayView(OutElements, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservations::GetSetObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservationToArrayView: Object is nullptr."));
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservationToArrayView: Invalid Observation Object."));
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetSetObservationToArrayView: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Set)
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservationToArrayView: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Set));
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	const UE::Learning::Observation::FObjectSetParameters Parameters = Object->ObservationObject.GetSet(Element.ObjectElement);

	if (Parameters.Elements.Num() != OutElements.Num())
	{
		UE_LOG(LogLearning, Error, TEXT("GetSetObservationToArrayView: Observation '%s' size does not match. Observation is '%i' elements but asked for '%i'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			Parameters.Elements.Num(), OutElements.Num());
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 ElementIdx = 0; ElementIdx < Parameters.Elements.Num(); ElementIdx++)
	{
		if (!Object->ObservationObject.IsValid(Parameters.Elements[ElementIdx]))
		{
			UE_LOG(LogLearning, Error, TEXT("GetSetObservationToArrayView: Invalid Observation Object."));
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}

		OutElements[ElementIdx] = { Parameters.Elements[ElementIdx] };
	}

	return true;
}

bool ULearningAgentsObservations::GetPairObservation(FLearningAgentsObservationObjectElement& OutKey, FLearningAgentsObservationObjectElement& OutValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	TStaticArray<FName, 2> OutElementNames;
	TStaticArray<FLearningAgentsObservationObjectElement, 2> OutElements;
	if (!GetStructObservationToArrayViews(OutElementNames, OutElements, Object, Element, Tag))
	{
		OutKey = FLearningAgentsObservationObjectElement();
		OutValue = FLearningAgentsObservationObjectElement();
		return false;
	}

	OutKey = OutElements[MakeArrayView(OutElementNames).Find(TEXT("Key"))];
	OutValue = OutElements[MakeArrayView(OutElementNames).Find(TEXT("Value"))];
	return true;
}

bool ULearningAgentsObservations::GetArrayObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	return GetSetObservationNum(OutNum, Object, Element, Tag);
}

bool ULearningAgentsObservations::GetArrayObservation(TArray<FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const int32 MaxNum, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetArrayObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.SetNumUninitialized(OutElementNum);

	if (!GetArrayObservationToArrayView(OutElements, Object, Element, MaxNum, Tag))
	{
		OutElements.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservations::GetArrayObservationToArrayView(TArrayView<FLearningAgentsObservationObjectElement> OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const int32 MaxNum, const FName Tag)
{
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> Pairs;
	Pairs.SetNumUninitialized(OutElements.Num());
	if (!GetSetObservationToArrayView(Pairs, Object, Element, Tag))
	{
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 PairIdx = 0; PairIdx < Pairs.Num(); PairIdx++)
	{
		FLearningAgentsObservationObjectElement Key, Value;
		if (!GetPairObservation(Key, Value, Object, Pairs[PairIdx]))
		{
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}

		int32 Index = INDEX_NONE;
		if (!GetCountObservation(Index, Object, Key, MaxNum))
		{
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutElements, FLearningAgentsObservationObjectElement());
			return false;
		}
		Index = FMath::Clamp(Index, 0, OutElements.Num() - 1);

		OutElements[Index] = Value;
	}

	return true;
}

bool ULearningAgentsObservations::GetMapObservationNum(int32& OutNum, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	return GetSetObservationNum(OutNum, Object, Element, Tag);
}

bool ULearningAgentsObservations::GetMapObservation(TMap<FLearningAgentsObservationObjectElement, FLearningAgentsObservationObjectElement>& OutElements, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetMapObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> Pairs;
	Pairs.SetNumUninitialized(OutElementNum);
	if (!GetSetObservationToArrayView(Pairs, Object, Element, Tag))
	{
		OutElements.Empty();
		return false;
	}

	OutElements.Empty(OutElementNum);
	for (int32 PairIdx = 0; PairIdx < OutElementNum; PairIdx++)
	{
		FLearningAgentsObservationObjectElement Key, Value;
		if (!GetPairObservation(Key, Value, Object, Pairs[PairIdx]))
		{
			OutElements.Empty();
			return false;
		}

		OutElements.Add(Key, Value);
	}

	return true;
}

bool ULearningAgentsObservations::GetMapObservationToArrays(TArray<FLearningAgentsObservationObjectElement>& OutKeys, TArray<FLearningAgentsObservationObjectElement>& OutValues, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	int32 OutElementNum = 0;
	if (!GetMapObservationNum(OutElementNum, Object, Element, Tag))
	{
		OutKeys.Empty();
		OutValues.Empty();
		return false;
	}

	OutKeys.SetNumUninitialized(OutElementNum);
	OutValues.SetNumUninitialized(OutElementNum);
	if (!GetMapObservationToArrayViews(OutKeys, OutValues, Object, Element, Tag))
	{
		OutKeys.Empty();
		OutValues.Empty();
		return false;
	}

	return true;
}

bool ULearningAgentsObservations::GetMapObservationToArrayViews(TArrayView<FLearningAgentsObservationObjectElement> OutKeys, TArrayView<FLearningAgentsObservationObjectElement> OutValues, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	TArray<FLearningAgentsObservationObjectElement, TInlineAllocator<16>> Pairs;
	Pairs.SetNumUninitialized(OutKeys.Num());
	if (!GetSetObservationToArrayView(Pairs, Object, Element, Tag))
	{
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutKeys, FLearningAgentsObservationObjectElement());
		UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutValues, FLearningAgentsObservationObjectElement());
		return false;
	}

	for (int32 PairIdx = 0; PairIdx < Pairs.Num(); PairIdx++)
	{
		FLearningAgentsObservationObjectElement Key, Value;
		if (!GetPairObservation(Key, Value, Object, Pairs[PairIdx]))
		{
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutKeys, FLearningAgentsObservationObjectElement());
			UE::Learning::Array::Set<1, FLearningAgentsObservationObjectElement>(OutValues, FLearningAgentsObservationObjectElement());
			return false;
		}

		OutKeys[PairIdx] = Key;
		OutValues[PairIdx] = Value;
	}

	return true;
}

bool ULearningAgentsObservations::GetEnumObservation(uint8& OutEnumValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const UEnum* Enum, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEnumObservation: Enum is nullptr."));
		OutEnumValue = 0;
		return false;
	}

	int32 EnumValueNum;
	if (!GetContinuousObservationNum(EnumValueNum, Object, Element, Tag))
	{
		OutEnumValue = 0;
		return false;
	}
	
	if (EnumValueNum != Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEnumObservation: Too many values for Enum '%s'. Expected %i, got %i."), *Enum->GetName(), Enum->NumEnums() - 1, EnumValueNum);
		OutEnumValue = 0;
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.SetNumUninitialized(EnumValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Object, Element, Tag))
	{
		OutEnumValue = 0;
		return false;
	}

	int32 EnumValueIndex = INDEX_NONE;
	for (int32 EnumIdx = 0; EnumIdx < EnumValueNum; EnumIdx++)
	{
		if (OneHot[EnumIdx])
		{
			EnumValueIndex = EnumIdx;
			break;
		}
	}

	if (EnumValueIndex == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEnumObservation: Index not found."));
		OutEnumValue = 0;
		return false;
	}

	const int32 EnumValue = Enum->GetValueByIndex(EnumValueIndex);

	if (EnumValue == INDEX_NONE)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEnumObservation: Enum Value not found for index %i."), EnumValueIndex);
		OutEnumValue = 0;
		return false;
	}

	OutEnumValue = (uint8)EnumValue;
	return true;
}

bool ULearningAgentsObservations::GetBitmaskObservation(int32& OutBitmaskValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const UEnum* Enum, const FName Tag)
{
	if (!Enum)
	{
		UE_LOG(LogLearning, Error, TEXT("GetBitmaskObservation: Enum is nullptr."));
		OutBitmaskValue = 0;
		return false;
	}

	if (Enum->NumEnums() - 1 > 32)
	{
		UE_LOG(LogLearning, Error, TEXT("GetBitmaskObservation: Too many values in Enum to use as Bitmask (%i)."), Enum->NumEnums() - 1);
		OutBitmaskValue = 0;
		return false;
	}

	int32 EnumValueNum;
	if (!GetContinuousObservationNum(EnumValueNum, Object, Element, Tag))
	{
		OutBitmaskValue = 0;
		return false;
	}

	if (EnumValueNum != Enum->NumEnums() - 1)
	{
		UE_LOG(LogLearning, Error, TEXT("GetBitmaskObservation: Too many values for Enum '%s'. Expected %i, got %i."), *Enum->GetName(), Enum->NumEnums() - 1, EnumValueNum);
		OutBitmaskValue = 0;
		return false;
	}

	TArray<float, TInlineAllocator<32>> OneHot;
	OneHot.Init(0.0f, EnumValueNum);
	if (!GetContinuousObservationToArrayView(OneHot, Object, Element, Tag))
	{
		OutBitmaskValue = 0;
		return false;
	}

	OutBitmaskValue = 0;
	for (int32 OneHotIdx = 0; OneHotIdx < EnumValueNum; OneHotIdx++)
	{
		if (OneHot[OneHotIdx])
		{
			OutBitmaskValue |= (1 << OneHotIdx);
		}
	}
	return true;
}


bool ULearningAgentsObservations::GetOptionalObservation(ELearningAgentsOptionalObservation& OutOption, FLearningAgentsObservationObjectElement& OutElement, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	FName OutName = NAME_None;
	if (!GetExclusiveUnionObservation(OutName, OutElement, Object, Element, Tag))
	{
		OutOption = ELearningAgentsOptionalObservation::Null;
		return false;
	}

	OutOption = OutName == TEXT("Null") ? ELearningAgentsOptionalObservation::Null : ELearningAgentsOptionalObservation::Valid;
	return true;
}

bool ULearningAgentsObservations::GetEitherObservation(ELearningAgentsEitherObservation& OutEither, FLearningAgentsObservationObjectElement& OutElement, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	FName OutName = NAME_None;
	if (!GetExclusiveUnionObservation(OutName, OutElement, Object, Element, Tag))
	{
		OutEither = ELearningAgentsEitherObservation::A;
		return false;
	}

	OutEither = OutName == TEXT("A") ? ELearningAgentsEitherObservation::A : ELearningAgentsEitherObservation::B;
	return true;
}

bool ULearningAgentsObservations::GetEncodingObservation(FLearningAgentsObservationObjectElement& OutElement, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!Object)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEncodingObservation: Object is nullptr."));
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	if (!Object->ObservationObject.IsValid(Element.ObjectElement))
	{
		UE_LOG(LogLearning, Error, TEXT("GetEncodingObservation: Invalid Observation Object."));
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	if (Object->ObservationObject.GetTag(Element.ObjectElement) != Tag)
	{
		UE_LOG(LogLearning, Warning, TEXT("GetEncodingObservation: Observation tag does not match. Observation is '%s' but asked for '%s'."), *Object->ObservationObject.GetTag(Element.ObjectElement).ToString(), *Tag.ToString());
	}

	if (Object->ObservationObject.GetType(Element.ObjectElement) != UE::Learning::Observation::EType::Encoding)
	{
		UE_LOG(LogLearning, Error, TEXT("GetEncodingObservation: Observation '%s' type does not match. Observation is '%s' but asked for '%s'."),
			*Object->ObservationObject.GetTag(Element.ObjectElement).ToString(),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(Object->ObservationObject.GetType(Element.ObjectElement)),
			UE::Learning::Agents::Observation::Private::GetObservationTypeString(UE::Learning::Observation::EType::Encoding));
		OutElement = FLearningAgentsObservationObjectElement();
		return false;
	}

	OutElement = { Object->ObservationObject.GetEncoding(Element.ObjectElement).Element };

	return true;
}

bool ULearningAgentsObservations::GetBoolObservation(bool& bOutValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	float OutValue = 0.0f;
	if (!GetFloatObservation(OutValue, Object, Element, 1.0f, Tag))
	{
		bOutValue = false;
		return false;
	}

	bOutValue = OutValue >= 0.0f;
	return true;
}

bool ULearningAgentsObservations::GetFloatObservation(float& OutValue, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const float FloatScale, const FName Tag)
{
	float OutValuesData;
	if (!GetContinuousObservationToArrayView(MakeArrayView(&OutValuesData, 1), Object, Element, Tag))
	{
		OutValue = 0.0f;
		return false;
	}

	OutValue = OutValuesData * FloatScale;
	return true;
}

bool ULearningAgentsObservations::GetLocationObservation(FVector& OutLocation, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const float LocationScale, const FName Tag)
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Object, Element, Tag))
	{
		OutLocation = FVector::ZeroVector;
		return false;
	}

	OutLocation = RelativeTransform.TransformPosition(LocationScale * FVector(OutValues[0], OutValues[1], OutValues[2]));
	return true;
}

bool ULearningAgentsObservations::GetRotationObservation(FRotator& OutRotation, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FRotator RelativeRotation, const FName Tag)
{
	FQuat OutRotationQuat;
	if (!GetRotationObservationAsQuat(OutRotationQuat, Object, Element, FQuat::MakeFromRotator(RelativeRotation), Tag))
	{
		OutRotation = FRotator::ZeroRotator;
		return false;
	}

	OutRotation = OutRotationQuat.Rotator();
	return true;
}

bool ULearningAgentsObservations::GetRotationObservationAsQuat(FQuat& OutRotation, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FQuat RelativeRotation, const FName Tag)
{
	TStaticArray<float, 6> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Object, Element, Tag))
	{
		OutRotation = FQuat::Identity;
		return false;
	}

	const FVector LocalAxisForward = FVector(OutValues[0], OutValues[1], OutValues[2]);
	const FVector LocalAxisRight = FVector(OutValues[3], OutValues[4], OutValues[5]);
	const FVector AxisUp = LocalAxisForward.Cross(LocalAxisRight).GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector AxisRight = AxisUp.Cross(LocalAxisForward).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
	const FVector AxisForward = LocalAxisForward.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	FMatrix RotationMatrix = FMatrix::Identity;
	RotationMatrix.SetAxis(0, AxisForward);
	RotationMatrix.SetAxis(1, AxisRight);
	RotationMatrix.SetAxis(2, AxisUp);

	OutRotation = RelativeRotation * RotationMatrix.ToQuat();
	return true;
}

bool ULearningAgentsObservations::GetScaleObservation(FVector& OutScale, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FVector RelativeScale, const FName Tag)
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Object, Element, Tag))
	{
		OutScale = FVector::OneVector;
		return false;
	}

	OutScale = RelativeScale * UE::Learning::Agents::Observation::Private::VectorExp(FVector(OutValues[0], OutValues[1], OutValues[2]));
	return true;
}

bool ULearningAgentsObservations::GetTransformObservation(FTransform& OutTransform, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const float LocationScale, const FName Tag)
{
	TStaticArray<FName, 3> OutElementNames;
	TStaticArray<FLearningAgentsObservationObjectElement, 3> OutElements;
	if (!GetStructObservationToArrayViews(OutElementNames, OutElements, Object, Element, Tag))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	const int32 LocationElement = MakeArrayView(OutElementNames).Find(TEXT("Location"));
	FVector OutLocation;
	if (LocationElement == INDEX_NONE || !GetLocationObservation(OutLocation, Object, OutElements[LocationElement], RelativeTransform, LocationScale))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	const int32 RotationElement = MakeArrayView(OutElementNames).Find(TEXT("Rotation"));
	FQuat OutRotation;
	if (RotationElement == INDEX_NONE || !GetRotationObservationAsQuat(OutRotation, Object, OutElements[RotationElement], RelativeTransform.GetRotation()))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	const int32 ScaleElement = MakeArrayView(OutElementNames).Find(TEXT("Scale"));
	FVector OutScale;
	if (ScaleElement == INDEX_NONE || !GetScaleObservation(OutScale, Object, OutElements[ScaleElement], RelativeTransform.GetScale3D()))
	{
		OutTransform = FTransform::Identity;
		return false;
	}

	OutTransform = FTransform(OutRotation, OutLocation, OutScale);
	return true;
}

bool ULearningAgentsObservations::GetAngleObservationRadians(float& OutAngle, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const float RelativeAngle, const FName Tag)
{
	TStaticArray<float, 2> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Object, Element, Tag))
	{
		OutAngle = 0.0f;
		return false;
	}

	OutAngle = RelativeAngle + FMath::Atan2(OutValues[0], OutValues[1]);
	return true;
}


bool ULearningAgentsObservations::GetAngleObservation(float& OutAngle, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const float RelativeAngle, const FName Tag)
{
	if (!GetAngleObservationRadians(OutAngle, Object, Element, FMath::DegreesToRadians(RelativeAngle), Tag))
	{
		return false;
	}

	OutAngle = FMath::RadiansToDegrees(OutAngle);
	return true;
}

bool ULearningAgentsObservations::GetVelocityObservation(FVector& OutVelocity, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const float VelocityScale, const FName Tag)
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Object, Element, Tag))
	{
		OutVelocity = FVector::ZeroVector;
		return false;
	}

	OutVelocity = RelativeTransform.TransformVectorNoScale(VelocityScale * FVector(OutValues[0], OutValues[1], OutValues[2]));
	return true;
}

bool ULearningAgentsObservations::GetDirectionObservation(FVector& OutDirection, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const FName Tag)
{
	TStaticArray<float, 3> OutValues;
	if (!GetContinuousObservationToArrayView(OutValues, Object, Element, Tag))
	{
		OutDirection = FVector::ForwardVector;
		return false;
	}

	OutDirection = RelativeTransform.TransformVectorNoScale(FVector(OutValues[0], OutValues[1], OutValues[2]).GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector));
	return true;
}

bool ULearningAgentsObservations::GetLocationAlongSplineObservation(FVector& OutLocation, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const float LocationScale, const FName Tag)
{
	return GetLocationObservation(OutLocation, Object, Element, RelativeTransform, LocationScale, Tag);
}

bool ULearningAgentsObservations::GetProportionAlongSplineObservation(bool& bOutIsClosedLoop, float& OutAngle, float& OutPropotion, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	FName SubName;
	FLearningAgentsObservationObjectElement SubElement;
	if (!GetExclusiveUnionObservation(SubName, SubElement, Object, Element, Tag))
	{
		bOutIsClosedLoop = false;
		OutAngle = 0.0f;
		OutPropotion = 0.0;
		return false;
	}

	if (SubName == TEXT("Angle"))
	{
		bOutIsClosedLoop = true;
		OutPropotion = 0.0f;
		return GetAngleObservation(OutAngle, Object, SubElement);
	}
	else
	{
		bOutIsClosedLoop = false;
		OutAngle = 0.0f;
		return GetFloatObservation(OutPropotion, Object, SubElement);
	}
}

bool ULearningAgentsObservations::GetDirectionAlongSplineObservation(FVector& OutDirection, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FTransform RelativeTransform, const FName Tag)
{
	return GetDirectionObservation(OutDirection, Object, Element, RelativeTransform, Tag);
}

bool ULearningAgentsObservations::GetProportionAlongRayObservation(float& OutProportion, const ULearningAgentsObservationObject* Object, const FLearningAgentsObservationObjectElement Element, const FName Tag)
{
	if (!GetFloatObservation(OutProportion, Object, Element, 1.0f, Tag))
	{
		OutProportion = 0.0f;
		return false;
	}

	OutProportion = 1.0f - OutProportion;
	return true;
}

