// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "MuR/System.h"

#include "CustomizableObjectNodeObject.generated.h"

namespace ENodeTitleType { enum Type : int; }

class UCustomizableObject;
class UCustomizableObjectNodeMaterial;
class UCustomizableObjectNodeRemapPins;
class UObject;
struct FPropertyChangedEvent;
struct FSoftObjectPath;

USTRUCT()
struct FBoneToRemove
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bOnlyRemoveChildren = false;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	FName BoneName;
};


USTRUCT()
struct FLODReductionSettings
{
	GENERATED_USTRUCT_BODY()

	/** Selects which bones will be removed from the final skeleton
	* BoneName: Name of the bone that will be removed. Its children will be removed too.
	* Remove Only Children: If true, only the children of the selected bone will be removed. The selected bone will remain.
	*/
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FBoneToRemove> BonesToRemove;
};


USTRUCT()
struct FComponentSettings
{
	GENERATED_USTRUCT_BODY()
	
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FLODReductionSettings> LODReductionSettings;
};


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	FString Name;

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TArray<FString> RuntimeParameters;

	/** Special treatment of texture compression for this state. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	ETextureCompressionStrategy TextureCompressionStrategy = ETextureCompressionStrategy::None;

	/** If this is enabled, texture streaming won't be used for this state, and full images will be generated when an instance is first updated. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bDisableTextureStreaming = false;

	/** LiveUpdateMode will reuse instance temp. data between updates and speed up update times, but spend much more memory. Good for customization screens, not for actual gameplay modes. */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bLiveUpdateMode = false;

	// Enables the reuse of all possible textures when the instance is updated without any changes in geometry or state (the first update after creation doesn't reuse any)
	// It will only work if the textures aren't compressed, so set the instance to a Mutable state with texture compression disabled
	// WARNING! If texture reuse is enabled, do NOT keep external references to the textures of the instance. The instance owns the textures.
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bReuseInstanceTextures = false;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bBuildOnlyFirstLOD = false;

	/** If there's an entry for a specific platform, when compiling for that platform Num LODs will be built after bBuildOnlyFirstLOD */
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TMap<FString, int32> NumExtraLODsToBuildPerPlatform;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TMap<FString, FString> ForcedParameterValues;

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "State UI Metadata"))
	FMutableParamUIMetadata StateUIMetadata;

	// Deprecated
	
	/** This is now TextureCompressionStrategy.  */
	UPROPERTY()
	bool bDontCompressRuntimeTextures_DEPRECATED = false;
};


UENUM()
enum class ECustomizableObjectAutomaticLODStrategy : uint8
{
	// Use the same strategy than the parent object. If root, then use "Manual".
	Inherited = 0 UMETA(DisplayName = "Inherit from parent object"),
	// Don't try to generate LODs automatically for the child nodes. Only the ones tha explicitely define them will be used.
	Manual = 1 UMETA(DisplayName = "Only manually created LODs"),
	// Try to generate the same material structure than LOd 0 if the source meshes have LODs.
	AutomaticFromMesh = 2 UMETA(DisplayName = "Automatic from mesh")
};

UENUM()
enum class ECustomizableObjectSelectionOverride : uint8
{
    NoOverride = 0 UMETA(DisplayName = "No Override"),
    Disable    = 1 UMETA(DisplayName = "Disable"    ),
    Enable     = 2 UMETA(DisplayName = "Enable"     )
};

USTRUCT()
struct FRealTimeMorphSelectionOverride
{
	GENERATED_BODY()

    FRealTimeMorphSelectionOverride()
    {
    }

    FRealTimeMorphSelectionOverride(const FName& InMorphName)
        : MorphName(InMorphName)
    {
    }

    UPROPERTY()
    FName MorphName;

    UPROPERTY()
    ECustomizableObjectSelectionOverride SelectionOverride = 
        ECustomizableObjectSelectionOverride::NoOverride;

    UPROPERTY()
    TArray<FName> SkeletalMeshesNames;

    UPROPERTY()
    TArray<ECustomizableObjectSelectionOverride> Override;
};

UCLASS()
class CUSTOMIZABLEOBJECTEDITOR_API UCustomizableObjectNodeObject : public UCustomizableObjectNode
{
public:
	GENERATED_BODY()

