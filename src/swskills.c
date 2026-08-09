/***************************************************************************
*                           STAR WARS REALITY 1.0                          *
*--------------------------------------------------------------------------*
* Star Wars Reality Code Additions and changes from the Smaug Code         *
* copyright (c) 1997 by Sean Cooper                                        *
* -------------------------------------------------------------------------*
* Starwars and Starwars Names copyright(c) Lucasfilm Ltd.                  *
*--------------------------------------------------------------------------*
* SMAUG 1.0 (C) 1994, 1995, 1996 by Derek Snider                           *
* SMAUG code team: Thoric, Altrag, Blodkai, Narn, Haus,                    *
* Scryn, Rennard, Swordbearer, Gorog, Grishnakh and Tricops                *
* ------------------------------------------------------------------------ *
* Merc 2.1 Diku Mud improvments copyright (C) 1992, 1993 by Michael        *
* Chastain, Michael Quan, and Mitchell Tse.                                *
* Original Diku Mud copyright (C) 1990, 1991 by Sebastian Hammer,          *
* Michael Seifert, Hans Henrik St{rfeldt, Tom Madsen, and Katja Nyboe.     *
* ------------------------------------------------------------------------ *
*		   New Star Wars Skills Unit    			   *   
****************************************************************************/

#include <math.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mud.h"

void add_reinforcements( CHAR_DATA * ch );
ch_ret one_hit( CHAR_DATA * ch, CHAR_DATA * victim, int dt );
int xp_compute( CHAR_DATA * ch, CHAR_DATA * victim );
int ris_save( CHAR_DATA * ch, int schance, int ris );
CHAR_DATA *get_char_room_mp( CHAR_DATA * ch, const char *argument );

extern int top_affect;

/*
 * do_makeblade is intentionally not registered as a player command
 * (v1.18) - it only crafts the intermediate vibro-blade component;
 * make_vibrosword already covers the weapon players actually want.
 */
void do_makeblade( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance, charge = 0;
   int strengthmin = 0, strengthmax = 0;
   bool checktool, checkdura, checkbatt, checkoven;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;
   AFFECT_DATA *paf;
   AFFECT_DATA *paf2;
   AFFECT_DATA *paf3;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makeblade <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkbatt = FALSE;
         checkoven = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;

            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need toolkit to make a vibro-blade.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make it out of.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for your blade.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to heat the metal.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeblade] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of crafting a vibroblade.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 10, do_makeblade, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 15, do_makeblade, 1 );
            else add_timer( ch, TIMER_DO_FUN, 25, do_makeblade, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makeblade );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeblade] );
   vnum = 10422;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkbatt = FALSE;
   checkoven = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      /*
       * Durasteel quality matters here (v1.18, confirmed against SWL
       * source engineering.c and SWGD's ARCCHANGES patch notes:
       * "Durasteel quality given a purpose, it affects engineering
       * blades, swords, bludgeons, and now force pikes"). value[0] and
       * value[1] on the consumed durasteel are its quality rating (0 =
       * scrap-grade, higher = better refined material) - better
       * durasteel produces a sharper, more damaging blade, independent
       * of the crafter's own skill level.
       */
      if( obj->item_type == ITEM_DURASTEEL && checkdura == FALSE )
      {
         float exdam = ( ch->subclass == SUBCLASS_WEAPONSMITH ) ? 1.1f : 1.0f;

         strengthmin = ( int ) URANGE( level / 20 + 10, exdam * ( obj->value[0] * 8 + level / 4 ), level / 10 + 60 );
         strengthmax = ( int ) URANGE( level / 10 + 20, exdam * ( obj->value[1] * 15 + level / 3 ), level / 5 + 120 );
         if( strengthmin > strengthmax )
            strengthmax = strengthmin;
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         charge = UMAX( 5, obj->value[0] );
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeblade] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkbatt ) || ( !checkoven ) )
   {
      send_to_char( "&RYou activate your newly created vibroblade.\r\n", ch );
      send_to_char( "&RIt hums softly for a few seconds then begins to shake violently.\r\n", ch );
      send_to_char( "&RIt finally shatters breaking apart into a dozen pieces.\r\n", ch );
      learn_from_failure( ch, gsn_makeblade );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 3;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " vibro-blade blade", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   CREATE( paf, AFFECT_DATA, 1 );
   paf->type = -1;
   paf->duration = -1;
   paf->location = get_atype( "backstab" );
   paf->modifier = level / 3;
   paf->bitvector = 0;
   paf->next = NULL;
   LINK( paf, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   /*
    * v1.18 fix: SWL source (engineering.c do_makeblade) confirms this
    * should be a hitroll BONUS of level/33+1, not the -2 penalty this
    * had before, and there should be a third damroll bonus (level/33)
    * that was missing entirely. Matched to confirmed source.
    */
   CREATE( paf2, AFFECT_DATA, 1 );
   paf2->type = -1;
   paf2->duration = -1;
   paf2->location = get_atype( "hitroll" );
   paf2->modifier = level / 33 + 1;
   paf2->bitvector = 0;
   paf2->next = NULL;
   LINK( paf2, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf3, AFFECT_DATA, 1 );
   paf3->type = -1;
   paf3->duration = -1;
   paf3->location = get_atype( "damroll" );
   paf3->modifier = level / 33;
   paf3->bitvector = 0;
   paf3->next = NULL;
   LINK( paf3, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   obj->value[0] = INIT_WEAPON_CONDITION;
   obj->value[1] = strengthmin;  /* min dmg - scaled by durasteel quality, see loop above */
   obj->value[2] = strengthmax;  /* max dmg - scaled by durasteel quality, see loop above */
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
   {
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
      obj->value[2] = ( int )( obj->value[2] * 1.1 );
   }
   obj->value[3] = WEAPON_VIBRO_BLADE;
   obj->value[4] = charge;
   obj->value[5] = charge;
   obj->cost = obj->value[2] * 10;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created blade.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting a vibro-blade.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 200,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_makeblade );
}

/*
 * Five new crafting recipes - v1.18, modeled directly on do_makeblade's
 * structure (same toolkit/durasteel/battery/oven component requirements,
 * same timer/substate flow, same quality-scaling formula pattern already
 * used by make_flamethrower and every other weapon recipe in this file).
 */
void do_makeaxe( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance, charge = 0;
   int strengthmin = 0, strengthmax = 0;
   bool checktool, checkdura, checkbatt, checkoven;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makeaxe <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkbatt = FALSE;
         checkoven = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make a vibro-axe.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make it out of.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for your axe.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to heat the metal.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeaxe] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of crafting a vibro-axe.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 10, do_makeaxe, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 15, do_makeaxe, 1 );
            else add_timer( ch, TIMER_DO_FUN, 25, do_makeaxe, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makeaxe );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeaxe] );
   vnum = 68;   /* limbo.are vibro-axe prototype */

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkbatt = FALSE;
   checkoven = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      /*
       * Durasteel quality matters here (v1.18, confirmed against SWL
       * source and SWGD's ARCCHANGES patch notes: "Durasteel quality
       * given a purpose, it affects engineering blades, swords,
       * bludgeons, and now force pikes"). value[0]/value[1] on the
       * consumed durasteel are its quality rating - better durasteel
       * produces a harder, more damaging axe head, independent of the
       * crafter's own skill level.
       */
      if( obj->item_type == ITEM_DURASTEEL && checkdura == FALSE )
      {
         float exdam = ( ch->subclass == SUBCLASS_WEAPONSMITH ) ? 1.1f : 1.0f;

         strengthmin = ( int ) URANGE( level / 15 + 14, exdam * ( obj->value[0] * 10 + level / 3 ), level / 8 + 70 );
         strengthmax = ( int ) URANGE( level / 8 + 26, exdam * ( obj->value[1] * 18 + level / 2 ), level / 4 + 140 );
         if( strengthmin > strengthmax )
            strengthmax = strengthmin;
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         charge = UMAX( 5, obj->value[0] );
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeaxe] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkbatt ) || ( !checkoven ) )
   {
      send_to_char( "&RYou activate your newly created vibro-axe.\r\n", ch );
      send_to_char( "&RIt hums softly for a few seconds then begins to shake violently.\r\n", ch );
      send_to_char( "&RIt finally shatters breaking apart into a dozen pieces.\r\n", ch );
      learn_from_failure( ch, gsn_makeaxe );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 9;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " vibro-axe axe", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = INIT_WEAPON_CONDITION;
   obj->value[1] = strengthmin;  /* min dmg - scaled by durasteel quality, see loop above */
   obj->value[2] = strengthmax;  /* max dmg - scaled by durasteel quality, see loop above */
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
   {
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
      obj->value[2] = ( int )( obj->value[2] * 1.1 );
   }
   obj->value[3] = WEAPON_VIBRO_AXE;
   obj->value[4] = charge;
   obj->value[5] = charge;
   obj->cost = obj->value[2] * 10;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created axe.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting a vibro-axe.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 200,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_makeaxe );
}

void do_makewhip( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance;
   int strengthmin = 0, strengthmax = 0;
   bool checktool, checkdura, checkoven;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makewhip <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkoven = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make a whip.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make it out of.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to treat the material.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makewhip] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin braiding and treating the material for a whip.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and begins to work on something.", ch, NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 8, do_makewhip, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 12, do_makewhip, 1 );
            else add_timer( ch, TIMER_DO_FUN, 20, do_makewhip, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to braid the material properly.\r\n", ch );
         learn_from_failure( ch, gsn_makewhip );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makewhip] );
   vnum = 69;   /* limbo.are whip prototype */

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkoven = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      /*
       * Durasteel quality matters here (v1.18, confirmed against SWL
       * source and SWGD's ARCCHANGES patch notes: "Durasteel quality
       * given a purpose, it affects engineering blades, swords,
       * bludgeons, and now force pikes"). Whip wasn't itself in that
       * SWL list (it didn't exist yet in SWL), but this codebase's whip
       * uses durasteel for its weighted core/tip, so the same principle
       * applies: value[0]/value[1] on the consumed durasteel is its
       * quality rating - better durasteel makes for a more damaging
       * whip, independent of the crafter's own skill level.
       */
      if( obj->item_type == ITEM_DURASTEEL && checkdura == FALSE )
      {
         float exdam = ( ch->subclass == SUBCLASS_WEAPONSMITH ) ? 1.1f : 1.0f;

         strengthmin = ( int ) URANGE( level / 20 + 3, exdam * ( obj->value[0] * 4 + level / 6 ), level / 15 + 20 );
         strengthmax = ( int ) URANGE( level / 12 + 6, exdam * ( obj->value[1] * 7 + level / 4 ), level / 8 + 40 );
         if( strengthmin > strengthmax )
            strengthmax = strengthmin;
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makewhip] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkoven ) )
   {
      send_to_char( "&RYour whip comes apart in your hands as you finish it.\r\n", ch );
      learn_from_failure( ch, gsn_makewhip );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 3;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " whip", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " lies here, coiled.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = INIT_WEAPON_CONDITION;
   obj->value[1] = strengthmin;  /* min dmg - scaled by durasteel quality, see loop above */
   obj->value[2] = strengthmax;  /* max dmg - scaled by durasteel quality, see loop above */
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
   {
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
      obj->value[2] = ( int )( obj->value[2] * 1.1 );
   }
   obj->value[3] = WEAPON_WHIP;
   obj->value[4] = 0;
   obj->value[5] = 0;
   obj->cost = obj->value[2] * 10;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish braiding your whip and give it an experimental crack.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting a whip.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 200,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_makewhip );
}

void do_makebowcaster( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance;
   int strengthmin = 0, strengthmax = 0;
   bool checktool, checkdura, checkoven;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makebowcaster <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkoven = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make a bowcaster.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make it out of.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to shape the housing.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makebowcaster] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of crafting a bowcaster.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 10, do_makebowcaster, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 15, do_makebowcaster, 1 );
            else add_timer( ch, TIMER_DO_FUN, 25, do_makebowcaster, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makebowcaster );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makebowcaster] );
   vnum = 6828;   /* existing bowcaster prototype (quarren.are) */

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkoven = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      /*
       * Durasteel quality matters here (v1.18, confirmed against SWL
       * source and SWGD's ARCCHANGES patch notes: "Durasteel quality
       * given a purpose, it affects engineering blades, swords,
       * bludgeons, and now force pikes"). value[0]/value[1] on the
       * consumed durasteel are its quality rating - better durasteel
       * makes for a more precise barrel and firing mechanism,
       * independent of the crafter's own skill level.
       */
      if( obj->item_type == ITEM_DURASTEEL && checkdura == FALSE )
      {
         float exdam = ( ch->subclass == SUBCLASS_WEAPONSMITH ) ? 1.1f : 1.0f;

         strengthmin = ( int ) URANGE( level / 15 + 10, exdam * ( obj->value[0] * 9 + level / 3 ), level / 8 + 60 );
         strengthmax = ( int ) URANGE( level / 9 + 22, exdam * ( obj->value[1] * 17 + level / 2 ), level / 4 + 120 );
         if( strengthmin > strengthmax )
            strengthmax = strengthmin;
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makebowcaster] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkoven ) )
   {
      send_to_char( "&RYour bowcaster sparks and jams as you finish assembling it.\r\n", ch );
      learn_from_failure( ch, gsn_makebowcaster );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 8;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " bowcaster", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = INIT_WEAPON_CONDITION;
   obj->value[1] = strengthmin;  /* min dmg - scaled by durasteel quality, see loop above */
   obj->value[2] = strengthmax;  /* max dmg - scaled by durasteel quality, see loop above */
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
   {
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
      obj->value[2] = ( int )( obj->value[2] * 1.1 );
   }
   obj->value[3] = WEAPON_BOWCASTER;
   obj->value[4] = 0;
   obj->value[5] = 0;
   obj->cost = obj->value[2] * 10;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created bowcaster.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting a bowcaster.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 200,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_makebowcaster );
}

void do_makeforcepike( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance, charge = 0;
   int strengthmin = 0, strengthmax = 0;
   bool checktool, checkdura, checkbatt, checkoven;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;
   AFFECT_DATA *paf;
   AFFECT_DATA *paf2;
   AFFECT_DATA *paf3;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makeforcepike <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkbatt = FALSE;
         checkoven = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make a force pike.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make it out of.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for your force pike.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to heat the metal.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeforcepike] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of crafting a force pike.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 12, do_makeforcepike, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 18, do_makeforcepike, 1 );
            else add_timer( ch, TIMER_DO_FUN, 30, do_makeforcepike, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makeforcepike );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeforcepike] );
   vnum = 32200;   /* existing force pike prototype (space.are) */

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkbatt = FALSE;
   checkoven = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      /*
       * Durasteel quality matters here (v1.18) - and this one has real
       * SWL predecessor source behind it (engineering.c do_makeforcepike):
       * "durasteel affects damage" via obj->value[0]/value[1] on the
       * consumed durasteel, clamped by level-based floor/ceiling, with a
       * Weaponsmith 1.1x multiplier. Coefficients rescaled here to fit
       * this codebase's existing damage numbers rather than SWL's much
       * larger absolute scale, but the formula shape is the same.
       */
      if( obj->item_type == ITEM_DURASTEEL && checkdura == FALSE )
      {
         float exdam = ( ch->subclass == SUBCLASS_WEAPONSMITH ) ? 1.1f : 1.0f;

         strengthmin = ( int ) URANGE( level / 15 + 12, exdam * ( obj->value[0] * 10 + level / 3 ), level / 8 + 70 );
         strengthmax = ( int ) URANGE( level / 8 + 24, exdam * ( obj->value[1] * 18 + level / 2 ), level / 4 + 140 );
         if( strengthmin > strengthmax )
            strengthmax = strengthmin;
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         charge = UMAX( 5, obj->value[0] );
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeforcepike] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkbatt ) || ( !checkoven ) )
   {
      send_to_char( "&RYour force pike crackles wildly then goes dead in your hands.\r\n", ch );
      learn_from_failure( ch, gsn_makeforcepike );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 7;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " force pike", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   /*
    * Disarm/hitroll/damroll bonuses - confirmed against SWL source
    * (engineering.c do_makeforcepike). These scale with the crafter's
    * level, not the durasteel's quality stat directly - a higher-level
    * crafter produces a force pike that's harder to disarm and hits
    * more accurately/harder, independent of the raw material used.
    */
   CREATE( paf, AFFECT_DATA, 1 );
   paf->type = -1;
   paf->duration = -1;
   paf->location = get_atype( "disarm" );
   paf->modifier = level / 2;
   paf->bitvector = 0;
   paf->next = NULL;
   LINK( paf, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf2, AFFECT_DATA, 1 );
   paf2->type = -1;
   paf2->duration = -1;
   paf2->location = get_atype( "hitroll" );
   paf2->modifier = level / 25 + 1;
   paf2->bitvector = 0;
   paf2->next = NULL;
   LINK( paf2, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf3, AFFECT_DATA, 1 );
   paf3->type = -1;
   paf3->duration = -1;
   paf3->location = get_atype( "damroll" );
   paf3->modifier = level / 25 + 1;
   paf3->bitvector = 0;
   paf3->next = NULL;
   LINK( paf3, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   obj->value[0] = INIT_WEAPON_CONDITION;
   obj->value[1] = strengthmin;  /* min dmg - scaled by durasteel quality, see loop above */
   obj->value[2] = strengthmax;  /* max dmg - scaled by durasteel quality, see loop above */
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
   {
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
      obj->value[2] = ( int )( obj->value[2] * 1.1 );
   }
   obj->value[3] = WEAPON_FORCE_PIKE;
   obj->value[4] = charge;
   obj->value[5] = charge;
   obj->cost = obj->value[2] * 10;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created force pike.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting a force pike.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 200,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_makeforcepike );
}

void do_makedrinkcon( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance;
   bool checktool, checkfabric, checkthread;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makedrinkcon <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkfabric = FALSE;
         checkthread = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_FABRIC )
               checkfabric = TRUE;
            if( obj->item_type == ITEM_THREAD )
               checkthread = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a needle and toolkit to make a container.\r\n", ch );
            return;
         }

         if( !checkfabric )
         {
            send_to_char( "&RYou need some fabric to sew it out of.\r\n", ch );
            return;
         }

         if( !checkthread )
         {
            send_to_char( "&RYou need thread to sew the seams.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makedrinkcon] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin sewing a canteen.\r\n", ch );
            act( AT_PLAIN, "$n takes out a needle and thread and begins to work on some fabric.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 4, do_makedrinkcon, 1 );
            else if( ch->subclass == SUBCLASS_TAILOR ) add_timer( ch, TIMER_DO_FUN, 3, do_makedrinkcon, 1 );
            else add_timer( ch, TIMER_DO_FUN, 8, do_makedrinkcon, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to seal the seams properly.\r\n", ch );
         learn_from_failure( ch, gsn_makedrinkcon );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makedrinkcon] );
   vnum = 70;   /* limbo.are canteen prototype */

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkfabric = FALSE;
   checkthread = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_FABRIC && checkfabric == FALSE )
      {
         checkfabric = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_THREAD && checkthread == FALSE )
      {
         checkthread = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makedrinkcon] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkfabric ) || ( !checkthread ) )
   {
      send_to_char( "&RThe seams split and your canteen falls apart, leaking everywhere.\r\n", ch );
      learn_from_failure( ch, gsn_makedrinkcon );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_DRINK_CON;
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   SET_BIT( obj->wear_flags, ITEM_HOLD );
   obj->level = level;
   obj->weight = 2;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " canteen drinkcon", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = UMIN( 60, 20 + level / 3 );   /* max liquid capacity */
   obj->value[1] = obj->value[0];                /* starts full */
   obj->value[2] = 0;                            /* water */
   obj->value[3] = 0;                            /* not poisoned */
   obj->cost = obj->value[0] * 5;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish shaping your canteen.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting a canteen.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 200,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_makedrinkcon );
}

void do_makevibrosword( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance, charge = 0;
   int blade_min = 0, blade_max = 0;   /* quality of consumed vibroblade */
   bool checktool, checkdura, checkbatt, checkoven, checkblade;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;
   AFFECT_DATA *paf;
   AFFECT_DATA *paf2;
   AFFECT_DATA *paf3;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makevibrosword <name>\r\n&w", ch );
            return;
         }

         checktool  = FALSE;
         checkdura  = FALSE;
         checkbatt  = FALSE;
         checkoven  = FALSE;
         checkblade = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            /* Vibro-blade component: weapon with type WEAPON_VIBRO_BLADE */
            if( obj->item_type == ITEM_WEAPON && obj->value[3] == WEAPON_VIBRO_BLADE && !checkblade )
               checkblade = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make a vibro-sword.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make it out of.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for your sword.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to heat the metal.\r\n", ch );
            return;
         }

         if( !checkblade )
         {
            send_to_char( "&RYou need a vibro-blade to serve as the core of your sword.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makevibrosword] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of crafting a vibro-sword.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 10, do_makevibrosword, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 15, do_makevibrosword, 1 );
            else add_timer( ch, TIMER_DO_FUN, 25, do_makevibrosword, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makevibrosword );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interrupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makevibrosword] );
   vnum = 10431;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool  = FALSE;
   checkdura  = FALSE;
   checkbatt  = FALSE;
   checkoven  = FALSE;
   checkblade = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( obj->item_type == ITEM_DURASTEEL && !checkdura )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && !checkbatt )
      {
         charge = UMAX( 5, obj->value[0] );
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
      /* Consume the vibro-blade and record its damage values for scaling */
      if( obj->item_type == ITEM_WEAPON && obj->value[3] == WEAPON_VIBRO_BLADE && !checkblade )
      {
         blade_min = obj->value[1];
         blade_max = obj->value[2];
         checkblade = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makevibrosword] );

   if( number_percent(  ) > schance * 2 || !checktool || !checkdura || !checkbatt || !checkoven || !checkblade )
   {
      send_to_char( "&RYou activate your newly created vibro-sword.\r\n", ch );
      send_to_char( "&RIt hums softly for a few seconds then begins to shake violently.\r\n", ch );
      send_to_char( "&RIt finally shatters breaking apart into a dozen pieces.\r\n", ch );
      learn_from_failure( ch, gsn_makevibrosword );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 4;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " vibro-sword sword", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   /*
    * v1.18 fix: SWL source (engineering.c do_makesword) confirms parry
    * level/10, hitroll level/10, damroll level/20 - this had hitroll and
    * damroll both at level/4 (noticeably stronger than source) and no
    * parry bonus at all. Matched to confirmed source.
    */
   CREATE( paf, AFFECT_DATA, 1 );
   paf->type      = -1;
   paf->duration  = -1;
   paf->location  = get_atype( "parry" );
   paf->modifier  = level / 10;
   paf->bitvector = 0;
   paf->next      = NULL;
   LINK( paf, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf2, AFFECT_DATA, 1 );
   paf2->type      = -1;
   paf2->duration  = -1;
   paf2->location  = get_atype( "hitroll" );
   paf2->modifier  = level / 10;
   paf2->bitvector = 0;
   paf2->next      = NULL;
   LINK( paf2, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf3, AFFECT_DATA, 1 );
   paf3->type      = -1;
   paf3->duration  = -1;
   paf3->location  = get_atype( "damroll" );
   paf3->modifier  = level / 20;
   paf3->bitvector = 0;
   paf3->next      = NULL;
   LINK( paf3, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;

   /*
    * Base damage from skill level, then add a bonus proportional to the
    * quality of the consumed vibro-blade.  A high-damage blade yields a
    * noticeably stronger sword.  blade_max drives the bonus so that better
    * blades always produce better swords, with a small floor bonus of +1
    * to reward any blade being used at all.
    */
   obj->value[0] = INIT_WEAPON_CONDITION;
   obj->value[1] = ( int )( level / 15 + 12 ) + ( blade_min / 4 ) + 1;
   obj->value[2] = ( int )( level / 8  + 22 ) + ( blade_max / 3 ) + 1;
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
   {
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
      obj->value[2] = ( int )( obj->value[2] * 1.1 );
   }
   obj->value[3] = WEAPON_VIBRO_SWORD;
   obj->value[4] = charge;
   obj->value[5] = charge;
   obj->cost = obj->value[2] * 10;

   obj = obj_to_char( obj, ch );

   ch_printf( ch, "&GYou finish your work and hold up your newly created vibro-sword.  "
                  "(Damage: %d-%d)&w\r\n", obj->value[1], obj->value[2] );
   act( AT_PLAIN, "$n finishes crafting a vibro-sword.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 200,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_makevibrosword );
}

void do_makeblaster( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance;
   bool checktool, checkdura, checkbatt, checkoven, checkcond, checkcirc, checkammo;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum, power, scope, ammo;
   AFFECT_DATA *paf;
   AFFECT_DATA *paf2;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makeblaster <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkbatt = FALSE;
         checkoven = FALSE;
         checkcond = FALSE;
         checkcirc = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURAPLAST )
               checkdura = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
            if( obj->item_type == ITEM_SUPERCONDUCTOR )
               checkcond = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need toolkit to make a blaster.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make it out of.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for your blaster.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to heat the plastics.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit board to control the firing mechanism.\r\n", ch );
            return;
         }

         if( !checkcond )
         {
            send_to_char( "&RYou still need a small superconductor.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeblaster] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of making a blaster.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 10, do_makeblaster, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 15, do_makeblaster, 1 );
            else add_timer( ch, TIMER_DO_FUN, 25, do_makeblaster, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makeblaster );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeblaster] );
   vnum = 10420;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checkammo = FALSE;
   checktool = FALSE;
   checkdura = FALSE;
   checkbatt = FALSE;
   checkoven = FALSE;
   checkcond = FALSE;
   checkcirc = FALSE;
   power = 0;
   scope = 0;
   ammo = 0;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( obj->item_type == ITEM_DURAPLAST && checkdura == FALSE )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_AMMO && checkammo == FALSE )
      {
         ammo = obj->value[0];
         checkammo = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
      if( obj->item_type == ITEM_LENS && scope == 0 )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         scope++;
      }
      if( obj->item_type == ITEM_SUPERCONDUCTOR && power < 2 )
      {
         power++;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcond = TRUE;
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcirc = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeblaster] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkbatt ) || ( !checkoven )
       || ( !checkcond ) || ( !checkcirc ) )
   {
      send_to_char( "&RYou hold up your new blaster and aim at a leftover piece of plastic.\r\n", ch );
      send_to_char( "&RYou slowly squeeze the trigger hoping for the best...\r\n", ch );
      send_to_char( "&RYour blaster backfires destroying your weapon and burning your hand.\r\n", ch );
      learn_from_failure( ch, gsn_makeblaster );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 2 + level / 10;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " blaster", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was carelessly misplaced here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   CREATE( paf, AFFECT_DATA, 1 );
   paf->type = -1;
   paf->duration = -1;
   paf->location = get_atype( "hitroll" );
   paf->modifier = URANGE( 0, 1 + scope, level / 30 );
   paf->bitvector = 0;
   paf->next = NULL;
   LINK( paf, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf2, AFFECT_DATA, 1 );
   paf2->type = -1;
   paf2->duration = -1;
   paf2->location = get_atype( "damroll" );
   paf2->modifier = URANGE( 0, power, level / 30 );
   paf2->bitvector = 0;
   paf2->next = NULL;
   LINK( paf2, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   obj->value[0] = INIT_WEAPON_CONDITION; /* condition  */
   obj->value[1] = ( int )( level / 10 + 15 );  /* min dmg  */
   obj->value[2] = ( int )( level / 5 + 25 );   /* max dmg  */
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
   {
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
      obj->value[2] = ( int )( obj->value[2] * 1.1 );
   }
   obj->value[3] = WEAPON_BLASTER;
   obj->value[4] = ammo;
   obj->value[5] = 2000;
   obj->cost = obj->value[2] * 50;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created blaster.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes making $s new blaster.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 50,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }
   learn_from_success( ch, gsn_makeblaster );
}

void do_makelightsaber( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int schance;
   bool checktool, checkdura, checkbatt, checkoven, checkcond, checkcirc, checklens, checkgems, checkmirr;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum, level, gems, charge, gemtype;
   AFFECT_DATA *paf;
   AFFECT_DATA *paf2;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makelightsaber <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkbatt = FALSE;
         checkoven = FALSE;
         checkcond = FALSE;
         checkcirc = FALSE;
         checklens = FALSE;
         checkgems = FALSE;
         checkmirr = FALSE;

         if( ( !IS_SET( ch->in_room->room_flags, ROOM_SAFE ) || !IS_SET( ch->in_room->room_flags, ROOM_SILENCE ) ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a quiet peaceful place to craft a lightsaber.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_LENS )
               checklens = TRUE;
            if( obj->item_type == ITEM_CRYSTAL )
               checkgems = TRUE;
            if( obj->item_type == ITEM_MIRROR )
               checkmirr = TRUE;
            if( obj->item_type == ITEM_DURAPLAST || obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
            if( obj->item_type == ITEM_SUPERCONDUCTOR )
               checkcond = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need toolkit to make a lightsaber.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make it out of.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for your lightsaber.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to heat and shape the components.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit board.\r\n", ch );
            return;
         }

         if( !checkcond )
         {
            send_to_char( "&RYou still need a small superconductor for your lightsaber.\r\n", ch );
            return;
         }

         if( !checklens )
         {
            send_to_char( "&RYou still need a lens to focus the beam.\r\n", ch );
            return;
         }

         if( !checkgems )
         {
            send_to_char( "&RLightsabers require 1 to 3 gems to work properly.\r\n", ch );
            return;
         }

         if( !checkmirr )
         {
            send_to_char( "&RYou need a high intesity reflective cup to create a lightsaber.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_lightsaber_crafting] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of crafting a lightsaber.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 10, do_makelightsaber, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 15, do_makelightsaber, 1 );
            else add_timer( ch, TIMER_DO_FUN, 25, do_makelightsaber, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_lightsaber_crafting );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_lightsaber_crafting] );
   vnum = 10421;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkbatt = FALSE;
   checkoven = FALSE;
   checkcond = FALSE;
   checkcirc = FALSE;
   checklens = FALSE;
   checkgems = FALSE;
   checkmirr = FALSE;
   gems = 0;
   charge = 0;
   gemtype = 0;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( ( obj->item_type == ITEM_DURAPLAST || obj->item_type == ITEM_DURASTEEL ) && checkdura == FALSE )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_DURASTEEL && checkdura == FALSE )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         charge = UMIN( obj->value[1], 10 );
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
      if( obj->item_type == ITEM_SUPERCONDUCTOR && checkcond == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcond = TRUE;
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcirc = TRUE;
      }
      if( obj->item_type == ITEM_LENS && checklens == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checklens = TRUE;
      }
      if( obj->item_type == ITEM_MIRROR && checkmirr == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkmirr = TRUE;
      }
      if( obj->item_type == ITEM_CRYSTAL && gems < 3 )
      {
         gems++;
         if( gemtype < obj->value[0] )
            gemtype = obj->value[0];
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkgems = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_lightsaber_crafting] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkbatt ) || ( !checkoven )
       || ( !checkmirr ) || ( !checklens ) || ( !checkgems ) || ( !checkcond ) || ( !checkcirc ) )

   {
      send_to_char( "&RYou hold up your new lightsaber and press the switch hoping for the best.\r\n", ch );
      send_to_char( "&RInstead of a blade of light, smoke starts pouring from the handle.\r\n", ch );
      send_to_char( "&RYou drop the hot handle and watch as it melts on away on the floor.\r\n", ch );
      learn_from_failure( ch, gsn_lightsaber_crafting );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   SET_BIT( obj->extra_flags, ITEM_ANTI_SOLDIER );
   SET_BIT( obj->extra_flags, ITEM_ANTI_THIEF );
   SET_BIT( obj->extra_flags, ITEM_ANTI_HUNTER );
   SET_BIT( obj->extra_flags, ITEM_ANTI_PILOT );
   SET_BIT( obj->extra_flags, ITEM_ANTI_CITIZEN );
   obj->level = level;
   obj->weight = 5;
   STRFREE( obj->name );
   obj->name = STRALLOC( "lightsaber saber" );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was carelessly misplaced here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   STRFREE( obj->action_desc );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " ignites with a hum and a soft glow.", MAX_STRING_LENGTH );
   obj->action_desc = STRALLOC( buf );
   CREATE( paf, AFFECT_DATA, 1 );
   paf->type = -1;
   paf->duration = -1;
   paf->location = get_atype( "hitroll" );
   paf->modifier = URANGE( 0, gems, level / 30 );
   paf->bitvector = 0;
   paf->next = NULL;
   LINK( paf, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf2, AFFECT_DATA, 1 );
   paf2->type = -1;
   paf2->duration = -1;
   paf2->location = get_atype( "parry" );
   paf2->modifier = ( level / 3 );
   paf2->bitvector = 0;
   paf2->next = NULL;
   LINK( paf2, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   obj->value[0] = INIT_WEAPON_CONDITION; /* condition  */
   obj->value[1] = ( int )( level / 10 + gemtype * 2 );  /* min dmg  */
   obj->value[2] = ( int )( level / 5 + gemtype * 6 );   /* max dmg */
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
   {
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
      obj->value[2] = ( int )( obj->value[2] * 1.1 );
   }
   obj->value[3] = WEAPON_LIGHTSABER;
   obj->value[4] = charge;
   obj->value[5] = charge;
   obj->cost = obj->value[2] * 75;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created lightsaber.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes making $s new lightsaber.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 50,
               ( exp_level( ch->skill_level[FORCE_ABILITY] + 1 ) - exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, FORCE_ABILITY );
      ch_printf( ch, "You gain %ld force experience.", xpgain );
   }
   learn_from_success( ch, gsn_lightsaber_crafting );
}

/*
 * do_makeflamethrower is intentionally not registered as a player
 * command (v1.18) - per Mark's call, since SWGD's "flamethrowers"
 * weapon-type proficiency wasn't ported over with the rest of the
 * throwing-group weapon types.
 */
void do_makeflamethrower( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int schance;
   bool checktool, checkdura, checkoven, checkcirc, checkfuel;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum, level, fueltype;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makeflamethrower <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkoven = FALSE;
         checkcirc = FALSE;
         checkfuel = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURAPLAST || obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
            if( obj->item_type == ITEM_FUEL_CANISTER )
               checkfuel = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make a flamethrower.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make the housing out of.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to heat and shape the components.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit board for the ignition system.\r\n", ch );
            return;
         }

         if( !checkfuel )
         {
            send_to_char( "&RYou need a fuel canister to power the flamethrower.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeflamethrower] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of assembling a wrist-flamethrower.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK )
               add_timer( ch, TIMER_DO_FUN, 10, do_makeflamethrower, 1 );
            else
               add_timer( ch, TIMER_DO_FUN, 25, do_makeflamethrower, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makeflamethrower );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeflamethrower] );
   vnum = 66;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkoven = FALSE;
   checkcirc = FALSE;
   checkfuel = FALSE;
   fueltype = 0;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( ( obj->item_type == ITEM_DURAPLAST || obj->item_type == ITEM_DURASTEEL ) && checkdura == FALSE )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcirc = TRUE;
      }
      if( obj->item_type == ITEM_FUEL_CANISTER && checkfuel == FALSE )
      {
         if( fueltype < obj->value[0] )
            fueltype = obj->value[0];
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkfuel = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeflamethrower] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkoven ) || ( !checkcirc )
       || ( !checkfuel ) )
   {
      send_to_char( "&RYou strap on your new flamethrower and give the trigger a test squeeze.\r\n", ch );
      send_to_char( "&RInstead of a jet of flame, it sputters, sparks, and falls apart on your arm.\r\n", ch );
      learn_from_failure( ch, gsn_makeflamethrower );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_FLAMETHROWER;
   SET_BIT( obj->wear_flags, ITEM_WEAR_WRIST );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 5;
   STRFREE( obj->name );
   obj->name = STRALLOC( "flamethrower wrist" );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was carelessly misplaced here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );

   obj->value[0] = fueltype;                                    /* fuel/quality tier */
   obj->value[1] = ( int )( level / 15 + fueltype * 30 );        /* min dmg */
   obj->value[2] = ( int )( level / 15 + fueltype * 78 );        /* max dmg */
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
   {
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
      obj->value[2] = ( int )( obj->value[2] * 1.1 );
   }
   obj->cost = obj->value[2] * 75;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly assembled wrist-flamethrower.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes assembling $s new wrist-flamethrower.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 50,
               ( exp_level( ch->skill_level[HUNTING_ABILITY] + 1 ) - exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, HUNTING_ABILITY );
      ch_printf( ch, "You gain %ld hunting experience.", xpgain );
   }
   learn_from_success( ch, gsn_makeflamethrower );
}

