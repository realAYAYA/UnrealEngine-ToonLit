/*
 * Copyright 1995, 2003 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * gzip.h - a Gzip class for producing gzip streams
 *
 * Classes Defined:
 *
 *	Gzip - a compressor/uncompressor
 *
 * Public variables:
 *
 *	buf[4096] - convenient buffer for input or output 
 *			Not used internally, but is/os can point there.
 *
 *	is, ie - start and end of input buffer
 *	os, oe - start and end of output buffer
 *
 * Public methods:
 *
 *	Compress() - compress data in is->ie into os->oe.
 *			Set is = 0 to flush.  Returns 0 when
 *			flushing complete.
 *
 *	Uncompress() - expand data in is->ie into os->oe.
 *			Returns 0 when uncompression complete.
 *
 *	InputEmpty() - Available input consumed.  
 *			Always returns 0 when flushing.
 *
 *	OutputFull() - Available output full.
 */

typedef struct z_stream_s z_stream;

class Gzip
{
    public:
			Gzip();
			~Gzip();

	int		Compress( Error *e );
	int		Uncompress( Error *e );

	// input/output start/end

	const char 	*is, *ie;
	char 		*os, *oe;

	int		InputEmpty() { return is && is == ie; }
	int		OutputFull() { return os == oe; }

    private:

	z_stream	*zstream;
	int		isInflate;
	int		isDeflate;
	int		state;

	// temp buffer writing

	char		*ws, *we;

	unsigned long	crc;
	char		tmpbuf[10];	// sizeof gz_magic

	// header handling 

	int		hflags;
	int		hxlen;

} ;
