// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeImportTestPlan.h"
#include "JsonObjectConverter.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Logging/MessageLog.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeImportTestPlan)

#define LOCTEXT_NAMESPACE "InterchangeImportTestPlan"


bool UInterchangeImportTestPlan::ReadFromJson(const FString& Filename)
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *Filename))
	{
		return false;
	}

	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(JsonReader, JsonObject) || !JsonObject.IsValid())
	{
		return false;
	}

	if (!FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), UInterchangeImportTestPlan::StaticClass(), this))
	{
		return false;
	}

	return true;
}


void UInterchangeImportTestPlan::WriteToJson(const FString& Filename)
{
	FString JsonString;
	if (!FJsonObjectConverter::UStructToJsonObjectString(UInterchangeImportTestPlan::StaticClass(), this, JsonString))
	{
		return;
	}

	FFileHelper::SaveStringToFile(JsonString, *Filename);
}


void UInterchangeImportTestPlan::RunThisTest()
{
	FMessageLog AutomationEditorLog("AutomationTestingLog");
	FString NewPageName = FString::Printf(TEXT("----- Interchange Import Test: %s----"), *GetPathName());
	FText NewPageNameText = FText::FromString(*NewPageName);
	AutomationEditorLog.Open();
	AutomationEditorLog.NewPage(NewPageNameText);
	AutomationEditorLog.Info(NewPageNameText);

	FAutomationTestFramework& TestFramework = FAutomationTestFramework::Get();
	
	TestFramework.StartTestByName(FString(TEXT("FInterchangeImportTest ")) + GetPathName(), 0);

	FAutomationTestExecutionInfo ExecutionInfo;
	if (TestFramework.StopTest(ExecutionInfo))
	{
		AutomationEditorLog.Info(LOCTEXT("TestPassed", "Passed"));
	}
	else
	{
		for (const auto& Entry : ExecutionInfo.GetEntries())
		{
			switch (Entry.Event.Type)
			{
			case EAutomationEventType::Error:
				AutomationEditorLog.Error(FText::FromString(Entry.ToString()));
				break;

			case EAutomationEventType::Warning:
				AutomationEditorLog.Warning(FText::FromString(Entry.ToString()));
				break;

			case EAutomationEventType::Info:
				AutomationEditorLog.Info(FText::FromString(Entry.ToString()));
				break;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

