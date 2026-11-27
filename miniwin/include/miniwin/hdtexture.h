#pragma once

// [library:hd] HD texture overrides (LegoRacers4K).
//
// The game keeps loading its original low-resolution textures exactly as before
// (sizes, palettes, colour keys and UVs are unchanged). When a replacement PNG exists
// for a texture, the surface is tagged with it and the render backend samples the PNG
// instead of the original pixels. UVs are normalised, so a larger image just works.
//
// Replacements live in a folder that mirrors LEGO.JAM: HD/GAMEDATA/COMMON/AD_CHST.png
// replaces GAMEDATA\COMMON\AD_CHST.BMP. The folder is found from RACERS_HD_DIR, then
// "HD" next to the game data, then "HD" next to the executable. --nohd turns it off.

struct IDirectDrawSurface;

// Region of an original image held by one texture surface. A whole-image texture uses
// x = y = 0 and the image size; tiled 2D images (menus) use one region per tile.
struct MiniwinHdRegion {
	int m_imageWidth;  // original image size, in original pixels
	int m_imageHeight;
	int m_srcX; // region of the original image copied into the texture's top-left
	int m_srcY;
	int m_srcWidth;
	int m_srcHeight;
	int m_textureWidth; // texture surface size, in original pixels (may be padded)
	int m_textureHeight;
	bool m_flipVertically; // the game stored rows bottom-up
};

// Returns true when HD replacements are enabled and one exists for this game path
// (e.g. "GAMEDATA\\COMMON\\AD_CHST.BMP"). Case-insensitive; the extension is ignored.
bool MiniwinHdHasReplacement(const char* p_gamePath);

// Tags p_surface with the replacement for p_gamePath. Returns false (and leaves the
// surface untouched) when there is no usable replacement.
bool MiniwinHdAttach(IDirectDrawSurface* p_surface, const char* p_gamePath, const MiniwinHdRegion& p_region);

// Removes any HD tag (e.g. when the game starts writing the texture's pixels itself).
void MiniwinHdDetach(IDirectDrawSurface* p_surface);

// Disables HD replacements for this run (command line --nohd).
void MiniwinHdSetEnabled(bool p_enabled);
