// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/SimulationModuleBase.h"

namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;
	class FClusterUnionPhysicsProxy;
	class FSuspensionConstraint;

	struct CHAOSVEHICLESCORE_API FSuspensionSimModuleDatas : public FModuleNetData
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		FSuspensionSimModuleDatas(int NodeArrayIndex, const FString& InDebugString) : FModuleNetData(NodeArrayIndex, InDebugString) {}
#else
		FSuspensionSimModuleDatas(int NodeArrayIndex) : FModuleNetData(NodeArrayIndex) {}
#endif

		virtual eSimType GetType() override { return eSimType::Suspension; }

		virtual void FillSimState(ISimulationModuleBase* SimModule) override;

		virtual void FillNetState(const ISimulationModuleBase* SimModule) override;

		virtual void Serialize(FArchive& Ar) override
		{
			Ar << SpringDisplacement;
			Ar << LastDisplacement;
		}

		virtual void Lerp(const float LerpFactor, const FModuleNetData& Min, const FModuleNetData& Max) override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		virtual FString ToString() const override;
#endif

		float SpringDisplacement = 0.0f;
		float LastDisplacement = 0.0f;
	};

	struct CHAOSVEHICLESCORE_API FSuspensionOutputData : public FSimOutputData
	{
		virtual FSimOutputData* MakeNewData() override { return FSuspensionOutputData::MakeNew(); }
		static FSimOutputData* MakeNew() { return new FSuspensionOutputData(); }

		virtual eSimType GetType() override { return eSimType::Suspension; }
		virtual void FillOutputState(const ISimulationModuleBase* SimModule) override;
		virtual void Lerp(const FSimOutputData& InCurrent, const FSimOutputData& InNext, float Alpha) override;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		virtual FString ToString() override;
#endif

		float SpringDisplacement;
		float SpringSpeed;
	};

	struct CHAOSVEHICLESCORE_API FSuspensionSettings
	{
		FSuspensionSettings()
			: SuspensionAxis(FVector(0.f, 0.f, -1.f))
			, RestOffset(FVector::ZeroVector)
			, MaxRaise(5.f)
			, MaxDrop(5.f)
			, MaxLength(0.f)
			, SpringRate(1.f)
			, SpringPreload(0.5f)
			, SpringDamping(0.9f)
			, SuspensionForceEffect(100.0f)
			//	, SwaybarEffect(0.5f)
		{

		}

		FVector SuspensionAxis;		// local axis, direction of suspension force raycast traces
		FVector RestOffset;
		float MaxRaise;				// distance [cm]
		float MaxDrop;				// distance [cm]
		float MaxLength;			// distance [cm]

		float SpringRate;			// spring constant
		float SpringPreload;		// Amount of Spring force (independent spring movement)
		float SpringDamping;		// limit compression/rebound speed

		float SuspensionForceEffect; // force that presses the wheels into the ground - producing grip

		//	float Swaybar;				// Anti-roll bar
	};

	/** Suspension world ray/shape trace start and end positions */
	struct CHAOSVEHICLESCORE_API FSpringTrace
	{
		FVector Start;
		FVector End;

		FVector TraceDir()
		{
			FVector Dir(End - Start);
			return Dir.FVector::GetSafeNormal();
		}

		float Length()
		{
			FVector Dir(End - Start);
			return Dir.Size();
		}
	};

	class CHAOSVEHICLESCORE_API FSuspensionSimModule : public ISimulationModuleBase, public TSimModuleSettings<FSuspensionSettings>
	{
		friend FSuspensionSimModuleDatas;
		friend FSuspensionOutputData;

	public:

		FSuspensionSimModule(const FSuspensionSettings& Settings);

		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType & Raycast); }
		virtual TSharedPtr<FModuleNetData> GenerateNetData(int SimArrayIndex) const
		{
			return MakeShared<FSuspensionSimModuleDatas>(
				SimArrayIndex
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				, GetDebugName()
#endif			
			);
		}

		virtual FSimOutputData* GenerateOutputData() const override
		{
			return FSuspensionOutputData::MakeNew();
		}

		virtual eSimType GetSimType() const { return eSimType::Suspension; }

		virtual const FString GetDebugName() const { return TEXT("Suspension"); }

		float GetSpringLength() const;
		void SetSpringLength(float InLength, float WheelRadius);
		void GetWorldRaycastLocation(const FTransform& BodyTransform, float WheelRadius, FSpringTrace& OutTrace);

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		virtual void Animate(Chaos::FClusterUnionPhysicsProxy* Proxy) override;

		const FVector& GetRestLocation() const { return Setup().RestOffset; }

		void SetWheelSimTreeIndex(int WheelTreeIndexIn) { WheelSimTreeIndex = WheelTreeIndexIn; }
		int GetWheelSimTreeIndex() const { return WheelSimTreeIndex; } 

		void UpdateConstraint();

		void SetSuspensionConstraint(FSuspensionConstraint* InConstraint);
		void SetConstraintIndex(int32 InConstraintIndex) { ConstraintIndex = InConstraintIndex; }
		int32 GetConstraintIndex() const { return ConstraintIndex; }
		void SetTargetPoint(const FVector& InTargetPoint, const FVector& InImpactNormal, bool InWheelInContact)
		{
			TargetPos = InTargetPoint;
			ImpactNormal = InImpactNormal;
			WheelInContact = InWheelInContact;
		}
	private:

		float SpringDisplacement;
		float LastDisplacement;
		float SpringSpeed;
		int WheelSimTreeIndex;

		FSuspensionConstraint* Constraint;
		int32 ConstraintIndex;
		FVector TargetPos;
		FVector ImpactNormal;
		bool WheelInContact;
	};


} // namespace Chaos


