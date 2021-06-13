#include "Utils.h"
#include "AudioCapture.h"
#define VERSION "0.3.0"

int initializeCorsairLighting(std::vector<CorsairLedArray>& memoryLeds, std::vector<CorsairDevice>& devices) {
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
		if (cmds[1] == "effect") {
			if (cmds[2] == "bars") {
				AudioLightingEffect* newEffect = new BarsEffect(*(opt.effect));
				delete opt.effect;
				opt.effect = newEffect;
				return 0;
			}
			if (cmds[2] == "pulse") {
				AudioLightingEffect* newEffect = new PulseEffect(*(opt.effect));
				delete opt.effect;
				opt.effect = newEffect;
				return 0;
			}

			out << "set effect: " << cmds[2] << " is not a valid effect name" << std::endl;
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
		saveProfile(opt, (cmds[1] + ".cavprof").c_str());
		return 0;
	}
	if (cmds[0] == "load") {
		if (cmds.size() < 2) {
			out << "load: must specify profile name" << std::endl;
			return 0;
		}

		std::ifstream file((cmds[1] + ".cavprof").c_str(), std::ios::in);
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
	if (cmds[0] == "list" || cmds[0] == "ls") {
		std::vector<std::string> list;
		listDirectory(".", list);

		for (int i = 0; i < list.size(); i++) {
			if (list[i].size() > 8 && list[i].substr(list[i].size() - 8) == ".cavprof")
				out << list[i].substr(0, list[i].size() - 8) << std::endl;
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

	// Initialize lighting effect
	AudioLightingEffect* effect = new BarsEffect(&memoryLeds, &devices);

	// Initialize options
	Color colors[10];
	for (int i = 0; i < 10; i++) colors[i] = { 255, 255, 255 };
	VisualizerOptions opt{ effect, { 0, 0, 0 }, colors, 20, 0, 0, 100, true, true };

	std::string def = "load default";
	processCommand(def, opt);

	while (!quit) {
		reset = false;
		std::cout << "Starting..." << std::endl;
		std::thread workerThread(audioCapture, &reset, &opt);

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