///////////////////////////////////////////////////////////////////////
//
//  *** INTERACTIVE DATA VISUALIZATION (IDV) CONFIDENTIAL AND PROPRIETARY INFORMATION ***
//
//  This software is supplied under the terms of a license agreement or
//  nondisclosure agreement with Interactive Data Visualization, Inc. and
//  may not be copied, disclosed, or exploited except in accordance with
//  the terms of that agreement.
//
//      Copyright (c) 2003-2017 IDV, Inc.
//      All rights reserved in all media.
//
//      IDV, Inc.
//      http://www.idvinc.com

///////////////////////////////////////////////////////////////////////
//	Preprocessor / Includes

#pragma once
#include "DataBuffer.h"


///////////////////////////////////////////////////////////////////////
//  Packing

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(push, 4)
#endif


///////////////////////////////////////////////////////////////////////
//	namespace SpeedTreeDataBuffer

namespace SpeedTreeDataBuffer
{
	namespace GameEngine9
	{

		///////////////////////////////////////////////////////////////////////
		//	Data Structs

		struct Vec2
		{
			st_float32 x, y;
		};

		struct Vec3
		{
			st_float32 x, y, z;
		};

		struct Vec4
		{
			st_float32 x, y, z, w;
		};

		struct SVertex
		{
			Vec3				m_vAnchor;
			Vec3				m_vOffset;
			Vec3				m_vLodOffset;

			Vec3				m_vNormal;
			Vec3				m_vTangent;
			Vec3				m_vBinormal;

			Vec2				m_vTexCoord;
			Vec2				m_vLightmapTexCoord;
			Vec3				m_vColor;
			st_float32			m_fAmbientOcclusion;
			st_float32			m_fBlendWeight;

			Vec3				m_vBranchWind1; // pos, dir, weight
			Vec3				m_vBranchWind2;
			st_float32			m_fRippleWeight;
			st_bool				m_bCameraFacing;

			st_uint32			m_uiBoneID;
		};

		struct SDrawCall
		{
			st_uint32			m_uiMaterialIndex;
			st_bool				m_bContainsFacingGeometry;

			st_uint32			m_uiIndexStart;
			st_uint32			m_uiIndexCount;
		};

		struct SBone
		{
			st_uint32			m_uiID;
			st_uint32			m_uiParentID;
			Vec3				m_vStart;
			Vec3				m_vEnd;
			float				m_fRadius;
		};


		///////////////////////////////////////////////////////////////////////
		//	Data Tables

		class CLod : public CTable
		{
		public:
			ST_INLINE CArray<SVertex>	Vertices(void) { return GetContainer<CArray<SVertex> >(0); }
			ST_INLINE CArray<st_uint32>	Indices(void) { return GetContainer<CArray<st_uint32> >(1); }
			ST_INLINE CArray<SDrawCall>	DrawCalls(void) { return GetContainer<CArray<SDrawCall> >(2); }
		};

		class CMaterialMap : public CTable
		{
		public:
			ST_INLINE st_bool						Used(void) { return GetValue<st_bool>(0); }
			ST_INLINE CString	Path(void) { return GetContainer<CString>(1); }
			ST_INLINE Vec4							Color(void) { return GetValue<Vec4>(2); }
		};

		class CMaterial : public CTable
		{
		public:
			ST_INLINE CString					Name(void) { return GetContainer<CString>(0); }
			ST_INLINE st_bool					TwoSided(void) { return GetValue<st_bool>(1); }
			ST_INLINE st_bool					FlipNormalsOnBackside(void) { return GetValue<st_bool>(2); }
			ST_INLINE st_bool					Billboard(void) { return GetValue<st_bool>(3); }
			ST_INLINE CTableArray<CMaterialMap>	Maps(void) { return GetContainer<CTableArray<CMaterialMap> >(4); }
		};

		class CBillboardInfo : public CTable
		{
		public:
			ST_INLINE st_bool	LastLodIsBillboard(void)	{ return GetValue<st_bool>(0); }
			ST_INLINE st_bool	IncludesTopDown(void)		{ return GetValue<st_bool>(1); }
			ST_INLINE st_uint32	SideViewCount(void)			{ return GetValue<st_uint32>(2); }
		};

		class CCollisionObject : public CTable
		{
		public:
			ST_INLINE Vec3		Position(void) { return GetValue<Vec3>(0); }
			ST_INLINE Vec3		Position2(void) { return GetValue<Vec3>(1); }
			ST_INLINE float		Radius(void) { return GetValue<float>(2); }
			ST_INLINE CString	UserData(void) { return GetContainer<CString>(3); }
		};


		///////////////////////////////////////////////////////////////////////
		//	Wind Data Tables

		class CWindConfigCommon : public CTable
		{
		public:
			ST_INLINE st_float32	StrengthResponse(void) { return GetValue<st_float32>(0); }
			ST_INLINE st_float32	DirectionResponse(void) { return GetValue<st_float32>(1); }