void do_makejetpack( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int schance;
   bool checktool, checkdura, checkoven, checkcirc, checkbatt;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum, level;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makejetpack <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkoven = FALSE;
         checkcirc = FALSE;
         checkbatt = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURAPLAST || obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make a jetpack.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make the housing and thruster frame out of.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to heat and shape the components.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit board for the flight controls.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for the thrusters.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makejetpack] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of assembling a jetpack.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK )
               add_timer( ch, TIMER_DO_FUN, 10, do_makejetpack, 1 );
            else
               add_timer( ch, TIMER_DO_FUN, 25, do_makejetpack, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makejetpack );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makejetpack] );
   vnum = 67;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkoven = FALSE;
   checkcirc = FALSE;
   checkbatt = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( ( obj->item_type == ITEM_DURAPLAST || obj->item_type == ITEM_DURASTEEL ) && checkdura == FALSE )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcirc = TRUE;
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makejetpack] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkoven ) || ( !checkcirc )
       || ( !checkbatt ) )
   {
      send_to_char( "&RYou strap on your new jetpack and give the ignition a try.\r\n", ch );
      send_to_char( "&RInstead of lifting off, it sputters, sparks, and falls apart on your back.\r\n", ch );
      learn_from_failure( ch, gsn_makejetpack );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_JETPACK;
   SET_BIT( obj->wear_flags, ITEM_WEAR_ABOUT );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 10;
   STRFREE( obj->name );
   obj->name = STRALLOC( "jetpack rockets" );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was carelessly misplaced here, a pair of rockets still attached to its frame.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->cost = level * 75;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly assembled jetpack, a pair of rockets attached.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes assembling $s new jetpack.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 50,
               ( exp_level( ch->skill_level[HUNTING_ABILITY] + 1 ) - exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, HUNTING_ABILITY );
      ch_printf( ch, "You gain %ld hunting experience.", xpgain );
   }
   learn_from_success( ch, gsn_makejetpack );
}

void do_makedoublelightsaber( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int chance;
   bool checktool, checkdura, checkbatt, checkoven, checkcond, checkcirc, checklens, checkgems, checkmirr;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum, level, gems, charge, gemtype;
   AFFECT_DATA *paf;
   AFFECT_DATA *paf2;
   AFFECT_DATA *paf3;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "Usage: Makedoublelightsaber <name>\r\n", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkbatt = FALSE;
         checkoven = FALSE;
         checkcond = FALSE;
         checkcirc = FALSE;
         checklens = FALSE;
         checkgems = FALSE;
         checkmirr = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_SILENCE ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a quiet peaceful place to craft a lightsaber.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_LENS )
               checklens = TRUE;
            if( obj->item_type == ITEM_CRYSTAL )
               checkgems = TRUE;
            if( obj->item_type == ITEM_MIRROR )
               checkmirr = TRUE;
            if( obj->item_type == ITEM_DURAPLAST || obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
            if( obj->item_type == ITEM_SUPERCONDUCTOR )
               checkcond = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make a double-bladed lightsaber.\r\n", ch );
            return;
         }

         if( !checkdura )
         {
            send_to_char( "&RYou need something to make it out of.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for your double-bladed lightsaber.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to heat and shape the components.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit board.\r\n", ch );
            return;
         }

         if( !checkcond )
         {
            send_to_char( "&RYou need a small superconductor to make your double-bladed lightsaber.\r\n", ch );
            return;
         }

         if( !checklens )
         {
            send_to_char( "&RYou need a lens to focus the beam.\r\n", ch );
            return;
         }

         if( !checkgems )
         {
            send_to_char( "&RDouble-bladed lightsabers require 1 to 3 gems to work properly.\r\n", ch );
            return;
         }

         if( !checkmirr )
         {
            send_to_char( "&RYou need a high intensity reflective cup to create a double-bladed lightsaber.\r\n", ch );
            return;
         }

         chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_doublelightsaber_crafting] );
         if( number_percent(  ) < chance )
         {
            send_to_char( "&GYou begin the long process of crafting a double-bladed lightsaber.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            add_timer( ch, TIMER_DO_FUN, 25, do_makedoublelightsaber, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out what to do! What an idiot!\r\n", ch );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_doublelightsaber_crafting] );
   vnum = 10421;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkbatt = FALSE;
   checkoven = FALSE;
   checkcond = FALSE;
   checkcirc = FALSE;
   checklens = FALSE;
   checkgems = FALSE;
   checkmirr = FALSE;
   gems = 0;
   charge = 0;
   gemtype = 0;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( ( obj->item_type == ITEM_DURAPLAST || obj->item_type == ITEM_DURASTEEL ) && checkdura == FALSE )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_DURASTEEL && checkdura == FALSE )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         charge = UMIN( obj->value[1], 10 );
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
      if( obj->item_type == ITEM_SUPERCONDUCTOR && checkcond == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcond = TRUE;
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcirc = TRUE;
      }
      if( obj->item_type == ITEM_LENS && checklens == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checklens = TRUE;
      }
      if( obj->item_type == ITEM_MIRROR && checkmirr == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkmirr = TRUE;
      }
      if( obj->item_type == ITEM_CRYSTAL && gems < 3 )
      {
         gems++;
         if( gemtype < obj->value[0] )
            gemtype = obj->value[0];
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkgems = TRUE;
      }
   }

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_doublelightsaber_crafting] );
   if( number_percent(  ) > chance * 2 || ( !checktool ) || ( !checkdura ) || ( !checkbatt ) || ( !checkoven )
       || ( !checkmirr ) || ( !checklens ) || ( !checkgems ) || ( !checkcond ) || ( !checkcirc ) )

   {
      send_to_char( "&RYou hold up your new double-bladed lightsaber and press the switches hoping for the best.\r\n", ch );
      send_to_char( "&RInstead of a blade of light, smoke starts pouring from the handle.\r\n", ch );
      send_to_char( "&RYou drop the hot handle and watch as it melts on away on the floor.\r\n", ch );
      learn_from_failure( ch, gsn_doublelightsaber_crafting );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   SET_BIT( obj->extra_flags, ITEM_ANTI_GOOD );
   obj->level = level;
   obj->weight = 10;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " doublelightsaber saber double-saber", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was carelessly misplaced here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   STRFREE( obj->action_desc );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " ignites with a hum and a soft glow.", MAX_STRING_LENGTH );
   obj->action_desc = STRALLOC( buf );
   CREATE( paf, AFFECT_DATA, 1 );
   paf->type = -1;
   paf->duration = -1;
   paf->location = get_atype( "hitroll" );
   paf->modifier = URANGE( 0, gems, level / 10 );
   paf->bitvector = 0;
   paf->next = NULL;
   LINK( paf, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf2, AFFECT_DATA, 1 );
   paf2->type = -1;
   paf2->duration = -1;
   paf2->location = get_atype( "damroll" );
   paf2->modifier = URANGE( 0, gems, level / 20 );
   paf2->bitvector = 0;
   paf2->next = NULL;
   LINK( paf2, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf3, AFFECT_DATA, 1 );
   paf3->type = -1;
   paf3->duration = -1;
   paf3->location = get_atype( "parry" );
   paf3->modifier = ( level / 2 );
   paf3->bitvector = 0;
   paf3->next = NULL;
   LINK( paf3, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   obj->value[0] = INIT_WEAPON_CONDITION; /* condition */
   obj->value[1] = ( int )( level / 10 + gemtype * 6 ) + 15;   /* min dmg */
   obj->value[2] = ( int )( level / 5 + gemtype * 11 ) + 150;  /* max dmg */
   obj->value[3] = WEAPON_LIGHTSABER;
   obj->value[4] = charge;
   obj->value[5] = charge;
   obj->cost = obj->value[2] * 150;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created double-bladed lightsaber.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes making $s new double-bladed lightsaber.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain = UMIN( obj->cost * 50,
                     ( exp_level( ch->skill_level[FORCE_ABILITY] + 1 ) - exp_level( ch->skill_level[FORCE_ABILITY] ) ) );
      gain_exp( ch, xpgain, FORCE_ABILITY );
      ch_printf( ch, "You gain %ld experience.", xpgain );
   }

   learn_from_success( ch, gsn_doublelightsaber_crafting );
}

void do_makespice( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int schance;
   OBJ_DATA *obj;

   switch ( ch->substate )
   {
      default:
         strlcpy( arg, argument, MAX_INPUT_LENGTH );

         if( arg[0] == '\0' )
         {
            send_to_char( "&RFrom what?\r\n&w", ch );
            return;
         }

         if( !IS_SET( ch->in_room->room_flags, ROOM_REFINERY ) )
         {
            send_to_char( "&RYou need to be in a refinery to create drugs from spice.\r\n", ch );
            return;
         }

         if( ms_find_obj( ch ) )
            return;

         if( ( obj = get_obj_carry( ch, arg ) ) == NULL )
         {
            send_to_char( "&RYou do not have that item.\r\n&w", ch );
            return;
         }

         if( obj->item_type != ITEM_RAWSPICE )
         {
            send_to_char( "&RYou can't make a drug out of that\r\n&w", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_spice_refining] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of refining spice into a drug.\r\n", ch );
            act( AT_PLAIN, "$n begins working on something.", ch, NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 5, do_makespice, 1 );
            else add_timer( ch, TIMER_DO_FUN, 10, do_makespice, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out what to do with the stuff.\r\n", ch );
         learn_from_failure( ch, gsn_spice_refining );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are distracted and are unable to finish your work.\r\n&w", ch );
         return;
   }

   ch->substate = SUB_NONE;

   if( ( obj = get_obj_carry( ch, arg ) ) == NULL )
   {
      send_to_char( "You seem to have lost your spice!\r\n", ch );
      return;
   }
   if( obj->item_type != ITEM_RAWSPICE )
   {
      send_to_char( "&RYou get your tools mixed up and can't finish your work.\r\n&w", ch );
      return;
   }

   obj->value[1] = URANGE( 10, obj->value[1], ( IS_NPC( ch ) ? ch->top_level
                                                : ( int )( ch->pcdata->learned[gsn_spice_refining] ) ) + 10 );
   strlcpy( buf, obj->name, MAX_STRING_LENGTH );
   STRFREE( obj->name );
   strlcat( buf, " drug spice", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, "a drug made from ", MAX_STRING_LENGTH );
   strlcat( buf, obj->short_descr, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   strlcat( buf, " was foolishly left lying around here.", MAX_STRING_LENGTH );
   STRFREE( obj->description );
   obj->description = STRALLOC( buf );
   obj->item_type = ITEM_SPICE;

   send_to_char( "&GYou finish your work.\r\n", ch );
   act( AT_PLAIN, "$n finishes $s work.", ch, NULL, argument, TO_ROOM );

   obj->cost += obj->value[1] * 10;
   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 50,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_spice_refining );
}

void do_makegrenade( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance, strength = 0, weight = 0;
   bool checktool, checkdrink, checkbatt, checkchem, checkcirc;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makegrenade <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdrink = FALSE;
         checkbatt = FALSE;
         checkchem = FALSE;
         checkcirc = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 )
               checkdrink = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
            if( obj->item_type == ITEM_CHEMICAL )
               checkchem = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need toolkit to make a grenade.\r\n", ch );
            return;
         }

         if( !checkdrink )
         {
            send_to_char( "&RYou will need an empty drink container to mix and hold the chemicals.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a small battery for the timer.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit for the timer.\r\n", ch );
            return;
         }

         if( !checkchem )
         {
            send_to_char( "&RSome explosive chemicals would come in handy!\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makegrenade] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of making a grenade.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a drink container and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 10, do_makegrenade, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 15, do_makegrenade, 1 );
            else add_timer( ch, TIMER_DO_FUN, 25, do_makegrenade, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makegrenade );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makegrenade] );
   vnum = 10425;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdrink = FALSE;
   checkbatt = FALSE;
   checkchem = FALSE;
   checkcirc = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_DRINK_CON && checkdrink == FALSE && obj->value[1] == 0 )
      {
         checkdrink = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
      if( obj->item_type == ITEM_CHEMICAL )
      {
         strength = URANGE( 10, obj->value[0], level * 5 );
         weight = obj->weight;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkchem = TRUE;
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcirc = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makegrenade] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdrink ) || ( !checkbatt ) || ( !checkchem )
       || ( !checkcirc ) )
   {
      send_to_char
         ( "&RJust as you are about to finish your work,\r\nyour newly created grenade explodes in your hands...doh!\r\n",
           ch );
      learn_from_failure( ch, gsn_makegrenade );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_GRENADE;
   SET_BIT( obj->wear_flags, ITEM_HOLD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = weight;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " grenade", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was carelessly misplaced here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = strength / 2;
   obj->value[1] = strength;
   obj->cost = obj->value[1] * 5;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created grenade.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes making $s new grenade.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 50,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }
   learn_from_success( ch, gsn_makegrenade );
}

void do_makelandmine( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance, strength = 0, weight = 0;
   bool checktool, checkdrink, checkbatt, checkchem, checkcirc;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makelandmine <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdrink = FALSE;
         checkbatt = FALSE;
         checkchem = FALSE;
         checkcirc = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 )
               checkdrink = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
            if( obj->item_type == ITEM_CHEMICAL )
               checkchem = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need toolkit to make a landmine.\r\n", ch );
            return;
         }

         if( !checkdrink )
         {
            send_to_char( "&RYou will need an empty drink container to mix and hold the chemicals.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a small battery for the detonator.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit for the detonator.\r\n", ch );
            return;
         }

         if( !checkchem )
         {
            send_to_char( "&RSome explosive chemicals would come in handy!\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makelandmine] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of making a landmine.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a drink container and begins to work on something.", ch,
                 NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 10, do_makelandmine, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 15, do_makelandmine, 1 );
            else add_timer( ch, TIMER_DO_FUN, 25, do_makelandmine, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makelandmine );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makelandmine] );
   vnum = 10427;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkdrink = FALSE;
   checkbatt = FALSE;
   checkchem = FALSE;
   checkcirc = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_DRINK_CON && checkdrink == FALSE && obj->value[1] == 0 )
      {
         checkdrink = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
      if( obj->item_type == ITEM_CHEMICAL )
      {
         strength = URANGE( 10, obj->value[0], level * 5 );
         weight = obj->weight;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkchem = TRUE;
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcirc = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makelandmine] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkdrink ) || ( !checkbatt ) || ( !checkchem )
       || ( !checkcirc ) )
   {
      send_to_char
         ( "&RJust as you are about to finish your work,\r\nyour newly created landmine explodes in your hands...doh!\r\n",
           ch );
      learn_from_failure( ch, gsn_makelandmine );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_LANDMINE;
   SET_BIT( obj->wear_flags, ITEM_HOLD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = weight;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " landmine", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was carelessly misplaced here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = strength / 2;
   obj->value[1] = strength;
   obj->cost = obj->value[1] * 5;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created landmine.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes making $s new landmine.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 50,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }
   learn_from_success( ch, gsn_makelandmine );
}

void do_makelight( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance, strength = 0;
   bool checktool, checkbatt, checkchem, checkcirc, checklens;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makeflashlight <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkbatt = FALSE;
         checkchem = FALSE;
         checkcirc = FALSE;
         checklens = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
            if( obj->item_type == ITEM_CHEMICAL )
               checkchem = TRUE;
            if( obj->item_type == ITEM_LENS )
               checklens = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need toolkit to make a light.\r\n", ch );
            return;
         }

         if( !checklens )
         {
            send_to_char( "&RYou need a lens to make a light.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a battery for the light to work.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit.\r\n", ch );
            return;
         }

         if( !checkchem )
         {
            send_to_char( "&RSome chemicals to light would come in handy!\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makelight] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of making a light.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and begins to work on something.", ch, NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 5, do_makelight, 1 );
            else add_timer( ch, TIMER_DO_FUN, 10, do_makelight, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makelight );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makelight] );
   vnum = 10428;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checklens = FALSE;
   checkbatt = FALSE;
   checkchem = FALSE;
   checkcirc = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         strength = obj->value[0];
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
      if( obj->item_type == ITEM_CHEMICAL )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkchem = TRUE;
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcirc = TRUE;
      }
      if( obj->item_type == ITEM_LENS && checklens == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checklens = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makelight] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checklens ) || ( !checkbatt ) || ( !checkchem )
       || ( !checkcirc ) )
   {
      send_to_char
         ( "&RJust as you are about to finish your work,\r\nyour newly created light explodes in your hands...doh!\r\n",
           ch );
      learn_from_failure( ch, gsn_makelight );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_LIGHT;
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 3;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " light", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was carelessly misplaced here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[2] = strength;
   obj->cost = obj->value[2];

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created light.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes making $s new light.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 100,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }
   learn_from_success( ch, gsn_makelight );
}

void do_makejewelry( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance;
   bool checktool, checkoven, checkmetal;
   OBJ_DATA *obj;
   OBJ_DATA *metal = NULL;
   int value, cost;

   argument = one_argument( argument, arg );
   strlcpy( arg2, argument, MAX_INPUT_LENGTH );

   if( !str_cmp( arg, "body" )
       || !str_cmp( arg, "head" )
       || !str_cmp( arg, "legs" )
       || !str_cmp( arg, "arms" )
       || !str_cmp( arg, "about" )
       || !str_cmp( arg, "eyes" )
       || !str_cmp( arg, "waist" ) || !str_cmp( arg, "hold" ) || !str_cmp( arg, "feet" ) || !str_cmp( arg, "hands" ) )
   {
      send_to_char( "&RYou cannot make jewelry for that body part.\r\n&w", ch );
      send_to_char( "&RTry MAKEARMOR.\r\n&w", ch );
      return;
   }
   if( !str_cmp( arg, "shield" ) )
   {
      send_to_char( "&RYou cannot make jewelry worn as a shield.\r\n&w", ch );
      send_to_char( "&RTry MAKESHIELD.\r\n&w", ch );
      return;
   }
   if( !str_cmp( arg, "wield" ) )
   {
      send_to_char( "&RAre you going to fight with your jewelry?\r\n&w", ch );
      send_to_char( "&RTry MAKEBLADE...\r\n&w", ch );
      return;
   }

   switch ( ch->substate )
   {
      default:

         if( arg2[0] == '\0' )
         {
            send_to_char( "&RUsage: Makejewelry <wearloc> <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkoven = FALSE;
         checkmetal = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_RARE_METAL )
               checkmetal = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need an oven.\r\n", ch );
            return;
         }

         if( !checkmetal )
         {
            send_to_char( "&RYou need some precious metal.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makejewelry] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of creating some jewelry.\r\n", ch );
            act( AT_PLAIN, "$n takes $s toolkit and some metal and begins to work.", ch, NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 6, do_makejewelry, 1 );
            else if( ch->subclass == SUBCLASS_TAILOR ) add_timer( ch, TIMER_DO_FUN, 9, do_makejewelry, 1 );
            else add_timer( ch, TIMER_DO_FUN, 15, do_makejewelry, 1 );
            ch->dest_buf = strdup( arg );
            ch->dest_buf_2 = strdup( arg2 );
            return;
         }
         send_to_char( "&RYou can't figure out what to do.\r\n", ch );
         learn_from_failure( ch, gsn_makejewelry );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         if( !ch->dest_buf_2 )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         strlcpy( arg2, ( const char* ) ch->dest_buf_2, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf_2 );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         DISPOSE( ch->dest_buf_2 );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makejewelry] );

   checkmetal = FALSE;
   checkoven = FALSE;
   checktool = FALSE;
   value = 0;
   cost = 0;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( obj->item_type == ITEM_RARE_METAL && checkmetal == FALSE )
      {
         checkmetal = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         metal = obj;
      }
      if( obj->item_type == ITEM_CRYSTAL )
      {
         cost += obj->cost;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makejewelry] );

   if( number_percent(  ) > schance * 2 || ( !checkoven ) || ( !checktool ) || ( !checkmetal ) )
   {
      send_to_char( "&RYou hold up your newly created jewelry.\r\n", ch );
      send_to_char( "&RIt suddenly dawns upon you that you have created the most useless\r\n", ch );
      send_to_char( "&Rpiece of junk you've ever seen. You quickly hide your mistake...\r\n", ch );
      learn_from_failure( ch, gsn_makejewelry );
      return;
   }

   obj = metal;

   obj->item_type = ITEM_ARMOR;
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   value = get_wflag( arg );
   if( value < 0 || value > 31 )
      SET_BIT( obj->wear_flags, ITEM_WEAR_NECK );
   else
      SET_BIT( obj->wear_flags, 1 << value );
   obj->level = level;
   STRFREE( obj->name );
   strlcpy( buf, arg2, MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg2, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was dropped here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   if( ch->subclass == SUBCLASS_TAILOR )
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
   obj->value[0] = obj->value[1];
   obj->cost *= 10;
   obj->cost += cost;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created jewelry.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes sewing some new jewelry.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 100,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }
   learn_from_success( ch, gsn_makejewelry );
}

void do_makearmor( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance;
   bool checksew, checkfab;
   OBJ_DATA *obj;
   OBJ_DATA *material = NULL;
   int value;

   argument = one_argument( argument, arg );
   strlcpy( arg2, argument, MAX_INPUT_LENGTH );

   if( !str_cmp( arg, "eyes" )
       || !str_cmp( arg, "ears" ) || !str_cmp( arg, "finger" ) || !str_cmp( arg, "neck" ) || !str_cmp( arg, "wrist" ) )
   {
      send_to_char( "&RYou cannot make clothing for that body part.\r\n&w", ch );
      send_to_char( "&RTry MAKEJEWELRY.\r\n&w", ch );
      return;
   }
   if( !str_cmp( arg, "shield" ) )
   {
      send_to_char( "&RYou cannot make clothing worn as a shield.\r\n&w", ch );
      send_to_char( "&RTry MAKESHIELD.\r\n&w", ch );
      return;
   }
   if( !str_cmp( arg, "wield" ) )
   {
      send_to_char( "&RAre you going to fight with your clothing?\r\n&w", ch );
      send_to_char( "&RTry MAKEBLADE...\r\n&w", ch );
      return;
   }

   switch ( ch->substate )
   {
      default:

         if( arg2[0] == '\0' )
         {
            send_to_char( "&RUsage: Makearmor <wearloc> <name>\r\n&w", ch );
            return;
         }

         checksew = FALSE;
         checkfab = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_FABRIC )
               checkfab = TRUE;
            if( obj->item_type == ITEM_THREAD )
               checksew = TRUE;
         }

         if( !checkfab )
         {
            send_to_char( "&RYou need some sort of fabric or material.\r\n", ch );
            return;
         }

         if( !checksew )
         {
            send_to_char( "&RYou need a needle and some thread.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makearmor] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of creating some armor.\r\n", ch );
            act( AT_PLAIN, "$n takes $s sewing kit and some material and begins to work.", ch, NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 6, do_makearmor, 1 );
            else if( ch->subclass == SUBCLASS_TAILOR ) add_timer( ch, TIMER_DO_FUN, 9, do_makearmor, 1 );
            else add_timer( ch, TIMER_DO_FUN, 15, do_makearmor, 1 );
            ch->dest_buf = strdup( arg );
            ch->dest_buf_2 = strdup( arg2 );
            return;
         }
         send_to_char( "&RYou can't figure out what to do.\r\n", ch );
         learn_from_failure( ch, gsn_makearmor );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         if( !ch->dest_buf_2 )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         strlcpy( arg2, ( const char* ) ch->dest_buf_2, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf_2 );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         DISPOSE( ch->dest_buf_2 );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makearmor] );

   checksew = FALSE;
   checkfab = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_THREAD )
         checksew = TRUE;
      if( obj->item_type == ITEM_FABRIC && checkfab == FALSE )
      {
         checkfab = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         material = obj;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makearmor] );

   if( number_percent(  ) > schance * 2 || ( !checkfab ) || ( !checksew ) )
   {
      send_to_char( "&RYou hold up your newly created armor.\r\n", ch );
      send_to_char( "&RIt suddenly dawns upon you that you have created the most useless\r\n", ch );
      send_to_char( "&Rgarment you've ever seen. You quickly hide your mistake...\r\n", ch );
      learn_from_failure( ch, gsn_makearmor );
      return;
   }

   obj = material;

   obj->item_type = ITEM_ARMOR;
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   value = get_wflag( arg );
   if( value < 0 || value > 31 )
      SET_BIT( obj->wear_flags, ITEM_WEAR_BODY );
   else
      SET_BIT( obj->wear_flags, 1 << value );
   obj->level = level;
   STRFREE( obj->name );
   strlcpy( buf, arg2, MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg2, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was dropped here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   if( ch->subclass == SUBCLASS_TAILOR )
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
   obj->value[0] = obj->value[1];
   obj->cost *= 10;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created garment.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes sewing some new armor.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 100,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }
   learn_from_success( ch, gsn_makearmor );
}

void do_makecomlink( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int schance;
   bool checktool, checkgem, checkbatt, checkcirc;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makecomlink <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkgem = FALSE;
         checkbatt = FALSE;
         checkcirc = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_CRYSTAL )
               checkgem = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need toolkit to make a comlink.\r\n", ch );
            return;
         }

         if( !checkgem )
         {
            send_to_char( "&RYou need a small crystal.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for your comlink.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makecomlink] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of making a comlink.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and begins to work on something.", ch, NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 5, do_makecomlink, 1 );
            else add_timer( ch, TIMER_DO_FUN, 10, do_makecomlink, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makecomlink );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   vnum = 10430;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkgem = FALSE;
   checkbatt = FALSE;
   checkcirc = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_CRYSTAL && checkgem == FALSE )
      {
         checkgem = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         checkcirc = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makecomlink] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkcirc ) || ( !checkbatt ) || ( !checkgem ) )
   {
      send_to_char( "&RYou hold up your newly created comlink....\r\n", ch );
      send_to_char( "&Rand it falls apart in your hands.\r\n", ch );
      learn_from_failure( ch, gsn_makecomlink );
      return;
   }

   obj = create_object( pObjIndex, ch->top_level );

   obj->item_type = ITEM_COMLINK;
   SET_BIT( obj->wear_flags, ITEM_HOLD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->weight = 3;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " comlink", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->cost = 50;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created comlink.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting a comlink.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 100,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }
   learn_from_success( ch, gsn_makecomlink );
}

void do_makeshield( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int schance;
   bool checktool, checkbatt, checkcond, checkcirc, checkgems;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum, level, charge, gemtype = 0;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makeshield <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkbatt = FALSE;
         checkcond = FALSE;
         checkcirc = FALSE;
         checkgems = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a workshop.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_CRYSTAL )
               checkgems = TRUE;
            if( obj->item_type == ITEM_BATTERY )
               checkbatt = TRUE;
            if( obj->item_type == ITEM_CIRCUIT )
               checkcirc = TRUE;
            if( obj->item_type == ITEM_SUPERCONDUCTOR )
               checkcond = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need toolkit to make an energy shield.\r\n", ch );
            return;
         }

         if( !checkbatt )
         {
            send_to_char( "&RYou need a power source for your energy shield.\r\n", ch );
            return;
         }

         if( !checkcirc )
         {
            send_to_char( "&RYou need a small circuit board.\r\n", ch );
            return;
         }

         if( !checkcond )
         {
            send_to_char( "&RYou still need a small superconductor for your energy shield.\r\n", ch );
            return;
         }

         if( !checkgems )
         {
            send_to_char( "&RYou need a small crystal.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeshield] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of crafting an energy shield.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and begins to work on something.", ch, NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 8, do_makeshield, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH ) add_timer( ch, TIMER_DO_FUN, 12, do_makeshield, 1 );
            else add_timer( ch, TIMER_DO_FUN, 20, do_makeshield, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to fit the parts together.\r\n", ch );
         learn_from_failure( ch, gsn_makeshield );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeshield] );
   vnum = 10429;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char
         ( "&RThe item you are trying to create is missing from the database.\r\nPlease inform the administration of this error.\r\n",
           ch );
      return;
   }

   checktool = FALSE;
   checkbatt = FALSE;
   checkcond = FALSE;
   checkcirc = FALSE;
   checkgems = FALSE;
   charge = 0;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;

      if( obj->item_type == ITEM_BATTERY && checkbatt == FALSE )
      {
         charge = UMIN( obj->value[1], 10 );
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkbatt = TRUE;
      }
      if( obj->item_type == ITEM_SUPERCONDUCTOR && checkcond == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcond = TRUE;
      }
      if( obj->item_type == ITEM_CIRCUIT && checkcirc == FALSE )
      {
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkcirc = TRUE;
      }
      if( obj->item_type == ITEM_CRYSTAL && checkgems == FALSE )
      {
         gemtype = obj->value[0];
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
         checkgems = TRUE;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeshield] );

   if( number_percent(  ) > schance * 2 || ( !checktool ) || ( !checkbatt )
       || ( !checkgems ) || ( !checkcond ) || ( !checkcirc ) )

   {
      send_to_char( "&RYou hold up your new energy shield and press the switch hoping for the best.\r\n", ch );
      send_to_char( "&RInstead of a field of energy being created, smoke starts pouring from the device.\r\n", ch );
      send_to_char( "&RYou drop the hot device and watch as it melts on away on the floor.\r\n", ch );
      learn_from_failure( ch, gsn_makeshield );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_ARMOR;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_WEAR_SHIELD );
   obj->level = level;
   obj->weight = 2;
   STRFREE( obj->name );
   obj->name = STRALLOC( "energy shield" );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was carelessly misplaced here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = ( int )( level / 10 + gemtype * 2 );  /* condition */
   obj->value[1] = ( int )( level / 10 + gemtype * 2 );  /* armor */
   if( ch->subclass == SUBCLASS_WEAPONSMITH )
      obj->value[1] = ( int )( obj->value[1] * 1.1 );
   obj->value[4] = charge;
   obj->value[5] = charge;
   obj->cost = obj->value[2] * 100;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created energy shield.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes making $s new energy shield.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 50,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }
   learn_from_success( ch, gsn_makeshield );
}

void do_makecontainer( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, schance;
   bool checksew, checkfab;
   OBJ_DATA *obj;
   OBJ_DATA *material = NULL;
   int value;

   argument = one_argument( argument, arg );
   strlcpy( arg2, argument, MAX_INPUT_LENGTH );

   if( !str_cmp( arg, "eyes" )
       || !str_cmp( arg, "ears" ) || !str_cmp( arg, "finger" ) || !str_cmp( arg, "neck" ) || !str_cmp( arg, "wrist" ) )
   {
      send_to_char( "&RYou cannot make a container for that body part.\r\n&w", ch );
      send_to_char( "&RTry MAKEJEWELRY.\r\n&w", ch );
      return;
   }
   if( !str_cmp( arg, "feet" ) || !str_cmp( arg, "hands" ) )
   {
      send_to_char( "&RYou cannot make a container for that body part.\r\n&w", ch );
      send_to_char( "&RTry MAKEARMOR.\r\n&w", ch );
      return;
   }
   if( !str_cmp( arg, "shield" ) )
   {
      send_to_char( "&RYou cannot make a container a shield.\r\n&w", ch );
      send_to_char( "&RTry MAKESHIELD.\r\n&w", ch );
      return;
   }
   if( !str_cmp( arg, "wield" ) )
   {
      send_to_char( "&RAre you going to fight with a container?\r\n&w", ch );
      send_to_char( "&RTry MAKEBLADE...\r\n&w", ch );
      return;
   }

   switch ( ch->substate )
   {
      default:

         if( arg2[0] == '\0' )
         {
            send_to_char( "&RUsage: Makecontainer <wearloc> <name>\r\n&w", ch );
            return;
         }

         checksew = FALSE;
         checkfab = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_FABRIC )
               checkfab = TRUE;
            if( obj->item_type == ITEM_THREAD )
               checksew = TRUE;
         }

         if( !checkfab )
         {
            send_to_char( "&RYou need some sort of fabric or material.\r\n", ch );
            return;
         }

         if( !checksew )
         {
            send_to_char( "&RYou need a needle and some thread.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makecontainer] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin the long process of creating a bag.\r\n", ch );
            act( AT_PLAIN, "$n takes $s sewing kit and some material and begins to work.", ch, NULL, argument, TO_ROOM );
            if( ch->subclass == SUBCLASS_QUICKWORK ) add_timer( ch, TIMER_DO_FUN, 5, do_makecontainer, 1 );
            else add_timer( ch, TIMER_DO_FUN, 10, do_makecontainer, 1 );
            ch->dest_buf = strdup( arg );
            ch->dest_buf_2 = strdup( arg2 );
            return;
         }
         send_to_char( "&RYou can't figure out what to do.\r\n", ch );
         learn_from_failure( ch, gsn_makecontainer );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         if( !ch->dest_buf_2 )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         strlcpy( arg2, ( const char* ) ch->dest_buf_2, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf_2 );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         DISPOSE( ch->dest_buf_2 );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makecontainer] );

   checksew = FALSE;
   checkfab = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_THREAD )
         checksew = TRUE;
      if( obj->item_type == ITEM_FABRIC && checkfab == FALSE )
      {
         checkfab = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         material = obj;
      }
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makecontainer] );

   if( number_percent(  ) > schance * 2 || ( !checkfab ) || ( !checksew ) )
   {
      send_to_char( "&RYou hold up your newly created container.\r\n", ch );
      send_to_char( "&RIt suddenly dawns upon you that you have created the most useless\r\n", ch );
      send_to_char( "&Rcontainer you've ever seen. You quickly hide your mistake...\r\n", ch );
      learn_from_failure( ch, gsn_makecontainer );
      return;
   }

   obj = material;

   obj->item_type = ITEM_CONTAINER;
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   value = get_wflag( arg );
   if( value < 0 || value > 31 )
      SET_BIT( obj->wear_flags, ITEM_HOLD );
   else
      SET_BIT( obj->wear_flags, 1 << value );
   obj->level = level;
   STRFREE( obj->name );
   strlcpy( buf, arg2, MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg2, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was dropped here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = level;
   obj->value[1] = 0;
   obj->value[2] = 0;
   obj->value[3] = 10;
   obj->cost *= 2;

   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created container.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes sewing a new container.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 100,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }
   learn_from_success( ch, gsn_makecontainer );
}

void do_gemcutting( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int chance, cap;
   bool checktool, checkoven;
   OBJ_DATA *obj;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RCut what gem?\r\n&w", ch );
            return;
         }

         if( ( obj = get_obj_carry( ch, arg ) ) == NULL )
         {
            send_to_char( "&RYou do not have that item.&w\r\n", ch );
            return;
         }

         if( obj->item_type != ITEM_CRYSTAL )
         {
            send_to_char( "&RTry choosing a gem to cut perhaps?&w\r\n", ch );
            return;
         }

         if( obj->value[1] != 0 )
         {
            send_to_char( "&RThat gem has already been cut.&w\r\n", ch );
            return;
         }

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         checktool = FALSE;
         checkoven = FALSE;
         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to cut a gem.\r\n", ch );
            return;
         }

         if( !checkoven )
         {
            send_to_char( "&RYou need a small furnace to work the stone.\r\n", ch );
            return;
         }

         send_to_char( "&GYou begin carefully cutting the gem.\r\n", ch );
         act( AT_PLAIN, "$n takes out $s tools and begins cutting a gem.", ch, NULL, argument, TO_ROOM );

         if( IS_IMMORTAL( ch ) )
            add_timer( ch, TIMER_DO_FUN, 1, do_gemcutting, 1 );
         else if( ch->subclass == SUBCLASS_QUICKWORK )
            add_timer( ch, TIMER_DO_FUN, 7, do_gemcutting, 1 );
         else if( ch->subclass == SUBCLASS_TAILOR )
            add_timer( ch, TIMER_DO_FUN, 27, do_gemcutting, 1 );
         else
            add_timer( ch, TIMER_DO_FUN, 30, do_gemcutting, 1 );

         ch->dest_buf = strdup( arg );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   checktool = FALSE;
   checkoven = FALSE;
   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
   }

   cap = 65;
   if( ch->subclass == SUBCLASS_TAILOR )
      cap += 10;
   if( !IS_NPC( ch ) && knows_skill( ch, gsn_improved_concentration ) )
      cap += 10;

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_gemcutting] );
   if( !IS_NPC( ch ) && knows_skill( ch, gsn_improved_concentration ) )
      chance += 50;
   chance = UMIN( chance, cap );

   /* A pure luck chance to do a little better */
   chance += number_range( 1, get_curr_lck( ch ) );

   if( ( obj = get_obj_carry( ch, arg ) ) == NULL )
   {
      send_to_char( "&RYou do not have that item.&w\r\n", ch );
      DISPOSE( ch->dest_buf );
      return;
   }

   if( obj->item_type != ITEM_CRYSTAL )
   {
      send_to_char( "&RTry choosing a gem to cut perhaps?&w\r\n", ch );
      DISPOSE( ch->dest_buf );
      return;
   }

   if( obj->value[1] != 0 )
   {
      send_to_char( "&RThat gem has already been cut.&w\r\n", ch );
      DISPOSE( ch->dest_buf );
      return;
   }

   DISPOSE( ch->dest_buf );

   if( number_percent(  ) > chance || !checktool || !checkoven )
   {
      send_to_char( "&RAs you are about to finish, your chisel slips and renders the gem useless.\r\n", ch );
      separate_obj( obj );
      obj_from_char( obj );
      extract_obj( obj );
      learn_from_failure( ch, gsn_gemcutting );
      return;
   }

   separate_obj( obj );
   obj->value[1] = 1;   /* mark as cut */
   obj->cost = obj->cost * 2;
   STRFREE( obj->short_descr );
   strlcpy( buf, "a beautifully cut gem", MAX_STRING_LENGTH );
   obj->short_descr = STRALLOC( buf );

   send_to_char( "&GYour chisel finds the perfect angle - the gem is beautifully cut.\r\n", ch );
   act( AT_PLAIN, "$n finishes cutting a gem.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain =
         UMIN( obj->cost * 200,
               ( exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) -
                 exp_level( ch->skill_level[ENGINEERING_ABILITY] ) ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_gemcutting );
}

