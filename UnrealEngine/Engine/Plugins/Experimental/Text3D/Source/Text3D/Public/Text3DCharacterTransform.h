// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Text3DCharacterTransform.generated.h"


UENUM()
enum class EText3DCharacterEffectOrder : uint8
{
	Normal			UMETA(DisplayName = "Normal"),
	FromCenter		UMETA(DisplayName = "From Center"),
	ToCenter		UMETA(DisplayName = "To Center"),
	Opposite		UMETA(DisplayName = "Opposite"),
};


UCLASS(ClassGroup=(Text3D), HideCategories = (Collision, Tags, Activation, Cooking), Meta = (BlueprintSpawnableComponent))
class TEXT3D_API UText3DCharacterTransform : public USceneComponent
{
	GENERATED_BODY()

public:	
	UText3DCharacterTransform();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Location

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetLocationEnabled, Category = "Location")
	bool bLocationEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetLocationProgress, Category = "Location", meta = (EditCondition = "bLocationEnabled", ClampMin = 0, ClampMax = 100))
	float LocationProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetLocationOrder, Category = "Location", meta = (EditCondition = "bLocationEnabled"))
	EText3DCharacterEffectOrder LocationOrder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetLocationRange, Category = "Location", meta = (EditCondition = "bLocationEnabled", ClampMin = 0, ClampMax = 100))
	float LocationRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetLocationDistance, Category = "Location", meta = (EditCondition = "bLocationEnabled"))
	FVector LocationDistance;

	UFUNCTION(BlueprintCallable, Category = "Location")
	void SetLocationEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Location")
	void SetLocationProgress(float Progress);

	UFUNCTION(BlueprintCallable, Category = "Location")
	void SetLocationOrder(EText3DCharacterEffectOrder Order);

	UFUNCTION(BlueprintCallable, Category = "Location")
	void SetLocationRange(float Range);

	UFUNCTION(BlueprintCallable, Category = "Location")
	void SetLocationDistance(FVector Distance);


	// Scale

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetScaleEnabled, Category = "Scale")
	bool bScaleEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetScaleProgress, Category = "Scale", meta = (EditCondition = "bScaleEnabled", ClampMin = 0, ClampMax = 100))
	float ScaleProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetScaleOrder, Category = "Scale", meta = (EditCondition = "bScaleEnabled"))
	EText3DCharacterEffectOrder ScaleOrder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetScaleRange, Category = "Scale", meta = (EditCondition = "bScaleEnabled", ClampMin = 0, ClampMax = 100))
	float ScaleRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetScaleBegin, Category = "Scale", meta = (EditCondition = "bScaleEnabled", ClampMin = 0))
	FVector ScaleBegin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetScaleEnd, Category = "Scale", meta = (EditCondition = "bScaleEnabled", ClampMin = 0))
	FVector ScaleEnd;


	UFUNCTION(BlueprintCallable, Category = "Scale")
	void SetScaleEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Scale")
	void SetScaleProgress(float Progress);

	UFUNCTION(BlueprintCallable, Category = "Scale")
	void SetScaleOrder(EText3DCharacterEffectOrder Order);

	UFUNCTION(BlueprintCallable, Category = "Scale")
	void SetScaleRange(float Range);

	UFUNCTION(BlueprintCallable, Category = "Scale")
	void SetScaleBegin(FVector Value);

	UFUNCTION(BlueprintCallable, Category = "Scale")
	void SetScaleEnd(FVector Value);


	// Rotate

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetRotateEnabled, Category = "Rotate")
	bool bRotateEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetRotateProgress, Category = "Rotate", meta = (EditCondition = "bRotateEnabled", ClampMin = 0, ClampMax = 100))
	float RotateProgress;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetRotateOrder, Category = "Rotate", meta = (EditCondition = "bRotateEnabled"))
	EText3DCharacterEffectOrder RotateOrder;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetRotateRange, Category = "Rotate", meta = (EditCondition = "bRotateEnabled", ClampMin = 0, ClampMax = 100))
	float RotateRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetRotateBegin, Category = "Rotate", meta = (EditCondition = "bRotateEnabled"))
	FRotator RotateBegin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, BlueprintSetter = SetRotateEnd, Category = "Rotate", meta = (EditCondition = "bRotateEnabled"))
	FRotator RotateEnd;


	UFUNCTION(BlueprintCallable, Category = "Rotate")
	void SetRotateEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Rotate")
	void SetRotateProgress(float Progress);

	UFUNCTION(BlueprintCallable, Category = "Rotate")
	void SetRotateOrder(EText3DCharacterEffectOrder Order);

	UFUNCTION(BlueprintCallable, Category = "Rotate")
	void SetRotateRange(float Range);

	UFUNCTION(BlueprintCallable, Category = "Rotate")
	void SetRotateBegin(FRotator Value);

	UFUNCTION(BlueprintCallable, Category = "Rotate")
	void SetRotateEnd(FRotator Value);

private:
	class UText3DComponent* GetText3DComponent();

	void ProcessEffect();

	float GetEffectValue(int32 Index, int32 Total, EText3DCharacterEffectOrder Order, float Progress, float Range);
	void GetLineParameters(float Range, EText3DCharacterEffectOrder Order, int32 Count, float & EffectOut, float & StripOut);
	int32 GetEffectPosition(int32 Index, int32 Total, EText3DCharacterEffectOrder Order);

	void ResetLocation();
	void ResetRotate();
	void ResetScale();

	bool bInitialized;
};
