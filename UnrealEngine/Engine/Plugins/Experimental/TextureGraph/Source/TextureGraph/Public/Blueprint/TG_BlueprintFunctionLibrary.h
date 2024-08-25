// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/ObjectMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TextureGraph.h"
#include "TG_OutputSettings.h"
#include "TG_BlueprintFunctionLibrary.generated.h"

UCLASS( meta=(ScriptName="TextureScreptingLibrary"))
class UTG_BlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Sets a Texture parameter value on the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category="TextureGraph", meta=(Keywords="SetTextureParameterValue", WorldContext="WorldContextObject"))
	static void SetTextureParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName, UTexture* ParameterValue);

	/** Gets a texture parameter value from the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category="TextureGraph", meta=(Keywords="GetTextureParameterValue", WorldContext="WorldContextObject"))
	static UTexture* GetTextureParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName);

	/** Sets a scalar parameter value on the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category = "TextureGraph", meta = (Keywords = "SetScalarParameterValue", WorldContext = "WorldContextObject"))
	static void SetScalarParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName, float ParameterValue);

	/** Gets a scalar parameter value from the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category = "TextureGraph", meta = (Keywords = "GetScalarParameterValue", WorldContext = "WorldContextObject"))
	static float GetScalarParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName);

	/** Sets a Vector parameter value on the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category = "TextureGraph", meta = (Keywords = "SetVectorParameterValue", WorldContext = "WorldContextObject"))
	static void SetVectorParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName, FLinearColor ParameterValue);

	/** Gets a Vector parameter value from the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category = "TextureGraph", meta = (Keywords = "GetVectorParameterValue", WorldContext = "WorldContextObject"))
	static FLinearColor GetVectorParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName);

	/** Sets a color parameter value on the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category = "TextureGraph", meta = (Keywords = "SetColorParameterValue", WorldContext = "WorldContextObject"))
	static void SetColorParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName, FLinearColor ParameterValue);

	/** Gets a color parameter value from the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category = "TextureGraph", meta = (Keywords = "GetColorParameterValue", WorldContext = "WorldContextObject"))
	static FLinearColor GetColorParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName);

	/** Sets a FTG_OutputSettings parameter value on the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category = "TextureGraph", meta = (Keywords = "SetSettingsParameterValue", WorldContext = "WorldContextObject"))
	static void SetSettingsParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName, int Width,int Height, FName FileName = "None",
		FName Path = "None", ETG_TextureFormat Format = ETG_TextureFormat::BGRA8 , ETG_TexturePresetType TextureType = ETG_TexturePresetType::None,
		TextureGroup LODTextureGroup = TextureGroup::TEXTUREGROUP_World, TextureCompressionSettings Compression = TextureCompressionSettings::TC_Default, bool SRGB = false);

	/** Gets a FTG_OutputSettings parameter value from the TextureGraph instance. Logs if ParameterName is invalid. */
	UFUNCTION(BlueprintCallable, Category = "TextureGraph", meta = (Keywords = "GetOutputSettingsParameterValue", WorldContext = "WorldContextObject"))
	static FTG_OutputSettings GetSettingsParameterValue(UObject* WorldContextObject, UTextureGraph* TextureScript, FName ParameterName , int& Width, int& Height);

	static void AddParamWarning(FName ParamName, UObject* ObjectPtr, FString FunctionName);
	static void AddError(UObject* ObjectPtr, FString FunctionName, FString Error);
};
