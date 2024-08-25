// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/TVariant.h"
#include "Math/MathFwd.h"

#include "MuR/Parameters.h"
#include "MuR/SerialisationPrivate.h"
#include "MuR/Operations.h"
#include "MuR/Image.h"

#include "MuR/Model.h"

#include <string>

namespace mu
{

namespace Private
{

    template<class T>
    class TIndirectObject
    {
        TUniquePtr<T> StoragePtr;

    public:
        template<typename... TArgs>
        TIndirectObject(TArgs&&... Args) : StoragePtr(MakeUnique<T>(Forward<TArgs>(Args)...)) 
        {
        }
        
        TIndirectObject(const TIndirectObject<T>& Other) : StoragePtr(MakeUnique<T>()) 
        { 
            Get() = Other; 
        }

		TIndirectObject(TIndirectObject<T>&& Other) : StoragePtr(MoveTemp(Other.StoragePtr)) 
        {
        }

        TIndirectObject(const T& Object) : StoragePtr(MakeUnique<T>()) 
        { 
            Get() = Object; 
        }
        
        TIndirectObject(T&& Object) : StoragePtr(MakeUnique<T>()) 
        { 
            Get() = MoveTemp(Object); 
        }

        TIndirectObject& operator=(TIndirectObject<T>&&) = default;

        TIndirectObject& operator=(const TIndirectObject<T>& Other) 
        { 
            Get() = Other; 
			return *this;
        }

        TIndirectObject& operator=(const T& Object) 
        { 
            Get() = Object; 
            return *this;
        }

        TIndirectObject& operator=(const T&& Object) 
        { 
            Get() = MoveTemp(Object); 
            return *this;
        }

        const T& Get() const 
        { 
            check(StoragePtr); 
            return *StoragePtr; 
        }

        T& Get() 
        { 
            check(StoragePtr); 
            return *StoragePtr; 
        }

        operator T&() 
        { 
            return Get(); 
        }

        operator const T&() const 
        { 
            return Get(); 
        }

        T* operator &() 
        { 
            return StoragePtr.Get(); 
        }

        const T* operator &() const 
        { 
            return StoragePtr.Get(); 
        }

        T& operator *() 
        { 
            return Get(); 
        }

        const T& operator *() const 
        { 
            return Get(); 
        }

        bool operator==(const TIndirectObject<T>& Other) const 
        { 
            return *StoragePtr == Other; 
        }

        bool operator==(const T& Object) const 
        { 
            return *StoragePtr == Object; 
        }

		//!
		void Serialise(OutputArchive& Arch) const
		{
			Arch << Get();
		}

		//!
		void Unserialise(InputArchive& Arch)
		{
			Arch >> Get();
		} 
    };

}

    MUTABLE_DEFINE_ENUM_SERIALISABLE(PARAMETER_TYPE)
    MUTABLE_DEFINE_ENUM_SERIALISABLE(PROJECTOR_TYPE)


    /** Description of a projector to project an image on a mesh. */
    struct FProjector
    {
        PROJECTOR_TYPE type = PROJECTOR_TYPE::PLANAR;
        FVector3f position = {0,0,0};
		FVector3f direction = {0,0,0};
		FVector3f up = {0,0,0};
		FVector3f scale = {0,0,0};
        float projectionAngle = 0.0f;

        //!
        inline void GetDirectionSideUp(FVector3f& OutDirection, FVector3f& OutSide, FVector3f& OutUp) const
        {
			OutDirection = direction;
            OutUp = up;
            OutSide = FVector3f::CrossProduct( up, direction );
			OutSide.Normalize();
        }


        //!
        void Serialise( OutputArchive& arch ) const
        {
            arch << type;
            arch << position;
            arch << direction;
            arch << up;
            arch << scale;
            arch << projectionAngle;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            arch >> type;
            arch >> position;
            arch >> direction;
            arch >> up;
            arch >> scale;
            arch >> projectionAngle;
        }

        bool operator==( const FProjector& o ) const
        {
            return     type==o.type
                    && position==o.position
                    && up==o.up
                    && direction==o.direction
                    && scale==o.scale
                    && projectionAngle==o.projectionAngle;
        }

    };

	//---------------------------------------------------------------------------------------------
	//! Information about a generic shape in space.
	//---------------------------------------------------------------------------------------------
	struct FShape
	{
		// Transform
		FVector3f position;
		FVector3f up;
		FVector3f side;
		
		FVector3f size;

		// 
		enum class Type : uint8
		{
			None = 0,
			Ellipse,
			AABox
		};
        uint8 type = 0;
		

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

        bool operator==( const FShape& o ) const
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

		enum class InterpMode : uint8
		{
			Linear = 0,
			Constant,
			Cubic,
			None,

			//! Utility enumeration value, not really a Interp mode.
			Count
		};
        uint8 interp_mode;


		enum class TangentMode : uint8
		{
			Auto = 0,
			Manual,
			Broken,
			None,

			//! Utility enumeration value, not really a Tangent mode.
			Count
		};
        uint8 tangent_mode;


		enum class TangentWeightMode : uint8
		{
			Auto = 0,
			Manual,
			Broken,
			None,

			//! Utility enumeration value, not really a Tangent weight mode.
			Count
		};
        uint8 tangent_weight_mode;

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

