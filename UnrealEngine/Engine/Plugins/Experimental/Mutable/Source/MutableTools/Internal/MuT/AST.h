// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Platform.h"
#include "MuT/Compiler.h"
#include "MuR/Image.h"
#include "MuR/Mesh.h"
#include "MuR/ModelPrivate.h"
#include "MuR/MutableMath.h"
#include "MuR/MutableMemory.h"
#include "MuR/Operations.h"
#include "MuR/ParametersPrivate.h"
#include "MuR/Ptr.h"
#include "MuR/RefCounted.h"

#include "Containers/Array.h"
#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/Map.h"
#include "HAL/PlatformCrt.h"
#include "HAL/PlatformMath.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeHash.h"
#include "Templates/Function.h"
#include "Hash/CityHash.h"

#include <array>
#include <atomic>
#include <unordered_map>
#include <unordered_set>
#include <set>


namespace std
{

  template<typename T>
  struct hash<mu::Ptr<T>>
  {
    uint64 operator()(const mu::Ptr<T>& k) const
    {
      return hash<const void*>()(k.get());
    }
  };

}


namespace mu
{
	class ASTOp;
	class ASTOpFixed;
	struct FProxyFileContext;

	template<typename T>
	inline uint32 GetTypeHash(const Ptr<const T>& p)
	{
		return ::GetTypeHash(p.get());
	}


	template<typename T>
	inline uint32 GetTypeHash(const Ptr<T>& p)
	{
		return ::GetTypeHash(p.get());
	}


    //---------------------------------------------------------------------------------------------
    //! This class stores the expression that defines the size of an image.
    //--------------------------------------------------------------------------------------------
    class ImageSizeExpression : public RefCounted
    {
    public:

        enum
        {
            ISET_UNKNOWN,
            ISET_CONSTANT,
            ISET_LAYOUTFACTOR,
            ISET_CONDITIONAL
        } type;

        // For constant sizes
        FImageSize size = FImageSize(0, 0);

        // For layout factor sizes
        Ptr<class ASTOp> layout;
        uint16 factor[2];

        // For conditionals
        Ptr<class ASTOp> condition;
        Ptr<ImageSizeExpression> yes;
        Ptr<ImageSizeExpression> no;

        //!
        ImageSizeExpression()
        {
            type = ISET_UNKNOWN;
            size[0] = 0;
            size[1] = 0;
            layout = 0;
            factor[0] = 0;
            factor[1] = 0;
            condition = 0;
        }

        //!
        void CopyFrom(const ImageSizeExpression& o)
        {
            type = o.type;
            size = o.size;
            layout = o.layout;
            factor[0] = o.factor[0];
            factor[1] = o.factor[1];
            condition = o.condition;
            yes = o.yes;
            no = o.no;
        }

        //!
        bool operator==( const ImageSizeExpression& other ) const
        {
            if ( type == other.type )
            {
                switch (type)
                {
                case ISET_CONSTANT:
                    return size[0]==other.size[0]
                            && size[1] == other.size[1];

                case ISET_LAYOUTFACTOR:
                    return layout==other.layout
                            && factor[0]==other.factor[0]
                            && factor[1]==other.factor[1];

                case ISET_CONDITIONAL:
                    return condition==other.condition
                            && *yes==*(other.yes)
                            && *no==*(other.no);

                default:
                    return false;
                }
            }

            return false;
        }

        void Optimise()
        {
            switch (type)
            {
            case ISET_CONSTANT:
                break;

            case ISET_LAYOUTFACTOR:
                // TODO: See if the layout is constant and so is this expression.
                break;

            case ISET_CONDITIONAL:
                yes->Optimise();
                no->Optimise();

                // TODO: See if the condition is constant
                if ( *yes==*no )
                {
                    CopyFrom( *yes );
                }

            default:
                break;
            }
        }

    };


    typedef TArray<Ptr<ASTOp>> ASTOpList;
    typedef TSet<Ptr<ASTOp>> ASTOpSet;


    //! Detailed optimization flags
    struct FModelOptimizationOptions
    {
        bool bEnabled = true;
        bool bOptimiseOverlappedMasks = false;
        bool bConstReduction = true;

        //! Preprocess all mesh fragments so that they use the same skeleton, even if not all bones
        //! are relevant for all fragments.
        bool bUniformizeSkeleton = true;

        //! Maximum number of iterations when optimising models. If 0 as many as necessary will be performed.
        int32 MaxOptimisationLoopCount = 8;

