/*	SCCS Id: @(#)do_name.c	3.4	2003/01/14	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

#include "hack.h"

#ifdef OVLB

STATIC_DCL void FDECL(do_oname, (struct obj *));
static void FDECL(getpos_help, (BOOLEAN_P,const char *));

extern const char what_is_an_unknown_object[];		/* from pager.c */

/* the response for '?' help request in getpos() */
static void
getpos_help(force, goal)
boolean force;
const char *goal;
{
    char sbuf[BUFSZ];
    boolean doing_what_is;
    winid tmpwin = create_nhwindow(NHW_MENU);

    Sprintf(sbuf, "Use [%s] to move the cursor to %s.",
	    iflags.num_pad ? "2468" : "hjkl", goal);
    putstr(tmpwin, 0, sbuf);
    putstr(tmpwin, 0, "Use [HJKL] to move the cursor 8 units at a time.");
    putstr(tmpwin, 0, "Or enter a background symbol (ex. <).");
    /* disgusting hack; the alternate selection characters work for any
       getpos call, but they only matter for dowhatis (and doquickwhatis) */
    doing_what_is = (goal == what_is_an_unknown_object);
    Sprintf(sbuf, "Type a .%s when you are at the right place.",
            doing_what_is ? " or , or ; or :" : "");
    putstr(tmpwin, 0, sbuf);
    if (!force)
	putstr(tmpwin, 0, "Type Space or Escape when you're done.");
    putstr(tmpwin, 0, "");
    display_nhwindow(tmpwin, TRUE);
    destroy_nhwindow(tmpwin);
}

int
getpos(cc,force,goal)
coord *cc;
boolean force;
const char *goal;
{
    int result = 0;
    int cx, cy, i, c;
    int sidx, tx, ty;
    boolean msg_given = TRUE;	/* clear message window by default */
    static const char pick_chars[] = ".,;:";
    const char *cp;
    const char *sdp;
    if(iflags.num_pad) sdp = ndir; else sdp = sdir;	/* DICE workaround */

    if (flags.verbose) {
	pline("(For instructions type a ?)");
	msg_given = TRUE;
    }
    cx = cc->x;
    cy = cc->y;
#ifdef CLIPPING
    cliparound(cx, cy);
#endif
    curs(WIN_MAP, cx,cy);
    flush_screen(0);
#ifdef MAC
    lock_mouse_cursor(TRUE);
#endif
    for (;;) {
	c = nh_poskey(&tx, &ty, &sidx);
	if (c == '\033') {
	    cx = cy = -10;
	    msg_given = TRUE;	/* force clear */
	    result = -1;
	    break;
	}
	if(c == 0) {
	    if (!isok(tx, ty)) continue;
	    /* a mouse click event, just assign and return */
	    cx = tx;
	    cy = ty;
	    break;
	}
	if ((cp = index(pick_chars, c)) != 0) {
	    /* '.' => 0, ',' => 1, ';' => 2, ':' => 3 */
	    result = cp - pick_chars;
	    break;
	}
	for (i = 0; i < 8; i++) {
	    int dx, dy;

	    if (sdp[i] == c) {
		/* a normal movement letter or digit */
		dx = xdir[i];
		dy = ydir[i];
	    } else if (sdir[i] == lowc((char)c)) {
		/* a shifted movement letter */
		dx = 8 * xdir[i];
		dy = 8 * ydir[i];
	    } else
		continue;

	    /* truncate at map edge; diagonal moves complicate this... */
	    if (cx + dx < 1) {
		dy -= sgn(dy) * (1 - (cx + dx));
		dx = 1 - cx;		/* so that (cx+dx == 1) */
	    } else if (cx + dx > COLNO-1) {
		dy += sgn(dy) * ((COLNO-1) - (cx + dx));
		dx = (COLNO-1) - cx;
	    }
	    if (cy + dy < 0) {
		dx -= sgn(dx) * (0 - (cy + dy));
		dy = 0 - cy;		/* so that (cy+dy == 0) */
	    } else if (cy + dy > ROWNO-1) {
		dx += sgn(dx) * ((ROWNO-1) - (cy + dy));
		dy = (ROWNO-1) - cy;
	    }
	    cx += dx;
	    cy += dy;
	    goto nxtc;
	}

	if(c == '?'){
	    getpos_help(force, goal);
	} else {
	    if (!index(quitchars, c)) {
		char matching[MAXPCHARS];
		int pass, lo_x, lo_y, hi_x, hi_y, k = 0;
		(void)memset((genericptr_t)matching, 0, sizeof matching);
		for (sidx = 1; sidx < MAXPCHARS; sidx++)
		    if (c == defsyms[sidx].sym || c == (int)showsyms[sidx])
			matching[sidx] = (char) ++k;
		if (k) {
		    for (pass = 0; pass <= 1; pass++) {
			/* pass 0: just past current pos to lower right;
			   pass 1: upper left corner to current pos */
			lo_y = (pass == 0) ? cy : 0;
			hi_y = (pass == 0) ? ROWNO - 1 : cy;
			for (ty = lo_y; ty <= hi_y; ty++) {
			    lo_x = (pass == 0 && ty == lo_y) ? cx + 1 : 1;
			    hi_x = (pass == 1 && ty == hi_y) ? cx : COLNO - 1;
			    for (tx = lo_x; tx <= hi_x; tx++) {
				k = glyph_at(tx, ty);
				if (glyph_is_cmap(k) &&
					matching[glyph_to_cmap(k)]) {
				    cx = tx,  cy = ty;
				    if (msg_given) {
					clear_nhwindow(WIN_MESSAGE);
					msg_given = FALSE;
				    }
				    goto nxtc;
				}
			    }	/* column */
			}	/* row */
		    }		/* pass */
		    pline("Can't find dungeon feature '%c'.", c);
		    msg_given = TRUE;
		    goto nxtc;
		} else {
		    pline("Unknown direction: '%s' (%s).",
			  visctrl((char)c),
			  !force ? "aborted" :
			  iflags.num_pad ? "use 2468 or ." : "use hjkl or .");
		    msg_given = TRUE;
		} /* k => matching */
	    } /* !quitchars */
	    if (force) goto nxtc;
	    pline("Done.");
	    msg_given = FALSE;	/* suppress clear */
	    cx = -1;
	    cy = 0;
	    result = 0;	/* not -1 */
	    break;
	}
    nxtc:	;
#ifdef CLIPPING
	cliparound(cx, cy);
#endif
	curs(WIN_MAP,cx,cy);
	flush_screen(0);
    }
#ifdef MAC
    lock_mouse_cursor(FALSE);
#endif
    if (msg_given) clear_nhwindow(WIN_MESSAGE);
    cc->x = cx;
    cc->y = cy;
    return result;
}

struct monst *
christen_monst(mtmp, name)
struct monst *mtmp;
const char *name;
{
	int lth;
	struct monst *mtmp2;
	char buf[PL_PSIZ];

	/* dogname & catname are PL_PSIZ arrays; object names have same limit */
	lth = *name ? (int)(strlen(name) + 1) : 0;
	if(lth > PL_PSIZ){
		lth = PL_PSIZ;
		name = strncpy(buf, name, PL_PSIZ - 1);
		buf[PL_PSIZ - 1] = '\0';
	}
	if (lth == mtmp->mnamelth) {
		/* don't need to allocate a new monst struct */
		if (lth) Strcpy(NAME(mtmp), name);
		return mtmp;
	}
	mtmp2 = newmonst(mtmp->mxlth + lth);
	*mtmp2 = *mtmp;
	(void) memcpy((genericptr_t)mtmp2->mextra,
		      (genericptr_t)mtmp->mextra, mtmp->mxlth);
	mtmp2->mnamelth = lth;
	if (lth) Strcpy(NAME(mtmp2), name);
	replmon(mtmp,mtmp2);
	return(mtmp2);
}

int
do_mname()
{
	char buf[BUFSZ];
	coord cc;
	register int cx,cy;
	register struct monst *mtmp;
	char qbuf[QBUFSZ];

	if (Hallucination) {
		You("would never recognize it anyway.");
		return 0;
	}
	cc.x = u.ux;
	cc.y = u.uy;
	if (getpos(&cc, FALSE, "the monster you want to name") < 0 ||
			(cx = cc.x) < 0)
		return 0;
	cy = cc.y;

	if (cx == u.ux && cy == u.uy) {
#ifdef STEED
	    if (u.usteed && canspotmon(u.usteed))
		mtmp = u.usteed;
	    else {
#endif
		pline("This %s creature is called %s and cannot be renamed.",
		ACURR(A_CHA) > 14 ?
		(flags.female ? "beautiful" : "handsome") :
		"ugly",
		plname);
		return(0);
#ifdef STEED
	    }
#endif
	} else
	    mtmp = m_at(cx, cy);

	if (!mtmp || (!sensemon(mtmp) &&
			(!(cansee(cx,cy) || see_with_infrared(mtmp)) || mtmp->mundetected
			|| mtmp->m_ap_type == M_AP_FURNITURE
			|| mtmp->m_ap_type == M_AP_OBJECT
			|| (mtmp->minvis && !See_invisible)))) {
		pline("I see no monster there.");
		return(0);
	}
	/* special case similar to the one in lookat() */
	(void) distant_monnam(mtmp, ARTICLE_THE, buf);
	Sprintf(qbuf, "What do you want to call %s?", buf);
	getlin(qbuf,buf);
	if(!*buf || *buf == '\033') return(0);
	/* strip leading and trailing spaces; unnames monster if all spaces */
	(void)mungspaces(buf);

	if ( mtmp->data == &mons[PM_HIGH_PRIEST] )
	    pline("Abusing the astral call bug, huh, cheater? That's not gonna work anymore! --Amy");
	else if (mtmp->data->geno & G_UNIQ)
	    pline("%s doesn't like being called names!", Monnam(mtmp));
	else
	    (void) christen_monst(mtmp, buf);
	return(0);
}

/*
 * This routine changes the address of obj. Be careful not to call it
 * when there might be pointers around in unknown places. For now: only
 * when obj is in the inventory.
 */
STATIC_OVL
void
do_oname(obj)
register struct obj *obj;
{
	char buf[BUFSZ], qbuf[QBUFSZ];
	const char *aname;
	short objtyp;

	Sprintf(qbuf, "What do you want to name %s %s?",
		is_plural(obj) ? "these" : "this", xname(obj));
	getlin(qbuf, buf);
	if(!*buf || *buf == '\033')	return;
	/* strip leading and trailing spaces; unnames item if all spaces */
	(void)mungspaces(buf);

	/* relax restrictions over proper capitalization for artifacts */
	if ((aname = artifact_name(buf, &objtyp)) != 0 && objtyp == obj->otyp)
		Strcpy(buf, aname);

	if (obj->oartifact) {
		pline_The("artifact seems to resist the attempt.");
		return;
	} else if (restrict_name(obj, buf) || exist_artifact(obj->otyp, buf)) {
		int n = rn2((int)strlen(buf));
		register char c1, c2;

		c1 = lowc(buf[n]);
		do c2 = 'a' + rn2('z'-'a'); while (c1 == c2);
		buf[n] = (buf[n] == c1) ? c2 : highc(c2);  /* keep same case */
		pline("While engraving your %s slips.", body_part(HAND));
		display_nhwindow(WIN_MESSAGE, FALSE);
		You("engrave: \"%s\".",buf);
	}
	obj = oname(obj, buf);
}

/*
 * Allocate a new and possibly larger storage space for an obj.
 */
struct obj *
realloc_obj(obj, oextra_size, oextra_src, oname_size, name)
struct obj *obj;
int oextra_size;		/* storage to allocate for oextra            */
genericptr_t oextra_src;
int oname_size;			/* size of name string + 1 (null terminator) */
const char *name;
{
	struct obj *otmp;

	otmp = newobj(oextra_size + oname_size);
	*otmp = *obj;	/* the cobj pointer is copied to otmp */
	if (oextra_size) {
	    if (oextra_src)
		(void) memcpy((genericptr_t)otmp->oextra, oextra_src,
							oextra_size);
	} else {
	    otmp->oattached = OATTACHED_NOTHING;
	}
	otmp->oxlth = oextra_size;

	otmp->onamelth = oname_size;
	otmp->timed = 0;	/* not timed, yet */
	otmp->lamplit = 0;	/* ditto */
	/* __GNUC__ note:  if the assignment of otmp->onamelth immediately
	   precedes this `if' statement, a gcc bug will miscompile the
	   test on vax (`insv' instruction used to store bitfield does
	   not set condition codes, but optimizer behaves as if it did).
	   gcc-2.7.2.1 finally fixed this.... */
	if (oname_size) {
	    if (name)
		Strcpy(ONAME(otmp), name);
	}

	if (obj->owornmask) {
		boolean save_twoweap = u.twoweap;
		/* unwearing the old instance will clear dual-wield mode
		   if this object is either of the two weapons */
		setworn((struct obj *)0, obj->owornmask);
		setworn(otmp, otmp->owornmask);
		u.twoweap = save_twoweap;
	}

	/* replace obj with otmp */
	replace_object(obj, otmp);

	/* fix ocontainer pointers */
	if (Has_contents(obj)) {
		struct obj *inside;

		for(inside = obj->cobj; inside; inside = inside->nobj)
			inside->ocontainer = otmp;
	}

	/* move timers and light sources from obj to otmp */
	if (obj->timed) obj_move_timers(obj, otmp);
	if (obj->lamplit) obj_move_light_source(obj, otmp);

	/* objects possibly being manipulated by multi-turn occupations
	   which have been interrupted but might be subsequently resumed */
	if (obj->oclass == FOOD_CLASS)
	    food_substitution(obj, otmp);	/* eat food or open tin */
	else if (obj->oclass == SPBOOK_CLASS)
	    book_substitution(obj, otmp);	/* read spellbook */

	/* obfree(obj, otmp);	now unnecessary: no pointers on bill */
	dealloc_obj(obj);	/* let us hope nobody else saved a pointer */
	return otmp;
}

