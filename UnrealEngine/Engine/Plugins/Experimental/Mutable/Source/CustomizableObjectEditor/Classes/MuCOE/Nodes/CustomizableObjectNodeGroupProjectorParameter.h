// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNodeProjectorParameter.h"

#include "CustomizableObjectNodeGroupProjectorParameter.generated.h"

class FArchive;
class UCustomizableObjectNodeRemapPins;
class UDataTable;
class UObject;
class UPoseAsset;
class UTexture2D;

USTRUCT()
struct FGroupProjectorParameterImage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	FString OptionName;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	TObjectPtr<UTexture2D> OptionImage = nullptr;
};


USTRUCT()
struct FGroupProjectorParameterPose
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	FString PoseName;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	TObjectPtr<UPoseAsset> OptionPose = nullptr;
};


UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeGroupProjectorParameter : public UCustomizableObjectNodeProjectorParameter
{
public:
	GENERATED_BODY()

	/** Return array with the sticker name and UTexture2D for projection */
	TArray<FGroupProjectorParameterImage> GetOptionImagesFromTable() const;

	/** Returns the final option images without repeated elements in the option names,
	* the data table has preference over elements in the OptionImages array */
	TArray<FGroupProjectorParameterImage> GetFinalOptionImagesNoRepeat() const;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup, Meta = (ToolTip = "Only used when connected to a Group node. Specifies which material channel in the Group node's child material nodes will be connected to the projection."))
	FString MaterialChannelNameToConnect;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup, Meta = (ToolTip = "Only used when connected to a Group node. Specifies which material channel will be used to mask out the projection."))
	FString MaskedOutAreaMaterialChannelName;

	/** Reference Texture used to decide the texture properties of the mutable-generated textures
	* connected to this material. If null, it will try to be guessed at compile time from
	* the graph. */
	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	TObjectPtr<UTexture2D> ReferenceTexture = nullptr;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	int32 ProjectionTextureSize = 512;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup, Meta = (ToolTip = "Specifies at which LOD level the projection texture will not be used and possibly save memory. A negative value means they will never be dropped."))
	int32 DropProjectionTextureAtLOD = -1;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup, Meta = (ToolTip = "If true, projection textures will be shared between LODs of the same object, and will save memory. Only use if all the LODs share the same UV layout."))
	bool bShareProjectionTexturesBetweenLODs = false;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	TArray<FGroupProjectorParameterImage> OptionImages;

	/** Poses will only affect component 0 of the CO */ // TODO UE-206803
	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	TArray<FGroupProjectorParameterPose> OptionPoses;

	/** Name of the column in the Option Images Data Table with the additional option images, (UTexture2D assets). */
	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	FName DataTableTextureColumnName;

	/** Table where additional option images besides Option Images are read. The elements in this table have priority
	* over elements from Option Images in case of duplicity. Use the "Data Table Texture Column Name" property to specify
	* the name of the column where textures are read in the table. */
	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	TObjectPtr<UDataTable> OptionImagesDataTable = nullptr;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	FString AlternateProjectionResolutionStateName;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup)
	float AlternateProjectionResolutionFactor = 0.0f;

	UPROPERTY(EditAnywhere, Category = ProjectorGroup, Meta = (ClampMin = "0"))
	int32 UVLayout = 0;

	// EdGraphNode interface.
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetTooltipText() const override;

	// UCustomizableObjectNode interface.
	virtual void BackwardsCompatibleFixup() override;
	virtual void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins);
};

