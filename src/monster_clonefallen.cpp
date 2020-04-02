
#include "main.hpp"
#include "game.hpp"
#include "stat.hpp"
#include "entity.hpp"
#include "monster.hpp"
#include "sound.hpp"
#include "items.hpp"
#include "net.hpp"
#include "collision.hpp"
#include "player.hpp"
#include "magic/magic.hpp"
#include "paths.hpp"

static const int LICH_BODY = 0;
static const int LICH_RIGHTARM = 2;
static const int LICH_LEFTARM = 3;
static const int LICH_HEAD = 4;
static const int LICH_WEAPON = 5;

void initCloneFallen(Entity* my, Stat* myStats)
{
	my->initMonster(1456);

	if ( multiplayer != CLIENT )
	{
		MONSTER_SPOTSND = 657;
		MONSTER_SPOTVAR = 4;
		MONSTER_IDLESND = -1;
		MONSTER_IDLEVAR = 1;
	}
	if ( multiplayer != CLIENT && !MONSTER_INIT )
	{
		if ( myStats != nullptr )
		{
			if ( !myStats->leader_uid )
			{
				myStats->leader_uid = 0;
			}

			// apply random stat increases if set in stat_shared.cpp or editor
			setRandomMonsterStats(myStats);

			myStats->HP = myStats->MAXHP;
			myStats->OLDHP = myStats->HP;

			// generate 6 items max, less if there are any forced items from boss variants
			int customItemsToGenerate = ITEM_CUSTOM_SLOT_LIMIT;

			// boss variants

			// random effects
			myStats->EFFECTS[EFF_LEVITATING] = true;
			myStats->EFFECTS_TIMERS[EFF_LEVITATING] = 0;

			// generates equipment and weapons if available from editor
			createMonsterEquipment(myStats);

			// create any custom inventory items from editor if available
			createCustomInventory(myStats, customItemsToGenerate);

			// count if any custom inventory items from editor
			int customItems = countCustomItems(myStats); //max limit of 6 custom items per entity.

														 // count any inventory items set to default in edtior
			int defaultItems = countDefaultItems(myStats);

			my->setHardcoreStats(*myStats);

			// generate the default inventory items for the monster, provided the editor sprite allowed enough default slots
			switch ( defaultItems )
			{
				case 6:
				case 5:
				case 4:
				case 3:
				case 2:
				case 1:
				default:
					break;
			}

			//give weapon
			if ( myStats->weapon == NULL && myStats->EDITOR_ITEMS[ITEM_SLOT_WEAPON] == 1 )
			{
				myStats->weapon = newItem(SPELLBOOK_BLEED, EXCELLENT, -3, 1, rand(), true, NULL);//change to new axe/bow once made
			}
		}
	}

	// right arm
	Entity* entity = newEntity(1230, 0, map.entities, nullptr);
	entity->sizex = 4;
	entity->sizey = 4;
	entity->skill[2] = my->getUID();
	entity->flags[PASSABLE] = true;
	entity->flags[NOUPDATE] = true;
	entity->flags[USERFLAG2] = my->flags[USERFLAG2];
	entity->focalx = limbs[CLONE_FALLEN][1][0]; // 0
	entity->focaly = limbs[CLONE_FALLEN][1][1]; // 0
	entity->focalz = limbs[CLONE_FALLEN][1][2]; // 2
	entity->behavior = &actCloneFallenLimb;
	entity->parent = my->getUID();
	node_t* node = list_AddNodeLast(&my->children);
	node->element = entity;
	node->deconstructor = &emptyDeconstructor;
	node->size = sizeof(Entity*);
	my->bodyparts.push_back(entity);

	// left arm
	entity = newEntity(1229, 0, map.entities, nullptr);
	entity->sizex = 4;
	entity->sizey = 4;
	entity->skill[2] = my->getUID();
	entity->flags[PASSABLE] = true;
	entity->flags[NOUPDATE] = true;
	entity->flags[USERFLAG2] = my->flags[USERFLAG2];
	entity->focalx = limbs[CLONE_FALLEN][2][0]; // 0
	entity->focaly = limbs[CLONE_FALLEN][2][1]; // 0
	entity->focalz = limbs[CLONE_FALLEN][2][2]; // 2
	entity->behavior = &actCloneFallenLimb;
	entity->parent = my->getUID();
	node = list_AddNodeLast(&my->children);
	node->element = entity;
	node->deconstructor = &emptyDeconstructor;
	node->size = sizeof(Entity*);
	my->bodyparts.push_back(entity);

	// head
	entity = newEntity(1227, 0, map.entities, nullptr);
	entity->yaw = my->yaw;
	entity->sizex = 4;
	entity->sizey = 4;
	entity->skill[2] = my->getUID();
	entity->flags[PASSABLE] = true;
	entity->flags[NOUPDATE] = true;
	entity->flags[USERFLAG2] = my->flags[USERFLAG2];
	entity->focalx = limbs[CLONE_FALLEN][3][0]; // 0
	entity->focaly = limbs[CLONE_FALLEN][3][1]; // 0
	entity->focalz = limbs[CLONE_FALLEN][3][2]; // -2
	entity->behavior = &actCloneFallenLimb;
	entity->parent = my->getUID();
	node = list_AddNodeLast(&my->children);
	node->element = entity;
	node->deconstructor = &emptyDeconstructor;
	node->size = sizeof(Entity*);
	my->bodyparts.push_back(entity);

	// world weapon
	entity = newEntity(-1, 0, map.entities, nullptr);
	entity->sizex = 4;
	entity->sizey = 4;
	entity->skill[2] = my->getUID();
	entity->flags[PASSABLE] = true;
	entity->flags[NOUPDATE] = true;
	entity->flags[USERFLAG2] = my->flags[USERFLAG2];
	entity->focalx = limbs[CLONE_FALLEN][4][0]; // 1.5
	entity->focaly = limbs[CLONE_FALLEN][4][1]; // 0
	entity->focalz = limbs[CLONE_FALLEN][4][2]; // -.5
	entity->behavior = &actCloneFallenLimb;
	entity->parent = my->getUID();
	entity->pitch = .25;
	node = list_AddNodeLast(&my->children);
	node->element = entity;
	node->deconstructor = &emptyDeconstructor;
	node->size = sizeof(Entity*);
	my->bodyparts.push_back(entity);
}

