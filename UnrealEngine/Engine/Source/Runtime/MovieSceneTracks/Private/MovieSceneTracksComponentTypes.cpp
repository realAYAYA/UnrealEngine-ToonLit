// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieSceneTracksComponentTypes.h"
#include "Camera/CameraShakeBase.h"
#include "Camera/CameraShakeSourceComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "MovieSceneTracksCustomAccessors.h"
#include "EntitySystem/BuiltInComponentTypes.h"
#include "EntitySystem/MovieSceneEntityManager.h"
#include "EntitySystem/MovieSceneEntitySystemLinker.h"
#include "EntitySystem/MovieSceneComponentRegistry.h"
#include "EntitySystem/MovieSceneBlenderSystem.h"
#include "Systems/MovieScenePiecewiseBoolBlenderSystem.h"
#include "Systems/MovieScenePiecewiseByteBlenderSystem.h"
#include "Systems/MovieScenePiecewiseEnumBlenderSystem.h"
#include "Systems/MovieScenePiecewiseIntegerBlenderSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"
#include "Systems/MovieScenePiecewiseDoubleBlenderSystem.h"
#include "EntitySystem/MovieScenePropertyComponentHandler.h"
#include "EntitySystem/MovieSceneEntityFactoryTemplates.h"
#include "EntitySystem/MovieScenePropertyMetaDataTraits.inl"
#include "PreAnimatedState/MovieScenePreAnimatedComponentTransformStorage.h"
#include "Systems/MovieSceneColorPropertySystem.h"
#include "Systems/MovieSceneVectorPropertySystem.h"
#include "MovieSceneObjectBindingID.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/App.h"
#include "PhysicsEngine/BodyInstance.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneTracksComponentTypes)

