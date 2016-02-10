/***
*
*	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
*	
*	This product contains software technology licensed from Id 
*	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc. 
*	All Rights Reserved.
*
*   This source code contains proprietary and confidential information of
*   Valve LLC and its suppliers.  Access to this code is restricted to
*   persons who have executed a written SDK license with Valve.  Any access,
*   use or distribution of this code by or to any unlicensed person is illegal.
*
****/

#include	"extdll.h"
#include	"util.h"
#include	"cbase.h"
#include	"monster.h"
#include  "animation.h"
#include	"weapons.h"
#include	"skill.h"
#include	"player.h"

extern DLL_GLOBAL BOOL		g_fXashEngine;

//=========================================================
// 
// AI UTILITY FUNCTIONS
//
//=========================================================
class CPathCorner : public CBaseToggle	// need a "wait" field
{
public:
	void Spawn( );
	void Precache( void );
	void Touch( CBaseEntity *pOther );
};

LINK_ENTITY_TO_CLASS( path_corner, CPathCorner );

//
// path_corner isn't have model, so we need restore them size here
//
void CPathCorner :: Precache( void )
{
	UTIL_SetSize( pev, Vector( -8, -8, -8 ), Vector( 8, 8, 8 ));
}

void CPathCorner :: Spawn( void )
{
	ASSERTSZ( !FStringNull( pev->targetname ), "path_corner without a targetname" );

	pev->solid = SOLID_TRIGGER;
	Precache ();
}

void CPathCorner :: Touch( CBaseEntity *pOther )
{
	if (pOther->m_hMoveTarget != this)
		return;

	// If OTHER has an enemy, this touch is incidental, ignore
	if ( pOther->m_hEnemy != NULL )
		return; // fighting, not following a path

	CQuakeMonster *pMonster = pOther->GetMonster();

	if( pMonster )
		pMonster->CornerReached ();	// ogre uses this

	pOther->m_pGoalEnt = GetNextTarget();
	pOther->m_hMoveTarget = pOther->m_pGoalEnt;

	// If "next spot" was not found (does not exist - level design error)
	if ( pOther->m_hMoveTarget == NULL)
	{
		if( pMonster )
		{
			// path is ends here
			pMonster->m_flPauseTime = 99999999;
			pMonster->MonsterIdle();
		}
	}
	else
	{
		// Turn towards the next stop in the path.
		pOther->pev->ideal_yaw = UTIL_VecToYaw ( pOther->m_pGoalEnt->pev->origin - pOther->pev->origin );
	}
}

const char *AIState[] =
{
	"Idle",
	"Walk",
	"Run",
	"Attack",
	"Pain",
	"Dead"
};

const char *AttackState[] =
{
	"None",
	"Straight",
	"Sliding",
	"Melee",
	"Missile"
};

TYPEDESCRIPTION CQuakeMonster :: m_SaveData[] = 
{
	DEFINE_FIELD( CQuakeMonster, m_iAIState, FIELD_INTEGER ),
	DEFINE_FIELD( CQuakeMonster, m_iAttackState, FIELD_INTEGER ),
	DEFINE_FIELD( CQuakeMonster, m_Activity, FIELD_INTEGER ),
	DEFINE_FIELD( CQuakeMonster, m_IdealActivity, FIELD_INTEGER ),
	DEFINE_FIELD( CQuakeMonster, m_flMonsterSpeed, FIELD_FLOAT ),
	DEFINE_FIELD( CQuakeMonster, m_flMoveDistance, FIELD_FLOAT ),
	DEFINE_FIELD( CQuakeMonster, m_fLeftY, FIELD_BOOLEAN ),
	DEFINE_FIELD( CQuakeMonster, m_fEnemyInFront, FIELD_BOOLEAN ),
	DEFINE_FIELD( CQuakeMonster, m_fEnemyVisible, FIELD_BOOLEAN ),
	DEFINE_FIELD( CQuakeMonster, m_hSightEntity, FIELD_EHANDLE ),
	DEFINE_FIELD( CQuakeMonster, m_iRefireCount, FIELD_INTEGER ),
	DEFINE_FIELD( CQuakeMonster, m_flEnemyYaw, FIELD_FLOAT ),
	DEFINE_FIELD( CQuakeMonster, m_iEnemyRange, FIELD_INTEGER ),
	DEFINE_FIELD( CQuakeMonster, m_flSearchTime, FIELD_TIME ),
	DEFINE_FIELD( CQuakeMonster, m_flPauseTime, FIELD_TIME ),
	DEFINE_FIELD( CQuakeMonster, m_flSightTime, FIELD_TIME ),
}; IMPLEMENT_SAVERESTORE( CQuakeMonster, CBaseAnimating );

