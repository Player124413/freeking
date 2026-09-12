#include "Lightmap.h"

namespace Freeking
{
	LightmapImage::LightmapImage(int width, int height) :
		_width(width), _height(height)
	{
		Data.resize((width * height) * 3);
	}

	void LightmapImage::Insert(int rectX, int rectY, int width, int height, const uint8_t* buffer)
	{
		if (height <= 0 || width <= 0 || buffer == nullptr)
		{
			return;
		}

		// Clamp to the atlas: a misbehaving packer must not turn into a
		// heap out-of-bounds write.
		int x0 = rectX < 0 ? -rectX : 0;
		int y0 = rectY < 0 ? -rectY : 0;
		int x1 = width;
		int y1 = height;
		if (rectX + x1 > _width) x1 = _width - rectX;
		if (rectY + y1 > _height) y1 = _height - rectY;
		if (x1 <= x0 || y1 <= y0)
		{
			return;
		}

		for (int x = x0; x < x1; ++x)
		{
			for (int y = y0; y < y1; ++y)
			{
				int dstPixel = ((rectY + y) * _width) + (rectX + x);
				int dstIndex = dstPixel * 3;
				int srcPixel = (y * width) + x;
				int srcIndex = (srcPixel * 3);

				Data[dstIndex] = buffer[srcIndex];
				Data[dstIndex + 1] = buffer[srcIndex + 1];
				Data[dstIndex + 2] = buffer[srcIndex + 2];
			}
		}
	}
}
