// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/ObjectMacros.h"
#include "HAL/Platform.h"
#include "DataWrappers/ChaosVDCollisionDataWrappers.h"

#include "ChaosVDParticleDataWrapper.generated.h"

UENUM()
enum class EChaosVDParticleType : uint8
{
	Static,
	Kinematic,
	Rigid,
	Clustered,
	StaticMesh,
	SkeletalMesh,
	GeometryCollection,
	Unknown
};

UENUM()
enum class EChaosVDSleepType : uint8
{
	MaterialSleep,
	NeverSleep
};

UENUM()
enum class EChaosVDObjectStateType: int8
{
	Uninitialized = 0,
	Sleeping = 1,
	Kinematic = 2,
	Static = 3,
	Dynamic = 4,

	Count
};

/** Base struct that declares the interface to be used for any ParticleData Viewer */
USTRUCT()
struct FChaosVDParticleDataBase
{
	GENERATED_BODY()
	virtual ~FChaosVDParticleDataBase() = default;

	virtual bool HasValidData() const { return bHasValidData; }

protected:
	UPROPERTY()
	bool bHasValidData = false;
};

USTRUCT()
struct FChaosVDFRigidParticleControlFlags : public FChaosVDParticleDataBase 
{
	GENERATED_BODY()

	FChaosVDFRigidParticleControlFlags()
		: bGravityEnabled(false),
		  bCCDEnabled(false),
		  bOneWayInteractionEnabled(false),
		  bMaxDepenetrationVelocityOverrideEnabled(false),
		  bInertiaConditioningEnabled(false), GravityGroupIndex(0)
	{
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		bGravityEnabled = Other.GetGravityEnabled();
		bCCDEnabled = Other.GetCCDEnabled();
		bOneWayInteractionEnabled = Other.GetOneWayInteractionEnabled();
		bMaxDepenetrationVelocityOverrideEnabled = Other.GetMaxDepenetrationVelocityOverrideEnabled();
		bInertiaConditioningEnabled = Other.GetInertiaConditioningEnabled();
		GravityGroupIndex = Other.GetGravityGroupIndex();

		bHasValidData = true;
	}

	UPROPERTY(EditAnywhere, Category= "Particle Control Flags")
	bool bGravityEnabled;
	UPROPERTY(EditAnywhere, Category= "Particle Control Flags")
	bool bCCDEnabled;
	UPROPERTY(EditAnywhere, Category= "Particle Control Flags")
	bool bOneWayInteractionEnabled;
	UPROPERTY(EditAnywhere, Category= "Particle Control Flags")
	bool bMaxDepenetrationVelocityOverrideEnabled;
	UPROPERTY(EditAnywhere, Category= "Particle Control Flags")
	bool bInertiaConditioningEnabled;
	UPROPERTY(EditAnywhere, Category= "Particle Control Flags")
	int32 GravityGroupIndex;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDFRigidParticleControlFlags& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDFRigidParticleControlFlags> : public TStructOpsTypeTraitsBase2<FChaosVDFRigidParticleControlFlags>
{
	enum
	{
		WithSerializer = true,
	};
};

/** Simplified UStruct version of FParticlePositionRotation.
 * Used to be able to show the values in the editor and allow changes via the Property Editor.
 */
USTRUCT()
struct FChaosVDParticlePositionRotation : public FChaosVDParticleDataBase
{
	GENERATED_BODY()

	FChaosVDParticlePositionRotation()
	{
	}

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		MX = Other.X();
		MR = Other.R();
		
		bHasValidData = true;
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	UPROPERTY(EditAnywhere, Category= "Particle Velocities")
	FVector MX;

	UPROPERTY(EditAnywhere, Category= "Particle Velocities")
	FQuat MR;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDParticlePositionRotation& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDParticlePositionRotation> : public TStructOpsTypeTraitsBase2<FChaosVDParticlePositionRotation>
{
	enum
	{
		WithSerializer = true,
	};
};

/** Simplified UStruct version of FParticleVelocities.
 * Used to be able to show the values in the editor and allow changes via the Property Editor.
 */
USTRUCT()
struct FChaosVDParticleVelocities : public FChaosVDParticleDataBase
{
	GENERATED_BODY()

