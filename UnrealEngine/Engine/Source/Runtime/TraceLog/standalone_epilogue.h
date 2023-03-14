// Copyright Epic Games, Inc. All Rights Reserved.

#if PLATFORM_WINDOWS
#	pragma warning(pop)
#endif

#if TRACE_UE_COMPAT_LAYER

#if PLATFORM_WINDOWS
#	if defined(UNICODE) || defined(_UNICODE)
#		undef TEXT
#		undef TCHAR
#		define TEXT(x)	L##x
#	endif
#endif

#endif // TRACE_UE_COMPAT_LAYER

#include <string_view>

#define TRACE_EVENT_DEFINE			UE_TRACE_EVENT_DEFINE
#define TRACE_EVENT_BEGIN			UE_TRACE_EVENT_BEGIN
#define TRACE_EVENT_BEGIN_EXTERN	UE_TRACE_EVENT_BEGIN_EXTERN
#define TRACE_EVENT_FIELD			UE_TRACE_EVENT_FIELD
#define TRACE_EVENT_END				UE_TRACE_EVENT_END
#define TRACE_LOG					UE_TRACE_LOG
#define TRACE_LOG_SCOPED			UE_TRACE_LOG_SCOPED
#define TRACE_LOG_SCOPED_T			UE_TRACE_LOG_SCOPED_T
#define TRACE_CHANNEL				UE_TRACE_CHANNEL
#define TRACE_CHANNEL_EXTERN		UE_TRACE_CHANNEL_EXTERN
#define TRACE_CHANNEL_DEFINE		UE_TRACE_CHANNEL_DEFINE

namespace trace = UE::Trace;

#define TRACE_PRIVATE_CONCAT_(x, y)		x##y
#define TRACE_PRIVATE_CONCAT(x, y)		TRACE_PRIVATE_CONCAT_(x, y)
#define TRACE_PRIVATE_UNIQUE_VAR(name)	TRACE_PRIVATE_CONCAT($trace_##name, __LINE__)

TRACE_CHANNEL_EXTERN(CpuChannel)

namespace UE {
namespace Trace {

struct TraceCpuScope
{
			~TraceCpuScope();
	void	Enter(int ScopeId);
	int		_ScopeId = 0;
};

int ScopeNew(const std::string_view& Name);

} // namespace Trace
} // namespace UE

#define TRACE_CPU_SCOPE(name) \
	using namespace std::literals; \
	trace::TraceCpuScope TRACE_PRIVATE_UNIQUE_VAR(cpu_scope); \
	if (CpuChannel) { \
		static int TRACE_PRIVATE_UNIQUE_VAR(scope_id); \
		if (0 == TRACE_PRIVATE_UNIQUE_VAR(scope_id)) \
			TRACE_PRIVATE_UNIQUE_VAR(scope_id) = trace::ScopeNew(name##sv); \
		TRACE_PRIVATE_UNIQUE_VAR(cpu_scope).Enter(TRACE_PRIVATE_UNIQUE_VAR(scope_id)); \
	} \
	do {} while (0)

#if TRACE_IMPLEMENT

////////////////////////////////////////////////////////////////////////////////
TRACE_CHANNEL_DEFINE(CpuChannel)

TRACE_EVENT_BEGIN(CpuProfiler, EventSpec, NoSync|Important)
	TRACE_EVENT_FIELD(uint32, Id)
	TRACE_EVENT_FIELD(UE::Trace::AnsiString, Name)
TRACE_EVENT_END()

TRACE_EVENT_BEGIN(CpuProfiler, EventBatch, NoSync)
	TRACE_EVENT_FIELD(uint8[], Data)
TRACE_EVENT_END()

namespace UE {
namespace Trace {
namespace Private {

static int32_t encode32_7bit(int32_t value, void* __restrict out)
{
	// Calculate the number of bytes
#if 0
	int32_t msb_test = (value << sizeof(value)) | 0x10;
#if _MSC_VER
	unsigned long bit_index;
	_BitScanReverse(&bit_index, msb_test);
#else
	int32_t leading_zeros = __builtin_clz(msb_test);
	int32_t bit_index = ((sizeof(value) * 8) - 1) - leading_zeros;
#endif
	int32_t length = (bit_index + 3) / 7;
#else
	int32_t length = 1;
	length += (value >= (1 <<  7));
	length += (value >= (1 << 14));
	length += (value >= (1 << 21));
#endif

	// Add a gap every eigth bit for the continuations
	int32_t ret = value;
	ret = (ret & 0x0000'3fff) | ((ret & 0x0fff'c000) << 2);
	ret = (ret & 0x007f'007f) | ((ret & 0x3f80'3f80) << 1);

	// Set the bits indicating another byte follows
	int32_t continuations = 0x0080'8080;
	continuations >>= (sizeof(value) - length) * 8;
	ret |= continuations;

	::memcpy(out, &ret, sizeof(value));

	return length;
}

static int32_t encode64_7bit(int64_t value, void* __restrict out)
{
	// Calculate the output length
#if 0
	int64_t msb_test = (value << sizeof(value)) | 0x100ull;
#if _MSC_VER
	unsigned long bit_index;
	_BitScanReverse64(&bit_index, msb_test);
#else
	int32_t leading_zeros = __builtin_clzll(msb_test);
	int32_t bit_index = ((sizeof(value) * 8) - 1) - leading_zeros;
#endif
	int32_t length = (bit_index - 1) / 7;
#else
	uint32_t length = 1;
	length += (value >= (1ll <<  7));
	length += (value >= (1ll << 14));
	length += (value >= (1ll << 21));
	length += (value >= (1ll << 28));
	length += (value >= (1ll << 35));
	length += (value >= (1ll << 42));
	length += (value >= (1ll << 49));
#endif

	// Add a gap every eigth bit for the continuations
	int64_t ret = value;
	ret = (ret & 0x0000'0000'0fff'ffffull) | ((ret & 0x00ff'ffff'f000'0000ull) << 4);
	ret = (ret & 0x0000'3fff'0000'3fffull) | ((ret & 0x0fff'c000'0fff'c000ull) << 2);
	ret = (ret & 0x007f'007f'007f'007full) | ((ret & 0x3f80'3f80'3f80'3f80ull) << 1);

	// Set the bits indicating another byte follows
	int64_t continuations = 0x0080'8080'8080'8080ull;
	continuations >>= (sizeof(value) - length) * 8;
	ret |= continuations;

	::memcpy(out, &ret, sizeof(value));

	return length;
}

