/**
 * @file r_model_brush.c
 * @brief brush model loading
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"
#include "r_lightmap.h"
#include "../../shared/parse.h"

/*
===============================================================================
BRUSHMODEL LOADING
===============================================================================
*/

/**
 * @brief The model base pointer - bases for the lump offsets
 */
static const byte *mod_base;
/**
 * @brief The shift array is used for random map assemblies (RMA) to shift
 * the mins/maxs and stuff like that
 */
static ipos3_t shift;
/**
 * @brief The currently loaded world model for the actual tile
 * @sa r_mapTiles
 */
static model_t *r_worldmodel;

#define MIN_AMBIENT_COMPONENT 0.05
#define MIN_AMBIENT_SUM 1.25

/**
 * @brief Load the lightmap data
 */
static void R_ModLoadLighting (const lump_t *l, qboolean day)
{
	const char *s;
	const char *ambientLightString = day ? "\"ambient_day\"" : "\"ambient_night\"";

	if (!l->filelen) {
		r_worldmodel->bsp.lightdata = NULL;
		r_worldmodel->bsp.lightquant = 4;
		return;
	}

	/* resolve ambient light */
	if ((s = strstr(CM_EntityString(), ambientLightString))) {
		int i;
		const char *c;

		c = COM_Parse(&s);  /* parse the string itself */
		c = COM_Parse(&s);  /* and then the value */

		if (sscanf(c, "%f %f %f", &refdef.ambient_light[0],
			&refdef.ambient_light[1], &refdef.ambient_light[2]) != 3)
			Com_Printf("Invalid light vector given: '%s'\n", c);

		for (i = 0; i < 3; i++) {  /* clamp it */
			if (refdef.ambient_light[i] < MIN_AMBIENT_COMPONENT)
				refdef.ambient_light[i] = MIN_AMBIENT_COMPONENT;
		}

		Com_DPrintf(DEBUG_RENDERER, "Resolved %s: %1.2f %1.2f %1.2f\n",
			ambientLightString, refdef.ambient_light[0], refdef.ambient_light[1], refdef.ambient_light[2]);
 	} else  /* ensure sane default */
		VectorSet(refdef.ambient_light, MIN_AMBIENT_COMPONENT, MIN_AMBIENT_COMPONENT, MIN_AMBIENT_COMPONENT);

	/* scale it into a reasonable range, the clamp above ensures this will work */
	while (VectorSum(refdef.ambient_light) < MIN_AMBIENT_SUM)
		VectorScale(refdef.ambient_light, 1.25, refdef.ambient_light);

	r_worldmodel->bsp.lightdata = Mem_PoolAlloc(l->filelen, vid_lightPool, 0);
	r_worldmodel->bsp.lightquant = *(const byte *) (mod_base + l->fileofs);
	memcpy(r_worldmodel->bsp.lightdata, mod_base + l->fileofs, l->filelen);
}

static void R_ModLoadVertexes (const lump_t *l)
{
	const dBspVertex_t *in;
	mBspVertex_t *out;
	int i, count;

	in = (const void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadVertexes: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...verts: %i\n", count);

	r_worldmodel->bsp.vertexes = out;
	r_worldmodel->bsp.numvertexes = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->position[0] = LittleFloat(in->point[0]);
		out->position[1] = LittleFloat(in->point[1]);
		out->position[2] = LittleFloat(in->point[2]);
	}
}

static void R_ModLoadNormals (const lump_t *l)
{
	const dBspNormal_t *in;
	mBspVertex_t *out;
	int i, count;

	in = (const void *)(mod_base + l->fileofs);
	if (l->filelen % sizeof(*in)) {
		Com_Error(ERR_DROP, "R_LoadNormals: Funny lump size in %s.", r_worldmodel->name);
	}
	count = l->filelen / sizeof(*in);

	if (count != r_worldmodel->bsp.numvertexes) {  /* ensure sane normals count */
		Com_Error(ERR_DROP, "R_LoadNormals: unexpected normals count in %s: (%d != %d).",
				r_worldmodel->name, count, r_worldmodel->bsp.numvertexes);
	}

	out = r_worldmodel->bsp.vertexes;

	for (i = 0; i < count; i++, in++, out++) {
		out->normal[0] = LittleFloat(in->normal[0]);
		out->normal[1] = LittleFloat(in->normal[1]);
		out->normal[2] = LittleFloat(in->normal[2]);
	}
}

static inline float R_RadiusFromBounds (const vec3_t mins, const vec3_t maxs)
{
	int i;
	vec3_t corner;

	for (i = 0; i < 3; i++)
		corner[i] = fabsf(mins[i]) > fabsf(maxs[i]) ? fabsf(mins[i]) : fabsf(maxs[i]);

	return VectorLength(corner);
}


/**
 * @brief Loads brush entities like func_door and func_breakable
 * @sa CMod_LoadSubmodels
 */