void do_reinforcements( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int schance, credits;

   if( IS_NPC( ch ) || !ch->pcdata )
      return;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( ch->backup_wait )
         {
            send_to_char( "&RYour reinforcements are already on the way.\r\n", ch );
            return;
         }

         if( !ch->pcdata->clan )
         {
            send_to_char( "&RYou need to be a member of an organization before you can call for reinforcements.\r\n", ch );
            return;
         }

         if( ch->gold < ch->skill_level[LEADERSHIP_ABILITY] * 50 )
         {
            ch_printf( ch, "&RYou dont have enough credits to send for reinforcements.\r\n" );
            return;
         }

         schance = ( int )( ch->pcdata->learned[gsn_reinforcements] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin making the call for reinforcements.\r\n", ch );
            act( AT_PLAIN, "$n begins issuing orders int $s comlink.", ch, NULL, argument, TO_ROOM );
            add_timer( ch, TIMER_DO_FUN, 1, do_reinforcements, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou call for reinforcements but nobody answers.\r\n", ch );
         learn_from_failure( ch, gsn_reinforcements );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted before you can finish your call.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   send_to_char( "&GYour reinforcements are on the way.\r\n", ch );
   credits = ch->skill_level[LEADERSHIP_ABILITY] * 50;
   ch_printf( ch, "It cost you %d credits.\r\n", credits );
   ch->gold -= UMIN( credits, ch->gold );

   learn_from_success( ch, gsn_reinforcements );

   if( nifty_is_name( "empire", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_STORMTROOPER;
   else if( nifty_is_name( "republic", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_NR_TROOPER;
   else
      ch->backup_mob = MOB_VNUM_MERCINARY;

   ch->backup_type = 1;
   ch->backup_elite = FALSE;
   ch->backup_wait = number_range( 1, 2 );
}

void do_postguard( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int schance, credits;

   if( IS_NPC( ch ) || !ch->pcdata )
      return;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( ch->backup_wait )
         {
            send_to_char( "&RYou already have backup coming.\r\n", ch );
            return;
         }

         if( !ch->pcdata->clan )
         {
            send_to_char( "&RYou need to be a member of an organization before you can call for a guard.\r\n", ch );
            return;
         }

         if( ch->gold < ch->skill_level[LEADERSHIP_ABILITY] * 30 )
         {
            send_to_char( "&RYou dont have enough credits.\r\n", ch );
            return;
         }

         schance = ( int )( ch->pcdata->learned[gsn_postguard] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin making the call for reinforcements.\r\n", ch );
            act( AT_PLAIN, "$n begins issuing orders int $s comlink.", ch, NULL, argument, TO_ROOM );
            add_timer( ch, TIMER_DO_FUN, 1, do_postguard, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou call for a guard but nobody answers.\r\n", ch );
         learn_from_failure( ch, gsn_postguard );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted before you can finish your call.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   send_to_char( "&GYour guard is on the way.\r\n", ch );

   credits = ch->skill_level[LEADERSHIP_ABILITY] * 30;
   ch_printf( ch, "It cost you %d credits.\r\n", credits );
   ch->gold -= UMIN( credits, ch->gold );

   learn_from_success( ch, gsn_postguard );

   if( nifty_is_name( "empire", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_IMP_GUARD;
   else if( nifty_is_name( "republic", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_NR_GUARD;
   else
      ch->backup_mob = MOB_VNUM_BOUNCER;

   ch->backup_type = 2;
   ch->backup_elite = FALSE;
   ch->backup_wait = 1;
}

void add_reinforcements( CHAR_DATA * ch )
{
   MOB_INDEX_DATA *pMobIndex;
   OBJ_DATA *blaster;
   OBJ_INDEX_DATA *pObjIndex;
   double elite_mult = ch->backup_elite ? 2.0 : 1.0;

   if( ( pMobIndex = get_mob_index( ch->backup_mob ) ) == NULL )
      return;

   if( ch->backup_type == 1 )
   {
      CHAR_DATA *mob[3];
      int mob_cnt;

      send_to_char( ch->backup_elite ? "Your special forces have arrived.\r\n" : "Your reinforcements have arrived.\r\n", ch );
      for( mob_cnt = 0; mob_cnt < 3; mob_cnt++ )
      {
         int ability;
         mob[mob_cnt] = create_mobile( pMobIndex );
         char_to_room( mob[mob_cnt], ch->in_room );
         act( AT_IMMORT, "$N has arrived.", ch, NULL, mob[mob_cnt], TO_ROOM );
         mob[mob_cnt]->top_level = ch->skill_level[LEADERSHIP_ABILITY] / 3;
         for( ability = 0; ability < MAX_ABILITY; ability++ )
            mob[mob_cnt]->skill_level[ability] = mob[mob_cnt]->top_level;
         mob[mob_cnt]->hit = ( int )( mob[mob_cnt]->top_level * 15 * elite_mult );
         mob[mob_cnt]->max_hit = mob[mob_cnt]->hit;
         mob[mob_cnt]->armor = ( short )( LEVEL_HERO - mob[mob_cnt]->top_level * 2.5 );
         mob[mob_cnt]->damroll = ( int )( mob[mob_cnt]->top_level / 5 * elite_mult );
         mob[mob_cnt]->hitroll = ( int )( mob[mob_cnt]->top_level / 5 * elite_mult );
         /* Officer subclass: double troop effectiveness on top of any elite bonus */
         if( ch->subclass == SUBCLASS_OFFICER )
         {
            mob[mob_cnt]->hit     = UMIN( mob[mob_cnt]->hit * 2, 32000 );
            mob[mob_cnt]->max_hit = mob[mob_cnt]->hit;
            mob[mob_cnt]->armor   = -1300;
            mob[mob_cnt]->damroll = UMIN( mob[mob_cnt]->damroll * 2, 1000 );
            mob[mob_cnt]->hitroll = UMIN( mob[mob_cnt]->hitroll * 2, 1000 );
         }
         if( ( pObjIndex = get_obj_index( OBJ_VNUM_BLASTECH_E11 ) ) != NULL )
         {
            blaster = create_object( pObjIndex, mob[mob_cnt]->top_level );
            obj_to_char( blaster, mob[mob_cnt] );
            equip_char( mob[mob_cnt], blaster, WEAR_WIELD );
         }
         if( mob[mob_cnt]->master )
            stop_follower( mob[mob_cnt] );
         add_follower( mob[mob_cnt], ch );
         SET_BIT( mob[mob_cnt]->affected_by, AFF_CHARM );
         do_setblaster( mob[mob_cnt], "full" );
      }
   }
   else
   {
      CHAR_DATA *mob;
      int ability;
      bool stationary = ( ch->backup_type == 2 );

      mob = create_mobile( pMobIndex );
      char_to_room( mob, ch->in_room );
      if( ch->pcdata && ch->pcdata->clan )
      {
         char tmpbuf[MAX_STRING_LENGTH];

         STRFREE( mob->name );
         mob->name = STRALLOC( ch->pcdata->clan->name );
         snprintf( tmpbuf, MAX_STRING_LENGTH, "(%s) %s", ch->pcdata->clan->name, mob->long_descr );
         STRFREE( mob->long_descr );
         mob->long_descr = STRALLOC( tmpbuf );
      }
      act( AT_IMMORT, "$N has arrived.", ch, NULL, mob, TO_ROOM );
      send_to_char( stationary ? ( ch->backup_elite ? "Your elite guard has arrived.\r\n" : "Your guard has arrived.\r\n" )
                                : ( ch->backup_elite ? "Your elite patrol has arrived.\r\n" : "Your patrol has arrived.\r\n" ), ch );
      mob->top_level = ch->skill_level[LEADERSHIP_ABILITY];
      for( ability = 0; ability < MAX_ABILITY; ability++ )
         mob->skill_level[ability] = mob->top_level;
      mob->hit = ( int )( mob->top_level * 15 * elite_mult );
      mob->max_hit = mob->hit;
      mob->armor = ( short )( LEVEL_HERO - mob->top_level * 2.5 );
      mob->damroll = ( int )( mob->top_level / 5 * elite_mult );
      mob->hitroll = ( int )( mob->top_level / 5 * elite_mult );
      /* Officer subclass: guard/patrol is significantly stronger on top of any elite bonus */
      if( ch->subclass == SUBCLASS_OFFICER )
      {
         mob->hit     = UMIN( mob->hit * 2, 32000 );
         mob->max_hit = mob->hit;
         mob->armor   = -1300;
         mob->damroll = UMIN( mob->damroll * 2, 1000 );
         mob->hitroll = UMIN( mob->hitroll * 2, 1000 );
      }
      if( ( pObjIndex = get_obj_index( OBJ_VNUM_BLASTECH_E11 ) ) != NULL )
      {
         blaster = create_object( pObjIndex, mob->top_level );
         obj_to_char( blaster, mob );
         equip_char( mob, blaster, WEAR_WIELD );
      }

      /* Guards stay put; patrols are left free to wander the area on their own. */
      if( stationary )
         SET_BIT( mob->act, ACT_SENTINEL );

      /*
       * for making this more accurate in the future 
       */

      if( mob->mob_clan )
         STRFREE( mob->mob_clan );
      if( ch->pcdata && ch->pcdata->clan )
         mob->mob_clan = STRALLOC( ch->pcdata->clan->name );
   }
}

void do_torture( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   int schance, dam;
   bool fail;

   if( !IS_NPC( ch ) && ch->pcdata->learned[gsn_torture] <= 0 )
   {
      send_to_char( "Your mind races as you realize you have no idea how to do that.\r\n", ch );
      return;
   }

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't do that right now.\r\n", ch );
      return;
   }

   one_argument( argument, arg );

   if( ch->mount )
   {
      send_to_char( "You can't get close enough while mounted.\r\n", ch );
      return;
   }

   if( arg[0] == '\0' )
   {
      send_to_char( "Torture whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "Are you masacistic or what...\r\n", ch );
      return;
   }

   if( !IS_AWAKE( victim ) )
   {
      send_to_char( "You need to wake them first.\r\n", ch );
      return;
   }

   if( is_safe( ch, victim ) )
      return;

   if( victim->fighting )
   {
      send_to_char( "You can't torture someone whos in combat.\r\n", ch );
      return;
   }

   ch->alignment = ch->alignment - 100;
   ch->alignment = URANGE( -1000, ch->alignment, 1000 );

   WAIT_STATE( ch, skill_table[gsn_torture]->beats );

   fail = FALSE;
   schance = ris_save( victim, ch->skill_level[HUNTING_ABILITY], RIS_PARALYSIS );
   if( schance == 1000 )
      fail = TRUE;
   else
      fail = saves_para_petri( schance, victim );

   if( !IS_NPC( ch ) && !IS_NPC( victim ) )
      schance = sysdata.stun_plr_vs_plr;
   else
      schance = sysdata.stun_regular;
   if( !fail && ( IS_NPC( ch ) || ( number_percent(  ) + schance ) < ch->pcdata->learned[gsn_torture] ) )
   {
      learn_from_success( ch, gsn_torture );
      WAIT_STATE( ch, 2 * PULSE_VIOLENCE );
      WAIT_STATE( victim, PULSE_VIOLENCE );
      act( AT_SKILL, "$N slowly tortures you. The pain is excruciating.", victim, NULL, ch, TO_CHAR );
      act( AT_SKILL, "You torture $N, leaving $M screaming in pain.", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n tortures $N, leaving $M screaming in agony!", ch, NULL, victim, TO_NOTVICT );

      dam = dice( ch->skill_level[HUNTING_ABILITY] / 10, 4 );
      dam = URANGE( 0, victim->max_hit - 10, dam );
      victim->hit -= dam;
      victim->max_hit -= dam;

      ch_printf( victim, "You lose %d permanent hit points.", dam );
      ch_printf( ch, "They lose %d permanent hit points.", dam );

   }
   else
   {
      act( AT_SKILL, "$N tries to cut off your finger!", victim, NULL, ch, TO_CHAR );
      act( AT_SKILL, "You mess up big time.", ch, NULL, victim, TO_CHAR );
      act( AT_SKILL, "$n tries to painfully torture $N.", ch, NULL, victim, TO_NOTVICT );
      WAIT_STATE( ch, 2 * PULSE_VIOLENCE );
      global_retcode = multi_hit( victim, ch, TYPE_UNDEFINED );
   }
}

void do_disguise( CHAR_DATA * ch, const char *argument )
{
   int schance;

   if( IS_NPC( ch ) )
      return;

   if( IS_SET( ch->pcdata->flags, PCFLAG_NOTITLE ) )
   {
      send_to_char( "You try but the Force resists you.\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
   {
      send_to_char( "Change your title to what?\r\n", ch );
      return;
   }

   schance = ( int )( ch->pcdata->learned[gsn_disguise] );

   if( number_percent(  ) > schance )
   {
      send_to_char( "You try to disguise yourself but fail.\r\n", ch );
      return;
   }

   char title[50];
   strlcpy( title, argument, 50 );

   smash_tilde( title );
   set_title( ch, title );
   learn_from_success( ch, gsn_disguise );
   send_to_char( "Ok.\r\n", ch );
}

void do_mine( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   bool shovel;
   short move;

   if( ch->pcdata->learned[gsn_mine] <= 0 )
   {
      ch_printf( ch, "You have no idea how to do that.\r\n" );
      return;
   }

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      send_to_char( "And what will you mine the room with?\r\n", ch );
      return;
   }

   if( ms_find_obj( ch ) )
      return;

   shovel = FALSE;
   for( obj = ch->first_carrying; obj; obj = obj->next_content )
      if( obj->item_type == ITEM_SHOVEL )
      {
         shovel = TRUE;
         break;
      }

   obj = get_obj_list_rev( ch, arg, ch->in_room->last_content );
   if( !obj )
   {
      send_to_char( "You don't see on here.\r\n", ch );
      return;
   }

   separate_obj( obj );
   if( obj->item_type != ITEM_LANDMINE )
   {
      act( AT_PLAIN, "That's not a landmine!", ch, obj, 0, TO_CHAR );
      return;
   }

   if( !CAN_WEAR( obj, ITEM_TAKE ) )
   {
      act( AT_PLAIN, "You cannot bury $p.", ch, obj, 0, TO_CHAR );
      return;
   }

   switch ( ch->in_room->sector_type )
   {
      case SECT_CITY:
      case SECT_INSIDE:
         send_to_char( "The floor is too hard to dig through.\r\n", ch );
         return;
      case SECT_WATER_SWIM:
      case SECT_WATER_NOSWIM:
      case SECT_UNDERWATER:
         send_to_char( "You cannot bury a mine in the water.\r\n", ch );
         return;
      case SECT_AIR:
         send_to_char( "What?  Bury a mine in the air?!\r\n", ch );
         return;
   }

   if( obj->weight > ( UMAX( 5, ( can_carry_w( ch ) / 10 ) ) ) && !shovel )
   {
      send_to_char( "You'd need a shovel to bury something that big.\r\n", ch );
      return;
   }

   move = ( obj->weight * 50 * ( shovel ? 1 : 5 ) ) / UMAX( 1, can_carry_w( ch ) );
   move = URANGE( 2, move, 1000 );
   if( move > ch->move )
   {
      send_to_char( "You don't have the energy to bury something of that size.\r\n", ch );
      return;
   }
   ch->move -= move;

   SET_BIT( obj->extra_flags, ITEM_BURRIED );
   WAIT_STATE( ch, URANGE( 10, move / 2, 100 ) );

   STRFREE( obj->armed_by );
   obj->armed_by = STRALLOC( ch->name );

   ch_printf( ch, "You arm and bury %s.\r\n", obj->short_descr );
   act( AT_PLAIN, "$n arms and buries $p.", ch, obj, NULL, TO_ROOM );

   learn_from_success( ch, gsn_mine );
}

void do_first_aid( CHAR_DATA * ch, const char *argument )
{
   OBJ_DATA *medpac;
   CHAR_DATA *victim;
   int heal;
   int healm;
   char buf[MAX_STRING_LENGTH];

   if( ch->position == POS_FIGHTING && ch->subclass != SUBCLASS_PARAMEDIC )
   {
      send_to_char( "You can't do that while fighting!\r\n", ch );
      return;
   }

   medpac = get_eq_char( ch, WEAR_HOLD );
   if( !medpac || medpac->item_type != ITEM_MEDPAC )
   {
      send_to_char( "You need to be holding a medpac.\r\n", ch );
      return;
   }

   if( medpac->value[0] <= 0 )
   {
      send_to_char( "Your medpac seems to be empty.\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
      victim = ch;
   else
      victim = get_char_room( ch, argument );

   if( !victim )
   {
      ch_printf( ch, "I don't see any %s here...\r\n", argument );
      return;
   }

   heal = number_range( 1, 150 );

   if( ch->subclass == SUBCLASS_MEDIC )
      heal = ( int )( heal * 1.1 );
   if( ch->subclass == SUBCLASS_DOCTOR )
      heal = ( int )( heal * 1.5 );

   if( heal > ch->pcdata->learned[gsn_first_aid] * 2
       + ( ch->subclass == SUBCLASS_MEDIC ? 20 : 0 )
       + ( ch->subclass == SUBCLASS_DOCTOR ? 50 : 0 ) )
   {
      ch_printf( ch, "You fail in your attempt at first aid.\r\n" );
      learn_from_failure( ch, gsn_first_aid );
      return;
   }

   if( victim == ch )
   {
      ch_printf( ch, "You tend to your wounds.\r\n" );
      snprintf( buf, MAX_STRING_LENGTH, "$n uses %s to help heal $s wounds.", medpac->short_descr );
      act( AT_ACTION, buf, ch, NULL, victim, TO_ROOM );
   }
   else
   {
      act( AT_ACTION, "You tend to $N's wounds.", ch, NULL, victim, TO_CHAR );
      snprintf( buf, MAX_STRING_LENGTH,"$n uses %s to help heal $N's wounds.", medpac->short_descr );
      act( AT_ACTION, buf, ch, NULL, victim, TO_NOTVICT );
      snprintf( buf, MAX_STRING_LENGTH, "$n uses %s to help heal your wounds.", medpac->short_descr );
      act( AT_ACTION, buf, ch, NULL, victim, TO_VICT );
   }

   --medpac->value[0];
   victim->hit += URANGE( 0, heal, victim->max_hit - victim->hit );
   healm = heal * 2;
   victim->move += URANGE( 0, healm, victim->max_move - victim->move );

   {
      int xp;

      xp = victim->top_level * 50 + 1500;
      xp = UMIN( xp, exp_level( ch->skill_level[MEDICAL_ABILITY] + 1 ) - exp_level( ch->skill_level[MEDICAL_ABILITY] ) );
      gain_exp( ch, victim->top_level * 20, MEDICAL_ABILITY );
      ch_printf( ch, "You gain %d medical experience.\r\n", victim->top_level * 20 );
   }

   learn_from_success( ch, gsn_first_aid );
}

void do_snipe( CHAR_DATA * ch, const char *argument )
{
   OBJ_DATA *wield;
   char arg[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   short dir, dist;
   short max_dist = 3;
   EXIT_DATA *pexit;
   ROOM_INDEX_DATA *was_in_room;
   ROOM_INDEX_DATA *to_room;
   CHAR_DATA *victim = NULL;
   int schance;
   char buf[MAX_STRING_LENGTH];
   bool pfound = FALSE;

   if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_SNIPER )
      max_dist = 10;

   if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
   {
      set_char_color( AT_MAGIC, ch );
      send_to_char( "You'll have to do that elswhere.\r\n", ch );
      return;
   }

   if( get_eq_char( ch, WEAR_DUAL_WIELD ) != NULL )
   {
      send_to_char( "You can't do that while wielding two weapons.", ch );
      return;
   }

   wield = get_eq_char( ch, WEAR_WIELD );
   if( !wield || wield->item_type != ITEM_WEAPON || wield->value[3] != WEAPON_BLASTER )
   {
      send_to_char( "You don't seem to be holding a blaster", ch );
      return;
   }

   argument = one_argument( argument, arg );
   argument = one_argument( argument, arg2 );

   if( ( dir = get_door( arg ) ) == -1 || arg2[0] == '\0' )
   {
      send_to_char( "Usage: snipe <dir> <target>\r\n", ch );
      return;
   }

   if( ( pexit = get_exit( ch->in_room, dir ) ) == NULL )
   {
      send_to_char( "Are you expecting to fire through a wall!?\r\n", ch );
      return;
   }

   if( IS_SET( pexit->exit_info, EX_CLOSED ) )
   {
      send_to_char( "Are you expecting to fire through a door!?\r\n", ch );
      return;
   }

   was_in_room = ch->in_room;

   for( dist = 0; dist <= max_dist; dist++ )
   {
      if( IS_SET( pexit->exit_info, EX_CLOSED ) )
         break;

      if( !pexit->to_room )
         break;

      to_room = NULL;
      if( pexit->distance > 1 )
         to_room = generate_exit( ch->in_room, &pexit );

      if( to_room == NULL )
         to_room = pexit->to_room;

      char_from_room( ch );
      char_to_room( ch, to_room );


      if( IS_NPC( ch ) && ( victim = get_char_room_mp( ch, arg2 ) ) != NULL )
      {
         pfound = TRUE;
         break;
      }
      else if( !IS_NPC( ch ) && ( victim = get_char_room( ch, arg2 ) ) != NULL )
      {
         pfound = TRUE;
         break;
      }


      if( ( pexit = get_exit( ch->in_room, dir ) ) == NULL )
         break;

   }

   char_from_room( ch );
   char_to_room( ch, was_in_room );

   if( !pfound )
   {
      ch_printf( ch, "You don't see that person to the %s!\r\n", dir_name[dir] );
      char_from_room( ch );
      char_to_room( ch, was_in_room );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "Shoot yourself ... really?\r\n", ch );
      return;
   }

   if( IS_SET( victim->in_room->room_flags, ROOM_SAFE ) )
   {
      set_char_color( AT_MAGIC, ch );
      send_to_char( "You can't shoot them there.\r\n", ch );
      return;
   }

   if( is_safe( ch, victim ) )
      return;

   if( IS_AFFECTED( ch, AFF_CHARM ) && ch->master == victim )
   {
      act( AT_PLAIN, "$N is your beloved master.", ch, NULL, victim, TO_CHAR );
      return;
   }

   if( ch->position == POS_FIGHTING )
   {
      send_to_char( "You do the best you can!\r\n", ch );
      return;
   }

   if( !IS_NPC( victim ) && IS_SET( ch->act, PLR_NICE ) )
   {
      send_to_char( "You feel too nice to do that!\r\n", ch );
      return;
   }

   schance = IS_NPC( ch ) ? 100 : ( int )( ch->pcdata->learned[gsn_snipe] );

   switch ( dir )
   {
      case 0:
      case 1:
         dir += 2;
         break;
      case 2:
      case 3:
         dir -= 2;
         break;
      case 4:
      case 7:
         dir += 1;
         break;
      case 5:
      case 8:
         dir -= 1;
         break;
      case 6:
         dir += 3;
         break;
      case 9:
         dir -= 3;
         break;
   }

   if( number_percent(  ) < schance )
   {
      char_from_room( ch );
      char_to_room( ch, was_in_room );
      snprintf( buf, MAX_STRING_LENGTH, "$n fires a blaster shot to the %s.", dir_name[get_door(arg)] );
      act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );

      char_from_room( ch );
      char_to_room( ch, victim->in_room );

      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_SNIPER )
      {
         /* 5% one-shot-kill chance for Snipers */
         int osk_chance = 5; /* was + (IS_AFFECTED(ch, AFF_SCRYING) ? 5 : 0) -- disabled; AFF_SCRYING's bit (BV24) was later reclaimed for AFF_TRUCE (force_stun), and both are now gone entirely (v1.47/v1.48) - BV24 is unclaimed */
         if( number_percent(  ) <= osk_chance )
         {
            act( AT_ACTION,
                 "You whisper, one shot, one kill, as you line up $N with your scope.",
                 ch, NULL, victim, TO_CHAR );
            if( !IS_IMMORTAL( victim ) )
               raw_kill( ch, victim );
            else
               damage( ch, victim, 32700, gsn_snipe );
            char_from_room( ch );
            char_to_room( ch, was_in_room );
            learn_from_success( ch, gsn_snipe );
            goto snipe_cleanup;
         }
         /* Triple shot */
         snprintf( buf, MAX_STRING_LENGTH, "A flurry of shots fires at you from the %s.", dir_name[dir] );
         act( AT_ACTION, buf, victim, NULL, ch, TO_CHAR );
         act( AT_ACTION, "You squeeze three shots off at $N.", ch, NULL, victim, TO_CHAR );
         snprintf( buf, MAX_STRING_LENGTH, "A trio of shots fires at $N from the %s.", dir_name[dir] );
         act( AT_ACTION, buf, ch, NULL, victim, TO_NOTVICT );
         one_hit( ch, victim, TYPE_UNDEFINED );
         if( !char_died( ch ) && who_fighting( ch ) == victim )
            one_hit( ch, victim, TYPE_UNDEFINED );
         if( !char_died( ch ) && who_fighting( ch ) == victim )
            one_hit( ch, victim, TYPE_UNDEFINED );
      }
      else
      {
         snprintf( buf, MAX_STRING_LENGTH, "A blaster shot fires at you from the %s.", dir_name[dir] );
         act( AT_ACTION, buf, victim, NULL, ch, TO_CHAR );
         act( AT_ACTION, "You fire at $N.", ch, NULL, victim, TO_CHAR );
         snprintf( buf, MAX_STRING_LENGTH, "A blaster shot fires at $N from the %s.", dir_name[dir] );
         act( AT_ACTION, buf, ch, NULL, victim, TO_NOTVICT );
         one_hit( ch, victim, TYPE_UNDEFINED );
      }

      if( char_died( ch ) )
         return;

      stop_fighting( ch, TRUE );

      learn_from_success( ch, gsn_snipe );
   }
   else
   {
      char_from_room( ch );
      char_to_room( ch, was_in_room );
      snprintf( buf, MAX_STRING_LENGTH, "$n fires a blaster shot to the %s.", dir_name[get_door(arg)] );
      act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );

      char_from_room( ch );
      char_to_room( ch, victim->in_room );

      act( AT_ACTION, "You fire at $N but don't even come close.", ch, NULL, victim, TO_CHAR );
      snprintf( buf, MAX_STRING_LENGTH, "A blaster shot fired from the %s barely misses you.", dir_name[dir] );
      act( AT_ACTION, buf, ch, NULL, victim, TO_ROOM );
      learn_from_failure( ch, gsn_snipe );
   }

   snipe_cleanup:

   char_from_room( ch );
   char_to_room( ch, was_in_room );

   if( IS_NPC( ch ) )
      WAIT_STATE( ch, 1 * PULSE_VIOLENCE );
   else
   {
      if( number_percent(  ) < ch->pcdata->learned[gsn_third_attack] )
         WAIT_STATE( ch, 1 * PULSE_PER_SECOND );
      else if( number_percent(  ) < ch->pcdata->learned[gsn_second_attack] )
         WAIT_STATE( ch, 2 * PULSE_PER_SECOND );
      else
         WAIT_STATE( ch, 3 * PULSE_PER_SECOND );
   }
   if( IS_NPC( victim ) && !char_died( victim ) )
   {
      if( IS_SET( victim->act, ACT_SENTINEL ) )
      {
         victim->was_sentinel = victim->in_room;
         REMOVE_BIT( victim->act, ACT_SENTINEL );
      }

      start_hating( victim, ch );
      start_hunting( victim, ch );

   }
}

/* syntax throw <obj> [direction] [target] */
void do_throw( CHAR_DATA * ch, const char *argument )
{
   OBJ_DATA *obj;
   OBJ_DATA *tmpobj;
   char arg[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   char arg3[MAX_INPUT_LENGTH];
   short dir;
   EXIT_DATA *pexit;
   ROOM_INDEX_DATA *was_in_room;
   ROOM_INDEX_DATA *to_room;
   CHAR_DATA *victim;
   char buf[MAX_STRING_LENGTH];
   int weapon_type = 0;
   int bonus_sn = -1;
   int weapon_bonus = 0;
   bool melee_weapon = FALSE;
   int telekinetic_throwing = 0;

   argument = one_argument( argument, arg );
   argument = one_argument( argument, arg2 );
   argument = one_argument( argument, arg3 );

   was_in_room = ch->in_room;

   if( arg[0] == '\0' )
   {
      send_to_char( "Usage: throw <object> [direction] [target]\r\n", ch );
      return;
   }

   obj = get_eq_char( ch, WEAR_MISSILE_WIELD );
   if( !obj || !nifty_is_name( arg, obj->name ) )
      obj = get_eq_char( ch, WEAR_HOLD );
   if( !obj || !nifty_is_name( arg, obj->name ) )
      obj = get_eq_char( ch, WEAR_WIELD );
   if( !obj || !nifty_is_name( arg, obj->name ) )
      obj = get_eq_char( ch, WEAR_DUAL_WIELD );
   if( !obj || !nifty_is_name( arg, obj->name ) )
      if( !obj || !nifty_is_name_prefix( arg, obj->name ) )
         obj = get_eq_char( ch, WEAR_HOLD );
   if( !obj || !nifty_is_name_prefix( arg, obj->name ) )
      obj = get_eq_char( ch, WEAR_WIELD );
   if( !obj || !nifty_is_name_prefix( arg, obj->name ) )
      obj = get_eq_char( ch, WEAR_DUAL_WIELD );
   if( !obj || !nifty_is_name_prefix( arg, obj->name ) )
   {
      ch_printf( ch, "You don't seem to be holding or wielding %s.\r\n", arg );
      return;
   }

   if( IS_OBJ_STAT( obj, ITEM_NOREMOVE ) )
   {
      act( AT_PLAIN, "You can't throw $p.", ch, obj, NULL, TO_CHAR );
      return;
   }

   /* Throwing mastery / telekinetic setup - v1.17 */
   if( obj->item_type == ITEM_WEAPON )
   {
      weapon_type = obj->value[3];
      bonus_sn = throwing_mastery_skill( weapon_type );
      melee_weapon = is_melee_weapon( weapon_type );
      if( IS_VALID_SN( bonus_sn ) && !IS_NPC( ch ) )
         weapon_bonus = ch->pcdata->learned[bonus_sn];
   }

   if( ch->position == POS_FIGHTING )
   {
      victim = who_fighting( ch );
      if( char_died( victim ) )
         return;
      act( AT_ACTION, "You throw $p at $N.", ch, obj, victim, TO_CHAR );
      act( AT_ACTION, "$n throws $p at $N.", ch, obj, victim, TO_NOTVICT );
      act( AT_ACTION, "$n throw $p at you.", ch, obj, victim, TO_VICT );
   }
   else if( arg2[0] == '\0' )
   {
      snprintf( buf, MAX_STRING_LENGTH, "$n throws %s at the floor.", obj->short_descr );
      act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );
      ch_printf( ch, "You throw %s at the floor.\r\n", obj->short_descr );

      victim = NULL;
   }
   else if( ( dir = get_door( arg2 ) ) != -1 )
   {
      if( ( pexit = get_exit( ch->in_room, dir ) ) == NULL )
      {
         send_to_char( "Are you expecting to throw it through a wall!?\r\n", ch );
         return;
      }

      if( IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         send_to_char( "Are you expecting to throw it  through a door!?\r\n", ch );
         return;
      }

      switch ( dir )
      {
         case 0:
         case 1:
            dir += 2;
            break;
         case 2:
         case 3:
            dir -= 2;
            break;
         case 4:
         case 7:
            dir += 1;
            break;
         case 5:
         case 8:
            dir -= 1;
            break;
         case 6:
            dir += 3;
            break;
         case 9:
            dir -= 3;
            break;
      }

      to_room = NULL;
      if( pexit->distance > 1 )
         to_room = generate_exit( ch->in_room, &pexit );

      if( to_room == NULL )
         to_room = pexit->to_room;

      char_from_room( ch );
      char_to_room( ch, to_room );

      victim = get_char_room( ch, arg3 );

      if( victim )
      {
         if( is_safe( ch, victim ) )
            return;

         if( IS_AFFECTED( ch, AFF_CHARM ) && ch->master == victim )
         {
            act( AT_PLAIN, "$N is your beloved master.", ch, NULL, victim, TO_CHAR );
            return;
         }

         if( !IS_NPC( victim ) && IS_SET( ch->act, PLR_NICE ) )
         {
            send_to_char( "You feel too nice to do that!\r\n", ch );
            return;
         }

         char_from_room( ch );
         char_to_room( ch, was_in_room );

         if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
         {
            set_char_color( AT_MAGIC, ch );
            send_to_char( "You'll have to do that elswhere.\r\n", ch );
            return;
         }

         to_room = NULL;
         if( pexit->distance > 1 )
            to_room = generate_exit( ch->in_room, &pexit );

         if( to_room == NULL )
            to_room = pexit->to_room;

         char_from_room( ch );
         char_to_room( ch, to_room );

         snprintf( buf, MAX_STRING_LENGTH, "Someone throws %s at you from the %s.", obj->short_descr, dir_name[dir] );
         act( AT_ACTION, buf, victim, NULL, ch, TO_CHAR );
         act( AT_ACTION, "You throw $p at $N.", ch, obj, victim, TO_CHAR );
         char_from_room( ch );
         char_to_room( ch, was_in_room );
         snprintf( buf, MAX_STRING_LENGTH, "$n throws %s to the %s.", obj->short_descr, dir_name[get_dir(arg2)] );
         act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );
         char_from_room( ch );
         char_to_room( ch, to_room );
         snprintf( buf, MAX_STRING_LENGTH, "%s is thrown at $N from the %s.", obj->short_descr, dir_name[dir] );
         act( AT_ACTION, buf, ch, NULL, victim, TO_NOTVICT );
      }
      else
      {
         ch_printf( ch, "You throw %s %s.\r\n", obj->short_descr, dir_name[get_dir( arg2 )] );
         char_from_room( ch );
         char_to_room( ch, was_in_room );
         snprintf( buf, MAX_STRING_LENGTH, "$n throws %s to the %s.", obj->short_descr, dir_name[get_dir(arg2)] );
         act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );
         char_from_room( ch );
         char_to_room( ch, to_room );
         snprintf( buf, MAX_STRING_LENGTH, "%s is thrown from the %s.", obj->short_descr, dir_name[dir] );
         act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );
      }
   }
   else if( ( victim = get_char_room( ch, arg2 ) ) != NULL )
   {
      if( is_safe( ch, victim ) )
         return;

      if( IS_AFFECTED( ch, AFF_CHARM ) && ch->master == victim )
      {
         act( AT_PLAIN, "$N is your beloved master.", ch, NULL, victim, TO_CHAR );
         return;
      }

      if( !IS_NPC( victim ) && IS_SET( ch->act, PLR_NICE ) )
      {
         send_to_char( "You feel too nice to do that!\r\n", ch );
         return;
      }

   }
   else
   {
      ch_printf( ch, "They don't seem to be here!\r\n" );
      return;
   }

   if( obj == get_eq_char( ch, WEAR_WIELD ) && ( tmpobj = get_eq_char( ch, WEAR_DUAL_WIELD ) ) != NULL )
      tmpobj->wear_loc = WEAR_WIELD;

/* NOT NEEDED UNLESS REFERING TO OBJECT AGAIN 

   if( obj_extracted(obj) )
      return;
*/
   if( ch->in_room != was_in_room )
   {
      char_from_room( ch );
      char_to_room( ch, was_in_room );
   }

   if( !victim || char_died( victim ) )
      learn_from_failure( ch, gsn_throw );
   else
   {
      int throw_dt;

      if( bonus_sn == gsn_entangling_throw )
         throw_dt = gsn_entangling_throw;
      else if( weapon_type > 0 && weapon_type < (int)( sizeof( attack_table ) / sizeof( attack_table[0] ) ) )
         throw_dt = TYPE_HIT + weapon_type;
      else
         throw_dt = TYPE_HIT;

      WAIT_STATE( ch, skill_table[gsn_throw]->beats );
      if( IS_NPC( ch ) || number_percent(  ) < ch->pcdata->learned[gsn_throw] + ( weapon_bonus / 5 ) )
      {
         learn_from_success( ch, gsn_throw );
         if( IS_VALID_SN( bonus_sn ) )
            learn_from_success( ch, bonus_sn );
         {
            int throw_dam = number_range( obj->weight * 2, ( obj->weight * 2 + ch->perm_str ) );
            int deadeye_bonus = IS_NPC( ch ) ? 0 : ch->pcdata->learned[gsn_deadeye];

            if( deadeye_bonus )
            {
               throw_dam = ( int )( throw_dam * ( 1 + deadeye_bonus / 100.0 ) );
               learn_from_success( ch, gsn_deadeye );
            }
            global_retcode = damage( ch, victim, throw_dam, throw_dt );
         }
      }
      else
      {
         learn_from_failure( ch, gsn_throw );
         if( IS_VALID_SN( bonus_sn ) )
            learn_from_failure( ch, bonus_sn );
         global_retcode = damage( ch, victim, 0, TYPE_HIT );
      }

      if( IS_NPC( victim ) && !char_died( victim ) )
      {
         if( IS_SET( victim->act, ACT_SENTINEL ) )
         {
            victim->was_sentinel = victim->in_room;
            REMOVE_BIT( victim->act, ACT_SENTINEL );
         }

         start_hating( victim, ch );
         start_hunting( victim, ch );

      }

   }

   /* Telekinetic throwing - v1.17. Only melee weapons can be pulled back,
    * and never grenades (they're meant to be released). Costs mana, gated
    * by skill percent same as any other roll. */
   telekinetic_throwing = ( melee_weapon && !IS_NPC( ch ) && obj->item_type != ITEM_GRENADE )
      ? ch->pcdata->learned[gsn_telekinetic_throwing] : 0;

   if( telekinetic_throwing > 0 && ch->mana >= skill_table[gsn_telekinetic_throwing]->min_mana )
   {
      ch->mana = UMAX( 0, ch->mana - skill_table[gsn_telekinetic_throwing]->min_mana );

      if( number_percent(  ) > telekinetic_throwing )
      {
         /* Failed the pull - weapon flies away and lands normally. */
         learn_from_failure( ch, gsn_telekinetic_throwing );
         act( AT_ACTION, "You extend your hand as if to pull $p back, but it continues to fly away.",
              ch, obj, NULL, TO_CHAR );
         unequip_char( ch, obj );
         separate_obj( obj );
         obj_from_char( obj );
         obj = obj_to_room( obj, ch->in_room );

         if( obj->item_type != ITEM_GRENADE )
         {
            if( weapon_bonus == 0 || number_percent(  ) > weapon_bonus )
               damage_obj( obj );
         }
      }
      else
      {
         /* Success - the weapon never actually left your hand. */
         learn_from_success( ch, gsn_telekinetic_throwing );
         act( AT_ACTION, "You extend your hand and $p flies right back into it.", ch, obj, NULL, TO_CHAR );
         if( obj->wear_loc == WEAR_WIELD && get_eq_char( ch, WEAR_DUAL_WIELD ) == NULL )
            obj->wear_loc = WEAR_DUAL_WIELD;
         /* No unequip/obj_to_room, no wear - it stays equipped and undamaged. */
      }
   }
   else
   {
      unequip_char( ch, obj );
      separate_obj( obj );
      obj_from_char( obj );
      obj = obj_to_room( obj, ch->in_room );

      /* Weapon mastery reduces wear on a well-thrown weapon - v1.17. */
      if( obj->item_type != ITEM_GRENADE )
      {
         if( weapon_bonus == 0 || number_percent(  ) > weapon_bonus )
            damage_obj( obj );
      }
   }

}

void do_beg( CHAR_DATA * ch, const char *argument )
{
   char buf[MAX_STRING_LENGTH];
   char arg1[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   int percent, xp;
   int amount;

   if( IS_NPC( ch ) )
      return;

   argument = one_argument( argument, arg1 );

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( arg1[0] == '\0' )
   {
      send_to_char( "Beg fo money from whom?\r\n", ch );
      return;
   }

   if( ms_find_obj( ch ) )
      return;

   if( ( victim = get_char_room( ch, arg1 ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "That's pointless.\r\n", ch );
      return;
   }

   if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
   {
      set_char_color( AT_MAGIC, ch );
      send_to_char( "This isn't a good place to do that.\r\n", ch );
      return;
   }

   if( ch->position == POS_FIGHTING )
   {
      send_to_char( "Interesting combat technique.\r\n", ch );
      return;
   }

   if( victim->position == POS_FIGHTING )
   {
      send_to_char( "They're a little busy right now.\r\n", ch );
      return;
   }

   if( ch->position <= POS_SLEEPING )
   {
      send_to_char( "In your dreams or what?\r\n", ch );
      return;
   }

   if( victim->position <= POS_SLEEPING )
   {
      send_to_char( "You might want to wake them first...\r\n", ch );
      return;
   }

   if( !IS_NPC( victim ) )
   {
      send_to_char( "You beg them for money.\r\n", ch );
      act( AT_ACTION, "$n begs you to give $s some change.\r\n", ch, NULL, victim, TO_VICT );
      act( AT_ACTION, "$n begs $N for change.\r\n", ch, NULL, victim, TO_NOTVICT );
      return;
   }

   WAIT_STATE( ch, skill_table[gsn_beg]->beats );
   percent = number_percent(  ) + ch->skill_level[SMUGGLING_ABILITY] + victim->top_level;

   if( percent > ch->pcdata->learned[gsn_beg] )
   {
      /*
       * Failure.
       */
      send_to_char( "You beg them for money but don't get any!\r\n", ch );
      act( AT_ACTION, "$n is really getting on your nerves with all this begging!\r\n", ch, NULL, victim, TO_VICT );
      act( AT_ACTION, "$n begs $N for money.\r\n", ch, NULL, victim, TO_NOTVICT );

      if( victim->alignment < 0 && victim->top_level >= ch->top_level + 5 )
      {
         snprintf( buf, MAX_STRING_LENGTH, "%s is an annoying beggar and needs to be taught a lesson!", ch->name );
         do_yell( victim, buf );
         global_retcode = multi_hit( victim, ch, TYPE_UNDEFINED );
      }

      learn_from_failure( ch, gsn_beg );

      return;
   }

   act( AT_ACTION, "$n begs $N for money.\r\n", ch, NULL, victim, TO_NOTVICT );
   act( AT_ACTION, "$n begs you for money!\r\n", ch, NULL, victim, TO_VICT );

   amount = UMIN( victim->gold, number_range( 1, 10 ) );
   if( amount <= 0 )
   {
      do_look( victim, ch->name );
      do_say( victim, "Sorry I have nothing to spare.\r\n" );
      learn_from_failure( ch, gsn_beg );
      return;
   }

   ch->gold += amount;
   victim->gold -= amount;
   ch_printf( ch, "%s gives you %d credits.\r\n", victim->short_descr, amount );
   learn_from_success( ch, gsn_beg );
   xp =
      UMIN( amount * 10,
            ( exp_level( ch->skill_level[SMUGGLING_ABILITY] + 1 ) - exp_level( ch->skill_level[SMUGGLING_ABILITY] ) ) );
   xp = UMIN( xp, xp_compute( ch, victim ) );
   gain_exp( ch, xp, SMUGGLING_ABILITY );
   ch_printf( ch, "&WYou gain %d smuggling experience points!\r\n", xp );
   act( AT_ACTION, "$N gives $n some money.\r\n", ch, NULL, victim, TO_NOTVICT );
   act( AT_ACTION, "You give $n some money.\r\n", ch, NULL, victim, TO_VICT );
}

void do_pickshiplock( CHAR_DATA * ch, const char *argument )
{
   do_pick( ch, argument );
}

void do_hijack( CHAR_DATA * ch, const char *argument )
{
   int schance;
   SHIP_DATA *ship;
   char buf[MAX_STRING_LENGTH];

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the cockpit of a ship to do that!\r\n", ch );
      return;
   }

   if( ship->ship_class > SHIP_PLATFORM )
   {
      send_to_char( "&RThis isn't a spacecraft!\r\n", ch );
      return;
   }

   if( ( ship = ship_from_pilotseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou don't seem to be in the pilot seat!\r\n", ch );
      return;
   }

   if( check_pilot( ch, ship ) )
   {
      send_to_char( "&RWhat would be the point of that!\r\n", ch );
      return;
   }

   if( ship->type == MOB_SHIP && get_trust( ch ) < 102 )
   {
      send_to_char( "&RThis ship isn't pilotable by mortals at this point in time...\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "You can't do that here.\r\n", ch );
      return;
   }

   if( ship->lastdoc != ship->location )
   {
      send_to_char( "&rYou don't seem to be docked right now.\r\n", ch );
      return;
   }

   if( ship->shipstate != SHIP_DOCKED && ship->shipstate != SHIP_DISABLED )
   {
      send_to_char( "The ship is not docked right now.\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_DISABLED )
   {
      send_to_char( "The ships drive is disabled .\r\n", ch );
      return;
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_hijack] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "You fail to figure out the correct launch code.\r\n", ch );
      learn_from_failure( ch, gsn_hijack );
      return;
   }

   if( ship->ship_class == FIGHTER_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_starfighters] );
   if( ship->ship_class == MIDSIZE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_midships] );
   if( ship->ship_class == CAPITAL_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_capitalships] );
   if( number_percent(  ) < schance )
   {

      if( ship->hatchopen )
      {
         ship->hatchopen = FALSE;
         snprintf( buf, MAX_STRING_LENGTH, "The hatch on %s closes.", ship->name );
         echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
         echo_to_room( AT_YELLOW, get_room_index( ship->entrance ), "The hatch slides shut." );
      }
      set_char_color( AT_GREEN, ch );
      send_to_char( "Launch sequence initiated.\r\n", ch );
      act( AT_PLAIN, "$n starts up the ship and begins the launch sequence.", ch, NULL, argument, TO_ROOM );
      echo_to_ship( AT_YELLOW, ship, "The ship hums as it lifts off the ground." );
      snprintf( buf, MAX_STRING_LENGTH, "%s begins to launch.", ship->name );
      echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
      ship->shipstate = SHIP_LAUNCH;
      ship->currspeed = ship->realspeed;
      if( ship->ship_class == FIGHTER_SHIP )
         learn_from_success( ch, gsn_starfighters );
      if( ship->ship_class == MIDSIZE_SHIP )
         learn_from_success( ch, gsn_midships );
      if( ship->ship_class == CAPITAL_SHIP )
         learn_from_success( ch, gsn_capitalships );

      learn_from_success( ch, gsn_hijack );
      snprintf( buf, MAX_STRING_LENGTH, "%s has been hijacked!", ship->name );
      echo_to_all( AT_RED, buf, 0 );

      return;
   }
   set_char_color( AT_RED, ch );
   send_to_char( "You fail to work the controls properly!\r\n", ch );
   if( ship->ship_class == FIGHTER_SHIP )
      learn_from_failure( ch, gsn_starfighters );
   if( ship->ship_class == MIDSIZE_SHIP )
      learn_from_failure( ch, gsn_midships );
   if( ship->ship_class == CAPITAL_SHIP )
      learn_from_failure( ch, gsn_capitalships );
}

/*
 * Dismiss a summoned clan NPC (patrol, guard, reinforcements, etc.) -
 * v1.19, ported from SWGD leadership.c do_dismiss. Adapted: SWGD gates
 * this on RANK_OFFICER/RANK_LEADER clan authority, but this codebase's
 * summon commands (add_patrol, elite_patrol, etc.) don't check rank
 * authority at all - just clan membership - and nothing else in this
 * codebase currently gates actions on clan->ranks[]->authority, so this
 * matches that same simpler local convention for consistency.
 */
void do_dismiss( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;

   if( IS_NPC( ch ) )
      return;

   if( !ch->pcdata->clan )
   {
      send_to_char( "Dismiss whom on what authority?\r\n", ch );
      return;
   }

   if( argument[0] == '\0' || ( victim = get_char_room( ch, argument ) ) == NULL )
   {
      send_to_char( "Dismiss whom?\r\n", ch );
      return;
   }

   if( !IS_NPC( victim ) )
   {
      send_to_char( "Very funny.\r\n", ch );
      return;
   }

   if( victim->mob_clan && victim->mob_clan[0] != '\0' && str_cmp( victim->mob_clan, ch->pcdata->clan->name ) )
   {
      send_to_char( "You can only dismiss people from your organization.\r\n", ch );
      return;
   }

   act( AT_MAGIC, "You dismiss $N.", ch, NULL, victim, TO_CHAR );
   act( AT_MAGIC, "$n dismisses $N.", ch, NULL, victim, TO_NOTVICT );
   if( victim->master )
      stop_follower( victim );
   extract_char( victim, TRUE );
}

void do_add_patrol( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int schance, credits;

   if( IS_NPC( ch ) || !ch->pcdata )
      return;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( ch->backup_wait )
         {
            send_to_char( "&RYou already have a patrol coming.\r\n", ch );
            return;
         }

         if( !ch->pcdata->clan )
         {
            send_to_char( "&RYou need to be a member of an organization before you can call for a patrol.\r\n", ch );
            return;
         }

         if( ch->gold < ch->skill_level[LEADERSHIP_ABILITY] * 30 )
         {
            send_to_char( "&RYou dont have enough credits.\r\n", ch );
            return;
         }

         schance = ( int )( ch->pcdata->learned[gsn_addpatrol] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin making the call for a patrol.\r\n", ch );
            act( AT_PLAIN, "$n begins issuing orders int $s comlink.", ch, NULL, argument, TO_ROOM );
            add_timer( ch, TIMER_DO_FUN, 1, do_add_patrol, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou call for a patrol but nobody answers.\r\n", ch );
         learn_from_failure( ch, gsn_addpatrol );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted before you can finish your call.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   send_to_char( "&GYour patrol is on the way.\r\n", ch );

   credits = ch->skill_level[LEADERSHIP_ABILITY] * 30;
   ch_printf( ch, "It cost you %d credits.\r\n", credits );
   ch->gold -= UMIN( credits, ch->gold );

   learn_from_success( ch, gsn_addpatrol );

   if( nifty_is_name( "empire", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_IMP_PATROL;
   else if( nifty_is_name( "republic", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_NR_PATROL;
   else
      ch->backup_mob = MOB_VNUM_MERC_PATROL;

   ch->backup_type = 3;
   ch->backup_elite = FALSE;
   ch->backup_wait = 1;
}

void do_special_forces( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int schance, credits;

   if( IS_NPC( ch ) || !ch->pcdata )
      return;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( ch->backup_wait )
         {
            send_to_char( "&RYour special forces are already on the way.\r\n", ch );
            return;
         }

         if( !ch->pcdata->clan )
         {
            send_to_char( "&RYou need to be a member of an organization before you can call for special forces.\r\n", ch );
            return;
         }

         if( ch->gold < ch->skill_level[LEADERSHIP_ABILITY] * 60 )
         {
            ch_printf( ch, "&RYou dont have enough credits to send for special forces.\r\n" );
            return;
         }

         schance = ( int )( ch->pcdata->learned[gsn_specialforces] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin making the call for special forces.\r\n", ch );
            act( AT_PLAIN, "$n begins issuing orders int $s comlink.", ch, NULL, argument, TO_ROOM );
            add_timer( ch, TIMER_DO_FUN, 1, do_special_forces, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou call for special forces but nobody answers.\r\n", ch );
         learn_from_failure( ch, gsn_specialforces );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted before you can finish your call.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   send_to_char( "&GYour reinforcements are on the way.\r\n", ch );
   credits = ch->skill_level[LEADERSHIP_ABILITY] * 60;
   ch_printf( ch, "It cost you %d credits.\r\n", credits );
   ch->gold -= UMIN( credits, ch->gold );

   learn_from_success( ch, gsn_specialforces );

   if( nifty_is_name( "empire", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_STORMTROOPER;
   else if( nifty_is_name( "republic", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_NR_TROOPER;
   else
      ch->backup_mob = MOB_VNUM_MERCINARY;

   ch->backup_type = 1;
   ch->backup_elite = TRUE;
   ch->backup_wait = number_range( 1, 2 );
}

void do_elite_guard( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int schance, credits;

   if( IS_NPC( ch ) || !ch->pcdata )
      return;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( ch->backup_wait )
         {
            send_to_char( "&RYou already have elite backup coming.\r\n", ch );
            return;
         }

         if( !ch->pcdata->clan )
         {
            send_to_char( "&RYou need to be a member of an organization before you can call for an elite guard.\r\n", ch );
            return;
         }

         if( ch->gold < ch->skill_level[LEADERSHIP_ABILITY] * 50 )
         {
            ch_printf( ch, "&RYou dont have enough credits.\r\n" );
            return;
         }

         schance = ( int )( ch->pcdata->learned[gsn_eliteguard] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin making the call for elite reinforcements.\r\n", ch );
            act( AT_PLAIN, "$n begins issuing orders int $s comlink.", ch, NULL, argument, TO_ROOM );
            add_timer( ch, TIMER_DO_FUN, 1, do_elite_guard, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou call for a guard but nobody answers.\r\n", ch );
         learn_from_failure( ch, gsn_eliteguard );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted before you can finish your call.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   send_to_char( "&GYour guard is on the way.\r\n", ch );

   credits = ch->skill_level[LEADERSHIP_ABILITY] * 50;
   ch_printf( ch, "It cost you %d credits.\r\n", credits );
   ch->gold -= UMIN( credits, ch->gold );

   learn_from_success( ch, gsn_eliteguard );

   if( nifty_is_name( "empire", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_IMP_GUARD;
   else if( nifty_is_name( "republic", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_NR_GUARD;
   else
      ch->backup_mob = MOB_VNUM_BOUNCER;

   ch->backup_type = 2;
   ch->backup_elite = TRUE;
   ch->backup_wait = 1;
}

void do_jail( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim = NULL;
   CLAN_DATA *clan = NULL;
   ROOM_INDEX_DATA *jail = NULL;

   if( IS_NPC( ch ) )
      return;

   if( !ch->pcdata || ( clan = ch->pcdata->clan ) == NULL )
   {
      send_to_char( "Only members of organizations can jail their enemies.\r\n", ch );
      return;
   }

   jail = get_room_index( clan->jail );
   if( !jail && clan->mainclan )
      jail = get_room_index( clan->mainclan->jail );

   if( !jail )
   {
      send_to_char( "Your orginization does not have a suitable prison.\r\n", ch );
      return;
   }

   if( jail->area && ch->in_room->area
       && jail->area != ch->in_room->area && ( !jail->area->planet || jail->area->planet != ch->in_room->area->planet ) )
   {
      send_to_char( "Your orginizations prison is to far away.\r\n", ch );
      return;
   }

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
   {
      send_to_char( "Jail who?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, argument ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "That's pointless.\r\n", ch );
      return;
   }

   if( IS_NPC( victim ) )
   {
      send_to_char( "That would be a waste of time.\r\n", ch );
      return;
   }

   if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
   {
      set_char_color( AT_MAGIC, ch );
      send_to_char( "This isn't a good place to do that.\r\n", ch );
      return;
   }

   if( ch->position == POS_FIGHTING )
   {
      send_to_char( "Interesting combat technique.\r\n", ch );
      return;
   }

   if( ch->position <= POS_SLEEPING )
   {
      send_to_char( "In your dreams or what?\r\n", ch );
      return;
   }

   if( victim->position >= POS_SLEEPING )
   {
      send_to_char( "You will have to stun them first.\r\n", ch );
      return;
   }

   send_to_char( "You have them escorted off to jail.\r\n", ch );
   act( AT_ACTION, "You have a strange feeling that you've been moved.\r\n", ch, NULL, victim, TO_VICT );
   act( AT_ACTION, "$n has $N escorted away.\r\n", ch, NULL, victim, TO_NOTVICT );

   char_from_room( victim );
   char_to_room( victim, jail );

   act( AT_ACTION, "The door opens briefly as $n is shoved into the room.\r\n", victim, NULL, NULL, TO_ROOM );

   learn_from_success( ch, gsn_jail );
}

void do_smalltalk( CHAR_DATA * ch, const char *argument )
{
   char buf[MAX_STRING_LENGTH];
   char arg1[MAX_INPUT_LENGTH];
   CHAR_DATA *victim = NULL;
   PLANET_DATA *planet = NULL;
   CLAN_DATA *clan = NULL;
   int percent = 0;

   if( IS_NPC( ch ) || !ch->pcdata )
   {
      send_to_char( "What would be the point of that.\r\n", ch );
   }

   argument = one_argument( argument, arg1 );

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( arg1[0] == '\0' )
   {
      send_to_char( "Create smalltalk with whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg1 ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "That's pointless.\r\n", ch );
      return;
   }

   if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
   {
      set_char_color( AT_MAGIC, ch );
      send_to_char( "This isn't a good place to do that.\r\n", ch );
      return;
   }

   if( ch->position == POS_FIGHTING )
   {
      send_to_char( "Interesting combat technique.\r\n", ch );
      return;
   }

   if( victim->position == POS_FIGHTING )
   {
      send_to_char( "They're a little busy right now.\r\n", ch );
      return;
   }


   if( !IS_NPC( victim ) || victim->vip_flags == 0 )
   {
      send_to_char( "Diplomacy would be wasted on them.\r\n", ch );
      return;
   }

   if( ch->position <= POS_SLEEPING )
   {
      send_to_char( "In your dreams or what?\r\n", ch );
      return;
   }

   if( victim->position <= POS_SLEEPING )
   {
      send_to_char( "You might want to wake them first...\r\n", ch );
      return;
   }

   WAIT_STATE( ch, skill_table[gsn_smalltalk]->beats );

   if( percent - ch->skill_level[DIPLOMACY_ABILITY] + victim->top_level > ch->pcdata->learned[gsn_smalltalk] )
   {
      /*
       * Failure.
       */
      send_to_char( "You attempt to make smalltalk with them.. but are ignored.\r\n", ch );
      act( AT_ACTION, "$n is really getting on your nerves with all this chatter!\r\n", ch, NULL, victim, TO_VICT );
      act( AT_ACTION, "$n asks $N about the weather but is ignored.\r\n", ch, NULL, victim, TO_NOTVICT );

      if( victim->alignment < -500 && victim->top_level >= ch->top_level + 5 )
      {
         snprintf( buf, MAX_STRING_LENGTH, "SHUT UP %s!", ch->name );
         do_yell( victim, buf );
         global_retcode = multi_hit( victim, ch, TYPE_UNDEFINED );
      }

      return;
   }

   send_to_char( "You strike up a short conversation with them.\r\n", ch );
   act( AT_ACTION, "$n smiles at you and says, 'hello'.\r\n", ch, NULL, victim, TO_VICT );
   act( AT_ACTION, "$n chats briefly with $N.\r\n", ch, NULL, victim, TO_NOTVICT );

   if( IS_NPC( ch ) || !ch->pcdata || !ch->pcdata->clan || !ch->in_room->area || !ch->in_room->area->planet )
      return;

   if( ( clan = ch->pcdata->clan->mainclan ) == NULL )
      clan = ch->pcdata->clan;

   planet = ch->in_room->area->planet;

   if( clan != planet->governed_by )
      return;

   planet->pop_support += 0.2;
   send_to_char( "Popular support for your organization increases slightly.\r\n", ch );

   gain_exp( ch, victim->top_level * 10, DIPLOMACY_ABILITY );
   ch_printf( ch, "You gain %d diplomacy experience.\r\n", victim->top_level * 10 );

   learn_from_success( ch, gsn_smalltalk );

   if( planet->pop_support > 100 )
      planet->pop_support = 100;
}

void do_propeganda( CHAR_DATA * ch, const char *argument )
{
   char buf[MAX_STRING_LENGTH];
   char arg1[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   PLANET_DATA *planet;
   CLAN_DATA *clan;
   int percent = 0;

   if( IS_NPC( ch ) || !ch->pcdata || !ch->pcdata->clan || !ch->in_room->area || !ch->in_room->area->planet )
   {
      send_to_char( "What would be the point of that.\r\n", ch );
      return;
   }

   argument = one_argument( argument, arg1 );

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( arg1[0] == '\0' )
   {
      send_to_char( "Spread propeganda to who?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg1 ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "That's pointless.\r\n", ch );
      return;
   }

   if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
   {
      set_char_color( AT_MAGIC, ch );
      send_to_char( "This isn't a good place to do that.\r\n", ch );
      return;
   }

   if( ch->position == POS_FIGHTING )
   {
      send_to_char( "Interesting combat technique.\r\n", ch );
      return;
   }

   if( victim->position == POS_FIGHTING )
   {
      send_to_char( "They're a little busy right now.\r\n", ch );
      return;
   }


   if( victim->vip_flags == 0 )
   {
      send_to_char( "Diplomacy would be wasted on them.\r\n", ch );
      return;
   }

   if( ch->position <= POS_SLEEPING )
   {
      send_to_char( "In your dreams or what?\r\n", ch );
      return;
   }

   if( victim->position <= POS_SLEEPING )
   {
      send_to_char( "You might want to wake them first...\r\n", ch );
      return;
   }

   if( ( clan = ch->pcdata->clan->mainclan ) == NULL )
      clan = ch->pcdata->clan;

   planet = ch->in_room->area->planet;

   snprintf( buf, MAX_STRING_LENGTH, ", and the evils of %s", planet->governed_by ? planet->governed_by->name : "their current leaders" );
   ch_printf( ch, "You speak to them about the benifits of the %s%s.\r\n", ch->pcdata->clan->name,
              planet->governed_by == clan ? "" : buf );
   act( AT_ACTION, "$n speaks about his organization.\r\n", ch, NULL, victim, TO_VICT );
   act( AT_ACTION, "$n tells $N about their organization.\r\n", ch, NULL, victim, TO_NOTVICT );

   WAIT_STATE( ch, skill_table[gsn_propeganda]->beats );

   if( percent - get_curr_cha( ch ) + victim->top_level > ch->pcdata->learned[gsn_propeganda] )
   {

      if( planet->governed_by != clan )
      {
         snprintf( buf, MAX_STRING_LENGTH, "%s is a traitor!", ch->name );
         do_yell( victim, buf );
         global_retcode = multi_hit( victim, ch, TYPE_UNDEFINED );
      }

      return;
   }

   if( planet->governed_by == clan )
   {
      planet->pop_support += ( ch->subclass == SUBCLASS_SENATOR )
         ? .5 + ch->top_level / 50 * 2
         : .5 + ch->top_level / 50;
      send_to_char( "Popular support for your organization increases.\r\n", ch );
   }
   else
   {
      planet->pop_support -= ( ch->subclass == SUBCLASS_SENATOR )
         ? ch->top_level / 50 * 2
         : ch->top_level / 50;
      send_to_char( "Popular support for the current government decreases.\r\n", ch );
   }

   gain_exp( ch, victim->top_level * 100, DIPLOMACY_ABILITY );
   ch_printf( ch, "You gain %d diplomacy experience.\r\n", victim->top_level * 100 );

   learn_from_success( ch, gsn_propeganda );

   if( planet->pop_support > 100 )
      planet->pop_support = 100;
   if( planet->pop_support < -100 )
      planet->pop_support = -100;
}

void do_bribe( CHAR_DATA * ch, const char *argument )
{
   char arg1[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   PLANET_DATA *planet;
   CLAN_DATA *clan;
   int percent = 0, amount;

   if( IS_NPC( ch ) || !ch->pcdata || !ch->pcdata->clan || !ch->in_room->area || !ch->in_room->area->planet )
   {
      send_to_char( "What would be the point of that.\r\n", ch );
      return;
   }

   argument = one_argument( argument, arg1 );

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
   {
      send_to_char( "Bribe who how much?\r\n", ch );
      return;
   }

   amount = atoi( argument );

   if( ( victim = get_char_room( ch, arg1 ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "That's pointless.\r\n", ch );
      return;
   }

   if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
   {
      set_char_color( AT_MAGIC, ch );
      send_to_char( "This isn't a good place to do that.\r\n", ch );
      return;
   }

   if( amount <= 0 )
   {
      send_to_char( "A little bit more money would be a good plan.\r\n", ch );
      return;
   }

   if( ch->position == POS_FIGHTING )
   {
      send_to_char( "Interesting combat technique.\r\n", ch );
      return;
   }

   if( victim->position == POS_FIGHTING )
   {
      send_to_char( "They're a little busy right now.\r\n", ch );
      return;
   }

   if( ch->position <= POS_SLEEPING )
   {
      send_to_char( "In your dreams or what?\r\n", ch );
      return;
   }

   if( victim->position <= POS_SLEEPING )
   {
      send_to_char( "You might want to wake them first...\r\n", ch );
      return;
   }

   if( victim->vip_flags == 0 )
   {
      send_to_char( "Diplomacy would be wasted on them.\r\n", ch );
      return;
   }

   if( ch->gold < amount )
   {
      send_to_char( "You don't have that much money!\r\n", ch );
      return;
   }

   ch->gold -= amount;
   victim->gold += amount;

   ch_printf( ch, "You give them a small gift on behalf of %s.\r\n", ch->pcdata->clan->name );
   act( AT_ACTION, "$n offers you a small bribe.\r\n", ch, NULL, victim, TO_VICT );
   act( AT_ACTION, "$n gives $N some money.\r\n", ch, NULL, victim, TO_NOTVICT );

   if( !IS_NPC( victim ) )
      return;

   WAIT_STATE( ch, skill_table[gsn_bribe]->beats );

   if( percent - amount + victim->top_level > ch->pcdata->learned[gsn_bribe] )
      return;

   if( ( clan = ch->pcdata->clan->mainclan ) == NULL )
      clan = ch->pcdata->clan;

   planet = ch->in_room->area->planet;


   if( clan == planet->governed_by )
   {
     planet->pop_support += URANGE( ( int ) 0.1, amount / 1000, 2 );
     send_to_char( "Popular support for your organization increases slightly.\r\n", ch );

     amount =
       UMIN( amount,
	     ( exp_level( ch->skill_level[DIPLOMACY_ABILITY] + 1 ) - exp_level( ch->skill_level[DIPLOMACY_ABILITY] ) ) );

     gain_exp( ch, amount, DIPLOMACY_ABILITY );
     ch_printf( ch, "You gain %d diplomacy experience.\r\n", amount );

     learn_from_success( ch, gsn_bribe );
   }

   if( planet->pop_support > 100 )
      planet->pop_support = 100;
}

void do_seduce( CHAR_DATA * ch, const char *argument )
{
}

void do_mass_propeganda( CHAR_DATA * ch, const char *argument )
{
   char buf[MAX_STRING_LENGTH];
   char arg1[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   PLANET_DATA *planet;
   CLAN_DATA *clan;
   int percent = 0, xp;

   if( IS_NPC( ch ) || !ch->pcdata || !ch->pcdata->clan || !ch->in_room->area || !ch->in_room->area->planet )
   {
      send_to_char( "What would be the point of that?\r\n", ch );
      return;
   }

   argument = one_argument( argument, arg1 );

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( arg1[0] == '\0' )
   {
      send_to_char( "Spread mass propaganda to whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg1 ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "That's pointless.\r\n", ch );
      return;
   }

   if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
   {
      set_char_color( AT_MAGIC, ch );
      send_to_char( "This isn't a good place to do that.\r\n", ch );
      return;
   }

   if( ch->position == POS_FIGHTING )
   {
      send_to_char( "Interesting combat technique.\r\n", ch );
      return;
   }

   if( victim->position == POS_FIGHTING )
   {
      send_to_char( "They're a little busy right now.\r\n", ch );
      return;
   }

   if( victim->vip_flags == 0 )
   {
      send_to_char( "Diplomacy would be wasted on them.\r\n", ch );
      return;
   }

   if( ch->position <= POS_SLEEPING )
   {
      send_to_char( "In your dreams or what?\r\n", ch );
      return;
   }

   if( victim->position <= POS_SLEEPING )
   {
      send_to_char( "You might want to wake them first...\r\n", ch );
      return;
   }

   if( ( clan = ch->pcdata->clan->mainclan ) == NULL )
      clan = ch->pcdata->clan;

   planet = ch->in_room->area->planet;

   snprintf( buf, MAX_STRING_LENGTH, ", and the evils of %s",
             planet->governed_by ? planet->governed_by->name : "their current leaders" );
   ch_printf( ch, "You deliver a rousing speech about the %s%s.\r\n", ch->pcdata->clan->name,
              planet->governed_by == clan ? "" : buf );
   act( AT_ACTION, "$n delivers a rousing political speech.", ch, NULL, victim, TO_VICT );
   act( AT_ACTION, "$n tells $N about their organization.", ch, NULL, victim, TO_NOTVICT );

   WAIT_STATE( ch, skill_table[gsn_masspropeganda]->beats );

   if( percent - get_curr_cha( ch ) + victim->top_level > ch->pcdata->learned[gsn_masspropeganda] )
   {
      if( planet->governed_by != clan )
      {
         snprintf( buf, MAX_STRING_LENGTH, "%s is a traitor!", ch->name );
         do_yell( victim, buf );
         global_retcode = multi_hit( victim, ch, TYPE_UNDEFINED );
      }
      return;
   }

   if( planet->governed_by == clan )
   {
      planet->pop_support += ( ch->subclass == SUBCLASS_SENATOR )
         ? 5 + ch->top_level / 50 * 2
         : 5 + ch->top_level / 50;
      send_to_char( "Popular support for your organization increases significantly.\r\n", ch );
   }
   else
   {
      planet->pop_support -= ( ch->subclass == SUBCLASS_SENATOR )
         ? 5 + ch->top_level / 50 * 2
         : 5 + ch->top_level / 50;
      send_to_char( "Popular support for the current government decreases significantly.\r\n", ch );
   }

   xp = 5000 + victim->top_level * 200;
   xp = UMIN( xp, exp_level( ch->skill_level[DIPLOMACY_ABILITY] + 1 ) - exp_level( ch->skill_level[DIPLOMACY_ABILITY] ) );
   gain_exp( ch, xp, DIPLOMACY_ABILITY );
   ch_printf( ch, "You gain %d diplomacy experience.\r\n", xp );

   learn_from_success( ch, gsn_masspropeganda );

   if( planet->pop_support > 100 )
      planet->pop_support = 100;
   if( planet->pop_support < -100 )
      planet->pop_support = -100;
}

void do_gather_intelligence( CHAR_DATA * ch, const char *argument )
{
   AFFECT_DATA af;
   CHAR_DATA *victim;
   int chance;

   if( argument[0] == '\0' )
   {
      send_to_char( "Research whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_world( ch, argument ) ) == NULL
       || victim == ch
       || !victim->in_room
       || IS_NPC( victim ) )
   {
      send_to_char( "Your target cannot be found.\r\n", ch );
      return;
   }

   chance = IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_gather_intelligence];
   WAIT_STATE( ch, PULSE_VIOLENCE );

   if( number_percent(  ) >= chance )
   {
      ch_printf( ch, "You fail to dig up any useful information on %s.\r\n", victim->name );
      learn_from_failure( ch, gsn_gather_intelligence );
      return;
   }

   if( victim->subclass == SUBCLASS_STEALTH_HUNT || victim->subclass == SUBCLASS_SNEAK )
   {
      ch_printf( ch, "It is impossible to dig up information on %s.\r\n", victim->name );
      learn_from_success( ch, gsn_gather_intelligence );
      return;
   }

   if( IS_IMMORTAL( victim ) && victim->top_level > ch->top_level )
   {
      af.type      = gsn_gather_intelligence;
      af.location  = APPLY_HITROLL;
      af.modifier  = -10;
      af.duration  = 5;
      af.bitvector = AFF_BLIND;
      affect_to_char( ch, &af );
      send_to_char( "You are blinded by your target's aura!\r\n", ch );
      learn_from_failure( ch, gsn_gather_intelligence );
      return;
   }

   ch_printf( ch, "&wYour research has revealed the following:\r\n" );
   ch_printf( ch, "&wTitle:    %s&w\r\n", victim->pcdata->title );
   ch_printf( ch, "&wRace: %-15s  Subclass: %s\r\n",
              capitalize( get_race( victim ) ), subclasses[victim->subclass] );
   if( victim->pcdata->clan_name && victim->pcdata->clan_name[0] != '\0' )
      ch_printf( ch, "&wAffiliation: %s\r\n", victim->pcdata->clan_name );
   ch_printf( ch, "&wHitpoints: %d/%d  Movement: %d/%d\r\n",
              victim->hit, victim->max_hit, victim->move, victim->max_move );
   if( IS_EVIL( victim ) && is_affected( victim, gsn_maskaura ) )
      ch_printf( ch, "&wArmor: %d  Hitroll: %d  Damroll: %d  Alignment: &Wmasked&w\r\n",
                 GET_AC( victim ), GET_HITROLL( victim ), GET_DAMROLL( victim ) );
   else
      ch_printf( ch, "&wArmor: %d  Hitroll: %d  Damroll: %d  Alignment: %d\r\n",
                 GET_AC( victim ), GET_HITROLL( victim ), GET_DAMROLL( victim ), victim->alignment );

   learn_from_success( ch, gsn_gather_intelligence );
}

/*
 * Battlecry - ported from SWGD (v1.18). Confirmed against source: requires
 * active combat, level is drawn from LEADERSHIP_ABILITY (not combat), and
 * the resulting affect is stripped automatically when the fight ends
 * (free_fight() in fight.c already checks is_affected(ch, gsn_battlecry) -
 * that hook predates this patch and was just waiting for something to
 * strip). The buff itself below is a reconstruction, not verified against
 * the rest of the SWGD function body - only the opening validation and the
 * LEADERSHIP_ABILITY level source were confirmed.
 */
void do_battlecry( CHAR_DATA * ch, const char *argument )
{
   int level, chance;
   CHAR_DATA *gch;

   if( !ch->fighting )
   {
      send_to_char( "But you aren't fighting!\r\n", ch );
      return;
   }

   level = IS_NPC( ch ) ? ch->top_level : ch->skill_level[LEADERSHIP_ABILITY];
   chance = IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_battlecry];

   WAIT_STATE( ch, skill_table[gsn_battlecry]->beats );

   if( number_percent(  ) > chance )
   {
      learn_from_failure( ch, gsn_battlecry );
      act( AT_SKILL, "You try to rally your allies, but your cry falls flat.", ch, NULL, NULL, TO_CHAR );
      act( AT_SKILL, "$n tries to bellow a battlecry, but it falls flat.", ch, NULL, NULL, TO_ROOM );
      return;
   }

   act( AT_SKILL, "You let out a fierce battlecry, rallying everyone in your group!", ch, NULL, NULL, TO_CHAR );
   act( AT_SKILL, "$n lets out a fierce battlecry, rallying $s group!", ch, NULL, NULL, TO_ROOM );

   /* Group-wide effect - confirmed against SWGD help text: affects every
    * person in your group, usually lasting until the fight ends. */
   for( gch = ch->in_room->first_person; gch; gch = gch->next_in_room )
   {
      AFFECT_DATA af;

      if( !is_same_group( gch, ch ) )
         continue;

      af.type = gsn_battlecry;
      af.duration = -1;   /* stripped by free_fight() when combat ends */
      af.location = APPLY_HITROLL;
      af.modifier = UMIN( 10, ( int )( level / 15 ) );
      af.bitvector = 0;
      affect_to_char( gch, &af );

      af.location = APPLY_DAMROLL;
      af.modifier = UMIN( 10, ( int )( level / 15 ) );
      affect_to_char( gch, &af );

      if( gch != ch )
         act( AT_SKILL, "$n's battlecry steels your resolve!", ch, NULL, gch, TO_VICT );
   }

   learn_from_success( ch, gsn_battlecry );
}

/* =====================================================================
 * Medical crafting and field-use suite, ported from SWL/GD medic.c
 * ===================================================================== */

void do_makemedpac( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, chance, timer;
   bool checktool, checkdura, checkchem, checkoven, checkneedle, checkfab, checkdrink;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makemedpac <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkneedle = FALSE;
         checkoven = FALSE;
         checkfab = FALSE;
         checkchem = FALSE;
         checkdrink = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_THREAD )
               checkneedle = TRUE;
            if( obj->item_type == ITEM_CHEMICAL )
               checkchem = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_FABRIC )
               checkfab = TRUE;
            if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 )
               checkdrink = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make a medpac.\r\n", ch );
            return;
         }
         if( !checkdura )
         {
            send_to_char( "&RYou need some durasteel to make the shell.\r\n", ch );
            return;
         }
         if( !checkfab )
         {
            send_to_char( "&RYou need fabric to make the bandages out of.\r\n", ch );
            return;
         }
         if( !checkneedle )
         {
            send_to_char( "&RYou need a needle to sew your bandages.\r\n", ch );
            return;
         }
         if( !checkchem )
         {
            send_to_char( "&RYou need chemicals to prepare for the treatments.\r\n", ch );
            return;
         }
         if( !checkdrink )
         {
            send_to_char( "&RYou need empty vials to save the chemicals in.\r\n", ch );
            return;
         }
         if( !checkoven )
         {
            send_to_char( "&RYou need an oven to prepare the chemicals.\r\n", ch );
            return;
         }

         chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makemedpac] );
         if( number_percent(  ) < chance )
         {
            send_to_char( "&GYou begin the long process of preparing a medpac.\r\n", ch );
            act( AT_PLAIN, "$n begins preparing something.", ch, NULL, argument, TO_ROOM );
            if( IS_IMMORTAL( ch ) )
               timer = 1;
            else if( ch->subclass == SUBCLASS_QUICKWORK )
               timer = 10;
            else if( ch->subclass == SUBCLASS_DOCTOR || ch->subclass == SUBCLASS_MEDIC )
               timer = 9;
            else
               timer = 25;
            add_timer( ch, TIMER_DO_FUN, timer, do_makemedpac, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to prepare the medpac.\r\n", ch );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interrupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->skill_level[MEDICAL_ABILITY] : ( int )( ch->pcdata->learned[gsn_makemedpac] );
   vnum = 10439;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char( "&RThe item you are trying to create is missing from the database.\r\n"
                    "Please inform the administration of this error.\r\n", ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkneedle = FALSE;
   checkoven = FALSE;
   checkfab = FALSE;
   checkchem = FALSE;
   checkdrink = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( obj->item_type == ITEM_DURASTEEL && !checkdura )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_THREAD )
         checkneedle = TRUE;
      if( obj->item_type == ITEM_CHEMICAL && !checkchem )
      {
         checkchem = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_FABRIC && !checkfab )
      {
         checkfab = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 && !checkdrink )
      {
         checkdrink = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
   }

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makemedpac] );
   level = chance;
   if( ch->subclass == SUBCLASS_MEDIC )
   {
      level += 10;
      chance += 10;
   }
   if( ch->subclass == SUBCLASS_DOCTOR )
   {
      level += 50;
      chance += 50;
   }

   if( number_percent(  ) > chance * 2 || !checktool || !checkdura || !checkneedle
       || !checkoven || !checkfab || !checkchem || !checkdrink )
   {
      send_to_char( "&RYou inspect your new medpac.\r\n", ch );
      send_to_char( "&RIt is lacking proper medicines and bandages.\r\n", ch );
      send_to_char( "&RIt is unsatisfactory, so you trash it.\r\n", ch );
      learn_from_failure( ch, gsn_makemedpac );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_MEDPAC;
   SET_BIT( obj->wear_flags, ITEM_HOLD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 3;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " medpac medkit", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = level / 5;
   obj->value[1] = level / 5;
   obj->cost = obj->value[0] * 150;
   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created medpac.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting a medpac.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain = UMIN( 10000, exp_level( ch->skill_level[MEDICAL_ABILITY] + 1 ) - exp_level( ch->skill_level[MEDICAL_ABILITY] ) );
      gain_exp( ch, xpgain, MEDICAL_ABILITY );
      ch_printf( ch, "You gain %ld medical experience.", xpgain );
   }

   learn_from_success( ch, gsn_makemedpac );
}

void do_makeinoculator( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, chance, timer;
   bool checktool, checkdura, checkoven, checkneedle, checkdrink;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makeinoculator <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkneedle = FALSE;
         checkoven = FALSE;
         checkdrink = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_DURASTEEL )
               checkdura = TRUE;
            if( obj->item_type == ITEM_THREAD )
               checkneedle = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 )
               checkdrink = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to make an inoculator.\r\n", ch );
            return;
         }
         if( !checkdura )
         {
            send_to_char( "&RYou need some durasteel to make the casing.\r\n", ch );
            return;
         }
         if( !checkneedle )
         {
            send_to_char( "&RYou need a needle for the tip.\r\n", ch );
            return;
         }
         if( !checkdrink )
         {
            send_to_char( "&RYou need empty vials for the cartridge casing.\r\n", ch );
            return;
         }
         if( !checkoven )
         {
            send_to_char( "&RYou need an oven to make the casing.\r\n", ch );
            return;
         }

         chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeinoculator] );
         if( number_percent(  ) < chance )
         {
            send_to_char( "&GYou begin the long process of crafting an inoculator.\r\n", ch );
            act( AT_PLAIN, "$n begins preparing something.", ch, NULL, argument, TO_ROOM );
            if( IS_IMMORTAL( ch ) )
               timer = 1;
            else if( ch->subclass == SUBCLASS_QUICKWORK )
               timer = 10;
            else if( ch->subclass == SUBCLASS_DOCTOR || ch->subclass == SUBCLASS_MEDIC )
               timer = 9;
            else
               timer = 25;
            add_timer( ch, TIMER_DO_FUN, timer, do_makeinoculator, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou cannot figure out how to put the inoculator together.\r\n", ch );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interrupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->skill_level[MEDICAL_ABILITY] : ( int )( ch->pcdata->learned[gsn_makeinoculator] );
   vnum = 10440;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char( "&RThe item you are trying to create is missing from the database.\r\n"
                    "Please inform the administration of this error.\r\n", ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkneedle = FALSE;
   checkoven = FALSE;
   checkdrink = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( obj->item_type == ITEM_DURASTEEL && !checkdura )
      {
         checkdura = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_THREAD && !checkneedle )
      {
         checkneedle = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
      if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 && !checkdrink )
      {
         checkdrink = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
   }

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeinoculator] );
   level = chance;
   if( ch->subclass == SUBCLASS_MEDIC )
   {
      level += 10;
      chance += 10;
   }
   if( ch->subclass == SUBCLASS_DOCTOR )
   {
      level += 50;
      chance += 50;
   }

   if( number_percent(  ) > chance * 2 || !checktool || !checkdura || !checkneedle || !checkoven || !checkdrink )
   {
      send_to_char( "&RYou inspect your new inoculator.\r\n", ch );
      send_to_char( "&RThe casing is faulty and the chamber is cracked.\r\n", ch );
      send_to_char( "&RIt is unsatisfactory, so you trash it.\r\n", ch );
      learn_from_failure( ch, gsn_makeinoculator );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_INOCULATOR;
   SET_BIT( obj->wear_flags, ITEM_HOLD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 2;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " inoculator", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = level / 5;
   obj->value[1] = 0;
   obj->value[2] = 0;
   obj->value[3] = 0;
   obj->value[4] = 0;
   obj->cost = obj->value[0] * 150;
   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created inoculator.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting an inoculator.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain = UMIN( 30000, exp_level( ch->skill_level[MEDICAL_ABILITY] + 1 ) - exp_level( ch->skill_level[MEDICAL_ABILITY] ) );
      gain_exp( ch, xpgain, MEDICAL_ABILITY );
      ch_printf( ch, "You gain %ld medical experience.", xpgain );
   }

   learn_from_success( ch, gsn_makeinoculator );
}

void do_makeshot( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, chance, timer;
   bool checktool, checkdrink, checkoven;
   OBJ_DATA *obj, *medsrc = NULL;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;
   int med_type = MEDICINE_NONE, med_type2 = MEDICINE_NONE;
   int med_str = 0, med_str2 = 0;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makeshot <medicine>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkoven = FALSE;
         checkdrink = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         if( ( obj = get_obj_carry( ch, argument ) ) == NULL )
         {
            send_to_char( "&RYou do not have that item.&w\r\n", ch );
            return;
         }
         if( obj->item_type != ITEM_MEDICINE )
         {
            send_to_char( "That isn't a medicine.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 )
               checkdrink = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to prepare a shot.\r\n", ch );
            return;
         }
         if( !checkdrink )
         {
            send_to_char( "&RYou need an empty vial for the cartridge casing.\r\n", ch );
            return;
         }
         if( !checkoven )
         {
            send_to_char( "&RYou need an oven to make the casing.\r\n", ch );
            return;
         }

         chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeshot] );
         if( number_percent(  ) < chance )
         {
            send_to_char( "&GYou begin the long process of preparing a shot.\r\n", ch );
            act( AT_PLAIN, "$n begins preparing something.", ch, NULL, argument, TO_ROOM );
            if( IS_IMMORTAL( ch ) )
               timer = 1;
            else if( ch->subclass == SUBCLASS_QUICKWORK )
               timer = 10;
            else if( ch->subclass == SUBCLASS_DOCTOR || ch->subclass == SUBCLASS_MEDIC )
               timer = 9;
            else
               timer = 25;
            add_timer( ch, TIMER_DO_FUN, timer, do_makeshot, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to prepare the shot.\r\n", ch );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interrupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->skill_level[MEDICAL_ABILITY] : ( int )( ch->pcdata->learned[gsn_makeshot] );
   vnum = 10441;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char( "&RThe item you are trying to create is missing from the database.\r\n"
                    "Please inform the administration of this error.\r\n", ch );
      return;
   }

   if( ( medsrc = get_obj_carry( ch, arg ) ) == NULL )
   {
      send_to_char( "&RYou do not have that item.&w\r\n", ch );
      return;
   }
   if( medsrc->item_type != ITEM_MEDICINE )
   {
      send_to_char( "That isn't a medicine. BUG OR LACK OF OBJ.\r\n", ch );
      return;
   }

   med_type = medsrc->value[0];
   med_str = medsrc->value[1];
   med_type2 = medsrc->value[2];
   med_str2 = medsrc->value[3];

   checktool = FALSE;
   checkoven = FALSE;
   checkdrink = FALSE;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 && !checkdrink )
      {
         checkdrink = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
   }

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makeshot] );
   level = chance;
   if( ch->subclass == SUBCLASS_MEDIC )
   {
      level += 10;
      chance += 10;
   }
   if( ch->subclass == SUBCLASS_DOCTOR )
   {
      level += 50;
      chance += 50;
   }

   if( number_percent(  ) > chance * 2 || !checktool || !checkoven || !checkdrink )
   {
      send_to_char( "&RYou inspect your prepared shot.\r\n", ch );
      send_to_char( "&RThe dosage is unstable.\r\n", ch );
      send_to_char( "&RIt is unsatisfactory, so you trash it.\r\n", ch );
      learn_from_failure( ch, gsn_makeshot );
      separate_obj( medsrc );
      obj_from_char( medsrc );
      extract_obj( medsrc );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_SHOT;
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 1;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " shot cartridge", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " was left here.", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   obj->value[0] = med_type;
   obj->value[1] = med_str;
   obj->value[2] = med_type2;
   obj->value[3] = med_str2;
   obj->cost = med_str * 25 + med_str2 * 25 + 100;
   obj = obj_to_char( obj, ch );

   separate_obj( medsrc );
   obj_from_char( medsrc );
   extract_obj( medsrc );

   send_to_char( "&GYou finish your work and hold up your newly prepared shot.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes preparing a shot.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain = UMIN( 12000, exp_level( ch->skill_level[MEDICAL_ABILITY] + 1 ) - exp_level( ch->skill_level[MEDICAL_ABILITY] ) );
      gain_exp( ch, xpgain, MEDICAL_ABILITY );
      ch_printf( ch, "You gain %ld medical experience.", xpgain );
   }

   learn_from_success( ch, gsn_makeshot );
}

void do_loadinoculator( CHAR_DATA * ch, const char *argument )
{
   OBJ_DATA *inoculator;
   OBJ_DATA *obj;
   bool checkammo = FALSE;

   inoculator = get_eq_char( ch, WEAR_HOLD );
   if( !inoculator || inoculator->item_type != ITEM_INOCULATOR )
   {
      send_to_char( "You aren't holding an inoculator to load up.\r\n", ch );
      return;
   }
   if( inoculator->value[0] < 1 )
   {
      send_to_char( "That inoculator is useless.\r\n", ch );
      return;
   }
   if( argument[0] == '\0' )
   {
      send_to_char( "&RUsage: Loadinoculator <obj>&w\r\n", ch );
      return;
   }
   if( ( obj = get_obj_carry( ch, argument ) ) == NULL )
   {
      send_to_char( "&RYou do not have that item.&w\r\n", ch );
      return;
   }
   if( obj->item_type == ITEM_SHOT )
   {
      checkammo = TRUE;
      separate_obj( obj );
      inoculator->value[1] = obj->value[0];
      inoculator->value[2] = obj->value[1];
      inoculator->value[3] = obj->value[2];
      inoculator->value[4] = obj->value[3];
      obj_from_char( obj );
      extract_obj( obj );
   }

   if( checkammo )
      send_to_char( "You insert the shot successfully, and it clicks into place.\r\n", ch );
   else
      send_to_char( "That is not a suitable shot to load the inoculator with.\r\n", ch );
}

void do_inoculate( CHAR_DATA * ch, const char *argument )
{
   OBJ_DATA *inoculator;
   CHAR_DATA *victim;
   char buf[MAX_STRING_LENGTH];
   int percent, xp;
   ch_ret retcode;

   if( ch->position == POS_FIGHTING && ch->subclass != SUBCLASS_PARAMEDIC )
   {
      send_to_char( "You can't do that while fighting!\r\n", ch );
      return;
   }

   inoculator = get_eq_char( ch, WEAR_HOLD );
   if( !inoculator || inoculator->item_type != ITEM_INOCULATOR )
   {
      send_to_char( "You need to be holding an inoculator.\r\n", ch );
      return;
   }

   if( inoculator->value[1] == 0 && inoculator->value[3] == 0 )
   {
      send_to_char( "Your inoculator cartridge seems to be empty.\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
      victim = ch;
   else
      victim = get_char_room( ch, argument );
   if( !victim )
   {
      ch_printf( ch, "I don't see any %s here...\r\n", argument );
      return;
   }

   percent = IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_inoculate];
   percent += ch->subclass == SUBCLASS_MEDIC ? 10 : 0;
   percent += ch->subclass == SUBCLASS_DOCTOR ? 50 : 0;
   if( number_percent(  ) > percent )
   {
      send_to_char( "You fail to find a proper vein.\r\n", ch );
      WAIT_STATE( ch, PULSE_PER_SECOND );
      learn_from_failure( ch, gsn_inoculate );
      return;
   }

   if( victim == ch )
   {
      ch_printf( ch, "You shoot yourself in the arm.\r\n" );
      snprintf( buf, MAX_STRING_LENGTH, "$n uses %s to inoculate $s own arm.", inoculator->short_descr );
      act( AT_ACTION, buf, ch, NULL, victim, TO_ROOM );
   }
   else
   {
      act( AT_ACTION, "You shoot $N's arm up.", ch, NULL, victim, TO_CHAR );
      snprintf( buf, MAX_STRING_LENGTH, "$n uses %s to inoculate $N.", inoculator->short_descr );
      act( AT_ACTION, buf, ch, NULL, victim, TO_NOTVICT );
      snprintf( buf, MAX_STRING_LENGTH, "$n uses %s to inoculate your arm.", inoculator->short_descr );
      act( AT_ACTION, buf, ch, NULL, victim, TO_VICT );
   }

   --inoculator->value[0];
   retcode = rNONE;

   switch ( inoculator->value[1] )
   {
      default:
         break;
      case MEDICINE_HP:
         victim->hit += inoculator->value[2];
         ch_printf( victim, "That feels much better, you feel healed %d points of health.\r\n", inoculator->value[2] );
         if( victim->hit > victim->max_hit )
            victim->hit = victim->max_hit;
         break;
      case MEDICINE_MV:
         victim->move += inoculator->value[2];
         ch_printf( victim, "That feels much better, you feel rejuvenated %d points of movement.\r\n", inoculator->value[2] );
         if( victim->move > victim->max_move )
            victim->move = victim->max_move;
         break;
      case MEDICINE_POISON:
         retcode = obj_cast_spell( skill_lookup( "cure poison" ), inoculator->value[2], ch, victim, NULL );
         break;
      case MEDICINE_BLINDNESS:
         retcode = obj_cast_spell( skill_lookup( "cure blindness" ), inoculator->value[2], ch, victim, NULL );
         break;
      case MEDICINE_CBLIND:
         retcode = obj_cast_spell( skill_lookup( "blindness" ), inoculator->value[2], ch, victim, NULL );
         break;
      case MEDICINE_CPOISON:
         retcode = obj_cast_spell( skill_lookup( "poison" ), inoculator->value[2], ch, victim, NULL );
         break;
      case MEDICINE_SLEEP:
         retcode = obj_cast_spell( skill_lookup( "sleep" ), inoculator->value[2], ch, victim, NULL );
         break;
   }

   if( retcode == rNONE )
   {
      switch ( inoculator->value[3] )
      {
         default:
            break;
         case MEDICINE_HP:
            victim->hit += inoculator->value[4];
            ch_printf( victim, "That feels much better, you feel healed %d points of health.\r\n", inoculator->value[4] );
            if( victim->hit > victim->max_hit )
               victim->hit = victim->max_hit;
            break;
         case MEDICINE_MV:
            victim->move += inoculator->value[4];
            ch_printf( victim, "That feels much better, you feel rejuvenated %d points of movement.\r\n", inoculator->value[4] );
            if( victim->move > victim->max_move )
               victim->move = victim->max_move;
            break;
         case MEDICINE_POISON:
            retcode = obj_cast_spell( skill_lookup( "cure poison" ), inoculator->value[4], ch, victim, NULL );
            break;
         case MEDICINE_BLINDNESS:
            retcode = obj_cast_spell( skill_lookup( "cure blindness" ), inoculator->value[4], ch, victim, NULL );
            break;
         case MEDICINE_CBLIND:
            retcode = obj_cast_spell( skill_lookup( "blindness" ), inoculator->value[4], ch, victim, NULL );
            break;
         case MEDICINE_CPOISON:
            retcode = obj_cast_spell( skill_lookup( "poison" ), inoculator->value[4], ch, victim, NULL );
            break;
         case MEDICINE_SLEEP:
            retcode = obj_cast_spell( skill_lookup( "sleep" ), inoculator->value[4], ch, victim, NULL );
            break;
      }
   }

   WAIT_STATE( ch, PULSE_VIOLENCE );
   inoculator->value[1] = 0;
   inoculator->value[2] = 0;
   inoculator->value[3] = 0;
   inoculator->value[4] = 0;
   if( inoculator->value[0] < 1 )
      send_to_char( "Your inoculator has become useless.\r\n", ch );

   xp = victim->top_level * 50 + 3500;
   xp = UMIN( xp, exp_level( ch->skill_level[MEDICAL_ABILITY] + 1 ) - exp_level( ch->skill_level[MEDICAL_ABILITY] ) );
   gain_exp( ch, xp, MEDICAL_ABILITY );
   ch_printf( ch, "You gain %d medical experience.\r\n", xp );
   learn_from_success( ch, gsn_inoculate );
}

void do_concentrate( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int chance, timer;
   bool checktool, checkoven;
   OBJ_DATA *obj;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Concentrate <obj>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkoven = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to extract its strength.\r\n", ch );
            return;
         }
         if( !checkoven )
         {
            send_to_char( "&RYou need an oven to extract its strength.\r\n", ch );
            return;
         }
         if( ( obj = get_obj_carry( ch, argument ) ) == NULL )
         {
            send_to_char( "&RYou do not have that item.&w\r\n", ch );
            return;
         }
         else if( obj->item_type != ITEM_MEDICINE )
         {
            send_to_char( "&RThat isn't a medicine.&w\r\n", ch );
            return;
         }
         if( obj->value[5] == 1 )
         {
            send_to_char( "&RYou have already concentrated that medicine.\r\n", ch );
            return;
         }

         chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_concentrate] );
         if( number_percent(  ) < chance )
         {
            send_to_char( "&GYou begin the long process of extracting the medicine's strength.\r\n", ch );
            act( AT_PLAIN, "$n begins preparing something.", ch, NULL, argument, TO_ROOM );
            if( IS_IMMORTAL( ch ) )
               timer = 1;
            else if( ch->subclass == SUBCLASS_QUICKWORK )
               timer = 10;
            else if( ch->subclass == SUBCLASS_DOCTOR || ch->subclass == SUBCLASS_MEDIC )
               timer = 9;
            else
               timer = 25;
            add_timer( ch, TIMER_DO_FUN, timer, do_concentrate, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou cannot figure out how to handle the medicine.\r\n", ch );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interrupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   checktool = FALSE;
   checkoven = FALSE;
   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
   }
   if( ( obj = get_obj_carry( ch, arg ) ) == NULL )
   {
      send_to_char( "&RYou do not have that item.&w\r\n", ch );
      return;
   }

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_concentrate] );
   if( ch->subclass == SUBCLASS_MEDIC )
      chance += 10;
   if( ch->subclass == SUBCLASS_DOCTOR )
      chance += 50;
   if( !IS_NPC( ch ) && knows_skill( ch, gsn_improved_concentration ) )
      chance += 50;

   if( number_percent(  ) > chance * 2 || !checktool || !checkoven )
   {
      send_to_char( "&RYou inspect your medicine concentration.\r\n", ch );
      send_to_char( "&RThere was a fatal mistake, and the medicine is useless.\r\n", ch );
      send_to_char( "&RIt is unsatisfactory, so you trash it.\r\n", ch );
      learn_from_failure( ch, gsn_concentrate );
      separate_obj( obj );
      obj_from_char( obj );
      extract_obj( obj );
      return;
   }

   separate_obj( obj );
   obj->value[1] *= 2;
   obj->value[3] *= 2;
   obj->value[5] = 1;
   obj->cost = chance + obj->value[1] + obj->value[3] + obj->cost;

   send_to_char( "&GYou finish concentrating the medicine.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes concentrating the medicine.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain = UMIN( 50000, exp_level( ch->skill_level[MEDICAL_ABILITY] + 1 ) - exp_level( ch->skill_level[MEDICAL_ABILITY] ) );
      gain_exp( ch, xpgain, MEDICAL_ABILITY );
      ch_printf( ch, "You gain %ld medical experience.", xpgain );
   }

   learn_from_success( ch, gsn_concentrate );
}

void do_mix( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH], arg2[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, chance, timer;
   bool checktool, checkdrink, checkoven;
   OBJ_DATA *obj, *med, *med2;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;

   argument = one_argument( argument, arg );
   strlcpy( arg2, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:

         if( arg2[0] == '\0' )
         {
            send_to_char( "&RUsage: Mix <obj> <obj>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkoven = FALSE;
         checkdrink = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }
         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )
               checktool = TRUE;
            if( obj->item_type == ITEM_OVEN )
               checkoven = TRUE;
            if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 )
               checkdrink = TRUE;
         }

         if( !checktool )
         {
            send_to_char( "&RYou need a toolkit to mix medicines.\r\n", ch );
            return;
         }
         if( ( med = get_obj_carry( ch, arg ) ) == NULL )
         {
            send_to_char( "&RYou do not have that item.&w\r\n", ch );
            return;
         }
         if( med->item_type != ITEM_MEDICINE )
         {
            send_to_char( "That isn't a medicine.\r\n", ch );
            return;
         }
         if( med->value[5] != 0 )
         {
            send_to_char( "You can't mix concentrated medicines.\r\n", ch );
            return;
         }
         if( ( med2 = get_obj_carry( ch, arg2 ) ) == NULL )
         {
            send_to_char( "&RYou do not have that item.&w\r\n", ch );
            return;
         }
         if( med2->item_type != ITEM_MEDICINE )
         {
            send_to_char( "That isn't a medicine.\r\n", ch );
            return;
         }
         if( med2->value[5] != 0 )
         {
            send_to_char( "You can't mix concentrated medicines.\r\n", ch );
            return;
         }
         if( med == med2 )
         {
            send_to_char( "You can't mix the same object with itself.\r\n", ch );
            return;
         }
         if( med->value[0] && med->value[2] )
         {
            send_to_char( "The first medicine has already been mixed.\r\n", ch );
            return;
         }
         if( med2->value[0] && med2->value[2] )
         {
            send_to_char( "The second medicine has already been mixed.\r\n", ch );
            return;
         }
         if( med->value[0] == 0 )
         {
            med->value[0] = med->value[2];
            med->value[1] = med->value[3];
         }
         if( med2->value[0] == 0 )
         {
            med2->value[0] = med2->value[2];
            med2->value[1] = med2->value[3];
         }
         if( med->value[0] == med2->value[0] )
         {
            send_to_char( "You can't mix two medicines of the same type.\r\n", ch );
            return;
         }
         if( !checkdrink )
         {
            send_to_char( "&RYou need some empty vials to mix the medicines.\r\n", ch );
            return;
         }
         if( !checkoven )
         {
            send_to_char( "&RYou need an oven to mix the medicines.\r\n", ch );
            return;
         }

         chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_mix] );
         if( number_percent(  ) < chance )
         {
            send_to_char( "&GYou begin the long process of mixing medicines.\r\n", ch );
            act( AT_PLAIN, "$n begins mixing something.", ch, NULL, argument, TO_ROOM );
            if( IS_IMMORTAL( ch ) )
               timer = 1;
            else if( ch->subclass == SUBCLASS_QUICKWORK )
               timer = 10;
            else if( ch->subclass == SUBCLASS_DOCTOR || ch->subclass == SUBCLASS_MEDIC )
               timer = 9;
            else
               timer = 25;
            add_timer( ch, TIMER_DO_FUN, timer, do_mix, 1 );
            ch->dest_buf = strdup( arg );
            ch->dest_buf_2 = strdup( arg2 );
            return;
         }
         send_to_char( "&RYou cannot figure out how to handle the medicine.\r\n", ch );
         return;

      case 1:
         if( !ch->dest_buf || !ch->dest_buf_2 )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         strlcpy( arg2, ( const char* ) ch->dest_buf_2, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf_2 );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         if( ch->dest_buf_2 )
            DISPOSE( ch->dest_buf_2 );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interrupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->skill_level[MEDICAL_ABILITY] : ( int )( ch->pcdata->learned[gsn_mix] );
   vnum = 10442;

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char( "&RThe item you are trying to create is missing from the database.\r\n"
                    "Please inform the administration of this error.\r\n", ch );
      return;
   }

   if( ( med = get_obj_carry( ch, arg ) ) == NULL )
   {
      send_to_char( "&RYou do not have that item.&w\r\n", ch );
      return;
   }
   if( med->item_type != ITEM_MEDICINE )
   {
      send_to_char( "That isn't a medicine. BUG OR LACK OF OBJ.\r\n", ch );
      return;
   }
   if( ( med2 = get_obj_carry( ch, arg2 ) ) == NULL )
   {
      send_to_char( "&RYou do not have that item.&w\r\n", ch );
      return;
   }
   if( med2->item_type != ITEM_MEDICINE )
   {
      send_to_char( "That isn't a medicine. BUG OR LACK OF OBJ.\r\n", ch );
      return;
   }

   checktool = FALSE;
   checkoven = FALSE;
   checkdrink = FALSE;
   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT )
         checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )
         checkoven = TRUE;
      if( obj->item_type == ITEM_DRINK_CON && obj->value[1] == 0 && !checkdrink )
      {
         checkdrink = TRUE;
         separate_obj( obj );
         obj_from_char( obj );
         extract_obj( obj );
      }
   }

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_mix] );
   level = chance;
   if( ch->subclass == SUBCLASS_MEDIC )
   {
      level += 10;
      chance += 10;
   }
   if( ch->subclass == SUBCLASS_DOCTOR )
   {
      level += 50;
      chance += 50;
   }

   if( number_percent(  ) > chance * 2 || !checktool || !checkoven || !checkdrink )
   {
      send_to_char( "&RYou test a small quantity of your medicine.\r\n", ch );
      send_to_char( "&RThere appears to be unexpected side effects.\r\n", ch );
      send_to_char( "&RIt is unsatisfactory, so you trash it.\r\n", ch );
      learn_from_failure( ch, gsn_mix );
      separate_obj( med );
      obj_from_char( med );
      extract_obj( med );
      separate_obj( med2 );
      obj_from_char( med2 );
      extract_obj( med2 );
      return;
   }

   obj = create_object( pObjIndex, level );

   obj->item_type = ITEM_MEDICINE;
   SET_BIT( obj->wear_flags, ITEM_HOLD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level = level;
   obj->weight = 1;
   STRFREE( obj->name );
   snprintf( buf, MAX_STRING_LENGTH, "mixture of %s and %s", med->short_descr, med2->short_descr );
   obj->name = STRALLOC( buf );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   snprintf( buf, MAX_STRING_LENGTH, "A mixture of %s and %s was left here.", med->short_descr, med2->short_descr );
   obj->description = STRALLOC( buf );
   obj->value[0] = med->value[0];
   obj->value[1] = med->value[1];
   obj->value[2] = med2->value[0];
   obj->value[3] = med2->value[1];
   obj->cost = level + obj->value[1] + obj->value[3] + 2000;
   obj = obj_to_char( obj, ch );

   separate_obj( med );
   obj_from_char( med );
   extract_obj( med );
   separate_obj( med2 );
   obj_from_char( med2 );
   extract_obj( med2 );

   send_to_char( "&GYou finish your work and hold up your newly prepared mixture.&w\r\n", ch );
   act( AT_PLAIN, "$n finishes preparing a mixture.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;

      xpgain = UMIN( 40000, exp_level( ch->skill_level[MEDICAL_ABILITY] + 1 ) - exp_level( ch->skill_level[MEDICAL_ABILITY] ) );
      gain_exp( ch, xpgain, MEDICAL_ABILITY );
      ch_printf( ch, "You gain %ld medical experience.", xpgain );
   }

   learn_from_success( ch, gsn_mix );
}
/*
 * appraiseitem - v1.19, ported from SWGD flavor text (help entry found,
 * source function itself wasn't locatable in project knowledge after
 * several searches). Smuggling-guild skill: reveals weapon/armor/device
 * details on a carried item, modeled on this codebase's existing
 * loremedicine skill-check pattern since no more authentic source turned
 * up. Not verified against exact SWGD numbers/wording - flag if the real
 * source turns up later and this needs correcting.
 */
void do_appraiseitem( CHAR_DATA * ch, const char *argument )
{
   OBJ_DATA *obj;
   int chance;

   if( IS_NPC( ch ) )
      return;

   if( argument[0] == '\0' )
   {
      send_to_char( "Appraise what?\r\n", ch );
      return;
   }

   if( ( obj = get_obj_carry( ch, argument ) ) == NULL )
   {
      send_to_char( "You do not have that item.\r\n", ch );
      return;
   }

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_appraiseitem] );

   if( number_percent(  ) > chance )
   {
      send_to_char( "You can't quite figure out what you're looking at.\r\n", ch );
      learn_from_failure( ch, gsn_appraiseitem );
      return;
   }

   set_char_color( AT_LBLUE, ch );
   ch_printf( ch, "Object '%s' is %s.\r\nIts weight is %d, its value is %d.\r\n",
              obj->name, aoran( item_type_name( obj ) ), obj->weight, obj->cost );

   switch ( obj->item_type )
   {
      case ITEM_WEAPON:
         if( obj->value[3] >= 0 && obj->value[3] < (int)( sizeof( weapon_table ) / sizeof( weapon_table[0] ) ) )
            ch_printf( ch, "It appears to be %s%s, dealing %d-%d damage.\r\n",
                       aoran( weapon_table[obj->value[3]] ), weapon_table[obj->value[3]], obj->value[1], obj->value[2] );
         ch_printf( ch, "Condition: %d/%d.\r\n", obj->value[0], INIT_WEAPON_CONDITION );
         break;

      case ITEM_ARMOR:
         ch_printf( ch, "It provides %d points of armor (condition %d/%d).\r\n",
                    obj->value[1], obj->value[0], obj->value[1] );
         break;

      case ITEM_DEVICE:
         ch_printf( ch, "It's charged to %d/%d.\r\n", obj->value[0], obj->value[1] );
         break;

      default:
         break;
   }

   learn_from_success( ch, gsn_appraiseitem );
}

void do_loremedicine( CHAR_DATA * ch, const char *argument )
{
   OBJ_DATA *obj;
   int mixed = 0;
   int chance;

   if( argument[0] == '\0' )
   {
      send_to_char( "What medical object would you like to lore?\r\n", ch );
      return;
   }

   if( ( obj = get_obj_carry( ch, argument ) ) != NULL )
   {
      if( obj->item_type != ITEM_MEDICINE && obj->item_type != ITEM_SHOT
          && obj->item_type != ITEM_INOCULATOR && obj->item_type != ITEM_ELIXIR )
      {
         send_to_char( "That's not a medical object.\r\n", ch );
         return;
      }

      chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_loremedicine] );
      if( ch->subclass == SUBCLASS_MEDIC )
         chance += 10;
      if( ch->subclass == SUBCLASS_DOCTOR )
         chance += 50;
      if( number_percent(  ) > chance )
      {
         send_to_char( "You can't seem to identify it at all.\r\n", ch );
         learn_from_failure( ch, gsn_loremedicine );
         return;
      }

      set_char_color( AT_LBLUE, ch );
      ch_printf( ch, "Object '%s' is a %s\r\nIts weight is %d, value is %d.\r\n",
                 obj->name, aoran( item_type_name( obj ) ), obj->weight, obj->cost );

      switch ( obj->item_type )
      {
         case ITEM_MEDICINE:
         case ITEM_SHOT:
         case ITEM_ELIXIR:
            switch ( obj->value[0] )
            {
               default:
                  break;
               case MEDICINE_HP:
                  mixed++;
                  ch_printf( ch, "It has a healing reagent of strength %d\r\n", obj->value[1] );
                  break;
               case MEDICINE_MV:
                  mixed++;
                  ch_printf( ch, "It has a rejuvenation reagent of strength %d\r\n", obj->value[1] );
                  break;
               case MEDICINE_POISON:
                  mixed++;
                  ch_printf( ch, "It has a poison cure reagent of strength %d\r\n", obj->value[1] );
                  break;
               case MEDICINE_BLINDNESS:
                  mixed++;
                  ch_printf( ch, "It has a blindness cure reagent of strength %d\r\n", obj->value[1] );
                  break;
               case MEDICINE_CBLIND:
                  ch_printf( ch, "It causes blindness at a strength of %d\r\n", obj->value[1] );
                  break;
               case MEDICINE_CPOISON:
                  ch_printf( ch, "It causes poison at a strength of %d\r\n", obj->value[1] );
                  break;
               case MEDICINE_SLEEP:
                  ch_printf( ch, "It causes sleepiness at a strength of %d\r\n", obj->value[1] );
                  break;
            }
            switch ( obj->value[2] )
            {
               default:
                  break;
               case MEDICINE_HP:
                  mixed++;
                  ch_printf( ch, "It has a healing reagent of strength %d\r\n", obj->value[3] );
                  break;
               case MEDICINE_MV:
                  mixed++;
                  ch_printf( ch, "It has a rejuvenation reagent of strength %d\r\n", obj->value[3] );
                  break;
               case MEDICINE_POISON:
                  mixed++;
                  ch_printf( ch, "It has a poison cure reagent of strength %d\r\n", obj->value[3] );
                  break;
               case MEDICINE_BLINDNESS:
                  mixed++;
                  ch_printf( ch, "It has a blindness cure reagent of strength %d\r\n", obj->value[3] );
                  break;
               case MEDICINE_CBLIND:
                  ch_printf( ch, "It causes blindness at a strength of %d\r\n", obj->value[3] );
                  break;
               case MEDICINE_CPOISON:
                  ch_printf( ch, "It causes poison at a strength of %d\r\n", obj->value[3] );
                  break;
               case MEDICINE_SLEEP:
                  ch_printf( ch, "It causes sleepiness at a strength of %d\r\n", obj->value[3] );
                  break;
            }
            if( mixed == 2 )
               send_to_char( "It is a mixture of two medicines.\r\n", ch );
            if( obj->value[5] != 0 )
               send_to_char( "It is concentrated.\r\n", ch );
            else
               send_to_char( "It is not concentrated.\r\n", ch );
            learn_from_success( ch, gsn_loremedicine );
            break;

         case ITEM_INOCULATOR:
            if( obj->value[0] == 0 )
            {
               send_to_char( "This inoculator is worthless.\r\n", ch );
               learn_from_success( ch, gsn_loremedicine );
               return;
            }
            ch_printf( ch, "This inoculator has %d uses left on it.\r\n", obj->value[0] );

            switch ( obj->value[1] )
            {
               default:
                  break;
               case MEDICINE_HP:
                  mixed++;
                  ch_printf( ch, "It has a healing reagent of strength %d\r\n", obj->value[2] );
                  break;
               case MEDICINE_MV:
                  mixed++;
                  ch_printf( ch, "It has a rejuvenation reagent of strength %d\r\n", obj->value[2] );
                  break;
               case MEDICINE_POISON:
                  mixed++;
                  ch_printf( ch, "It has a poison cure reagent of strength %d\r\n", obj->value[2] );
                  break;
               case MEDICINE_BLINDNESS:
                  mixed++;
                  ch_printf( ch, "It has a blindness cure reagent of strength %d\r\n", obj->value[2] );
                  break;
               case MEDICINE_CBLIND:
                  ch_printf( ch, "It causes blindness at a strength of %d\r\n", obj->value[2] );
                  break;
               case MEDICINE_CPOISON:
                  ch_printf( ch, "It causes poison at a strength of %d\r\n", obj->value[2] );
                  break;
               case MEDICINE_SLEEP:
                  ch_printf( ch, "It causes sleepiness at a strength of %d\r\n", obj->value[2] );
                  break;
            }
            switch ( obj->value[3] )
            {
               default:
                  break;
               case MEDICINE_HP:
                  mixed++;
                  ch_printf( ch, "It has a healing reagent of strength %d\r\n", obj->value[4] );
                  break;
               case MEDICINE_MV:
                  mixed++;
                  ch_printf( ch, "It has a rejuvenation reagent of strength %d\r\n", obj->value[4] );
                  break;
               case MEDICINE_POISON:
                  mixed++;
                  ch_printf( ch, "It has a poison cure reagent of strength %d\r\n", obj->value[4] );
                  break;
               case MEDICINE_BLINDNESS:
                  mixed++;
                  ch_printf( ch, "It has a blindness cure reagent of strength %d\r\n", obj->value[4] );
                  break;
               case MEDICINE_CBLIND:
                  ch_printf( ch, "It causes blindness at a strength of %d\r\n", obj->value[4] );
                  break;
               case MEDICINE_CPOISON:
                  ch_printf( ch, "It causes poison at a strength of %d\r\n", obj->value[4] );
                  break;
               case MEDICINE_SLEEP:
                  ch_printf( ch, "It causes sleepiness at a strength of %d\r\n", obj->value[4] );
                  break;
            }
            if( mixed == 2 )
               send_to_char( "It has a combination of two medicines.\r\n", ch );
            learn_from_success( ch, gsn_loremedicine );
            break;

         default:
            break;
      }
      return;
   }
   send_to_char( "You do not possess that item.\r\n", ch );
}

void do_blackjack( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   int percent;
   AFFECT_DATA af;

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't do that right now.\r\n", ch );
      return;
   }

   one_argument( argument, arg );

   if( ch->mount )
   {
      send_to_char( "You can't get close enough while mounted.\r\n", ch );
      return;
   }

   if( arg[0] == '\0' )
   {
      send_to_char( "Blackjack whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "That's not going to accomplish much.\r\n", ch );
      return;
   }

   if( is_safe( ch, victim ) )
      return;

   {
      OBJ_DATA *obj = get_eq_char( ch, WEAR_WIELD );
      if( !obj || obj->item_type != ITEM_WEAPON || obj->value[3] != WEAPON_BLUDGEON )
      {
         send_to_char( "You need to wield a blunt weapon.\r\n", ch );
         return;
      }
   }

   if( victim->fighting )
   {
      send_to_char( "You can't blackjack someone who is in combat.\r\n", ch );
      return;
   }

   if( victim->position == POS_STUNNED )
   {
      send_to_char( "You can't blackjack someone who's already stunned.\r\n", ch );
      return;
   }

   if( victim->hit < victim->max_hit && IS_AWAKE( victim ) )
   {
      act( AT_PLAIN, "$N is hurt and suspicious... you can't sneak up.", ch, NULL, victim, TO_CHAR );
      return;
   }

   if( !IS_AFFECTED( ch, AFF_HIDE ) )
   {
      send_to_char( "You need to be hidden to blackjack someone.\r\n", ch );
      return;
   }

   if( !( !IS_AFFECTED( victim, AFF_SIT_AWARE ) || !IS_AWAKE( victim ) ) )
   {
      if( number_percent(  ) < 60 )
      {
         send_to_char( "Your opponent senses you approach and sidesteps your blow!\r\n", ch );
         learn_from_failure( ch, gsn_blackjack );
         global_retcode = damage( ch, victim, 0, gsn_blackjack );
         return;
      }
   }

   percent = ( number_percent(  ) - ( get_curr_lck( victim ) - 20 )
               - ( IS_NPC( ch ) ? 0 : ch->pcdata->learned[gsn_bludgeons] / 2 ) );

   WAIT_STATE( ch, skill_table[gsn_blackjack]->beats );
   if( !IS_AWAKE( victim )
       || IS_NPC( ch )
       || percent < UMIN( 15, ( ch->pcdata->learned[gsn_blackjack] / 2 ) ) )
   {
      learn_from_success( ch, gsn_blackjack );
      if( !IS_NPC( victim ) )
         send_to_char( "You have been knocked unconscious!\r\n", victim );
      ch_printf( ch, "You knocked %s unconscious!\r\n", victim->name );
      if( !IS_AFFECTED( victim, AFF_PARALYSIS ) )
      {
         af.type      = gsn_stun;
         af.location  = APPLY_AC;
         af.modifier  = 20;
         af.duration  = IS_NPC( ch ) ? 2 : ( ch->pcdata->learned[gsn_blackjack] / 10 );
         af.bitvector = AFF_PARALYSIS;
         affect_to_char( victim, &af );
         update_pos( victim );
      }
      act( AT_ACTION, "&c$n crumples to the ground.", victim, NULL, NULL, TO_ROOM );
   }
   else
   {
      learn_from_failure( ch, gsn_blackjack );
      send_to_char( "You failed!\r\n", ch );
      global_retcode = damage( ch, victim, 0, gsn_blackjack );
   }
}

void do_cheap( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;
   int percent;

   if( !IS_NPC( ch ) && IS_SET( ch->pcdata->flags, PCFLAG_BOUND ) )
   {
      send_to_char( "Not while you're bound.\r\n", ch );
      return;
   }

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "You can't do that right now.\r\n", ch );
      return;
   }

   one_argument( argument, arg );

   if( ch->mount )
   {
      send_to_char( "You can't get close enough while mounted.\r\n", ch );
      return;
   }

   if( arg[0] == '\0' )
   {
      send_to_char( "Cheapattack whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "How can you sneak up on yourself?\r\n", ch );
      return;
   }

   if( is_safe( ch, victim ) )
      return;

   if( victim->fighting )
   {
      send_to_char( "You can't cheapattack someone who is in combat.\r\n", ch );
      return;
   }

   if( victim->hit < victim->max_hit && IS_AWAKE( victim ) )
   {
      act( AT_PLAIN, "$N is hurt and suspicious... you can't sneak up.", ch, NULL, victim, TO_CHAR );
      return;
   }

   if( !( !IS_AFFECTED( victim, AFF_SIT_AWARE ) || !IS_AWAKE( victim ) ) )
   {
      if( number_percent(  ) < 50 )
      {
         send_to_char( "Your opponent senses you approach and sidesteps your blow!\r\n", ch );
         learn_from_failure( ch, gsn_cheap );
         global_retcode = damage( ch, victim, 0, gsn_cheap );
         return;
      }
   }

   percent = number_percent(  ) - ( get_curr_lck( ch ) - 14 )
             + ( get_curr_lck( victim ) - 13 );

   WAIT_STATE( ch, skill_table[gsn_cheap]->beats );
   if( !IS_AWAKE( victim )
       || IS_NPC( ch )
       || percent < ch->pcdata->learned[gsn_cheap] )
   {
      learn_from_success( ch, gsn_cheap );
      global_retcode = multi_hit( ch, victim, gsn_cheap );
   }
   else
   {
      learn_from_failure( ch, gsn_cheap );
      global_retcode = damage( ch, victim, 0, gsn_cheap );
   }
}

void do_bind( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char logbuf[MAX_STRING_LENGTH];
   CHAR_DATA *victim;
   int percent, bindchance;

   argument = one_argument( argument, arg );

   if( !IS_NPC( ch ) && IS_SET( ch->pcdata->flags, PCFLAG_BOUND ) )
   {
      send_to_char( "Nice try — you're already bound.\r\n", ch );
      return;
   }

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( arg[0] == '\0' )
   {
      send_to_char( "Bind whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( ch->fighting )
   {
      send_to_char( "You can't get away from the fight long enough.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "That's pointless.\r\n", ch );
      return;
   }

   if( IS_NPC( victim ) )
   {
      send_to_char( "You can't bind a mob.\r\n", ch );
      return;
   }

   if( IS_SET( victim->pcdata->flags, PCFLAG_BOUND ) )
   {
      send_to_char( "They already appear to be bound.\r\n", ch );
      return;
   }

   if( victim->fighting )
   {
      send_to_char( "You can't get in close enough while they are fighting.\r\n", ch );
      return;
   }

   percent    = number_percent(  );
   bindchance = IS_NPC( ch ) ? ch->top_level : (int)( ch->pcdata->learned[gsn_bind] * 0.6 );

   snprintf( logbuf, MAX_STRING_LENGTH, "%s binding %s (%d/%d)", ch->name, victim->name, percent, bindchance );
   log_string( logbuf );

   if( victim->position <= POS_SLEEPING )
      bindchance += 75;

   if( !can_see( victim, ch ) )
      bindchance += 50;

   if( victim->perm_frc > 0 )
      bindchance -= ( victim->perm_frc * 2 ) / 5;

   if( percent > bindchance )
   {
      send_to_char( "You fumble the binders!\r\n", ch );
      act( AT_ACTION, "$n tried to put you in binders!", ch, NULL, victim, TO_VICT );
      act( AT_ACTION, "$n tried to bind $N.", ch, NULL, victim, TO_NOTVICT );
      learn_from_failure( ch, gsn_bind );
      return;
   }

   SET_BIT( victim->pcdata->flags, PCFLAG_BOUND );
   snprintf( logbuf, MAX_STRING_LENGTH, "%s successfully bound %s", ch->name, victim->name );
   log_string( logbuf );
   send_to_char( "You put them in binders successfully.\r\n", ch );
   act( AT_ACTION, "$n has put you in binders!", ch, NULL, victim, TO_VICT );
   act( AT_ACTION, "$n has put $N in binders!", ch, NULL, victim, TO_NOTVICT );
   gain_exp( ch, 2500, LEADERSHIP_ABILITY );
   learn_from_success( ch, gsn_bind );
}

void do_unbind( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   CHAR_DATA *victim;

   argument = one_argument( argument, arg );

   if( !IS_NPC( ch ) && IS_SET( ch->pcdata->flags, PCFLAG_BOUND ) )
   {
      send_to_char( "Not when you're bound you aren't.\r\n", ch );
      return;
   }

   if( ch->mount )
   {
      send_to_char( "You can't do that while mounted.\r\n", ch );
      return;
   }

   if( ch->fighting )
   {
      send_to_char( "You are a little occupied at the moment.\r\n", ch );
      return;
   }

   if( arg[0] == '\0' )
   {
      send_to_char( "Unbind whom?\r\n", ch );
      return;
   }

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   {
      send_to_char( "They aren't here.\r\n", ch );
      return;
   }

   if( victim == ch )
   {
      send_to_char( "How do you intend to do that?\r\n", ch );
      return;
   }

   if( IS_NPC( victim ) )
   {
      send_to_char( "You can't unbind a mob.\r\n", ch );
      return;
   }

   if( !IS_SET( victim->pcdata->flags, PCFLAG_BOUND ) )
   {
      send_to_char( "They aren't bound.\r\n", ch );
      return;
   }

   REMOVE_BIT( victim->pcdata->flags, PCFLAG_BOUND );
   send_to_char( "You carefully remove the bindings.\r\n", ch );
   act( AT_ACTION, "$n has removed your bindings!", ch, NULL, victim, TO_VICT );
   act( AT_ACTION, "$n has removed $N's bindings.", ch, NULL, victim, TO_NOTVICT );
}

void do_scrapattack( CHAR_DATA * ch, const char *argument )
{
   CHAR_DATA *victim;
   int dameq;
   OBJ_DATA *damobj;

   if( IS_NPC( ch ) && IS_AFFECTED( ch, AFF_CHARM ) )
   {
      send_to_char( "Not while charmed!\r\n", ch );
      return;
   }

   if( ( victim = who_fighting( ch ) ) == NULL )
   {
      send_to_char( "You aren't fighting anyone.\r\n", ch );
      return;
   }

   WAIT_STATE( ch, skill_table[gsn_kick]->beats );
   if( IS_NPC( ch ) || number_percent(  ) < ch->pcdata->learned[gsn_scrapattack] )
   {
      send_to_char( "You focus on damaging your foe's equipment...\r\n", ch );
      learn_from_success( ch, gsn_scrapattack );
      global_retcode = damage( ch, victim,
                               number_range( 1, ch->skill_level[ENGINEERING_ABILITY] ),
                               gsn_scrapattack );
      dameq  = number_range( WEAR_LIGHT, WEAR_EYES );
      damobj = get_eq_char( victim, dameq );
      if( damobj ) { set_cur_obj( damobj ); damage_obj( damobj ); }
      dameq  = number_range( WEAR_LIGHT, WEAR_EYES );
      damobj = get_eq_char( victim, dameq );
      if( damobj ) { set_cur_obj( damobj ); damage_obj( damobj ); }
      dameq  = number_range( WEAR_LIGHT, WEAR_EYES );
      damobj = get_eq_char( victim, dameq );
      if( damobj ) { set_cur_obj( damobj ); damage_obj( damobj ); }
   }
   else
   {
      send_to_char( "You are unable to damage your foe's equipment...\r\n", ch );
      learn_from_failure( ch, gsn_scrapattack );
      global_retcode = damage( ch, victim, 0, gsn_kick );
   }
}

void do_makebludgeon( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char buf[MAX_STRING_LENGTH];
   int level, chance, strengthmin, strengthmax;
   bool checktool, checkdura, checkfab, checkoven;
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *pObjIndex;
   int vnum;
   AFFECT_DATA *paf;
   AFFECT_DATA *paf2;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   if( !IS_NPC( ch ) && IS_SET( ch->pcdata->flags, PCFLAG_BOUND ) )
   {
      send_to_char( "How do you intend to do that when bound?\r\n", ch );
      return;
   }

   switch ( ch->substate )
   {
      default:
         if( arg[0] == '\0' )
         {
            send_to_char( "&RUsage: Makebludgeon <name>\r\n&w", ch );
            return;
         }

         checktool = FALSE;
         checkdura = FALSE;
         checkfab  = FALSE;
         checkoven = FALSE;

         if( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) && ch->subclass != SUBCLASS_JURYRIGGER )
         {
            send_to_char( "&RYou need to be in a factory or workshop to do that.\r\n", ch );
            return;
         }

         for( obj = ch->last_carrying; obj; obj = obj->prev_content )
         {
            if( obj->item_type == ITEM_TOOLKIT )  checktool = TRUE;
            if( obj->item_type == ITEM_DURASTEEL ) checkdura = TRUE;
            if( obj->item_type == ITEM_FABRIC )    checkfab  = TRUE;
            if( obj->item_type == ITEM_OVEN )      checkoven = TRUE;
         }

         if( !checktool ) { send_to_char( "&RYou need a toolkit to make a bludgeon.\r\n", ch ); return; }
         if( !checkdura ) { send_to_char( "&RA big hunk of metal would be nice...\r\n", ch ); return; }
         if( !checkfab )  { send_to_char( "&RYou need some fabric for the grip.\r\n", ch ); return; }
         if( !checkoven ) { send_to_char( "&RYou need an oven to heat the metal.\r\n", ch ); return; }

         chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makebludgeon] );
         if( ch->subclass == SUBCLASS_WEAPONSMITH ) chance += 10;

         if( number_percent(  ) < chance )
         {
            send_to_char( "&GYou begin the long process of crafting a bludgeon.\r\n", ch );
            act( AT_PLAIN, "$n takes $s tools and a small oven and begins to work on something.", ch, NULL, argument, TO_ROOM );
            if( IS_IMMORTAL( ch ) )
               add_timer( ch, TIMER_DO_FUN, 1, do_makebludgeon, 1 );
            else if( ch->subclass == SUBCLASS_QUICKWORK )
               add_timer( ch, TIMER_DO_FUN, 3, do_makebludgeon, 1 );
            else if( ch->subclass == SUBCLASS_WEAPONSMITH )
               add_timer( ch, TIMER_DO_FUN, 10, do_makebludgeon, 1 );
            else
               add_timer( ch, TIMER_DO_FUN, 12, do_makebludgeon, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou can't figure out how to shove the parts together.\r\n", ch );
         return;

      case 1:
         if( !ch->dest_buf ) return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interrupted and fail to finish your work.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   level = IS_NPC( ch ) ? ch->skill_level[ENGINEERING_ABILITY] : ( int )( ch->pcdata->learned[gsn_makebludgeon] );
   vnum  = 10432;   /* bludgeon prototype object */

   if( ( pObjIndex = get_obj_index( vnum ) ) == NULL )
   {
      send_to_char( "&RThe item you are trying to create is missing from the database.\r\n"
                    "Please inform the administration of this error.\r\n", ch );
      return;
   }

   checktool = FALSE;
   checkdura = FALSE;
   checkfab  = FALSE;
   checkoven = FALSE;
   strengthmin = 10;
   strengthmax = 20;

   if( ch->subclass == SUBCLASS_WEAPONSMITH ) level += 10;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->item_type == ITEM_TOOLKIT ) checktool = TRUE;
      if( obj->item_type == ITEM_OVEN )    checkoven = TRUE;
      if( obj->item_type == ITEM_DURASTEEL && !checkdura )
      {
         float exdam = ( ch->subclass == SUBCLASS_WEAPONSMITH ) ? 1.1f : 1.0f;
         strengthmin = ( int ) URANGE( level / 5 + 20, exdam * ( obj->value[0] * 10 + level / 2 ), level / 2 + 100 );
         strengthmax = ( int ) URANGE( level + 50,     exdam * ( obj->value[1] * 20 + 40 ),         level * 2 + 40 );
         if( strengthmin > strengthmax ) strengthmax = strengthmin;
         checkdura = TRUE;
         separate_obj( obj ); obj_from_char( obj ); extract_obj( obj );
      }
      if( obj->item_type == ITEM_FABRIC && !checkfab )
      {
         checkfab = TRUE;
         separate_obj( obj ); obj_from_char( obj ); extract_obj( obj );
      }
   }

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_makebludgeon] );
   if( ch->subclass == SUBCLASS_WEAPONSMITH ) chance += 10;

   if( number_percent(  ) > chance * 2 || !checktool || !checkdura || !checkfab || !checkoven )
   {
      send_to_char( "&RYou wave your new bludgeon around...\r\n", ch );
      send_to_char( "&RThe deformed metal looks really crappy, so you hide it before someone sees it.\r\n", ch );
      learn_from_failure( ch, gsn_makebludgeon );
      return;
   }

   obj = create_object( pObjIndex, level );
   obj->item_type = ITEM_WEAPON;
   SET_BIT( obj->wear_flags, ITEM_WIELD );
   SET_BIT( obj->wear_flags, ITEM_TAKE );
   obj->level  = level;
   obj->weight = 8;
   STRFREE( obj->name );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   strlcat( buf, " bludgeon bludge", MAX_STRING_LENGTH );
   obj->name = STRALLOC( buf );
   strlcpy( buf, arg, MAX_STRING_LENGTH );
   STRFREE( obj->short_descr );
   obj->short_descr = STRALLOC( buf );
   STRFREE( obj->description );
   strlcat( buf, " could be used to bash some brains...", MAX_STRING_LENGTH );
   obj->description = STRALLOC( buf );
   CREATE( paf, AFFECT_DATA, 1 );
   paf->type     = -1;
   paf->duration = -1;
   paf->location = get_atype( "hitroll" );
   paf->modifier = level / 10;
   paf->bitvector = 0;
   paf->next = NULL;
   LINK( paf, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   CREATE( paf2, AFFECT_DATA, 1 );
   paf2->type     = -1;
   paf2->duration = -1;
   paf2->location = get_atype( "damroll" );
   paf2->modifier = ( int )( level / 7.5f );
   paf2->bitvector = 0;
   paf2->next = NULL;
   LINK( paf2, obj->first_affect, obj->last_affect, next, prev );
   ++top_affect;
   obj->value[0] = INIT_WEAPON_CONDITION;
   obj->value[1] = strengthmin;
   obj->value[2] = strengthmax;
   obj->value[3] = WEAPON_BLUDGEON;
   obj->cost = obj->value[2] * 10;
   obj = obj_to_char( obj, ch );

   send_to_char( "&GYou finish your work and hold up your newly created bludgeon!&w\r\n", ch );
   act( AT_PLAIN, "$n finishes crafting a bludgeoning weapon.", ch, NULL, argument, TO_ROOM );

   {
      long xpgain;
      xpgain = UMIN( 55000, exp_level( ch->skill_level[ENGINEERING_ABILITY] + 1 ) - exp_level( ch->skill_level[ENGINEERING_ABILITY] ) );
      gain_exp( ch, xpgain, ENGINEERING_ABILITY );
      ch_printf( ch, "You gain %ld engineering experience.", xpgain );
   }

   learn_from_success( ch, gsn_makebludgeon );
}

/* ===== Ported from GD ===== */

ALIAS_DATA *find_alias( CHAR_DATA *ch, const char *name )
{
   ALIAS_DATA *pal;
   if( IS_NPC( ch ) || !ch->pcdata ) return NULL;
   for( pal = ch->pcdata->first_alias; pal; pal = pal->next )
      if( !str_cmp( pal->name, name ) ) return pal;
   return NULL;
}

bool knows_skill( CHAR_DATA *ch, int sn )
{
   if( IS_NPC( ch ) ) return ( ch->top_level > 0 );
   return ( ch->pcdata->learned[sn] > 0 );
}

void get_obj_palm( CHAR_DATA *ch, OBJ_DATA *obj, OBJ_DATA *container )
{
   int weight, chance, percent;
   CHAR_DATA *rch;

   chance = IS_NPC( ch ) ? 50
                         : ( int )( ch->pcdata->learned[gsn_palm] * 0.75 + get_curr_dex( ch ) / 2 );

   if( !CAN_WEAR( obj, ITEM_TAKE ) && ch->top_level < sysdata.level_getobjnotake )
   { send_to_char( "You can't take that.\r\n", ch ); return; }

   if( IS_OBJ_STAT( obj, ITEM_PROTOTYPE ) && !can_take_proto( ch ) )
   { send_to_char( "A godly force prevents you from getting close to it.\r\n", ch ); return; }

   if( ch->carry_number + get_obj_number( obj ) > can_carry_n( ch ) )
   { act( AT_PLAIN, "$d: you can't carry that many items.", ch, NULL, obj->name, TO_CHAR ); return; }

   weight = get_obj_weight( obj );
   if( ch->carry_weight + weight > can_carry_w( ch ) )
   { act( AT_PLAIN, "$d: you can't carry that much weight.", ch, NULL, obj->name, TO_CHAR ); return; }

   if( container )
      act( AT_ACTION, "You palm $p from $P.", ch, obj, container, TO_CHAR );
   else
      act( AT_ACTION, "You palm $p.", ch, obj, NULL, TO_CHAR );

   for( rch = ch->in_room->first_person; rch; rch = rch->next_in_room )
   {
      if( rch == ch ) continue;
      percent = number_percent();
      if( !can_see( rch, ch ) ) percent -= 25;
      if( !IS_AWAKE( rch ) ) percent = 0;
      if( IS_AFFECTED( rch, AFF_SIT_AWARE ) ) percent += 15;
      if( IS_IMMORTAL( rch ) && IS_AWAKE( rch ) ) percent += 100;
      if( percent > chance )
      {
         set_char_color( AT_ACTION, rch );
         if( container )
            ch_printf( rch, "%s gets %s from %s.\r\n", ch->name, obj->short_descr, container->short_descr );
         else
            ch_printf( rch, "%s gets %s.\r\n", ch->name, obj->short_descr );
         learn_from_failure( ch, gsn_palm );
      }
      else learn_from_success( ch, gsn_palm );
   }

   if( container ) obj_from_obj( obj ); else obj_from_room( obj );
   if( obj->item_type != ITEM_CONTAINER ) check_for_trap( ch, obj, TRAP_GET );
   if( char_died( ch ) ) return;
   if( obj->item_type == ITEM_MONEY ) { ch->gold += obj->value[0]; extract_obj( obj ); }
   else obj = obj_to_char( obj, ch );
   if( char_died( ch ) || obj_extracted( obj ) ) return;
   oprog_get_trigger( ch, obj );
}

void do_disabled( CHAR_DATA *ch, const char *argument )
{
   send_to_char( "That command is not available.\r\n", ch );
}

void do_alias( CHAR_DATA *ch, const char *argument )
{
    ALIAS_DATA *pal = NULL;
    char arg[MAX_INPUT_LENGTH];
    const char *p;
    
    if( IS_NPC(ch) )
        return;

    for( p = argument; *p != '\0'; p++ )
    {
       if ( *p == '~' )
 	 {
	    send_to_char( "Command not acceptable, cannot use the ~ character.\n\r", ch );
	    return;
	 }
    }

    argument = one_argument(argument, arg);
    
    if ( !*arg ) 
    {
        if( !ch->pcdata->first_alias )
        {
            send_to_char( "You have no aliases defined!\n\r", ch );
            return;
        }
        pager_printf( ch, "%-20s What it does\n\r", "Alias" );
        for( pal = ch->pcdata->first_alias; pal; pal = pal->next )
            pager_printf( ch, "%-20s %s\n\r", pal->name, pal->cmd );
        return;
    }
    
    if ( !*argument )
    {
        if ( (pal = find_alias(ch, arg)) != NULL )
        {
            DISPOSE(pal->name);
            DISPOSE(pal->cmd);
            UNLINK(pal, ch->pcdata->first_alias, ch->pcdata->last_alias, next, prev);
            DISPOSE(pal);
            send_to_char("Deleted Alias.\n\r", ch);
        } 
	  else
            send_to_char("That alias does not exist.\n\r", ch);
        return;
    }
    
    if( ( pal = find_alias( ch, arg ) ) == NULL )
    {
        CREATE( pal, ALIAS_DATA, 1 );
        pal->name = str_dup( arg );
        pal->cmd  = str_dup( argument );
        LINK( pal, ch->pcdata->first_alias, ch->pcdata->last_alias, next, prev );
        send_to_char( "Created Alias.\n\r", ch );
    } 
    else 
    {
        if( pal->cmd );
        DISPOSE( pal->cmd );
        pal->cmd  = str_dup( argument );
        send_to_char( "Modified Alias.\n\r", ch );
    }
}

void do_flushpoison( CHAR_DATA *ch, const char *argument )
{
    short percent;
    OBJ_DATA *wobj;
    if ( ch->fighting )
    {
        send_to_char("You can't concentrate enough while you are fighting.\n\r", ch);
        return;
    }
        for ( wobj = ch->first_carrying; wobj; wobj = wobj->next_content )
    {
        if ( wobj->item_type == ITEM_DRINK_CON
        && wobj->value[1]  >  0
        && wobj->value[2]  == 0 )
        break;
    }
    if ( !wobj )
    {
        send_to_char( "With what water?\n\r", ch );
        return;
    }
    if ( is_affected( ch, gsn_poison ) )
    {   
        wobj->value[1] -= 1;
	percent = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_flushpoison] );
        percent = UMIN(75, percent);
        if( number_percent() >= percent )
        {
           send_to_char( "You fail to concentrate.", ch); 
           learn_from_failure( ch, gsn_flushpoison);
           return;
        }
        affect_strip( ch, gsn_poison );
	set_char_color( AT_MAGIC, ch);
	send_to_char( "You concentrate and flush the poison from your body.\n\r", ch );
	ch->mental_state = URANGE( -100, ch->mental_state, -10 );
        learn_from_success(ch, gsn_flushpoison);
	return;
    }
    else
    {
       send_to_char("You aren't poisoned.\n\r", ch);
       return;
    }
}

/* v1.31: picks a random person in ch's room, excluding ch itself. Used by
 * do_spray below for its "spray a direction" mode. Ported from SWGD's
 * combat.c (same name, same shape). */
CHAR_DATA *get_random_victim( CHAR_DATA *ch )
{
   CHAR_DATA *victim = NULL;
   CHAR_DATA *temp;

   if( ch->in_room->first_person == ch && ch->in_room->last_person == ch )
      return NULL;

   while( victim == NULL )
   {
      for( temp = ch->in_room->first_person; temp; temp = temp->next_in_room )
      {
         if( ch == temp )
            continue;

         if( number_percent(  ) > 50 )
         {
            victim = temp;
            temp = NULL;
            break;
         }
      }
   }
   return victim;
}

/* v1.31: SWGD-derived spray - sustained automatic fire, either at a target
 * in the room or blindly through an exit at whoever's on the other side.
 * Adapted from SWGD's combat.c: SWGD gates this on a separate weapons_table
 * of weapon "families" (FWEAPON_BLASTER/BOWCASTER/SLUGTHROWER/FLECHETTE)
 * plus a WEAPON_AUTOMATIC flag/ITEM_AUTOMATIC extra-flag - none of that
 * infrastructure exists here. Adapted to gate directly on this codebase's
 * flat weapon-type slot (value[3] == WEAPON_BLASTER or WEAPON_BOWCASTER),
 * since those are our only two ammo-fed ranged weapon types. Ammo is
 * value[4] (current) same as everywhere else in this codebase's weapon
 * system (see the admin/builder manual's object-values section). */
void do_spray( CHAR_DATA *ch, const char *argument )
{
   OBJ_DATA *wield;
   OBJ_DATA *dual_wield;
   char arg[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   short dir;
   EXIT_DATA *pexit;
   ROOM_INDEX_DATA *was_in_room;
   ROOM_INDEX_DATA *to_room;
   CHAR_DATA *victim = NULL;
   int chance;
   char buf[MAX_STRING_LENGTH];
   bool pfound = FALSE;
   bool has_dual = FALSE;
   bool has_wield = FALSE;
   bool had_wield = FALSE;
   bool had_dual = FALSE;
   bool ammo_wield = FALSE;
   bool ammo_dual = FALSE;
   int rounds = 0;

   if( IS_SET( ch->in_room->room_flags, ROOM_SAFE ) )
   {
      set_char_color( AT_MAGIC, ch );
      send_to_char( "You'll have to do that elsewhere.\r\n", ch );
      return;
   }

   wield = get_eq_char( ch, WEAR_WIELD );
   dual_wield = get_eq_char( ch, WEAR_DUAL_WIELD );

   if( wield && wield->item_type == ITEM_WEAPON
       && ( wield->value[3] == WEAPON_BLASTER || wield->value[3] == WEAPON_BOWCASTER ) )
   {
      has_wield = TRUE;
      ammo_wield = TRUE;
   }
   if( dual_wield && dual_wield->item_type == ITEM_WEAPON
       && ( dual_wield->value[3] == WEAPON_BLASTER || dual_wield->value[3] == WEAPON_BOWCASTER ) )
   {
      has_dual = TRUE;
      ammo_dual = TRUE;
   }

   if( !has_wield && !has_dual )
   {
      send_to_char( "You don't seem to be holding an automatic weapon.\r\n", ch );
      return;
   }

   had_wield = has_wield;
   had_dual = has_dual;

   if( has_wield && wield->value[4] < 1 && ammo_wield )
      has_wield = FALSE;
   if( has_dual && dual_wield->value[4] < 1 && ammo_dual )
      has_dual = FALSE;

   if( !has_wield && !has_dual )
   {
      if( had_wield )
         act( AT_ACTION, "$p doesn't appear to be loaded.", ch, wield, NULL, TO_CHAR );
      if( had_dual )
         act( AT_ACTION, "$p is depleted of ammo.", ch, dual_wield, NULL, TO_CHAR );
      return;
   }

   argument = one_argument( argument, arg );
   argument = one_argument( argument, arg2 );

   if( arg2[0] == '\0' || atoi( arg2 ) <= 0 )
   {
      send_to_char( "Usage: spray <dir> <rounds>\r\n", ch );
      send_to_char( "Usage: spray <person> <rounds>\r\n", ch );
      return;
   }
   rounds = atoi( arg2 );

   if( ( dir = get_door( arg ) ) == -1 )
   {
      if( ( victim = get_char_room( ch, arg ) ) == NULL )
      {
         send_to_char( "Invalid targets selected.\r\n", ch );
         return;
      }
   }

   chance = IS_NPC( ch ) ? 100 : ch->pcdata->learned[gsn_spray];
   chance = ( has_wield && has_dual ) ? chance / 2 : chance;

   if( dir != -1 )   /* Spray a direction, blindly */
   {
      short prev_dir = dir;

      if( ( pexit = get_exit( ch->in_room, dir ) ) == NULL )
      {
         send_to_char( "Are you expecting to fire through a wall!?\r\n", ch );
         return;
      }
      if( IS_SET( pexit->exit_info, EX_CLOSED ) )
      {
         send_to_char( "Are you expecting to fire through a door!?\r\n", ch );
         return;
      }

      was_in_room = ch->in_room;
      to_room = NULL;
      if( pexit->distance > 1 )
         to_room = generate_exit( ch->in_room, &pexit );
      if( to_room == NULL )
         to_room = pexit->to_room;

      char_from_room( ch );
      char_to_room( ch, to_room );
      victim = get_random_victim( ch );
      if( victim )
         pfound = TRUE;
      char_from_room( ch );
      char_to_room( ch, was_in_room );

      if( !pfound )
      {
         ch_printf( ch, "You don't see anybody to the %s!\r\n", dir_name[dir] );
         return;
      }
      if( victim == ch )
      {
         send_to_char( "Shoot yourself ... really?\r\n", ch );
         return;
      }
      if( IS_SET( victim->in_room->room_flags, ROOM_SAFE ) )
      {
         set_char_color( AT_MAGIC, ch );
         send_to_char( "You can't shoot them there.\r\n", ch );
         return;
      }
      if( is_safe( ch, victim ) )
         return;
      if( ch->position == POS_FIGHTING )
      {
         send_to_char( "You do the best you can!\r\n", ch );
         return;
      }

      WAIT_STATE( ch, PULSE_VIOLENCE * 2 );

      if( has_wield )
      {
         snprintf( buf, MAX_STRING_LENGTH, "$n points $p to the %s and squeezes the trigger.", dir_name[prev_dir] );
         act( AT_ACTION, buf, ch, wield, NULL, TO_ROOM );
         snprintf( buf, MAX_STRING_LENGTH, "You point $p to the %s and squeeze the trigger.", dir_name[prev_dir] );
         act( AT_ACTION, buf, ch, wield, NULL, TO_CHAR );
      }
      if( has_dual )
      {
         snprintf( buf, MAX_STRING_LENGTH, "$n points $p to the %s and squeezes the trigger.", dir_name[prev_dir] );
         act( AT_ACTION, buf, ch, dual_wield, NULL, TO_ROOM );
         snprintf( buf, MAX_STRING_LENGTH, "You point $p to the %s and squeeze the trigger.", dir_name[prev_dir] );
         act( AT_ACTION, buf, ch, dual_wield, NULL, TO_CHAR );
      }

      char_from_room( ch );
      char_to_room( ch, victim->in_room );

      for( ; rounds > 0; rounds-- )
      {
         if( has_wield && wield->value[4] <= 0 && ammo_wield )
         {
            has_wield = FALSE;
            act( AT_ACTION, "$p clicks as you squeeze the trigger.", ch, wield, NULL, TO_CHAR );
         }
         if( has_dual && dual_wield->value[4] <= 0 && ammo_dual )
         {
            has_dual = FALSE;
            act( AT_ACTION, "$p clicks as you squeeze the trigger.", ch, dual_wield, NULL, TO_CHAR );
         }
         if( !has_wield && !has_dual )
         {
            rounds = 0;
            continue;
         }

         if( chance <= 0 )   /* All remaining shots are wasted */
         {
            snprintf( buf, MAX_STRING_LENGTH, "A wild spray of automatic fire storms in from the %s.", dir_name[dir] );
            act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );
            if( has_wield && ammo_wield )
            {
               wield->value[4] -= rounds;
               if( wield->value[4] < 0 )
                  wield->value[4] = 0;
            }
            if( has_dual && ammo_dual )
            {
               dual_wield->value[4] -= rounds;
               if( dual_wield->value[4] < 0 )
                  dual_wield->value[4] = 0;
            }
            learn_from_failure( ch, gsn_spray );
            rounds = 0;
            continue;
         }

         if( number_percent(  ) < chance )
         {
            victim = get_random_victim( ch );
            if( victim == NULL )
            {
               if( has_wield && ammo_wield )
               {
                  wield->value[4] -= rounds;
                  if( wield->value[4] < 0 )
                     wield->value[4] = 0;
               }
               if( has_dual && ammo_dual )
               {
                  dual_wield->value[4] -= rounds;
                  if( dual_wield->value[4] < 0 )
                     dual_wield->value[4] = 0;
               }
               rounds = 0;
               continue;
            }

            snprintf( buf, MAX_STRING_LENGTH, "A sweeping torrent of bullets spray from the %s at $N.", dir_name[dir] );
            act( AT_ACTION, buf, ch, NULL, victim, TO_NOTVICT );

            if( has_wield && !char_died( victim ) )
            {
               act( AT_ACTION, "You lock dead-onto $N and mercilessly spray $M with $p.", ch, wield, victim, TO_CHAR );
               dual_flip = FALSE;
               one_hit( ch, victim, TYPE_UNDEFINED );
               if( ammo_wield )
                  wield->value[4]--;
            }
            if( has_dual && !char_died( victim ) )
            {
               act( AT_ACTION, "You raise your off-hand and spray an arch of shells at $N with $p.", ch, dual_wield, victim, TO_CHAR );
               dual_flip = TRUE;
               one_hit( ch, victim, TYPE_UNDEFINED );
               if( ammo_dual )
                  dual_wield->value[4]--;
            }
            learn_from_success( ch, gsn_spray );

            if( IS_NPC( victim ) && !char_died( victim ) )
            {
               start_hating( victim, ch );
               start_hunting( victim, ch );
            }
         }
         else
         {
            snprintf( buf, MAX_STRING_LENGTH, "A hellish spray of stray autofire cuts from the %s.", dir_name[dir] );
            act( AT_ACTION, buf, ch, NULL, NULL, TO_ROOM );
            if( has_wield )
            {
               act( AT_ACTION, "The recoil slackens your grip, and $p goes wildly off-target.", ch, wield, NULL, TO_CHAR );
               if( ammo_wield )
               {
                  wield->value[4]--;
                  if( wield->value[4] <= 0 )
                     wield->value[4] = 0;
               }
            }
            if( has_dual )
            {
               act( AT_ACTION, "Your grip shakes as $p locks and goes wild with recoil.", ch, dual_wield, NULL, TO_CHAR );
               if( ammo_dual )
               {
                  dual_wield->value[4]--;
                  if( dual_wield->value[4] <= 0 )
                     dual_wield->value[4] = 0;
               }
            }
            learn_from_failure( ch, gsn_spray );
         }
         chance -= 10;
      }

      char_from_room( ch );
      char_to_room( ch, was_in_room );

      if( had_wield && !has_wield )
         act( AT_ACTION, "$p clicks madly as $n squeezes the trigger once.", ch, wield, NULL, TO_ROOM );
      if( had_dual && !has_dual )
         act( AT_ACTION, "$p clicks insanely as $n squeezes the trigger once.", ch, dual_wield, NULL, TO_ROOM );
   }
   else   /* Spray a specific person directly in the room */
   {
      WAIT_STATE( ch, PULSE_VIOLENCE );

      if( has_wield )
      {
         act( AT_ACTION, "$n levels $s $p at $N and squeezes the trigger.", ch, wield, victim, TO_NOTVICT );
         act( AT_ACTION, "$n smoothly swings $p at you and squeezes the trigger.", ch, wield, victim, TO_VICT );
         act( AT_ACTION, "You narrow your eyes and target $p at $N, clicking the trigger.", ch, wield, victim, TO_CHAR );
      }
      if( has_dual )
      {
         act( AT_ACTION, "$n extends $p towards $N and fires off a spray of bullets.", ch, dual_wield, victim, TO_NOTVICT );
         act( AT_ACTION, "$n turns $p at you and clicks in the trigger.", ch, dual_wield, victim, TO_VICT );
         act( AT_ACTION, "You turn $p to $N and spray $M to kill.", ch, dual_wield, victim, TO_CHAR );
      }

      for( ; rounds > 0; rounds-- )
      {
         if( has_wield && ammo_wield && wield->value[4] <= 0 )
         {
            act( AT_ACTION, "$p clicks wildly, but does nothing.", ch, wield, victim, TO_CHAR );
            has_wield = FALSE;
         }
         if( has_dual && ammo_dual && dual_wield->value[4] <= 0 )
         {
            act( AT_ACTION, "$p does nothing but click wildly.", ch, dual_wield, victim, TO_CHAR );
            has_dual = FALSE;
         }
         if( !has_wield && !has_dual )
         {
            rounds = 0;
            continue;
         }
         if( char_died( victim ) )
         {
            rounds = 0;
            continue;
         }

         if( number_percent(  ) < chance )
         {
            if( has_wield )
            {
               act( AT_ACTION, "You lock dead-onto $N and mercilessly spray $M with $p.", ch, wield, victim, TO_CHAR );
               dual_flip = FALSE;
               one_hit( ch, victim, TYPE_UNDEFINED );
               if( ammo_wield )
                  wield->value[4]--;
            }
            if( has_dual && !char_died( victim ) )
            {
               act( AT_ACTION, "You raise your off-hand and spray an arch of shells at $N with $p.", ch, dual_wield, victim, TO_CHAR );
               dual_flip = TRUE;
               one_hit( ch, victim, TYPE_UNDEFINED );
               if( ammo_dual )
                  dual_wield->value[4]--;
            }
            learn_from_success( ch, gsn_spray );
         }
         else
         {
            act( AT_ACTION, "You fire a shot from $p that goes wild.", ch, wield ? wield : dual_wield, victim, TO_CHAR );
            if( has_wield && ammo_wield )
               wield->value[4]--;
            if( has_dual && ammo_dual )
               dual_wield->value[4]--;
            learn_from_failure( ch, gsn_spray );
         }
         chance -= 10;
      }
   }
}

void do_haymaker( CHAR_DATA *ch, const char *argument )
{
    CHAR_DATA *victim;
    int martist = 1;
    int dam = 0;
    char arg[MAX_INPUT_LENGTH];
    AFFECT_DATA af;
    int level;
    int cap = 60;

    if(ch->subclass == SUBCLASS_MARTIST)
    {
       martist = 2;
       cap = 80;
    }
    one_argument( argument, arg );

    if ( !IS_NPC(ch)
    &&   ch->pcdata->learned[gsn_haymaker] <= 0 )
    {
        send_to_char(
            "Your mind races as you realize you have no idea how to do that.\n\r", ch );
        return;
    }

    if ( arg[0] == '\0' )
    {
        if( ( victim = who_fighting ( ch )) == NULL)
        {
           send_to_char( "Swing a haymaker at whom?\n\r", ch );
           return;
        }
    }
    else if ( ( victim = get_char_room( ch, arg ) ) == NULL )
    {
        send_to_char( "They aren't here.\n\r", ch );
        return;
    }

    if( victim == ch )
    {
       send_to_char( "A novel idea, but you'd fall over first.\n\r", ch );
       return;
    }

    level = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_haymaker] );
    if(IS_NPC(ch))
    {
       dam = number_range( ch->barenumdie, ch->baresizedie * ch->barenumdie );
       dam += ch->damplus;
    }
    else
         dam = number_range( ch->skill_level[COMBAT_ABILITY]/2, ch->skill_level[COMBAT_ABILITY] );

    dam += ch->damroll + level;
    dam *= martist;
    WAIT_STATE( ch, skill_table[gsn_haymaker]->beats );
    WAIT_STATE( ch, PULSE_VIOLENCE );

    if ( IS_NPC(ch) || number_percent( ) < UMIN(cap, level) )
    {
        learn_from_success( ch, gsn_haymaker);
        if( check_save( gsn_haymaker, level, ch, victim ) )
            dam /= 2;
        global_retcode = damage( ch, victim, dam, gsn_haymaker );
    }
    else
    {
        af.type      = gsn_haymaker;
        af.location  = APPLY_SUSCEPTIBLE;
        af.modifier  = RIS_NONMAGIC + RIS_MAGIC;
        af.duration  = number_range(1, 3);
        af.bitvector = AFF_NONE;
        affect_join( ch, &af );
        learn_from_failure( ch, gsn_haymaker );
        global_retcode = damage( ch, victim, 0, gsn_haymaker );
    }
    return;
}

void do_trace_comlink( CHAR_DATA *ch, const char *argument )
{
    AFFECT_DATA af;

    if ( ch->mount )
    {
	send_to_char( "You can't do that while mounted.\n\r", ch );
	return;
    }

    send_to_char( "You begin messing with some wires on your comlink.\n\r", ch );
    affect_strip( ch, gsn_trace_comlink );

    if ( number_percent( ) < ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_trace_comlink] )  )
    {
	af.type      = gsn_trace_comlink;
	af.duration  = ch->skill_level[HUNTING_ABILITY]  * DUR_CONV;
	af.location  = APPLY_NONE;
	af.modifier  = 0;
	af.bitvector = AFF_TRACING;
	affect_to_char( ch, &af );
	learn_from_success( ch, gsn_trace_comlink );
    }
    else
	learn_from_failure( ch, gsn_trace_comlink );

    return;
}

void do_trap( CHAR_DATA *ch, const char *argument )
{
    OBJ_DATA *obj, *target;
    short chance;
    char arg[MAX_INPUT_LENGTH];
    char argd[MAX_INPUT_LENGTH];
    short dir;
    EXIT_DATA       * pexit;
    bool target_room = TRUE;
    bool target_door = FALSE;

    argument = one_argument( argument, argd );
    argument = one_argument( argument, arg );

    if (!knows_skill(ch, gsn_traps ) )
    {
      ch_printf( ch, "You have no idea how to do that.\n\r" );
      return;
    }

    chance = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_traps] );

    obj = get_obj_carry( ch, argd );

    if ( !obj )
    {
       ch_printf( ch, "You are not carrying that item.\n\r" );
       return;
    }
    if( obj->item_type != ITEM_TRAP_KIT )
    {
       ch_printf( ch, "That isn't a trap kit.\n\r" );
       return;
    }

    if( obj->value[4] != TRAP_MECHANISM_TRIPWIRE ) /* We don't need to have a specific exit, but let them attach traps to doors */
    {
        target = get_obj_here( ch, arg ); 
        if( target != NULL )
        {
            separate_obj(target);
            if( target == obj || target->item_type == ITEM_TRAP )
            {
                send_to_char( "You can't trap a trap. Trap something else.\n\r", ch );
                return;
            }           
            target_room = FALSE;
        }
        else
        {
            dir = get_door(arg);
            if( dir != -1 )
            {
               target_room = FALSE; 
               target_door  = TRUE;
               if( dir == DIR_SOMEWHERE )
               {
                  send_to_char( "There isn't room for a trap there.\n\r", ch );
                  return;
               }
               if ( ( pexit = get_exit( ch->in_room, dir ) ) == NULL )
               {
                   send_to_char( "There isn't anything there to trap.\n\r", ch );
                   return;
               }
            }
        }
        if( number_percent() > chance )
        {
           send_to_char( "Careful now, you're going to hurt yourself.\n\r", ch );
           learn_from_failure( ch, gsn_traps );
           return;
        }
        separate_obj( obj );
        obj->item_type = ITEM_TRAP;
        obj->value[3]  = 0;
        if( obj->value[4] == TRAP_MECHANISM_HEAT )
            SET_BIT( obj->value[3], TRAP_EXAMINE ); /* Don't get too to these ones. */
        WAIT_STATE( ch, PULSE_VIOLENCE ); 
        if( target_room ) 
        {   
            SET_BIT( obj->value[3], TRAP_ROOM );
            SET_BIT( obj->value[3], TRAP_ENTER_ROOM );
            /* Exceptionally tempting to add TRAP_LEAVE_ROOM, but then this option would be probably the dumbest
             * trap to place; because it leaves you victim to your own trap.
             * -Lajos
             */
             obj_from_char( obj );
             obj_to_room( obj, ch->in_room );
             ch_printf( ch, "You successfully place the trap.\n\r" );
        }
        else if( target_door )
        {
           SET_BIT( obj->value[3], TRAP_ROOM );
           switch(dir)
           {
           default:
           case DIR_NORTH:      SET_BIT( obj->value[3], TRAP_N );  break;
           case DIR_EAST:       SET_BIT( obj->value[3], TRAP_E );  break;
           case DIR_SOUTH:      SET_BIT( obj->value[3], TRAP_S );  break;
           case DIR_WEST:       SET_BIT( obj->value[3], TRAP_W );  break;
           case DIR_UP:         SET_BIT( obj->value[3], TRAP_U );  break;
           case DIR_DOWN:       SET_BIT( obj->value[3], TRAP_D );  break;
           case DIR_NORTHEAST:  SET_BIT( obj->value[3], TRAP_NE ); break;
           case DIR_NORTHWEST:  SET_BIT( obj->value[3], TRAP_NW ); break;
           case DIR_SOUTHEAST:  SET_BIT( obj->value[3], TRAP_SE ); break;
           case DIR_SOUTHWEST:  SET_BIT( obj->value[3], TRAP_SW ); break;
           }
           /* Since these are not tripwires, we can pretty much assume they'll go off as soon as someone fucks with them
            * -Lajos
            */
           SET_BIT( obj->value[3], TRAP_PICK );
           SET_BIT( obj->value[3], TRAP_UNLOCK );
           SET_BIT( obj->value[3], TRAP_OPEN );
           SET_BIT( obj->value[3], TRAP_CLOSE );
           ch_printf( ch, "You successfully trap the %s exit.\n\r", dir_name[dir] );
           obj_from_char( obj );
           obj_to_room( obj, ch->in_room );     
        }
        else if( target != NULL )
        {
           SET_BIT( obj->value[3], TRAP_OBJ );
           SET_BIT( obj->value[3], TRAP_OPEN );
           SET_BIT( obj->value[3], TRAP_PUT );
           SET_BIT( obj->value[3], TRAP_CLOSE );
           SET_BIT( obj->value[3], TRAP_GET );
           obj_from_char( obj );
           obj_to_obj( obj, target );
           ch_printf(ch, "You successfully place your trap.\n\r" );
        }       
        learn_from_success( ch, gsn_traps );
        return;
    }
   
    /* Now we do need an exit */
   if ( ( dir = get_door( arg ) ) == -1 )
   {
     send_to_char( "Syntax: set_trap <trapkit> <direction>\n\r", ch );
     return;
   }
   if( dir == DIR_SOMEWHERE )
   {
     send_to_char( "There isn't room for a trap there.\n\r", ch );
     return;
   }

   if ( ( pexit = get_exit( ch->in_room, dir ) ) == NULL )
   {
     send_to_char( "There isn't anything there to trap.\n\r", ch );
     return;
   }
   if( number_percent() > chance )
   {
     send_to_char( "Careful now, you might hurt yourself.\n\r", ch );
     learn_from_failure( ch, gsn_traps );
     return;
   }
   separate_obj( obj );
   obj->item_type = ITEM_TRAP;
   obj->value[3] = 0;
   SET_BIT( obj->value[3], TRAP_ROOM );
   switch(dir)
   {
   default:
   case DIR_NORTH:      SET_BIT( obj->value[3], TRAP_N );  break;
   case DIR_EAST:       SET_BIT( obj->value[3], TRAP_E );  break;
   case DIR_SOUTH:      SET_BIT( obj->value[3], TRAP_S );  break;
   case DIR_WEST:       SET_BIT( obj->value[3], TRAP_W );  break;
   case DIR_UP:         SET_BIT( obj->value[3], TRAP_U );  break;
   case DIR_DOWN:       SET_BIT( obj->value[3], TRAP_D );  break;
   case DIR_NORTHEAST:  SET_BIT( obj->value[3], TRAP_NE ); break;
   case DIR_NORTHWEST:  SET_BIT( obj->value[3], TRAP_NW ); break;
   case DIR_SOUTHEAST:  SET_BIT( obj->value[3], TRAP_SE ); break;
   case DIR_SOUTHWEST:  SET_BIT( obj->value[3], TRAP_SW ); break;
   }
   WAIT_STATE( ch, PULSE_VIOLENCE ); 
   ch_printf( ch, "You successfully trap the %s exit.\n\r", dir_name[dir] );
   obj_from_char( obj );
   obj_to_room( obj, ch->in_room );
   learn_from_success( ch, gsn_traps );
   return;
}

