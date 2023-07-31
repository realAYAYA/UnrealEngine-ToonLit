/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */


/*
 * SHA1 - compute sha1 checksum given a bunch of blocks of data. This header
 *        file includes two related classes:
 *
 *        Sha1Digester: utility wrapper around the OpenSSL SHA1 libraries
 *        to simplify computing a SHA1 of some data.
 *
 *        Sha1: encapsulation of a 160-bit SHA1 value.
 */

# define Sha1Length 20

class Sha1
{
    public:
			Sha1() { Clear(); }
			Sha1( unsigned const char *bytes) { Import( bytes ); }

			~Sha1() {}

	void		Clear() { memset( data, 0, Sha1Length ); }

	void		Import( unsigned const char *bytes )
	                { memcpy( data, bytes, Sha1Length ); }
	
	void		Export( unsigned char *bytes ) const
	                { memcpy( bytes, data, Sha1Length ); }

	int		Compare( const Sha1 &other ) const
	                { return memcmp( data, other.data, Sha1Length ); }

	int		Compare( unsigned const char *bytes ) const
	                { return memcmp( data, bytes, Sha1Length ); }

	void		FromString( const StrPtr *sha );

	void		Fmt( StrBuf &buf ) const;

	int		IsSet() const;
	                    
	unsigned char data [ Sha1Length ];
} ;

class Sha1Digester
{

    public:
			Sha1Digester();
			~Sha1Digester();
	void		Update( const StrPtr &buf );
	void		Final( StrBuf &output );
	void		Final( unsigned char digest[ Sha1Length ] );
	void		Final( Sha1 &sha );

    private:
	void		*ctx;

};

