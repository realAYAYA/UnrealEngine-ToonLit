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
#include "Tasks/Task.h"
#include "Templates/Tuple.h"


/** If set to 1, this enables some expensive Unreal Insights traces, but can lead to 5x slower mutable operation. 
* Other cheaper traces are enabled at all times.
*/
#define UE_MUTABLE_ENABLE_SLOW_TRACES	0


#include "System.generated.h"


/** Despite being an UEnum, this is not always version-serialized (in MutableTools).
 * Beware of changing the enum options or order.
 */
UENUM()
enum class ETextureCompressionStrategy : uint8
{
	/** Don't change the generated format. */
	None,

	/** If a texture depends on run-time parameters for an object state, don't compress. */
	DontCompressRuntime,

	/** Never compress the textures for this state. */
	NeverCompress
};


MUTABLERUNTIME_API extern TAutoConsoleVariable<bool> CVarTaskGraphBusyWait;


namespace mu
{
	// Forward references
	class Model;
	class ModelReader;
	class Parameters;
    class Mesh;
	class ExtensionDataStreamer;

    class System;
    using SystemPtr=Ptr<System>;
    using SystemPtrConst=Ptr<const System>;


	MUTABLE_DEFINE_ENUM_SERIALISABLE(ETextureCompressionStrategy);


	/** */
	enum class EExecutionStrategy : uint8
	{
		/** Undefined. */
		None = 0,

		/** Always try to run operations that reduce working memory first. */
		MinimizeMemory,

		/** Always try to run operations that unlock more operations first. */
		MaximizeConcurrency,

		/** Utility value with the number of error types. */
		Count
	};


    /** Interface to request external images used as parameters. */
    class MUTABLERUNTIME_API ImageParameterGenerator
    {
    public:

        //! Ensure virtual destruction
        virtual ~ImageParameterGenerator() = default;

        //! Returns the completion event and a cleanup function that must be called once event is completed.
		virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetImageAsync(FName Id, uint8 MipmapsToSkip, TFunction<void(Ptr<Image>)>& ResultCallback) = 0;
		virtual TTuple<UE::Tasks::FTask, TFunction<void()>> GetReferencedImageAsync(const void* ModelPtr, int32 Id, uint8 MipmapsToSkip, TFunction<void(Ptr<Image>)>& ResultCallback) { check(false); return {}; }

        virtual mu::FImageDesc GetImageDesc(FName Id, uint8 MipmapsToSkip) = 0;
    };


    //! \brief Main system class to load models and build instances.
	//! \ingroup runtime
	class MUTABLERUNTIME_API System : public RefCounted
	{
    public:

        //! This constant can be used in place of the lodMask in methods like BeginUpdate
        static constexpr uint32 AllLODs = 0xffffffff;

    public:

		//! Constructor of a system object to build data.
        //! \param Settings Optional class with the settings to use in this system. The default
        //! value configures a production-ready system.
		//! \param DataStreamer Optional interface to allow the Model to stream in ExtensionData from disk
        System( const Ptr<Settings>& Settings = nullptr, const TSharedPtr<ExtensionDataStreamer>& DataStreamer = nullptr );

        //! Set a new provider for model data. 
        void SetStreamingInterface(const TSharedPtr<ModelReader>& );

        /** Set the working memory limit, overrding any set in the settings when the system was created.
         * Refer to Settings::SetWorkingMemoryBudget for more information.
		 */
        void SetWorkingMemoryBytes( uint64 Bytes );

        /** Removes all the possible working memory regardless of the budget set. This may make following 
		* operations take longer.
		*/
		void ClearWorkingMemory();
		
		/** Set the amount of generated resources keys that will be stored for resource reusal. */
		void SetGeneratedCacheSize(uint32 InCount);

        /** Set a new provider for external image data. This is only necessary if image parameters are used in the models. */
        void SetImageParameterGenerator(const TSharedPtr<ImageParameterGenerator>&);

