#include "pch.h"
#include "SpatialAudioClientInterop.h"

#include <winrt/Windows.Media.Devices.h>

#include <wrl.h>
#include <mmdeviceapi.h>
#include <map>
#include <mutex>

using namespace Microsoft::WRL;
using namespace winrt::Windows::Media::Devices;

// Class which implmenets the WinRT spatial audio client
class SpatialAudioClientRendererWinRT :
	public RuntimeClass<RuntimeClassFlags<ClassicCom>, FtmBase, IActivateAudioInterfaceCompletionHandler, ISpatialAudioObjectRenderStreamNotify>
{
public:
	enum RenderState {
		Inactive = 0,
		Active,
		Resetting
	};

	SpatialAudioClientRendererWinRT() :
		m_SpatialAudioClient(nullptr),
		m_SpatialAudioStream(nullptr),
		m_RenderState(RenderState::Inactive),
		m_bufferCompletionEvent(nullptr),
		m_MaxDynamicObjects(0),
		m_SampleRate(0),
		m_NumSources(0)
	{
	}

	HRESULT InitializeAudioDeviceAsync(UINT32 InNumSources, UINT32 InSampleRate)
	{
		HRESULT hr = S_OK;

		// Don'tneed to reinitialize if we've already got a SAC
		if (nullptr == m_SpatialAudioClient)
		{
			m_SampleRate = InSampleRate;
			m_NumSources = InNumSources;

			ComPtr<IActivateAudioInterfaceAsyncOperation> AsyncOperation;

			// Get a string representing the Default Audio Device Renderer
			m_DeviceIdString = MediaDevice::GetDefaultAudioRenderId(AudioDeviceRole::Default);

			// This call must be made on the main UI thread.  Async operation will call back to 
			// IActivateAudioInterfaceCompletionHandler::ActivateCompleted, which must be an agile interface implementation
			hr = ActivateAudioInterfaceAsync(m_DeviceIdString.c_str(), __uuidof(ISpatialAudioClient), nullptr, this, &AsyncOperation);
			if (FAILED(hr))
			{
				m_RenderState = RenderState::Inactive;
			}
		}

		return hr;
	}

	bool IsActive() { return m_RenderState == RenderState::Active; };
	bool IsResetting() { return m_RenderState == RenderState::Resetting; }
	void Reset() { m_RenderState = RenderState::Resetting; }
	UINT32 GetMaxDynamicObjects() { return m_MaxDynamicObjects; }

	STDMETHOD(ActivateCompleted)(IActivateAudioInterfaceAsyncOperation* InOperation)
	{
		HRESULT hr = S_OK;
		HRESULT hrActivateResult = S_OK;
		ComPtr<IUnknown> punkAudioInterface = nullptr;

		// Check for a successful activation result
		hr = InOperation->GetActivateResult(&hrActivateResult, &punkAudioInterface);
		if (SUCCEEDED(hr) && SUCCEEDED(hrActivateResult))
		{
			// Get the pointer for the Audio Client
			punkAudioInterface.Get()->QueryInterface(IID_PPV_ARGS(&m_SpatialAudioClient));
			if (nullptr == m_SpatialAudioClient)
			{
				hr = E_FAIL;
				goto exit;
			}

			// Store the max dynamic object count
			//Note: we do not actually use this value other than to put a warning in the log.
			hr = m_SpatialAudioClient->GetMaxDynamicObjectCount(&m_MaxDynamicObjects);
			if (FAILED(hr))
			{
				hr = E_FAIL;
				goto exit;
			}

			// Check the available rendering formats 
			ComPtr<IAudioFormatEnumerator> audioObjectFormatEnumerator;
			hr = m_SpatialAudioClient->GetSupportedAudioObjectFormatEnumerator(&audioObjectFormatEnumerator);

			// WavFileIO is helper class to read WAV file
			WAVEFORMATEX* Format = nullptr;
			UINT32 audioObjectFormatCount;
			hr = audioObjectFormatEnumerator->GetCount(&audioObjectFormatCount); // There is at least one format that the API accept
			if (audioObjectFormatCount == 0)
			{
				hr = E_FAIL;
				goto exit;
			}

			// Select the most favorable format, first one
			hr = audioObjectFormatEnumerator->GetFormat(0, &Format);

			// Set the sample rate to what we want
			Format->wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
			Format->wBitsPerSample = 32;
			Format->nChannels = 1;
			Format->nSamplesPerSec = m_SampleRate;
			Format->nBlockAlign = (Format->wBitsPerSample >> 3) * Format->nChannels;
			Format->nAvgBytesPerSec = Format->nBlockAlign * Format->nSamplesPerSec;
			Format->cbSize = 0;

			// Create the event that will be used to signal the client for more data
			m_bufferCompletionEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

			SpatialAudioObjectRenderStreamActivationParams StreamActivationParams;
			StreamActivationParams.ObjectFormat = Format;
			StreamActivationParams.StaticObjectTypeMask = AudioObjectType_None;
			StreamActivationParams.MinDynamicObjectCount = 0;
			StreamActivationParams.MaxDynamicObjectCount = m_NumSources;
			StreamActivationParams.Category = AudioCategory_GameEffects;
			StreamActivationParams.EventHandle = m_bufferCompletionEvent;
			StreamActivationParams.NotifyObject = nullptr;

			PROPVARIANT SpatialAudioStreamProperty;
			PropVariantInit(&SpatialAudioStreamProperty);
			SpatialAudioStreamProperty.vt = VT_BLOB;
			SpatialAudioStreamProperty.blob.cbSize = sizeof(StreamActivationParams);
			SpatialAudioStreamProperty.blob.pBlobData = reinterpret_cast<BYTE*>(&StreamActivationParams);

			hr = m_SpatialAudioClient->ActivateSpatialAudioStream(&SpatialAudioStreamProperty, __uuidof(m_SpatialAudioStream), &m_SpatialAudioStream);
			if (FAILED(hr))
			{
				hr = E_FAIL;
				goto exit;
			}

			// Start streaming / rendering  
			hr = m_SpatialAudioStream->Start();
			if (FAILED(hr))
			{
				hr = E_FAIL;
				goto exit;
			}

			m_RenderState = RenderState::Active;
		}

	exit:

		if (FAILED(hr))
		{
			m_RenderState = RenderState::Inactive;
		}

		// Need to return S_OK
		return S_OK;
	}

	STDMETHOD(OnAvailableDynamicObjectCountChange)(ISpatialAudioObjectRenderStreamBase* sender, LONGLONG hnsComplianceDeadlineTime, UINT32 availableDynamicObjectCountChange)
	{
		sender;
		hnsComplianceDeadlineTime;

		m_MaxDynamicObjects = availableDynamicObjectCountChange;
		return S_OK;
	}

	HRESULT Stop()
	{
		HRESULT hr = S_OK;
		if (m_SpatialAudioStream && IsActive())
		{
			hr = m_SpatialAudioStream->Stop();
			if (SUCCEEDED(hr))
			{
				hr = m_SpatialAudioStream->Reset();
			}

			CloseHandle(m_bufferCompletionEvent);
			m_bufferCompletionEvent = 0;

			m_RenderState = Inactive;
		}
		return hr;
	}

	HRESULT ActivatDynamicSpatialAudioObject(ISpatialAudioObject** OutObject)
	{
		HRESULT hr = E_FAIL;
		if (m_SpatialAudioStream)
		{
			hr = m_SpatialAudioStream->ActivateSpatialAudioObject(AudioObjectType::AudioObjectType_Dynamic, OutObject);
		}
		return hr;
	}

	HRESULT BeginUpdatingAudioObjects(UINT32* AvailableObjects, UINT32* FrameCount)
	{
		HRESULT hr = E_FAIL;
		if (m_SpatialAudioStream)
		{
			hr = m_SpatialAudioStream->BeginUpdatingAudioObjects(AvailableObjects, FrameCount);
		}
		return hr;
	}

	HRESULT EndUpdatingAudioObjects()
	{
		HRESULT hr = E_FAIL;
		if (m_SpatialAudioStream)
		{
			hr = m_SpatialAudioStream->EndUpdatingAudioObjects();
		}
		return hr;
	}

	bool WaitTillBufferCompletionEvent()
	{
		bool bTimedOut = false;
		if (m_bufferCompletionEvent)
		{
			// Wait for a signal from the audio-engine to start the next processing pass
			if (WaitForSingleObject(m_bufferCompletionEvent, 100) != WAIT_OBJECT_0)
			{
				m_SpatialAudioStream->Reset();
				bTimedOut = true;
			}
		}
		return bTimedOut;
	}

private:

	~SpatialAudioClientRendererWinRT()
	{}

	ComPtr<ISpatialAudioClient> m_SpatialAudioClient;
	ComPtr<ISpatialAudioObjectRenderStream> m_SpatialAudioStream;
	HANDLE m_bufferCompletionEvent;

	std::wstring m_DeviceIdString;
	RenderState	m_RenderState;
	UINT32 m_MaxDynamicObjects;
	UINT32 m_SampleRate;
	UINT32 m_NumSources;
};

