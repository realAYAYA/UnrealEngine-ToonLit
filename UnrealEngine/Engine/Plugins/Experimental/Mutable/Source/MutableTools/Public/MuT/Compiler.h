// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "MuR/Image.h"
#include "MuR/System.h"
#include "Templates/SharedPointer.h"
#include "HAL/PlatformMath.h"
#include "Tasks/Task.h"

namespace mu
{
    // Forward declarations
    class Compiler;
    typedef Ptr<Compiler> CompilerPtr;
    typedef Ptr<const Compiler> CompilerPtrConst;

    class Model;
    using ModelPtr=Ptr<Model>;
    using ModelPtrConst=Ptr<const Model>;

    class Transform;
    using TransformPtr=Ptr<Transform>;
    using TransformPtrConst=Ptr<const Transform>;

    class ErrorLog;
    using ErrorLogPtr=Ptr<ErrorLog>;
    using ErrorLogPtrConst=Ptr<const ErrorLog>;

    class Node;
    class NodeTransformedObject;

	typedef TFunction<UE::Tasks::FTaskEvent(int32, TSharedPtr<Ptr<Image>>, bool)> FReferencedResourceFunc;
	typedef TFunction<void(float)> FReferencedResourceGameThreadTickFunc;

    //! \brief Options used to compile the models with a compiler.
    class MUTABLETOOLS_API CompilerOptions : public RefCounted
    {
    public:

        //! Create new settings with the default values as explained in each method below.
        CompilerOptions();

        //!
        void SetLogEnabled( bool bEnabled );

        //!
        void SetOptimisationEnabled( bool bEnabled );

        //!
        void SetConstReductionEnabled( bool bEnabled );

        //!
        void SetOptimisationMaxIteration( int32 MaxIterations );

        //!
        void SetIgnoreStates( bool bIgnore );

		/** Enable concurrent compilation. It's faster, but uses more CPU and memory. */
		void SetUseConcurrency(bool bEnabled);

        //! If enabled, the disk will be used as temporary memory. This will make the compilation
        //! process very slow, but will be able to compile very large models.
        void SetUseDiskCache( bool bEnabled );

		//! Set the quality for the image compression algorithms. The level value is used internally
		//! with System::SetImagecompressionQuality
		void SetImageCompressionQuality(int32 Quality);

		/** Set the image tiling strategy :
		 * If 0 (default) there is no tiling. Otherwise, images will be generated in tiles of the given size or less, and assembled afterwards as a final step.
		 */
		void SetImageTiling(int32 Tiling);

        //! 
        void SetDataPackingStrategy( int32 MinTextureResidentMipCount, uint64 EmbeddedDataBytesLimit, uint64 PackagedDataBytesLimit);

		/** If enabled it will make sure that the object is compile to generate smaller mips of the images. */
		void SetEnableProgressiveImages(bool bEnabled);

		/** Set an optional pixel conversion function that will be called before any pixel format conversion. */
		void SetImagePixelFormatOverride(const FImageOperator::FImagePixelFormatFunc&);

		/** */
		void SetReferencedResourceCallback(const FReferencedResourceFunc&, const FReferencedResourceGameThreadTickFunc&);

        //! Different data packing strategies
        enum class TextureLayoutStrategy : uint8
        {
            //! Pack texture layouts without changing any scale
            Pack,

            //! Do not touch mesh or image texture layouts
            None,

            //! Helper value, not really a strategy.
            Count
        };

        //! 
        static const char* GetTextureLayoutStrategyName( TextureLayoutStrategy s );

		/** Output some stats about the complete compilation to the log. */
		void LogStats() const;

        //-----------------------------------------------------------------------------------------
        // Interface pattern
        //-----------------------------------------------------------------------------------------
        class Private;
        Private* GetPrivate() const;

        CompilerOptions( const CompilerOptions& ) = delete;
        CompilerOptions( CompilerOptions&& ) = delete;
        CompilerOptions& operator=(const CompilerOptions&) = delete;
        CompilerOptions& operator=(CompilerOptions&&) = delete;

    protected:

        //! Forbidden. Manage with the Ptr<> template.
        ~CompilerOptions() override;

    private:

        Private* m_pD;

    };


    //!
    struct FStateOptimizationOptions
    {
		uint8 FirstLOD = 0;
		uint8 NumExtraLODsToBuildAfterFirstLOD = 0;
		bool bOnlyFirstLOD = false;
		ETextureCompressionStrategy TextureCompressionStrategy = ETextureCompressionStrategy::None;

        void Serialise( OutputArchive& arch ) const
        {
            const int32_t ver = 4;
            arch << ver;

			arch << FirstLOD;
			arch << bOnlyFirstLOD;
            arch << TextureCompressionStrategy;
			arch << NumExtraLODsToBuildAfterFirstLOD;
        }


        void Unserialise( InputArchive& arch )
        {
            int32_t ver = 0;
            arch >> ver;
			check(ver <= 4);

			if (ver >= 2)
			{
				arch >> FirstLOD;
			}
			else
			{
				FirstLOD = 0;
			}

            arch >> bOnlyFirstLOD;

			if (ver >= 4)
			{
				arch >> TextureCompressionStrategy;
			}
			else
			{
				bool bAvoidRuntimeCompression;
				arch >> bAvoidRuntimeCompression;
				TextureCompressionStrategy = bAvoidRuntimeCompression ? ETextureCompressionStrategy::DontCompressRuntime : ETextureCompressionStrategy::None;
			}

			if (ver == 3)
			{
				int32 OldNumExtraLODsToBuildAfterFirstLOD;
				arch >> OldNumExtraLODsToBuildAfterFirstLOD;
				NumExtraLODsToBuildAfterFirstLOD = OldNumExtraLODsToBuildAfterFirstLOD;
			}
			else if (ver >= 4)
			{
				arch >> NumExtraLODsToBuildAfterFirstLOD;
			}
			else
			{
				NumExtraLODsToBuildAfterFirstLOD = 0;
			}

		}
    };


	//! Information about an object state in the source data
	struct FObjectState
	{
		//! Name used to identify the state from the code and user interface.
		FString m_name;

		//! GPU Optimisation options
		FStateOptimizationOptions m_optimisation;

		//! List of names of the runtime parameters in this state
		TArray<FString> m_runtimeParams;

		void Serialise( OutputArchive& arch ) const;


		void Unserialise( InputArchive& arch );
	};

	
    //! This utility class compiles two types of expressions: models and transforms.
    //! Model expressions are compiled into run-time usable objects.
    //! Transform expressions are compiled into Transform objects than can be applied to Model
    //! expressions.
    class MUTABLETOOLS_API Compiler : public RefCounted
    {
    public:

        Compiler( Ptr<CompilerOptions> Options=nullptr );

        //! Compile the expression into a run-time model.
		TSharedPtr<Model> Compile( const Ptr<Node>& pNode );

        //! Return the log of messages of all the compile operations executed so far.
        ErrorLogPtrConst GetLog() const;

    protected:

        //! Forbidden. Manage with the Ptr<> template.
        ~Compiler();

    private:

        class Private;
        Private* m_pD;

    };

}
