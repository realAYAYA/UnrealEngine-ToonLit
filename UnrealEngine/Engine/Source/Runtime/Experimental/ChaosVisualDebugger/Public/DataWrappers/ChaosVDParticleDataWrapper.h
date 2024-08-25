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
struct FChaosVDWrapperDataBase
{
	GENERATED_BODY()
	virtual ~FChaosVDWrapperDataBase() = default;

	virtual bool HasValidData() const { return bHasValidData; }

	void MarkAsValid() { bHasValidData = true; }

protected:
	UPROPERTY()
	bool bHasValidData = false;
};

USTRUCT()
struct FChaosVDFRigidParticleControlFlags : public FChaosVDWrapperDataBase 
{
	GENERATED_BODY()

	FChaosVDFRigidParticleControlFlags()
		: bGravityEnabled(false),
		  bCCDEnabled(false),
		  bOneWayInteractionEnabled(false),
		  bInertiaConditioningEnabled(false), 
		  GravityGroupIndex(0),
		  bMACDEnabled(false)
	{
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		bGravityEnabled = Other.GetGravityEnabled();
		bCCDEnabled = Other.GetCCDEnabled();
		bOneWayInteractionEnabled = Other.GetOneWayInteractionEnabled();
		bInertiaConditioningEnabled = Other.GetInertiaConditioningEnabled();
		GravityGroupIndex = Other.GetGravityGroupIndex();
		bMACDEnabled = Other.GetMACDEnabled();

		bHasValidData = true;
	}

	UPROPERTY(VisibleAnywhere, Category= "Particle Control Flags")
	bool bGravityEnabled;
	UPROPERTY(VisibleAnywhere, Category= "Particle Control Flags")
	bool bCCDEnabled;
	UPROPERTY(VisibleAnywhere, Category= "Particle Control Flags")
	bool bOneWayInteractionEnabled;
	UPROPERTY(VisibleAnywhere, Category = "Particle Control Flags")
	bool bInertiaConditioningEnabled;
	UPROPERTY(VisibleAnywhere, Category= "Particle Control Flags")
	int32 GravityGroupIndex;
	UPROPERTY(VisibleAnywhere, Category = "Particle Control Flags")
	bool bMACDEnabled;
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
struct FChaosVDParticlePositionRotation : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	FChaosVDParticlePositionRotation()
	{
	}

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		MX = Other->GetP();
		MR = Other->GetQ();
		
		bHasValidData = true;
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	UPROPERTY(VisibleAnywhere, Category= "Particle Velocities")
	FVector MX = FVector(ForceInit);

	UPROPERTY(VisibleAnywhere, Category= "Particle Velocities")
	FQuat MR = FQuat(ForceInit);
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
struct FChaosVDParticleVelocities : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	FChaosVDParticleVelocities()
	{
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		MV = Other.GetV();
		MW = Other.GetW();
		bHasValidData = true;
	}
	
	UPROPERTY(VisibleAnywhere, Category= "Particle Velocities")
	FVector MV = FVector(ForceInit);

	UPROPERTY(VisibleAnywhere, Category= "Particle Velocities")
	FVector MW = FVector(ForceInit);
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
struct FChaosVDParticleDynamics : public FChaosVDWrapperDataBase
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

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamics")
	FVector MAcceleration = FVector(ForceInit);

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamics")
	FVector MAngularAcceleration = FVector(ForceInit);

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamics")
	FVector MLinearImpulseVelocity = FVector(ForceInit);

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamics")
	FVector MAngularImpulseVelocity = FVector(ForceInit);
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
struct FChaosVDParticleMassProps : public FChaosVDWrapperDataBase
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
	
	UPROPERTY(VisibleAnywhere, Category= "Particle Mass Props")
	FVector MCenterOfMass = FVector(ForceInit);

	UPROPERTY(VisibleAnywhere, Category= "Particle Mass Props")
	FQuat MRotationOfMass = FQuat(ForceInit);

