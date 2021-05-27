#pragma once

#include <CUESDK.h>

#include <AudioClient.h>
#include <AudioPolicy.h>
#include <MMDeviceApi.h>
#include <atlbase.h>

#include <iostream>
#include <vector>
#include <string>
#include <unordered_set>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <filesystem>

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MSEC 10000

const char* crsErrorToString(CorsairError error) {
	switch (error) {
	case CE_Success:
		return "CE_Success";
	case CE_ServerNotFound:
		return "CE_ServerNotFound";
	case CE_NoControl:
		return "CE_NoControl";
	case CE_ProtocolHandshakeMissing:
		return "CE_ProtocolHandshakeMissing";
	case CE_IncompatibleProtocol:
		return "CE_IncompatibleProtocol";
	case CE_InvalidArguments:
		return "CE_InvalidArguments";
	default:
		return "unknown error";
	}
}

const char* crsDevTypeToString(CorsairDeviceType devType) {
	switch (devType) {
	case CDT_Keyboard:
		return "Keyboard";
	case CDT_Mouse:
		return "Mouse";
	case CDT_MouseMat:
		return "Mouse mat";
	case CDT_Headset:
		return "Headset";
	case CDT_HeadsetStand:
		return "Headset stand";
	case CDT_CommanderPro:
		return "Commander Pro";
	case CDT_LightingNodePro:
		return "Lighting Node Pro";
	case CDT_MemoryModule:
		return "Memory module";
	case CDT_Motherboard:
		return "Motherboard";
	case CDT_GraphicsCard:
		return "Graphics card";
	case CDT_Cooler:
		return "Cooler";
	default:
		return "Unknown device";
	}
}

struct CorsairDevice {
	int index;
	CorsairDeviceInfo* info;
};

template <class T> void SafeRelease(T** ppT) {
	if (*ppT) {
		(*ppT)->Release();
		*ppT = NULL;
	}
}

struct Color {
	int r;
	int g;
	int b;
};

struct VisualizerOptions {
	Color background;
	Color* colors;
	float gain;
	float fall;
	float hold;
	int frequency;
	bool smooth;
	bool multicolor;
};

int saveProfile(VisualizerOptions& opt, const char* name) {
	if (std::filesystem::exists(name)) {
		std::string answer;
		do {
			std::cout << "The profile " << name << " already exists. Overwrite? [y/N]" << std::endl;
			std::getline(std::cin, answer);
		} while (answer != "y" && answer != "Y" && answer != "n" && answer != "N" && answer != "");
		if (answer != "y" && answer != "Y") return -1;
	}

	std::cout << "Saving profile " << name << "..." << std::endl;
	std::ofstream file(name, std::ios::trunc);
	for (int i = 0; i < 10; i++) 
		file << "color " << i << " " << opt.colors[i].r << " " << opt.colors[i].g << " " << opt.colors[i].b << std::endl;
	file << "background " << opt.background.r << " " << opt.background.g << " " << opt.background.b << std::endl;
	file << "smooth " << (opt.smooth ? "true" : "false") << std::endl;
	file << "gain " << opt.gain << std::endl;
	file << "fall " << opt.fall << std::endl;
	file << "hold " << opt.hold << std::endl;
	file << "frequency " << opt.frequency << std::endl;
	file << "multicolor " << (opt.multicolor ? "true" : "false");
}

// Define a unary operation to get a filename (leaf) string from a path object
struct pathLeafStr {
	std::string operator()(const std::filesystem::directory_entry& entry) const {
		return entry.path().filename().string();
	}
};

void listDirectory(const std::string& dir, std::vector<std::string>& ls) {
	std::filesystem::path path(dir);
	std::filesystem::directory_iterator start(path);
	std::filesystem::directory_iterator end;
	std::transform(start, end, std::back_inserter(ls), pathLeafStr());
}