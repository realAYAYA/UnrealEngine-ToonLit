/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * handler.h - last chance handlers to keep track of loose objects
 *
 * Handlers provide a way of associating an object with a string so
 * context can be retained across RPC dispatched function calls.  
 * This is used for file transfers, which are carried out in a series
 * of RPC calls.  The sender picks a handle name and then uses that
 * consistenly during the transfer.  The receiver uses the provided handle
 * name to stash and retrieve the object the represents the open file.
 *
 * Handlers also provide a means of tracking across objects.  If any
 * object encounters an error, it can mark the handle so that a subsequent
 * call to AnyErrors() can report so.
 *
 * Public classes:
 *
 *	Handlers - a list of LastChance objects
 *	LastChance - a virtual base class that gets deleted with the
 *			handlers.
 */

class LastChance;

struct Handler {
	    StrBuf	name;
	    int		anyErrors;
	    LastChance	*lastChance;
} ;

class LastChance {

    public:
			LastChance()
			{
			    handler = 0;
			    isError = 0;
			    deleteOnRelease = 0;
			}

	virtual		~LastChance();

	void		Install( Handler *h )
			{
			    handler = h; 
			    handler->lastChance = this;
			}

	void		SetError()
			{
			    isError = 1;
			}

	void		SetError( Error *e )
			{
			    if( e->Test() ) isError = 1;
			}

	int		IsError()
			{
			    return isError;
			}

	int		DeleteOnRelease()
			{
			    return deleteOnRelease;
			}

    protected:
	int		deleteOnRelease;

    private:
	Handler		*handler;
	int		isError;

} ;

const int maxHandlers = 10;

class Handlers {

    public:
			Handlers();
			~Handlers();

	void		Install( const StrPtr *name, 
				LastChance *lastChance,
				Error *e );

	LastChance *	Get( const StrPtr *name, Error *e = 0 );

	int		AnyErrors( const StrPtr *nane );

	void		SetError( const StrPtr *name, Error *e );

	void		Release();

    private:

	int		numHandlers;
	Handler 	table[maxHandlers];
	Handler		*Find( const StrPtr *handle, Error *e = 0 );
} ;

