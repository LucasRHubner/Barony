/*-------------------------------------------------------------------------------

	BARONY
	File: actHandMagic.cpp
	Desc: the spellcasting animations

	Copyright 2013-2016 (c) Turning Wheel LLC, all rights reserved.
	See LICENSE for details.

-------------------------------------------------------------------------------*/

#include "../main.hpp"
#include "../game.hpp"
#include "../stat.hpp"
#include "../entity.hpp"
#include "../interface/interface.hpp"
#include "../sound.hpp"
#include "../items.hpp"
#include "../player.hpp"
#include "magic.hpp"
#include "../net.hpp"
#include "../scores.hpp"

//The spellcasting animation stages:
#define CIRCLE 0 //One circle
#define THROW 1 //Throw spell!

spellcasting_animation_manager_t cast_animation;
bool overDrawDamageNotify = false;
Entity* magicLeftHand = NULL;
Entity* magicRightHand = NULL;

#define HANDMAGIC_INIT my->skill[0]
#define HANDMAGIC_TESTVAR my->skill[1]
#define HANDMAGIC_YAW my->fskill[3]
#define HANDMAGIC_PITCH my->fskill[4]
#define HANDMAGIC_ROLL my->fskill[5]
#define HANDMAGIC_PARTICLESPRAY1 my->skill[3]
#define HANDMAGIC_CIRCLE_RADIUS 0.8
#define HANDMAGIC_CIRCLE_SPEED 0.3

void fireOffSpellAnimation(spellcasting_animation_manager_t* animation_manager, Uint32 caster_uid, spell_t* spell, bool usingSpellbook)
{
	//This function triggers the spellcasting animation and sets up everything.

	if (!animation_manager)
	{
		return;
	}
	Entity* caster = uidToEntity(caster_uid);
	if (!caster)
	{
		return;
	}
	if (!spell)
	{
		return;
	}
	if (!magicLeftHand)
	{
		return;
	}
	if (!magicRightHand)
	{
		return;
	}

	playSoundEntity(caster, 170, 128 );
	Stat* stat = caster->getStats();

	//Save these two very important pieces of data.
	animation_manager->caster = caster->getUID();
	animation_manager->spell = spell;

	if ( !usingSpellbook )
	{
		animation_manager->active = true;
	}
	else
	{
		animation_manager->active_spellbook = true;
	}
	animation_manager->stage = CIRCLE;

	//Make the HUDWEAPON disappear, or somesuch?
	if ( stat->type != RAT )
	{
		if ( !usingSpellbook )
		{
			magicLeftHand->flags[INVISIBLE] = false;
		}
		magicRightHand->flags[INVISIBLE] = false;
	}

	animation_manager->lefthand_angle = 0;
	animation_manager->lefthand_movex = 0;
	animation_manager->lefthand_movey = 0;
	int spellCost = getCostOfSpell(spell, caster);
	animation_manager->circle_count = 0;
	animation_manager->times_to_circle = (spellCost / 10) + 1; //Circle once for every 10 mana the spell costs.
	animation_manager->mana_left = spellCost;
	animation_manager->consumeMana = true;
	if ( spell->ID == SPELL_FORCEBOLT && caster->skillCapstoneUnlockedEntity(PRO_SPELLCASTING) )
	{
		animation_manager->consumeMana = false;
	}

	if (stat->PROFICIENCIES[PRO_SPELLCASTING] < SPELLCASTING_BEGINNER)   //There's a chance that caster is newer to magic (and thus takes longer to cast a spell).
	{
		int chance = rand() % 10;
		if (chance >= stat->PROFICIENCIES[PRO_SPELLCASTING] / 15)
		{
			int amount = (rand() % 50) / std::max(stat->PROFICIENCIES[PRO_SPELLCASTING] + statGetINT(stat, caster), 1);
			amount = std::min(amount, CASTING_EXTRA_TIMES_CAP);
			animation_manager->times_to_circle += amount;
		}
	}
	if ( usingSpellbook && stat->shield && itemCategory(stat->shield) == SPELLBOOK )
	{
		if ( !playerLearnedSpellbook(stat->shield) || (stat->shield->beatitude < 0 && !shouldInvertEquipmentBeatitude(stat)) )
		{
			// for every tier below the spell you are, add 3 circle for 1 tier, or add 2 for every additional tier.
			int casterAbility = std::min(100, std::max(0, stat->PROFICIENCIES[PRO_SPELLCASTING] + statGetINT(stat, caster))) / 20;
			if ( stat->shield->beatitude < 0 )
			{
				casterAbility = 0; // cursed book has cast penalty.
			}
			int difficulty = spell->difficulty / 20;
			if ( difficulty > casterAbility )
			{
				animation_manager->times_to_circle += (std::min(5, 1 + 2 * (difficulty - casterAbility)));
			}
		}
		else if ( stat->PROFICIENCIES[PRO_SPELLCASTING] >= SPELLCASTING_BEGINNER )
		{
			animation_manager->times_to_circle = (spellCost / 20) + 1; //Circle once for every 20 mana the spell costs.
		}
	}
	animation_manager->consume_interval = (animation_manager->times_to_circle * ((2 * PI) / HANDMAGIC_CIRCLE_SPEED)) / spellCost;
	animation_manager->consume_timer = animation_manager->consume_interval;
}

