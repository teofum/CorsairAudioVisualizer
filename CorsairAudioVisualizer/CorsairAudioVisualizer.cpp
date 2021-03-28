#include "Utils.h"
#define VERSION "0.1.1"

int initializeCorsairLighting(std::vector<std::vector<CorsairLedColor>>& memoryLeds, std::vector<CorsairDevice>& devices) {
	// Preflight, make sure everything's working
	CorsairPerformProtocolHandshake();
	if (const auto error = CorsairGetLastError()) {
		std::cout << "Handshake failed: " << crsErrorToString(error) << std::endl;
		return -1;
	}

	// Get all devices
	for (int devIndex = 0, devCount = CorsairGetDeviceCount(); devIndex < devCount; devIndex++) {
		auto devInfo = CorsairGetDeviceInfo(devIndex);
		devices.push_back({ devIndex, devInfo });
	}

	// Find two RAM modules
	std::vector<int> memory;
	memory.reserve(2);
	for (auto& device : devices) {
		if (device.info->type == CDT_MemoryModule && memory.size() < 2) memory.push_back(device.index);
	}
	if (memory.size() != 2) {
		std::cout << "Initialization failed: Could not find two memory modules." << std::endl;
		return -1;
	}

	// Get LEDs for each ram module
	for (int moduleIndex : memory) {
		auto ledSet = std::unordered_set<int>();
		if (const auto ledPositions = CorsairGetLedPositionsByDeviceIndex(moduleIndex)) {
			for (int i = 0; i < ledPositions->numberOfLed; i++) {
				const auto ledId = ledPositions->pLedPosition[i].ledId;
				ledSet.insert(ledId);
			}
		}

		std::vector<CorsairLedColor> colors;
		for (auto& ledId : ledSet) colors.push_back({ static_cast<CorsairLedId>(ledId), 0, 0, 0 });
		memoryLeds.push_back(colors);
	}

	return 0;
}

