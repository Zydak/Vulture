#pragma once

#ifndef MAX_FRAMES_IN_FLIGHT
#define MAX_FRAMES_IN_FLIGHT 1
#endif

// These message IDs will be ignored by layers
static std::vector<int32_t> s_LayersIgnoreList =
{
	-602362517, // Small allocation, imgui constantly throws that
	-1277938581, // Small allocation again
	1413273847, // Memory priority
	330418563, // Load and clear op warning
	-1994573612, // imgui descriptor realocation
	-735258470 // pipeline layout too big
};