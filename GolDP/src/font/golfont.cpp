#include "font/golfont.h"

#include <cmath>
#include <miniwin/hdtexture.h>
#include <string>
#include <unordered_map>
#include <vector>

// [library:hd] Font sets with natural glyph widths (cells.csv): each glyph's HD columns,
// indexed like m_glyphs before sorting, plus the original-pixel width of the HD strip.
struct HdFontCells {
	std::vector<int> m_hdX;
	std::vector<int> m_hdWidth;
	int m_imageWidth = 0;
};

static std::unordered_map<const GolFont*, HdFontCells>& HdFontCellMap()
{
	static std::unordered_map<const GolFont*, HdFontCells> s_cells;
	return s_cells;
}

// [library:hd] Game path each font was loaded from, for its glyph textures' HD pixels.
static std::unordered_map<const GolFont*, std::string>& HdFontPaths()
{
	static std::unordered_map<const GolFont*, std::string> s_paths;
	return s_paths;
}

#include "decomp.h"
#include "device/golpalettebase.h"
#include "golbmpfile.h"
#include "golerror.h"
#include "golimgfile.h"
#include "golsurface.h"
#include "goltgafile.h"
#include "material/golsoftwarematerial.h"
#include "render/gold3drenderdevice.h"
#include "render/rectangle.h"
#include "surface/gold3dtexture.h"

#include <stdlib.h>
#include <string.h>

DECOMP_SIZE_ASSERT(GolFont, 0xa0)

// GLOBAL: GOLDP 0x10062568
static GolImgFile g_fontImgFile;

extern GolTexture* g_fontSourceImage;

extern GolTgaFile g_textureTgaFile;

extern GolBmpFile g_textureBmpFile;

extern const ColorRGBA g_transparentBlack;

// FUNCTION: GOLDP 0x100043d0
GolFont::GolFont()
{
	m_textures = NULL;
	m_materials = NULL;
	m_renderer = NULL;
}

// FUNCTION: GOLDP 0x10004480
GolFont::~GolFont()
{
	HdFontPaths().erase(this);   // [library:hd]
	HdFontCellMap().erase(this); // [library:hd]
	Clear();
}

// FUNCTION: GOLDP 0x10004520
void GolFont::Clear()
{
	m_sourceImage.Destroy();
	ReleaseSurfaces();
	GolFontBase::Clear();
}

// FUNCTION: GOLDP 0x10004570
void GolFont::Load(const LegoChar* p_name, GolD3DRenderDevice* p_renderer)
{
	GolSurfaceFormat sourceFormat;

	GolImgFile* imageFile = &g_textureTgaFile;
	if (!(m_flags & c_flagTgaSource)) {
		imageFile = &g_textureBmpFile;
	}

	imageFile->Open(p_name);
	LegoU32 sourceHeight = imageFile->GetHeight();
	sourceFormat = imageFile->GetTextureFormat();
	m_fontHeight = sourceHeight;

	if (sourceFormat.m_paletteMask && sourceFormat.m_bitsPerPixel == 4) {
		::memset(&sourceFormat, 0, sizeof(sourceFormat));
		sourceFormat.m_bitsPerPixel = 8;
		sourceFormat.m_paletteMask = 0x00ff;
	}

	LegoU32 sourceWidth = imageFile->GetWidth();
	g_fontSourceImage = &m_sourceImage;
	m_sourceImage.Allocate(*p_renderer, sourceFormat, sourceWidth, m_fontHeight);
	imageFile->LoadSurface(&m_sourceImage, m_flags & c_flagBit2, NULL);
	HdFontPaths()[this] = imageFile->GetHdSourcePath(); // [library:hd]
	imageFile->Destroy();

	ScanGlyphs(p_name);
	ApplyHdFontCells(); // [library:hd]

	GolSurfaceFormat textureFormat;
	g_fontSourceImage = &m_sourceImage;
	GolSurfaceFormat surfaceFormat = m_sourceImage.GetTextureFormat();
	LegoU32 selectFlags = m_flags & c_flagColorKeyed;
	p_renderer->SelectTextureFormat(surfaceFormat, &textureFormat, selectFlags);
	PackGlyphTextures(p_renderer, &textureFormat);
	CreateSurfaces(p_renderer, &textureFormat);
	CopyGlyphsToTextures(p_renderer, &surfaceFormat, &textureFormat);

	::qsort(m_glyphs, m_glyphCount, sizeof(Glyph), GolFontBase::CompareGlyphChars);
}