/*
======================
ReportAIState

======================
*/
void CQuakeMonster :: ReportAIState( void )
{
	ALERT( at_console, "AIState [%s], AttackState [%s]\n", AIState[m_iAIState], AttackState[m_iAttackState] );
}

/*
======================
CloseEnough

======================
*/
BOOL CQuakeMonster :: CloseEnough( float flDist )
{
	if (!m_pGoalEnt)
		return FALSE;

	for (int i = 0; i < 3; i++)
	{
		if (m_pGoalEnt->pev->absmin[i] > pev->absmax[i] + flDist)
			return FALSE;
		if (m_pGoalEnt->pev->absmax[i] < pev->absmin[i] - flDist)
			return FALSE;
	}
	return TRUE;
}

BOOL CQuakeMonster :: WalkMove( float flYaw, float flDist )
{
	return SV_WalkMove( this, flYaw, flDist );
}

void CQuakeMonster :: MoveToGoal( float flDist )
{
	SV_MoveToGoal( this, flDist );
}

//=========================================================
// SetEyePosition
//
// queries the monster's model for $eyeposition and copies
// that vector to the monster's view_ofs
//
//=========================================================
void CQuakeMonster :: SetEyePosition( void )
{
	Vector  vecEyePosition;
	void	*pmodel = GET_MODEL_PTR( ENT(pev) );

	GetEyePosition( pmodel, vecEyePosition );

	pev->view_ofs = vecEyePosition;

	if ( pev->view_ofs == g_vecZero )
	{
		pev->view_ofs = Vector( 0, 0, 25 );
	}
}

BOOL CQuakeMonster::ShouldGibMonster( int iGib )
{
	if ( ( iGib == GIB_NORMAL && pev->health < GIB_HEALTH_VALUE ) || ( iGib == GIB_ALWAYS ) )
		return TRUE;
	
	return FALSE;
}

//=========================================================
// SetActivity 
//=========================================================
void CQuakeMonster :: SetActivity( Activity NewActivity )
{
	int	iSequence;

	iSequence = LookupActivity ( NewActivity );

	// Set to the desired anim, or default anim if the desired is not present
	if ( iSequence > ACTIVITY_NOT_AVAILABLE )
	{
		if ( pev->sequence != iSequence || !m_fSequenceLoops )
		{
			// don't reset frame between walk and run
			if ( !(m_Activity == ACT_WALK || m_Activity == ACT_RUN) || !(NewActivity == ACT_WALK || NewActivity == ACT_RUN))
				pev->frame = 0;
		}

		pev->sequence = iSequence;	// Set to the reset anim (if it's there)
		ResetSequenceInfo( );
	}
	else
	{
		// Not available try to get default anim
		ALERT ( at_aiconsole, "%s has no sequence for act:%s\n", STRING(pev->classname), activity_map[NewActivity].name );
		pev->sequence = 0;	// Set to the reset anim (if it's there)
	}

	m_Activity = NewActivity; // Go ahead and set this so it doesn't keep trying when the anim is not present
	
	// In case someone calls this with something other than the ideal activity
	m_IdealActivity = m_Activity;
}

