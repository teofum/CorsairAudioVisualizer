#pragma once

#include "Dependencies.h"

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MSEC 10000

typedef std::vector<CorsairLedColor> CorsairLedArray;

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

const char* crsErrorToString(CorsairError error);
const char* crsDevTypeToString(CorsairDeviceType devType);
int saveProfile(VisualizerOptions& opt, const char* name);

// Define a unary operation to get a filename (leaf) string from a path object
struct pathLeafStr {
	std::string operator()(const std::filesystem::directory_entry& entry) const {
		return entry.path().filename().string();
	}
};

void listDirectory(const std::string& dir, std::vector<std::string>& ls);