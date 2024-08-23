#include "MyTcpServer.h"

#include "IWebSocketNetworkingModule.h"
#include "IWebSocketServer.h"
#include "MyNetFwd.h"

// ========================================================================= //

struct FTcpServerImpl
{
	TUniquePtr<IWebSocketServer> WebSocketServer;
};

// ========================================================================= //

FMyTcpServer::FMyTcpServer()
{
	ListenPort = 0;
	NextConnId = 1;

	ServerImpl = MakeUnique<FTcpServerImpl>();
}

FMyTcpServer::~FMyTcpServer()
{
}

bool FMyTcpServer::Start(const int32 ServerPort)
{
	ListenPort = ServerPort == 0 ? ListenPort : ServerPort;
	if (ListenPort <= 0)
	{
		UE_LOG(LogNetLib, Error, TEXT("%s: [TcpServer] start failed, 端口未初始化."), *FString(__FUNCTION__));
		return false;
	}
	
	if (IsRunning())
	{
		UE_LOG(LogNetLib, Error, TEXT("%s: [TcpServer] start failed, 已经在运行."), *FString(__FUNCTION__));
		return false;
	}

	// 回调: 产生连接时
	FWebSocketClientConnectedCallBack CallBack;
	CallBack.BindLambda([this](INetworkingWebSocket* InWebSocket)->void
	{
		auto TcpPtr = NewConnection();
		const auto SocketPtr = MakeShared<FMySocketServerSide>(TcpPtr->GetId());
		SocketPtr->Init(InWebSocket);
		TcpPtr->Init(SocketPtr);
		TcpPtr->Start();
		Connections.Emplace(TcpPtr->GetId(), TcpPtr);
	});
	
	ServerImpl->WebSocketServer = FModuleManager::Get().LoadModuleChecked<IWebSocketNetworkingModule>(TEXT("WebSocketNetworking")).CreateServer();
	if (!ServerImpl->WebSocketServer || !ServerImpl->WebSocketServer->Init(ServerPort, CallBack))
	{
		UE_LOG(LogNetLib, Error, TEXT("%s: [TcpServer] start failed, 创建服务器实例失败, Port=%d."), *FString(__FUNCTION__), ListenPort);
		ServerImpl->WebSocketServer.Reset();
		return false;
	}

	UE_LOG(LogNetLib, Display, TEXT("%s: [TcpServer] start succssed."), *FString(__FUNCTION__));
	return true;
}

void FMyTcpServer::Stop()
{
	if (ServerImpl->WebSocketServer)
		ServerImpl->WebSocketServer.Reset();

	Connections.Reset();
	NextConnId = 1;
}

bool FMyTcpServer::IsRunning() const
{
	return ServerImpl->WebSocketServer.IsValid();
}

void FMyTcpServer::Tick(float DeltaTime)
{
	ServerImpl->WebSocketServer->Tick();
}

void FMyTcpServer::RemoveConnection(const FTcpConnectionPtr& InConn)
{
	Connections.Remove(InConn->GetId());
}

uint64 FMyTcpServer::GenerateConnId()
{
	return NextConnId++;
}

FTcpConnectionPtr FPbTcpServer::NewConnection()
{
	auto Ptr = MakeShared<FPbConnection>(GenerateConnId());

	Ptr->SetPackageCallback([this](FPbConnectionPtr InConn, uint64 InCode, FMyDataBufferPtr InMessage)
	{
		if (PackageCallback)
			PackageCallback(InConn, InCode, InMessage);
	});

	Ptr->SetConnectedCallback([this](FPbConnectionPtr InConn)
	{
		if (ConnectedCallback)
			ConnectedCallback(InConn);
	});

	Ptr->SetDisconnectedCallback([this](FPbConnectionPtr InConn)
	{
		if (DisconnectedCallback)
			DisconnectedCallback(InConn);
		
		RemoveConnection(InConn);
	});

	Ptr->SetErrorCallback([this](FPbConnectionPtr InConn)
	{
		if (ErrorCallback)
			ErrorCallback(InConn);
	});
	
	return Ptr;
}
