/*
 * Copyright (C) 2009 Maciej Cencora.
 * Copyright (C) 2008 Nicolai Haehnle.
 *
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "radeon_mipmap_tree.h"

#include <errno.h>
#include <unistd.h>

#include "main/simple_list.h"
#include "main/texcompress.h"
#include "main/teximage.h"
#include "main/texobj.h"
#include "radeon_texture.h"

static unsigned get_aligned_compressed_row_stride(
		gl_format format,
		unsigned width,
		unsigned minStride)
{
	const unsigned blockSize = _mesa_get_format_bytes(format);
	unsigned blockWidth, blockHeight, numXBlocks;

	_mesa_get_format_block_size(format, &blockWidth, &blockHeight);
	numXBlocks = (width + blockWidth - 1) / blockWidth;

	while (numXBlocks * blockSize < minStride)
	{
		++numXBlocks;
	}

	return numXBlocks * blockSize;
}

static unsigned get_compressed_image_size(
		gl_format format,
		unsigned rowStride,
		unsigned height)
{
	unsigned blockWidth, blockHeight;

	_mesa_get_format_block_size(format, &blockWidth, &blockHeight);

	return rowStride * ((height + blockHeight - 1) / blockHeight);
}

/**
 * Compute sizes and fill in offset and blit information for the given
 * image (determined by \p face and \p level).
 *
 * \param curOffset points to the offset at which the image is to be stored
 * and is updated by this function according to the size of the image.
 */
static void compute_tex_image_offset(radeonContextPtr rmesa, radeon_mipmap_tree *mt,
	GLuint face, GLuint level, GLuint* curOffset)
{
	radeon_mipmap_level *lvl = &mt->levels[level];
	uint32_t row_align;

	/* Find image size in bytes */
	if (_mesa_is_format_compressed(mt->mesaFormat)) {
		lvl->rowstride = get_aligned_compressed_row_stride(mt->mesaFormat, lvl->width, rmesa->texture_compressed_row_align);
		lvl->size = get_compressed_image_size(mt->mesaFormat, lvl->rowstride, lvl->height);
	} else if (mt->target == GL_TEXTURE_RECTANGLE_NV) {
		row_align = rmesa->texture_rect_row_align - 1;
		lvl->rowstride = (_mesa_format_row_stride(mt->mesaFormat, lvl->width) + row_align) & ~row_align;
		lvl->size = lvl->rowstride * lvl->height;
	} else if (mt->tilebits & RADEON_TXO_MICRO_TILE) {
		/* tile pattern is 16 bytes x2. mipmaps stay 32 byte aligned,
		 * though the actual offset may be different (if texture is less than
		 * 32 bytes width) to the untiled case */
		lvl->rowstride = (_mesa_format_row_stride(mt->mesaFormat, lvl->width) * 2 + 31) & ~31;
		lvl->size = lvl->rowstride * ((lvl->height + 1) / 2) * lvl->depth;
	} else {
		row_align = rmesa->texture_row_align - 1;
		lvl->rowstride = (_mesa_format_row_stride(mt->mesaFormat, lvl->width) + row_align) & ~row_align;
		lvl->size = lvl->rowstride * lvl->height * lvl->depth;
	}
	assert(lvl->size > 0);

	/* All images are aligned to a 32-byte offset */
	*curOffset = (*curOffset + 0x1f) & ~0x1f;
	lvl->faces[face].offset = *curOffset;
	*curOffset += lvl->size;

	if (RADEON_DEBUG & RADEON_TEXTURE)
	  fprintf(stderr,
		  "level %d, face %d: rs:%d %dx%d at %d\n",
		  level, face, lvl->rowstride, lvl->width, lvl->height, lvl->faces[face].offset);
}

static GLuint minify(GLuint size, GLuint levels)
{
	size = size >> levels;
	if (size < 1)
		size = 1;
	return size;
}


static void calculate_miptree_layout_r100(radeonContextPtr rmesa, radeon_mipmap_tree *mt)
{
	GLuint curOffset, i, face, level;

	assert(mt->numLevels <= rmesa->glCtx->Const.MaxTextureLevels);

	curOffset = 0;
	for(face = 0; face < mt->faces; face++) {

		for(i = 0, level = mt->baseLevel; i < mt->numLevels; i++, level++) {
			mt->levels[level].valid = 1;
			mt->levels[level].width = minify(mt->width0, i);
			mt->levels[level].height = minify(mt->height0, i);
			mt->levels[level].depth = minify(mt->depth0, i);
			compute_tex_image_offset(rmesa, mt, face, level, &curOffset);
		}
	}

	/* Note the required size in memory */
	mt->totalsize = (curOffset + RADEON_OFFSET_MASK) & ~RADEON_OFFSET_MASK;
}

