/*	SCCS Id: @(#)mcastu.c	3.4	2003/01/08	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"
#include "edog.h" /* sporkhack MGC_ENRAGE needs this */

/* monster mage spells */
#define MGC_PSI_BOLT	 0
#define MGC_CURE_SELF	 1
#define MGC_HASTE_SELF	 2
#define MGC_STUN_YOU	 3
#define MGC_DISAPPEAR	 4
#define MGC_WEAKEN_YOU	 5
#define MGC_DESTRY_ARMR	 6
#define MGC_CURSE_ITEMS	 7
#define MGC_AGGRAVATION	 8
#define MGC_SUMMON_MONS	 9
#define MGC_CLONE_WIZ	10
#define MGC_DEATH_TOUCH	11
#define MGC_CREATE_POOL	12
#define MGC_CALL_UNDEAD	13
#define MGC_WITHER	14
#define MGC_DAMAGE_ARMR	15
#define MGC_ENRAGE	16
#define MGC_DIVINE_WRATH	17
#define MGC_SUMMON_GHOST	18

/* monster cleric spells */
#define CLC_OPEN_WOUNDS	 0
#define CLC_CURE_SELF	 1
#define CLC_CONFUSE_YOU	 2
#define CLC_PARALYZE	 3
#define CLC_BLIND_YOU	 4
#define CLC_INSECTS	 5
#define CLC_CURSE_ITEMS	 6
#define CLC_LIGHTNING	 7
#define CLC_FIRE_PILLAR	 8
#define CLC_GEYSER	 9
#define CLC_AGGRAVATION	 10
#define CLC_PETRIFY	 11 /* currently unused */
#define CLC_RANDOM	 12

/* monster miscellaneous spells, assigned to specific monster types, rather 
 * than given to all monsters. Since these have to coexist with the SPE_*
 * spell names, make them negative (we also don't need to bother with pseudo
 * objects for these).
 */
#define MSP_POISON_BLAST -1	/* From Slash'EM */
#define MSP_POISON_FOG	 -2	/* Fancy name for stinking cloud :-) */
#define MSP_ACID_FOG     -3     /* Corrosive fog */
#define MSP_BLADES	 -4	/* Whirling Blades of Doom :-) */
#define MSP_FORCE_ANIM	 -5	/* Animate object to hit target */
#define MSP_FORCE_SLAM	 -6	/* Bash target into wall or ground */
#define MSP_FORCE_REPEL	 -7	/* Only escape, if too close to the player, 
				 * pushes the player away */



STATIC_DCL void FDECL(cursetxt,(struct monst *,BOOLEAN_P));
STATIC_DCL int FDECL(choose_magic_spell, (int));
STATIC_DCL int FDECL(get_cloud_radius, (struct monst *, int, int, int));
STATIC_DCL int FDECL(m_choose_magic_spell, (int));
STATIC_DCL int FDECL(choose_clerical_spell, (int));
STATIC_DCL int FDECL(m_choose_clerical_spell, (int));
STATIC_DCL void FDECL(cast_wizard_spell,(struct monst *, int,int));
STATIC_DCL void FDECL(cast_cleric_spell,(struct monst *, int,int));
STATIC_DCL boolean FDECL(is_undirected_spell,(unsigned int,int));
STATIC_DCL boolean FDECL(is_melee_spell,(unsigned int,int));
STATIC_DCL boolean FDECL(spell_would_be_useless,(struct monst *,unsigned int,int));
STATIC_PTR void FDECL(set_litZ, (int,int,genericptr_t));
STATIC_DCL boolean FDECL(getjumptarget, (struct monst *, struct monst *,
					 int *, int *, int));
STATIC_DCL boolean FDECL(resists_attk, (struct monst *, struct monst *));
STATIC_DCL int FDECL(damagem, (struct monst *, struct monst *, int));

#ifdef OVL0

extern const char * const flash_types[];	/* from zap.c */

extern boolean m_using;

/* Non-standard monster spells must be defined in arrays here; this is clumsy,
 * so we should probably put this in a text file somewhere and let makedefs.c
 * parse the file and spit out these arrays automatically. 
 */

/* If a spell can be used for both attack and escape, define its monspell 
 * twice */
#define ATTACK  0
#define ESCAPE	1

/* Monster specific spells section */

/* The idea here is to make magic users significantly more dangerous in the
 * early/middle game. Making the endgame difficult is difficult :-) */
static struct monspell gnomewiz[] = { 
    /* Gnomish wizards get a little muscle */
    { SPE_FORCE_BOLT, 1, 25, ATTACK },
    { SPE_JUMPING   , 2, 45,  ATTACK },	/* Close in on hero */
    { SPE_JUMPING   , 2, 90, ESCAPE },
    { SPE_KNOCK	    , 1, 60, ESCAPE },
    { SPE_CURE_BLINDNESS, 4, 60, ESCAPE },
    { STRANGE_OBJECT, 0, 0, 0 }
};

static struct monspell orcshaman[] = {
    { SPE_FORCE_BOLT, 1, 20, ATTACK },
    /* Dangerous! */
    { MSP_BLADES    , 1, 50, ATTACK },
    { STRANGE_OBJECT, 0, 0, 0 }
};

static struct monspell koboldsh[] = {
    /* Magic missile is apt to kill the shaman himself */
/*    { SPE_MAGIC_MISSILE ,  1, 10, ATTACK }, */
    /* Kobold shamans can safely specialize in poison attacks */
/*    { MSP_POISON_BLAST  ,  1, 5, ATTACK }, */
    /* This seems quite dangerous for low-level characters :-) */
    { MSP_POISON_FOG	,  2, 10, ATTACK },

    /* Unlikely to be used, ever */
    { SPE_RESTORE_ABILITY, 3, 5, ESCAPE },
    /* Might be used, but who cares? */
    { SPE_LEVITATION    ,  2, 50, ESCAPE },
    { STRANGE_OBJECT, 0, 0, 0 }
};

/* Archons should concentrate more on other spells, since the hero's probably
 * immune to everything by the time he meets archons */
static struct monspell archon[] = {
    { SPE_CHARM_MONSTER, 5, 15, ATTACK },
    { SPE_FIREBALL     , 3, 10, ATTACK },
    { SPE_CONE_OF_COLD,  3,  5, ATTACK },
    { STRANGE_OBJECT, 0, 0, 0 }
};

/* My favourite nasty now */
static struct monspell goldennaga[] = {
    /* Creates an interesting mix of monsters, unlike summon nasties */
    { SPE_CREATE_MONSTER, 5, 15, ATTACK },
    /* And the naga is usually tricky to pin down */
    { SPE_TELEPORT_AWAY,  3, 30, ESCAPE },
    { SPE_LEVITATION   ,  2, 80, ESCAPE },
    { STRANGE_OBJECT, 0, 0, 0 }
};

static struct monspell darkone[] = {
    { SPE_DRAIN_LIFE, 2, 30, ATTACK },
    { STRANGE_OBJECT, 0, 0, 0 }
};

static struct monspell nalfeshnee[] = {
    { SPE_FINGER_OF_DEATH, 2, 5, ATTACK },
    { SPE_CHARM_MONSTER, 2, 30, ATTACK },
    { STRANGE_OBJECT, 0, 0, 0 }
};

static struct monspell kirin[] = {
    { SPE_STONE_TO_FLESH, 3, 50, ATTACK },	/* Animate statues */
    { SPE_STONE_TO_FLESH, 0, 75, ESCAPE },	/* Ward against stoning */
    { SPE_RESTORE_ABILITY, 2, 45, ESCAPE },
    { STRANGE_OBJECT, 0, 0, 0 }
};

/* Liches are nasty enough as they are */
static struct monspell archlich[] = {
    { SPE_CAUSE_FEAR, 2, 5, ATTACK },
    { STRANGE_OBJECT, 0, 0, 0 }
};

static struct monspell rodney[] = {
    { SPE_STONE_TO_FLESH, 0, 80, ESCAPE },
    { STRANGE_OBJECT, 0, 0, 0 }
};

static struct monspell thothamon[] = {
    { SPE_MAGIC_MISSILE, 2, 5, ATTACK },
    { STRANGE_OBJECT, 0, 0, 0 }
};

static struct monspell titan[] = {
    { SPE_SLEEP		 , 2, 10, ATTACK },
    { SPE_KNOCK		 , 1, 80, ESCAPE },
    { STRANGE_OBJECT, 0, 0, 0 }
};

static struct monspell priest[] = {
    /* The standard priestly spells are nicely varied and effective, and we
     * don't want to reduce those spells by defining general attack spells 
     * here. Happily, acid fog won't be used if the target is nearby. */
    { MSP_ACID_FOG	 , 4, 10, ATTACK },
    { STRANGE_OBJECT, 0, 0, 0 }
};

STATIC_OVL
struct monspell *
findmspell(spells, spellnum, escape)
struct monspell *spells;
int spellnum;
boolean escape;
{
    int i;

    for (i = 0; spells[i].spell != STRANGE_OBJECT; ++i) {
	if (spells[i].spell == spellnum && spells[i].escape == escape)
	    return &spells[i];
    }
    return NULL;
}

STATIC_OVL
struct monspell *
getmspells(mon)
struct monst *mon;
{
    int ndx = monsndx(mon->data);

    switch (ndx) {
    case PM_ALIGNED_PRIEST:
    case PM_HIGH_PRIEST:
      	return priest;
    case PM_ARCH_LICH:
	return archlich;
    case PM_ARCHON:
	return archon;
    case PM_DARK_ONE:
	return darkone;
    case PM_GNOMISH_WIZARD:
	return gnomewiz;
    case PM_GOLDEN_NAGA:
	return goldennaga;
    case PM_KI_RIN:
	return kirin;
    case PM_KOBOLD_SHAMAN:
	return koboldsh;
    case PM_NALFESHNEE:
	return nalfeshnee;
    case PM_ORC_SHAMAN:
	return orcshaman;
    case PM_THOTH_AMON:
    case PM_NEFERET_THE_GREEN:
	return thothamon;
    case PM_TITAN:
	return titan;
    case PM_WIZARD_OF_YENDOR:
	return rodney;
    }

    return NULL;
}

/* Returns monspell if the monster can cast the spell - makes no judgement on
 * how castable the spell is */
struct monspell *
getmspell(mon, spell, escape)
struct monst *mon;
int spell;
boolean escape;
{
    struct monspell *msp = getmspells(mon);

    return msp? findmspell(msp, spell, escape) : NULL;
}

int 
cancast(mon, spell)
struct monst *mon;
int spell;
{
    struct monspell *msp = NULL;

    /* A spellcaster versed in restore ability can use it even when confused */
    if (mon->mcan || (mon->mconf && spell != SPE_RESTORE_ABILITY))
	return 0;

    msp = getmspell(mon, spell, ESCAPE);
    return (msp && msp->pref > rn2(100));
}

/* Returns the radius of a moderately sized region (presumably to be occupied by
 * a damaging fog), which does not include the position of the spellcaster, or
 * the spellcaster's friends.
 */
STATIC_OVL
int
get_cloud_radius(mon, x, y, dist)
struct monst *mon;
int x, y, dist;
{
    int radius;

    if (dist < 0) dist = dist2(x, y, mon->mx, mon->my);
    if (dist < 9) return 0;

    /* A high level spellcaster might create a cloud as big as a b ?oSC would */
    if ((radius = 2 + (mon->m_lev / 5)) > 4) radius = 4;

    /* Reduce cloud radius until the caster is safe. Crude. */
    while (dist < radius * radius) radius--;
    /* If there are friends inside, do nothing */
    if (region_has_friendly(mon, x, y, radius)) return 0;

    return radius;
}

/* Find a statue adjacent to (x,y) - does a light search, looking only at the
 * top two objects on each square, to prevent a major performance hit. Statues
 * at (x,y) are not considered */
STATIC_OVL
struct obj *
find_adjacent_statue(x, y)
int x, y;
{
    int dx, dy, px, py, bailout;
    struct obj *otmp;

    /* Randomize direction of search to prevent the top-left-object-first
     * syndrome; this still leaves us with the corner-object-first problem, but
     * that can't be helped. */
    int xi = rn2(100) >= 50? 1 : -1, xend = x + xi + xi,
        yi = rn2(100) >= 50? 1 : -1, yend = y + yi + yi;

    for (dx = x - xi; dx != xend; dx += xi) {
	for (dy = y - yi; dy != yend; dy += yi) {
	    if ((dx == x && dy == y) || !isok(dx, dy)) continue;
	
	    for (otmp = level.objects[dx][dy], bailout = 0;
			    otmp && bailout < 2; 
			    otmp = otmp->nexthere, bailout++)
		if (otmp->otyp == STATUE)
		    return otmp;
	}
    }
    return NULL;
}