/*
===========
FindTarget

Self is currently not attacking anything, so try to find a target

Returns TRUE if an enemy was sighted

When a player fires a missile, the point of impact becomes a fakeplayer so
that monsters that see the impact will respond as if they had seen the
player.

To avoid spending too much time, only a single client (or fakeclient) is
checked each frame.  This means multi player games will have slightly
slower noticing monsters.
============
*/
BOOL CQuakeMonster :: FindTarget( void )
{
	CBaseEntity *pTarget;
	RANGETYPE	range;

	// if the first spawnflag bit is set, the monster will only wake up on
	// really seeing the player, not another monster getting angry

	// spawnflags & 3 is a big hack, because zombie crucified used the first
	// spawn flag prior to the ambush flag, and I forgot about it, so the second
	// spawn flag works as well
	if (m_flSightTime >= (gpGlobals->time - 0.1f) && !FBitSet(pev->spawnflags, 3) )
	{
		pTarget = m_hSightEntity;
		if (FNullEnt( pTarget ) || ((CBaseEntity *)pTarget->m_hEnemy) == ((CBaseEntity *)m_hEnemy))
			return FALSE;
	}
	else
	{
		pTarget = UTIL_FindClientInPVS( ENT(pev) );
		if (FNullEnt( pTarget ))
			return FALSE; // current check entity isn't in PVS
	}

	if (pTarget->pev->health <= 0)
		return FALSE; // g-cont. target is died

	if (pTarget == m_hEnemy)
		return FALSE;

	if (pTarget->pev->flags & FL_NOTARGET)
		return FALSE;

	if (pTarget->m_iItems & IT_INVISIBILITY)
		return FALSE;

	range = TargetRange (pTarget);
	if (range == RANGE_FAR)
		return FALSE;
		
	if (!TargetVisible (pTarget))
		return FALSE;

	if (range == RANGE_NEAR)
	{
		if (pTarget->m_flShowHostile < gpGlobals->time && !InFront(pTarget) )
			return FALSE;
	}
	else if (range == RANGE_MID)
	{
		if ( !InFront(pTarget))
			return FALSE;
	}
	
	// got one
	m_hEnemy = pTarget;

	if (!FClassnameIs( m_hEnemy->pev, "player"))
	{
		m_hEnemy = m_hEnemy->m_hEnemy;
		if (m_hEnemy == NULL || !FClassnameIs( m_hEnemy->pev, "player"))
		{
			m_flPauseTime = gpGlobals->time + 3.0f;
			m_pGoalEnt = m_hMoveTarget;	// restore last path_corner (if present)
			m_hEnemy = NULL;
			return FALSE;
		}
	}

	FoundTarget ();

	return TRUE;
}

void CQuakeMonster :: FoundTarget( void )
{
	if (m_hEnemy != NULL && FClassnameIs( m_hEnemy->pev, "player"))
	{	
		// let other monsters see this monster for a while
		m_hSightEntity = this;
		m_flSightTime = gpGlobals->time;
	}
	
	m_flShowHostile = gpGlobals->time + 1.0; // wake up other monsters

	MonsterSight ();	// monster see enemy!
	HuntTarget ();
}

void CQuakeMonster :: HuntTarget ( void )
{
	m_pGoalEnt = m_hEnemy;
	m_iAIState = STATE_RUN;

	pev->ideal_yaw = UTIL_VecToYaw (m_hEnemy->pev->origin - pev->origin);

	SetThink( &CQuakeMonster::MonsterThink );
	pev->nextthink = gpGlobals->time + 0.1;

	MonsterRun();	// change activity
	AttackFinished (1);	// wait a while before first attack
}

/*
=============
InFront

returns 1 if the entity is in front (in sight) of self
=============
*/
BOOL CQuakeMonster :: InFront( CBaseEntity *pTarg )
{
	UTIL_MakeVectors (pev->angles);
	Vector dir = (pTarg->pev->origin - pev->origin).Normalize();

	float flDot = DotProduct(dir, gpGlobals->v_forward);

	if (flDot > 0.3)
	{
		return TRUE;
	}
	return FALSE;
}

/*
=============
TargetRange

returns the range catagorization of an entity reletive to self
0	melee range, will become hostile even if back is turned
1	visibility and infront, or visibility and show hostile
2	infront and show hostile
3	only triggered by damage
=============
*/
RANGETYPE CQuakeMonster :: TargetRange( CBaseEntity *pTarg )
{
	Vector spot1 = EyePosition();
	Vector spot2 = pTarg->EyePosition();
	
	float dist = (spot1 - spot2).Length();
	if (dist < 120)
		return RANGE_MELEE;
	if (dist < 500)
		return RANGE_NEAR;
	if (dist < 1000)
		return RANGE_MID;
	return RANGE_FAR;
}

/*
=============
TargetVisible

returns 1 if the entity is visible to self, even if not InFront ()
=============
*/
BOOL CQuakeMonster :: TargetVisible( CBaseEntity *pTarg )
{
	TraceResult tr;

	Vector spot1 = EyePosition();
	Vector spot2 = pTarg->EyePosition();

	// see through other monsters
	UTIL_TraceLine(spot1, spot2, ignore_monsters, ignore_glass, ENT(pev), &tr);
	
	if (tr.fInOpen && tr.fInWater)
		return FALSE;		// sight line crossed contents

	if (tr.flFraction == 1.0)
		return TRUE;
	return FALSE;
}

/*
============
FacingIdeal

============
*/
BOOL CQuakeMonster :: FacingIdeal( void )
{
	float delta = UTIL_AngleMod(pev->angles.y - pev->ideal_yaw);

	if (delta > 45 && delta < 315)
		return FALSE;
	return TRUE;
}

void CQuakeMonster :: MonsterSight( void )
{
}