	using ParamBoolType = bool;
	using ParamIntType = int32;
	using ParamFloatType = float;
	using ParamColorType = FVector4f;
	using ParamProjectorType = Private::TIndirectObject<FProjector>;
	using ParamImageType = FName;
	using ParamStringType = Private::TIndirectObject<FString>;
	
	using PARAMETER_VALUE = TVariant<
            ParamBoolType, ParamIntType, ParamFloatType, ParamColorType, ParamProjectorType, ParamImageType, ParamStringType>;

    // static_assert to track PARAMETER_VALUE size changes. It is ok to change if needed.
    static_assert(sizeof(PARAMETER_VALUE) == 8*4, "PARAMETER_VALUE size has changed.");

	// TVariant currently does not support this operator. Once supported remove it.
	inline bool operator==(const PARAMETER_VALUE& ValueA, const PARAMETER_VALUE& ValueB)
	{
		const int32 IndexA = ValueA.GetIndex();
		const int32 IndexB = ValueB.GetIndex();

		if (IndexA != IndexB)
		{
			return false;
		}

		return Visit([&ValueB](const auto& StoredValueA)
		{
			using Type = typename TDecay<decltype(StoredValueA)>::Type;
			return StoredValueA == ValueB.Get<Type>();
		}, ValueA);
	}
	

    struct FParameterDesc
    {
        FString m_name;

        //! Unique id (provided externally, so no actual guarantee that it is unique.)
		FGuid m_uid;

        PARAMETER_TYPE m_type = PARAMETER_TYPE::T_NONE;

        PARAMETER_VALUE m_defaultValue;

        //! Ranges, if the parameter is multi-dimensional. The indices refer to the Model's program
        //! vector of range descriptors.
		TArray<uint32> m_ranges;

        //! Possible values of the parameter in case of being an integer, and its names
        struct FIntValueDesc
        {
            int16 m_value;
			FString m_name;

            //!
            bool operator==( const FIntValueDesc& other ) const
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
		TArray<FIntValueDesc> m_possibleValues;

        //!
        bool operator==( const FParameterDesc& other ) const
        {
            return m_name == other.m_name && m_uid == other.m_uid && m_type == other.m_type &&
                   m_defaultValue == other.m_defaultValue &&
                   m_ranges == other.m_ranges &&
                   m_possibleValues == other.m_possibleValues;
        }

        //!
        void Serialise( OutputArchive& arch ) const
        {
            const int32 ver = 10;
            arch << ver;

			arch << m_name;
			arch << m_uid;
            arch << m_type;
            arch << m_defaultValue;
            arch << m_ranges;
            arch << m_possibleValues;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            int32 ver;
            arch >> ver;
            check( ver == 10 );

			arch >> m_name;
			arch >> m_uid;
            arch >> m_type;
			arch >> m_defaultValue;
            arch >> m_ranges;
			arch >> m_possibleValues;
        }
    };


    struct FRangeDesc
    {
		FString m_name;
		FString m_uid;

		/** Parameter that controls the size of this range, if any. */
		int32 m_dimensionParameter = -1;

        //!
        bool operator==( const FRangeDesc& other ) const
        {
            return m_name==other.m_name
				&&
				m_uid == other.m_uid 
				&&
				m_dimensionParameter == other.m_dimensionParameter;
        }

        //!
        void Serialise( OutputArchive& arch ) const
        {
            const uint32 ver = 3;
            arch << ver;

            arch << m_name;
            arch << m_uid;
			arch << m_dimensionParameter;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32 ver;
            arch >> ver;
            check( ver == 3 );

			arch >> m_name;
			arch >> m_uid;
			arch >> m_dimensionParameter;
        }
    };


    class MUTABLERUNTIME_API RangeIndex::Private
    {
    public:

        //! Run-time data
        Ptr<const Parameters> m_pParameters;

        //! Index of the parameter for which we are a range index
        int m_parameter = -1;

        //! Position in the several dimension of the range, as defined in m_pParameters
		TArray<int> m_values;
    };


    class MUTABLERUNTIME_API Parameters::Private
    {
    public:

        //! Warning: update Parameters::Clone method if this members change.

        //! Run-time data
        TSharedPtr<const Model> m_pModel;
 
        //! Values for the parameters if they are not multidimensional.
		TArray<PARAMETER_VALUE> m_values;


        //! If the parameter is multidemensional, the values are stored here.
        //! The key of the map is the vector of values stored in a RangeIndex
		TArray< TMap< TArray<int32>, PARAMETER_VALUE > > m_multiValues;


        //!
        void Serialise( OutputArchive& arch ) const
        {
            const uint32 ver = 3;
            arch << ver;

            arch << m_values;
            arch << m_multiValues;
        }

        //!
        void Unserialise( InputArchive& arch )
        {
            uint32 ver;
            arch >> ver;
			check(ver == 3);
       
		    arch >> m_values;
			arch >> m_multiValues;
        }

        //!
        int32 Find( const FString& Name ) const;

        //!
        FProjector GetProjectorValue( int index, const Ptr<const RangeIndex>& pos ) const;

		/** Return true if the parameter has any multi-dimensional values set. This is independent to if the model
		* accepts multi-dimensional parameters for this particular parameter.
		*/
		inline bool HasMultipleValues(int32 ParamIndex) const
		{
			if (ParamIndex >= m_multiValues.Num())
			{
				return false;
			}

			return m_multiValues[ParamIndex].Num()>0;
		}
    };

}
