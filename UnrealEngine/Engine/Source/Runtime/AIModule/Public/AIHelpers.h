// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AITypes.h"
#include "Containers/Array.h"
#include "Math/MathFwd.h"
#include "Math/Quat.h"
#include "Math/Rotator.h"
#include "Misc/Optional.h"
#include "Templates/SubclassOf.h"

class AActor;
class UActorComponent;

namespace FAISystem
{
	AIMODULE_API FVector FindClosestLocation(const FVector& Origin, const TArray<FVector>& Locations);

	//----------------------------------------------------------------------//
	// CheckIsTargetInSightCone
	//                     F
	//                   *****  
	//              *             *
	//          *                     *
	//       *                           *
	//     *                               *
	//   *                                   * 
	//    \                                 /
	//     \                               /
	//      \                             /
	//       \             X             /
	//        \                         /
	//         \          ***          /
	//          \     *    N    *     /
	//           \ *               * /
	//            N                 N
	//            
	//           
	//           
	//           
	//
	// 
	//                     B 
	//
	// X = StartLocation
	// B = Backward offset
	// N = Near Clipping Radius (from the StartLocation adjusted by Backward offset)
	// F = Far Clipping Radius (from the StartLocation adjusted by Backward offset)
	//----------------------------------------------------------------------//
	AIMODULE_API bool CheckIsTargetInSightCone(const FVector& StartLocation, const FVector& ConeDirectionNormal, float PeripheralVisionAngleCos,
											   float ConeDirectionBackwardOffset, float NearClippingRadiusSq, float const FarClippingRadiusSq, const FVector& TargetLocation);
}

namespace UE::AI
{
	/**
	 * This method will extract the yaw radian from the specified vector (The vector does not need to be normalized)
	 * if it is not possible to compute yaw, the function will return an invalid value	 *
	 */
	extern AIMODULE_API TOptional<float> GetYawFromVector(const FVector& Vector);

	/**
	 * This method will extract the yaw radian from the specified rotator
	 * if it is not possible to compute yaw, the function will not return an invalid value
	 */
	extern AIMODULE_API TOptional<float> GetYawFromRotator(const FRotator& Rotator);

	/**
	 * This method will extract the yaw radian from the specified quaternion
	 * if it is not possible to compute yaw, the function will not return an invalid value
	 */
	extern AIMODULE_API TOptional<float> GetYawFromQuaternion(const FQuat& Quaternion);

	/**
	 * Fetches all the components of ActorClass's CDO, including the ones added via the BP editor (which AActor.GetComponents fails to do)
	 * @param ActorClass class of AActor for which we will retrieve all components
	 * @param OutComponents this is where the found components will end up. Note that the preexisting contents of OutComponents will get overridden.
	 * @param InComponentClass if supplied will be used to filter the results
	 */
	extern AIMODULE_API void GetActorClassDefaultComponents(const TSubclassOf<AActor>& ActorClass, TArray<UActorComponent*>& OutComponents, const TSubclassOf<UActorComponent>& InComponentClass = TSubclassOf<UActorComponent>());
} // UE::AI