        /** If valied, store resource data in disk instead of memory. */
		FProxyFileContext* DiskCacheContext = nullptr;

		/** Compile optimizing for the generation of smaller mipmaps of every image. */
		bool bEnableProgressiveImages = false;

        // Additional advanced fine-tuning parameters
        //---------------------------------------------------------------------

        //! Ratio used to decide if it is worth to generate a crop operation
        float AcceptableCropRatio = 0.5f;

        //! Ratio used to decide if it is worth to generate a crop operation
        float MinRLECompressionGain = 1.2f;

		// External resource provision functions
		//---------------------------------------------------------------------

		/** Function used to request an engine resource (image) from the compiler. */
		FReferencedResourceFunc ReferencedResourceProvider;

		/** Function used to ensure the provision of engine resources with ReferencedResourceProvider advances. */
		FReferencedResourceGameThreadTickFunc ReferencedResourceProviderTick;
	};


	struct FLinkerOptions
	{
		FLinkerOptions(FImageOperator& InImOp)
			: ImageOperator(InImOp)
		{
		}

		// TODO: Unused?
		int32 MinTextureResidentMipCount = 0;

		/** This flag controls the splitting of image data into mips to store separately. It is usually necessary to
		* be able to generate progressive textures (for texture streaming).
		*/
		bool bSeparateImageMips = true;

		/** Structure used to speedup mesh constant comparison. */
		struct FDeduplicationMeshFuncs : TDefaultMapHashableKeyFuncs<Ptr<const Mesh>, int32, false>
		{
			static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
			{
				return *A == *B;
			}

			static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
			{
				const mu::Mesh* Data = Key.get();
				return HashCombineFast(
					::GetTypeHash(Data->m_VertexBuffers.GetElementCount()),
					::GetTypeHash(Data->m_IndexBuffers.GetElementCount())
				);
			}
		};

		TMap<Ptr<const Mesh>, int32, FDefaultSetAllocator, FDeduplicationMeshFuncs> MeshConstantMap;

		/** Structure used to speedup image mip comparison. */
		struct FDeduplicationImageFuncs : TDefaultMapHashableKeyFuncs<Ptr<const Image>, int32, false>
		{
			static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
			{
				return *A == *B;
			}

			static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
			{
				const mu::Image* Data = Key.get();
				uint32 Hash = HashCombineFast(::GetTypeHash(Data->GetFormat()), GetTypeHash(Data->GetSize()));

				TArrayView<const uint8> DataView = Data->DataStorage.GetLOD(0);
				uint64 DataHash = CityHash64(reinterpret_cast<const char*>(DataView.GetData()), DataView.Num());
				Hash = HashCombineFast(Hash, ::GetTypeHash(DataHash));

				return Hash;
			}
		};

		TMap<Ptr<const Image>, int32, FDefaultSetAllocator, FDeduplicationImageFuncs> ImageConstantMipMap;

		/** Image operation functions, so that they can be overriden. */
		FImageOperator& ImageOperator;
	};


	//! For each operation we sink, the map from old instructions to new instructions.
	struct FSinkerOldToNewKey
	{
		Ptr<const ASTOp> Op;
		Ptr<const ASTOp> SinkingOp;

		friend inline uint32 GetTypeHash(const FSinkerOldToNewKey& Key)
		{
			return HashCombineFast(::GetTypeHash(Key.Op.get()), ::GetTypeHash(Key.SinkingOp.get()));
		}

		friend inline bool operator==(const FSinkerOldToNewKey& A, const FSinkerOldToNewKey& B)
		{
			return A.Op==B.Op && A.SinkingOp==B.SinkingOp;
		}
	};


	class Sink_ImageCropAST
	{
	public:

		Ptr<ASTOp> Apply(const class ASTOpImageCrop* root);

	protected:

		const class ASTOpImageCrop* m_root = nullptr;
		Ptr<ASTOp> m_initialSource;
		//! For each operation we sink, the map from old instructions to new instructions.
		TMap<FSinkerOldToNewKey, Ptr<ASTOp>> OldToNew;

