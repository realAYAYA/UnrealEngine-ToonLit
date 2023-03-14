// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/ManagedArrayCollection.h"

namespace Chaos::Softs
{
	class CHAOS_API FThreadingProxy {
		const UObject* Owner = nullptr;
		FName TypeName = FName("");

	public:
		typedef const UObject* FKey;

		FThreadingProxy(const UObject* InOwner = nullptr, FName InTypeName = FName(""))
			: Owner(InOwner)
			, TypeName(InTypeName)
		{}
		virtual ~FThreadingProxy() {}

		FName BaseTypeName() const { return TypeName; }

		template<class T>
		T* As() { return T::TypeName().IsEqual(BaseTypeName()) ? (T*)this : nullptr; }

		template<class T>
		T* As() const { return T::TypeName().IsEqual(BaseTypeName()) ? (T*)this : nullptr; }

		const UObject* GetOwner() const { return Owner; }


		class FBuffer
		{
			const UObject* Owner = nullptr;
			FName TypeName = FName("");

		public:
			FBuffer(const FThreadingProxy& Ref)
				: Owner(Ref.GetOwner())
				, TypeName(Ref.BaseTypeName()) {}

			FBuffer(const UObject* InOwner,FName InTypeName)
				: Owner(InOwner)
				, TypeName(InTypeName) {}
			virtual ~FBuffer() {}

			FName BaseTypeName() const { return TypeName; }

			template<class T>
			T* As() { return T::Source::TypeName().IsEqual(BaseTypeName()) ? (T*)this : nullptr; }

			template<class T>
			T* As() const { return T::Source::TypeName().IsEqual(BaseTypeName()) ? (T*)this : nullptr; }
		};
	};

	class CHAOS_API FFleshThreadingProxy : public FThreadingProxy
	{
		typedef FThreadingProxy Super;
		FTransform InitialTransform = FTransform::Identity;
		const FManagedArrayCollection& Rest;
		FManagedArrayCollection Dynamic;
		FIntVector2 SolverParticleRange = FIntVector2(0, 0);

	public:

		FFleshThreadingProxy(UObject* InOwner,
			const FTransform& InInitialTransform,
			const FManagedArrayCollection& InRest,
			const FManagedArrayCollection& InDynamic)
			: Super(InOwner, TypeName())
			, InitialTransform(InInitialTransform)
			, Rest(InRest)
			, Dynamic(InDynamic)
		{}
		virtual ~FFleshThreadingProxy() {}

		static FName TypeName() { return FName("Flesh"); }

		const FTransform& GetInitialTransform() const
		{
			return InitialTransform;
		}

		void SetSolverParticleRange(int32 InStart, int32 InRange)
		{
			SolverParticleRange[0] = InStart;
			SolverParticleRange[1] = InRange;
		}

		const FIntVector2& GetSolverParticleRange() const
		{
			return SolverParticleRange;
		}

		FManagedArrayCollection& GetDynamicCollection() {
			return Dynamic;
		}
		const FManagedArrayCollection& GetDynamicCollection() const {
			return Dynamic;
		}

		const FManagedArrayCollection& GetRestCollection() const {
			return Rest;
		}

		class FFleshInputBuffer : public FThreadingProxy::FBuffer
		{
			typedef FThreadingProxy::FBuffer Super;

		public:
			FFleshInputBuffer(const TArray<FTransform> & InTransforms, const UObject* InOwner = nullptr)
				: Super(InOwner, FFleshThreadingProxy::TypeName())
				, Transforms(InTransforms)
			{}
			virtual ~FFleshInputBuffer() {}

			TArray<FTransform> Transforms;
			FManagedArrayCollection InputData;
		};

		class FFleshOutputBuffer : public FThreadingProxy::FBuffer
		{
			typedef FThreadingProxy::FBuffer Super;

		public:
			typedef FFleshThreadingProxy Source;
			FFleshOutputBuffer(const Source& Ref)
				: Super(Ref)
			{
				// Ref.Dynamic will have updated solver data 
				Dynamic.AddAttribute<FVector3f>("Vertex", FGeometryCollection::VerticesGroup);
				Dynamic.CopyMatchingAttributesFrom(Ref.GetDynamicCollection());
			}
			virtual ~FFleshOutputBuffer() {}

			FManagedArrayCollection Dynamic;
		};


	};
}// namespace Chaos::Softs