void CQuakeMonster :: MonsterPain( CBaseEntity *pAttacker, float flDamage )
{
	m_iAIState = STATE_PAIN;
	SetActivity( ACT_BIG_FLINCH );
	m_flMonsterSpeed = 0;
}

void CQuakeMonster :: MonsterAttack( void )
{
	// default relationship
	AI_Face();
}

/*

in nightmare mode, all attack_finished times become 0
some monsters refire twice automatically

*/
void CQuakeMonster :: AttackFinished( float flFinishTime )
{
	m_iRefireCount = 0; // refire count for nightmare

	if (g_iSkillLevel != SKILL_NIGHTMARE)
		m_flAttackFinished = gpGlobals->time + flFinishTime;
}

BOOL CQuakeMonster :: CheckRefire( void )
{
	if( g_iSkillLevel != SKILL_NIGHTMARE )
		return FALSE;

	if( m_iRefireCount )
		return FALSE;

	if( !TargetVisible(m_hEnemy))
		return FALSE;

	m_iRefireCount++;
	return TRUE;
}

BOOL CQuakeMonster :: MonsterCheckAttack( void )
{
	Vector spot1, spot2;
	CBaseEntity *pTarg;
	float chance;

	pTarg = m_hEnemy;

	// see if any entities are in the way of the shot
	spot1 = EyePosition();
	spot2 = pTarg->EyePosition();

	TraceResult tr;
	UTIL_TraceLine(spot1, spot2, dont_ignore_monsters, dont_ignore_glass, ENT(pev), &tr);

	if (tr.pHit != pTarg->edict())
		return FALSE;		// don't have a clear shot
			
	if (tr.fInOpen && tr.fInWater)
		return FALSE;		// sight line crossed contents

	if (m_iEnemyRange == RANGE_MELEE)
	{	
		// melee attack
		if (MonsterHasMeleeAttack())
		{
			MonsterMeleeAttack();
			return TRUE;
		}
	}
	
	// missile attack
	if (!MonsterHasMissileAttack())
		return FALSE;
		
	if (gpGlobals->time < m_flAttackFinished)
		return FALSE;
		
	if (m_iEnemyRange == RANGE_FAR)
		return FALSE;
		
	if (m_iEnemyRange == RANGE_MELEE)
	{
		chance = 0.9;
		m_flAttackFinished = 0;
	}
	else if (m_iEnemyRange == RANGE_NEAR)
	{
		if (MonsterHasMeleeAttack())
			chance = 0.2;
		else
			chance = 0.4;
	}
	else if (m_iEnemyRange == RANGE_MID)
	{
		if (MonsterHasMeleeAttack())
			chance = 0.05;
		else
			chance = 0.1;
	}
	else
		chance = 0;

	if (RANDOM_FLOAT(0, 1) < chance)
	{
		MonsterMissileAttack();
		AttackFinished (RANDOM_FLOAT(0, 2));
		return TRUE;
	}

	return FALSE;
}

BOOL CQuakeMonster :: MonsterCheckAnyAttack( void )
{
	if (!m_fEnemyVisible)
		return FALSE;
	return MonsterCheckAttack();
}

void CQuakeMonster :: MonsterUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if (m_hEnemy != NULL)
		return;

	if (pev->health <= 0)
		return;

	if (pActivator->m_iItems & IT_INVISIBILITY)
		return;

	if (pActivator->pev->flags & FL_NOTARGET)
		return;

	if (!FClassnameIs( pActivator->pev, "player"))
		return;
	
	// delay reaction so if the monster is teleported,
	// its sound is still heard
	m_hEnemy = pActivator;

	SetThink( &CQuakeMonster::FoundTarget );
	pev->nextthink = gpGlobals->time + 0.1;
}

void CQuakeMonster :: MonsterDeathUse( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	// fall to ground
	pev->flags &= ~(FL_FLY|FL_SWIM);

	if (FStringNull(pev->target))
		return;

	SUB_UseTargets ( pActivator, useType, value );
}

void CQuakeMonster :: MonsterThink( void )
{
	pev->nextthink = gpGlobals->time + 0.1;
	Vector oldorigin = pev->origin;

	// NOTE: keep an constant interval to make sure what all events works as predicted
	float flInterval = StudioFrameAdvance( 0.099f ); // animate

	if ( m_iAIState != STATE_DEAD && m_fSequenceFinished )
	{
		ResetSequenceInfo( );

		if( m_iAIState == STATE_PAIN )
			MonsterRun(); // change activity
	}

	DispatchAnimEvents( flInterval );

	if( m_iAIState == STATE_IDLE )
	{
		AI_Idle();
	}
	else if( m_iAIState == STATE_WALK )
	{
		AI_Walk( m_flMonsterSpeed );
	}
	else if( m_iAIState == STATE_ATTACK )
	{
		MonsterAttack();
	}
	else if( m_iAIState == STATE_RUN )
	{
		AI_Run( m_flMonsterSpeed );
	}

	// strange bug on a e4m8 - some monsters can't through trigger_teleport
	if( pev->origin != oldorigin && g_fXashEngine )
         		LINK_ENTITY( ENT( pev ), true );
}

