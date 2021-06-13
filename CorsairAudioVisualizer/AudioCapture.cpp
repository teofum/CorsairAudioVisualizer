#include "AudioCapture.h"

// Audio capture thread
HRESULT audioCapture(
	std::atomic_bool* exit,
	VisualizerOptions* opt
) {
	// Various UUIDs we need to identify things and tell Windows what we want to create and/or work with
	const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
	const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
	const IID IID_IAudioClient = __uuidof(IAudioClient);
	const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
	const IID IID_IAudioRenderClient = __uuidof(IAudioRenderClient);

	CComPtr<IMMDeviceEnumerator> devEnum;
	CComPtr<IMMDevice> device;
	IAudioClient* audioClient;
	IAudioCaptureClient* captureClient;
	WAVEFORMATEX* deviceFormat;
	REFERENCE_TIME requestedDuration = REFTIMES_PER_SEC;
	REFERENCE_TIME actualDuration;
	UINT32 bufferFrameCount;
	UINT32 packetLength = 0;
	bool done = false;
	BYTE* data;
	DWORD flags;
	UINT32 framesAvailable;

	HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if (FAILED(hr)) return hr;

	hr = devEnum.CoCreateInstance(CLSID_MMDeviceEnumerator);
	if (FAILED(hr)) goto Exit;

	// Get the default audio device (presumably the active one? check this later)
	hr = devEnum->GetDefaultAudioEndpoint(
		eRender, // This means we're looking for an output device
		eConsole, // Docs say this doesn't change anything
		&device // Device interface goes here
	);
	if (FAILED(hr)) goto Exit;

	// Get an audio client
	hr = device->Activate(
		IID_IAudioClient, // Type of interface we want, there's a fuck ton but AudioLevel uses this one
		CLSCTX_ALL, // No context restrictions
		NULL, // This interface type takes no parameters
		(void**)&audioClient // AC interface goes here, you know the drill by now
	);
	if (FAILED(hr)) goto Exit;

	hr = audioClient->GetMixFormat(&deviceFormat);
	if (FAILED(hr)) goto Exit;

	hr = audioClient->Initialize(
		AUDCLNT_SHAREMODE_SHARED,
		AUDCLNT_STREAMFLAGS_LOOPBACK, // Configure the audio stream as a loopback capture stream
		requestedDuration,
		0, // Must be 0 for shared mode
		deviceFormat,
		NULL // Audio session ID, we don't need this
	);
	if (FAILED(hr)) goto Exit;

	hr = audioClient->GetBufferSize(&bufferFrameCount);
	if (FAILED(hr)) goto Exit;

	hr = audioClient->GetService(
		IID_IAudioCaptureClient,
		(void**)&captureClient
	);
	if (FAILED(hr)) goto Exit;

	actualDuration = (double)REFTIMES_PER_SEC * bufferFrameCount / deviceFormat->nSamplesPerSec;
	hr = audioClient->Start(); // Start recording
	if (FAILED(hr)) goto Exit;

	while (!(*exit)) {
		Sleep(actualDuration / REFTIMES_PER_MSEC / opt->frequency);

		hr = captureClient->GetNextPacketSize(&packetLength);
		if (FAILED(hr)) goto Exit;

		while (packetLength != 0) {
			hr = captureClient->GetBuffer(
				&data,
				&framesAvailable,
				&flags, NULL, NULL
			);
			if (FAILED(hr)) goto Exit;

			// Lighting effect
			opt->effect->effect(opt, framesAvailable, (float*)data);
			// End of lighting effect

			hr = captureClient->ReleaseBuffer(framesAvailable);
			if (FAILED(hr)) goto Exit;

			hr = captureClient->GetNextPacketSize(&packetLength);
			if (FAILED(hr)) goto Exit;
		}
	}

	hr = audioClient->Stop();
	if (FAILED(hr)) goto Exit;

Exit:
	SafeRelease(&audioClient);
	SafeRelease(&captureClient);
	CoUninitialize();
	return hr;
}
