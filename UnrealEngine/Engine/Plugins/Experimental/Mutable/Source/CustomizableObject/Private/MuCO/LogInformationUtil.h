// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/Platform.h"

class FString;
class UCustomizableObjectInstance;
struct FTexturePlatformData;

#define STRINGIZE(A) STRINGIZE_NX(A)
#define STRINGIZE_NX(A) #A
#define ADD_INTEGER_LOG_VARIABLE(String, info, booleanValue) {String += FString::Printf(TEXT(STRINGIZE_NX(info %d)), #booleanValue);}
#define ADD_FLOAT_LOG_VARIABLE(String, info, floatValue) {String += FString::Printf(TEXT(STRINGIZE_NX(info %d)), #floatValue);}
#define ADD_STRING_LOG_VARIABLE(String, info, stringValue) {String += FString::Printf(TEXT(STRINGIZE_NX(info %s)), #stringValue);}
#define ADD_STRING_LOG(String, stringValue) {String += FString::Printf(TEXT(STRINGIZE_NX(stringValues)), #stringValue);}

class LogInformationUtil
{
public:
	// Show full data about all UCustomizableObjectInstance existing elements
	static void LogShowInstanceDataFull(const UCustomizableObjectInstance* CustomizableObjectInstance, bool ShowMaterialInfo);

	// Show data about all UCustomizableObjectInstance existing elements
	static void LogShowInstanceData(const UCustomizableObjectInstance* CustomizableObjectInstance, FString& LogData);

	// Utility method just to log information, adds to the given FString containing the
	// log message the log information of BoolParameters
	static void PrintBoolParameters(const TArray<struct FCustomizableObjectBoolParameterValue>& BoolParameters, FString& Log);

	// Utility method just to log information, adds to the given FString containing the
	// log message the log information of IntParameters
	static void PrintIntParameters(const TArray<struct FCustomizableObjectIntParameterValue>& IntParameters, FString& Log);

	// Utility method just to log information, adds to the given FString containing the
	// log message the log information of FloatParameters
	static void PrintFloatParameters(const TArray<struct FCustomizableObjectFloatParameterValue>& FloatParameters, FString& Log);

	// Utility method just to log information, adds to the given FString containing the
	// log message the log information of VectorParameters
	static void PrintVectorParameters(const TArray<struct FCustomizableObjectVectorParameterValue>& VectorParameters, FString& Log);

	// Utility method just to log information, adds to the given FString containing the
	// log message the log information of ProjectorParameters
	static void PrintProjectorParameters(const TArray<struct FCustomizableObjectProjectorParameterValue>& ProjectorParameters, FString& Log);

	// Utility method just to log information, adds to the given FString containing the
	// log message the log information of GeneratedMaterials
	static void PrintGeneratedMaterial(const TArray<struct FGeneratedMaterial> GeneratedMaterials, FString& Log);

	// Utility method just to log information, adds to the given FString containing the
	// log message the log information of GeneratedTextures
	static void PrintGeneratedTextures(const TArray<struct FGeneratedTexture> GeneratedTextures, FString& Log, bool DoPrintInitialMessage);

	// Utility method just to log information, adds to the given FString containing the
	// log message the log information of Texture
	static void PrintTextureData(const class UTexture2D* Texture, FString& Log, bool UseTabulation);

	// Reset static lod counters
	static void ResetCounters();

	// Fills with empty spaces the Data string so it reaches a kength of NewLength (is lower than current length, nothing is done)
	static void FillToLength(FString& Data, int32 NewLength);

	// Utility method just to log information, adds to the given FString containing the
	// log message the log information of ImageToPlatformDataMap
	static void PrintImageToPlatformDataMap(const TMap<uint32, FTexturePlatformData*>& ImageToPlatformDataMap, FString& Log);

	static int CountLOD0;
	static int CountLOD1;
	static int CountLOD2;
};
