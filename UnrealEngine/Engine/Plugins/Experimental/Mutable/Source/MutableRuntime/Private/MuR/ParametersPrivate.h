// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Parameters.h"

#include "MuR/SerialisationPrivate.h"
#include "MuR/MemoryPrivate.h"
#include "MuR/Operations.h"
#include "MuR/ImagePrivate.h"

#include "MuR/Model.h"


namespace mu
{

    MUTABLE_DEFINE_ENUM_SERIALISABLE( PARAMETER_TYPE )
    MUTABLE_DEFINE_ENUM_SERIALISABLE( PARAMETER_DETAILED_TYPE )
    MUTABLE_DEFINE_ENUM_SERIALISABLE( PROJECTOR_TYPE )


    //---------------------------------------------------------------------------------------------
    //! Information to project an image on a mesh.
    //---------------------------------------------------------------------------------------------
    struct PROJECTOR
    {
        PROJECTOR_TYPE type = PROJECTOR_TYPE::PLANAR;
        float position[3] = {0,0,0};
        float direction[3] = {0,0,0};
        float up[3] = {0,0,0};
        float scale[3] = {0,0,0};
        float projectionAngle = 0.0f;

        //!
        inline void GetDirectionSideUp( vec3f& d,
                                        vec3f& s,
                                        vec3f& u ) const
        {
            d = vec3f( direction[0], direction[1], direction[2] );
            u = vec3f( up[0], up[1], up[2] );
            s = cross( u, d );
            normalise(s);
        }


        //!
        void Serialise( OutputArchive& arch ) const
        {
            arch << type;
            arch << position[0];
            arch << position[1];
            arch << position[2];
            arch << direction[0];
            arch << direction[1];
            arch << direction[2];
            arch << up[0];
            arch << up[1];
            arch << up[2];
            arch << scale[0];
            arch << scale[1];
            arch << scale[2];
            arch << projectionAngle;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            arch >> type;
            arch >> position[0];
            arch >> position[1];
            arch >> position[2];
            arch >> direction[0];
            arch >> direction[1];
            arch >> direction[2];
            arch >> up[0];
            arch >> up[1];
            arch >> up[2];
            arch >> scale[0];
            arch >> scale[1];
            arch >> scale[2];
            arch >> projectionAngle;
        }

        bool operator==( const PROJECTOR& o ) const
        {
            return     type==o.type
                    && position[0]==o.position[0]
                    && position[1]==o.position[1]
                    && position[2]==o.position[2]
                    && up[0]==o.up[0]
                    && up[1]==o.up[1]
                    && up[2]==o.up[2]
                    && direction[0]==o.direction[0]
                    && direction[1]==o.direction[1]
                    && direction[2]==o.direction[2]
                    && scale[0]==o.scale[0]
                    && scale[1]==o.scale[1]
                    && scale[2]==o.scale[2]
                    && projectionAngle==o.projectionAngle;
        }

    };

	//---------------------------------------------------------------------------------------------
	//! Information about a generic shape in space.
	//---------------------------------------------------------------------------------------------
	struct SHAPE
	{
		// Transform
		vec3f position;
		vec3f up;
		vec3f side;
		
		vec3f size;

		// 
		enum class Type : uint8_t
		{
			None = 0,
			Ellipse,
			AABox
		};
        uint8_t type = 0;
		

		//!
		void Serialise(OutputArchive& arch) const
		{
			arch << position;
			arch << up;
			arch << side;
			arch << size;
			arch << type;
		}

		//!
		void Unserialise(InputArchive& arch)
		{
			arch >> position;
			arch >> up;
			arch >> side;
			arch >> size;
			arch >> type;
		}

        bool operator==( const SHAPE& o ) const
        {
            return type==o.type
                    && position==o.position
                    && up==o.up
                    && side==o.side
                    && size==o.size;
        }
	};


	//---------------------------------------------------------------------------------------------
	//! Information about a curve interpolated from keyframes.
	//---------------------------------------------------------------------------------------------
	struct CurveKeyFrame
	{
		float time;
		float value;
		float in_tangent;
		float in_tangent_weight;
		float out_tangent;
		float out_tangent_weight;

		enum class InterpMode : uint8_t
		{
			Linear = 0,
			Constant,
			Cubic,
			None,

			//! Utility enumeration value, not really a Interp mode.
			Count
		};
        uint8_t interp_mode;


		enum class TangentMode : uint8_t
		{
			Auto = 0,
			Manual,
			Broken,
			None,

			//! Utility enumeration value, not really a Tangent mode.
			Count
		};
        uint8_t tangent_mode;


		enum class TangentWeightMode : uint8_t
		{
			Auto = 0,
			Manual,
			Broken,
			None,

			//! Utility enumeration value, not really a Tangent weight mode.
			Count
		};
        uint8_t tangent_weight_mode;

        //!
        bool operator==( const CurveKeyFrame& other ) const
        {
            return time==other.time &&
                    value==other.value&&
                    in_tangent==other.in_tangent&&
                    in_tangent_weight==other.in_tangent_weight&&
                    out_tangent==other.out_tangent&&
                    out_tangent_weight==other.out_tangent_weight&&
                    interp_mode==other.interp_mode&&
                    tangent_mode==other.tangent_mode&&
                    tangent_weight_mode==other.tangent_weight_mode;
        }

		//!
		void Serialise(OutputArchive& arch) const
		{
			arch << time;
			arch << value;
			arch << in_tangent;
			arch << in_tangent_weight;
			arch << out_tangent;
			arch << out_tangent_weight;

			arch << interp_mode;
			arch << tangent_mode;
			arch << tangent_weight_mode;
		}