// FUNCTION: GOLDP 0x100046e0
void GolFont::ReleaseSurfaces()
{
	if (m_materials != NULL) {
		delete[] m_materials;
		m_materials = NULL;
	}

	if (m_textures != NULL) {
		delete[] m_textures;
		m_textures = NULL;
	}
}

// FUNCTION: GOLDP 0x10004720
void GolFont::RefreshSurfaces(GolD3DRenderDevice* p_renderer)
{
	GolSurfaceFormat textureFormat;

	g_fontSourceImage = &m_sourceImage;
	GolSurfaceFormat sourceFormat = m_sourceImage.GetTextureFormat();
	p_renderer->SelectTextureFormat(sourceFormat, &textureFormat, m_flags & c_flagColorKeyed);
	PackGlyphTextures(p_renderer, &textureFormat);
	CreateSurfaces(p_renderer, &textureFormat);
	CopyGlyphsToTextures(p_renderer, &sourceFormat, &textureFormat);
}

// FUNCTION: GOLDP 0x100047b0
void GolFont::CreateSurfaces(GolD3DRenderDevice* p_renderer, GolSurfaceFormat* p_textureFormat)
{
	m_textures = new GolD3DTexture[m_surfaceCount];
	if (m_textures == NULL) {
		GOL_FATALERROR(c_golErrorOutOfMemory);
	}

	m_materials = new GolSoftwareMaterial[m_surfaceCount];
	if (m_materials == NULL) {
		GOL_FATALERROR(c_golErrorOutOfMemory);
	}

	LegoU32 i;
	for (i = 0; i < m_surfaceCount - 1; i++) {
		if (m_flags & c_flagColorKeyed) {
			m_textures[i].SetTextureFlags(GolTexture::c_textureFlagColorKeyed);
			m_textures[i].SetColorKey(m_colorKey);
		}

		m_textures[i].AllocateDeviceOnly(*p_renderer, *p_textureFormat, m_maxTextureWidth, m_maxTextureHeight);

		GolSoftwareMaterial* material = &m_materials[i];
		GolMaterialParams params;
		params.m_flags = c_fontMaterialFlags;
		params.m_texture = &m_textures[i];
		params.m_ambient.m_red = 0xff;
		params.m_ambient.m_grn = 0xff;
		params.m_ambient.m_blu = 0xff;
		params.m_ambient.m_alp = 0xff;
		params.m_diffuse.m_red = 0xff;
		params.m_diffuse.m_grn = 0xff;
		params.m_diffuse.m_blu = 0xff;
		params.m_diffuse.m_alp = 0xff;
		params.m_alphaFunc = 2;
		params.m_alphaRef = 0;
		params.m_srcBlend = 1;
		params.m_destBlend = 0;

		material->SetParams(p_renderer, params);
		material->Create(*p_renderer);
	}

	if (m_flags & c_flagColorKeyed) {
		m_textures[m_surfaceCount - 1].SetTextureFlags(GolTexture::c_textureFlagColorKeyed);
		m_textures[m_surfaceCount - 1].SetColorKey(m_colorKey);
	}

	m_textures[m_surfaceCount - 1].AllocateDeviceOnly(*p_renderer, *p_textureFormat, m_textureWidth, m_textureHeight);
	m_textures[m_surfaceCount - 1].SetName(m_name);

	{
		GolMaterialParams params;
		params.m_flags = c_fontMaterialFlags;
		params.m_ambient.m_red = 0xff;
		params.m_ambient.m_grn = 0xff;
		params.m_ambient.m_blu = 0xff;
		params.m_ambient.m_alp = 0xff;
		params.m_diffuse.m_red = 0xff;
		params.m_diffuse.m_grn = 0xff;
		params.m_diffuse.m_blu = 0xff;
		params.m_diffuse.m_alp = 0xff;
		params.m_alphaFunc = 2;
		params.m_alphaRef = 0;
		params.m_srcBlend = 1;
		params.m_destBlend = 0;

		GolSoftwareMaterial* material = &m_materials[m_surfaceCount - 1];
		params.m_texture = &m_textures[m_surfaceCount - 1];

		material->SetParams(p_renderer, params);
		material->Create(*p_renderer);
	}
}