void do_besiege( CHAR_DATA *ch, const char *argument )
{
    int chance; 
    SHIP_DATA *ship;
    char logbuf[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];
            
    	        if ( (ship = ship_from_cockpit(ch->in_room->vnum)) == NULL )  
    	        {
    	            send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
    	            return;
    	        }
    	        
    	        if ( ship->ship_class > SHIP_PLATFORM )
    	        {
    	            send_to_char("&RThis isn't a spacecraft!\n\r",ch);
    	            return;
    	        }
    	        
    	        if ( (ship = ship_from_pilotseat(ch->in_room->vnum)) == NULL )  
    	        {
    	            send_to_char("&RYou don't seem to be in the pilot seat!\n\r",ch);
    	            return;
    	        }
    	        
    	        if ( check_pilot( ch , ship ) )
    	        {
    	            send_to_char("&RWhat would be the point of that!?\n\r",ch);
    	            return;
    	        }
    	        
    	        if ( ship->type == MOB_SHIP )
    	        {
    	            send_to_char("&RThis ship isn't pilotable at this point in time...\n\r",ch);
    	            return;
    	        }
    	        
                if  ( ship->ship_class == SHIP_PLATFORM )
                {
                   send_to_char( "You can't do that here.\n\r" , ch );
                   return;
                }   
    
		if ( ship->autopilot==FALSE )
		{
		    send_to_char( "This ship's autopilot is already off.\n\r", ch);
		    return;
		}

                chance = IS_NPC(ch) ? ch->top_level
	                 : ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_besiege] );
                if ( number_percent( ) > chance )
    		{  
    		    send_to_char("You fail to disable the autopilot.\n\r",ch);
                    learn_from_failure( ch, gsn_besiege );
    	            return;
                }

                {              
                
     		   set_char_color( AT_GREEN, ch );

    		   send_to_char( "Autopilot disabled.\n\r", ch);
    		   act( AT_PLAIN, "$n starts up the ship and begins the launch sequence.", ch,
		        NULL, argument , TO_ROOM );
		   echo_to_ship( AT_YELLOW , ship , "The ship's autopilot has been overridden.");
    		   sprintf( buf, "%s disables the autopilot.", ship->name );
    		   echo_to_room( AT_YELLOW , get_room_index(ship->location) , buf );
    		   ship->currspeed = ship->realspeed;
		   ship->autopilot = FALSE;
                   learn_from_success( ch, gsn_besiege );
                   sprintf( buf, "%s broadcasts an SOS signal!", ship->name );
    		   echo_to_all( AT_RED , buf, 0 );
                   sprintf( logbuf, "%s has been sieged by %s!", ship->name, ch->name);
                   log_string( logbuf );
    		   
                   return;   	   	
                }
    	   	return;	
    	
}