void spellcastingAnimationManager_deactivate(spellcasting_animation_manager_t* animation_manager)
{
	animation_manager->caster = -1;
	animation_manager->spell = NULL;
	animation_manager->active = false;
	animation_manager->active_spellbook = false;
	animation_manager->stage = 0;

	//Make the hands invisible (should probably fall away or something, but whatever. That's another project for another day)
	if ( magicLeftHand )
	{
		magicLeftHand->flags[INVISIBLE] = true;
	}
	if ( magicRightHand )
	{
		magicRightHand->flags[INVISIBLE] = true;
	}
}

void spellcastingAnimationManager_completeSpell(spellcasting_animation_manager_t* animation_manager)
{
	castSpell(animation_manager->caster, animation_manager->spell, false, false, animation_manager->active_spellbook); //Actually cast the spell.

	spellcastingAnimationManager_deactivate(animation_manager);
}

/*
[12:48:29 PM] Sheridan Kane Rathbun: you can move the entities about by modifying their x, y, z, yaw, pitch, and roll parameters.
[12:48:43 PM] Sheridan Kane Rathbun: everything's relative to the camera since the OVERDRAW flag is on.
[12:49:05 PM] Sheridan Kane Rathbun: so adding x will move it forward, adding y will move it sideways (forget which way) and adding z will move it up and down.
[12:49:46 PM] Sheridan Kane Rathbun: the first step is to get the hands visible on the screen when you cast. worry about moving them when that critical part is done.
*/

