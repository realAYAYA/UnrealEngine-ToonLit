// Copyright Epic Games, Inc. All Rights Reserved.

#include "MeshRepresentationCommon.h"
#include "MaterialShared.h"
#include "MeshUtilities.h"
#include "MeshUtilitiesPrivate.h"
#include "DerivedMeshDataTaskUtils.h"

static FVector3f UniformSampleHemisphere(FVector2D Uniforms)
{
	Uniforms = Uniforms * 2.0f - 1.0f;

	if (Uniforms == FVector2D::ZeroVector)
	{
		return FVector3f::ZeroVector;
	}

	float R;
	float Theta;

	if (FMath::Abs(Uniforms.X) > FMath::Abs(Uniforms.Y))
	{
		R = Uniforms.X;
		Theta = (float)PI / 4 * (Uniforms.Y / Uniforms.X);
	}
	else
	{
		R = Uniforms.Y;
		Theta = (float)PI / 2 - (float)PI / 4 * (Uniforms.X / Uniforms.Y);
	}

	// concentric disk sample
	const float U = R * FMath::Cos(Theta);
	const float V = R * FMath::Sin(Theta);
	const float R2 = R * R;

	// map to hemisphere [P. Shirley, Kenneth Chiu; 1997; A Low Distortion Map Between Disk and Square]
	return FVector3f(U * FMath::Sqrt(2 - R2), V * FMath::Sqrt(2 - R2), 1.0f - R2);
}

void MeshUtilities::GenerateStratifiedUniformHemisphereSamples(int32 NumSamples, FRandomStream& RandomStream, TArray<FVector3f>& Samples)
{
	const int32 NumSamplesDim = FMath::TruncToInt(FMath::Sqrt((float)NumSamples));

	Samples.Empty(NumSamplesDim * NumSamplesDim);

	for (int32 IndexX = 0; IndexX < NumSamplesDim; IndexX++)
	{
		for (int32 IndexY = 0; IndexY < NumSamplesDim; IndexY++)
		{
			const float U1 = RandomStream.GetFraction();
			const float U2 = RandomStream.GetFraction();

			const float Fraction1 = (IndexX + U1) / (float)NumSamplesDim;
			const float Fraction2 = (IndexY + U2) / (float)NumSamplesDim;

			FVector3f Tmp = UniformSampleHemisphere(FVector2D(Fraction1, Fraction2));

			// Workaround issue with compiler optimization by using copy constructor here.
			Samples.Add(FVector3f(Tmp));
		}
	}
}

// [Frisvad 2012, "Building an Orthonormal Basis from a 3D Unit Vector Without Normalization"]
FMatrix44f MeshRepresentation::GetTangentBasisFrisvad(FVector3f TangentZ)
{
	FVector3f TangentX;
	FVector3f TangentY;

	if (TangentZ.Z < -0.9999999f)
	{
		TangentX = FVector3f(0, -1, 0);
		TangentY = FVector3f(-1, 0, 0);
	}
	else
	{
		float A = 1.0f / (1.0f + TangentZ.Z);
		float B = -TangentZ.X * TangentZ.Y * A;
		TangentX = FVector3f(1.0f - TangentZ.X * TangentZ.X * A, B, -TangentZ.X);
		TangentY = FVector3f(B, 1.0f - TangentZ.Y * TangentZ.Y * A, -TangentZ.Y);
	}

	FMatrix44f LocalBasis;
	LocalBasis.SetIdentity();
	LocalBasis.SetAxis(0, TangentX);
	LocalBasis.SetAxis(1, TangentY);
	LocalBasis.SetAxis(2, TangentZ);
	return LocalBasis;
}