static void calculate_miptree_layout_r300(radeonContextPtr rmesa, radeon_mipmap_tree *mt)
{
	GLuint curOffset, i, level;

	assert(mt->numLevels <= rmesa->glCtx->Const.MaxTextureLevels);

	curOffset = 0;
	for(i = 0, level = mt->baseLevel; i < mt->numLevels; i++, level++) {
		GLuint face;

		mt->levels[level].valid = 1;
		mt->levels[level].width = minify(mt->width0, i);
		mt->levels[level].height = minify(mt->height0, i);
		mt->levels[level].depth = minify(mt->depth0, i);

		for(face = 0; face < mt->faces; face++)
			compute_tex_image_offset(rmesa, mt, face, level, &curOffset);
	}

	/* Note the required size in memory */
	mt->totalsize = (curOffset + RADEON_OFFSET_MASK) & ~RADEON_OFFSET_MASK;
}

/**
 * Create a new mipmap tree, calculate its layout and allocate memory.
 */
static radeon_mipmap_tree* radeon_miptree_create(radeonContextPtr rmesa,
		GLenum target, gl_format mesaFormat, GLuint baseLevel, GLuint numLevels,
		GLuint width0, GLuint height0, GLuint depth0, GLuint tilebits)
{
	radeon_mipmap_tree *mt = CALLOC_STRUCT(_radeon_mipmap_tree);

	mt->mesaFormat = mesaFormat;
	mt->refcount = 1;
	mt->target = target;
	mt->faces = (target == GL_TEXTURE_CUBE_MAP) ? 6 : 1;
	mt->baseLevel = baseLevel;
	mt->numLevels = numLevels;
	mt->width0 = width0;
	mt->height0 = height0;
	mt->depth0 = depth0;
	mt->tilebits = tilebits;

	if (rmesa->radeonScreen->chip_family >= CHIP_FAMILY_R300)
		calculate_miptree_layout_r300(rmesa, mt);
	else
		calculate_miptree_layout_r100(rmesa, mt);

	mt->bo = radeon_bo_open(rmesa->radeonScreen->bom,
                            0, mt->totalsize, 1024,
                            RADEON_GEM_DOMAIN_VRAM,
                            0);

	return mt;
}

void radeon_miptree_reference(radeon_mipmap_tree *mt, radeon_mipmap_tree **ptr)
{
	assert(!*ptr);

	mt->refcount++;
	assert(mt->refcount > 0);

	*ptr = mt;
}

void radeon_miptree_unreference(radeon_mipmap_tree **ptr)
{
	radeon_mipmap_tree *mt = *ptr;
	if (!mt)
		return;

	assert(mt->refcount > 0);

	mt->refcount--;
	if (!mt->refcount) {
		radeon_bo_unref(mt->bo);
		free(mt);
	}

	*ptr = 0;
}

/**
 * Calculate min and max LOD for the given texture object.
 * @param[in] tObj texture object whose LOD values to calculate
 * @param[out] pminLod minimal LOD
 * @param[out] pmaxLod maximal LOD
 */
static void calculate_min_max_lod(struct gl_texture_object *tObj,
				       unsigned *pminLod, unsigned *pmaxLod)
{
	int minLod, maxLod;
	/* Yes, this looks overly complicated, but it's all needed.
	*/
	switch (tObj->Target) {
	case GL_TEXTURE_1D:
	case GL_TEXTURE_2D:
	case GL_TEXTURE_3D:
	case GL_TEXTURE_CUBE_MAP:
		if (tObj->MinFilter == GL_NEAREST || tObj->MinFilter == GL_LINEAR) {
			/* GL_NEAREST and GL_LINEAR only care about GL_TEXTURE_BASE_LEVEL.
			*/
			minLod = maxLod = tObj->BaseLevel;
		} else {
			minLod = tObj->BaseLevel + (GLint)(tObj->MinLod);
			minLod = MAX2(minLod, tObj->BaseLevel);
			minLod = MIN2(minLod, tObj->MaxLevel);
			maxLod = tObj->BaseLevel + (GLint)(tObj->MaxLod + 0.5);
			maxLod = MIN2(maxLod, tObj->MaxLevel);
			maxLod = MIN2(maxLod, tObj->Image[0][minLod]->MaxLog2 + minLod);
			maxLod = MAX2(maxLod, minLod); /* need at least one level */
		}
		break;
	case GL_TEXTURE_RECTANGLE_NV:
	case GL_TEXTURE_4D_SGIS:
		minLod = maxLod = 0;
		break;
	default:
		return;
	}

	/* save these values */
	*pminLod = minLod;
	*pmaxLod = maxLod;
}

