// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonNode.h"
#include "Json/GLTFJsonCamera.h"
#include "Json/GLTFJsonSkin.h"
#include "Json/GLTFJsonMesh.h"
#include "Json/GLTFJsonLight.h"

void FGLTFJsonNode::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	if (!Translation.IsNearlyEqual(FGLTFJsonVector3::Zero, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("translation"), Translation);
	}

	if (!Rotation.IsNearlyEqual(FGLTFJsonQuaternion::Identity, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("rotation"), Rotation);
	}

	if (!Scale.IsNearlyEqual(FGLTFJsonVector3::One, Writer.DefaultTolerance))
	{
		Writer.Write(TEXT("scale"), Scale);
	}

	if (Camera != nullptr)
	{
		Writer.Write(TEXT("camera"), Camera);
	}

	if (Skin != nullptr)
	{
		Writer.Write(TEXT("skin"), Skin);
	}

	if (Mesh != nullptr)
	{
		Writer.Write(TEXT("mesh"), Mesh);
	}

	if (Light != nullptr)
	{
		Writer.StartExtensions();

		Writer.StartExtension(EGLTFJsonExtension::KHR_LightsPunctual);
		Writer.Write(TEXT("light"), Light);
		Writer.EndExtension();

		Writer.EndExtensions();
	}

	if (Children.Num() > 0)
	{
		Writer.Write(TEXT("children"), Children);
	}
}
