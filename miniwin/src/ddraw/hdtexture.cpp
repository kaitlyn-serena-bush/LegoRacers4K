// [library:hd] HD texture overrides (LegoRacers4K). See miniwin/hdtexture.h.

#include "ddraw_impl.h"
#include "hdtexture_impl.h"

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

#define NANOSVG_IMPLEMENTATION
#define NANOSVGRAST_IMPLEMENTATION
#include "nanosvg.h"
#include "nanosvgrast.h"

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
	bool m_hasAlpha = false;
	std::vector<Uint8> m_rgba;
};

struct VectorCell {
	int m_x;
	int m_width;
	int m_advance;
};

struct HdState {
	std::mutex m_mutex;
	bool m_enabled = true;
	bool m_indexed = false;
	std::string m_root;
	std::unordered_map<std::string, std::string> m_files; // "GAMEDATA/COMMON/AD_CHST" -> full path
	// Small most-recently-used cache: menu images are split into many tiles that all
	// read the same PNG, so decode it once per image rather than once per tile.
	std::list<std::pair<std::string, std::shared_ptr<HdImage> > > m_cache;
	size_t m_cacheBytes = 0;
	// Font sets: alternative font strips that override the base folder's files while
	// selected (LegoRacers4K Options > Font). Set name -> (key -> full path).
	std::unordered_map<std::string, std::unordered_map<std::string, std::string> > m_fontSets;
	std::unordered_map<std::string, std::string> m_fontSetRoots; // set name -> folder
	// Font strips rasterised from a set's vector outlines, by the replaced PNG's path.
	// Kept for the whole run (never evicted) so every texture sees the same image.
	std::unordered_map<std::string, std::shared_ptr<HdImage> > m_generated;
	std::unordered_map<std::string, std::vector<VectorCell> > m_generatedCells;
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

size_t IndexFolder(const fs::path& p_root, std::unordered_map<std::string, std::string>& p_files)
{
	std::error_code ec;
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
		p_files[KeyFromPath(relative.c_str())] = path.string();
		count++;
	}
	return count;
}

bool TryRoot(const fs::path& p_root)
{
	std::error_code ec;
	if (p_root.empty() || !fs::is_directory(p_root, ec)) {
		return false;
	}

	HdState& state = State();
	size_t count = IndexFolder(p_root, state.m_files);
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

// The selected font set's folder of replacement files, indexed on first use. Sets live
// in RACERS_FONT_SETS, else <HD folder>/../fonts/alt/sets, else "fonts" next to the exe.
const std::unordered_map<std::string, std::string>* FontSetFiles()
{
	const char* name = SDL_GetStringProperty(SDL_GetGlobalProperties(), "miniwin.hd_font_set", "");
	if (!name || !*name) {
		return nullptr;
	}

	HdState& state = State();
	auto found = state.m_fontSets.find(name);
	if (found != state.m_fontSets.end()) {
		return &found->second;
	}

	std::unordered_map<std::string, std::string>& files = state.m_fontSets[name];
	std::vector<fs::path> roots;
	if (const char* env = SDL_getenv("RACERS_FONT_SETS")) {
		roots.push_back(fs::path(env));
	}
	if (!state.m_root.empty()) {
		roots.push_back(fs::path(state.m_root).parent_path() / "fonts" / "alt" / "sets");
	}
	if (const char* base = SDL_GetBasePath()) {
		roots.push_back(fs::path(base) / "fonts");
	}
	for (const fs::path& root : roots) {
		std::error_code ec;
		fs::path dir = root / name;
		if (fs::is_directory(dir, ec) && IndexFolder(dir, files) > 0) {
			state.m_fontSetRoots[name] = dir.string();
			SDL_Log("HD textures: font set '%s' from %s", name, dir.string().c_str());
			break;
		}
	}
	if (files.empty()) {
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "HD textures: font set '%s' not found", name);
	}
	return &files;
}

