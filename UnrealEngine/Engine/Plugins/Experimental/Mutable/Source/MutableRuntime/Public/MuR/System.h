// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "MuR/Image.h"
#include "MuR/Instance.h"
#include "MuR/MutableMemory.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Settings.h"
#include "MuR/Types.h"

// This define will use the newer task graph interface to manage mutable concurrency. 
// This is currently broken in Switch and maybe other consoles, for some unknown reason.
//#define MUTABLE_USE_NEW_TASKGRAPH


namespace mu
{
	// Forward references
	class Model;
	class ModelStreamer;

    using ModelPtr=Ptr<Model>;
    using ModelPtrConst=Ptr<const Model>;

	class Parameters;

    using ParametersPtr=Ptr<Parameters>;
    using ParametersPtrConst=Ptr<const Parameters>;

    class Mesh;

    using MeshPtr=Ptr<Mesh>;
    using MeshPtrConst=Ptr<const Mesh>;

    class System;

    using SystemPtr=Ptr<System>;
    using SystemPtrConst=Ptr<const System>;



    //! List of critical errors that may happen during execution of mutable code
    enum class Error
    {
        //! No error happened.
        None = 0,

        //! A memory allocation failed
        OutOfMemory,

        //! A file was missing or the data was corrupted.
        StreamingError,

        //! The necessary functionality is not supported in this version of Mutable.
        Unsupported,

        //! Utility value with the number of error types.
        Count
    };

    //! \brief Get an error code for the most critical error that happened since mutable
    //! initialisation.
    //! \ingroup runtime
	MUTABLERUNTIME_API extern Error GetError();

    //! \brief Remove the last error code, setting it to Error::None. This should obnly be used
    //! in case of possible recovery from one of the critical errors, which is very unlikely.
    //! \ingroup runtime
	MUTABLERUNTIME_API extern void ClearError();


    //! \brief Interface to request external images used as parameters.
    //! \ingroup runtime
    class MUTABLERUNTIME_API ImageParameterGenerator : public Base
    {
    public:

        //! Ensure virtual destruction
        virtual ~ImageParameterGenerator() = default;

        //!
        virtual ImagePtr GetImage( EXTERNAL_IMAGE_ID id ) = 0;
    };


    //! \brief Main system class to load models and build instances.
	//! \ingroup runtime
	class MUTABLERUNTIME_API System : public RefCounted
	{
    public:

        //! This constant can be used in place of the lodMask in methods like BeginUpdate
        static constexpr unsigned AllLODs = 0xffffffff;

    public:

		//! Constructor of a system object to build data.
        //! \param settings Optional class with the settings to use in this system. The default
        //! value configures a production-ready system.
        System( const SettingsPtr& settings = nullptr );

        //! Set a new provider for model data. The provider will become owned by this instance and
        //! destroyed when necessary.
        void SetStreamingInterface( ModelStreamer* );

        //! Overwrite the streaming memory limit set in the settings when the system was created.
        //! Refer to Settings::SetStreamingCache for more information.
        void SetStreamingCache( uint64 bytes );

        //! Set a new provider for external image data. This is only necessary if image parameters
        //! are used in the models.
        void SetImageParameterGenerator( ImageParameterGenerator* );

        //! Set the maximum memory that this system can use. This memory is used for built data,
        //! cached data, and streaming. If set to 0, the system will use as much memory as required.
        //! \warning This limit is ignored if the system was built with SF_REFERENCE.
        //! \param mem Maximum memory, in bytes
        void SetMemoryLimit( uint32 mem );

        //! Create a new instance from the given model. The instance can then be configured through
        //! calls to BeginUpdate/EndUpdate.
        //! A call to NewInstance must be paired to a call to ReleasesInstance when the instance is
        //! no longer needed.
        //! \param pModel Model to build an instance of
        //! \return An identifier that is always bigger than 0.
        Instance::ID NewInstance( const ModelPtrConst& pModel );