/* Cast an attack spell at a target, possibly the player. Returns nonzero if
 * a spell was actually cast, zero if the spell was aborted. */
int 
mcast_attk_spell(mtmp, mspel, mtarg)
struct monst *mtmp, *mtarg;
struct monspell *mspel;
{
    int 	tx, ty;
    boolean	lined = FALSE;
    struct obj *pseudo = (struct obj *) 0;
    char 	caster[BUFSZ];
    boolean	youtarg = (mtarg == &youmonst);
    boolean	seecast = canseemon(mtmp),
    		seevict = (youtarg || canseemon(mtarg)),
		mess    = seecast,
		wiz	= (mtmp->m_lev > 15);	/* Skilled spellcaster */
    int 	ret 	= 0, dist;
    struct attack mattk;

    mattk.aatyp = AT_MAGC;
    mattk.adtyp = AD_SPEL;

    /* Simplistic damage (lev/2)d6 */
    mattk.damn  = mtmp->m_lev / 2 + 1;
    /* Cap the damage at a reasonable number? */
    if (mattk.damn > 10) mattk.damn = 10;
    mattk.damd  = 6;

    if (mtarg == &youmonst) {
	tx = mtmp->mux;
	ty = mtmp->muy;
    } else {
	tx = mtarg->mx;
	ty = mtarg->my;
    }

    dist  = dist2(tx, ty, mtmp->mx, mtmp->my);
    lined = linedup(tx, ty, mtmp->mx, mtmp->my);

    /* If the target is adjacent, force a friendly check before using ray 
     * attacks, since adjacent targets are a result of mhitm attacks, rather
     * than pets attacking at range. */
    if (lined && dist < 4 && !youtarg && mtmp->mtame &&
	    	find_friends(mtmp, mtarg, 15)) 
	lined = FALSE;

    tbx   = sgn(tbx);
    tby   = sgn(tby);

    /* Build pseudo spell object, if this is a player spell */
    if (mspel->spell >= 0) {
	pseudo = mksobj(mspel->spell, FALSE, FALSE);
	pseudo->blessed = pseudo->cursed = 0;
	pseudo->quan    = 20L;
    }
    
    strcpy(caster, Monnam(mtmp));
    switch (mspel->spell) {
    case SPE_FORCE_BOLT:
	if (!lined) break;

	ret = 1;
	if (mess)
		pline("%s zaps a force bolt at %s!", caster, youtarg? "you" : 
							  mon_nam(mtarg));
	m_using = TRUE;
	dombhit(mtmp, rn1(8, 6), mbhitm, bhito, pseudo, tbx, tby);
	m_using = FALSE;
	break;
    case SPE_MAGIC_MISSILE:
	if (!lined) break;
	mattk.adtyp = AD_MAGM;
	m_using = TRUE;
	ret = buzzmm(mtmp, &mattk, mtarg);
	m_using = FALSE;
	break;
    case MSP_POISON_BLAST:
	if (!lined) break;
	mattk.adtyp = AD_DRST;
	m_using = TRUE;
	ret = buzzmm(mtmp, &mattk, mtarg);
	m_using = FALSE;
	break;
    case MSP_POISON_FOG:
	{
	    int radius = get_cloud_radius(mtmp, tx, ty, dist);
	    if (radius <= 1) break;
	    /* Don't allow overlapping regions */
	    if (visible_region_at(tx, ty)) break;

	    if (mess) pline("%s utters an incantation!", caster);
	    ret = 1;
	    if (!create_gas_cloud(tx, ty, radius, 5 + mattk.damn) 
		    && mess)
		pline("%s seems annoyed.", caster);
	}
	break;
    case MSP_ACID_FOG:
	/* In contrast to poison fog, this is a high-damage fog, and acid
	 * resistance is hard to acquire. Monsters that use this are likely to
	 * be uncommonly nasty.
	 */
	{
	    int radius = get_cloud_radius(mtmp, tx, ty, dist);
	    if (radius <= 1) break;
	    /* Don't allow overlapping regions */
	    if (visible_region_at(tx, ty)) break;

	    if (mess) pline("%s utters an incantation!", caster);
	    ret = 1;
	    /* What happens if there are many, many acid clouds around? */
	    if (!create_acid_cloud(tx, ty, radius, 8 + mattk.damn) 
		    && mess)
		pline("%s seems annoyed.", caster);
	}
	break;
    case SPE_CONE_OF_COLD:
	if (!lined && !wiz) break;
	ret = 1;
	if (wiz) {
	    if (mess)
		pline("%s casts a cone of cold!", caster);

	    if (dist < 4 && !resists_cold(mtmp) ||
		    (mtmp->mtame && 
		     	has_adjacent_enemy(mtmp, tx, ty, FALSE))) {
		ret = 0;
		break;
	    }
	    m_using = TRUE;
	    explode(tx, ty, -(pseudo->otyp - SPE_MAGIC_MISSILE), 
			20 + rnd(30),
			SPBOOK_CLASS,
			EXPL_FROSTY);
	    m_using = FALSE;
	} else {
	    mattk.adtyp = AD_COLD;
	    m_using = TRUE;
	    buzzmm(mtmp, &mattk, mtarg);
	    m_using = FALSE;
	}
	break;
    case SPE_FIREBALL:
	if (!lined && !wiz) break;
	ret = 1;
	if (wiz) {
	    if (mess)
		pline("%s zaps a fireball!", caster);
	    if (dist < 4 && !resists_fire(mtmp) ||
		    (mtmp->mtame && has_adjacent_enemy(mtmp, tx, ty, FALSE))) {
		ret = 0;
		break;
	    }
	    m_using = TRUE;
	    explode(tx, ty, -(pseudo->otyp - SPE_MAGIC_MISSILE), 
			20 + rnd(30),
			SPBOOK_CLASS,
			EXPL_FIERY);
	    m_using = FALSE;
	} else {
	    mattk.adtyp = AD_FIRE;
	    m_using = TRUE;
	    buzzmm(mtmp, &mattk, mtarg);
	    m_using = FALSE;
	}
	break;
    case SPE_SLEEP:
	if (!lined) break;
	ret = 1;
	mattk.adtyp = AD_SLEE;
	m_using = TRUE;
	buzzmm(mtmp, &mattk, mtarg);
	m_using = FALSE;
	break;
    case SPE_FINGER_OF_DEATH:
	if (!lined) break;
	ret = 1;
	mattk.adtyp = AD_DISN;
	m_using = TRUE;
	buzzmm(mtmp, &mattk, mtarg);
	m_using = FALSE;
	break;
    case SPE_CREATE_MONSTER:
	/* We don't care whether it's lined up or not */
	{
	    struct monst *mon;		
	    struct permonst *pm = 0, *fish = 0;
	    coord cc;
	    int cnt = 1 + (!rn2(3)? rnd(4) : 0);

	    if (mtmp->mtame || mtmp->mpeaceful) break;

	    ret = 1;

	    /* Not really satisfactory, since there might be only the one
	     * monster created. */
	    if (mess) pline("%s creates monsters!", Monnam(mtmp));
	    
	    while(cnt--) {
		/* `fish' potentially gives bias towards water locations;
		   `pm' is what to actually create (0 => random) */
		if (!enexto(&cc, mtmp->mx, mtmp->my, fish)) break;
		mon = makemon(pm, cc.x, cc.y, NO_MM_FLAGS);
	    }
	}
	break;
    case SPE_DRAIN_LIFE:
	if (!lined) break;
	ret = 1;

	if (mess || youtarg)
	    pline("%s zaps a spell at %s!", caster, youtarg? "you" :
			    				mon_nam(mtarg));
	m_using = TRUE;
	dombhit(mtmp, rn1(8, 6), mbhitm, bhito, pseudo, tbx, tby);
	m_using = FALSE;
	break;
    case SPE_CHARM_MONSTER:
	if (mtarg != &youmonst && mtarg->mtame && !mtmp->mpeaceful) {
	    boolean notice;

	    /* Will pet resist? */
	    if (mess || seevict)
		pline("%s makes a hypnotic gesture at %s!", caster,
				mon_nam(mtarg));

	    notice = (haseyes(mtarg->data) && mtarg->mcansee
			    && !mtarg->msleeping && !mtarg->mconf 
			    && !mtarg->mstun);

	    if (!resists_attk(mtarg, mtmp) && !mindless(mtarg->data)
			    && notice) {
		if (seevict) pline("%s turns on you!", Monnam(mtarg));
		mtarg->mtame 		= 0;
		mtarg->mpeaceful	= 0;
	    } else {
		if (seevict) {
		    if (notice)
			pline("%s seems unimpressed.", Monnam(mtarg));
		    else
			pline("%s doesn't notice.", Monnam(mtarg));
		}
	    }
	    ret = 1;
	}
	break;
    case SPE_CAUSE_FEAR:
	if (mtarg != &youmonst && !mtarg->mflee && mtmp->mcansee) {
	    static char *fear[] = {
		"utters a fearsome curse.",
		"mutters a dark incantation.",
		"snarls a curse.",
		"sings 'Gunga Din'!",
		"dials 911!",
		"demands an audit!"
	    };
	    if (mess) {
		int index = rn2(3) + (Hallucination? 3 : 0);
		pline("%s stares at %s and %s", caster, mon_nam(mtarg),
						fear[index]);
	    }

	    if (!mindless(mtarg->data) && !mtarg->mconf
			    && !resists_attk(mtarg, mtmp)) {
		monflee(mtarg, rn1(5, 5), TRUE, TRUE);
	    } else {
		if (seevict)
		    pline("%s seems unimpressed.", Monnam(mtarg));
	    }
	    ret = 1;
	}
	break;
    case SPE_STONE_TO_FLESH:
	/* Exotic spell - try to stf any nearby statue */
	if (mtmp->mpeaceful) break;
	{
	    struct obj *adj_statue = find_adjacent_statue(mtmp->mx, mtmp->my);
	    if (adj_statue) {
		ret   = 1;
		if (mess)
		    pline("%s casts a spell!", caster);
		lined = linedup(adj_statue->ox, adj_statue->oy,
				mtmp->mx, mtmp->my);
		tbx = sgn(tbx);
		tby = sgn(tby);

		m_using = TRUE;
		dombhit(mtmp, rn1(8, 5), mbhitm, bhito, pseudo, tbx, tby);
		m_using = FALSE;
	    }
	}
	break;
    case SPE_JUMPING:
	{
	    int ax, ay; /* Target to jump to */
     	    
	    /* Levitating monster can't jump, even magically. Keeps consistent
	     * with player movement code and makes life easier :) */
	    if (is_levitating(mtmp) || !getjumptarget(mtmp, mtarg, &ax, &ay, 1)
			    || mtmp->data->msize >= MZ_HUGE
			    || mtmp->mtrapped)
		break;

	    ret = 3;
	    if (seecast)
		pline("%s jumps towards %s!", caster, youtarg? "you" :
			    				mon_nam(mtarg));
    	    mhurtle(mtmp, ax, ay, -1);
	}
	break;
    case MSP_BLADES:
	{
	    int dmg = d(mattk.damn, mattk.damd);

	    ret = 1;
	    if (mess)
	    	pline("%s points at %s and chants a cryptic spell.", caster,
		    (youtarg? "you" : mon_nam(mtarg)));
	    if (youtarg) {
		if (Blind)
		    You_feel("blades slashing at you!");
		else
		    You("are slashed by whirling blades!");
	    } else if (seevict)
		pline("%s is slashed by whirling blades!", Monnam(mtarg));

	    {
		/* Work out damage */
		struct permonst *mp = youtarg? youmonst.data : mtarg->data;

		if (is_whirly(mp) || unsolid(mp)) {
		    if (youtarg || seevict)
			pline("The blades don't seem to harm %s.",
			    (youtarg? "you" : mon_nam(mtarg)));
		    break;
		}
		/* There's no to-hit calculation here - assume that those blades
		 * never miss :-) But since this is weapon-based damage, armor
		 * does protect. */
		if (youtarg) {
		    if (u.uac < 0) dmg -= rnd(-u.uac);
		} else {
		    int mac = find_mac(mtarg);
		    if (mac < 0) dmg -= rnd(-mac);
		}
		if (dmg < 0) break;
	    }

	    if (youtarg) losehp(dmg, "whirling blade", KILLED_BY_AN);
	    else damagem(mtmp, mtarg, dmg);
	}
	break;
    } /* switch (mspel->spell) */
    
done:
    if (pseudo) obfree(pseudo, (struct obj *) 0);
    if (ret && !DEADMONSTER(mtmp))
	mtmp->mspec_used = mspel->cost;
    return ret;
}