#if USE_EMBREE
void EmbreeFilterFunc(const struct RTCFilterFunctionNArguments* args)
{
	FEmbreeGeometry* EmbreeGeometry = (FEmbreeGeometry*)args->geometryUserPtr;
	FEmbreeTriangleDesc Desc = EmbreeGeometry->TriangleDescs[RTCHitN_primID(args->hit, 1, 0)];

	FEmbreeIntersectionContext& IntersectionContext = *static_cast<FEmbreeIntersectionContext*>(args->context);
	IntersectionContext.ElementIndex = Desc.ElementIndex;

	const RTCHit& EmbreeHit = *(RTCHit*)args->hit;
	if (IntersectionContext.SkipPrimId != RTC_INVALID_GEOMETRY_ID && IntersectionContext.SkipPrimId == EmbreeHit.primID)
	{
		// Ignore hit in order to continue tracing
		args->valid[0] = 0;
	}
}

void EmbreeErrorFunc(void* userPtr, RTCError code, const char* str)
{
	FString ErrorString;
	TArray<TCHAR, FString::AllocatorType>& ErrorStringArray = ErrorString.GetCharArray();
	ErrorStringArray.Empty();

	int32 StrLen = FCStringAnsi::Strlen(str);
	int32 Length = FUTF8ToTCHAR_Convert::ConvertedLength(str, StrLen);
	ErrorStringArray.AddUninitialized(Length + 1); // +1 for the null terminator
	FUTF8ToTCHAR_Convert::Convert(ErrorStringArray.GetData(), ErrorStringArray.Num(), reinterpret_cast<const ANSICHAR*>(str), StrLen);
	ErrorStringArray[Length] = TEXT('\0');

	UE_LOG(LogMeshUtilities, Error, TEXT("Embree error: %s Code=%u"), *ErrorString, (uint32)code);
}
#endif

