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
	namespace GameEngine8
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

		enum EWindGeometryType
		{
			Branch, Frond, Leaf, FacingLeaf, Billboard
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

			Vec2				m_vWindBranch;
			Vec3				m_vWindNonBranch;
			st_float32			m_fWindRandom;

			st_float32			m_fAmbientOcclusion;
			st_float32			m_fBlendWeight;
			st_uint32			m_uiBoneID;
			st_byte				m_bWindLeaf2Flag;
		};

		struct SDrawCall
		{
			st_uint32			m_uiMaterialIndex;
			EWindGeometryType	m_eWindGeometryType;

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

		struct SWindConfig
		{
			static const int c_nNumBranchLevels = 2;
			static const int c_nNumLeafGroups = 2;
			static const int c_nNumPointsInCurves = 10;

			// main settings
			enum EWindPreset
			{
				WIND_PRESET_NONE,
				WIND_PRESET_FASTEST,
				WIND_PRESET_FAST,
				WIND_PRESET_BETTER,
				WIND_PRESET_BEST,
				WIND_PRESET_PALM
			};

			EWindPreset			m_ePreset;
			float				m_fStrengthResponse;
			float				m_fDirectionResponse;
			float				m_fAnchorOffset;
			float				m_fAnchorDistanceScale;

			// gusting
			float				m_fGustFrequency;
			float				m_fGustStrengthMin;
			float				m_fGustStrengthMax;
			float				m_fGustDurationMin;
			float				m_fGustDurationMax;
			float				m_fGustRiseScalar;
			float				m_fGustFallScalar;

			// oscillations
			enum EOscillationComponents
			{
				OSC_GLOBAL,
				OSC_BRANCH_1,
				OSC_BRANCH_2,
				OSC_LEAF_1_RIPPLE,
				OSC_LEAF_1_TUMBLE,
				OSC_LEAF_1_TWITCH,
				OSC_LEAF_2_RIPPLE,
				OSC_LEAF_2_TUMBLE,
				OSC_LEAF_2_TWITCH,
				OSC_FROND_RIPPLE,
				NUM_OSC_COMPONENTS
			};
			float				m_afFrequencies[NUM_OSC_COMPONENTS][c_nNumPointsInCurves];

			// global motion
			float				m_fGlobalHeight;
			float				m_fGlobalHeightExponent;
			float				m_afGlobalDistance[c_nNumPointsInCurves];
			float				m_afGlobalDirectionAdherence[c_nNumPointsInCurves];

			// branch motion
			struct SBranchWindLevel
			{
				float			m_afDistance[c_nNumPointsInCurves];
				float			m_afDirectionAdherence[c_nNumPointsInCurves];
				float			m_afWhip[c_nNumPointsInCurves];
				float			m_fTurbulence;
				float			m_fTwitch;
				float			m_fTwitchFreqScale;
			};
			SBranchWindLevel	m_asBranch[c_nNumBranchLevels];
			float				m_vFrondStyleBranchAnchor[3];
			float				m_fMaxBranchLength;

			// leaf motion
			struct SLeafGroup
			{
				float			m_afRippleDistance[c_nNumPointsInCurves];
				float			m_afTumbleFlip[c_nNumPointsInCurves];
				float			m_afTumbleTwist[c_nNumPointsInCurves];
				float			m_afTumbleDirectionAdherence[c_nNumPointsInCurves];
				float			m_afTwitchThrow[c_nNumPointsInCurves];
				float			m_fTwitchSharpness;
				float			m_fLeewardScalar;
			};
			SLeafGroup			m_asLeaf[c_nNumLeafGroups];

			// frond motion
			float				m_afFrondRippleDistance[c_nNumPointsInCurves];
			float				m_fFrondRippleTile;
			float				m_fFrondRippleLightingScalar;

			// options
			enum EOptions
			{
				GLOBAL_WIND,
				GLOBAL_PRESERVE_SHAPE,

				BRANCH_1_SIMPLE,
				BRANCH_1_DIRECTIONAL,
				BRANCH_1_DIRECTIONAL_FROND,
				BRANCH_1_TURBULENCE,
				BRANCH_1_WHIP,
				BRANCH_1_OSC_COMPLEX,

				BRANCH_2_SIMPLE,
				BRANCH_2_DIRECTIONAL,
				BRANCH_2_DIRECTIONAL_FROND,
				BRANCH_2_TURBULENCE,
				BRANCH_2_WHIP,
				BRANCH_2_OSC_COMPLEX,

				LEAF_RIPPLE_VERTEX_NORMAL_1,
				LEAF_RIPPLE_COMPUTED_1,
				LEAF_TUMBLE_1,
				LEAF_TWITCH_1,
				LEAF_OCCLUSION_1,

				LEAF_RIPPLE_VERTEX_NORMAL_2,
				LEAF_RIPPLE_COMPUTED_2,
				LEAF_TUMBLE_2,
				LEAF_TWITCH_2,
				LEAF_OCCLUSION_2,

				FROND_RIPPLE_ONE_SIDED,
				FROND_RIPPLE_TWO_SIDED,
				FROND_RIPPLE_ADJUST_LIGHTING,

				ROLLING,

				NUM_WIND_OPTIONS
			};

			st_byte				m_abOptions[NUM_WIND_OPTIONS];
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
			ST_INLINE st_bool	Used(void) { return GetValue<st_bool>(0); }
			ST_INLINE CString	Path(void) { return GetContainer<CString>(1); }
			ST_INLINE Vec4		Color(void) { return GetValue<Vec4>(2); }
		};

		class CMaterial : public CTable
		{
		public:
			ST_INLINE CString					Name(void) { return GetContainer<CString>(0); }
			ST_INLINE st_bool					TwoSided(void) { return GetValue<st_bool>(1); }
			ST_INLINE st_bool					Billboard(void) { return GetValue<st_bool>(2); }
			ST_INLINE CTableArray<CMaterialMap>	Maps(void) { return GetContainer<CTableArray<CMaterialMap> >(3); }
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
		//	class CTree

		class CTree : public CReader
		{
		public:

			// file info
			ST_INLINE st_uint32						VersionMajor(void) { return GetValue<st_uint32>(0); }
			ST_INLINE st_uint32						VersionMinor(void) { return GetValue<st_uint32>(1); }

			// geometry info
			ST_INLINE st_bool						LastLodIsBillboard(void) { return GetValue<st_bool>(2); }
			ST_INLINE CTableArray<CLod>				Lods(void) { return GetContainer<CTableArray<CLod> >(3); }
			ST_INLINE CTableArray<CCollisionObject>	CollisionObjects(void) { return GetContainer<CTableArray<CCollisionObject> >(4); }

			// material info
			ST_INLINE CTableArray<CMaterial>		Materials(void) { return GetContainer<CTableArray<CMaterial> >(5); }
			ST_INLINE st_uint32						LightmapSize(void) { return GetValue<st_uint32>(6); }

			// wind
			ST_INLINE const SWindConfig&			Wind(void) { return GetValue<SWindConfig>(7); }

			// bones/skeleton
			ST_INLINE const CArray<SBone>			Bones(void) { return GetContainer<CArray<SBone> >(8); }

		protected:
			ST_INLINE	const st_char* FileToken(void) const { return "SpeedTree___"; }

		};

	} // end namespace GameEngine8

} // end namespace SpeedTreeDataBuffer

#ifdef ST_SETS_PACKING_INTERNALLY
	#pragma pack(pop)
#endif
