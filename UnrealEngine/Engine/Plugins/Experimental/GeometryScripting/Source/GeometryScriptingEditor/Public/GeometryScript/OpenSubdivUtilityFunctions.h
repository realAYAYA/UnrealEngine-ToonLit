// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "OpenSubdivUtilityFunctions.generated.h"

UCLASS(meta = (ScriptName = "GeometryScript_OpenSubdiv"))
class GEOMETRYSCRIPTINGEDITOR_API UGeometryScriptLibrary_OpenSubdivFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|OpenSubdiv", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyPolygroupCatmullClarkSubD(
		UDynamicMesh* FromDynamicMesh, 
		int32 Subdivisions,
		FGeometryScriptGroupLayer GroupLayer,
		UGeometryScriptDebug* Debug = nullptr);


	UFUNCTION(BlueprintCallable, Category = "GeometryScript|OpenSubdiv", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	ApplyTriangleLoopSubD(
		UDynamicMesh* FromDynamicMesh, 
		int32 Subdivisions,
		UGeometryScriptDebug* Debug = nullptr);

};