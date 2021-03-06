#include "g_local.h"

static bool G_CameraUse (edict_t *self, edict_t *activator)
{
	if (!activator || !G_IsActor(activator)) {
		return false;
	}

	self->active ^= true;

	return false;
}

static bool Destroy_Camera (edict_t *self)
{
	G_SpawnParticle(self->origin, self->spawnflags, self->particle);
	G_FreeEdict(self);
	return true;
}

#define CAMERAMODEL(X, IDX) case X: ent->model = "models/objects/cameras/camera" STRINGIFY(IDX); break

void G_InitCamera (edict_t *ent, camera_type_t cameraType, float angle, bool rotate)
{
	switch (cameraType) {
	CAMERAMODEL(CAMERA_MOBILE, 0);
	CAMERAMODEL(CAMERA_STATIONARY, 1);
	default:
		gi.DPrintf("unknown camera type given: %i\n", cameraType);
		G_FreeEdict(ent);
		return;
	}

	AABB modelAabb;
	if (gi.LoadModelAABB(ent->model, 0, modelAabb)) {
		VectorCopy(modelAabb.maxs, ent->maxs);
		VectorCopy(modelAabb.mins, ent->mins);

		ent->camera.cameraType = cameraType;
		ent->camera.rotate = rotate;
		ent->classname = "misc_camera";
		ent->type = ET_CAMERA;
		ent->solid = SOLID_BBOX;
		ent->flags |= FL_DESTROYABLE;
		ent->material = MAT_ELECTRICAL;
		ent->fieldSize = ACTOR_SIZE_NORMAL;
		ent->destroy = Destroy_Camera;
		ent->use = G_CameraUse;
		ent->dir = AngleToDir(angle);

		/* Set the position of the entity */
		VecToPos(ent->origin, ent->pos);

		gi.LinkEdict(ent);
	} else {
		gi.DPrintf("Could not get bounding box for model '%s'\n", ent->model);
		G_FreeEdict(ent);
	}
}

edict_t *G_SpawnCamera (const vec3_t origin, int team, camera_type_t cameraType)
{
	edict_t* ent = G_Spawn();
	VectorCopy(origin, ent->origin);
	ent->team = team;

	G_InitCamera(ent, cameraType, 0, true);

	G_CheckVis(ent);

	return ent;
}