		//!
		void Unserialise(InputArchive& arch)
		{
			arch >> time;
			arch >> value;
			arch >> in_tangent;
			arch >> in_tangent_weight;
			arch >> out_tangent;
			arch >> out_tangent_weight;

			arch >> interp_mode;
			arch >> tangent_mode;
			arch >> tangent_weight_mode;
		}
	};


	struct Curve
	{
		TArray<CurveKeyFrame> keyFrames;
		float defaultValue = 0.f;

        //!
        bool operator==( const Curve& other ) const
        {
            return keyFrames == other.keyFrames && defaultValue == other.defaultValue;
        }

		//!
		void Serialise(OutputArchive& arch) const
		{
            arch << keyFrames;
            arch << defaultValue;
		}

		//!
		void Unserialise(InputArchive& arch)
		{
			arch >> keyFrames;
            arch >> defaultValue;
        }
	};


    union PARAMETER_VALUE
    {
        PARAMETER_VALUE()
			: m_projector()
        {
            memset( this, 0, sizeof(PARAMETER_VALUE) );
        }

        bool m_bool;

        int m_int;

        float m_float;

        float m_colour[3];

        PROJECTOR m_projector;

        EXTERNAL_IMAGE_ID m_image;

        char m_text[MUTABLE_MAX_STRING_PARAM_LENGTH+1];

        //!
        bool operator==( const PARAMETER_VALUE& other ) const
        {
            return FMemory::Memcmp(this, &other, sizeof(PARAMETER_VALUE))==0;
        }

    };

    MUTABLE_DEFINE_POD_SERIALISABLE( PARAMETER_VALUE )


    struct PARAMETER_DESC
    {
        string m_name;

        //! Unique id (provided externally, so no actual guarantee that it is unique.)
		string m_uid;

        PARAMETER_TYPE m_type = PARAMETER_TYPE::T_NONE;

        PARAMETER_DETAILED_TYPE m_detailedType = PARAMETER_DETAILED_TYPE::UNKNOWN;

        PARAMETER_VALUE m_defaultValue;

        //! Ranges, if the parameter is multi-dimensional. The indices refer to the Model's program
        //! vector of range descriptors.
		TArray<uint32_t> m_ranges;

        //! Additional parameter description information
		TArray<OP::ADDRESS> m_descImages;

        //! Possible values of the parameter in case of being an integer, and its names
        struct INT_VALUE_DESC
        {
            int16_t m_value;
            string m_name;

            //!
            bool operator==( const INT_VALUE_DESC& other ) const
            {
                return m_value==other.m_value &&
                        m_name==other.m_name;
            }

            //!
            void Serialise( OutputArchive& arch ) const
            {
                arch << m_value;
                arch << m_name;
            }

            //!
            void Unserialise( InputArchive& arch )
            {
                arch >> m_value;
                arch >> m_name;
            }
        };

        //! For integer parameters, this contains the description of the possible values.
        //! If empty, the integer may have any value.
		TArray<INT_VALUE_DESC> m_possibleValues;

        //!
        bool operator==( const PARAMETER_DESC& other ) const
        {
            return m_name == other.m_name && m_uid == other.m_uid && m_type == other.m_type &&
                   m_defaultValue == other.m_defaultValue &&
                   m_ranges == other.m_ranges && m_descImages == other.m_descImages &&
                   m_possibleValues == other.m_possibleValues;
        }

        //!
        void Serialise( OutputArchive& arch ) const
        {
            const int32_t ver = 5;
            arch << ver;

			arch << m_name;
			arch << m_uid;
            arch << m_type;
            arch << m_defaultValue;
            arch << m_ranges;
            arch << m_descImages;
            arch << m_possibleValues;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            int32_t ver;
            arch >> ver;
            check( ver == 5 );

			arch >> m_name;
            arch >> m_uid;
            arch >> m_type;
            arch >> m_defaultValue;
            arch >> m_ranges;
            arch >> m_descImages;
            arch >> m_possibleValues;
        }
    };


    struct RANGE_DESC
    {
        string m_name;
        string m_uid;

        //!
        bool operator==( const RANGE_DESC& other ) const
        {
            return m_name==other.m_name
                    &&
                    m_uid==other.m_uid;
        }

        //!
        void Serialise( OutputArchive& arch ) const
        {
            const int32_t ver = 1;
            arch << ver;

            arch << m_name;
            arch << m_uid;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            int32_t ver;
            arch >> ver;
            check( ver == 1 );

            arch >> m_name;
            arch >> m_uid;
        }
    };


    class MUTABLERUNTIME_API RangeIndex::Private : public Base
    {
    public:

        //! Run-time data
        Ptr<const Parameters> m_pParameters;

        //! Index of the parameter for which we are a range index
        int m_parameter = -1;

        //! Position in the several dimension of the range, as defined in m_pParameters
		TArray<int> m_values;
    };


    class MUTABLERUNTIME_API Parameters::Private : public Base
    {
    public:

        //! Warning: update Parameters::Clone method if this members change.

        //! Run-time data
        ModelPtrConst m_pModel;

        //! Values for the parameters if they are not multidimensional.
		TArray<PARAMETER_VALUE> m_values;

        //! If the parameter is multidemensional, the values are stored here.
        //! The key of the map is the vector of values stored in a RangeIndex
		TArray< TMap< TArray<int32_t>, PARAMETER_VALUE > > m_multiValues;


        //!
        void Serialise( OutputArchive& arch ) const
        {
            const uint32_t ver = 1;
            arch << ver;

            arch << m_values;
            arch << m_multiValues;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32_t ver;
            arch >> ver;
            check( ver == 1 );

            arch >> m_values;
            arch >> m_multiValues;
        }

        //!
        int Find( const char* strName ) const;

        //!
        PROJECTOR GetProjectorValue( int index, const Ptr<const RangeIndex>& pos ) const;
    };

}
