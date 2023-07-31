// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "HAL/Platform.h"
#include "Internationalization/Text.h"
#include "Math/Color.h"
#include "Misc/Guid.h"
#include "MuCO/CustomizableObjectUIData.h"
#include "MuCOE/Nodes/CustomizableObjectNode.h"
#include "UObject/NameTypes.h"
#include "UObject/ObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectNodeObject.generated.h"

class UCustomizableObject;
class UCustomizableObjectNodeMaterial;
class UCustomizableObjectNodeRemapPins;
class UObject;
struct FPropertyChangedEvent;
struct FSoftObjectPath;


USTRUCT()
struct CUSTOMIZABLEOBJECTEDITOR_API FCustomizableObjectState
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	FString Name;

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TArray<FString> RuntimeParameters;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bDontCompressRuntimeTextures = false;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	bool bBuildOnlyFirstLOD = false;

	UPROPERTY(EditAnywhere, Category = CustomizableObject)
	TMap<FString, FString> ForcedParameterValues;

	UPROPERTY(EditAnywhere, Category = UI, meta = (DisplayName = "State UI Metadata"))
	FMutableParamUIMetadata StateUIMetadata;
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
	ECustomizableObjectAutomaticLODStrategy AutoLODStrategy;

	UPROPERTY(EditAnywhere, Category = CustomizableObject, meta = (ClampMin = "1"))
	int32 NumMeshComponents = 1;

	UPROPERTY(EditAnywhere, Category=CustomizableObject)
	TArray<FCustomizableObjectState> States;

	UPROPERTY(EditAnywhere, Category = AttachedToExternalObject)
	TObjectPtr<UCustomizableObject> ParentObject;

	UPROPERTY(EditAnywhere, Category = AttachedToExternalObject)
	FGuid ParentObjectGroupId;

	UPROPERTY()
	FGuid Identifier;

    // Soft references SkeletalMeshes found in the provoius compilation.
    // Only populated if the node is the root.
    UPROPERTY()
    TArray<FSoftObjectPath> ReferencedSkeletalMeshes;

    // Information about the realtime morph targets usage. It indexes to ReferncedSkeletakMeshes array
    // so it is need to keep them syncronized. 
    // This overrides the per skeletal mesh node selection 
    UPROPERTY()
    TArray<FRealTimeMorphSelectionOverride> RealTimeMorphSelectionOverrides; 
	
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
	virtual void PostBackwardsCompatibleFixup() override;
	void AllocateDefaultPins(UCustomizableObjectNodeRemapPins* RemapPins) override;

	// Own Interface
	void SetParentObject(UCustomizableObject* CustomizableParentObject);

	// Own interface
	UPROPERTY()
	bool bIsBase;

	UEdGraphPin* LODPin(int32 LODIndex) const
	{
		FString LODName = FString::Printf(TEXT("LOD %d "), LODIndex);
		return FindPin(LODName);
	}

	int32 GetNumLODPins() const
	{
		int32 Count = 0;

		for (UEdGraphPin* Pin : GetAllNonOrphanPins())
		{
			if (Pin->GetName().StartsWith(TEXT("LOD ")))
			{
				Count++;
			}
		}

		return Count;
	}

	UEdGraphPin* ChildrenPin() const
	{
		return FindPin(TEXT("Children"));
	}

	UEdGraphPin* OutputPin() const
	{
		return FindPin(TEXT("Object"));
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
};