		Ptr<ASTOp> Visit(Ptr<ASTOp> at, const class ASTOpImageCrop* currentCropOp);
	};

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class Sink_ImagePixelFormatAST
	{
	public:

		Ptr<ASTOp> Apply(const class ASTOpImagePixelFormat* root);

	protected:

		const class ASTOpImagePixelFormat* m_root = nullptr;
		Ptr<ASTOp> m_initialSource;
		TMap<FSinkerOldToNewKey, Ptr<ASTOp>> OldToNew;

		Ptr<ASTOp> Visit(Ptr<ASTOp> at, const class ASTOpImagePixelFormat* currentFormatOp);
	};

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class Sink_ImageSwizzleAST
	{
	public:

		Ptr<ASTOp> Apply(const class ASTOpImageSwizzle* Root);

	protected:

		const class ASTOpImageSwizzle* Root = nullptr;
		Ptr<ASTOp> InitialSource;
		TMap<FSinkerOldToNewKey, Ptr<ASTOp>> OldToNew;

		Ptr<ASTOp> Visit(Ptr<ASTOp> at, const class ASTOpImageSwizzle* CurrentSwizzleOp);
	};


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class Sink_MeshFormatAST
	{
	public:

		// \TODO This is recursive and may cause stack overflows in big models.
		Ptr<ASTOp> Apply(const class ASTOpMeshFormat* Root);

	protected:

		const class ASTOpMeshFormat* m_root = nullptr;
		Ptr<ASTOp> m_initialSource;
		TMap<FSinkerOldToNewKey, Ptr<ASTOp>> OldToNew;

		Ptr<ASTOp> Visit(const Ptr<ASTOp>& at, const class ASTOpMeshFormat* currentFormatOp);
	};


	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
	class Sink_ImageMipmapAST
	{
	public:

		Ptr<ASTOp> Apply(const class ASTOpImageMipmap* Root);

	protected:

		const class ASTOpImageMipmap* m_root = nullptr;
		Ptr<ASTOp> m_initialSource;

		//! For each operation we sink, the map from old instructions to new instructions.
		TMap<FSinkerOldToNewKey, Ptr<ASTOp>> OldToNew;

		Ptr<ASTOp> Visit(Ptr<ASTOp> at, const class ASTOpImageMipmap* currentMipmapOp);

	};

	struct FOptimizeSinkContext
	{
		Sink_ImageCropAST ImageCropSinker;
		Sink_ImagePixelFormatAST ImagePixelFormatSinker;
		Sink_ImageSwizzleAST ImageSwizzleSinker;
		Sink_ImageMipmapAST ImageMipmapSinker;
		Sink_MeshFormatAST MeshFormatSinker;
	};


    //!
    class ASTChild
    {
    public:
        explicit ASTChild(ASTOp* parent, const Ptr<ASTOp>& child=Ptr<ASTOp>());
        ASTChild(const Ptr<ASTOp>& parent, const Ptr<ASTOp>& child);
        ~ASTChild();

        ASTChild(const ASTChild&) = delete;
        ASTChild& operator=(const ASTChild&) = delete;

        // move constructor
        ASTChild(ASTChild&& rhs)
             : m_parent(rhs.m_parent)
             , m_child(rhs.m_child)
             , m_parentIndexInChild(rhs.m_parentIndexInChild)
        {
            rhs.m_parent=nullptr;
            rhs.m_child.reset();
        }

        // Move assignment
        ASTChild& operator=( ASTChild&& rhs );

        ASTChild& operator=( const Ptr<ASTOp>& c );

        inline explicit operator bool() const
        {
            return m_child.get()!=nullptr;
        }

        inline Ptr<class ASTOp>& child()
        {
            return m_child;
        }

        inline const Ptr<class ASTOp>& child() const
        {
            return m_child;
        }

        inline const Ptr<class ASTOp>& operator->() const
        {
            return m_child;
        }

        inline bool operator==(const ASTChild& o) const
        {
            return m_child==o.m_child;
        }

        class ASTOp* m_parent;
        Ptr<class ASTOp> m_child;
        int32 m_parentIndexInChild = 0;

    private:

        inline void AddParent();

        inline void ClearParent();
    };


    //---------------------------------------------------------------------------------------------
    //! Abstract Syntax Tree of operations in the mutable virtual machine.
    //! Avoid any kind of recursivity here, since the hierarchy can be very deep, and it will
    //! easily cause stack overflows with production models.
    //---------------------------------------------------------------------------------------------
    class ASTOp : public RefCounted
    {
    private:

        //! Operations referring to this one. They may be null: elements are never removed
        //! from this TArray.
		TArray<ASTOp*, TInlineAllocator<4> > m_parents;

