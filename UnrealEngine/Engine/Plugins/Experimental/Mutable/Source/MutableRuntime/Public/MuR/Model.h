// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "Templates/Function.h"
#include "Templates/SharedPointer.h"


//! This version number changes whenever there is a compatibility-breaking change in the Model
//! data structures. Compiled models are not necessarily compatible when the runtime is updated,
//! so this version number can be used externally to verify this. It is not used internally, and
//! serializing models from different versions than this runtime will probably result in a crash.
#define MUTABLE_COMPILED_MODEL_CODE_VERSION		uint32( 83 )
#define MUTABLE_PARAMETERS_VERSION              uint32( 3 )


namespace mu
{
	class InputArchive;
	class ModelWriter;
	class OutputArchive;

	typedef Ptr<Parameters> ParametersPtr;
	typedef Ptr<const Parameters> ParametersPtrConst;

    class ModelParametersGenerator;

    typedef Ptr<ModelParametersGenerator> ModelParametersGeneratorPtr;
    typedef Ptr<const ModelParametersGenerator> ModelParametersGeneratorPtrConst;

    class System;

    typedef Ptr<System> SystemPtr;
    typedef Ptr<const System> SystemPtrConst;


    //! \brief A Model represents a customisable object with any number of parameters.
    //!
    //! When values are given to the parameters, specific Instances can be built, which hold the
    //! built application-usable data.
	//! \ingroup runtime
    class MUTABLERUNTIME_API Model
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------
		Model();

		//! Don't call directly. Manage with a TSharedPtr.
		~Model();

		static void Serialise( const Model* p, OutputArchive& arch );
		static TSharedPtr<Model> StaticUnserialise( InputArchive& arch );

		//! Special serialise operation that serialises the data in separate "files". An object
        //! with the ModelStreamer interface is responsible of storing this data and providing
		//! the "file" concept.
        static void Serialise( Model* p, ModelWriter& arch );

		//! Return true if the model has external data in other files. This kind of models will
		//! require data streaming when used.
		bool HasExternalData() const;

#if WITH_EDITOR
		//! Return true unless the streamed resources were destroyed, which could happen in the
		//! editor after recompiling the CO.
		bool IsValid() const;

		//! Invalidate the Model. Compiling a compiled CO will invalidate the model kept by previously
		//! generated resources, like streamed textures.
		void Invalidate();
#endif

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Create a set of new parameters of the model with the default values.
		//! If old parameters are provided, they will be reused when possible instead of the
		//! default values.
        static ParametersPtr NewParameters( TSharedPtr<const Model> Model, const Parameters* pOld = nullptr );

		/** Return true if the parameter is multi-dimensional */
		bool IsParameterMultidimensional(int32 ParameterIndex) const;

		//! Get the number of states in the model.
		int GetStateCount() const;

		//! Get a state name by state index from 0 to GetStateCount-1
		const FString& GetStateName( int32 StateIndex ) const;

		//! Find a state index by state name
		int32 FindState( const FString& Name ) const;

		//! Get the number of parameters available in a particular state.
		int GetStateParameterCount( int stateIndex ) const;

		//! Get the index of one of the parameters in the given state. The index refers to the
		//! parameters in a Parameters object obtained from this model with NewParameters.
		int GetStateParameterIndex( int stateIndex, int paramIndex ) const;

        //! Free memory used by streaming data that may be loaded again when needed.
        void UnloadExternalData();
    	
		//! Return the default value of a boolean parameter.
		//! \pre The parameter specified by index is a T_BOOL.
		//! \param Index Index of the parameter from 0 to GetCount()-1
    	bool GetBoolDefaultValue(int32 Index) const;

   		//! Return the default value of a integer parameter.
		//! \pre The parameter specified by index is a T_INT.
		//! \param Index Index of the parameter from 0 to GetCount()-1
    	int32 GetIntDefaultValue(int32 Index) const;

		//! Return the default value of a float parameter.
		//! \pre The parameter specified by index is a T_FLOAT.
		//! \param Index Index of the parameter from 0 to GetCount()-1
        float GetFloatDefaultValue(int32 Index) const;

		//! Return the default value of a colour parameter.
		//! \pre The parameter specified by index is a T_FLOAT.
        //! \param Index Index of the parameter from 0 to GetCount()-1
        //! \param R,G,B Pointers to values where every resulting colour channel will be stored
    	void GetColourDefaultValue(int32 Index, float* R, float* G, float* B, float* A) const;

    	//! Return the default value of a projector parameter, as a 4x4 matrix. The matrix is supposed to be
		//! a linear transform in column-major.
		//! \pre The parameter specified by index is a T_PROJECTOR.
        //! \param Index Index of the parameter from 0 to GetCount()-1
        //! \param OutPos Pointer to where the object-space position coordinates of the projector will be stored.
        //! \param OutDir Pointer to where the object-space direction vector of the projector will be stored.
        //! \param OutUp Pointer to where the object-space vertically up direction vector
        //!         of the projector will be stored. This controls the "roll" angle of the
        //!         projector.
        //! \param OutScale Pointer to the projector-space scaling of the projector.
    	void GetProjectorDefaultValue(int32 Index, PROJECTOR_TYPE* OutProjectionType, FVector3f* OutPos,
    	 	FVector3f* OutDir, FVector3f* OutUp, FVector3f* OutScale, float* OutProjectionAngle) const;

        //! Return the default value of an image parameter.
        //! \pre The parameter specified by index is a T_IMAGE.
        //! \param Index Index of the parameter from 0 to GetCount()-1
		//! \return The externalId specified when setting the image value (\see SetImageValue)
		FName GetImageDefaultValue(int32 Index) const;

    	int32 GetRomCount() const;

		uint32 GetRomId(int32 Index) const;

		uint32 GetRomSize(int32 Index) const;
    	
		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		Private* GetPrivate() const;

	private:

		Private* m_pD;

	};


    //! \brief This class enumerates all possible model instances and generates the parameters for
    //! each of them.
    //!
    //! This class can be used to generate all the possible instances of a model, or to generate
    //! random instances. It only takes into account discrete parameters (bools and ints) but not
    //! continuous parameters like colours and floats, which may be set to random values.
    //!
    //! \ingroup runtime
    class MUTABLERUNTIME_API ModelParametersGenerator : public RefCounted
    {
    public:

        //!
        ModelParametersGenerator( TSharedPtr<const Model>, System* );

        //! Return the number of different possible instances that can be built from the model.
        int64 GetInstanceCount();

        //! Return the parameters of one of the possible instances of the model.
        //! \param index is an index in the range of 0 to GetInstanceCount-1
        //! \param randomGenerator if not null, this function will be used to generate random values
        //! for the continuous parameters of the instance.
        ParametersPtr GetInstance( int64 index, TFunction<float()> RandomGenerator);

        //! Return the parameters of one of a random instance of the model.
        //! \param randomGenerator This function will be used to generate random values
        //! for the continuous parameters of the instance.
        ParametersPtr GetRandomInstance(TFunctionRef<float()> RandomGenerator );

        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;

        Private* GetPrivate() const;

    protected:

        //! Forbidden. Manage with the Ptr<> template.
        ~ModelParametersGenerator();

    private:

        Private* m_pD;
    };

}

