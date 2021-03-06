/**
 * @file
 */

/*
Copyright (C) 2002-2011 UFO: Alien Invasion.

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

#include "cl_inventory_callbacks.h"
#include "cl_shared.h"
#include "cl_inventory.h"
#include "ui/ui_main.h"
#include "ui/ui_nodes.h"
#include "cgame/cl_game.h"
#include "ui/ui_popup.h"

static const objDef_t *currentDisplayedObject;
static int itemIndex;
static int fireModeIndex;

/**
 * @brief Translate a weaponSkill integer to a translated string
 */
static const char* CL_WeaponSkillToName (int weaponSkill)
{
	switch (weaponSkill) {
	case SKILL_CLOSE:
		return _("skill_close");
	case SKILL_HEAVY:
		return _("skill_heavy");
	case SKILL_ASSAULT:
		return _("skill_assault");
	case SKILL_SNIPER:
		return _("skill_sniper");
	case SKILL_EXPLOSIVE:
		return _("skill_explosive");
	default:
		return _("Unknown weapon skill");
	}
}

/**
 * @brief Prints the description for items (weapons, armour, ...)
 * @param[in] od The object definition of the item
 * @note Not only called from UFOpaedia but also from other places to display
 * weapon and ammo statistics
 * @todo Do we need to add checks for @c od->isDummy here somewhere?
 */