namespace UE
{
namespace MovieScene
{

/* ---------------------------------------------------------------------------
 * Transform conversion functions
 * ---------------------------------------------------------------------------*/
void ConvertOperationalProperty(const FIntermediate3DTransform& In, FEulerTransform& Out)
{
	Out.Location = In.GetTranslation();
	Out.Rotation = In.GetRotation();
	Out.Scale = In.GetScale();
}
void ConvertOperationalProperty(const FEulerTransform& In, FIntermediate3DTransform& Out)
{
	Out = FIntermediate3DTransform(In.Location, In.Rotation, In.Scale);
}

void ConvertOperationalProperty(const FIntermediate3DTransform& In, FTransform& Out)
{
	Out = FTransform(In.GetRotation().Quaternion(), In.GetTranslation(), In.GetScale());
}
void ConvertOperationalProperty(const FTransform& In, FIntermediate3DTransform& Out)
{
	FVector Location = In.GetTranslation();
	FRotator Rotation = In.GetRotation().Rotator();
	FVector Scale = In.GetScale3D();

	Out = FIntermediate3DTransform(Location, Rotation, Scale);
}

/* ---------------------------------------------------------------------------
 * Color conversion functions
 * ---------------------------------------------------------------------------*/
void ConvertOperationalProperty(const FIntermediateColor& InColor, FColor& Out)
{
	Out = InColor.GetColor();
}

void ConvertOperationalProperty(const FIntermediateColor& InColor, FLinearColor& Out)
{
	Out = InColor.GetLinearColor();
}

void ConvertOperationalProperty(const FIntermediateColor& InColor, FSlateColor& Out)
{
	Out = InColor.GetSlateColor();
}

void ConvertOperationalProperty(const FColor& InColor, FIntermediateColor& OutIntermediate)
{
	OutIntermediate = FIntermediateColor(InColor);
}

void ConvertOperationalProperty(const FLinearColor& InColor, FIntermediateColor& OutIntermediate)
{
	OutIntermediate = FIntermediateColor(InColor);
}

void ConvertOperationalProperty(const FSlateColor& InColor, FIntermediateColor& OutIntermediate)
{
	OutIntermediate = FIntermediateColor(InColor);
}


/* ---------------------------------------------------------------------------
 * Vector conversion functions
 * ---------------------------------------------------------------------------*/
void ConvertOperationalProperty(const FFloatIntermediateVector& InVector, FVector2f& Out)
{
	Out = FVector2f(InVector.X, InVector.Y);
}

void ConvertOperationalProperty(const FFloatIntermediateVector& InVector, FVector3f& Out)
{
	Out = FVector3f(InVector.X, InVector.Y, InVector.Z);
}

void ConvertOperationalProperty(const FFloatIntermediateVector& InVector, FVector4f& Out)
{
	Out = FVector4f(InVector.X, InVector.Y, InVector.Z, InVector.W);
}

void ConvertOperationalProperty(const FVector2f& In, FFloatIntermediateVector& Out)
{
	Out = FFloatIntermediateVector(In.X, In.Y);
}

void ConvertOperationalProperty(const FVector3f& In, FFloatIntermediateVector& Out)
{
	Out = FFloatIntermediateVector(In.X, In.Y, In.Z);
}

void ConvertOperationalProperty(const FVector4f& In, FFloatIntermediateVector& Out)
{
	Out = FFloatIntermediateVector(In.X, In.Y, In.Z, In.W);
}

void ConvertOperationalProperty(const FDoubleIntermediateVector& InVector, FVector2d& Out)
{
	Out = FVector2d(InVector.X, InVector.Y);
}

void ConvertOperationalProperty(const FDoubleIntermediateVector& InVector, FVector3d& Out)
{
	Out = FVector3d(InVector.X, InVector.Y, InVector.Z);
}

void ConvertOperationalProperty(const FDoubleIntermediateVector& InVector, FVector4d& Out)
{
	Out = FVector4d(InVector.X, InVector.Y, InVector.Z, InVector.W);
}

void ConvertOperationalProperty(const FVector2d& In, FDoubleIntermediateVector& Out)
{
	Out = FDoubleIntermediateVector(In.X, In.Y);
}

void ConvertOperationalProperty(const FVector3d& In, FDoubleIntermediateVector& Out)
{
	Out = FDoubleIntermediateVector(In.X, In.Y, In.Z);
}

void ConvertOperationalProperty(const FVector4d& In, FDoubleIntermediateVector& Out)
{
	Out = FDoubleIntermediateVector(In.X, In.Y, In.Z, In.W);
}

void ConvertOperationalProperty(float In, double& Out)
{
	Out = static_cast<double>(In);
}

void ConvertOperationalProperty(double In, float& Out)
{
	Out = static_cast<float>(In);
}

void ConvertOperationalProperty(const FObjectComponent& In, UObject*& Out)
{
	Out = In.GetObject();
}

void ConvertOperationalProperty(UObject* In, FObjectComponent& Out)
{
	Out = FObjectComponent::Strong(In);
}

uint8 GetSkeletalMeshAnimationMode(const UObject* Object)
{
	const USkeletalMeshComponent* SkeletalMeshComponent = CastChecked<const USkeletalMeshComponent>(Object);
	return SkeletalMeshComponent->GetAnimationMode();
}

void SetSkeletalMeshAnimationMode(UObject* Object, uint8 InAnimationMode)
{
	USkeletalMeshComponent* SkeletalMeshComponent = CastChecked<USkeletalMeshComponent>(Object);
	constexpr bool bForceInitAnimScriptInstance = false; // Avoid reinits each frame if an anim node track is added with AnimBlueprint mode
	SkeletalMeshComponent->SetAnimationMode((EAnimationMode::Type)InAnimationMode, bForceInitAnimScriptInstance);
}

void FIntermediate3DTransform::ApplyTo(USceneComponent* SceneComponent) const
{
	ApplyTransformTo(SceneComponent, *this);
}

void FIntermediate3DTransform::ApplyTransformTo(USceneComponent* SceneComponent, const FIntermediate3DTransform& Transform)
{
	double DeltaTime = FApp::GetDeltaTime();
	if (DeltaTime <= 0)
	{
		SetComponentTransform(SceneComponent, Transform);
	}
	else
	{
		/* Cache initial absolute position */
		FVector PreviousPosition = SceneComponent->GetComponentLocation();

		SetComponentTransform(SceneComponent, Transform);

		/* Get current absolute position and set component velocity */
		FVector CurrentPosition = SceneComponent->GetComponentLocation();
		FVector ComponentVelocity = (CurrentPosition - PreviousPosition) / DeltaTime;
		SceneComponent->ComponentVelocity = ComponentVelocity;
	}
}

void FIntermediate3DTransform::ApplyTranslationAndRotationTo(USceneComponent* SceneComponent, const FIntermediate3DTransform& Transform)
{
	double DeltaTime = FApp::GetDeltaTime();
	if (DeltaTime <= 0)
	{
		SetComponentTranslationAndRotation(SceneComponent, Transform);
	}
	else
	{
		/* Cache initial absolute position */
		FVector PreviousPosition = SceneComponent->GetComponentLocation();

		SetComponentTranslationAndRotation(SceneComponent, Transform);

		/* Get current absolute position and set component velocity */
		FVector CurrentPosition = SceneComponent->GetComponentLocation();
		FVector ComponentVelocity = (CurrentPosition - PreviousPosition) / DeltaTime;
		SceneComponent->ComponentVelocity = ComponentVelocity;
	}
}

USceneComponent* FComponentAttachParamsDestination::ResolveAttachment(AActor* InParentActor) const
{
	if (SocketName != NAME_None)
	{
		if (ComponentName != NAME_None )
		{
			TInlineComponentArray<USceneComponent*> PotentialAttachComponents(InParentActor);
			for (USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
			{
				if (PotentialAttachComponent->GetFName() == ComponentName && PotentialAttachComponent->DoesSocketExist(SocketName))
				{
					return PotentialAttachComponent;
				}
			}
		}
		else if (InParentActor->GetRootComponent()->DoesSocketExist(SocketName))
		{
			return InParentActor->GetRootComponent();
		}
	}
	else if (ComponentName != NAME_None )
	{
		TInlineComponentArray<USceneComponent*> PotentialAttachComponents(InParentActor);
		for (USceneComponent* PotentialAttachComponent : PotentialAttachComponents)
		{
			if (PotentialAttachComponent->GetFName() == ComponentName)
			{
				return PotentialAttachComponent;
			}
		}
	}

	if (InParentActor->GetDefaultAttachComponent())
	{
		return InParentActor->GetDefaultAttachComponent();
	}
	else
	{
		return InParentActor->GetRootComponent();
	}
}

void FComponentAttachParams::ApplyAttach(USceneComponent* ChildComponentToAttach, USceneComponent* NewAttachParent, const FName& SocketName) const
{
	if (ChildComponentToAttach->GetAttachParent() != NewAttachParent || ChildComponentToAttach->GetAttachSocketName() != SocketName)
	{
		// Attachment changes may try to mark a package as dirty but this prevents us from restoring the level to the pre-animated
		// state correctly which causes issues with validation.
		MovieSceneHelpers::FMovieSceneScopedPackageDirtyGuard DirtyFlagGuard(ChildComponentToAttach);

		FAttachmentTransformRules AttachmentRules(AttachmentLocationRule, AttachmentRotationRule, AttachmentScaleRule, false);
		ChildComponentToAttach->AttachToComponent(NewAttachParent, AttachmentRules, SocketName);
	}

	// Match the component velocity of the parent. If the attached child has any transformation, the velocity will be 
	// computed by the component transform system.
	if (ChildComponentToAttach->GetAttachParent())
	{
		ChildComponentToAttach->ComponentVelocity = ChildComponentToAttach->GetAttachParent()->GetComponentVelocity();
	}
}

void FComponentDetachParams::ApplyDetach(USceneComponent* ChildComponentToAttach, USceneComponent* NewAttachParent, const FName& SocketName) const
{
	// Detach if there was no pre-existing parent
	if (!NewAttachParent)
	{
		MovieSceneHelpers::FMovieSceneScopedPackageDirtyGuard DirtyFlagGuard(ChildComponentToAttach);

		FDetachmentTransformRules DetachmentRules(DetachmentLocationRule, DetachmentRotationRule, DetachmentScaleRule, false);
		ChildComponentToAttach->DetachFromComponent(DetachmentRules);
	}
	else
	{
		MovieSceneHelpers::FMovieSceneScopedPackageDirtyGuard DirtyFlagGuard(ChildComponentToAttach);

		ChildComponentToAttach->AttachToComponent(NewAttachParent, FAttachmentTransformRules::KeepRelativeTransform, SocketName);
	}
}


static bool GMovieSceneTracksComponentTypesDestroyed = false;
static TUniquePtr<FMovieSceneTracksComponentTypes> GMovieSceneTracksComponentTypes;

struct FFloatHandler : TPropertyComponentHandler<FFloatPropertyTraits, double>
{
};

struct FColorHandler : TPropertyComponentHandler<FColorPropertyTraits, double, double, double, double>
{
	virtual void DispatchInitializePropertyMetaDataTasks(const FPropertyDefinition& Definition, FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents, UMovieSceneEntitySystemLinker* Linker) override
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

		FEntityTaskBuilder()
		.Read(BuiltInComponents->BoundObject)
		.Read(BuiltInComponents->PropertyBinding)
		.Write(TrackComponents->Color.MetaDataComponents.GetType<0>())
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.Iterate_PerEntity(&Linker->EntityManager, [](UObject* Object, const FMovieScenePropertyBinding& Binding, EColorPropertyType& OutType)
		{
			FStructProperty* BoundProperty = CastField<FStructProperty>(FTrackInstancePropertyBindings::FindProperty(Object, Binding.PropertyPath.ToString()));
			if (ensure(BoundProperty && BoundProperty->Struct))
			{
				if (BoundProperty->Struct == TBaseStructure<FColor>::Get())
				{
					// We assume the color we get back is in sRGB, assigning it to a linear color will implicitly
					// convert it to a linear color instead of using ReinterpretAsLinear which will just change the
					// bytes into floats using divide by 255.
					OutType = EColorPropertyType::Color;
				}
				else if (BoundProperty->Struct == TBaseStructure<FSlateColor>::Get())
				{
					OutType = EColorPropertyType::Slate;
				}
				else
				{
					ensure(BoundProperty->Struct == TBaseStructure<FLinearColor>::Get());
					OutType = EColorPropertyType::Linear;
				}
			}
			else
			{
				OutType = EColorPropertyType::Linear;
			}
		});
	}
};

struct FFloatParameterHandler : TPropertyComponentHandler<FFloatParameterTraits, double>
{
	virtual IInitialValueProcessor* GetInitialValueProcessor() override
	{
		return nullptr;
	}
};

struct FColorParameterHandler : TPropertyComponentHandler<FColorParameterTraits, double, double, double, double>
{
	virtual IInitialValueProcessor* GetInitialValueProcessor() override
	{
		return nullptr;
	}
};


struct FFloatVectorHandler : TPropertyComponentHandler<FFloatVectorPropertyTraits, double, double, double, double>
{
	virtual void DispatchInitializePropertyMetaDataTasks(const FPropertyDefinition& Definition, FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents, UMovieSceneEntitySystemLinker* Linker) override
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