std::mutex sacRendererLock;
std::map<int, ComPtr<SpatialAudioClientRendererWinRT>> sacRendererMap;
int sacRendererIndex = 0;

static ComPtr<SpatialAudioClientRendererWinRT> GetSac(int32_t Id)
{
	std::lock_guard<std::mutex> lock(sacRendererLock);
	return sacRendererMap[Id];
}

/**
* SpatialAudioClient implementation
*/

bool SpatialAudioClient::Start(UINT32 InNumSources, UINT32 InSampleRate)
{
	HRESULT hr = S_OK;
	if (ComPtr<SpatialAudioClientRendererWinRT> sac = GetSac(sacId))
	{
		hr = sac->InitializeAudioDeviceAsync(InNumSources, InSampleRate);
	}
	return SUCCEEDED(hr);
}

bool SpatialAudioClient::Stop()
{
	if (ComPtr<SpatialAudioClientRendererWinRT> sac = GetSac(sacId))
	{
		return sac->Stop();
	}
	return true;
}

bool SpatialAudioClient::IsActive()
{
	if (ComPtr<SpatialAudioClientRendererWinRT> sac = GetSac(sacId))
	{
		return sac->IsActive();
	}
	return false;
}

UINT32 SpatialAudioClient::GetMaxDynamicObjects() const
{
	if (ComPtr<SpatialAudioClientRendererWinRT> sac = GetSac(sacId))
	{
		return sac->GetMaxDynamicObjects();
	}
	return 0;
}