struct obj *
oname(obj, name)
struct obj *obj;
const char *name;
{
	int lth;
	char buf[PL_PSIZ];

	lth = *name ? (int)(strlen(name) + 1) : 0;
	if (lth > PL_PSIZ) {
		lth = PL_PSIZ;
		name = strncpy(buf, name, PL_PSIZ - 1);
		buf[PL_PSIZ - 1] = '\0';
	}
	/* If named artifact exists in the game, do not create another.
	 * Also trying to create an artifact shouldn't de-artifact
	 * it (e.g. Excalibur from prayer). In this case the object
	 * will retain its current name. */
	if (obj->oartifact || (lth && exist_artifact(obj->otyp, name)))
		return obj;

	if (lth == obj->onamelth) {
		/* no need to replace entire object */
		if (lth) Strcpy(ONAME(obj), name);
	} else {
		obj = realloc_obj(obj, obj->oxlth,
			      (genericptr_t)obj->oextra, lth, name);
	}
	if (lth) artifact_exists(obj, name, TRUE);
	if (obj->oartifact) {
	    /* can't dual-wield with artifact as secondary weapon */
	    if (obj == uswapwep) untwoweapon();
	    /* activate warning if you've just named your weapon "Sting" */
	    if (obj == uwep) set_artifact_intrinsic(obj, TRUE, W_WEP);
	}
	if (carried(obj)) update_inventory();
	return obj;
}

static NEARDATA const char callable[] = {
	SCROLL_CLASS, POTION_CLASS, WAND_CLASS, RING_CLASS, AMULET_CLASS,
	GEM_CLASS, SPBOOK_CLASS, ARMOR_CLASS, TOOL_CLASS, 0 };

int
ddocall()
{
	register struct obj *obj;
#ifdef REDO
	char	ch;
#endif
	char allowall[2];

	switch(
#ifdef REDO
		ch =
#endif
		ynq("Name an individual object?")) {
	case 'q':
		break;
	case 'y':
#ifdef REDO
		savech(ch);
#endif
		allowall[0] = ALL_CLASSES; allowall[1] = '\0';
		obj = getobj(allowall, "name");
		if(obj) do_oname(obj);
		break;
	default :
#ifdef REDO
		savech(ch);
#endif
		obj = getobj(callable, "call");
		if (obj) {
			/* behave as if examining it in inventory;
			   this might set dknown if it was picked up
			   while blind and the hero can now see */
			(void) xname(obj);

			if (!obj->dknown) {
				You("would never recognize another one.");
				return 0;
			}
			docall(obj);
		}
		break;
	}
	return 0;
}

void
docall(obj)
register struct obj *obj;
{
	char buf[BUFSZ], qbuf[QBUFSZ];
	struct obj otemp;
	register char **str1;

	if (!obj->dknown) return; /* probably blind */
	otemp = *obj;
	otemp.quan = 1L;
	otemp.onamelth = 0;
	otemp.oxlth = 0;
	if (objects[otemp.otyp].oc_class == POTION_CLASS && otemp.fromsink)
	    /* kludge, meaning it's sink water */
	    Sprintf(qbuf,"Call a stream of %s fluid:",
		    OBJ_DESCR(objects[otemp.otyp]));
	else
	    Sprintf(qbuf, "Call %s:", an(xname(&otemp)));
	getlin(qbuf, buf);
	if(!*buf || *buf == '\033')
		return;

	/* clear old name */
	str1 = &(objects[obj->otyp].oc_uname);
	if(*str1) free((genericptr_t)*str1);

	/* strip leading and trailing spaces; uncalls item if all spaces */
	(void)mungspaces(buf);
	if (!*buf) {
	    if (*str1) {	/* had name, so possibly remove from disco[] */
		/* strip name first, for the update_inventory() call
		   from undiscover_object() */
		*str1 = (char *)0;
		undiscover_object(obj->otyp);
	    }
	} else {
	    *str1 = strcpy((char *) alloc((unsigned)strlen(buf)+1), buf);
	    discover_object(obj->otyp, FALSE, TRUE); /* possibly add to disco[] */
	}
}

#endif /*OVLB*/
#ifdef OVL0

static const char * const ghostnames[] = {
	/* these names should have length < PL_NSIZ */
	/* Capitalize the names for aesthetics -dgk */
	"Adri", "Andries", "Andreas", "Bert", "David", "Dirk", "Emile",
	"Frans", "Fred", "Greg", "Hether", "Jay", "John", "Jon", "Karnov",
	"Kay", "Kenny", "Kevin", "Maud", "Michiel", "Mike", "Peter", "Robert",
	"Ron", "Tom", "Wilmar", "Nick Danger", "Phoenix", "Jiro", "Mizue",
	"Stephan", "Lance Braccus", "Shadowhawk"
};

/* ghost names formerly set by x_monnam(), now by makemon() instead */
const char *
rndghostname()
{
    return rn2(7) ? ghostnames[rn2(SIZE(ghostnames))] : (const char *)plname;
}

/* undead player monsters with names --Amy */

static const char * const plrmonnames[] = {

"Wolf", "Big Bear", "Ryu", "Tacitus", "Urbaldi", "Pete", "Lex", "Denshi Gasu", "Mr. Black", "Tiger's Claw", "Katzou", "Mohmar Deathstrike", "Ingo", "Septimus", "Martius", "Faster-Than-All-Others", "Senator Antius", "H.", "Pokoh", "Davide", "Aee", "Doctor Maex", "Marc", "Arno", "Hailbush", "Romann", "Siegfried", "Roy", "G-cheater", "Bastian", "Nicyan", "Queelix", "Miesael", "Honno", "Robin", "JNR", "Lars", "Tommy", "Giglio", "Kastortransport", "Larry", "Morton", "Iggy", "Lemmy", "Ludwig", "Oberdan", "Len-kind", "Ilie", "Till", "Tomas", "Nikolob", "Tillbull"

};

/* it's obvious that I seem to be better at making up female names ;) */

static const char * const plrmonnamesfemale[] = {

"JoJo", "Jyllia", "Sabrina", "Sabine", "Yvara", "Lenka", "Evita", "Liebea", "Isolde", "Elli", "Vilja", "Sunija", "Rhea", "Jasmin", "Erosina", "Irmina", "Melirija", "Larissa", "Sysette", "Miss Haskill", "Elenya", "Golden Mary", "Lara", "Sandrina", "Tonilia", "Claire", "Lumia", "Lahira", "Estrella", "Maricia", "Sontaire", "Marje", "Jill", "Trycja", "Kersey", "Sally", "Hannya", "Svantje", "Jynnifyr", "Elke", "Rinka", "Nicoletta", "Betti", "Ina", "Heikipa", "Jora", "Maitine", "Esruth", "Verene", "Lousie", "Irinella", "Amandina", "Lillie", "Leodoch", "Mirella", "Fisoa", "Suesska", "Ann", "Nurisha", "Desiree", "Birgit", "Elsbeth", "Lamy", "Lissie", "Arabella", "Anastasia", "Henrietta", "Katrin", "Jana", "Aniya", "Yasni", "Almina", "Xeni", "Mirri", "Eleanor", "Kirja", "Inge", "Helli", "Lucia", "Viktorija", "Simona", "Natalyana", "Krista", "Nellina", "Raidara", "Vera", "Noko", "Jasajeen", "Marika", "Merbek", "Marianna", "Sinja", "Rodotha", "Natinya", "Aline", "Michaela", "Mare", "Noenoe", "Tschulia", "Lea", "Sarah", "Charravalga", "Fridrika", "Great Jaguar Claw", "Lynette", "Celina", "Irya", "Mariya", "Wendy", "Katia", "Tanja", "Vanessa", "Anne", "Lena", "Jeanetta", "Rungud", "Melissa", "Everella", "Madeleine", "Anita", "Nina", "Natascha", "Manola", "Litta", "Kiwi", "Maja", "Natalje", "Little Marie" 

};

/* the following functions are used by makemon.c */

const char *
rndplrmonname()
{
    return plrmonnames[rn2(SIZE(plrmonnames))];
}

const char *
rndplrmonnamefemale()
{
    return plrmonnamesfemale[rn2(SIZE(plrmonnamesfemale))];
}

/* Monster naming functions:
 * x_monnam is the generic monster-naming function.
 *		  seen	      unseen	   detected		  named
 * mon_nam:	the newt	it	the invisible orc	Fido
 * noit_mon_nam:the newt (as if detected) the invisible orc	Fido
 * l_monnam:	newt		it	invisible orc		dog called fido
 * Monnam:	The newt	It	The invisible orc	Fido
 * noit_Monnam: The newt (as if detected) The invisible orc	Fido
 * Adjmonnam:	The poor newt	It	The poor invisible orc	The poor Fido
 * Amonnam:	A newt		It	An invisible orc	Fido
 * a_monnam:	a newt		it	an invisible orc	Fido
 * m_monnam:	newt		xan	orc			Fido
 * y_monnam:	your newt     your xan	your invisible orc	Fido
 */

/* Bug: if the monster is a priest or shopkeeper, not every one of these
 * options works, since those are special cases.
 */
char *
x_monnam(mtmp, article, adjective, suppress, called)
register struct monst *mtmp;
int article;
/* ARTICLE_NONE, ARTICLE_THE, ARTICLE_A: obvious
 * ARTICLE_YOUR: "your" on pets, "the" on everything else
 *
 * If the monster would be referred to as "it" or if the monster has a name
 * _and_ there is no adjective, "invisible", "saddled", etc., override this
 * and always use no article.
 */
const char *adjective;
int suppress;
/* SUPPRESS_IT, SUPPRESS_INVISIBLE, SUPPRESS_HALLUCINATION, SUPPRESS_SADDLE.
 * EXACT_NAME: combination of all the above
 */
