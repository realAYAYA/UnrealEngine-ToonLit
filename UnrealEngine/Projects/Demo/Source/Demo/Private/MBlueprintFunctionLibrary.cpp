#include "MBlueprintFunctionLibrary.h"

int32 UMBlueprintFunctionLibrary::ConvertFloat4ToRGBA8(FVector4 V)
{
	return (uint32(V.W) & 0x000000FF) << 24U |
		(uint32(V.Z) & 0x000000FF) << 16U |
			(uint32(V.Y) & 0x000000FF) << 8U |
				uint32(V.X) & 0x000000FF;
}

FVector4 UMBlueprintFunctionLibrary::ConvertRGBA8ToFloat4(int32 V)
{
	uint32 Val = V;
	return FVector4(
		float(Val & 0x000000FF),
		float((Val & 0x0000FF00) >> 8U),
		float((Val & 0x00FF0000) >> 16U),
		float((Val & 0xFF000000) >> 24U));
}