        //! \brief Update an instance with a new parameter set and/or state.
        //!
        //! \warning a call to BeginUpdate must be paired with a call to EndUpdate once the returned
        //! data has been processed.
        //! \param instanceID The id of the instance to update, as created by a NewInstance call.
        //! \param pParams The parameters that customise this instance.
        //! \param stateIndex The index of the state this instance will be set to. The states range
        //! from 0 to Model::GetStateCount-1
        //! \param lodMask Bitmask selecting the levels of detail to build (i-th bit selects i-th lod).
        //! \return the instance data with all the LOD, components, and ids to generate the meshes 
        //! and images.  The returned Instance is only valid until the next call to EndUpdate with 
        //! the same instanceID parameter.
        const Instance* BeginUpdate( Instance::ID instanceID,
                                     const ParametersPtrConst& pParams,
                                     int32 stateIndex,
                                     uint32 lodMask );

		//! Only valid between BeginUpdate and EndUpdate
		//! Calculate the description of an image, without generating it.
		void GetImageDesc(Instance::ID instanceID, RESOURCE_ID imageId, FImageDesc& OutDesc );

		//! Only valid between BeginUpdate and EndUpdate
		//! \param MipsToSkip Number of mips to skip compared from the full image.
		//! If 0, all mip levels will be generated. If more levels than possible to discard are specified, 
		//! the image will still contain a minimum number of mips specified at model compile time.
		ImagePtrConst GetImage(Instance::ID instanceID, RESOURCE_ID imageId, int32 MipsToSkip = 0);

        //! Only valid between BeginUpdate and EndUpdate
        MeshPtrConst GetMesh(Instance::ID instanceID, RESOURCE_ID meshId);

        //! Invalidate and free the last Instance data returned by a call to BeginUpdate with
        //! the same instance index. After a call to this method, that Instance cannot be used any
        //! more and its content is undefined.
        //! \param instance The index of the instance whose last data will be invalidated.
        void EndUpdate( Instance::ID instance );

        //! Completely destroy an instance. After a call to this method the given instance cannot be
        //! updated any more, and its resources may have been freed.
        //! \param instance The id of the instance to destroy.
        void ReleaseInstance( Instance::ID instance );

		//! Build one of the images defined in a model parameter as additional description. These
		//! images can be used for colour bars, icons, etc...
        ImagePtrConst BuildParameterAdditionalImage( const ModelPtrConst& pModel,
                                                     const ParametersPtrConst& pParams,
                                                     int parameter,
                                                     int image );

		//! Calculate the relevancy of every parameter. Some parameters may be unused depending on
		//! the values of other parameters. This method will set to true the flags for parameters
		//! that are relevant, and to false otherwise. This is useful to hide irrelevant parameters
		//! in dynamic user interfaces.
        //! \param pModel The model used to create the Parameters instance.
        //! \param pParameters Parameter set that we want to find the relevancy of.
        //! \param pFlags is a pointer to a preallocated array of booleans that contains at least
		//! pParameters->GetCount() elements.
        void GetParameterRelevancy( const ModelPtrConst& pModel,
									const ParametersPtrConst& pParameters,
									bool* pFlags );

        //! Free memory used in internal runtime caches. This is useful for long-running processes
        //! that keep models loaded. This could be called when a game finishes, or a change of
        //! level.
        void ClearCaches();

        //-----------------------------------------------------------------------------------------
        // Debugging and profiling
        //-----------------------------------------------------------------------------------------

        //! Types of metrics that can be queried
        enum class ProfileMetric : uint8
        {
            //! No metric
            None,

            //! Number of model instances currently being created or updated.
            LiveInstanceCount,

            //! Number of bytes currently loaded for streaming data.
            StreamingCacheBytes,

            //! Number of update operations performed since the system was created.
            InstanceUpdateCount,

            //! Utility value that doesn't really represent a metric.
            _Count
        };

        //! Get the values of several profile metrics. This can be called any time in a valid
        //! system object from any thread.
        //! These values can change any time.
        uint64 GetProfileMetric( ProfileMetric ) const;

		//-----------------------------------------------------------------------------------------
		// Interface pattern
		//-----------------------------------------------------------------------------------------
		class Private;

		Private* GetPrivate() const;

    public:

        // Prevent copy, move and assignment.
        System( const System& ) = delete;
        System& operator=( const System& ) = delete;
        System( System&& ) = delete;
        System& operator=( System&& ) = delete;

	protected:

		//! Forbidden. Manage with the Ptr<> template.
        ~System() override;

	private:

		Private* m_pD;

	};


}

