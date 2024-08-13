#pragma once

#include "Containers/Array.h"
#include "Templates/UniquePtr.h"
#include "Templates/SharedPointer.h"
#include "Templates/Function.h"

// 工作队列
class MCOMMON_API FMyJobQueue
{
	
public:

	struct FJob
	{
		TFunction<void()> Task;
		bool bQuit = false;
	};
	typedef TSharedPtr<FJob, ESPMode::ThreadSafe> FJobPtr;

	FMyJobQueue();
	~FMyJobQueue();

	void PostJob(const TFunction<void()>& Func);  // 投递任务
	void PostQuitJob(const TFunction<void()>& Func);  // 投递结束任务(此任务结束任务队列就会被清空)
	uint32 DoWork();  // 处理队列中的所有任务

	void Clear();

	bool IsRunning() const;
	
private:

	void ReloadWorkQueue();  // 将`接收队列`中所有任务迁移至`工作队列`

	// TODO(hudawei): 可考虑直接使用 TQueue<FJobPtr, EQueueMode::Mpsc> 代替双队列
	TQueue<FJobPtr> IncomingQueue;  // 接收队列
	TQueue<FJobPtr> WorkQueue;  // 工作队列
	FCriticalSection Mutex;  // 队列锁

	bool bRunning = true;
};