boolean called;
{
#ifdef LINT	/* static char buf[BUFSZ]; */
	char buf[BUFSZ];
#else
	static char buf[BUFSZ];
#endif
	struct permonst *mdat = mtmp->data;
	boolean do_hallu, do_invis, do_it, do_saddle;
	boolean name_at_start, has_adjectives;
	char *bp;

	if (program_state.gameover)
	    suppress |= SUPPRESS_HALLUCINATION;
	if (article == ARTICLE_YOUR && !mtmp->mtame)
	    article = ARTICLE_THE;

	do_hallu = Hallucination && !(suppress & SUPPRESS_HALLUCINATION);
	do_invis = mtmp->minvis && !(suppress & SUPPRESS_INVISIBLE);
	do_it = !canspotmon(mtmp) &&
	    article != ARTICLE_YOUR &&
	    !program_state.gameover &&
#ifdef STEED
	    mtmp != u.usteed &&
#endif
	    !(u.uswallow && mtmp == u.ustuck) &&
	    !(suppress & SUPPRESS_IT);
	do_saddle = !(suppress & SUPPRESS_SADDLE);

	buf[0] = 0;

	/* unseen monsters, etc.  Use "it" */
	if (do_it) {
	    Strcpy(buf, "it");
	    return buf;
	}

	/* priests and minions: don't even use this function */
	if (mtmp->ispriest || mtmp->isminion) {
	    char priestnambuf[BUFSZ];
	    char *name;
	    long save_prop = EHalluc_resistance;
	    unsigned save_invis = mtmp->minvis;

	    /* when true name is wanted, explicitly block Hallucination */
	    if (!do_hallu) EHalluc_resistance = 1L;
	    if (!do_invis) mtmp->minvis = 0;
	    name = priestname(mtmp, priestnambuf);
	    EHalluc_resistance = save_prop;
	    mtmp->minvis = save_invis;
	    if (article == ARTICLE_NONE && !strncmp(name, "the ", 4))
		name += 4;
	    return strcpy(buf, name);
	}

	/* Shopkeepers: use shopkeeper name.  For normal shopkeepers, just
	 * "Asidonhopo"; for unusual ones, "Asidonhopo the invisible
	 * shopkeeper" or "Asidonhopo the blue dragon".  If hallucinating,
	 * none of this applies.
	 */
	if (mtmp->isshk && !do_hallu) {
	    if (adjective && article == ARTICLE_THE) {
		/* pathological case: "the angry Asidonhopo the blue dragon"
		   sounds silly */
		Strcpy(buf, "the ");
		Strcat(strcat(buf, adjective), " ");
		Strcat(buf, shkname(mtmp));
		return buf;
	    }
	    Strcat(buf, shkname(mtmp));
	    if (mdat == &mons[PM_SHOPKEEPER] && !do_invis)
		return buf;
	    Strcat(buf, " the ");
	    if (do_invis)
		Strcat(buf, "invisible ");
	    Strcat(buf, mdat->mname);
	    return buf;
	}

	/* Put the adjectives in the buffer */
	if (adjective)
	    Strcat(strcat(buf, adjective), " ");
	if (do_invis)
	    Strcat(buf, "invisible ");
#ifdef STEED
	if (do_saddle && (mtmp->misc_worn_check & W_SADDLE) &&
	    !Blind && !Hallucination)
	    Strcat(buf, "saddled ");
#endif
	if (buf[0] != 0)
	    has_adjectives = TRUE;
	else
	    has_adjectives = FALSE;

	/* Put the actual monster name or type into the buffer now */
	/* Be sure to remember whether the buffer starts with a name */
	if (do_hallu) {
	    Strcat(buf, rndmonnam());
	    name_at_start = FALSE;
	} else if (mtmp->mnamelth) {
	    char *name = NAME(mtmp);

	    if (mdat == &mons[PM_GHOST]) {
		Sprintf(eos(buf), "%s ghost", s_suffix(name));
		name_at_start = TRUE;
	    } else if (called) {
		Sprintf(eos(buf), "%s called %s", mdat->mname, name);
		name_at_start = (boolean)type_is_pname(mdat);
	    } else if (is_mplayer(mdat) && (bp = strstri(name, " the ")) != 0) {
		/* <name> the <adjective> <invisible> <saddled> <rank> */
		char pbuf[BUFSZ];

		Strcpy(pbuf, name);
		pbuf[bp - name + 5] = '\0'; /* adjectives right after " the " */
		if (has_adjectives)
		    Strcat(pbuf, buf);
		Strcat(pbuf, bp + 5);	/* append the rest of the name */
		Strcpy(buf, pbuf);
		article = ARTICLE_NONE;
		name_at_start = TRUE;
	    } else {
		Strcat(buf, name);
		name_at_start = TRUE;
	    }
	} else if (is_mplayer(mdat) && !In_endgame(&u.uz)) {
	    char pbuf[BUFSZ];
	    Strcpy(pbuf, rank_of((int)mtmp->m_lev,
				 monsndx(mdat),
				 (boolean)mtmp->female));
	    Strcat(buf, lcase(pbuf));
	    name_at_start = FALSE;
	} else {
	    Strcat(buf, mdat->mname);
	    name_at_start = (boolean)type_is_pname(mdat);
	}

	if (name_at_start && (article == ARTICLE_YOUR || !has_adjectives)) {
	    if (mdat == &mons[PM_WIZARD_OF_YENDOR])
		article = ARTICLE_THE;
	    else
		article = ARTICLE_NONE;
	} else if ((mdat->geno & G_UNIQ) && article == ARTICLE_A) {
	    article = ARTICLE_THE;
	}

	{
	    char buf2[BUFSZ];

	    switch(article) {
		case ARTICLE_YOUR:
		    Strcpy(buf2, "your ");
		    Strcat(buf2, buf);
		    Strcpy(buf, buf2);
		    return buf;
		case ARTICLE_THE:
		    Strcpy(buf2, "the ");
		    Strcat(buf2, buf);
		    Strcpy(buf, buf2);
		    return buf;
		case ARTICLE_A:
		    return(an(buf));
		case ARTICLE_NONE:
		default:
		    return buf;
	    }
	}
}

#endif /* OVL0 */
#ifdef OVLB

char *
l_monnam(mtmp)
register struct monst *mtmp;
{
	return(x_monnam(mtmp, ARTICLE_NONE, (char *)0,
		mtmp->mnamelth ? SUPPRESS_SADDLE : 0, TRUE));
}

#endif /* OVLB */
#ifdef OVL0

char *
mon_nam(mtmp)
register struct monst *mtmp;
{
	return(x_monnam(mtmp, ARTICLE_THE, (char *)0,
		mtmp->mnamelth ? SUPPRESS_SADDLE : 0, FALSE));
}

/* print the name as if mon_nam() was called, but assume that the player
 * can always see the monster--used for probing and for monsters aggravating
 * the player with a cursed potion of invisibility
 */
char *
noit_mon_nam(mtmp)
register struct monst *mtmp;
{
	return(x_monnam(mtmp, ARTICLE_THE, (char *)0,
		mtmp->mnamelth ? (SUPPRESS_SADDLE|SUPPRESS_IT) :
		    SUPPRESS_IT, FALSE));
}

char *
Monnam(mtmp)
register struct monst *mtmp;
{
	register char *bp = mon_nam(mtmp);

	*bp = highc(*bp);
	return(bp);
}

char *
noit_Monnam(mtmp)
register struct monst *mtmp;
{
	register char *bp = noit_mon_nam(mtmp);

	*bp = highc(*bp);
	return(bp);
}

/* monster's own name */
char *
m_monnam(mtmp)
struct monst *mtmp;
{
	return x_monnam(mtmp, ARTICLE_NONE, (char *)0, EXACT_NAME, FALSE);
}

/* pet name: "your little dog" */
char *
y_monnam(mtmp)
struct monst *mtmp;
{
	int prefix, suppression_flag;

	prefix = mtmp->mtame ? ARTICLE_YOUR : ARTICLE_THE;
	suppression_flag = (mtmp->mnamelth
#ifdef STEED
			    /* "saddled" is redundant when mounted */
			    || mtmp == u.usteed
#endif
			    ) ? SUPPRESS_SADDLE : 0;

	return x_monnam(mtmp, prefix, (char *)0, suppression_flag, FALSE);
}

#endif /* OVL0 */
#ifdef OVLB

char *
Adjmonnam(mtmp, adj)
register struct monst *mtmp;
register const char *adj;
{
	register char *bp = x_monnam(mtmp, ARTICLE_THE, adj,
		mtmp->mnamelth ? SUPPRESS_SADDLE : 0, FALSE);

	*bp = highc(*bp);
	return(bp);
}

char *
a_monnam(mtmp)
register struct monst *mtmp;
{
	return x_monnam(mtmp, ARTICLE_A, (char *)0,
		mtmp->mnamelth ? SUPPRESS_SADDLE : 0, FALSE);
}

char *
Amonnam(mtmp)
register struct monst *mtmp;
{
	register char *bp = a_monnam(mtmp);

	*bp = highc(*bp);
	return(bp);
}

/* used for monster ID by the '/', ';', and 'C' commands to block remote
   identification of the endgame altars via their attending priests */
char *
distant_monnam(mon, article, outbuf)
struct monst *mon;
int article;	/* only ARTICLE_NONE and ARTICLE_THE are handled here */
char *outbuf;
{
    /* high priest(ess)'s identity is concealed on the Astral Plane,
       unless you're adjacent (overridden for hallucination which does
       its own obfuscation) */
    if (mon->data == &mons[PM_HIGH_PRIEST] && !Hallucination &&
	    Is_astralevel(&u.uz) && distu(mon->mx, mon->my) > 2) {
	Strcpy(outbuf, article == ARTICLE_THE ? "the " : "");
	Strcat(outbuf, mon->female ? "high priestess" : "high priest");
    } else {
	Strcpy(outbuf, x_monnam(mon, article, (char *)0, 0, TRUE));
    }
    return outbuf;
}