#define JUMP_DIST 4
STATIC_DCL
boolean
getjumptarget(mon, mtarg, x, y, appr)
struct monst *mon, *mtarg;
int *x, *y;
int appr;	/* -1 to go away from hero, 1 to jump to hero. */
{
    int dx, dy, tx, ty;
    int dist, sqdist; 

    if (mtarg == &youmonst) {
	tx = mon->mux;
	ty = mon->muy;
    } else {
	tx = mtarg->mx;
	ty = mtarg->my;
    }

    dist = dist2(tx, ty, mon->mx, mon->my);

    if (dist <= 16 || dist > (BOLT_LIM + 4) * (BOLT_LIM + 4))
	return FALSE;

    dx = tx - mon->mx;
    dy = ty - mon->my;

    if (appr == -1) {
	dx = -dx;
	dy = -dy;
    }

    if (dist > JUMP_DIST * JUMP_DIST) {
	sqdist = isqrt(dist);

	/* Scale back */
	dx = (dx * JUMP_DIST) / sqdist;
	dy = (dy * JUMP_DIST) / sqdist;
    }

    if (dx * dx + dy * dy < 9) /* Too close, no point in jumping */
	return FALSE;

    /* Convert relative to abs coords */
    dx += mon->mx;
    dy += mon->my;

    if (goodpos(dx, dy, mon, 0)) {
	*x = dx;
	*y = dy;
	return TRUE;
    }
    return FALSE;
}

int
mcast_escape_spell(mtmp, spell)
struct monst *mtmp;
int spell;
{
    struct monspell *msp = getmspell(mtmp, spell, ESCAPE);
    boolean vismon = canseemon(mtmp);
    int ret = 0;
    char caster[BUFSZ];

    strcpy(caster, Monnam(mtmp)); 

    if (!msp) {
	impossible("Monster doesn't know spell?");
	return 0;
    }

    switch (msp->spell) {
    case SPE_TELEPORT_AWAY:
	/* Duplicated from muse.c */
	if (vismon)
	    pline("%s casts a teleportation spell!", caster);
	if (tele_restrict(mtmp)) {
	    ret = 2; 
	    break;
	}
	if ((On_W_tower_level(&u.uz)) && !rn2(3)) {
	    if (vismon)
		pline("%s seems disoriented for a moment.", caster);
	    ret = 2;
	    break;
	}
	rloc(mtmp, FALSE);
	ret = 1;
	break;
    case SPE_LEVITATION:
#ifdef SINKS
	/* If the monster's on a sink, don't even bother */
	if (IS_SINK(levl[mtmp->mx][mtmp->my].typ)) break;
#endif
	ret = 3;
	if (vismon) {
	    if (is_levitating(mtmp))
		pline("%s casts a levitation spell!", caster);
	    else
	    	pline("%s begins to levitate!", caster);
	}
	begin_levitation(mtmp, 10 + rnd(20));
	mtmp->mintrinsics |= MR2_LEVITATE;
	break;
    case SPE_JUMPING:
	{
	    int ax, ay; /* Target to jump to */
     	    
	    /* Levitating monster can't jump, even magically. Keeps consistent
	     * with player movement code and makes life easier :) */
	    if (is_levitating(mtmp) || !getjumptarget(mtmp, &youmonst, 
				    		      &ax, &ay, -1)
			    || mtmp->data->msize >= MZ_HUGE
			    || mtmp->mtrapped)
		break;

	    ret = 3;
	    if (vismon)
		pline("%s jumps away!", caster);
    	    mhurtle(mtmp, ax, ay, -1);
	}
	break;
    case SPE_STONE_TO_FLESH:
	/* This should be called only if the monster needs unstoning */
	ret = 1;

	/* Nothing much to do to the caster, but... */
	if (vismon)
		pline("%s casts a spell at %sself!", caster, mhim(mtmp));

	/* ... caster's inventory is nuked by the spell */
	{
	    struct obj *pseudo, *otemp, *onext;
	    boolean didmerge;
	    
	    pseudo = mksobj(msp->spell, FALSE, FALSE);
	    pseudo->blessed = pseudo->cursed = 0;
	    pseudo->quan    = 20L;

	    /* This code borrowed from zap.c: */
	    for (otemp = mtmp->minvent; otemp; otemp = onext) {
		onext = otemp->nobj;
		(void) bhito(otemp, pseudo);
	    }

	    obfree(pseudo, (struct obj *) 0);

	    /* 
	     * Also from zap.c - it's questionable whether a monster's 
	     * inventory really needs merging, but the speed hit is arguably
	     * excusable, since it's the rare turn that a monster casts stone
	     * to flesh at itself 
	     */
	    /*
	     * It is possible that we can now merge some inventory.
	     * Do a higly paranoid merge.  Restart from the beginning
	     * until no merges.
	     */
	    do {
		didmerge = FALSE;
		for (otemp = mtmp->minvent; !didmerge && otemp; 
				otemp = otemp->nobj)
		    for (onext = otemp->nobj; onext; onext = onext->nobj)
			if (merged(&otemp, &onext)) {
			    didmerge = TRUE;
			    break;
			}
	    } while (didmerge);
	}

	break;
    case SPE_KNOCK:
	/* Nothing to do here, caller does all the real work */
	if (vismon)
	    pline("%s casts a spell.", caster);
	ret = 1;
	break;
    case SPE_CURE_BLINDNESS:
	if (mtmp->mblinded) {
	    if (vismon)
		pline("%s casts a spell at %sself.", caster, mhim(mtmp));

	    mtmp->mblinded = 0;
	    mtmp->mcansee  = 1;
	    if (vismon)
		pline("%s can see again!", caster);
	    ret = 1;
	}
	break;
    case SPE_RESTORE_ABILITY:
	if (mtmp->mblinded || mtmp->mconf || mtmp->mstun) {
	    ret = 1;
	    if (vismon)
		pline("%s casts a spell at %sself.", caster, mhim(mtmp));

	    if (mtmp->mblinded) {
		mtmp->mblinded = 0;
		mtmp->mcansee  = 1;
		if (vismon)
		    pline("%s can see again!", caster);
	    }

	    if (mtmp->mconf || mtmp->mstun) {
		mtmp->mconf = mtmp->mstun = 0;
		if (vismon)
		    pline("%s seems steadier now.", caster);
	    }
	}
	break;
    }

    if (ret)
	mtmp->mspec_used = msp->cost;

    return ret;
}

#define spellname(spell)	OBJ_NAME(objects[spell])
/* return values:
 * 1: successful spell
 * 0: unsuccessful spell
 */
int
gcastm(mtmp, mattk, mtarg)
register struct monst *mtmp, *mtarg;
register struct attack *mattk;
{
    struct monspell *msp, *bestspell = NULL;
    int bestscore = -1, count = 0, chance = rn2(100);
    int tx, ty;

    /* First, see whether we should cast the monster's favourite 
     * attack spells */
    if (mtmp->mcan || mtmp->mspec_used || mtmp->mconf 
		    || !mtmp->mcansee)
	return 0;

    if ((mtarg == &youmonst && !m_canseeu(mtmp)) ||
	    (mtarg != &youmonst && !m_cansee(mtmp, mtarg->mx, mtarg->my)))
	return 0;

    if (mtarg == &youmonst) {
	tx = mtmp->mux;
	ty = mtmp->muy;
    } else {
	tx = mtarg->mx;
	ty = mtarg->my;
    }

    if (dist2(tx, ty, mtmp->mx, mtmp->my) > BOLT_LIM * BOLT_LIM)
	return 0;

    if (!(msp = getmspells(mtmp)))
	return 0;

    for ( ; msp->spell != STRANGE_OBJECT; msp++) {
	int score;

	if (msp->escape) 
	    continue;

	count++;
	if ((chance -= msp->pref) < 0) {
	    bestscore = 1;
	    bestspell = msp;
	    break;
	}
    }

    /* Have we chosen a spell? */
    if (bestscore <= 0)
	return 0;

    return mcast_attk_spell(mtmp, bestspell, mtarg);
}



/* feedback when frustrated monster couldn't cast a spell */
STATIC_OVL
void
cursetxt(mtmp, undirected)
struct monst *mtmp;
boolean undirected;
{
	if (canseemon(mtmp) && couldsee(mtmp->mx, mtmp->my)) {
	    const char *point_msg;  /* spellcasting monsters are impolite */

	    if (undirected)
		point_msg = "all around, then curses";
	    else if ((Invis && !perceives(mtmp->data) &&
			(mtmp->mux != u.ux || mtmp->muy != u.uy)) ||
		    (youmonst.m_ap_type == M_AP_OBJECT &&
			youmonst.mappearance == STRANGE_OBJECT) ||
		    u.uundetected)
		point_msg = "and curses in your general direction";
	    else if (Displaced && (mtmp->mux != u.ux || mtmp->muy != u.uy))
		point_msg = "and curses at your displaced image";
	    else
		point_msg = "at you, then curses";

	    pline("%s points %s.", Monnam(mtmp), point_msg);
	} else if ((!(moves % 4) || !rn2(4))) {
	    if (flags.soundok) Norep("You hear a mumbled curse.");
	}
}

#endif /* OVL0 */
#ifdef OVLB

/* convert a level based random selection into a specific mage spell;
   inappropriate choices will be screened out by spell_would_be_useless() */
STATIC_OVL int
choose_magic_spell(spellval)
int spellval;
{

    switch (spellval) {
    case 22:
	if (!rn2(25)) return MGC_DIVINE_WRATH; /* waaaay too overpowered, so this will appear much more rarely --Amy */
	else return MGC_ENRAGE;
    case 21:
	return MGC_ENRAGE; /* we reduce the risk of getting a touch of death */
    case 20:
	return MGC_DEATH_TOUCH;
    case 19:
    case 18:
	if (!rn2(25)) return MGC_SUMMON_GHOST; /* Should be rare --Amy */
	else return MGC_CLONE_WIZ;
    case 17:
    case 16:
	return MGC_HASTE_SELF;
    case 15:
	if (!rn2(2)) return MGC_AGGRAVATION;
	else return MGC_SUMMON_MONS;
    case 14:
    case 13:
	return MGC_AGGRAVATION;
    case 12:
	return MGC_CREATE_POOL;
    case 11:
    case 10:
	return MGC_CURSE_ITEMS;
    case 9:
	if (!rn2(2)) return MGC_AGGRAVATION;
	else return MGC_CALL_UNDEAD;
    case 8:
	if (!rn2(4)) return MGC_WITHER;
	else if (!rn2(2)) return MGC_DAMAGE_ARMR;
	else return MGC_DESTRY_ARMR;
    case 7:
    case 6:
	return MGC_WEAKEN_YOU;
    case 5:
    case 4:
	return MGC_DISAPPEAR;
    case 3:
	return MGC_STUN_YOU;
    case 2:
	return MGC_HASTE_SELF;
    case 1:
	return MGC_CURE_SELF;
    case 0:
    default:
	return MGC_PSI_BOLT;
    }
}

/* convert a level based random selection into a specific mage spell for
 * a monster at monster spell */
STATIC_OVL int
m_choose_magic_spell(spellval)
int spellval;
{
    switch (spellval) {
    case 22:
    case 21:
    case 20:
	return MGC_DEATH_TOUCH;
    case 8:
    case 7:
    case 6:
	return MGC_WEAKEN_YOU;
    case 4:
    case 3:
	return MGC_STUN_YOU;
    default:
	return MGC_PSI_BOLT;
    }
}