static void R_ModLoadSubmodels (const lump_t *l)
{
	const dBspModel_t *in;
	mBspHeader_t *out;
	int i, j, count;

	in = (const void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadSubmodels: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...submodels: %i\n", count);

	r_worldmodel->bsp.submodels = out;
	r_worldmodel->bsp.numsubmodels = count;

	for (i = 0; i < count; i++, in++, out++) {
		/* spread the mins / maxs by a pixel */
		for (j = 0; j < 3; j++) {
			out->mins[j] = LittleFloat(in->mins[j]) - 1.0f + (float)shift[j];
			out->maxs[j] = LittleFloat(in->maxs[j]) + 1.0f + (float)shift[j];
			out->origin[j] = LittleFloat(in->origin[j]) + (float)shift[j];
		}
		out->radius = R_RadiusFromBounds(out->mins, out->maxs);
		out->headnode = LittleLong(in->headnode);
		out->firstface = LittleLong(in->firstface);
		out->numfaces = LittleLong(in->numfaces);
	}
}

static void R_ModLoadEdges (const lump_t *l)
{
	const dBspEdge_t *in;
	mBspEdge_t *out;
	int i, count;

	in = (const void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadEdges: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_PoolAlloc((count + 1) * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...edges: %i\n", count);

	r_worldmodel->bsp.edges = out;
	r_worldmodel->bsp.numedges = count;

	for (i = 0; i < count; i++, in++, out++) {
		out->v[0] = (unsigned short) LittleShort(in->v[0]);
		out->v[1] = (unsigned short) LittleShort(in->v[1]);
	}
}

/**
 * @sa CP_StartMissionMap
 */
static void R_ModLoadTexinfo (const lump_t *l)
{
	const dBspTexinfo_t *in;
	mBspTexInfo_t *out;
	int i, j, count;
	char name[MAX_QPATH];

	in = (const void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadTexinfo: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...texinfo: %i\n", count);

	r_worldmodel->bsp.texinfo = out;
	r_worldmodel->bsp.numtexinfo = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 4; j++) {
			out->vecs[0][j] = LittleFloat(in->vecs[0][j]);
			out->vecs[1][j] = LittleFloat(in->vecs[1][j]);
		}

		out->flags = LittleLong(in->surfaceFlags);

		/* exchange the textures with the ones that are needed for base assembly */
		if (refdef.mapZone && strstr(in->texture, "tex_terrain/dummy"))
			Com_sprintf(name, sizeof(name), "textures/tex_terrain/%s", refdef.mapZone);
		else
			Com_sprintf(name, sizeof(name), "textures/%s", in->texture);

		out->image = R_FindImage(name, it_world);
	}
}

/**
 * @brief Fills in s->stmins[] and s->stmaxs[]
 */
static void R_SetSurfaceExtents (mBspSurface_t *surf, const model_t* mod)
{
	vec3_t mins, maxs;
	vec2_t stmins, stmaxs;
	int i, j;
	const mBspTexInfo_t *tex;

	VectorSet(mins, 999999, 999999, 999999);
	VectorSet(maxs, -999999, -999999, -999999);

	Vector2Set(stmins, 999999, 999999);
	Vector2Set(stmaxs, -999999, -999999);

	tex = surf->texinfo;

	for (i = 0; i < surf->numedges; i++) {
		const int e = mod->bsp.surfedges[surf->firstedge + i];
		const mBspVertex_t *v;
		if (e >= 0)
			v = &mod->bsp.vertexes[mod->bsp.edges[e].v[0]];
		else
			v = &mod->bsp.vertexes[mod->bsp.edges[-e].v[1]];

		for (j = 0; j < 3; j++) {  /* calculate mins, maxs */
			if (v->position[j] > maxs[j])
				maxs[j] = v->position[j];
			if (v->position[j] < mins[j])
				mins[j] = v->position[j];
		}

		for (j = 0; j < 2; j++) {  /* calculate stmins, stmaxs */
			const float val = DotProduct(v->position, tex->vecs[j]) + tex->vecs[j][3];
			if (val < stmins[j])
				stmins[j] = val;
			if (val > stmaxs[j])
				stmaxs[j] = val;
		}
	}

	VectorCopy(mins, surf->mins);
	VectorCopy(maxs, surf->maxs);

	for (i = 0; i < 3; i++)
		surf->center[i] = (mins[i] + maxs[i]) / 2.0;

	for (i = 0; i < 2; i++) {
		const int bmins = floor(stmins[i] / surf->lightmap_scale);
		const int bmaxs = ceil(stmaxs[i] / surf->lightmap_scale);

		surf->stmins[i] = bmins * surf->lightmap_scale;
		surf->stmaxs[i] = bmaxs * surf->lightmap_scale;

		surf->stcenter[i] = (surf->stmaxs[i] + surf->stmins[i]) / 2.0;
		surf->stextents[i] = (bmaxs - bmins) * surf->lightmap_scale;
	}
}

static void R_ModLoadSurfaces (qboolean day, const lump_t *l)
{
	const dBspFace_t *in;
	mBspSurface_t *out;
	int i, count, surfnum;
	int planenum, side;
	int ti;

	in = (const void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadSurfaces: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...faces: %i\n", count);

	r_worldmodel->bsp.surfaces = out;
	r_worldmodel->bsp.numsurfaces = count;

	for (surfnum = 0; surfnum < count; surfnum++, in++, out++) {
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);

		/* resolve plane */
		planenum = LittleShort(in->planenum);
		assert(planenum >= 0);
		out->plane = r_worldmodel->bsp.planes + planenum;

		/* and sideness */
		side = LittleShort(in->side);
		if (side) {
			out->flags |= MSURF_PLANEBACK;
			VectorNegate(out->plane->normal, out->normal);
		} else {
			VectorCopy(out->plane->normal, out->normal);
		}

		ti = LittleShort(in->texinfo);
		if (ti < 0 || ti >= r_worldmodel->bsp.numtexinfo)
			Com_Error(ERR_DROP, "R_ModLoadSurfaces: bad texinfo number");
		out->texinfo = r_worldmodel->bsp.texinfo + ti;

		out->lightmap_scale = (1 << r_worldmodel->bsp.lightquant);

		/* and size, texcoords, etc */
		R_SetSurfaceExtents(out, r_worldmodel);

		/* lastly lighting info */
		if (day)
			i = LittleLong(in->lightofs[LIGHTMAP_DAY]);
		else
			i = LittleLong(in->lightofs[LIGHTMAP_NIGHT]);

		if (i == -1 || (out->texinfo->flags & SURF_WARP))
			out->samples = NULL;
		else {
			out->samples = r_worldmodel->bsp.lightdata + i;
			out->flags |= MSURF_LIGHTMAP;
		}

		/* create lightmaps */
		R_CreateSurfaceLightmap(out);

		out->tile = r_numMapTiles - 1;
	}
}

/**
 * @sa TR_BuildTracingNode_r
 * @sa R_RecurseSetParent
 */
static void R_ModLoadNodes (const lump_t *l)
{
	int i, j, count, p;
	const dBspNode_t *in;
	mBspNode_t *out;
	mBspNode_t *parent = NULL;

	in = (const void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadNodes: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...nodes: %i\n", count);

	r_worldmodel->bsp.nodes = out;
	r_worldmodel->bsp.numnodes = count;

	for (i = 0; i < count; i++, in++, out++) {
		p = LittleLong(in->planenum);

		/* skip special pathfinding nodes - they have a negative index */
		if (p == PLANENUM_LEAF) {
			/* in case of "special" pathfinding nodes (they don't have a plane)
			 * we have to set this to NULL */
			out->plane = NULL;
			out->contents = CONTENTS_PATHFINDING_NODE;
			parent = NULL;
		} else {
			out->plane = r_worldmodel->bsp.planes + p;
			/* differentiate from leafs */
			out->contents = CONTENTS_NODE;
			parent = out;
		}

		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + (float)shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + (float)shift[j];
		}

		out->firstsurface = LittleShort(in->firstface);
		out->numsurfaces = LittleShort(in->numfaces);

		for (j = 0; j < 2; j++) {
			p = LittleLong(in->children[j]);
			if (p > LEAFNODE) {
				assert(p < r_worldmodel->bsp.numnodes);
				out->children[j] = r_worldmodel->bsp.nodes + p;
			} else {
				assert((LEAFNODE - p) < r_worldmodel->bsp.numleafs);
				out->children[j] = (mBspNode_t *) (r_worldmodel->bsp.leafs + (LEAFNODE - p));
			}
			out->children[j]->parent = parent;
		}
	}
}

static void R_ModLoadLeafs (const lump_t *l)
{
	const dBspLeaf_t *in;
	mBspLeaf_t *out;
	int i, j, count;

	in = (const void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadLeafs: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...leafs: %i\n", count);

	r_worldmodel->bsp.leafs = out;
	r_worldmodel->bsp.numleafs = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++) {
			out->minmaxs[j] = LittleShort(in->mins[j]) + (float)shift[j];
			out->minmaxs[3 + j] = LittleShort(in->maxs[j]) + (float)shift[j];
		}

		out->contents = LittleLong(in->contentFlags);
	}
}

static void R_ModLoadSurfedges (const lump_t *l)
{
	int i, count;
	const int *in;
	int *out;

	in = (const void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadSurfedges: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	if (count < 1 || count >= MAX_MAP_SURFEDGES)
		Com_Error(ERR_DROP, "R_ModLoadSurfedges: bad surfedges count in %s: %i", r_worldmodel->name, count);

	out = Mem_PoolAlloc(count * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...surface edges: %i\n", count);

	r_worldmodel->bsp.surfedges = out;
	r_worldmodel->bsp.numsurfedges = count;

	for (i = 0; i < count; i++)
		out[i] = LittleLong(in[i]);
}

/**
 * @sa CMod_LoadPlanes
 */
static void R_ModLoadPlanes (const lump_t *l)
{
	int i, j;
	cBspPlane_t *out;
	const dBspPlane_t *in;
	int count;

	in = (const void *) (mod_base + l->fileofs);
	if (l->filelen % sizeof(*in))
		Com_Error(ERR_DROP, "R_ModLoadPlanes: funny lump size in %s", r_worldmodel->name);
	count = l->filelen / sizeof(*in);
	out = Mem_PoolAlloc(count * 2 * sizeof(*out), vid_modelPool, 0);
	Com_DPrintf(DEBUG_RENDERER, "...planes: %i\n", count);

	r_worldmodel->bsp.planes = out;
	r_worldmodel->bsp.numplanes = count;

	for (i = 0; i < count; i++, in++, out++) {
		for (j = 0; j < 3; j++)
			out->normal[j] = LittleFloat(in->normal[j]);
		out->dist = LittleFloat(in->dist);
		out->type = LittleLong(in->type);
	}
}

/**
 * @brief Shift the verts for map assemblies
 * @note This is needed because you want to place a bsp file to a given
 * position on the grid - see R_ModAddMapTile for the shift vector calculation
 * This vector differs for every map - and depends on the grid position the bsp
 * map tile is placed in the world
 * @note Call this after the buffers were generated in R_LoadBspVertexArrays
 * @sa R_LoadBspVertexArrays
 */
static void R_ModShiftTile (void)
{
	mBspVertex_t *vert;
	cBspPlane_t *plane;
	int i, j;

	/* we can't do this instantly, because of rounding errors on extents calculation */
	/* shift vertexes */
	for (i = 0, vert = r_worldmodel->bsp.vertexes; i < r_worldmodel->bsp.numvertexes; i++, vert++)
		for (j = 0; j < 3; j++)
			vert->position[j] += shift[j];

	/* shift planes */
	for (i = 0, plane = r_worldmodel->bsp.planes; i < r_worldmodel->bsp.numplanes; i++, plane++)
		for (j = 0; j < 3; j++)
			plane->dist += plane->normal[j] * shift[j];
}

/**
 * @brief Puts the map data into buffers
 * @sa R_ModAddMapTile
 * @note Shift the verts after the texcoords for diffuse and lightmap are loaded
 * @sa R_ModShiftTile
 * @todo Don't use the buffers from r_state here - they might overflow
 * @todo Decrease MAX_GL_ARRAY_LENGTH to 32768 again when this is fixed
 */
static void R_LoadBspVertexArrays (model_t *mod)
{
	int i, j;
	int vertind, coordind, tangind;
	float *vecShifted;
	float soff, toff, s, t;
	float *point, *sdir, *tdir;
	vec4_t tangent;
	vec3_t binormal;
	mBspSurface_t *surf;
	mBspVertex_t *vert;
	int vertexcount;

	vertind = coordind = tangind = vertexcount = 0;

	for (i = 0, surf = mod->bsp.surfaces; i < mod->bsp.numsurfaces; i++, surf++)
		for (j = 0; j < surf->numedges; j++)
			vertexcount++;

	surf = mod->bsp.surfaces;

	/* allocate the vertex arrays */
	mod->bsp.texcoords = (GLfloat *)Mem_PoolAlloc(vertexcount * 2 * sizeof(GLfloat), vid_modelPool, 0);
	mod->bsp.lmtexcoords = (GLfloat *)Mem_PoolAlloc(vertexcount * 2 * sizeof(GLfloat), vid_modelPool, 0);
	mod->bsp.verts = (GLfloat *)Mem_PoolAlloc(vertexcount * 3 * sizeof(GLfloat), vid_modelPool, 0);
	mod->bsp.normals = (GLfloat *)Mem_PoolAlloc(vertexcount * 3 * sizeof(GLfloat), vid_modelPool, 0);
	mod->bsp.tangents = (GLfloat *)Mem_PoolAlloc(vertexcount * 4 * sizeof(GLfloat), vid_modelPool, 0);

	for (i = 0; i < mod->bsp.numsurfaces; i++, surf++) {
		surf->index = vertind / 3;

		for (j = 0; j < surf->numedges; j++) {
			const float *normal;
			const int index = mod->bsp.surfedges[surf->firstedge + j];

			if (vertind >= MAX_GL_ARRAY_LENGTH * 3)
				Com_Error(ERR_DROP, "R_LoadBspVertexArrays: Exceeded MAX_GL_ARRAY_LENGTH %i", vertind);

			/* vertex */
			if (index > 0) {  /* negative indices to differentiate which end of the edge */
				const mBspEdge_t *edge = &mod->bsp.edges[index];
				vert = &mod->bsp.vertexes[edge->v[0]];
			} else {
				const mBspEdge_t *edge = &mod->bsp.edges[-index];
				vert = &mod->bsp.vertexes[edge->v[1]];
			}

			point = vert->position;

			/* shift it for assembled maps */
			vecShifted = &mod->bsp.verts[vertind];
			VectorAdd(point, shift, vecShifted);

			/* texture directional vectors and offsets */
			sdir = surf->texinfo->vecs[0];
			soff = surf->texinfo->vecs[0][3];

			tdir = surf->texinfo->vecs[1];
			toff = surf->texinfo->vecs[1][3];

			/* texture coordinates */
			s = DotProduct(point, sdir) + soff;
			s /= surf->texinfo->image->width;

			t = DotProduct(point, tdir) + toff;
			t /= surf->texinfo->image->height;

			mod->bsp.texcoords[coordind + 0] = s;
			mod->bsp.texcoords[coordind + 1] = t;

			if (surf->flags & MSURF_LIGHTMAP) {  /* lightmap coordinates */
				s = DotProduct(point, sdir) + soff;
				s -= surf->stmins[0];
				s += surf->light_s * surf->lightmap_scale;
				s += surf->lightmap_scale / 2.0;
				s /= r_lightmaps.size * surf->lightmap_scale;

				t = DotProduct(point, tdir) + toff;
				t -= surf->stmins[1];
				t += surf->light_t * surf->lightmap_scale;
				t += surf->lightmap_scale / 2.0;
				t /= r_lightmaps.size * surf->lightmap_scale;
			}

			mod->bsp.lmtexcoords[coordind + 0] = s;
			mod->bsp.lmtexcoords[coordind + 1] = t;

			/* normal vectors */
			if (surf->texinfo->flags & SURF_PHONG &&
					!VectorCompare(vert->normal, vec3_origin))
				normal = vert->normal; /* phong shaded */
			else
				normal = surf->normal; /* per plane */

			memcpy(&mod->bsp.normals[vertind], normal, sizeof(vec3_t));

			/* tangent vector */
			TangentVectors(normal, sdir, tdir, tangent, binormal);
			memcpy(&mod->bsp.tangents[tangind], tangent, sizeof(vec4_t));

			vertind += 3;
			coordind += 2;
			tangind += 4;
		}
	}

	if (qglBindBuffer) {
		/* and also the vertex buffer objects */
		qglGenBuffers(1, &mod->bsp.vertex_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.vertex_buffer);
		qglBufferData(GL_ARRAY_BUFFER, vertind * sizeof(GLfloat), mod->bsp.verts, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.texcoord_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.texcoord_buffer);
		qglBufferData(GL_ARRAY_BUFFER, coordind * sizeof(GLfloat), mod->bsp.texcoords, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.lmtexcoord_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.lmtexcoord_buffer);
		qglBufferData(GL_ARRAY_BUFFER, coordind * sizeof(GLfloat), mod->bsp.lmtexcoords, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.normal_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.normal_buffer);
		qglBufferData(GL_ARRAY_BUFFER, vertind * sizeof(GLfloat), mod->bsp.normals, GL_STATIC_DRAW);

		qglGenBuffers(1, &mod->bsp.tangent_buffer);
		qglBindBuffer(GL_ARRAY_BUFFER, mod->bsp.tangent_buffer);
		qglBufferData(GL_ARRAY_BUFFER, tangind * sizeof(GLfloat), mod->bsp.tangents, GL_STATIC_DRAW);

		qglBindBuffer(GL_ARRAY_BUFFER, 0);
	}
}

/** @brief temporary space for sorting surfaces by texture index */
static mBspSurfaces_t *r_sorted_surfaces[MAX_GL_TEXTURES];

static void R_SortSurfacesArrays_ (mBspSurfaces_t *surfs)
{
	int i, j;

	for (i = 0; i < surfs->count; i++) {
		const int texnum = surfs->surfaces[i]->texinfo->image->texnum;
		R_SurfaceToSurfaces(r_sorted_surfaces[texnum], surfs->surfaces[i]);
	}

	surfs->count = 0;

	for (i = 0; i < r_numImages; i++) {
		mBspSurfaces_t *sorted = r_sorted_surfaces[r_images[i].texnum];
		if (sorted && sorted->count) {
			for (j = 0; j < sorted->count; j++)
				R_SurfaceToSurfaces(surfs, sorted->surfaces[j]);

			sorted->count = 0;
		}
	}
}

/**
 * @brief Reorders all surfaces arrays for the specified model, grouping the surface
 * pointers by texture.  This dramatically reduces glBindTexture calls.
 */
static void R_SortSurfacesArrays (const model_t *mod)
{
	const mBspSurface_t *surf, *s;
	int i, ns;

	/* resolve the start surface and total surface count */
	if (mod->type == mod_bsp) {  /*  world model */
		s = mod->bsp.surfaces;
		ns = mod->bsp.numsurfaces;
	} else {  /* submodels */
		s = &mod->bsp.surfaces[mod->bsp.firstmodelsurface];
		ns = mod->bsp.nummodelsurfaces;
	}

	memset(r_sorted_surfaces, 0, sizeof(r_sorted_surfaces));

	/* allocate the per-texture surfaces arrays and determine counts */
	for (i = 0, surf = s; i < ns; i++, surf++) {
		mBspSurfaces_t *surfs = r_sorted_surfaces[surf->texinfo->image->texnum];
		if (!surfs) {  /* allocate it */
			surfs = (mBspSurfaces_t *)Mem_PoolAlloc(sizeof(*surfs), vid_modelPool, 0);
			r_sorted_surfaces[surf->texinfo->image->texnum] = surfs;
		}

		surfs->count++;
	}

	/* allocate the surfaces pointers based on counts */
	for (i = 0; i < r_numImages; i++) {
		mBspSurfaces_t *surfs = r_sorted_surfaces[r_images[i].texnum];
		if (surfs) {
			surfs->surfaces = (mBspSurface_t **)Mem_PoolAlloc(sizeof(mBspSurface_t *) * surfs->count, vid_modelPool, 0);
			surfs->count = 0;
		}
	}

	/* sort the model's surfaces arrays into the per-texture arrays */
	for (i = 0; i < NUM_SURFACES_ARRAYS; i++) {
		if (mod->bsp.sorted_surfaces[i]->count) {
			R_SortSurfacesArrays_(mod->bsp.sorted_surfaces[i]);
			Com_DPrintf(DEBUG_RENDERER, "%i: #%i surfaces\n", i, mod->bsp.sorted_surfaces[i]->count);
		}
	}

	/* free the per-texture surfaces arrays */
	for (i = 0; i < r_numImages; i++) {
		mBspSurfaces_t *surfs = r_sorted_surfaces[r_images[i].texnum];
		if (surfs) {
			if (surfs->surfaces)
				Mem_Free(surfs->surfaces);
			Mem_Free(surfs);
		}
	}
}

static void R_LoadSurfacesArrays_ (model_t *mod)
{
	mBspSurface_t *surf, *s;
	int i, ns;

	/* allocate the surfaces array structures */
	for (i = 0; i < NUM_SURFACES_ARRAYS; i++)
		mod->bsp.sorted_surfaces[i] = (mBspSurfaces_t *)Mem_PoolAlloc(sizeof(mBspSurfaces_t), vid_modelPool, 0);

	/* resolve the start surface and total surface count */
	if (mod->type == mod_bsp) {  /* world model */
		s = mod->bsp.surfaces;
		ns = mod->bsp.numsurfaces;
	} else {  /* submodels */
		s = &mod->bsp.surfaces[mod->bsp.firstmodelsurface];
		ns = mod->bsp.nummodelsurfaces;
	}

	/* determine the maximum counts for each rendered type in order to
	 * allocate only what is necessary for the specified model */
	for (i = 0, surf = s; i < ns; i++, surf++) {
		if (surf->texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)) {
			if (surf->texinfo->flags & SURF_WARP)
				mod->bsp.blend_warp_surfaces->count++;
			else
				mod->bsp.blend_surfaces->count++;
		} else {
			if (surf->texinfo->flags & SURF_WARP)
				mod->bsp.opaque_warp_surfaces->count++;
			else if (surf->texinfo->flags & SURF_ALPHATEST)
				mod->bsp.alpha_test_surfaces->count++;
			else
				mod->bsp.opaque_surfaces->count++;
		}

		if (surf->texinfo->image->material.flags & STAGE_RENDER)
			mod->bsp.material_surfaces->count++;
	}

	/* allocate the surfaces pointers based on the counts */
	for (i = 0; i < NUM_SURFACES_ARRAYS; i++) {
		if (mod->bsp.sorted_surfaces[i]->count) {
			mod->bsp.sorted_surfaces[i]->surfaces = (mBspSurface_t **)Mem_PoolAlloc(
					sizeof(mBspSurface_t *) * mod->bsp.sorted_surfaces[i]->count, vid_modelPool, 0);

			mod->bsp.sorted_surfaces[i]->count = 0;
		}
	}

	/* iterate the surfaces again, populating the allocated arrays based
	 * on primary render type */
	for (i = 0, surf = s; i < ns; i++, surf++) {
		if (surf->texinfo->flags & (SURF_BLEND33 | SURF_BLEND66)) {
			if (surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(mod->bsp.blend_warp_surfaces, surf);
			else
				R_SurfaceToSurfaces(mod->bsp.blend_surfaces, surf);
		} else {
			if (surf->texinfo->flags & SURF_WARP)
				R_SurfaceToSurfaces(mod->bsp.opaque_warp_surfaces, surf);
			else if (surf->texinfo->flags & SURF_ALPHATEST)
				R_SurfaceToSurfaces(mod->bsp.alpha_test_surfaces, surf);
			else
				R_SurfaceToSurfaces(mod->bsp.opaque_surfaces, surf);
		}

		if (surf->texinfo->image->material.flags & STAGE_RENDER)
			R_SurfaceToSurfaces(mod->bsp.material_surfaces, surf);
	}

	/* now sort them by texture */
	R_SortSurfacesArrays(mod);
}

static void R_LoadSurfacesArrays (model_t *mod)
{
	int i;

	R_LoadSurfacesArrays_(mod);

	for (i = 0; i < r_numModelsInline; i++)
		R_LoadSurfacesArrays_(&r_modelsInline[i]);
}


/**
 * @sa R_SetParent
 */
static void R_SetModel (mBspNode_t *node, model_t *mod)
{
	node->model = mod;

	if (node->contents > CONTENTS_NODE)
		return;

	R_SetModel(node->children[0], mod);
	R_SetModel(node->children[1], mod);
}


/**
 * @sa R_RecurseSetParent
 */
static void R_RecursiveSetModel (mBspNode_t *node, model_t *mod)
{
	/* skip special pathfinding nodes */
	if (node->contents == CONTENTS_PATHFINDING_NODE) {
		R_RecursiveSetModel(node->children[0], mod);
		R_RecursiveSetModel(node->children[1], mod);
	} else {
		R_SetModel(node, mod);
	}
}

/**
 * @brief Adds the specified static light source to the world model, after first
 * ensuring that it can not be merged with any known sources.
 */
static void R_AddBspLight (model_t* mod, vec3_t org, float radius)
{
	mBspLight_t *l;
	vec3_t delta;
	int i;

	l = mod->bsp.bsplights;
	for (i = 0; i < mod->bsp.numbsplights; i++, l++) {
		VectorSubtract(org, l->org, delta);

		if (VectorLength(delta) <= 48.0)  /* merge them */
			break;
	}

	if (i == mod->bsp.numbsplights) {
		l = (mBspLight_t *)Mem_PoolAlloc(sizeof(*l), vid_modelPool, 0);

		VectorCopy(org, l->org);

		if (!mod->bsp.bsplights)  /* first source */
			mod->bsp.bsplights = l;

		mod->bsp.numbsplights++;
	}

	l->radius += radius;

	if (l->radius > 250.0)  /* clamp */
		l->radius = 250.0;
}

/**
 * @brief Parse the entity string and resolve all static light sources. Also
 * iterate the world surfaces, allocating static light sources for those
 * which emit light.
 */
static void R_LoadBspLights (model_t *mod)
{
	const char *ents;
	vec3_t org;
	qboolean entity, light;
	float radius;
	int i;
	mBspSurface_t *surf;

	ents = CM_EntityString();

	VectorClear(org);
	radius = 0.0;

	entity = light = qfalse;

	while (qtrue) {
		const char *c = COM_Parse(&ents);
		if (!strlen(c))
			break;

		if (c[0] == '{')
			entity = qtrue;

		if (!entity)  /* skip any whitespace between ents */
			continue;

		if (c[0] == '}') {
			entity = qfalse;

			if (light) {
				R_AddBspLight(mod, org, radius);
				light = qfalse;
			}
		}

		if (!strcmp(c, "classname")) {
			c = COM_Parse(&ents);
			if (!strcmp(c, "light"))
				light = qtrue;
		}

		if (!strcmp(c, "origin")) {
			if (sscanf(COM_Parse(&ents), "%f %f %f", &org[0], &org[1], &org[2]) != 3)
				Com_Printf("Invalid origin vector given\n");
			continue;
		}

		if (!strcmp(c, "light")) {
			radius = atof(COM_Parse(&ents));
			continue;
		}
	}

	surf = mod->bsp.surfaces;
	for (i = 0; i < mod->bsp.numsurfaces; i++, surf++) {
		if (surf->texinfo->flags & SURF_LIGHT) {
			vec3_t tmp;

			VectorMA(surf->center, 1.0, surf->normal, org);

			VectorSubtract(surf->maxs, surf->mins, tmp);
			radius = VectorLength(tmp);

			R_AddBspLight(mod, org, radius > 100.0 ? radius : 100.0);
		}
	}
}

/**
 * @brief Sets up bmodels (brush models) like doors and breakable objects
 */
static void R_SetupSubmodels (void)
{
	int i;

	/* set up the submodels, the first 255 submodels are the models of the
	 * different levels, don't care about them */
	for (i = NUM_REGULAR_MODELS; i < r_worldmodel->bsp.numsubmodels; i++) {
		model_t *mod = &r_modelsInline[r_numModelsInline];
		const mBspHeader_t *sub = &r_worldmodel->bsp.submodels[i];

		/* copy most info from world */
		*mod = *r_worldmodel;
		mod->type = mod_bsp_submodel;

		Com_sprintf(mod->name, sizeof(mod->name), "*%d", i);

		/* copy the rest from the submodel */
		VectorCopy(sub->maxs, mod->maxs);
		VectorCopy(sub->mins, mod->mins);
		mod->radius = sub->radius;

		mod->bsp.firstnode = sub->headnode;
		mod->bsp.nodes = &r_worldmodel->bsp.nodes[mod->bsp.firstnode];
		mod->bsp.maptile = r_numMapTiles - 1;
		if (mod->bsp.firstnode >= r_worldmodel->bsp.numnodes)
			Com_Error(ERR_DROP, "R_SetupSubmodels: Inline model %i has bad firstnode", i);

		R_RecursiveSetModel(mod->bsp.nodes, mod);

		mod->bsp.firstmodelsurface = sub->firstface;
		mod->bsp.nummodelsurfaces = sub->numfaces;
		/*mod->bsp.numleafs = sub->visleafs;*/
		r_numModelsInline++;
	}
}

/**
 * @sa CM_AddMapTile
 * @sa R_ModBeginLoading
 * @param[in] name The name of the map. Relative to maps/ and without extension
 * @param[in] day Load the day lightmap
 * @param[in] sX Shift x grid units
 * @param[in] sY Shift y grid units
 * @param[in] sZ Shift z grid units
 * @sa UNIT_SIZE
 */
static void R_ModAddMapTile (const char *name, qboolean day, int sX, int sY, int sZ)
{
	int i;
	byte *buffer;
	dBspHeader_t *header;
	const int lightingLump = day ? LUMP_LIGHTING_DAY : LUMP_LIGHTING_NIGHT;

	/* get new model */
	if (r_numModels < 0 || r_numModels >= MAX_MOD_KNOWN)
		Com_Error(ERR_DROP, "R_ModAddMapTile: r_numModels >= MAX_MOD_KNOWN");

	if (r_numMapTiles < 0 || r_numMapTiles >= MAX_MAPTILES)
		Com_Error(ERR_DROP, "R_ModAddMapTile: Too many map tiles");

	/* alloc model and tile */
	r_worldmodel = &r_models[r_numModels++];
	r_mapTiles[r_numMapTiles++] = r_worldmodel;
	memset(r_worldmodel, 0, sizeof(*r_worldmodel));
	Com_sprintf(r_worldmodel->name, sizeof(r_worldmodel->name), "maps/%s.bsp", name);

	/* load the file */
	FS_LoadFile(r_worldmodel->name, &buffer);
	if (!buffer)
		Com_Error(ERR_DROP, "R_ModAddMapTile: %s not found", r_worldmodel->name);

	/* init */
	r_worldmodel->type = mod_bsp;

	/* prepare shifting */
	VectorSet(shift, sX * UNIT_SIZE, sY * UNIT_SIZE, sZ * UNIT_SIZE);

	/* test version */
	header = (dBspHeader_t *) buffer;
	i = LittleLong(header->version);
	if (i != BSPVERSION)
		Com_Error(ERR_DROP, "R_ModAddMapTile: %s has wrong version number (%i should be %i)", r_worldmodel->name, i, BSPVERSION);

	/* swap all the lumps */
	mod_base = (byte *) header;

	for (i = 0; i < (int)sizeof(dBspHeader_t) / 4; i++)
		((int *) header)[i] = LittleLong(((int *) header)[i]);

	/* load into heap */
	R_ModLoadVertexes(&header->lumps[LUMP_VERTEXES]);
	R_ModLoadNormals(&header->lumps[LUMP_NORMALS]);
	R_ModLoadEdges(&header->lumps[LUMP_EDGES]);
	R_ModLoadSurfedges(&header->lumps[LUMP_SURFEDGES]);
	R_ModLoadLighting(&header->lumps[lightingLump], day);
	R_ModLoadPlanes(&header->lumps[LUMP_PLANES]);
	R_ModLoadTexinfo(&header->lumps[LUMP_TEXINFO]);
	R_ModLoadSurfaces(day, &header->lumps[LUMP_FACES]);
	R_ModLoadLeafs(&header->lumps[LUMP_LEAFS]);
	R_ModLoadNodes(&header->lumps[LUMP_NODES]);
	R_ModLoadSubmodels(&header->lumps[LUMP_MODELS]);

	R_SetupSubmodels();

	R_LoadBspLights(r_worldmodel);

	R_LoadBspVertexArrays(r_worldmodel);

	R_LoadSurfacesArrays(r_worldmodel);

	/* in case of random map assembly shift some vectors */
	if (VectorNotEmpty(shift))
		R_ModShiftTile();

	FS_FreeFile(buffer);
}

/**
 * @brief Specifies the model that will be used as the world
 * @param[in] tiles The tiles string can be only one map or a list of space
 * seperated map tiles for random assembly. In case of random assembly we also
 * need the @c pos string. Every tile needs an entry in the @c pos string, too.
 * @param[in] pos In case of a random map assembly this is the string that holds
 * the world grid positions of the tiles. The positions are x, y and z values.
 * They are just written one after another for every tile in the @c tiles string
 * and every of the three components must exists for every tile.
 * @param[in] mapName The mapname that the get from the server (used to identify
 * the correct name for the materials in case of a random assembly).
 * @sa R_ModAddMapTile
 * @sa CM_LoadMap
 * @note This function is called for listen servers, too. This loads the bsp
 * struct for rendering it. The @c CM_LoadMap code only loads the collision
 * and pathfinding stuff.
 * @sa MN_BuildRadarImageList
 */
void R_ModBeginLoading (const char *tiles, qboolean day, const char *pos, const char *mapName)
{
	char name[MAX_VAR];
	char base[MAX_QPATH];
	ipos3_t sh;
	int i;

	assert(mapName);

	R_FreeWorldImages();

	if (mapName[0] == '+')
		R_LoadMaterials(mapName + 1);
	/* already assembled maps via console command? Skip them */
	else if (mapName[0] != '-')
		R_LoadMaterials(mapName);

	/* fix this, currently needed, slows down loading times */
	R_ShutdownModels(qfalse);

	/* init */
	R_BeginBuildingLightmaps();
	r_numModelsInline = 0;
	r_numMapTiles = 0;

	/* load tiles */
	while (tiles) {
		/* get tile name */
		const char *token = COM_Parse(&tiles);
		if (!tiles) {
			/* finish */
			R_EndBuildingLightmaps();
			return;
		}

		/* get base path */
		if (token[0] == '-') {
			Q_strncpyz(base, token + 1, sizeof(base));
			continue;
		}

		/* get tile name */
		if (token[0] == '+')
			Com_sprintf(name, sizeof(name), "%s%s", base, token + 1);
		else
			Q_strncpyz(name, token, sizeof(name));

		if (pos && pos[0]) {
			/* get grid position and add a tile */
			for (i = 0; i < 3; i++) {
				token = COM_Parse(&pos);
				if (!pos)
					Com_Error(ERR_DROP, "R_ModBeginLoading: invalid positions\n");
				sh[i] = atoi(token);
			}
			if (sh[0] <= -(PATHFINDING_WIDTH / 2) || sh[0] >= PATHFINDING_WIDTH / 2)
				Com_Error(ERR_DROP, "R_ModBeginLoading: invalid x position given: %i\n", sh[0]);
			if (sh[1] <= -(PATHFINDING_WIDTH / 2) || sh[1] >= PATHFINDING_WIDTH / 2)
				Com_Error(ERR_DROP, "R_ModBeginLoading: invalid y position given: %i\n", sh[1]);
			if (sh[2] >= PATHFINDING_HEIGHT)
				Com_Error(ERR_DROP, "R_ModBeginLoading: invalid z position given: %i\n", sh[2]);
			R_ModAddMapTile(name, day, sh[0], sh[1], sh[2]);
		} else {
			/* load only a single tile, if no positions are specified */
			R_ModAddMapTile(name, day, 0, 0, 0);
			R_EndBuildingLightmaps();
			return;
		}
	}

	Com_Error(ERR_DROP, "R_ModBeginLoading: invalid tile names\n");
}
