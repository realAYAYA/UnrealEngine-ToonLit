#pragma once

#include "P4BridgeClient.h"

class P4BridgeClient;

class ClientManager : public DoublyLinkedList
{

public:
	ClientManager(void);
	virtual ~ClientManager(void);

	P4BridgeClient* CreateNewUI(int cmdId);

	P4BridgeClient* GetUI(int cmdId);

	P4BridgeClient* GetDefaultUI();

	void ReleaseUI(int cmdId);

	// If the P4 Server is Unicode enabled, the output will be in
	// UTF-8 or UTF-16 based on the char set specified by the client
	bool isUnicode;
	
	// If the P4 Server is Unicode enabled, the output will be in
	// UTF-8 or UTF-16 based on the char set specified by the client
	void UseUnicode(int val) { isUnicode = (val != 0); }

	// Put the calls to the callback in Structured Exception Handlers to catch
	//  any problems in the call like bad function pointers.
	void CallTextResultsCallbackFn( int cmdId, const char *data) ;
	void CallInfoResultsCallbackFn( int cmdId, char level, const char *data );
	void CallTaggedOutputCallbackFn( int cmdId, int objId, const char *pKey, const char * pVal );
	void CallErrorCallbackFn( int cmdId, int severity, int errorId, const char * errMsg );
	void CallBinaryResultsCallbackFn( int cmdId, void * data, int length );

	// Set the call back function to receive the tagged output
	void SetTaggedOutputCallbackFn(IntTextTextCallbackFn* pNew);

	// Set the call back function to receive the error output
	void SetErrorCallbackFn(IntIntIntTextCallbackFn* pNew);

	void Prompt( int cmdId, const StrPtr &msg, StrBuf &rsp, 
				int noEcho, Error *e );

	void SetPromptCallbackFn( PromptCallbackFn * pNew);

	// Set the call back function to receive the information output
	void SetInfoResultsCallbackFn(IntIntTextCallbackFn* pNew);

	// Set the call back function to receive the text output
	void SetTextResultsCallbackFn(TextCallbackFn* pNew);

	// Set the call back function to receive the binary output
	void SetBinaryResultsCallbackFn(BinaryCallbackFn* pNew);

	// Callbacks for handling interactive resolve
	int	Resolve( int cmdId, ClientMerge *m, Error *e );
	int	Resolve( int cmdId, ClientResolveA *r, int preview, Error *e );

	void SetResolveCallbackFn(ResolveCallbackFn * pNew);
	void SetResolveACallbackFn(ResolveACallbackFn * pNew);

private:

	P4BridgeClient* defaultUI;

	// Save the error from an exception to be reported in the exception handler block
	// to prevent possible recursion if it happens when reporting an error.
	StrBuf * ExceptionError;

	// Internal exception handler to handle platform exceptions i.e. Null 
	//      pointer access
	int HandleException(unsigned int c, struct _EXCEPTION_POINTERS *e);

	// Call back function used to send text results back to the client
	//
	// The function prototype is:
	//
	// void TextCallbackFn(const char*);
	//
	// The first parameter is the text data. Multiple callbacks might be made 
	//      a single command and the text should be concatenated to obtain
	//      the entire results.

	TextCallbackFn* pTextResultsCallbackFn;

	// Call back function used to send informational messages back 
	//      to the client. This is generally the output from a command
	//      when not using tagged protocol.
	//
	// The function prototype is:
	//
	// void void IntTextCallbackFn(int, const char*);
	//
	// The first parameter is the message level. Generally, a message of a 
	//      higher level is a sub field of the preceding lower level message,
	//      i.e a level zero message may be followed by one or more level one
	//      messages containing details about the output. This is an older way
	//      of grouping output into logical objects that is superseded by
	//      tagged output.
	// The second parameter is the information

	IntIntTextCallbackFn* pInfoResultsCallbackFn;
		
	// Callback function used to send tagged output to the client. Tagged or
	// data is sent from the api as one or more StrDict objects representing
	// one or more data objects. Each dictionary represents one data objects.
	// I.e. for a list of file stats, each dictionary object will represent the
	// data for one file. The data is stored as Key:Value pairs, i.e 
	// Filename:"MyCode.cpp"
	//
	// The function prototype is:
	//
	//  void IntTextTextCallbackFn(int, const char*, const char*);
	//      
	// The first parameter is an object ID, a given command can return 
	//      multiple objects. ID's will start at 0 and increment for each
	//      successive object.
	// The second parameter is the text 'key'
	// The third parameter is the text value
	// 
	// The client will receive the call back multiple times, once for each
	// Key:Value pair in all of the dictionaries. For m data objects each
	// with n Key:Value pairs, the client will receive m * n call backs. The
	// object id can be used to group the data with there correct objects.
	//

	IntTextTextCallbackFn* pTaggedOutputCallbackFn;
	
	// Call back function used to send error messages back to the client
	//
	// The function prototype is:
	//
	// void IntTextCallbackFn( int, const char* );
	//
	// The first parameter is the error level, i.e Error or Warning
	// The second parameter is the error message

	IntIntIntTextCallbackFn* pErrorCallbackFn;
	
	// Call back function used to send text results back to the client
	//
	// The function prototype is:
	//
	// void BinaryCallbackFn(const void *, int);
	//
	// The first parameter is the binary data. Multiple callbacks might be made
	//      a single command and the results should be concatenated to obtain
	//      the entire results.
	// The second parameter is the size of the data in bytes.

	BinaryCallbackFn* pBinaryResultsCallbackFn;
	
	PromptCallbackFn * pPromptCallbackFn;

	ResolveCallbackFn * pResolveCallbackFn;
	ResolveACallbackFn * pResolveACallbackFn;

	int Resolve_int( int cmdId, P4ClientMerge *merger);
	int Resolve_int( int cmdId, P4ClientResolve *resolver, int preview, Error *e);
};