void do_palm( CHAR_DATA *ch, const char *argument )
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    OBJ_DATA *container;
    short number;
    bool found;

    argument = one_argument( argument, arg1 );
    if ( is_number(arg1) )
    {
	number = atoi(arg1);
	if ( number < 1 )
	{
	    send_to_char( "Mine now.\n\r", ch );
	    return;
	}
	if ( (ch->carry_number + number) > can_carry_n(ch) )
	{
	    send_to_char( "You can't carry that many.\n\r", ch );
	    return;
	}
	argument = one_argument( argument, arg1 );
    }
    else
	number = 0;
    argument = one_argument( argument, arg2 );
    /* munch optional words */
    if ( !str_cmp( arg2, "from" ) && argument[0] != '\0' )
	argument = one_argument( argument, arg2 );

    /* Get type. */
    if ( arg1[0] == '\0' )
    {
	send_to_char( "Palm what?\n\r", ch );
	return;
    }

    if ( ms_find_obj(ch) )
	return;

    if ( arg2[0] == '\0' )
    {
	if ( number <= 1 && str_cmp( arg1, "all" ) && str_prefix( "all.", arg1 ) )
	{
	    /* 'get obj' */
	    obj = get_obj_list( ch, arg1, ch->in_room->first_content );
	    if ( !obj )
	    {
		act( AT_PLAIN, "I see no $T here.", ch, NULL, arg1, TO_CHAR );
		return;
	    }
	    separate_obj(obj);
	    get_obj_palm( ch, obj, NULL );
	    if ( char_died(ch) )
		return;
	    if ( IS_SET( sysdata.save_flags, SV_GET ) )
		save_char_obj( ch );
	}
	else
	{
	    short cnt = 0;
	    bool fAll;
	    char *chk;

	    if ( IS_SET( ch->in_room->room_flags, ROOM_DONATION ) )
	    {
		send_to_char( "The gods frown upon such a display of greed!\n\r", ch );
		return;
	    }
	    if ( !str_cmp(arg1, "all") )
		fAll = TRUE;
	    else
		fAll = FALSE;
	    if ( number > 1 )
		chk = arg1;
	    else
		chk = &arg1[4];
	    /* 'get all' or 'get all.obj' */
	    found = FALSE;
	    for ( obj = ch->in_room->first_content; obj; obj = obj_next )
	    {
		obj_next = obj->next_content;
		if ( ( fAll || nifty_is_name( chk, obj->name ) )
		&&   can_see_obj( ch, obj ) )
		{
		    found = TRUE;
		    if ( number && (cnt + obj->count) > number )
			split_obj( obj, number - cnt );
		    cnt += obj->count;
		    get_obj_palm( ch, obj, NULL );
		    if ( char_died(ch)
		    ||   ch->carry_number >= can_carry_n( ch )
		    ||   ch->carry_weight >= can_carry_w( ch )
		    ||   (number && cnt >= number) )
		    {
			if ( IS_SET(sysdata.save_flags, SV_GET)
			&&  !char_died(ch) )
			    save_char_obj(ch);
			return;
		    }
		}
	    }

	    if ( !found )
	    {
		if ( fAll )
		  send_to_char( "I see nothing here.\n\r", ch );
		else
		  act( AT_PLAIN, "I see no $T here.", ch, NULL, chk, TO_CHAR );
	    }
	    else
	    if ( IS_SET( sysdata.save_flags, SV_GET ) )
		save_char_obj( ch );
	}
    }
    else
    {
	/* 'get ... container' */
	if ( !str_cmp( arg2, "all" ) || !str_prefix( "all.", arg2 ) )
	{
	    send_to_char( "You can't do that.\n\r", ch );
	    return;
	}

	if ( ( container = get_obj_here( ch, arg2 ) ) == NULL )
	{
	    act( AT_PLAIN, "I see no $T here.", ch, NULL, arg2, TO_CHAR );
	    return;
	}

	switch ( container->item_type )
	{
	default:
	    if ( !IS_OBJ_STAT( container, ITEM_COVERING ) )
	    {
		send_to_char( "That's not a container.\n\r", ch );
		return;
	    }
	    if ( ch->carry_weight + container->weight > can_carry_w( ch ) )
	    {
		send_to_char( "It's too heavy for you to lift.\n\r", ch );
		return;
	    }
	    break;

	case ITEM_CONTAINER:
	case ITEM_DROID_CORPSE:
	case ITEM_CORPSE_PC:
	case ITEM_CORPSE_NPC:
	    break;
	}

	if ( !IS_OBJ_STAT(container, ITEM_COVERING )
	&&    IS_SET(container->value[1], CONT_CLOSED) )
	{
	    act( AT_PLAIN, "The $d is closed.", ch, NULL, container->name, TO_CHAR );
	    return;
	}

	if ( number <= 1 && str_cmp( arg1, "all" ) && str_prefix( "all.", arg1 ) )
	{
	    /* 'get obj container' */
	    obj = get_obj_list( ch, arg1, container->first_content );
	    if ( !obj )
	    {
		act( AT_PLAIN, IS_OBJ_STAT(container, ITEM_COVERING) ?
			"I see nothing like that beneath the $T." :
			"I see nothing like that in the $T.",
			ch, NULL, arg2, TO_CHAR );
		return;
	    }
	    separate_obj(obj);
	    get_obj_palm( ch, obj, container );

	    check_for_trap( ch, container, TRAP_GET );
	    if ( char_died(ch) )
	      return;
	    if ( IS_SET( sysdata.save_flags, SV_GET ) )
		save_char_obj( ch );
	}
	else
	{
	    int cnt = 0;
	    bool fAll;
	    char *chk;

	    /* 'get all container' or 'get all.obj container' */
/*	    if ( IS_OBJ_STAT( container, ITEM_DONATION ) )
	    {
		send_to_char( "The gods frown upon such an act of greed!\n\r", ch );
		return;
	    }*/
	    if ( !str_cmp(arg1, "all") )
		fAll = TRUE;
	    else
		fAll = FALSE;
	    if ( number > 1 )
		chk = arg1;
	    else
		chk = &arg1[4];
	    found = FALSE;
	    for ( obj = container->first_content; obj; obj = obj_next )
	    {
		obj_next = obj->next_content;
		if ( ( fAll || nifty_is_name( chk, obj->name ) )
		&&   can_see_obj( ch, obj ) )
		{
		    found = TRUE;
		    if ( number && (cnt + obj->count) > number )
			split_obj( obj, number - cnt );
		    cnt += obj->count;
		    get_obj_palm( ch, obj, container );
		    if ( char_died(ch)
		    ||   ch->carry_number >= can_carry_n( ch )
		    ||   ch->carry_weight >= can_carry_w( ch )
		    ||   (number && cnt >= number) )
		      return;
		}
	    }

	    if ( !found )
	    {
		if ( fAll )
		  act( AT_PLAIN, IS_OBJ_STAT(container, ITEM_COVERING) ?
			"I see nothing beneath the $T." :
			"I see nothing in the $T.",
			ch, NULL, arg2, TO_CHAR );
		else
		  act( AT_PLAIN, IS_OBJ_STAT(container, ITEM_COVERING) ?
			"I see nothing like that beneath the $T." :
			"I see nothing like that in the $T.",
			ch, NULL, arg2, TO_CHAR );
	    }
	    else
	      check_for_trap( ch, container, TRAP_GET );
	    if ( char_died(ch) )
		return;
	    if ( found && IS_SET( sysdata.save_flags, SV_GET ) )
		save_char_obj( ch );
	}
    }
    return;
}

