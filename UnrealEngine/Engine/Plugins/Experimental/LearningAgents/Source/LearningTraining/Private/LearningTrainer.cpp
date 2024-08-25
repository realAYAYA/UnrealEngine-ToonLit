// Copyright Epic Games, Inc. All Rights Reserved.

#include "LearningTrainer.h"

#include "LearningObservation.h"
#include "LearningAction.h"

#include "HAL/Platform.h"
#include "Dom/JsonObject.h"

namespace UE::Learning::Trainer
{
	TSharedPtr<FJsonObject> ConvertObservationSchemaToJSON(
		const Observation::FSchema& ObservationSchema,
		const Observation::FSchemaElement& ObservationSchemaElement)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("VectorSize"), ObservationSchema.GetObservationVectorSize(ObservationSchemaElement));
		Object->SetNumberField(TEXT("EncodedSize"), ObservationSchema.GetEncodedVectorSize(ObservationSchemaElement));

		switch (ObservationSchema.GetType(ObservationSchemaElement))
		{
		case Observation::EType::Null:
		{
			Object->SetStringField(TEXT("Type"), TEXT("Null"));
			break;
		}

		case Observation::EType::Continuous:
		{
			const Observation::FSchemaContinuousParameters Parameters = ObservationSchema.GetContinuous(ObservationSchemaElement);
			Object->SetStringField(TEXT("Type"), TEXT("Continuous"));
			Object->SetNumberField(TEXT("Num"), Parameters.Num);
			break;
		}

		case Observation::EType::And:
		{
			const Observation::FSchemaAndParameters Parameters = ObservationSchema.GetAnd(ObservationSchemaElement);
			
			Object->SetStringField(TEXT("Type"), TEXT("And"));

			TSharedPtr<FJsonObject> SubObject = MakeShared<FJsonObject>();
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				TSharedPtr<FJsonObject> SubElement = ConvertObservationSchemaToJSON(ObservationSchema, Parameters.Elements[SubElementIdx]);
				SubElement->SetNumberField(TEXT("Index"), SubElementIdx);
				SubObject->SetObjectField(Parameters.ElementNames[SubElementIdx].ToString(), SubElement);
			}

			Object->SetObjectField(TEXT("Elements"), SubObject);
			break;
		}

		case Observation::EType::OrExclusive:
		{
			const Observation::FSchemaOrExclusiveParameters Parameters = ObservationSchema.GetOrExclusive(ObservationSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("OrExclusive"));
			Object->SetNumberField(TEXT("EncodingSize"), Parameters.EncodingSize);

			TSharedPtr<FJsonObject> SubObject = MakeShared<FJsonObject>();
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				TSharedPtr<FJsonObject> SubElement = ConvertObservationSchemaToJSON(ObservationSchema, Parameters.Elements[SubElementIdx]);
				SubElement->SetNumberField(TEXT("Index"), SubElementIdx);
				SubObject->SetObjectField(Parameters.ElementNames[SubElementIdx].ToString(), SubElement);
			}

			Object->SetObjectField(TEXT("Elements"), SubObject);
			break;
		}

		case Observation::EType::OrInclusive:
		{
			const Observation::FSchemaOrInclusiveParameters Parameters = ObservationSchema.GetOrInclusive(ObservationSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("OrInclusive"));
			Object->SetNumberField(TEXT("AttentionEncodingSize"), Parameters.AttentionEncodingSize);
			Object->SetNumberField(TEXT("AttentionHeadNum"), Parameters.AttentionHeadNum);
			Object->SetNumberField(TEXT("ValueEncodingSize"), Parameters.ValueEncodingSize);

			TSharedPtr<FJsonObject> SubObject = MakeShared<FJsonObject>();
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				TSharedPtr<FJsonObject> SubElement = ConvertObservationSchemaToJSON(ObservationSchema, Parameters.Elements[SubElementIdx]);
				SubElement->SetNumberField(TEXT("Index"), SubElementIdx);
				SubObject->SetObjectField(Parameters.ElementNames[SubElementIdx].ToString(), SubElement);
			}

			Object->SetObjectField(TEXT("Elements"), SubObject);
			break;
		}

		case Observation::EType::Array:
		{
			const Observation::FSchemaArrayParameters Parameters = ObservationSchema.GetArray(ObservationSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("Array"));
			Object->SetNumberField(TEXT("Num"), Parameters.Num);
			Object->SetObjectField(TEXT("Element"), ConvertObservationSchemaToJSON(ObservationSchema, Parameters.Element));
			break;
		}

		case Observation::EType::Set:
		{
			const Observation::FSchemaSetParameters Parameters = ObservationSchema.GetSet(ObservationSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("Set"));
			Object->SetNumberField(TEXT("MaxNum"), Parameters.MaxNum);
			Object->SetNumberField(TEXT("AttentionEncodingSize"), Parameters.AttentionEncodingSize);
			Object->SetNumberField(TEXT("AttentionHeadNum"), Parameters.AttentionHeadNum);
			Object->SetNumberField(TEXT("ValueEncodingSize"), Parameters.ValueEncodingSize);
			Object->SetObjectField(TEXT("Element"), ConvertObservationSchemaToJSON(ObservationSchema, Parameters.Element));
			break;
		}

		case Observation::EType::Encoding:
		{
			const Observation::FSchemaEncodingParameters Parameters = ObservationSchema.GetEncoding(ObservationSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("Encoding"));
			Object->SetNumberField(TEXT("EncodingSize"), Parameters.EncodingSize);
			Object->SetObjectField(TEXT("Element"), ConvertObservationSchemaToJSON(ObservationSchema, Parameters.Element));
			break;
		}

		default:
			UE_LEARNING_NOT_IMPLEMENTED();
		}

		return Object;
	}

	TSharedPtr<FJsonObject> ConvertActionSchemaToJSON(
		const Action::FSchema& ActionSchema,
		const Action::FSchemaElement& ActionSchemaElement)
	{
		TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetNumberField(TEXT("VectorSize"), ActionSchema.GetActionVectorSize(ActionSchemaElement));
		Object->SetNumberField(TEXT("DistributionSize"), ActionSchema.GetActionDistributionVectorSize(ActionSchemaElement));
		Object->SetNumberField(TEXT("EncodedSize"), ActionSchema.GetEncodedVectorSize(ActionSchemaElement));

		switch (ActionSchema.GetType(ActionSchemaElement))
		{
		case Action::EType::Null:
		{
			Object->SetStringField(TEXT("Type"), TEXT("Null"));
			break;
		}

		case Action::EType::Continuous:
		{
			const Action::FSchemaContinuousParameters Parameters = ActionSchema.GetContinuous(ActionSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("Continuous"));
			Object->SetNumberField(TEXT("Num"), Parameters.Num);
			break;
		}

		case Action::EType::DiscreteExclusive:
		{
			const Action::FSchemaDiscreteExclusiveParameters Parameters = ActionSchema.GetDiscreteExclusive(ActionSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("DiscreteExclusive"));
			Object->SetNumberField(TEXT("Num"), Parameters.Num);
			break;
		}

		case Action::EType::DiscreteInclusive:
		{
			const Action::FSchemaDiscreteInclusiveParameters Parameters = ActionSchema.GetDiscreteInclusive(ActionSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("DiscreteInclusive"));
			Object->SetNumberField(TEXT("Num"), Parameters.Num);
			break;
		}

		case Action::EType::And:
		{
			const Action::FSchemaAndParameters Parameters = ActionSchema.GetAnd(ActionSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("And"));

			TSharedPtr<FJsonObject> SubObject = MakeShared<FJsonObject>();
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				TSharedPtr<FJsonObject> SubElement = ConvertActionSchemaToJSON(ActionSchema, Parameters.Elements[SubElementIdx]);
				SubElement->SetNumberField(TEXT("Index"), SubElementIdx);
				SubObject->SetObjectField(Parameters.ElementNames[SubElementIdx].ToString(), SubElement);
			}

			Object->SetObjectField(TEXT("Elements"), SubObject);
			break;
		}

		case Action::EType::OrExclusive:
		{
			const Action::FSchemaOrExclusiveParameters Parameters = ActionSchema.GetOrExclusive(ActionSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("OrExclusive"));

			TSharedPtr<FJsonObject> SubObject = MakeShared<FJsonObject>();
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				TSharedPtr<FJsonObject> SubElement = ConvertActionSchemaToJSON(ActionSchema, Parameters.Elements[SubElementIdx]);
				SubElement->SetNumberField(TEXT("Index"), SubElementIdx);
				SubObject->SetObjectField(Parameters.ElementNames[SubElementIdx].ToString(), SubElement);
			}

			Object->SetObjectField(TEXT("Elements"), SubObject);
			break;
		}

		case Action::EType::OrInclusive:
		{
			const Action::FSchemaOrInclusiveParameters Parameters = ActionSchema.GetOrInclusive(ActionSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("OrInclusive"));

			TSharedPtr<FJsonObject> SubObject = MakeShared<FJsonObject>();
			for (int32 SubElementIdx = 0; SubElementIdx < Parameters.Elements.Num(); SubElementIdx++)
			{
				TSharedPtr<FJsonObject> SubElement = ConvertActionSchemaToJSON(ActionSchema, Parameters.Elements[SubElementIdx]);
				SubElement->SetNumberField(TEXT("Index"), SubElementIdx);
				SubObject->SetObjectField(Parameters.ElementNames[SubElementIdx].ToString(), SubElement);
			}

			Object->SetObjectField(TEXT("Elements"), SubObject);
			break;
		}

		case Action::EType::Array:
		{
			const Action::FSchemaArrayParameters Parameters = ActionSchema.GetArray(ActionSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("Array"));
			Object->SetNumberField(TEXT("Num"), Parameters.Num);
			Object->SetObjectField(TEXT("Element"), ConvertActionSchemaToJSON(ActionSchema, Parameters.Element));
			break;
		}

		case Action::EType::Encoding:
		{
			const Action::FSchemaEncodingParameters Parameters = ActionSchema.GetEncoding(ActionSchemaElement);

			Object->SetStringField(TEXT("Type"), TEXT("Encoding"));
			Object->SetNumberField(TEXT("EncodingSize"), Parameters.EncodingSize);
			Object->SetObjectField(TEXT("Element"), ConvertActionSchemaToJSON(ActionSchema, Parameters.Element));
			break;
		}

		default:
			UE_LEARNING_NOT_IMPLEMENTED();
		}

		return Object;
	}

	const TCHAR* GetDeviceString(const ETrainerDevice Device)
	{
		switch (Device)
		{
		case ETrainerDevice::GPU: return TEXT("GPU");
		case ETrainerDevice::CPU: return TEXT("CPU");
		default: UE_LEARNING_NOT_IMPLEMENTED(); return TEXT("Unknown");
		}
	}

	const TCHAR* GetResponseString(const ETrainerResponse Response)
	{
		switch (Response)
		{
		case ETrainerResponse::Success: return TEXT("Success");
		case ETrainerResponse::Unexpected: return TEXT("Unexpected communication received");
		case ETrainerResponse::Completed: return TEXT("Training completed");
		case ETrainerResponse::Stopped: return TEXT("Training stopped");
		case ETrainerResponse::Timeout: return TEXT("Communication timeout");
		default: UE_LEARNING_NOT_IMPLEMENTED(); return TEXT("Unknown");
		}
	}

	float DiscountFactorFromHalfLife(const float HalfLife, const float DeltaTime)
	{
		return FMath::Pow(0.5f, DeltaTime / FMath::Max(HalfLife, UE_SMALL_NUMBER));
	}

	float DiscountFactorFromHalfLifeSteps(const int32 HalfLifeSteps)
	{
		UE_LEARNING_CHECKF(HalfLifeSteps >= 1, TEXT("Number of HalfLifeSteps should be at least 1 but got %i"), HalfLifeSteps);

		return FMath::Pow(0.5f, 1.0f / FMath::Max(HalfLifeSteps, 1));
	}

	FString GetPythonExecutablePath(const FString& IntermediateDir)
	{
		UE_LEARNING_CHECKF(PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX, TEXT("Python only supported on Windows, Mac, and Linux."));

		return IntermediateDir / TEXT("PipInstall") / (PLATFORM_WINDOWS ? TEXT("Scripts/python.exe") : TEXT("bin/python3"));
	}

	FString GetSitePackagesPath(const FString& EngineDir)
	{
		UE_LEARNING_CHECKF(PLATFORM_WINDOWS || PLATFORM_MAC || PLATFORM_LINUX, TEXT("Python only supported on Windows, Mac, and Linux."));

		return EngineDir / TEXT("Plugins/Experimental/PythonFoundationPackages/Content/Python/Lib") / FPlatformMisc::GetUBTPlatform() / TEXT("site-packages");
	}

	FString GetPythonContentPath(const FString& EngineDir)
	{
		return EngineDir / TEXT("Plugins/Experimental/LearningAgents/Content/Python/");
	}

	FString GetIntermediatePath(const FString& IntermediateDir)
	{
		return IntermediateDir / TEXT("LearningAgents");
	}

}