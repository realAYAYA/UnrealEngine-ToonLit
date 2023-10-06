// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Styling/SlateTypes.h"
#include "UIFWidget.h"

#include "LocalizableMessage.h"

#include "UIFTextBlock.generated.h"

namespace ETextJustify { enum Type : int; }

class UTextBlock;

/**
 *
 */
UCLASS(Abstract, HideDropDown)
class UIFRAMEWORK_API UUIFrameworkTextBase : public UUIFrameworkWidget
{
	GENERATED_BODY()

public:
	UUIFrameworkTextBase();

	void SetMessage(FLocalizableMessage&& InMessage);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	FText GetText() const
	{
		return Text;
	}
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetTextColor(FLinearColor TextColor);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	FLinearColor GetTextColor() const
	{
		return TextColor;
	}

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetJustification(ETextJustify::Type Justification);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	ETextJustify::Type GetJustification() const
	{
		return Justification;
	}
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetOverflowPolicy(ETextOverflowPolicy OverflowPolicy);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	ETextOverflowPolicy GetOverflowPolicy() const
	{
		return OverflowPolicy;
	}

	virtual void LocalOnUMGWidgetCreated() override;

private:
	UFUNCTION()
	void OnRep_Message();
	
	UFUNCTION()
	void OnRep_TextColor();

	UFUNCTION()
	void OnRep_Justification();

	UFUNCTION()
	void OnRep_OverflowPolicy();

	void SetText(const FLocalizableMessage& InMessage);

protected:
	virtual void SetTextToWidget(const FText&)
	{
	}
	virtual void SetTextColorToWidget(FLinearColor)
	{
	}
	virtual void SetJustificationToWidget(ETextJustify::Type)
	{
	}
	virtual void SetOverflowPolicyToWidget(ETextOverflowPolicy)
	{
	}

private:
	UPROPERTY(Transient)
	FText Text;

	UPROPERTY(ReplicatedUsing = OnRep_Message)
	FLocalizableMessage Message;

	UPROPERTY(ReplicatedUsing = OnRep_TextColor)
	FLinearColor TextColor = FLinearColor::Black;

	UPROPERTY(ReplicatedUsing = OnRep_Justification)
	TEnumAsByte<ETextJustify::Type> Justification;

	UPROPERTY(ReplicatedUsing = OnRep_OverflowPolicy)
	ETextOverflowPolicy OverflowPolicy = ETextOverflowPolicy::Clip;
};

/**
 *
 */
UCLASS(DisplayName = "Text Block UIFramework")
class UIFRAMEWORK_API UUIFrameworkTextBlock : public UUIFrameworkTextBase
{
	GENERATED_BODY()

public:
	UUIFrameworkTextBlock();

	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetShadowOffset(FVector2f ShadowOffset);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	FVector2f GetShadowOffset() const
	{
		return ShadowOffset;
	}
	
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category = "UI Framework")
	void SetShadowColor(FLinearColor ShadowColor);

	UFUNCTION(BlueprintCallable, Category = "UI Framework")
	FLinearColor GetShadowColor() const
	{
		return ShadowColor;
	}

	virtual void LocalOnUMGWidgetCreated() override;

private:
	UFUNCTION()
	void OnRep_ShadowOffset();
	UFUNCTION()
	void OnRep_ShadowColor();

protected:
	virtual void SetTextToWidget(const FText&) override;
	virtual void SetJustificationToWidget(ETextJustify::Type) override;
	virtual void SetOverflowPolicyToWidget(ETextOverflowPolicy) override;
	virtual void SetTextColorToWidget(FLinearColor) override;
	virtual void SetShadowOffsetToWidget(FVector2f);
	virtual void SetShadowColorToWidget(FLinearColor);

private:
	UPROPERTY(ReplicatedUsing = OnRep_ShadowOffset)
	FVector2f ShadowOffset = FVector2f::ZeroVector;
	
	UPROPERTY(ReplicatedUsing = OnRep_ShadowColor)
	FLinearColor ShadowColor = FLinearColor::Black;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Framework/Text/TextLayout.h"
#endif
