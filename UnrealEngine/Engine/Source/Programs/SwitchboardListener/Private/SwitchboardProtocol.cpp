// Copyright Epic Games, Inc. All Rights Reserved.

#include "SwitchboardProtocol.h"

#include "SwitchboardPacket.h"
#include "SwitchboardTasks.h"
#include "SyncStatus.h"

#include "Dom/JsonValue.h"
#include "JsonObjectConverter.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"


DECLARE_LOG_CATEGORY_EXTERN(LogSwitchboardProtocol, Verbose, All);
DEFINE_LOG_CATEGORY(LogSwitchboardProtocol);


FString CreateMessage(const FString& InStateDescription, bool bInState, const TMap<FString, FString>& InAdditionalFields)
{
	FString Message;
	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Message);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(InStateDescription, bInState); // TODO: Phase out this field and replace with two below because parser now needs to check all possibilities.
	JsonWriter->WriteValue(TEXT("command"), InStateDescription);
	JsonWriter->WriteValue(TEXT("bAck"), bInState);
	for (const auto& Value : InAdditionalFields)
	{
		JsonWriter->WriteValue(Value.Key, Value.Value);
	}
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();
	return Message;
}

FString CreateTaskDeclinedMessage(const FSwitchboardTask& InTask, const FString& InErrorMessage, const TMap<FString, FString>& InAdditionalFields)
{
	TMap<FString, FString> AdditionalFields = InAdditionalFields;

	AdditionalFields.Add(TEXT("id"), InTask.TaskID.ToString());
	AdditionalFields.Add(TEXT("error"), InErrorMessage);

	return CreateMessage(InTask.Name, false, AdditionalFields);
}

FString CreateCommandAcceptedMessage(const FGuid& InMessageID)
{
	return CreateMessage(TEXT("command accepted"), true, { { TEXT("id"), InMessageID.ToString() } });
}

FString CreateCommandDeclinedMessage(const FGuid& InMessageID, const FString& InErrorMessage)
{
	return CreateMessage(TEXT("command accepted"), false, { { TEXT("id"), InMessageID.ToString() }, {TEXT("error"), InErrorMessage} });
}

FString CreateSyncStatusMessage(const FSyncStatus& SyncStatus)
{
	FString SyncStatusJsonString;
	const bool bJsonStringOk = FJsonObjectConverter::UStructToJsonObjectString(SyncStatus, SyncStatusJsonString);

	check(bJsonStringOk);

	FString Message;

	TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Message);
	JsonWriter->WriteObjectStart();
	JsonWriter->WriteValue(TEXT("get sync status"), true); // TODO: Phase out this field and replace with two below because parser now needs to check all possibilities.
	JsonWriter->WriteValue(TEXT("command"), TEXT("get sync status"));
	JsonWriter->WriteValue(TEXT("bAck"), true);
	JsonWriter->WriteRawJSONValue(TEXT("syncStatus"), SyncStatusJsonString);
	JsonWriter->WriteObjectEnd();
	JsonWriter->Close();

	return Message;
}


FString CreateReceiveFileFromClientCompletedMessage(const FString& InDestinationPath)
{
	return CreateMessage(TEXT("send file complete"), true, { { TEXT("destination"), InDestinationPath } });
}

FString CreateReceiveFileFromClientFailedMessage(const FString& InDestinationPath, const FString& InError)
{
	return CreateMessage(TEXT("send file complete"), false, { { TEXT("destination"), InDestinationPath }, { TEXT("error"), InError } });
}

FString CreateSendFileToClientCompletedMessage(const FString& InSourcePath, const FString& InFileContent)
{
	return CreateMessage(TEXT("receive file complete"), true, { { TEXT("source"), InSourcePath }, { TEXT("content"), InFileContent } });
}

FString CreateSendFileToClientFailedMessage(const FString& InSourcePath, const FString& InError)
{
	return CreateMessage(TEXT("receive file complete"), false, { { TEXT("source"), InSourcePath }, { TEXT("error"), InError } });
}

