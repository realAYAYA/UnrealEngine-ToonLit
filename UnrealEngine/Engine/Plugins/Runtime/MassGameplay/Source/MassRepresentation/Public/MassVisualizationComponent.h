// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MassRepresentationTypes.h"
#include "Components/ActorComponent.h"
#include "MassRepresentationTypes.h"
#include "Misc/MTAccessDetector.h"

#include "MassVisualizationComponent.generated.h"

/** 
 * This component handles all the static mesh instances for a MassRepresentationProcessor and is an actor component off a MassVisualizer actor.
 * Meant to be created at runtime and owned by an MassVisualizer actor. Will ensure if placed on a different type of actor. 
 */
UCLASS()
class MASSREPRESENTATION_API UMassVisualizationComponent : public UActorComponent
{
	GENERATED_BODY()
public:

	/** 
	 * Get the index of the visual type, will add a new one if does not exist
	 * @param Desc is the information for the visual that will be instantiated later via AddVisualInstance()
	 * @return The index of the visual type 
	 */
	int16 FindOrAddVisualDesc(const FStaticMeshInstanceVisualizationDesc& Desc);

	/** @todo: need to add removal API at some point for visual types */

	/** Get the array of all visual instance informations */
	FMassInstancedStaticMeshInfoArrayView GetMutableVisualInfos()
	{
		FMassInstancedStaticMeshInfoArrayView View = MAKE_MASS_INSTANCED_STATIC_MESH_INFO_ARRAY_VIEW(MakeArrayView(InstancedStaticMeshInfos), InstancedStaticMeshInfosDetector);
		return MoveTemp(View);
	}

	/** Destroy all visual instances */
 	void ClearAllVisualInstances();

	/** Dirty render state on all static mesh components */
 	void DirtyVisuals();

	/** Signal the beginning of the static mesh instance changes, used to prepare the batching update of the static mesh instance transforms*/
	void BeginVisualChanges();

	/** Signal the end of the static mesh instance changes, used to batch apply the transforms on the static mesh instances*/
	void EndVisualChanges();

protected:
	/** Recreate all the static mesh components from the InstancedStaticMeshInfos */
	void ConstructStaticMeshComponents();

	/** Overridden to make sure this component is only added to a MassVisualizer actor */
	virtual void PostInitProperties() override;

	/** The information of all the instanced static meshes */
	UPROPERTY(Transient)
	TArray<FMassInstancedStaticMeshInfo> InstancedStaticMeshInfos;
	UE_MT_DECLARE_RW_RECURSIVE_ACCESS_DETECTOR(InstancedStaticMeshInfosDetector);

	FISMCSharedDataMap ISMCSharedData;


	/** Whether there is a need to create a StaticMeshComponent */
	bool bNeedStaticMeshComponentConstruction = false;
};