			ST_INLINE st_float32	GustFrequency(void) { return GetValue<st_float32>(5); }
			ST_INLINE st_float32	GustStrengthMin(void) { return GetValue<st_float32>(6); }
			ST_INLINE st_float32	GustStrengthMax(void) { return GetValue<st_float32>(7); }
			ST_INLINE st_float32	GustDurationMin(void) { return GetValue<st_float32>(8); }
			ST_INLINE st_float32	GustDurationMax(void) { return GetValue<st_float32>(9); }
			ST_INLINE st_float32	GustRiseScalar(void) { return GetValue<st_float32>(10); }
			ST_INLINE st_float32	GustFallScalar(void) { return GetValue<st_float32>(11); }

			ST_INLINE st_float32	CurrentStrength(void) { return GetValue<st_float32>(15); }
		};

		class CWindConfigSDK : public CTable
		{
		public:
			class CBranch : public CTable
			{
			public:
				ST_INLINE CArray<st_float32>	Bend(void) { return GetContainer<CArray<st_float32> >(0); }
				ST_INLINE CArray<st_float32>	Oscillation(void) { return GetContainer<CArray<st_float32> >(1); }
				ST_INLINE CArray<st_float32>	Speed(void) { return GetContainer<CArray<st_float32> >(2); }
				ST_INLINE CArray<st_float32>	Turbulence(void) { return GetContainer<CArray<st_float32> >(3); }
				ST_INLINE CArray<st_float32>	Flexibility(void) { return GetContainer<CArray<st_float32> >(4); }
				ST_INLINE st_float32			Independence(void) { return GetValue<st_float32>(5); }
			};

			class CRipple : public CTable
			{
			public:
				ST_INLINE CArray<st_float32>	Planar(void) { return GetContainer<CArray<st_float32> >(0); }
				ST_INLINE CArray<st_float32>	Directional(void) { return GetContainer<CArray<st_float32> >(1); }
				ST_INLINE CArray<st_float32>	Speed(void) { return GetContainer<CArray<st_float32> >(2); }
				ST_INLINE CArray<st_float32>	Flexibility(void) { return GetContainer<CArray<st_float32> >(3); }
				ST_INLINE st_float32			Shimmer(void) { return GetValue<st_float32>(4); }
				ST_INLINE st_float32			Independence(void) { return GetValue<st_float32>(5); }
			};

			ST_INLINE CWindConfigCommon		Common(void) { return GetContainer<CWindConfigCommon>(0); }
			ST_INLINE CBranch				Shared(void) { return GetContainer<CBranch>(1); }
			ST_INLINE CBranch				Branch1(void) { return GetContainer<CBranch>(2); }
			ST_INLINE CBranch				Branch2(void) { return GetContainer<CBranch>(3); }
			ST_INLINE CRipple				Ripple(void) { return GetContainer<CRipple>(4); }

			ST_INLINE st_float32			SharedStartHeight(void) { return GetValue<st_float32>(10); }
			ST_INLINE st_float32			Branch1StretchLimit(void) { return GetValue<st_float32>(11); }
			ST_INLINE st_float32			Branch2StretchLimit(void) { return GetValue<st_float32>(12); }

			ST_INLINE st_bool				DoShared(void) { return GetValue<st_bool>(15); }
			ST_INLINE st_bool				DoBranch1(void) { return GetValue<st_bool>(16); }
			ST_INLINE st_bool				DoBranch2(void) { return GetValue<st_bool>(17); }
			ST_INLINE st_bool				DoRipple(void) { return GetValue<st_bool>(18); }
			ST_INLINE st_bool				DoShimmer(void) { return GetValue<st_bool>(19); }
		};


		///////////////////////////////////////////////////////////////////////
		//	class CTree

		class CTree : public CReader
		{
		public:

			// file info
			ST_INLINE st_uint32						VersionMajor(void) { return GetValue<st_uint32>(0); }
			ST_INLINE st_uint32						VersionMinor(void) { return GetValue<st_uint32>(1); }

			// geometry info
			ST_INLINE CTableArray<CLod>				Lods(void) { return GetContainer<CTableArray<CLod> >(5); }
			ST_INLINE CBillboardInfo				BillboardInfo(void) { return GetContainer<CBillboardInfo>(6); }
			ST_INLINE CTableArray<CCollisionObject>	CollisionObjects(void) { return GetContainer<CTableArray<CCollisionObject> >(7); }

			// material info
			ST_INLINE CTableArray<CMaterial>		Materials(void) { return GetContainer<CTableArray<CMaterial> >(10); }
			ST_INLINE st_uint32						LightmapSize(void) { return GetValue<st_uint32>(11); }
			ST_INLINE CString						TexturePacker(void) { return GetContainer<CString>(12); }

			// wind
			ST_INLINE CWindConfigSDK				Wind(void) { return GetContainer<CWindConfigSDK>(15); }

			// bones/skeleton
			ST_INLINE CArray<SBone>					Bones(void) { return GetContainer<CArray<SBone> >(20); }

		protected:
			ST_INLINE	const st_char* FileToken(void) const { return "SpeedTree9______"; }

		};

	} // end namespace GameEngine9

} // end namespace SpeedTree

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif
