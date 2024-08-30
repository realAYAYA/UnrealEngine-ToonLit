#pragma once
#include "MBlueprintFunctionLibrary.generated.h"

UCLASS()
class UMBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{

	GENERATED_BODY()

	
public:


	UFUNCTION(BlueprintCallable, Category = "TestM")
	static int32 ConvertFloat4ToRGBA8(FVector4 V);

	UFUNCTION(BlueprintCallable, Category = "TestM")
	static FVector4 ConvertRGBA8ToFloat4(int32 V);
	
};