	UPROPERTY(VisibleAnywhere, Category= "Particle Mass Props")
	FVector MI = FVector(ForceInit);

	UPROPERTY(VisibleAnywhere, Category= "Particle Mass Props")
	FVector MInvI = FVector(ForceInit);

	UPROPERTY(VisibleAnywhere, Category= "Particle Mass Props")
	double MM = 0.0;

	UPROPERTY(VisibleAnywhere, Category= "Particle Mass Props")
	double MInvM = 0.0;
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
struct FChaosVDParticleDynamicMisc : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	FChaosVDParticleDynamicMisc(): MAngularEtherDrag(0), MMaxLinearSpeedSq(0), MMaxAngularSpeedSq(0), MInitialOverlapDepenetrationVelocity(0), MSleepThresholdMultiplier(1),
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
		MInitialOverlapDepenetrationVelocity = Other.InitialOverlapDepenetrationVelocity();
		MSleepThresholdMultiplier = Other.SleepThresholdMultiplier();
		MObjectState = static_cast<EChaosVDObjectStateType>(Other.ObjectState());
		MCollisionGroup = Other.CollisionGroup();
		MSleepType =  static_cast<EChaosVDSleepType>(Other.SleepType());
		MCollisionConstraintFlag = Other.CollisionConstraintFlags();
	
		MControlFlags.CopyFrom(Other.ControlFlags());
		
		bDisabled = Other.Disabled();

		bHasValidData = true;
	}

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamic Misc")
	double MAngularEtherDrag;

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamic Misc")
	double MMaxLinearSpeedSq;
	
	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamic Misc")
	double MMaxAngularSpeedSq;

	UPROPERTY(VisibleAnywhere, Category = "Particle Dynamic Misc")
	float MInitialOverlapDepenetrationVelocity;

	UPROPERTY(VisibleAnywhere, Category = "Particle Dynamic Misc")
	float MSleepThresholdMultiplier;

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamic Misc")
	int32 MCollisionGroup;

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamic Misc")
	EChaosVDObjectStateType MObjectState;

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamic Misc")
	EChaosVDSleepType MSleepType;

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamic Misc")
	uint32 MCollisionConstraintFlag = 0;

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamic Misc")
	FChaosVDFRigidParticleControlFlags MControlFlags;

	UPROPERTY(VisibleAnywhere, Category= "Particle Dynamic Misc")
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

/** Represents the data of a connectivity Edge that CVD can use to reconstruct it during playback */
USTRUCT()
struct FChaosVDConnectivityEdge
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category=ConnectivityEdge)
	int32 SiblingParticleID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category=ConnectivityEdge)
	float Strain = 0.0f;

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar)
	{
		Ar << SiblingParticleID;
		Ar << Strain;

		return true;
	}
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDConnectivityEdge& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDConnectivityEdge> : public TStructOpsTypeTraitsBase2<FChaosVDConnectivityEdge>
{
	enum
	{
		WithSerializer = true,
	};
};

/** Structure contained data from a Clustered Particle.
 * Used to be able to show the values in the editor and allow changes via the Property Editor.
 */
USTRUCT()
struct FChaosVDParticleCluster : public FChaosVDWrapperDataBase
{
	GENERATED_BODY()

	FChaosVDParticleCluster()
	{
	}

	CHAOSVDRUNTIME_API bool Serialize(FArchive& Ar);

