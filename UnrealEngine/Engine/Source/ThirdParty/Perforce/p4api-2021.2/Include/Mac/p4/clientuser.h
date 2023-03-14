/*
 * Copyright 1995, 2000 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * ClientUser - user interface primitives
 *
 * Public classes:
 *
 *	ClientUser - user interface for client services
 *
 * Note that not all methods are always used.  Here's a guideline:
 *
 * 	Used by almost all Perforce commands:
 *
 *		Finished
 *		HandleError
 *		OutputBinary
 *		OutputError
 *		OutputInfo
 *		OutputText
 *		File
 *
 *	Used only by commands that read the client's stdin:
 *		
 *		InputData
 *
 *	Used only by 'p4 fstat' and some other commands when the
 *	protocol variable 'tag' is set:
 *
 *		OutputStat
 *
 *	Used only by interactive commands that can generally be avoided:
 *
 *		Diff
 *		Edit
 *		ErrorPause
 *		Prompt
 *
 *	Used only by the default ClientUser implementation:
 *
 *		Help
 *		Merge
 *
 * Public methods:
 *
 *	ClientUser::InputData() - provide data to 'p4 spec-command -i'; 
 *		spec-command is branch, change, client, job, label, protect, 
 *		user, etc.
 *
 *	ClientUser::HandleError() - process error data, the result of a failed
 *		command.  Default is to format output and call OutputError().
 *
 *	ClientUser::Message() - output error or tabular data.  This is the
 *		2002.1 replacement for OutputInfo/Error: earlier servers
 *		will invoke still HandleError() and OutputInfo().
 *
 *	ClinetUser::OutputError() - output error data, the result of a failed
 *		command.
 *
 *	ClientUser::OutputInfo() - output tabular data, the result of most
 *		commands that report metadata.
 *
 *	ClientUser::OutputBinary() - output binary data, generally the result
 *		of 'p4 print binary_file'.
 *
 *	ClientUser::OutputText() - output text data, generally the result
 *		of 'p4 print text_file'.
 *
 *	ClientUser::OutputStat() - output results of 'p4 fstat'; requires
 *		calling StrDict::GetVar() to get the actual variable results.
 *
 *	ClientUser::Prompt() - prompt the user, and wait for a response.
 *		Optionally takes a noOutput flag to suppress the prompt and
 *		just collect the response.
 *
 *	ClientUser::ErrorPause() - print an error message and wait for the
 *		user before continuing.
 *
 *	ClientUser::Edit() - bring the user's editor up on a file; generally
 *		part of 'p4 spec-command'.
 *
 *	ClientUser::Diff() - diff two files, and display the results; the
 *		result of 'p4 diff'. Optionally takes a FileSys object to
 *		direct output to a target file instead of stdout.
 *
 *	ClientUser::Merge() - merge three files and save the results; the
 *		result of saying 'm' to the resolve dialog of 'p4 resolve'.
 *
 *	ClientUser::Help() - dump out a block of help text to the user;
 *		used by the resolve dialogs.
 *
 *	ClientUser::File() - produce a FileSys object for reading
 *		and writing files in client workspace.
 *
 *	ClientUser::Finished() - called when tagged client call is finished.
 */

class Enviro;
class ClientMerge;
class ClientResolveA;
class ClientProgress;
class ClientTransfer;
class ClientSSO;
class ClientApi;

class ClientUser {

    public:
			ClientUser(
			    int autoLoginPrompt = 0,
			    int apiVersion   = -1 );

	virtual		~ClientUser();

	virtual void	InputData( StrBuf *strbuf, Error *e );

	virtual void 	HandleError( Error *err );
	virtual void 	Message( Error *err );
	virtual void 	OutputError( const char *errBuf );
	virtual void	OutputInfo( char level, const char *data );
	virtual void 	OutputBinary( const char *data, int length );
	virtual void 	OutputText( const char *data, int length );

	virtual void	OutputStat( StrDict *varList );
	virtual int	OutputStatPartial( StrDict * ) { return 0; }
	// The above method returns 0 to carry the fstat partials or non-0
	// to drop them (return 1 if you print them as you get them)

	virtual void	Prompt( Error *err, StrBuf &rsp, 
				int noEcho, Error *e );
	virtual void	Prompt( Error *err, StrBuf &rsp,
				int noEcho, int noOutput, Error *e );
	virtual void	Prompt( const StrPtr &msg, StrBuf &rsp, 
				int noEcho, Error *e );
	virtual void	Prompt( const StrPtr &msg, StrBuf &rsp,
				int noEcho, int noOutput, Error *e );
	virtual void	ErrorPause( char *errBuf, Error *e );
	virtual void	HandleUrl( const StrPtr *url );