		FEntityTaskBuilder()
		.Read(BuiltInComponents->BoundObject)
		.Read(BuiltInComponents->PropertyBinding)
		.Write(TrackComponents->FloatVector.MetaDataComponents.GetType<0>())
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.Iterate_PerEntity(&Linker->EntityManager, [](UObject* Object, const FMovieScenePropertyBinding& Binding, FVectorPropertyMetaData& OutMetaData)
		{
			FStructProperty* BoundProperty = CastField<FStructProperty>(FTrackInstancePropertyBindings::FindProperty(Object, Binding.PropertyPath.ToString()));
			if (ensure(BoundProperty && BoundProperty->Struct))
			{
				if (BoundProperty->Struct == TVariantStructure<FVector2f>::Get())
				{
					OutMetaData.NumChannels = 2;
				}
				else if (BoundProperty->Struct == TVariantStructure<FVector3f>::Get())
				{
					OutMetaData.NumChannels = 3;
				}
				else
				{
					ensure(BoundProperty->Struct == TVariantStructure<FVector4f>::Get());
					OutMetaData.NumChannels = 4;
				}
			}
			else
			{
				OutMetaData.NumChannels = 4;
			}
		});
	}
};


struct FDoubleVectorHandler : TPropertyComponentHandler<FDoubleVectorPropertyTraits, double, double, double, double>
{
	virtual void DispatchInitializePropertyMetaDataTasks(const FPropertyDefinition& Definition, FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents, UMovieSceneEntitySystemLinker* Linker) override
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