	template <typename TOther>
	void CopyFrom(const TOther& Other)
	{
		ParentParticleID = Other.ClusterIds().Id ? Other.ClusterIds().Id->UniqueIdx().Idx : INDEX_NONE;

		NumChildren = Other.ClusterIds().NumChildren;

		ChildToParent = Other.ChildToParent();
		ClusterGroupIndex = Other.ClusterGroupIndex();
		bInternalCluster = Other.InternalCluster();
		CollisionImpulse = Other.CollisionImpulses();
		ExternalStrains = Other.GetExternalStrain();
		InternalStrains = Other.GetInternalStrains();
		Strain = Other.Strain();


		ConnectivityEdges.Reserve(Other.ConnectivityEdges().Num());
		for (auto& Edge : Other.ConnectivityEdges())
		{
			int32 SiblingId = Edge.Sibling ? Edge.Sibling->UniqueIdx().Idx : INDEX_NONE;
			ConnectivityEdges.Add( { SiblingId,  Edge.Strain });
		}

		bIsAnchored = Other.IsAnchored();
		bUnbreakable = Other.Unbreakable();
		bIsChildToParentLocked = Other.IsChildToParentLocked();
		
		bHasValidData = true;
	}

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	int32 ParentParticleID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster | Cluster Id")
	int32 NumChildren = INDEX_NONE;
	
	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	FTransform ChildToParent;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	int32 ClusterGroupIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	bool bInternalCluster = false;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	float CollisionImpulse = 0.0f;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	float ExternalStrains = 0.0f;
	
	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	float InternalStrains = 0.0f;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	float Strain = 0.0f;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	TArray<FChaosVDConnectivityEdge> ConnectivityEdges;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	bool bIsAnchored = false;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	bool bUnbreakable = false;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster")
	bool bIsChildToParentLocked = false;
};

inline FArchive& operator<<(FArchive& Ar, FChaosVDParticleCluster& Data)
{
	Data.Serialize(Ar);
	return Ar;
}

template<>
struct TStructOpsTypeTraits<FChaosVDParticleCluster> : public TStructOpsTypeTraitsBase2<FChaosVDParticleCluster>
{
	enum
	{
		WithSerializer = true,
	};
};


/** Simplified UStruct version of FChaosVDParticleDataWrapper.
 * Used to be able to show the values in the editor and allow changes via the Property Editor.
 */
USTRUCT(DisplayName="Particle Data")
struct FChaosVDParticleDataWrapper : public FChaosVDWrapperDataBase
{
	virtual ~FChaosVDParticleDataWrapper() override = default;

	inline static FStringView WrapperTypeName = TEXT("FChaosVDParticleDataWrapper");

	GENERATED_BODY()

	FChaosVDParticleDataWrapper()
	{
	}

	UPROPERTY(VisibleAnywhere, Category= "Particle Non Frequent Data")
	uint32 GeometryHash = 0;

	UPROPERTY(VisibleAnywhere, Category= "Particle Non Frequent Data")
	FString DebugName;

	UPROPERTY(VisibleAnywhere, Category= "Particle Non Frequent Data")
	int32 ParticleIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category= "Particle Non Frequent Data")
	int32 SolverID = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, Category= "Particle Non Frequent Data")
	EChaosVDParticleType Type = EChaosVDParticleType::Unknown;

	UPROPERTY(VisibleAnywhere, Category= "Particle Position Rotation")
	FChaosVDParticlePositionRotation ParticlePositionRotation;

	UPROPERTY(VisibleAnywhere, Category= "Particle Particle Velocities")
	FChaosVDParticleVelocities ParticleVelocities;

	UPROPERTY(VisibleAnywhere, Category= "Particle Particle Dynamics")
	FChaosVDParticleDynamics ParticleDynamics;

	UPROPERTY(VisibleAnywhere, Category= "Particle Particle Dynamics Misc")
	FChaosVDParticleDynamicMisc ParticleDynamicsMisc;

	UPROPERTY(VisibleAnywhere, Category= "Particle Particle Mass Props")
	FChaosVDParticleMassProps ParticleMassProps;

	UPROPERTY(VisibleAnywhere, Category= "Particle Cluster Data")
	FChaosVDParticleCluster ParticleCluster;

	UPROPERTY()
	TArray<FChaosVDShapeCollisionData> CollisionDataPerShape;

	/** Only used during recording */
	FString* DebugNamePtr = nullptr;
	bool bHasDebugName = false;

	virtual bool HasValidData() const override { return bHasValidData; }

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