// Full path of the replacement for p_key, or an empty string.
std::string ResolveFile(const std::string& p_key)
{
	HdState& state = State();
	if (const auto* set = FontSetFiles()) {
		auto it = set->find(p_key);
		if (it != set->end()) {
			return it->second;
		}
	}
	auto it = state.m_files.find(p_key);
	return it != state.m_files.end() ? it->second : std::string();
}

std::shared_ptr<HdImage> LoadImage(const std::string& p_key, const std::string& p_file)
{
	HdState& state = State();
	auto generated = state.m_generated.find(p_file);
	if (generated != state.m_generated.end()) {
		return generated->second;
	}
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
	image->m_hasAlpha = channels == 2 || channels == 4;
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

std::vector<std::string> SplitCsvLine(const std::string& p_line)
{
	std::vector<std::string> fields;
	std::string field;
	bool quoted = false;
	for (size_t i = 0; i < p_line.size(); i++) {
		char c = p_line[i];
		if (c == '"') {
			if (quoted && i + 1 < p_line.size() && p_line[i + 1] == '"') {
				field += '"';
				i++;
			}
			else {
				quoted = !quoted;
			}
		}
		else if (c == ',' && !quoted) {
			fields.push_back(field);
			field.clear();
		}
		else if (c != '\r') {
			field += c;
		}
	}
	fields.push_back(field);
	return fields;
}

// Rows of a CSV file after its header row.
std::vector<std::vector<std::string> > ReadCsvRows(const fs::path& p_file)
{
	std::vector<std::vector<std::string> > rows;
	size_t size = 0;
	char* text = (char*) SDL_LoadFile(p_file.string().c_str(), &size);
	if (!text) {
		return rows;
	}
	std::string all(text, size);
	SDL_free(text);
	size_t pos = all.find('\n');
	while (pos != std::string::npos && pos + 1 < all.size()) {
		size_t end = all.find('\n', pos + 1);
		std::string line = all.substr(pos + 1, end == std::string::npos ? std::string::npos : end - pos - 1);
		pos = end;
		if (!line.empty() && line != "\r") {
			rows.push_back(SplitCsvLine(line));
		}
	}
	return rows;
}

bool ParseHexColor(const std::string& p_text, Uint8 p_rgb[3])
{
	const char* text = p_text.c_str();
	if (*text == '#') {
		text++;
	}
	if (SDL_strlen(text) != 6) {
		return false;
	}
	unsigned long value = SDL_strtoul(text, nullptr, 16);
	p_rgb[0] = (Uint8) (value >> 16);
	p_rgb[1] = (Uint8) (value >> 8);
	p_rgb[2] = (Uint8) value;
	return true;
}

// Screen pixels per original (480-line) pixel: the display's height in pixels, so text is
// rasterised at the size it is shown (RACERS_FONT_SCALE overrides).
float FontRasterScale()
{
	if (const char* env = SDL_getenv("RACERS_FONT_SCALE")) {
		float scale = (float) SDL_atof(env);
		if (scale > 0.f) {
			return std::min(scale, 16.f);
		}
	}
	float height = 0.f;
	SDL_DisplayID display = SDL_GetPrimaryDisplay();
	if (const SDL_DisplayMode* mode = display ? SDL_GetDesktopDisplayMode(display) : nullptr) {
		height = mode->h * (mode->pixel_density > 0.f ? mode->pixel_density : 1.f);
	}
	int count = 0;
	if (SDL_Window** windows = SDL_GetWindows(&count)) {
		for (int i = 0; i < count; i++) {
			int w = 0;
			int h = 0;
			if (SDL_GetWindowSizeInPixels(windows[i], &w, &h)) {
				height = std::max(height, (float) h);
			}
		}
		SDL_free(windows);
	}
	if (height <= 0.f) {
		height = 2160.f;
	}
	return std::clamp(height / 480.f, 1.f, 16.f);
}

// Composites colour p_rgb with coverage p_cov (0..1) over p_dst (straight alpha).
void Over(float* p_dst, const Uint8 p_rgb[3], float p_cov)
{
	if (p_cov <= 0.f) {
		return;
	}
	float da = p_dst[3];
	float oa = p_cov + da * (1.f - p_cov);
	for (int c = 0; c < 3; c++) {
		p_dst[c] = (p_rgb[c] * p_cov + p_dst[c] * da * (1.f - p_cov)) / oa;
	}
	p_dst[3] = oa;
}

// Builds a font strip from the set's vector outlines (vector/strips.csv layout, glyph order
// from cells.csv, vector/glyphs/U+XXXX.svg outlines in font units, baseline at y=0) at the
// screen's size: same layout rules as the PNG strips (column 0 empty, one run of columns
// per glyph, even gap). Transparent pixels carry the face colour so filtering never
// darkens the edges.
std::shared_ptr<HdImage> BuildVectorStrip(
	const std::string& p_root,
	const std::string& p_key,
	int p_origHeight,
	std::vector<VectorCell>& p_cells
)
{
	fs::path vector = fs::path(p_root) / "vector";
	std::error_code ec;
	if (p_origHeight <= 0 || !fs::is_directory(vector, ec)) {
		return nullptr;
	}

	std::vector<std::string> layout;
	for (auto& row : ReadCsvRows(vector / "strips.csv")) {
		if (row.size() >= 8 && KeyFromPath(row[0].c_str()) == p_key) {
			layout = row;
			break;
		}
	}
	if (layout.empty()) {
		return nullptr;
	}

	std::vector<unsigned> codepoints;
	for (auto& row : ReadCsvRows(fs::path(p_root) / "cells.csv")) {
		if (row.size() >= 3 && KeyFromPath(row[0].c_str()) == p_key) {
			const char* cp = row[2].c_str();
			if (SDL_strncasecmp(cp, "U+", 2) == 0) {
				cp += 2;
			}
			codepoints.push_back((unsigned) SDL_strtoul(cp, nullptr, 16));
		}
	}
	if (codepoints.empty()) {
		return nullptr;
	}

	double capTop = SDL_atof(layout[1].c_str());
	double base = SDL_atof(layout[2].c_str());
	double gapUnits = SDL_atof(layout[3].c_str());
	double emboss = SDL_atof(layout[4].c_str());
	Uint8 face[3] = {255, 255, 255};
	Uint8 hi[3] = {255, 255, 255};
	Uint8 shadow[3] = {0, 0, 0};
	ParseHexColor(layout[5], face);
	ParseHexColor(layout[6], hi);
	ParseHexColor(layout[7], shadow);
	if (base <= capTop || base > 1.0) {
		return nullptr;
	}

	const double capUnits = 1180.0; // cap height of the capitals, font units
	int height = (int) std::lround(p_origHeight * FontRasterScale());
	int basePx = (int) std::lround(base * height);
	double k = (basePx - std::lround(capTop * height)) / capUnits; // pixels per font unit
	int offset = emboss > 0.0 ? std::max(1, (int) std::lround(emboss * height)) : 0;
	int gap = std::max(2, (int) std::lround(gapUnits * k));

	NSVGrasterizer* rasterizer = nsvgCreateRasterizer();
	if (!rasterizer) {
		return nullptr;
	}

	struct Piece {
		int m_width;
		std::vector<Uint8> m_rgba;
	};
	std::vector<Piece> pieces;
	bool ok = true;
	for (unsigned codepoint : codepoints) {
		char name[32];
		SDL_snprintf(name, sizeof(name), "U+%04X.svg", codepoint);
		std::string file = (vector / "glyphs" / name).string();
		size_t size = 0;
		char* text = (char*) SDL_LoadFile(file.c_str(), &size);
		if (!text && codepoint >= 1 && codepoint <= 9) {
			// The FDBs list a second run of digits as codes 1-9.
			SDL_snprintf(name, sizeof(name), "U+%04X.svg", '0' + codepoint);
			file = (vector / "glyphs" / name).string();
			text = (char*) SDL_LoadFile(file.c_str(), &size);
		}
		if (!text) {
			SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "HD textures: no outline %s", file.c_str());
			ok = false;
			break;
		}
		std::string svgText(text, size);
		SDL_free(text);

		// viewBox "x0 -y1 w h": the parsed shapes start at the bounds' top-left corner.
		double viewTop = 0.0;
		size_t vb = svgText.find("viewBox=\"");
		if (vb != std::string::npos) {
			double values[4] = {};
			const char* p = svgText.c_str() + vb + 9;
			for (double& value : values) {
				char* end = nullptr;
				value = SDL_strtod(p, &end);
				p = end;
				while (*p == ' ' || *p == ',') {
					p++;
				}
			}
			viewTop = values[1];
		}
		NSVGimage* svg = nsvgParse(svgText.data(), "px", 96.0f);
		if (!svg) {
			ok = false;
			break;
		}
		// One pixel of padding each side, plus room for the emboss layers.
		int inkW = (int) std::ceil(svg->width * k);
		int width = inkW + 2 * offset + 2;
		int topPx = (int) std::lround(basePx + viewTop * k); // viewTop = -y1
		std::vector<Uint8> coverage((size_t) width * height * 4, 0);
		nsvgRasterize(
			rasterizer,
			svg,
			(float) (offset + 1),
			(float) topPx,
			(float) k,
			coverage.data(),
			width,
			height,
			width * 4
		);
		nsvgDelete(svg);

		std::vector<float> layered((size_t) width * height * 4, 0.f);
		for (size_t i = 0; i < (size_t) width * height; i++) {
			layered[i * 4 + 0] = face[0];
			layered[i * 4 + 1] = face[1];
			layered[i * 4 + 2] = face[2];
		}
		auto cov = [&](int x, int y) -> float {
			if (x < 0 || y < 0 || x >= width || y >= height) {
				return 0.f;
			}
			return coverage[((size_t) y * width + x) * 4 + 3] / 255.f;
		};
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < width; x++) {
				float* dst = &layered[((size_t) y * width + x) * 4];
				if (offset > 0) {
					Over(dst, hi, cov(x + offset, y + offset));
					Over(dst, shadow, cov(x - offset, y - offset));
				}
				Over(dst, face, cov(x, y));
			}
		}

		// Trim to the inked columns.
		int first = width;
		int last = -1;
		for (int x = 0; x < width; x++) {
			for (int y = 0; y < height; y++) {
				if (layered[((size_t) y * width + x) * 4 + 3] > 0.f) {
					first = std::min(first, x);
					last = std::max(last, x);
					break;
				}
			}
		}
		if (last < first) {
			first = 0;
			last = 0;
		}
		Piece piece;
		piece.m_width = last - first + 1;
		piece.m_rgba.resize((size_t) piece.m_width * height * 4);
		for (int y = 0; y < height; y++) {
			for (int x = 0; x < piece.m_width; x++) {
				const float* src = &layered[((size_t) y * width + first + x) * 4];
				Uint8* dst = &piece.m_rgba[((size_t) y * piece.m_width + x) * 4];
				for (int c = 0; c < 3; c++) {
					dst[c] = (Uint8) std::clamp((int) std::lround(src[c]), 0, 255);
				}
				dst[3] = (Uint8) std::clamp((int) std::lround(src[3] * 255.f), 0, 255);
			}
		}
		pieces.push_back(std::move(piece));
	}
	nsvgDeleteRasterizer(rasterizer);
	if (!ok) {
		return nullptr;
	}

	int total = gap;
	for (const Piece& piece : pieces) {
		total += piece.m_width + gap;
	}
	auto image = std::make_shared<HdImage>();
	image->m_width = total;
	image->m_height = height;
	image->m_hasAlpha = true;
	image->m_rgba.resize((size_t) total * height * 4);
	for (size_t i = 0; i < (size_t) total * height; i++) {
		image->m_rgba[i * 4 + 0] = face[0];
		image->m_rgba[i * 4 + 1] = face[1];
		image->m_rgba[i * 4 + 2] = face[2];
		image->m_rgba[i * 4 + 3] = 0;
	}
	p_cells.clear();
	int x = gap;
	for (const Piece& piece : pieces) {
		for (int y = 0; y < height; y++) {
			SDL_memcpy(
				&image->m_rgba[((size_t) y * total + x) * 4],
				&piece.m_rgba[(size_t) y * piece.m_width * 4],
				(size_t) piece.m_width * 4
			);
		}
		p_cells.push_back({x, piece.m_width, piece.m_width + gap});
		x += piece.m_width + gap;
	}
	SDL_Log(
		"HD textures: %s rasterised from outlines at %dx%d (%.2fx), %d glyphs",
		p_key.c_str(),
		total,
		height,
		(double) height / p_origHeight,
		(int) pieces.size()
	);
	return image;
}

} // namespace