void do_seal( CHAR_DATA *ch, const char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    EXIT_DATA *pexit;
    int chance;
    one_argument( argument, arg );

    if ( arg[0] == '\0' )
    {
        send_to_char( "Seal what?\n\r", ch );
        return;
    }
    if ( ( pexit = find_door( ch, arg, TRUE ) ) != NULL )
    {
        /* 'lock door' */
           if( IS_SET(pexit->exit_info, EX_SECRET ) )
               { send_to_char("You don't see that here.\n\r", ch); return; }
           if ( !IS_SET(pexit->exit_info, EX_ISDOOR) )
               { send_to_char( "You can't do that.\n\r",      ch ); return; }
           if ( !IS_SET(pexit->exit_info, EX_CLOSED) )
               { send_to_char( "It's not closed.\n\r",        ch ); return; }
           if ( pexit->key < 0 )
               { send_to_char( "It can't be sealed.\n\r",     ch ); return; }
           if ( IS_SET(pexit->exit_info, EX_LOCKED) )
               { send_to_char( "It's already locked.\n\r",    ch ); return; }
           chance = IS_NPC(ch) ? ch->skill_level[SMUGGLING_ABILITY]
                               : ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_seal] );
           if(number_percent() > chance)
           {
              send_to_char("You failed.\n\r", ch);
              learn_from_failure(ch, gsn_seal);
              act( AT_ACTION, "$n attempts to seal up the $d.", ch, NULL, pexit->keyword, TO_ROOM );
              return;
           }
           send_to_char( "*Clack*\n\r", ch );
           act( AT_ACTION, "$n seals up the $d.", ch, NULL, pexit->keyword, TO_ROOM );
           set_bexit_flag( pexit, EX_LOCKED );
           learn_from_success(ch, gsn_seal);  
           return;
    }
    ch_printf( ch, "You see no %s here.\n\r", arg );
    return;
}