	UCustomizableObjectNodeObject();

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	FString ObjectName;

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "Parameter UI Metadata"))
	FMutableParamUIMetadata ParamUIMetadata;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	int32 NumLODs;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	ECustomizableObjectAutomaticLODStrategy AutoLODStrategy = ECustomizableObjectAutomaticLODStrategy::AutomaticFromMesh;

	UPROPERTY(EditAnywhere, Category = CustomizableObject, meta = (ClampMin = "1"))
	int32 NumMeshComponents = 1;

	UPROPERTY(EditAnywhere, Category=CustomizableObject ,meta = (TitleProperty = "Name"))
	TArray<FCustomizableObjectState> States;

	UPROPERTY(EditAnywhere, Category = AttachedToExternalObject)
	TObjectPtr<UCustomizableObject> ParentObject;

	UPROPERTY(EditAnywhere, Category = AttachedToExternalObject)
	FGuid ParentObjectGroupId;

	UPROPERTY()
	FGuid Identifier;

    // Soft references SkeletalMeshes found in the previous compilation.
    // Only populated if the node is the root.
    UPROPERTY()
    TArray<FSoftObjectPath> ReferencedSkeletalMeshes;

    // Information about the realtime morph targets usage. It indexes to ReferncedSkeletakMeshes array
    // so it is need to keep them syncronized. 
    // This overrides the per skeletal mesh node selection 
    UPROPERTY()
    TArray<FRealTimeMorphSelectionOverride> RealTimeMorphSelectionOverrides;

	// Array of bones to remove from the mesh.All influences assigned to these bones will be transferred to the closest valid bone.
	// Selected per component and LOD. Bones will be accumulated down the line.
	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TArray<FComponentSettings> ComponentSettings;
	
    // To avoid any no properly saved GUIDs
	FGuid IdentifierVerification;

	// UObject interface.
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	// EdGraphNode interface
	FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	FLinearColor GetNodeTitleColor() const override;
	FText GetTooltipText() const override;
	void PrepareForCopying() override;
	void PostPasteNode() override;
	void PostDuplicate(bool bDuplicateForPIE) override;

	// UCustomizableObjectNode interface
	virtual void BackwardsCompatibleFixup() override;
	virtual void PostBackwardsCompatibleFixup() override;
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// Own Interface
	void SetParentObject(UCustomizableObject* CustomizableParentObject);

	// Own interface
	UPROPERTY()
	bool bIsBase;

	UEdGraphPin* LODPin(int32 LODIndex) const
	{
		FString LODName = FString::Printf(TEXT("%s%d "), LODPinNamePrefix, LODIndex);
		return FindPin(LODName);
	}

	int32 GetNumLODPins() const
	{
		int32 Count = 0;

		for (UEdGraphPin* Pin : GetAllNonOrphanPins())
		{
			if (Pin->GetName().StartsWith(LODPinNamePrefix))
			{
				Count++;
			}
		}

		return Count;
	}

	UEdGraphPin* ChildrenPin() const
	{
		return FindPin(ChildrenPinName);
	}

	UEdGraphPin* OutputPin() const
	{
		return FindPin(OutputPinName);
	}

	/** Return the LOD which a LOD pin references to. Retrun -1 if a pin does not belong to any LOD. */
	int32 GetLOD(UEdGraphPin* Pin) const;

	virtual bool CanUserDeleteNode() const override;
	virtual bool CanDuplicateNode() const override;

	/** Get all Material Nodes int his Customizable Object which belong to the given LOD.
	 *
	 * @param LOD LOD which materials have to belong to.
	 */
	TArray<UCustomizableObjectNodeMaterial*> GetMaterialNodes(int LOD) const;

	bool IsSingleOutputNode() const override;

	void SetMeshComponentNumFromParent(int32 MeshComponentNum)
	{
		NumMeshComponents = MeshComponentNum;
	}

	// Node Details Support
	int32 CurrentComponent = 0;
	int32 CurrentLOD = 0;

private:
	static const FName ChildrenPinName;
	static const FName OutputPinName;
	static const TCHAR* LODPinNamePrefix;

	static bool IsBuiltInPin(FName PinName);
};

