// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Types.h"
#include "Templates/SharedPointer.h"
#include "VectorTypes.h"


namespace mu
{

	//!
	class InputArchive;
	class OutputArchive;

	class Parameters;
    using ParametersPtr = Ptr<Parameters>;
    using ParametersPtrConst = Ptr<const Parameters>;

    class RangeIndex;

	using RangeIndexPtr = Ptr<RangeIndex>;
	using RangeIndexPtrConst = Ptr<const RangeIndex>;

	class Model;

	//! Default parameter values definition
	static const FVector4f DefaultMutableColorValue(-UE_BIG_NUMBER);


    //! \brief Model parameter types.
	//! \ingroup runtime
    enum class PARAMETER_TYPE : uint32
    {
        //! Undefined parameter type.
        T_NONE,

        //! Boolean parameter type (true or false)
        T_BOOL,

        //! Integer parameter type. It usually has a limited range of possible values that can be
        //! queried in the Parameters object.
        T_INT,

        //! Floating point value in the range of 0.0 to 1.0
        T_FLOAT,

        //! Floating point RGBA colour, with each channel ranging from 0.0 to 1.0
        T_COLOUR,

        //! 3D Projector type, defining a position, scale and orientation. Basically used for
        //! projected decals.
        T_PROJECTOR,

        //! An externally provided image.
        T_IMAGE,

        //! A text string.
        T_STRING,

        //! Utility enumeration value, not really a parameter type.
        T_COUNT

    };


    //! \brief Types of projectors.
    //! \ingroup runtime
    enum class PROJECTOR_TYPE : uint32
    {
        //!
        PLANAR,

        //!
        CYLINDRICAL,

		//!
        WRAPPING,

        //! Utility enumeration value, not really a projector type.
        COUNT
    };


    //! \brief Class used to set or read multi-dimensional parameter values.
    //! If parameters have multiple values because of ranges, RangeIndex can be used to specify
    //! what value is read or set in the methods of the Parameter class.
    //! RangeIndex objects can be reused in multiple calls for the same parameter in the Parameter
    //! class interface even after changing the position values.
    //! \ingroup runtime
    class MUTABLERUNTIME_API RangeIndex : public RefCounted
    {
    public:

        //-----------------------------------------------------------------------------------------
        // Life cycle
        //-----------------------------------------------------------------------------------------


        //-----------------------------------------------------------------------------------------
        // Own interface
        //-----------------------------------------------------------------------------------------

        //! Return the number of ranges (or dimensions) used by this index
        int GetRangeCount() const;

        //! Return the name of a range.
        //! \param index Index of the range from 0 to GetRangeCount()-1
        const FString& GetRangeName( int index ) const;

        //! Return the Guid of the parameter, resistant to parameter name changes.
        //! \param index Index of the parameter from 0 to GetRangeCount()-1
        const FString& GetRangeUid( int index ) const;

        //! Set the position in one of the dimensions of the index.
        //! \param index Index of the parameter from 0 to GetRangeCount()-1
        void SetPosition( int index, int position );

        //! Return the position in one of the dimensions of the index.
        //! \param index Index of the parameter from 0 to GetRangeCount()-1
        int GetPosition( int index ) const;


        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;

        Private* GetPrivate() const;

    protected:

        //! Forbid creation. Instances of this class must be obtained from a Parameters instance
        RangeIndex();

        //! Forbidden. Manage with the Ptr<> template.
        ~RangeIndex() override;

    private:

        friend class Parameters;
        Private* m_pD;

    };


    //! \brief  This class represents the parameters of a model including description, type and
    //! value, and additional resources associated to the parameter.
    //!
    //! \warning Every object of this class holds a reference to the model that it was created from.
    //! This implies that while any instance of a Parameters object is alive, its model will not
    //! be freed.
    //!
	//! \ingroup runtime
	class MUTABLERUNTIME_API Parameters : public RefCounted
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------

        //! Serialisation
        static void Serialise( const Parameters* p, OutputArchive& arch );
        static ParametersPtr StaticUnserialise( InputArchive& arch );

        //! Portable serialisation, which tries to keep values when:
        //! - new parameters have been added
        //! - parameters have been removed
        //! - parameter types have changed
        //! - parameter names have changed, and the application is using UIDs
        //static void SerialisePortable( const Parameters*, OutputArchive& );
        //static ParametersPtr UnserialisePortable( InputArchive&, TSharedPtr<const Model> );

		//! Deep clone this object.
		ParametersPtr Clone() const;

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Return the number of parameters
		int GetCount() const;

		//! Return the name of the parameter.
		//! \param index Index of the parameter from 0 to GetCount()-1
		const FString& GetName( int index ) const;

