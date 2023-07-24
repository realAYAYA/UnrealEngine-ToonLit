/*
 * Copyright 2018 Perforce Software.  All rights reserved.
 *
 * This file is part of Perforce - the FAST SCM System.
 */

/*
 * msgscript.h - definitions of errors for scripting subsystem.
 */

class MsgScript {

    public:

	static ErrorId ScriptRuntimeError;
	static ErrorId ScriptMaxRunErr;
	static ErrorId DoNotBlameTheScript;
	static ErrorId ExtAddChangeDesc;
	static ErrorId ExtEditChangeDesc;
	static ErrorId ExtOverChangeDesc;
	static ErrorId ExtDelChangeDesc;
	static ErrorId ExtLoadErr;
	static ErrorId ExtDisabled;
	static ErrorId ExtCodingErr;
	static ErrorId ExtCodingGenErr;
	static ErrorId DevErr;
	static ErrorId ExtClientMsg;
	static ErrorId ExtClientError;
	static ErrorId ExtClientPrompt;
	static ErrorId ExtClientCmdRejected;
	static ErrorId ExtClientRuntimeFail;
	static ErrorId ExtResourceErr;
	static ErrorId ScriptLangUnknown;
	static ErrorId ScriptLangVerUnknown;
	static ErrorId ExtWrongProduct;
	static ErrorId ExtScriptNotInBuild;
	static ErrorId OsExitRealError;
	static ErrorId ExtCertAddChangeDesc;
	static ErrorId ExtCertDelChangeDesc;
	static ErrorId GenericFatal;
};
