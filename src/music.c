/*	SCCS Id: @(#)music.c	3.4	2003/05/25	*/
/*	Copyright (c) 1989 by Jean-Christophe Collet */
/* NetHack may be freely redistributed.  See license for details. */

/*
 * This file contains the different functions designed to manipulate the
 * musical instruments and their various effects.
 *
 * Actually the list of instruments / effects is :
 *
 * (wooden) flute	may calm snakes if player has enough dexterity
 * magic flute		may put monsters to sleep:  area of effect depends
 *			on player level.
 * (tooled) horn	Will awaken monsters:  area of effect depends on player
 *			level.  May also scare monsters.
 * fire horn		Acts like a wand of fire.
 * frost horn		Acts like a wand of cold.
 * bugle		Will awaken soldiers (if any):  area of effect depends
 *			on player level.
 * (wooden) harp	May calm nymph if player has enough dexterity.
 * magic harp		Charm monsters:  area of effect depends on player
 *			level.
 * (leather) drum	Will awaken monsters like the horn.
 * drum of earthquake	Will initiate an earthquake whose intensity depends
 *			on player level.  That is, it creates random pits
 *			called here chasms.
 */

#include "hack.h"
#include "skills.h"
#include "edog.h"

STATIC_DCL void FDECL(awaken_monsters,(int));
STATIC_DCL void FDECL(put_monsters_to_sleep,(int));
STATIC_DCL void FDECL(charm_snakes,(int));
STATIC_DCL void FDECL(calm_nymphs,(int));
STATIC_DCL void FDECL(charm_monsters,(int));
STATIC_DCL void FDECL(do_earthquake,(int));
STATIC_DCL int FDECL(do_improvisation,(struct obj *));
STATIC_DCL void FDECL(tame_song,(int));
STATIC_DCL void FDECL(sleep_song,(int));
STATIC_DCL void FDECL(scary_song,(int));
STATIC_DCL void FDECL(confusion_song,(int));
STATIC_DCL void FDECL(cancel_song,(int));
STATIC_DCL void FDECL(rally_song,(int));
STATIC_DCL unsigned char FDECL(songs_menu,(struct obj *));
STATIC_PTR int NDECL(play_song);
STATIC_DCL void FDECL(slowness_song,(int));
STATIC_DCL void FDECL(haste_song,(int));
STATIC_DCL void FDECL(heal_song,(int));
STATIC_DCL void FDECL(encourage_pets,(int));


#ifdef UNIX386MUSIC
STATIC_DCL int NDECL(atconsole);
STATIC_DCL void FDECL(speaker,(struct obj *,char *));
#endif
#ifdef VPIX_MUSIC
extern int sco_flag_console;	/* will need changing if not _M_UNIX */
STATIC_DCL void NDECL(playinit);
STATIC_DCL void FDECL(playstring, (char *,size_t));
STATIC_DCL void FDECL(speaker,(struct obj *,char *));
#endif
#ifdef PCMUSIC
void FDECL( pc_speaker, ( struct obj *, char * ) );
#endif
#ifdef AMIGA
void FDECL( amii_speaker, ( struct obj *, char *, int ) );
#endif

struct songspell {
	short	sp_id;
	char	*name;
	xchar	level;
	xchar	turns;
	int	instr1; /* this instrument can play this music */
	int	instr2; /* this one has a bonus to successfully play this song */
};	

/* we need at least these defines (so they're outside #define BARD) */
#define SNG_NONE            0
#define SNG_IMPROVISE       99
#define SNG_NOTES           98
#define SNG_PASSTUNE        97
/* same order and indices as below */
#define SNG_SLEEP		1
#define SNG_CONFUSION	2
#define SNG_SLOW		3
#define SNG_FEAR		4
#define SNG_TAME		5
#define SNG_COURAGE		6
#define SNG_FIRST		SNG_SLEEP
#define SNG_LAST_ENCHANTMENT	SNG_COURAGE	/* last song based on an enchantment spell */
#define SNG_HASTE		7
#define SNG_HEAL		8
#define SNG_CNCL		9
#define SNG_RLLY	   10
#define SNG_LAST		SNG_RLLY
#define SNG_IMPROVISE_CHAR	'x'
#define SNG_NOTES_CHAR		'n'
#define SNG_PASSTUNE_CHAR	'p'

/* songs based on enchantment spells must be the first ones on list, because of
   SNG_LAST_ENCHANTMENT */
NEARDATA const struct songspell songs[] = {
	/* sp_id		name	    level turns instr1		instr2 */
	{ 0,				"None",		0, 1,	0,		0 },
	{ SPE_SLEEP,		"Lullaby",	1, 4,			WOODEN_HARP,	WOODEN_FLUTE },
	{ SPE_CONFUSE_MONSTER,	"Cacophony",	2, 5,	LEATHER_DRUM,	TOOLED_HORN },
	{ SPE_SLOW_MONSTER,	"Drowsiness",	2, 5,		WOODEN_FLUTE,	WOODEN_HARP },
	{ SPE_CAUSE_FEAR,	"Despair",	3, 6,			LEATHER_DRUM,	TOOLED_HORN },
	{ SPE_CHARM_MONSTER,"Friendship",	3, 6,		WOODEN_FLUTE,	WOODEN_HARP },
	{ SPE_CAUSE_FEAR,	"Inspire Courage",3,6,		LEATHER_DRUM,	BUGLE },
	{ SPE_HASTE_SELF,	"Charge", 2, 5, 			LEATHER_DRUM,	BUGLE },
	{ SPE_EXTRA_HEALING,"Meditative Healing", 1,4,	WOODEN_HARP,	WOODEN_FLUTE },
	{ SPE_CANCELLATION,	"Disruption", 2,5,			BUGLE,			TOOLED_HORN },
	{ SPE_TELEPORT_AWAY,"Rally",	1, 1,			TOOLED_HORN,	BUGLE }
/*	random ideas that weren't implemented -- based in spells from other schools
	{ SPE_CURE_BLINDNESS,	"Cause Blindness"
	{ SPE_CURE_SICKNESS,	"Cause Sickness"
	{ SPE_POLYMORPH,	"Change?", poly pets to higher level monster temporarily
	{ SPE_FORCE_BOLT,	"Shatter", shatter glass/wood/stone objects
	not spell based
	{ ?		       	"Shout/Sound Burst", area damage
	{ ?			"Silence", monster spells fail
	{ ?			"Ventriloquism", makes monster think you're at a given location
	{ ?			"Rage", pet uses special attack
*/
};

static NEARDATA schar song_delay;	/* moves left for this song */
struct obj *song_instr;			/* musical instrument being played */
uchar song_played = SNG_NONE;	/* song being played (songs[] index)*/
boolean song_penalty;			/* instrument penalty (see do_play_instrument) */
static NEARDATA int petsing;		/* effect of pets singing with the player */
static NEARDATA long petsing_lastcheck = 0L; /* last time pets were checked */
static NEARDATA char msgbuf[BUFSZ];


/*
 * Ugly kludge. Returns the song being played at the moment.
 * We cannot check song_played directly because if the song was interrupted,
 * we have no means to reset song_played.
 */
int inline
song_being_played()
{
    if (occupation != play_song)
		song_played = SNG_NONE;
    return song_played;
}

STATIC_PTR int
reset_song()
{
    song_played = SNG_NONE;
    song_delay = 0;
    song_penalty = 0;
/*	song_lastturn = 0L;*/
    return 0;
}

/* music being played is at its last turn? */
#define	SNG_FIRST_TURN	(song_being_played() == SNG_NONE ? \
			 FALSE : (song_delay == songs[song_being_played()].turns))


/**
 * Checks if this pet can sing, helping the player, returning a bonus
 */
