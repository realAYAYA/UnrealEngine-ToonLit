/*
 * Copyright 1995, 1996 Perforce Software.  All rights reserved.
 */

/*
 * ErrorLog.h - report layered errors
 *
 * Class Defined:
 *
 *	ErrorLog - write errors to log/syslog (static)
 *
 * Public methods:
 *
 *	ErrorLog::Report() - blurt out the contents of the Error to stderr
 *	ErrorLog::Abort() - blurt out an error and exit
 *	ErrorLog::Fmt() - format an error message
 *
 *	ErrorLog::SetLog() - redirect Abort() and Report() to named file
 *	ErrorLog::SetTag() - replace standard tag used by Report()
 *
 *	ErrorLog::SetSyslog() - redirect error messages to syslog on UNIX.
 *	ErrorLog::UnsetSyslog() - Cancel syslog redirection. Revert to log file.
 */

class FileSys;

typedef void (*StructuredLogHook)( void *context, const Error *e );

/*
 * class ErrorLog - write errors to log/syslog
 */

class ErrorLog {

    public:
			enum log_types
			{
			    type_none,
			    type_stdout,
			    type_stderr,
			    type_syslog
			};
			ErrorLog(): hook(NULL), context(NULL){ init(); }
			ErrorLog( ErrorLog *from );
			~ErrorLog();

	void		Abort( const Error *e );
	void		SysLog( const Error *e, int tagged, const char *et,
				const char *buf );

	                enum ReportFlags
	                {
	                    REPORT_NO_FLAGS = 0,
	                    REPORT_TAGGED   = 1,
	                    REPORT_HOOKED   = 2,
	                    REPORT_STDIO    = 4,
			    
	                    REPORT_ALL      = 0x3,
	                    REPORT_ABORT    = 0x7
	                } ;

	void		Report( const Error *e ){ Report( e, REPORT_ALL ); }
	void		ReportAbort( const Error *e ){ Report( e, REPORT_ABORT ); }
	void		ReportNoTag( const Error *e ){ Report( e, REPORT_HOOKED ); }
	void		ReportNoHook( const Error *e ){ Report( e, REPORT_TAGGED ); }
	void		Report( const Error *e, int flags );
	void		LogWrite( const StrPtr &, int stdio = 0 );
	void		StdioWrite( const StrPtr &, int err = 1 );

	// Utility methods

	offL_t		Size();
	int		Exists() { return errorFsys != 0; }
	const		char *Name();

	// Global settings

	void		SetLog( const char *file );
	void		SetSyslog() { logType = type_syslog; }
	void		UnsetSyslog() { logType = type_stderr; }
	void		UnsetLogType() { logType = type_none; }
	void		SetTag( const char *tag ) { errorTag = tag; }
	void		EnableCritSec();

	void		Rename( const char *file, Error *e );

	void SetStructuredLogHook( void *ctx, StructuredLogHook hk )
		{ hook = hk; context = ctx; }

    private:
	void		init();

	const 		char *errorTag;
	int		logType;
	FileSys		*errorFsys;

	StructuredLogHook hook;
	void		*context;

	void		*vp_critsec;
} ;

/*
 * AssertError() - in case you need a global error to Abort() on
 */

extern Error AssertError;
extern ErrorLog AssertLog;

