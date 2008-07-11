/**
 * @file cl_installationmanagement.h
 * @brief Header for installation management related stuff.
 */

/*
Copyright (C) 2002-2007 UFO: Alien Invasion team.

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

#ifndef CLIENT_CL_INSTALLATION_H
#define CLIENT_CL_INSTALLATION_H

#include "cl_basemanagement.h"

#define MAX_LIST_CHAR		1024
#define MAX_INSTALLATIONS	20

#define MAX_INSTALLATION_DAMAGE	100
#define MAX_INSTALLATION_BATTERIES	5
#define MAX_INSTALLATION_SLOT	4

/**
 * @brief Possible installation states
 * @note: Don't change the order or you have to change the installationmenu scriptfiles, too
 */
typedef enum {
	INSTALLATION_NOT_USED,
	INSTALLATION_WORKING		/**< nothing special */
} installationStatus_t;

/**
 * @brief Possible installation states
 * @note: Don't change the order or you have to change the installationmenu scriptfiles, too
 */
typedef enum {
	INSTALLATION_RADAR,
	INSTALLATION_UFO_YARD,
	INSTALLATION_SAM
} installationType_t;


typedef struct installationWeapon_s {
	/* int idx; */
	aircraftSlot_t slot;	/**< Weapon. */
	aircraft_t *target;		/**< Aimed target for the weapon. */
} installationWeapon_t;

/** @brief A installation with all it's data */
typedef struct installation_s {
	int idx;					/**< Self link. Index in the global installation-list. */
	char name[MAX_VAR];			/**< Name of the installation */

	installationType_t installationType; /** type of installation.  Radar, Sam Site or UFO Yard **/ 

	qboolean founded;	/**< already founded? */
	vec3_t pos;		/**< pos on geoscape */

	/** All ufo aircraft in this installation.  This used for UFO Yards. **/
	aircraft_t aircraft[MAX_AIRCRAFT];
	int numAircraftInInstallation;	/**< How many aircraft are in this installation. */

	capacities_t aircraftCapacitiy;		/**< Capacity of UFO Yard. */

	installationStatus_t installationStatus; /**< the current installation status */

	float alienInterest;	/**< How much aliens know this installation (and may attack it) */

	radar_t	radar;	

	baseWeapon_t batteries[MAX_INSTALLATION_BATTERIES];	/**< Missile/Laser batteries assigned to this installation.  For Sam Sites Only.  */
	int numBatteries;

	int installationDamage;			/**< Hit points of installation */

} installation_t;

/** Currently displayed/accessed installation. */
/* extern installation_t *installationCurrent; */

installation_t* INS_GetInstallationByIDX (int instIdx);
installation_t* INS_GetFoundedInstallationByIDX (int instIdx);
void INS_SetUpInstallation (installation_t* installation);

void INS_NewInstallations(void);
void INS_ResetInstallation(void);

/** Coordinates to place the new installation at (long, lat) */
extern vec3_t newInstallationPos;

int INS_GetFoundedInstallationCount(void);
installation_t* INS_GetInstallationByIDX(int installationIdx);
installation_t* INS_GetFoundedInstallationByIDX(int installationIdx);

void INS_NewInstallations(void);

aircraft_t *INS_GetAircraftFromInstallationByIndex(installation_t* installation, int index);

void CL_InstallationDestroy(installation_t *installation);

void INS_InitStartup(void);
void INS_ParseInstallationNames(const char *name, const char **text);

qboolean INS_Load(sizebuf_t* sb, void* data);
qboolean INS_Save(sizebuf_t* sb, void* data);

#endif /* CLIENT_CL_INSTALLATION_H */