int
pet_can_sing(mtmp, domsg)
struct monst *mtmp;
boolean domsg;
{
    int r = 0;

    if (song_being_played() == SNG_NONE) return 0;

    if ((mtmp->mcanmove) && (!mtmp->msleeping) && (!Conflict)
	&& (!mtmp->mconf) && (!mtmp->mflee) && (!mtmp->mcan)
	&& (distu(mtmp->mx, mtmp->my) <= 25)) {
	    /* nymphs and some elves sing along harps */
	    if ((song_instr->otyp == WOODEN_HARP || song_instr->otyp == MAGIC_HARP)
		&& (mtmp->data->mlet == S_NYMPH 
		    || (mtmp->data >= &mons[PM_ELF] && mtmp->data <= &mons[PM_ELVENKING])) 
		&& (mtmp->mhp*2 > mtmp->mhpmax))
		    r = max(10,(mtmp->data->mlet == S_NYMPH ? mtmp->m_lev*2 : mtmp->m_lev));
	    /* undeads sing along horns */
	    else if ((song_instr->otyp == TOOLED_HORN)
		     && (mtmp->data->mlet == S_LICH || mtmp->data->mlet == S_MUMMY
			 || mtmp->data->mlet == S_VAMPIRE || mtmp->data->mlet == S_WRAITH
			 || mtmp->data->mlet == S_DEMON || mtmp->data->mlet == S_GHOST))
		    r = max(10,(mtmp->data->mlet == S_LICH || mtmp->data->mlet == S_DEMON
				? mtmp->m_lev*2 : mtmp->m_lev));
	    /* orcs and ogres sing along (shout, actually) drums and bugles */
	    else if ((song_instr->otyp == LEATHER_DRUM || song_instr->otyp == BUGLE)
		     && (mtmp->data->mlet == S_ORC || mtmp->data->mlet == S_OGRE))
		    r = max(10, mtmp->m_lev);
    }

    if (domsg && (r > 0))
		if (canseemon(mtmp)) {
			if (mtmp->data->mlet == S_LICH || mtmp->data->mlet == S_DEMON 
				|| mtmp->data->mlet == S_VAMPIRE)
				pline("%s's dreadful voice chants your song!", Monnam(mtmp));
			else if (mtmp->data->mlet == S_MUMMY || mtmp->data->mlet == S_GHOST
					 || mtmp->data->mlet == S_WRAITH)
				pline("%s mourns while you play!", Monnam(mtmp));
			else if (mtmp->data->mlet == S_NYMPH)
				pline("%s's charming voice sings along!", Monnam(mtmp));
			else if (mtmp->data->mlet == S_ORC || mtmp->data->mlet == S_OGRE)
				pline("%s shouts!", Monnam(mtmp));
			else
				pline("%s sings while you play!", Monnam(mtmp));
		}
	else
		if (mtmp->data->mlet == S_LICH || mtmp->data->mlet == S_DEMON 
			|| mtmp->data->mlet == S_VAMPIRE)
			You_hear("a horrible voice chanting your song!");
		else if (mtmp->data->mlet == S_MUMMY || mtmp->data->mlet == S_GHOST
				 || mtmp->data->mlet == S_WRAITH)
			You_hear("someone mourning while you play!", Monnam(mtmp));
		else if (mtmp->data->mlet == S_NYMPH)
			You_hear("a charming voice singing along!");
		else if (mtmp->data->mlet == S_ORC || mtmp->data->mlet == S_OGRE)
			You_hear("a shout!");
		else
			You_hear("someone singing while you play!");

    return r;
}

/** Singing pets bonus for this turn.
 * Calculates the bonus of all singing pets (see above), if not yet done
 * for this turn
 */
STATIC_DCL int
singing_pets_effect()
{
    register struct monst *mtmp;

    if (song_being_played() == SNG_NONE) return 0;
    if (monstermoves != petsing_lastcheck) {
	petsing_lastcheck = monstermoves;
	petsing = 0;
	for (mtmp = fmon; mtmp; mtmp = mtmp->nmon)
	    if (mtmp->mtame)
		petsing += pet_can_sing(mtmp, TRUE);
    }

    return petsing;
}


/** Chance of succesfully playing a song. 
 * Depends on dexterity, different from the chance of the song actually affecting
 * the creature, which depends more on charisma than dexterity.
 */
int
song_success(song_id, instr, know_spell)
int song_id;
struct obj *instr;
int know_spell;
{
	int a;
	int chance;

	//TODO: must check if the spell is still memorized

	if (!know_spell)
		return 0;

	chance = ( ACURR(A_DEX) * 2 * (P_SKILL(P_MUSICALIZE)-P_UNSKILLED+1) + u.ulevel)
		- (songs[song_id].level * (instr->blessed ? 15 : 20));

	if (instr->oartifact || instr->otyp == songs[song_id].instr2)
		chance = (chance*3)/2;

	/* not easy to play 'peaceful' music when badly injured */
	if (u.uhp < u.uhpmax * 0.3 && 
		(song_id == SNG_SLEEP || song_id == SNG_TAME || song_id == SNG_HASTE || song_id == SNG_HEAL)
	) chance /= 2;

	/* it's also difficult to play some instruments while wearing a shield. */
	if (uarms && (instr->otyp == WOODEN_HARP || instr->otyp == LEATHER_DRUM)) 
		chance /= 2;

	/* but it's easier with the eyes closed */
	if (Blind) chance += u.ulevel;

	if (instr->oartifact == ART_LYRE_OF_ORPHEUS && (song_id != SNG_RLLY && song_id != SNG_HEAL && song_id != SNG_SLEEP && song_id != SNG_TAME))
		chance /= 2;

	if (chance > 100) chance = 100;
	if (chance < 0) chance = 0;

	return chance;
}

/**
 * Shows the music menu.
 * Returns a SNG_* identifier
 */
STATIC_DCL unsigned char
songs_menu(instr)
struct obj *instr;
{
	char buf[BUFSZ];
	winid tmpwin;
	anything any;
	menu_item *selected;
	int a,b;
	int song, know_spell;
	char hardtoplay;

	tmpwin = create_nhwindow(NHW_MENU);
	start_menu(tmpwin);
	any.a_void = 0;  /* zero out all bits */
	Sprintf(buf, "(songs marked with # have a bonus with this instrument)");
	add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, buf, MENU_UNSELECTED);
	add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE, "", MENU_UNSELECTED);
	Sprintf(buf, "    Song             Level Turns   Fail");
	add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_BOLD, buf, MENU_UNSELECTED);

	/* improvise option */
	any.a_int = SNG_IMPROVISE;
	add_menu(tmpwin, NO_GLYPH, &any, SNG_IMPROVISE_CHAR, 0, ATR_NONE,
		 "improvise", MENU_UNSELECTED);
	/* play notes option */
	any.a_int = SNG_NOTES;
	add_menu(tmpwin, NO_GLYPH, &any, SNG_NOTES_CHAR, 0, ATR_NONE,
		 "a sequence of notes", MENU_UNSELECTED);
	/* play the passtune option */
	if (u.uevent.uheard_tune == 2) {
		any.a_int = SNG_PASSTUNE;
		add_menu(tmpwin, NO_GLYPH, &any, SNG_PASSTUNE_CHAR, 0, ATR_NONE,
			 "passtune", MENU_UNSELECTED);
	}

	for (a = SNG_FIRST; a <= SNG_LAST; a++) {
		/* For a song to be available in the menu:
		   - Need a suitable instrument (the Lyre of Orpheus can play any song)
		   - Must know the related spell (Bards already know any enchantment based song)
		*/
		know_spell = (!P_RESTRICTED(P_MUSICALIZE));
		if (!know_spell)
			for (b = 0; b < MAXSPELL; b++)
				if (spl_book[b].sp_id == songs[a].sp_id)
					know_spell = TRUE;
		
		if (know_spell && (instr->oartifact == ART_LYRE_OF_ORPHEUS
				   || instr->otyp == songs[a].instr1 || instr->otyp == songs[a].instr2)) {
			any.a_int = a+1;
			if (instr->oartifact == ART_LYRE_OF_ORPHEUS)
				hardtoplay = (a == SNG_FEAR || a == SNG_COURAGE || a == SNG_CONFUSION || a == SNG_SLOW || a == SNG_CNCL ? ' ' : '#');
			else
				hardtoplay = (songs[a].instr1 == instr->otyp ? ' ' : '#');
					
			Sprintf(buf, "%-20s %i     %i %c %3i%%", songs[a].name, 
				songs[a].level,	songs[a].turns,
				hardtoplay,
				100 - song_success(a, instr, know_spell));
			add_menu(tmpwin, NO_GLYPH, &any, 0, 0, ATR_NONE,
				 buf, MENU_UNSELECTED);
		}
	}

	Sprintf(buf, "Play which song with the %s?", xname(instr));
	end_menu(tmpwin, buf);
	a = select_menu(tmpwin, PICK_ONE, &selected);
	if (a > 0)
		song = selected[0].item.a_int - (selected[0].item.a_int >= SNG_PASSTUNE ? 0 : 1);
	else
		song = SNG_NONE;
	/*  if (song > 0) song--; */
	free((genericptr_t)selected);
	destroy_nhwindow(tmpwin);
	
	return song;
}


