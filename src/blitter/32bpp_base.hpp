/* $Id$ */

/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file 32bpp_base.hpp Base for all 32 bits blitters. */

#ifndef BLITTER_32BPP_BASE_HPP
#define BLITTER_32BPP_BASE_HPP

#include "base.hpp"
#include "../core/bitmath_func.hpp"
#include "../core/math_func.hpp"
#include "../gfx_func.h"

/** Base for all 32bpp blitters. */
class Blitter_32bppBase : public Blitter {
public:
	/* virtual */ uint8 GetScreenDepth() { return 32; }
	/* virtual */ void *MoveTo(void *video, int x, int y);
	/* virtual */ void SetPixel(void *video, int x, int y, uint8 colour);
	/* virtual */ void DrawRect(void *video, int width, int height, uint8 colour);
	/* virtual */ void CopyFromBuffer(void *video, const void *src, int width, int height);
	/* virtual */ void CopyToBuffer(const void *video, void *dst, int width, int height);
	/* virtual */ void CopyImageToBuffer(const void *video, void *dst, int width, int height, int dst_pitch);
	/* virtual */ void ScrollBuffer(void *video, int &left, int &top, int &width, int &height, int scroll_x, int scroll_y);
	/* virtual */ int BufferSize(int width, int height);
	/* virtual */ void PaletteAnimate(const Palette &palette);
	/* virtual */ Blitter::PaletteAnimation UsePaletteAnimation();
	/* virtual */ int GetBytesPerPixel() { return 4; }

	/**
	 * Compose a colour based on RGB values.
	 */
	static inline uint32 ComposeColour(uint a, uint r, uint g, uint b)
	{
		return (((a) << 24) & 0xFF000000) | (((r) << 16) & 0x00FF0000) | (((g) << 8) & 0x0000FF00) | ((b) & 0x000000FF);
	}

	/**
	 * Look up the colour in the current palette.
	 */
	static inline uint32 LookupColourInPalette(uint index)
	{
		return _cur_palette.palette[index].data;
	}

	/**
	 * Compose a colour based on RGBA values and the current pixel value.
	 */
	static inline uint32 ComposeColourRGBANoCheck(uint r, uint g, uint b, uint a, uint32 current)
	{
		uint cr = GB(current, 16, 8);
		uint cg = GB(current, 8,  8);
		uint cb = GB(current, 0,  8);

		/* The 256 is wrong, it should be 255, but 256 is much faster... */
		return ComposeColour(0xFF,
							((int)(r - cr) * a) / 256 + cr,
							((int)(g - cg) * a) / 256 + cg,
							((int)(b - cb) * a) / 256 + cb);
	}

	/**
	 * Compose a colour based on RGBA values and the current pixel value.
	 * Handles fully transparent and solid pixels in a special (faster) way.
	 */
	static inline uint32 ComposeColourRGBA(uint r, uint g, uint b, uint a, uint32 current)
	{
		if (a == 0) return current;
		if (a >= 255) return ComposeColour(0xFF, r, g, b);

		return ComposeColourRGBANoCheck(r, g, b, a, current);
	}

	/**
	 * Compose a colour based on Pixel value, alpha value, and the current pixel value.
	 */
	static inline uint32 ComposeColourPANoCheck(uint32 colour, uint a, uint32 current)
	{
		uint r  = GB(colour,  16, 8);
		uint g  = GB(colour,  8,  8);
		uint b  = GB(colour,  0,  8);

		return ComposeColourRGBANoCheck(r, g, b, a, current);
	}

	/**
	 * Compose a colour based on Pixel value, alpha value, and the current pixel value.
	 * Handles fully transparent and solid pixels in a special (faster) way.
	 */
	static inline uint32 ComposeColourPA(uint32 colour, uint a, uint32 current)
	{
		if (a == 0) return current;
		if (a >= 255) return (colour | 0xFF000000);

		return ComposeColourPANoCheck(colour, a, current);
	}

	/**
	 * Make a pixel looks like it is transparent.
	 * @param colour the colour already on the screen.
	 * @param nom the amount of transparency, nominator, makes colour lighter.
	 * @param denom denominator, makes colour darker.
	 * @return the new colour for the screen.
	 */
	static inline uint32 MakeTransparent(uint32 colour, uint nom, uint denom = 256)
	{
		uint r = GB(colour, 16, 8);
		uint g = GB(colour, 8,  8);
		uint b = GB(colour, 0,  8);

		return ComposeColour(0xFF, r * nom / denom, g * nom / denom, b * nom / denom);
	}

	/**
	 * Make a colour grey - based.
	 * @param colour the colour to make grey.
	 * @return the new colour, now grey.
	 */
	static inline uint32 MakeGrey(uint32 colour)
	{
		uint r = GB(colour, 16, 8);
		uint g = GB(colour, 8,  8);
		uint b = GB(colour, 0,  8);

		/* To avoid doubles and stuff, multiple it with a total of 65536 (16bits), then
		 *  divide by it to normalize the value to a byte again. See heightmap.cpp for
		 *  information about the formula. */
		colour = ((r * 19595) + (g * 38470) + (b * 7471)) / 65536;

		return ComposeColour(0xFF, colour, colour, colour);
	}

	static const int DEFAULT_BRIGHTNESS = 64;

	static inline uint32 AdjustBrightness(uint32 colour, uint8 brightness)
	{
		/* Shortcut for normal brightness */
		if (brightness == DEFAULT_BRIGHTNESS) return colour;

		uint16 ob = 0;
		uint16 r = GB(colour, 16, 8) * brightness / DEFAULT_BRIGHTNESS;
		uint16 g = GB(colour, 8,  8) * brightness / DEFAULT_BRIGHTNESS;
		uint16 b = GB(colour, 0,  8) * brightness / DEFAULT_BRIGHTNESS;

		/* Sum overbright */
		if (r > 255) ob += r - 255;
		if (g > 255) ob += g - 255;
		if (b > 255) ob += b - 255;

		if (ob == 0) return ComposeColour(GB(colour, 24, 8), r, g, b);

		/* Reduce overbright strength */
		ob /= 2;
		return ComposeColour(GB(colour, 24, 8),
		                     r >= 255 ? 255 : min(r + ob * (255 - r) / 256, 255),
		                     g >= 255 ? 255 : min(g + ob * (255 - g) / 256, 255),
		                     b >= 255 ? 255 : min(b + ob * (255 - b) / 256, 255));
	}
};

#endif /* BLITTER_32BPP_BASE_HPP */