void do_slide( CHAR_DATA *ch, const char *argument )
{
    char arg1 [MAX_INPUT_LENGTH];
    char arg2 [MAX_INPUT_LENGTH];
    char buf  [MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    CHAR_DATA *rch;
    OBJ_DATA  *obj;
    int chance, percent;

    chance = IS_NPC(ch) ? 50
                        : ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_slide] )*.75+ch->mod_dex/2;

    argument = one_argument( argument, arg1 );
    argument = one_argument( argument, arg2 );
    if ( !str_cmp( arg2, "to" ) && argument[0] != '\0' )
        argument = one_argument( argument, arg2 );

    if ( arg1[0] == '\0' || arg2[0] == '\0' )
    {
        send_to_char( "Slide what to whom?\n\r", ch );
        return;
    }

    if ( ms_find_obj(ch) )
        return;
    if ( is_number( arg1 ) )
    {
        /* 'give NNNN coins victim' */
        int amount;

        amount   = atoi(arg1);
        if ( amount <= 0
        || ( str_cmp( arg2, "credits" ) && str_cmp( arg2, "credit" ) ) )
        {
            send_to_char( "Sorry, you can't do that.\n\r", ch );
            return;
        }

        argument = one_argument( argument, arg2 );
        if ( !str_cmp( arg2, "to" ) && argument[0] != '\0' )
          argument = one_argument( argument, arg2 );
        if ( arg2[0] == '\0' )
        {
            send_to_char( "Slide what to whom?\n\r", ch );
            return;
        }

        if ( ( victim = get_char_room( ch, arg2 ) ) == NULL )
        {
            send_to_char( "They aren't here.\n\r", ch );
            return;
        }

        if ( ch->gold < amount )
        {
            send_to_char( "Very generous of you, but you haven't got that many credits.\n\r", ch );
            return;
        }

        ch->gold     -= amount;
        victim->gold += amount;
        strcpy(buf, "$n gives you ");
        strcat(buf, arg1);
        strcat(buf, (amount > 1) ? " credits." : " credit.");
        set_char_color( AT_ACTION, ch );
        ch_printf( ch, "You slide %s some credits.\n\r", NAME(victim) );
        for ( rch = ch->in_room->first_person; rch; rch = rch->next_in_room )
        {
           percent = number_percent();
           if ( !can_see( rch, ch ) )
             percent -= 25; /* Kinda hard to detect someone you can't see. */
           if( rch == ch ) /* blah that I forgot this, you don't count. */
             continue;
           if(IS_AWAKE(rch))
           {
             if(IS_AFFECTED(rch, AFF_SIT_AWARE))
             {
                percent += 15;
             }
           }
          else /* How are you going to see someone give something when you are asleep? */
          {
             percent = 0;
          }
          if(IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
             percent += 100;
          if(percent > chance)
          {
             set_char_color(AT_ACTION, rch);
             if(victim == rch )
                ch_printf( rch, "%s\n\r", buf );
             else
                ch_printf( rch, "%s gives %s some credits.\n\r", NAME(ch), NAME(victim) );
             learn_from_failure(ch, gsn_slide);
          }
          else
             learn_from_success(ch, gsn_slide);
        }
        send_to_char( "OK.\n\r", ch );
        mprog_bribe_trigger( victim, ch, amount );
        if ( IS_SET( sysdata.save_flags, SV_GIVE ) && !char_died(ch) )
          save_char_obj(ch);
        if ( IS_SET( sysdata.save_flags, SV_RECEIVE ) && !char_died(victim) )
          save_char_obj(victim);
        return;
    }

    if ( ( obj = get_obj_carry( ch, arg1 ) ) == NULL )
    {
        send_to_char( "You do not have that item.\n\r", ch );
        return;
    }

    if ( obj->wear_loc != WEAR_NONE )
    {
        send_to_char( "You must remove it first.\n\r", ch );
        return;
    }

    if ( ( victim = get_char_room( ch, arg2 ) ) == NULL )
    {
        send_to_char( "They aren't here.\n\r", ch );
        return;
    }

    if ( !can_drop_obj( ch, obj ) )
    {
        send_to_char( "You can't let go of it.\n\r", ch );
        return;
    }

    if ( victim->carry_number + (get_obj_number(obj)/obj->count) > can_carry_n( victim ) )
    {
        act( AT_PLAIN, "$N has $S hands full.", ch, NULL, victim, TO_CHAR );
        return;
    }

    if ( victim->carry_weight + (get_obj_weight(obj)/obj->count) > can_carry_w( victim ) )
    {
        act( AT_PLAIN, "$N can't carry that much weight.", ch, NULL, victim, TO_CHAR );
        return;
    }

    if ( !can_see_obj( victim, obj ) )
    {
        act( AT_PLAIN, "$N can't see it.", ch, NULL, victim, TO_CHAR );
        return;
    }
    if (IS_OBJ_STAT( obj, ITEM_PROTOTYPE ) && !can_take_proto( victim ) )
    {
        act( AT_PLAIN, "You cannot give that to $N!", ch, NULL, victim, TO_CHAR );
        return;
    }

    separate_obj( obj );
    obj_from_char( obj );
    set_char_color( AT_ACTION, ch);
    ch_printf( ch, "You slide %s to %s.\n\r", obj->short_descr, NAME(victim) );
    for ( rch = ch->in_room->first_person; rch; rch = rch->next_in_room )
    {
       percent = number_percent();
       if ( !can_see( rch, ch ) )
         percent -= 25; /* Kinda hard to detect someone you can't see. */
       if( rch == ch ) /* blah that I forgot this, you don't count. */
         continue;
       if(IS_AWAKE(rch))
       {
         if(IS_AFFECTED(rch, AFF_SIT_AWARE))
         {
            percent += 15;
         }
       }
       else /* How are you going to see someone give something when you are asleep? */
       {
         percent = 0;
       }
       if(IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
          percent += 100;
       if(percent > chance)
       {
          set_char_color(AT_ACTION, rch);
          if(victim == rch )
             ch_printf( rch, "%s gives you %s.\n\r", NAME(ch), obj->short_descr );
          else
             ch_printf( rch, "%s gives %s %s.\n\r", NAME(ch), NAME(victim), obj->short_descr );
          learn_from_failure(ch, gsn_slide);
       }
       else
          learn_from_success(ch, gsn_slide);
    }
    obj = obj_to_char( obj, victim );

    mprog_give_trigger( victim, ch, obj );
    if ( IS_SET( sysdata.save_flags, SV_GIVE ) && !char_died(ch) )
        save_char_obj(ch);
    if ( IS_SET( sysdata.save_flags, SV_RECEIVE ) && !char_died(victim) )
        save_char_obj(victim);
    return;
}

void do_tuck( CHAR_DATA *ch, const char *argument )
{
    char arg1[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    OBJ_DATA *container;
    OBJ_DATA *obj;
    OBJ_DATA *obj_next;
    CLAN_DATA *clan;
    short      count;
    int         number;
    bool        save_char = FALSE;
    int percent, chance;
    CHAR_DATA *rch;
    chance = IS_NPC(ch) ? 50
                        : ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_tuck] )*.75+ch->mod_dex/2;
    argument = one_argument( argument, arg1 );
    if ( is_number(arg1) )
    {
        number = atoi(arg1);
        if ( number < 1 )
        {
            send_to_char( "That was easy...\n\r", ch );
            return;
        }
        argument = one_argument( argument, arg1 );
    }
    else
        number = 0;
    argument = one_argument( argument, arg2 );
    /* munch optional words */
    if ( (!str_cmp(arg2, "into") || !str_cmp(arg2, "inside") || !str_cmp(arg2, "in"))
    &&   argument[0] != '\0' )
        argument = one_argument( argument, arg2 );

    if ( arg1[0] == '\0' || arg2[0] == '\0' )
    {
        send_to_char( "Tuck what in what?\n\r", ch );
        return;
    }

    if ( ms_find_obj(ch) )
        return;

    if ( !str_cmp( arg2, "all" ) || !str_prefix( "all.", arg2 ) )
    {
        send_to_char( "You can't do that.\n\r", ch );
        return;
    }

    if ( ( container = get_obj_here( ch, arg2 ) ) == NULL )
    {
        act( AT_PLAIN, "I see no $T here.", ch, NULL, arg2, TO_CHAR );
        return;
    }

    if ( !container->carried_by && IS_SET( sysdata.save_flags, SV_PUT ) )
        save_char = TRUE;

    if ( IS_OBJ_STAT(container, ITEM_COVERING) )
    {
        if ( ch->carry_weight + container->weight > can_carry_w( ch ) )
        {
            send_to_char( "It's too heavy for you to lift.\n\r", ch );
            return;
        }
    }
    else
    {
        if ( container->item_type != ITEM_CONTAINER )
        {
            send_to_char( "That's not a container.\n\r", ch );
            return;
        }

        if ( IS_SET(container->value[1], CONT_CLOSED) )
        {
            act( AT_PLAIN, "The $d is closed.", ch, NULL, container->name, TO_CHAR );
            return;
        }
    }

    if ( number <= 1 && str_cmp( arg1, "all" ) && str_prefix( "all.", arg1 ) )
    {
        /* 'put obj container' */
        if ( ( obj = get_obj_carry( ch, arg1 ) ) == NULL )
        {
            send_to_char( "You do not have that item.\n\r", ch );
            return;
        }

        if ( obj == container )
        {
            send_to_char( "You can't fold it into itself.\n\r", ch );
            return;
        }

        if ( !can_drop_obj( ch, obj ) )
        {
            send_to_char( "You can't let go of it.\n\r", ch );
            return;
        }

        if ( (IS_OBJ_STAT(container, ITEM_COVERING)
        &&   (get_obj_weight( obj ) / obj->count)
          > ((get_obj_weight( container ) / container->count)
          -   container->weight)) )
        {
            send_to_char( "It won't fit under there.\n\r", ch );
            return;
        }

        if ( (get_obj_weight( obj ) / obj->count)
           + (get_obj_weight( container ) / container->count)
           >  container->value[0] )
        {
            send_to_char( "It won't fit.\n\r", ch );
            return;
        }

        separate_obj(obj);
        separate_obj(container);
        obj_from_char( obj );
        obj = obj_to_obj( obj, container );
        check_for_trap ( ch, container, TRAP_PUT );
        if ( char_died(ch) )
          return;
        count = obj->count;
        obj->count = 1;
        act( AT_ACTION, IS_OBJ_STAT( container, ITEM_COVERING )
                ? "You tuck $p beneath $P." : "You tuck $p in $P.",
                ch, obj, container, TO_CHAR );
        for ( rch = ch->in_room->first_person; rch; rch = rch->next_in_room )
        {
          percent = number_percent();
          if ( !can_see( rch, ch ) )
             percent -= 25; /* Kinda hard to detect someone you can't see. */
          if( rch == ch ) /* Damn me for forgetting this again. */
             continue;
          if(IS_AWAKE(rch))
          {
             if(IS_AFFECTED(rch, AFF_SIT_AWARE))
             {
                percent += 15;
             }
          }
          else /* How are you going to see someone get something when you are asleep? */
          {
             percent = 0;
          }
          if(IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
             percent += 100;
          if(percent > chance)
          {
             set_char_color(AT_ACTION, rch);
             if(IS_OBJ_STAT( container, ITEM_COVERING ) )
                ch_printf(rch, "%s hides %s beneath %s.\n\r", NAME(ch), obj->short_descr, container->short_descr);
             else
                ch_printf(rch, "%s puts %s in %s.\n\r", NAME(ch), obj->short_descr, container->short_descr); 
             learn_from_failure(ch, gsn_tuck);
          }
          else
             learn_from_success(ch, gsn_tuck);
        }
        obj->count = count;

        if ( save_char )
          save_char_obj(ch);
        /* Clan storeroom check */
        if ( IS_SET(ch->in_room->room_flags, ROOM_CLANSTOREROOM)
        &&   container->carried_by == NULL)
           for ( clan = first_clan; clan; clan = clan->next )
              if ( clan->storeroom == ch->in_room->vnum )
                save_clan_storeroom(ch, clan);
    }
    else
    {
        bool found = FALSE;
        int cnt = 0;
        bool fAll;
        char *chk;

        if ( !str_cmp(arg1, "all") )
            fAll = TRUE;
        else
            fAll = FALSE;
        if ( number > 1 )
            chk = arg1;
        else
            chk = &arg1[4];

        separate_obj(container);
        /* 'put all container' or 'put all.obj container' */
        for ( obj = ch->first_carrying; obj; obj = obj_next )
        {
            obj_next = obj->next_content;

            if ( ( fAll || nifty_is_name( chk, obj->name ) )
            &&   can_see_obj( ch, obj )
            &&   obj->wear_loc == WEAR_NONE
            &&   obj != container
            &&   can_drop_obj( ch, obj )
            &&   get_obj_weight( obj ) + get_obj_weight( container )
                 <= container->value[0] )
            {
                if ( number && (cnt + obj->count) > number )
                    split_obj( obj, number - cnt );
                cnt += obj->count;
                obj_from_char( obj );
                act( AT_ACTION, "You tuck $p in $P.", ch, obj, container, TO_CHAR );
                for ( rch = ch->in_room->first_person; rch; rch = rch->next_in_room )
                {
                   percent = number_percent();
                   if ( !can_see( rch, ch ) )
                      percent -= 25; /* Kinda hard to detect someone you can't see. */
                   if( rch == ch ) /* Damn me for forgetting this again. */
                      continue;
                   if(IS_AWAKE(rch))
                   {
                      if(IS_AFFECTED(rch, AFF_SIT_AWARE))
                      {
                         percent += 15;
                      }
                   }
                   else /* How are you going to see someone get something when you are asleep? */
                   {
                      percent = 0;
                   }
                   if(IS_IMMORTAL(rch) && IS_AWAKE(rch)) /* You aren't sneaking anything by an immortal. */
                       percent += 100;
                   if(percent > chance)
                   {
                      set_char_color(AT_ACTION, rch);
                      ch_printf(rch, "%s puts %s in %s.\n\r", NAME(ch), obj->short_descr, container->short_descr); 
                      learn_from_failure(ch, gsn_tuck);
                   }
                   else
                      learn_from_success(ch, gsn_tuck);
                }
                obj = obj_to_obj( obj, container );
                found = TRUE;

                check_for_trap( ch, container, TRAP_PUT );
                if ( char_died(ch) )
                  return;
                if ( number && cnt >= number )
                  break;
            }
        }

        /*
         * Don't bother to save anything if nothing was dropped   -Thoric
         */
        if ( !found )
        {
            if ( fAll )
              act( AT_PLAIN, "You are not carrying anything.",
                    ch, NULL, NULL, TO_CHAR );
            else
              act( AT_PLAIN, "You are not carrying any $T.",
                    ch, NULL, chk, TO_CHAR );
            return;
        }
        if ( save_char )
            save_char_obj(ch);
        /* Clan storeroom check */
        if ( IS_SET(ch->in_room->room_flags, ROOM_CLANSTOREROOM)
        && container->carried_by == NULL )
          for ( clan = first_clan; clan; clan = clan->next )
             if ( clan->storeroom == ch->in_room->vnum )
                save_clan_storeroom(ch, clan);
    }
    return;
}

void do_rembounty( CHAR_DATA * ch, const char *argument )
{
  BOUNTY_DATA *bounty;
  char buf[MAX_STRING_LENGTH];
    
  if(IS_NPC(ch))
  { 
    ch_printf(ch, "Huh?\n\r");
    return;
  }     
        
  if(!argument || argument[0] == '\0')
  {     
    ch_printf(ch, "Syntax: rembounty <person>\n\r");
    return;
  }     

  if((bounty = get_disintigration(argument)) == NULL)
  {            
    ch_printf(ch, "No bounties on that person.\n\r");
    return;
  }     

  sprintf( buf, "%s has removed the bounty on %s.", ch->name, bounty->target );
  echo_to_all ( AT_RED , buf, 0 );
  
  remove_disintigration(bounty);
  
  return;
}

void do_researchtarget( CHAR_DATA * ch, const char *argument )
{
    AFFECT_DATA af;
    CHAR_DATA *victim;
    int chance;
    if( argument[0] == '\0')
    {
       send_to_char("You need to choose a target.\n\r", ch);
       return;
    }
    if ( ( victim = get_char_world( ch, argument ) ) == NULL
    ||   victim == ch    
    ||   !victim->in_room
    ||  (IS_NPC(victim)) )
    {
        send_to_char( "Your target cannot be found.\n\r", ch );
        return;
    }
    if(IS_NPC(victim))
    {
        send_to_char("This skill doesn't work on NPCs.\n\r", ch);
        return;
    }
    chance = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_researchtarget] );
    WAIT_STATE(ch, PULSE_VIOLENCE);
    if (  number_percent() >= chance )
    {
        ch_printf(ch, "You fail to dig up dirt on %s.\n\r", NAME(victim) );
        learn_from_failure(ch, gsn_researchtarget);
        return;
    }

    if ( victim->subclass == SUBCLASS_STEALTH_HUNT || victim->subclass == SUBCLASS_SNEAK )
    {
        ch_printf(ch, "It is impossible to dig up dirt on %s.\n\r", NAME(victim));
        learn_from_success(ch, gsn_researchtarget);
        return;
    }


    if ( IS_IMMORTAL(victim) && (victim->top_level > ch->top_level ) )
    {
       af.type      = gsn_researchtarget;
       af.location  = APPLY_HITROLL;
       af.modifier  = -666;
       af.duration  = 32000;
       af.bitvector = AFF_BLIND;
       affect_to_char( ch, &af );
       set_char_color( AT_MAGIC, victim );
       send_to_char( "You are blinded by your target's immortal aura!\n\r", ch );
       return;
    }
    ch_printf(ch, "&wYour research has concluded the following information:\n\r");
    ch_printf(ch, "&wTitle: %.74s&w\n\r", victim->pcdata->title);
    ch_printf(ch, "&wRace: %.20s Class: %.20s Subclass: %.20s\n\r", npc_race[victim->race], class_name[victim->main_ability], subclasses[victim->subclass]);
    if(victim->pcdata->clan)
        ch_printf(ch, "Affiliation: %.20s\n\r", victim->pcdata->clan_name);
    ch_printf(ch, "Hitpoints: %5.5d/%5.5d Movement: %5.5d/%5.5d\n\r", victim->hit, victim->max_hit, victim->move, victim->max_move);
    if( IS_EVIL( victim ) && is_affected( victim, gsn_maskaura ) )
        ch_printf(ch, "Armor: %4.4d Hitroll: %5.5d Damroll: %5.5d Alignment: masked", GET_AC(victim), GET_HITROLL(victim),
            GET_DAMROLL(victim));
    else
        ch_printf(ch, "Armor: %4.4d Hitroll: %5.5d Damroll: %5.5d Alignment: %4.4d", GET_AC(victim), GET_HITROLL(victim), 
            GET_DAMROLL(victim), victim->alignment);
    learn_from_success(ch, gsn_researchtarget);
    return;
}