	virtual void	Edit( FileSys *f1, Error *e );

	virtual void	Diff( FileSys *f1, FileSys *f2, int doPage, 
				char *diffFlags, Error *e );
	virtual void	Diff( FileSys *f1, FileSys *f2, FileSys *fout,
				int doPage, char *diffFlags, Error *e );

	virtual void	Merge( FileSys *base, FileSys *leg1, FileSys *leg2, 
				FileSys *result, Error *e );

	virtual int	Resolve( ClientMerge *m, Error *e );
	virtual int	Resolve( ClientResolveA *r, int preview, Error *e );

	virtual void	Help( const char *const *help );

	virtual FileSys	*File( FileSysType type );
	virtual ClientProgress *CreateProgress( int );
	virtual int	ProgressIndicator();

	virtual void	Finished() {}

	StrDict		*varList;	// (cheesy) access to RPC buffer
	Enviro		*enviro;	// (cheesy) access to Client's env

	static void	Edit( FileSys *f1, Enviro * env, Error *e );

	static void	RunCmd( const char *command, const char *arg1, 
				const char *arg2, const char *arg3, 
				const char *arg4, const char *arg5,
				const char *pager, 
				Error *e );

	virtual void	SetOutputCharset( int );
	virtual void	DisableTmpCleanup();
	virtual void	SetQuiet();
	virtual int	CanAutoLoginPrompt();
	virtual int	IsOutputTaggedWithErrorLevel();
	
	// ClientTransfer allows the threading behavor of parallel sync/submit
	// to be overridden by the client application. The ClientTransfer will
	// be deleted with the the ClientUser.
	virtual void	SetTransfer( ClientTransfer* t );
	virtual ClientTransfer*	GetTransfer() { return transfer; }

	// ClientSSO allows the P4LOGINSSO behavor to be overriden from the
	// application using the P4API. This is intended for use where an SSO
	// agent would not be able to be directly invoked on the user's machine
	virtual void	SetSSOHandler( ClientSSO* t );
	virtual ClientSSO*	GetSSOHandler() { return ssoHandler; }

	// Output... and Help must use 'const char' instead of 'char'
	// The following will cause compile time errors for using 'char'
	virtual int 	OutputError( char *errBuf )
	    { OutputError( (const char *)errBuf ); return 0; }
	virtual int	OutputInfo( char level, char *data )
	    { OutputInfo( level, (const char *)data ); return 0; }
	virtual int 	OutputBinary( char *data, int length )
	    { OutputBinary( (const char *)data, length ); return 0; }
	virtual int 	OutputText( char *data, int length )
	    { OutputText( (const char *)data, length ); return 0; }
	virtual int	Help( char *const *help )
	    { Help( (const char * const *)help ); return 0; }

    private:
	int		binaryStdout;	// stdout is in binary mode
	int		quiet;		// OutputInfo does nothing.
	int		autoLogin;	// Can this implementation autoprompt

    protected:
	int		outputCharset;	// P4CHARSET for output
	StrBuf		editFile;
	int		outputTaggedWithErrorLevel;	// "p4 -s cmd" yes/no
	int		apiVer;
	ClientTransfer	*transfer;
	ClientSSO	*ssoHandler;

} ;

class ClientUserProgress : public ClientUser {
    public:
			ClientUserProgress( int autoLoginPrompt = 0,
			                    int apiVersion = -1 ) :
			           ClientUser( autoLoginPrompt, apiVersion ) {}
	virtual ClientProgress *CreateProgress( int );
	virtual int	ProgressIndicator();
} ;

class ClientTransfer {
    public:
	virtual		~ClientTransfer() {}

	virtual int	Transfer( ClientApi *client,
			          ClientUser *ui,
			          const char *cmd,
			          StrArray &args,
			          StrDict &pVars,
			          int threads,
			          Error *e ) = 0;
};

enum ClientSSOStatus {
	CSS_PASS,	// SSO succeeded (result is an authentication token)
	CSS_FAIL,	// SSO failed (result will be logged as error message)
	CSS_UNSET,	// Client has no SSO support
	CSS_EXIT,	// Stop login process
	CSS_SKIP	// Fall back to default P4API behavior
};

class ClientSSO {
    public:
	virtual			~ClientSSO() {}

	virtual ClientSSOStatus	Authorize( StrDict &vars,
				           int maxLength,
				           StrBuf &result ) = 0;
};

/*
 * StrDict now provides the GetVar() interface for OutputStat();
 * ClientVarList defined for backward compatability.
 */

typedef StrDict ClientVarList;