/* convert a level based random selection into a specific cleric spell */
STATIC_OVL int
choose_clerical_spell(spellnum)
int spellnum;
{

    switch (spellnum) {
    case 13:
	/*if (rn2(10)) */return CLC_GEYSER;
	/*else return CLC_PETRIFY;*/ /* this is incorporated into CLC_GEYSER now, see below --Amy */
    case 12:
	return CLC_FIRE_PILLAR;
    case 11:
	return CLC_LIGHTNING;
    case 10:
    case 9:
	return CLC_CURSE_ITEMS;
    case 8:
	if (rn2(2)) return CLC_RANDOM;
	else if (!rn2(3)) return CLC_AGGRAVATION;
	else return CLC_INSECTS;
    case 7:
    case 6:
	return CLC_BLIND_YOU;
    case 5:
    case 4:
	return CLC_PARALYZE;
    case 3:
    case 2:
	return CLC_CONFUSE_YOU;
    case 1:
	return CLC_CURE_SELF;
    case 0:
    default:
	return CLC_OPEN_WOUNDS;
    }
}

STATIC_OVL int
m_choose_clerical_spell(spellnum)
int spellnum;
{
    switch (spellnum) {
    case 13:
	return CLC_GEYSER;
    case 12:
	return CLC_FIRE_PILLAR;
    case 11:
	return CLC_LIGHTNING;
    case 7:
    case 6:
	return CLC_BLIND_YOU;
    case 5:
    case 4:
	return CLC_PARALYZE;
    case 3:
    case 2:
	return CLC_CONFUSE_YOU;
    case 1:
	return -1;
    case 0:
    default:
	return CLC_OPEN_WOUNDS;
    }
}


/* return values:
 * 1: successful spell
 * 0: unsuccessful spell
 */