int CQuakeMonster :: TakeDamage( entvars_t* pevInflictor, entvars_t* pevAttacker, float flDamage, int bitsDamageType )
{
	if (!pev->takedamage)
		return 0;

	CBaseEntity *pAttacker = CBaseEntity::Instance( pevAttacker );
	CBaseEntity *pInflictor = CBaseEntity::Instance( pevInflictor );

	// check for quad damage powerup on the attacker
	if( pAttacker->IsPlayer( ))
	{
		CBasePlayer *pPlayer = (CBasePlayer *)pAttacker;

		if( pPlayer->m_flSuperDamageFinished > gpGlobals->time )
			flDamage *= 4; // QUAD DAMAGE!!!
	}

	// save damage based on the target's armor level
	float dmg_save = ceil( pev->armortype * flDamage );

	if (dmg_save >= pev->armorvalue)
	{
		dmg_save = pev->armorvalue;
		pev->armortype = 0;	// lost all armor
		m_iItems &= ~(IT_ARMOR1|IT_ARMOR2|IT_ARMOR3);
	}
	
	pev->armorvalue -= dmg_save;
	float dmg_take = ceil(flDamage - dmg_save);

	// add to the damage total for clients, which will be sent as a single
	// message at the end of the frame
	// FIXME: remove after combining shotgun blasts?
	if( pev->flags & FL_CLIENT )
	{
		pev->dmg_take += dmg_take;
		pev->dmg_save += dmg_save;
		pev->dmg_inflictor = ENT( pevInflictor );
	}

	// figure momentum add
	if ( (pInflictor != gpWorld) && (pev->movetype == MOVETYPE_WALK) )
	{
		Vector dir = (pev->origin - pInflictor->Center()).Normalize();
		pev->velocity += dir * flDamage * 8;
	}

	// check for godmode or invincibility
	if (pev->flags & FL_GODMODE)
		return 0;

	if( IsPlayer( ))
	{
		CBasePlayer *pPlayer = (CBasePlayer *)this;

		if (pPlayer->m_flInvincibleFinished >= gpGlobals->time)
		{
			if (pPlayer->m_flInvincibleSound < gpGlobals->time)
			{
				EMIT_SOUND( edict(), CHAN_ITEM, "items/protect3.wav", 1, ATTN_NORM );
				pPlayer->m_flInvincibleSound = gpGlobals->time + 2;
			}
			return 0;
		}
	}

	// team play damage avoidance
	if ( (gpGlobals->teamplay == 1) && (pev->team > 0) && (pev->team == pevAttacker->team) )
		return 0;

	// do the damage
	pev->health -= dmg_take;

	if (pev->health <= 0)
	{
		Killed( pevAttacker, GIB_NORMAL );
		return 0;
	}

	if( FBitSet( pev->flags, FL_MONSTER ) && pAttacker != gpWorld )
	{
		// get mad unless of the same class (except for soldiers)
		if( this != pAttacker && pAttacker != m_hEnemy )
		{
			if( !FStrEq( STRING(pev->classname), STRING(pevAttacker->classname)) || FClassnameIs(pev, "monster_army"))
			{
				if (m_hEnemy != NULL && FClassnameIs(m_hEnemy->pev, "player"))
					m_hOldEnemy = m_hEnemy;
				m_hEnemy = pAttacker;
				FoundTarget ();
			}
		}
	}

	if( MonsterHasPain( ))
	{
		MonsterPain( pAttacker, dmg_take );

		// nightmare mode monsters don't go into pain frames often
		if (g_iSkillLevel == SKILL_NIGHTMARE)
			pev->pain_finished = gpGlobals->time + 5.0;		
	}

	return 1;
}