static const char * const bogusmons[] = {
	"jumbo shrimp", "giant pigmy", "gnu", "killer penguin",
	"giant cockroach", "giant slug", "pterodactyl",
	"tyrannosaurus rex", "rot grub", "bookworm", "mastah lichen",
	"hologram", "jester", "attorney", "sleazoid",
	"killer tomato", "amazon", "robot", "battlemech",
	"rhinovirus", "lion-dog", "rat-ant", "Y2K bug",
						/* misc. */
	"grue", "Christmas-tree monster", "luck sucker", "paskald",
	"brogmoid", "dornbeast",		/* Quendor (Zork, &c.) */
	"Ancient Multi-Hued Dragon", "Evil Iggy",
						/* Moria */
	"emu", "kestrel", "xeroc", "venus flytrap",
						/* Rogue */
	"creeping coins",			/* Wizardry */
	"siren",                                /* Greek legend */
	"killer bunny",				/* Monty Python */
	"rodent of unusual size",		/* The Princess Bride */
	"Smokey the bear",	/* "Only you can prevent forest fires!" */
	"Luggage",				/* Discworld */
	"Ent",					/* Lord of the Rings */
	"tangle tree", "wiggle",                /* Xanth */
	"white rabbit", "snark",		/* Lewis Carroll */
	"pushmi-pullyu",			/* Dr. Doolittle */
	"smurf",				/* The Smurfs */
	"tribble", "Klingon", "Borg",		/* Star Trek */
	"Ewok",					/* Star Wars */
	"Totoro",				/* Tonari no Totoro */
	"ohmu",					/* Nausicaa */
	"youma",				/* Sailor Moon */
	"nyaasu",				/* Pokemon (Meowth) */
	"Godzilla", "King Kong",		/* monster movies */
	"earthquake beast",			/* old L of SH */
	"Invid",				/* Robotech */
	"Terminator",				/* The Terminator */
	"boomer",				/* Bubblegum Crisis */
	"Dalek",				/* Dr. Who ("Exterminate!") */
	"microscopic space fleet", "Ravenous Bugblatter Beast of Traal",
						/* HGttG */
	"teenage mutant ninja turtle",		/* TMNT */
	"samurai rabbit",			/* Usagi Yojimbo */
	"aardvark",				/* Cerebus */
	"Audrey II",				/* Little Shop of Horrors */
	"witch doctor", "one-eyed one-horned flying purple people eater",
						/* 50's rock 'n' roll */
	"Barney the dinosaur",			/* saccharine kiddy TV */
	"Azog the Orc King", "Morgoth",		/* Angband */

	/*[Tom] new wacky names */
	"commando", "green beret", "sherman tank",
						/* Military */
	"Jedi knight", "tie fighter", "protocol droid", "R2 unit", "Emperor",
						/* Star Wars */
	"Vorlon",				/* Babylon 5 */
	"keg","Diet Pepsi",
						/* drinks */
	"questing beast",		/* King Arthur */
	"Predator",				/* Movie */
	"green light", "automobile", "invisible Wizard of Yendor",
	"piece of yellowish-brown glass", "wand of nothing",
	"ocean","ballpoint pen","paper cut",
						/* misc */
	"Rune", "Gurk", "Yuval",		/* people I know */
	"mother-in-law"				/* common pest */
	"one-winged dewinged stab-bat",		/* KoL */
	"praying mantis",
	"arch-pedant",
	"beluga whale",
	"bluebird of happiness",
	"bouncing eye", "floating nose",
	"buffer overflow", "dangling pointer", "walking disk drive",
	"cacodemon", "scrag",
	"cardboard golem", "duct tape golem",
	"chess pawn",
	"chocolate pudding",
	"coelacanth",
	"corpulent porpoise",
	"Crow T. Robot",
	"diagonally moving grid bug",
	"dropbear",
	"Dudley",
	"El Pollo Diablo",
	"evil overlord",
	"existential angst",
	"figment of your imagination", "flash of insight",
	"flying pig",
	"gazebo",
	"gonzo journalist",
	"gray goo", "magnetic monopole",
	"heisenbug",
	"lag monster",
	"loan shark",
	"Lord British",
	"newsgroup troll",
	"ninja pirate zombie robot",
	"octarine dragon",
	"particle man",
	"possessed waffle iron",
	"poultrygeist",
	"raging nerd",
	"roomba",
	"sea cucumber",
	"spelling bee",
	"Strong Bad",
	"stuffed raccoon puppet",
	"tapeworm",
	"liger",
	"velociraptor",
	"vermicious knid",
	"viking",
	"voluptuous ampersand",
	"wee green blobbie",
	"wereplatypus",
	"zergling",
	"hag of bolding",
	"grind bug",
	"enderman",
	"wight supremacist",
	"Magical Trevor",
	"first category perpetual motion device",
	"ghoti",
	"regex engine",
	"netsplit",
	"peer",
	"pigasus",
	"Semigorgon",
	"meeple",
	"conventioneer",
	"terracotta warrior",
	"large microbat", "small megabat",

	/* soundex and typos of monsters, from NAO, added in UnNetHack */
	"gloating eye",
	"flush golem"
	"martyr orc",
	"mortar orc",
	"acute blob",
	"aria elemental",
	"aliasing priest",
	"aligned parasite",
	"aligned parquet",
	"aligned proctor",
	"baby balky dragon",
	"baby blues dragon",
	"baby caricature",
	"baby crochet",
	"baby grainy dragon",
	"baby bong worm",
	"baby long word",
	"baby parable worm",
	"barfed devil",
	"beer wight",
	"boor wight",
	"brawny mold",
	"rave spider",
	"clue golem",
	"bust vortex",
	"errata elemental",
	"elastic eel",
	"electrocardiogram eel",
	"fir elemental",
	"tire elemental",
	"flamingo sphere",
	"fallacy golem",
	"frizzed centaur",
	"forest centerfold",
	"fierceness sphere",
	"frosted giant",
	"geriatric snake",
	"gnat ant",
	"giant bath",
	"grant beetle",
	"giant mango",
	"glossy golem",
	"gnome laureate",
	"gnome dummy",
	"gooier ooze",
	"green slide",
	"guardian nacho",
	"hell hound pun",
	"high purist",
	"hairnet devil",
	"ice trowel",
	"feather golem",
	"lounge worm",
	"mountain lymph",
	"pager golem",
	"pie fiend",
	"prophylactic worm",
	"sock mole",
	"rogue piercer",
	"seesawing sphere",
	"simile mimic",
	"moldier ant",
	"stain vortex",
	"scone giant",
	"umbrella hulk",
	"vampire mace",
	"verbal jabberwock",
	"water lemon",
	"water melon",
	"winged grizzly",
	"yellow wight",

	/* from http://www.alt.org/nethack/addmsgs/viewmsgs.php added in UnNetHackPlus*/
	"lurker below",
	"worthless yellowish-brown glass golem",
	"writhing mass of primal chaos", /* ADOM */
	"hallucinatory monster",
	"jumping brain",
	"colorless green idea",
	"floating ear",
	"floating tongue",
	"hallucinogen-distorted hallucination",
	"mountain dwarf",
	"were(random beast)",
	"weremindflayer",
	"wereplatypus",
	"Gnome With the Wand of Death",
	"arch-lichen",
	"Baba Yaga",
	"harmless protoplasm",
	"badger",
	"giant dwarf",
	"magically animated Vorpal Blade",
	"Legendary black beast of Arrrgh",

	/* from UnNetHack */
	"apostroph golem", "Bob the angry flower",
	"bonsai-kitten", "Boxxy", "lonelygirl15",
	"tie-thulu", "Domo-kun", "nyan cat",
	"looooooooooooong cat",			/* internet memes */
	"bohrbug", "mandelbug", "schroedinbug", /* bugs */
	"Gerbenok",				/* Monty Python killer rabbit */
	"doenertier",				/* Erkan & Stefan */
	"Invisible Pink Unicorn",
	"Flying Spaghetti Monster",		/* deities */
	"Bluebear", "Professor Abdullah Nightingale",
	"Qwerty Uiop", "troglotroll",		/* Zamonien */
	"wolpertinger", "elwedritsche", "skvader",
	"Nessie", "tatzelwurm", "dahu",		/* european cryptids */
	"three-headed monkey",			/* Monkey Island */
	"little green man",			/* modern folklore */
	"weighted Companion Cube",		/* Portal */
	"/b/tard",				/* /b/ */
	"manbearpig",				/* South Park */
	"ceiling cat", "basement cat",
	"monorail cat",				/* the Internet is made for cat pix */
	"rape golem",				/* schnippi */
	"tridude",				/* POWDER */
	"orcus cosmicus",			/* Radomir Dopieralski */
	"yeek", "quylthulg",
	"Greater Hell Beast",			/* Angband */
	"Vendor of Yizard",			/* Souljazz */
	"Sigmund", "lernaean hydra", "Ijyb",
	"Gloorx Vloq", "Blork the orc",		/* Dungeon Crawl Stone Soup */
	"unicorn pegasus kitten",		/* Wil Wheaton, John Scalzi */
	"dwerga nethackus", "dwerga castrum",	/* Ask ASCII Ponies */

	/* from UnNetHackPlus */
	"King Krakus",               /* Polish folklore */
	"Topielec",                  /* Slavic folklore */
	"pink oliphaunt",            /* Lord of the Rings + silliness */
	"Amphisbaena",               /* Greek mythology */
	"phoenix",                   /* Greek mythology */
	"catoblepas",                /* Greek mythology */
	"phantom kangaroo",          /* urban legend */
	"echinemon",                 /* from medieval literature, "enemy of the dragon" */
	"Ratatoskr",                 /* Norse mythology */
	"Twrch Trwyth",              /* Arthurian legends */
	"Unperson",                  /* Nineteen Eighty-Four */
	"Somebody Else's Problem",   /* Douglas Adams */
	"Armok",                     /* Dwarf Fortress */
	"Dwarf-Eating Carp",         /* Dwarf Fortress */
	"Urist McDwarf",             /* Dwarf Fortress */
	"werecapybara",              /* Dwarf Fortress */
	"werecthulhu",
	"weresomething",
	"Evil Otto",                 /* Berzerk - via GruntHack */
	"P'lod",                     /* Weekly World News */
	"mortgage golem",
	"dark matter golem",
	"giant orange brain",        /* Dungeon Crawl Stone Soup */
	"ugly thing",                /* Dungeon Crawl Stone Soup */
	"hellephant",                /* Dungeon Crawl Stone Soup */
	"inept mimic",               /* Dungeon Crawl Stone Soup */
	"hungry ghost",              /* Dungeon Crawl Stone Soup */
	"unborn deep dwarf",         /* Dungeon Crawl Stone Soup */
	"Wandering mushroom",        /* Dungeon Crawl Stone Soup */
	"Vlad the Inhaler",
	"Delaunay tessellation field estimator",
	"unnameable horror from beyond",/* NAO fruit name*/
	"munchkin",
	"error-spamming shambling horror",          /* SporkHack */
	"Grid Bug Mk. 2",            /* SLAS'EM (nickname of arc bugs) */
	/* "killer tripe ration", */      /* SLAS'EM */
	"yet another D&D monster",
	"kobold mage",
	"hobbyte",
	/* via ProgressQuest */
	"will-o'-the-wisp",
	"ignis fatuus",
	"triceratops",
	"sylph",
	"stegosaurus",
	"sphinx",
	"spectre",
	"lamassu",
	"su-monster",                /* Dungeons & Dragons */
	"shambling mound",           /* Dungeons & Dragons */
	"sand elemental",            /* Dungeons & Dragons */
	"rubber golem"               /* Dungeons & Dragons */
	"remorhaz",                  /* Dungeons & Dragons */
	"otyugh",                    /* Dungeons & Dragons */
	"bacon elemental",
	"roper",
	"roc",
	"peryton",                   /* Jorge Luis Borges - Book of Imaginary Beings */
	"octopus",
	"beer golem",                /* ProgressQuest */
	"rice giant",                /* ProgressQuest */
	"porn elemental",            /* ProgressQuest */
	"demicanadian",              /* ProgressQuest */
	"gyrognome",                 /* ProgressQuest */
	"cardboard golem",
	"cheese elemental",
	"dervish",
	"dragon turtle",
	"megalosaurus",
	"organist",
	/* end of monsters via ProgressQuest */
	"Lucius Malfoy",             /* Harry Potter */
	"Dumbledore",                /* Harry Potter */
	"Harry Potter",              /* Harry Potter */
	"Crumple-Horned Snorkack",   /* Harry Potter */
	"mailer daemon",               /* with defined MAIL it may be selected as real monster in get_bogus_monster_name */
	"Vaarsuvius",                /* The Order of the Stick */
	"Durkon Thundershield",      /* The Order of the Stick */
	"Roy Greenhilt",             /* The Order of the Stick */
	"Lord Voldemort", "He-who-may-not-be-named", "Tom Marvolo Riddle", "Al-Mutasim", "Basil the Bat Lord", "Insectoid Queen Gypsy Moth", "run-time error", "abnormal program termination", "unhandled exception", "assertion failure", "file read error",
	"halt", "giant error", "world killer", "eater of worlds", "Mickey Mouse", "Donald Duck", "Scrooge McDuck", "Link", "Zelda", "Ganondorf", "Enderdragon", "Pikachu", "Aerodactyl", "Ho-oh", "Elite Four Bruno", "Gym Leader Claire", "Champion Lance",
	"Team Rocket Grunt", "Team Missile Bomb Grunt", "Hamburgler Grunt", "Broil Bunch Grunt", "Arcanine", "Mewtwo", "Mewthree", "Ebony Dark'ness Dementia Raven Way", "B'loody Mary Smith", "Vampire Potter", "Mehrunes Dagon", "Emperor Uriel Septim",
	"General Tullius", "Ulfric Stormcloak", "+7 daedric long sword", "blessed spellbook of Fus-Ro-Dah", "cursed called", "super wooden statue", "Hrungnir, The Hill Giant Lord", "The Demon Lord Surtur", "your godfather", "Donkey Kong", "King K. Rool",
	"Andariel, the Maiden of Anguish", "Duriel, the Lord of Pain", "Mephisto, the Lord of Hate", "Diablo, the Lord of Terror", "Baal, the Lord of Destruction", "B-a-a-l", "extra strong might-enchanted multiple shots lightning enchanted gloam",
	"Caesar", "General Jing-Wei", "President Eden", "Colonel B. Astard", "Elder Owyn Lyons", "Tobar", "General Chase", "Krzzzzssssssthhhhuuuuuullll Hnnnnnngggggggghhhhhhhhhhhh", "Scribe Vallincourt", "Scribe Bigsley", "annoying lab-coat-wearing scientist",
	"nerdy geek", "greedy doctor", "Lord Ashur", "Father Elijah", "Joshua Graham", "Courier Ulysses", "Claude Speed", "Tommy Vercetti", "Carl Johnson", "Niko Bellic", "Dimitri Rascalov", "Melvin 'Big Smoke' Harris", "Sonny Forelli", "Don Salvatore",
	"Mr. Don", "Jimmy Pegorino", "Galdryn the Green", "Shaduroth", "Nauselom", "Greuvenia the Pox", "Ikrella the Witch", "Elder Demus Fathien", "game-freezing ninja lord", "Yagu Matasai", "Osayo Narakami wearing a cerberus band and a winter katana",
	"General Ironside", "pain in the butt", "ass-fucker", "General Mohmar Deathstrike", "paladin tank", "emperor overlord", "aurora alpha bomber", "king raptor", "attack outpost", "lag defense tower", "stinger site", "tunnel network", "nuclear missile silo",
	"particle uplink cannon", "scud storm", "construction dozer", "tactical superweapon", "commander in chief", "terrorist", "S.W.A.T. member", "undercover cop", "Superman", "Gordon Freeman", "G-Man", "Colonel Shepard", "Drill Sergeant Sharp",
	"Drill Sergeant Nasty", "grand inquisitor", "game over screen", "dywypi", "yasd", "fatal food poisoning", "dark wraith", "gigantic mind flayer", "dream eater", "Super Mario", "Princess Peach", "Princess Toadstool", "goomba", "koopa troopa",
	"Lakitu", "Bowser", "Bullet Bill", "homing Bullet Bill", "Blarog", "Wart", "Birdo-Ostro", "Clawglip", "Larry Koopa", "Morton Koopa", "Wendy O. Koopa", "Iggy Koopa", "Roy Koopa", "Lemmy Koopa", "Ludwig von Koopa", "Simon Belmont", "Ryu Hayabusa",
	"Jaquio", "Ashtar", "hallucinogen-distorted Wizard of Yendor", "polytrap abomination", "result of a bad polytrap", "outta depth giant shoggoth", "freaking monadic deva", "James Bond", "Bud Spencer", "Le Chiffre", "Renard", "disappointing final boss",
	"wimpy final boss", "warm-up boss", "wake up call boss", "that one boss", "that one boss named Whitney", "unbeatable Air Man", "Sheriff of Nottingham", "Guy of Gisborne", "gnome who zaps a hexagonal wand", "invisible player character", "invisible outta depth monster",
	"disintegration-breathing cockatrice", "animated gray dragon scale mail", "stupidity in motion", "death-is-death loonie", "hardcore player", "pro-gamer", "hardcore internet nerd", "DDOS attack", "blue screen of death", "blackscreen bug",
	"fatal system error", "windows subsystem has stopped unexpectedly", "general protection fault", "stack overflow", "stack underflow", "pure virtual function call", "not enough space for environment", "low local memory", "not a valid save file",
	"save-game corruption", "savegame erasing bug", "program in disorder", "unstable equilibrium", "crash-prone operating system", "sudden reboot", "power failure", "wide-angle disintegration beam", "infidel priest of Moloch", "Marduk the Creator",
	"killer cram ration", "huge pile of killer rocks", "hallucinogen supplier", "nightmare fuel", "kitten called Wizard of Yendor needs food badly", "little dog called savescum13", "Team Ant Leader", "Team A Leader", "extra fast soldier ant", "soldier ant with the wand of death",
	"invisible soldier ant", "self-replicating soldier ant", "number one cause of nethack deaths", "hallucinogen-distorted master mind flayer", "werecockatrice", "black weredragon", "wererocktroll", "werelich", "were-ki-rin", "weremedusa",
	"weresuccubus", "wereincubus", "hallucinogen-distorted werehallucinator", "gnome wielding the Tsurugi of Muramasa", "goblin wielding a sword called vorpal blade", "pain elemental of Moloch", "air elemental of Air", "fire elemental of Fire",
	"dremora caitiff", "mythical dawn agent", "Lord Sheogorath", "archcouatl", "master solar", "archfiend summon", "archnemesis", "plaster blaster", "psych orb", "Na-Krul", "felltwin", "schizophrenic", "bipolar oddity", "Ford Sierra Cosworth",
	"Lancia Integrale", "Lancia Stratos", "Ford Focus", "Ford Escort", "Toyota Corolla", "MG Metro 6R4", "Peugeot 205 Turbo 16", "drunken driver", "Commissioner Hunter", "Hydra Aurora Bomber", "VTOL aircraft", "multi-purpose amphibian assault ship",
	"devteam member", "player who can ascend any character", "ascension runner", "total noobie", "biggest noob ever", "critically injured smirking sneak thief", "application error", "integer divide by 0", "unrecoverable internal error",

"$cat",
"$dog",
"$fruit -headed Wizard of Yendor",
"$playername",
"$playername 's imaginary girlfriend dual-wielding lightsabers",
"'74 Cadillac",
"(random gas-colour e.g. paisley) dragon",
"... wait a second. You're high!",
"/b/",
"1 of 60 Templars",
"15 m class Eoten",
"23-headed lizard-monkey",
"4chan",
"50 Cent",
"@ sign",
"A Giant Baked Bean",
"A MILF",
"A Mathematician",
"A One Eyed Trouser Snake",
"A Weapon of Mass Destruction",
"A Wood Carver",
"AM",
"ASCII representation of a monster",
"Abraham Lincoln",
"Absolutely nothing",
"Academia",
"Acererak in a TIE fighter",
"Acid worm",
"Adom player",
"Aerith",
"Agent Smith",
"Ahriman",
"Air golem",
"Airman",
"Akkat",
"Albert Einstein",
"Alf",
"Angband player",
"Angra Mainyu",
"Anime Cosplayer",
"Ann Coulter",
"Ann Coulter",
"Anywere",
"Applejack",
"Arceus",
"Astral call bug",
"Aud Ketilsdatter",
"Aunt Irma",
"BASIC elemental",
"BDFL",
"BIKECAT",
"Baba Yaga",
"Baby Tank",
"Bachelor's party",
"Bad Dude",
"Bag of doorknobs",
"Baiowulf",
"Ballos",
"Balrog",
"Ban monster",
"Barbariccia",
"Barky fox",
"Baron Harkonnen",
"Barzahl",
"Basement Cat",
"Basic mind flayer",
"Basic mind flayer",
"Batman",
"Battlesnake",
"Beelzebub",
"Behemoth",
"Bellybutton lint monster",
"Bennifer",
"Betamax",
"Betty & Veronica",
"Big Bird",
"Big, Round, kind of greyish Golem",
"Bill Gates",
"Bill O�Reilly",
"Bj�rk",
"Black Cat",
"Black Knight (with limbs)",
"Black Knight (without limbs)",
"Blaster",
"Blue-Eyes White Dragon",
"Bo Derek",
"Bob Barker",
"Bob Hope",
"Bob the Blob",
"Bon Jovi",
"Bort Sim-pesson",
"Bort Simpesson",
"Bort Sipesson",
"Bowser",
"Bozo the Clown",
"Brain eating snake",
"Brain kicker",
"Brick monster",
"Britney Spears",
"Brotherman Bill",
"Bugger Hive-Queen",
"Bugs Bunny",
"Bugs Bunny",
"Cactaur",
"Caerbannog",
"Cagnazzo",
"Calculator store",
"Calvin & Hobbes",
"Can of Spackling Paste",
"Cantankerous Californian",
"Captain Crunch",
"Captain Gordon, Defender of Earth",
"Captain Kirk",
"Captain Tagon",
"Captain Viridian",
"Care Bear",
"Carrot Soldier",
"Carrot Top",
"Carry Nation",
"Ceiling Cat",
"Cerberus",
"Cerenus",
"Chao",
"Chaos Butterfly",
"Charles Barkley",
"Charlie Chaplin",
"Charon",
"Cheese Golem",
"Chess Piece Face",
"Choronzon",
"Chris the Ninja Pirate",
"Christopher Walken",
"Chuck Norris",
"Chucky",
"Clippy the Shopping Assistant",
"Coach Z",
"Colin Farrell",
"Colonel Sanders",
"Combine soldier",
"Cosmos",
"Crawl player",
"Crow T Robot",
"Crowley",
"Crumple-Horned Snorkack",
"Cryptosporidium",
"Cryptosporidium-136",
"Cthulhu",
"Cthulhu",
"Cthulhu",
"Cucumber",
"Cucumber sandwich",
"Curly Brace",
"Curse of the Were-Rabbit",
"Cursed pile of coins",
"Cyberman",
"Dagoth Ur",
"Dalek",
"Dan Quayle",
"Dante Alighieri",
"Daphne",
"Darth Vader",
"Darwin's fish",
"David Hasselhoff",
"Deadline",
"Death of Rats",
"Descoladore",
"Detritus",
"Dev Team",
"Dion Nicholas",
"Disco Bandit",
"Discord",
"Disemheaded body",
"Disgruntled postal worker",
"Doctor",
"Dogley",
"Don Knotts",
"Donut head, who says 'I am not a hallucination.'",
"Dorothy Gale",
"Dr. Funkenstein",
"Dr. McNinja",
"Dr. Phil",
"Dr. strange",
"Dremora",
"Drunken Politician",
"Dudley",
"Dudley",
"Dumbledore",
"Dungeon Master",
"Dust Bunny",
"Dust speck",
"Dyspeptic hamster",
"Dzhokar Tsarnaev",
"EVA Unit 01",
"EXTERMINATE! EXTERMINATE! EXTERMINATE!",
"Easss",
"Eblis",
"Edward Cullen",
"Eg",
"Eh! Steve!",
"Eidolos",
"El Pollo Diablo",
"Elbereth Engraving",
"Elvis",
"Entropy",
"Eris",
"Error from the Clone Lab",
"Evil Devil",
"Evil angel",
"Evilking",
"Exor",
"Explorington III",
"Extraordinary person",
"Eye of Sauron",
"F.O.E.",
"FOXHOUND covert operative",
"FPSRussia",
"Fat Albert",
"Fat Momma",
"Feedback",
"Fig Newton of your imagination",
"Filet-O-Fish",
"Flaming Violist",
"Flea Man",
"Fluttershy",
"Flying Jeans",
"Flying Nun",
"Flying Spaghetti Monster",
"Flying Spaghetti Monster",
"Flying Spaghetti Monster",
"Flying angry evil skull that flies upside-down when holy water is sprinkled on it",
"Foreign Host",
"Formido Oppugnatura Exsequens",
"Foul Ole Ron",
"Frankenstein",
"Fred",
"Fred Durst's disembodied head",
"Freddie Mercury",
"Freddy",
"Frog-eating surrender monkey",
"Fuzz golem",
"GEP Gun",
"Galactus",
"Galo Sengen",
"Gangsta",
"Garfield",
"Garfield the Cat",
"Garfield the President",
"George W. Bush",
"Geraldo Rivera",
"Ghost Dad",
"Ghoul Wizard",
"Giant Belly",
"Giant Hand",
"Giant Midget",
"Giant Rubix Cube",
"Giant Slobbering Piemonster",
"Giant Stick",
"Giant Tongue",
"Giant shoggoth",
"Giygas",
"Gizmo the Gremlin",
"Glinda",
"Gnome With A Wand Of Death",
"Gnome-With-The-Wand-Of-Death",
"Godzilla",
"Godzilla",
"Godzilla",
"Google monster",
"Gordon Freeman",
"Gouda Golem",
"Grammar Nazi",
"Grand master mind flayer",
"Grand master mind flayer",
"Grandma",
"Granny Weatherwax",
"Gravemind",
"Greased Scotsman",
"Greater Queen of England",
"Greebo",
"Green Flesh-Thresher",
"Grover",
"Grue",
"Gundam",
"Guymelef",
"HAL 9000",
"HURD kernel",
"Hacker",
"Hag of Bolding",
"Haggis Golem",
"Half Chewed Taxi Squasher",
"Halloween Document",
"Hallucinatory Monster",
"Hamburglar",
"Hammer Pants",
"Hanniwa",
"Happy Fun Ball",
"Harmless Protoplasm",
"Harry Potter",
"Hastur",
"Haxor",
"He-Man",
"Head Rot",
"Headcrab",
"Hell Baron",
"Here lies /usr/bin/nethack, killed by SIGSEGV.",
"Herobrine",
"Highwind Airship",
"Hildr, engraged",
"Hippopotamus",
"Holocaust",
"Holy Spirit",
"Honest Politician",
"Hong Kong Fooey",
"Humpty Dumpty",
"Hyperion",
"I think I see Death!",
"IFC Yipes",
"Ian-Keith on crack",
"Idiotic nerd that looks like your momma",
"Ig",
"Igor",
"Integrated Data Sentient Entity",
"Inu Yasha",
"Invisible Pink Unicorn",
"Iolo",
"It",
"It explodes!",
"Its your FATHER!",
"Its your MOTHER!",
"J. R. 'Bob' Dobbs",
"J.R.R. Tolkien",
"Jackie Chan",
"James Bond",
"Jason",
"Jean-Luc Picard",
"Jean-Paul Sartre",
"Jebus",
"Jerry Garcia",
"Jesus H. Christ",
"Jigglypuff",
"Jimmy Hoffa",
"John Madden",
"Johnny Depp",
"Juba the Sniper",
"Jubilex",
"Jumping brain",
"KOMPRESSOR",
"Kaster Maen",
"Katniss Everdeen",
"Kedama",
"Kermit the Frog",
"Ketchup Golem",
"Kibo",
"Killer Beatle",
"Killer Rabbit",
"Killer ant",
"Killer tripe ration",
"Kilrathi",
"King Kong",
"King Prawn",
"King Richard III",
"King of All Cosmos",
"Kirby <('_')>",
"Kit-Kat bar",
"Knob Goblin",
"Kounosuke Kuri",
"Kraid",
"Kung Fu Jesus",
"Kwyjibo",
"L dressed as Darth Vader, sans helmet, wielding a red lightsaber",
"Lady GaGa",
"Lady of the Lake",
"Lara Croft",
"Large Marge",
"Lavos",
"LeChuck",
"Legendary black beast of Arrrgh",
"Leonard Bernstein",
"Lex Luther",
"Life",
"Living Door",
"Long worm bug",
"Lorax",
"Lord British",
"Lord Goda",
"Lucius Malfoy",
"Luigi",
"M. C. Escher",
"M. Drew Streib",
"MAH LAZOR!",
"Magical Trevor",
"Magically animated vorpal blade",
"Magikarp",
"Mail Daemon",
"Major Victory",
"Malaclypse the Younger, Omnibenevolent Polyfather of Virginity in Gold",
"Male Daemon",
"Mammon",
"Mani Mani",
"Marduk/Merodach",
"Marilyn Monroe",
"Mario",
"Marmie",
"Martha Stewart",
"Martian jellymould",
"Mary-Kate and Ashely Olson",
"Master Chief",
"Master Light Wings Close Range Support Cruel Battle Machine Evaccania DOOM",
"Maud",
"Mayor and his three Daughters",
"Mecha-Streisand",
"Mega Man",
"Megadeus",
"Megatron",
"Mephistopheles",
"Metaknight",
"Metric stormtrooper",
"Mewtwo",
"Michael Jackson",
"Mickey Mouse",
"Microsoft Windows",
"Mike Nelson",
"Miku Hatsune",
"Misery",
"Miss Marple",
"Missingno",
"Mithos Yggdrasil",
"Mobile Suit",
"Moloch",
"Monkey Woman",
"Monkey-man of Delhi",
"Monster-with-a-petrifying-gaze",
"Mooninite",
"Morgoth",
"Most Interesting Man in the World",
"Mother-in-law",
"Mr Rogers",
"Mr. Blobby",
"Mr. Friend",
"Mr. Potato Head",
"Mr. T",
"Mudkip",
"Mumm-Ra",
"Mumm-Ra",
"Mushroom Giant",
"Mustard Golem",
"My Pet Monster!",
"NS13",
"Naughty Sorceress",
"Navi",
"Necromancer",
"Neil Diamond",
"Nelson Munce",
"NetHack player",
"New Age Retro Hippy",
"New Age Retro Hippy",
"Newtoghu",
"Nigerian prince Fela Kanye Okonma",
"Ninja Panda",
"Ninjapiratezombierobot",
"Nobody",
"Nuclear Bomb",
"Nyan Cat",
"Nyarlathotep",
"Og",
"Og",
"Old Scratch",
"Olly Jolly Puffball",
"Omar Khayyam Ravenhurst, K.S.C.",
"Omicronian",
"One-winged two wing bird",
"Oprah",
"Optimus Prime",
"Oracle publicitary blimp",
"Original Bubs",
"PK",
"Panty & Stocking",
"Pantyhose Golem",
"Papa Legba",
"Papa Smurf",
"Paxed",
"Penis Dragon",
"Personal Trainer",
"Peter Griffin",
"Peter Piper",
"Ph.D. Student",
"Phoenix Wright, Ace Attorney",
"Picasso",
"Pikachu",
"Pinhead",
"Pinkie Pie",
"Pit Pat",
"Poly gone",
"Polymorphic Virus",
"Polyself bug",
"Poppler",
"Porn Golem",
"Possessed waffle iron",
"Potted Plant",
"Power Rangers",
"Preacher man wants to save your soul",
"Prince",
"Probot",
"Professor Genki",
"Professor Snape",
"Psychadelic Eyeball",
"Pudding Farmer",
"Pyramid Head",
"Pyramidhead",
"Quote",
"RNG",
"RNG",
"RNG",
"Rabite",
"Racecar Bob and Bob Racecar",
"Rainbow Dash",
"Rakanishu wielding a MAC-10",
"Rancor Monster",
"Random Number God",
"Rarity",
"Rast",
"Ravenous Bugblatter Beast of Traal",
"Raymond Luxury Yacht",
"RedMachineD",
"Republican",
"Reshiram",
"Revan's Mom",
"Rhakkus",
"Richard Stallman",
"Rick James",
"Ridley",
"Robert, Wizard of Trebor",
"Robotic Kraken",
"Rodent of an Unusual Size",
"Rodney",
"Rodney",
"Rogue AI",
"Ron Jeremy",
"Ronald McDonald",
"Royal jelly",
"Rubicante",
"Ruby Weapon",
"SARS-in-a-can",
"SHODAN",
"Saddam Hussein",
"Saiyan",
"Sally Bowles",
"Sam the One-Eyed Marketeer",
"Samurai named Ken",
"Santa Claus",
"Sauron",
"Sauron",
"Scarmliogne",
"Schr�dinger's cat",
"Scooby-Doo",
"Screaming Heebie-Jeebie",
"Scuzzlebutt",
"Senor Cardgage",
"Sentiant cheese sandwhich",
"Sergeant Schlock",
"Server",
"Shaggy",
"Shambler (Quake)",
"Shnardlewonk",
"Short worm",
"Shredder",
"Shroedinger's Cat",
"Shy Guy",
"Sigmund",
"Silent Bob",
"Silly-String golem",
"Sim@",
"Singing and dancing frog",
"Sister Mary Loquacious of the Chattering Order of Satanic Nuns",
"Skeletor",
"Skeletor",
"Skeletor",
"Skilled mind flayer",
"Skilled mind flayer",
"Slashdot Troll",
"Slenderman",
"Slenderman",
"Slenderman",
"Slim Shady",
"Slime Maul",
"Slime Maul",
"Slut",
"Small minotaur",
"Snoopy",
"Snowager",
"Sonic The Hedgehog",
"Soviet Russia",
"Spaceman Spiff",
"Spaghetti Giant",
"Spaghetti Monster",
"Speedrunner",
"Spellbook of wishing",
"Spider Pig",
"Spiderman",
"Stephen Colbert",
"Steve Jobs",
"Strong Bad",
"Struttin' Evil Mushroom",
"Stupendous Man",
"SubGenius",
"Super Mario",
"Super-Saiyan",
"Supercaptaincoolman",
"Superman",
"Surfshack Tito",
"Swine flu",
"Sylvester Stallone",
"TAEB",
"Taco Giant",
"Takakazu Abe",
"Talking Paperclip",
"Teen Titan",
"Teracotta Warrior",
"The Anti-Butler",
"The Big Cheese",
"The Bugblatter Beast of Traal",
"The Creeper",
"The Creeper (Jeepers Creepers)",
"The Curse of Jenni",
"The Destroyer of Levels",
"The Flying Spaghetti Monster",
"The Fonz",
"The Grateful Dead",
"The Grimace",
"The Last Unicorn",
"The Pink invisible Unicorn",
"The President",
"The RNG",
"The Real Slim Shady",
"The Red Sox",
"The Savage Decider",
"The Sound of Silence",
"The System",
"The Theory of Relativity",
"The Undertaker",
"The United States",
"The Zombie of Steve Irwin",
"The one-eyed flesh python",
"Thin air",
"Thing That Should Not Be",
"Thomas Biskup",
"Thursday Next",
"Tinkerbell",
"Tinky-Winky",
"Toilet Duck",
"Tom Nook",
"Tom Servo",
"Tonberry",
"Toroko",
"Toto",
"Trekkie Monster",
"Trogdor the Burninator",
"Turmaculus",
"Turmaculus",
"Turok-Han",
"Tutu-wearing Sarevok",
"Twilight Sparkle",
"UboaAAAAAAAAAAA~!",
"Umpah Lumpah",
"Uncle Henry",
"Undead Core",
"Undead angel",
"Underwater Basket Weaving Kobold",
"Unladen swallow",
"Unskilled mind flayer",
"Unskilled mind flayer",
"Upperclass Twit of the Year",
"Urist McDwarf",
"Ursa Major",
"Vaas Montenegro",
"Van Darkholme",
"Vault Dweller",
"Velma",
"Violist",
"Viper on his bike, dual-wielding chains",
"Vlad the Impala",
"Voldemort",
"Voltron",
"Vore (Quake)",
"Vrock Samson",
"W**dch*ck",
"Waffle",
"Waiter waiting for a #tip",
"Walpurgis Night",
"Wangsta",
"Weapons of Mass Destruction",
"Were-skunk",
"Wes Craven",
"When we introduced Eg to the giant, he misunderstood.",
"Whiteface Charcoalpants",
"Whiteface Charcoalpants",
"Whiteface Charcoalpants",
"Witchalok",
"Wizard of Yendoor",
"Wolfoid",
"Wombat",
"Wormtongue, Agent of Saruman",
"Xena",
"Xena, Warrior Princess",
"Xenu",
"Xivilai",
"Yendor",
"Yog-Sothoth",
"YouBane",
"Your Dad On A Sybian",
"Your Mom",
"Yourself",
"Yourself",
"Ysalamir",
"Yukari Yakumo",
"Zadir the Washer",
"Zaku",
"Zamboni",
"Zeeky H. Bomb",
"Zekrom",
"Zeruel",
"Zorn's lemma",
"a FNORD",
"a SPAM monster",
"a black dragon with Snoopy's head",
"a dentist",
"a flock of Bird Flu chickens",
"a garlic monster",
"a giant crab",
"a giant octopus",
"a mob of PETA activists",
"a one-winged dewinged stab-bat",
"a plain gold ring",
"a plastic surgeon",
"a praying mantis",
"a squee",
"a team of Caballists",
"a tree full of monkeys on nitrous oxide",
"a vision of Ragnar�k",
"a whark",
"a winged serpent",
"a ytram",
"aardvark",
"absurd pop-culture reference",
"abyss",
"acid blog",
"africanized bee",
"albatross",
"alien",
"alien space bat",
"alluring being",
"aluminum elemental",
"ambulatory fortune cookie",
"amoeba",
"amusing movie reference",
"an Anger Management counsellor",
"an angry Madonna",
"an italian grandmother",
"an sandwich",
"ancient karmic dragon",
"angry Norwegian",
"angry mail demon",
"angry mariachi",
"antagonist",
"anteater",
"anticoffee",
"antimony elemental",
"arch-lichen",
"arch-pedant",
"archie the cockroach",
"argon elemental",
"armed bear",
"arsenic elemental",
"artila",
"astatine elemental",
"astral snake",
"astral wyvern",
"attack chopper",
"badger",
"baleen whale",
"bandersnatch",
"banshee",
"barium elemental",
"barrel goat",
"battle alpha",
"battle beta",
"battle bunny",
"battle mech",
"bee of the bird of the moth",
"beer-can golem",
"beholder",
"berdache with a bardiche",
"beryllium elemental",
"big red button",
"bismuth elemental",
"black hole",
"black smoke monster",
"black tapioca pudding",
"blargian snagglebeast",
"blood-spurting onion",
"blue light",
"blue whale",
"bluebird of happiness",
"bonta-kun",
"bonzai bush",
"boron elemental",
"bottle of Heinz Tomato Ketchup",
"bouncing eye",
"bowl of petunias",
"box ghost",
"boyfriend",
"broken clock",
"bromine elemental",
"brontosaurus",
"brony",
"brown dragon",
"buffer overflow",
"bugs on the floor",
"bungo pony",
"butler",
"cacodaemon",
"cadmium elemental",
"calcium elemental",
"candied kobold",
"capital I",
"carbon elemental",
"carbosilicate amorph",
"cardboard golem",
"casey",
"cesium elemental",
"chaos demon",
"cheese golem",
"chess pawn",
"chibi",
"chicken",
"chihuahua",
"chilli elemental",
"chlorine elemental",
"chocolate golem",
"chocolate pudding",
"chocolate pudding",
"christmas elf",
"chromium elemental",
"chupacabra",
"chupacabras",
"circus freak",
"clawbug",
"cobalt elemental",
"code fault",
"coelacanth",
"collapsed mine golem",
"colorless green idea",
"commissar",
"contrabassoon",
"convention furry",
"conventioneer",
"cookie monster",
"copper elemental",
"corpulent porpoise",
"cossack",
"cow",
"cow beneath the sea",
"crap fodder",
"crazy bastard",
"crazy bastard",
"crocodile-dragon Tharagavverug",
"cryoa",
"cryodrayk",
"cubist kobold",
"cursed amulet of strangulation",
"dancing Savior, Jesus Christ Superstar,",
"dancing blade",
"dangling pointer",
"dark archon",
"darkness owl",
"database",
"dead parrot",
"death vortex",
"demonic talking skull",
"diagonally moving grid bug",
"dick in a box",
"dilithium crystal golem",
"dilithium crystal golem",
"dirty bastard",
"disillusioner",
"district attorney",
"dolphin",
"door-to-door salesman",
"dopefish",
"double-eyed cyclops",
"dragon farmer",
"drakon",
"drayk",
"dropbear",
"drunk Dragonborn",
"drunk guy",
"dryer monster",
"duct tape golem",
"dust bunny",
"dvorak beast",
"dwarven construct",
"economic crisis",
"ego-death",
"elder xorn",
"emperor lich",
"empty cell",
"enchanted food ration",
"ender-dragon",
"enderman",
"entropy",
"er... dragon? It sure looks like one...",
"evil brain",
"evil overlord",
"evil stepmothership",
"exceptionally large pair of buttocks",
"excremental",
"existential angst",
"expert mind flayer",
"expert mind flayer",
"exploding cake",
"eyebeast",
"fail rat",
"fan",
"feeling that you're being watched",
"fetus",
"figment of your imagination",
"fire monkey",
"first category perpetual motion device",
"fish",
"flaming skull",
"flash of insight",
"flatulence vortex",
"fleas",
"floating ear",
"floating nose",
"floating pair of eyes",
"floating tongue",
"floor",
"flumph",
"fluorine elemental",
"flying pig",
"fnord",
"foocubus",
"food eater",
"football",
"footrice",
"fractal fish finger tree",
"frerf",
"frog prince",
"fruit bat",
"frumious bandersnatch",
"funkateer",
"funny little man",
"furby",
"furry",
"future diary holder",
"fyora",
"gallium elemental",
"gang member",
"gaping goatse",
"gargantua",
"gazebo",
"gazer",
"gerbil",
"germanium elemental",
"ghost of christmas past",
"ghost of christmas present",
"ghost of christmas yet to come",
"giant Chibi Maruko-chan on wheels",
"giant bong",
"giant cockroach",
"giant dwarf",
"giant enemy crab",
"giant gnome",
"giant letter D",
"giant marshmallow man",
"giant mushroom",
"giant robot",
"giant squid",
"gigantic kobold",
"gimp",
"gingerbread man",
"girlfriend",
"glahk",
"glom of nit",
"gnu",
"gold elemental",
"golden goblin",
"golem made out of other golems",
"gonzo journalist",
"goomba",
"grad student",
"grass mud horse",
"gray goo",
"gray stoner",
"grease golem",
"greased pig",
"green light",
"green... no, gray dragon",
"grid bug hits! Invisible Demogorgon",
"grid feature",
"grinch",
"grue",
"guardianal",
"haggis",
"hair golem",
"half-hobbit",
"half-horse half-monkey",
"hallucination",
"hallucinatory monster",
"hallucinogen-distorted hallucination",
"hamster",
"hardware bug",
"haunted television",
"haxorus",
"head crab",
"headless M�mir",
"headless thompson gunner",
"hedge golem",
"hedgehog",
"heisenbug",
"helium elemental",
"henchman",
"hentai tentacle beast",
"herobrine",
"hipster",
"hockey elemental",
"horrible gelatinous blob",
"hotel detective",
"humorous message",
"hydrogen elemental",
"hypnosis vortex",
"icecream pooping giant taco monster",
"imaginary number",
"indescribable horror",
"indestructable monster",
"indium elemental",
"infernal seal",
"inside joke",
"inside-out bag of holding",
"invisible hand of Adam Smith",
"invisible pink unicorn",
"invisible potion of invisibility",
"invisible shiny Bulbasaur",
"iodine elemental",
"iridium elemental",
"iron elemental",
"janitor",
"jehovah's witness",
"jell-o golem",
"jolly green giant",
"jophur",
"jub-jub bird",
"juvenile delinquent",
"kangaroo",
"katamari",
"keebler elf",
"kernel bug",
"killer bunny",
"killer tomato",
"kitten prospecting robot",
"kl;kl;kl;kl",
"klingon warrior",
"koala",
"krypton elemental",
"kzin",
"laboratory mouse",
"lag monster",
"land octopus",
"land wyvern",
"large letter",
"laser cat",
"lawnmower",
"lawyer",
"lead elemental",
"left eye",
"lego brick",
"lemon",
"lemur",
"liquid nitrogen elemental",
"lithium elemental",
"little brother",
"little green man",
"living apple",
"loan shark",
"logic bug",
"lol man",
"lombax",
"long worm brain",
"lorax",
"lower-case h",
"lumberjack",
"lungfish",
"lurker below",
"machine elf",
"mad Pierson's Puppeteer",
"mad god",
"mad scientist",
"mad scientist",
"mage jackal",
"magnesium elemental",
"magnetic monopole",
"mamul",
"man in a pink tutu",
"man-eating banana",
"manbearpig",
"manbearpig",
"manganese elemental",
"marshmallow peep",
"martian flat cat",
"master baker",
"master grid bug",
"master lichen",
"mehitabel the cat",
"member of the Greate Race of Yith",
"meme monster",
"mentat",
"mercury elemental",
"metroid",
"mexican jumping beans",
"midget",
"mind vortex",
"molybdenum elemental",
"morlock",
"mothership",
"mountain dwarf",
"moving hole",
"mumak mummy",
"mummy zombie",
"munchkin",
"mutant rat",
"mysterious force",
"mysterious force",
"naughty sorceress",
"neon elemental",
"neosquid",
"net troll",
"nethaxorus",
"newsgroup troll",
"newt called Dudleyslayer",
"newt mummy",
"nickel elemental",
"nigiri monster",
"ninja pirate zombie robot",
"ninja snowman",
"nitrogen elemental",
"non-player character",
"nonesuch",
"oblivion",
"obstinate tank",
"octarine dragon",
"octarine dragon",
"octarine mold",
"oiraM repuS",
"on the succubus",
"one-eyed, one-horned, flying, purple people eater",
"opossum",
"orangutan",
"ornk",
"osmium elemental",
"osquip",
"oxygen elemental",
"paint blob",
"palladium elemental",
"parachute pants",
"parliamentarian",
"particle man",
"paxed",
"peer",
"penguin",
"peregrine falcon",
"perfume vortex",
"phoenix",
"phosphorus elemental",
"pime taradox",
"pink dinosaur",
"pink dragon",
"pink elephant",
"pirate",
"pirate ninja",
"pissed-off shopkeeper",
"pixel golem",
"pizza delivery boy",
"plaid blob",
"plaid dragon",
"plaid dragon",
"plain trained automaton",
"plated bug",
"platinum elemental",
"platypus",
"player",
"playername",
"plumper",
"plutonium elemental",
"polka-dot dragon",
"pop-culture reference",
"pop-up ad",
"pope",
"pot fiend",
"potassium elemental",
"potentate of amnesia",
"potted plant",
"poultrygeist",
"power armor Hitler",
"pr0n prawn",
"prairie dog",
"president of the United States",
"primal white jelly",
"prismatic dragon",
"prissy, two-faced, backstabbing Templar whore",
"professional wrestler",
"programmer",
"psychedelic beetle",
"psycho teddy bear",
"puppeteer",
"purple dinosaur",
"purple haze",
"purple unicorn",
"pyrex golem",
"pyroroamer",
"qwerty beast",
"rabid lawyer",
"rabid squirrel",
"radium elemental",
"radon elemental",
"raging nerd",
"rakshasha",
"random monster",
"random number generator",
"random number golem",
"rare nymph",
"rat king",
"realization of your life's true meaning",
"really, really, really big dragon",
"red D",
"red d",
"red light",
"redditor",
"redneck tree",
"repo man",
"republican",
"rhodium elemental",
"rice pudding",
"right eye",
"road runner",
"roamer",
"robot finding stegosaurus",
"roomba",
"roostertrice",
"rotdhizon",
"rotghroth",
"rubber baby buggy bumper",
"rubber chicken",
"rubber ducky",
"rubber golem",
"rubidium elemental",
"saiph",
"samoan attorney",
"sauceror",
"scaly, no-feather-bullshit, kosher raptor",
"scandium elemental",
"scary clown",
"scary devil monk",
"scrag",
"screaming dizbuster",
"scroll of genocide",
"sea cucumber",
"sea orc",
"sea wyvern",
"searing artila",
"second law of thermodynamics",
"segmentation fault",
"selenium elemental",
"seven-headed, fire-breathing $dog",
"shawnz",
"sheep",
"shimmering $playername",
"shimmering dragon",
"shinigami",
"shoggoth",
"shoggoth",
"shrubbery",
"silicon elemental",
"silly in-joke",
"silver elemental",
"silver saber-tooth tiger",
"sinking ear",
"sky wyvern",
"sleaze elemental",
"sleestak",
"slime mold",
"slow loris",
"small letter",
"smurf",
"snark",
"snow golem",
"snow goon",
"social worker",
"sodium elemental",
"sodium elemental",
"space-time anomaly",
"spam dragon",
"spammer",
"spelling mistake",
"spleling eror elmental",
"spoiler bug",
"spotted dragon",
"spycrab",
"star-bellied sneetch",
"star-nosed mole",
"starman",
"stasis vortex",
"stdev",
"stick figure",
"storm of buttered footballs",
"stormtrooper",
"strawberry jelly",
"street preacher",
"strontium elemental",
"strontium elemental",
"stuffed raccoon puppet",
"styrofoam elemental",
"succutrice",
"sulfur elemental",
"sumatran rat-monkey",
"super grue",
"surfer dude",
"sysadmin",
"system administrator",
"tapeworm",
"taxman",
"teal deer",
"technetium elemental",
"telemarketer",
"tellurium elemental",
"terror vlish",
"thahd",
"thahd shade",
"the Candyman",
"the Chupacabra",
"the Creeping Terror",
"the DevTeam",
"the F.O.E.",
"the Goblin King",
"the Godfather",
"the Incredible Hulk's weight in bees",
"the Internet",
"the Joker",
"the Loch Ness monster",
"the ROFLchopter",
"the ROFLcopter",
"the Roomba",
"the Scarecrow",
"the Spice Girls",
"the Teenage Mutant Ninja Turtles",
"the Terminator",
"the Three Stooges",
"the White Witch",
"the Wicked Witch of the East",
"the Wicked Witch of the West",
"the Wizard of Oz",
"the Wizard of Yeldud",
"the blessed +5 Sword of Damocles",
"the cyberdemon",
"the darkness",
"the embodiment of life, death and everything inbetween",
"the ghost of Tupac Shakur",
"the goddess Discordia",
"the goddess Eris",
"the hellbride",
"the invisible Demogorgon",
"the invisible cockatrice",
"the invisible werewaiter",
"the largest prime number",
"the paparazzi",
"the road",
"the roots of Yggdrasil",
"the smashing pumpkin",
"the urge that you're being watched",
"the urge to look at your hands",
"thing",
"thing",
"thing with 2.3i heads",
"thoroughly rusty thoroughly corroded iron golem",
"three weaving crones",
"three-headed cockatrice",
"three-headed monkey",
"three-headed monkey",
"thunder fiend of gain level",
"thwomp",
"timberwolf",
"time paradox",
"timey wimey ball",
"tin elemental",
"tin of gnome meat",
"titanium elemental",
"toilet paper golem",
"topiary golem",
"trapper",
"triple Gandalf",
"trippy vortex",
"trouser snake",
"truck driver",
"tube rat",
"tubgirl",
"tungsten elemental",
"turbonium dragon",
"turducken",
"twen",
"two-headed two-bodied goat",
"twonky",
"tyger",
"ultra mega battle robot",
"umpire",
"unassuming local guy",
"universe",
"unpredictably deadly pyromaniac midget",
"ur-drakon",
"ur-glahk",
"uranium elemental",
"urge to stare at your hands",
"vampire bird",
"vampire chicken",
"vampire slayer",
"vanadium elementa",
"vanadium elemental",
"velcro golem",
"velociraptor",
"vending machine",
"venture capitalist",
"vermicious knid",
"vertigo vortex",
"viking",
"violent fungus",
"violent fungus",
"visible stalker",
"vlish",
"vogon",
"voluptuous ampersand",
"vorpal jabberwock",
"vortigaunt",
"vulture",
"waddle dee",
"waddle doo",
"waddle doo",
"walking disk drive",
"walrus",
"wanderer",
"warning level 1",
"warning level 2",
"warning level 3",
"warning level 4",
"warning level 5",
"water buffalo",
"weasel",
"wee green blobbie",
"were(random beast)",
"werecar",
"weredolphin",
"werefoo",
"werehorse",
"werehouse",
"werehuman",
"weremindflayer",
"weremold",
"weremouse",
"wereplatypus",
"white rabbit",
"wiener sausage",
"wif dove",
"wight supremacist",
"willow-the-wisp",
"wombat",
"wookie",
"worthless yellowish-brown glass golem",
"writhing mass of primal chaos",
"wub",
"wumpus",
"wumpus mummy",
"xenon elemental",
"xoff",
"xon",
"xorff",
"yellowish-brown dragon",
"your ex",
"your foot",
"your inner demons",
"your mom",
"your mom",
"your own hands",
"your pet idiot",
"ysalamiri",
"yttrium elemental",
"zappy eel",
"zappy eel",
"zergling",
"zinc elemental",
"zirconium elemental",
"zmobie",
"zombi Oracle",
"zombie mummy",
"zombie robot ninja samurai pirate sorcerer gunman",
"zombie sasquatch",
"zombie zombie",
"zombified tarantulas",
"zorkmid",
"a series of disconected lines", /* nondescript*/ /*DnD*/ "a cerulean weeping-willow", /* it's magic. Unlike the others, this one works. Keep in sync with engrave.h!*/ /*Special behavior, these move across the floor, keep in sync with allmain.c*/ "a north-east facing glider", "a north-west facing glider", "a south-west facing glider", "a south-east facing glider", "a square" /* books */ "a set of holy horns", "a Summoning Dark mine-sign", "a Long Dark mine-sign", "a Following Dark mine-sign", "a Closing Dark mine-sign", "an Opening Dark mine-sign", "a Breathing Dark mine-sign", "a Speaking Dark mine-sign", "a Catching Dark mine-sign", "a Secret Dark mine-sign", "a Calling Dark mine-sign", "a Waiting Dark mine-sign", "a florid crest dominated by a double-headed bat", "a Guarding Dark mine-sign", "the mark of the Caller of Eight", /* Discworld */ "a lidless eye", /* Lord of the Rings */ "a white tree", /* Gondor, Lord of the Rings */ "a triangle enclosing a circle and bisected by a line", /* Harry Potter */ "a set of three trefoils, the lower most inverted", /* describes the three of clubs. Too Many Magicians*/ "a Trump of Doom", "a Sign of Chaos", "a Pattern of Amber", "a Ghostwheel", "a Court symbol", "a Forest symbol", "the sign of the Wandering Eye", /* Gunnerkrigg Court */ /* Not quite */ "a heptagenarian", "an octogram", "a pentagrain", "a circle of da Vinci", "a hand making a rude gesture", "a junior sign", "a childish compound eye", "a Sign of an Illegitimate Step-daughter", "a cenotaph of a catgirl", "a groovy rendition of the wings of Gargula", "a Robotech Defense Force insignia", /*...Robotech*/ "a Black Knights insignia", /* Code Geass */ "an inverted triangle flanked by seven eyes", /* NGE */ "a laughing man", /* Ghost in the Shell */ "an alchemic array", "a human transmutation circle", /* Fullmetal Alchemist */ "a triangle composed of three smaller triangles", "an eye and single tear", "a circle enclosing four swirling lines", "a flame inside a circle", "a snowflake within a circle", "an inverted triangle with a dot above each face, enclosed by a circle", "a sign resembling an eyeless yin-yang", "a circle surrounding a triangle of dots and another of triangels",/*Zelda*/ "a setting (rising?) sun", /* Dresden Codak */ "an asymmetric, stylized arrowhead, point upwards", /* Star Trek*/ "a set of three blades, the top blade straight, the dexter curved down, the sinister curved up", "a Sharuan Mindharp", /* Star Wars expanded universe */ "a winged blade of light", /* Jedi Order symbol */ "an angular S before a segmented circle",/*a screw attack symbol*/ "more dakka", "a symbol of pain", /* DnD */ /* Planescape */ "a mimir", "a symbol of torment", "a circle enclosing two colliding arrows", "a pair of triangles, drawn tip to tip,", "a stylized beast", "a triangle crowned by a single line", "a simple image of many mountains", "a sketch of a shining diamond", "a tree-rune", "an eight-toothed gear", "a random scribble", "a square with two small handles on opposite sides", "a square enclosing a spiral", "an eye with three inverted crosses", "an infinity symbol crossed by a burning downwards arrow", "a set of four nested triangles", "a watchful eye blocking an upward arrow", "a pitchfork stabbing the ground", /* Zodiac */ "an Aries sign", "a Taurus sign", "a Gemini sign", "a Cancer sign", "a Leo sign", "a Virgo sign", "a Libra sign", "a Scorpio sign", "a Sagittarius sign", "a Capricorn sign", "an Aquarius sign", "a Pisces sign", "a heart pierced through with an arrow", "a broken heart", "a skull and crossed bones", "a bad situation", "a zorkmid", "a diagram of the bridges of Konigsberg", "a hand-mirror of Aphrodite", "a shield and spear of Ares", /* alchemy/male/female */ "a black moon lilith sign", "a window", /* op-sys*/ "a no symbol", "a test pattern", "a work of modern art", "a flag of Neverland", "a hyped-up duck dressed in a sailor's shirt and hat", /* Disney */ "a mouse with 2d ears", "a set of three circles in the shape of a mouse's head", "a meaningless coincidence", /*Corporate Logos*/ "a stylized, fan-shaped seashell", "a bitten apple", "a pair of arches meeting to form an \"M\"", "a Swoosh mark", "a set of five interlocked rings", /*Olympics logo*/ "a running man", /* Exit */ "a running man holding a cane", "a one-and-zero", /* Power toggle */ "a thick soup of mist", "a pattern of squared circles", "a void", "a notable lack of images", "a stark absence of pictures", "nothing much", "a convergence of parallel lines", "a sphere", /* How did you manage that? */ "a yin-yang", "a taijitu",/* Taoist */ "a hand of Eris", /* Discordian */ "an ichthus", "a Cross", /* Christian*/ "a wheel with eight spokes", /* Budhism */ "a fish with legs", "a fat fish", "a fish with tentacles, legs, and wings", /* ichthus parodies/derivitives: darwin, buddha, and Cthulhu. */ "a set of seven concentric circles", "a left-handed trefoil knot", "a triskelion", /* Ancient Symbol */ "a rough circle enclosing an A", /* Anarchy */ "a Tree of Life", /* Kabbalah */ "a winged oak", "a wheel cross", "a labyrinth", "sign of Shamash", "a naudh rune", /* misery */ "an Eye of Providence", "a pyramid surmounted by an eye", /* Christian */ "a one-way staircase", "an 'a' encircled by its own tail" /* meta */ ,

	"robber", "homie", "motherfucker", "mofo", "hell bride", "bitch", "hard motherfucker", "hard mofo", "slut with syphilis", "lustful girl", "quick learner", "supersmart woman", 
"Wolf", "Big Bear", "Ryu", "Tacitus", "Urbaldi", "Pete", "Lex", "JoJo", "Jyllia", "Sabrina", "Sabine", "Yvara", "Lenka",
"Evita", "Liebea", "Denshi Gasu", "Mr. Black", "Tiger's Claw", "Katzou", "Mohmar Deathstrike", "Ingo", "Septimus",
"Isolde", "Elli", "Vilja", "Sunija", "Rhea", "Jasmin", "Erosina", "Irmina", "Martius", "Faster-Than-All-Others",
"Senator Antius", "H.", "Pokoh", "Davide", "Aee", "Melirija", "Larissa", "Sysette", "Miss Haskill", "Elenya",
"Golden Mary", "Lara", "Sandrina", "Doctor Maex", "Marc", "Arno", "Hailbush", "Romann", "Siegfried", "Roy", "Tonilia",
"Claire", "Lumia", "Lahira", "Estrella", "Maricia", "Sontaire", "Marje",

"Jill", "Trycja", "Kersey", "Sally", "Hannya", "Svantje", "Jynnifyr", "Elke", "Rinka", "Nicoletta", "Betti", "Ina", "Heikipa", "Jora", "Maitine", "Esruth", "Verene", "Lousie", "G-cheater", "Irinella", "Bastian", "Amandina", "Lillie", "Nicyan", "Leodoch", "Mirella", "Queelix", "Fisoa", "Suesska", "Ann", "Nurisha", "Desiree", "Birgit",

"Elsbeth", "Lamy", "Lissie", "Arabella", "Anastasia", "Henrietta", "Katrin", "Jana", "Aniya", "Yasni", "Almina", "Xeni", "Mirri", "Eleanor", "Kirja", "Inge", "Helli", "Lucia", "Viktorija", "Simona", "Natalyana", "Krista", "Nellina", "Raidara", "Vera", "Noko", "Jasajeen", "Marika", "Miesael", "Merbek", "Marianna", "Sinja", "Rodotha", "Natinya", "Honno", "Aline", "Michaela", "Robin", "JNR", "Lars", "Mare", "Noenoe", "Tschulia", "Lea", "Tommy", "Sarah",

"Giglio", "Charravalga", "Fridrika", "Great Jaguar Claw", "Lynette", "Kastortransport", "Celina", "Irya", "Mariya", "Wendy", "Katia", "Tanja", "Vanessa", "Anne", "Lena", "Jeanetta", "Rungud", "Melissa", "Everella", "Madeleine", "Anita", "Nina", "Natascha", "Manola", "Larry", "Morton", "Iggy", "Lemmy", "Ludwig", "Oberdan", "Len-kind", "Litta", "Ilie", "Kiwi", "Maja", "Till", "Tomas", "Natalje", "Little Marie", "Nikolob", "Tillbull",

       "blessed greased +5 silly object of hilarity",
       /* Modern */
       "polo mallet",
       "string vest",
       "YAFM",                             /* rgrn */
       "applied theology textbook",        /* AFutD */
       "handbag",
       "onion ring",
       "tuxedo",
       "breath mint",
       "potion of antacid",
       "traffic cone",
       "chainsaw",
       "pair of high-heeled stilettos",    /* the *other* stiletto */

       /* Silly */
       "left-handed iron chain",
       "holy hand grenade",                /* Monty Python */
       "decoder ring",
       "amulet of huge gold chains",       /* Foo' */
       "rubber Marduk"
       "unicron horn",                     /* Transformers */
       "holy grail",                       /* Monty Python */
       "chainmail bikini",
       "first class one-way ticket to Albuquerque", /* Weird Al */
       "yellow spandex dragon scale mail", /* X-Men */

       /* Musical Instruments */
       "grand piano",
       "two slightly sampled electric eels", /* Oldfield */
       "kick drum",                        /* 303 */
       "tooled airhorn",

       /* Pop Culture */
       "flux capacitor",                   /* BTTF */
       "Walther PPK",                      /* Bond */
       "hanging chad",                     /* US Election 2000 */
       "99 red balloons",                  /* 80s */
       "pincers of peril",                 /* Goonies */
       "ring of schwartz",                 /* Spaceballs */
       "signed copy of Diaspora",          /* Greg Egan */
       "the missing evidence in the Kelner case", /* Naked Gun */
       "blessed +9 helm of Des Lynam",     /* Bottom */

       /* compile with -DBRITHACK for British goodness */
       "bum bag",
       "blessed tin of marmite",
       "tesco value potion",
       "ringtone of drawbridge opening",
       "burberry cap",
       "potion of bitter",
       "cursed -2 bargain plane ticket to Ibiza",
       "black pudding corpse",
       /* Fantasy */
       "Necronomicon",                     /* Lovecraft */
       "pipe weed",                        /* LOTR */
       "knife missile",                    /* Iain M. Banks */
       "large gem",                        /* Valhalla */
       "monster manual",                   /* D&D */
       "spellbook called Octavo",          /* Discworld */
       "ring of power",                    /* LOTR */
       "lightsaber",
       "no tea",                           /* HGttG game */
       "pan-galactic gargle blaster",      /* HGttG */
       "silmaril",                         /* LOTR */
       "pentagram of protection",          /* Quake */

       /* Geekery */
       "AAA chipset",                      /* Amiga */
       "thoroughly used copy of Nethack for Dummies",
       "named pipe",                       /* UNIX */
       "kernel trap",
       "copy of nethack 3.4.4",            /* recursion... */
       "cursed smooth manifold",           /* Topology */
       "vi clone",
       "maximally subsentient emacs mode",
       "bongard diagram",                  /* Intelligence test */

       /* Historical */
       "dead sea scroll",
       "cat o'nine tails",
       "pieces of eight",
       "codpiece",
       "straight-jacket",
       "bayonet",
       "iron maiden",
       "oubliette",
       "pestle and mortar"
       "plowshare",

       /* Mashups */
       "potion of score doubling",
       "scroll labelled ED AWK YACC",      /* the standard scroll */
       "scroll labelled RTFM",
       "scroll labelled KLAATU BARADA NIKTO", /* Evil Dead 3 */
       "scroll of omniscience",
       "scroll of mash keyboard",
       "scroll of RNG taming",
       "scroll of fungicide",
       "helm of telemetry",
       "blue suede boots of charisma",
       "cubic zirconium",
       "amulet of instadeath",
       "amulet of bad luck",
       "amulet of refraction",
       "potion of rebigulation",           /* Simpsons */
       "O-ring",
       "wand of washing",
       "ring named Frost Band",
       "expensive exact replica of the Amulet of Yendor",
       "giant beatle",
       "lodestone",
       "rubber chicken",                   /* c corpse */
       "tin of Player meat",
       "figurine of a god",
       "tin of whoop ass",
       "cursed -3 earring of adornment",
       "wisdom boots",
       "ornamental cape",
       "acid blob skeleton",
       "brand new, all time lowest introductory rate special offer",
       "dirty rag",

};

