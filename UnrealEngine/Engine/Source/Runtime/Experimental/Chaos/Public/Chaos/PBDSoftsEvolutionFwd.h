// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/GeometryParticlesfwd.h"  // for EGeometryParticlesSimType

namespace UE::Math
{
	template<typename T> struct TTransform;
}

namespace Chaos
{
	// Chaos types
	template <typename T, int d> class TVector;
	template<class T, int m, int n> class PMatrix;
	template <class T, int d> class TPBDParticles;
	template <class T, int d, EGeometryParticlesSimType SimType> class TKinematicGeometryParticlesImp;
	template <class T, int d> class TRotation;
	template <class T, int d> class TRigidTransform;
	template <class T, int d> class TAABB;
	template <class T> struct TVector3AndScalar;

	class FPerParticleGravity;

	// Softs types
	namespace Softs
	{
		// Softs solver types
		//using FSolverReal = FReal;
		using FSolverReal = FRealSingle;
		using FSolverVec2 = TVector<FSolverReal, 2>;
		using FSolverVec3 = TVector<FSolverReal, 3>;
		using FSolverMatrix33 = PMatrix<FSolverReal, 3, 3>;
		using FSolverMatrix44 = PMatrix<FSolverReal, 4, 4>;
		using FSolverRigidParticles = TKinematicGeometryParticlesImp<FSolverReal, 3, EGeometryParticlesSimType::Other>;
		using FSolverRotation3 = TRotation<FSolverReal, 3>;
		using FSolverRigidTransform3 = TRigidTransform<FSolverReal, 3>;
		using FSolverTransform3 = UE::Math::TTransform<FSolverReal>;
		using FSolverAABB3 = TAABB<FSolverReal, 3>;
		class FSolverParticles;
		struct FPAndInvM;

		// Softs solver class
		class FPBDEvolution;

		// Softs solver constraint classes
		class FPBDSpringConstraints;
		class FXPBDSpringConstraints;
		class FPBDBendingConstraints;
		class FXPBDBendingConstraints;
		class FPBDAxialSpringConstraints;
		class FXPBDAxialSpringConstraints;
		class FPBDVolumeConstraint;
		class FPBDLongRangeConstraints;
		class FXPBDLongRangeConstraints;
		class FPBDSphericalConstraint;
		class FPBDSphericalBackstopConstraint;
		class FPBDAnimDriveConstraint;
		class FPBDShapeConstraints;
		class FPBDCollisionSpringConstraints;
		class FPBDTriangleMeshIntersections;
		class FPBDTriangleMeshCollisions;

		// Softs solver forces
		class FVelocityAndPressureField;
		using FVelocityField UE_DEPRECATED(5.1, "Chaos::Softs::FVelocityField has been renamed FVelocityAndPressureField to match its new behavior.") = FVelocityAndPressureField;
	}  // End namespace Softs
}  // End namespace Chaos