ISpatialAudioObject* SpatialAudioClient::ActivatDynamicSpatialAudioObject()
{
	if (ComPtr<SpatialAudioClientRendererWinRT> sac = GetSac(sacId))
	{
		ISpatialAudioObject* NewSpatAudioObject = nullptr;
		HRESULT hr = sac->ActivatDynamicSpatialAudioObject(&NewSpatAudioObject);
		if (SUCCEEDED(hr))
		{
			return NewSpatAudioObject;
		}
	}
	return nullptr;
}

bool SpatialAudioClient::BeginUpdating(UINT32* OutAvailableDynamicObjectCount, UINT32* OutFrameCountPerBuffer)
{
	if (ComPtr<SpatialAudioClientRendererWinRT> sac = GetSac(sacId))
	{
		HRESULT hr = sac->BeginUpdatingAudioObjects(OutAvailableDynamicObjectCount, OutFrameCountPerBuffer);
		return SUCCEEDED(hr);
	}
	return false;
}

bool SpatialAudioClient::EndUpdating()
{
	if (ComPtr<SpatialAudioClientRendererWinRT> sac = GetSac(sacId))
	{
		HRESULT hr = sac->EndUpdatingAudioObjects();
		return SUCCEEDED(hr);
	}
	return false;
}

bool SpatialAudioClient::WaitTillBufferCompletionEvent()
{
	if (ComPtr<SpatialAudioClientRendererWinRT> sac = GetSac(sacId))
	{
		return sac->WaitTillBufferCompletionEvent();
	}
	return false;
}

SpatialAudioClient::SpatialAudioClient()
{
	ComPtr<SpatialAudioClientRendererWinRT> NewSac = Make< SpatialAudioClientRendererWinRT>();

	{
		std::lock_guard<std::mutex> lock(sacRendererLock);
		sacId = sacRendererIndex++;
		sacRendererMap[sacId] = NewSac;
	}
}

SpatialAudioClient::~SpatialAudioClient()
{
	Stop();
	Release();
}

void SpatialAudioClient::Release()
{
	std::lock_guard<std::mutex> lock(sacRendererLock);
	sacRendererMap.erase(sacId);
}
