// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GameFramework/Actor.h"

#include "DMXControlConsoleActor.generated.h"

class UDMXControlConsoleData;

class USceneComponent;


/** Actor class for DMX Control Console */
UCLASS(NotBlueprintable)
class DMXCONTROLCONSOLE_API ADMXControlConsoleActor
	: public AActor
{
	GENERATED_BODY()

public:
	/** Constructor */
	ADMXControlConsoleActor();

	/** Sets the Control Console Data used in this actor */
	void SetDMXControlConsoleData(UDMXControlConsoleData* InDMXControlConsoleData);

	/** Returns the Control Console Data used for this actor */
	UDMXControlConsoleData* GetControlConsoleData() const { return ControlConsoleData; }

	/** Sets current DMX Control Console to start sending DMX data */
	UFUNCTION(BlueprintCallable, Category = "DMX Control Console")
	void StartSendingDMX();

	/** Sets current DMX Control Console to stop sending DMX data */
	UFUNCTION(BlueprintCallable, Category = "DMX Control Console")
	void StopSendingDMX();

#if WITH_EDITOR
	// Property name getters
	static FName GetControlConsoleDataPropertyNameChecked() { return GET_MEMBER_NAME_CHECKED(ADMXControlConsoleActor, ControlConsoleData); }
	static FName GetAutoActivatePropertyNameChecked() { return GET_MEMBER_NAME_CHECKED(ADMXControlConsoleActor, bAutoActivate); }
	static FName GetSendDMXInEditorPropertyNameChecked() { return GET_MEMBER_NAME_CHECKED(ADMXControlConsoleActor, bSendDMXInEditor); }
#endif

protected:
	//~ Begin AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End AActor interface

private:
	/** The Control Console Data used in this actor */
	UPROPERTY(VisibleAnywhere, Category = "DMX Control Console")
	TObjectPtr<UDMXControlConsoleData> ControlConsoleData;

	/** True if the Control Console should send DMX data in runtime */
	UPROPERTY(EditAnywhere, Category = "DMX Control Console")
	bool bAutoActivate = true;

#if WITH_EDITORONLY_DATA
	/** True if the Control Console should send DMX data in Editor */
	UPROPERTY(EditAnywhere, Category = "DMX Control Console", Meta = (DisplayName = "Send DMX in Editor"))
	bool bSendDMXInEditor = false;
#endif // WITH_EDITORONLY_DATA

	/** Scene component to make the Actor easily visible in Editor */
	UPROPERTY(VisibleAnywhere, Category = "Actor", AdvancedDisplay, Meta = (AllowPrivateAccess = true))
	TObjectPtr<USceneComponent> RootSceneComponent;
};