/* Return a random monster name, for hallucination.
 * KNOWN BUG: May be a proper name (Godzilla, Barney), may not
 * (the Terminator, a Dalek).  There's no elegant way to deal
 * with this without radically modifying the calling functions.
 */
const char *
rndmonnam()
{
	int name;

	/*do {*/
	    name = rn1(NUMMONS + SIZE(bogusmons) - LOW_PM, LOW_PM);
	/*} while (name < NUMMONS &&
	    (type_is_pname(&mons[name]) || (mons[name].geno & G_NOGEN)));*/
/* All monster names should be possible, even unique and genocided ones. This adds more variety. --Amy */

	if (name >= NUMMONS) return bogusmons[name - NUMMONS];
	return mons[name].mname;
}

#ifdef REINCARNATION
const char *
roguename() /* Name of a Rogue player */
{
	char *i, *opts;

	if ((opts = nh_getenv("ROGUEOPTS")) != 0) {
		for (i = opts; *i; i++)
			if (!strncmp("name=",i,5)) {
				char *j;
				if ((j = index(i+5,',')) != 0)
					*j = (char)0;
				return i+5;
			}
	}
	return rn2(3) ? (rn2(2) ? "Michael Toy" : "Kenneth Arnold")
		: "Glenn Wichman";
}
#endif /* REINCARNATION */
#endif /* OVLB */