		//! Return the Guid of the parameter, resistant to parameter name changes.
		//! \param index Index of the parameter from 0 to GetCount()-1
		const FGuid& GetUid( int index ) const;

		//! Find the parameter index by name.
		//! It returns -1 if the parameter doesn't exist.
		int Find( const FString& Name ) const;

		//! Return the type of the parameter.
		//! \param index Index of the parameter from 0 to GetCount()-1
		PARAMETER_TYPE GetType( int index ) const;

        //! Create a new RangeIndex object to use to access a multi-dimensional parameter.
        //! It will return nullptr if the parameter is not multidimensional.
        Ptr<RangeIndex> NewRangeIndex( int paramIndex ) const;

        //! Return the number of values other than the default that have been set to a specific
        //! parameter.
        int GetValueCount( int paramIndex ) const;

        //! Return the RangeIndex of a value that has been set to a parameter.
        //! \param paramIndex Index of the parameter from 0 to GetCount()-1
        //! \param valueIndex Index of the value from 0 to GetValueCount()-1
		Ptr<RangeIndex> GetValueIndex( int paramIndex, int valueIndex ) const;

        //! Remove all the multidimensional values for a parameter. The current non-dimensional
        //! value is kept.
        void ClearAllValues( int paramIndex );

		//! Return the value of a boolean parameter.
		//! \pre The parameter specified by index is a T_BOOL.
		//! \param index Index of the parameter from 0 to GetCount()-1
        //! \param pos Only for multidimensional parameters: relevant position to get in the ranges
        bool GetBoolValue( int index, const Ptr<const RangeIndex>& pos=nullptr ) const;

		//! Set the value of a parameter of type boolean.
		//! \pre The parameter specified by index is a T_BOOL.
		//! \param index Index of the parameter from 0 to GetCount()-1
		//! \param value new value of the parameter
        //! \param pos optional parameter to set a specific value for the given multidimensional
        //! position. If null, the value is set for all possible positions.
        void SetBoolValue( int index, bool value, const Ptr<const RangeIndex>& pos=nullptr );

		//! Get the number of possible values for this integer parameter.
		//! If the number is zero, it means any integer value is accepted.
		int GetIntPossibleValueCount( int paramIndex ) const;

		//! Get the value of one of the possible values for this integer.
		//! The paramIndex is in the range of 0 to GetIntPossibleValueCount()-1
		int GetIntPossibleValue( int paramIndex, int valueIndex ) const;

        //! Get the name of one of the possible values for this integer.
        //! The paramIndex is in the range of 0 to GetIntPossibleValueCount()-1
        const FString& GetIntPossibleValueName( int paramIndex, int valueIndex ) const;

        //! Get the index of the value of one of the possible values for this integer.
        //! The paramIndex is in the range of 0 to GetIntPossibleValueCount()-1
		int GetIntValueIndex(int paramIndex, const FString& valueName) const;

		//! Get the index of the value of one of the possible values for this integer.
		//! The paramIndex is in the range of 0 to GetIntPossibleValueCount()-1
		int GetIntValueIndex(int paramIndex, int32 Value) const;

		//! Return the value of a integer parameter.
		//! \pre The parameter specified by index is a T_INT.
		//! \param index Index of the parameter from 0 to GetCount()-1
        //! \param pos Only for multidimensional parameters: relevant position to get in the ranges
        int GetIntValue( int index, const Ptr<const RangeIndex>& pos=nullptr ) const;

		//! If the parameter is of the integer type, set its value.
		//! \param index Index of the parameter from 0 to GetCount()-1
		//! \param value new value of the parameter. It must be in the possible values for this
		//! parameter (see GetIntPossibleValue), or the method will leave the value unchanged.
        //! \param pos Only for multidimensional parameters: relevant position to set in the ranges
        void SetIntValue( int index, int value, const Ptr<const RangeIndex>& pos=nullptr );

		//! Return the value of a float parameter.
		//! \pre The parameter specified by index is a T_FLOAT.
		//! \param index Index of the parameter from 0 to GetCount()-1
        //! \param pos Only for multidimensional parameters: relevant position to get in the ranges
        float GetFloatValue( int index, const Ptr<const RangeIndex>& pos=nullptr ) const;

		//! If the parameter is of the float type, set its value.
		//! \param index Index of the parameter from 0 to GetCount()-1
		//! \param value new value of the parameter
        //! \param pos Only for multidimensional parameters: relevant position to set in the ranges
        void SetFloatValue( int index, float value, const Ptr<const RangeIndex>& pos=nullptr );

