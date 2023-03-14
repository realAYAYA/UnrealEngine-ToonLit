// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraComponent.h"
#include "CoreMinimal.h"
#include "Roles/LiveLinkCameraTypes.h"

#include "VCamModifier.generated.h"

class UInputAction;
class UInputMappingContext;
class UVCamComponent;
class UVCamModifierContext;
class UInputComponent;

struct FModifierStackEntry;

USTRUCT(BlueprintType)
struct VCAMCORE_API FVCamModifierConnectionPoint
{
	GENERATED_BODY()

	// An optional action to associate with this connection point
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
	TObjectPtr<UInputAction> AssociatedAction;
};

UCLASS(Blueprintable, Abstract, EditInlineNew)
class VCAMCORE_API UVCamModifier : public UObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(UVCamModifierContext* Context, UInputComponent* InputComponent = nullptr);
	
	virtual void Deinitialize();

	virtual void Apply(UVCamModifierContext* Context, UCineCameraComponent* CameraComponent, const float DeltaTime) {};

	virtual void PostLoad() override;

	bool DoesRequireInitialization() const { return bRequiresInitialization; };

	UFUNCTION(BlueprintCallable, Category="VirtualCamera")
	UVCamComponent* GetOwningVCamComponent() const;

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void GetCurrentLiveLinkDataFromOwningComponent(FLiveLinkCameraBlueprintData& LiveLinkData);

	UFUNCTION(BlueprintCallable, Category = "VirtualCamera")
	void SetEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintPure, Category = "VirtualCamera", meta=(ReturnDisplayName="Enabled"))
	bool IsEnabled() const;

	// Sets the name of the modifier in the associated modifier stack
	// Returns a bool for whether 
	UFUNCTION(BlueprintCallable, Category = "VirtualCamera", meta=(ReturnDisplayName="Success"))
	bool SetStackEntryName(FName NewName);

	// Gets the name of the modifier in the associated modifier stack
	UFUNCTION(BlueprintPure, Category = "VirtualCamera", meta=(ReturnDisplayName="Name"))
	FName GetStackEntryName() const;

	// If an Input Mapping Context is specified then that Context will be automatically added to the input system when this Modifier is Initialized 
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VCam Input", meta=(EditCondition="bRegisterForInput"))
	TObjectPtr<UInputMappingContext> InputMappingContext = nullptr;

	// If an Input Mapping Context is provided then this value defines the priority level that the context is added to the input system with
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="VCam Input", meta=(EditCondition="bRegisterForInput"))
	int32 InputContextPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VCam Connection Points")
	TMap<FName, FVCamModifierConnectionPoint> ConnectionPoints;

	virtual UWorld* GetWorld() const override;
	
private:
	FModifierStackEntry* GetCorrespondingStackEntry() const;

	bool bRequiresInitialization = true;
};

UCLASS(EditInlineNew)
class VCAMCORE_API UVCamBlueprintModifier : public UVCamModifier
{
	GENERATED_BODY()

public:
	virtual void Initialize(UVCamModifierContext* Context, UInputComponent* InputComponent=nullptr) override;
	virtual void Deinitialize() override;
	virtual void Apply(UVCamModifierContext* Context, UCineCameraComponent* CameraComponent, const float DeltaTime) override;
	
	UFUNCTION(BlueprintImplementableEvent, Category="VirtualCamera")
	void OnInitialize(UVCamModifierContext* Context);

	UFUNCTION(BlueprintImplementableEvent, Category="VirtualCamera")
	void OnDeinitialize();

	UFUNCTION(BlueprintImplementableEvent, Category="VirtualCamera")
	void OnApply(UVCamModifierContext* Context, UCineCameraComponent* CameraComponent, const float DeltaTime);

	// This function is deliberately non-working to force cleanup of Input Contexts
	// Please move any previous values to the new properties in Class Defaults
	UFUNCTION(BlueprintImplementableEvent, Category="VirtualCamera")
	void GetInputMappingContextAndPriority() const;
};