#ifdef OVL2

static NEARDATA const char * const hcolors[] = {
	"ultraviolet", "infrared", "bluish-orange",
	"reddish-green", "dark white", "light black", "sky blue-pink",
	"salty", "sweet", "sour", "bitter", "umami",
	"striped", "spiral", "swirly", "plaid", "checkered", "argyle",
	"paisley", "blotchy", "guernsey-spotted", "polka-dotted",
	"square", "round", "triangular", "octarine",
	"cabernet", "sangria", "fuchsia", "wisteria",
	"lemon-lime", "strawberry-banana", "peppermint",
	"romantic", "incandescent", "multicolored"
};

const char *
hcolor(colorpref)
const char *colorpref;
{
	return (Hallucination || !colorpref) ?
		hcolors[rn2(SIZE(hcolors))] : colorpref;
}

/* return a random real color unless hallucinating */
const char *
rndcolor()
{
	int k = rn2(CLR_MAX);
	return Hallucination ? hcolor((char *)0) : (k == NO_COLOR) ?
		"colorless" : c_obj_colors[k];
}

/* Aliases for road-runner nemesis
 */
static const char * const coynames[] = {
	"Carnivorous Vulgaris","Road-Runnerus Digestus",
	"Eatibus Anythingus"  ,"Famishus-Famishus",
	"Eatibus Almost Anythingus","Eatius Birdius",
	"Famishius Fantasticus","Eternalii Famishiis",
	"Famishus Vulgarus","Famishius Vulgaris Ingeniusi",
	"Eatius-Slobbius","Hardheadipus Oedipus",
	"Carnivorous Slobbius","Hard-Headipus Ravenus",
	"Evereadii Eatibus","Apetitius Giganticus",
	"Hungrii Flea-Bagius","Overconfidentii Vulgaris",
	"Caninus Nervous Rex","Grotesques Appetitus",
	"Nemesis Riduclii","Canis latrans"
};

char *
coyotename(mtmp, buf)
struct monst *mtmp;
char *buf;
{
    if (mtmp && buf) {
	Sprintf(buf, "%s - %s",
	    x_monnam(mtmp, ARTICLE_NONE, (char *)0, 0, TRUE),
	    mtmp->mcan ? coynames[SIZE(coynames)-1] : coynames[rn2(SIZE(coynames)-1)]);
    }
    return buf;
}
#endif /* OVL2 */

/*do_name.c*/