void MiniwinHdSetFontSet(const char* p_name)
{
	SDL_SetStringProperty(SDL_GetGlobalProperties(), "miniwin.hd_font_set", p_name ? p_name : "");
}

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
	return !ResolveFile(KeyFromPath(p_gamePath)).empty();
}

static bool AttachBinding(
	IDirectDrawSurface* p_surface,
	const char* p_gamePath,
	const MiniwinHdRegion& p_region,
	const MiniwinHdBlit* p_blits,
	int p_blitCount
)
{
	if (!p_surface || !MiniwinHdHasReplacement(p_gamePath)) {
		return false;
	}
	if (p_region.m_imageWidth <= 0 || p_region.m_imageHeight <= 0 || p_region.m_textureWidth <= 0 ||
		p_region.m_textureHeight <= 0) {
		return false;
	}
	if (!p_blits && (p_region.m_srcWidth <= 0 || p_region.m_srcHeight <= 0)) {
		return false;
	}

	auto* surface = static_cast<MiniwinSurface*>(p_surface);
	auto hd = std::make_shared<MiniwinHdBinding>();
	hd->m_key = KeyFromPath(p_gamePath);
	{
		std::lock_guard<std::mutex> lock(State().m_mutex);
		hd->m_file = ResolveFile(hd->m_key);
	}
	hd->m_region = p_region;
	if (p_blits) {
		hd->m_blits.assign(p_blits, p_blits + p_blitCount);
	}
	surface->m_hd = hd;
	surface->m_textureDirty = true;
	return true;
}

