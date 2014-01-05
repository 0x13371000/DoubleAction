#include "cbase.h"

#include "da_bulletmanager.h"

#include "engine/ivdebugoverlay.h"
#include "effect_dispatch_data.h"
#include "ammodef.h"

#include "sdk_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CBulletManager g_BulletManager( "CBulletManager" );

CBulletManager& BulletManager()
{
	return g_BulletManager;
}

CBulletManager::CBulletManager( char const* name )
	: CAutoGameSystemPerFrame(name)
{
}

void CBulletManager::LevelInitPostEntity()
{
	m_aBullets.SetSize(200);

	for (int i = 0; i < m_aBullets.Count(); i++)
		m_aBullets[i].m_bAvailable = true;
}

CBulletManager::CBullet CBulletManager::MakeBullet(CSDKPlayer* pShooter, const Vector& vecSrc, const Vector& vecDirection, SDKWeaponID eWeapon, int iDamage, int iBulletType, bool bDoEffects)
{
	CBullet oBullet;

	oBullet.m_bAvailable = false;

	oBullet.m_hShooter = pShooter;
	oBullet.m_flShotTime = gpGlobals->curtime;
	oBullet.m_vecOrigin = vecSrc;
	oBullet.m_vecDirection = vecDirection;
	oBullet.m_eWeaponID = eWeapon;
	oBullet.m_bDoEffects = bDoEffects;
	oBullet.m_iBulletType = iBulletType;
	oBullet.m_iBulletDamage = iDamage;
	oBullet.m_ahObjectsHit.RemoveAll();

	return oBullet;
}

void CBulletManager::AddBullet(const CBullet& oBullet)
{
#ifdef CLIENT_DLL
	// If we're on the client then our only job is to do effects.
	// If this is false on the client it means the client prediction is running,
	// so don't bother. It's always false on the server but we
	// should still do the bullet since we need to do damage etc.
	if (!oBullet.m_bDoEffects)
		return;
#endif

	// Find an empty spot
	int iEmpty = -1;
	for (int i = 0; i < m_aBullets.Count(); i++)
	{
		if (m_aBullets[i].m_bAvailable)
		{
			iEmpty = i;
			break;
		}
	}

	Assert(iEmpty >= 0);
	if (iEmpty < 0)
		return;

	m_aBullets[iEmpty] = oBullet;
	m_aBullets[iEmpty].m_bAvailable = false;
}

void CBulletManager::Update(float frametime)
{
	BulletsThink(frametime);
}

void CBulletManager::FrameUpdatePreEntityThink()
{
	BulletsThink(gpGlobals->frametime);
}

ConVar da_bullet_speed("da_bullet_speed", "2000", FCVAR_REPLICATED|FCVAR_CHEAT|FCVAR_DEVELOPMENTONLY, "How fast do bullets go during slow motion?" );

void CBulletManager::BulletsThink(float flFrameTime)
{
	for (int i = 0; i < m_aBullets.Count(); i++)
	{
		CBullet& oBullet = m_aBullets[i];
		if (oBullet.m_bAvailable)
			continue;

		if (!oBullet.m_hShooter)
		{
			oBullet.m_bAvailable = true;
			continue;
		}

		if (gpGlobals->curtime > oBullet.m_flShotTime + 10)
		{
			oBullet.m_bAvailable = true;
			continue;
		}

		float flSpeed = da_bullet_speed.GetFloat();

		if (oBullet.m_hShooter->m_Shared.m_iStyleSkill == SKILL_MARKSMAN || oBullet.m_hShooter->m_Shared.m_bSuperSkill)
			flSpeed *= 2;

		float dt;
		if (oBullet.m_hShooter->GetSlowMoMultiplier() == 1)
			dt = -1;
		else
			dt = flSpeed * oBullet.m_hShooter->GetSlowMoMultiplier() * flFrameTime;

		Vector vecOriginal = oBullet.m_vecOrigin;

		SimulateBullet(oBullet, dt);

#ifdef CLIENT_DLL
		DebugDrawLine(vecOriginal, oBullet.m_vecOrigin, 0, 0, 255, false, 0.1);
#else
		DebugDrawLine(vecOriginal, oBullet.m_vecOrigin, 255, 0, 0, false, 0.1);
#endif
	}
}

