/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 */

/*
 * Error.h - accumulate and report layered errors
 *
 * Class Defined:
 *
 *	Error - holder of layered error
 *
 *	    The basic idea of Error is that the top level caller
 *	    should have one on its stack and hand it by reference
 *	    down to all lower layers.  If any operation fails, it
 *	    can add its description of the error to the structure
 *	    with Set().  After each operation that can potentially
 *	    fail, the caller should use Test() to check for errors.
 *	    The top level should do the check and then call Report()
 *	    if necessary.
 *
 *	    Caveat: All messages now have named parameters, be very
 *	            careful not to nest messages that can have the same
 *	            parameter name.  
 *
 * Public methods:
 *
 *	Error::Clear() - clean an Error struct
 *	Error::Test() - see if an error is present ( i.e. > E_INFO )
 *	Error::IsFatal() - is most severe error fatal?
 *	Error::IsWarning() - is most severe error just a warning?
 *	Error::IsInfo() - is most severe error just information?
 *	Error::GetSeverity() - return ErrorSeverity of most severe error
 *	Error::GetGeneric() - return Generic code of most severe error
 *
 *	Error::operator = - copy Error structs
 *
 *	Error::Set() - add an error message into an Error struct
 *	Error::operator << - add argument to error message
 *	Error::Sys() - add a system error message into an Error struct
 *	Error::Net() - add a network error message into an Error struct
 *
 * 	Error::GetId() - get an individual Error item
 *	Error::CheckId() - is first error a particular code?
 *	Error::CheckIdi() - is i'th error a particular code?
 *	Error::CheckIds() - is any error a particular code?
 *	Error::Fmt() - format an error message
 *	Error::GetDict() - get StrDict of error parameters
 *
 *	Error::Marshall() - pack an Error into a StrBuf/StrDict
 *	Error::UnMarshall() - unpack an Error from a StrBuf/StrDict
 *	Error::Snap() - after UnMarshall, copy all data into Error
 *
 *	Error::Dump() - dump out error struct, for debugging
 */

# ifndef __ERROR_H__
# define __ERROR_H__

class StrBuf;
class StrDict;
class StrPtr;
class ErrorPrivate;

/*
 * ErrorSeverity - how bad is the error?
 */

enum ErrorSeverity {

	E_EMPTY = 0,	// nothing yet
	E_INFO = 1,	// something good happened
	E_WARN = 2,	// something not good happened
	E_FAILED = 3,	// user did somthing wrong
	E_FATAL = 4	// system broken -- nothing can continue

} ;

/*
 * ErrorID - an error code and message
 * ErrorOf() - construct an ErrorID from bits
 *
 *	sev - ErrorSeverity (4 bits)
 *	arg - # of arguments, error specific (4 bits)
 *	gen - generic error, defined in errornum.h (8 bits)
 *	sub - subsystem id, defined in errornum.h (6 bits)
 *	cod - code within subsystem, error specific (10 bits)
 */

struct ErrorId {
	int		code;		// ErrorOf
	const char	*fmt;

	int	SubCode() const		{ return (code >> 0) & 0x3ff; }
	int	Subsystem() const	{ return (code >> 10) & 0x3f; }
	int	Generic() const		{ return (code >> 16) & 0xff; }
	int	ArgCount() const	{ return (code >> 24) & 0x0f; }
	int	Severity() const	{ return (code >> 28) & 0x0f; }
	int	UniqueCode() const	{ return code & 0xffff; }

} ;

struct ErrorIdMap {
    ErrorId incomingError;
    ErrorId outgoingError;
};

# define ErrorOf( sub, cod, sev, gen, arg ) \
	((sev<<28)|(arg<<24)|(gen<<16)|(sub<<10)|cod)

enum ErrorFmtOps {
	EF_PLAIN = 0x00,	// for info messages
	EF_INDENT = 0x01,	// indent each line with \t
	EF_NEWLINE = 0x02,	// terminate buffer with \n
	EF_NOXLATE = 0x04,	// don't use P4LANGUAGE formats
	EF_CODE = 0x08		// include error code
} ;

