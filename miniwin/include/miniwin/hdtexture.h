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
	int m_imageWidth; // original image size, in original pixels
	int m_imageHeight;
	int m_srcX; // region of the original image copied into the texture's top-left
	int m_srcY;
	int m_srcWidth;
	int m_srcHeight;
	int m_textureWidth; // texture surface size, in original pixels (may be padded)
	int m_textureHeight;
	bool m_flipVertically; // the game stored rows bottom-up
	// Colour-keyed textures: replacements without an alpha channel get this colour
	// turned transparent, like the original key.
	bool m_colorKeyed;
	unsigned char m_keyRed;
	unsigned char m_keyGreen;
	unsigned char m_keyBlue;
};

// One rectangle of the original image copied to a spot in the texture, in original
// pixels. Used for textures assembled from pieces (font glyphs packed into a texture).
struct MiniwinHdBlit {
	int m_srcX;
	int m_srcY;
	int m_width;
	int m_height;
	int m_dstX;
	int m_dstY;
	// Optional: the piece's columns in the HD image itself (HD pixels), for HD strips laid
	// out differently from the original (font sets with natural glyph widths). The piece
	// is stretched to fill its scaled destination width.
	int m_hdSrcX = -1;
	int m_hdSrcWidth = 0;
};

// Returns true when HD replacements are enabled and one exists for this game path
// (e.g. "GAMEDATA\\COMMON\\AD_CHST.BMP"). Case-insensitive; the extension is ignored.
bool MiniwinHdHasReplacement(const char* p_gamePath);

// Tags p_surface with the replacement for p_gamePath. Returns false (and leaves the
// surface untouched) when there is no usable replacement.
bool MiniwinHdAttach(IDirectDrawSurface* p_surface, const char* p_gamePath, const MiniwinHdRegion& p_region);

// Like MiniwinHdAttach, for a texture assembled from several pieces of the image.
// p_region gives the image size, texture size, flip and colour key; its src fields are unused.
bool MiniwinHdAttachBlits(
	IDirectDrawSurface* p_surface,
	const char* p_gamePath,
	const MiniwinHdRegion& p_region,
	const MiniwinHdBlit* p_blits,
	int p_blitCount
);

// Removes any HD tag (e.g. when the game starts writing the texture's pixels itself).
void MiniwinHdDetach(IDirectDrawSurface* p_surface);

// Selects a font set: files in that set's folder (mirroring the HD folder) override the
// HD folder's while selected. NULL or "" = none. Shared by every copy of miniwin through
// an SDL global property; textures bound afterwards pick it up.
void MiniwinHdSetFontSet(const char* p_name);

// Glyph cells of a font strip whose replacement comes from a font set with natural glyph
// widths: the set's cells.csv rows for that strip, in strip order (HD pixels). Also gives
// the HD image size and its first glyph's ink height. Returns the number of cells (0 when
// the replacement is not such a strip).
int MiniwinHdGetFontCells(
	const char* p_gamePath,
	int* p_x,
	int* p_width,
	int* p_advance,
	int p_maxCells,
	int* p_hdWidth,
	int* p_hdHeight,
	int* p_inkHeight
);

// Disables HD replacements for this run (command line --nohd).
void MiniwinHdSetEnabled(bool p_enabled);
