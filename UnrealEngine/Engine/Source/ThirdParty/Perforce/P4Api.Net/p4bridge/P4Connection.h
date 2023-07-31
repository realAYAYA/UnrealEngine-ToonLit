#pragma once

class P4BridgeClient;
class P4BridgeServer;

#ifdef _DEBUG_MEMORY
class P4Connection : public ClientApi, public KeepAlive, p4base
#else
class P4Connection : public ClientApi, public KeepAlive
#endif
{
private:
	P4Connection(void);

	// KeepAlive status
	int	isAlive;
	int cmdId;

	// these are for the connection manager
	P4Connection(P4BridgeServer* pServer, int cmdId);
	virtual ~P4Connection();

	void setId(int id) { cmdId = id; }

	P4BridgeClient* ui;

	bool IsConnected() const;
	friend class P4BridgeServer;

public:

	int getId() { return cmdId; }
	P4BridgeClient* getUi() { return ui;  }

	// has the client been initialized
	int clientNeedsInit;

	void Disconnect( void );

	// KeepAlive functionality
	virtual int	IsAlive();
	void IsAlive(int val) {isAlive = val;}

	void cancel_command();

	void SetCharset( CharSetApi::CharSet c, CharSetApi::CharSet filec );

	const StrPtr	&GetClient();
	void		SetClient( const char *c );
	void		SetClient( const StrPtr *c );

	// Epic
	void		SetHost(const char* c);
	void		SetHost(const StrPtr* c);

	void		SetTicketFile(const char *c);

	unsigned _int64 ReleaseTime;

#ifdef _DEBUG_MEMORY
	    // Simple type identification for registering objects
	virtual int Type(void) {return tP4Connection;}
#endif
};

