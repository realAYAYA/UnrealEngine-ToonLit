// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ViewportToolBarContext.generated.h"

class SViewportToolBar;

UCLASS()
class UNREALED_API UViewportToolBarContext : public UObject
{
	GENERATED_BODY()
public:
	TWeakPtr<SViewportToolBar> ViewportToolBar;
};