/** Returns a positive number if monster is affected by song, or a negative number
 * if monster resisted it. 
 * The number means the difference between the song 'attack' level and the monster
 * resistance level against it.
 * Charisma weights more than dexterity, different from the chance of 
 * successfully playing the song, which depends on dexterity.
 */
int
resist_song(mtmp, song, instr)
struct monst *mtmp;
int song;
struct obj * instr;
{
	/* atack level, defense level, defense level before modifiers */
	int alev, dlev, dlev0;
	int showmsg;
	char *msg;

	showmsg = (song_delay == songs[song_played].level + 3) && canseemon(mtmp);
	msg = (void *)0;

	/* Attack level */
	//alev = min(P_SKILL(P_MUSICALIZE) - songs[song].level + 1, 0);
	alev = P_SKILL(P_MUSICALIZE) - P_UNSKILLED + 1;
	alev = ( (alev * ACURR(A_CHA)) + ACURR(A_DEX) ) / 3;
	// blessed/cursed instruments make it a little easier/harder to 'cast' the song
	alev += bcsign(instr)*5;
	// account for pets that can sing with the bard's song
	alev += max(0, singing_pets_effect() - song_delay);
	// polymorphed into something that can't sing
	if (is_silent(youmonst.data)) alev /= 2;

	/* Defense level */
	dlev = (int)mtmp->m_lev*2;
	if (nonliving(mtmp->data) && mindless(mtmp->data)) dlev = 100;
	dlev0 = dlev;

	/* "peaceful" songs */
	if (song == SNG_SLEEP || song == SNG_TAME || song == SNG_HASTE || song == SNG_HEAL) {
		if (mindless(mtmp->data)) dlev += dlev0/10;
		if (is_animal(mtmp->data)) dlev -= dlev0/10; // music calm the beasts
		if (is_domestic(mtmp->data)) dlev -= dlev0/10;
		if (likes_magic(mtmp->data)) dlev += dlev0/5;
		if (your_race(mtmp->data)) dlev -= dlev0/10;

		// undead and demons don't care about 'peaceful' music
		if (is_undead(mtmp->data) || is_demon(mtmp->data)) dlev += 50;
		if (always_hostile(mtmp->data)) dlev += dlev0/10;
		if (race_peaceful(mtmp->data)) dlev -= dlev0/10;

		// rats like music from flutes (The Pied Piper of Hamelin)
		if (mtmp->data->mlet == S_RODENT && instr->otyp == WOODEN_FLUTE) {
			dlev -= dlev0/5;
			if (showmsg) msg = "%s seems to briefly swing with your music.";
		}
		// angels like the sound of harps
		if ((mtmp->data->mlet == S_ANGEL) && (mtmp->malign >= A_COALIGNED)
		    && (instr->otyp == WOODEN_HARP))
			dlev -= dlev0/5;
		// snakes (and nagas) also like music from flutes
		if (((mtmp->data->mlet == S_SNAKE) || (mtmp->data->mlet == S_NAGA))
		    && (instr->otyp == WOODEN_FLUTE)) {
			dlev -= dlev0/5;
			if (showmsg) msg = "%s briefly dances with your music.";
		}
		// the Lyre of Orpheus is very good at peaceful music
		if (instr->oartifact == ART_LYRE_OF_ORPHEUS)
			alev += (P_SKILL(P_MUSICALIZE) - P_UNSKILLED) * 5;

		// finally, music will do little effect on monsters if they're badly injured
		if (mtmp->mhp < mtmp->mhpmax*0.3) {
			dlev *= 5;
			if(song == SNG_HEAL) dlev *= -1;
			else if (song_delay == songs[song].turns && showmsg)
				msg = "%s cares more about surviving than listening to your music!";
		} else if (mtmp->mhp < mtmp->mhpmax*0.6) {
			dlev *= 2;
			if(song == SNG_HEAL) dlev *= -1;
			else if (song_delay == songs[song].turns && showmsg)
				msg = "%s is too hurt to listen closely to your song.";
		}
	} else if (song == SNG_FEAR || song == SNG_CONFUSION || song == SNG_CNCL) {
		int canseeu;

		// the Lyre isn't so good to scare people or to sow confusion
		if (instr->oartifact == ART_LYRE_OF_ORPHEUS) alev /= 2;
		// undeads and demons like scary music
		if (song == SNG_FEAR && is_undead(mtmp->data)) dlev -= dlev0/3;
		if (song == SNG_FEAR && is_demon(mtmp->data)) dlev -= dlev0/5;
		// monster is scared/confused easily if it can't see you
		canseeu = m_canseeu(mtmp);
		if (!canseeu) dlev -= dlev0/5;
		// but its harder to confuse it if it can see you
		if (song == SNG_CONFUSION && canseeu) dlev += dlev0/5;

	} else if (song == SNG_COURAGE || song == SNG_RLLY) {
		/* when badly injured, it's easier to encourage others */
		if (u.uhp < u.uhpmax * 0.6) alev *= 2;
		if (u.uhp < u.uhpmax * 0.3) alev *= 3;
		/* hostile monsters are easily encouraged */
		if (always_hostile(mtmp->data)) dlev -= dlev0/5;
		if (race_hostile(mtmp->data)) dlev -= dlev0/5;
		if (is_mercenary(mtmp->data)) dlev -= dlev0/5;
	}

    if (dlev < 1) dlev = is_mplayer(mtmp->data) ? u.ulevel : 1;
    if (song_penalty) alev /= 2;

	alev += rnd(20);
	
    if (wizard)
	    pline("[%s:%i/%i]", mon_nam(mtmp), alev, dlev);

    if (alev >= dlev && msg != (void *)0)
	    pline(msg, Monnam(mtmp));

    return (alev - dlev);
}




STATIC_PTR int
play_song()
{
	register struct monst *mtmp;
	int distance;

	distance = (P_SKILL(P_MUSICALIZE) - P_UNSKILLED + 1) * 9 + (u.ulevel/2);

	/* songs only have effect after the 1st turn */
	//if (song_delay <= songs[song_played].level+2) 
	switch (song_being_played()) {
		case SNG_SLEEP: 
			sleep_song(distance);
			break;
		case SNG_CONFUSION:
			confusion_song(distance);
			break;
		case SNG_SLOW:
			slowness_song(distance);
			break;
		case SNG_HASTE:
			haste_song(distance);
			break;
		case SNG_HEAL:
			heal_song(distance);
			break;
		case SNG_FEAR:
			scary_song(distance);
			break;
		case SNG_COURAGE:
			encourage_pets(distance);
			break;
		case SNG_TAME:
			tame_song(distance);
			break;
		case SNG_CNCL:
			cancel_song(distance);
			break;
		case SNG_RLLY:
			rally_song(distance);
			break;
		}

	song_delay--;
	if (song_delay <= 0) {
		reset_song();
		use_skill(P_MUSICALIZE, 1);
		exercise(A_DEX, TRUE);
		/*if (songs[song_played].turns > 1)*/
		You("finish the song.");
		return 0;
	}
	return 1;
}


/* monster is affected by song if:
   - not tamed; or
   - if tamed, player's musicalize skill is only basic, AND tamed monster
     isn't of any kind that sings with your song (considering peaceful
	 songs only)
   Rationale: a skilled musician must be able to, say, make enemies sleep but
   not his/her pets, but a less skilled one will end up affecting pets too.
*/
#define mon_affected_by_peace_song(mtmp) \
	(!mtmp->mtame || \
	(mtmp->mtame && (P_SKILL(P_MUSICALIZE) < P_SKILLED) && \
	 mtmp->data->mlet != S_NYMPH &&	!is_elf(mtmp->data)))