// FUNCTION: GOLDP 0x10004b60
GolD3DTexture* GolFont::GetTexture(LegoU32 p_index)
{
	return &m_textures[p_index];
}

// FUNCTION: GOLDP 0x10004b80
void GolFont::BeginDrawing(GolRenderDevice* p_renderer, LegoU32)
{
	m_renderer = static_cast<GolD3DRenderDevice*>(p_renderer);
}

// FUNCTION: GOLDP 0x10004b90
void GolFont::SelectSurface(LegoU32 p_index)
{
	LegoU32 index = p_index;
	GolSoftwareMaterial* material = &m_materials[index];
	(m_renderer->*m_renderer->m_applyMaterialFn)(material);

	GolTexture* texture = &m_textures[index];
	m_renderer->SetCurrentTexture(texture);
	m_inverseTextureWidth = 1.0f / static_cast<LegoFloat>(m_textures[index].GetWidth());
	m_inverseTextureHeight = 1.0f / static_cast<LegoFloat>(m_textures[index].GetHeight());
}

// FUNCTION: GOLDP 0x10004c20
void GolFont::DrawGlyph(Rect* p_sourceRect, Rect* p_destRect)
{
	LegoS32 sourceBottom = p_sourceRect->m_bottom - 1;
	D3DTLVERTEX vertices[4];

	vertices[0].sz = 0.0f;
	vertices[1].sz = 0.0f;
	vertices[2].sz = 0.0f;

	vertices[0].sx = (static_cast<LegoFloat>(p_destRect->m_left));
	vertices[3].sz = 0.0f;
	vertices[0].rhw = 1.0f;
	vertices[0].sy = (static_cast<LegoFloat>(p_destRect->m_top));
	vertices[1].rhw = 1.0f;
	vertices[2].rhw = 1.0f;
	vertices[1].sx = vertices[0].sx;
	vertices[1].sy = (static_cast<LegoFloat>(p_destRect->m_bottom));
	vertices[2].sx = (static_cast<LegoFloat>(p_destRect->m_right));
	vertices[3].sx = vertices[2].sx;
	vertices[2].sy = vertices[0].sy;
	vertices[3].rhw = 1.0f;
	vertices[3].sy = vertices[1].sy;

	LegoFloat quarter = m_inverseTextureWidth;
	quarter = (quarter * 0.25f);
	vertices[0].tu = (((static_cast<LegoFloat>(p_sourceRect->m_left))) * m_inverseTextureWidth) + quarter;
	vertices[0].tv = (((((static_cast<LegoFloat>(p_sourceRect->m_top))) * m_inverseTextureHeight)));
	vertices[1].tu = vertices[0].tu;
	vertices[1].tv = ((((static_cast<LegoFloat>(sourceBottom))) * m_inverseTextureHeight));
	vertices[2].tu = (((static_cast<LegoFloat>(p_sourceRect->m_right))) * GetInverseTextureWidth()) + quarter;
	vertices[2].tv = vertices[0].tv;
	vertices[3].tu = vertices[2].tu;
	vertices[3].tv = vertices[1].tv;

	LegoU32 color =
		(m_color.m_alp << 24) | (m_color.m_red << 16) | (m_color.m_grn << 8) | static_cast<LegoU32>(m_color.m_blu);
	vertices[0].color = color;
	vertices[1].color = color;
	vertices[2].color = color;
	vertices[3].color = color;

	vertices[0].specular = 0xffffffff;
	vertices[1].specular = 0xffffffff;
	vertices[2].specular = 0xffffffff;
	vertices[3].specular = 0xffffffff;

	m_renderer->DrawTriangleStrip(vertices, sizeOfArray(vertices));
}

