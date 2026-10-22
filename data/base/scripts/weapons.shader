// Weapon effects (P6). Colours come from the entity (shaderRGBA) or vertex
// colours, so one texture serves every weapon. Additive, so they glow on dark
// and light ground alike.

skaar/fx_glow
{
	nopicmip
	cull none
	{
		map gfx/skaar/fx_glow.tga
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen entity
		alphaGen entity
	}
}

skaar/fx_beam
{
	nopicmip
	cull none
	{
		map gfx/skaar/fx_beam.tga
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen vertex
		alphaGen vertex
	}
}

skaar/fx_bolt
{
	nopicmip
	cull none
	{
		map gfx/skaar/fx_bolt.tga
		blendFunc GL_SRC_ALPHA GL_ONE
		rgbGen entity
		tcMod scroll -4 0
	}
}

skaar/fx_ring
{
	nopicmip
	cull none
	{
		map gfx/skaar/fx_ring.tga
		blendFunc GL_SRC_ALPHA GL_ONE_MINUS_SRC_ALPHA
		rgbGen entity
		alphaGen entity
	}
}

// practice dummies: flat colour from the entity with the soft target matcap over it
skaar/dummy
{
	nopicmip
	{
		map $whiteimage
		rgbGen entity
	}
	{
		map gfx/skaar/target_shade.tga
		tcGen environment
		blendFunc GL_DST_COLOR GL_ZERO
		rgbGen identity
	}
}