#define mon_affected_by_song(mtmp) \
	(!mtmp->mtame || \
	 (mtmp->mtame && (P_SKILL(P_MUSICALIZE) < P_SKILLED)))


/** Fear song effects.
 * Contributed by Johanna Ploog
 */
void
scary_song(distance)
int distance;
{
	register struct monst *mtmp, *m = fmon;
	register int r;

	while(m) {
		mtmp = m;
		m = m->nmon;
		r = resist_song(mtmp, SNG_FEAR, song_instr);

		if (!DEADMONSTER(mtmp) && distu(mtmp->mx, mtmp->my) < distance &&
			mon_affected_by_song(mtmp) && r >= 0) {

			if (is_undead(mtmp->data) || is_demon(mtmp->data)) {
				// small chance of side effect
				r = r/songs[song_being_played()].turns;
				if (wizard) pline("[%i%% side effect]", r);
			}
	
			/* fear song actually can pacify undead */
			if (is_undead(mtmp->data)) {
				if (rn2(100) < r) {
					if (canseemon(mtmp))
						pline((Hallucination ? "%s starts to coreograph a dance!" 
						       : (mtmp->data->mlet == S_LICH ? 
							  "%s makes a sinister grin in approval of your music."
							  : "%s groans in the rhythm of your music.")),
						      Monnam(mtmp));
					mtmp->mpeaceful = 1;
					mtmp->mavenge = 0;
				} else {
					if (canseemon(mtmp) && SNG_FIRST_TURN)
						pline("%s stops to %s your music for a moment.", Monnam(mtmp),
						      (Hallucination ? "smell" : "hear"));
					mtmp->movement = 0;
				}
			} else if (is_demon(mtmp->data)) {
				/* but angers demon lords */
				if (is_dlord(mtmp->data) || is_dprince(mtmp->data)) {
					pline("%s laughs fiendishly!", Monnam(mtmp));
					verbalize("Thou playest thy own funeral march, weakling!");
					mtmp->mpeaceful = 0;
					mtmp->mavenge = 1;
				} else if (rn2(100) < r) {
					/* and can pacify normal demons */
					if (canseemon(mtmp))
						pline("%s makes a sinister grin in approval of your music.", Monnam(mtmp));
					mtmp->mpeaceful = 1;
					mtmp->mavenge = 0;
				} else {
					if (canseemon(mtmp) && SNG_FIRST_TURN)
						pline("%s to %s your music for a moment.", Monnam(mtmp),
						      (Hallucination ? "smell" : "hear"));
					mtmp->movement = 0;
				}
			} else {
				monflee(mtmp, 
					min(1, P_SKILL(P_MUSICALIZE)-P_UNSKILLED) * 3, 
					FALSE, TRUE);
				if (mtmp->mpeaceful && rn2(10)) {
					mtmp->mpeaceful = 0;
				}
			}
			/*
			  possible additions:
			  + aggravate undeads
			  + chance to unpacify peaceful (non-deaf) monsters
			  (humans are not affected by this, they're used to worse... ;-) )
			*/
		}
	}
}


STATIC_OVL void
slowness_song(distance)
int distance;
{
	register struct monst *mtmp = fmon;
	register int distm;

	while(mtmp) {
		if (!DEADMONSTER(mtmp) && distu(mtmp->mx, mtmp->my) < distance &&
			mon_affected_by_peace_song(mtmp) &&
		    resist_song(mtmp, SNG_SLOW, song_instr) >= 0) {
			switch (P_SKILL(P_MUSICALIZE)) {
			case P_UNSKILLED:
			case P_BASIC:
				mtmp->movement -= (NORMAL_SPEED/2);
				break;
			case P_SKILLED:
				mtmp->movement -= (NORMAL_SPEED*3/4);
				break;
			case P_EXPERT:
				mtmp->movement -= (NORMAL_SPEED-1);
				break;
			}
			//mtmp->movement -= NORMAL_SPEED / (5 - max(1,P_SKILL(P_MUSICALIZE)-P_UNSKILLED));
			if (song_delay == songs[SNG_SLOW].turns && canseemon(mtmp))
				pline("%s seems slower.", Monnam(mtmp));

			if (u.uswallow && (mtmp == u.ustuck) &&
			    is_whirly(mtmp->data)) {
				You("disrupt %s!", mon_nam(mtmp));
				pline("A huge hole opens up...");
				expels(mtmp, mtmp->data, TRUE);
			}
		}
	    mtmp = mtmp->nmon;
	}
}


STATIC_OVL void
haste_song(distance)
int distance;
{
	register struct monst *mtmp = fmon;
	register int distm;
	
	while(mtmp) {
		if (!DEADMONSTER(mtmp) && distu(mtmp->mx, mtmp->my) < distance &&
			mtmp->mtame && resist_song(mtmp, SNG_HASTE, song_instr) >= 0
		) {
			mtmp->movement += max(1,P_SKILL(P_MUSICALIZE))*3;
			if (song_delay == songs[SNG_HASTE].turns && canseemon(mtmp))
				pline("%s moves quickly to attack.", Monnam(mtmp));
		}
	    mtmp = mtmp->nmon;
	}
}


STATIC_OVL void
heal_song(distance)
int distance;
{
	register struct monst *mtmp = fmon;
	register int distm;
	
	while(mtmp) {
		if (!DEADMONSTER(mtmp) && mtmp->mtame && mtmp->mhp < mtmp->mhpmax &&
			distu(mtmp->mx, mtmp->my) < distance && 
			resist_song(mtmp, SNG_HEAL, song_instr) >= 0
		) {
			mtmp->mcanmove = 0;
			mtmp->mfrozen = 1;
			if(song_delay - max(1,P_SKILL(P_MUSICALIZE)) > 0){
				mtmp->mhp++;
			} else {
				mtmp->mhp += mtmp->m_lev;
				if (canseemon(mtmp)) pline("%s looks %s!", Monnam(mtmp), Hallucination ? "mellow" : "well rested");
			}
			if(mtmp->mhp > mtmp->mhpmax) mtmp->mhp = mtmp->mhpmax;
		}
	    mtmp = mtmp->nmon;
	}
}


STATIC_OVL void
encourage_pets(distance)
int distance;
{
	register struct monst *mtmp = fmon;

	while(mtmp) {
		if (!DEADMONSTER(mtmp) && mtmp->mtame && distu(mtmp->mx, mtmp->my) < distance &&
		    resist_song(mtmp, SNG_COURAGE, song_instr) >= 0) {
			if (EDOG(mtmp)->encouraged < EDOG_ENCOURAGED_MAX)
				EDOG(mtmp)->encouraged += (P_SKILL(P_MUSICALIZE)-P_UNSKILLED+1) * 6;
			if (mtmp->mflee)
				switch (P_SKILL(P_MUSICALIZE)) {
				case P_UNSKILLED:
				case P_BASIC:
					mtmp->mfleetim /= 2;
					break;
				case P_SKILLED:
					mtmp->mfleetim /= 4;
					break;
				case P_EXPERT:
					mtmp->mfleetim = 0;
					break;
				}
			if (canseemon(mtmp))
				if (Hallucination)
					pline("%s looks %s!", Monnam(mtmp),
					      EDOG(mtmp)->encouraged == EDOG_ENCOURAGED_MAX ? "way cool" :
					      EDOG(mtmp)->encouraged > (EDOG_ENCOURAGED_MAX/2) ? "cooler" : "cool");
				else
					pline("%s looks %s!", Monnam(mtmp),
					      EDOG(mtmp)->encouraged == EDOG_ENCOURAGED_MAX ? "berserk" :
					      EDOG(mtmp)->encouraged > (EDOG_ENCOURAGED_MAX/2) ? "wilder" : "wild");
		}
		mtmp = mtmp->nmon;
	}
}

