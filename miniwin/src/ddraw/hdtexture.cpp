// [library:hd] HD texture overrides (LegoRacers4K). See miniwin/hdtexture.h.

#include "hdtexture_impl.h"

#include "ddraw_impl.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <list>
#include <mutex>
#include <string>
#include <system_error>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace fs = std::filesystem;

namespace
{

struct HdImage {
	int m_width = 0;
	int m_height = 0;
	std::vector<Uint8> m_rgba;
};

struct HdState {
	std::mutex m_mutex;
	bool m_enabled = true;
	bool m_indexed = false;
	std::string m_root;
	std::unordered_map<std::string, std::string> m_files; // "GAMEDATA/COMMON/AD_CHST" -> full path
	// Small most-recently-used cache: menu images are split into many tiles that all
	// read the same PNG, so decode it once per image rather than once per tile.
	std::list<std::pair<std::string, std::shared_ptr<HdImage>>> m_cache;
	size_t m_cacheBytes = 0;
};

HdState& State()
{
	static HdState s_state;
	return s_state;
}

const size_t c_cacheLimitBytes = 256u * 1024u * 1024u;

std::string KeyFromPath(const char* p_path)
{
	std::string key;
	for (const char* c = p_path; *c; ++c) {
		char ch = *c == '\\' ? '/' : (char) std::toupper((unsigned char) *c);
		key.push_back(ch);
	}
	while (key.size() > 2 && key[0] == '.' && key[1] == '/') {
		key.erase(0, 2);
	}
	size_t slash = key.find_last_of('/');
	size_t dot = key.find_last_of('.');
	if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
		key.erase(dot);
	}
	return key;
}

bool TryRoot(const fs::path& p_root)
{
	std::error_code ec;
	if (p_root.empty() || !fs::is_directory(p_root, ec)) {
		return false;
	}

	HdState& state = State();
	size_t count = 0;
	fs::recursive_directory_iterator it(p_root, fs::directory_options::skip_permission_denied, ec);
	for (; !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
		const fs::path& path = it->path();
		std::string name = path.filename().string();
		if (it->is_directory(ec)) {
			// Review leftovers (_alternates, _replaced, ...) are not part of the set.
			if (!name.empty() && name[0] == '_') {
				it.disable_recursion_pending();
			}
			continue;
		}
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char) std::tolower(c); });
		if (ext != ".png") {
			continue;
		}
		std::string relative = fs::relative(path, p_root, ec).generic_string();
		if (ec) {
			continue;
		}
		state.m_files[KeyFromPath(relative.c_str())] = path.string();
		count++;
	}

	if (count == 0) {
		return false;
	}

	state.m_root = p_root.string();
	SDL_Log("HD textures: %zu replacements from %s", count, state.m_root.c_str());
	return true;
}

void EnsureIndexed()
{
	HdState& state = State();
	if (state.m_indexed) {
		return;
	}
	state.m_indexed = true;

	// Set by --nohd. An environment variable rather than a call so it reaches every copy
	// of miniwin (the game and GolDP each link their own).
	if (SDL_getenv("RACERS_NO_HD")) {
		state.m_enabled = false;
		SDL_Log("HD textures: off (--nohd)");
		return;
	}

	if (const char* env = SDL_getenv("RACERS_HD_DIR")) {
		if (TryRoot(fs::path(env))) {
			return;
		}
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "HD textures: RACERS_HD_DIR '%s' has no PNGs", env);
	}
	if (TryRoot(fs::path("HD"))) {
		return;
	}
	if (const char* base = SDL_GetBasePath()) {
		if (TryRoot(fs::path(base) / "HD")) {
			return;
		}
	}
	SDL_Log("HD textures: no HD folder found, using original textures");
}

std::shared_ptr<HdImage> LoadImage(const std::string& p_key, const std::string& p_file)
{
	HdState& state = State();
	for (auto it = state.m_cache.begin(); it != state.m_cache.end(); ++it) {
		if (it->first == p_key) {
			state.m_cache.splice(state.m_cache.begin(), state.m_cache, it);
			return it->second;
		}
	}

	size_t fileSize = 0;
	void* fileData = SDL_LoadFile(p_file.c_str(), &fileSize);
	if (!fileData) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "HD textures: cannot read %s", p_file.c_str());
		return nullptr;
	}

	int width = 0;
	int height = 0;
	int channels = 0;
	stbi_uc* pixels = stbi_load_from_memory((const stbi_uc*) fileData, (int) fileSize, &width, &height, &channels, 4);
	SDL_free(fileData);
	if (!pixels) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "HD textures: cannot decode %s", p_file.c_str());
		return nullptr;
	}

	auto image = std::make_shared<HdImage>();
	image->m_width = width;
	image->m_height = height;
	image->m_rgba.assign(pixels, pixels + (size_t) width * height * 4);
	stbi_image_free(pixels);

	state.m_cache.emplace_front(p_key, image);
	state.m_cacheBytes += image->m_rgba.size();
	while (state.m_cacheBytes > c_cacheLimitBytes && state.m_cache.size() > 1) {
		state.m_cacheBytes -= state.m_cache.back().second->m_rgba.size();
		state.m_cache.pop_back();
	}
	return image;
}

} // namespace

