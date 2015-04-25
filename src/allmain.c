/*	SCCS Id: @(#)allmain.c	3.4	2003/04/02	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/* various code that was replicated in *main.c */

#include "hack.h"
#ifndef NO_SIGNAL
#include <signal.h>
#endif
#ifdef SHORT_FILENAMES
#include "patchlev.h"
#else
#include "patchlevel.h"
#endif

#ifdef POSITIONBAR
STATIC_DCL void NDECL(do_positionbar);
#endif

#define decrnknow(spell)	spl_book[spell].sp_know--
#define spellid(spell)		spl_book[spell].sp_id
#define spellknow(spell)	spl_book[spell].sp_know

#ifdef OVL0

void
moveloop()
{
#if defined(MICRO) || defined(WIN32)
    char ch;
    int abort_lev;
#endif
	struct obj *pobj; /* 5lo: *pobj exists on other platforms, not just Windows */
    int moveamt = 0, wtcap = 0, change = 0;
	int randsp;
	int randmnst;
	int randmnsx;
	int i;
    boolean didmove = FALSE, monscanmove = FALSE;
    /* don't make it obvious when monsters will start speeding up */
    int monclock;
    int xtraclock;
    /*int timeout_start = rnz(10000)+rnz(15000);*/
    /*int clock_base = rnz(10000)+rnz(20000)+timeout_start;*/
	int timeout_start = u.monstertimeout;
	int clock_base = u.monstertimefinish; /* values set in u_init */
    int past_clock;
	/*u.monstertimeout = timeout_start;*/
	/*u.monstertimefinish = clock_base;*/

    flags.moonphase = phase_of_the_moon();
    if(flags.moonphase == FULL_MOON) {
	You("are lucky!  Full moon tonight.");
	change_luck(1);
    } else if(flags.moonphase == NEW_MOON) {
	pline("Be careful!  New moon tonight.");
	adjalign(-3); 
    }
    flags.friday13 = friday_13th();
    if (flags.friday13) {
	pline("Watch out!  Bad things can happen on Friday the 13th.");
	change_luck(-1);
	adjalign(-10); 
    }
    /* KMH -- February 2 */
    flags.groundhogday = groundhog_day();
    if (flags.groundhogday)
	pline("Happy Groundhog Day!");

    initrack();


    /* Note:  these initializers don't do anything except guarantee that
	    we're linked properly.
    */
    decl_init();
    monst_init();
    monstr_init();	/* monster strengths */
    objects_init();

#ifdef WIZARD
    if (wizard) add_debug_extended_commands();
#endif

    (void) encumber_msg(); /* in case they auto-picked up something */
    if (defer_see_monsters) {
	defer_see_monsters = FALSE;
	see_monsters();
    }

    u.uz0.dlevel = u.uz.dlevel;
    youmonst.movement = NORMAL_SPEED;	/* give the hero some movement points */

    for(;;) {
	get_nh_event();
#ifdef POSITIONBAR
	do_positionbar();
#endif

	didmove = flags.move;
	if(didmove) {
	    /* actual time passed */
	    youmonst.movement -= NORMAL_SPEED;

	    do { /* hero can't move this turn loop */
		wtcap = encumber_msg();

		flags.mon_moving = TRUE;
		do {
		    monscanmove = movemon();
		    if (youmonst.movement > NORMAL_SPEED)
			break;	/* it's now your turn */
		} while (monscanmove);
		flags.mon_moving = FALSE;

		if (!monscanmove && youmonst.movement < NORMAL_SPEED) {
		    /* both you and the monsters are out of steam this round */
		    /* set up for a new turn */
		    struct monst *mtmp;
		    mcalcdistress();	/* adjust monsters' trap, blind, etc */

		    /* reallocate movement rations to monsters */
		    for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
			mtmp->movement += mcalcmove(mtmp);

			 /* Vanilla generates a critter every 70-ish turns.
			  * The rate accelerates to every 50 or so below the Castle,
			  * and 'round every 25 turns once you've done the Invocation.
			  *
			  * We will push it even further.  Monsters post-Invocation
			  * will almost always appear on the stairs (if present), and 
			  * much more frequently; this, along with the extra intervene()
			  * calls, should certainly make it seem like you're wading back
			  * through the teeming hordes.
			  *
			  * Aside from that, a more general clock should be put on things;
			  * after about 30,000 turns, the frequency rate of appearance
			  * and (TODO) difficulty of monsters generated will slowly increase until
			  * it reaches the point it will be at as if you were post-Invocation.
			  *
			  * 80,000 turns should be adequate as a target mark for this effect;
			  * if you haven't ascended in 80,000 turns, you're intentionally
			  * fiddling around somewhere and will certainly be strong enough
			  * to handle anything that comes your way, so this won't be 
			  * dropping newbies off the edge of the planet.  -- DSR 12/2/07
			  */
#ifdef MORE_SPAWNS
			monclock = 70;
			if (u.uevent.udemigod) {
				monclock = 15;
			} else {
				if (depth(&u.uz) > depth(&stronghold_level)) {
					monclock = 50;
				}
				past_clock = moves - timeout_start;
				if (past_clock > 0) {
					monclock -= past_clock*55/clock_base;
				}
			}
			/* make sure we don't fall off the bottom */
			if (monclock < 15) { monclock = 15; }

			/* TODO: adj difficulty in makemon */
			if (!rn2(monclock)) {
				if (u.uevent.udemigod && xupstair && rn2(10)) {
					(void) makemon((struct permonst *)0, xupstair, yupstair, MM_ADJACENTOK);
				} else {
					(void) makemon((struct permonst *)0, 0, 0, NO_MM_FLAGS);
				}
			}

			xtraclock = 10000;
			if (u.uevent.udemigod) {
				xtraclock = 3000;
			} else {
				if (depth(&u.uz) > depth(&stronghold_level)) {
					xtraclock = 7000;
				}
				past_clock = moves - timeout_start;
				if (past_clock > 0) {
					xtraclock -= past_clock*7000/clock_base;
				}
			}
			/* make sure we don't fall off the bottom */
			if (xtraclock < 3000) { xtraclock = 3000; }

			/* new group spawn system by Amy */
			if (!rn2(xtraclock)) {

				randsp = (rn2(14) + 2);
				randmnst = (rn2(181) + 1);
				randmnsx = (rn2(100) + 1);

				if (wizard || !rn2(10)) pline("You suddenly feel a surge of tension!");

			for (i = 0; i < randsp; i++) {
			/* This function will fill the map with a random amount of monsters of one class. --Amy */

			if (randmnst < 6)
		 	    (void) makemon(mkclass(S_ANT,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 9)
		 	    (void) makemon(mkclass(S_BLOB,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 11)
		 	    (void) makemon(mkclass(S_COCKATRICE,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 15)
		 	    (void) makemon(mkclass(S_DOG,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 18)
		 	    (void) makemon(mkclass(S_EYE,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 22)
		 	    (void) makemon(mkclass(S_FELINE,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 24)
		 	    (void) makemon(mkclass(S_GREMLIN,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 29)
		 	    (void) makemon(mkclass(S_HUMANOID,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 33)
		 	    (void) makemon(mkclass(S_IMP,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 36)
		 	    (void) makemon(mkclass(S_JELLY,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 41)
		 	    (void) makemon(mkclass(S_KOBOLD,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 44)
		 	    (void) makemon(mkclass(S_LEPRECHAUN,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 47)
		 	    (void) makemon(mkclass(S_MIMIC,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 50)
		 	    (void) makemon(mkclass(S_NYMPH,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 54)
		 	    (void) makemon(mkclass(S_ORC,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 55)
		 	    (void) makemon(mkclass(S_PIERCER,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 58)
		 	    (void) makemon(mkclass(S_QUADRUPED,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 62)
		 	    (void) makemon(mkclass(S_RODENT,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 65)
		 	    (void) makemon(mkclass(S_SPIDER,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 66)
		 	    (void) makemon(mkclass(S_TRAPPER,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 69)
		 	    (void) makemon(mkclass(S_UNICORN,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 71)
		 	    (void) makemon(mkclass(S_VORTEX,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 73)
		 	    (void) makemon(mkclass(S_WORM,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 75)
		 	    (void) makemon(mkclass(S_XAN,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 76)
		 	    (void) makemon(mkclass(S_LIGHT,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 77)
		 	    (void) makemon(mkclass(S_ZOUTHERN,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 78)
		 	    (void) makemon(mkclass(S_ANGEL,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 81)
		 	    (void) makemon(mkclass(S_BAT,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 83)
		 	    (void) makemon(mkclass(S_CENTAUR,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 86)
		 	    (void) makemon(mkclass(S_DRAGON,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 89)
		 	    (void) makemon(mkclass(S_ELEMENTAL,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 94)
		 	    (void) makemon(mkclass(S_FUNGUS,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 99)
		 	    (void) makemon(mkclass(S_GNOME,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 102)
		 	    (void) makemon(mkclass(S_GIANT,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 103)
		 	    (void) makemon(mkclass(S_JABBERWOCK,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 104)
		 	    (void) makemon(mkclass(S_KOP,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 105)
		 	    (void) makemon(mkclass(S_LICH,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 108)
		 	    (void) makemon(mkclass(S_MUMMY,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 110)
		 	    (void) makemon(mkclass(S_NAGA,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 113)
		 	    (void) makemon(mkclass(S_OGRE,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 115)
		 	    (void) makemon(mkclass(S_PUDDING,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 116)
		 	    (void) makemon(mkclass(S_QUANTMECH,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 118)
		 	    (void) makemon(mkclass(S_RUSTMONST,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 121)
		 	    (void) makemon(mkclass(S_SNAKE,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 123)
		 	    (void) makemon(mkclass(S_TROLL,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 124)
		 	    (void) makemon(mkclass(S_UMBER,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 125)
		 	    (void) makemon(mkclass(S_VAMPIRE,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 127)
		 	    (void) makemon(mkclass(S_WRAITH,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 128)
		 	    (void) makemon(mkclass(S_XORN,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 130)
		 	    (void) makemon(mkclass(S_YETI,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 135)
		 	    (void) makemon(mkclass(S_ZOMBIE,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 145)
		 	    (void) makemon(mkclass(S_HUMAN,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 147)
		 	    (void) makemon(mkclass(S_GHOST,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 149)
		 	    (void) makemon(mkclass(S_GOLEM,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 152)
		 	    (void) makemon(mkclass(S_DEMON,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 155)
		 	    (void) makemon(mkclass(S_EEL,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 160)
		 	    (void) makemon(mkclass(S_LIZARD,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 162)
		 	    (void) makemon(mkclass(S_BAD_FOOD,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 165)
		 	    (void) makemon(mkclass(S_BAD_COINS,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 166) {
				if (randmnsx < 96)
		 	    (void) makemon(mkclass(S_HUMAN,0), 0, 0, NO_MM_FLAGS);
				else
		 	    (void) makemon(mkclass(S_NEMESE,0), 0, 0, NO_MM_FLAGS);
				}
			else if (randmnst < 171)
		 	    (void) makemon(mkclass(S_GRUE,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 176)
		 	    (void) makemon(mkclass(S_WALLMONST,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 180)
		 	    (void) makemon(mkclass(S_RUBMONST,0), 0, 0, NO_MM_FLAGS);
			else if (randmnst < 181) {
				if (randmnsx < 99)
		 	    (void) makemon(mkclass(S_HUMAN,0), 0, 0, NO_MM_FLAGS);
				else
		 	    (void) makemon(mkclass(S_ARCHFIEND,0), 0, 0, NO_MM_FLAGS);
				}
			else
		 	    (void) makemon((struct permonst *)0, 0, 0, NO_MM_FLAGS);

				}
			}
#endif /* MORE_SPAWNS */
			if (uarmf && uarmf->otyp == ZIPPER_BOOTS && !EWounded_legs) EWounded_legs = 1;
#ifdef MORE_SPAWNS
		    if(!rn2(u.uevent.udemigod ? 125 :
			    (depth(&u.uz) > depth(&stronghold_level)) ? 250 : 340))
	/* still keeping the old monstermaking routine up, but drastically reducing their spawn rate. --Amy */
#else
		    if(!rn2(u.uevent.udemigod ? 25 :
			    (depth(&u.uz) > depth(&stronghold_level)) ? 50 : 70))
#endif /* MORE_SPAWNS */
			(void) makemon((struct permonst *)0, 0, 0, NO_MM_FLAGS);

		    /* calculate how much time passed. */
#ifdef STEED
		    if (u.usteed && u.umoved) {
			/* your speed doesn't augment steed's speed */
			moveamt = mcalcmove(u.usteed);
		    } else
#endif
		    {
			moveamt = youmonst.data->mmove;
#ifdef ELDER_SCROLLS
			if (Race_if(PM_ASGARDIAN) && !rn2(20) ) /* Asgardians are slower sometimes, this is intentional. --Amy */
				moveamt -= NORMAL_SPEED / 2;
#endif
			if (moveamt < 0) moveamt == 0;

			if (Very_fast) {	/* speed boots or potion */
			    /* average movement is 1.67 times normal */
			    moveamt += NORMAL_SPEED / 2;
			    if (rn2(3) == 0) moveamt += NORMAL_SPEED / 2;
			} else if (Fast) {
			    /* average movement is 1.33 times normal */
			    if (rn2(3) != 0) moveamt += NORMAL_SPEED / 2;
			}
			if (tech_inuse(T_BLINK)) { /* TECH: Blinking! */
			    /* Case    Average  Variance
			     * -------------------------
			     * Normal    12         0
			     * Fast      16        12
			     * V fast    20        12
			     * Blinking  24        12
			     * F & B     28        18
			     * V F & B   30        18
			     */
			    moveamt += NORMAL_SPEED * 2 / 3;
			    if (rn2(3) == 0) moveamt += NORMAL_SPEED / 2;
			}
		    }

		    switch (wtcap) {
			case UNENCUMBERED: break;
			case SLT_ENCUMBER: moveamt -= (moveamt / 4); break;
			case MOD_ENCUMBER: moveamt -= (moveamt / 2); break;
			case HVY_ENCUMBER: moveamt -= ((moveamt * 3) / 4); break;
			case EXT_ENCUMBER: moveamt -= ((moveamt * 7) / 8); break;
			default: break;
		    }

		    youmonst.movement += moveamt;
		    if (youmonst.movement < 0) youmonst.movement = 0;
		    settrack();

		    monstermoves++;
		    moves++;

		    /********************************/
		    /* once-per-turn things go here */
		    /********************************/

		    if (flags.bypasses) clear_bypasses();
		    if(IsGlib) glibr();
		    nh_timeout();
		    run_regions();

#ifdef DUNGEON_GROWTH
		    dgn_growths(TRUE, TRUE);
#endif

		    if (u.ublesscnt)  u.ublesscnt--;
		    
		    if(flags.time && !flags.run)
			flags.botl = 1;

			if(u.ukinghill){
				if(u.protean > 0) u.protean--;
				else{
					for(pobj = invent; pobj; pobj=pobj->nobj)
						if(pobj->oartifact == ART_TREASURY_OF_PROTEUS)
							break;
					if(!pobj) pline("Treasury not actually in inventory??");
					else if(pobj->cobj){
						arti_poly_contents(pobj);
					}
					u.protean = rnz(100)+d(3,10);
					update_inventory();
				}
			}

		    /* One possible result of prayer is healing.  Whether or
		     * not you get healed depends on your current hit points.
		     * If you are allowed to regenerate during the prayer, the
		     * end-of-prayer calculation messes up on this.
		     * Another possible result is rehumanization, which requires
		     * that encumbrance and movement rate be recalculated.
		     */
		    if (u.uinvulnerable) {
			/* for the moment at least, you're in tiptop shape */
			wtcap = UNENCUMBERED;
		    } else if (Upolyd && youmonst.data->mlet == S_EEL && !is_pool(u.ux,u.uy) && !Is_waterlevel(&u.uz)) {
			if (u.mh > 1) {
			    u.mh--;
			    flags.botl = 1;
			} else if (u.mh < 1)
			    rehumanize();
		    } else if (Upolyd && u.mh < u.mhmax) {
			if (u.mh < 1)
			    rehumanize();
			else if (Regeneration ||
				    (wtcap < MOD_ENCUMBER && !(moves%20))) {
			    flags.botl = 1;
			    u.mh++;
			}
		    } else if (u.uhp < u.uhpmax &&
			 (wtcap < MOD_ENCUMBER || !u.umoved || Regeneration)) {
/*
 * KMH, balance patch -- New regeneration code
 * Healthstones have been added, which alter your effective
 * experience level and constitution (-2 cursed, +1 uncursed,
 * +2 blessed) for the basis of regeneration calculations.
 */


 			int efflev = u.ulevel + (u.uhealbonus);
 			int effcon = ACURR(A_CON) + (u.uhealbonus);
			if (P_SKILL(P_RIDING) == P_SKILLED) efflev += 2;
			if (P_SKILL(P_RIDING) == P_EXPERT) efflev += 5;
			if (P_SKILL(P_RIDING) == P_SKILLED) effcon += 2;
			if (P_SKILL(P_RIDING) == P_EXPERT) effcon += 5;
	/* Yeah I know this makes no sense at all, but it improves the usefulness of the riding skill. --Amy */
			int heal = 1;


			if ( efflev > 9 && !(moves % 3)) {
			    if (effcon <= 12) {
				heal = 1;
			    } else {
				heal = rnd(effcon / 5) + 1;
  				if (heal > efflev-9) heal = efflev-9;
			    }
			    flags.botl = 1;
			    u.uhp += heal;
			    if(u.uhp > u.uhpmax)
				u.uhp = u.uhpmax;
			} else if (Regeneration ||
			     (efflev <= 9 &&
			      !(moves % ((MAXULEV+12) / (u.ulevel+2) + 1)))) {
			    flags.botl = 1;
			    u.uhp++;
			}
		    }

		    if (!u.uinvulnerable && u.uen > 0 && u.uhp < u.uhpmax &&
			    tech_inuse(T_CHI_HEALING)) {
			u.uen--;
			u.uhp++;
			flags.botl = 1;
		    }

		    /* moving around while encumbered is hard work */
		    if (wtcap > MOD_ENCUMBER && u.umoved) {
			if(!(wtcap < EXT_ENCUMBER ? moves%30 : moves%10)) {
			    if (Upolyd && u.mh > 1) {
				u.mh--;
			    } else if (!Upolyd && u.uhp > 1) {
				u.uhp--;
			    } else {
				You("pass out from exertion!");
				exercise(A_CON, FALSE);
				fall_asleep(-10, FALSE);
			    }
			}
		    }

		    
		    /* KMH -- OK to regenerate if you don't move */
		    if ((u.uen < u.uenmax) && 
				((Energy_regeneration && !rn2(5)) || /* greatly nerfed overpowered wizard artifact --Amy */
				((wtcap < MOD_ENCUMBER || !flags.mv) &&
				(!(moves%((MAXULEV + 15 - u.ulevel) *                                    
				(Role_if(PM_WIZARD) ? 3 : 4) / 6)))))) {
			u.uen += rn1((int)(ACURR(A_WIS) + ACURR(A_INT)) / 15 + 1,1);
#ifdef WIZ_PATCH_DEBUG
                pline("mana was = %d now = %d",temp,u.uen);
#endif

			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;
		    }

		/* leveling up will give a small boost to mana regeneration now --Amy */
		    if ( u.uen < u.uenmax && ( 
			(u.ulevel >= 5 && !rn2(200)) ||
			(u.ulevel >= 10 && !rn2(100)) ||
			(u.ulevel >= 15 && !rn2(50)) ||
			(u.ulevel >= 20 && !rn2(30)) ||
			(u.ulevel >= 25 && !rn2(20)) ||
			(u.ulevel >= 30 && !rn2(10))
			)
			)
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			/* Having a spell school at skilled will improve mana regeneration.
			 * Having a spell school at expert will improve it by even more. --Amy */

			if (P_SKILL(P_ATTACK_SPELL) == P_SKILLED && !rn2(200))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_ATTACK_SPELL) == P_EXPERT && !rn2(100))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_DIVINATION_SPELL) == P_SKILLED && !rn2(200))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_DIVINATION_SPELL) == P_EXPERT && !rn2(100))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_MATTER_SPELL) == P_SKILLED && !rn2(200))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_MATTER_SPELL) == P_EXPERT && !rn2(100))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_BODY_SPELL) == P_SKILLED && !rn2(200))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_BODY_SPELL) == P_EXPERT && !rn2(100))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_PROTECTION_SPELL) == P_SKILLED && !rn2(200))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_PROTECTION_SPELL) == P_EXPERT && !rn2(100))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_ENCHANTMENT_SPELL) == P_SKILLED && !rn2(200))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_ENCHANTMENT_SPELL) == P_EXPERT && !rn2(100))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_HEALING_SPELL) == P_SKILLED && !rn2(200))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;

			if (P_SKILL(P_HEALING_SPELL) == P_EXPERT && !rn2(100))
			u.uen += 1;
			if (u.uen > u.uenmax)  u.uen = u.uenmax;
			flags.botl = 1;


		    if(!u.uinvulnerable) {
			if(Teleportation && !rn2(250)) {
			    xchar old_ux = u.ux, old_uy = u.uy;
				You("suddenly get teleported!");
			    tele();
			    if (u.ux != old_ux || u.uy != old_uy) {
				if (!next_to_u()) {
				    check_leash(old_ux, old_uy);
				}
#ifdef REDO
				/* clear doagain keystrokes */
				pushch(0);
				savech(0);
#endif
			    }
			}
			/* delayed change may not be valid anymore */
			if ((change == 1 && !Polymorph) ||
			    (change == 2 && u.ulycn == NON_PM))
			    change = 0;
			if(Polymorph && !rn2(1000))
			    change = 1;
	/* let's allow the moulds to stop sucking so much. Make them polymorph more often. --Amy */
			if(Polymorph && !rn2(200) && !Upolyd && Race_if(PM_MOULD) )
			    change = 1;
			else if (u.ulycn >= LOW_PM && !Upolyd &&
				 !rn2(1200 - (200 * night())))
			    change = 2;
			if (change && !Unchanging) {
			    if (multi >= 0) {
				if (occupation)
				    stop_occupation();
				else
				    nomul(0, 0);
				if (change == 1) polyself(FALSE);
				else you_were();
				change = 0;
			    }
			}
		}	/* !u.uinvulnerable */

		    if(Searching && multi >= 0) (void) dosearch0(1);
		    dosounds();
		    do_storms();
		    gethungry();
		    age_spells();
		    exerchk();
		    invault();
		    if (u.uhave.amulet) amulet();
		if (!rn2(40+(int)(ACURR(A_DEX)*3))) u_wipe_engr(rnd(3));
		    if (u.uevent.udemigod && !u.uinvulnerable) {
			if (u.udg_cnt) u.udg_cnt--;
			if (!u.udg_cnt) {
			    intervene();
			    u.udg_cnt = rn1(200, 50);
			}
		    }
		    restore_attrib();

		    /* underwater and waterlevel vision are done here */
		    if (Is_waterlevel(&u.uz))
			movebubbles();
		    else if (Underwater)
			under_water(0);
		    /* vision while buried done here */
		    else if (u.uburied) under_ground(0);

		    /* when immobile, count is in turns */
		    if(multi < 0) {
			if (++multi == 0) {	/* finished yet? */
			    unmul((char *)0);
			    /* if unmul caused a level change, take it now */
			    if (u.utotype) deferred_goto();
			}
		    }
		}
	    } while (youmonst.movement<NORMAL_SPEED); /* hero can't move loop */

	    /******************************************/
	    /* once-per-hero-took-time things go here */
	    /******************************************/


	} /* actual time passed */

	/****************************************/
	/* once-per-player-input things go here */
	/****************************************/

	find_ac();
	if(!flags.mv || Blind) {
	    /* redo monsters if hallu or wearing a helm of telepathy */
	    if (Hallucination) {	/* update screen randomly */
		see_monsters();
		see_objects();
		see_traps();
		if (u.uswallow) swallowed(0);
	    } else if (Unblind_telepat) {
		see_monsters();
	    } else if (Warning || Warn_of_mon)
	     	see_monsters();

	    if (vision_full_recalc) vision_recalc(0);	/* vision! */
	}

#ifdef REALTIME_ON_BOTL
        if(iflags.showrealtime) {
            /* Update the bottom line if the number of minutes has
             * changed */
            if(get_realtime() / 60 != realtime_data.last_displayed_time / 60)
                flags.botl = 1;
        }
#endif
  
	if(flags.botl || flags.botlx) bot();

	flags.move = 1;

	if(multi >= 0 && occupation) {
#if defined(MICRO) || defined(WIN32)
	    abort_lev = 0;
	    if (kbhit()) {
		if ((ch = Getchar()) == ABORT)
		    abort_lev++;
# ifdef REDO
		else
		    pushch(ch);
# endif /* REDO */
	    }
	    if (!abort_lev && (*occupation)() == 0)
#else
	    if ((*occupation)() == 0)
#endif
		occupation = 0;
	    if(
#if defined(MICRO) || defined(WIN32)
		   abort_lev ||
#endif
		   monster_nearby()) {
		stop_occupation();
		reset_eat();
	    }
#if defined(MICRO) || defined(WIN32)
	    if (!(++occtime % 7))
		display_nhwindow(WIN_MAP, FALSE);
#endif
	    continue;
	}

	if ((u.uhave.amulet || Clairvoyant) &&
	    !In_endgame(&u.uz) && !BClairvoyant &&
	    !(moves % 15) && !rn2(2))
		do_vicinity_map();

	if(u.utrap && u.utraptype == TT_LAVA) {
	    if(!is_lava(u.ux,u.uy))
		u.utrap = 0;
	    else if (!u.uinvulnerable) {
		u.utrap -= 1<<8;
		if(u.utrap < 1<<8) {
		    killer_format = KILLED_BY;
		    killer = "molten lava";
		    You("sink below the surface and die.");
		    done(DISSOLVED);
		} else if(didmove && !u.umoved) {
		    Norep("You sink deeper into the lava.");
		    u.utrap += rnd(4);
		}
	    }
	}

#ifdef WIZARD
	if (iflags.sanity_check)
	    sanity_check();
#elif defined(OBJ_SANITY)
	if (iflags.sanity_check)
	    obj_sanity_check();
#endif

#ifdef CLIPPING
	/* just before rhack */
	cliparound(u.ux, u.uy);
#endif

	u.umoved = FALSE;

	if (multi > 0) {
	    lookaround();
	    if (!multi) {
		/* lookaround may clear multi */
		flags.move = 0;
		if (flags.time) flags.botl = 1;
		continue;
	    }
	    if (flags.mv) {
		if(multi < COLNO && !--multi)
		    flags.travel = iflags.travel1 = flags.mv = flags.run = 0;
		domove();
	    } else {
		--multi;
		rhack(save_cm);
	    }
	} else if (multi == 0) {
#ifdef MAIL
	    ckmailstatus();
#endif
	    rhack((char *)0);
	}
	if (u.utotype)		/* change dungeon level */
	    deferred_goto();	/* after rhack() */
	/* !flags.move here: multiple movement command stopped */
	else if (flags.time && (!flags.move || !flags.mv))
	    flags.botl = 1;

	if (vision_full_recalc) vision_recalc(0);	/* vision! */
	/* when running in non-tport mode, this gets done through domove() */
	if ((!flags.run || iflags.runmode == RUN_TPORT) &&
		(multi && (!flags.travel ? !(multi % 7) : !(moves % 7L)))) {
	    if (flags.time && flags.run) flags.botl = 1;
	    display_nhwindow(WIN_MAP, FALSE);
	}
    }
}


#endif /* OVL0 */
#ifdef OVL1

void
stop_occupation()
{
	if(occupation) {
		if (!maybe_finished_meal(TRUE))
		    You("stop %s.", occtxt);
		occupation = 0;
		flags.botl = 1; /* in case u.uhs changed */
/* fainting stops your occupation, there's no reason to sync.
		sync_hunger();
*/
#ifdef REDO
		nomul(0, 0);
		pushch(0);
#endif
	}
}

#endif /* OVL1 */
#ifdef OVLB

void
display_gamewindows()
{
    WIN_MESSAGE = create_nhwindow(NHW_MESSAGE);
    WIN_STATUS = create_nhwindow(NHW_STATUS);
    WIN_MAP = create_nhwindow(NHW_MAP);
    WIN_INVEN = create_nhwindow(NHW_MENU);

#ifdef MAC
    /*
     * This _is_ the right place for this - maybe we will
     * have to split display_gamewindows into create_gamewindows
     * and show_gamewindows to get rid of this ifdef...
     */
	if ( ! strcmp ( windowprocs . name , "mac" ) ) {
	    SanePositions ( ) ;
	}
#endif

    /*
     * The mac port is not DEPENDENT on the order of these
     * displays, but it looks a lot better this way...
     */
    display_nhwindow(WIN_STATUS, FALSE);
    display_nhwindow(WIN_MESSAGE, FALSE);
    clear_glyph_buffer();
    display_nhwindow(WIN_MAP, FALSE);
}

void
newgame()
{
	int i;

#ifdef MFLOPPY
	gameDiskPrompt();
#endif

	flags.ident = 1;

	for (i = 0; i < NUMMONS; i++)
		mvitals[i].mvflags = mons[i].geno & G_NOCORPSE;

	init_objects();		/* must be before u_init() */

	flags.pantheon = -1;	/* role_init() will reset this */
	role_init();		/* must be before init_dungeons(), u_init(),
				 * and init_artifacts() */

	init_dungeons();	/* must be before u_init() to avoid rndmonst()
				 * creating odd monsters for any tins and eggs
				 * in hero's initial inventory */
	init_artifacts();	/* before u_init() in case $WIZKIT specifies
				 * any artifacts */
	u_init();
	init_artifacts1();	/* must be after u_init() */

#ifndef NO_SIGNAL
	(void) signal(SIGINT, (SIG_RET_TYPE) done1);
#endif
#ifdef NEWS
	if(iflags.news) display_file_area(NEWS_AREA, NEWS, FALSE);
#endif

	load_qtlist();	/* load up the quest text info */
/*	quest_init();*/	/* Now part of role_init() */

	mklev();
	u_on_upstairs();
	vision_reset();		/* set up internals for level (after mklev) */
	check_special_room(FALSE);

	flags.botlx = 1;

	/* Move the monster from under you or else
	 * makedog() will fail when it calls makemon().
	 *			- ucsfcgl!kneller
	 */

	if(MON_AT(u.ux, u.uy)) mnexto(m_at(u.ux, u.uy));
	(void) makedog();

	docrt();

	/* 5lo: Iron balls made no sense, removed.  Now that they have actual stats they don't need this kind of a
	 * nerf anyway */
#ifdef CONVICT
       if (Role_if(PM_CONVICT)) {
              setworn(mkobj(CHAIN_CLASS, TRUE), W_CHAIN);
              setworn(mkobj(BALL_CLASS, TRUE), W_BALL);
              uball->spe = 1;
              placebc();
              newsym(u.ux,u.uy);
       }
#endif /* CONVICT */

	if (flags.legacy) {
		flush_screen(1);
#ifdef CONVICT
        if (Role_if(PM_CONVICT)) {
		    com_pager(199);
        } else {
		    com_pager(1);
        }
#else
		com_pager(1);
#endif /* CONVICT */
	}
#ifdef INSURANCE
	save_currentstate();
#endif
	program_state.something_worth_saving++;	/* useful data now exists */

#if defined(RECORD_REALTIME) || defined(REALTIME_ON_BOTL)

        /* Start the timer here */
        realtime_data.realtime = (time_t)0L;

#if defined(BSD) && !defined(POSIX_TYPES)
        (void) time((long *)&realtime_data.restoretime);
#else
        (void) time(&realtime_data.restoretime);
#endif

#endif /* RECORD_REALTIME || REALTIME_ON_BOTL */

	/* Success! */
	welcome(TRUE);
	return;
}

/* show "welcome [back] to nethack" message at program startup */
void
welcome(new_game)
boolean new_game;	/* false => restoring an old game */
{
    char buf[BUFSZ];
    boolean currentgend = Upolyd ? u.mfemale : flags.female;

    /*
     * The "welcome back" message always describes your innate form
     * even when polymorphed or wearing a helm of opposite alignment.
     * Alignment is shown unconditionally for new games; for restores
     * it's only shown if it has changed from its original value.
     * Sex is shown for new games except when it is redundant; for
     * restores it's only shown if different from its original value.
     */
    *buf = '\0';
    if (new_game || u.ualignbase[A_ORIGINAL] != u.ualignbase[A_CURRENT])
	Sprintf(eos(buf), " %s", align_str(u.ualignbase[A_ORIGINAL]));
    if (!urole.name.f &&
	    (new_game ? (urole.allow & ROLE_GENDMASK) == (ROLE_MALE|ROLE_FEMALE) :
	     currentgend != flags.initgend))
	Sprintf(eos(buf), " %s", genders[currentgend].adj);

#if 0
    pline(new_game ? "%s %s, welcome to NetHack!  You are a%s %s %s."
		   : "%s %s, the%s %s %s, welcome back to NetHack!",
	  Hello((struct monst *) 0), plname, buf, urace.adj,
	  (currentgend && urole.name.f) ? urole.name.f : urole.name.m);
#endif
    if (new_game) pline("%s %s, welcome to %s!  You are a%s %s %s.",
	  Hello((struct monst *) 0), plname, DEF_GAME_NAME, buf, urace.adj,
	  (currentgend && urole.name.f) ? urole.name.f : urole.name.m);
    else pline("%s %s, the%s %s %s, welcome back to %s!",
	  Hello((struct monst *) 0), plname, buf, urace.adj,
	  (currentgend && urole.name.f) ? urole.name.f : urole.name.m, 
	  DEF_GAME_NAME);

	if (Role_if(PM_ACTIVISTOR) && new_game) {

		int ammount;
		ammount = 0;

		while (ammount < 5) {

		switch (rnd(50)) {

		case 1: 
		case 2: 
		case 3: 
		    HFire_resistance |= FROMOUTSIDE; break;
		case 4: 
		case 5: 
		case 6: 
		    HCold_resistance |= FROMOUTSIDE; break;
		case 7: 
		case 8: 
		case 9: 
		    HSleep_resistance |= FROMOUTSIDE; break;
		case 10: 
		case 11: 
		    HDisint_resistance |= FROMOUTSIDE; break;
		case 12: 
		case 13: 
		case 14: 
		    HShock_resistance |= FROMOUTSIDE; break;
		case 15: 
		case 16: 
		case 17: 
		    HPoison_resistance |= FROMOUTSIDE; break;
		case 18: 
		    HDrain_resistance |= FROMOUTSIDE; break;
		case 19: 
		    HSick_resistance |= FROMOUTSIDE; break;
		case 20: 
		    HAcid_resistance |= FROMOUTSIDE; break;
		case 21: 
		case 22: 
		    HHunger |= FROMOUTSIDE; break;
		case 23: 
		case 24: 
		    HSee_invisible |= FROMOUTSIDE; break;
		case 25: 
		    HTelepat |= FROMOUTSIDE; break;
		case 26: 
		case 27: 
		    HWarning |= FROMOUTSIDE; break;
		case 28: 
		case 29: 
		    HSearching |= FROMOUTSIDE; break;
		case 30: 
		case 31: 
		    HStealth |= FROMOUTSIDE; break;
		case 32: 
		case 33: 
		    HAggravate_monster |= FROMOUTSIDE; break;
		case 34: 
		    HConflict |= FROMOUTSIDE; break;
		case 35: 
		case 36: 
		    HTeleportation |= FROMOUTSIDE; break;
		case 37: 
		    HTeleport_control |= FROMOUTSIDE; break;
		case 38: 
		    HFlying |= FROMOUTSIDE; break;
		case 39: 
		    HSwimming |= FROMOUTSIDE; break;
		case 40: 
		    HMagical_breathing |= FROMOUTSIDE; break;
		case 41: 
		    HSlow_digestion |= FROMOUTSIDE; break;
		case 42: 
		case 43: 
		    HRegeneration |= FROMOUTSIDE; break;
		case 44: 
		    HPolymorph |= FROMOUTSIDE; break;
		case 45: 
		    HPolymorph_control |= FROMOUTSIDE; break;
		case 46: 
		case 47: 
		    HFast |= FROMOUTSIDE; break;
		case 48: 
		    HInvis |= FROMOUTSIDE; break;
		default:
			break;
			}
		ammount++;

		}
	}

	if (flags.lostsoul && !flags.uberlostsoul && new_game) { 
	goto_level(&medusa_level, TRUE, FALSE, FALSE); /* inspired by Tome, an Angband mod --Amy */
	pline("These are the halls of Mandos... err, Medusa. Good luck making your way back up!");
	}

	if (flags.uberlostsoul && new_game) { 

	goto_level((&sanctum_level - 1), TRUE, FALSE, FALSE);
	pline("These are the halls of Mandos... err, Gehennom. Looks nice, huh?");

			        register int newlev = 64;
				d_level newlevel;
				get_level(&newlevel, newlev);
				goto_level(&newlevel, TRUE, FALSE, FALSE);
				pline("Enjoy your stay, and try to get out if you can.");


	}

	if (Role_if(PM_TRANSVESTITE) && new_game && flags.female) {
		    makeknown(AMULET_OF_CHANGE);
		    You("don't feel like being female!");
			change_sex();
		    flags.botl = 1;

	}

	if (Role_if(PM_TOPMODEL) && new_game && !flags.female) {
		    makeknown(AMULET_OF_CHANGE);
		    You("don't feel like being male!");
			change_sex();
		    flags.botl = 1;

	}

	if (Race_if(PM_UNGENOMOLD) && new_game) {
		  makeknown(SCR_GENOCIDE);
	    polyself(FALSE);
		mvitals[PM_UNGENOMOLD].mvflags |= (G_GENOD|G_NOCORPSE);
	    pline("Wiped out all ungenomolds.");
 		You_feel("dead inside.");

	}
#ifdef LIVELOGFILE
	/* Start live reporting */
		  livelog_start();
#endif

}

#ifdef POSITIONBAR
STATIC_DCL void
do_positionbar()
{
	static char pbar[COLNO];
	char *p;
	
	p = pbar;
	/* up stairway */
	if (upstair.sx &&
#ifdef DISPLAY_LAYERS
	   (level.locations[upstair.sx][upstair.sy].mem_bg == S_upstair ||
	    level.locations[upstair.sx][upstair.sy].mem_bg == S_upladder)) {
#else
	   (glyph_to_cmap(level.locations[upstair.sx][upstair.sy].glyph) ==
	    S_upstair ||
 	    glyph_to_cmap(level.locations[upstair.sx][upstair.sy].glyph) ==
	    S_upladder)) {
#endif
		*p++ = '<';
		*p++ = upstair.sx;
	}
	if (sstairs.sx &&
#ifdef DISPLAY_LAYERS
	   (level.locations[sstairs.sx][sstairs.sy].mem_bg == S_upstair ||
	    level.locations[sstairs.sx][sstairs.sy].mem_bg == S_upladder)) {
#else
	   (glyph_to_cmap(level.locations[sstairs.sx][sstairs.sy].glyph) ==
	    S_upstair ||
 	    glyph_to_cmap(level.locations[sstairs.sx][sstairs.sy].glyph) ==
	    S_upladder)) {
#endif
		*p++ = '<';
		*p++ = sstairs.sx;
	}

	/* down stairway */
	if (dnstair.sx &&
#ifdef DISPLAY_LAYERS
	   (level.locations[dnstair.sx][dnstair.sy].mem_bg == S_dnstair ||
	    level.locations[dnstair.sx][dnstair.sy].mem_bg == S_dnladder)) {
#else
	   (glyph_to_cmap(level.locations[dnstair.sx][dnstair.sy].glyph) ==
	    S_dnstair ||
 	    glyph_to_cmap(level.locations[dnstair.sx][dnstair.sy].glyph) ==
	    S_dnladder)) {
#endif
		*p++ = '>';
		*p++ = dnstair.sx;
	}
	if (sstairs.sx &&
#ifdef DISPLAY_LAYERS
	   (level.locations[sstairs.sx][sstairs.sy].mem_bg == S_dnstair ||
	    level.locations[sstairs.sx][sstairs.sy].mem_bg == S_dnladder)) {
#else
	   (glyph_to_cmap(level.locations[sstairs.sx][sstairs.sy].glyph) ==
	    S_dnstair ||
 	    glyph_to_cmap(level.locations[sstairs.sx][sstairs.sy].glyph) ==
	    S_dnladder)) {
#endif
		*p++ = '>';
		*p++ = sstairs.sx;
	}

	/* hero location */
	if (u.ux) {
		*p++ = '@';
		*p++ = u.ux;
	}
	/* fence post */
	*p = 0;

	update_positionbar(pbar);
}
#endif

#if defined(REALTIME_ON_BOTL) || defined (RECORD_REALTIME)
time_t
get_realtime(void)
{
    time_t curtime;

    /* Get current time */
#if defined(BSD) && !defined(POSIX_TYPES)
    (void) time((long *)&curtime);
#else
    (void) time(&curtime);
#endif

    /* Since the timer isn't set until the game starts, this prevents us
     * from displaying nonsense on the bottom line before it does. */
    if(realtime_data.restoretime == 0) {
        curtime = realtime_data.realtime;
    } else {
        curtime -= realtime_data.restoretime;
        curtime += realtime_data.realtime;
    }
 
    return curtime;
}
#endif /* REALTIME_ON_BOTL || RECORD_REALTIME */


#endif /* OVLB */

/*allmain.c*/