void cloneFallenDie(Entity* my)
{
	node_t* node, *nextnode;
	int c;
	for ( c = 0; c < 5; c++ )
	{
		Entity* entity = spawnGib(my);
		if ( entity )
		{
			serverSpawnGibForClient(entity);
		}
	}
	my->removeMonsterDeathNodes();
	my->removeLightField();
	list_RemoveNode(my->mynode);
	return;
}

void actCloneFallenLimb(Entity* my)
{
	my->actMonsterLimb();
}

void cloneFallenAnimate(Entity* my, Stat* myStats, double dist)
{
	node_t* node;
	Entity* entity = nullptr, *entity2 = nullptr;
	Entity* rightbody = nullptr;
	Entity* weaponarm = nullptr;
	Entity* head = nullptr;
	Entity* spellarm = nullptr;
	int bodypart;
	bool wearingring = false;

	// remove old light field
	my->removeLightField();

	// obtain head entity
	node = list_Node(&my->children, LICH_HEAD);
	if ( node )
	{
		head = (Entity*)node->element;
	}

	// set invisibility //TODO: isInvisible()?
	if ( multiplayer != CLIENT )
	{
		if ( myStats->ring != nullptr )
			if ( myStats->ring->type == RING_INVISIBILITY )
			{
				wearingring = true;
			}
		if ( myStats->cloak != nullptr )
			if ( myStats->cloak->type == CLOAK_INVISIBILITY )
			{
				wearingring = true;
			}
		if (myStats->mask != nullptr)
		{
			if (myStats->mask->type == ABYSSAL_MASK)
			{
				wearingring = true;
			}
		}
		if ( myStats->EFFECTS[EFF_INVISIBLE] == true || wearingring == true )
		{
			my->flags[INVISIBLE] = true;
			my->flags[BLOCKSIGHT] = false;
			bodypart = 0;
			for ( node = my->children.first; node != nullptr; node = node->next )
			{
				entity = (Entity*)node->element;
				if ( !entity->flags[INVISIBLE] )
				{
					entity->flags[INVISIBLE] = true;
					serverUpdateEntityBodypart(my, bodypart);
				}
				bodypart++;
			}
		}
		else
		{
			my->flags[INVISIBLE] = false;
			my->flags[BLOCKSIGHT] = true;
			bodypart = 0;
			for ( node = my->children.first; node != nullptr; node = node->next )
			{
				if ( bodypart < LICH_RIGHTARM )
				{
					bodypart++;
					continue;
				}
				if ( bodypart >= LICH_WEAPON )
				{
					break;
				}
				entity = (Entity*)node->element;
				if ( entity->flags[INVISIBLE] )
				{
					entity->flags[INVISIBLE] = false;
					serverUpdateEntityBodypart(my, bodypart);
					serverUpdateEntityFlag(my, INVISIBLE);
				}
				bodypart++;
			}
		}

		// check tiles around the monster to be freed.
		if ( my->monsterLichBattleState == LICH_BATTLE_IMMOBILE && my->ticks > TICKS_PER_SECOND )
		{
			int sides = 0;
			int my_x = static_cast<int>(my->x) >> 4;
			int my_y = static_cast<int>(my->y) >> 4;
			int mapIndex = (my_y)* MAPLAYERS + (my_x + 1) * MAPLAYERS * map.height;
			if ( map.tiles[OBSTACLELAYER + mapIndex] )   // wall
			{
				++sides;
			}
			mapIndex = (my_y)* MAPLAYERS + (my_x - 1) * MAPLAYERS * map.height;
			if ( map.tiles[OBSTACLELAYER + mapIndex] )   // wall
			{
				++sides;
			}
			mapIndex = (my_y + 1) * MAPLAYERS + (my_x)* MAPLAYERS * map.height;
			if ( map.tiles[OBSTACLELAYER + mapIndex] )   // wall
			{
				++sides;
			}
			mapIndex = (my_y - 1) * MAPLAYERS + (my_x)* MAPLAYERS * map.height;
			if ( map.tiles[OBSTACLELAYER + mapIndex] )   // wall
			{
				++sides;
			}
			if ( sides != 4 )
			{
				my->monsterLichBattleState = LICH_BATTLE_READY;
				generatePathMaps();
				real_t distToPlayer = 0;
				int c, playerToChase = -1;
				for ( c = 0; c < MAXPLAYERS; c++ )
				{
					if ( players[c] && players[c]->entity )
					{
						if ( !distToPlayer )
						{
							distToPlayer = sqrt(pow(my->x - players[c]->entity->x, 2) + pow(my->y - players[c]->entity->y, 2));
							playerToChase = c;
						}
						else
						{
							double newDistToPlayer = sqrt(pow(my->x - players[c]->entity->x, 2) + pow(my->y - players[c]->entity->y, 2));
							if ( newDistToPlayer < distToPlayer )
							{
								distToPlayer = newDistToPlayer;
								playerToChase = c;
							}
						}
					}
				}
				if ( playerToChase >= 0 )
				{
					if ( players[playerToChase] && players[playerToChase]->entity )
					{
						my->monsterAcquireAttackTarget(*players[playerToChase]->entity, MONSTER_STATE_PATH);
					}
				}
			}
		}

		// passive floating effect, server only.
		if ( my->monsterState == MONSTER_STATE_LICHFALLEN_DIE )
		{
			my->z -= 0.03;
		}
		if ( my->monsterAttack == 0 )
		{
			if ( my->monsterAnimationLimbOvershoot == ANIMATE_OVERSHOOT_NONE )
			{
				if ( my->z < -1.2 )
				{
					my->z += 0.25;
				}
				else
				{
					my->z = -1.2;
					my->monsterAnimationLimbOvershoot = ANIMATE_OVERSHOOT_TO_SETPOINT;
				}
			}
			if ( dist < 0.1 )
			{
				// not moving, float.
				limbAnimateWithOvershoot(my, ANIMATE_Z, 0.005, -1.5, 0.005, -1.2, ANIMATE_DIR_NEGATIVE);
			}
		}
		else if ( my->monsterAttack == 1 || my->monsterAttack == 3 )
		{
			if ( my->z < -1.2 )
			{
				my->z += 0.25;
			}
			else
			{
				my->z = -1.2;
				my->monsterAnimationLimbOvershoot = ANIMATE_OVERSHOOT_NONE;
			}
		}
	}
	else
	{
	}

	if ( !my->light )
	{
		my->light = lightSphereShadow(my->x / 16, my->y / 16, 4, 192);
	}

	//Lich stares you down while he does his special ability windup, and any of his spellcasting animations.
	if ( (my->monsterAttack == MONSTER_POSE_MAGIC_WINDUP1
		|| my->monsterAttack == MONSTER_POSE_MAGIC_WINDUP2
		|| my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP1
		|| my->monsterAttack == MONSTER_POSE_MAGIC_CAST1
		|| my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP2
		|| my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP3
		|| my->monsterState == MONSTER_STATE_LICH_CASTSPELLS)
		&& my->monsterState != MONSTER_STATE_LICHFALLEN_DIE )
	{
		//Always turn to face the target.
		Entity* target = uidToEntity(my->monsterTarget);
		if ( target )
		{
			my->lookAtEntity(*target);
			my->monsterRotate();
		}
	}

	// move arms
	Entity* rightarm = nullptr;
	for ( bodypart = 0, node = my->children.first; node != NULL; node = node->next, bodypart++ )
	{
		if ( bodypart < LICH_RIGHTARM )
		{
			if ( bodypart == 0 ) // insert head/body animation here.
			{
				if ( my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP1 )
				{
					if ( multiplayer != CLIENT && my->monsterAnimationLimbOvershoot >= ANIMATE_OVERSHOOT_TO_SETPOINT )
					{
						// handle z movement on windup
						limbAnimateWithOvershoot(my, ANIMATE_Z, 0.2, -0.6, 0.1, -3.2, ANIMATE_DIR_POSITIVE); // default z is -1.2
						if ( my->z > -0.5 )
						{
							my->z = -0.6; //failsafe for floating too low sometimes?
						}
					}
				}
				else if ( my->monsterAttack == MONSTER_POSE_MELEE_WINDUP3 || my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP3 )
				{
					if ( multiplayer != CLIENT && my->monsterAnimationLimbOvershoot >= ANIMATE_OVERSHOOT_TO_SETPOINT )
					{
						// handle z movement on windup
						limbAnimateWithOvershoot(my, ANIMATE_Z, 0.3, -0.6, 0.3, -4.0, ANIMATE_DIR_POSITIVE); // default z is -1.2
						if ( my->z > -0.5 )
						{
							my->z = -0.6; //failsafe for floating too low sometimes?
						}
					}
				}
				else if ( my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP2 )
				{
					if ( multiplayer != CLIENT && my->monsterAnimationLimbOvershoot >= ANIMATE_OVERSHOOT_TO_SETPOINT )
					{
						// handle z movement on windup
						limbAnimateWithOvershoot(my, ANIMATE_Z, 0.05, -0.6, 0.1, -2.0, ANIMATE_DIR_POSITIVE); // default z is -1.2
						if ( my->z > -0.5 )
						{
							my->z = -0.6; //failsafe for floating too low sometimes?
						}
					}
				}
				else if ( my->monsterAttack == MONSTER_POSE_MAGIC_WINDUP3 )
				{
					if ( multiplayer != CLIENT && my->monsterAnimationLimbOvershoot >= ANIMATE_OVERSHOOT_TO_SETPOINT )
					{
						// handle z movement on windup
						limbAnimateWithOvershoot(my, ANIMATE_Z, 0.3, -0.6, 0.3, -4.0, ANIMATE_DIR_POSITIVE); // default z is -1.2
						if ( my->z > -0.5 )
						{
							my->z = -0.6; //failsafe for floating too low sometimes?
						}
					}
				}
				else
				{
					if ( head != nullptr )
					{
						if ( head->pitch > PI )
						{
							limbAnimateToLimit(head, ANIMATE_PITCH, 0.1, 0, false, 0.0); // return head to a neutral position.
						}
						else if ( head->pitch < PI && head->pitch > 0 )
						{
							limbAnimateToLimit(head, ANIMATE_PITCH, -0.1, 0, false, 0.0); // return head to a neutral position.
						}
					}
				}
			}
			continue;
		}
		entity = (Entity*)node->element;
		entity->x = my->x;
		entity->y = my->y;
		entity->z = my->z;
		if ( bodypart != LICH_HEAD )
		{
			// lich head turns to track player, other limbs will rotate as normal.
			if ( bodypart == LICH_LEFTARM && my->monsterAttack == MONSTER_POSE_MAGIC_WINDUP1 )
			{
				// don't rotate leftarm here during spellcast.
			}
			else
			{
				entity->yaw = my->yaw;
			}
		}
		else
		{

		}
		if ( bodypart == LICH_RIGHTARM )
		{
			// weapon holding arm.
			weaponarm = entity;
			if ( my->monsterAttack == 0 )
			{
				entity->pitch = PI / 8; // default arm pitch when not attacking.
			}
			else
			{
				// vertical chop windup
				if ( my->monsterAttack == MONSTER_POSE_MELEE_WINDUP1 )
				{
					if ( my->monsterAttackTime == 0 )
					{
						// init rotations
						my->monsterWeaponYaw = 0;
						weaponarm->roll = 0;
						weaponarm->skill[1] = 0;
					}

					limbAnimateToLimit(weaponarm, ANIMATE_PITCH, -0.3, 5 * PI / 4, false, 0.0);

					if ( my->monsterAttackTime >= ANIMATE_DURATION_WINDUP / (monsterGlobalAnimationMultiplier / 10.0) )
					{
						if ( multiplayer != CLIENT )
						{
							my->attack(1, 0, nullptr);
						}
					}
				}
				// vertical chop attack
				else if ( my->monsterAttack == 1 )
				{
					if ( weaponarm->skill[1] == 0 )
					{
						// chop forwards
						if ( limbAnimateToLimit(weaponarm, ANIMATE_PITCH, 0.4, PI / 2, false, 0.0) )
						{
							weaponarm->skill[1] = 1;
						}
					}
					else if ( weaponarm->skill[1] == 1 )
					{
						if ( limbAnimateToLimit(weaponarm, ANIMATE_PITCH, -0.4, PI / 8, false, 0.0) )
						{
							my->monsterWeaponYaw = 0;
							weaponarm->pitch = PI / 8;
							weaponarm->roll = 0;
							my->monsterAttack = 0;
						}
					}
				}
				// horizontal chop windup
				else if ( my->monsterAttack == MONSTER_POSE_MELEE_WINDUP2 )
				{
					if ( my->monsterAttackTime == 0 )
					{
						// init rotations
						weaponarm->pitch = PI / 4;
						weaponarm->roll = 0;
						my->monsterArmbended = 1; // don't actually bend the arm, we're just using this to adjust the limb offsets in the weapon code.
						weaponarm->skill[1] = 0;
						my->monsterWeaponYaw = 6 * PI / 4;
					}

					limbAnimateToLimit(weaponarm, ANIMATE_ROLL, -0.2, 3 * PI / 2, false, 0.0);
					limbAnimateToLimit(weaponarm, ANIMATE_PITCH, -0.2, 0, false, 0.0);


					if ( my->monsterAttackTime >= ANIMATE_DURATION_WINDUP / (monsterGlobalAnimationMultiplier / 10.0) )
					{
						if ( multiplayer != CLIENT )
						{
							my->attack(2, 0, nullptr);
						}
					}
				}
				// horizontal chop attack
				else if ( my->monsterAttack == 2 )
				{
					if ( weaponarm->skill[1] == 0 )
					{
						// swing
						// this->weaponyaw is OK to change for clients, as server doesn't update it for them.
						if ( limbAnimateToLimit(my, ANIMATE_WEAPON_YAW, 0.3, 2 * PI / 8, false, 0.0) )
						{
							weaponarm->skill[1] = 1;
						}
					}
					else if ( weaponarm->skill[1] == 1 )
					{
						// post-swing return to normal weapon yaw
						if ( limbAnimateToLimit(my, ANIMATE_WEAPON_YAW, -0.5, 0, false, 0.0) )
						{
							// restore pitch and roll after yaw is set
							if ( limbAnimateToLimit(weaponarm, ANIMATE_ROLL, 0.4, 0, false, 0.0)
								&& limbAnimateToLimit(weaponarm, ANIMATE_PITCH, 0.4, PI / 8, false, 0.0) )
							{
								weaponarm->skill[1] = 0;
								my->monsterWeaponYaw = 0;
								weaponarm->pitch = PI / 8;
								weaponarm->roll = 0;
								my->monsterArmbended = 0;
								my->monsterAttack = 0;
								my->monsterAttackTime = 0;
							}
						}
					}
				}
				else if ( my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP1 )
				{
					if ( my->monsterAttackTime == 0 )
					{
						// init rotations
						my->monsterWeaponYaw = 0;
						weaponarm->roll = 0;
						weaponarm->skill[1] = 0;
						createParticleDot(my);
						if ( multiplayer != CLIENT )
						{
							my->monsterAnimationLimbOvershoot = ANIMATE_OVERSHOOT_TO_SETPOINT;
							// lich can't be paralyzed, use EFF_STUNNED instead.
							myStats->EFFECTS[EFF_STUNNED] = true;
							myStats->EFFECTS_TIMERS[EFF_STUNNED] = 50;
						}
					}

					// only do the following during 2nd + end stage of overshoot animation.
					if ( my->monsterAnimationLimbOvershoot != ANIMATE_OVERSHOOT_TO_SETPOINT )
					{
						limbAnimateToLimit(head, ANIMATE_PITCH, -0.1, 11 * PI / 6, true, 0.05);
						limbAnimateToLimit(weaponarm, ANIMATE_PITCH, -0.3, 5 * PI / 4, false, 0.0);

						if ( my->monsterAttackTime >= 50 / (monsterGlobalAnimationMultiplier / 10.0) )
						{
							if ( multiplayer != CLIENT )
							{
								my->attack(1, 0, nullptr);
								real_t dir = 0.f;
								for ( int i = 0; i < 8; ++i )
								{
									my->castFallingMagicMissile(SPELL_COLD, 16 + rand() % 8, dir + i * PI / 4, 0);
								}
							}
						}
					}
				}
				else if ( my->monsterAttack == MONSTER_POSE_MELEE_WINDUP3 )
				{
					int windupDuration = 40; //(my->monsterState == MONSTER_STATE_LICHFIRE_CASTSPELLS) ? 20 : 40;
					if ( my->monsterAttackTime == 0 )
					{
						// init rotations
						my->monsterWeaponYaw = 10 * PI / 6;
						weaponarm->roll = 0;
						weaponarm->skill[1] = 0;
						createParticleDot(my);
						if ( multiplayer != CLIENT )
						{
							my->monsterAnimationLimbOvershoot = ANIMATE_OVERSHOOT_TO_SETPOINT;
							//	// lich can't be paralyzed, use EFF_STUNNED instead.
							myStats->EFFECTS[EFF_STUNNED] = true;
							myStats->EFFECTS_TIMERS[EFF_STUNNED] = windupDuration;
						}
					}

					// only do the following during 2nd + end stage of overshoot animation.
					if ( my->monsterAnimationLimbOvershoot != ANIMATE_OVERSHOOT_TO_SETPOINT )
					{
						limbAnimateToLimit(head, ANIMATE_PITCH, -0.1, 11 * PI / 6, true, 0.05);
						limbAnimateToLimit(weaponarm, ANIMATE_PITCH, -0.3, 5 * PI / 4, false, 0.0);

						if ( my->monsterAttackTime >= windupDuration / (monsterGlobalAnimationMultiplier / 10.0) )
						{
							if ( multiplayer != CLIENT )
							{
								my->attack(3, 0, nullptr);
							}
						}
					}
				}
				// vertical chop after melee3
				else if ( my->monsterAttack == 3 )
				{
					if ( weaponarm->skill[1] == 0 )
					{
						// chop forwards
						if ( limbAnimateToLimit(weaponarm, ANIMATE_PITCH, 0.4, PI / 2, false, 0.0) )
						{
							weaponarm->skill[1] = 1;
						}
						limbAnimateToLimit(my, ANIMATE_WEAPON_YAW, 0.15, 1 * PI / 6, false, 0.0); // swing across the body
					}
					else if ( weaponarm->skill[1] == 1 )
					{
						if ( limbAnimateToLimit(weaponarm, ANIMATE_PITCH, -0.25, PI / 8, false, 0.0) )
						{
							my->monsterWeaponYaw = 0;
							weaponarm->pitch = PI / 8;
							weaponarm->roll = 0;
							my->monsterAttack = 0;
						}
					}
				}
				else if ( my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP2 )
				{
					if ( my->monsterAttackTime == 0 )
					{
						// init rotations
						my->monsterWeaponYaw = 0;
						weaponarm->roll = 0;
						weaponarm->skill[1] = 0;
						createParticleDropRising(my, 593, 0.7);
						if ( multiplayer != CLIENT )
						{
							if ( my->monsterState != MONSTER_STATE_LICHFALLEN_DIE )
							{
								my->monsterAnimationLimbOvershoot = ANIMATE_OVERSHOOT_TO_SETPOINT;
								// lich can't be paralyzed, use EFF_STUNNED instead.
								myStats->EFFECTS[EFF_STUNNED] = true;
								myStats->EFFECTS_TIMERS[EFF_STUNNED] = 50;
							}
							else
							{
								myStats->EFFECTS[EFF_STUNNED] = true;
								myStats->EFFECTS_TIMERS[EFF_STUNNED] = 25;
							}
						}
					}

					limbAnimateToLimit(head, ANIMATE_PITCH, 0.3, 1 * PI / 6, true, 0.05);
					limbAnimateToLimit(my, ANIMATE_WEAPON_YAW, -0.1, 7 * PI / 4, false, 0.0);

					// only do the following during 2nd + end stage of overshoot animation.
					if ( my->monsterAnimationLimbOvershoot != ANIMATE_OVERSHOOT_TO_SETPOINT )
					{
						limbAnimateToLimit(weaponarm, ANIMATE_PITCH, -0.3, 5 * PI / 4, false, 0.0);
						if ( my->monsterAttackTime >= 50 / (monsterGlobalAnimationMultiplier / 10.0) )
						{
							if ( multiplayer != CLIENT )
							{
								if ( my->monsterState != MONSTER_STATE_LICHFALLEN_DIE )
								{
									my->attack(1, 0, nullptr);
								}
								else
								{
									my->monsterAttackTime = 25; //reset this attack time to allow successive strikes
								}

								if ( my->monsterState == MONSTER_STATE_LICHFALLEN_DIE )
								{
									int spellID = SPELL_DRAIN_SOUL;
									for ( int i = 0; i < 8; ++i )
									{
										Entity* spell = castSpell(my->getUID(), getSpellFromID(spellID), true, false);
										// do some minor variations in spell angle
										spell->yaw += i * PI / 4 + ((PI * (-4 + rand() % 9)) / 64);
										spell->vel_x = 4 * cos(spell->yaw);
										spell->vel_y = 4 * sin(spell->yaw);
										spell->skill[5] = 50; // travel time
									}
								}
								else
								{
									int spellID = SPELL_COLD;
									if ( rand() % 5 == 0 || (my->monsterLichAllyStatus == LICH_ALLY_DEAD && rand() % 2 == 0) )
									{
										spellID = SPELL_DRAIN_SOUL;
									}
									for ( int i = 0; i < 8; ++i )
									{
										Entity* spell = castSpell(my->getUID(), getSpellFromID(spellID), true, false);
										// do some minor variations in spell angle
										spell->yaw += i * PI / 4 + ((PI * (-4 + rand() % 9)) / 64);
										spell->vel_x = 4 * cos(spell->yaw);
										spell->vel_y = 4 * sin(spell->yaw);
										spell->skill[5] = 50; // travel time
									}
								}
							}
						}
					}
				}
				else if ( my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP3 )
				{
					if ( my->monsterAttackTime == 0 )
					{
						// init rotations
						my->monsterWeaponYaw = 0;
						weaponarm->roll = 0;
						weaponarm->skill[1] = 0;
						createParticleDropRising(my, 678, 1.0);
						if ( multiplayer != CLIENT )
						{
							my->monsterAnimationLimbOvershoot = ANIMATE_OVERSHOOT_TO_SETPOINT;
							// lich can't be paralyzed, use EFF_STUNNED instead.
							myStats->EFFECTS[EFF_STUNNED] = true;
							myStats->EFFECTS_TIMERS[EFF_STUNNED] = 50;
						}
					}

					limbAnimateToLimit(head, ANIMATE_PITCH, -0.3, 11 * PI / 6, false, 0.0);
					limbAnimateToLimit(my, ANIMATE_WEAPON_YAW, 0.1, 1 * PI / 4, false, 0.0);

					// only do the following during 2nd + end stage of overshoot animation.
					if ( my->monsterAnimationLimbOvershoot != ANIMATE_OVERSHOOT_TO_SETPOINT )
					{
						limbAnimateToLimit(weaponarm, ANIMATE_PITCH, -0.3, 5 * PI / 4, false, 0.0);
						if ( my->monsterAttackTime >= 50 / (monsterGlobalAnimationMultiplier / 10.0) )
						{
							if ( multiplayer != CLIENT )
							{
								my->attack(1, 0, nullptr);
								for ( int i = 0; i < 3; ++i )
								{
									Entity* spell = castSpell(my->getUID(), getSpellFromID(SPELL_MAGICMISSILE), true, false);
									real_t horizontalSpeed = 3.0;
									if ( i != 0 )
									{
										// do some minor variations in spell angle
										spell->yaw += ((PI * (-4 + rand() % 9)) / 40);
									}
									Entity* target = uidToEntity(my->monsterTarget);
									if ( target )
									{
										real_t spellDistance = sqrt(pow(spell->x - target->x, 2) + pow(spell->y - target->y, 2));
										spell->vel_z = 22.0 / (spellDistance / horizontalSpeed);
									}
									else
									{
										spell->vel_z = 2.0 - (i * 0.8);
									}
									spell->vel_x = horizontalSpeed * cos(spell->yaw);
									spell->vel_y = horizontalSpeed * sin(spell->yaw);
									spell->actmagicIsVertical = MAGIC_ISVERTICAL_XYZ;
									spell->z = -22.0;
									spell->pitch = atan2(spell->vel_z, horizontalSpeed);
								}
							}
						}
					}
				}
				else if ( my->monsterAttack == MONSTER_POSE_MAGIC_WINDUP3 )
				{
					if ( my->monsterAttackTime == 0 )
					{
						// init rotations
						my->monsterWeaponYaw = 0;
						weaponarm->roll = 0;
						weaponarm->skill[1] = 0;
						createParticleDropRising(my, 174, 0.5);
						if ( multiplayer != CLIENT )
						{
							my->monsterAnimationLimbOvershoot = ANIMATE_OVERSHOOT_TO_SETPOINT;
							// lich can't be paralyzed, use EFF_STUNNED instead.
							myStats->EFFECTS[EFF_STUNNED] = true;
							myStats->EFFECTS_TIMERS[EFF_STUNNED] = 80;
						}
					}
					else if ( my->monsterAttackTime % 20 == 0 )
					{
						createParticleDropRising(my, 174, 0.5);
					}

					limbAnimateToLimit(head, ANIMATE_PITCH, -0.05, 10 * PI / 6, true, 0.05);
					limbAnimateToLimit(weaponarm, ANIMATE_PITCH, -0.1, 5 * PI / 4, false, 0.0);
					limbAnimateToLimit(my, ANIMATE_WEAPON_YAW, 0.1, PI / 4, false, 0.0);

					if ( my->monsterAttackTime >= 50 / (monsterGlobalAnimationMultiplier / 10.0) )
					{
						if ( multiplayer != CLIENT )
						{
							my->attack(1, 0, nullptr);
						}
					}
				}
			}
		}
		else if ( bodypart == LICH_LEFTARM )
		{
			spellarm = entity;
			if ( my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP1 
				|| my->monsterAttack == MONSTER_POSE_MELEE_WINDUP3
				|| my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP2
				|| my->monsterAttack == MONSTER_POSE_SPECIAL_WINDUP3
				|| my->monsterAttack == MONSTER_POSE_MAGIC_WINDUP3 )
			{
				spellarm->pitch = weaponarm->pitch;
			}
			else if ( my->monsterAttack == MONSTER_POSE_MAGIC_WINDUP1 )
			{
				if ( my->monsterAttackTime == 0 )
				{
					// init rotations
					spellarm->roll = 0;
					spellarm->skill[1] = 0;
					spellarm->pitch = 12 * PI / 8;
					spellarm->yaw = my->yaw;
					createParticleDot(my);
					playSoundEntityLocal(my, 170, 32);
					if ( multiplayer != CLIENT )
					{
						myStats->EFFECTS[EFF_STUNNED] = true;
						myStats->EFFECTS_TIMERS[EFF_STUNNED] = 20;
					}
				}
				double animationYawSetpoint = normaliseAngle2PI(my->yaw + 1 * PI / 8);
				double animationYawEndpoint = normaliseAngle2PI(my->yaw - 1 * PI / 8);
				double armSwingRate = 0.15;
				double animationPitchSetpoint = 13 * PI / 8;
				double animationPitchEndpoint = 11 * PI / 8;

				if ( spellarm->skill[1] == 0 )
				{
					if ( limbAnimateToLimit(spellarm, ANIMATE_PITCH, armSwingRate, animationPitchSetpoint, false, 0.0) )
					{
						if ( limbAnimateToLimit(spellarm, ANIMATE_YAW, armSwingRate, animationYawSetpoint, false, 0.0) )
						{
							spellarm->skill[1] = 1;
						}
					}
				}
				else
				{
					if ( limbAnimateToLimit(spellarm, ANIMATE_PITCH, -armSwingRate, animationPitchEndpoint, false, 0.0) )
					{
						if ( limbAnimateToLimit(spellarm, ANIMATE_YAW, -armSwingRate, animationYawEndpoint, false, 0.0) )
						{
							spellarm->skill[1] = 0;
						}
					}
				}

				if ( my->monsterAttackTime >= 1 * ANIMATE_DURATION_WINDUP / (monsterGlobalAnimationMultiplier / 10.0) )
				{
					if ( multiplayer != CLIENT )
					{
						// swing the arm after we prepped the spell
						my->attack(MONSTER_POSE_MAGIC_WINDUP2, 0, nullptr);
					}
				}
			}
			// raise arm to cast spell
			else if ( my->monsterAttack == MONSTER_POSE_MAGIC_WINDUP2 )
			{
				if ( my->monsterAttackTime == 0 )
				{
					// init rotations
					spellarm->pitch = 0;
					spellarm->roll = 0;
				}
				spellarm->skill[1] = 0;

				if ( limbAnimateToLimit(spellarm, ANIMATE_PITCH, -0.3, 5 * PI / 4, false, 0.0) )
				{
					if ( multiplayer != CLIENT )
					{
						my->attack(MONSTER_POSE_MAGIC_CAST1, 0, nullptr);
					}
				}
			}
			// vertical spell attack
			else if ( my->monsterAttack == MONSTER_POSE_MAGIC_CAST1 )
			{
				if ( spellarm->skill[1] == 0 )
				{
					// chop forwards
					if ( limbAnimateToLimit(spellarm, ANIMATE_PITCH, 0.4, PI / 2, false, 0.0) )
					{
						spellarm->skill[1] = 1;
						if ( multiplayer != CLIENT )
						{
							if ( rand() % 5 == 0 )
							{
								castSpell(my->getUID(), getSpellFromID(SPELL_SLOW), true, false);
							}
							else
							{
								castSpell(my->getUID(), getSpellFromID(SPELL_DRAIN_SOUL), true, false);
							}
						}
					}
				}
				else if ( spellarm->skill[1] == 1 )
				{
					if ( limbAnimateToLimit(spellarm, ANIMATE_PITCH, -0.25, PI / 8, false, 0.0) )
					{
						spellarm->pitch = 0;
						spellarm->roll = 0;
						my->monsterAttack = 0;
					}
				}
			}
			else
			{
				entity->pitch = 0;
			}
		}
		switch ( bodypart )
		{
			// right arm
			case LICH_RIGHTARM:
				entity->x += 2.75 * cos(my->yaw + PI / 2);
				entity->y += 2.75 * sin(my->yaw + PI / 2);
				entity->z -= 3.25;
				entity->yaw += MONSTER_WEAPONYAW;
				break;
				// left arm
			case LICH_LEFTARM:
				entity->x -= 2.75 * cos(my->yaw + PI / 2);
				entity->y -= 2.75 * sin(my->yaw + PI / 2);
				entity->z -= 3.25;
				if ( !(my->monsterAttack == MONSTER_POSE_MELEE_WINDUP2
					|| my->monsterAttack == 2
					|| my->monsterAttack == MONSTER_POSE_MAGIC_WINDUP1
					|| my->monsterAttack == 3)
					)
				{
					entity->yaw -= MONSTER_WEAPONYAW;
				}
				break;
				// head
			case LICH_HEAD:
			{
				entity->z -= 8.5;//lich_fallen is 4.25;
				node_t* tempNode;
				Entity* playertotrack = NULL;
				double disttoplayer = 0.0;
				Entity* target = uidToEntity(my->monsterTarget);
				if ( target && my->monsterAttack == 0 )
				{
					entity->lookAtEntity(*target);
					entity->monsterRotate();
				}
				else
				{
					// align head as normal if attacking.
					entity->yaw = my->yaw;
				}
				break;
			}
			case LICH_WEAPON:
				// set sprites, invisibility check etc.
				if ( multiplayer != CLIENT )
				{
					if ( myStats->weapon == nullptr || myStats->EFFECTS[EFF_INVISIBLE] || wearingring ) //TODO: isInvisible()?
					{
						entity->flags[INVISIBLE] = true;
					}
					else
					{
						entity->sprite = itemModel(myStats->weapon);
						if ( itemCategory(myStats->weapon) == SPELLBOOK )
						{
							entity->flags[INVISIBLE] = true;
						}
						else
						{
							entity->flags[INVISIBLE] = false;
						}
					}
					if ( multiplayer == SERVER )
					{
						// update sprites for clients
						if ( entity->skill[10] != entity->sprite )
						{
							entity->skill[10] = entity->sprite;
							serverUpdateEntityBodypart(my, bodypart);
						}
						if ( entity->skill[11] != entity->flags[INVISIBLE] )
						{
							entity->skill[11] = entity->flags[INVISIBLE];
							serverUpdateEntityBodypart(my, bodypart);
						}
						if ( entity->getUID() % (TICKS_PER_SECOND * 10) == ticks % (TICKS_PER_SECOND * 10) )
						{
							serverUpdateEntityBodypart(my, bodypart);
						}
					}
				}
				else
				{
					if ( entity->sprite <= 0 )
					{
						entity->flags[INVISIBLE] = true;
					}
				}

				// animation
				if ( entity != nullptr )
				{
					if ( weaponarm == nullptr )
					{
						return;
					}
					entity->x = weaponarm->x;// +1.5 * cos(weaponarm->yaw);// *(my->monsterAttack == 0);
					entity->y = weaponarm->y;// +1.5 * sin(weaponarm->yaw);// * (my->monsterAttack == 0);
					entity->z = weaponarm->z;// -.5 * (my->monsterAttack == 0);
					entity->pitch = weaponarm->pitch;
					entity->yaw = weaponarm->yaw + 0.1 * (my->monsterAttack == 0);
					entity->roll = weaponarm->roll;
					if ( my->monsterAttack == 2 || my->monsterAttack == MONSTER_POSE_MELEE_WINDUP2 )
					{
						// don't boost pitch during side-swipe
					}
					else
					{
						entity->pitch += PI / 2 + 0.25;
					}

					entity->focalx = limbs[CLONE_FALLEN][4][0];
					entity->focaly = limbs[CLONE_FALLEN][4][1];
					entity->focalz = limbs[CLONE_FALLEN][4][2];
					if ( my->monsterArmbended )
					{
						// adjust focal points during side swing
						entity->focalx = limbs[CLONE_FALLEN][4][0] - 0.8;
						entity->focalz = limbs[CLONE_FALLEN][4][2] + 1;
						entity->pitch += cos(weaponarm->roll) * PI / 2;
						entity->yaw -= sin(weaponarm->roll) * PI / 2;
					}
				}
				break;
			default:
				break;
		}
	}
	if ( my->monsterAttack > 0 && my->monsterAttack <= MONSTER_POSE_MAGIC_CAST3 )
	{
		my->monsterAttackTime++;
	}
	else if ( my->monsterAttack == 0 )
	{
		my->monsterAttackTime = 0;
	}
	else
	{
		// do nothing, don't reset attacktime or increment it.
	}
}