/*
 * class Error - hold layered errors.
 */

class Error {

    public:
			Error() { ep = 0; severity = E_EMPTY; }
			~Error();

	void 		operator =( const Error &source );
	Error &		Merge( const Error &source );

	void		Clear() { severity = E_EMPTY; }
	const ErrorId  *MapError( const struct ErrorIdMap map[] );

	int		Test() const { return severity > E_INFO; }
	int		IsInfo() const { return severity == E_INFO; }
	int		IsWarning() const { return severity == E_WARN; }
	int		IsError() const { return severity >= E_FAILED; }
	int		IsFatal() const { return severity == E_FATAL; }

	int		GetSeverity() const { return severity; }
	const char *	FmtSeverity() const { return severityText[severity]; }
	int		GetGeneric() const { return genericCode; }

	// Set errors, the new way

	Error &		Set( const ErrorId &id );

	Error &		Set( ErrorSeverity s, const char *fmt )
			{
			    ErrorId eid;
			    eid.code = ErrorOf( 0, 0, s, 0, 0 );
			    eid.fmt = fmt;
			    return Set( eid );
			}

	Error &		operator <<( const StrPtr &arg );
	Error &		operator <<( const StrPtr *arg );
	Error &		operator <<( const char *arg );
	Error &		operator <<( int arg );

	// Save system errors

	void		Sys( const char *op, const char *arg );
	void		Net( const char *op, const char *arg );
	void		Net2( const char *op, const char *arg );
	static bool	IsSysError(); // is there a global system error?
	static bool	IsNetError(); // is there a global network error?
	static bool	IsSysNetError(); // is there a global (system or network) error?
	static int	GetNetError(); // return the last network error code
	static void	SetNetError(int err); // set the "last" network error code
	static StrPtr &	StrNetError(StrBuf &buf); // get text of last network error
	static StrPtr &	StrError(StrBuf &buf); // get text of last system (or network) error
	static StrPtr &	StrError(StrBuf &buf, int errnum); // get text of specified error

	// Output

	int		GetErrorCount() const;
	void		LimitErrorCount();

	ErrorId *	GetId( int i ) const;

	int		CheckId( const ErrorId &id ) const
			{ return CheckIdi( 0, id ); }
	int		CheckIdi( const int i, const ErrorId &id ) const
			{ return severity &&
				GetId(i)->Subsystem() == id.Subsystem() &&
				GetId(i)->SubCode()   == id.SubCode(); }
	int		CheckIds( const ErrorId &id ) const;

	StrDict *	GetDict();

	void		Fmt( StrBuf &buf, int opts ) const;
	void		Fmt( StrBuf *buf, int opts = EF_NEWLINE ) const 
			{ Fmt( *buf, opts ); }
	void		Fmt( int i, StrBuf &buf, int opts ) const;

	// Moving across client/server boundary
	// 0 is pre-2002.1
	// 1 is 2002.1
	// 2 is 2002.1 loopback (not used by client)

	void		Marshall0( StrBuf &out ) const;
	void		Marshall1( StrDict &out, int uniquote = 0 ) const;
	void		Marshall2( StrBuf &out ) const;

	void		UnMarshall0( const StrPtr &in );
	void		UnMarshall1( StrDict &in );
	void		UnMarshall2( const StrPtr &in );

	void		Snap();

	// Debugging

	void		Dump( const char *trace );

    private:

	// Remainder is the actual error info

	ErrorSeverity	severity;	// of worst error
	int		genericCode;	// of worst error

	ErrorPrivate	*ep;		// for actual error data

	static const char *severityText[];
} ;

// Note Macro can only be used in methods with void return values
# define IF_ERROR_STOP( e ) \
	if( (e)->Test() ) \
	    return;
# define IF_ERROR_GOTO_FAIL( e ) \
	if( (e)->Test() ) \
	    goto fail;
# endif /* __ERROR_H__ */