		FEntityTaskBuilder()
		.Read(BuiltInComponents->BoundObject)
		.Read(BuiltInComponents->PropertyBinding)
		.Write(TrackComponents->DoubleVector.MetaDataComponents.GetType<0>())
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.Iterate_PerEntity(&Linker->EntityManager, [](UObject* Object, const FMovieScenePropertyBinding& Binding, FVectorPropertyMetaData& OutMetaData)
		{
			FStructProperty* BoundProperty = CastField<FStructProperty>(FTrackInstancePropertyBindings::FindProperty(Object, Binding.PropertyPath.ToString()));
			if (ensure(BoundProperty && BoundProperty->Struct))
			{
				if (BoundProperty->Struct == TBaseStructure<FVector2D>::Get())
				{
					OutMetaData.NumChannels = 2;
				}
				else if (BoundProperty->Struct == TBaseStructure<FVector>::Get() || BoundProperty->Struct == TVariantStructure<FVector3d>::Get())
				{
					OutMetaData.NumChannels = 3;
				}
				else
				{
					ensure(BoundProperty->Struct == TBaseStructure<FVector4>::Get() || BoundProperty->Struct == TVariantStructure<FVector4d>::Get());
					OutMetaData.NumChannels = 4;
				}
			}
			else
			{
				OutMetaData.NumChannels = 4;
			}
		});
	}
};

struct FBoolHandler : TPropertyComponentHandler<FBoolPropertyTraits, bool>
{
	virtual void DispatchInitializePropertyMetaDataTasks(const FPropertyDefinition& Definition, FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents, UMovieSceneEntitySystemLinker* Linker) override
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

		FEntityTaskBuilder()
		.Read(BuiltInComponents->BoundObject)
		.Read(BuiltInComponents->PropertyBinding)
		.Write(TrackComponents->Bool.MetaDataComponents.GetType<0>())
		.FilterAll({ BuiltInComponents->Tags.NeedsLink })
		.Iterate_PerEntity(&Linker->EntityManager, [](UObject* Object, const FMovieScenePropertyBinding& Binding, FBoolPropertyTraits::FBoolMetaData& OutMetaData)
		{
			FBoolProperty* BoundProperty = CastField<FBoolProperty>(FTrackInstancePropertyBindings::FindProperty(Object, Binding.PropertyPath.ToString()));
			if (ensure(BoundProperty))
			{
				if (BoundProperty->IsNativeBool())
				{
					OutMetaData.BitFieldSize = 0;
					OutMetaData.BitIndex     = 0;
				}
				else
				{
					auto FieldMask = BoundProperty->GetFieldMask();
					static_assert(std::is_same_v<decltype(FieldMask), uint8>, "Unexpected size of field mask returned from FBoolProperty::FieldMask");

					OutMetaData.BitFieldSize = static_cast<uint8>(BoundProperty->ElementSize);
					OutMetaData.BitIndex     = static_cast<uint8>(FMath::CountTrailingZeros(FieldMask));
				}
			}
		});
	}
};

struct FComponentTransformHandler : TPropertyComponentHandler<FComponentTransformPropertyTraits, double, double, double, double, double, double, double, double, double>
{
	TSharedPtr<IPreAnimatedStorage> GetPreAnimatedStateStorage(const FPropertyDefinition& Definition, FPreAnimatedStateExtension* Container) override
	{
		return Container->GetOrCreateStorage<FPreAnimatedComponentTransformStorage>();
	}
};

struct FObjectHandler : TPropertyComponentHandler<FObjectPropertyTraits, FObjectComponent>
{
	virtual void DispatchInitializePropertyMetaDataTasks(const FPropertyDefinition& Definition, FSystemTaskPrerequisites& InPrerequisites, FSystemSubsequentTasks& Subsequents, UMovieSceneEntitySystemLinker* Linker) override
	{
		FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();
		FMovieSceneTracksComponentTypes* TrackComponents = FMovieSceneTracksComponentTypes::Get();

		FEntityTaskBuilder()
			.Read(BuiltInComponents->BoundObject)
			.Read(BuiltInComponents->PropertyBinding)
			.Write(TrackComponents->Object.MetaDataComponents.GetType<0>())
			.FilterAll({ BuiltInComponents->Tags.NeedsLink })
			.Iterate_PerEntity(&Linker->EntityManager, [](UObject* Object, const FMovieScenePropertyBinding& Binding, FObjectPropertyTraits::FObjectMetadata& OutMetaData)
				{
					FObjectPropertyBase* BoundProperty = CastField<FObjectPropertyBase>(FTrackInstancePropertyBindings::FindProperty(Object, Binding.PropertyPath.ToString()));
					if (ensure(BoundProperty))
					{
						OutMetaData.ObjectClass = BoundProperty->PropertyClass;
						OutMetaData.bAllowsClear = !BoundProperty->HasAnyPropertyFlags(CPF_NoClear);
					}
				});
	}
};