STATIC_OVL void
confusion_song(distance)
int distance;
{
	register struct monst *mtmp = fmon;

	while(mtmp) {
		if (!DEADMONSTER(mtmp) && !mtmp->mtame && !mtmp->mconf &&
		    distu(mtmp->mx, mtmp->my) < distance &&
		    resist_song(mtmp, SNG_CONFUSION, song_instr) >= 0) {
			if (canseemon(mtmp))
				pline("%s seems confused.", Monnam(mtmp));
			mtmp->mconf = 1;
		}
		mtmp = mtmp->nmon;
	}
}

STATIC_OVL void
cancel_song(distance)
int distance;
{
	register struct monst *mtmp = fmon;

	while(mtmp) {
		if (!DEADMONSTER(mtmp) && !mtmp->mtame && 
		    distu(mtmp->mx, mtmp->my) < distance &&
		    resist_song(mtmp, SNG_CNCL, song_instr) >= 0) {
			if (canseemon(mtmp))
				pline("%s seems unable to focus.", Monnam(mtmp));
			mtmp->mspec_used += min(max(1, P_SKILL(P_MUSICALIZE)-P_UNSKILLED), 255);
			if(wizard) pline("mspec cooldown: %d", (int)mtmp->mspec_used);
		}
		mtmp = mtmp->nmon;
	}
}

STATIC_OVL void
rally_song(distance)
int distance;
{
	register struct monst *mtmp = fmon, *nextmon;

	while(mtmp) {
		nextmon = mtmp->nmon;
		if (!DEADMONSTER(mtmp) && mtmp->mtame && 
		    distu(mtmp->mx, mtmp->my) < distance &&
		    resist_song(mtmp, SNG_RLLY, song_instr) >= 0) {
			if (mtmp->mtrapped) {
			    /* no longer in previous trap (affects mintrap) */
			    mtmp->mtrapped = 0;
			    fill_pit(mtmp->mx, mtmp->my);
			}
			switch (P_SKILL(P_MUSICALIZE)) {
			case P_EXPERT:
				mtmp->mcan = 0;
				mtmp->mspec_used = 0;
			case P_SKILLED:
				if(!mtmp->mcansee && mtmp->mblinded){
					mtmp->mcansee = 1;
					mtmp->mblinded = 0;
				}
			case P_BASIC:
				if(!mtmp->mcanmove && mtmp->mfrozen){
					mtmp->mcanmove = 1;
					mtmp->mfrozen = 0;
				}
				mtmp->mstun = 0;
				mtmp->mconf = 0;
			case P_UNSKILLED:
				mtmp->msleeping = 0;
				mtmp->mflee = 0;
				mtmp->mfleetim = 0;
			}
			mnexto(mtmp);
			if (mintrap(mtmp) == 2) change_luck(-1);
		}
		mtmp = nextmon;
	}
}


/*
 * Wake every monster in range...
 */

STATIC_OVL void
awaken_monsters(distance)
int distance;
{
	register struct monst *mtmp = fmon;
	register int distm;

	while(mtmp) {
	    if (!DEADMONSTER(mtmp)) {
		distm = distu(mtmp->mx, mtmp->my);
		if (distm < distance) {
		    mtmp->msleeping = 0;
		    mtmp->mcanmove = 1;
		    mtmp->mfrozen = 0;
		    /* May scare some monsters */
		    if (distm < distance/3 &&
			    !resist(mtmp, TOOL_CLASS, 0, NOTELL))
			monflee(mtmp, 0, FALSE, TRUE);
		}
	    }
	    mtmp = mtmp->nmon;
	}
}

/*
 * Make monsters fall asleep.  Note that they may resist the spell.
 */

STATIC_OVL void
sleep_song(distance)
int distance;
{
	register struct monst *mtmp = fmon;
// to do: peaceful music can aggravate demons

	while(mtmp) {
		if (!DEADMONSTER(mtmp) && distu(mtmp->mx, mtmp->my) < distance &&
			mon_affected_by_peace_song(mtmp) &&
			!resists_sleep(mtmp) &&
		    resist_song(mtmp, SNG_SLEEP, song_instr) >= 0) {
			/* pets, if affected, sleep less time */
			mtmp->mfrozen = min( mtmp->mfrozen +
					     max(1, P_SKILL(P_MUSICALIZE)-P_UNSKILLED)
					     * (mtmp->mtame ? 2 : 3), 127);
			if (wizard)
				pline("[%s:%i turns]", mon_nam(mtmp), mtmp->mfrozen);
			if (!mtmp->mcanmove) {
				if (canseemon(mtmp) && flags.verbose && !rn2(10))
					pline("%s moves while sleeping.", Monnam(mtmp));
			} else {
				mtmp->mcanmove = 0;
				if (canseemon(mtmp) && flags.verbose)
					pline("%s sleeps.", Monnam(mtmp));
			}
			slept_monst(mtmp);
		}
		mtmp = mtmp->nmon;
	}
}


STATIC_OVL void
put_monsters_to_sleep(distance)
int distance;
{
	register struct monst *mtmp = fmon;

	while(mtmp) {
		if (!DEADMONSTER(mtmp) && distu(mtmp->mx, mtmp->my) < distance &&
			sleep_monst(mtmp, d(10,10), TOOL_CLASS)) {
		    mtmp->msleeping = 1; /* 10d10 turns + wake_nearby to rouse */
		    slept_monst(mtmp);
		}
		mtmp = mtmp->nmon;
	}
}

/*
 * Charm snakes in range.  Note that the snakes are NOT tamed.
 */

STATIC_OVL void
tame_song(distance)
int distance;
{
	struct monst *mtmp=0, *mtmp2;
	xchar tame, waspeaceful;

	if (u.uswallow) {
		if (resist_song(u.ustuck, SNG_TAME, song_instr) >= 0) {
			mtmp = tamedog(u.ustuck, (struct obj *) 0);
			if(mtmp){
				EDOG(mtmp)->friend = 1;
				mtmp->mtame = min(max(1, P_SKILL(P_MUSICALIZE)-P_UNSKILLED)*2, 255);
			}
		}
	} else {
		for (mtmp = fmon; mtmp; mtmp = mtmp2) {
			mtmp2 = mtmp->nmon;
			if (!DEADMONSTER(mtmp) && distu(mtmp->mx, mtmp->my) <= distance &&
			    resist_song(mtmp, SNG_TAME, song_instr) >= 0) {
				mtmp->mflee = 0;
				/* no other effect if monster was already tame by other means */
				if (mtmp->mtame && !(EDOG(mtmp)->friend))
					continue;
				tame = mtmp->mtame;
				waspeaceful = mtmp->mpeaceful;
				if (!tame) {
					mtmp = tamedog(mtmp, (struct obj *) 0);
					if (mtmp){
						EDOG(mtmp)->waspeaceful = waspeaceful;
						if (canseemon(mtmp) && flags.verbose && !mtmp->msleeping)
							pline("%s seems to like your song.", Monnam(mtmp));
						EDOG(mtmp)->friend = 1;
					}
				} else {
					EDOG(mtmp)->friend = 1;
				}
				if(mtmp){
					/* tameness of song is temporary. uses tameness
					 as timeout counter */
					mtmp->mtame = min(mtmp->mtame
							   + max(1, P_SKILL(P_MUSICALIZE)-P_UNSKILLED)*2,
							   255);
				}
			}
		}
	}
}


STATIC_OVL void
charm_snakes(distance)
int distance;
{
	register struct monst *mtmp = fmon;
	int could_see_mon, was_peaceful;

	while (mtmp) {
	    if (!DEADMONSTER(mtmp) && mtmp->data->mlet == S_SNAKE && mtmp->mcanmove &&
		    distu(mtmp->mx, mtmp->my) < distance) {
		was_peaceful = mtmp->mpeaceful;
		mtmp->mpeaceful = 1;
		mtmp->mavenge = 0;
		could_see_mon = canseemon(mtmp);
		mtmp->mundetected = 0;
		newsym(mtmp->mx, mtmp->my);
		if (canseemon(mtmp)) {
		    if (!could_see_mon)
			You("notice %s, swaying with the music.",
			    a_monnam(mtmp));
		    else
			pline("%s freezes, then sways with the music%s.",
			      Monnam(mtmp),
			      was_peaceful ? "" : ", and now seems quieter");
		}
	    }
	    mtmp = mtmp->nmon;
	}
}