void MeshRepresentation::SetupEmbreeScene(
	FString MeshName,
	const FSourceMeshDataForDerivedDataTask& SourceMeshData,
	const FStaticMeshLODResources& LODModel,
	const TArray<FSignedDistanceFieldBuildSectionData>& SectionData,
	bool bGenerateAsIfTwoSided,
	bool bIncludeTranslucentTriangles,
	FEmbreeScene& EmbreeScene)
{
	const uint32 NumVertices = SourceMeshData.IsValid() ? SourceMeshData.GetNumVertices() : LODModel.VertexBuffers.PositionVertexBuffer.GetNumVertices();
	const uint32 NumIndices = SourceMeshData.IsValid() ? SourceMeshData.GetNumIndices() : LODModel.IndexBuffer.GetNumIndices();
	const int32 NumTriangles = NumIndices / 3;
	EmbreeScene.NumIndices = NumTriangles;

	const FStaticMeshSectionArray& Sections = SourceMeshData.IsValid() ? SourceMeshData.Sections : LODModel.Sections;

	TArray<FkDOPBuildCollisionTriangle<uint32> > BuildTriangles;

#if USE_EMBREE
	EmbreeScene.bUseEmbree = true;

	if (EmbreeScene.bUseEmbree)
	{
		EmbreeScene.EmbreeDevice = rtcNewDevice(nullptr);
		rtcSetDeviceErrorFunction(EmbreeScene.EmbreeDevice, EmbreeErrorFunc, nullptr);

		RTCError ReturnErrorNewDevice = rtcGetDeviceError(EmbreeScene.EmbreeDevice);
		if (ReturnErrorNewDevice != RTC_ERROR_NONE)
		{
			UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcNewDevice failed. Code: %d"), *MeshName, (int32)ReturnErrorNewDevice);
			return;
		}

		EmbreeScene.EmbreeScene = rtcNewScene(EmbreeScene.EmbreeDevice);
		rtcSetSceneFlags(EmbreeScene.EmbreeScene, RTC_SCENE_FLAG_NONE);

		RTCError ReturnErrorNewScene = rtcGetDeviceError(EmbreeScene.EmbreeDevice);
		if (ReturnErrorNewScene != RTC_ERROR_NONE)
		{
			UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcNewScene failed. Code: %d"), *MeshName, (int32)ReturnErrorNewScene);
			rtcReleaseDevice(EmbreeScene.EmbreeDevice);
			return;
		}
	}
#endif

	/*
	if (LODModel.Sections.Num() > SectionData.Num())
	{
		UE_LOG(LogMeshUtilities, Warning, TEXT("Unexpected number of mesh sections when setting up Embree Scene for %s."), *MeshName);
	}
	*/

	TArray<int32> FilteredTriangles;
	FilteredTriangles.Empty(NumTriangles);

	for (int32 TriangleIndex = 0; TriangleIndex < NumTriangles; ++TriangleIndex)
	{
		FVector3f V0, V1, V2;

		if (SourceMeshData.IsValid())
		{
			const uint32 I0 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 0];
			const uint32 I1 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 1];
			const uint32 I2 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 2];

			V0 = SourceMeshData.VertexPositions[I0];
			V1 = SourceMeshData.VertexPositions[I1];
			V2 = SourceMeshData.VertexPositions[I2];
		}
		else
		{
			const FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
			const uint32 I0 = Indices[TriangleIndex * 3 + 0];
			const uint32 I1 = Indices[TriangleIndex * 3 + 1];
			const uint32 I2 = Indices[TriangleIndex * 3 + 2];

			V0 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I0);
			V1 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I1);
			V2 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I2);
		}

		const FVector3f TriangleNormal = ((V1 - V2) ^ (V0 - V2));
		const bool bDegenerateTriangle = TriangleNormal.SizeSquared() < SMALL_NUMBER;
		if (!bDegenerateTriangle)
		{
			bool bIncludeTriangle = false;

			for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); SectionIndex++)
			{
				const FStaticMeshSection& Section = Sections[SectionIndex];

				if ((uint32)(TriangleIndex * 3) >= Section.FirstIndex && (uint32)(TriangleIndex * 3) < Section.FirstIndex + Section.NumTriangles * 3)
				{
					if (SectionData.IsValidIndex(SectionIndex))
					{
						const bool bIsOpaqueOrMasked = !IsTranslucentBlendMode(SectionData[SectionIndex].BlendMode);
						bIncludeTriangle = (bIsOpaqueOrMasked || bIncludeTranslucentTriangles) && SectionData[SectionIndex].bAffectDistanceFieldLighting;
					}

					break;
				}
			}

			if (bIncludeTriangle)
			{
				FilteredTriangles.Add(TriangleIndex);
			}
		}
	}

	const int32 NumBufferVerts = 1; // Reserve extra space at the end of the array, as embree has an internal bug where they read and discard 4 bytes off the end of the array
	EmbreeScene.Geometry.VertexArray.Empty(NumVertices + NumBufferVerts);
	EmbreeScene.Geometry.VertexArray.AddUninitialized(NumVertices + NumBufferVerts);

	const int32 NumFilteredIndices = FilteredTriangles.Num() * 3;

	EmbreeScene.Geometry.IndexArray.Empty(NumFilteredIndices);
	EmbreeScene.Geometry.IndexArray.AddUninitialized(NumFilteredIndices);

	FVector3f* EmbreeVertices = EmbreeScene.Geometry.VertexArray.GetData();
	uint32* EmbreeIndices = EmbreeScene.Geometry.IndexArray.GetData();
	EmbreeScene.Geometry.TriangleDescs.Empty(FilteredTriangles.Num());

	for (int32 FilteredTriangleIndex = 0; FilteredTriangleIndex < FilteredTriangles.Num(); FilteredTriangleIndex++)
	{
		uint32 I0, I1, I2;
		FVector3f V0, V1, V2;

		const int32 TriangleIndex = FilteredTriangles[FilteredTriangleIndex];
		if (SourceMeshData.IsValid())
		{
			I0 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 0];
			I1 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 1];
			I2 = SourceMeshData.TriangleIndices[TriangleIndex * 3 + 2];

			V0 = SourceMeshData.VertexPositions[I0];
			V1 = SourceMeshData.VertexPositions[I1];
			V2 = SourceMeshData.VertexPositions[I2];
		}
		else
		{
			const FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
			I0 = Indices[TriangleIndex * 3 + 0];
			I1 = Indices[TriangleIndex * 3 + 1];
			I2 = Indices[TriangleIndex * 3 + 2];

			V0 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I0);
			V1 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I1);
			V2 = LODModel.VertexBuffers.PositionVertexBuffer.VertexPosition(I2);
		}

		bool bTriangleIsTwoSided = false;

		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); SectionIndex++)
		{
			const FStaticMeshSection& Section = Sections[SectionIndex];

			if ((uint32)(TriangleIndex * 3) >= Section.FirstIndex && (uint32)(TriangleIndex * 3) < Section.FirstIndex + Section.NumTriangles * 3)
			{
				if (SectionData.IsValidIndex(SectionIndex))
				{
					bTriangleIsTwoSided = SectionData[SectionIndex].bTwoSided;
				}

				break;
			}
		}

		if (EmbreeScene.bUseEmbree)
		{
			EmbreeIndices[FilteredTriangleIndex * 3 + 0] = I0;
			EmbreeIndices[FilteredTriangleIndex * 3 + 1] = I1;
			EmbreeIndices[FilteredTriangleIndex * 3 + 2] = I2;

			EmbreeVertices[I0] = V0;
			EmbreeVertices[I1] = V1;
			EmbreeVertices[I2] = V2;

			FEmbreeTriangleDesc Desc;
			// Store bGenerateAsIfTwoSided in material index
			Desc.ElementIndex = bGenerateAsIfTwoSided || bTriangleIsTwoSided ? 1 : 0;
			EmbreeScene.Geometry.TriangleDescs.Add(Desc);
		}
		else
		{
			BuildTriangles.Add(FkDOPBuildCollisionTriangle<uint32>(
				// Store bGenerateAsIfTwoSided in material index
				bGenerateAsIfTwoSided || bTriangleIsTwoSided ? 1 : 0,
				FVector(V0),
				FVector(V1),
				FVector(V2)));
		}
	}