FString CreateRedeployStatusMessage(const FGuid& InMessageID, bool bAck, const FString& Status)
{
	return CreateMessage(TEXT("redeploy server status"), bAck, { { TEXT("id"), InMessageID.ToString() }, { TEXT("status"), Status } });
}


// This logs an error with some context in the event the field is missing.
TSharedPtr<FJsonValue> TryGetCommandRequiredField(const TSharedPtr<FJsonObject>& CommandObj, const FString& FieldName)
{
	TSharedPtr<FJsonValue> Field = CommandObj->TryGetField(FieldName);
	if (!Field)
	{
		UE_LOG(LogSwitchboardProtocol, Error, TEXT("\"%s\" command missing required field \"%s\""),
			*CommandObj->GetStringField(TEXT("command")).ToLower(), *FieldName);
	}
	return Field;
}

bool CreateTaskFromCommand(const FString& InCommand, const FIPv4Endpoint& InEndpoint, TUniquePtr<FSwitchboardTask>& OutTask, bool& bOutEcho)
{
	TSharedRef<TJsonReader<TCHAR>> Reader = FJsonStringReader::Create(InCommand);

	TSharedPtr<FJsonObject> JsonData;

	if (!FJsonSerializer::Deserialize(Reader, JsonData))
	{
		return false;
	}

	TSharedPtr<FJsonValue> CommandField = JsonData->TryGetField(TEXT("command"));
	TSharedPtr<FJsonValue> IdField = JsonData->TryGetField(TEXT("id"));

	if (!CommandField.IsValid() || !IdField.IsValid())
	{
		return false;
	}

	FGuid MessageID;

	if (!FGuid::Parse(IdField->AsString(), MessageID))
	{
		return false;
	}

	// Should we echo this command in the output log?
	{
		TSharedPtr<FJsonValue> EchoField = JsonData->TryGetField(TEXT("bEcho"));
		bOutEcho = EchoField.IsValid() ? EchoField->AsBool() : true;
	}

	const FString CommandName = CommandField->AsString().ToLower();
	if (CommandName == TEXT("start"))
	{
		TSharedPtr<FJsonValue> ExeField = TryGetCommandRequiredField(JsonData, TEXT("exe"));
		TSharedPtr<FJsonValue> ArgsField = TryGetCommandRequiredField(JsonData, TEXT("args"));
		TSharedPtr<FJsonValue> NameField = TryGetCommandRequiredField(JsonData, TEXT("name"));
		TSharedPtr<FJsonValue> CallerField = TryGetCommandRequiredField(JsonData, TEXT("caller"));
		TSharedPtr<FJsonValue> WorkingDirField = TryGetCommandRequiredField(JsonData, TEXT("working_dir"));

		if (!ExeField || !ArgsField || !NameField || !CallerField || !WorkingDirField)
		{
			return false;
		}

		TUniquePtr<FSwitchboardStartTask> Task = MakeUnique<FSwitchboardStartTask>(
			MessageID,
			InEndpoint,
			ExeField->AsString(),
			ArgsField->AsString(),
			NameField->AsString(),
			CallerField->AsString(),
			WorkingDirField->AsString()
		);

		if (TSharedPtr<FJsonValue> UpdateClientsWithStdoutField = JsonData->TryGetField(TEXT("bUpdateClientsWithStdout")))
		{
			UpdateClientsWithStdoutField->TryGetBool(Task->bUpdateClientsWithStdout);
		}

		if (TSharedPtr<FJsonValue> PriorityModifierField = JsonData->TryGetField(TEXT("priority_modifier")))
		{
			PriorityModifierField->TryGetNumber(Task->PriorityModifier);
		}

		OutTask = MoveTemp(Task);
		return true;
	}
	else if (CommandName == TEXT("kill"))
	{
		TSharedPtr<FJsonValue> UUIDField = TryGetCommandRequiredField(JsonData, TEXT("uuid"));

		FGuid ProgramID;
		if (UUIDField.IsValid() && FGuid::Parse(UUIDField->AsString(), ProgramID))
		{
			OutTask = MakeUnique<FSwitchboardKillTask>(MessageID, InEndpoint, ProgramID);
			return true;
		}
	}
	else if (CommandName == TEXT("send file"))
	{
		TSharedPtr<FJsonValue> DestinationField = TryGetCommandRequiredField(JsonData, TEXT("destination"));
		TSharedPtr<FJsonValue> FileContentField = TryGetCommandRequiredField(JsonData, TEXT("content"));
		
		if (!DestinationField || !FileContentField)
		{
			return false;
		}

		TUniquePtr<FSwitchboardReceiveFileFromClientTask> Task = MakeUnique<FSwitchboardReceiveFileFromClientTask>(
			MessageID,
			InEndpoint,
			DestinationField->AsString(),
			FileContentField->AsString()
		);

		if (TSharedPtr<FJsonValue> ForceOverwriteField = JsonData->TryGetField(TEXT("force_overwrite")))
		{
			ForceOverwriteField->TryGetBool(Task->bForceOverwrite);
		}

		OutTask = MoveTemp(Task);
		return true;
	}
	else if (CommandName == TEXT("receive file"))
	{
		TSharedPtr<FJsonValue> SourceField = TryGetCommandRequiredField(JsonData, TEXT("source"));

		if (SourceField.IsValid())
		{
			OutTask = MakeUnique<FSwitchboardSendFileToClientTask>(MessageID, InEndpoint, SourceField->AsString());
			return true;
		}
	}
	else if (CommandName == TEXT("disconnect"))
	{
		OutTask = MakeUnique<FSwitchboardDisconnectTask>(MessageID, InEndpoint);
		return true;
	}
	else if (CommandName == TEXT("keep alive"))
	{
		OutTask = MakeUnique<FSwitchboardKeepAliveTask>(MessageID, InEndpoint);
		return true;
	}
	else if (CommandName == TEXT("get sync status"))
	{
		TSharedPtr<FJsonValue> UUIDField = TryGetCommandRequiredField(JsonData, TEXT("uuid"));

		FGuid ProgramID;
		if (UUIDField.IsValid() && FGuid::Parse(UUIDField->AsString(), ProgramID))
		{
			OutTask = MakeUnique<FSwitchboardGetSyncStatusTask>(MessageID, InEndpoint, ProgramID);
			return true;
		}
	}
	else if (CommandName == TEXT("redeploy listener"))
	{
		TSharedPtr<FJsonValue> Sha1Field = TryGetCommandRequiredField(JsonData, TEXT("sha1"));
		TSharedPtr<FJsonValue> FileContentField = TryGetCommandRequiredField(JsonData, TEXT("content"));

		if (Sha1Field.IsValid() && FileContentField.IsValid())
		{
			OutTask = MakeUnique<FSwitchboardRedeployListenerTask>(MessageID, InEndpoint, Sha1Field->AsString(), FileContentField->AsString());
			return true;
		}
	}
	else if (CommandName == TEXT("fixExeFlags"))
	{
		TSharedPtr<FJsonValue> UUIDField = TryGetCommandRequiredField(JsonData, TEXT("uuid"));

		FGuid ProgramID;
		if (UUIDField.IsValid() && FGuid::Parse(UUIDField->AsString(), ProgramID))
		{
			OutTask = MakeUnique<FSwitchboardFixExeFlagsTask>(MessageID, InEndpoint, ProgramID);
			return true;
		}
	}
	else if (CommandName == TEXT("refresh mosaics"))
	{
		OutTask = MakeUnique<FSwitchboardRefreshMosaicsTask>(MessageID, InEndpoint);
		return true;
	}
	else if (CommandName == TEXT("minimize windows"))
	{
		OutTask = MakeUnique<FSwitchboardMinimizeWindowsTask>(MessageID, InEndpoint);
		return true;
	}
	else if (CommandName == TEXT("set inactive timeout"))
	{
		TSharedPtr<FJsonValue> SecondsField = TryGetCommandRequiredField(JsonData, TEXT("seconds"));

		if (SecondsField.IsValid())
		{
			OutTask = MakeUnique<FSwitchboardSetInactiveTimeoutTask>(MessageID, InEndpoint, SecondsField->AsNumber());
			return true;
		}
	}

	return false;
}
