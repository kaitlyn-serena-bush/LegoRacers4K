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

struct MiniwinSurface;

// Surfaces whose HD image is decoded ahead of time (see hdtexture.cpp) are also uploaded
// ahead of time, a few per frame, so the screens that show them never stall on first draw.
// Returns the next surface whose image is ready, or nullptr.
MiniwinSurface* MiniwinHdNextPreupload();
void MiniwinHdForgetSurface(MiniwinSurface* p_surface);

// For prewarmed single-region bindings: a key that identical bindings share (same image,
// region and texture size), so the device can reuse one uploaded texture for all of them.
bool MiniwinHdShareKey(const MiniwinHdBinding& p_binding, std::string& p_key);