void actLeftHandMagic(Entity* my)
{
	//int c = 0;
	if (intro == true)
	{
		my->flags[INVISIBLE] = true;
		return;
	}

	//Initialize
	if (!HANDMAGIC_INIT)
	{
		HANDMAGIC_INIT = 1;
		HANDMAGIC_TESTVAR = 0;
		my->focalz = -1.5;
	}

	if (players[clientnum] == nullptr || players[clientnum]->entity == nullptr
		|| (players[clientnum]->entity && players[clientnum]->entity->playerCreatedDeathCam != 0) )
	{
		magicLeftHand = nullptr;
		spellcastingAnimationManager_deactivate(&cast_animation);
		list_RemoveNode(my->mynode);
		return;
	}

	//Set the initial values. (For the particle spray)
	my->x = 8;
	my->y = -3;
	my->z = (camera.z * .5 - players[clientnum]->entity->z) + 7;
	my->z -= 4;
	my->yaw = HANDMAGIC_YAW - camera_shakex2;
	double defaultpitch = (0 - 2.2);
	my->pitch = defaultpitch + HANDMAGIC_PITCH - camera_shakey2 / 200.f;
	my->roll = HANDMAGIC_ROLL;
	my->scalex = 0.5f;
	my->scaley = 0.5f;
	my->scalez = 0.5f;
	my->z -= 0.75;

	//Sprite
	Monster playerRace = players[clientnum]->entity->getMonsterFromPlayerRace(stats[clientnum]->playerRace);
	int playerAppearance = stats[clientnum]->appearance;
	if ( players[clientnum]->entity->effectShapeshift != NOTHING )
	{
		playerRace = static_cast<Monster>(players[clientnum]->entity->effectShapeshift);
	}
	else if ( players[clientnum]->entity->effectPolymorph != NOTHING )
	{
		if ( players[clientnum]->entity->effectPolymorph > NUMMONSTERS )
		{
			playerRace = HUMAN;
			playerAppearance = players[clientnum]->entity->effectPolymorph - 100;
		}
		else
		{
			playerRace = static_cast<Monster>(players[clientnum]->entity->effectPolymorph);
		}
	}

	bool noGloves = false;
	if ( stats[clientnum]->gloves == NULL
		|| playerRace == SPIDER
		|| playerRace == RAT
		|| playerRace == CREATURE_IMP
		|| playerRace == TROLL )
	{
		noGloves = true;
	}
	else
	{
		if ( stats[clientnum]->gloves->type == GLOVES || stats[clientnum]->gloves->type == GLOVES_DEXTERITY )
		{
			my->sprite = 659;
		}
		else if ( stats[clientnum]->gloves->type == BRACERS || stats[clientnum]->gloves->type == BRACERS_CONSTITUTION )
		{
			my->sprite = 660;
		}
		else if ( stats[clientnum]->gloves->type == GAUNTLETS || stats[clientnum]->gloves->type == GAUNTLETS_STRENGTH )
		{
			my->sprite = 661;
		}
		else if ( stats[clientnum]->gloves->type == BRASS_KNUCKLES )
		{
			my->sprite = 662;
		}
		else if ( stats[clientnum]->gloves->type == IRON_KNUCKLES )
		{
			my->sprite = 663;
		}
		else if ( stats[clientnum]->gloves->type == SPIKED_GAUNTLETS )
		{
			my->sprite = 664;
		}
		else if ( stats[clientnum]->gloves->type == CRYSTAL_GLOVES )
		{
			my->sprite = 666;
		}
		else if ( stats[clientnum]->gloves->type == ARTIFACT_GLOVES )
		{
			my->sprite = 665;
		}
		else if ( stats[clientnum]->gloves->type == SUEDE_GLOVES )
		{
			my->sprite = 803;
		}
		else if (stats[clientnum]->gloves->type == ABYSSAL_KNUCKLES)
		{
			my->sprite = 1125;
		}
		else if (stats[clientnum]->gloves->type == ICE_GLOVES)
		{
			my->sprite = 1279;
		}
		else if (stats[clientnum]->gloves->type == INQUISITOR_GLOVES)
		{
			my->sprite = 1290;
		}
		else if (stats[clientnum]->gloves->type == LIFESTEAL_KNUCKLES)
		{
			my->sprite = 1320;
		}
		else if (stats[clientnum]->gloves->type == MANA_GLOVES)
		{
			my->sprite = 1323;
		}
		else if (stats[clientnum]->gloves->type == TIN_GLOVES)
		{
			my->sprite = 1335;
		}
		else if (stats[clientnum]->gloves->type == LOST_GAUNTLETS)
		{
			my->sprite = 1359;
		}

	}


	if ( noGloves )
	{

		switch ( playerRace )
		{
			case SKELETON:
				my->sprite = 773;
				break;
			case INCUBUS:
				my->sprite = 775;
				break;
			case SUCCUBUS:
				my->sprite = 777;
				break;
			case GOBLIN:
				my->sprite = 779;
				break;
			case AUTOMATON:
				my->sprite = 781;
				break;
			case INSECTOID:
				if ( stats[clientnum]->sex == FEMALE )
				{
					my->sprite = 785;
				}
				else
				{
					my->sprite = 783;
				}
				break;
			case GOATMAN:
				my->sprite = 787;
				break;
			case VAMPIRE:
				my->sprite = 789;
				break;
			case HUMAN:
				if ( playerAppearance / 6 == 0 )
				{
					my->sprite = 656;
				}
				else if ( playerAppearance / 6 == 1 )
				{
					my->sprite = 657;
				}
				else
				{
					my->sprite = 658;
				}
				break;
			case TROLL:
				my->sprite = 856;
				break;
			case SPIDER:
				my->sprite = 854;
				break;
			case CREATURE_IMP:
				my->sprite = 858;
				break;
			default:
				my->sprite = 656;
				break;
		}
		/*}
		else if ( playerAppearance / 6 == 0 )
		{
			my->sprite = 656;
		}
		else if ( playerAppearance / 6 == 1 )
		{
			my->sprite = 657;
		}
		else
		{
			my->sprite = 658;
		}*/
	}

	if ( playerRace == RAT )
	{
		my->flags[INVISIBLE] = true;
		my->y = 0;
		my->z += 1;
	}
	if ( playerRace == SPIDER && hudarm && players[clientnum]->entity->bodyparts.at(0) )
	{
		my->x = hudarm->x;
		my->y = -hudarm->y;
		//my->z = hudArm->z;
		my->pitch = hudarm->pitch;
		my->roll = -hudarm->roll;
		my->yaw = -players[clientnum]->entity->bodyparts.at(0)->yaw;
		my->scalex = hudarm->scalex;
		my->scaley = hudarm->scaley;
		my->scalez = hudarm->scalez;
		my->focalz = hudarm->focalz;
	}
	else
	{
		my->focalz = -1.5;
	}

	bool wearingring = false;

	//Select model
	if (stats[clientnum]->ring != NULL)
	{
		if (stats[clientnum]->ring->type == RING_INVISIBILITY)
		{
			wearingring = true;
		}
	}
	if (stats[clientnum]->cloak != NULL)
	{
		if (stats[clientnum]->cloak->type == CLOAK_INVISIBILITY)
		{
			wearingring = true;
		}
	}
	if (stats[clientnum]->mask != NULL)
	{
		if (stats[clientnum]->mask->type == ABYSSAL_MASK)
		{
			wearingring = true;
		}
	}
	if (players[clientnum]->entity->skill[3] == 1 || players[clientnum]->entity->isInvisible() )   // debug cam or player invisible
	{
		my->flags[INVISIBLE] = true;
	}

	if ( (cast_animation.active || cast_animation.active_spellbook) )
	{
		switch (cast_animation.stage)
		{
			case CIRCLE:
				if ( ticks % 5 == 0 && !(players[clientnum]->entity->skill[3] == 1) )
				{
					Entity* entity = spawnGib(my);
					entity->flags[INVISIBLE] = false;
					entity->flags[SPRITE] = true;
					entity->flags[NOUPDATE] = true;
					entity->flags[UPDATENEEDED] = false;
					entity->flags[OVERDRAW] = true;
					entity->flags[BRIGHT] = true;
					entity->scalex = 0.25f; //MAKE 'EM SMALL PLEASE!
					entity->scaley = 0.25f;
					entity->scalez = 0.25f;
					entity->sprite = 16; //TODO: Originally. 22. 16 -- spark sprite instead?
					if ( cast_animation.active_spellbook )
					{
						entity->y -= 1.5;
						entity->z += 1;
					}
					entity->yaw = ((rand() % 6) * 60) * PI / 180.0;
					entity->pitch = (rand() % 360) * PI / 180.0;
					entity->roll = (rand() % 360) * PI / 180.0;
					entity->vel_x = cos(entity->yaw) * .1;
					entity->vel_y = sin(entity->yaw) * .1;
					entity->vel_z = -.15;
					entity->fskill[3] = 0.01;
				}
				cast_animation.consume_timer--;
				if ( cast_animation.consume_timer < 0 && cast_animation.mana_left > 0 )
				{
					//Time to consume mana and reset the ticker!
					cast_animation.consume_timer = cast_animation.consume_interval;
					if ( multiplayer == SINGLE && cast_animation.consumeMana )
					{
						int HP = stats[clientnum]->HP;
						players[clientnum]->entity->drainMP(1, false); // don't notify otherwise we'll get spammed each 1 mp
						if ( (HP > stats[clientnum]->HP) && !overDrawDamageNotify )
						{
							overDrawDamageNotify = true;
							camera_shakex += 0.1;
							camera_shakey += 10;
							playSoundPlayer(clientnum, 28, 92);
							Uint32 color = SDL_MapRGB(mainsurface->format, 255, 255, 0);
							messagePlayerColor(clientnum, color, language[621]);
						}
					}
					--cast_animation.mana_left;
				}

				cast_animation.lefthand_angle += HANDMAGIC_CIRCLE_SPEED;
				cast_animation.lefthand_movex = cos(cast_animation.lefthand_angle) * HANDMAGIC_CIRCLE_RADIUS;
				cast_animation.lefthand_movey = sin(cast_animation.lefthand_angle) * HANDMAGIC_CIRCLE_RADIUS;
				if (cast_animation.lefthand_angle >= 2 * PI)   //Completed one loop.
				{
					cast_animation.lefthand_angle = 0;
					cast_animation.circle_count++;
					if (cast_animation.circle_count >= cast_animation.times_to_circle)
						//Finished circling. Time to move on!
					{
						cast_animation.stage++;
					}
				}
				break;
			case THROW:
				//messagePlayer(clientnum, "IN THROW");
				//TODO: Throw animation! Or something.
				cast_animation.stage++;
				break;
			default:
				//messagePlayer(clientnum, "DEFAULT CASE");
				spellcastingAnimationManager_completeSpell(&cast_animation);
				break;
		}
	}
	else
	{
		overDrawDamageNotify = false;
	}

	//Final position code.
	if (players[clientnum] == nullptr || players[clientnum]->entity == nullptr)
	{
		return;
	}
	//double defaultpitch = PI / 8.f;
	//double defaultpitch = 0;
	//double defaultpitch = PI / (0-4.f);
	//defaultpitch = (0 - 2.8);
	//my->x = 6 + HUDWEAPON_MOVEX;

	if ( playerRace == SPIDER && hudarm && players[clientnum]->entity->bodyparts.at(0) )
	{
		my->x = hudarm->x;
		my->y = -hudarm->y;
		my->z = hudarm->z;
		my->pitch = hudarm->pitch;
		my->roll = -hudarm->roll;
		my->yaw = -players[clientnum]->entity->bodyparts.at(0)->yaw;
		my->scalex = hudarm->scalex;
		my->scaley = hudarm->scaley;
		my->scalez = hudarm->scalez;
		my->focalz = hudarm->focalz;
	}
	else
	{
		my->y = -3;
		my->z = (camera.z * .5 - players[clientnum]->entity->z) + 7;
		my->z -= 4;
		my->yaw = HANDMAGIC_YAW - camera_shakex2;
		my->pitch = defaultpitch + HANDMAGIC_PITCH - camera_shakey2 / 200.f;
		my->roll = HANDMAGIC_ROLL;
		my->focalz = -1.5;
	}

	//my->y = 3 + HUDWEAPON_MOVEY;
	//my->z = (camera.z*.5-players[clientnum]->z)+7+HUDWEAPON_MOVEZ; //TODO: NOT a PLAYERSWAP
	my->x += cast_animation.lefthand_movex;
	my->y += cast_animation.lefthand_movey;
}