		//! Return the value of a colour parameter.
		//! \pre The parameter specified by index is a T_FLOAT.
        //! \param index Index of the parameter from 0 to GetCount()-1
        //! \param pR,pG,pB Pointers to values where every resulting colour channel will be stored
        //! \param pos Only for multidimensional parameters: relevant position to get in the ranges
        void GetColourValue( int index, float* pR, float* pG, float* pB, float* pA,
                             const Ptr<const RangeIndex>& pos=nullptr ) const;

		//! If the parameter is of the colour type, set its value.
		//! \param index Index of the parameter from 0 to GetCount()-1
        //! \param r,g,b new value of the parameter
        //! \param pos Only for multidimensional parameters: relevant position to set in the ranges
        void SetColourValue( int index, float R, float G, float B, float A,
                             const Ptr<const RangeIndex>& pos=nullptr );

		//! Return the value of a projector parameter, as a 4x4 matrix. The matrix is supposed to be
		//! a linear transform in column-major.
		//! \pre The parameter specified by index is a T_PROJECTOR.
        //! \param ParameterIndex Index of the parameter from 0 to GetCount()-1
        //! \param OutPos Pointer to where the object-space position coordinates of the projector will be stored.
        //! \param OutDir Pointer to where the object-space direction vector of the projector will be stored.
        //! \param OutUp Pointer to where the object-space vertically up direction vector
        //!         of the projector will be stored. This controls the "roll" angle of the
        //!         projector.
        //! \param OutScale Pointer to the projector-space scaling of the projector.
        //! \param RangePosition Only for multidimensional parameters: relevant position to get in the ranges
        void GetProjectorValue( int ParameterIndex,
                                PROJECTOR_TYPE* OutProjectionType,
								FVector3f* OutPos,
								FVector3f* OutDir,
								FVector3f* OutUp,
								FVector3f* OutScale,
                                float* OutProjectionAngle,
                                const Ptr<const RangeIndex>& RangePosition=nullptr ) const;

		//! If the parameter is of the projector type, set its value.
		//! \param ParameterIndex Index of the parameter from 0 to GetCount()-1
        //! \param pos Object-space position coordinates of the projector.
        //! \param dir Object-space direction vector of the projector.
        //! \param up Object-space vertically up direction vector of the projector.
        //! \param scale Projector-space scaling of the projector.
        //! \param projectionAngle [only for Cylindrical projectors], the angle in radians of the
        //! projection area on the cylinder surface.
        //! \param RangePosition Only for multidimensional parameters: relevant position to set in the ranges
        void SetProjectorValue( int ParameterIndex,
								const FVector3f& pos,
								const FVector3f& dir,
								const FVector3f& up,
								const FVector3f& scale,
                                float projectionAngle,
                                const Ptr<const RangeIndex>& RangePosition=nullptr );

        //! Return the value of an image parameter.
        //! \pre The parameter specified by index is a T_IMAGE.
        //! \param index Index of the parameter from 0 to GetCount()-1
		//! \param pos Only for multidimensional parameters: relevant position to set in the ranges
	    //! \return The externalId specified when setting the image value (\see SetImageValue)
		FName GetImageValue( int index, const Ptr<const RangeIndex>& pos = nullptr) const;

        //! If the parameter is of the image type, set its value.
        //! \param index Index of the parameter from 0 to GetCount()-1
        //! \param externalId Application-specific id used to identify this image during replication.
		//! \param pos Only for multidimensional parameters: relevant position to set in the ranges
		void SetImageValue( int index, FName externalId, const Ptr<const RangeIndex>& pos = nullptr);

        //! Return the value of a float parameter.
        //! \pre The parameter specified by index is a T_FLOAT.
        //! \param index Index of the parameter from 0 to GetCount()-1
        //! \param pos Only for multidimensional parameters: relevant position to get in the ranges
        void GetStringValue( int index, FString& OutValue, const Ptr<const RangeIndex>& pos = nullptr) const;

        //! If the parameter is of the float type, set its value.
        //! \param index Index of the parameter from 0 to GetCount()-1
        //! \param value new value of the parameter
        //! \param pos Only for multidimensional parameters: relevant position to set in the ranges
        void SetStringValue( int index, const FString& Value, const Ptr<const RangeIndex>& pos = nullptr );

        //! Utility method to compare the values of a specific parameter with the values of another
        //! Parameters object. It returns false if type or values are different.
        bool HasSameValue( int thisParamIndex, const ParametersPtrConst& other, int otherParamIndex ) const;

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		Private* GetPrivate() const;

	protected:

		//! Forbid creation. Instances of this class must be obtained from a Model instance
		Parameters();

		//! Forbidden. Manage with the Ptr<> template.
		~Parameters();

	private:

		friend class Model;
		Private* m_pD;

	};


}