//=========================================================
// TraceAttack
//=========================================================
void CQuakeMonster :: TraceAttack( entvars_t *pevAttacker, float flDamage, Vector vecDir, TraceResult *ptr, int bitsDamageType )
{
	Vector vecOrigin = ptr->vecEndPos - vecDir * 4;

	if( pev->takedamage )
	{
		AddMultiDamage( pevAttacker, this, flDamage, bitsDamageType );

		int blood = BloodColor();
		
		if ( blood != DONT_BLEED )
		{
			SpawnBlood( vecOrigin, blood, flDamage );// a little surface blood.
			TraceBleed( flDamage, vecDir, ptr, bitsDamageType );
		}
	}
	else
	{
		MESSAGE_BEGIN( MSG_BROADCAST, gmsgTempEntity );
			WRITE_BYTE( TE_GUNSHOT );
			WRITE_COORD( vecOrigin.x );
			WRITE_COORD( vecOrigin.y );
			WRITE_COORD( vecOrigin.z );
		MESSAGE_END();
	}
}

void CQuakeMonster :: Killed( entvars_t *pevAttacker, int iGib )
{
	MonsterKilled( pevAttacker, iGib );
	SetActivity( ACT_DIESIMPLE );
	m_iAIState = STATE_DEAD;
	pev->takedamage = DAMAGE_NO;
	pev->deadflag = DEAD_DEAD;
	pev->solid = SOLID_NOT;

	gpWorld->killed_monsters++;

	if( m_hEnemy == NULL )
		m_hEnemy = CBaseEntity::Instance( pevAttacker );
	MonsterDeathUse( m_hEnemy, this, USE_TOGGLE, 0.0f );

	// just an event to increase internal client counter
	MESSAGE_BEGIN( MSG_ALL, gmsgKilledMonster );
	MESSAGE_END();
}

void CQuakeMonster :: AI_Forward( float flDist )
{
	WalkMove( pev->angles.y, flDist );
}

void CQuakeMonster :: AI_Backward( float flDist )
{
	WalkMove( pev->angles.y + 180.0f, flDist );
}

void CQuakeMonster :: AI_Pain( float flDist )
{
	AI_Backward( flDist );
}

void CQuakeMonster :: AI_PainForward( float flDist )
{
	WalkMove( pev->ideal_yaw, flDist );
}

void CQuakeMonster :: AI_Walk( float flDist )
{
	m_flMoveDistance = flDist;
	
	// check for noticing a player
	if (FindTarget ())
		return;

	MoveToGoal( flDist );
}

void CQuakeMonster :: AI_Run( float flDist )
{
	Vector	delta;

	m_flMoveDistance = flDist;

	// see if the enemy is dead
	if (m_hEnemy == NULL || m_hEnemy->pev->health <= 0)
	{
		m_hEnemy = NULL;

		// FIXME: look all around for other targets
		if (m_hOldEnemy != NULL && m_hOldEnemy->pev->health > 0)
		{
			m_hEnemy = m_hOldEnemy;
			HuntTarget ();
		}
		else
		{
			if (m_hMoveTarget)
			{
				// g-cont. stay over defeated player a few seconds
				// then continue patrol (if present)
				m_flPauseTime = gpGlobals->time + 5.0f;
				m_pGoalEnt = m_hMoveTarget;
			}

			MonsterIdle();
			return;
		}
	}

	m_flShowHostile = gpGlobals->time + 1.0; // wake up other monsters

	// check knowledge of enemy
	m_fEnemyVisible = TargetVisible(m_hEnemy);

	if (m_fEnemyVisible)
		m_flSearchTime = gpGlobals->time + 5.0;

	// look for other coop players
	if (gpGlobals->coop && m_flSearchTime < gpGlobals->time)
	{
		if (FindTarget ())
			return;
	}

	m_fEnemyInFront = InFront(m_hEnemy);
	m_iEnemyRange = TargetRange(m_hEnemy);
	m_flEnemyYaw = UTIL_VecToYaw(m_hEnemy->pev->origin - pev->origin);
	
	if (m_iAttackState == ATTACK_MISSILE)
	{
		AI_Run_Missile();
		return;
	}
	if (m_iAttackState == ATTACK_MELEE)
	{
		AI_Run_Melee();
		return;
	}

	if (MonsterCheckAnyAttack ())
		return;					// beginning an attack
		
	if (m_iAttackState == ATTACK_SLIDING)
	{
		AI_Run_Slide();
		return;
	}
		
	// head straight in
	MoveToGoal (flDist);	// done in C code...
}

void CQuakeMonster :: AI_Idle( void )
{
	if (FindTarget ())
		return;
	
	if (gpGlobals->time > m_flPauseTime)
	{
		MonsterWalk();
		return;
	}

	// change angle slightly
}