/**
 * Checks whether the given miptree can hold the given texture image at the
 * given face and level.
 */
GLboolean radeon_miptree_matches_image(radeon_mipmap_tree *mt,
		struct gl_texture_image *texImage, GLuint face, GLuint level)
{
	radeon_mipmap_level *lvl;

	if (face >= mt->faces)
		return GL_FALSE;

	if (texImage->TexFormat != mt->mesaFormat)
		return GL_FALSE;

	lvl = &mt->levels[level];
	if (!lvl->valid ||
	    lvl->width != texImage->Width ||
	    lvl->height != texImage->Height ||
	    lvl->depth != texImage->Depth)
		return GL_FALSE;

	return GL_TRUE;
}

/**
 * Checks whether the given miptree has the right format to store the given texture object.
 */
static GLboolean radeon_miptree_matches_texture(radeon_mipmap_tree *mt, struct gl_texture_object *texObj)
{
	struct gl_texture_image *firstImage;
	unsigned numLevels;
	radeon_mipmap_level *mtBaseLevel;

	if (texObj->BaseLevel < mt->baseLevel)
		return GL_FALSE;

	mtBaseLevel = &mt->levels[texObj->BaseLevel - mt->baseLevel];
	firstImage = texObj->Image[0][texObj->BaseLevel];
	numLevels = MIN2(texObj->MaxLevel - texObj->BaseLevel + 1, firstImage->MaxLog2 + 1);

	if (RADEON_DEBUG & RADEON_TEXTURE) {
		fprintf(stderr, "Checking if miptree %p matches texObj %p\n", mt, texObj);
		fprintf(stderr, "target %d vs %d\n", mt->target, texObj->Target);
		fprintf(stderr, "format %d vs %d\n", mt->mesaFormat, firstImage->TexFormat);
		fprintf(stderr, "numLevels %d vs %d\n", mt->numLevels, numLevels);
		fprintf(stderr, "width0 %d vs %d\n", mtBaseLevel->width, firstImage->Width);
		fprintf(stderr, "height0 %d vs %d\n", mtBaseLevel->height, firstImage->Height);
		fprintf(stderr, "depth0 %d vs %d\n", mtBaseLevel->depth, firstImage->Depth);
		if (mt->target == texObj->Target &&
	        mt->mesaFormat == firstImage->TexFormat &&
	        mt->numLevels >= numLevels &&
	        mtBaseLevel->width == firstImage->Width &&
	        mtBaseLevel->height == firstImage->Height &&
	        mtBaseLevel->depth == firstImage->Depth) {
			fprintf(stderr, "MATCHED\n");
		} else {
			fprintf(stderr, "NOT MATCHED\n");
		}
	}

	return (mt->target == texObj->Target &&
	        mt->mesaFormat == firstImage->TexFormat &&
	        mt->numLevels >= numLevels &&
	        mtBaseLevel->width == firstImage->Width &&
	        mtBaseLevel->height == firstImage->Height &&
	        mtBaseLevel->depth == firstImage->Depth);
}

/**
 * Try to allocate a mipmap tree for the given texture object.
 * @param[in] rmesa radeon context
 * @param[in] t radeon texture object
 */
void radeon_try_alloc_miptree(radeonContextPtr rmesa, radeonTexObj *t)
{
	struct gl_texture_object *texObj = &t->base;
	struct gl_texture_image *texImg = texObj->Image[0][texObj->BaseLevel];
	GLuint numLevels;

	assert(!t->mt);

	if (!texImg)
		return;

	numLevels = MIN2(texObj->MaxLevel - texObj->BaseLevel + 1, texImg->MaxLog2 + 1);

	t->mt = radeon_miptree_create(rmesa, t->base.Target,
		texImg->TexFormat, texObj->BaseLevel,
		numLevels, texImg->Width, texImg->Height,
		texImg->Depth, t->tile_bits);
}

