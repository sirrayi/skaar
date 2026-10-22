// Aim-trainer targets. The cgame sets the colour per entity (shaderRGBA);
// the second stage multiplies a soft matcap over it so spheres read as round
// without depending on map light.

skaar/target
{
	nopicmip
	cull none
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
