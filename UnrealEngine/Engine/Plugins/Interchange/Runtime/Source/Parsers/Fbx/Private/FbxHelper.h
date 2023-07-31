// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "FbxInclude.h"

namespace UE
{
	namespace Interchange
	{
		namespace Private
		{
			class FFbxParser;

			class FPayloadContextBase
			{
			public:
				virtual ~FPayloadContextBase() {}
				virtual FString GetPayloadType() const { return FString(); }
				virtual bool FetchPayloadToFile(FFbxParser& Parser, const FString& PayloadFilepath) { return false; }
				virtual bool FetchAnimationBakeTransformPayloadToFile(FFbxParser& Parser, const double BakeFrequency, const double RangeStartTime, const double RangeEndTime, const FString& PayloadFilepath) { return false; }
			};

			struct FFbxHelper
			{
			public:

				static FString GetMeshName(FbxGeometryBase* Mesh);
				static FString GetMeshUniqueID(FbxGeometryBase* Mesh);

				static FString GetNodeAttributeName(FbxNodeAttribute* NodeAttribute, const FStringView DefaultNamePrefix);
				static FString GetNodeAttributeUniqueID(FbxNodeAttribute* NodeAttribute, const FStringView Prefix);

				/**
				 * Return the name of an FbxProperty, return empty string if the property is null.
				 */
				static FString GetFbxPropertyName(const FbxProperty& Property);

				/**
				 * Return the name of an FbxObject, return empty string if the object is null.
				 */
				static FString GetFbxObjectName(const FbxObject* Object);

				/**
				 * Return a string with the name of all the parent in the hierarchy separate by a dot( . ) from the fbx root node to the specified node.
				 * This is a way to have a valid unique ID for a fbx node that will be the same if the fbx change when we re-import.
				 * Using the fbx sdk int32 uniqueID is not valid anymore if the fbx is re-exported.
				 */
				static FString GetFbxNodeHierarchyName(const FbxNode* Node);

			protected:
				static FString GetUniqueIDString(const uint64 UniqueID);
			};
		}//ns Private
	}//ns Interchange
}//ns UE