/*
 * Calm nymphs in range.
 */

STATIC_OVL void
calm_nymphs(distance)
int distance;
{
	register struct monst *mtmp = fmon;

	while (mtmp) {
	    if (!DEADMONSTER(mtmp) && mtmp->data->mlet == S_NYMPH && mtmp->mcanmove &&
		    distu(mtmp->mx, mtmp->my) < distance) {
		mtmp->msleeping = 0;
		mtmp->mpeaceful = 1;
		mtmp->mavenge = 0;
		if (canseemon(mtmp))
		    pline(
		     "%s listens cheerfully to the music, then seems quieter.",
			  Monnam(mtmp));
	    }
	    mtmp = mtmp->nmon;
	}
}

/* Awake only soldiers of the level. */

void
awaken_soldiers()
{
	register struct monst *mtmp = fmon;

	while(mtmp) {
	    if (!DEADMONSTER(mtmp) &&
			is_mercenary(mtmp->data) && mtmp->data != &mons[PM_GUARD]) {
		mtmp->mpeaceful = mtmp->msleeping = mtmp->mfrozen = 0;
		mtmp->mcanmove = 1;
		if (canseemon(mtmp))
		    pline("%s is now ready for battle!", Monnam(mtmp));
		else
		    Norep("You hear the rattle of battle gear being readied.");
	    }
	    mtmp = mtmp->nmon;
	}
}

/* Charm monsters in range.  Note that they may resist the spell.
 * If swallowed, range is reduced to 0.
 */

STATIC_OVL void
charm_monsters(distance)
int distance;
{
	struct monst *mtmp, *mtmp2;

	if (u.uswallow) {
	    if (!resist(u.ustuck, TOOL_CLASS, 0, NOTELL))
		(void) tamedog(u.ustuck, (struct obj *) 0);
	} else {
	    for (mtmp = fmon; mtmp; mtmp = mtmp2) {
		mtmp2 = mtmp->nmon;
		if (DEADMONSTER(mtmp)) continue;

		if (distu(mtmp->mx, mtmp->my) <= distance) {
		    if (!resist(mtmp, TOOL_CLASS, 0, NOTELL))
			(void) tamedog(mtmp, (struct obj *) 0);
		}
	    }
	}

}

/* Generate earthquake :-) of desired force.
 * That is:  create random chasms (pits).
 */

STATIC_OVL void
do_earthquake(force)
int force;
{
	register int x,y;
	struct monst *mtmp;
	struct obj *otmp;
	struct trap *chasm;
	int start_x, start_y, end_x, end_y;

	start_x = u.ux - (force * 2);
	start_y = u.uy - (force * 2);
	end_x = u.ux + (force * 2);
	end_y = u.uy + (force * 2);
	if (start_x < 1) start_x = 1;
	if (start_y < 1) start_y = 1;
	if (end_x >= COLNO) end_x = COLNO - 1;
	if (end_y >= ROWNO) end_y = ROWNO - 1;
	for (x=start_x; x<=end_x; x++) for (y=start_y; y<=end_y; y++) {
	    if ((mtmp = m_at(x,y)) != 0) {
		wakeup(mtmp);	/* peaceful monster will become hostile */
		if (mtmp->mundetected && is_hider(mtmp->data)) {
		    mtmp->mundetected = 0;
		    if (cansee(x,y))
			pline("%s is shaken loose from the ceiling!",
							    Amonnam(mtmp));
		    else
			You_hear("a thumping sound.");
		    if (x==u.ux && y==u.uy)
			You("easily dodge the falling %s.",
							    mon_nam(mtmp));
		    newsym(x,y);
		}
	    }
	    if (!rn2(14 - force)) switch (levl[x][y].typ) {
		  case FOUNTAIN : /* Make the fountain disappear */
			if (cansee(x,y))
				pline_The("fountain falls into a chasm.");
			goto do_pit;
#ifdef SINKS
		  case SINK :
			if (cansee(x,y))
				pline_The("kitchen sink falls into a chasm.");
			goto do_pit;
		  case TOILET :
			if (cansee(x,y))
				pline("The toilet falls into a chasm.");
			goto do_pit;
#endif
		  case ALTAR :
			if (Is_astralevel(&u.uz) || Is_sanctum(&u.uz)) break;

			if (cansee(x,y))
				pline_The("altar falls into a chasm.");
			goto do_pit;
		  case GRAVE :
			if (cansee(x,y))
				pline_The("headstone topples into a chasm.");
			goto do_pit;
		  case THRONE :
			if (cansee(x,y))
				pline_The("throne falls into a chasm.");
			/* Falls into next case */
		  case ROOM :
		  case CORR : /* Try to make a pit */
do_pit:		    chasm = maketrap(x,y,PIT);
		    if (!chasm) break;	/* no pit if portal at that location */
		    chasm->tseen = 1;

		    levl[x][y].doormask = 0;

		    mtmp = m_at(x,y);

		    if ((otmp = sobj_at(BOULDER, x, y)) != 0) {
			if (cansee(x, y))
			   pline("KADOOM! The boulder falls into a chasm%s!",
			      ((x == u.ux) && (y == u.uy)) ? " below you" : "");
			if (mtmp)
				mtmp->mtrapped = 0;
			obj_extract_self(otmp);
			(void) flooreffects(otmp, x, y, "");
			break;
		    }

		    /* We have to check whether monsters or player
		       falls in a chasm... */

		    if (mtmp) {
			if(!is_flyer(mtmp->data) && !is_clinger(mtmp->data)) {
			    mtmp->mtrapped = 1;
			    if(cansee(x,y))
				pline("%s falls into a chasm!", Monnam(mtmp));
			    else if (flags.soundok && humanoid(mtmp->data))
				You_hear("a scream!");
			    mselftouch(mtmp, "Falling, ", TRUE);
			    if (mtmp->mhp > 0)
				if ((mtmp->mhp -= rnd(6)) <= 0) {
				    if(!cansee(x,y))
					pline("It is destroyed!");
				    else {
					You("destroy %s!", mtmp->mtame ?
					    x_monnam(mtmp, ARTICLE_THE, "poor",
				mtmp->mnamelth ? SUPPRESS_SADDLE : 0, FALSE):
					    mon_nam(mtmp));
				    }
				    xkilled(mtmp,0);
				}
			}
		    } else if (x == u.ux && y == u.uy) {
				/* KMH, balance patch -- new intrinsic */
			    if (Levitation || Flying ||
						is_clinger(youmonst.data)) {
				    pline("A chasm opens up under you!");
				    You("don't fall in!");
			    } else {
				    You("fall into a chasm!");
				    u.utrap = rn1(6,2);
				    u.utraptype = TT_PIT;
				    losehp(rnd(6),"fell into a chasm",
					NO_KILLER_PREFIX);
				    selftouch("Falling, you");
			    }
		    } else newsym(x,y);
		    break;
		  case DOOR : /* Make the door collapse */
		    /* ALI - artifact doors */
		    if (artifact_door(x, y))  break;
		    if (levl[x][y].doormask == D_NODOOR) goto do_pit;
		    if (cansee(x,y))
			pline_The("door collapses.");
		    if (*in_rooms(x, y, SHOPBASE))
			add_damage(x, y, 0L);
		    levl[x][y].doormask = D_NODOOR;
		    unblock_point(x,y);
		    newsym(x,y);
		    break;
	    }
	}
}

/*
 * The player is trying to extract something from his/her instrument.
 */

STATIC_OVL int
do_improvisation(instr)
struct obj *instr;
{
	int damage, do_spec = !Confusion;
#if defined(MAC) || defined(AMIGA) || defined(VPIX_MUSIC) || defined (PCMUSIC)
	struct obj itmp;

	itmp = *instr;
	/* if won't yield special effect, make sound of mundane counterpart */
	if (!do_spec || instr->spe <= 0)
	    while (objects[itmp.otyp].oc_magic) itmp.otyp -= 1;
# ifdef MAC
	mac_speaker(&itmp, "C");
# endif
# ifdef AMIGA
	amii_speaker(&itmp, "Cw", AMII_OKAY_VOLUME);
# endif
# ifdef VPIX_MUSIC
	if (sco_flag_console)
	    speaker(&itmp, "C");
# endif
#ifdef PCMUSIC
	  pc_speaker ( &itmp, "C");
#endif
#endif /* MAC || AMIGA || VPIX_MUSIC || PCMUSIC */