////////////////////////////////////////////////////////////////////////////////
class ThreadBuffer
{
public:
	static void	Enter(uint64_t Timestamp, uint32_t ScopeId) { TlsInstance.EnterImpl(Timestamp, ScopeId); }
	static void	Leave(uint64_t Timestamp)					{ TlsInstance.LeaveImpl(Timestamp); }

private:
				~ThreadBuffer();
	void		Flush(bool Force);
	void		EnterImpl(uint64_t Timestamp, uint32_t ScopeId);
	void		LeaveImpl(uint64_t Timestamp);
	enum
	{
		BufferSize	= 256,
		Overflow	= 16,
		EnterLsb	= 1,
		LeaveLsb	= 0,
	};
	uint64_t	PrevTimestamp = 0;
	uint8_t*	Cursor = Buffer;
	uint8_t		Buffer[BufferSize];

	static thread_local ThreadBuffer TlsInstance;
};

thread_local ThreadBuffer ThreadBuffer::TlsInstance;

////////////////////////////////////////////////////////////////////////////////
ThreadBuffer::~ThreadBuffer()
{
	Flush(true);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadBuffer::Flush(bool Force)
{
	if (!Force && (Cursor <= (Buffer + BufferSize - Overflow)))
		return;

	TRACE_LOG(CpuProfiler, EventBatch, true)
		<< EventBatch.Data(Buffer, uint32(ptrdiff_t(Cursor - Buffer)));

	PrevTimestamp = 0;
	Cursor = Buffer;
}

////////////////////////////////////////////////////////////////////////////////
void ThreadBuffer::EnterImpl(uint64_t Timestamp, uint32_t ScopeId)
{
	Timestamp -= PrevTimestamp;
	PrevTimestamp += Timestamp;
	Cursor += encode64_7bit((Timestamp) << 1 | EnterLsb, Cursor);
	Cursor += encode32_7bit(ScopeId, Cursor);

	Flush(false);
}

////////////////////////////////////////////////////////////////////////////////
void ThreadBuffer::LeaveImpl(uint64_t Timestamp)
{
	Timestamp -= PrevTimestamp;
	PrevTimestamp += Timestamp;
	Cursor += encode64_7bit((Timestamp << 1) | LeaveLsb, Cursor);

	Flush(false);
}

} // namespace Private



////////////////////////////////////////////////////////////////////////////////
int ScopeNew(const std::string_view& Name)
{
	static int volatile NextSpecId = 1;
	int SpecId = Private::AtomicAddRelaxed(&NextSpecId, 1);

	uint32 NameSize = uint32(Name.size());
	TRACE_LOG(CpuProfiler, EventSpec, true, NameSize)
		<< EventSpec.Id(uint32(SpecId))
		<< EventSpec.Name(Name.data(), NameSize);

	return SpecId;
}



////////////////////////////////////////////////////////////////////////////////
TraceCpuScope::~TraceCpuScope()
{
	using namespace Private;

	if (!_ScopeId)
		return;

	uint64 Timestamp = TimeGetTimestamp();
	ThreadBuffer::Leave(Timestamp);
}

////////////////////////////////////////////////////////////////////////////////
void TraceCpuScope::Enter(int ScopeId)
{
	using namespace Private;

	_ScopeId = ScopeId;
	uint64 Timestamp = TimeGetTimestamp();
	ThreadBuffer::Enter(Timestamp, ScopeId);
}

} // namespace Trace
} // namespace UE

#endif // TRACE_IMPLEMENT

/* vim: set noet foldlevel=1 foldmethod=marker : */
