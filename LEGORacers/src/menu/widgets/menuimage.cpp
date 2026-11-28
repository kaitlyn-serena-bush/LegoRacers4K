#include "menu/widgets/menuimage.h"

#include "image/golimage.h"
#include "render/gold3drenderdevice.h"
#include "surface/golrendertarget.h"

#include <miniwin/miniwinapp.h>

DECOMP_SIZE_ASSERT(MenuImage, 0x5c)
DECOMP_SIZE_ASSERT(MenuImage::CreateParams, 0x3c)

// FUNCTION: LEGORACERS 0x0046f080
MenuImage::MenuImage()
{
	Reset();
}

// FUNCTION: LEGORACERS 0x0046f0f0
MenuImage::~MenuImage()
{
	MenuWidget::Destroy();
}

// FUNCTION: LEGORACERS 0x0046f140
void MenuImage::Reset()
{
	m_image = NULL;
	MenuWidget::Reset();
}

// FUNCTION: LEGORACERS 0x0046f150
LegoBool32 MenuImage::Create(CreateParams* p_createParams)
{
	Destroy();

	m_image = p_createParams->m_image;

	if (!p_createParams->m_rect.m_right) {
		p_createParams->m_rect.m_right = m_image->GetWidth() + p_createParams->m_rect.m_left;
	}

	if (!p_createParams->m_rect.m_bottom) {
		p_createParams->m_rect.m_bottom = m_image->GetHeight() + p_createParams->m_rect.m_top;
	}

	return CreateWidget(p_createParams);
}

// FUNCTION: LEGORACERS 0x0046f1a0
MenuWidget* MenuImage::DrawSelf(Rect* p_rect, Rect* p_arg)
{
	Rect sourceRect;
	sourceRect.m_top = 0;
	sourceRect.m_left = 0;
	sourceRect.m_right = m_image->GetWidth();
	sourceRect.m_bottom = m_image->GetHeight();

	ComputeScale(&sourceRect, &m_rect);
	ScaleRect(p_rect, &sourceRect);
	DrawImage(p_arg, &sourceRect, m_image);

	// [library:3d] Widescreen: a full-screen backdrop continues into the side areas.
	// The backdrop is seamlessly tileable, so each side wraps around to the opposite
	// edge of the image and the stud grid and shading continue without a seam.
	LegoS32 wideExtra = MiniwinGetWideExtra();
	const GolRenderTarget* target = m_renderer->GetRenderTargetInfo();
	if (wideExtra > 0 && m_rect.m_left == 0 && m_rect.m_top == 0 &&
		m_rect.m_right == static_cast<LegoS32>(target->GetWidth()) &&
		m_rect.m_bottom == static_cast<LegoS32>(target->GetHeight())) {
		LegoS32 imageWidth = m_image->GetWidth();
		LegoS32 sourceExtra = wideExtra * imageWidth / m_rect.m_right;
		Rect side;
		Rect source;
		side.m_top = 0;
		side.m_bottom = m_rect.m_bottom;
		source.m_top = 0;
		source.m_bottom = m_image->GetHeight();

		if (sourceExtra > imageWidth) {
			sourceExtra = imageWidth;
		}

		side.m_left = -wideExtra;
		side.m_right = 0;
		source.m_left = imageWidth - sourceExtra;
		source.m_right = imageWidth;
		DrawImage(&side, &source, m_image);

		side.m_left = m_rect.m_right;
		side.m_right = m_rect.m_right + wideExtra;
		source.m_left = 0;
		source.m_right = sourceExtra;
		DrawImage(&side, &source, m_image);
	}

	return NULL;
}
