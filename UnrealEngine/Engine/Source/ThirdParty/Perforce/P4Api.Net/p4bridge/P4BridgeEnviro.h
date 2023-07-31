#pragma once
/*******************************************************************************

Copyright (c) 2017, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: P4BridgeEnviro.h
 *
 * Author	: nrm
 *
 * Description	:  P4BridgeEnviro - a wrapper for p4api Enviro
 *     fixes Windows issues with the 16.1 Enviro ordering changes.
 *     
 * We've fixed the ordering differences in 16.1 so that Enviro::Get() now gets with this precedence:
 * 1. In memory Enviro updates (Enviro::Update)
 * 2. P4CONFIG
 * 3. Service registry (NT only)
 * 4. ENV (system environment)
 * 5. P4ENVIRO (p4 set on UNIX)
 * 6. User Registry (NT only - p4 set on NT)
 * 7. System registry (NT only)
 * 8. Unset (cached 'unfound')
 * 
 * This means that calling Enviro::Set() may give you a result from a higher precedence location:
 * P4PORT set in the system environment will override the P4ENVIRO file or the user registry.
 * This is the intended behavior. Also, please remember that calling Enviro::Set() will set the
 * registry that all Perforce Clients will read:  you need to be careful not to change the user's
 * environment when they're not expecting it.
 * 
 * P4BridgeEnviro fixes the Update() call which is broken in the NT C++ API - see job086738 for more...
 * It provides in memory storage of variables by copying a portion of the existing Enviro
 * class to be used to maintain our own Update() level Cache in the Bridge
 * Should the Enviro() class in the C++ API get refactored, a lot of this cut and paste code could be removed.
 ******************************************************************************/
#include <enviro.h>

#if 1
typedef Enviro P4BridgeEnviro;
#else
class EnviroTable;
struct EnviroItem;
class Error;
class StrBuf;
class StrPtr;
class StrArray;
class FileSys;
struct KeyPair;

class P4BridgeEnviro : public Enviro
{
public:
	P4BridgeEnviro();
	~P4BridgeEnviro();

	char	*GetLocal( const char *var );
	char	*Get( const char *var );
	void	Update( const char *var, const char *value );
	void	ClearUpdate(const char *var) const;
	void	Config( const StrPtr &cwd );
	const	StrPtr & GetConfig();
	const	StrArray* GetConfigs();
	void	LoadConfig( const StrPtr &cwd, const StrBuf setfile, int checkSyntax );
	void	ReadConfig(FileSys* f, Error* e, int checkSyntax, Enviro::ItemType ty);
private:
	void Setup();
	EnviroItem  *GetItem( const char *var );
	EnviroTable	*symbolTab;
	StrBuf		configFile;
	StrArray	*configFiles;
	int			charset;
};
#endif