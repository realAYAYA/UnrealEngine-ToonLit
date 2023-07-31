// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/SimulationModuleBase.h"

namespace Chaos
{
	struct FAllInputs;
	class FSimModuleTree;

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
			, CompressionDamping(0.9f)
			, ReboundDamping(0.9f)
		//	, SwaybarEffect(0.5f)
		//	, DampingRatio(0.3f)
		{

		}

		FVector SuspensionAxis;		// local axis, direction of suspension force raycast traces
		FVector RestOffset;	
		float MaxRaise;				// distance [cm]
		float MaxDrop;				// distance [cm]
		float MaxLength;			// distance [cm]

		float SpringRate;			// spring constant
		float SpringPreload;		// Amount of Spring force (independent spring movement)
		float CompressionDamping;	// limit compression speed
		float ReboundDamping;		// limit rebound speed

	//	float Swaybar;				// Anti-roll bar

	//	float DampingRatio;			// value between (0-no damping) and (1-critical damping)
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
	public:

		FSuspensionSimModule(const FSuspensionSettings& Settings);

		virtual bool IsBehaviourType(eSimModuleTypeFlags InType) const override { return (InType & Raycast); }

		virtual eSimType GetSimType() const { return eSimType::Suspension; }

		virtual const FString GetDebugName() const { return TEXT("Suspension"); }

		float GetSpringLength() const;
		void SetSpringLength(float InLength, float WheelRadius);
		void GetWorldRaycastLocation(const FTransform& BodyTransform, float WheelRadius, FSpringTrace& OutTrace);

		virtual void Simulate(float DeltaTime, const FAllInputs& Inputs, FSimModuleTree& VehicleModuleSystem) override;

		void SetLocation(const FVector& LocationIn) { Location = LocationIn; }
		const FVector GetLocation() const { return /*Setup().RestOffset + */Location; }

		const FVector& GetRestLocation() const { return Setup().RestOffset; }

		void SetWheelSimTreeIndex(int WheelTreeIndexIn) { WheelSimTreeIndex = WheelTreeIndexIn; }
		int GetWheelSimTreeIndex() const { return WheelSimTreeIndex; }

	private:

		float SpringDisplacement;
		float LastDisplacement;
		int WheelSimTreeIndex;

		FVector Location;
	};


} // namespace Chaos