bool MiniwinHdAttach(IDirectDrawSurface* p_surface, const char* p_gamePath, const MiniwinHdRegion& p_region)
{
	return AttachBinding(p_surface, p_gamePath, p_region, nullptr, 0);
}

bool MiniwinHdAttachBlits(
	IDirectDrawSurface* p_surface,
	const char* p_gamePath,
	const MiniwinHdRegion& p_region,
	const MiniwinHdBlit* p_blits,
	int p_blitCount
)
{
	if (!p_blits || p_blitCount <= 0) {
		return false;
	}
	return AttachBinding(p_surface, p_gamePath, p_region, p_blits, p_blitCount);
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
		image = LoadImage(p_binding.m_file, p_binding.m_file); // keyed by file: font sets share keys
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

	// GPU limit; every current backend handles 8192, and the art set caps at 4096.
	const int c_maxSize = 8192;
	if (outW <= 0 || outH <= 0 || outW > c_maxSize || outH > c_maxSize) {
		return false;
	}

	auto imageRowPtr = [&](int p_row) {
		if (region.m_flipVertically) {
			// The game stored the image's rows bottom-up; regions are in that flipped space.
			p_row = image->m_height - 1 - p_row;
		}
		return image->m_rgba.data() + (size_t) p_row * image->m_width * 4;
	};

	p_width = outW;
	p_height = outH;
	p_out.assign((size_t) outW * outH * 4, 0);

	if (p_binding.m_blits.empty()) {
		int srcX = (int) std::lround(region.m_srcX * scaleX);
		int srcY = (int) std::lround(region.m_srcY * scaleY);
		int copyW = std::min((int) std::lround(region.m_srcWidth * scaleX), image->m_width - srcX);
		int copyH = std::min((int) std::lround(region.m_srcHeight * scaleY), image->m_height - srcY);
		copyW = std::min(copyW, outW);
		copyH = std::min(copyH, outH);
		if (copyW <= 0 || copyH <= 0) {
			return false;
		}

		for (int y = 0; y < outH; y++) {
			// Rows/columns past the copied region repeat its edge so linear filtering at
			// the region's edge does not pull in the padding.
			const Uint8* src = imageRowPtr(srcY + std::min(y, copyH - 1)) + (size_t) srcX * 4;
			Uint8* dst = p_out.data() + (size_t) y * outW * 4;
			SDL_memcpy(dst, src, (size_t) copyW * 4);
			for (int x = copyW; x < outW; x++) {
				SDL_memcpy(dst + x * 4, src + (copyW - 1) * 4, 4);
			}
		}
	}
	else {
		for (const MiniwinHdBlit& blit : p_binding.m_blits) {
			// Scale edges rather than sizes so neighbouring pieces meet without gaps.
			int sx0 = (int) std::lround(blit.m_srcX * scaleX);
			int sx1 = (int) std::lround((blit.m_srcX + blit.m_width) * scaleX);
			if (blit.m_hdSrcX >= 0 && blit.m_hdSrcWidth > 0) {
				// Natural-width glyph: stretch its HD columns over the scaled destination.
				int srcX0 = blit.m_hdSrcX;
				int srcW = std::min(blit.m_hdSrcWidth, image->m_width - srcX0);
				int dstX0 = (int) std::lround(blit.m_dstX * scaleX);
				int dstW = std::min((int) std::lround((blit.m_dstX + blit.m_width) * scaleX) - dstX0, outW - dstX0);
				int ry0 = (int) std::lround(blit.m_srcY * scaleY);
				int ry1 = (int) std::lround((blit.m_srcY + blit.m_height) * scaleY);
				int dstY0 = (int) std::lround(blit.m_dstY * scaleY);
				int rows = std::min({ry1 - ry0, image->m_height - ry0, outH - dstY0});
				if (srcW <= 0 || dstW <= 0 || rows <= 0 || dstX0 < 0 || dstY0 < 0 || ry0 < 0) {
					continue;
				}
				// The glyph's cell carries a one original pixel transparent margin on each side so
				// filtered sampling never picks up the neighbouring glyph in the packed texture.
				int margin = std::max(1, (int) std::lround(scaleX));
				int inkW = dstW - 2 * margin;
				for (int y = 0; y < rows; y++) {
					const Uint8* srcRow = imageRowPtr(ry0 + y);
					Uint8* dstRow = p_out.data() + ((size_t) (dstY0 + y) * outW + dstX0) * 4;
					for (int x = 0; x < dstW; x++) {
						int ix = x - margin;
						if (inkW <= 0 || ix < 0 || ix >= inkW) {
							SDL_memset(dstRow + x * 4, 0, 4);
							continue;
						}
						int sx = srcX0 + std::min(srcW - 1, (int) ((ix + 0.5) * srcW / inkW));
						SDL_memcpy(dstRow + x * 4, srcRow + (size_t) sx * 4, 4);
					}
				}
				continue;
			}
			int sy0 = (int) std::lround(blit.m_srcY * scaleY);
			int sy1 = (int) std::lround((blit.m_srcY + blit.m_height) * scaleY);
			int dx0 = (int) std::lround(blit.m_dstX * scaleX);
			int dy0 = (int) std::lround(blit.m_dstY * scaleY);
			int w = std::min({sx1 - sx0, image->m_width - sx0, outW - dx0});
			int h = std::min({sy1 - sy0, image->m_height - sy0, outH - dy0});
			if (w <= 0 || h <= 0 || sx0 < 0 || sy0 < 0 || dx0 < 0 || dy0 < 0) {
				continue;
			}
			for (int y = 0; y < h; y++) {
				SDL_memcpy(
					p_out.data() + ((size_t) (dy0 + y) * outW + dx0) * 4,
					imageRowPtr(sy0 + y) + (size_t) sx0 * 4,
					(size_t) w * 4
				);
			}
		}
	}

	if (region.m_colorKeyed && !image->m_hasAlpha) {
		// No alpha in the replacement: key out its background colour like the original.
		const int c_tolerance = 40;
		for (size_t i = 0; i < p_out.size(); i += 4) {
			int dr = (int) p_out[i] - region.m_keyRed;
			int dg = (int) p_out[i + 1] - region.m_keyGreen;
			int db = (int) p_out[i + 2] - region.m_keyBlue;
			if (dr * dr + dg * dg + db * db <= c_tolerance * c_tolerance) {
				p_out[i + 3] = 0;
			}
		}
	}
	return true;
}