FMovieSceneTracksComponentTypes::FMovieSceneTracksComponentTypes()
{
	FComponentRegistry* ComponentRegistry = UMovieSceneEntitySystemLinker::GetComponents();

	ComponentRegistry->NewPropertyType(Bool, TEXT("bool"));
	ComponentRegistry->NewPropertyType(Byte, TEXT("byte"));
	ComponentRegistry->NewPropertyType(Enum, TEXT("enum"));
	ComponentRegistry->NewPropertyType(Float, TEXT("float"));
	ComponentRegistry->NewPropertyType(Double, TEXT("double"));
	ComponentRegistry->NewPropertyType(Color, TEXT("color"));
	ComponentRegistry->NewPropertyType(Integer, TEXT("int32"));
	ComponentRegistry->NewPropertyType(FloatVector, TEXT("float vector"));
	ComponentRegistry->NewPropertyType(DoubleVector, TEXT("double vector"));
	ComponentRegistry->NewPropertyType(String, TEXT("FString"));
	ComponentRegistry->NewPropertyType(Object, TEXT("Object"));

	ComponentRegistry->NewPropertyType(Transform, TEXT("FTransform"));
	ComponentRegistry->NewPropertyType(EulerTransform, TEXT("FEulerTransform"));
	ComponentRegistry->NewPropertyType(ComponentTransform, TEXT("Component Transform"));

	ComponentRegistry->NewPropertyType(FloatParameter, TEXT("float parameter"));
	ComponentRegistry->NewPropertyType(ColorParameter, TEXT("color parameter"));

	Bool.MetaDataComponents.Initialize(ComponentRegistry, TEXT("Bool Bitfield"));
	Color.MetaDataComponents.Initialize(ComponentRegistry, TEXT("Color Type"));
	FloatVector.MetaDataComponents.Initialize(ComponentRegistry, TEXT("Num Float Vector Channels"));
	DoubleVector.MetaDataComponents.Initialize(ComponentRegistry, TEXT("Num Double Vector Channels"));
	Object.MetaDataComponents.Initialize(ComponentRegistry, TEXT("Object Class"));

	ComponentRegistry->NewComponentType(&QuaternionRotationChannel[0], TEXT("Quaternion Rotation Channel 0"));
	ComponentRegistry->NewComponentType(&QuaternionRotationChannel[1], TEXT("Quaternion Rotation Channel 1"));
	ComponentRegistry->NewComponentType(&QuaternionRotationChannel[2], TEXT("Quaternion Rotation Channel 2"));

	ComponentRegistry->NewComponentType(&ConstraintChannel, TEXT("Constraint Channel"));

	ComponentRegistry->NewComponentType(&AttachParent, TEXT("Attach Parent"));
	ComponentRegistry->NewComponentType(&AttachComponent, TEXT("Attachment Component"));
	ComponentRegistry->NewComponentType(&AttachParentBinding, TEXT("Attach Parent Binding"));
	ComponentRegistry->NewComponentType(&FloatPerlinNoiseChannel, TEXT("Float Perlin Noise Channel"));
	ComponentRegistry->NewComponentType(&DoublePerlinNoiseChannel, TEXT("Double Perlin Noise Channel"));

	ComponentRegistry->NewComponentType(&SkeletalAnimation, TEXT("Skeletal Animation"));

	ComponentRegistry->NewComponentType(&LevelVisibility, TEXT("Level Visibility"));
	ComponentRegistry->NewComponentType(&DataLayer, TEXT("Data Layer"));

	ComponentRegistry->NewComponentType(&ComponentMaterialInfo,		TEXT("Component Material Info"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&BoundMaterial,				TEXT("Bound Material"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&MPC,						TEXT("Material Parameter Collection"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);

	ComponentRegistry->NewComponentType(&BoolParameterName,      TEXT("Bool Parameter Name"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&ScalarParameterName,    TEXT("Scalar Parameter Name"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&Vector2DParameterName,  TEXT("Vector2D Parameter Name"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&VectorParameterName,    TEXT("Vector Parameter Name"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&ColorParameterName,     TEXT("Color Parameter Name"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&TransformParameterName, TEXT("Transform Parameter Name"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);

	ComponentRegistry->NewComponentType(&ScalarMaterialParameterInfo, TEXT("Scalar Material Parameter Info"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&ColorMaterialParameterInfo, TEXT("Color Material Parameter Info"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);
	ComponentRegistry->NewComponentType(&VectorMaterialParameterInfo, TEXT("Vector Material Parameter Info"), EComponentTypeFlags::CopyToChildren | EComponentTypeFlags::CopyToOutput);

	ComponentRegistry->NewComponentType(&Fade,                   TEXT("Fade"), EComponentTypeFlags::CopyToChildren);

	ComponentRegistry->NewComponentType(&Audio,                  TEXT("Audio"), EComponentTypeFlags::CopyToChildren);
	ComponentRegistry->NewComponentType(&AudioInputs,            TEXT("Audio Inputs"), EComponentTypeFlags::CopyToChildren);
	ComponentRegistry->NewComponentType(&AudioTriggerName,       TEXT("Audio Trigger Name"), EComponentTypeFlags::CopyToChildren);

	ComponentRegistry->NewComponentType(&CameraShake,            TEXT("Camera Shake"), EComponentTypeFlags::CopyToChildren);
	ComponentRegistry->NewComponentType(&CameraShakeInstance,    TEXT("Camera Shake Instance"), EComponentTypeFlags::Preserved);

	Tags.BoundMaterialChanged = ComponentRegistry->NewTag(TEXT("Bound Material Changed"));
	FBuiltInComponentTypes::Get()->RequiresInstantiationMask.Set(Tags.BoundMaterialChanged);

	Tags.Slomo = ComponentRegistry->NewTag(TEXT("Slomo"));
	ComponentRegistry->Factories.DefineChildComponent(Tags.Slomo, Tags.Slomo);

	Tags.Visibility = ComponentRegistry->NewTag(TEXT("Visibility"));
	ComponentRegistry->Factories.DefineChildComponent(Tags.Visibility, Tags.Visibility);

	// Used to indicate the ParameterName component for certain parameter types (scalar, vector2d, vector, color)
	// should be interpreted as an index for custom primitive data.
	Tags.CustomPrimitiveData = ComponentRegistry->NewTag(TEXT("Custom Primitive Data"));

	FBuiltInComponentTypes* BuiltInComponents = FBuiltInComponentTypes::Get();

	// --------------------------------------------------------------------------------------------
	// Set up bool properties
	BuiltInComponents->PropertyRegistry.DefineProperty(Bool, TEXT("Apply bool Properties"))
	.AddSoleChannel(BuiltInComponents->BoolResult)
	.SetBlenderSystem<UMovieScenePiecewiseBoolBlenderSystem>()
	.SetCustomAccessors(&Accessors.Bool)
	.Commit(FBoolHandler());

	// Set up FTransform properties
	BuiltInComponents->PropertyRegistry.DefineCompositeProperty(Transform, TEXT("Apply FTransform Properties"))
	.AddComposite(BuiltInComponents->DoubleResult[0], &FIntermediate3DTransform::T_X)
	.AddComposite(BuiltInComponents->DoubleResult[1], &FIntermediate3DTransform::T_Y)
	.AddComposite(BuiltInComponents->DoubleResult[2], &FIntermediate3DTransform::T_Z)
	.AddComposite(BuiltInComponents->DoubleResult[3], &FIntermediate3DTransform::R_X)
	.AddComposite(BuiltInComponents->DoubleResult[4], &FIntermediate3DTransform::R_Y)
	.AddComposite(BuiltInComponents->DoubleResult[5], &FIntermediate3DTransform::R_Z)
	.AddComposite(BuiltInComponents->DoubleResult[6], &FIntermediate3DTransform::S_X)
	.AddComposite(BuiltInComponents->DoubleResult[7], &FIntermediate3DTransform::S_Y)
	.AddComposite(BuiltInComponents->DoubleResult[8], &FIntermediate3DTransform::S_Z)
	.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
	.Commit();

	// --------------------------------------------------------------------------------------------
	// Set up byte properties
	BuiltInComponents->PropertyRegistry.DefineProperty(Byte, TEXT("Apply Byte Properties"))
	.AddSoleChannel(BuiltInComponents->ByteResult)
	.SetBlenderSystem<UMovieScenePiecewiseByteBlenderSystem>()
	.SetCustomAccessors(&Accessors.Byte)
	.Commit();

	// --------------------------------------------------------------------------------------------
	// Set up enum properties
	BuiltInComponents->PropertyRegistry.DefineProperty(Enum, TEXT("Apply Enum Properties"))
	.AddSoleChannel(BuiltInComponents->ByteResult)
	.SetBlenderSystem<UMovieScenePiecewiseEnumBlenderSystem>()
	.SetCustomAccessors(&Accessors.Enum)
	.Commit();

	// --------------------------------------------------------------------------------------------
	// Set up integer properties
	BuiltInComponents->PropertyRegistry.DefineProperty(Integer, TEXT("Apply Integer Properties"))
	.AddSoleChannel(BuiltInComponents->IntegerResult)
	.SetBlenderSystem<UMovieScenePiecewiseIntegerBlenderSystem>()
	.SetCustomAccessors(&Accessors.Integer)
	.Commit();

	// --------------------------------------------------------------------------------------------
	// Set up float properties
	BuiltInComponents->PropertyRegistry.DefineProperty(Float, TEXT("Apply float Properties"))
	.AddSoleChannel(BuiltInComponents->DoubleResult[0])
	.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
	.SetCustomAccessors(&Accessors.Float)
	.Commit(FFloatHandler());

	// --------------------------------------------------------------------------------------------
	// Set up double properties
	BuiltInComponents->PropertyRegistry.DefineProperty(Double, TEXT("Apply Double Properties"))
	.AddSoleChannel(BuiltInComponents->DoubleResult[0])
	.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
	.SetCustomAccessors(&Accessors.Double)
	.Commit();

	// --------------------------------------------------------------------------------------------
	// Set up color properties
	BuiltInComponents->PropertyRegistry.DefineCompositeProperty(Color, TEXT("Apply Color Properties"))
	.AddComposite(BuiltInComponents->DoubleResult[0], &FIntermediateColor::R)
	.AddComposite(BuiltInComponents->DoubleResult[1], &FIntermediateColor::G)
	.AddComposite(BuiltInComponents->DoubleResult[2], &FIntermediateColor::B)
	.AddComposite(BuiltInComponents->DoubleResult[3], &FIntermediateColor::A)
	.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
	.SetCustomAccessors(&Accessors.Color)
	.Commit(FColorHandler());

	// --------------------------------------------------------------------------------------------
	// Set up string properties
	BuiltInComponents->PropertyRegistry.DefineProperty(String, TEXT("Apply String Properties"))
	.AddSoleChannel(BuiltInComponents->StringResult)
	.Commit();

	// --------------------------------------------------------------------------------------------
	// Set up Object properties
	BuiltInComponents->PropertyRegistry.DefineProperty(Object, TEXT("Apply Object Properties"))
	.AddSoleChannel(BuiltInComponents->ObjectResult)
	.SetCustomAccessors(&Accessors.Object)
	.Commit(FObjectHandler());

	// --------------------------------------------------------------------------------------------
	// Set up float parameters
	BuiltInComponents->PropertyRegistry.DefineProperty(FloatParameter, TEXT("Apply Float Parameters"))
	.AddSoleChannel(BuiltInComponents->DoubleResult[0])
	.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
	.Commit(FFloatParameterHandler());

	// --------------------------------------------------------------------------------------------
	// Set up color parameters
	BuiltInComponents->PropertyRegistry.DefineCompositeProperty(ColorParameter, TEXT("Apply Color Parameters"))
	.AddComposite(BuiltInComponents->DoubleResult[0], &FIntermediateColor::R)
	.AddComposite(BuiltInComponents->DoubleResult[1], &FIntermediateColor::G)
	.AddComposite(BuiltInComponents->DoubleResult[2], &FIntermediateColor::B)
	.AddComposite(BuiltInComponents->DoubleResult[3], &FIntermediateColor::A)
	.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
	.Commit(FColorParameterHandler());

	Accessors.Byte.Add(
		USkeletalMeshComponent::StaticClass(), USkeletalMeshComponent::GetAnimationModePropertyNameChecked(),
		GetSkeletalMeshAnimationMode, SetSkeletalMeshAnimationMode);
	Accessors.Enum.Add(
		USkeletalMeshComponent::StaticClass(), USkeletalMeshComponent::GetAnimationModePropertyNameChecked(),
		GetSkeletalMeshAnimationMode, SetSkeletalMeshAnimationMode);

	// --------------------------------------------------------------------------------------------
	// Set up vector properties
	BuiltInComponents->PropertyRegistry.DefineCompositeProperty(FloatVector, TEXT("Apply FloatVector Properties"))
	.AddComposite(BuiltInComponents->DoubleResult[0], &FFloatIntermediateVector::X)
	.AddComposite(BuiltInComponents->DoubleResult[1], &FFloatIntermediateVector::Y)
	.AddComposite(BuiltInComponents->DoubleResult[2], &FFloatIntermediateVector::Z)
	.AddComposite(BuiltInComponents->DoubleResult[3], &FFloatIntermediateVector::W)
	.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
	.SetCustomAccessors(&Accessors.FloatVector)
	.Commit(FFloatVectorHandler());

	BuiltInComponents->PropertyRegistry.DefineCompositeProperty(DoubleVector, TEXT("Apply DoubleVector Properties"))
	.AddComposite(BuiltInComponents->DoubleResult[0], &FDoubleIntermediateVector::X)
	.AddComposite(BuiltInComponents->DoubleResult[1], &FDoubleIntermediateVector::Y)
	.AddComposite(BuiltInComponents->DoubleResult[2], &FDoubleIntermediateVector::Z)
	.AddComposite(BuiltInComponents->DoubleResult[3], &FDoubleIntermediateVector::W)
	.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
	.SetCustomAccessors(&Accessors.DoubleVector)
	.Commit(FDoubleVectorHandler());

	// --------------------------------------------------------------------------------------------
	// Set up FEulerTransform properties
	BuiltInComponents->PropertyRegistry.DefineCompositeProperty(EulerTransform, TEXT("Apply FEulerTransform Properties"))
	.AddComposite(BuiltInComponents->DoubleResult[0], &FIntermediate3DTransform::T_X)
	.AddComposite(BuiltInComponents->DoubleResult[1], &FIntermediate3DTransform::T_Y)
	.AddComposite(BuiltInComponents->DoubleResult[2], &FIntermediate3DTransform::T_Z)
	.AddComposite(BuiltInComponents->DoubleResult[3], &FIntermediate3DTransform::R_X)
	.AddComposite(BuiltInComponents->DoubleResult[4], &FIntermediate3DTransform::R_Y)
	.AddComposite(BuiltInComponents->DoubleResult[5], &FIntermediate3DTransform::R_Z)
	.AddComposite(BuiltInComponents->DoubleResult[6], &FIntermediate3DTransform::S_X)
	.AddComposite(BuiltInComponents->DoubleResult[7], &FIntermediate3DTransform::S_Y)
	.AddComposite(BuiltInComponents->DoubleResult[8], &FIntermediate3DTransform::S_Z)
	.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
	.Commit();

	// --------------------------------------------------------------------------------------------
	// Set up component transforms
	{
		BuiltInComponents->PropertyRegistry.DefineCompositeProperty(ComponentTransform, TEXT("Call USceneComponent::SetRelativeTransform"))
		.AddComposite(BuiltInComponents->DoubleResult[0], &FIntermediate3DTransform::T_X)
		.AddComposite(BuiltInComponents->DoubleResult[1], &FIntermediate3DTransform::T_Y)
		.AddComposite(BuiltInComponents->DoubleResult[2], &FIntermediate3DTransform::T_Z)
		.AddComposite(BuiltInComponents->DoubleResult[3], &FIntermediate3DTransform::R_X)
		.AddComposite(BuiltInComponents->DoubleResult[4], &FIntermediate3DTransform::R_Y)
		.AddComposite(BuiltInComponents->DoubleResult[5], &FIntermediate3DTransform::R_Z)
		.AddComposite(BuiltInComponents->DoubleResult[6], &FIntermediate3DTransform::S_X)
		.AddComposite(BuiltInComponents->DoubleResult[7], &FIntermediate3DTransform::S_Y)
		.AddComposite(BuiltInComponents->DoubleResult[8], &FIntermediate3DTransform::S_Z)
		.SetBlenderSystem<UMovieScenePiecewiseDoubleBlenderSystem>()
		.SetCustomAccessors(&Accessors.ComponentTransform)
		.Commit(FComponentTransformHandler());
	}

	// --------------------------------------------------------------------------------------------
	// Set up quaternion rotation components
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(QuaternionRotationChannel); ++Index)
	{
		ComponentRegistry->Factories.DuplicateChildComponent(QuaternionRotationChannel[Index]);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(QuaternionRotationChannel[Index], BuiltInComponents->DoubleResult[Index + 3]);
		ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(QuaternionRotationChannel[Index], BuiltInComponents->EvalTime);
	}

	// -------------------------------------------------------------------------------------------
	// Set up constraint components
	ComponentRegistry->Factories.DuplicateChildComponent(ConstraintChannel);
	ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(ConstraintChannel, BuiltInComponents->EvalTime);

	// --------------------------------------------------------------------------------------------
	// Set up attachment components
	ComponentRegistry->Factories.DefineChildComponent(AttachParentBinding, AttachParent);

	ComponentRegistry->Factories.DuplicateChildComponent(AttachParentBinding);
	ComponentRegistry->Factories.DuplicateChildComponent(AttachComponent);

	// --------------------------------------------------------------------------------------------
	// Set up PerlinNoise components
	ComponentRegistry->Factories.DuplicateChildComponent(FloatPerlinNoiseChannel);
	ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(FloatPerlinNoiseChannel, BuiltInComponents->EvalSeconds);

	ComponentRegistry->Factories.DuplicateChildComponent(DoublePerlinNoiseChannel);
	ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(DoublePerlinNoiseChannel, BuiltInComponents->EvalSeconds);

	// --------------------------------------------------------------------------------------------
	// Set up SkeletalAnimation components
	ComponentRegistry->Factories.DuplicateChildComponent(SkeletalAnimation);

	// --------------------------------------------------------------------------------------------
	// Set up custom primitive data components
	ComponentRegistry->Factories.DefineChildComponent(Tags.CustomPrimitiveData, Tags.CustomPrimitiveData);

	// --------------------------------------------------------------------------------------------
	// Set up camera shake components
	ComponentRegistry->Factories.DefineMutuallyInclusiveComponent(CameraShake, CameraShakeInstance);

	InitializeMovieSceneTracksAccessors(this);
}

FMovieSceneTracksComponentTypes::~FMovieSceneTracksComponentTypes()
{
}

void FMovieSceneTracksComponentTypes::Destroy()
{
	GMovieSceneTracksComponentTypes.Reset();
	GMovieSceneTracksComponentTypesDestroyed = true;
}

FMovieSceneTracksComponentTypes* FMovieSceneTracksComponentTypes::Get()
{
	if (!GMovieSceneTracksComponentTypes.IsValid())
	{
		check(!GMovieSceneTracksComponentTypesDestroyed);
		GMovieSceneTracksComponentTypes.Reset(new FMovieSceneTracksComponentTypes);
	}
	return GMovieSceneTracksComponentTypes.Get();
}

} // namespace MovieScene
} // namespace UE

FPerlinNoiseParams::FPerlinNoiseParams()
	: Frequency(4.0f)
	, Amplitude(1.0f)
	, Offset(0)
{
}

FPerlinNoiseParams::FPerlinNoiseParams(float InFrequency, double InAmplitude)
	: Frequency(InFrequency)
	, Amplitude(InAmplitude)
	, Offset(0)
{
}

void FPerlinNoiseParams::RandomizeOffset(float InMaxOffset)
{
	Offset = FMath::FRand() * InMaxOffset;
}