// FUNCTION: GOLDP 0x10004d70
void GolFont::CopyGlyphsToTextures(
	GolD3DRenderDevice* p_renderer,
	const GolSurfaceFormat* p_sourceFormat,
	GolSurfaceFormat* p_textureFormat
)
{
	GolFont* font = this;
	LegoU32 currentSurface = 0;
	GolD3DTexture* texture = font->GetTexture(currentSurface);

	ColorRGBA* paletteEntries;
	LegoU32 paletteSize;
	if (p_sourceFormat->m_paletteMask) {
		paletteEntries = font->m_sourceImage.GetPaletteEntries();
		paletteSize = font->m_sourceImage.GetPaletteSize();
	}
	else {
		paletteEntries = NULL;
		paletteSize = 0;
	}

	ColorRGBA* colorKey;
	if (font->m_flags & c_flagColorKeyed) {
		if (p_renderer->GetFlags() & GolRenderDevice::c_flagBlackColorKey) {
			g_fontImgFile.SetColorKeyReplacement(g_transparentBlack);
		}
		else {
			g_fontImgFile.SetColorKeyReplacement(font->m_colorKey);
		}
		colorKey = &font->m_colorKey;
	}
	else {
		colorKey = NULL;
	}

	LegoU8* sourcePixels;
	LegoU32 sourcePitch;
	font->m_sourceImage.LockPixels(&sourcePixels, &sourcePitch, GolSurface::c_lockRequestRead);

	LegoU8* destPixels;
	LegoU32 destPitch;
	texture->LockPixels(&destPixels, &destPitch, GolSurface::c_lockRequestWrite);

	for (LegoU32 i = 0; i < static_cast<LegoU32>(font->m_glyphCount); i++) {
		if (font->m_glyphs[i].m_surfaceIndex != currentSurface) {
			texture->UnlockPixels();
			currentSurface = font->m_glyphs[i].m_surfaceIndex;
			texture = font->GetTexture(currentSurface);
			texture->LockPixels(&destPixels, &destPitch, GolSurface::c_lockRequestWrite);
		}

		g_fontImgFile.SetImageInfo(
			*p_sourceFormat,
			font->m_glyphs[i].m_width,
			font->m_fontHeight,
			sourcePitch,
			paletteEntries,
			paletteSize
		);

		LegoU8* source =
			sourcePixels +
			((static_cast<LegoU32>(p_sourceFormat->m_bitsPerPixel) * font->m_glyphs[i].m_sourceX + 7) >> 3);
		LegoU8* dest =
			destPixels + destPitch * font->m_glyphs[i].m_textureY +
			((static_cast<LegoU32>(p_textureFormat->m_bitsPerPixel) * font->m_glyphs[i].m_textureX + 7) >> 3);

		GolPaletteBase* palette = p_textureFormat->m_paletteMask ? texture->GetPalette() : NULL;
		g_fontImgFile.ConvertImage(
			source,
			dest,
			font->m_glyphs[i].m_width,
			font->m_fontHeight,
			destPitch,
			*p_textureFormat,
			palette,
			0,
			colorKey
		);
	}

	texture->UnlockPixels();
	font->m_sourceImage.UnlockPixels();

	// [library:hd] Give each glyph texture the same glyphs cut from the HD font strip.
	auto hdPath = HdFontPaths().find(this);
	if (hdPath != HdFontPaths().end()) {
		for (LegoU32 surface = 0; surface < m_surfaceCount; surface++) {
			std::vector<MiniwinHdBlit> blits;
			auto cells = HdFontCellMap().find(this);
			for (LegoU32 i = 0; i < static_cast<LegoU32>(m_glyphCount); i++) {
				if (m_glyphs[i].m_surfaceIndex != surface) {
					continue;
				}
				MiniwinHdBlit blit;
				if (cells != HdFontCellMap().end() && i < cells->second.m_hdX.size()) {
					blit.m_hdSrcX = cells->second.m_hdX[i];
					blit.m_hdSrcWidth = cells->second.m_hdWidth[i];
				}
				blit.m_srcX = m_glyphs[i].m_sourceX;
				blit.m_srcY = 0;
				blit.m_width = m_glyphs[i].m_width;
				blit.m_height = m_fontHeight;
				blit.m_dstX = m_glyphs[i].m_textureX;
				blit.m_dstY = m_glyphs[i].m_textureY;
				blits.push_back(blit);
			}
			auto cellsForSize = HdFontCellMap().find(this);
			GetTexture(surface)->BindHdReplacementBlits(
				hdPath->second.c_str(),
				cellsForSize != HdFontCellMap().end() ? cellsForSize->second.m_imageWidth : m_sourceImage.GetWidth(),
				m_sourceImage.GetHeight(),
				(m_flags & c_flagBit2) != 0,
				blits.data(),
				(LegoU32) blits.size()
			);
		}
	}
}

