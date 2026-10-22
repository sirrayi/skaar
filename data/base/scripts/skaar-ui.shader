// 2D UI shaders. Scripts make the engine mipmap these images even when they're
// registered with RegisterShaderNoMip (the only option the ui module has), so
// text stays smooth at every size. nopicmip keeps full resolution.

fonts/montserrat-200
{
	nopicmip
	{
		map fonts/montserrat-200.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
}

fonts/montserrat-300
{
	nopicmip
	{
		map fonts/montserrat-300.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
}

fonts/jbmono-400
{
	nopicmip
	{
		map fonts/jbmono-400.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
}

gfx/skaar/mark
{
	nopicmip
	{
		map gfx/skaar/mark.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
}

gfx/skaar/cursor
{
	nopicmip
	nomipmaps
	{
		map gfx/skaar/cursor.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen vertex
		alphaGen vertex
	}
}
