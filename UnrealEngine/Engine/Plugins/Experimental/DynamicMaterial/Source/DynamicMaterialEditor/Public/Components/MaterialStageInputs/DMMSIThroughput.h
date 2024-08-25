// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialStageInput.h"
#include "DMMSIThroughput.generated.h"

class UDMMaterialStage;
class UDMMaterialStageThroughput;
class UDMMaterialSubStage;
class UDynamicMaterialModel;
struct FDMMaterialBuildState;

UCLASS(Abstract, BlueprintType, ClassGroup = "Material Designer")
class DYNAMICMATERIALEDITOR_API UDMMaterialStageInputThroughput : public UDMMaterialStageInput
{
	GENERATED_BODY()

public:
	static const FString SubStagePathToken;

	virtual FText GetComponentDescription() const override;
	virtual FText GetChannelDescription(const FDMMaterialStageConnectorChannel& Channel) override;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	TSubclassOf<UDMMaterialStageThroughput> GetMaterialStageThroughputClass() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialStageThroughput* GetMaterialStageThroughput() const;

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDMMaterialSubStage* GetSubStage() const { return SubStage; }

	virtual void GenerateExpressions(const TSharedRef<FDMMaterialBuildState>& InBuildState) const override;
	virtual int32 GetInnateMaskOutput(int32 OutputIndex, int32 OutputChannels) const;
	virtual int32 GetOutputChannelOverride(int32 InOutputIndex) const override;

	virtual bool IsPropertyVisible(FName Property) const override;

	//~ Begin UObject
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	//~ End UObject

	//~ Start UDMMaterialComponent
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent

protected:
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Instanced, Category = "Material Designer")
	TObjectPtr<UDMMaterialSubStage> SubStage;

	UDMMaterialStageInputThroughput();

	void OnSubStageUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	void SetMaterialStageThroughputClass(TSubclassOf<UDMMaterialStageThroughput> InMaterialStageThroughputClass);

	void InitSubStage();

	//~ Begin UDMMaterialComponent
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
	virtual void GetComponentPathInternal(TArray<FString>& OutChildComponentPathComponents) const override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
	//~ End UDMMaterialComponent
};
