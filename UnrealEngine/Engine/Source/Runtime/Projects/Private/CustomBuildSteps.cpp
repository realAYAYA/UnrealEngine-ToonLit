// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomBuildSteps.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "ModuleDescriptor"

bool FCustomBuildSteps::IsEmpty() const
{
	return HostPlatformToCommands.Num() == 0;
}

void FCustomBuildSteps::Read(const FJsonObject& Object, const FString& FieldName)
{
	TSharedPtr<FJsonValue> StepsValue = Object.TryGetField(FieldName);
	if(StepsValue.IsValid() && StepsValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject>& StepsObject = StepsValue->AsObject();
		for(const TPair<FString, TSharedPtr<FJsonValue>>& HostPlatformAndSteps : StepsObject->Values)
		{
			TArray<FString>& Commands = HostPlatformToCommands.FindOrAdd(HostPlatformAndSteps.Key);
			if(HostPlatformAndSteps.Value.IsValid() && HostPlatformAndSteps.Value->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>>& CommandsArray = HostPlatformAndSteps.Value->AsArray();
				for(const TSharedPtr<FJsonValue>& CommandValue: CommandsArray)
				{
					if(CommandValue->Type == EJson::String)
					{
						Commands.Add(CommandValue->AsString());
					}
				}
			}
		}
	}
}

void FCustomBuildSteps::Write(TJsonWriter<>& Writer, const FString& FieldName) const
{
	if (!IsEmpty())
	{
		TSharedRef<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		UpdateJson(*JsonObject, FieldName);

		if (TSharedPtr<FJsonValue> StepsValue = JsonObject->TryGetField(FieldName))
		{
			FJsonSerializer::Serialize(StepsValue, FieldName, Writer);
		}
	}
}

void FCustomBuildSteps::UpdateJson(FJsonObject& JsonObject, const FString& FieldName) const
{
	if (!IsEmpty())
	{
		TSharedPtr<FJsonObject> StepsObject;
		{
			const TSharedPtr<FJsonObject>* StepsObjectPtr = nullptr;
			if (JsonObject.TryGetObjectField(FieldName, StepsObjectPtr) && StepsObjectPtr)
			{
				StepsObject = *StepsObjectPtr;
			}
			else
			{
				StepsObject = MakeShared<FJsonObject>();
				JsonObject.SetObjectField(FieldName, StepsObject);
			}
		}

		if (ensure(StepsObject.IsValid()))
		{
			for (const TPair<FString, TArray<FString>>& HostPlatformAndCommands : HostPlatformToCommands)
			{
				TArray<TSharedPtr<FJsonValue>> CommandValues;

				for (const FString& Command : HostPlatformAndCommands.Value)
				{
					CommandValues.Add(MakeShareable(new FJsonValueString(Command)));
				}

				StepsObject->SetArrayField(HostPlatformAndCommands.Key, CommandValues);
			}
		}
	}
	else
	{
		JsonObject.RemoveField(FieldName);
	}
}

#undef LOCTEXT_NAMESPACE

