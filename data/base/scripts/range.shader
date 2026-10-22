// Aim-range surfaces. mapc lights them by vertex; exactVertex takes those
// colours as baked, without the engine halving them for overbright, so the
// light taupe textures land on screen close to their designed values.

textures/skaar/range_floor
{
	{
		map textures/skaar/range_floor.tga
		rgbGen exactVertex
	}
}

textures/skaar/range_wall
{
	{
		map textures/skaar/range_wall.tga
		rgbGen exactVertex
	}
}

textures/skaar/range_ceiling
{
	{
		map textures/skaar/range_ceiling.tga
		rgbGen exactVertex
	}
}

textures/skaar/range_pad
{
	{
		map textures/skaar/range_pad.tga
		rgbGen exactVertex
	}
}
