// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuT/Platform.h"


namespace mu
{

	//---------------------------------------------------------------------------------------------
	//!
	//---------------------------------------------------------------------------------------------
    class BaseVisitor
    {
    public:
        virtual ~BaseVisitor() {}
    };

    template <class T, typename R = void, bool ConstVisit = false>
    class Visitor;

    template <class T, typename R>
    class Visitor<T, R, false>
    {
    public:
        typedef R ReturnType;
        typedef T ParamType;
        virtual ~Visitor() {}
        virtual ReturnType Visit(ParamType&) = 0;
    };

    template <class T, typename R>
    class Visitor<T, R, true>
    {
    public:
        typedef R ReturnType;
        typedef const T ParamType;
        virtual ~Visitor() {}
        virtual ReturnType Visit(ParamType&) = 0;
    };


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
	template <typename R, typename Visited>
	struct DefaultCatchAll
	{
		static R OnUnknownVisitor( Visited&, BaseVisitor& )
		{
			check( false );
			return R();
		}
	};


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
	template
    <
        typename R = void,
        template <typename, class> class CatchAll = DefaultCatchAll,
        bool ConstVisitable = false
    >
    class BaseVisitable;

    template<typename R,template <typename, class> class CatchAll>
    class BaseVisitable<R, CatchAll, false>
    {
    public:
        typedef R ReturnType;
        virtual ~BaseVisitable() {}
        virtual ReturnType Accept(BaseVisitor&) = 0;

    protected: // give access only to the hierarchy
        template <class T>
        static ReturnType AcceptImpl(T& visited, BaseVisitor& guest)
        {
            // Apply the Acyclic Visitor
            if (Visitor<T,R>* p = dynamic_cast<Visitor<T,R>*>(&guest))
            {
                return p->Visit(visited);
            }
            return CatchAll<R, T>::OnUnknownVisitor(visited, guest);
        }
    };


    //---------------------------------------------------------------------------------------------
    //!
    //---------------------------------------------------------------------------------------------
    template<typename R,template <typename, class> class CatchAll>
    class BaseVisitable<R, CatchAll, true>
    {
    public:
        typedef R ReturnType;
        virtual ~BaseVisitable() {}
        virtual ReturnType Accept(BaseVisitor&) const = 0;

    protected: // give access only to the hierarchy
        template <class T>
        static ReturnType AcceptImpl(const T& visited, BaseVisitor& guest)
        {
            // Apply the Acyclic Visitor
            if (Visitor<T,R,true>* p = dynamic_cast<Visitor<T,R,true>*>(&guest))
            {
                return p->Visit(visited);
            }
            return CatchAll<R, T>::OnUnknownVisitor(const_cast<T&>(visited), guest);
        }
    };


   //---------------------------------------------------------------------------------------------
   //!
   //---------------------------------------------------------------------------------------------
	#define MUTABLE_DEFINE_VISITABLE() 								\
        ReturnType Accept(::mu::BaseVisitor& guest) override	\
			{ return AcceptImpl(*this, guest); }


   //---------------------------------------------------------------------------------------------
   //!
   //---------------------------------------------------------------------------------------------
	#define MUTABLE_DEFINE_CONST_VISITABLE() 							\
        ReturnType Accept(::mu::BaseVisitor& guest) const override	\
			{ return AcceptImpl(*this, guest); }


}
