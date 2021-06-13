#include "LightingEffect.h"

void DoubleBarsEffect::effect(VisualizerOptions* opt, UINT32 framesAvailable, float* fdata) {
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
			float halfLevel = level * 0.5f;
			int offset = abs(i - lCount * 0.5f);
			float intensity = opt->smooth ? (max(0, min(1, halfLevel - offset))) : (halfLevel > offset ? 1 : 0);

			Color color = opt->multicolor ? opt->colors[i] : opt->colors[0];
			led->r = (int)(color.r * intensity + opt->background.r * (1 - intensity));
			led->g = (int)(color.g * intensity + opt->background.g * (1 - intensity));
			led->b = (int)(color.b * intensity + opt->background.b * (1 - intensity));
		}
		CorsairSetLedsColorsBufferByDeviceIndex(devices->at(c).index, memoryLeds->at(c).size(), memoryLeds->at(c).data());
	}

	CorsairSetLedsColorsFlushBufferAsync(nullptr, nullptr);
}