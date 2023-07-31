// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include <Chaos/Real.h>

// Use to define out code blocks that need to be adapted to use Particle Handles in a searchable way (better than #if 0)
#define CHAOS_PARTICLEHANDLE_TODO 0

namespace Chaos
{
	// TGeometryParticle 

	template <typename T, int d>
	class TGeometryParticle;
	//using TGeometryParticleFloat3 = TGeometryParticle<float, 3>;

	template <typename T, int d, bool bProcessing>
	class TGeometryParticleHandleImp;

	template <typename T, int d>
	using TGeometryParticleHandle = TGeometryParticleHandleImp<T, d, true>;

	template <typename T, int d>
	using TTransientGeometryParticleHandle = TGeometryParticleHandleImp<T, d, false>;

	// TKinematicGeometryParticle

	template <typename T, int d>
	class TKinematicGeometryParticle;

	template <typename T, int d, bool bProcessing>
	class TKinematicGeometryParticleHandleImp;

	template <typename T, int d>
	using TKinematicGeometryParticleHandle = TKinematicGeometryParticleHandleImp<T, d, true>;

	using FKinematicGeometryParticleHandle = TKinematicGeometryParticleHandle<FReal, 3>;

	template <typename T, int d>
	using TTransientKinematicGeometryParticleHandle = TKinematicGeometryParticleHandleImp<T, d, false>;

	using FTransientKinematicGeometryParticleHandle = TTransientKinematicGeometryParticleHandle<FReal, 3>;

	// TPBDRigidParticle

	template <typename T, int d>
	class TPBDRigidParticle;

	template <typename T, int d, bool bProcessing>
	class TPBDRigidParticleHandleImp;

	template <typename T, int d>
	using TPBDRigidParticleHandle = TPBDRigidParticleHandleImp<T, d, true>;
	//using TPBDRigidParticleHandleFloat3 = TPBDRigidParticleHandle<float, 3>;

	using FPBDRigidParticleHandle = TPBDRigidParticleHandle<FReal, 3>;

	template <typename T, int d>
	using TTransientPBDRigidParticleHandle = TPBDRigidParticleHandleImp<T, d, false>;

	using FTransientPBDRigidParticleHandle = TTransientPBDRigidParticleHandle<FReal, 3>;

	// TPBDRigidClusteredParticleHandle

	template <typename T, int d, bool bProcessing>
	class TPBDRigidClusteredParticleHandleImp;

	template <typename T, int d>
	using TPBDRigidClusteredParticleHandle = TPBDRigidClusteredParticleHandleImp<T, d, true>;
	//using TPBDRigidClusteredParticleHandleFloat3 = TPBDRigidClusteredParticleHandle<float, 3>;

	using FPBDRigidClusteredParticleHandle = TPBDRigidClusteredParticleHandle<FReal, 3>;

	template <typename T, int d>
	using TTransientPBDRigidClusteredParticleHandle = TPBDRigidClusteredParticleHandleImp<T, d, false>;

	using FTransientPBDRigidClusteredParticleHandle = TTransientPBDRigidClusteredParticleHandle<FReal, 3>;

	// TPBDGeometryCollectionParticleHandle

	class FPBDGeometryCollectionParticle;

	template <typename T, int d, bool bPersistent>
	class TPBDGeometryCollectionParticleHandleImp;

	template <typename T, int d>
	using TPBDGeometryCollectionParticleHandle = TPBDGeometryCollectionParticleHandleImp<T, d, true>;
	//using TPBDGeometryCollectionParticleHandleFloat3 = TPBDGeometryCollectionParticleHandle<float, 3>;

	using FPBDGeometryCollectionParticleHandle = TPBDGeometryCollectionParticleHandle<FReal, 3>;

	template <typename T, int d>
	using TTransientPBDGeometryCollectionParticleHandle = TPBDGeometryCollectionParticleHandleImp<T, d, false>;
	
	class FGenericParticleHandle;
	class FConstGenericParticleHandle;

	// TParticleIterator

	template <typename TSOA>
	class TParticleIterator;

	template <typename TSOA>
	class TConstParticleIterator;

	using FGeometryParticle = TGeometryParticle<FReal, 3>;
	using FGeometryParticleHandle = TGeometryParticleHandle<FReal, 3>;
	using FKinematicGeometryParticle = TKinematicGeometryParticle<FReal, 3>;
	using FPBDRigidParticle = TPBDRigidParticle<FReal, 3>;
}
