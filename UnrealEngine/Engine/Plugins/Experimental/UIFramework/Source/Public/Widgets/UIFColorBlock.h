// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UIFWidget.h"

#include "UIFColorBlock.generated.h"

class UImage;
class UMaterialInterface;
struct FStreamableHandle;
class UTexture2D;

/**
 *
 */
UCLASS(DisplayName = "Color Block UIFramework")
class UIFRAMEWORK_API UUIFrameworkColorBlock : public UUIFrameworkWidget
{
	GENERATED_BODY()

public:
	UUIFrameworkColorBlock();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetColor(FLinearColor Tint);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	FLinearColor GetColor() const
	{
		return Color;
	}

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetDesiredSize(FVector2f DesiredSize);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	FVector2f GetDesiredSize() const
	{
		return DesiredSize;
	}
	
	virtual void LocalOnUMGWidgetCreated() override;

private:
	UFUNCTION()
	void OnRep_Color();
	
	UFUNCTION()
	void OnRep_DesiredSize();

private:
	UPROPERTY(ReplicatedUsing = OnRep_Color)
	FLinearColor Color;
	
	UPROPERTY(ReplicatedUsing = OnRep_DesiredSize)
	FVector2f DesiredSize;
};

