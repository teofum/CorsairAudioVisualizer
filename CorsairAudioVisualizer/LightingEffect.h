#pragma once

#include "Utils.h"

class AudioLightingEffect {
protected:
	std::vector<CorsairLedArray>* memoryLeds;
	std::vector<CorsairDevice>* devices;

public:
	AudioLightingEffect(std::vector<CorsairLedArray>* leds, std::vector<CorsairDevice>* devices)
		: memoryLeds(leds), devices(devices)
	{ }

	virtual void effect(VisualizerOptions*, UINT32, float*) = 0;
};

class BarsEffect : public AudioLightingEffect {
private:
	float last[2] = { 0, 0 };
	float hold[2] = { 0, 0 };
	float holdTimer[2] = { 0, 0 };

public:
	BarsEffect(std::vector<CorsairLedArray>* leds, std::vector<CorsairDevice>* devices)
		: AudioLightingEffect(leds, devices)
	{ }

	void effect(VisualizerOptions*, UINT32, float*);
};