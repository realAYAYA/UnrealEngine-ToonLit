// Copyright Epic Games, Inc. All Rights Reserved.

#include "SlateTestHelper.h"
#include "CoreMinimal.h"
#include "Rendering/DrawElements.h"
#include "Rendering/DrawElementPayloads.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace UE::SlateWidgetAutomationTest
{
bool FSlateTestHelper::WriteJSONToTxt(TSharedRef<FJsonObject> ObjToWrite, const FString& TestName)
{
	FString JsonString;
	TSharedRef<TJsonWriter<>> JsonWriter = TJsonWriterFactory<>::Create(&JsonString);
	if (FJsonSerializer::Serialize(ObjToWrite, JsonWriter, true))
	{
		return FFileHelper::SaveStringToFile(JsonString, *FSlateTestHelper::GetFullPath(TestName));

	}
	return false;
}

TSharedPtr<FJsonObject> FSlateTestHelper::LoadJSONFromTxt(const FString& TestName)
{
	FString JsonString;
	TSharedPtr<FJsonObject> OutJsonObject;

	//load file and convert to JSON
	if (FFileHelper::LoadFileToString(JsonString, *FSlateTestHelper::GetFullPath(TestName)))
	{
		TSharedRef<TJsonReader<>> JsonReader = TJsonReaderFactory<>::Create(JsonString);
		FJsonSerializer::Deserialize(JsonReader, OutJsonObject);
	}

	return OutJsonObject;
}

FString FSlateTestHelper::GetFullPath(FStringView TestName)
{
	FString FileLocation = FPaths::ConvertRelativePathToFull(FPaths::EnginePluginsDir()) + TEXT("Tests/WidgetAutomationTests/Resources/TestBaselineFiles/");
	FString FullPath = FString::Printf(TEXT("%sUIAutomation_%s.json"), *FileLocation, TestName.GetData());
	return FullPath;
}

void FSlateTestHelper::ToJson(const FGeometry& MyGeometry, TSharedPtr<FJsonObject> MyJsonObject)
{
	MyJsonObject->SetStringField("AllottedGeometry", MyGeometry.ToString());
}

void FSlateTestHelper::ToJson(const FWidgetStyle& InWidgetStyle, TSharedPtr<FJsonObject> MyJsonObject)
{
	TSharedPtr<FJsonObject> InnerJsonObject = MakeShared<FJsonObject>();
	InnerJsonObject->SetStringField("ColorAndOpacityTint", InWidgetStyle.GetColorAndOpacityTint().ToString());
	InnerJsonObject->SetStringField("ForegroundColor", InWidgetStyle.GetForegroundColor().ToString());
	InnerJsonObject->SetStringField("SubduedForeground", InWidgetStyle.GetSubduedForegroundColor().ToString());
	MyJsonObject->SetObjectField("WidgetStyle", InnerJsonObject);
}

void FSlateTestHelper::ToJson(const FSlateShapedTextElement& ShapedTextElement, TSharedPtr<FJsonObject> MyJsonObject)
{
	TSharedPtr<FJsonObject> InnerJsonObject = MakeShared<FJsonObject>();
	InnerJsonObject->SetStringField("Tint", ShapedTextElement.GetTint().ToString());
	InnerJsonObject->SetStringField("OutlineTint", ShapedTextElement.GetOutlineTint().ToString());
	MyJsonObject->SetObjectField("ShapedTextElement", InnerJsonObject);
}
}
#endif