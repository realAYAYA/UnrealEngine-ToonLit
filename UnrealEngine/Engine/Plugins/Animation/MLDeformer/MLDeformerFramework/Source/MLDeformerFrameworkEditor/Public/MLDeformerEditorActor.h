// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class UDebugSkelMeshComponent;
class UTextRenderComponent;
class UMLDeformerComponent;

namespace UE::MLDeformer
{
	/**
	 * The default ID's for editor actors.
	 * You can assign any ID to the editor actors, although some code in the editor model classes will assume
	 * that some of these exist. You can always overload certain methods to change that behavior though.
	 * @see FMLDeformerEditorActor
	 */
	enum : int32
	{
		/** The linear skinned (base) actor in training mode. */
		ActorID_Train_Base,

		/** The ground truth (target) actor in training mode. This is our training target that we try to approximate. */
		ActorID_Train_GroundTruth,

		/** The linear skinned (base) actor, in test mode. */
		ActorID_Test_Base,

		/** The ML Deformed actor, in test mode. This one should have an UMLDeformerComponent on it. */
		ActorID_Test_MLDeformed,

		/** The ground truth test actor, in test mode. This will represent your test anim sequence ground truth. */
		ActorID_Test_GroundTruth
	};

	/**
	 * An actor in the ML Deformer asset editor viewport.
	 * The UE Actor object itself is created by the FMLDeformerEditorModel class, after which this editor actor class
	 * can create components on it.
	 * It also contains some methods to control visibility and playback, among some other helper methods.
	 */
	class MLDEFORMERFRAMEWORKEDITOR_API FMLDeformerEditorActor
	{
	public:
		/**
		 * The construction settings, which can be used as info when creating new components.
		 */
		struct MLDEFORMERFRAMEWORKEDITOR_API FConstructSettings
		{
			/** The UE actor object that we should add components to. */
			AActor* Actor = nullptr;

			/** The ID of the actor. Look at the UE::MLDeformer::ActorID_Train_Base etc for example. */
			int32 TypeID = -1;

			/** The color of the label text. */
			FLinearColor LabelColor = FLinearColor(1.0f, 0.0f, 0.0f);

			/** The label text, which should be something like "Linear Skinned" or "Ground Truth", basically describing the actor. This will appear inside the render viewport. */
			FText LabelText;

			/** Is this an actor used during training? Set to false if it is a testing mode actor. */
			bool bIsTrainingActor = false;
		};

		FMLDeformerEditorActor(const FConstructSettings& Settings);
		virtual ~FMLDeformerEditorActor();

		// Main methods you can override.
		virtual void SetVisibility(bool bIsVisible);
		virtual bool IsVisible() const;
		virtual void SetPlayPosition(float TimeInSeconds, bool bAutoPause = true);
		virtual float GetPlayPosition() const;
		virtual void SetPlaySpeed(float PlaySpeed);
		virtual float GetPlaySpeed();

		virtual FBox GetBoundingBox() const;
		virtual void Pause(bool bPaused);
		virtual bool IsPlaying() const;
		virtual bool IsGroundTruthActor() const;
		virtual bool HasVisualMesh() const;

		AActor* GetActor() const { return Actor; }
		int32 GetTypeID() const { return TypeID; }

		UDebugSkelMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent.Get(); }
		UTextRenderComponent* GetLabelComponent() const { return LabelComponent.Get(); }
		UMLDeformerComponent* GetMLDeformerComponent() const { return MLDeformerComponent.Get(); }
		void SetSkeletalMeshComponent(UDebugSkelMeshComponent* SkelMeshComponent) { SkeletalMeshComponent = SkelMeshComponent; }
		void SetMLDeformerComponent(UMLDeformerComponent* Component) { MLDeformerComponent = Component; }
		void SetMeshOffsetFactor(float OffsetFactor) { MeshOffsetFactor = OffsetFactor; }
		bool IsTrainingActor() const { return bIsTrainingActor; }
		bool IsTestActor() const { return !bIsTrainingActor; }
		float GetMeshOffsetFactor() const { return MeshOffsetFactor; }

	protected:
		UTextRenderComponent* CreateLabelComponent(AActor* InActor, FLinearColor Color, const FText& Text) const;

	protected:
		/**
		 * The ID of the editor actor type.
		 * This can be used to identify what actor we are dealing with, for example the base actor or ML Deformed one etc.
		 */
		int32 TypeID = -1;

		/** The label component, which shows above the actor. */
		TObjectPtr<UTextRenderComponent> LabelComponent = nullptr;

		/** The actual actor pointer. */
		TObjectPtr<AActor> Actor = nullptr;

		/** The skeletal mesh component (can be nullptr). */
		TObjectPtr<UDebugSkelMeshComponent> SkeletalMeshComponent = nullptr;

		/** The ML Deformer component (can be nullptr). */
		TObjectPtr<UMLDeformerComponent> MLDeformerComponent = nullptr;

		/** The position offset factor of the actor. A value of 1.0 would offset the actor with the Mesh Spacing amount, a value of 2.0 would be two times the mesh spacing offset, etc. */
		float MeshOffsetFactor = 0.0f;

		/** Is this actor used for training? */
		bool bIsTrainingActor = true;
	};
} // namespace UE::MLDeformer
