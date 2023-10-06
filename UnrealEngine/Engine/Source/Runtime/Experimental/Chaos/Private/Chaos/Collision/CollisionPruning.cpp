// Copyright Epic Games, Inc. All Rights Reserved.
#include "Chaos/Collision/CollisionPruning.h"

#include "Chaos/Collision/CollisionConstraintAllocator.h"
#include "Chaos/Collision/ParticleCollisions.h"
#include "Chaos/Collision/PBDCollisionConstraint.h"
#include "Chaos/ParticleHandle.h"

namespace Chaos
{
	extern FRealSingle Chaos_Collision_EdgePrunePlaneDistance;


	// Loop over edge collisions, then all plane collisions and remove the edge collision if it is hidden by a plane collision
	// NOTE: We only look at plane collisions where the other shape owns the plane.
	// @todo(chaos): this should probably only disable individual manifold points
	// @todo(chaos): we should probably only reject edges if the plane contact is also close to the edge contact
	// @todo(chaos): we should also try to eliminate face contacts from sub-surface faces
	// @todo(chaos): perf issue: this processes contacts in world space, but we don't calculated that data until Gather. Fix this.
	void FParticleEdgeCollisionPruner::Prune()
	{
		// Chaos_Collision_EdgePrunePlaneDistance is prune distance for unit sized object (100cm) so scale to the size of this object
		const FReal EdgePlaneTolerancePerCm = FReal(Chaos_Collision_EdgePrunePlaneDistance) * FReal(0.01);
		const FReal EdgePlaneToleranceSize = Particle->HasBounds() ? Particle->LocalBounds().Extents().Min() : FReal(1);
		const FReal EdgePlaneTolerance = EdgePlaneToleranceSize * EdgePlaneTolerancePerCm;

		FParticleCollisions& ParticleCollisions = Particle->ParticleCollisions();

		ParticleCollisions.VisitCollisions(
			[this, &ParticleCollisions, EdgePlaneTolerance](FPBDCollisionConstraint& EdgeCollision)
			{
				if (EdgeCollision.IsEnabled())
				{
					const int32 EdgeOtherShapeIndex = (EdgeCollision.GetParticle0() == Particle) ? 1 : 0;
					const EContactPointType VertexContactType = (EdgeOtherShapeIndex == 0) ? EContactPointType::VertexPlane : EContactPointType::PlaneVertex;

					for (const FManifoldPoint& EdgeManifoldPoint : EdgeCollision.GetManifoldPoints())
					{
						const bool bIsEdgeContact = (EdgeManifoldPoint.ContactPoint.ContactType == EContactPointType::EdgeEdge);

						if (bIsEdgeContact)
						{
							const FRigidTransform3& EdgeTransform = (EdgeOtherShapeIndex == 0) ? EdgeCollision.GetShapeWorldTransform0() : EdgeCollision.GetShapeWorldTransform1();
							const FVec3 EdgePos = EdgeTransform.TransformPositionNoScale(FVec3(EdgeManifoldPoint.ContactPoint.ShapeContactPoints[EdgeOtherShapeIndex]));

							// Loop over plane collisions
							ECollisionVisitorResult PlaneResult = ParticleCollisions.VisitConstCollisions(
								[this, &EdgeCollision, &EdgePos, EdgePlaneTolerance](const FPBDCollisionConstraint& PlaneCollision)
								{
									if ((&PlaneCollision != &EdgeCollision) && PlaneCollision.IsEnabled())
									{
										const int32 PlaneOtherShapeIndex = (PlaneCollision.GetParticle0() == Particle) ? 1 : 0;
										const EContactPointType PlaneContactType = (PlaneOtherShapeIndex == 0) ? EContactPointType::PlaneVertex : EContactPointType::VertexPlane;
										const FRigidTransform3& PlaneTransform = (PlaneOtherShapeIndex == 0) ? PlaneCollision.GetShapeWorldTransform0() : PlaneCollision.GetShapeWorldTransform1();

										for (const FManifoldPoint& PlaneManifoldPoint : PlaneCollision.GetManifoldPoints())
										{
											if (PlaneManifoldPoint.ContactPoint.ContactType == PlaneContactType)
											{
												// If the edge position is in the plane, disable it
												const FVec3 PlanePos = PlaneTransform.TransformPositionNoScale(FVec3(PlaneManifoldPoint.ContactPoint.ShapeContactPoints[PlaneOtherShapeIndex]));
												const FVec3 PlaneNormal = PlaneCollision.GetShapeWorldTransform1().TransformVectorNoScale(FVec3(PlaneManifoldPoint.ContactPoint.ShapeContactNormal));

												const FVec3 EdgePlaneDelta = EdgePos - PlanePos;
												const FReal EdgePlaneDistance = FVec3::DotProduct(EdgePlaneDelta, PlaneNormal);
												if (FMath::Abs(EdgePlaneDistance) < EdgePlaneTolerance)
												{
													// The edge contact is hidden by a plane contact so disable it and stop the inner loop
													EdgeCollision.SetDisabled(true);
													return ECollisionVisitorResult::Stop;
												}
											}
										}
									}
									return ECollisionVisitorResult::Continue;
								});

							// If we disabled this constraint, move to the next one and ignore remaining manifold points
							if (PlaneResult == ECollisionVisitorResult::Stop)
							{
								break;
							}
						}
					}
				}
				return ECollisionVisitorResult::Continue;
			});
	}