void do_negotiate( CHAR_DATA *ch, const char *argument ) /* Made by Charles July '02 */
{
    CHAR_DATA *victim;
    AFFECT_DATA *paf;
    SKILLTYPE *skill;
    int credits;

    if ( ( victim = who_fighting( ch ) ) == NULL )
    {
        send_to_char( "&c&CYou aren't fighting anyone.\n\r", ch );
        return;
    }
    if ( IS_AFFECTED(victim, AFF_BERSERK) )
    {
       send_to_char( "&c&CYou cannot persuade them, they are in a berserking rage", ch );
       return;
    }
    if ( !IS_NPC( victim ) )
    {
       credits = victim->skill_level[COMBAT_ABILITY] * 1500;  //1.5k per combat level.//
    }
    if ( IS_NPC( victim ) )
    {
       credits = 10000; //mobs u pay 10k//
    }
    if ( ch->gold < credits )
    {
       send_to_char( "&c&CYou think smiling will make him stop fighting?", ch );
       return;
    }
    for (paf = victim->first_affect; paf; paf = paf->next)
    {
       if ( (skill=get_skilltype(paf->type)) != NULL )
       {
         if ( !str_cmp( skill->name, "rage" ) )
         {
           send_to_char( "&c&CThey are In a rage of fury! They won't bend to your negotiations\r\n", ch );
           return;
         }
       }
    }
    WAIT_STATE( ch, skill_table[gsn_negotiate]->beats );
    if ( IS_NPC(ch) || number_percent( ) < ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_negotiate] ) )
    {
        stop_fighting( victim, TRUE );
        act( AT_LBLUE, "You negotiate with $N, with credits.",  ch, NULL, victim, TO_CHAR    );
        act( AT_LBLUE, "$n negotiates with you, with credits.", ch, NULL, victim, TO_VICT    );
        act( AT_LBLUE, "$n negotiates with $N.",  ch, NULL, victim, TO_NOTVICT );
        ch->gold -= credits;
        victim->gold += credits;
        learn_from_success( ch, gsn_negotiate );
        stop_hating( victim );
        stop_hunting( victim );
        stop_fearing( victim );
    }
    else
    {
        act( AT_LBLUE, "You failed the negotiations with $N.",  ch, NULL, victim, TO_CHAR    );
        act( AT_LBLUE, "$n negotiate with you but does it poorly don't you think?", ch, NULL, victim, TO_VICT    );
        act( AT_LBLUE, "$n fails the negotiations with $N.",  ch, NULL, victim, TO_NOTVICT );
        learn_from_failure( ch, gsn_negotiate );
    }
    return;
}

void do_embed_comlink( CHAR_DATA * ch, const char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    char arg2[MAX_INPUT_LENGTH];
    int level, chance;
    bool checksew, checkcom, checkarm;
    OBJ_DATA *obj;
    OBJ_DATA *material;
    OBJ_DATA *comlink;

    argument = one_argument( argument, arg );
    strcpy( arg2 , argument );

    switch( ch->substate )
    {
        default:

                if ( arg2[0] == '\0' )
                {
                  send_to_char( "&RUsage: embed_comlink <obj> <obj>\n\r&w", ch);
                  return;
                }

                checksew = FALSE;
                checkarm = FALSE;
                checkcom = FALSE;

                if ( !IS_SET( ch->in_room->room_flags, ROOM_FACTORY ) )
                {
                   send_to_char( "&RYou need to be in a factory or workshop to do that.\n\r", ch);
                   return;
                }
                if ( ( comlink = get_obj_carry( ch, arg ) ) == NULL )
                {
                     send_to_char("&RYou do not have that item.&w\n\r", ch );
                     return;
                }
                if ( ( material = get_obj_carry( ch, arg2 ) ) == NULL )
                {
                     send_to_char("&RYou do not have that item.&w\n\r", ch );
                     return;
                }
                if(comlink->item_type == ITEM_COMLINK || material->item_type == ITEM_COMLINK )
                   checkcom = TRUE;
                if(comlink->item_type == ITEM_ARMOR || material->item_type == ITEM_ARMOR )
                   checkarm = TRUE;
  
                for ( obj = ch->last_carrying; obj; obj = obj->prev_content )
                {
                  if (obj->item_type == ITEM_THREAD)
                    checksew = TRUE;
                }

                if ( !checkcom && checkarm)
                {
                   send_to_char( "&RBoth a comlink and armor are required.\n\r", ch);
                   return;
                }
                if( !checkarm && checkcom )
                {
                    send_to_char( "&RBoth a comlink and armor are required.\n\r", ch);
                    return;
                }
                if( !checkcom )
                {
                    send_to_char( "&ROne of the objects must be a comlink.\n\r", ch);
                    return;
                }
                if( !checkarm )
                {
                    send_to_char( "&ROne of the objects must be a piece of armor.\n\r", ch);
                    return;
                }
                if ( !checksew )
                {
                   send_to_char( "&RYou need a needle and some thread.\n\r", ch);
                   return;
                }

                chance = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_embed_comlink] );
                if ( number_percent( ) < chance )
                {
                   send_to_char( "&GYou begin the long process of embedding a comlink.\n\r", ch);
                   act( AT_PLAIN, "$n takes $s a sewing kit and begins to work.", ch,
                        NULL, argument , TO_ROOM );
                   if(IS_IMMORTAL(ch))
                   add_timer(ch, TIMER_DO_FUN, 1, do_embed_comlink, 1);
                   else if (ch->subclass == SUBCLASS_QUICKWORK )
                   add_timer ( ch , TIMER_DO_FUN , 2 , do_embed_comlink , 1 );
                        else
                   add_timer ( ch , TIMER_DO_FUN , 5 , do_embed_comlink , 1 );
                   ch->dest_buf = str_dup(arg);
                   ch->dest_buf_2 = str_dup(arg2);
                   return;
                }
                send_to_char("&RYou can't figure out what to do.\n\r",ch);
                return;

        case 1:
                if ( !ch->dest_buf )
                     return;
                if ( !ch->dest_buf_2 )
                     return;
                strlcpy( arg, (const char *)ch->dest_buf, MAX_INPUT_LENGTH );
                DISPOSE( ch->dest_buf);
                strlcpy( arg2, (const char *)ch->dest_buf_2, MAX_INPUT_LENGTH );
                DISPOSE( ch->dest_buf_2);
                break;

        case SUB_TIMER_DO_ABORT:
                DISPOSE( ch->dest_buf );
                DISPOSE( ch->dest_buf_2 );
                ch->substate = SUB_NONE;
                send_to_char("&RYou are interupted and fail to finish your work.\n\r", ch);
                return;
    }

    ch->substate = SUB_NONE;

    level = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_embed_comlink] );

    checksew = FALSE;
    checkarm = FALSE;
    checkcom = FALSE;

    comlink = get_obj_carry( ch, arg );
    material = get_obj_carry( ch, arg2 );
    if(comlink != NULL && material != NULL)
    {
        if(comlink->item_type == ITEM_COMLINK)
            checkcom = TRUE;
        if(material->item_type == ITEM_ARMOR )
            checkarm = TRUE;
        /* See if they are reversed. If they are don't bother checking again. */
        if(material->item_type == ITEM_COMLINK && !checkcom )
        {
           checkcom = TRUE;
           /* Do an object swap. */
           obj = material;
           material = comlink;
           comlink = obj; 
           if(material->item_type == ITEM_ARMOR )
              checkarm = TRUE;
        }   
    }   
    for ( obj = ch->last_carrying; obj; obj = obj->prev_content )
    {
       if (obj->item_type == ITEM_THREAD)
          checksew = TRUE;
    }

    chance = level;

    if( checkcom )
    {
       separate_obj( comlink );
       obj_from_char( comlink );
    }
    if( checkarm )
    { 
       separate_obj( material );
       obj_from_char( material );
    }
    if ( number_percent( ) > chance  || ( !checkarm ) || ( !checksew ) || ( !checkcom )  )
    {
       send_to_char( "&RYou double check over your work.\n\r", ch);
       send_to_char( "&RThe seams weren't perfect, and the comlink wiring is useless.\n\r", ch);
       send_to_char( "&RThe comlink shreds drop to the floor and the armor takes a beating.\n\r", ch);
       if ( checkarm )
       {
          material = obj_to_char( material, ch );
          damage_obj( material );
       }
       if(checkcom)
          extract_obj( comlink );
       learn_from_failure( ch, gsn_embed_comlink );
       return;
    }
    obj = material;
    obj->item_type = ITEM_ARMOR;
    obj = obj_to_char( obj, ch );
    /* transfer_affects not available */
    comlink = obj_to_obj(comlink, obj);
    obj->cost += comlink->cost;
    send_to_char( "&GYou finish embedding the comlink.&w\n\r", ch);
    act( AT_PLAIN, "$n finishes installing a comlink.", ch,
         NULL, argument , TO_ROOM );
    learn_from_success( ch, gsn_embed_comlink );
}

void do_repairitem( CHAR_DATA * ch, const char *argument )
{
   int chance;
   bool checktool, checkoven, checkneedle;
   OBJ_DATA *obj;
   checktool = FALSE;
   checkoven = FALSE;
   checkneedle = FALSE;

   if(IS_NPC(ch))
   {
      send_to_char("And just what do you think your doing?.\n\r", ch );
      return;
   }

   switch(ch->substate)
   {
   default:
   if (!IS_SET(ch->in_room->room_flags, ROOM_FACTORY))
   {
      send_to_char("&RYou need to be in a factory or workshop to do that.&w\n\r", ch );
      return;
   }
   for ( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if (obj->item_type == ITEM_TOOLKIT)
         checktool = TRUE;
      if (obj->item_type == ITEM_OVEN)
         checkoven = TRUE;
      if (obj->item_type == ITEM_THREAD)
         checkneedle = TRUE;
   }
   if (!checktool)
   {
      send_to_char("&RYou need a toolkit to bend items back into shape.&w\n\r", ch );
      return;
   }
   if (!checkoven)
   {
      send_to_char("&RYou might need an oven to remelt the item.&w\n\r", ch );
      return;
   }
   if (!checkneedle)
   {
      send_to_char("&RHow do you expect to put it back together without thread?&w\n\r", ch );
      return;
   }
   if ( ( obj = get_obj_carry( ch, argument ) ) == NULL )
   {
      send_to_char("&RYou do not have that item.&w\n\r", ch );
      return;
   }
   else
   {
      if(!(obj->item_type == ITEM_WEAPON ||
           obj->item_type == ITEM_ARMOR || obj->item_type == ITEM_ARMOR))
      {
         send_to_char("&RYou only know how to repair weapons and armor.&w\n\r", ch );
         return;
      }
   }
   chance = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_repairitem] );
   if (number_percent() < chance)
   {
      send_to_char( "&GYou begin the long process repairing an item.\n\r", ch);
      act( AT_PLAIN, "$n takes $s tools, a small oven, and some thread and begins to repair something.", ch, NULL, argument , TO_ROOM );
      if(IS_IMMORTAL(ch))
       add_timer(ch , TIMER_DO_FUN, 1, do_repairitem, 1);
	else   if (ch->subclass == SUBCLASS_QUICKWORK )
      add_timer ( ch , TIMER_DO_FUN , 3 , do_repairitem , 1 );
	else
      add_timer ( ch , TIMER_DO_FUN , 12 , do_repairitem , 1 );
                   ch->dest_buf   = str_dup(argument);
                   return;
   }
   else
   {
      send_to_char( "&RYou can't figure out the problem.&w\n\r", ch );
      return;
   }
   case 1:
   checktool = FALSE;
   checkoven = FALSE;
   checkneedle = FALSE;
   for ( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if(obj->item_type == ITEM_TOOLKIT)
         checktool = TRUE;
      if(obj->item_type == ITEM_OVEN)
         checkoven = TRUE;
      if(obj->item_type == ITEM_THREAD)
         checkneedle = TRUE;
   }
   chance = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_repairitem] );
   chance = UMIN(chance, 95);
   if ( ( obj = get_obj_carry( ch, (const char *)ch->dest_buf ) ) == NULL )
   {
      send_to_char("&RYou do not have that item.&w\n\r", ch );
      DISPOSE(ch->dest_buf);
      return;
   }
   if ( number_percent( ) > chance*2  || ( !checktool ) || ( !checkoven )  || ( !checkneedle ) )
   {
       send_to_char( "&RYou finish putting the item back together\n\r", ch);
       send_to_char( "&RYou reexamine the schematics you wrote.\n\r", ch);
       send_to_char( "&RAnd find several errors damaging the item.\n\r", ch);
       damage_obj(obj);
       learn_from_failure( ch, gsn_repairitem);
       return;
    }
    send_to_char("You successfully put the item back together.\n\r", ch);
    switch ( obj->item_type )
    {
      default:
        send_to_char( "???\n\r", ch);
        break;
      case ITEM_ARMOR:
        obj->value[0] = obj->value[1];
        break;
      case ITEM_WEAPON:
        obj->value[0] = INIT_WEAPON_CONDITION;
        break;
    }
    learn_from_success(ch, gsn_repairitem);
    oprog_repair_trigger(ch, obj);
    DISPOSE(ch->dest_buf);
    return;

    case SUB_TIMER_DO_ABORT:
        DISPOSE( ch->dest_buf );
        ch->substate = SUB_NONE;
        send_to_char("&RYou are interupted and fail to finish your work.\n\r", ch);
        return;
    }
}

void do_salvage( CHAR_DATA * ch, const char *argument )
{
   int chance;
   bool checktool;
   OBJ_DATA *obj;
   OBJ_DATA *salvage;
   OBJ_DATA *next_obj;


   checktool = FALSE;

   if(IS_NPC(ch))
   {
      send_to_char("And just what do you think your doing?.\n\r", ch );
      return;
   }

   switch(ch->substate)
   {
   default:
   if (!IS_SET(ch->in_room->room_flags, ROOM_FACTORY))
   {
      send_to_char("&RYou need to be in a factory or workshop to do that.&w\n\r", ch );
      return;
   }
   for ( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if (obj->item_type == ITEM_TOOLKIT)
         checktool = TRUE;
   }
   if (!checktool)
   {
      send_to_char("&RYou'll need a well stocked tool kit to take anything apart.&w\n\r", ch );
      return;
   }

   if ( ( obj = get_obj_carry( ch, argument ) ) == NULL )
   {
      send_to_char("&RYou do not have that item.&w\n\r", ch );
      return;
   }
   else
   {
      if( obj->item_type == ITEM_CONTAINER || obj->item_type == ITEM_HOLSTER 
       || obj->item_type == ITEM_DRINK_CON || obj->item_type == ITEM_SPEED_LOADER 
)
      {
         send_to_char("&RTaking that apart would serve no purpose, it has no salvageable parts.&w\n\r", ch );
         return;
      }
      if( !obj->first_content )
      {
         send_to_char("&RTaking that apart would serve no purpose, it has no salvageable parts.&w\n\r", ch );
         return;
      }
   }
   chance = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_salvage] );

   if (number_percent() < chance)
   {
      int obj_weight;
      obj_weight = get_obj_weight( obj );
      (void)obj_weight;

      act( AT_PLAIN, "You begin to carefully deconstruct $p.", ch, obj, NULL, TO_CHAR );
      act( AT_PLAIN, "$n begins to carefully deconstruct $p.", ch, obj, NULL , TO_ROOM );
      if(IS_IMMORTAL(ch))
         add_timer(ch , TIMER_DO_FUN, 1, do_salvage, 1);
      else if (ch->subclass == SUBCLASS_QUICKWORK )
         add_timer ( ch , TIMER_DO_FUN , obj->weight/4 , do_salvage , 1 );
      else
         add_timer ( ch , TIMER_DO_FUN , obj->weight , do_salvage , 1 );
      ch->dest_buf   = str_dup(argument);
      return;
   }
   else
   {
      send_to_char( "&RYou can't figure out where to begin, but you could always smash it open.&w\n\r", ch );
      return;
   }
   case 1:
       checktool = FALSE;
       
       for ( obj = ch->last_carrying; obj; obj = obj->prev_content )
       {
          if(obj->item_type == ITEM_TOOLKIT)
             checktool = TRUE;
       }
       
       if( !checktool ) 
       {
           send_to_char( "You seem to have misplaced your tool kit.", ch );
           DISPOSE( ch->dest_buf );
           return;
       }
       
       if ( ( obj = get_obj_carry( ch, (const char *)ch->dest_buf ) ) == NULL )
       {
           send_to_char("&RYou do not have that item.&w\n\r", ch );
           DISPOSE(ch->dest_buf);
           return;
       }
       
       act( AT_PLAIN, "You finish taking $p apart and check for undamaged components.", ch, obj, NULL, TO_CHAR );
       act( AT_PLAIN, "$n finishes taking $p apart and checks for undamaged components.", ch, obj, NULL, TO_ROOM );
       for( salvage = obj->first_content; salvage != NULL; salvage = next_obj )
       {
           next_obj = salvage->next_content; 
           chance = ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_salvage] );
           chance = UMIN( chance, 85 );
           
           if( number_percent() > chance )
           {
               obj_from_obj( salvage );
               extract_obj( salvage );
               learn_from_failure( ch, gsn_salvage );
           }
           else
           {
               obj_from_obj( salvage );
               obj_to_char( salvage, ch );
               act( AT_PLAIN, "You carefully recover $p from $P.", ch, salvage, obj, TO_CHAR );
               act( AT_PLAIN, "$n carefully recovers $p from $P.", ch, salvage, obj, TO_ROOM );
               learn_from_success( ch, gsn_salvage );
           }
       }
       
       extract_obj( obj );
       DISPOSE(ch->dest_buf);
       return;
       
   case SUB_TIMER_DO_ABORT:
        DISPOSE( ch->dest_buf );
        ch->substate = SUB_NONE;
        send_to_char("&RYou are interupted and fail to finish your work.\n\r", ch);
        return;
    }
}

void do_revive( CHAR_DATA *ch, const char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    CHAR_DATA *victim;
    int percent;
    OBJ_DATA *defibulator;

    if ( IS_NPC(ch) && IS_AFFECTED( ch, AFF_CHARM ) )
    {
        send_to_char( "You can't concentrate enough for that.\n\r", ch );
        return;
    }
    if ( ch->mount )
    {
        send_to_char( "You can't do that while mounted.\n\r", ch );
        return;
    }

    one_argument( argument, arg );
    if ( arg[0] == '\0' )
    {
        send_to_char( "Revive whom?\n\r", ch );
        return;
    }

    if ( ( victim = get_char_room( ch, arg ) ) == NULL )
    {
        send_to_char( "They aren't here.\n\r", ch );
        return;
    }

    if ( victim == ch )
    {
        send_to_char( "Wouldn't it be nice if you could pull that one off?\n\r", ch );
        return;
    }

    if(ch->position == POS_FIGHTING && ch->subclass != SUBCLASS_PARAMEDIC)
    {
       send_to_char("Hard enough to get close, much less help.\n\r", ch);
       return;
    }

    if ( victim->position > POS_MORTAL )
    {
        act( AT_PLAIN, "$N doesn't need to be revived.", ch, NULL, victim,
             TO_CHAR);
        return;
    }

   defibulator = get_eq_char( ch, WEAR_HOLD );
   if ( !defibulator || defibulator->item_type != ITEM_INOCULATOR)
   {
         send_to_char( "You need to be holding a defibulator.\n\r",ch );
         return;
   }

   if ( defibulator->value[0] <= 0 )
   {
         send_to_char( "Your defibulator lacks charge.\n\r",ch );
         return;
   }

   defibulator->value[0]--;
   
    WAIT_STATE( ch, PULSE_PER_SECOND );
    act( AT_SKILL, "You charge up $p and place it on $N's chest.", ch, defibulator, victim, TO_CHAR );
    act( AT_SKILL, "$n charges up $p and places it on $N's chest.", ch, defibulator, victim, TO_NOTVICT );

    percent = number_percent();

    WAIT_STATE( ch, PULSE_PER_SECOND );
    if ( !IS_NPC(ch) && percent > (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_revive] ) / 2) )
    {
        act( AT_SKILL, "$N's body tenses up, but fails to revive from the shock of $p.", ch, defibulator, victim, TO_CHAR );
        act( AT_SKILL, "$N's body tenses up, but fails to revive from the shock of $p.", ch, defibulator, victim, TO_NOTVICT );
        learn_from_failure( ch, gsn_revive );
        return;
    }

    victim->hit = 1;
    affect_strip( victim, gsn_stun );
    affect_strip( victim, gsn_sleep );
    update_pos( victim );

    act( AT_SKILL, "$N's body tenses up, and revives from the shock of $p.",  ch, defibulator, victim, TO_CHAR    );
    act( AT_SKILL, "$N's body tenses up, and revives from the shock of $p.",  ch, defibulator, victim, TO_NOTVICT );
    act( AT_SKILL, "You awaken to a sharp pain in your chest.", ch, defibulator, victim, TO_VICT );

    WAIT_STATE( victim, PULSE_VIOLENCE );

    learn_from_success( ch, gsn_revive );
    return;
}

void do_dieroll( CHAR_DATA *ch, const char *argument )
{
   int die, dieroll;
   char buf[MAX_STRING_LENGTH];

   if( argument[0] == '\0')
   {
      send_to_char("&RSyntax: dieroll <number>&w\n\r", ch);
      return;
   }
   die = atoi(argument);
   if( die <= 3 || die > 500 )
   {
      send_to_char("&RDie must have 4 to 500 sides.&w\n\r", ch);
      return;
   }
   dieroll = number_range(1, die);
   sprintf(buf, "&cYou &w&zroll a &c%d &w&zsided die and it lands on &C%d&w&z.", die, dieroll);
   act( AT_PLAIN, buf, ch, NULL, NULL, TO_CHAR );
   sprintf(buf, "&c$n &w&zrolls a &c%d &w&zsided die and it lands on &C%d&w&z.", die, dieroll);
   act( AT_PLAIN, buf, ch, NULL, NULL, TO_ROOM );
   return;
}

void do_peek( CHAR_DATA *ch, const char *argument )
{
   CHAR_DATA *victim;
   OBJ_DATA *obj;
   char arg[MAX_INPUT_LENGTH];
   bool found;
   int iWear, chance;

   one_argument( argument, arg );

   if( IS_NPC( ch ) ) return;
   if( !check_blind( ch ) ) return;

   if( ( victim = get_char_room( ch, arg ) ) == NULL )
   { send_to_char( "They aren't here.\r\n", ch ); return; }

   chance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_peek] );

   if( number_percent() > chance )
   {
      act( AT_ACTION, "$n glances at you.", ch, NULL, victim, TO_VICT );
      act( AT_ACTION, "$n glances at $N.", ch, NULL, victim, TO_NOTVICT );
   }

   ch_printf( ch, "You peek at %s.\r\n", NAME( victim ) );

   if( victim->description[0] != '\0' )
      send_to_char( victim->description, ch );
   else
      act( AT_PLAIN, "You see nothing special about $M.", ch, NULL, victim, TO_CHAR );

   show_condition( ch, victim );

   found = FALSE;
   for( iWear = 0; iWear < MAX_WEAR; iWear++ )
   {
      if( ( obj = get_eq_char( victim, iWear ) ) != NULL && can_see_obj( ch, obj ) )
      {
         if( !found )
         {
            send_to_char( "\r\n", ch );
            act( AT_PLAIN, "$N is using:", ch, NULL, victim, TO_CHAR );
            found = TRUE;
         }
         send_to_char( "&g", ch );
         send_to_char( where_name[iWear], ch );
         send_to_char( format_obj_to_char( obj, ch, TRUE ), ch );
         send_to_char( "\r\n", ch );
      }
   }

   if( IS_NPC( ch ) || victim == ch ) return;

   if( number_percent() < chance )
   {
      send_to_char( "\r\nYou peek at the inventory:\r\n", ch );
      show_list_to_char( victim->first_carrying, ch, TRUE, TRUE );
      learn_from_success( ch, gsn_peek );
   }
   else learn_from_failure( ch, gsn_peek );
}

void do_elite_patrol( CHAR_DATA *ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int schance, credits;

   if( IS_NPC( ch ) || !ch->pcdata )
      return;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( ch->backup_wait )
         {
            send_to_char( "&RYou already have an elite patrol coming.\r\n", ch );
            return;
         }

         if( !ch->pcdata->clan )
         {
            send_to_char( "&RYou need to be a member of an organization before you can call for an elite patrol.\r\n", ch );
            return;
         }

         if( ch->gold < ch->skill_level[LEADERSHIP_ABILITY] * 50 )
         {
            ch_printf( ch, "&RYou dont have enough credits.\r\n" );
            return;
         }

         schance = ( int )( ch->pcdata->learned[gsn_elitepatrol] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin making the call for an elite patrol.\r\n", ch );
            act( AT_PLAIN, "$n begins issuing orders int $s comlink.", ch, NULL, argument, TO_ROOM );
            add_timer( ch, TIMER_DO_FUN, 1, do_elite_patrol, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou call for an elite patrol but nobody answers.\r\n", ch );
         learn_from_failure( ch, gsn_elitepatrol );
         return;

      case 1:
         if( !ch->dest_buf )
            return;
         strlcpy( arg, ( const char* ) ch->dest_buf, MAX_INPUT_LENGTH );
         DISPOSE( ch->dest_buf );
         break;

      case SUB_TIMER_DO_ABORT:
         DISPOSE( ch->dest_buf );
         ch->substate = SUB_NONE;
         send_to_char( "&RYou are interupted before you can finish your call.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   send_to_char( "&GYour elite patrol is on the way.\r\n", ch );

   credits = ch->skill_level[LEADERSHIP_ABILITY] * 50;
   ch_printf( ch, "It cost you %d credits.\r\n", credits );
   ch->gold -= UMIN( credits, ch->gold );

   learn_from_success( ch, gsn_elitepatrol );

   if( nifty_is_name( "empire", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_IMP_PATROL;
   else if( nifty_is_name( "republic", ch->pcdata->clan->name ) )
      ch->backup_mob = MOB_VNUM_NR_PATROL;
   else
      ch->backup_mob = MOB_VNUM_MERC_PATROL;

   ch->backup_type = 3;
   ch->backup_elite = TRUE;
   ch->backup_wait = 1;
}