/*
=============
ai_turn

don't move, but turn towards ideal_yaw
=============
*/
void CQuakeMonster :: AI_Turn( void )
{
	if (FindTarget ())
		return;

	CHANGE_YAW( ENT(pev) );
}

void CQuakeMonster :: AI_Run_Melee( void )
{
	pev->ideal_yaw = m_flEnemyYaw;
	CHANGE_YAW( ENT(pev) );

	if (FacingIdeal())
	{
		MonsterMeleeAttack();
		m_iAttackState = ATTACK_STRAIGHT;
	}
}

void CQuakeMonster :: AI_Run_Missile( void )
{
	pev->ideal_yaw = m_flEnemyYaw;
	CHANGE_YAW( ENT(pev) );

	if (FacingIdeal())
	{
		MonsterMissileAttack();
		m_iAttackState = ATTACK_STRAIGHT;
	}
}

void CQuakeMonster :: AI_Run_Slide( void )
{
	float ofs;
	
	pev->ideal_yaw = m_flEnemyYaw;
	CHANGE_YAW( ENT(pev) );

	if (m_fLeftY)
		ofs = 90;
	else
		ofs = -90;
	
	if( WalkMove( pev->ideal_yaw + ofs, m_flMoveDistance ))
		return;
		
	m_fLeftY = !m_fLeftY;
	
	WalkMove( pev->ideal_yaw - ofs, m_flMoveDistance );
}

void CQuakeMonster :: AI_Face( void )
{
	if (m_hEnemy != NULL)
		pev->ideal_yaw = UTIL_VecToYaw(m_hEnemy->pev->origin - pev->origin);
	CHANGE_YAW( ENT(pev) );
}

/*
=============
ai_charge

The monster is in a melee attack, so get as close as possible to .enemy
=============
*/
void CQuakeMonster :: AI_Charge( float flDist )
{
	AI_Face ();
	MoveToGoal (flDist);
}

void CQuakeMonster :: AI_Charge_Side( void )
{
	float heading;

	if (m_hEnemy == NULL)
		return; // removed before stroke
	
	// aim to the left of the enemy for a flyby
	AI_Face ();

	UTIL_MakeVectors (pev->angles);
	Vector dtemp = m_hEnemy->pev->origin - gpGlobals->v_right * 30;
	heading = UTIL_VecToYaw(dtemp - pev->origin);
	
	WalkMove( heading, 20 );
}

/*
=============
ai_melee

=============
*/
void CQuakeMonster :: AI_Melee( void )
{
	Vector	delta;
	float 	ldmg;

	if (m_hEnemy == NULL)
		return; // removed before stroke
		
	delta = m_hEnemy->pev->origin - pev->origin;

	if (delta.Length() > 60)
		return;
		
	ldmg = (RANDOM_FLOAT(0,3) + RANDOM_FLOAT(0,3) + RANDOM_FLOAT(0,3));
	m_hEnemy->TakeDamage (pev, pev, ldmg, DMG_GENERIC);
}

void CQuakeMonster :: AI_Melee_Side( void )
{
	Vector	delta;
	float 	ldmg;

	if (m_hEnemy == NULL)
		return; // removed before stroke
		
	AI_Charge_Side();

	delta = m_hEnemy->pev->origin - pev->origin;

	if (delta.Length() > 60)
		return;

	if (!Q_CanDamage (m_hEnemy, this))
		return;

	ldmg = (RANDOM_FLOAT(0,3) + RANDOM_FLOAT(0,3) + RANDOM_FLOAT(0,3));
	m_hEnemy->TakeDamage (pev, pev, ldmg, DMG_GENERIC);
}

void CQuakeMonster :: FlyMonsterInitThink( void )
{
	pev->takedamage = DAMAGE_AIM;
	pev->ideal_yaw = pev->angles.y;

	if (!pev->yaw_speed)
		pev->yaw_speed = 10;

	SetEyePosition();
	SetUse( &CQuakeMonster::MonsterUse );

	pev->flags |= (FL_FLY|FL_MONSTER);

	if (!WalkMove( 0, 0 ))
	{
		ALERT(at_error, "Monster %s stuck in wall--level design error\n", STRING(pev->classname));
		pev->effects = EF_BRIGHTFIELD;
	}

	if (!FStringNull(pev->target))
	{
		m_pGoalEnt = GetNextTarget();
		m_hMoveTarget = m_pGoalEnt;

		if (!m_pGoalEnt)
		{
			ALERT(at_error, "MonsterInit()--%s couldn't find target %s", STRING(pev->classname), STRING(pev->target));
		}

		// this used to be an objerror
		if (!FNullEnt(m_pGoalEnt) && FClassnameIs( m_pGoalEnt->pev, "path_corner"))
		{
			MonsterWalk();
		}
		else
		{
			m_flPauseTime = 99999999;
			MonsterIdle();
		}
	}
	else
	{
		m_flPauseTime = 99999999;
		MonsterIdle();
	}

	// run AI for monster
	SetThink( &CQuakeMonster::MonsterThink );
	MonsterThink();
}