int
castmu(mtmp, mattk, thinks_it_foundyou, foundyou)
	register struct monst *mtmp;
	register struct attack *mattk;
	boolean thinks_it_foundyou;
	boolean foundyou;
{
	int	dmg, ml = mtmp->m_lev;
	int ret = 0;
	int spellnum = 0;
	int spellev, chance, difficulty, splcaster, learning;

 	if (!mtmp->mpeaceful && 
 			(ret = gcastm(mtmp, mattk, &youmonst)))
 	    return ret;

	/* Three cases:
	 * -- monster is attacking you.  Search for a useful spell.
	 * -- monster thinks it's attacking you.  Search for a useful spell,
	 *    without checking for undirected.  If the spell found is directed,
	 *    it fails with cursetxt() and loss of mspec_used.
	 * -- monster isn't trying to attack.  Select a spell once.  Don't keep
	 *    searching; if that spell is not useful (or if it's directed),
	 *    return and do something else. 
	 * Since most spells are directed, this means that a monster that isn't
	 * attacking casts spells only a small portion of the time that an
	 * attacking monster does.
	 */
	if ((mattk->adtyp == AD_SPEL || mattk->adtyp == AD_CLRC) && ml) {
	    int cnt = 40;

	    do {
		spellnum = rn2(ml);
		/* Casting level is limited by available energy */
		spellev = spellnum / 7 + 1;
		if (spellev > 10) spellev = 10;
		if (spellev * 5 > mtmp->m_en) {
		    spellev = mtmp->m_en / 5;
		    spellnum = (spellev - 1) * 7 + 1;
		}
		if (mattk->adtyp == AD_SPEL)
		    spellnum = choose_magic_spell(spellnum);
		else
		    spellnum = choose_clerical_spell(spellnum);
		/* not trying to attack?  don't allow directed spells */
		if (!thinks_it_foundyou) {
		    if ( (!is_undirected_spell(mattk->adtyp, spellnum) && rn2(250) ) || is_melee_spell(mattk->adtyp, spellnum) || spell_would_be_useless(mtmp, mattk->adtyp, spellnum)) {
			if (foundyou)
			    impossible("spellcasting monster found you and doesn't know it?");
			return 0;
		    }
		    break;
		}
	    } while(--cnt > 0 &&
		    spell_would_be_useless(mtmp, mattk->adtyp, spellnum));
	    if (cnt == 0) return 0;
	} else {
	    /* Casting level is limited by available energy */
	    spellev = ml / 7 + 1;
	    if (spellev > 10) spellev = 10;
	    if (spellev * 5 > mtmp->m_en) {
		spellev = mtmp->m_en / 5;
		ml = (spellev - 1) * 7 + 1;
	    }
	}

	/* monster unable to cast spells? */
	if (mtmp->mcan || mtmp->m_en < 5 || mtmp->mspec_used || !ml) {
	    cursetxt(mtmp, is_undirected_spell(mattk->adtyp, spellnum));
	    return(0);
	}

	if (mattk->adtyp == AD_SPEL || mattk->adtyp == AD_CLRC) {
	    /*
	     * Spell use (especially MGC) is more common in Slash'EM.
	     * Still using mspec_used, just so monsters don't go bonkers.
	     */
#if 0
	    mtmp->mspec_used = 10 - mtmp->m_lev;
	    if (mtmp->mspec_used < 2) mtmp->mspec_used = 2;
#endif
	    mtmp->mspec_used = rn2(15) - mtmp->m_lev;
	    if (mattk->adtyp == AD_SPEL)
		mtmp->mspec_used = mtmp->mspec_used > 0 ? 2 : 0;
	    else if (mtmp->mspec_used < 2) mtmp->mspec_used = 2;
	}

	/* monster can cast spells, but is casting a directed spell at the
	   wrong place?  If so, give a message, and return.  Do this *after*
	   penalizing mspec_used. */
	if (!foundyou && thinks_it_foundyou &&
		!is_undirected_spell(mattk->adtyp, spellnum)) {
	    pline("%s casts a spell at %s!",
		canseemon(mtmp) ? Monnam(mtmp) : "Something",
		levl[mtmp->mux][mtmp->muy].typ == WATER
		    ? "empty water" : "thin air");
	    return(0);
	}

	nomul(0, 0);

	mtmp->m_en -= spellev * 5; /* Use up the energy now */

	/* We should probably do similar checks to what is done for
	 * the player - armor, etc.
	 * Checks for armour and other intrinsic ability change splcaster
	 * Difficulty and experience affect chance
	 * Assume that monsters only cast spells that they know well
	 */
	splcaster = 15 - (mtmp->m_lev / 2); /* Base for a wizard is 5...*/

	if (splcaster < 5) splcaster = 5;
	if (splcaster > 20) splcaster = 20;

	chance = 11 * (mtmp->m_lev > 25 ? 18 : (12 + (mtmp->m_lev / 5)));
	chance++ ;  /* Minimum chance of 1 */

	difficulty = (spellev - 1) * 4 - (mtmp->m_lev - 1);
	    /* law of diminishing returns sets in quickly for
	     * low-level spells.  That is, higher levels quickly
	     * result in almost no gain
	     */
	learning = 15 * (-difficulty / spellev);
	chance += learning > 20 ? 20 : learning;

	/* clamp the chance */
	if (chance < 0) chance = 0;
	if (chance > 120) chance = 120;

	/* combine */
	chance = chance * (20-splcaster) / 15 - splcaster;

	/* Clamp to percentile */
	if (chance > 100) chance = 100;
	if (chance < 0) chance = 0;

#if 0
	if(rn2(ml*10) < (mtmp->mconf ? 100 : 20)) {	/* fumbled attack */
#else
	if (mtmp->mconf || rnd(100) > chance) { /* fumbled attack */
#endif
	    if (canseemon(mtmp) && flags.soundok)
		pline_The("air crackles around %s.", mon_nam(mtmp));
	    return(0);
	}
	if (canspotmon(mtmp) || !is_undirected_spell(mattk->adtyp, spellnum)) {
	    pline("%s casts a spell%s!",
		  canspotmon(mtmp) ? Monnam(mtmp) : "Something",
		  is_undirected_spell(mattk->adtyp, spellnum) ? "" :
		  (Invisible && !perceives(mtmp->data) && 
		   (mtmp->mux != u.ux || mtmp->muy != u.uy)) ?
		  " at a spot near you" :
		  (Displaced && (mtmp->mux != u.ux || mtmp->muy != u.uy)) ?
		  " at your displaced image" :
		  " at you");
	}

/*
 *	As these are spells, the damage is related to the level
 *	of the monster casting the spell.
 */
	if (!foundyou) {
	    /*dmg = 0;*/
	    if (mattk->adtyp != AD_SPEL && mattk->adtyp != AD_CLRC) {
		impossible(
	      "%s casting non-hand-to-hand version of hand-to-hand spell %d?",
			   Monnam(mtmp), mattk->adtyp);
		return(0);
	    }
	} /*else*/ if (mattk->damd)
	    dmg = d((int)((ml/2) + mattk->damn), (int)mattk->damd);
	else dmg = d((int)((ml/2) + 1), 6);
	if (Half_spell_damage) dmg = (dmg+1) / 2;

	ret = 1;

	switch (mattk->adtyp) {

	    case AD_FIRE:
		pline("You're enveloped in flames.");
		if(Fire_resistance) {
			shieldeff(u.ux, u.uy);
			pline("But you resist the effects.");
			dmg = 0;
		}
		if (Slimed) {
			pline("The slime is burned away!");
			Slimed =0;
		}
		burn_away_slime();
		break;
	    case AD_COLD:
		pline("You're covered in frost.");
		if(Cold_resistance) {
			shieldeff(u.ux, u.uy);
			pline("But you resist the effects.");
			dmg = 0;
		}
		break;
	    case AD_MAGM:
		You("are hit by a shower of missiles!");
		if(Antimagic) {
			shieldeff(u.ux, u.uy);
			pline_The("missiles bounce off!");
			dmg = 0;
		}
		break;
	    case AD_SPEL:	/* wizard spell */
	    case AD_CLRC:       /* clerical spell */
	    {
		if (mattk->adtyp == AD_SPEL)
		    cast_wizard_spell(mtmp, dmg, spellnum);
		else
		    cast_cleric_spell(mtmp, dmg, spellnum);
		dmg = 0; /* done by the spell casting functions */
		break;
	    }
	}
	if(dmg) mdamageu(mtmp, dmg);
	return(ret);
}


/* monster wizard and cleric spellcasting functions */
/*
   If dmg is zero, then the monster is not casting at you.
   If the monster is intentionally not casting at you, we have previously
   called spell_would_be_useless() and spellnum should always be a valid
   undirected spell.
   If you modify either of these, be sure to change is_undirected_spell()
   and spell_would_be_useless().
 */
STATIC_OVL
void
cast_wizard_spell(mtmp, dmg, spellnum)
struct monst *mtmp;
int dmg;
int spellnum;
{

	struct monst* mtmp2;
	int seen,count;
	struct edog* edog;

    if (dmg == 0 && !is_undirected_spell(AD_SPEL, spellnum)) {
	impossible("cast directed wizard spell (%d) with dmg=0?", spellnum);
	return;
    }

    switch (spellnum) {
    case MGC_DEATH_TOUCH:
	pline("Oh no, %s's using the touch of death!", mhe(mtmp));
	if (nonliving(youmonst.data) || is_demon(youmonst.data)) {
	    You("seem no deader than before.");
	} else if (!Antimagic && rn2(mtmp->m_lev) > 12) {
	    if (Hallucination) {
		You("have an out of body experience.");
	    } else if (!rnd(50)) {
		killer_format = KILLED_BY_AN;
		killer = "touch of death";
		done(DIED);}
		else {
			dmg = d(8,6);
			/* Magic resistance or half spell damage will cut this in half... */
			/* and also prevent a reduction of maximum hit points */
			if (Antimagic || (Half_spell_damage) ) {
				shieldeff(u.ux, u.uy);
				dmg /= 2;
				u.uhpmax -= dmg/2;
				You("feel a loss of life.");
				losehp(dmg,"touch of death",KILLED_BY_AN);
			}
			else {
			You("feel drained...");
			u.uhpmax -= dmg/2;
			losehp(dmg,"touch of death",KILLED_BY_AN); }

		/*{pline("Whew! That could have been your demise.");*/
	    }
	} else {
	    if (Antimagic) shieldeff(u.ux, u.uy);
	    pline("Lucky for you, it didn't work!");
	}
	dmg = 0;
	break;
    case MGC_CREATE_POOL:
	if (levl[u.ux][u.uy].typ == ROOM || levl[u.ux][u.uy].typ == CORR) {
	    pline("A pool appears beneath you!");
	    levl[u.ux][u.uy].typ = POOL;
	    del_engr_at(u.ux, u.uy);
	    water_damage(level.objects[u.ux][u.uy], FALSE, TRUE);
	    spoteffects(FALSE);  /* possibly drown, notice objects */
	}
	else
	    impossible("bad pool creation?");
	dmg = 0;
	break;
    case MGC_CLONE_WIZ:
	if (mtmp->iswiz && (flags.no_of_wizards == 1 || !rn2(20)) ) { /* let's have a small chance of triple trouble --Amy */
	    if (flags.no_of_wizards == 1) pline("Double Trouble...");
	    else pline("Triple Trouble...");
	    clonewiz();
	    dmg = 0;
	} else
	    pline("For a moment you saw another Wizard, but it disappeared.");
	break;
    case MGC_SUMMON_MONS:
    {
	int count;

	count = nasty(mtmp);	/* summon something nasty */
	if (mtmp->iswiz)
	    verbalize("Destroy the thief, my pet%s!", plur(count));
	else {
	    const char *mappear =
		(count == 1) ? "A monster appears" : "Monsters appear";

	    /* messages not quite right if plural monsters created but
	       only a single monster is seen */
	    if (Invisible && !perceives(mtmp->data) &&
				    (mtmp->mux != u.ux || mtmp->muy != u.uy))
		pline("%s around a spot near you!", mappear);
	    else if (Displaced && (mtmp->mux != u.ux || mtmp->muy != u.uy))
		pline("%s around your displaced image!", mappear);
	    else
		pline("%s from nowhere!", mappear);
	}
	dmg = 0;
	break;
    }
    case MGC_SUMMON_GHOST:
    {
		coord mm;   
		mm.x = u.ux;   
		mm.y = u.uy;   

	tt_mname(&mm, FALSE, 0);	/* create player-monster ghosts */
	if (mtmp->iswiz)
	    verbalize("Destroy the thief, my pets!");
	else {
	    const char *mappear =
		"Ghosts appear";

	    /* messages not quite right if plural monsters created but
	       only a single monster is seen */
	    if (Invisible && !perceives(mtmp->data) &&
				    (mtmp->mux != u.ux || mtmp->muy != u.uy))
		pline("%s around a spot near you!", mappear);
	    else if (Displaced && (mtmp->mux != u.ux || mtmp->muy != u.uy))
		pline("%s around your displaced image!", mappear);
	    else
		pline("%s from nowhere!", mappear);
	}
	dmg = 0;
	break;
    }


	case MGC_CALL_UNDEAD:
	{
		coord mm;   
		mm.x = u.ux;   
		mm.y = u.uy;   
		pline("Undead creatures are called forth from the grave!");   
		mkundead(&mm, FALSE, 0);   
	}
	dmg = 0;   
	break;   
    case MGC_AGGRAVATION:
	You_feel("that monsters are aware of your presence.");
	aggravate();
	dmg = 0;
	break;
    case MGC_CURSE_ITEMS:
	You_feel("as if you need some help.");
	rndcurse();
	dmg = 0;
	break;
    case MGC_DESTRY_ARMR:
	if (Antimagic) {
	    shieldeff(u.ux, u.uy);
	    pline("A field of force surrounds you!");
	} else if (!destroy_arm(some_armor(&youmonst))) {
	    Your("skin itches.");
	}
	dmg = 0;
	break;

	/* from Sporkhack */
	/* Inspire critters to fight a little more vigorously...
	 *
	 * -- Peaceful critters may become hostile.
	 * -- Hostile critters may become berserk.
	 * -- Borderline tame critters, or tame critters
	 *    who have been treated poorly may ALSO become hostile!
	 */
	 case MGC_ENRAGE:
		for (mtmp2 = fmon; mtmp2; mtmp2 = mtmp2->nmon) {
			if (m_cansee(mtmp,mtmp2->mx,mtmp2->my) && rn2(3) &&
					mtmp2 != mtmp && distu(mtmp2->mx,mtmp2->my) < 16) {
				seen++;
				if (mtmp2->mtame) {
					edog = (mtmp2->isminion) ? 0 : EDOG(mtmp2);
					if (mtmp2->mtame <= /*3*/rnd(21) || (edog && edog->abuse >= /*5*/ rn2(6) )) {
						mtmp2->mtame = mtmp2->mpeaceful = 0;
						if (mtmp2->mleashed) { m_unleash(mtmp2,FALSE); }
						count++;
					}
				} else if (mtmp2->mpeaceful) {
					mtmp2->mpeaceful = 0;
					count++;
				} else {
					/*mtmp2->mberserk = 1;*/ /* removed because this attribute doesn't exist in this fork */
					mtmp2->mhp = mtmp2->mhpmax; /* let's heal them instead --Amy */
					count++;
				}
			}
		}
		/* Don't yell if we didn't see anyone to yell at. */
		if (seen && (!rn2(3) || mtmp->iswiz)) {
			verbalize("Get %s, you fools, or I'll have your figgin on a stick!",uhim());
		}
		if (count) {
			pline("It seems a little more dangerous here now...");
			doredraw();
		}
		dmg = 0;
		break;
    case MGC_DIVINE_WRATH: /* new idea by Amy. Yes, this is very evil. :D */

		u.ugangr++;
		if (!rn2(5)) u.ugangr++;
		if (!rn2(25)) u.ugangr++;
		prayer_done();

		dmg = 0;
		break;

    case MGC_WITHER:
	if (Antimagic) {
	    shieldeff(u.ux, u.uy);
	    pline("A field of force surrounds you!");
	} else {
	    pline("You sense a sinister feeling of loss!");

	while (1) {
	    switch(rn2(5)) {
	    case 0:
		if (!uarmh || !wither_dmg(uarmh, xname(uarmh), rn2(4), FALSE, &youmonst))
			continue;
		break;
	    case 1:
		if (uarmc) {
		    (void)wither_dmg(uarmc, xname(uarmc), rn2(4), TRUE, &youmonst);
		    break;
		}
		/* Note the difference between break and continue;
		 * break means it was hit and didn't rust; continue
		 * means it wasn't a target and though it didn't rust
		 * something else did.
		 */
		if (uarm)
		    (void)wither_dmg(uarm, xname(uarm), rn2(4), TRUE, &youmonst);
#ifdef TOURIST
		else if (uarmu)
		    (void)wither_dmg(uarmu, xname(uarmu), rn2(4), TRUE, &youmonst);
#endif
		break;
	    case 2:
		if (!uarms || !wither_dmg(uarms, xname(uarms), rn2(4), FALSE, &youmonst))
		    continue;
		break;
	    case 3:
		if (!uarmg || !wither_dmg(uarmg, xname(uarmg), rn2(4), FALSE, &youmonst))
		    continue;
		break;
	    case 4:
		if (!uarmf || !wither_dmg(uarmf, xname(uarmf), rn2(4), FALSE, &youmonst))
		    continue;
		break;
		}
	    break; /* Out of while loop */
	    }
	}
	dmg = 0;
	break;
    case MGC_DAMAGE_ARMR:
	if (Antimagic) {
	    shieldeff(u.ux, u.uy);
	    pline("A field of force surrounds you!");
	} else {
	    pline("Your body shakes!");

	while (1) {
	    switch(rn2(5)) {
	    case 0:
		if (!uarmh || !rust_dmg(uarmh, xname(uarmh), rn2(4), FALSE, &youmonst))
			continue;
		break;
	    case 1:
		if (uarmc) {
		    (void)rust_dmg(uarmc, xname(uarmc), rn2(4), TRUE, &youmonst);
		    break;
		}
		/* Note the difference between break and continue;
		 * break means it was hit and didn't rust; continue
		 * means it wasn't a target and though it didn't rust
		 * something else did.
		 */
		if (uarm)
		    (void)rust_dmg(uarm, xname(uarm), rn2(4), TRUE, &youmonst);
#ifdef TOURIST
		else if (uarmu)
		    (void)rust_dmg(uarmu, xname(uarmu), rn2(4), TRUE, &youmonst);
#endif
		break;
	    case 2:
		if (!uarms || !rust_dmg(uarms, xname(uarms), rn2(4), FALSE, &youmonst))
		    continue;
		break;
	    case 3:
		if (!uarmg || !rust_dmg(uarmg, xname(uarmg), rn2(4), FALSE, &youmonst))
		    continue;
		break;
	    case 4:
		if (!uarmf || !rust_dmg(uarmf, xname(uarmf), rn2(4), FALSE, &youmonst))
		    continue;
		break;
		}
	    break; /* Out of while loop */
	    }
	}
	dmg = 0;
	break;
    case MGC_WEAKEN_YOU:		/* drain strength */
	if (Antimagic) {
	    shieldeff(u.ux, u.uy);
	    You_feel("momentarily weakened.");
	} else {
	    You("suddenly feel weaker!");
	    dmg = mtmp->m_lev - 6;
	    if (Half_spell_damage) dmg = (dmg + 1) / 2;
	    losestr(rnd(dmg));
	    if (u.uhp < 1)
		done_in_by(mtmp);
	}
	dmg = 0;
	break;
    case MGC_DISAPPEAR:		/* makes self invisible */
	if (!mtmp->minvis && !mtmp->invis_blkd) {
	    if (canseemon(mtmp))
		pline("%s suddenly %s!", Monnam(mtmp),
		      !See_invisible ? "disappears" : "becomes transparent");
	    mon_set_minvis(mtmp, FALSE);
	    dmg = 0;
	    begin_invis(mtmp, 20 + rnd(20));
	} else
	    impossible("no reason for monster to cast disappear spell?");
	break;
    case MGC_STUN_YOU:
	if ((Antimagic || Free_action)) {
	    shieldeff(u.ux, u.uy);
	    if (!Stunned)
		You_feel("momentarily disoriented.");
	    make_stunned(1L, FALSE);
	} else {
	    You(Stunned ? "struggle to keep your balance." : "reel...");
	    dmg = d(ACURR(A_DEX) < 12 ? 6 : 4, 4);
	    if (Half_spell_damage) dmg = (dmg + 1) / 2;
	    make_stunned(HStun + dmg, FALSE);
	}
	dmg = 0;
	break;
    case MGC_HASTE_SELF:
	mon_adjust_speed(mtmp, 4, (struct obj *)0);
	begin_speed(mtmp, 20 + rnd(20));
	dmg = 0;
	break;
    case MGC_CURE_SELF:
	if (mtmp->mhp < mtmp->mhpmax) {
	    if (canseemon(mtmp))
		pline("%s looks better.", Monnam(mtmp));
	    /* note: player healing does 6d4; this used to do 1d8 */
	    if ((mtmp->mhp += d(3,6)) > mtmp->mhpmax)
		mtmp->mhp = mtmp->mhpmax;
	    dmg = 0;
	}
	break;
    case MGC_PSI_BOLT:
	/* prior to 3.4.0 Antimagic was setting the damage to 1--this
	   made the spell virtually harmless to players with magic res. */
	if (Antimagic) {
	    shieldeff(u.ux, u.uy);
	    dmg = (dmg + 1) / 2;
	}
	if (dmg <= 5)
	    You("get a slight %sache.", body_part(HEAD));
	else if (dmg <= 10)
	    Your("brain is on fire!");
	else if (dmg <= 20)
	    Your("%s suddenly aches painfully!", body_part(HEAD));
	else
	    Your("%s suddenly aches very painfully!", body_part(HEAD));
	break;
    default:
	impossible("mcastu: invalid magic spell (%d)", spellnum);
	dmg = 0;
	break;
    }

    if (dmg) mdamageu(mtmp, dmg);
}

STATIC_OVL
int
damagem(mtmp, mdef, dmg)
struct monst *mtmp, *mdef;
int dmg;
{
    if(DEADMONSTER(mdef) || (mdef->mhp -= dmg) < 1) {
	if (m_at(mdef->mx, mdef->my) == mtmp) {  /* see gulpmm() */
	    remove_monster(mdef->mx, mdef->my);
	    /* otherwise place_monster will complain */
	    mdef->mhp = 1;	
	    place_monster(mdef, mdef->mx, mdef->my);
	    mdef->mhp = 0;
	}
	/* AD_PHYS is okay here, since digest, disintegration attacks won't
	 * use damagem() anyway */
	monkilled(mdef, "", AD_PHYS);

	if (mdef->mhp > 0) return 0; /* mdef lifesaved */
	return (MM_DEF_DIED |
		((mtmp->mhp > 0 && grow_up(mtmp,mdef)) ? 0 : MM_AGR_DIED));
    }
}

STATIC_OVL 
boolean
resists_attk(mdef, magr)
/* No null checks on these */
struct monst *mdef, *magr;
{
	int alevel = magr->m_lev,
	    dlevel = mdef->m_lev;
	int chance;
	if (alevel < 1)
	    alevel = 1;
	if (alevel > 50)
	    alevel = 50;
	if (dlevel < 1)
	    dlevel = 1;
	if (dlevel > 50)
	    dlevel = 50;
	chance = 100 + alevel - dlevel;
	return mdef->data->mr > (chance > 0? rn2(chance) : 0);
}

STATIC_OVL
void
m_cast_wizard_spell(mtmp, mdef, dmg, spellnum)
struct monst *mtmp, *mdef;
int dmg;
int spellnum;
{
    int sees_agr = canseemon(mtmp), sees_def = canseemon(mdef);

    if (dmg == 0 && !is_undirected_spell(AD_SPEL, spellnum)) {
	impossible("cast directed wizard spell (%d) with dmg=0?", spellnum);
	return;
    }

    switch (spellnum) {
    case MGC_DEATH_TOUCH:
	if (sees_agr)
	    pline("%s is using the touch of death!", Monnam(mtmp));
	dmg = 0;
	if (nonliving(mdef->data) || is_demon(mdef->data)) {
	    if (sees_def)
	    	pline("%s seems no deader than before.", Monnam(mdef));
	} else if (!resists_magm(mdef)) {
	    dmg = mdef->mhp;
	} else {
	    shieldeff(mdef->mx, mdef->my);
	    if (sees_def && sees_agr)
		pline("That didn't work...");
	}
	break;
    case MGC_WEAKEN_YOU:		/* drain strength */
	/* For monster-monster combat, drain levels instead */
	/* D: Angelic beings won't drain levels */
	if (mtmp->data->mlet != S_ANGEL) {
	    /* D: In case level-drain fails */
	    dmg = 0;
	    if (!resists_drli(mdef) && !resists_attk(mdef, mtmp)) {
		/* D: Might drain up to 3 levels */
		int nlev = rnd(3);

		dmg = d(2 * nlev, 6);
		if (sees_def)
		    pline("%s suddenly seems %sweaker!", 
				    Monnam(mdef),
				    ((nlev > 1)? "a lot " : ""));
		if ((mdef->mhpmax -= dmg) < 1)
		    mdef->mhpmax = 1;
		/* D: hp itself is drained at the end */
		while (nlev--)
		    if (mdef->m_lev == 0) {
			dmg = mdef->mhp;
			mdef->mhpmax = 1;
			break;
		    }
		    else mdef->m_lev--;
		    /* Automatic kill if drained past level 0 */
	    }
	}

	break;
    case MGC_STUN_YOU:
	if (!resists_magm(mdef) && !resists_attk(mdef, mtmp)) {
	    if (sees_def && !unsolid(mdef->data) 
			&& !is_whirly(mdef->data) && !amorphous(mdef->data))
		pline("%s reels...", Monnam(mdef));
	    dmg = d(4, 4);
	    mdef->mstun = 1;
	}
	break;
    case MGC_PSI_BOLT:
	/* D: This is way too common - make it less so */
	if (rn2(3)) {
	    dmg = 0;
	    break;
	}

	if (resists_magm(mdef))
	    dmg = (dmg + 1) / 2;

	if (mindless(mdef->data))
	    dmg = 0;

	if (sees_agr && sees_def) {
	    char buf[BUFSZ];
	    strcpy(buf, mon_nam(mdef));
	    pline("%s casts a psi-bolt at %s!", Monnam(mtmp), buf);
	    if (dmg == 0)
		pline("%s seems unharmed.", Monnam(mdef));
	}
	
	break;
    default:
	dmg = 0;
	break;
    }

    if (dmg) damagem(mtmp, mdef, dmg);
}

STATIC_OVL
void
cast_cleric_spell(mtmp, dmg, spellnum)
struct monst *mtmp;
int dmg;
int spellnum;
{
	int aligntype;

    if (dmg == 0 && !is_undirected_spell(AD_CLRC, spellnum)) {
	impossible("cast directed cleric spell (%d) with dmg=0?", spellnum);
	return;
    }

    switch (spellnum) {
    case CLC_GEYSER:

	switch (rnd(37) ) {
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:

	/* this is physical damage, not magical damage */
	pline("A sudden geyser slams into you from nowhere!");
	dmg = d(8, 6);
	water_damage(invent, FALSE, FALSE); /* Come on, every other source of water rusts your stuff. --Amy */
	if (level.flags.lethe) lethe_damage(invent, FALSE, FALSE);
	if (Half_physical_damage) dmg = (dmg + 1) / 2;
	break;

	case 20:
	/* antimatter storm --Amy */
	pline("You are caught in an antimatter storm!");
	dmg = d(8, 6);
	withering_damage(invent, FALSE, FALSE); /* This can potentially damage all of your inventory items. --Amy */
	if (Half_physical_damage) dmg = (dmg + 1) / 2;

	break;

	case 21:
	case 22:
	case 23:
	case 24:
	case 25:

		/* petrify - similar to cockatrice hissing --Amy */
		pline("You feel a massive burden on your chest!");
		if (!Stoned && !Stone_resistance && !(poly_when_stoned(youmonst.data) &&
				 polymon(PM_STONE_GOLEM)) ) Stoned = 7;
		dmg = 0;
		break;

	case 26:
	case 27:
	case 28:
	case 29:
	case 30:

		/* sliming - similar to green slime attack --Amy */
		    if (!Slimed && !flaming(youmonst.data) && !Unchanging && youmonst.data != &mons[PM_GREEN_SLIME])
		 {You("don't feel very well.");
		    Slimed = 100L;
		    flags.botl = 1;}
		dmg = 0;
		break;

	case 31:
	case 32:
	case 33:
	case 34:
	case 35:
	case 36:
	case 37:

	/* -1 - A_CHAOTIC, 0 - A_NEUTRAL, 1 - A_LAWFUL */
		aligntype = rn2((int)A_LAWFUL+2) - 1;
		 pline("A servant of %s appears!",aligns[1 - aligntype].noun); /* summon elm, from sporkhack */
		summon_minion(aligntype, TRUE);
		dmg = 0;
		 break;


	default: /*failsafe*/
		You_feel("that monsters are aware of your presence."); /* aggravate monster */
		aggravate();
		dmg = 0;
		break;
	}
	break;

    case CLC_FIRE_PILLAR:
	pline("A pillar of fire strikes all around you!");
	if (Fire_resistance) {
	    shieldeff(u.ux, u.uy);
	    dmg = 0;
	} else
	    dmg = d(8, 6);
	if (Half_spell_damage) dmg = (dmg + 1) / 2;
	burn_away_slime();
	(void) burnarmor(&youmonst);
		    if (!rn2(15)) /* new calculations --Amy */	destroy_item(SCROLL_CLASS, AD_FIRE);
		    if (!rn2(15)) /* new calculations --Amy */	destroy_item(POTION_CLASS, AD_FIRE);
		    if (!rn2(15)) /* new calculations --Amy */	destroy_item(SPBOOK_CLASS, AD_FIRE);
	(void) burn_floor_paper(u.ux, u.uy, TRUE, FALSE);
	break;
    case CLC_LIGHTNING:
    {
	boolean reflects;

	/* WAC add lightning strike effect */
	zap_strike_fx(u.ux, u.uy, AD_ELEC - 1);
	pline("A bolt of lightning strikes down at you from above!");
	reflects = ureflects("It bounces off your %s%s.", "");
	if (!Blind) {
	    pline("You are blinded by the flash!");
	    make_blinded(Half_spell_damage ? 10L : 20L, FALSE);
	}
	if (reflects || Shock_resistance) {
	    shieldeff(u.ux, u.uy);
	    dmg = 0;
	    if (reflects)
		break;
	} else
	    dmg = d(8, 6);
	if (Half_spell_damage) dmg = (dmg + 1) / 2;
		    if (!rn2(15)) /* new calculations --Amy */	destroy_item(WAND_CLASS, AD_ELEC);
		    if (!rn2(15)) /* new calculations --Amy */	destroy_item(RING_CLASS, AD_ELEC);
	break;
    }
    case CLC_CURSE_ITEMS:
	You_feel("as if you need some help.");
	rndcurse();
	dmg = 0;
	break;

    case CLC_AGGRAVATION: /* aggravate monster */
	You_feel("that monsters are aware of your presence."); /* aggravate monster */
	aggravate();
	dmg = 0;
	break;

    case CLC_RANDOM: /* inofficial names see below */
	switch (rnd(38) ) {

	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 8:
	case 9:
	case 10:
		You_feel("a sense of intrinsic loss."); /* intrinsic loss */
	    attrcurse();
		dmg = 0;
		break;
	case 11:
	case 12:
	case 13:
	case 14:
	case 15:
	case 16:
	case 17:
	case 18:
	case 19:
	case 20:
		You_feel("a dark aura."); /* dark aura */
		int rangeX;

		rangeX = (rnd(1 + mtmp->m_lev)) / 2;
		if (rn2(4)) rangeX = rangeX / 2;

		if (rangeX < 1) rangeX = 1; if (rangeX > 15) rangeX = 15; 

	    do_clear_areaX(u.ux,u.uy,		/* darkness around player's position */
		/*15*/rangeX, set_litZ, (genericptr_t)((char *)0));
		    wake_nearby();
		dmg = 0;
                break;
	case 21:
	case 22:
	case 23:
		You_feel("very trippy all of a sudden!"); /* acid trip */
		int duratX;
		duratX = (rnd(1 + mtmp->m_lev));
		make_hallucinated(HHallucination + duratX,FALSE,0L);
		dmg = 0;
		break;
	case 24:
	case 25:
	      You_feel("endangered!!"); /* create traps */

			int rtrap;
		    int i, j, bd;
			bd = 1;
			if (!rn2(5)) bd += rnd(1);

		      for (i = -bd; i <= bd; i++) for(j = -bd; j <= bd; j++) {
				if (!isok(u.ux + i, u.uy + j)) continue;
				if ((levl[u.ux + i][u.uy + j].typ != ROOM && levl[u.ux + i][u.uy + j].typ != CORR) || MON_AT(u.ux + i, u.uy + j)) continue;
				if (t_at(u.ux + i, u.uy + j)) continue;

			      rtrap = rnd(TRAPNUM-1);
				if (rtrap == HOLE) rtrap = PIT;
				if (rtrap == MAGIC_PORTAL) rtrap = PIT;
				if (rtrap == TRAPDOOR && !Can_dig_down(&u.uz)) rtrap = PIT;
				if (rtrap == LEVEL_TELEP && level.flags.noteleport) rtrap = SQKY_BOARD;
				if (rtrap == TELEP_TRAP && level.flags.noteleport) rtrap = SQKY_BOARD;
				if (rtrap == ROLLING_BOULDER_TRAP) rtrap = ROCKTRAP;
				if (rtrap == NO_TRAP) rtrap = ARROW_TRAP;

				(void) maketrap(u.ux + i, u.uy + j, rtrap);
			}
		dmg = 0;
		break;
	case 26:
	      You_feel("yanked in a new direction!"); /* relocation */
		(void) safe_teleds(FALSE);
		dmg = 0;
		break;
	case 27:
	case 28:
	case 29:
	      pline("Your mana is sapped!"); /* mana drain */
		int manastealX;
		manastealX = (rnd(1 + mtmp->m_lev));
		drain_en(manastealX);
		dmg = 0;
		break;
	case 30:
	      You_feel("an ominous droning wind!"); /* disengrave */
		register struct engr *ep = engr_at(u.ux,u.uy);
	      if (ep && sengr_at("Elbereth", u.ux, u.uy) ) {
		pline("Suddenly, the engraving beneath your feet smudges and dissolves!");
	      del_engr(ep);
	      ep = (struct engr *)0;
		}
		dmg = 0;
		break;
	case 31:
	case 32:
	case 33:
	      pline("Your hands start trembling!"); /* disarm */
		int glibberX;
		glibberX = (rnd(1 + mtmp->m_lev));
		    incr_itimeout(&Glib, glibberX);
		dmg = 0;
		break;
	case 34:
	case 35:
	case 36:
	      pline("You feel pulsating winds!"); /* slow */
		    u_slow_down();
		dmg = 0;
		break;
	case 37:
	      pline("You feel out of luck!"); /* dementor force */
			change_luck(-1);
			if (!rn2(10)) change_luck(-5);
			adjalign(-10);
			if (!rn2(10)) adjalign(-50);
		dmg = 0;
		break;
	case 38:
	      pline("You feel burdened"); /* punishment - message is from Castle of the Winds */
			punishx();
		dmg = 0;
		break;
	default: /*failsafe*/
		You_feel("that monsters are aware of your presence."); /* aggravate monster */
		aggravate();
		dmg = 0;
		break;
	}


	break;

    case CLC_INSECTS:
      {
	/* Try for insects, and if there are none
	   left, go for (sticks to) snakes.  -3. */
	struct permonst *pm = mkclass(S_ANT,0);
	struct monst *mtmp2 = (struct monst *)0;
	char let = (pm ? S_ANT : S_SNAKE);
	boolean success;
	int i;
	coord bypos;
	int quan;

	/* Let's allow some variation. Unofficial spell names for each type of creature see below. --Amy */

	if (!rn2(10)) { switch (rnd(54)) {

		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
			let = S_SNAKE;
			break;
		case 11:
		case 12:
		case 13:
		case 14:
		case 15:
			let = S_MIMIC;
			break;
		case 16:
		case 17:
		case 18:
		case 19:
		case 20:
			let = S_NYMPH;
			break;
		case 21:
		case 22:
		case 23:
			let = S_PIERCER;
			break;
		case 24:
		case 25:
		case 26:
		case 27:
		case 28:
			let = S_RODENT;
			break;
		case 29:
		case 30:
		case 31:
		case 32:
		case 33:
			let = S_SPIDER;
			break;
		case 34:
		case 35:
		case 36:
		case 37:
		case 38:
			let = S_WORM;
			break;
		case 39:
		case 40:
		case 41:
		case 42:
		case 43:
			let = S_BAT;
			break;
		case 44:
			let = S_UMBER;
			break;
		case 45:
		case 46:
			let = S_HUMAN;
			break;
		case 47:
		case 48:
			let = S_GOLEM;
			break;
		case 49:
		case 50:
		case 51:
			let = S_EEL;
			break;
		case 52:
		case 53:
		case 54:
			let = S_BAD_FOOD;
			break;
		default: /* failsafe */
			let = S_ANT;
			break;

		}

	}

	quan = (mtmp->m_lev < 2) ? 1 : rnd((int)mtmp->m_lev / 2);
	if (rn2(3)) quan = (quan / 2);
	if (quan < 1) quan = 1;
	success = pm ? TRUE : FALSE;
	for (i = 0; i <= quan; i++) {
	    if (!enexto(&bypos, mtmp->mux, mtmp->muy, mtmp->data))
		break;
	    if ((pm = mkclass(let,0)) != 0 &&
		    (mtmp2 = makemon(pm, bypos.x, bypos.y, NO_MM_FLAGS)) != 0) {
		success = TRUE;
		mtmp2->msleeping = mtmp2->mpeaceful = mtmp2->mtame = 0;
		set_malign(mtmp2);
	    }
	}
	/* Not quite right:
         * -- message doesn't always make sense for unseen caster (particularly
	 *    the first message)
         * -- message assumes plural monsters summoned (non-plural should be
         *    very rare, unlike in nasty())
         * -- message assumes plural monsters seen
         */
	if (!success)
	    pline("%s casts at a clump of sticks, but nothing happens.",
		Monnam(mtmp));
	else if (let == S_SNAKE) /* sticks to snakes */
	    pline("%s transforms a clump of sticks into snakes!",
		Monnam(mtmp));
	else if (let == S_MIMIC) /* garbage spam */
	    pline("%s conjures up random garbage!",
		Monnam(mtmp));
	else if (let == S_NYMPH) /* charming seduction */
	    pline("%s summons some beautiful ladies!",
		Monnam(mtmp));
	else if (let == S_PIERCER) /* piercing ceiling */
	    pline("You hear a shrill ringing sound.",
		Monnam(mtmp));
	else if (let == S_RODENT) /* rat swarm */
	    pline("%s summons rats!",
		Monnam(mtmp));
	else if (let == S_SPIDER) /* demonic spiders */
	    pline("%s summons spiders!",
		Monnam(mtmp));
	else if (let == S_WORM) /* can of worms */
	    pline("%s opens up a can of worms!",
		Monnam(mtmp));
	else if (let == S_BAT) /* flapping bats */
	    pline("%s summons a bat swarm!",
		Monnam(mtmp));
	else if (let == S_UMBER) /* hulking brutes */
	    pline("%s summons terrifying creatures!",
		Monnam(mtmp));
	else if (let == S_HUMAN) /* court summoning */
	    pline("%s summons interesting people from around the world!",
		Monnam(mtmp));
	else if (let == S_GOLEM) /* golem mastery */
	    pline("%s transforms a heap of junk into animated beings!",
		Monnam(mtmp));
	else if (let == S_EEL) /* deep sea trouble */
	    pline("%s opens up a can of whoop-ass!",
		Monnam(mtmp));
	else if (let == S_BAD_FOOD) /* mystic nature */
	    pline("%s summons mystic natures!",
		Monnam(mtmp));
	else if (Invisible && !perceives(mtmp->data) &&
				(mtmp->mux != u.ux || mtmp->muy != u.uy))
	    pline("%s summons insects around a spot near you!",
		Monnam(mtmp));
	else if (Displaced && (mtmp->mux != u.ux || mtmp->muy != u.uy))
	    pline("%s summons insects around your displaced image!",
		Monnam(mtmp));
	else
	    pline("%s summons insects!", Monnam(mtmp));
	dmg = 0;
	break;
      }
    case CLC_BLIND_YOU:
	/* note: resists_blnd() doesn't apply here */
	if (!Blinded) {
	    int num_eyes = eyecount(youmonst.data);
	    pline("Scales cover your %s!",
		  (num_eyes == 1) ?
		  body_part(EYE) : makeplural(body_part(EYE)));
	    make_blinded(Half_spell_damage ? 100L : 200L, FALSE);
	    if (!Blind) Your(vision_clears);
	    dmg = 0;
	} else
	    impossible("no reason for monster to cast blindness spell?");
	break;
    case CLC_PARALYZE:
	if ((Antimagic || Free_action)) {
	    shieldeff(u.ux, u.uy);
	    if (multi >= 0)
		You("stiffen briefly.");
	    nomul(-1, "paralyzed by a monster spell");
	} else {
	    if (multi >= 0)
		You("are frozen in place!");
	    dmg = 4 + (int)mtmp->m_lev;
	    if (Half_spell_damage) dmg = (dmg + 1) / 2;
	    nomul(-dmg, "paralyzed by a monster spell");
	}
	nomovemsg = 0;
	dmg = 0;
	break;
    case CLC_CONFUSE_YOU:
	if (Antimagic) {
	    shieldeff(u.ux, u.uy);
	    You_feel("momentarily dizzy.");
	} else {
	    boolean oldprop = !!Confusion;

	    dmg = (int)mtmp->m_lev;
	    if (Half_spell_damage) dmg = (dmg + 1) / 2;
	    make_confused(HConfusion + dmg, TRUE);
	    if (Hallucination)
		You_feel("%s!", oldprop ? "trippier" : "trippy");
	    else
		You_feel("%sconfused!", oldprop ? "more " : "");
	}
	dmg = 0;
	break;
    case CLC_CURE_SELF:
	if (mtmp->mhp < mtmp->mhpmax) {
	    if (canseemon(mtmp))
		pline("%s looks better.", Monnam(mtmp));
	    /* note: player healing does 6d4; this used to do 1d8 */
	    if ((mtmp->mhp += d(3,6)) > mtmp->mhpmax)
		mtmp->mhp = mtmp->mhpmax;
	    dmg = 0;
	}
	break;
    case CLC_OPEN_WOUNDS:
	if (Antimagic) {
	    shieldeff(u.ux, u.uy);
	    dmg = (dmg + 1) / 2;
	}
	if (dmg <= 5)
	    Your("skin itches badly for a moment.");
	else if (dmg <= 10)
	    pline("Wounds appear on your body!");
	else if (dmg <= 20)
	    pline("Severe wounds appear on your body!");
	else
	    Your("body is covered with painful wounds!");
	break;
    default:
	impossible("mcastu: invalid clerical spell (%d)", spellnum);
	dmg = 0;
	break;
    }

    if (dmg) mdamageu(mtmp, dmg);
}

STATIC_OVL
void
m_cast_cleric_spell(mtmp, mdef, dmg, spellnum)
struct monst *mtmp, *mdef;
int dmg;
int spellnum;
{
    int sees_def = canseemon(mdef);

    if (dmg == 0 && !is_undirected_spell(AD_CLRC, spellnum)) {
	impossible("cast directed cleric spell (%d) with dmg=0?", spellnum);
	return;
    }

    switch (spellnum) {
    case CLC_GEYSER:
	/* this is physical damage, not magical damage */
	if (sees_def)
	    pline("A sudden geyser slams into %s from nowhere!", 
				mon_nam(mdef));
	dmg = d(8, 6);
	break;
    case CLC_FIRE_PILLAR:
	if (sees_def)
	    pline("A pillar of fire strikes all around %s!", mon_nam(mdef));
	if (resists_fire(mdef)) {
	    shieldeff(mdef->mx, mdef->my);
	    dmg = 0;
	} else
	    dmg = d(8, 6);
	(void) burnarmor(mdef);
	destroy_mitem(mdef, SCROLL_CLASS, AD_FIRE);
	destroy_mitem(mdef, POTION_CLASS, AD_FIRE);
	destroy_mitem(mdef, SPBOOK_CLASS, AD_FIRE);
	(void) burn_floor_paper(mdef->mx, mdef->my, TRUE, FALSE);
	break;
    case CLC_LIGHTNING:
    {
	boolean reflects;

	if (sees_def)
	    pline("A bolt of lightning strikes down at %s from above!",
			mon_nam(mdef));
	reflects = mon_reflects(mdef, "It bounces off %s %s.");
	if (reflects || resists_elec(mdef)) {
	    shieldeff(mdef->mx, mdef->my);
	    dmg = 0;
	    if (reflects)
		break;
	} else
	    dmg = d(8, 6);
	destroy_mitem(mdef, WAND_CLASS, AD_ELEC);
	destroy_mitem(mdef, RING_CLASS, AD_ELEC);
	break;
    }
    case CLC_BLIND_YOU:
	/* note: resists_blnd() doesn't apply here */
	if (mdef->mcansee && haseyes(mdef->data)) {
	    register unsigned rnd_tmp = rnd(50) + 5;
	    int num_eyes = eyecount(mdef->data);
	    if (sees_def)
		pline("Scales cover %s %s!",
		  s_suffix(mon_nam(mdef)),
		  (num_eyes == 1) ?
		  mbodypart(mdef, EYE) : makeplural(mbodypart(mdef, EYE)));
	    mdef->mcansee = 0;
	    if((mdef->mblinded + rnd_tmp) > 127)
		mdef->mblinded = 127;
	    else mdef->mblinded += rnd_tmp;
	}
	dmg = 0;	
	break;
    case CLC_PARALYZE:
	if (!resists_magm(mdef)) {
	    if (sees_def && mdef->mcanmove)
		pline("%s is frozen in place!", Monnam(mdef));

	    mdef->mcanmove = 0;
	    if (mdef->mfrozen < 20)
		mdef->mfrozen  += dmg;
	}
	dmg = 0;
	break;
    case CLC_CONFUSE_YOU:
	if (!resists_magm(mdef) && !mdef->mconf) {
	    if (sees_def)
		pline("%s looks confused.", Monnam(mdef));
	    dmg = (int)mtmp->m_lev;
	    mdef->mconf = 1;
	}
	dmg = 0;
	break;
    case CLC_OPEN_WOUNDS:
	if (resists_magm(mdef))
	    dmg = (dmg + 1) / 2;

	if (unsolid(mdef->data) || is_whirly(mdef->data))
	    dmg = 0;
	if (dmg > 0 && sees_def)
	    pline("%s is %s!", (is_golem(mdef->data)? "damaged" : "wounded"));
	break;
    default:
	dmg = 0;
	break;
    }

    if (dmg) damagem(mtmp, mdef, dmg);
}

STATIC_DCL
boolean
is_undirected_spell(adtyp, spellnum)
unsigned int adtyp;
int spellnum;
{
    if (adtyp == AD_SPEL) {
	switch (spellnum) {
	case MGC_CLONE_WIZ:
	case MGC_SUMMON_MONS:
	case MGC_SUMMON_GHOST:
	case MGC_AGGRAVATION:
	case MGC_DISAPPEAR:
	case MGC_HASTE_SELF:
	case MGC_CURE_SELF:
	case MGC_CALL_UNDEAD:
	    return TRUE;
	default:
	    break;
	}
    } else if (adtyp == AD_CLRC) {
	switch (spellnum) {
	case CLC_INSECTS:
	case CLC_CURE_SELF:
	case CLC_AGGRAVATION:
	    return TRUE;
	default:
	    break;
	}
    }
    return FALSE;
}

STATIC_DCL
boolean
is_melee_spell(adtyp, spellnum)
unsigned int adtyp;
int spellnum;
{
    if (adtyp == AD_SPEL) {
	switch (spellnum) {
	case MGC_DEATH_TOUCH:
	    return TRUE;
	default:
	    break;
	}
    } else if (adtyp == AD_CLRC) {
	switch (spellnum) {
	default:
	    break;
	}
    }
    return FALSE;
}


/* Some spells are useless under some circumstances. */
STATIC_DCL
boolean
spell_would_be_useless(mtmp, adtyp, spellnum)
struct monst *mtmp;
unsigned int adtyp;
int spellnum;
{
    /* Some spells don't require the player to really be there and can be cast
     * by the monster when you're invisible, yet still shouldn't be cast when
     * the monster doesn't even think you're there.
     * This check isn't quite right because it always uses your real position.
     * We really want something like "if the monster could see mux, muy".
     */
    boolean mcouldseeu = couldsee(mtmp->mx, mtmp->my);

    if (adtyp == AD_SPEL) {
	/* aggravate monsters, etc. won't be cast by peaceful monsters */
	if (mtmp->mpeaceful && (spellnum == MGC_AGGRAVATION || !is_undirected_spell(AD_SPEL,spellnum) ||
               spellnum == MGC_SUMMON_MONS || spellnum == MGC_SUMMON_GHOST || spellnum == MGC_CLONE_WIZ || spellnum == MGC_CREATE_POOL ||
               spellnum == MGC_CALL_UNDEAD))
	    return TRUE;
	/* Don't go making everything else bonkers if you're peaceful! */
	if (spellnum == MGC_ENRAGE && (mtmp->mpeaceful || mtmp->mtame)) {
		return TRUE;
	}
	/* haste self when already fast */
	if (mtmp->mspeed == MFAST && spellnum == MGC_HASTE_SELF)
	    return TRUE;
	/* invisibility when already invisible */
	if ((mtmp->minvis || mtmp->invis_blkd) && spellnum == MGC_DISAPPEAR)
	    return TRUE;
	/* peaceful monster won't cast invisibility if you can't see invisible,
	   same as when monsters drink potions of invisibility.  This doesn't
	   really make a lot of sense, but lets the player avoid hitting
	   peaceful monsters by mistake */
	if (mtmp->mpeaceful && !See_invisible && spellnum == MGC_DISAPPEAR)
	    return TRUE;
	/* healing when already healed */
	if (mtmp->mhp == mtmp->mhpmax && spellnum == MGC_CURE_SELF)
	    return TRUE;
#if 0
	/* summon monsters less often if the monster is low level --Amy */
	if ( (spellnum == MGC_SUMMON_MONS || spellnum == MGC_SUMMON_GHOST || spellnum == MGC_CALL_UNDEAD) && mtmp->m_lev < rnd(50) && rn2(5)) return TRUE;
#endif
	/* don't summon monsters if it doesn't think you're around */
	if (!mcouldseeu && (spellnum == MGC_SUMMON_MONS ||
		spellnum == MGC_CALL_UNDEAD || spellnum == MGC_SUMMON_GHOST||
		(!mtmp->iswiz && spellnum == MGC_CLONE_WIZ)))
	    return TRUE;
	/* only lichs can cast call undead */ /* well, not anymore --Amy */
	/*if (mtmp->data->mlet != S_LICH && spellnum == MGC_CALL_UNDEAD)
	    return TRUE;*/
	/* pools can only be created in certain locations and then only
	 * rarely unless you're carrying the amulet.
	 */
	if ((levl[u.ux][u.uy].typ != ROOM && levl[u.ux][u.uy].typ != CORR
		|| !u.uhave.amulet && rn2(10)) && spellnum == MGC_CREATE_POOL)
	    return TRUE;
	if ((!mtmp->iswiz || flags.no_of_wizards > 1)
						&& spellnum == MGC_CLONE_WIZ)
	    return TRUE;
    } else if (adtyp == AD_CLRC) {
	/* summon insects/sticks to snakes won't be cast by peaceful monsters */
	if (mtmp->mpeaceful && (spellnum == CLC_INSECTS || !is_undirected_spell(AD_CLRC,spellnum) || spellnum == CLC_AGGRAVATION ) )
	    return TRUE;
	/* healing when already healed */
	if (mtmp->mhp == mtmp->mhpmax && spellnum == CLC_CURE_SELF)
	    return TRUE;

	/* summon monsters less often if the monster is low level --Amy */
	if ( spellnum == CLC_INSECTS && mtmp->m_lev < rnd(50) && rn2(5)) return TRUE;

	/* don't summon insects if it doesn't think you're around */
	if (!mcouldseeu && spellnum == CLC_INSECTS)
	    return TRUE;
	/* blindness spell on blinded player */
	if (Blinded && spellnum == CLC_BLIND_YOU)
	    return TRUE;
    }
    return FALSE;
}


/* return values:
 * 1: successful spell
 * 0: unsuccessful spell
 */
int
castmm(mtmp, mattk, mdef)
	register struct monst *mtmp, *mdef;
	register struct attack *mattk;
{
	int dmg, ml = mtmp->m_lev;
	int ret;
	int spellnum = 0;
	int sees_def = canseemon(mdef), sees_agr = canseemon(mtmp),
	    dist = dist2(mdef->mx, mdef->my, mtmp->mx, mtmp->my);

	if (ret = gcastm(mtmp, mattk, mdef)) return ret;

	/* Too far away for a conventional spell? */
	if (dist >= 4) return 0;

	/* The chief difference between this and castmu() is that we expect
	 * to cast only close-range spells and that we _always_ know our
	 * target exactly. This allows us to simplify the function 
	 * considerably. Since monster healing spells are handled by castmu(),
	 * we can concentrate solely on attack spells here
	 */
	if (mattk->adtyp == AD_SPEL || mattk->adtyp == AD_CLRC) {
		spellnum = rn2(ml);
		if (mattk->adtyp == AD_SPEL)
		    spellnum = m_choose_magic_spell(spellnum);
		else
		    spellnum = m_choose_clerical_spell(spellnum);
	}

	/* monster unable to cast spells? */
	if(mtmp->mcan || mtmp->mspec_used || !ml || spellnum == -1) {
	    return(0);
	}

	if (mattk->adtyp == AD_SPEL || mattk->adtyp == AD_CLRC) {
	    mtmp->mspec_used = 10 - mtmp->m_lev;
	    if (mtmp->mspec_used < 2) mtmp->mspec_used = 2;
	}

	nomul(0,0);
	if(rn2(ml*10) < (mtmp->mconf ? 100 : 20)) {	/* fumbled attack */
	    if (canseemon(mtmp) && flags.soundok)
		pline_The("air crackles around %s.", mon_nam(mtmp));
	    return(0);
	}

/*
 *	As these are spells, the damage is related to the level
 *	of the monster casting the spell.
 */
	if (mattk->damd)
	    dmg = d((int)((ml/2) + mattk->damn), (int)mattk->damd);
	else dmg = d((int)((ml/2) + 1), 6);

	ret = 1;

	switch (mattk->adtyp) {

	    case AD_FIRE:
		if (sees_def)
			pline("%s is enveloped in flames.", Monnam(mdef));
		if(resists_fire(mdef)) {
			shieldeff(mdef->mx, mdef->my);
			if (sees_def)
			pline("But %s resists the effects.",
					mhe(mdef));
			dmg = 0;
		}
		break;
	    case AD_COLD:
		if (sees_def)
			pline("%s is covered in frost.", Monnam(mdef));
		if(resists_cold(mdef)) {
			shieldeff(mdef->mx, mdef->my);
			pline("But %s resists the effects.",
					mhe(mdef));
			dmg = 0;
		}
		break;
	    case AD_MAGM:
		if (sees_def)
		    pline("%s is hit by a shower of missiles!", Monnam(mdef));
		if(resists_magm(mdef)) {
			shieldeff(mdef->mx, mdef->my);
			pline_The("missiles bounce off!");
			dmg = 0;
		} else dmg = d((int)mtmp->m_lev/2 + 1,6);
		break;
	    case AD_SPEL:	/* wizard spell */
	    case AD_CLRC:       /* clerical spell */
	    {
		if (mattk->adtyp == AD_SPEL)
		    m_cast_wizard_spell(mtmp, mdef, dmg, spellnum);
		else
		    m_cast_cleric_spell(mtmp, mdef, dmg, spellnum);
		dmg = 0; /* done by the spell casting functions */
		break;
	    }
	}
	if(dmg) damagem(mtmp, mdef, dmg);

	return(ret);
}

#endif /* OVLB */
#ifdef OVL0

/* convert 1..10 to 0..9; add 10 for second group (spell casting) */
#define ad_to_typ(k) (10 + (int)k - 1)

int
buzzmu(mtmp, mattk)		/* monster uses spell (ranged) */
	register struct monst *mtmp;
	register struct attack  *mattk;
{
	/* don't print constant stream of curse messages for 'normal'
	   spellcasting monsters at range */
	if (mattk->adtyp > AD_SPC2)
	    return(0);

	if (mtmp->mcan) {
	    cursetxt(mtmp, FALSE);
	    return(0);
	}
	if(lined_up(mtmp) && rn2(3)) {
	    nomul(0, 0);
	    if(mattk->adtyp && (mattk->adtyp < 11)) { /* no cf unsigned >0 */
		if(canseemon(mtmp))
		    pline("%s zaps you with a %s!", Monnam(mtmp),
			  flash_types[ad_to_typ(mattk->adtyp)]);
		buzz(-ad_to_typ(mattk->adtyp), (int)mattk->damn,
		     mtmp->mx, mtmp->my, sgn(tbx), sgn(tby));
	    } else impossible("Monster spell %d cast", mattk->adtyp-1);
	}
	return(1);
}

int 
buzzmm(mtmp, mattk, mtarg)	/* monster zaps spell at another monster */
register struct monst *mtmp, *mtarg;
register struct attack  *mattk;
{
    if (mtarg == &youmonst)
	return buzzmu(mtmp, mattk);

    if(mtmp->mcan || mattk->adtyp > AD_SPC2) {
	return(0);
    }
    if(m_lined_up(mtarg, mtmp) && rn2(5)) {
	/* Before zapping fireballs, verify that our friends aren't
	 * adjacent */
	if (mattk->adtyp == AD_FIRE && mtmp->mtame) {
	    if (has_adjacent_enemy(mtmp, mtarg->mx, mtarg->my, FALSE))
		return (0);
	}
	if(mattk->adtyp && (mattk->adtyp < 11)) { /* no cf unsigned >0 */
	    if(canseemon(mtmp)) {
		pline("%s zaps a %s!", Monnam(mtmp),
				flash_types[ad_to_typ(mattk->adtyp)]);
	    }
	    dobuzz(-ad_to_typ(mattk->adtyp), (int)mattk->damn,
			    mtmp->mx, mtmp->my, sgn(tbx), sgn(tby), FALSE);
	} else impossible("Monster spell %d cast", mattk->adtyp-1);
    }
    return(1);
}

#endif /* OVL0 */

/*mcastu.c*/

STATIC_PTR void
set_litZ(x,y,val)
int x, y;
genericptr_t val;
{
	if (val)
	    levl[x][y].lit = 1;
	else {
	    levl[x][y].lit = 0;
	    snuff_light_source(x, y);
	}
}

