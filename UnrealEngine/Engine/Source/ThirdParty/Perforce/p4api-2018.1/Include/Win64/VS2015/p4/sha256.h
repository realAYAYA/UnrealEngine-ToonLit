/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */


/*
 * SHA256 - compute sha256 checksum given a bunch of blocks of data. This header
 *          file includes two related classes:
 *
 *          Sha256Digester: utility wrapper around the OpenSSL SHA256 libraries
 *          to simplify computing a SHA256 of some data.
 *
 *          Sha256: encapsulation of a 256-bit SHA256 value.
 */

# define Sha256Length 32

class Sha256
{
    public:
			Sha256() { Clear(); }
			Sha256( unsigned const char *bytes) { Import( bytes ); }

			~Sha256() {}

	void		Clear() { memset( data, 0, Sha256Length ); }

	void		Import( unsigned const char *bytes )
	                { memcpy( data, bytes, Sha256Length ); }
	
	void		Export( unsigned char *bytes ) const
	                { memcpy( bytes, data, Sha256Length ); }

	int		Compare( const Sha256 &other ) const
	                { return memcmp( data, other.data, Sha256Length ); }

	int		Compare( unsigned const char *bytes ) const
	                { return memcmp( data, bytes, Sha256Length ); }

	void		FromString( const StrPtr *sha );

	void		Fmt( StrBuf &buf ) const;

	int		IsSet() const;
	                    
	unsigned char data [ Sha256Length ];
} ;

class Sha256Digester
{

    public:
			Sha256Digester();
			~Sha256Digester();
	void		Update( const StrPtr &buf );
	void		Final( StrBuf &output );
	void		Final( unsigned char digest[ Sha256Length ] );
	void		Final( Sha256 &sha );

    private:
	void		*ctx;

};