/* Although we use the image_offset[] array to store relative offsets
 * to cube faces, Mesa doesn't know anything about this and expects
 * each cube face to be treated as a separate image.
 *
 * These functions present that view to mesa:
 */
void
radeon_miptree_depth_offsets(radeon_mipmap_tree *mt, GLuint level, GLuint *offsets)
{
	if (mt->target != GL_TEXTURE_3D || mt->faces == 1) {
		offsets[0] = 0;
	} else {
		int i;
		for (i = 0; i < 6; i++) {
			offsets[i] = mt->levels[level].faces[i].offset;
		}
	}
}

GLuint
radeon_miptree_image_offset(radeon_mipmap_tree *mt,
			    GLuint face, GLuint level)
{
	if (mt->target == GL_TEXTURE_CUBE_MAP_ARB)
		return (mt->levels[level].faces[face].offset);
	else
		return mt->levels[level].faces[0].offset;
}

/**
 * Ensure that the given image is stored in the given miptree from now on.
 */
static void migrate_image_to_miptree(radeon_mipmap_tree *mt,
									 radeon_texture_image *image,
									 int face, int level)
{
	radeon_mipmap_level *dstlvl = &mt->levels[level];
	unsigned char *dest;

	assert(image->mt != mt);
	assert(dstlvl->valid);
	assert(dstlvl->width == image->base.Width);
	assert(dstlvl->height == image->base.Height);
	assert(dstlvl->depth == image->base.Depth);

	radeon_bo_map(mt->bo, GL_TRUE);
	dest = mt->bo->ptr + dstlvl->faces[face].offset;

	if (image->mt) {
		/* Format etc. should match, so we really just need a memcpy().
		 * In fact, that memcpy() could be done by the hardware in many
		 * cases, provided that we have a proper memory manager.
		 */
		assert(mt->mesaFormat == image->base.TexFormat);

		radeon_mipmap_level *srclvl = &image->mt->levels[image->mtlevel];

		/* TODO: bring back these assertions once the FBOs are fixed */
#if 0
		assert(image->mtlevel == level);
		assert(srclvl->size == dstlvl->size);
		assert(srclvl->rowstride == dstlvl->rowstride);
#endif

		radeon_bo_map(image->mt->bo, GL_FALSE);

		memcpy(dest,
			image->mt->bo->ptr + srclvl->faces[face].offset,
			dstlvl->size);
		radeon_bo_unmap(image->mt->bo);

		radeon_miptree_unreference(&image->mt);
	} else if (image->base.Data) {
		/* This condition should be removed, it's here to workaround
		 * a segfault when mapping textures during software fallbacks.
		 */
		const uint32_t srcrowstride = _mesa_format_row_stride(image->base.TexFormat, image->base.Width);
		uint32_t rows = image->base.Height * image->base.Depth;

		if (_mesa_is_format_compressed(image->base.TexFormat)) {
			uint32_t blockWidth, blockHeight;
			_mesa_get_format_block_size(image->base.TexFormat, &blockWidth, &blockHeight);
			rows = (rows + blockHeight - 1) / blockHeight;
		}

		copy_rows(dest, dstlvl->rowstride, image->base.Data, srcrowstride,
				  rows, srcrowstride);

		_mesa_free_texmemory(image->base.Data);
		image->base.Data = 0;
	}

	radeon_bo_unmap(mt->bo);

	radeon_miptree_reference(mt, &image->mt);
	image->mtface = face;
	image->mtlevel = level;
}

/**
 * Filter matching miptrees, and select one with the most of data.
 * @param[in] texObj radeon texture object
 * @param[in] firstLevel first texture level to check
 * @param[in] lastLevel last texture level to check
 */
