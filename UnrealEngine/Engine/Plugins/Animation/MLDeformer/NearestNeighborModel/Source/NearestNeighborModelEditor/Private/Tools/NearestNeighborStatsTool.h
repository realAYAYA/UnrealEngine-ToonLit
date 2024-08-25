// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "NearestNeighborMenuTool.h"

#include "NearestNeighborStatsTool.generated.h"

class UAnimSequence;
class UMLDeformerAsset;
namespace UE::MLDeformer
{
	class FMLDeformerEditorModel;
	class FMLDeformerEditorToolkit;
};

UCLASS(Blueprintable)
class NEARESTNEIGHBORMODELEDITOR_API UNearestNeighborStatsData : public UObject
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category = "Input")
	TObjectPtr<const UMLDeformerAsset> NearestNeighborModelAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UAnimSequence> TestAnim; 

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	int32 SectionIndex = 0;
};

namespace UE::NearestNeighborModel
{
	class FNearestNeighborStatsTool : public FNearestNeighborMenuTool
	{
	public:
		virtual ~FNearestNeighborStatsTool() = default;
		virtual FName GetToolName() override;
		virtual FText GetToolTip() override;
		virtual UObject* CreateData() override;
		virtual void InitData(UObject& Data, UE::MLDeformer::FMLDeformerEditorToolkit& Toolkit) override;
		virtual TSharedRef<SWidget> CreateAdditionalWidgets(UObject& Data, TWeakPtr<UE::MLDeformer::FMLDeformerEditorModel> InEditorModel) override;
	};
};