void INV_ItemDescription (const objDef_t *od)
{
	static char itemText[UI_MAX_SMALLTEXTLEN];
	int i;
	int count;

	currentDisplayedObject = od;

	if (!od) {	/* If nothing selected return */
		Cvar_Set("mn_itemname", "");
		Cvar_Set("mn_item", "");
		UI_ResetData(TEXT_ITEMDESCRIPTION);
		itemIndex = fireModeIndex = 0;
		UI_ExecuteConfunc("itemdesc_view 0 0;");
		return;
	}

	/* select item */
	Cvar_Set("mn_itemname", _(od->name));
	Cvar_Set("mn_item", od->id);

	count = 0;
	if (INV_IsAmmo(od)) {
		/* We display the pre/next buttons for changing weapon only if there are at least 2 researched weapons
		 * we are counting the number of weapons that are usable with this ammo */
		for (i = 0; i < od->numWeapons; i++)
			if (GAME_ItemIsUseable(od->weapons[i]))
				count++;
		if (itemIndex >= od->numWeapons || itemIndex < 0)
			itemIndex = 0;
		if (count > 0) {
			while (!GAME_ItemIsUseable(od->weapons[itemIndex])) {
				itemIndex++;
				if (itemIndex >= od->numWeapons)
					itemIndex = 0;
			}
			Cvar_ForceSet("mn_linkname", _(od->weapons[itemIndex]->name));
		}
	} else if (od->weapon) {
		/* We display the pre/next buttons for changing ammo only if there are at least 2 researched ammo
		 * we are counting the number of ammo that is usable with this weapon */
		for (i = 0; i < od->numAmmos; i++)
			if (GAME_ItemIsUseable(od->ammos[i]))
				count++;

		if (itemIndex >= od->numAmmos || itemIndex < 0)
			itemIndex = 0;

		/* Only display ammos if at least one has been researched */
		if (count > 0) {
			/* We have a weapon that uses ammos */
			while (!GAME_ItemIsUseable(od->ammos[itemIndex])) {
				itemIndex++;
				if (itemIndex >= od->numAmmos)
					itemIndex = 0;
			}
			Cvar_ForceSet("mn_linkname", _(od->ammos[itemIndex]->name));
		}
	}

	/* set description text if item has been researched or one of its ammo/weapon has been researched */
	if (count > 0 || GAME_ItemIsUseable(od)) {
		int numFiredefs = 0;

		*itemText = '\0';
		if (INV_IsArmour(od)) {
			Com_sprintf(itemText, sizeof(itemText), _("Size:\t%i\n"), od->size);
			Q_strcat(itemText, va(_("Weight:\t%g Kg\n"), od->weight), sizeof(itemText));
			Q_strcat(itemText, "\n", sizeof(itemText));
			Q_strcat(itemText, _("^BDamage type:\tProtection:\n"), sizeof(itemText));
			for (i = 0; i < csi.numDTs; i++) {
				const damageType_t *dt = &csi.dts[i];
				if (!dt->showInMenu)
					continue;
				Q_strcat(itemText, va(_("%s\t%i\n"), _(dt->id), od->ratings[i]), sizeof(itemText));
			}
		} else if ((od->weapon && od->numAmmos) || INV_IsAmmo(od)) {
			const objDef_t *odAmmo;

			if (count > 0) {
				int weaponIndex;
				if (od->weapon) {
					Com_sprintf(itemText, sizeof(itemText), _("%s weapon\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
					if (od->ammo > 0)
						Q_strcat(itemText, va(_("Max ammo:\t%i\n"), od->ammo), sizeof(itemText));
					odAmmo = (od->numAmmos) ? od->ammos[itemIndex] : od;
					assert(odAmmo);
					for (weaponIndex = 0; (weaponIndex < odAmmo->numWeapons) && (odAmmo->weapons[weaponIndex] != od); weaponIndex++) {}
				} else {
					odAmmo = od;
					weaponIndex = itemIndex;
				}

				Q_strcat(itemText, va(_("Weight:\t%g Kg\n"), od->weight), sizeof(itemText));
				/** @todo is there ammo with no firedefs? */
				if (GAME_ItemIsUseable(odAmmo) && odAmmo->numFiredefs[weaponIndex] > 0) {
					const fireDef_t *fd;
					numFiredefs = odAmmo->numFiredefs[weaponIndex];

					/* This contains everything common for weapons and ammos */
					/* We check if the wanted firemode to display exists. */
					if (fireModeIndex > numFiredefs - 1)
						fireModeIndex = 0;
					if (fireModeIndex < 0)
						fireModeIndex = numFiredefs - 1;

					fd = &odAmmo->fd[weaponIndex][fireModeIndex];

					/* We always display the name of the firemode for an ammo */
					Cvar_Set("mn_firemodename", _(fd->name));

					/* We display the characteristics of this firemode */
					Q_strcat(itemText, va(_("Skill:\t%s\n"), CL_WeaponSkillToName(fd->weaponSkill)), sizeof(itemText));
					Q_strcat(itemText, va(_("Damage:\t%i\n"), (int) (fd->damage[0] + fd->spldmg[0]) * fd->shots), sizeof(itemText));
					Q_strcat(itemText, va(_("Time units:\t%i\n"), fd->time), sizeof(itemText));
					Q_strcat(itemText, va(_("Range:\t%g\n"), fd->range / UNIT_SIZE), sizeof(itemText));
					Q_strcat(itemText, va(_("Spreads:\t%g\n"), (fd->spread[0] + fd->spread[1]) / 2), sizeof(itemText));
				}
			} else {
				Com_sprintf(itemText, sizeof(itemText), _("%s. No detailed info available.\n"), INV_IsAmmo(od) ? _("Ammunition") : _("Weapon"));
				Q_strcat(itemText, va(_("Weight:\t%g Kg\n"), od->weight), sizeof(itemText));
			}
		} else if (od->weapon) {
			Com_sprintf(itemText, sizeof(itemText), _("%s ammo-less weapon\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Weight:\t%g Kg\n"), od->weight), sizeof(itemText));
		} else {
			/* just an item - only primary definition */
			Com_sprintf(itemText, sizeof(itemText), _("%s auxiliary equipment\n"), (od->fireTwoHanded ? _("Two-handed") : _("One-handed")));
			Q_strcat(itemText, va(_("Weight:\t%g Kg\n"), od->weight), sizeof(itemText));
			if (od->numWeapons > 0 && od->numFiredefs[0] > 0) {
				const fireDef_t *fd = &od->fd[0][0];
				Q_strcat(itemText, va(_("Action:\t%s\n"), _(fd->name)), sizeof(itemText));
				Q_strcat(itemText, va(_("Time units:\t%i\n"), fd->time), sizeof(itemText));
				Q_strcat(itemText, va(_("Range:\t%g\n"), fd->range / UNIT_SIZE), sizeof(itemText));
			}
		}

		UI_RegisterText(TEXT_ITEMDESCRIPTION, itemText);
		UI_ExecuteConfunc("itemdesc_view %i %i;", count, numFiredefs);
	} else {
		Com_sprintf(itemText, sizeof(itemText), _("Unknown - not useable"));
		UI_RegisterText(TEXT_ITEMDESCRIPTION, itemText);
		UI_ExecuteConfunc("itemdesc_view 0 0;");
	}
}

/**
 * @brief Increases the number of the firemode to display
 * @sa UP_ItemDescription
 */
static void INV_IncreaseFiremode_f (void)
{
	if (!currentDisplayedObject)
		return;

	fireModeIndex++;

	INV_ItemDescription(currentDisplayedObject);
}

/**
 * @brief Decreases the number of the firemode to display
 * @sa UP_ItemDescription
 */
static void INV_DecreaseFiremode_f (void)
{
	if (!currentDisplayedObject)
		return;

	fireModeIndex--;

	INV_ItemDescription(currentDisplayedObject);
}

/**
 * @brief Increases the number of the weapon to display (for ammo) or the ammo to display (for weapon)
 * @sa UP_ItemDescription
 */
static void INV_IncreaseItem_f (void)
{
	const objDef_t *od = currentDisplayedObject;

	if (!od)
		return;

	if (od->numWeapons) {
		const int current = itemIndex;
		do {
			itemIndex++;
			if (itemIndex > od->numWeapons - 1) {
				itemIndex = 0;
			}
		} while (itemIndex != current && !GAME_ItemIsUseable(od->weapons[itemIndex]));
	} else if (od->numAmmos) {
		const int current = itemIndex;
		do {
			itemIndex++;
			if (itemIndex > od->numAmmos - 1) {
				itemIndex = 0;
			}
		} while (itemIndex != current && !GAME_ItemIsUseable(od->ammos[itemIndex]));
	}
	INV_ItemDescription(od);
}

/**
 * @brief Decreases the number of the firemode to display (for ammo) or the ammo to display (for weapon)
 * @sa UP_ItemDescription
 */
static void INV_DecreaseItem_f (void)
{
	const objDef_t *od = currentDisplayedObject;

	if (!od)
		return;

	if (od->numWeapons) {
		const int current = itemIndex;
		do {
			itemIndex--;
			if (itemIndex < 0) {
				itemIndex = od->numWeapons - 1;
			}
		} while (itemIndex != current && !GAME_ItemIsUseable(od->weapons[itemIndex]));
	} else if (od->numAmmos) {
		const int current = itemIndex;
		do {
			itemIndex--;
			if (itemIndex < 0) {
				itemIndex = od->numAmmos - 1;
			}
		} while (itemIndex != current && !GAME_ItemIsUseable(od->ammos[itemIndex]));
	}
	INV_ItemDescription(od);
}

/**
 * @brief Update the GUI with the selected item
 */
static void INV_UpdateObject_f (void)
{
	int num;
	const objDef_t *obj;
	bool changeTab;

	/* check syntax */
	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: %s <objectid> <confunc> [mustwechangetab]\n", Cmd_Argv(0));
		return;
	}

	if (Cmd_Argc() == 4)
		changeTab = atoi(Cmd_Argv(3)) >= 1;
	else
		changeTab = true;

	num = atoi(Cmd_Argv(1));
	if (num < 0 || num >= csi.numODs) {
		Com_Printf("Id %i out of range 0..%i\n", num, csi.numODs);
		return;
	}
	obj = INVSH_GetItemByIDX(num);

	/* update tab */
	if (changeTab) {
		const cvar_t *var = Cvar_FindVar("mn_equiptype");
		const int filter = INV_GetFilterFromItem(obj);
		if (var && var->integer != filter) {
			Cvar_SetValue("mn_equiptype", filter);
			UI_ExecuteConfunc("%s", Cmd_Argv(2));
		}
	}

	/* update item description */
	INV_ItemDescription(obj);
}

