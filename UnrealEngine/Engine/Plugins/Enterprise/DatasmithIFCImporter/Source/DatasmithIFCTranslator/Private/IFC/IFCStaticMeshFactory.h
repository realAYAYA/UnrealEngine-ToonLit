// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MeshTypes.h"

#include "Templates/UniquePtr.h"
#include "UObject/ObjectMacros.h"
#include "Misc/SecureHash.h"

#include "IFC/IFCReader.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDatasmithIFCMeshFactory, Log, All);

class UStaticMesh;
struct FMeshDescription;

namespace IFC
{
	class FStaticMeshFactory
	{
	public:
		FStaticMeshFactory();
		~FStaticMeshFactory();

		inline const TArray<FLogMessage>& GetLogMessages() const { return Messages; }

		void FillMeshDescription(const IFC::FObject* InObject, FMeshDescription* OutMeshDescription) const;

		FMD5Hash ComputeHash(const IFC::FObject& InObject);

		float GetUniformScale() const;
		void  SetUniformScale(const float Scale);

		void SetReserveSize(uint32 Size);

	protected:
		mutable TArray<FLogMessage> Messages;

		float		ImportUniformScale = 100.0f;

	private:
		using FIndexVertexIdMap = TMap<int32, FVertexID>;
	};

}  // namespace IFC
