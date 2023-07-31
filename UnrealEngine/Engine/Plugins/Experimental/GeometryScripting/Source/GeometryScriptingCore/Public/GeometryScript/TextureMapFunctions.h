// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "TextureMapFunctions.generated.h"


UENUM(BlueprintType)
enum class EGeometryScriptPixelSamplingMethod : uint8
{
	Bilinear = 0,
	Nearest = 1
};




USTRUCT(BlueprintType)
struct GEOMETRYSCRIPTINGCORE_API FGeometryScriptSampleTextureOptions
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	EGeometryScriptPixelSamplingMethod SamplingMethod = EGeometryScriptPixelSamplingMethod::Bilinear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	bool bWrap = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FVector2D UVScale = FVector2D(1,1);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	FVector2D UVOffset = FVector2D(0,0);
};




UCLASS(meta = (ScriptName = "GeometryScript_TextureUtils"))
class GEOMETRYSCRIPTINGCORE_API UGeometryScriptLibrary_TextureMapFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:

	/**
	 * Sample the the given TextureMap at the list of UV positions and return the color at each position in ColorList output
	 */
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|TextureUtils")
	static void 
	SampleTexture2DAtUVPositions(  
		FGeometryScriptUVList UVList, 
		UTexture2D* Texture,
		FGeometryScriptSampleTextureOptions SampleOptions,
		FGeometryScriptColorList& ColorList,
		UGeometryScriptDebug* Debug = nullptr);


};