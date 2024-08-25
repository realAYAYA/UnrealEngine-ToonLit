// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"
#include "StateTreeEditorSettings.generated.h"

UENUM()
enum class EStateTreeSaveOnCompile : uint8
{
	Never UMETA(DisplayName = "Never"),
	SuccessOnly UMETA(DisplayName = "On Success Only"),
	Always UMETA(DisplayName = "Always"),
};

UCLASS(config = EditorPerProjectUserSettings)
class STATETREEEDITORMODULE_API UStateTreeEditorSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()
public:
	static UStateTreeEditorSettings& Get() { return *CastChecked<UStateTreeEditorSettings>(UStateTreeEditorSettings::StaticClass()->GetDefaultObject()); }

	/** Determines when to save StateTrees post-compile */
	UPROPERTY(EditAnywhere, config, Category = "Compiler")
	EStateTreeSaveOnCompile SaveOnCompile = EStateTreeSaveOnCompile::Never;

	/** If enabled, debugger starts recording information at the start of each PIE session. */
	UPROPERTY(EditAnywhere, config, Category = "Debugger")
	bool bShouldDebuggerAutoRecordOnPIE = true;

	/** If enabled, debugger will clear previous tracks at the start of each PIE session. */
	UPROPERTY(EditAnywhere, config, Category = "Debugger")
	bool bShouldDebuggerResetDataOnNewPIESession = false;

	/**
	 * If enabled, changing the class of a node will try to copy over values of properties with the same name and type.
	 * i.e. if you change one condition for another, and both have a "Target" BB key selector, it'll be kept.
	 */
	UPROPERTY(EditAnywhere, config, Experimental, Category = "Experimental")
	bool bRetainNodePropertyValues = false;

protected:
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	virtual FName GetCategoryName() const override;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