void MiniwinHdSetEnabled(bool p_enabled)
{
	std::lock_guard<std::mutex> lock(State().m_mutex);
	State().m_enabled = p_enabled;
}

bool MiniwinHdHasReplacement(const char* p_gamePath)
{
	if (!p_gamePath || !*p_gamePath) {
		return false;
	}
	HdState& state = State();
	std::lock_guard<std::mutex> lock(state.m_mutex);
	EnsureIndexed();
	if (!state.m_enabled) {
		return false;
	}
	return state.m_files.count(KeyFromPath(p_gamePath)) != 0;
}

bool MiniwinHdAttach(IDirectDrawSurface* p_surface, const char* p_gamePath, const MiniwinHdRegion& p_region)
{
	if (!p_surface || !MiniwinHdHasReplacement(p_gamePath)) {
		return false;
	}
	if (p_region.m_imageWidth <= 0 || p_region.m_imageHeight <= 0 || p_region.m_textureWidth <= 0 ||
		p_region.m_textureHeight <= 0 || p_region.m_srcWidth <= 0 || p_region.m_srcHeight <= 0) {
		return false;
	}

	auto* surface = static_cast<MiniwinSurface*>(p_surface);
	auto hd = std::make_shared<MiniwinHdBinding>();
	hd->m_key = KeyFromPath(p_gamePath);
	{
		std::lock_guard<std::mutex> lock(State().m_mutex);
		hd->m_file = State().m_files[hd->m_key];
	}
	hd->m_region = p_region;
	surface->m_hd = hd;
	surface->m_textureDirty = true;
	return true;
}

void MiniwinHdDetach(IDirectDrawSurface* p_surface)
{
	if (!p_surface) {
		return;
	}
	auto* surface = static_cast<MiniwinSurface*>(p_surface);
	if (surface->m_hd) {
		surface->m_hd.reset();
		surface->m_textureDirty = true;
	}
}

bool MiniwinHdBuildPixels(const MiniwinHdBinding& p_binding, std::vector<Uint8>& p_out, int& p_width, int& p_height)
{
	std::shared_ptr<HdImage> image;
	{
		std::lock_guard<std::mutex> lock(State().m_mutex);
		image = LoadImage(p_binding.m_key, p_binding.m_file);
	}
	if (!image) {
		return false;
	}

	const MiniwinHdRegion& region = p_binding.m_region;
	// Scale from original pixels to HD pixels. Upscales are uniform; use each axis's own
	// factor anyway so a slightly-off aspect ratio still lines up with the original UVs.
	double scaleX = (double) image->m_width / region.m_imageWidth;
	double scaleY = (double) image->m_height / region.m_imageHeight;
	if (scaleX < 1.0 || scaleY < 1.0) {
		return false; // not an upscale; keep the original
	}

	int outW = (int) std::lround(region.m_textureWidth * scaleX);
	int outH = (int) std::lround(region.m_textureHeight * scaleY);
	int srcX = (int) std::lround(region.m_srcX * scaleX);
	int srcY = (int) std::lround(region.m_srcY * scaleY);
	int copyW = std::min((int) std::lround(region.m_srcWidth * scaleX), image->m_width - srcX);
	int copyH = std::min((int) std::lround(region.m_srcHeight * scaleY), image->m_height - srcY);
	copyW = std::min(copyW, outW);
	copyH = std::min(copyH, outH);
	if (outW <= 0 || outH <= 0 || copyW <= 0 || copyH <= 0) {
		return false;
	}

	// GPU limit; every current backend handles 8192, and the art set caps at 4096.
	const int c_maxSize = 8192;
	if (outW > c_maxSize || outH > c_maxSize) {
		return false;
	}

	p_width = outW;
	p_height = outH;
	p_out.assign((size_t) outW * outH * 4, 0);

	for (int y = 0; y < outH; y++) {
		// Rows past the copied region repeat its last row so linear filtering at the
		// region's edge does not pull in the padding.
		int srcRow = std::min(y, copyH - 1);
		int imageRow = srcY + srcRow;
		if (region.m_flipVertically) {
			// The original image's rows were stored bottom-up, and the tile region was
			// taken in that flipped space.
			imageRow = image->m_height - 1 - imageRow;
		}
		const Uint8* src = image->m_rgba.data() + ((size_t) imageRow * image->m_width + srcX) * 4;
		Uint8* dst = p_out.data() + (size_t) y * outW * 4;
		SDL_memcpy(dst, src, (size_t) copyW * 4);
		for (int x = copyW; x < outW; x++) {
			SDL_memcpy(dst + x * 4, src + (copyW - 1) * 4, 4);
		}
	}
	return true;
}
