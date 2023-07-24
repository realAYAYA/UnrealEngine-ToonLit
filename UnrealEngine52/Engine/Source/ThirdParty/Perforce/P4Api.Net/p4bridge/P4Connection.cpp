#include "stdafx.h"
#include "P4BridgeClient.h"
#include "P4BridgeServer.h"
#include "P4Connection.h"

#ifdef _DEBUG_MEMORY
P4Connection::P4Connection(P4BridgeServer* pServer, int _cmdId)
	: p4base(tP4Connection)
#else
P4Connection::P4Connection(P4BridgeServer* pServer, int _cmdId)
#endif
{
	cmdId = _cmdId;
	clientNeedsInit = 1;
	ui = new P4BridgeClient(pServer, this);
	isAlive = 1;
}

P4Connection::~P4Connection(void)
{
	if (clientNeedsInit == 0)
	{
		Error e;
		this->Final( &e );
		clientNeedsInit = 1;
	}
	if (ui)
	{
		delete ui;
	}
}

void P4Connection::cancel_command() 
{
	LOG_ENTRY();
	isAlive = 0;
}

// KeepAlive functionality
int	P4Connection::IsAlive()
{
	LOG_DEBUG1(4, "P4Connection:::IsAlive == %d", isAlive);
	return isAlive;
}

bool P4Connection::IsConnected() const
{
	return clientNeedsInit == 0;
}

void P4Connection::Disconnect( void )
{
	if (clientNeedsInit == 0)
	{
		Error e;
		this->Final( &e );

		clientNeedsInit = 1;
	}
}

void P4Connection::SetCharset( CharSetApi::CharSet c, CharSetApi::CharSet filec )
{	
	ClientApi::SetCharset(CharSetApi::Name(c));
	SetTrans( CharSetApi::NOCONV, c, filec, CharSetApi::NOCONV );
}

const StrPtr & P4Connection::GetClient()
{
	return ClientApi::GetClient();
}

static Error err;

void P4Connection::SetClient(const char *c)
{
	ClientApi::SetClient(c);
}

void P4Connection::SetClient( const StrPtr *c )
{
	const char *val = c->Value();
	SetClient(val);
}

// Epic
void P4Connection::SetHost(const char* c)
{
	ClientApi::SetHost(c);
}

void P4Connection::SetHost(const StrPtr* c)
{
	const char* val = c->Value();
	SetHost(val);
}

void P4Connection::SetTicketFile(const char *c)
{
	ClientApi::SetTicketFile(c);
}




