// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/PlatformMath.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"
#include "Templates/SharedPointer.h"

namespace mu
{
    // Forward declarations
    class Compiler;
    typedef Ptr<Compiler> CompilerPtr;
    typedef Ptr<const Compiler> CompilerPtrConst;

    class CompilerOptions;
    using CompilerOptionsPtr=Ptr<CompilerOptions>;
    using CompilerOptionsPtrConst=Ptr<const CompilerOptions>;

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

        //! If enabled, the disk will be used as temporary memory. This will make the compilation
        //! process very slow, but will be able to compile very large models.
        void SetUseDiskCache( bool bEnabled );

		//! Set the quality for the image compression algorithms. The level value is used internally
		//! with System::SetImagecompressionQuality
		void SetImageCompressionQuality(int32 quality);

		/** Set the image tiling strategy :
		 * If 0 (default) there is no tiling. Otherwise, images will be generated in tiles of the given size or less, and assembled afterwards as a final step.
		 */
		void SetImageTiling(int32 Tiling);

        //! 
        void SetDataPackingStrategy( int32 minRomSize, int32 MinTextureResidentMipCount );

		/** If enabled it will make sure that the object is compile to generate smaller mips of the images. */
		void SetEnableProgressiveImages(bool bEnabled);

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

        //! Return a readable string for a GPU.
        static const char* GetTextureLayoutStrategyName( TextureLayoutStrategy s );

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


    //! This utility class compiles two types of expressions: models and transforms.
    //! Model expressions are compiled into run-time usable objects.
    //! Transform expressions are compiled into Transform objects than can be applied to Model
    //! expressions.
    class MUTABLETOOLS_API Compiler : public RefCounted
    {
    public:

        Compiler( CompilerOptionsPtr options=nullptr );

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
