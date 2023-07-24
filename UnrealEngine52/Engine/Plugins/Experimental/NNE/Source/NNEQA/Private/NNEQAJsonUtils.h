// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonSerializerMacros.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"

namespace UE::NNEQA::Private
{
namespace Json
{
	constexpr float JSON_TOLERANCE_NOTSET = -1.0f; 

	struct FTestConfigTensor : FJsonSerializable
	{
		TArray<int32> Shape;
		TArray<FString> Source;
		FString Type;
		//Idea: Extend with custom initializer:
		//	FString Initializer;
		//	JSON_SERIALIZE("initializer", ...);
		//		if (initializer && source)
		//			InRuntimeTensorData = initializer(source) <-- To add
		//		if (!initializer && source)
		//			InRuntimeTensorData = element-wise( static_cast<Type>(source[i]) ) <-- Current behavior
		//		if (!initializer && !source)
		//			InRuntimeTensorData = random from seed <-- Current behavior
		//Idea: Extend by adding source CPU or GPU resident data
		//	bool OnGpu;
		//	JSON_SERIALIZE("on_gpu", OnGpu);
		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE_ARRAY("shape", Shape);
			JSON_SERIALIZE("type", Type);
			JSON_SERIALIZE_ARRAY("source", Source);
		END_JSON_SERIALIZER
	};

	struct FTestConfigRuntime : FJsonSerializable
	{
		FString Name;
		float AbsoluteTolerance = JSON_TOLERANCE_NOTSET;
		float RelativeTolerance = JSON_TOLERANCE_NOTSET;
		bool Skip;
		bool SkipStatic;
		bool SkipVariadic;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("name", Name);
			JSON_SERIALIZE("skip", Skip);
			JSON_SERIALIZE("skip_static_test", SkipStatic);
			JSON_SERIALIZE("skip_variadic_test", SkipVariadic);
			JSON_SERIALIZE("absolute_tolerance", AbsoluteTolerance);
			JSON_SERIALIZE("relative_tolerance", RelativeTolerance);
		END_JSON_SERIALIZER
	};

	struct FTestConfigDataset : FJsonSerializable
	{
		TArray<FTestConfigTensor> Inputs;
		TArray<FTestConfigTensor> Weights;
		TArray<FTestConfigTensor> Outputs;
		TArray<FTestConfigRuntime> Runtimes;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("inputs", Inputs, FTestConfigTensor);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("weights", Weights, FTestConfigTensor);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("outputs", Outputs, FTestConfigTensor);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("runtimes", Runtimes, FTestConfigRuntime);
		END_JSON_SERIALIZER
	};

	struct FTestAttribute : FJsonSerializable
	{
		FString Name;
		FNNEAttributeValue Value;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("name", Name);
			
			// Since we prefer the attribute value to be in readable form in the Json and
			// we only serialize it, we construct it at runtime by manually serialize it
			// note: default type is float

			FString TypeStr;
			Serializer.Serialize(TEXT("type"), TypeStr);

			ENNEAttributeDataType Type = ENNEAttributeDataType::Float;
			if (!TypeStr.IsEmpty()) LexFromString(Type, *TypeStr);

			switch (Type)
			{
				case ENNEAttributeDataType::Float:
				{
					float TmpValue;
					JSON_SERIALIZE("value", TmpValue);
					Value = FNNEAttributeValue(TmpValue);
					break;
				}
				case ENNEAttributeDataType::FloatArray:
				{
					TArray<FString> TmpValue;
					JSON_SERIALIZE_ARRAY("value", TmpValue);
					TArray<float> ConvertedValue;
					Algo::Transform(TmpValue, ConvertedValue, [](const FString& In){ return FCString::Atof(*In); });
					Value = FNNEAttributeValue(ConvertedValue);
					break;
				}
				case ENNEAttributeDataType::Int32:
				{
					int TmpValue;
					JSON_SERIALIZE("value", TmpValue);
					Value = FNNEAttributeValue(TmpValue);
					break;
				}
				case ENNEAttributeDataType::Int32Array:
				{
					TArray<int32> TmpValue;
					JSON_SERIALIZE_ARRAY("value", TmpValue);
					Value = FNNEAttributeValue(TmpValue);
					break;
				}
				case ENNEAttributeDataType::String:
				{
					FString TmpValue;
					JSON_SERIALIZE("value", TmpValue);
					Value = FNNEAttributeValue(TmpValue);
					break;
				}
				case ENNEAttributeDataType::StringArray:
				{
					TArray<FString> TmpValue;
					JSON_SERIALIZE_ARRAY("value", TmpValue);
					Value = FNNEAttributeValue(TmpValue);
					break;
				}
				default:
					check(Type == ENNEAttributeDataType::None);
			}
		END_JSON_SERIALIZER
	};

	struct FTestAttributeMap : FJsonSerializable
	{
		TArray<FTestAttribute> Attributes;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("attributes", Attributes, FTestAttribute);
		END_JSON_SERIALIZER
	};

