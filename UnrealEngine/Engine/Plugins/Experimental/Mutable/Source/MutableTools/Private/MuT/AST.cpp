// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuT/AST.h"

#include "Containers/Set.h"
#include "Containers/Queue.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "MuR/Mesh.h"
#include "MuR/MutableTrace.h"
#include "MuR/Platform.h"
#include "MuT/ASTOpConstantResource.h"
#include "MuT/ASTOpMeshRemoveMask.h"
#include "MuT/ASTOpImageMultiLayer.h"
#include "MuT/ASTOpParameter.h"
#include "MuT/StreamsPrivate.h"
#include "MuT/Platform.h"
#include "MuT/ErrorLogPrivate.h"
#include "Trace/Detail/Channel.h"

#include <atomic>


std::atomic<uint32> mu::ASTOp::s_lastTraverseIndex(1);


namespace mu
{

//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ASTChild::ASTChild(ASTOp* p, const Ptr<ASTOp>& c)
    : m_parent(p)
    , m_child(c)
{
    if (m_parent && m_child.get())
    {
        AddParent();
    }
}

ASTChild::ASTChild( const Ptr<ASTOp>& p, const Ptr<ASTOp>& c)
    : ASTChild(p.get(),c)
{
}

ASTChild::~ASTChild()
{
    if (m_child && m_parent)
    {
        ClearParent();
    }
}

ASTChild& ASTChild::operator=( const Ptr<ASTOp>& c )
{
    if (c!=m_child)
    {
        if (m_child && m_parent)
        {
            ClearParent();
        }

        m_child = c;

        if (m_child && m_parent)
        {
            AddParent();
        }
    }

    return *this;
}

ASTChild& ASTChild::operator=( ASTChild&& rhs )
{
    m_parent = rhs.m_parent;
    m_parentIndexInChild = rhs.m_parentIndexInChild;
    m_child = rhs.m_child;
    rhs.m_parent=nullptr;
    rhs.m_child.reset();

    return *this;
}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ASTOp::ForEachParent(const TFunctionRef<void(ASTOp*)> f) const
{
	for (ASTOp* p : m_parents)
	{
		if (p)
		{
			f(p);
		}
	}
}


//-------------------------------------------------------------------------------------------------
void ASTOp::RemoveChildren()
{
    // Actually destroyed when running out of scope
    TArray<Ptr<ASTOp>> toDestroy;

    // Try to make children destruction iterative
    TArray<ASTOp*> pending;
	pending.Reserve(1024);
    pending.Add(this);

    while (pending.Num())
    {
        ASTOp* n = pending.Pop(EAllowShrinking::No);

        n->ForEachChild( [&](ASTChild& c)
        {
            if (c)
            {
                // Are we clearing the last reference?
                if (c.child()->IsUnique())
                {
                    toDestroy.Add(c.child());
                    pending.Add(c.child().get());
                }

                c = nullptr;
            }
        });
    }
}


//-------------------------------------------------------------------------------------------------
void ASTOp::Assert()
{
    // Check that every valid parent has us a child
    // TODO: Should count the numbers, since a node may be child of another in multiple connections.
    ForEachParent( [&](ASTOp*parent)
    {
        if(parent)
        {
            bool foundInParent = false;
            parent->ForEachChild([&](ASTChild&c)
            {
                if (c && c.m_child.get()==this)
                {
                    foundInParent = true;
                }
            });

            // If we hit this, we have a parent that doesn't know us.
            check(foundInParent);
        }
    });

    // Validate the children
	ForEachChild( [this](ASTChild&c)
    {
        if(c)
        {
            // The child must have us as the parent.
//            bool found = false;
//            c.child()->ForEachParent( [&](ASTOp* childParent)
//            {
//                if (childParent==this)
//                {
//                    found = true;
//                }
//            });
//            check(found);
            check( c.m_parentIndexInChild < size_t(c.child()->m_parents.Num()) );
            check( c.child()->m_parents[c.m_parentIndexInChild]==this );
        }
     });
}


//-------------------------------------------------------------------------------------------------
bool ASTOp::operator==( const ASTOp& other ) const
{
//    if (typeid(*this) != typeid(other))
//        return false;

    return IsEqual(other);
}


//---------------------------------------------------------------------------------------------
void ASTOp::FullAssert( const TArray<Ptr<ASTOp>>& Roots)
{
    MUTABLE_CPUPROFILER_SCOPE(AST_FullAssert);
    Traverse_TopDown_Unique_Imprecise(Roots, [](const Ptr<ASTOp>& n)
    {
        n->Assert();
        return true;
    });
}

//-------------------------------------------------------------------------------------------------
int32 ASTOp::CountNodes( const TArray<Ptr<ASTOp>>& Roots )
{
    MUTABLE_CPUPROFILER_SCOPE(AST_CountNodes);
	int32 Count=0;
    Traverse_TopRandom_Unique_NonReentrant(Roots, [&](const Ptr<ASTOp>&)
    {
        ++Count;
        return true;
    });
    return Count;
}


//-------------------------------------------------------------------------------------------------
mu::Ptr<ASTOp> ASTOp::DeepClone( const Ptr<ASTOp>& root )
{
    MUTABLE_CPUPROFILER_SCOPE(AST_DeepClone);

    std::unordered_map<Ptr<const ASTOp>,Ptr<ASTOp>> visited;

    MapChildFunc m = [&](const Ptr<ASTOp>&n)
    {
		if (!n)
		{
			return Ptr<ASTOp>();
		}
		std::unordered_map<Ptr<const ASTOp>, Ptr<ASTOp>>::const_iterator it = visited.find(n);
        check(it!=visited.end());
        return it->second;
    };

    Ptr<ASTOp> r = root;
    Traverse_BottomUp_Unique( r, [&](Ptr<ASTOp> n)
    {
        Ptr<ASTOp> cloned = n->Clone( m );
        visited[n] = cloned;
    });

	std::unordered_map<Ptr<const ASTOp>, Ptr<ASTOp>>::const_iterator it = visited.find(r);
    check(it!=visited.end());
    return it->second;
}


//-------------------------------------------------------------------------------------------------
OP::ADDRESS ASTOp::FullLink( Ptr<ASTOp>& Root, FProgram& Program, FLinkerOptions* Options )
{
    MUTABLE_CPUPROFILER_SCOPE(AST_FullLink);

    Traverse_BottomUp_Unique( Root,
                              [&](Ptr<ASTOp> n){ n->Link(Program, Options); },
                              [&](Ptr<const ASTOp> n){ return n->linkedAddress==0; });

	OP::ADDRESS Result = Root->linkedAddress;

	// This signals the caller that the Root pointer shouldn't be used anymore.
	Root = nullptr;

	return Result;
}


//---------------------------------------------------------------------------------------------
void ASTOp::LogHistogram( ASTOpList& roots )
{
    (void)roots;

 //   uint64 CountPerType[(int)OP_TYPE::COUNT];
 //   FMemory::Memzero(CountPerType,sizeof(CountPerType));

 //   size_t count=0;

 //   Traverse_TopRandom_Unique_NonReentrant( roots,
 //                            [&](const Ptr<ASTOp>& n)
 //   {
 //       ++count;
	//	CountPerType[(int)n->GetOpType()]++;
 //       return true;
 //   });

	//TArray< TPair<uint64, OP_TYPE> > Sorted;
	//Sorted.SetNum((int)OP_TYPE::COUNT);
 //   for (int i=0; i<(int)OP_TYPE::COUNT; ++i)
 //   {
 //       Sorted[i].Value = (OP_TYPE)i;
 //       Sorted[i].Key = CountPerType[i];
 //   }


	//Sorted.Sort( [](const TPair<uint64, OP_TYPE>& a, const TPair<uint64, OP_TYPE>& b)
 //   {
 //       return a.Key >b.Key;
 //   });

 //   UE_LOG(LogMutableCore,Log, TEXT("Op histogram (%llu ops):"), count);
 //   for(int i=0; i<8; ++i)
 //   {
 //       float p = Sorted[i].Key/float(count)*100.0f;
 //       int OpType = (int)Sorted[i].Value;
 //       UE_LOG(LogMutableCore, Log, TEXT("  %5.2f%% : %s"), p, s_opNames[OpType] );
 //   }

	//// Debug log part of the tree
	//Traverse_TopDown_Unique( roots,
	//	[&](const Ptr<ASTOp>& n)
	//	{
	//		if (n->GetOpType() == OP_TYPE::IM_MULTILAYER)
	//		{
	//			Ptr<const ASTOpImageMultiLayer> Typed = static_cast<const ASTOpImageMultiLayer*>(n.get());
	//			Ptr<const ASTOp> Base = Typed->base.child();
	//			Ptr<const ASTOpConstantResource> Constant = static_cast<const ASTOpConstantResource*>(Base.get());

	//			// Log the op
	//			UE_LOG(LogMutableCore, Log, TEXT("Multilayer at %x:"), n.get());
	//			UE_LOG(LogMutableCore, Log, TEXT("    base is type %s:"), s_opNames[(int)Base->GetOpType()]);
	//			if (Constant)
	//			{
	//				int Format = int( ((mu::Image*)Constant->GetValue().get())->GetFormat() );
	//				UE_LOG(LogMutableCore, Log, TEXT("    constant image format is %d:"), Format);
	//			}
	//			else
	//			{
	//				FGetImageDescContext Context;
	//				FImageDesc Desc = Base->GetImageDesc(true, &Context);
	//				UE_LOG(LogMutableCore, Log, TEXT("    estimated image format is %d:"), int(Desc.m_format) );
	//			}
	//		}
	//		return true;
	//	});

}


//-------------------------------------------------------------------------------------------------
void ASTOp::Traverse_TopDown_Unique( const TArray<Ptr<ASTOp>>& roots, TFunctionRef<bool(Ptr<ASTOp>&)> f )
{
    TQueue<Ptr<ASTOp>> pending;
	for (const Ptr<ASTOp>& r : roots)
	{
		pending.Enqueue(r);
	}

    TSet<Ptr<const ASTOp>> traversed;

    // We record the parents of all roots as traversed
    for (const Ptr<ASTOp>& r: roots)
    {
        r->ForEachParent( [&]( const ASTOp* parent )
        {
            // If the parent is also a root, we will want to process it.
            if ( !roots.Contains(parent) )
            {
                traversed.Add( parent );
            }
        });
    }

    while (!pending.IsEmpty())
    {
		Ptr<ASTOp> pCurrent;
		pending.Dequeue(pCurrent);
        if (!pCurrent)
        {
            continue;
        }

        // Did we traverse all parents?
        bool parentsTraversed = true;

        pCurrent->ForEachParent( [&]( const ASTOp* parent )
        {
            if (!traversed.Contains(parent))
            {
                // \todo Is the parent in the relevant subtree?


                parentsTraversed = false;
            }
        });

        if (!parentsTraversed)
        {
            pending.Enqueue(pCurrent);
        }
        else if (!traversed.Contains(pCurrent))
        {
            traversed.Add(pCurrent);

            // Process
            bool recurse = f(pCurrent);

            // Recurse children
            if (recurse)
            {
                pCurrent->ForEachChild([&]( ASTChild& c )
                {
                    if (c && !traversed.Contains(c.m_child))
                    {
                        pending.Enqueue( c.m_child );
                    }
                });
            }
        }
    }
}


//-------------------------------------------------------------------------------------------------
void ASTOp::Traverse_TopDown_Unique_Imprecise( const TArray<Ptr<ASTOp>>& roots, TFunctionRef<bool(Ptr<ASTOp>&)> f )
{
    TQueue<Ptr<ASTOp>> pending;
	for (const Ptr<ASTOp>& r : roots)
	{
		pending.Enqueue(r);
	}

    TSet<Ptr<const ASTOp>> traversed;

    while (!pending.IsEmpty())
    {
		Ptr<ASTOp> pCurrent;
		pending.Dequeue(pCurrent);

        // It could have been completed in another branch
        if (pCurrent && !traversed.Contains(pCurrent))
        {
            traversed.Add(pCurrent);

            // Process
            bool recurse = f(pCurrent);

            // Recurse children
            if (recurse)
            {
                pCurrent->ForEachChild([&]( ASTChild& c )
                {
                    if (c && !traversed.Contains(c.m_child))
                    {
                        pending.Enqueue( c.m_child );
                    }
                });
            }
        }
    }
}


//-------------------------------------------------------------------------------------------------
void ASTOp::Traverse_TopRandom_Unique_NonReentrant( const TArray<Ptr<ASTOp>>& roots,
	TFunctionRef<bool(Ptr<ASTOp>&)> f )
{
    ASTOpList pending;

    uint32 traverseIndex = s_lastTraverseIndex++;

    for (const Ptr<ASTOp>& r:roots)
    {
        if (r && r->m_traverseIndex!=traverseIndex )
        {
            r->m_traverseIndex = traverseIndex;
            pending.Add( r );
        }
    }
    for(const Ptr<ASTOp>& p : pending)
    {
        p->m_traverseIndex = traverseIndex-1;
    }

    while (pending.Num())
    {
        Ptr<ASTOp> pCurrent = pending.Pop();

        // It could have been completed in another branch
        if (pCurrent->m_traverseIndex!=traverseIndex)
        {
            pCurrent->m_traverseIndex = traverseIndex;

            // Process
            bool recurse = f(pCurrent);

            // Recurse children
            if (recurse)
            {
                pCurrent->ForEachChild([&]( ASTChild& c )
                {
                    if (c && c.m_child->m_traverseIndex!=traverseIndex)
                    {
                        pending.Add( c.m_child );
                    }
                });
            }
        }
    }
}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void Visitor_TopDown_Unique_Cloning::Traverse( Ptr<ASTOp>& root )
{
    // Visit the given root
    if (root)
    {
		m_pending.Add({ false,root });

        Process();

        root = GetOldToNew(root);
    }
}



void Visitor_TopDown_Unique_Cloning::Process()
{
    while ( m_pending.Num() )
    {
        TPair<bool,Ptr<ASTOp>> item = m_pending.Pop();

        Ptr<ASTOp> at = item.Value;

		ASTOp::MapChildFunc Identity = [](const Ptr<ASTOp>& o) {return o; };
		
		if (item.Key)
        {
            // Item indicating we finished with all the children of this instruction
			Ptr<ASTOp> cop = at->Clone(Identity);

            // Fix the references to the children
            bool childChanged = false;
            cop->ForEachChild( [&](ASTChild& ref)
            {
				std::unordered_map<Ptr<ASTOp>, Ptr<ASTOp>>::iterator it = m_oldToNew.find(ref.m_child);
                if ( ref && it!=m_oldToNew.end() && it->second.get()!=nullptr )
                {
                    auto oldRef = ref.m_child;
                    ref=GetOldToNew(ref.m_child);
                    if (ref.m_child!=oldRef)
                    {
                        childChanged = true;
                    }
                }
            });

            // If any child changed, we need to replace this instruction
            if ( childChanged )
            {
                m_oldToNew[at]=cop;
            }

        }
        else
        {
			std::unordered_map<Ptr<ASTOp>, Ptr<ASTOp>>::iterator it = m_oldToNew.find(at);
            if (it==m_oldToNew.end())
            {
				Ptr<ASTOp> initialAt = at;

                // Fix the references to the children, possibly adding a new instruction
                {
					Ptr<ASTOp> cop = at->Clone(Identity);
                    bool childChanged = false;
                    cop->ForEachChild( [&](ASTChild& ref)
                    {
						std::unordered_map<Ptr<ASTOp>, Ptr<ASTOp>>::iterator ito = m_oldToNew.find(ref.m_child);
                        if ( ref && ito!=m_oldToNew.end() && ito->second.get()!=nullptr )
                        {
							Ptr<ASTOp> oldRef = ref.m_child;
                            ref=GetOldToNew(ref.m_child);
                            if (ref.m_child!=oldRef)
                            {
                                childChanged = true;
                            }
                        }
                    });

                    // If any child changed, we need to re-add this instruction
                    if ( childChanged )
                    {
                        m_oldToNew[at]=cop;
                        at = cop;
                    }
                }

                //Ptr<ASTOp> test1 = at->Clone();
                //check(*test1==*at);

                bool processChildren = true;
				Ptr<ASTOp> newAt = Visit( at, processChildren );
                m_oldToNew[initialAt]=newAt;

                //check(*test1==*at);

                // Proceed with children
                if (processChildren)
                {
					m_pending.Add({ true, newAt });

                    at->ForEachChild( [&](ASTChild& ref)
                    {
                        if (ref && m_oldToNew.find(ref.m_child)==m_oldToNew.end())
                        {
							m_pending.Add({ false,ref.m_child });
                        }
                    });
                }
            }
        }

    }
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ASTOp::Traverse_BottomUp_Unique_NonReentrant
(
        ASTOpList& roots,
        TFunctionRef<void(Ptr<ASTOp>&)> f
)
{
    uint32 traverseIndex = s_lastTraverseIndex++;

    TArray< std::pair<Ptr<ASTOp>,int32> > pending;
    for (Ptr<ASTOp>& r:roots)
    {
        if (r && r->m_traverseIndex!=traverseIndex )
        {
            r->m_traverseIndex=traverseIndex;
            pending.Add( std::make_pair<>(r,0) );
        }
    }
    for(std::pair<Ptr<ASTOp>, int32>& p : pending)
    {
        p.first->m_traverseIndex = traverseIndex-1;
    }

    while (pending.Num())
    {
        int32 phase = pending.Last().second;
        Ptr<ASTOp> pCurrent = pending.Last().first;
        pending.Pop();

        // It could have been completed in another branch
        if (pCurrent->m_traverseIndex!=traverseIndex)
        {
            if (phase==0)
            {
                // Process this again...
                pending.Add( std::make_pair<>(pCurrent,1) );

                // ...after the children are processed
                pCurrent->ForEachChild([&]( ASTChild& c )
                {
                    if (c && c.m_child->m_traverseIndex!=traverseIndex )
                    {
						std::pair<Ptr<ASTOp>, int32> e = std::make_pair<>(c.m_child,0);
                        pending.Add( e );
                    }
                });
            }
            else
            {
                pCurrent->m_traverseIndex=traverseIndex;

                // Children have been be completed
                f(pCurrent);
            }
        }
    }
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ASTOp::Traverse_BottomUp_Unique_NonReentrant
(
        ASTOpList& roots,
        TFunctionRef<void(Ptr<ASTOp>&)> f,
        TFunctionRef<bool(const ASTOp*)> accept
)
{
    uint32 traverseIndex = s_lastTraverseIndex++;

    TArray< std::pair<Ptr<ASTOp>,int32> > pending;
    for (Ptr<ASTOp>& r:roots)
    {
        if (r && r->m_traverseIndex!=traverseIndex)
        {
            r->m_traverseIndex=traverseIndex;
            pending.Add( std::make_pair<>(r,0) );
        }
    }
    for(std::pair<Ptr<ASTOp>, int32>& p : pending)
    {
        p.first->m_traverseIndex = traverseIndex-1;
    }

    while (pending.Num())
    {
        int32 phase = pending.Last().second;
        Ptr<ASTOp> pCurrent = pending.Last().first;
        pending.Pop();

        // It could have been completed in another branch
        if (pCurrent->m_traverseIndex!=traverseIndex && accept(pCurrent.get()))
        {
            if (phase==0)
            {
                // Process this again...
                pending.Add( std::make_pair<>(pCurrent,1) );

                // ...after the children are processed
                pCurrent->ForEachChild([&]( ASTChild& c )
                {
                    if (c && accept(c.m_child.get()) && c.m_child->m_traverseIndex!=traverseIndex )
                    {
                        pending.Add( std::make_pair<>(c.m_child,0) );
                    }
                });
            }
            else
            {
                pCurrent->m_traverseIndex=traverseIndex;

                // Children have been be completed
                f(pCurrent);
            }
        }
    }
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ASTOp::Traverse_BottomUp_Unique
(
        ASTOpList& roots,
        TFunctionRef<void(Ptr<ASTOp>&)> f,
        TFunctionRef<bool(const ASTOp*)> accept
)
{
    TSet<Ptr<ASTOp>> Traversed;
    TArray< std::pair<Ptr<ASTOp>,int32> > Pending;
    for (Ptr<ASTOp>& r:roots)
    {
        if (r)
        {
			std::pair<Ptr<ASTOp>, int32>* It = Pending.FindByPredicate([&](const std::pair<Ptr<ASTOp>, int>& p)
				{
					return r == p.first;
				});
            if ( !It )
            {
                Pending.Add( std::make_pair<>(r,0) );
            }
        }
    }

    while (!Pending.IsEmpty())
    {
        int phase = Pending.Last().second;
        Ptr<ASTOp> pCurrent = Pending.Last().first;
        Pending.Pop();

        // It could have been completed in another branch
        if (accept(pCurrent.get()) && !Traversed.Find(pCurrent))
        {
            if (phase==0)
            {
                // Process this again...
				Pending.Add( std::make_pair<>(pCurrent,1) );

                // ...after the children are processed
                pCurrent->ForEachChild([&]( ASTChild& c )
                {
                    if (c && accept(c.m_child.get()) && !Traversed.Find(c.m_child) )
                    {
                        Pending.Add( std::make_pair<>(c.m_child,0) );
                    }
                });
            }
            else
            {
                Traversed.Add(pCurrent);

                // Children have been be completed
                f(pCurrent);
            }
        }
    }
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
void ASTOp::Traverse_BottomUp_Unique
(
        Ptr<ASTOp>& root,
        TFunctionRef<void(Ptr<ASTOp>&)> f,
        TFunctionRef<bool(const ASTOp*)> accept
)
{
    if (root)
    {
        ASTOpList roots;
        roots.Add(root);
        Traverse_BottomUp_Unique(roots,f,accept);
    }
}



//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
int ASTOp::GetParentCount() const
{
    int result=0;
    ForEachParent( [&](const ASTOp* p)
    {
        if (p!=nullptr) ++result;
    });
    return result;
}


void ASTOp::Replace( const Ptr<ASTOp>& node, const Ptr<ASTOp>& other )
{
    if(other==node)
    {
        return;
    }

	TArray<ASTOp*, TInlineAllocator<4> > parentsCopy = node->m_parents;

    for(ASTOp* p:parentsCopy)
    {
        if(p)
        {
            p->ForEachChild( [=](ASTChild& c)
            {
                if(c.m_child==node)
                {
                    c = other;
                }
            });
        }
    }
}


FImageDesc ASTOp::GetImageDesc( bool, FGetImageDescContext* ) const
{
    check(false);
    return FImageDesc();
}


bool ASTOp::IsImagePlainConstant(FVector4f&) const
{
	// Some image operations don't have this implemented and hit here.
	// \TODO: Optimize for those cases.
    //check(false);
    return false;
}


bool ASTOp::IsColourConstant(FVector4f&) const
{
    check(false);
    return false;
}


void ASTOp::GetBlockLayoutSize( int, int*, int*, FBlockLayoutSizeCache* )
{
    check(false);
}

void ASTOp::GetBlockLayoutSizeCached( int blockIndex, int* pBlockX, int* pBlockY,
                               FBlockLayoutSizeCache* cache )
{
	FBlockLayoutSizeCache::KeyType key(this,blockIndex);
	FBlockLayoutSizeCache::ValueType* ValuePtr = cache->Find( key );
    if (ValuePtr)
    {
        *pBlockX = ValuePtr->Key;
        *pBlockY = ValuePtr->Value;
        return;
    }

    GetBlockLayoutSize( blockIndex, pBlockX, pBlockY, cache );

	cache->Add(key, FBlockLayoutSizeCache::ValueType( *pBlockX, *pBlockY) );
}


void ASTOp::GetLayoutBlockSize( int*, int* )
{
    check(false);
}


bool ASTOp::GetNonBlackRect( FImageRect& ) const
{
    return false;
}


void ASTOp::LinkRange(FProgram& program,
	const FRangeData& range,
	OP::ADDRESS& rangeSize,
	uint16& rangeId)
{
	if (range.rangeSize)
	{
		if (range.rangeSize->linkedRange < 0)
		{
			check(program.m_ranges.Num() < 255);
			range.rangeSize->linkedRange = int8(program.m_ranges.Num());
			FRangeDesc rangeData;
			rangeData.m_name = range.rangeName;
			rangeData.m_uid = range.rangeUID;

			// Try to see if a parameter directly controls de size of the range. This is used
			// to store hint data for instance generation in tools or randomizers that want to
			// support multilayer, but it is not critical otherwise.
			int32 EstimatedSizeParameter = -1;
			if ( range.rangeSize->GetOpType() == OP_TYPE::SC_PARAMETER
				||
				range.rangeSize->GetOpType() == OP_TYPE::NU_PARAMETER )
			{
				const ASTOpParameter* ParamOp = static_cast<const ASTOpParameter*>(range.rangeSize.child().get());

				EstimatedSizeParameter = ParamOp->LinkedParameterIndex;
			}
			rangeData.m_dimensionParameter = EstimatedSizeParameter;

			program.m_ranges.Add(rangeData);
		}
		rangeSize = range.rangeSize->linkedAddress;
		rangeId = uint16(range.rangeSize->linkedRange);
	}
}


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
ASTOpFixed::ASTOpFixed()
    : children({{
			ASTChild(this,nullptr),ASTChild(this,nullptr),ASTChild(this,nullptr),ASTChild(this,nullptr),
			ASTChild(this,nullptr),ASTChild(this,nullptr),ASTChild(this,nullptr),ASTChild(this,nullptr)
		}})
{
}


ASTOpFixed::~ASTOpFixed()
{
    // Explicit call needed to avoid recursive destruction
    ASTOp::RemoveChildren();
}


void ASTOpFixed::ForEachChild( const TFunctionRef<void(ASTChild&)> f )
{
    // ugly but temporary while ASTOpFixed exists
    ForEachReference( op, [&](OP::ADDRESS* pAt)
    {
        if (pAt && *pAt)
        {
            check(children.size()>*pAt);
            f(children[*pAt]);
        }
    });
}


void ASTOpFixed::Link( FProgram& program, FLinkerOptions* )
{
	MUTABLE_CPUPROFILER_SCOPE(ASTOpFixed_Link);

    if (!linkedAddress)
    {
        OP lop = op;

        // Fix the linked op and generate children
        ForEachReference(lop, [&](OP::ADDRESS* pChildOp)
        {
            Ptr<ASTOp> pASTChild = children[*pChildOp].m_child;
            if (pASTChild)
            {
                *pChildOp = pASTChild->linkedAddress;
            }
            else
            {
                *pChildOp = 0;
            }
        });

        linkedAddress = (OP::ADDRESS)program.m_opAddress.Num();        
        program.m_opAddress.Add((uint32)program.m_byteCode.Num());

        AppendCode(program.m_byteCode,lop.type);
        if (lop.type==OP_TYPE::ME_MERGE)
        {
            AppendCode(program.m_byteCode,lop.args.MeshMerge);
        }
        else
        {
            // Generic encoding
            AppendCode(program.m_byteCode,lop.args);
        }
    }
}


bool ASTOpFixed::IsEqual(const ASTOp& otherUntyped) const
{
    if (otherUntyped.GetOpType()==GetOpType())
    {
		const ASTOpFixed* other = static_cast<const ASTOpFixed*>(&otherUntyped);
        return op==other->op && children==other->children;
    }
    return false;
}


mu::Ptr<ASTOp> ASTOpFixed::Clone( MapChildFuncRef mapChild ) const
{
    Ptr<ASTOpFixed> n = new ASTOpFixed();
    n->op = op;
    n->childCount = childCount;
    // Skip the children 0, used to represent null op.
    for(uint8_t i=1; i<childCount; ++i)
    {
        n->children[i] = mapChild(children[i].child());
    }
    return n;
}


uint64 ASTOpFixed::Hash() const
{
	uint64 res = std::hash<uint64>()(uint64(op.type));
    for (const ASTChild& c: children)
    {
        hash_combine( res, c.child().get() );
    }
    return res;
}


//!
FImageDesc ASTOpFixed::GetImageDesc( bool returnBestOption, FGetImageDescContext* context ) const
{
    FImageDesc res;

    // Local context in case it is necessary
    FGetImageDescContext localContext;
    if (!context)
    {
      context = &localContext;
    }
    else
    {
        // Cached result?
		FImageDesc* PtrValue = context->m_results.Find(this);
        if (PtrValue)
        {
            return *PtrValue;
        }
    }

    OP_TYPE type = (OP_TYPE) op.type;

    switch ( type )
    {

    case OP_TYPE::NONE:
        break;

    case OP_TYPE::IM_SATURATE:
        res = GetImageDesc( op.args.ImageSaturate.base, returnBestOption, context );
        break;

    case OP_TYPE::IM_LUMINANCE:
        res = GetImageDesc( op.args.ImageLuminance.base,returnBestOption, context );
        res.m_format = EImageFormat::IF_L_UBYTE;
        break;

    case OP_TYPE::IM_INTERPOLATE:
        res = GetImageDesc(op.args.ImageInterpolate.targets[0], returnBestOption, context );
        break;

    case OP_TYPE::IM_PLAINCOLOUR:
        res.m_format = EImageFormat(op.args.ImagePlainColour.format);
        res.m_size[0] = op.args.ImagePlainColour.size[0];
        res.m_size[1] = op.args.ImagePlainColour.size[1];
		res.m_lods = op.args.ImagePlainColour.LODs;
        check( res.m_format != EImageFormat::IF_NONE );
        break;

    case OP_TYPE::IM_RESIZE:
        res = GetImageDesc( op.args.ImageResize.source, returnBestOption, context );

        res.m_size = FImageSize
            (
                op.args.ImageResize.size[0],
                op.args.ImageResize.size[1]
            );
        break;

    case OP_TYPE::IM_RESIZEREL:
        res = GetImageDesc( op.args.ImageResizeRel.source, returnBestOption, context );
        res.m_size[0] = (uint16)( res.m_size[0]*op.args.ImageResizeRel.factor[0] );
        res.m_size[1] = (uint16)( res.m_size[1]*op.args.ImageResizeRel.factor[1] );
        break;

    case OP_TYPE::IM_RESIZELIKE:
        res = GetImageDesc( op.args.ImageResizeLike.source, returnBestOption, context );
        if (op.args.ImageResizeLike.sizeSource)
        {
            res.m_size = children[op.args.ImageResizeLike.sizeSource]->GetImageDesc( returnBestOption, context ).m_size;
        }
        break;

    case OP_TYPE::IM_GRADIENT:
        res.m_size[0] = op.args.ImageGradient.size[0];
        res.m_size[1] = op.args.ImageGradient.size[1];
        res.m_format = EImageFormat::IF_RGB_UBYTE;
        break;

    case OP_TYPE::IM_BLANKLAYOUT:
        // TODO: We would need to process the layout to find the grid size, and then use
        // the block size with it.
        res.m_size = FImageSize(0,0);
        res.m_format = op.args.ImageBlankLayout.format;
        break;

    case OP_TYPE::IM_BINARISE:
        res = GetImageDesc( op.args.ImageBinarise.base, returnBestOption, context );
        res.m_format = EImageFormat::IF_L_UBYTE;
        break;

    case OP_TYPE::IM_DISPLACE:
        res = GetImageDesc( op.args.ImageDisplace.source, returnBestOption, context );
        break;

	case OP_TYPE::IM_INVERT:
		res = GetImageDesc(op.args.ImageInvert.base, returnBestOption, context);
		break;

	case OP_TYPE::IM_COLOURMAP:
		res = GetImageDesc( op.args.ImageColourMap.base, returnBestOption, context );
		break;

    default:
        check(false);
    }

    // Cache the result
    if (context)
    {
        context->m_results.Add(this,res);
    }

    return res;
}


void ASTOpFixed::GetLayoutBlockSize( int* pBlockX, int* pBlockY )
{
    switch ( op.type )
    {
    
	case OP_TYPE::IM_RESIZEREL:
	{
		GetLayoutBlockSize(op.args.ImageResizeRel.source, pBlockX, pBlockY);
		(*pBlockX) = int(float(*pBlockX) * op.args.ImageResizeRel.factor[0]);
		(*pBlockY) = int(float(*pBlockX) * op.args.ImageResizeRel.factor[1]);

		break;
	}

	case OP_TYPE::IM_RESIZE:
	{
		GetLayoutBlockSize(op.args.ImageResize.source, pBlockX, pBlockY);

		if (*pBlockX > 0 && *pBlockY> 0)
		{
			FImageDesc sourceDesc = GetImageDesc(op.args.ImageResize.source);
			if (sourceDesc.m_size[0]>0 && sourceDesc.m_size[1]>0)
			{
				float factorX = float(op.args.ImageResize.size[0]) / float(sourceDesc.m_size[0]);
				float factorY = float(op.args.ImageResize.size[1]) / float(sourceDesc.m_size[1]);
				*pBlockX = int(*pBlockX * factorX);
				*pBlockY = int(*pBlockX * factorY);
			}
			else
			{
				*pBlockX = 0;
				*pBlockY = 0;
			}
		}

		break;
	}

	case OP_TYPE::IM_BLANKLAYOUT:
	{
		*pBlockX = int(op.args.ImageBlankLayout.blockSize[0]);
		*pBlockY = int(op.args.ImageBlankLayout.blockSize[1]);
		break;
	}

	case OP_TYPE::IM_PLAINCOLOUR:
	{
		*pBlockX = 0;
		*pBlockY = 0;
		break;
	}

    default:
        checkf( false, TEXT("Instruction not supported") );
    }

    //check( *pBlockX!=0 && *pBlockY!=0 );
}


ASTOp::FBoolEvalResult ASTOpFixed::EvaluateBool( ASTOpList& facts, FEvaluateBoolCache* cache ) const
{
    FEvaluateBoolCache localCache;
    if (!cache)
    {
        cache = &localCache;
    }
    else
    {
        // Is this in the cache?
		FEvaluateBoolCache::iterator it = cache->find(this);
        if (it!=cache->end())
        {
            return it->second;
        }
    }

    FBoolEvalResult result = BET_UNKNOWN;
    switch(op.type)
    {
    case OP_TYPE::BO_NOT:
    {
        if (children[op.args.BoolNot.source])
        {
            FBoolEvalResult child = children[op.args.BoolNot.source]->EvaluateBool(facts,cache);
            if (child==BET_TRUE)
            {
                result = BET_FALSE;
            }
            else if (child==BET_FALSE)
            {
                result = BET_TRUE;
            }
        }
        break;
    }

    case OP_TYPE::BO_EQUAL_INT_CONST:
    {
        int32 intValue = op.args.BoolEqualScalarConst.constant;
        const Ptr<ASTOp>& intExp = children[op.args.BoolEqualScalarConst.value].child();
        bool intUnknown = true;
        int intResult = intExp->EvaluateInt( facts, intUnknown );
        if (intUnknown)
        {
            result = BET_UNKNOWN;
        }
        else if (intResult==intValue)
        {
            result = BET_TRUE;
        }
        else
        {
            result = BET_FALSE;
        }
        break;
    }

    case OP_TYPE::BO_AND:
    {
        const Ptr<ASTOp>& a = children[op.args.BoolBinary.a].child();
        const Ptr<ASTOp>& b = children[op.args.BoolBinary.b].child();
        FBoolEvalResult resultA = BET_UNKNOWN;
        FBoolEvalResult resultB = BET_UNKNOWN;
        for ( size_t f=0; f<facts.Num(); ++f )
        {
            if ( a && resultA == BET_UNKNOWN )
            {
                resultA = a->EvaluateBool( facts, cache );

                if ( resultA==BET_TRUE && resultB==BET_TRUE )
                {
                    result = BET_TRUE;
                    break;
                }
                if ( resultA==BET_FALSE || resultB==BET_FALSE )
                {
                    result = BET_FALSE;
                    break;
                }
            }
            if ( b && resultB == BET_UNKNOWN )
            {
                resultB = b->EvaluateBool( facts, cache );

                if ( resultA==BET_TRUE && resultB==BET_TRUE )
                {
                    result = BET_TRUE;
                    break;
                }
                if ( resultA==BET_FALSE || resultB==BET_FALSE )
                {
                    result = BET_FALSE;
                    break;
                }
            }
        }
        break;
    }

    case OP_TYPE::BO_OR:
    {
        const Ptr<ASTOp>& a = children[op.args.BoolBinary.a].child();
        const Ptr<ASTOp>& b = children[op.args.BoolBinary.b].child();
        FBoolEvalResult resultA = BET_UNKNOWN;
        FBoolEvalResult resultB = BET_UNKNOWN;
        for ( size_t f=0; f<facts.Num(); ++f )
        {
            if ( a && resultA == BET_UNKNOWN )
            {
                resultA = a->EvaluateBool( facts, cache );

                if ( resultA==BET_TRUE || resultB==BET_TRUE )
                {
                    result = BET_TRUE;
                    break;
                }
                if ( resultA==BET_FALSE && resultB==BET_FALSE )
                {
                    result = BET_FALSE;
                    break;
                }
            }
            if ( b && resultB == BET_UNKNOWN )
            {
                resultB = b->EvaluateBool( facts, cache );

                if ( resultA==BET_TRUE || resultB==BET_TRUE )
                {
                    result = BET_TRUE;
                    break;
                }
                if ( resultA==BET_FALSE && resultB==BET_FALSE )
                {
                    result = BET_FALSE;
                    break;
                }
            }
        }
        break;
    }

    default:
        checkf( false, TEXT("Instruction not supported") );
    }

    (*cache)[this] = result;

    return result;
}


int ASTOpFixed::EvaluateInt( ASTOpList& /*facts*/, bool &unknown ) const
{
    unknown = true;

    switch (GetOpType())
    {
    case OP_TYPE::NU_CONSTANT:
        unknown = false;
        return op.args.IntConstant.value;
        break;

    case OP_TYPE::SC_CONSTANT:
        unknown = false;
        return (int)op.args.ScalarConstant.value;
        break;

        // TODO
//    case IntExpression::IET_PARAMETER:
//        for ( size_t f=0; (result==BET_UNKNOWN) && f<facts.Num(); ++f )
//        {
//            result = EvaluateIntEquality(facts[f].get(),intExp,value);
//        }
//        break;

    default:
        break;

    }


    return 0;
}


bool ASTOpFixed::IsImagePlainConstant(FVector4f& colour) const
{
    bool res = false;
    switch( op.type )
    {

    case OP_TYPE::IM_BLANKLAYOUT:
        res = true;
        colour[0] = 0;
        colour[1] = 0;
        colour[2] = 0;
        colour[3] = 0;
        break;

    case OP_TYPE::IM_RESIZE:
        res = children[op.args.ImageResize.source]->IsImagePlainConstant( colour );
        break;

    case OP_TYPE::IM_RESIZEREL:
        res = children[op.args.ImageResizeRel.source]->IsImagePlainConstant( colour );
        break;

    case OP_TYPE::IM_RESIZELIKE:
        res = children[op.args.ImageResizeLike.source]->IsImagePlainConstant(  colour );
        break;

    case OP_TYPE::IM_PLAINCOLOUR:
        res = children[op.args.ImagePlainColour.colour]->IsColourConstant( colour );
        break;

    default:
        // TODO: Improve this test with more operations
        //check( false );
        res = false;
        break;
    }

    return res;
}


//-------------------------------------------------------------------------------------------------
bool ASTOpFixed::IsColourConstant(FVector4f& colour) const
{
    bool res = false;
    switch( op.type )
    {
    case OP_TYPE::CO_CONSTANT:
        res = true;
        colour[0] = op.args.ColourConstant.value[0];
        colour[1] = op.args.ColourConstant.value[1];
        colour[2] = op.args.ColourConstant.value[2];
        colour[3] = op.args.ColourConstant.value[3];
        break;

    case OP_TYPE::CO_SAMPLEIMAGE:
    case OP_TYPE::CO_SWIZZLE:
    case OP_TYPE::CO_FROMSCALARS:
    case OP_TYPE::CO_ARITHMETIC:

    default:
        // TODO: Improve this test with more operations
        //check( false );
        res = false;
        break;
    }

    return res;
}


//-------------------------------------------------------------------------------------------------
mu::Ptr<ImageSizeExpression> ASTOpFixed::GetImageSizeExpression() const
{
    Ptr<ImageSizeExpression> pRes = new ImageSizeExpression;

    OP_TYPE type = GetOpType();
    switch ( type )
    {
    case OP_TYPE::NONE:
    {
        pRes->type = ImageSizeExpression::ISET_CONSTANT;
        pRes->size[0]=0;
        pRes->size[1]=0;
        break;
    }

    case OP_TYPE::IM_RESIZE:
        pRes->type = ImageSizeExpression::ISET_CONSTANT;
        pRes->size[0] = op.args.ImageResize.size[0];
        pRes->size[1] = op.args.ImageResize.size[1];
        break;

    case OP_TYPE::IM_RESIZEREL:
        if ( children[op.args.ImageResizeRel.source] )
        {
            pRes = children[op.args.ImageResizeRel.source].child()->GetImageSizeExpression();
            if (pRes->type == ImageSizeExpression::ISET_CONSTANT)
            {
                pRes->size[0] = uint16( pRes->size[0] * op.args.ImageResizeRel.factor[0] );
                pRes->size[1] = uint16( pRes->size[1] * op.args.ImageResizeRel.factor[1] );
            }
            else
            {
                // TODO: Proportional factor
                pRes = new ImageSizeExpression();
                pRes->type = ImageSizeExpression::ISET_UNKNOWN;
            }
        }
        break;

    case OP_TYPE::IM_RESIZELIKE:
        if ( children[op.args.ImageResizeLike.sizeSource] )
        {
            pRes = children[op.args.ImageResizeLike.sizeSource].child()->GetImageSizeExpression();
        }
        break;

    case OP_TYPE::IM_PLAINCOLOUR:
        pRes->type = ImageSizeExpression::ISET_CONSTANT;
        pRes->size[0] = op.args.ImagePlainColour.size[0];
        pRes->size[1] = op.args.ImagePlainColour.size[1];
        break;

    case OP_TYPE::IM_BLANKLAYOUT:
        pRes->type = ImageSizeExpression::ISET_LAYOUTFACTOR;
        pRes->layout = children[op.args.ImageBlankLayout.layout].child();
        pRes->factor[0] = op.args.ImageBlankLayout.blockSize[0];
        pRes->factor[1] = op.args.ImageBlankLayout.blockSize[1];
        break;

    case OP_TYPE::IM_INTERPOLATE:
        if ( children[op.args.ImageInterpolate.targets[0]] )
        {
            pRes = children[op.args.ImageInterpolate.targets[0]].child()->GetImageSizeExpression();
        }
        break;

    case OP_TYPE::IM_SATURATE:
        if ( children[op.args.ImageSaturate.base] )
        {
            pRes = children[op.args.ImageSaturate.base].child()->GetImageSizeExpression();
        }
        break;

    case OP_TYPE::IM_LUMINANCE:
        if ( children[op.args.ImageLuminance.base] )
        {
            pRes = children[op.args.ImageLuminance.base].child()->GetImageSizeExpression();
        }
        break;

    case OP_TYPE::IM_BINARISE:
        if ( children[op.args.ImageBinarise.base] )
        {
            pRes = children[op.args.ImageBinarise.base].child()->GetImageSizeExpression();
        }
        break;

    case OP_TYPE::IM_COLOURMAP:
        if ( children[op.args.ImageColourMap.base] )
        {
            pRes = children[op.args.ImageColourMap.base].child()->GetImageSizeExpression();
        }
        break;

    case OP_TYPE::IM_GRADIENT:
        pRes->type = ImageSizeExpression::ISET_CONSTANT;
        pRes->size[0] = op.args.ImageGradient.size[0];
        pRes->size[1] = op.args.ImageGradient.size[1];
        break;

    case OP_TYPE::IM_DISPLACE:
        if ( children[op.args.ImageDisplace.source] )
        {
            pRes = children[op.args.ImageDisplace.source].child()->GetImageSizeExpression();
        }
        break;

	case OP_TYPE::IM_INVERT:
		if (children[op.args.ImageInvert.base])
		{
			pRes = children[op.args.ImageInvert.base].child()->GetImageSizeExpression();
		}
		break;
 
    default:
    	check( false );
    	break;
    }

    return pRes;
}

}