    public:

        inline ASTOp()
        {
			bIsConstantSubgraph = false;
			bHasSpecialOpInSubgraph = false;
        }

        virtual ~ASTOp() {}

        //! Get the operation type
        virtual OP_TYPE GetOpType() const = 0;

        //! Validate that everything is fine with this tree
        virtual void Assert();

        //! Run something for each child operation, with a chance to modify it.
        virtual void ForEachChild( const TFunctionRef<void(ASTChild&)> ) = 0;

        //! Run something for each parent operation+.
		void ForEachParent(const TFunctionRef<void(ASTOp*)>) const;

        //! Run something for each child operation, with a chance to modify it.
        virtual bool operator==( const ASTOp& other ) const;

        //! Hint hash method for op sorting and containers. It's a hash of the actual operation.
        virtual uint64 Hash() const = 0;

        //! Shallow clone. New node will have no parents but reference to the same children.
		using MapChildFunc = TFunction<Ptr<ASTOp>(const Ptr<ASTOp>&)>;
		using MapChildFuncRef = TFunctionRef<Ptr<ASTOp>(const Ptr<ASTOp>&)>;
		virtual Ptr<ASTOp> Clone(MapChildFuncRef mapChild) const = 0;

		//
		virtual bool IsConditional() const { return false; }
		virtual bool IsSwitch() const { return false; }

    protected:

        void RemoveChildren();

        virtual bool IsEqual(const ASTOp& other) const = 0;

    public:

        //---------------------------------------------------------------------------------------------
        static void FullAssert( const TArray<Ptr<ASTOp>>& roots );

        static int32 CountNodes( const TArray<Ptr<ASTOp>>& roots );

		inline bool IsConstantOp() const
		{
			OP_TYPE Type = GetOpType();
			return Type == OP_TYPE::BO_CONSTANT
				|| Type == OP_TYPE::NU_CONSTANT
				|| Type == OP_TYPE::SC_CONSTANT
				|| Type == OP_TYPE::CO_CONSTANT
				|| Type == OP_TYPE::IM_CONSTANT
				|| Type == OP_TYPE::ME_CONSTANT
				|| Type == OP_TYPE::LA_CONSTANT
				|| Type == OP_TYPE::PR_CONSTANT
				|| Type == OP_TYPE::ST_CONSTANT
				|| Type == OP_TYPE::ED_CONSTANT
				;
		}

        //! Deep clone. New node will have no parents and reference new children
        static Ptr<ASTOp> DeepClone( const Ptr<ASTOp>& );

		//!
		static void LogHistogram(ASTOpList& roots);

        // Code optimisation methods
        //---------------------------------------------------------------------------------------------