// FUNCTION: GOLDP 0x10029920 FOLDED
void GolFont::EndDrawing()
{
	// empty
}

// [library:hd] LegoRacers4K: when this font's HD strip comes from a font set with natural
// glyph widths (cells.csv), take each glyph's width, the spacing and the space from it
// (converted to original pixels) instead of the original strip's cells. The original
// pixels copied into the glyph textures are then placeholders; the HD strip covers them.
void GolFont::ApplyHdFontCells()
{
	HdFontCellMap().erase(this);
	auto path = HdFontPaths().find(this);
	if (path == HdFontPaths().end() || path->second.empty() || m_glyphCount < 2 || !m_fontHeight) {
		return;
	}

	std::vector<int> x(m_glyphCount);
	std::vector<int> width(m_glyphCount);
	std::vector<int> advance(m_glyphCount);
	int hdWidth = 0;
	int hdHeight = 0;
	int inkHeight = 0;
	int count = MiniwinHdGetFontCells(
		path->second.c_str(),
		(int) m_fontHeight,
		x.data(),
		width.data(),
		advance.data(),
		(int) m_glyphCount,
		&hdWidth,
		&hdHeight,
		&inkHeight
	);
	if (count != (int) m_glyphCount - 1 || hdHeight <= 0) {
		return;
	}

	double scale = (double) hdHeight / m_fontHeight;
	LegoU32 sourceWidth = m_sourceImage.GetWidth();
	HdFontCells& cells = HdFontCellMap()[this];
	cells.m_imageWidth = (int) std::lround(hdWidth / scale);
	cells.m_hdX.assign(m_glyphCount, -1);
	cells.m_hdWidth.assign(m_glyphCount, 0);
	// The space has no ink: fill its texture cell from the strip's blank first column.
	cells.m_hdX[0] = 0;
	cells.m_hdWidth[0] = 1;
	for (LegoU32 i = 1; i < m_glyphCount; i++) {
		// +2: one transparent original pixel each side (filled in by the HD blit).
		LegoU32 glyphWidth = (LegoU32) std::max(1L, std::lround(width[i - 1] / scale)) + 2;
		if (glyphWidth > sourceWidth) {
			glyphWidth = sourceWidth;
		}
		m_glyphs[i].m_width = static_cast<LegoU16>(glyphWidth);
		m_glyphs[i].m_sourceX = 0;
		cells.m_hdX[i] = x[i - 1];
		cells.m_hdWidth[i] = width[i - 1];
	}

	m_charSpacing = (LegoS32) std::lround((advance[0] - width[0]) / scale) - 2;
	m_spaceWidth = (LegoS32) std::max(1L, std::lround(0.43 * inkHeight / scale)) + 2;
	m_glyphs[0].m_width = static_cast<LegoU16>(m_spaceWidth);
}
