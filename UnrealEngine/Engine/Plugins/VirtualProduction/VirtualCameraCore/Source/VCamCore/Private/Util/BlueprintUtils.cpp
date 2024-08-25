// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlueprintUtils.h"

#include "Output/VCamOutputProviderBase.h"
#include "VCamComponent.h"

namespace UE::VCamCore
{
	static bool CanInitVCamObject(UObject* Component)
	{
		/*
		 * Other GWorld unrelated UWorld assets might get as part of complex editor operations.
		 * The actors in such worlds are usually not consciously being worked on by the user.
		 * If there is a VCam instance in such a world, it would register to Live Link and lock up the viewport every UVCamComponent::Update().
		 * A user would not expect this to happen and cannot resolve it either except by restarting the editor.
		 *
		 * Examples how this could happen "legitimately":
		 *  - If you delete the VCam after an auto save, the delete code will check for any referencers.
		 *	It will find the auto save package and load a VCam component.
		 *	This temporary instance should not register any delegates.
		 *	- Load a Level Snapshot containing VCam (without there being a VCam in the level)
		 *	- (User) code calls LoadPackage
		 */
		UWorld* OwnerWorld = Component->GetWorld();
		const bool bIsInValidWorld =
			OwnerWorld // CDO's do not have an owner world
			&& (!GWorld // Can be nullptr during initial load
				|| GWorld == OwnerWorld
				|| GWorld->ContainsLevel(OwnerWorld->PersistentLevel)
				// PIE is always allowed
				|| OwnerWorld->IsGameWorld()
				);
		
		/*
		 * For temporary objects, the InputComponent should not be created and neither should we subscribe to global callbacks
		 *	1. The Blueprint editor has two objects:
		 *		1.1 The "real" one which saves the property data - this one is RF_ArchetypeObject
		 *		1.2 The preview one (which I assume is displayed in the viewport) - this one is RF_Transient.
		 *	2. When you drag-create an actor, level editor creates a RF_Transient template actor. After you release the mouse, a real one is created (not RF_Transient).
		 */
		return !Component->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient)
			&& !GIsCookerLoadingPackage
			&& bIsInValidWorld
			&& !IsRunningCommandlet();
	}

	bool CanInitVCamInstance(UVCamComponent* Component)
	{
		return CanInitVCamObject(Component);
	}

	bool CanInitVCamOutputProvider(UVCamOutputProviderBase* OutputProvider)
	{
		return CanInitVCamObject(OutputProvider);
	}
}