	/**
	 * @brief Information about a contact point held in a ContactGroup (points in the same plane)
	*/
	class FContactGroupManifoldPoint
	{
	public:
		FPBDCollisionConstraint* Collision;
		int32 ManifoldPointIndex;
		FVec3 ContactNormal;
		FVec3 ContactPosition;
	};

	/**
	 * @brief A set of contacts in the same plane
	*/
	class FContactGroup
	{
	public:
		FContactGroup()
			: PlanePosition()
			, PlaneNormal()
			, UpDotNormal(0)
			, bActive(false)
		{
		}

		void Add(FPBDCollisionConstraint& Collision, const int32 ManifoldPointIndex, const FVec3& InContactPosition, const FVec3& InContactNormal, const FReal InUpDotNormal)
		{
			if (ManifoldPoints.Num() == 0)
			{
				PlanePosition = InContactPosition;
				PlaneNormal = InContactNormal;
				UpDotNormal = InUpDotNormal;
				bActive = true;
			}

			FContactGroupManifoldPoint& Point = ManifoldPoints[ManifoldPoints.AddDefaulted()];
			Point.Collision = &Collision;
			Point.ManifoldPointIndex = ManifoldPointIndex;
			Point.ContactPosition = InContactPosition;
			Point.ContactNormal = InContactNormal;
		}

		TArray<FContactGroupManifoldPoint> ManifoldPoints;
		FVec3 PlanePosition;
		FVec3 PlaneNormal;
		FReal UpDotNormal;
		bool bActive;
	};

	/**
	 * @brief All the contacts on a body, held in sets where all contacts in each set are in the same plane
	*/
	class FContactGroupContainer
	{
	public:
		FContactGroupContainer(const FVec3& InUpVector)
			: UpVector(InUpVector)
		{
		}
		
		void Add(FPBDCollisionConstraint& Collision, const int32 ManifoldPointIndex, const FVec3& ContactPos, const FVec3& ContactNormal)
		{
			FContactGroup* ContactGroup = FindContactGroup(ContactPos, ContactNormal);
			if (ContactGroup == nullptr)
			{
				ContactGroup = &ContactGroups[ContactGroups.AddDefaulted()];
			}

			const FReal UpDotNormal = FVec3::DotProduct(UpVector, ContactNormal);
			ContactGroup->Add(Collision, ManifoldPointIndex, ContactPos, ContactNormal, UpDotNormal);
		}

		FContactGroup* FindContactGroup(const FVec3& ContactPos, const FVec3& ContactNormal)
		{
			const FReal NormalProjectionThreshold = FReal(0.99);	// 8deg
			const FReal PlaneDistanceThreshold = FReal(5);

			for (FContactGroup& ContactGroup : ContactGroups)
			{
				const FReal NormalProjection = FVec3::DotProduct(ContactNormal, ContactGroup.PlaneNormal);
				if (NormalProjection > NormalProjectionThreshold)
				{
					const FReal PlaneDistance = FVec3::DotProduct(ContactPos - ContactGroup.PlanePosition, ContactGroup.PlaneNormal);
					if (FMath::Abs(PlaneDistance) < PlaneDistanceThreshold)
					{
						return &ContactGroup;
					}
				}
			}
			return nullptr;
		}

		void SortContactGroups()
		{
			ContactGroups.Sort([](const FContactGroup& L, const FContactGroup& R)
			{
				return L.UpDotNormal > R.UpDotNormal;
			});
		}

		TArray<FContactGroup, TInlineAllocator<4>> ContactGroups;
		FVec3 UpVector;
	};


