#pragma once

#include "Utils.h"

class AudioLightingEffect {
protected:
	std::vector<CorsairLedArray>* memoryLeds;
	std::vector<CorsairDevice>* devices;

	float last[2] = { 0, 0 };
	float hold[2] = { 0, 0 };
	float holdTimer[2] = { 0, 0 };

public:
	AudioLightingEffect(std::vector<CorsairLedArray>* leds, std::vector<CorsairDevice>* devices)
		: memoryLeds(leds), devices(devices)
	{ }

	AudioLightingEffect(const AudioLightingEffect& other)
		: memoryLeds(other.memoryLeds), devices(other.devices)
	{ }

	virtual void effect(VisualizerOptions*, UINT32, float*) = 0;
};

class BarsEffect : public AudioLightingEffect {
public:
	BarsEffect(std::vector<CorsairLedArray>* leds, std::vector<CorsairDevice>* devices)
		: AudioLightingEffect(leds, devices)
	{ }

	BarsEffect(const AudioLightingEffect& other)
		: AudioLightingEffect(other)
	{ }

	void effect(VisualizerOptions*, UINT32, float*);
};

class PulseEffect : public AudioLightingEffect {
public:
	PulseEffect(std::vector<CorsairLedArray>* leds, std::vector<CorsairDevice>* devices)
		: AudioLightingEffect(leds, devices)
	{ }

	PulseEffect(const AudioLightingEffect& other)
		: AudioLightingEffect(other)
	{ }

	void effect(VisualizerOptions*, UINT32, float*);
};