	struct FTestConfigTarget : FJsonSerializable
	{
		FString Target;
		TArray<FString> Tags;
		TArray<FString> AdditionalDatasets;
		TArray<FString> RemovedDatasets;
		bool Skip;
		float AbsoluteTolerance = JSON_TOLERANCE_NOTSET;
		float RelativeTolerance = JSON_TOLERANCE_NOTSET;
		FString InputType;
		FString OutputType;
		TArray<FTestConfigRuntime> Runtimes;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("target", Target);
			JSON_SERIALIZE("skip", Skip);
			JSON_SERIALIZE("absolute_tolerance", AbsoluteTolerance);
			JSON_SERIALIZE("relative_tolerance", RelativeTolerance);
			JSON_SERIALIZE("input_type", InputType);
			JSON_SERIALIZE("output_type", OutputType);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("runtimes", Runtimes, FTestConfigRuntime);
			JSON_SERIALIZE_ARRAY("tags", Tags);
			JSON_SERIALIZE_ARRAY("additional_datasets", AdditionalDatasets);
			JSON_SERIALIZE_ARRAY("removed_datasets", RemovedDatasets);
		END_JSON_SERIALIZER
	};

	struct FTestCategory : FJsonSerializable
	{
		FString Category;
		TArray<FString> AdditionalDatasets;
		TArray<FString> RemovedDatasets;
		TArray<FTestConfigTarget> Targets;
		TArray<FTestConfigRuntime> Runtimes;
		bool Skip;
		bool IsModelTest;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("category", Category);
			JSON_SERIALIZE("skip", Skip);
			JSON_SERIALIZE("is_model_test", IsModelTest);
			JSON_SERIALIZE_ARRAY("additional_datasets", AdditionalDatasets);
			JSON_SERIALIZE_ARRAY("removed_datasets", RemovedDatasets);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("targets", Targets, FTestConfigTarget);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("runtimes", Runtimes, FTestConfigRuntime);
		END_JSON_SERIALIZER
	};

	struct FTestConfigInputOutputSet : FJsonSerializable
	{
		FString Name;
		TArray<FTestConfigDataset> Datasets;
		TArray<FTestConfigRuntime> Runtimes;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("name", Name);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("datasets", Datasets, FTestConfigDataset);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("runtimes", Runtimes, FTestConfigRuntime);
		END_JSON_SERIALIZER
	};

	struct FTestAttributeSet : FJsonSerializable
	{
		FString Name;
		TArray<FTestAttributeMap> AttributeMaps;
		TArray<FString> MultiplyWithAttributeSets;

		BEGIN_JSON_SERIALIZER
			JSON_SERIALIZE("name", Name);
			JSON_SERIALIZE_ARRAY_SERIALIZABLE("attribute_maps", AttributeMaps, FTestAttributeMap);
			JSON_SERIALIZE_ARRAY("multiply_with_attribute_sets", MultiplyWithAttributeSets);
		END_JSON_SERIALIZER
	};

	template<typename T>
	bool TrySerializeArray(TArray<T> &OutArray, const FString &FieldName, const FJsonObject &GlobalJsonObject)
	{
		const TArray<TSharedPtr<FJsonValue>>* JsonArrayValue;
		if (GlobalJsonObject.TryGetArrayField(FieldName, JsonArrayValue))
		{
			for (TSharedPtr<FJsonValue> JsonValue : *JsonArrayValue)
			{
				const TSharedPtr<FJsonObject>* JsonObject = nullptr;
				if (!JsonValue->TryGetObject(JsonObject))
				{
					return false;
				}
				OutArray.Emplace_GetRef().FromJson(*JsonObject);
			}
		}

		return true;
	}

	bool LoadTestDescriptionFromJson(const FString& FullPath,
		TArray<FTestCategory>& ModelTestCategories,
		TArray<FTestCategory>& OperatorCategories,
		TArray<FTestConfigInputOutputSet>& InputOutputSets,
		TArray<FTestAttributeSet>& AttributeSets)
	{
		ModelTestCategories.Empty();
		OperatorCategories.Empty();
		InputOutputSets.Empty();
		AttributeSets.Empty();

		FString JsonContent;
		FFileHelper::LoadFileToString(JsonContent, *FullPath);
		TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(JsonContent);
		TSharedPtr<FJsonObject> GlobalJsonObject = MakeShareable(new FJsonObject);

		if (!FJsonSerializer::Deserialize(JsonReader, GlobalJsonObject))
		{
			return false;
		}

		if (!GlobalJsonObject.IsValid())
		{
			return false;
		}

		bool bSuccess = false;
		bSuccess |= TrySerializeArray(ModelTestCategories, TEXT("model_test_categories"), *GlobalJsonObject);
		bSuccess |= TrySerializeArray(OperatorCategories, TEXT("operator_test_categories"), *GlobalJsonObject);
		bSuccess |= TrySerializeArray(InputOutputSets, TEXT("input_output_sets"), *GlobalJsonObject);
		bSuccess |= TrySerializeArray(AttributeSets, TEXT("attribute_sets"), *GlobalJsonObject);

		return bSuccess;
	}

} // namespace Json
} // namespace UE::NNEQA::Private