		/** Set a function that will be used to convert image pixel formats instead of the internal conversion. 
		* \warning The provided function can be called from any thread, and also concurrently.
		* If this function fails (returns false in the first parameter) the internal function is attempted next.
		* This is useful to provide higher-quality external compressors in editor or when cooking.
		*/
		void SetImagePixelConversionOverride(const FImageOperator::FImagePixelFormatFunc&);

        //! Create a new instance from the given model. The instance can then be configured through
        //! calls to BeginUpdate/EndUpdate.
        //! A call to NewInstance must be paired to a call to ReleasesInstance when the instance is
        //! no longer needed.
        //! \param pModel Model to build an instance of
        //! \return An identifier that is always bigger than 0.
        Instance::ID NewInstance(const TSharedPtr<const Model>& Model);

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
        const Instance* BeginUpdate(Instance::ID InstanceID,
                                    const Ptr<const Parameters>& Params,
                                    int32 StateIndex,
                                    uint32 LodMask);

		//! Only valid between BeginUpdate and EndUpdate
		//! Calculate the description of an image, without generating it.
		UE::Tasks::TTask<FImageDesc> GetImageDesc(Instance::ID InstanceID, FResourceID ImageId);

		//! Only valid between BeginUpdate and EndUpdate
		//! \param MipsToSkip Number of mips to skip compared from the full image.
		//! If 0, all mip levels will be generated. If more levels than possible to discard are specified, 
		//! the image will still contain a minimum number of mips specified at model compile time.
		UE::Tasks::TTask<Ptr<const Image>> GetImage(Instance::ID InstanceID, FResourceID ImageId, int32 MipsToSkip = 0, int32 LOD = 0);

        //! Only valid between BeginUpdate and EndUpdate
		UE::Tasks::TTask<Ptr<const Mesh>> GetMesh(Instance::ID InstanceID, FResourceID MeshId);

		//! Only valid between BeginUpdate and EndUpdate
		//! Calculate the description of an image, without generating it.
		void GetImageDescInline(Instance::ID InstanceID, FResourceID ImageId, FImageDesc& OutDesc);

		//! Only valid between BeginUpdate and EndUpdate
		//! \param MipsToSkip Number of mips to skip compared from the full image.
		//! If 0, all mip levels will be generated. If more levels than possible to discard are specified, 
		//! the image will still contain a minimum number of mips specified at model compile time.
		Ptr<const Image> GetImageInline(Instance::ID InstanceID, FResourceID ImageId, int32 MipsToSkip = 0, int32 LOD = 0);

        //! Only valid between BeginUpdate and EndUpdate
		Ptr<const Mesh> GetMeshInline(Instance::ID InstanceID, FResourceID MeshId);

        //! Invalidate and free the last Instance data returned by a call to BeginUpdate with
        //! the same instance index. After a call to this method, that Instance cannot be used any
        //! more and its content is undefined.
        //! \param instance The index of the instance whose last data will be invalidated.
        void EndUpdate(Instance::ID InstanceID);

        //! Completely destroy an instance. After a call to this method the given instance cannot be
        //! updated any more, and its resources may have been freed.
        //! \param instance The id of the instance to destroy.
        void ReleaseInstance(Instance::ID InstanceID);

		//! Calculate the relevancy of every parameter. Some parameters may be unused depending on
		//! the values of other parameters. This method will set to true the flags for parameters
		//! that are relevant, and to false otherwise. This is useful to hide irrelevant parameters
		//! in dynamic user interfaces.
        //! \param pModel The model used to create the Parameters instance.
        //! \param pParameters Parameter set that we want to find the relevancy of.
        //! \param pFlags is a pointer to a preallocated array of booleans that contains at least
		//! pParameters->GetCount() elements.
        void GetParameterRelevancy( Instance::ID InstanceID,
									const Ptr<const Parameters>& Parameters,
									bool* Flags );

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