	void FParticleSubSurfaceCollisionPruner::Prune(const FVec3& UpVector)
	{
		// Chaos_Collision_EdgePrunePlaneDistance is prune distance for unit sized object (100cm) so scale by size of this object
		const FReal PlaneDistanceTolerancePerCm = FReal(Chaos_Collision_EdgePrunePlaneDistance) * FReal(0.01);
		const FReal PlaneDistanceSize = Particle->HasBounds() ? Particle->LocalBounds().Extents().Min() : FReal(1);
		const FReal PlaneDistanceTolerance = PlaneDistanceSize * PlaneDistanceTolerancePerCm;
		const FReal PlaneNormalTolerance = FReal(0.95);

		FContactGroupContainer ContactGroupContainer(UpVector);

		FParticleCollisions& ParticleCollisions = Particle->ParticleCollisions();

		ParticleCollisions.VisitCollisions(
			[this, &ContactGroupContainer](FPBDCollisionConstraint& Collision)
			{
				if (Collision.IsEnabled())
				{
					const int32 OtherShapeIndex = (Collision.GetParticle0() == Particle) ? 1 : 0;
					const FRigidTransform3& ContactTransform = (OtherShapeIndex == 0) ? Collision.GetShapeWorldTransform0() : Collision.GetShapeWorldTransform1();
					FConstGenericParticleHandle P0 = Collision.GetParticle0();
					FConstGenericParticleHandle P1 = Collision.GetParticle1();
					FConstGenericParticleHandle OtherParticle = (OtherShapeIndex == 0) ? Collision.GetParticle0() : Collision.GetParticle1();

					// Only reject sub-surface contacts against static or kinematic objects
					if (!OtherParticle->IsDynamic())
					{
						for (int32 ManifoldPointIndex = 0; ManifoldPointIndex < Collision.NumManifoldPoints(); ++ManifoldPointIndex)
						{
							const FManifoldPoint& ManifoldPoint = Collision.GetManifoldPoint(ManifoldPointIndex);

							const FVec3 ContactPos = ContactTransform.TransformPositionNoScale(FVec3(ManifoldPoint.ContactPoint.ShapeContactPoints[OtherShapeIndex]));
							const FVec3 ContactNormal = Collision.GetShapeWorldTransform1().TransformVectorNoScale(FVec3(ManifoldPoint.ContactPoint.ShapeContactNormal));

							ContactGroupContainer.Add(Collision, ManifoldPointIndex, ContactPos, ContactNormal);
						}
					}
				}
				return ECollisionVisitorResult::Continue;
			});

		// Sort the groups by the most vertical normal so that we preferentially keep horizontal planes (wrt the UpVector)
		ContactGroupContainer.SortContactGroups();

		// If all contacts in one group lie beneath the plane of another group, reject it
		// Loop over all groups (i.e., contact planes)
		for (int32 ContactGroupIndex = 0; ContactGroupIndex < ContactGroupContainer.ContactGroups.Num(); ++ContactGroupIndex)
		{
			const FContactGroup& ContactGroup = ContactGroupContainer.ContactGroups[ContactGroupIndex];

			// @todo(chaos): assuming here that the contacts are not all in a line...do we need to check for that?
			if (!ContactGroup.bActive)
			{
				continue;
			}
			
			// Loop over all other contact groups
			for (int32 RejectCandidateContactGroupIndex = ContactGroupIndex + 1; RejectCandidateContactGroupIndex < ContactGroupContainer.ContactGroups.Num(); ++RejectCandidateContactGroupIndex)
			{
				FContactGroup& RejectCandidateContactGroup = ContactGroupContainer.ContactGroups[RejectCandidateContactGroupIndex];
				if (!RejectCandidateContactGroup.bActive)
				{
					continue;
				}
				
				// Are any point in the candidate group above the plane?
				bool bPointsAbovePlane = false;
				for (FContactGroupManifoldPoint& RejectCandidateManifoldPoint : RejectCandidateContactGroup.ManifoldPoints)
				{
					const FReal PlaneDistance = FVec3::DotProduct(RejectCandidateManifoldPoint.ContactPosition - ContactGroup.PlanePosition, ContactGroup.PlaneNormal);
					if (PlaneDistance > PlaneDistanceTolerance)
					{
						bPointsAbovePlane = true;
						break;
					}
				}

				// If all point in the candidate group are below the plane, reject them
				if (!bPointsAbovePlane)
				{
					for (FContactGroupManifoldPoint& RejectCandidateManifoldPoint : RejectCandidateContactGroup.ManifoldPoints)
					{
						RejectCandidateManifoldPoint.Collision->DisableManifoldPoint(RejectCandidateManifoldPoint.ManifoldPointIndex);
					}
					RejectCandidateContactGroup.bActive = false;
				}
			}
		}
	}
}