        //!
        virtual Ptr<ASTOp> OptimiseSize() const { return nullptr; }
        virtual Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions&, int32 Pass) const { return nullptr; }
        virtual Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext& ) const { return nullptr; }
        virtual Ptr<ImageSizeExpression> GetImageSizeExpression() const
        {
            check( false );
            return nullptr;
        }


        // Code linking
        //---------------------------------------------------------------------------------------------

		/** Convert the operation graph at Root into code in the given program.
		* Potentially destroys the data in this operation, so it shouldn't be used after calling Link.
		*/
		static OP::ADDRESS FullLink( Ptr<ASTOp>& Root, FProgram&, FLinkerOptions* );

    private:

        /** Convert this operation into code in the given program. 
		* It assumes children have been linked already
		* Potentially destroys the data in this operation, so it shouldn't be used after calling Link. 
		*/
        virtual void Link( FProgram&, FLinkerOptions* ) = 0;

    protected:

        //---------------------------------------------------------------------------------------------
        //!
        //---------------------------------------------------------------------------------------------
        struct FRangeData
        {
            //!
            ASTChild rangeSize;

            //!
            FString rangeName;

            //!
			FString rangeUID;

            //!
            FRangeData( ASTOp* parentOp, Ptr<ASTOp> childOp, const FString& name, const FString& uid )
                : rangeSize( parentOp, childOp )
                , rangeName(name)
                , rangeUID(uid)
            {
            }

            FRangeData(const FRangeData&) = delete;
            FRangeData& operator=(const FRangeData&) = delete;
            FRangeData& operator=(FRangeData&&) = delete;


            // move constructor
            FRangeData(FRangeData&& rhs)
                 : rangeSize(MoveTemp(rhs.rangeSize))
                 , rangeName(MoveTemp(rhs.rangeName))
                 , rangeUID(MoveTemp(rhs.rangeUID))
            {
            }

            //!

            //!
            bool operator==(const FRangeData& o) const
            {
                return rangeSize==o.rangeSize
                        &&
                        rangeName==o.rangeName
                        &&
                        rangeUID==o.rangeUID;
            }
        };

        //!
		static void LinkRange(FProgram& program,
			const FRangeData& range,
			OP::ADDRESS& rangeSize,
			uint16& rangeId);

    public:

        // Generic traversals
        //---------------------------------------------------------------------------------------------

        //!
        static void Traverse_TopDown_Unique( const TArray<Ptr<ASTOp>>& roots, TFunctionRef<bool(Ptr<ASTOp>&)> f );

        //! \todo: it is not strictly top down.
        static void Traverse_TopDown_Unique_Imprecise( const TArray<Ptr<ASTOp>>& roots, TFunctionRef<bool(Ptr<ASTOp>&)> f );

        //! Kind of top-down, but really not.
        //! This version is slighlty faster, but doesn't support recursive traversals so
        //! use it only in controlled cases.
        static void Traverse_TopRandom_Unique_NonReentrant
            (
                const TArray<Ptr<ASTOp>>& roots,
				TFunctionRef<bool(Ptr<ASTOp>&)> f
            );

        //! Kind of top-down, but really not.
        //! This version is slighlty faster, but doesn't support recursive traversals so
        //! use it only in controlled cases.
        template<typename STATE>
        static inline void Traverse_TopDown_Unique_Imprecise_WithState
            (
                Ptr<ASTOp>& root, const STATE& initialState,
				TFunctionRef<bool(Ptr<ASTOp>&, STATE&, TArray<TPair<Ptr<ASTOp>,STATE>>&)> f
            )
        {
            if (!root) { return; }

            TArray<TPair<Ptr<ASTOp>,STATE>> pending;
			pending.Emplace( root, initialState );


            struct custom_partial_hash
            {
				uint64 operator()(const std::pair<Ptr<ASTOp>,STATE>& k) const
                {
                    return std::hash<const void*>()(k.first.get());
                }
            };

            std::unordered_set<std::pair<Ptr<ASTOp>,STATE>, custom_partial_hash> traversed;

            while (!pending.IsEmpty())
            {
				TPair<Ptr<ASTOp>,STATE> current = pending.Pop();

                // It could have been completed in another branch
				auto iti = traversed.insert({ current.Key,current.Value });
                if (iti.second)
                {
                    // Process. State in current.Value may change
                    bool recurse = f(current.Key,current.Value,pending);

                    // Recurse children
                    if (recurse)
                    {
                        current.Key->ForEachChild([&]( ASTChild& c )
                        {
							std::pair<Ptr<ASTOp>, STATE> TraverseKey(c.m_child, current.Value);
							if (c.m_child && !traversed.count(TraverseKey))
                            {
								pending.Emplace( c.m_child, current.Value );
                            }
                        });
                    }
                }
            }
        }


        //! This version is slighlty faster, but doesn't support recursive traversals so
        //! use it only in controlled cases.
        static void Traverse_BottomUp_Unique_NonReentrant( ASTOpList& roots, TFunctionRef<void(Ptr<ASTOp>&)> f );

        //! This version is slighlty faster, but doesn't support recursive traversals so
        //! use it only in controlled cases.
        static void Traverse_BottomUp_Unique_NonReentrant
        (
            ASTOpList& roots,
			TFunctionRef<void(Ptr<ASTOp>&)> f,
			TFunctionRef<bool(const ASTOp*)> accept
        );

        //!
        static void Traverse_BottomUp_Unique
        (
            ASTOpList& roots,
			TFunctionRef<void(Ptr<ASTOp>&)> f,
			TFunctionRef<bool(const ASTOp*)> accept = [](const ASTOp*){return true;}
        );

        //!
        static void Traverse_BottomUp_Unique
        (
            Ptr<ASTOp>& root,
			TFunctionRef<void(Ptr<ASTOp>&)> f,
			TFunctionRef<bool(const ASTOp*)> accept = [](const ASTOp*){return true;}
        );

        //!
        OP::ADDRESS linkedAddress = 0;

        //! Generic traverse control counter. It should always be left to 0 after any process for all
        //! nodes in the hierarchy.
        uint32 m_traverseIndex = 0;
        static std::atomic<uint32> s_lastTraverseIndex;

        //!
        int8 linkedRange = -1;

        /** Embedded node data for the constant subtree detection.This flag is only valid if the
         * constant detection process has been executed and no relevant AST transformations have
         * happened. */
        uint8 bIsConstantSubgraph : 1;
        uint8 bHasSpecialOpInSubgraph : 1;

    private:
        friend class ASTChild;

        //! Remove a node from the parent list if it is there.
        //void RemoveParent(const ASTOp* parent);

    public:

		/** Get the number of ASTOps that have this one as child, in any existing AST graph. */
        int32 GetParentCount() const;

        //! Make all parents of this node point at the other node instead.
        static void Replace( const Ptr<ASTOp>& node, const Ptr<ASTOp>& other );

        // Other code generation utilities
        //---------------------------------------------------------------------------------------------

        //! This class contains the support data to accelerate the GetImageDesc recursive function.
        //! If none is provided in the call, one will be created at that level and used from there on.
        class FGetImageDescContext
        {
        public:
            TMap<const ASTOp*, FImageDesc> m_results;
        };

        //!
        virtual FImageDesc GetImageDesc( bool returnBestOption=false, FGetImageDescContext* context=nullptr ) const;

        //! Optional cache struct to use int he method below.
        using FBlockLayoutSizeCache=TMap< const TPair<ASTOp*,int>, TPair<int,int>>;

        //! Return the size in layout blocks of a particular block given by absolute index
        virtual void GetBlockLayoutSize( int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache* cache );
        void GetBlockLayoutSizeCached( int blockIndex, int* pBlockX, int* pBlockY, FBlockLayoutSizeCache* cache );


        //! Return the size in pixels of the layout grid block for the image operation
        virtual void GetLayoutBlockSize( int* pBlockX, int* pBlockY );

        virtual bool IsImagePlainConstant(FVector4f& colour ) const;
        virtual bool IsColourConstant(FVector4f& colour ) const;

        //!
        virtual bool GetNonBlackRect( FImageRect& maskUsage ) const;

        // Logic expression evaluation
        //---------------------------------------------------------------------------------------------
        typedef enum
        {
            BET_UNKNOWN,
            BET_TRUE,
            BET_FALSE,
        } FBoolEvalResult;


        using FEvaluateBoolCache = std::unordered_map<const ASTOp*,FBoolEvalResult>;

        virtual FBoolEvalResult EvaluateBool( ASTOpList& /*facts*/, FEvaluateBoolCache* = nullptr ) const
        {
            check(false);
            return BET_UNKNOWN;
        }

        virtual int EvaluateInt( ASTOpList& /*facts*/, bool &unknown ) const
        {
            check(false);
            unknown = true;
            return 0;
        }

    };

    template<class DERIVED>
    inline Ptr<DERIVED> Clone( const ASTOp* s )
    {
		ASTOp::MapChildFunc Identity = [](const Ptr<ASTOp>& o) {return o; };
		Ptr<ASTOp> c = s->Clone(Identity);
		Ptr<DERIVED> t = static_cast<DERIVED*>(c.get());
        return t;
    }

    template<class DERIVED>
    inline Ptr<DERIVED> Clone( const Ptr<const ASTOp>& s )
    {
		ASTOp::MapChildFunc Identity = [](const Ptr<ASTOp>& o) {return o; };
		Ptr<ASTOp> c = s->Clone(Identity);
        Ptr<DERIVED> t = static_cast<DERIVED*>(c.get());
		return t;
    }

    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    template<typename STATE>
    class Visitor_TopDown_Unique_Const
    {
    private:

        //!
        virtual bool Visit( const Ptr<ASTOp>& node ) = 0;

    private:

        //! States found so far
        TArray<STATE> m_states;

        //! Index of the current state, from the m_states TArray.
        int m_currentState;

        struct PENDING
        {
            PENDING()
            {
                stateIndex = 0;
            }
            PENDING(Ptr<ASTOp> _at, int _stateIndex)
            {
                at = _at;
                stateIndex = _stateIndex;
            }

            Ptr<ASTOp> at;
            int stateIndex;
        };

        TArray<PENDING> m_pending;

        //! List of traversed nodes with the state in which they were traversed.
        std::multimap<Ptr<ASTOp>,int> m_traversed;

    protected:

        //!
        const STATE& GetCurrentState() const
        {
            return m_states[m_currentState];
        }

        //!
        STATE GetDefaultState() const
        {
            return m_states[0];
        }

        //! For manual recursion that changes the state for a specific path.
        void RecurseWithState(const Ptr<ASTOp>& at, const STATE& newState)
        {
            if(at)
            {
                auto it = m_states.Find(newState);
                if (it==INDEX_NONE)
                {
                    m_states.Add(newState);
                }
                int stateIndex = m_states.Find(newState);

                m_pending.Add( PENDING(at,stateIndex) );
            }
        }

        //! For manual recursion that doesn't change the state for a specific path.
        void RecurseWithCurrentState(const Ptr<ASTOp>& at)
        {
            if(at)
            {
                m_pending.Emplace( at, m_currentState );
            }
        }

        //! Can be called from visit to set the state to visit all children ops
        void SetCurrentState(const STATE& newState)
        {
            auto it = m_states.Find(newState);
            if (it==INDEX_NONE)
            {
                m_states.Add(newState);
            }
            m_currentState = m_states.Find(newState);
        }

    public:

        //! Ensure virtual destruction
        virtual ~Visitor_TopDown_Unique_Const() {}

        //!
        void Traverse(const ASTOpList& roots, const STATE& initialState)
        {
            m_pending.Empty();
            m_states.Empty();
            m_states.Add(initialState);
            m_currentState = 0;

            for( const auto& r: roots)
            {
                if (r)
                {
                    m_pending.Add( PENDING(r,m_currentState) );
                }
            }

            while (m_pending.Num())
            {
                PENDING item = m_pending.Pop();

                auto thisNodeRange = m_traversed.equal_range(item.at);
                bool visitedInThisState = false;
                for (auto it=thisNodeRange.first; it!=thisNodeRange.second; ++it)
                {
                    if (it->second==item.stateIndex)
                    {
                        visitedInThisState = true;
                        break;
                    }
                }

                // It could have been completed in another branch
                if (!visitedInThisState)
                {
                    m_traversed.insert( std::make_pair(item.at,item.stateIndex) );

                    // Process
                    m_currentState = item.stateIndex;
                    bool recurse = Visit(item.at);

                    // Recurse children
                    if (recurse)
                    {
                        item.at->ForEachChild([&]( ASTChild& c )
                        {
                            if (c)
                            {
                                m_pending.Add( PENDING(c.child(),m_currentState) );
                            }
                        });
                    }
                }
            }
        }
    };


    //---------------------------------------------------------------------------------------------
    //! Stateless top-down code visitor that can change the instructions. Iterative version.
    //! Once an instruction has changed, all the chain of instructions up to the root will be
    //! cloned, referencing the new instruction.
    //---------------------------------------------------------------------------------------------
    class Visitor_TopDown_Unique_Cloning
    {
    public:

        //! Ensure virtual destruction
        virtual ~Visitor_TopDown_Unique_Cloning() {}

    protected:

        //! Do the actual work by overriding this in the derived classes.
        virtual Ptr<ASTOp> Visit( Ptr<ASTOp> at, bool& processChildren ) = 0;


        void Traverse( Ptr<ASTOp>& root );

    private:

        //! Operations to be processed
        TArray< TPair<bool,Ptr<ASTOp>> > m_pending;

        //! Map for visited operations
        std::unordered_map<Ptr<ASTOp>,Ptr<ASTOp>> m_oldToNew;

        //!
        inline Ptr<ASTOp> GetOldToNew( const Ptr<ASTOp>& old ) const
        {
            Ptr<ASTOp> n;
            auto it = m_oldToNew.find(old);
            if (it!=m_oldToNew.end())
            {
                n = it->second;
            }

            it = m_oldToNew.find(n);
            while (it!=m_oldToNew.end() && it->second.get()!=nullptr && it->second!=n)
            {
                n = it->second;
                it = m_oldToNew.find(n);
            }

            return n;
        }

        //! Process all the pending operations and visit all children if necessary
        void Process();

    };


    //---------------------------------------------------------------------------------------------
    //! This class of operation is used to ease the transition from legacy code generation.
    //---------------------------------------------------------------------------------------------
    class ASTOpFixed : public ASTOp
    {
    public:

        //! Fixed size op structure. Any code ADDRESS in it, refers to the children TArray below.
        OP op;

        //! Operations used by this one. To be indexed with the ADDRESSes in the m_op.
        //TArray<ASTChild> children;
        std::array<ASTChild,8> children;
        uint8 childCount = 1;

    public:
        ASTOpFixed();

        ~ASTOpFixed() override;

        OP_TYPE GetOpType() const override { return op.type; }

        Ptr<ASTOp> Clone( MapChildFuncRef mapChild ) const override;
        void ForEachChild( const TFunctionRef<void(ASTChild&)> f ) override;
        void Link( FProgram& program, FLinkerOptions* Options) override;
        bool IsEqual(const ASTOp& otherUntyped) const override;
		uint64 Hash() const override;
		FImageDesc GetImageDesc( bool returnBestOption, FGetImageDescContext* context ) const override;
        void GetLayoutBlockSize( int* pBlockX, int* pBlockY ) override;
        Ptr<ASTOp> OptimiseSize() const override;
        Ptr<ASTOp> OptimiseSemantic(const FModelOptimizationOptions&, int32 Pass) const override;
        Ptr<ASTOp> OptimiseSink(const FModelOptimizationOptions&, FOptimizeSinkContext&) const override;
        FBoolEvalResult EvaluateBool( ASTOpList& facts, FEvaluateBoolCache* cache ) const override;
        int EvaluateInt( ASTOpList& facts, bool &unknown ) const override;
        bool IsImagePlainConstant(FVector4f& colour ) const override;
        bool IsColourConstant(FVector4f& colour ) const override;
        Ptr<ImageSizeExpression> GetImageSizeExpression() const override;

        //---------------------------------------------------------------------------------------------
        //! Add child for OP fixed nodes.
        //---------------------------------------------------------------------------------------------
        inline void SetChild( OP::ADDRESS& at, const Ptr<ASTOp>& child )
        {
            // hack check
            check( ((uint8*)&at)>=((uint8*)&op.args)
                            &&
                            ((uint8*)&at)<=((uint8*)&op.args)+sizeof(op.args));

            if (!at)
            {
                if (child)
                {
                    check(childCount<8);
                    at = (OP::ADDRESS)childCount++;
                    children[at] = child;
                }
            }
            else
            {
                children[at]=child;
                if (!child)
                {
                    at=0;
                }
            }
        }

        inline void SetChild( OP::ADDRESS& at, const ASTChild& child )
        {
            SetChild(at,child.child());
        }

    private:

        //
        inline FImageDesc GetImageDesc( OP::ADDRESS at, bool returnBestOption=false, FGetImageDescContext* context=nullptr ) const
        {
			FImageDesc res;

            if (children[at])
            {
                res = children[at]->GetImageDesc( returnBestOption, context );
            }

            return res;
        }

        inline void GetLayoutBlockSize( OP::ADDRESS at, int* pBlockX, int* pBlockY )
        {
            if (children[at])
            {
                children[at]->GetLayoutBlockSize(pBlockX,pBlockY);
            }
        }

    };


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    inline void ASTChild::AddParent()
    {
        m_parentIndexInChild = m_child->m_parents.Num();
        m_child->m_parents.Add(m_parent);
    }

    //---------------------------------------------------------------------------------------------
    inline void ASTChild::ClearParent()
    {
        check( m_parentIndexInChild<m_child->m_parents.Num() );
		// Can't do this, because the indices are stored in children.
		//m_child->m_parents.RemoveAtSwap(m_parentIndexInChild);
		m_child->m_parents[m_parentIndexInChild] = nullptr;
	}


    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    //---------------------------------------------------------------------------------------------
    class UniqueOpPool
    {
    private:
        struct op_pointer_hash
        {
			uint64 operator() (const Ptr<ASTOp>& n) const
            {
                return n->Hash();
            }
        };

        struct op_pointer_equal
        {
            bool operator() (const Ptr<ASTOp>& a, const Ptr<ASTOp>& b) const
            {
                return *a==*b;
            }
        };

        // Existing ops, per type
        std::unordered_set<Ptr<ASTOp>,op_pointer_hash,op_pointer_equal> visited[(int)OP_TYPE::COUNT];

    public:

        bool m_disabled = false;

        Ptr<ASTOp> Add( const Ptr<ASTOp>& n )
        {
            if (m_disabled) return n;

            if (!n) return nullptr;

            auto& container = visited[(int)n->GetOpType()];
            auto it = container.insert(n);
            if( !it.second )
            {
                return *it.first;
            }
            else
            {
                return n;
            }
        }
    };

}