void CQuakeMonster :: FlyMonsterInit( void )
{
	// spread think times so they don't all happen at same time
	pev->nextthink += RANDOM_FLOAT( 0.1, 0.4 );
	SetThink( &CQuakeMonster::FlyMonsterInitThink );

	// add one monster to stat
	gpWorld->total_monsters++;
}

void CQuakeMonster :: WalkMonsterInitThink( void )
{
	pev->origin.z += 1;
	UTIL_DropToFloor ( this );
#if 0
	// Try to move the monster to make sure it's not stuck in a brush.
	if (!WALK_MOVE ( ENT(pev), 0, 0, WALKMOVE_NORMAL ) )
	{
		ALERT(at_error, "Monster %s stuck in wall--level design error\n", STRING(pev->classname));
		pev->effects = EF_BRIGHTFIELD;
	}
#endif
	pev->takedamage = DAMAGE_AIM;
	pev->ideal_yaw = pev->angles.y;

	if (!pev->yaw_speed)
		pev->yaw_speed = 20;

	SetEyePosition();
	SetUse( &CQuakeMonster::MonsterUse );

	pev->flags |= FL_MONSTER;

	if (!FStringNull(pev->target))
	{
		m_pGoalEnt = GetNextTarget();
		m_hMoveTarget = m_pGoalEnt;

		if (!m_pGoalEnt)
		{
			ALERT(at_error, "MonsterInit()--%s couldn't find target %s", STRING(pev->classname), STRING(pev->target));
		}
		else
		{
			pev->ideal_yaw = UTIL_VecToYaw( m_pGoalEnt->pev->origin - pev->origin );
		}

		// this used to be an objerror
		if (!FNullEnt(m_pGoalEnt) && FClassnameIs( m_pGoalEnt->pev, "path_corner"))
		{
			MonsterWalk();
		}
		else
		{
			m_flPauseTime = 99999999;
			MonsterIdle();
		}
	}
	else
	{
		m_flPauseTime = 99999999;
		MonsterIdle();
	}

	// run AI for monster
	SetThink( &CQuakeMonster::MonsterThink );
	MonsterThink();
}

void CQuakeMonster :: WalkMonsterInit( void )
{
	// spread think times so they don't all happen at same time
	pev->nextthink += RANDOM_FLOAT( 0.1, 0.4 );
	SetThink( &CQuakeMonster::WalkMonsterInitThink );

	// add one monster to stat
	gpWorld->total_monsters++;
}

void CQuakeMonster :: SwimMonsterInitThink( void )
{
	pev->takedamage = DAMAGE_AIM;
	pev->ideal_yaw = pev->angles.y;

	if (!pev->yaw_speed)
		pev->yaw_speed = 10;

	SetEyePosition();
	SetUse( &CQuakeMonster::MonsterUse );

	pev->flags |= (FL_MONSTER|FL_SWIM);

	if (!FStringNull(pev->target))
	{
		m_pGoalEnt = GetNextTarget();
		m_hMoveTarget = m_pGoalEnt;

		if (!m_pGoalEnt)
		{
			ALERT(at_error, "MonsterInit()--%s couldn't find target %s", STRING(pev->classname), STRING(pev->target));
		}
		else
		{
			pev->ideal_yaw = UTIL_VecToYaw( m_pGoalEnt->pev->origin - pev->origin );
		}

		// this used to be an objerror
		if (!FNullEnt(m_pGoalEnt) && FClassnameIs( m_pGoalEnt->pev, "path_corner"))
		{
			MonsterWalk();
		}
		else
		{
			m_flPauseTime = 99999999;
			MonsterIdle();
		}
	}
	else
	{
		m_flPauseTime = 99999999;
		MonsterIdle();
	}

	// run AI for monster
	SetThink( &CQuakeMonster::MonsterThink );
	MonsterThink();
}

void CQuakeMonster :: SwimMonsterInit( void )
{
	// spread think times so they don't all happen at same time
	pev->nextthink += RANDOM_FLOAT( 0.1, 0.4 );
	SetThink( &CQuakeMonster::SwimMonsterInitThink );

	// add one monster to stat
	gpWorld->total_monsters++;
}