	FChaosVDParticleVelocities()
	{
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		MV = Other.V();
		MW = Other.W();
		bHasValidData = true;
	}
	
	UPROPERTY(EditAnywhere, Category= "Particle Velocities")
	FVector MV;

	UPROPERTY(EditAnywhere, Category= "Particle Velocities")
	FVector MW;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDParticleVelocities& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDParticleVelocities> : public TStructOpsTypeTraitsBase2<FChaosVDParticleVelocities>
{
	enum
	{
		WithSerializer = true,
	};
};

/** Simplified UStruct version of FParticleDynamics.
 * Used to be able to show the values in the editor and allow changes via the Property Editor.
 */
USTRUCT()
struct FChaosVDParticleDynamics : public FChaosVDParticleDataBase
{
	GENERATED_BODY()
	
	FChaosVDParticleDynamics()
	{
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		MAcceleration = Other.Acceleration();
		MAngularAcceleration = Other.AngularAcceleration();
		MLinearImpulseVelocity = Other.LinearImpulseVelocity();
		MAngularImpulseVelocity = Other.AngularImpulseVelocity();

		bHasValidData = true;
	}

	UPROPERTY(EditAnywhere, Category= "Particle Dynamics")
	FVector MAcceleration;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamics")
	FVector MAngularAcceleration;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamics")
	FVector MLinearImpulseVelocity;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamics")
	FVector MAngularImpulseVelocity;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDParticleDynamics& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDParticleDynamics> : public TStructOpsTypeTraitsBase2<FChaosVDParticleDynamics>
{
	enum
	{
		WithSerializer = true,
	};
};


/** Simplified UStruct version of FParticleMassProps.
 * Used to be able to show the values in the editor and allow changes via the Property Editor.
 */
USTRUCT()
struct FChaosVDParticleMassProps : public FChaosVDParticleDataBase
{
	GENERATED_BODY()

	FChaosVDParticleMassProps(): MM(0), MInvM(0)
	{
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);
	
	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		MCenterOfMass = Other.CenterOfMass();
		MRotationOfMass = Other.RotationOfMass();
		MI = FVector(Other.I());
		MInvI = FVector(Other.InvI());
		MM = Other.M();
		MInvM = Other.InvM();

		bHasValidData = true;
	}
	
	UPROPERTY(EditAnywhere, Category= "Particle Mass Props")
	FVector MCenterOfMass;

	UPROPERTY(EditAnywhere, Category= "Particle Mass Props")
	FQuat MRotationOfMass;

	UPROPERTY(EditAnywhere, Category= "Particle Mass Props")
	FVector MI;

	UPROPERTY(EditAnywhere, Category= "Particle Mass Props")
	FVector MInvI;

	UPROPERTY(EditAnywhere, Category= "Particle Mass Props")
	double MM;

	UPROPERTY(EditAnywhere, Category= "Particle Mass Props")
	double MInvM;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDParticleMassProps& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDParticleMassProps> : public TStructOpsTypeTraitsBase2<FChaosVDParticleMassProps>
{
	enum
	{
		WithSerializer = true,
	};
};

/** Simplified UStruct version of FParticleDynamicMisc.
 * Used to be able to show the values in the editor and allow changes via the Property Editor.
 */
USTRUCT()
struct FChaosVDParticleDynamicMisc : public FChaosVDParticleDataBase
{
	GENERATED_BODY()

	FChaosVDParticleDynamicMisc(): MAngularEtherDrag(0), MMaxLinearSpeedSq(0), MMaxAngularSpeedSq(0),
	                               MCollisionGroup(0), MObjectState(), MSleepType(), bDisabled(false)
	{
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		MAngularEtherDrag = Other.LinearEtherDrag();
		MAngularEtherDrag = Other.AngularEtherDrag();
		MMaxLinearSpeedSq = Other.MaxLinearSpeedSq();
		MMaxAngularSpeedSq = Other.MaxAngularSpeedSq();
		MObjectState = static_cast<EChaosVDObjectStateType>(Other.ObjectState());
		MCollisionGroup = Other.CollisionGroup();
		MSleepType =  static_cast<EChaosVDSleepType>(Other.SleepType());
		MCollisionConstraintFlag = Other.CollisionConstraintFlags();
	
		MControlFlags.CopyFrom(Other.ControlFlags());
		
		bDisabled = Other.Disabled();

		bHasValidData = true;
	}