void actRightHandMagic(Entity* my)
{
	if (intro == true)
	{
		my->flags[INVISIBLE] = true;
		return;
	}

	//Initialize
	if ( !HANDMAGIC_INIT )
	{
		HANDMAGIC_INIT = 1;
		my->focalz = -1.5;
	}

	if (players[clientnum] == nullptr || players[clientnum]->entity == nullptr
		|| (players[clientnum]->entity && players[clientnum]->entity->playerCreatedDeathCam != 0) )
	{
		magicRightHand = nullptr;
		list_RemoveNode(my->mynode);
		return;
	}

	my->x = 8;
	my->y = 3;
	my->z = (camera.z * .5 - players[clientnum]->entity->z) + 7;
	my->z -= 4;
	my->yaw = HANDMAGIC_YAW - camera_shakex2;
	double defaultpitch = (0 - 2.2);
	my->pitch = defaultpitch + HANDMAGIC_PITCH - camera_shakey2 / 200.f;
	my->roll = HANDMAGIC_ROLL;
	my->scalex = 0.5f;
	my->scaley = 0.5f;
	my->scalez = 0.5f;
	my->z -= 0.75;

	//Sprite
	Monster playerRace = players[clientnum]->entity->getMonsterFromPlayerRace(stats[clientnum]->playerRace);
	int playerAppearance = stats[clientnum]->appearance;
	if ( players[clientnum]->entity->effectShapeshift != NOTHING )
	{
		playerRace = static_cast<Monster>(players[clientnum]->entity->effectShapeshift);
	}
	else if ( players[clientnum]->entity->effectPolymorph != NOTHING )
	{
		if ( players[clientnum]->entity->effectPolymorph > NUMMONSTERS )
		{
			playerRace = HUMAN;
			playerAppearance = players[clientnum]->entity->effectPolymorph - 100;
		}
		else
		{
			playerRace = static_cast<Monster>(players[clientnum]->entity->effectPolymorph);
		}
	}

	bool noGloves = false;
	if ( stats[clientnum]->gloves == NULL 
		|| playerRace == SPIDER 
		|| playerRace == RAT 
		|| playerRace == CREATURE_IMP
		|| playerRace == TROLL )
	{
		noGloves = true;
	}
	else
	{
		if ( stats[clientnum]->gloves->type == GLOVES || stats[clientnum]->gloves->type == GLOVES_DEXTERITY )
		{
			my->sprite = 637;
		}
		else if ( stats[clientnum]->gloves->type == BRACERS || stats[clientnum]->gloves->type == BRACERS_CONSTITUTION )
		{
			my->sprite = 638;
		}
		else if ( stats[clientnum]->gloves->type == GAUNTLETS || stats[clientnum]->gloves->type == GAUNTLETS_STRENGTH )
		{
			my->sprite = 639;
		}
		else if ( stats[clientnum]->gloves->type == BRASS_KNUCKLES )
		{
			my->sprite = 640;
		}
		else if ( stats[clientnum]->gloves->type == IRON_KNUCKLES )
		{
			my->sprite = 641;
		}
		else if ( stats[clientnum]->gloves->type == SPIKED_GAUNTLETS )
		{
			my->sprite = 642;
		}
		else if ( stats[clientnum]->gloves->type == CRYSTAL_GLOVES )
		{
			my->sprite = 591;
		}
		else if ( stats[clientnum]->gloves->type == ARTIFACT_GLOVES )
		{
			my->sprite = 590;
		}
		else if ( stats[clientnum]->gloves->type == SUEDE_GLOVES )
		{
			my->sprite = 802;
		}
		else if (stats[clientnum]->gloves->type == ABYSSAL_KNUCKLES)
		{
			my->sprite = 1100;
		}
		else if (stats[clientnum]->gloves->type == ICE_GLOVES)
		{
			my->sprite = 1232;
		}
		else if (stats[clientnum]->gloves->type == INQUISITOR_GLOVES)
		{
			my->sprite = 1289;
		}
		else if (stats[clientnum]->gloves->type == LIFESTEAL_KNUCKLES)
		{
			my->sprite = 1319;
		}
		else if (stats[clientnum]->gloves->type == MANA_GLOVES)
		{
			my->sprite = 1322;
		}
		else if (stats[clientnum]->gloves->type == TIN_GLOVES)
		{
			my->sprite = 1334;
		}
		else if (stats[clientnum]->gloves->type == LOST_GAUNTLETS)
		{
			my->sprite = 1358;
		}
	}

	if ( noGloves )
	{
		switch ( playerRace )
		{
			case SKELETON:
				my->sprite = 774;
				break;
			case INCUBUS:
				my->sprite = 776;
				break;
			case SUCCUBUS:
				my->sprite = 778;
				break;
			case GOBLIN:
				my->sprite = 780;
				break;
			case AUTOMATON:
				my->sprite = 782;
				break;
			case INSECTOID:
				if ( stats[clientnum]->sex == FEMALE )
				{
					my->sprite = 786;
				}
				else
				{
					my->sprite = 784;
				}
				break;
			case GOATMAN:
				my->sprite = 788;
				break;
			case VAMPIRE:
				my->sprite = 790;
				break;
			case HUMAN:
				if ( playerAppearance / 6 == 0 )
				{
					my->sprite = 634;
				}
				else if ( playerAppearance / 6 == 1 )
				{
					my->sprite = 635;
				}
				else
				{
					my->sprite = 636;
				}
				break;
			case TROLL:
				my->sprite = 855;
				break;
			case SPIDER:
				my->sprite = 853;
				break;
			case CREATURE_IMP:
				my->sprite = 857;
				break;
			default:
				my->sprite = 634;
				break;
		}
		/*else if ( playerAppearance / 6 == 0 )
		{
			my->sprite = 634;
		}
		else if ( playerAppearance / 6 == 1 )
		{
			my->sprite = 635;
		}
		else
		{
			my->sprite = 636;
		}*/
	}

	if ( playerRace == RAT )
	{
		my->flags[INVISIBLE] = true;
		my->y = 0;
		my->z += 1;
	}
	if ( playerRace == SPIDER && hudarm && players[clientnum]->entity->bodyparts.at(0) )
	{
		my->x = hudarm->x;
		my->y = hudarm->y;
		//my->z = hudArm->z;
		my->pitch = hudarm->pitch;
		my->roll = hudarm->roll;
		my->yaw = players[clientnum]->entity->bodyparts.at(0)->yaw;
		my->scalex = hudarm->scalex;
		my->scaley = hudarm->scaley;
		my->scalez = hudarm->scalez;
		my->focalz = hudarm->focalz;
	}
	else
	{
		my->focalz = -1.5;
	}

	bool wearingring = false;

	//Select model
	if (stats[clientnum]->ring != NULL)
	{
		if (stats[clientnum]->ring->type == RING_INVISIBILITY)
		{
			wearingring = true;
		}
	}
	if (stats[clientnum]->cloak != NULL)
	{
		if (stats[clientnum]->cloak->type == CLOAK_INVISIBILITY)
		{
			wearingring = true;
		}
	}
	if (stats[clientnum]->mask != NULL)
	{
		if (stats[clientnum]->mask->type == ABYSSAL_MASK)
		{
			wearingring = true;
		}
	}
	if ( players[clientnum]->entity->skill[3] == 1 || players[clientnum]->entity->isInvisible() )   // debug cam or player invisible
	{
		my->flags[INVISIBLE] = true;
	}

	if ( (cast_animation.active || cast_animation.active_spellbook) )
	{
		switch (cast_animation.stage)
		{
			case CIRCLE:
				if ( ticks % 5 == 0 && !(players[clientnum]->entity->skill[3] == 1) )
				{
					//messagePlayer(0, "Pingas!");
					Entity* entity = spawnGib(my);
					entity->flags[INVISIBLE] = false;
					entity->flags[SPRITE] = true;
					entity->flags[NOUPDATE] = true;
					entity->flags[UPDATENEEDED] = false;
					entity->flags[OVERDRAW] = true;
					entity->flags[BRIGHT] = true;
					//entity->sizex = 1; //MAKE 'EM SMALL PLEASE!
					//entity->sizey = 1;
					entity->scalex = 0.25f; //MAKE 'EM SMALL PLEASE!
					entity->scaley = 0.25f;
					entity->scalez = 0.25f;
					entity->sprite = 16; //TODO: Originally. 22. 16 -- spark sprite instead?
					entity->yaw = ((rand() % 6) * 60) * PI / 180.0;
					entity->pitch = (rand() % 360) * PI / 180.0;
					entity->roll = (rand() % 360) * PI / 180.0;
					entity->vel_x = cos(entity->yaw) * .1;
					entity->vel_y = sin(entity->yaw) * .1;
					entity->vel_z = -.15;
					entity->fskill[3] = 0.01;
				}
				break;
			case THROW:
				break;
			default:
				break;
		}
	}

	//Final position code.
	if (players[clientnum] == nullptr || players[clientnum]->entity == nullptr)
	{
		return;
	}

	if ( playerRace == SPIDER && hudarm && players[clientnum]->entity->bodyparts.at(0) )
	{
		my->x = hudarm->x;
		my->y = hudarm->y;
		my->z = hudarm->z;
		my->pitch = hudarm->pitch;
		my->roll = hudarm->roll;
		my->yaw = players[clientnum]->entity->bodyparts.at(0)->yaw;
		my->scalex = hudarm->scalex;
		my->scaley = hudarm->scaley;
		my->scalez = hudarm->scalez;
		my->focalz = hudarm->focalz;
	}
	else
	{
		my->x = 8;
		my->y = 3;
		my->z = (camera.z * .5 - players[clientnum]->entity->z) + 7;
		my->z -= 4;
		my->yaw = HANDMAGIC_YAW - camera_shakex2;
		my->pitch = defaultpitch + HANDMAGIC_PITCH - camera_shakey2 / 200.f;
		my->roll = HANDMAGIC_ROLL;
		my->focalz = -1.5;
	}

	my->x += cast_animation.lefthand_movex;
	my->y -= cast_animation.lefthand_movey;
}
