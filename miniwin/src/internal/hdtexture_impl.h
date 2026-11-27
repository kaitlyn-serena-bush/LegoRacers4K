#pragma once

// [library:hd] Internal side of miniwin/hdtexture.h.

#include <SDL3/SDL_stdinc.h>
#include <miniwin/hdtexture.h>
#include <string>
#include <vector>

struct MiniwinHdBinding {
	std::string m_key;
	std::string m_file;
	MiniwinHdRegion m_region;
	std::vector<MiniwinHdBlit> m_blits; // empty: the single region in m_region
};

// Builds the RGBA pixels the backend should sample for an HD-tagged surface.
// Returns false when the replacement cannot be used (the original pixels are used).
bool MiniwinHdBuildPixels(const MiniwinHdBinding& p_binding, std::vector<Uint8>& p_out, int& p_width, int& p_height);