	if (!do_spec)
	    pline("What you produce is quite far from music...");
	else
	    You("start playing %s.", the(xname(instr)));

	switch (instr->otyp) {
	case MAGIC_FLUTE:		/* Make monster fall asleep */
	    if (do_spec && instr->spe > 0) {
		consume_obj_charge(instr, TRUE);

		You("produce soft music.");
		put_monsters_to_sleep(u.ulevel * 5);
		exercise(A_DEX, TRUE);
		break;
	    } /* else FALLTHRU */
	case WOODEN_FLUTE:		/* May charm snakes */
	/* KMH, balance patch -- removed
	case PAN_PIPE: */
	    do_spec &= (rn2(ACURR(A_DEX)) + u.ulevel > 25);
	    pline("%s.", Tobjnam(instr, do_spec ? "trill" : "toot"));
	    if (do_spec) charm_snakes(u.ulevel * 3);
	    exercise(A_DEX, TRUE);
	    break;
	case FROST_HORN:		/* Idem wand of cold */
	case FIRE_HORN:			/* Idem wand of fire */
	    if (do_spec && instr->spe > 0) {
		consume_obj_charge(instr, TRUE);

		if (!getdir((char *)0)) {
		    pline("%s.", Tobjnam(instr, "vibrate"));
		    break;
		} else if (!u.dx && !u.dy && !u.dz) {
		    if ((damage = zapyourself(instr, TRUE)) != 0) {
			char buf[BUFSZ];
			Sprintf(buf, "using a magical horn on %sself", uhim());
			losehp(damage, buf, KILLED_BY);
		    }
		} else {
		    buzz((instr->otyp == FROST_HORN) ? AD_COLD-1 : AD_FIRE-1,
			 rn1(6,6), u.ux, u.uy, u.dx, u.dy);
		}
		makeknown(instr->otyp);
		break;
	    } /* else FALLTHRU */
	case TOOLED_HORN:		/* Awaken or scare monsters */
	    You("produce a frightful, grave sound.");
	    awaken_monsters(u.ulevel * 30);
	    exercise(A_WIS, FALSE);
	    break;
	case BUGLE:			/* Awaken & attract soldiers */
	    You("extract a loud noise from %s.", the(xname(instr)));
	    awaken_soldiers();
	    exercise(A_WIS, FALSE);
	    break;
	case MAGIC_HARP:		/* Charm monsters */
	    if (do_spec && instr->spe > 0) {
		consume_obj_charge(instr, TRUE);

		pline("%s very attractive music.", Tobjnam(instr, "produce"));
		charm_monsters((u.ulevel - 1) / 3 + 1);
		exercise(A_DEX, TRUE);
		break;
	    } /* else FALLTHRU */
	case WOODEN_HARP:		/* May calm Nymph */
	    do_spec &= (rn2(ACURR(A_DEX)) + u.ulevel > 25);
	    pline("%s %s.", The(xname(instr)),
		  do_spec ? "produces a lilting melody" : "twangs");
	    if (do_spec) calm_nymphs(u.ulevel * 3);
	    exercise(A_DEX, TRUE);
	    break;
	case DRUM_OF_EARTHQUAKE:	/* create several pits */
	    if (do_spec && instr->spe > 0) {
		consume_obj_charge(instr, TRUE);

		You("produce a heavy, thunderous rolling!");
		pline_The("entire dungeon is shaking around you!");
		do_earthquake((u.ulevel - 1) / 3 + 1);
		/* shake up monsters in a much larger radius... */
		awaken_monsters(ROWNO * COLNO);
		makeknown(DRUM_OF_EARTHQUAKE);
		break;
	    } /* else FALLTHRU */
	/* KMH, balance patch -- removed (in the wrong place anyways) */
#if 0
	case PAN_PIPE_OF_SUMMONING: /* yikes! */
	    if (instr->spe > 0) {
		register int cnt = 1;
		instr->spe--;
		cnt += rn2(4) + 3;
		while(cnt--)
		(void) makemon((struct permonst *) 0, u.ux, u.uy, NO_MM_FLAGS);
	    }
		break;
	case PAN_PIPE_OF_THE_SEWERS:
	    You("call out the rats!");
	    if (instr->spe > 0) {
		register int cnt = 1;
		register struct monst *mtmp;
		instr->spe--;
		cnt += rn2(4) + 3;
		while(cnt--) {
		mtmp = makemon(&mons[PM_SEWER_RAT], u.ux, u.uy, NO_MM_FLAGS);
		(void) tamedog(mtmp, (struct obj *) 0);
		}
	     }
		break;
#endif
	case LEATHER_DRUM:		/* Awaken monsters */
	    You("beat a deafening row!");
	    awaken_monsters(u.ulevel * 40);
	    exercise(A_WIS, FALSE);
	    break;
	default:
	    impossible("What a weird instrument (%d)!", instr->otyp);
	    break;
	}
	return 2;		/* That takes time */
}

/*
 * So you want music...
 */

int
do_play_instrument(instr)
struct obj *instr;
{
    char buf[BUFSZ];
    char *s;
    int x,y;
    int a;
    unsigned char song = SNG_NONE;
    boolean ok;

    if (Underwater) {
	You_cant("play music underwater!");
	return(0);
    }
    if (nohands(youmonst.data)) {
	You("have no hands!");
	return 0;
    }
    /*
    if (uarms) {
	You_cant("play music while wearing a shield!");
      	return(0);
    }
    */
    if (welded(uwep)) {
	You("need free hands to play music!");
	return(0);
    }
    /* also cursed gauntlets should mean your song will go bad */

    /* Another possibility would be playing only scary music
       while being thus affected. */
    if (Confusion > 0 || Stunned || Hallucination) {
		You_cant("play music while %s!", Confusion > 0 ? "confused" : 
			 (Stunned ? "stunned" : "stoned"));
		return 0;
    }

    if (uarms && (instr->otyp == WOODEN_HARP || instr->otyp == LEATHER_DRUM))
	    You("can't play properly while wearing a shield.");
    if (is_silent(youmonst.data))
	    pline("While in this form, you can't sing along your songs.");
    
