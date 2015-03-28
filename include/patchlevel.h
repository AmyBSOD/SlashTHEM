/*	SCCS Id: @(#)patchlevel.h	3.4	2003/12/06	*/
/* Copyright (c) Stichting Mathematisch Centrum, Amsterdam, 1985. */
/* NetHack may be freely redistributed.  See license for details. */

/*The name of the compiled game- should be same as stuff in makefile*/
/*for makedefs*/
/* KMH -- Made it mixed case, from which upper & lower case versions are made */
#ifdef WIN32 /* Workaround for reading the config on unix systems */
#define DEF_GAME_NAME   "SlashEM Extended"
#else
#define DEF_GAME_NAME   "SlashEM_Extended"
#endif
/*#define DEF_GAME_NAME   "NetHack"*/

/* Version */
#define VERSION_MAJOR   0
#define VERSION_MINOR   4
/*
 * PATCHLEVEL is updated for each release.
 */
#define PATCHLEVEL      0
#define EDITLEVEL	0
/*#define FIXLEVEL        0*/

#define COPYRIGHT_BANNER_A \
"This is SuperLotsoAddedStuffHack-Extended Magic 1997-2005"

#define COPYRIGHT_BANNER_B \
"NetHack, Copyright 1985-2003 Stichting Mathematisch Centrum, M. Stephenson."

#define COPYRIGHT_BANNER_C \
"Slash'EM Extended version by Amy (user Bluescreenofdeath at Nethackwiki)."

#define COPYRIGHT_BANNER_D \
"See license for details. Bug reports to Bluescreenofdeath's user talk page."

#if 1
/*
 * If two or more successive releases have compatible data files, define
 * this with the version number of the oldest such release so that the
 * new release will accept old save and bones files.  The format is
 *	0xMMmmPPeeL
 * 0x = literal prefix "0x", MM = major version, mm = minor version,
 * PP = patch level, ee = edit level, L = literal suffix "L",
 * with all four numbers specified as two hexadecimal digits.
 */
#define VERSION_COMPATIBILITY 0x00040000L
#endif

/*patchlevel.h*/
