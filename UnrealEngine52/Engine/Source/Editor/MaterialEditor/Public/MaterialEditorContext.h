// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MaterialEditorContext.generated.h"

class IMaterialEditor;

UCLASS()
class MATERIALEDITOR_API UMaterialEditorMenuContext : public UObject
{
	GENERATED_BODY()

public:

	TWeakPtr<IMaterialEditor> MaterialEditor;
};