// Audio capture thread
HRESULT audioCapture(std::vector<std::vector<CorsairLedColor>>* memoryLeds, std::vector<CorsairDevice>* devices, std::atomic_bool* exit, VisualizerOptions* opt) {
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
	float last[2] = { 0, 0 };
	float hold[2] = { 0, 0 };
	float holdTimer[2] = { 0, 0 };

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
			float* fdata = (float*)data;
			float gain = opt->gain;

			// Do this once per channel
			for (int c = 0; c < 2; c++) {
				float rms = 0;
				for (int i = 0; i < framesAvailable * 2; i += 2)
					rms += fdata[i + c] * fdata[i + c];
				rms = sqrtf(rms / framesAvailable);
				float level = rms * gain;
				if (opt->hold > 0) {
					if (level > hold[c]) {
						hold[c] = level;
						holdTimer[c] = opt->hold;
					}
					else {
						if (holdTimer[c] <= 0) hold[c] = 0;
						else {
							level = hold[c];
							holdTimer[c] -= 1.0f / opt->frequency;
						}
					}
				}

				if (opt->fall > 0) {
					level = max(level, last[c] - (opt->fall * gain / opt->frequency));
					last[c] = level;
				}

				int lCount = memoryLeds->at(c).size();
				for (int i = 0; i < lCount; i++) {
					auto led = &memoryLeds->at(c)[lCount - i - 1];
					float intensity = opt->smooth ? (max(0, min(1, level - i))) : (level > i ? 1 : 0);

					Color color = opt->multicolor ? opt->colors[i] : opt->colors[0];
					led->r = (int)(color.r * intensity + opt->background.r * (1 - intensity));
					led->g = (int)(color.g * intensity + opt->background.g * (1 - intensity));
					led->b = (int)(color.b * intensity + opt->background.b * (1 - intensity));
				}
				CorsairSetLedsColorsBufferByDeviceIndex(devices->at(c).index, memoryLeds->at(c).size(), memoryLeds->at(c).data());
			}

			CorsairSetLedsColorsFlushBufferAsync(nullptr, nullptr);
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

int processCommand(std::string& cmd, VisualizerOptions& opt, std::ostream& out = std::cout) {
	std::vector<std::string> cmds;
	int wordStart = 0;
	for (int i = 0; i < cmd.size(); i++) {
		char c = cmd.at(i);
		if (c == ' ' || i == cmd.size() - 1) {
			if (i == cmd.size() - 1) i++;
			if (i > wordStart) cmds.push_back(cmd.substr(wordStart, i - wordStart));
			wordStart = i + 1;
		}
	}

	if (cmds.size() < 1) return 0;

	if (cmds[0] == "quit") return 1;
	if (cmds[0] == "reset") return 2;

	if (cmds[0] == "set") {
		if (cmds.size() < 3) {
			out << "Usage: set <property> <params>" << std::endl;
			return 0;
		}
		if (cmds[1] == "red") {
			if (cmds.size() == 3) opt.colors[0].r = max(0, min(255, std::stoi(cmds[2])));
			if (cmds.size() == 4) opt.colors[std::stoi(cmds[2])].r = max(0, min(255, std::stoi(cmds[3])));
			return 0;
		}
		if (cmds[1] == "green") {
			if (cmds.size() == 3) opt.colors[0].g = max(0, min(255, std::stoi(cmds[2])));
			if (cmds.size() == 4) opt.colors[std::stoi(cmds[2])].g = max(0, min(255, std::stoi(cmds[3])));
			return 0;
		}
		if (cmds[1] == "blue") {
			if (cmds.size() == 3) opt.colors[0].b = max(0, min(255, std::stoi(cmds[2])));
			if (cmds.size() == 4) opt.colors[std::stoi(cmds[2])].b = max(0, min(255, std::stoi(cmds[3])));
			return 0;
		}
		if (cmds[1] == "color") {
			if (cmds.size() == 5) {
				opt.colors[0].r = max(0, min(255, std::stoi(cmds[2])));
				opt.colors[0].g = max(0, min(255, std::stoi(cmds[3])));
				opt.colors[0].b = max(0, min(255, std::stoi(cmds[4])));
			}
			if (cmds.size() == 6) {
				opt.colors[std::stoi(cmds[2])].r = max(0, min(255, std::stoi(cmds[3])));
				opt.colors[std::stoi(cmds[2])].g = max(0, min(255, std::stoi(cmds[4])));
				opt.colors[std::stoi(cmds[2])].b = max(0, min(255, std::stoi(cmds[5])));
			}
			return 0;
		}
		if (cmds[1] == "background") {
			if (cmds.size() == 5) {
				opt.background.r = max(0, min(255, std::stoi(cmds[2])));
				opt.background.g = max(0, min(255, std::stoi(cmds[3])));
				opt.background.b = max(0, min(255, std::stoi(cmds[4])));
			}
			return 0;
		}
		if (cmds[1] == "gain") {
			opt.gain = std::stof(cmds[2]);
			return 0;
		}
		if (cmds[1] == "frequency") {
			opt.frequency = std::stoi(cmds[2]);
			return 0;
		}
		if (cmds[1] == "smooth") {
			opt.smooth = cmds[2] == "true";
			return 0;
		}
		if (cmds[1] == "multicolor") {
			opt.multicolor = cmds[2] == "true";
			return 0;
		}
		if (cmds[1] == "fall") {
			opt.fall = std::stof(cmds[2]);
			return 0;
		}
		if (cmds[1] == "hold") {
			opt.hold = std::stof(cmds[2]);
			return 0;
		}

		out << "set: " << cmds[1] << " is not a valid property name" << std::endl;
		return 0;
	}
	if (cmds[0] == "save") {
		if (cmds.size() < 2) {
			out << "save: must specify profile name" << std::endl;
			return 0;
		}
		saveProfile(opt, cmds[1].c_str());
		return 0;
	}
	if (cmds[0] == "load") {
		if (cmds.size() < 2) {
			out << "load: must specify profile name" << std::endl;
			return 0;
		}

		std::ifstream file(cmds[1].c_str(), std::ios::in);
		if (!file.good()) {
			out << "load: failed to open file" << std::endl;
			return 0;
		}

		out << "Loading profile " << cmds[1] << "..." << std::endl;
		std::string line;
		while (!file.eof()) {
			std::getline(file, line);
			line = "set " + line;
			processCommand(line, opt);
		}
		return 0;
	}
	if (cmds[0] == "help") {
		std::string file = "help.txt";
		if (cmds.size() > 1) file = "help_" + cmds[1] + ".txt";

		std::ifstream help(file.c_str(), std::ios::in);
		if (help.is_open()) out << help.rdbuf() << std::endl;
		else out << "No help available" << std::endl;
		return 0;
	}
	if (cmds[0] == "version") {
		out << "Corsair Audio Visualizer v" << VERSION << std::endl;
		return 0;
	}

	out << cmds[0] << " is not a valid command\nType 'help' for a list of commands" << std::endl;
	return 0;
}

int main() {
	SetConsoleTitle(L"Corsair Audio Visualizer");

	std::vector<std::vector<CorsairLedColor>> memoryLeds;
	std::vector<CorsairDevice> devices;
	int silentRetry = 5;

	// Initialize Corsair API and find relevant LEDs
	std::cout << "Initializing Corsair API..." << std::endl;
	while (initializeCorsairLighting(memoryLeds, devices) < 0) {
		if (silentRetry-- > 0) Sleep(5000);
		else {
			std::cout << "Retry initialization? [Y/n]" << std::endl;
			char in = getchar();
			if (in == 'n' || in == 'N') return 0;
		}
	}

	bool quit = false;
	std::atomic_bool reset{ false };

	// Initialize options
	Color colors[10];
	for (int i = 0; i < 10; i++) colors[i] = { 255, 255, 255 };
	VisualizerOptions opt{ { 0, 0, 0 }, colors, 20, 0, 0, 100, true, true };

	std::string def = "load default";
	processCommand(def, opt);

	while (!quit) {
		reset = false;
		std::cout << "Starting..." << std::endl;
		std::thread workerThread(audioCapture, &memoryLeds, &devices, &reset, &opt);

		std::string cmd;
		std::cout << "Enter a command\nType 'help' for a list of commands, 'quit' to exit" << std::endl;
		while (!reset) {
			std::cout << "> ";
			std::getline(std::cin, cmd);
			int result = processCommand(cmd, opt);
			if (result == 1) quit = reset = true;
			else if (result == 2) reset = true;
		}

		workerThread.join();
	}
	return 0;
}