static radeon_mipmap_tree * get_biggest_matching_miptree(radeonTexObj *texObj,
														 unsigned firstLevel,
														 unsigned lastLevel)
{
	const unsigned numLevels = lastLevel - firstLevel + 1;
	unsigned *mtSizes = calloc(numLevels, sizeof(unsigned));
	radeon_mipmap_tree **mts = calloc(numLevels, sizeof(radeon_mipmap_tree *));
	unsigned mtCount = 0;
	unsigned maxMtIndex = 0;
	radeon_mipmap_tree *tmp;

	for (unsigned level = firstLevel; level <= lastLevel; ++level) {
		radeon_texture_image *img = get_radeon_texture_image(texObj->base.Image[0][level]);
		unsigned found = 0;
		// TODO: why this hack??
		if (!img)
			break;

		if (!img->mt)
			continue;

		for (int i = 0; i < mtCount; ++i) {
			if (mts[i] == img->mt) {
				found = 1;
				mtSizes[i] += img->mt->levels[img->mtlevel].size;
				break;
			}
		}

		if (!found && radeon_miptree_matches_texture(img->mt, &texObj->base)) {
			mtSizes[mtCount] = img->mt->levels[img->mtlevel].size;
			mts[mtCount] = img->mt;
			mtCount++;
		}
	}

	if (mtCount == 0) {
		return NULL;
	}

	for (int i = 1; i < mtCount; ++i) {
		if (mtSizes[i] > mtSizes[maxMtIndex]) {
			maxMtIndex = i;
		}
	}

	tmp = mts[maxMtIndex];
	free(mtSizes);
	free(mts);

	return tmp;
}

/**
 * Validate texture mipmap tree.
 * If individual images are stored in different mipmap trees
 * use the mipmap tree that has the most of the correct data.
 */
int radeon_validate_texture_miptree(GLcontext * ctx, struct gl_texture_object *texObj)
{
	radeonContextPtr rmesa = RADEON_CONTEXT(ctx);
	radeonTexObj *t = radeon_tex_obj(texObj);

	if (t->validated || t->image_override) {
		return GL_TRUE;
	}

	if (texObj->Image[0][texObj->BaseLevel]->Border > 0)
		return GL_FALSE;

	_mesa_test_texobj_completeness(rmesa->glCtx, texObj);
	if (!texObj->_Complete) {
		return GL_FALSE;
	}

	calculate_min_max_lod(&t->base, &t->minLod, &t->maxLod);

	if (RADEON_DEBUG & RADEON_TEXTURE)
		fprintf(stderr, "%s: Validating texture %p now, minLod = %d, maxLod = %d\n",
				__FUNCTION__, texObj ,t->minLod, t->maxLod);

	radeon_mipmap_tree *dst_miptree;
	dst_miptree = get_biggest_matching_miptree(t, t->minLod, t->maxLod);

	if (!dst_miptree) {
		radeon_miptree_unreference(&t->mt);
		radeon_try_alloc_miptree(rmesa, t);
		dst_miptree = t->mt;
		if (RADEON_DEBUG & RADEON_TEXTURE) {
			fprintf(stderr, "%s: No matching miptree found, allocated new one %p\n", __FUNCTION__, t->mt);
		}
	} else if (RADEON_DEBUG & RADEON_TEXTURE) {
		fprintf(stderr, "%s: Using miptree %p\n", __FUNCTION__, t->mt);
	}

	const unsigned faces = texObj->Target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
	unsigned face, level;
	radeon_texture_image *img;
	/* Validate only the levels that will actually be used during rendering */
	for (face = 0; face < faces; ++face) {
		for (level = t->minLod; level <= t->maxLod; ++level) {
			img = get_radeon_texture_image(texObj->Image[face][level]);

			if (RADEON_DEBUG & RADEON_TEXTURE) {
				fprintf(stderr, "Checking image level %d, face %d, mt %p ... ", level, face, img->mt);
			}
			
			if (img->mt != dst_miptree) {
				if (RADEON_DEBUG & RADEON_TEXTURE) {
					fprintf(stderr, "MIGRATING\n");
				}
				struct radeon_bo *src_bo = (img->mt) ? img->mt->bo : img->bo;
				if (src_bo && radeon_bo_is_referenced_by_cs(src_bo, rmesa->cmdbuf.cs)) {
					radeon_firevertices(rmesa);
				}
				migrate_image_to_miptree(dst_miptree, img, face, level);
			} else if (RADEON_DEBUG & RADEON_TEXTURE) {
				fprintf(stderr, "OK\n");
			}
		}
	}

	t->validated = GL_TRUE;

	return GL_TRUE;
}

uint32_t get_base_teximage_offset(radeonTexObj *texObj)
{
	if (!texObj->mt) {
		return 0;
	} else {
		return radeon_miptree_image_offset(texObj->mt, 0, texObj->minLod);
	}
}