int MiniwinHdGetFontCells(
	const char* p_gamePath,
	int p_origHeight,
	int* p_x,
	int* p_width,
	int* p_advance,
	int p_maxCells,
	int* p_hdWidth,
	int* p_hdHeight,
	int* p_inkHeight
)
{
	if (!p_gamePath || !*p_gamePath) {
		return 0;
	}

	HdState& state = State();
	std::string key = KeyFromPath(p_gamePath);
	std::string file;
	std::string root;
	{
		std::lock_guard<std::mutex> lock(state.m_mutex);
		EnsureIndexed();
		if (!state.m_enabled) {
			return 0;
		}
		const auto* set = FontSetFiles();
		const char* name = SDL_GetStringProperty(SDL_GetGlobalProperties(), "miniwin.hd_font_set", "");
		auto rootIt = state.m_fontSetRoots.find(name ? name : "");
		if (!set || rootIt == state.m_fontSetRoots.end()) {
			return 0;
		}
		auto fileIt = set->find(key);
		if (fileIt == set->end()) {
			return 0;
		}
		file = fileIt->second;
		root = rootIt->second;
	}

	int count = 0;
	std::shared_ptr<HdImage> image;
	{
		std::lock_guard<std::mutex> lock(state.m_mutex);
		auto generated = state.m_generated.find(file);
		if (generated != state.m_generated.end()) {
			image = generated->second;
		}
	}
	std::vector<VectorCell> vectorCells;
	if (!image) {
		image = BuildVectorStrip(root, key, p_origHeight, vectorCells);
		if (image) {
			std::lock_guard<std::mutex> lock(state.m_mutex);
			state.m_generated[file] = image;
			state.m_generatedCells[file] = vectorCells;
		}
	}
	else {
		std::lock_guard<std::mutex> lock(state.m_mutex);
		vectorCells = state.m_generatedCells[file];
	}
	if (image) {
		if ((int) vectorCells.size() > p_maxCells) {
			return 0;
		}
		for (const VectorCell& cell : vectorCells) {
			p_x[count] = cell.m_x;
			p_width[count] = cell.m_width;
			p_advance[count] = cell.m_advance;
			count++;
		}
	}
	else {
		for (auto& fields : ReadCsvRows(fs::path(root) / "cells.csv")) {
			if (fields.size() < 6 || KeyFromPath(fields[0].c_str()) != key) {
				continue;
			}
			if (count >= p_maxCells) {
				return 0;
			}
			p_x[count] = SDL_atoi(fields[3].c_str());
			p_width[count] = SDL_atoi(fields[4].c_str());
			p_advance[count] = SDL_atoi(fields[5].c_str());
			count++;
		}
	}
	if (count == 0) {
		return 0;
	}

	if (!image) {
		std::lock_guard<std::mutex> lock(state.m_mutex);
		image = LoadImage(file, file);
	}
	if (!image) {
		return 0;
	}
	*p_hdWidth = image->m_width;
	*p_hdHeight = image->m_height;

	// Ink height of the first glyph (a capital): rows with any visible pixel.
	int top = -1;
	int bottom = -1;
	for (int y = 0; y < image->m_height; y++) {
		const Uint8* row = image->m_rgba.data() + (size_t) y * image->m_width * 4;
		for (int x = p_x[0]; x < p_x[0] + p_width[0] && x < image->m_width; x++) {
			const Uint8* px = row + x * 4;
			bool ink = image->m_hasAlpha ? px[3] > 128 : (px[0] + px[1] + px[2]) > 60;
			if (ink) {
				if (top < 0) {
					top = y;
				}
				bottom = y;
				break;
			}
		}
	}
	*p_inkHeight = top >= 0 ? bottom - top + 1 : image->m_height;
	return count;
}
