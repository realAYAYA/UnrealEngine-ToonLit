// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

// "Fast, minimum storage ray-triangle intersection" Moller-Trumbore, 1997
bool RayTriangleIntersectionMollerTrumbore(float3 RayOrigin, float3 RayDirection, float3 Vertex0, float3 Vertex1, float3 Vertex2, out float IntersectionT, out float2 Barycentrics)
{
	const float Epsilon = 1e-9;

	const float3 E1 = Vertex1 - Vertex0;
	const float3 E2 = Vertex2 - Vertex0;
	const float3 P = cross(RayDirection, E2);
	const float Det = dot(E1, P);
#if BACKFACE_CULLING 
	if (Det < Epsilon) return false;
#else 	
	if (Det > -Epsilon && Det < Epsilon) return false;
#endif 
	const float InvDet = rcp(Det);

	const float3 T = RayOrigin - Vertex0;
	float U = dot(T, P) * InvDet;
	if (U < 0 || U > 1) return false;

	float3 Q = cross(T, E1);
	float V = dot(RayDirection, Q) * InvDet;
	if (V < 0 || U + V > 1) return false;

	const float t = dot(E2, Q) * InvDet;
	if (t <= 0) return false;

	// In the original paper UV is from Vertex0 but we want UV to be from Vertex2 to match DXR.
	Barycentrics = float2(1 - U - V, U);
	IntersectionT = t;

	return true;
}

// "Watertight Ray/Triangle Intersection" Woop, Benthin and Wald 2013
uint MaxDimension(float3 V)
{
	return V.y > V.x ? (V.z > V.y ? 2 : 1) : (V.z > V.x ? 2 : 0);
}

bool RayTriangleIntersectionWatertight(float3 RayOrigin, float3 RayDirection, float3 Vertex0, float3 Vertex1, float3 Vertex2, out float IntersectionT, out float2 Barycentrics)
{
	int kz = MaxDimension(abs(RayDirection));
	int kx = kz + 1; if (kx == 3) kx = 0;
	int ky = kx + 1; if (ky == 3) ky = 0;

	if (RayDirection[kz] < 0.0f)
	{
		int temp = kx;
		kx = ky;
		ky = temp;
	}

	const float3 ShearConstant = float3(RayDirection[kx], RayDirection[ky], 1.0f) * rcp(RayDirection[kz]);

	const float3 A = Vertex0 - RayOrigin;
	const float3 B = Vertex1 - RayOrigin;
	const float3 C = Vertex2 - RayOrigin;

	const float Ax = A[kx] - ShearConstant.x * A[kz];
	const float Ay = A[ky] - ShearConstant.y * A[kz];
	const float Bx = B[kx] - ShearConstant.x * B[kz];
	const float By = B[ky] - ShearConstant.y * B[kz];
	const float Cx = C[kx] - ShearConstant.x * C[kz];
	const float Cy = C[ky] - ShearConstant.y * C[kz];

	float3 UVW;
	UVW.x = Cx * By - Cy * Bx;
	UVW.y = Ax * Cy - Ay * Cx;
	UVW.z = Bx * Ay - By * Ax;

	if (any(UVW == 0.0f))
	{
		// double precision
	}

#if BACKFACE_CULLING	
	if (any(UVW < 0.0f)) return false;
#else	
	if (any(UVW < 0.0f) && any(UVW > 0.0f)) return false;
#endif

	const float Det = dot(UVW, 1.0f);
	if (Det == 0.0f) return false;

	const float3 ScaledZ = ShearConstant.z * float3(A[kz], B[kz], C[kz]);
	const float T = dot(UVW, ScaledZ);

	if (Det < 0 && T >= 0) return false;
	if (Det > 0 && T <= 0) return false;

	const float InvDet = rcp(Det);
	Barycentrics = UVW.xy * InvDet;
	IntersectionT = T * InvDet;

	return true;
}