    if (!P_RESTRICTED(P_MUSICALIZE)) {
	song = songs_menu(instr);
	if (song == SNG_NONE)
	    return 0;
    } else {

    if (instr->otyp != LEATHER_DRUM && instr->otyp != DRUM_OF_EARTHQUAKE) {
	    if (yn("Improvise?") == 'y') song = SNG_IMPROVISE;
	    else if (u.uevent.uheard_tune == 2 && yn("Play the passtune?") == 'y')
		song = SNG_PASSTUNE;
	    else
		song = SNG_NOTES;
    }
    }
    switch (song) {
    case SNG_NONE:
	return 0;
	break;
    case SNG_IMPROVISE:
	return do_improvisation(instr);
	break;
    case SNG_NOTES:
	getlin("What tune are you playing? [what 5 notes]", buf);
	break;
    case SNG_PASSTUNE:
	    Strcpy(buf, tune);
	break;
    default:
/*
	a = songs[song].level * (Role_if(PM_BARD) ? 2 : 5);
	if (a > u.uen) {
	    You("don't have enough energy to play that song.");
	    return 0;
	}
	u.uen -= a;
	flags.botl = 1;
*/
	    if (rnd(100) > song_success(song, instr, 1)) {
		    pline("What you produce is quite far from music...");
		    return 1;
	    }
	
	    song_played = song;
	    song_instr = instr;
	    song_delay = songs[song_played].turns;

	song_penalty = (songs[song_played].instr1 == instr->otyp);
	if (song_played == SNG_TAME && instr->oartifact == ART_LYRE_OF_ORPHEUS)
		song_penalty = 0;

	Sprintf(msgbuf, "playing %s", the(xname(instr)));
	if (instr->oartifact == ART_LYRE_OF_ORPHEUS)
		pline("%s the \"%s\" song!", Tobjnam(instr, "sing"), songs[song].name);
	else
		You("play the \"%s\" song...", songs[song].name);
	set_occupation(play_song, msgbuf, 0);
	return 0;
    }
	    (void)mungspaces(buf);
	    /* convert to uppercase and change any "H" to the expected "B" */
	    for (s = buf; *s; s++) {
#ifndef AMIGA
		*s = highc(*s);
#else
		/* The AMIGA supports two octaves of notes */
		if (*s == 'h') *s = 'b';
#endif
		if (*s == 'H') *s = 'B';
	    }
//	}
	You("extract a strange sound from %s!", the(xname(instr)));
#ifdef UNIX386MUSIC
	/* if user is at the console, play through the console speaker */
	if (atconsole())
	    speaker(instr, buf);
#endif
#ifdef VPIX_MUSIC
	if (sco_flag_console)
	    speaker(instr, buf);
#endif
#ifdef MAC
	mac_speaker ( instr , buf ) ;
#endif
#ifdef PCMUSIC
	pc_speaker ( instr, buf );
#endif
#ifdef AMIGA
	{
		char nbuf[ 20 ];
		int i;
		for( i = 0; buf[i] && i < 5; ++i )
		{
			nbuf[ i*2 ] = buf[ i ];
			nbuf[ (i*2)+1 ] = 'h';
		}
		nbuf[ i*2 ] = 0;
		amii_speaker ( instr , nbuf, AMII_OKAY_VOLUME ) ;
	}
#endif
	/* Check if there was the Stronghold drawbridge near
	 * and if the tune conforms to what we're waiting for.
	 */
	if(Is_stronghold(&u.uz)) {
	    exercise(A_WIS, TRUE);		/* just for trying */
	    if(!strcmp(buf,tune)) {
		/* Search for the drawbridge */
		for(y=u.uy-1; y<=u.uy+1; y++)
		    for(x=u.ux-1;x<=u.ux+1;x++)
			if(isok(x,y))
			if(find_drawbridge(&x,&y)) {
			    u.uevent.uheard_tune = 2; /* tune now fully known */
			    if(levl[x][y].typ == DRAWBRIDGE_DOWN)
				close_drawbridge(x,y);
			    else
				open_drawbridge(x,y);
			    return /*0*/1; /*annoying bug fixed now --Amy*/
			}
	    } else if(flags.soundok) {
		if (u.uevent.uheard_tune < 1) u.uevent.uheard_tune = 1;
		/* Okay, it wasn't the right tune, but perhaps
		 * we can give the player some hints like in the
		 * Mastermind game */
		ok = FALSE;
		for(y = u.uy-1; y <= u.uy+1 && !ok; y++)
		    for(x = u.ux-1; x <= u.ux+1 && !ok; x++)
			if(isok(x,y))
			if(IS_DRAWBRIDGE(levl[x][y].typ) ||
			   is_drawbridge_wall(x,y) >= 0)
				ok = TRUE;
		if(ok) { /* There is a drawbridge near */
		    int tumblers, gears;
		    boolean matched[5];

		    tumblers = gears = 0;
		    for(x=0; x < 5; x++)
			matched[x] = FALSE;

		    for(x=0; x < (int)strlen(buf); x++)
			if(x < 5) {
			    if(buf[x] == tune[x]) {
				gears++;
				matched[x] = TRUE;
			    } else
				for(y=0; y < 5; y++)
				    if(!matched[y] &&
				       buf[x] == tune[y] &&
				       buf[y] != tune[y]) {
					tumblers++;
					matched[y] = TRUE;
					break;
				    }
			}
			 if(tumblers) {
			if(gears)
			    You_hear("%d tumbler%s click and %d gear%s turn.",
				tumblers, plur(tumblers), gears, plur(gears));
			else
			    You_hear("%d tumbler%s click.",
				tumblers, plur(tumblers));
			 } else if(gears) {
			You_hear("%d gear%s turn.", gears, plur(gears));
			/* could only get `gears == 5' by playing five
			   correct notes followed by excess; otherwise,
			   tune would have matched above */
			if (gears == 5) u.uevent.uheard_tune = 2;
		    }
		}
	    }
	  }
	return 1;
//    } else
//	    return do_improvisation(instr);
}

#ifdef UNIX386MUSIC
/*
 * Play audible music on the machine's speaker if appropriate.
 */

STATIC_OVL int
atconsole()
{
    /*
     * Kluge alert: This code assumes that your [34]86 has no X terminals
     * attached and that the console tty type is AT386 (this is always true
     * under AT&T UNIX for these boxen). The theory here is that your remote
     * ttys will have terminal type `ansi' or something else other than
     * `AT386' or `xterm'. We'd like to do better than this, but testing
     * to see if we're running on the console physical terminal is quite
     * difficult given the presence of virtual consoles and other modern
     * UNIX impedimenta...
     */
    char	*termtype = nh_getenv("TERM");

     return(!strcmp(termtype, "AT386") || !strcmp(termtype, "xterm"));
}

STATIC_OVL void
speaker(instr, buf)
struct obj *instr;
char	*buf;
{
    /*
     * For this to work, you need to have installed the PD speaker-control
     * driver for PC-compatible UNIX boxes that I (esr@snark.thyrsus.com)
     * posted to comp.sources.unix in Feb 1990.  A copy should be included
     * with your nethack distribution.
     */
    int	fd;

    if ((fd = open("/dev/speaker", 1)) != -1)
    {
	/* send a prefix to modify instrumental `timbre' */
	switch (instr->otyp)
	{
	case WOODEN_FLUTE:
	case MAGIC_FLUTE:
	    (void) write(fd, ">ol", 1); /* up one octave & lock */
	    break;
	case TOOLED_HORN:
	case FROST_HORN:
	case FIRE_HORN:
	    (void) write(fd, "<<ol", 2); /* drop two octaves & lock */
	    break;
	case BUGLE:
	    (void) write(fd, "ol", 2); /* octave lock */
	    break;
	case WOODEN_HARP:
	case MAGIC_HARP:
	    (void) write(fd, "l8mlol", 4); /* fast, legato, octave lock */
	    break;
	}
	(void) write(fd, buf, strlen(buf));
	(void) close(fd);
    }
}
#endif /* UNIX386MUSIC */

#ifdef VPIX_MUSIC

# if 0
#include <sys/types.h>
#include <sys/console.h>
#include <sys/vtkd.h>
# else
#define KIOC ('K' << 8)
#define KDMKTONE (KIOC | 8)
# endif

#define noDEBUG

STATIC_OVL void tone(hz, ticks)
/* emit tone of frequency hz for given number of ticks */
unsigned int hz, ticks;
{
    ioctl(0,KDMKTONE,hz|((ticks*10)<<16));
# ifdef DEBUG
    printf("TONE: %6d %6d\n",hz,ticks * 10);
# endif
    nap(ticks * 10);
}

STATIC_OVL void rest(ticks)
/* rest for given number of ticks */
int	ticks;
{
    nap(ticks * 10);
# ifdef DEBUG
    printf("REST:        %6d\n",ticks * 10);
# endif
}


#include "interp.c"	/* from snd86unx.shr */


STATIC_OVL void
speaker(instr, buf)
struct obj *instr;
char	*buf;
{
    /* emit a prefix to modify instrumental `timbre' */
    playinit();
    switch (instr->otyp)
    {
	case WOODEN_FLUTE:
	case MAGIC_FLUTE:
	    playstring(">ol", 1); /* up one octave & lock */
	    break;
	case TOOLED_HORN:
	case FROST_HORN:
	case FIRE_HORN:
	    playstring("<<ol", 2); /* drop two octaves & lock */
	    break;
	case BUGLE:
	    playstring("ol", 2); /* octave lock */
	    break;
	case WOODEN_HARP:
	case MAGIC_HARP:
	    playstring("l8mlol", 4); /* fast, legato, octave lock */
	    break;
    }
    playstring( buf, strlen(buf));
}

# ifdef DEBUG
main(argc,argv)
char *argv[];
{
    if (argc == 2) {
	playinit();
	playstring(argv[1], strlen(argv[1]));
    }
}
# endif
#endif	/* VPIX_MUSIC */

/*music.c*/
