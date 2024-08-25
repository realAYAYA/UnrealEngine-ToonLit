// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeadlessChaos.h"
#include "HeadlessChaosTestUtility.h"

#include "Chaos/PBDRigidsEvolutionGBF.h"
#include "Chaos/Box.h"
#include "Chaos/Sphere.h"
#include "Chaos/Utilities.h"

#include "Modules/ModuleManager.h"

namespace ChaosTest
{

	using namespace Chaos;

	GTEST_TEST(DirtyParticleTests,Basic)
	{
#if 0
		auto Particle = FGeometryParticle::CreateParticle();
		Particle->SetX(FVec3(1,1,1));

		Chaos::FImplicitObjectPtr Ptr(new TSphere<FReal,3>(FVec3(0),0));
		const auto RawPtr = Ptr.Get();
		TWeakPtr<FImplicitObject,ESPMode::ThreadSafe> WeakPtr(Ptr);
		FDirtyPropertiesManager Manager;

		{
			FParticlePropertiesData RemoteData(&Manager);
			FShapeRemoteDataContainer ContainerData(&Manager);

			Particle->SetRemoteData(RemoteData, ContainerData);

			//then on PushToPhysics we'd do a pointer swap and use RemoteData internally

			EXPECT_TRUE(RemoteData.HasX());
			EXPECT_EQ(RemoteData.GetX(),FVec3(1,1,1));
			EXPECT_FALSE(RemoteData.HasInvM());	//was never set so it's false

			Particle->SetX(FVec3(2,1,1));
			EXPECT_EQ(RemoteData.GetX(),FVec3(2,1,1));	//remote is set so immediate change

			//make sure we are not leaking shared ptrs
			Particle->SetGeometry(Ptr);
			Ptr = nullptr;

			EXPECT_TRUE(WeakPtr.IsValid());	//still around because particle is holding on to it
			EXPECT_TRUE(RemoteData.HasGeometry());
			EXPECT_EQ(RemoteData.GetGeometry().Get(),RawPtr);

			Particle->DetachRemoteData();	//disconnect remote data so we can pretend we are freeing things on GT without affecting PT

			Particle->SetGeometry(Ptr);	//free geometry on GT side

			//geometry still on PT side
			EXPECT_TRUE(RemoteData.HasGeometry());
			EXPECT_EQ(RemoteData.GetGeometry().Get(),RawPtr);

			EXPECT_TRUE(WeakPtr.IsValid());	//still around because particle is holding on to it
		}
		
		//remote data is gone so geometry shared ptr is freed, even though pool is still around (i.e. it was removed from pool)
		EXPECT_FALSE(WeakPtr.IsValid());



#endif
	}
}