ConVar sv_showimpacts("sv_showimpacts", "0", FCVAR_REPLICATED|FCVAR_CHEAT|FCVAR_DEVELOPMENTONLY, "Shows client (red) and server (blue) bullet impact point" );
ConVar da_bullet_penetrations("da_bullet_penetrations", "5", FCVAR_REPLICATED|FCVAR_CHEAT|FCVAR_DEVELOPMENTONLY, "How many times will a bullet penetrate a wall?" );

void DispatchEffect( const char *pName, const CEffectData &data );

void CBulletManager::SimulateBullet(CBullet& oBullet, float dt)
{
	Assert(oBullet.m_hShooter.Get());
	if (!oBullet.m_hShooter)
		return;

	bool bHasTraveledBefore = false;
	if (oBullet.m_flDistanceTraveled > 0)
		bHasTraveledBefore = true;

	// initialize these before the penetration loop, we'll need them to make our tracer after
	Vector vecTracerSrc = oBullet.m_vecOrigin;
	trace_t tr; // main enter bullet trace

	float flRange = dt;

	if (flRange < 0)
		flRange = 8000;

	bool bFullPenetrationDistance = false;

	Vector vecEnd = oBullet.m_vecOrigin + oBullet.m_vecDirection * flRange;

	int i;
	for (i = oBullet.m_iPenetrations; i < da_bullet_penetrations.GetInt(); i++)
	{
		CTraceFilterSimpleList tf(COLLISION_GROUP_NONE);
		tf.AddEntityToIgnore(oBullet.m_hShooter);

		for (int j = 0; j < oBullet.m_ahObjectsHit.Count(); j++)
			tf.AddEntityToIgnore(oBullet.m_ahObjectsHit[j]);

		UTIL_TraceLine( oBullet.m_vecOrigin, vecEnd, MASK_SOLID|CONTENTS_DEBRIS|CONTENTS_HITBOX, &tf, &tr );

		Vector vecTraceEnd = tr.endpos;

		if (tr.allsolid)
		{
			oBullet.m_flDistanceTraveled += (oBullet.m_vecOrigin - vecEnd).Length();
			oBullet.m_vecOrigin = vecEnd;
			break; // We're inside something. Do nothing.
		}

		Assert(oBullet.m_iBulletType > 0);
		int iDamageType = DMG_BULLET | DMG_NEVERGIB | GetAmmoDef()->DamageType(oBullet.m_iBulletType);

		if (i == 0)
			iDamageType |= DMG_DIRECT;

		if (tr.startsolid)
		{
			trace_t tr2;

			UTIL_TraceLine( tr.endpos - oBullet.m_vecDirection, oBullet.m_vecOrigin, CONTENTS_SOLID|CONTENTS_MOVEABLE, NULL, COLLISION_GROUP_NONE, &tr2 );

			// let's have a bullet exit effect if we penetrated a solid surface
			if (oBullet.m_bDoEffects && tr2.m_pEnt && tr2.m_pEnt->IsBSPModel())
				UTIL_ImpactTrace( &tr2, iDamageType );

			// ignore the entity we just hit for the next trace to avoid weird impact behaviors
			oBullet.m_ahObjectsHit.AddToTail(tr2.m_pEnt);
		}

		if ( tr.fraction == 1.0f )
		{
			oBullet.m_flDistanceTraveled += (oBullet.m_vecOrigin - vecEnd).Length();
			oBullet.m_vecOrigin = vecEnd;
			break; // we didn't hit anything, stop tracing shoot
		}

		if ( sv_showimpacts.GetBool() )
		{
#ifdef CLIENT_DLL
			// draw red client impact markers
			debugoverlay->AddBoxOverlay( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), QAngle( 0, 0, 0), 255,0,0,127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				C_BasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawClientHitboxes( 4, true );
			}
#else
			// draw blue server impact markers
			NDebugOverlay::Box( tr.endpos, Vector(-2,-2,-2), Vector(2,2,2), 0,0,255,127, 4 );

			if ( tr.m_pEnt && tr.m_pEnt->IsPlayer() )
			{
				CBasePlayer *player = ToBasePlayer( tr.m_pEnt );
				player->DrawServerHitboxes( 4, true );
			}
#endif
		}

		weapontype_t eWeaponType = WT_NONE;

		CSDKWeaponInfo *pWeaponInfo = CSDKWeaponInfo::GetWeaponInfo(oBullet.m_eWeaponID);
		Assert(pWeaponInfo);
		if (pWeaponInfo)
			eWeaponType = pWeaponInfo->m_eWeaponType;

		float flDamageMultiplier = 1;
		float flMaxRange = 3000;

		// Power formula works like so:
		// pow( x, distance/y )
		// The damage will be at 1 when the distance is 0 units, and at
		// x% when the distance is y units, with a gradual decay approaching zero
		switch (eWeaponType)
		{
		case WT_RIFLE:
			flDamageMultiplier = 0.75f;
			flMaxRange = 3000;
			break;

		case WT_SHOTGUN:
			flDamageMultiplier = 0.40f;
			flMaxRange = 500;
			break;

		case WT_SMG:
			flDamageMultiplier = 0.50f;
			flMaxRange = 1000;
			break;

		case WT_PISTOL:
		default:
			flDamageMultiplier = 0.55f;
			flMaxRange = 1500;
			break;
		}

		flMaxRange *= oBullet.m_hShooter->m_Shared.ModifySkillValue(1, 0.5f, SKILL_MARKSMAN);

		//calculate the damage based on the distance the bullet travelled.
		oBullet.m_flDistanceTraveled += tr.fraction * flRange;

		float flCurrentDistance = oBullet.m_flDistanceTraveled;

		// First 500 units, no decrease in damage.
		if (eWeaponType == WT_SHOTGUN)
			flCurrentDistance -= 350;
		else
			flCurrentDistance -= 500;

		if (flCurrentDistance < 0)
			flCurrentDistance = 0;

		if (flCurrentDistance > flMaxRange)
			flCurrentDistance = flMaxRange;

		float flDistanceMultiplier = pow(flDamageMultiplier, (flCurrentDistance / flMaxRange));

		if( oBullet.m_bDoEffects )
		{
			// See if the bullet ended up underwater + started out of the water
			if ( enginetrace->GetPointContents( tr.endpos ) & (CONTENTS_WATER|CONTENTS_SLIME) )
			{	
				trace_t waterTrace;
				UTIL_TraceLine( oBullet.m_vecOrigin, tr.endpos, (MASK_SHOT|CONTENTS_WATER|CONTENTS_SLIME), oBullet.m_ahObjectsHit.Tail(), COLLISION_GROUP_NONE, &waterTrace );

				if( waterTrace.allsolid != 1 )
				{
					CEffectData	data;
					data.m_vOrigin = waterTrace.endpos;
					data.m_vNormal = waterTrace.plane.normal;
					data.m_flScale = random->RandomFloat( 8, 12 );

					if ( waterTrace.contents & CONTENTS_SLIME )
					{
						data.m_fFlags |= FX_WATER_IN_SLIME;
					}

					DispatchEffect( "gunshotsplash", data );
				}
			}
			else
			{
				//Do Regular hit effects

				// Don't decal nodraw surfaces
				if ( !( tr.surface.flags & (SURF_SKY|SURF_NODRAW|SURF_HINT|SURF_SKIP) ) )
				{
					CBaseEntity *pEntity = tr.m_pEnt;
					//Tony; only while using teams do we check for friendly fire.
					if ( SDKGameRules()->IsTeamplay() && pEntity && pEntity->IsPlayer() && (pEntity->GetBaseAnimating() && !pEntity->GetBaseAnimating()->IsRagdoll()) )
					{
						if ( pEntity->GetTeamNumber() != oBullet.m_hShooter->GetTeamNumber() )
						{
							UTIL_ImpactTrace( &tr, iDamageType );
						}
					}
					//Tony; non player, just go nuts,
					else
					{
						UTIL_ImpactTrace( &tr, iDamageType );
					}
				}
			}
		} // bDoEffects

		// add damage to entity that we hit

#ifdef GAME_DLL
		float flBulletDamage = oBullet.m_iBulletDamage * flDistanceMultiplier / (i+1);	// Each iteration the bullet drops in strength
		if (oBullet.m_hShooter->IsStyleSkillActive(SKILL_MARKSMAN))
			flBulletDamage = oBullet.m_iBulletDamage * flDistanceMultiplier / (i/2+1);	// Each iteration the bullet drops in strength but not nearly as much.

		ClearMultiDamage();

		CTakeDamageInfo info( oBullet.m_hShooter, oBullet.m_hShooter, flBulletDamage, iDamageType );
		CalculateBulletDamageForce( &info, oBullet.m_iBulletType, oBullet.m_vecDirection, tr.endpos );
		tr.m_pEnt->DispatchTraceAttack( info, oBullet.m_vecDirection, &tr );

		oBullet.m_hShooter->TraceAttackToTriggers( info, tr.startpos, tr.endpos, oBullet.m_vecDirection );

		ApplyMultiDamage();
#else
		flDistanceMultiplier = flDistanceMultiplier; // Silence warning.
#endif

		oBullet.m_ahObjectsHit.AddToTail(tr.m_pEnt);

		float flPenetrationDistance;
		switch (eWeaponType)
		{
		case WT_RIFLE:
			flPenetrationDistance = 25;
			break;

		case WT_SHOTGUN:
			flPenetrationDistance = 5;
			break;

		case WT_SMG:
			flPenetrationDistance = 15;
			break;

		case WT_PISTOL:
		default:
			flPenetrationDistance = 15;
			break;
		}

		flPenetrationDistance = oBullet.m_hShooter->m_Shared.ModifySkillValue(flPenetrationDistance, 1, SKILL_MARKSMAN);

		Vector vecBackwards = tr.endpos + oBullet.m_vecDirection * flPenetrationDistance;
		if (tr.m_pEnt->IsBSPModel())
			UTIL_TraceLine( vecBackwards, tr.endpos, CONTENTS_SOLID|CONTENTS_MOVEABLE, NULL, COLLISION_GROUP_NONE, &tr );
		else
			UTIL_TraceLine( vecBackwards, tr.endpos, CONTENTS_HITBOX, NULL, COLLISION_GROUP_NONE, &tr );

		if (tr.startsolid)
		{
			bFullPenetrationDistance = true;
			break;
		}

		// Set up the next trace. One unit in the direction of fire so that we firmly embed
		// ourselves in whatever solid was hit, to make sure we don't hit it again on next trace.
		oBullet.m_vecOrigin = vecTraceEnd + oBullet.m_vecDirection;
	}

	oBullet.m_iPenetrations = i;

	// the bullet's done penetrating, let's spawn our particle system
	if (oBullet.m_bDoEffects && dt < 0)
		oBullet.m_hShooter->MakeTracer( oBullet.m_vecOrigin, tr, TRACER_TYPE_DEFAULT, !bHasTraveledBefore );

	if (bFullPenetrationDistance || oBullet.m_iPenetrations >= da_bullet_penetrations.GetInt())
		oBullet.m_bAvailable = true;

	if (dt < 0)
		oBullet.m_bAvailable = true;
}