/**
 * @brief Update the equipment weight for the selected actor.
 */
static void INV_UpdateActorLoad_f (void)
{
	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: %s <callback>\n", Cmd_Argv(0));
		return;
	}

	const character_t *chr = GAME_GetSelectedChr();
	if (chr != NULL) {
		const float invWeight = INVSH_GetInventoryWeight(&chr->i);
		const int maxWeight = GAME_GetChrMaxLoad(chr);
		const float penalty = GET_ENCUMBRANCE_PENALTY(invWeight, chr->score.skills[ABILITY_POWER]);
		const int maxTU = GET_TU(chr->score.skills[ABILITY_SPEED]);
		const int tus = maxTU * penalty;
		const int tuPenalty = maxTU * WEIGHT_NORMAL_PENALTY - (maxTU - tus);
		int count = 0;

		for (containerIndex_t containerID = 0; containerID < csi.numIDs; containerID++) {
			if (csi.ids[containerID].temp)
				continue;
			for (invList_t *invList = chr->i.c[containerID], *next; invList; invList = next) {
				const fireDef_t *fireDef;
				next = invList->next;
				fireDef = FIRESH_FiredefForWeapon(&invList->item);
				if (fireDef == NULL)
					continue;
				for (int i = 0; i < MAX_FIREDEFS_PER_WEAPON; i++)
					if (fireDef[i].time > 0 && fireDef[i].time > tus) {
						if (count <= 0)
							Com_sprintf(popupText, sizeof(popupText), _("This soldier no longer has enough TUs to use the following items:\n\n"));
						Q_strcat(popupText, va("%s: %s (%i)\n", _(invList->item.item->name), _(fireDef[i].name), fireDef[i].time), sizeof(popupText));
						++count;
					}
			}
		}
		if (count > 0)
			UI_Popup(_("Warning"), popupText);

		UI_ExecuteConfunc("%s \"%g/%i %s\" \"%s %+i\" %f", Cmd_Argv(1), invWeight, maxWeight, _("Kg"), _("TU:"), tuPenalty, WEIGHT_NORMAL_PENALTY - (1 - penalty));
	}
}

void INV_InitCallbacks (void)
{
	Cmd_AddCommand("mn_increasefiremode", INV_IncreaseFiremode_f, "Increases the number of the firemode to display");
	Cmd_AddCommand("mn_decreasefiremode", INV_DecreaseFiremode_f, "Decreases the number of the firemode to display");
	Cmd_AddCommand("mn_increaseitem", INV_IncreaseItem_f, "Increases the number of the weapon or the ammo to display");
	Cmd_AddCommand("mn_decreaseitem", INV_DecreaseItem_f, "Decreases the number of the weapon or the ammo to display");
	Cmd_AddCommand("object_update", INV_UpdateObject_f, "Update the GUI with the selected item");
	Cmd_AddCommand("mn_updateactorload", INV_UpdateActorLoad_f, "Update the GUI with the selected actor inventory load");
}
