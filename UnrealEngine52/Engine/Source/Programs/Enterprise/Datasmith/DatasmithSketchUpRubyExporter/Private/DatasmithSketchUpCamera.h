// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithSketchUpCommon.h"

// SketchUp SDK.
#include "DatasmithSketchUpSDKBegins.h"
#include "SketchUpAPI/geometry.h"
#include "SketchUpAPI/model/defs.h"
#include "DatasmithSketchUpSDKCeases.h"

#include "Containers/UnrealString.h"
#include "Misc/SecureHash.h"

class IDatasmithCameraActorElement;

namespace DatasmithSketchUp
{
	class FExportContext;

	// Associates SketchUp Camera with Datasmith actor
	class FCamera : FNoncopyable
	{
	public:

		FCamera(SUCameraRef InCameraRef)
			: CameraRef(InCameraRef) {}

		static TSharedPtr<FCamera> Create(FExportContext& Context, SUCameraRef InCameraRef, const FString& Name);
		static TSharedPtr<FCamera> Create(FExportContext& Context, SUSceneRef InSceneRef);

		void Update(FExportContext& Context);
		FMD5Hash GetHash();

		SUCameraRef CameraRef;
		FString Name;
		TSharedPtr<IDatasmithCameraActorElement> DatasmithCamera;
		bool bIsActive;
	};

}
