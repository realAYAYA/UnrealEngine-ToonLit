// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Parameters.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "Templates/Function.h"

namespace mu { class InputArchive; }
namespace mu { class ModelStreamer; }
namespace mu { class OutputArchive; }


//! This version number changes whenever there is a compatibility-breaking change in the Model
//! data structures. Compiled models are not necessarily compatible when the runtime is updated,
//! so this version number can be used externally to verify this. It is not used internally, and
//! serializing models from different versions than this runtime will probably result in a crash.
#define MUTABLE_COMPILED_MODEL_CODE_VERSION		uint32( 48 )
#define MUTABLE_PARAMETERS_VERSION              uint32( 1 )


namespace mu
{

	typedef Ptr<Parameters> ParametersPtr;
	typedef Ptr<const Parameters> ParametersPtrConst;

    class Model;

    typedef Ptr<Model> ModelPtr;
    typedef Ptr<const Model> ModelPtrConst;

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
    class MUTABLERUNTIME_API Model : public RefCountedWeak
	{
	public:

		//-----------------------------------------------------------------------------------------
		// Life cycle
		//-----------------------------------------------------------------------------------------
		Model();

		static void Serialise( const Model* p, OutputArchive& arch );
		static ModelPtr StaticUnserialise( InputArchive& arch );

		//! Special serialise operation that serialises the data in separate "files". An object
        //! with the ModelStreamer interface is responsible of storing this data and providing
		//! the "file" concept.
        static void Serialise( Model* p, ModelStreamer& arch );

		//! Return true if the model has external data in other files. This kind of models will
		//! require data streaming when used.
		bool HasExternalData() const;

        //! Get the streamer-specific location data.
        //! This is a non-persistent string with meaning specific to every ModelStreamer
        //! implementation.
        const char* GetLocation( ) const;
        void SetLocation( const char* strLocation );

		//-----------------------------------------------------------------------------------------
		// Own interface
		//-----------------------------------------------------------------------------------------

		//! Create a set of new parameters of the model with the default values.
		//! If old parameters are provided, they will be reused when possible instead of the
		//! default values.
        ParametersPtr NewParameters( const Parameters* pOld = nullptr ) const;

		//! Get the number of states in the model.
		int GetStateCount() const;

		//! Get a state name by state index from 0 to GetStateCount-1
		const char* GetStateName( int stateIndex ) const;

		//! Find a state index by state name
		int FindState( const char* strName ) const;

		//! Get the number of parameters available in a particular state.
		int GetStateParameterCount( int stateIndex ) const;

		//! Get the index of one of the parameters in the given state. The index refers to the
		//! parameters in a Parameters object obtained from this model with NewParameters.
		int GetStateParameterIndex( int stateIndex, int paramIndex ) const;

        //! Free memory used by streaming data that may be loaded again when needed.
        void UnloadExternalData();

        //! Free memory used in internal runtime caches. This is useful for long-running processes
        //! that keep models loaded. This could be called when a game finishes, or a change of
        //! level.
        void ClearCaches();

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		Private* GetPrivate() const;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~Model() override;

	private:

		Private* m_pD;

	};


    //! \brief This class enumerates all possible model instances and generates the parameters for
    //! each of them.
    //!
    //! This class can be used to generate all the possible instances of a model, or to generate
    //! random instances. It only takes into account discrete parameters (bools and ints) but not
    //! continuous parameters like colours and floats.
    //!
    //! The use of this class can be very expensive for complex models, since the number of
    //! combinations grows geometrically. If considerRelevancy is set to false, a brute force
    //! approach will be used, but this will generate duplicated instances when changing parameters
    //! that are not relevant.
    //!
    //! \ingroup runtime
    class MUTABLERUNTIME_API ModelParametersGenerator : public RefCounted
    {
    public:

        //!
        ModelParametersGenerator( const Model* pModel, System* pSystem, bool considerRelevancy=true );

        //! Return the number of different possible instances that can be built from the model.
        int64 GetInstanceCount();

        //! Return the parameters of one of the possible instances of the model.
        //! \param index is an index in the range of 0 to GetInstanceCount-1
        //! \param randomGenerator if not null, this function will be used to generate random values
        //! for the continuous parameters of the instance.
        ParametersPtr GetInstance( int64 index, float (*randomGenerator )() );

        //! Return the parameters of one of a random instance of the model.
        //! \param randomGenerator This function will be used to generate random values
        //! for the continuous parameters of the instance.
        ParametersPtr GetRandomInstance(TFunctionRef<float()> randomGenerator );

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