#if USE_EMBREE
	if (EmbreeScene.bUseEmbree)
	{
		RTCGeometry Geometry = rtcNewGeometry(EmbreeScene.EmbreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);
		EmbreeScene.Geometry.InternalGeometry = Geometry;

		rtcSetSharedGeometryBuffer(Geometry, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, EmbreeVertices, 0, sizeof(FVector3f), NumVertices);
		rtcSetSharedGeometryBuffer(Geometry, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, EmbreeIndices, 0, sizeof(uint32) * 3, FilteredTriangles.Num());

		rtcSetGeometryUserData(Geometry, &EmbreeScene.Geometry);
		rtcSetGeometryIntersectFilterFunction(Geometry, EmbreeFilterFunc);

		rtcCommitGeometry(Geometry);
		rtcAttachGeometry(EmbreeScene.EmbreeScene, Geometry);
		rtcReleaseGeometry(Geometry);

		rtcCommitScene(EmbreeScene.EmbreeScene);

		RTCError ReturnError = rtcGetDeviceError(EmbreeScene.EmbreeDevice);
		if (ReturnError != RTC_ERROR_NONE)
		{
			UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcCommitScene failed. Code: %d"), *MeshName, (int32)ReturnError);
			return;
		}
	}
	else
#endif
	{
		EmbreeScene.kDopTree.Build(BuildTriangles);
	}

	// bMostlyTwoSided
	{
		uint32 NumTrianglesTotal = 0;
		uint32 NumTwoSidedTriangles = 0;

		for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); SectionIndex++)
		{
			const FStaticMeshSection& Section = Sections[SectionIndex];

			if (SectionData.IsValidIndex(SectionIndex))
			{
				NumTrianglesTotal += Section.NumTriangles;

				if (SectionData[SectionIndex].bTwoSided)
				{
					NumTwoSidedTriangles += Section.NumTriangles;
				}
			}
		}

		EmbreeScene.bMostlyTwoSided = NumTwoSidedTriangles * 4 >= NumTrianglesTotal || bGenerateAsIfTwoSided;
	}
}

void MeshRepresentation::DeleteEmbreeScene(FEmbreeScene& EmbreeScene)
{
#if USE_EMBREE
	if (EmbreeScene.bUseEmbree)
	{
		rtcReleaseScene(EmbreeScene.EmbreeScene);
		rtcReleaseDevice(EmbreeScene.EmbreeDevice);
	}
#endif
}