	UPROPERTY(EditAnywhere, Category= "Particle Dynamic Misc")
	double MAngularEtherDrag;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamic Misc")
	double MMaxLinearSpeedSq;
	
	UPROPERTY(EditAnywhere, Category= "Particle Dynamic Misc")
	double MMaxAngularSpeedSq;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamic Misc")
	int32 MCollisionGroup;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamic Misc")
	EChaosVDObjectStateType MObjectState;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamic Misc")
	EChaosVDSleepType MSleepType;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamic Misc")
	uint32 MCollisionConstraintFlag = 0;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamic Misc")
	FChaosVDFRigidParticleControlFlags MControlFlags;

	UPROPERTY(EditAnywhere, Category= "Particle Dynamic Misc")
	bool bDisabled;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDParticleDynamicMisc& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDParticleDynamicMisc> : public TStructOpsTypeTraitsBase2<FChaosVDParticleDynamicMisc>
{
	enum
	{
		WithSerializer = true,
	};
};

/** Simplified UStruct version of FChaosVDParticleDataWrapper.
 * Used to be able to show the values in the editor and allow changes via the Property Editor.
 */
USTRUCT()
struct FChaosVDParticleDataWrapper : public FChaosVDParticleDataBase
{
	virtual ~FChaosVDParticleDataWrapper() override = default;

	inline static FStringView WrapperTypeName = TEXT("FChaosVDParticleDataWrapper");

	GENERATED_BODY()

	FChaosVDParticleDataWrapper()
	{
	}

	UPROPERTY(EditAnywhere, Category= "Particle Non Frequent Data")
	uint32 GeometryHash = 0;

	UPROPERTY(VisibleAnywhere, Category= "Particle Non Frequent Data")
	FString DebugName;

	UPROPERTY(VisibleAnywhere, Category= "Particle Non Frequent Data")
	int32 ParticleIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category= "Particle Non Frequent Data")
	int32 SolverID = INDEX_NONE;

	UPROPERTY(EditAnywhere, Category= "Particle Non Frequent Data")
	EChaosVDParticleType Type = EChaosVDParticleType::Unknown;

	UPROPERTY(EditAnywhere, Category= "Particle Position Rotation")
	FChaosVDParticlePositionRotation ParticlePositionRotation;

	UPROPERTY(EditAnywhere, Category= "Particle Particle Velocities")
	FChaosVDParticleVelocities ParticleVelocities;

	UPROPERTY(EditAnywhere, Category= "Particle Particle Dynamics")
	FChaosVDParticleDynamics ParticleDynamics;

	UPROPERTY(EditAnywhere, Category= "Particle Particle Dynamics Misc")
	FChaosVDParticleDynamicMisc ParticleDynamicsMisc;

	UPROPERTY(EditAnywhere, Category= "Particle Particle Mass Props")
	FChaosVDParticleMassProps ParticleMassProps;

	UPROPERTY(EditAnywhere, Category= "Particle Collision")
	TArray<FChaosVDParticlePairMidPhase> ParticleMidPhases;

	UPROPERTY(EditAnywhere, Category= "Particle Collision")
	TArray<FChaosVDConstraint> ParticleConstraints;
	
	/** Only used during recording */
	TSharedPtr<FString> DebugNamePtr;
	bool bHasDebugName = false;

	virtual bool HasValidData() const override { return bHasValidData; }

	void SetHasValidData(bool bNewIsValid){ bHasValidData = bNewIsValid; }

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);
};

inline FArchive& operator<<(FArchive& Ar,FChaosVDParticleDataWrapper& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDParticleDataWrapper> : public TStructOpsTypeTraitsBase2<FChaosVDParticleDataWrapper>
{
	enum
	{
		WithSerializer = true,
	};
};
