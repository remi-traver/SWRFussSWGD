/***************************************************************************
*                           STAR WARS REALITY 1.0                          *
*--------------------------------------------------------------------------*
* Star Wars Reality Code Additions and changes from the Smaug Code         *
* copyright (c) 1997 by Sean Cooper                                        *
* -------------------------------------------------------------------------*
* Starwars and Starwars Names copyright(c) Lucas Film Ltd.                 *
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
*		                Space Module    			   *   
****************************************************************************/

#include <math.h>
#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mud.h"

SHIP_DATA *first_ship;
SHIP_DATA *last_ship;

MISSILE_DATA *first_missile;
MISSILE_DATA *last_missile;

SPACE_DATA *first_starsystem;
CARGO_DATA *cargo_table[MAX_CARGO];
int top_cargo_sn;


/*
 * Ship flag name lookup - v1.20, ported from SWGD. This codebase's
 * SHIPFLAG_* bit ordering (mud.h) matches SWGD's ship_flags[] order
 * exactly (confirmed bit-for-bit before porting this), so no reordering
 * was needed - just the missing array and lookup itself. r27-r31 are
 * genuinely unused bits in both codebases, kept as placeholders so the
 * array stays index-aligned with the real bit positions.
 */
const char *const ship_flags[32] = {
   "cloaked", "overdrive", "afterburner", "sablasers", "sabions",
   "sabengine", "sabturret1", "sabturret2", "sablaunchers",
   "shieldrlaser", "shieldrengine", "enginerlaser", "enginershield",
   "laserrengine", "laserrshield", "duallaser", "trilaser",
   "quadlaser", "dualion", "triion", "quadion", "simulator",
   "dualmissile", "dualtorpedo", "dualrocket", "sabrlaunchers",
   "sabtlaunchers", "r27", "r28", "r29", "r30", "r31"
};

int get_shipflag( const char *flag )
{
   int x;

   for( x = 0; x < 32; x++ )
      if( !str_cmp( flag, ship_flags[x] ) )
         return x;
   return -1;
}

short shipskill args( ( SHIP_DATA *ship ) );

/* Remove Extra Bits Like Linked Lasers on launch, endsimulator, blow up, etc.
 * Not Sabotage, Or Simulator Though - ported from SWGD (Arcturus) v1.22 */
void rem_extshipflags( SHIP_DATA *ship )
{
   int x;
   int vector;

   for( x = 0; x < 32; x++ )
   {
      vector = ( 1 << x );
      switch( vector )
      {
         /* Place immune flags here. */
         case SHIPFLAG_SABOTAGEDLASERS:
         case SHIPFLAG_SABOTAGEDIONS:
         case SHIPFLAG_SABOTAGEDENGINE:
         case SHIPFLAG_SABOTAGEDTURRET1:
         case SHIPFLAG_SABOTAGEDTURRET2:
         case SHIPFLAG_SABOTAGEDLAUNCHERS:
         case SHIPFLAG_SIMULATOR:
         case SHIPFLAG_SABOTAGEDRLAUNCHERS:
         case SHIPFLAG_SABOTAGEDTLAUNCHERS:
            break;
         default:
            /* REMOVE_BIT makes sure there isn't a bit there regardless
             * of whether it was set or not. */
            REMOVE_BIT( ship->flags, vector );
            break;
      }
   }
}
SPACE_DATA *last_starsystem;

int bus_pos = 0;
int bus_planet = 0;
int bus2_planet = 4;
int turbocar_stop = 0;
int corus_shuttle = 0;

#define MAX_STATION    10
#define MAX_BUS_STOP 10

#define STOP_PLANET     202
#define STOP_SHIPYARD   32015

#define SENATEPAD       10196
#define OUTERPAD        10195

int const station_vnum[MAX_STATION] = {
   215, 216, 217, 218, 219, 220, 221, 222, 223, 224
};

const char *const station_name[MAX_STATION] = {
   "Menari Spaceport", "Skydome Botanical Gardens", "Grand Towers",
   "Grandis Mon Theater", "Palace Station", "Great Galactic Museum",
   "College Station", "Holographic Zoo of Extinct Animals",
   "Dometown Station ", "Monument Plaza"
};

int const bus_vnum[MAX_BUS_STOP] = {
   201, 21100, 29001, 28038, 31872, 1001, 28613, 3060, 28247, 32297
};

const char *const bus_stop[MAX_BUS_STOP + 1] = {
   "Coruscant",
   "Mon Calamari", "Adari", "Gamorr", "Tatooine", "Honoghr",   /* "Ryloth", */
   "Kashyyyk", "Endor", "Byss", "Cloning Facilities", "Coruscant" /* last should always be same as first */
};

/* local routines */
void fread_ship args( ( SHIP_DATA * ship, FILE * fp ) );
bool load_ship_file( const char *shipfile );
void write_ship_list args( ( void ) );
void fread_starsystem args( ( SPACE_DATA * starsystem, FILE * fp ) );
bool load_starsystem( const char *starsystemfile );
void write_starsystem_list args( ( void ) );
void resetship args( ( SHIP_DATA * ship ) );
void landship( SHIP_DATA * ship, const char *arg );
void launchship args( ( SHIP_DATA * ship ) );
bool land_bus args( ( SHIP_DATA * ship, int destination ) );
void launch_bus args( ( SHIP_DATA * ship ) );
ch_ret drive_ship( CHAR_DATA * ch, SHIP_DATA * ship, EXIT_DATA * pexit, int fall );
bool autofly( SHIP_DATA * ship );
bool is_facing( SHIP_DATA * ship, SHIP_DATA * target );
void sound_to_ship( SHIP_DATA * ship, char *argument );

/* from comm.c */
bool write_to_descriptor( int desc, char *txt, int length );

void echo_to_room_dnr( int ecolor, ROOM_INDEX_DATA * room, const char *argument )
{
   CHAR_DATA *vic;

   if( room == NULL )
      return;

   for( vic = room->first_person; vic; vic = vic->next_in_room )
   {
      set_char_color( ecolor, vic );
      send_to_char( argument, vic );
   }
}

bool land_bus( SHIP_DATA * ship, int destination )
{
   char buf[MAX_STRING_LENGTH];

   if( !ship_to_room( ship, destination ) )
   {
      return FALSE;
   }
   echo_to_ship( AT_YELLOW, ship, "You feel a slight thud as the ship sets down on the ground." );
   ship->location = destination;
   ship->lastdoc = ship->location;
   ship->shipstate = SHIP_DOCKED;
   if( ship->starsystem )
      ship_from_starsystem( ship, ship->starsystem );
   snprintf( buf, MAX_STRING_LENGTH, "%s lands on the platform.", ship->name );
   echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
   snprintf( buf, MAX_STRING_LENGTH, "The hatch on %s opens.", ship->name );
   echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
   echo_to_room( AT_YELLOW, get_room_index( ship->entrance ), "The hatch opens." );
   ship->hatchopen = TRUE;
   sound_to_room( get_room_index( ship->entrance ), "!!SOUND(door)" );
   sound_to_room( get_room_index( ship->location ), "!!SOUND(door)" );
   return TRUE;
}

void launch_bus( SHIP_DATA * ship )
{
   char buf[MAX_STRING_LENGTH];

   sound_to_room( get_room_index( ship->entrance ), "!!SOUND(door)" );
   sound_to_room( get_room_index( ship->location ), "!!SOUND(door)" );
   snprintf( buf, MAX_STRING_LENGTH, "The hatch on %s closes and it begins to launch.", ship->name );
   echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
   echo_to_room( AT_YELLOW, get_room_index( ship->entrance ), "The hatch slides shut." );
   ship->hatchopen = FALSE;
   extract_ship( ship );
   echo_to_ship( AT_YELLOW, ship, "The ship begins to launch." );
   ship->location = 0;
   ship->shipstate = SHIP_READY;
}

void update_traffic(  )
{
   SHIP_DATA *shuttle, *senate;
   SHIP_DATA *turbocar;
   char buf[MAX_STRING_LENGTH];

   shuttle = ship_from_cockpit( ROOM_CORUSCANT_SHUTTLE );
   senate = ship_from_cockpit( ROOM_SENATE_SHUTTLE );
   if( senate != NULL && shuttle != NULL )
   {
      switch ( corus_shuttle )
      {
         default:
            corus_shuttle++;
            break;

         case 0:
            land_bus( shuttle, STOP_PLANET );
            land_bus( senate, SENATEPAD );
            corus_shuttle++;
            echo_to_ship( AT_CYAN, shuttle, "Welcome to Menari Spaceport." );
            echo_to_ship( AT_CYAN, senate, "Welcome to The Senate Halls." );
            break;

         case 4:
            launch_bus( shuttle );
            launch_bus( senate );
            corus_shuttle++;
            break;

         case 5:
            land_bus( shuttle, STOP_SHIPYARD );
            land_bus( senate, OUTERPAD );
            echo_to_ship( AT_CYAN, shuttle, "Welcome to Coruscant Shipyard." );
            echo_to_ship( AT_CYAN, senate, "Welcome to The Outer System Landing Area." );
            corus_shuttle++;
            break;

         case 9:
            launch_bus( shuttle );
            launch_bus( senate );
            corus_shuttle++;
            break;

      }

      if( corus_shuttle >= 10 )
         corus_shuttle = 0;
   }

   turbocar = ship_from_cockpit( ROOM_CORUSCANT_TURBOCAR );
   if( turbocar != NULL )
   {
      echo_to_room( AT_YELLOW, get_room_index( turbocar->location ), "The turbocar doors close and it speeds out of the station." );
      extract_ship( turbocar );
      turbocar->location = 0;
      ship_to_room( turbocar, station_vnum[turbocar_stop] );
      echo_to_ship( AT_YELLOW, turbocar, "The turbocar makes a quick journey to the next station." );
      turbocar->location = station_vnum[turbocar_stop];
      turbocar->lastdoc = turbocar->location;
      turbocar->shipstate = SHIP_DOCKED;
      if( turbocar->starsystem )
         ship_from_starsystem( turbocar, turbocar->starsystem );

      echo_to_room( AT_YELLOW, get_room_index( turbocar->location ), "A turbocar pulls into the platform and the doors slide open." );
      snprintf( buf, MAX_STRING_LENGTH, "Welcome to %s.", station_name[turbocar_stop] );
      echo_to_ship( AT_CYAN, turbocar, buf );
      turbocar->hatchopen = TRUE;

      turbocar_stop++;
      if( turbocar_stop >= MAX_STATION )
         turbocar_stop = 0;
   }
}

void update_bus(  )
{
   SHIP_DATA *ship;
   SHIP_DATA *ship2;
   SHIP_DATA *target;
   int destination;
   char buf[MAX_STRING_LENGTH];

   ship = ship_from_cockpit( ROOM_SHUTTLE_BUS );
   ship2 = ship_from_cockpit( ROOM_SHUTTLE_BUS_2 );

   if( ship == NULL && ship2 == NULL )
      return;

   switch ( bus_pos )
   {
      case 0:
         target = ship_from_hanger( bus_vnum[bus_planet] );
         if( target != NULL && !target->starsystem )
         {
            snprintf( buf, MAX_STRING_LENGTH, "An electronic voice says, 'Cannot land at %s ... it seems to have dissapeared.'",
                     bus_stop[bus_planet] );
            echo_to_ship( AT_CYAN, ship, buf );
            bus_pos = 5;
         }

         target = ship_from_hanger( bus_vnum[bus2_planet] );
         if( target != NULL && !target->starsystem )
         {
            snprintf( buf, MAX_STRING_LENGTH, "An electronic voice says, 'Cannot land at %s ... it seems to have dissapeared.'",
                     bus_stop[bus_planet] );
            echo_to_ship( AT_CYAN, ship2, buf );
            bus_pos = 5;
         }

         bus_pos++;
         break;

      case 6:
         launch_bus( ship );
         launch_bus( ship2 );
         bus_pos++;
         break;

      case 7:
         echo_to_ship( AT_YELLOW, ship, "The ship lurches slightly as it makes the jump to lightspeed." );
         echo_to_ship( AT_YELLOW, ship2, "The ship lurches slightly as it makes the jump to lightspeed." );
         bus_pos++;
         break;

      case 9:

         echo_to_ship( AT_YELLOW, ship, "The ship lurches slightly as it comes out of hyperspace.." );
         echo_to_ship( AT_YELLOW, ship2, "The ship lurches slightly as it comes out of hyperspace.." );
         bus_pos++;
         break;

      case 1:
         destination = bus_vnum[bus_planet];
         if( !land_bus( ship, destination ) )
         {
            snprintf( buf, MAX_STRING_LENGTH, "An electronic voice says, 'Oh My, %s seems to have dissapeared.'", bus_stop[bus_planet] );
            echo_to_ship( AT_CYAN, ship, buf );
            echo_to_ship( AT_CYAN, ship, "An electronic voice says, 'I do hope it wasn't a superlaser. Landing aborted.'" );
         }
         else
         {
            snprintf( buf, MAX_STRING_LENGTH, "An electronic voice says, 'Welcome to %s'", bus_stop[bus_planet] );
            echo_to_ship( AT_CYAN, ship, buf );
            echo_to_ship( AT_CYAN, ship, "It continues, 'Please exit through the main ramp. Enjoy your stay.'" );
         }
         destination = bus_vnum[bus2_planet];
         if( !land_bus( ship2, destination ) )
         {
            snprintf( buf, MAX_STRING_LENGTH, "An electronic voice says, 'Oh My, %s seems to have dissapeared.'", bus_stop[bus_planet] );
            echo_to_ship( AT_CYAN, ship2, buf );
            echo_to_ship( AT_CYAN, ship2, "An electronic voice says, 'I do hope it wasn't a superlaser. Landing aborted.'" );
         }
         else
         {
            snprintf( buf, MAX_STRING_LENGTH, "An electronic voice says, 'Welcome to %s'", bus_stop[bus2_planet] );
            echo_to_ship( AT_CYAN, ship2, buf );
            echo_to_ship( AT_CYAN, ship2, "It continues, 'Please exit through the main ramp. Enjoy your stay.'" );
         }
         bus_pos++;
         break;

      case 5:
         snprintf( buf, MAX_STRING_LENGTH, "It continues, 'Next stop, %s'", bus_stop[bus_planet + 1] );
         echo_to_ship( AT_CYAN, ship, "An electronic voice says, 'Preparing for launch.'" );
         echo_to_ship( AT_CYAN, ship, buf );
         snprintf( buf, MAX_STRING_LENGTH, "It continues, 'Next stop, %s'", bus_stop[bus2_planet + 1] );
         echo_to_ship( AT_CYAN, ship2, "An electronic voice says, 'Preparing for launch.'" );
         echo_to_ship( AT_CYAN, ship2, buf );
         bus_pos++;
         break;

      default:
         bus_pos++;
         break;
   }

   if( bus_pos >= 10 )
   {
      bus_pos = 0;
      bus_planet++;
      bus2_planet++;
   }

   if( bus_planet >= MAX_BUS_STOP )
      bus_planet = 0;
   if( bus2_planet >= MAX_BUS_STOP )
      bus2_planet = 0;
}

void move_ships()
{
  SHIP_DATA *ship;
  MISSILE_DATA *missile;
  MISSILE_DATA *m_next;
  SHIP_DATA *target;
  float dx, dy, dz, change;
  char buf[MAX_STRING_LENGTH];
  CHAR_DATA *ch;
  bool ch_found = FALSE;

  for( missile = first_missile; missile; missile = m_next )
    {
      m_next = missile->next;

      ship = missile->fired_from;
      target = missile->target;

      if( target->starsystem && target->starsystem == missile->starsystem )
	{
	  if( missile->mx < target->vx )
	    missile->mx += UMIN( missile->speed / 5, ( int )( target->vx - missile->mx ) );
	  else if( missile->mx > target->vx )
            missile->mx -= UMIN( missile->speed / 5, ( int )( missile->mx - target->vx ) );
	  if( missile->my < target->vy )
            missile->my += UMIN( missile->speed / 5, ( int )( target->vy - missile->my ) );
	  else if( missile->my > target->vy )
            missile->my -= UMIN( missile->speed / 5, ( int )( missile->my - target->vy ) );
	  if( missile->mz < target->vz )
            missile->mz += UMIN( missile->speed / 5, ( int )( target->vz - missile->mz ) );
	  else if( missile->mz > target->vz )
            missile->mz -= UMIN( missile->speed / 5, ( int )( missile->mz - target->vz ) );

	  if( abs( ( int ) missile->mx ) - abs( ( int ) target->vx ) <= 20
	      && abs( ( int ) missile->mx ) - abs( ( int ) target->vx ) >= -20
	      && abs( ( int ) missile->my ) - abs( ( int ) target->vy ) <= 20
	      && abs( ( int ) missile->my ) - abs( ( int ) target->vy ) >= -20
	      && abs( ( int ) missile->mz ) - abs( ( int ) target->vz ) <= 20
	      && abs( ( int ) missile->mz ) - abs( ( int ) target->vz ) >= -20)
	    {
	      if( target->chaff_released <= 0 )
		{
		  if( evaded( ship, target ) )
		  {
		    echo_to_room( AT_YELLOW, get_room_index( ship->gunseat ),
				  "Your missile overshoots its target and explodes!" );
		    echo_to_cockpit( AT_YELLOW, target, "You evade a missile." );
		    extract_missile( missile );
		  }
		  else
		  {
		    echo_to_room( AT_YELLOW, get_room_index( ship->gunseat ), "Your missile hits its target dead on!" );
		    echo_to_cockpit( AT_BLOOD, target, "The ship is hit by a missile." );
		    echo_to_ship( AT_RED, target, "A loud explosion shakes thee ship violently!" );
		    snprintf( buf, MAX_STRING_LENGTH, "You see a small explosion as %s is hit by a missile", target->name );
		    echo_to_system( AT_ORANGE, target, buf, ship );
		    for( ch = first_char; ch; ch = ch->next )
		      if( !IS_NPC( ch ) && nifty_is_name( missile->fired_by, ch->name ) )
			{
			  ch_found = TRUE;
			  damage_ship_ch( target, 20 + missile->missiletype * missile->missiletype * 20,
					  30 + missile->missiletype * missile->missiletype * missile->missiletype * 30, ch );
			}
		    if( !ch_found )
		      damage_ship( target, 20 + missile->missiletype * missile->missiletype * 20,
				   30 + missile->missiletype * missile->missiletype * ship->missiletype * 30 );
		    extract_missile( missile );
		  }
		}
	      else
		{
		  echo_to_room( AT_YELLOW, get_room_index( ship->gunseat ),
				"Your missile explodes harmlessly in a cloud of chaff!" );
		  echo_to_cockpit( AT_YELLOW, target, "A missile explodes in your chaff." );
		  extract_missile( missile );
		}
	      continue;
	    }
	  else
	    {
	      missile->age++;
	      if( missile->age >= 50 )
		{
		  extract_missile( missile );
		  continue;
		}
	    }
	}
      else
	{
	  extract_missile( missile );
	  continue;
	}
    }

  for( ship = first_ship; ship; ship = ship->next )
    {
      if( !ship->starsystem )
	continue;

      if( ship->currspeed > 0 )
	{
	  change = sqrt( ship->hx * ship->hx + ship->hy * ship->hy + ship->hz * ship->hz );

	  if( change > 0 )
	    {
	      dx = ship->hx / change;
	      dy = ship->hy / change;
	      dz = ship->hz / change;
	      ship->vx += ( dx * ship->currspeed / 5 );
	      ship->vy += ( dy * ship->currspeed / 5 );
	      ship->vz += ( dz * ship->currspeed / 5 );
	    }
	}

      if( autofly( ship ) )
	continue;

/*           
          if ( ship->class != SHIP_PLATFORM && !autofly(ship) )
          {
            if ( ship->starsystem->star1 && strcmp(ship->starsystem->star1,"") )
            {
              if (ship->vx >= ship->starsystem->s1x + 1 || ship->vx <= ship->starsystem->s1x - 1 )
                ship->vx -= URANGE(-3,(ship->starsystem->gravitys1)/(ship->vx - ship->starsystem->s1x)/2,3);
              if (ship->vy >= ship->starsystem->s1y + 1 || ship->vy <= ship->starsystem->s1y - 1 )
                ship->vy -= URANGE(-3,(ship->starsystem->gravitys1)/(ship->vy - ship->starsystem->s1y)/2,3);
              if (ship->vz >= ship->starsystem->s1z + 1 || ship->vz <= ship->starsystem->s1z - 1 )
                ship->vz -= URANGE(-3,(ship->starsystem->gravitys1)/(ship->vz - ship->starsystem->s1z)/2,3);
            }
          
            if ( ship->starsystem->star2 && strcmp(ship->starsystem->star2,"") )
            {
              if (ship->vx >= ship->starsystem->s2x + 1 || ship->vx <= ship->starsystem->s2x - 1 )
                ship->vx -= URANGE(-3,(ship->starsystem->gravitys2)/(ship->vx - ship->starsystem->s2x)/2,3);
              if (ship->vy >= ship->starsystem->s2y + 1 || ship->vy <= ship->starsystem->s2y - 1 )
                ship->vy -= URANGE(-3,(ship->starsystem->gravitys2)/(ship->vy - ship->starsystem->s2y)/2,3);
              if (ship->vz >= ship->starsystem->s2z + 1 || ship->vz <= ship->starsystem->s2z - 1 )
                ship->vz -= URANGE(-3,(ship->starsystem->gravitys2)/(ship->vz - ship->starsystem->s2z)/2,3);
            }
          
            if ( ship->starsystem->planet1 && strcmp(ship->starsystem->planet1,"") )
            {
              if (ship->vx >= ship->starsystem->p1x + 1 || ship->vx <= ship->starsystem->p1x - 1 )
                ship->vx -= URANGE(-3,(ship->starsystem->gravityp1)/(ship->vx - ship->starsystem->p1x)/2,3);
              if (ship->vy >= ship->starsystem->p1y + 1 || ship->vy <= ship->starsystem->p1y - 1 )
                ship->vy -= URANGE(-3,(ship->starsystem->gravityp1)/(ship->vy - ship->starsystem->p1y)/2,3);
              if (ship->vz >= ship->starsystem->p1z + 1 || ship->vz <= ship->starsystem->p1z - 1 )
                ship->vz -= URANGE(-3,(ship->starsystem->gravityp1)/(ship->vz - ship->starsystem->p1z)/2,3);
            }
          
            if ( ship->starsystem->planet2 && strcmp(ship->starsystem->planet2,"") )
            {
              if (ship->vx >= ship->starsystem->p2x + 1 || ship->vx <= ship->starsystem->p2x - 1 )
                ship->vx -= URANGE(-3,(ship->starsystem->gravityp2)/(ship->vx - ship->starsystem->p2x)/2,3);
              if (ship->vy >= ship->starsystem->p2y + 1 || ship->vy <= ship->starsystem->p2y - 1 )
                ship->vy -= URANGE(-3,(ship->starsystem->gravityp2)/(ship->vy - ship->starsystem->p2y)/2,3);
              if (ship->vz >= ship->starsystem->p2z + 1 || ship->vz <= ship->starsystem->p2z - 1 )
                ship->vz -= URANGE(-3,(ship->starsystem->gravityp2)/(ship->vz - ship->starsystem->p2z)/2,3);
            }
          
            if ( ship->starsystem->planet3 && strcmp(ship->starsystem->planet3,"") )
            {
              if (ship->vx >= ship->starsystem->p3x + 1 || ship->vx <= ship->starsystem->p3x - 1 )
                ship->vx -= URANGE(-3,(ship->starsystem->gravityp3)/(ship->vx - ship->starsystem->p3x)/2,3);
              if (ship->vy >= ship->starsystem->p3y + 1 || ship->vy <= ship->starsystem->p3y - 1 )
                ship->vy -= URANGE(-3,(ship->starsystem->gravityp3)/(ship->vy - ship->starsystem->p3y)/2,3);
              if (ship->vz >= ship->starsystem->p3z + 1 || ship->vz <= ship->starsystem->p3z - 1 )
                ship->vz -= URANGE(-3,(ship->starsystem->gravityp3)/(ship->vz - ship->starsystem->p3z)/2,3);
            }
          }

*/
/*
          for ( target = ship->starsystem->first_ship; target; target = target->next_in_starsystem)
          { 
                if ( target != ship &&
                    abs(ship->vx - target->vx) < 1 &&
                    abs(ship->vy - target->vy) < 1 &&
                    abs(ship->vz - target->vz) < 1 )
                {
                    ship->collision = target->maxhull;
                    target->collision = ship->maxhull;
                }                   
          }
*/
      if( ship->starsystem->star1 && strcmp( ship->starsystem->star1, "" )
	  && abs( ( int )( ship->vx - ship->starsystem->s1x ) ) < 10
	  && abs( ( int )( ship->vy - ship->starsystem->s1y ) ) < 10
	  && abs( ( int )( ship->vz - ship->starsystem->s1z ) ) < 10 )
	{
	  echo_to_cockpit( AT_BLOOD + AT_BLINK, ship, "You fly directly into the sun." );
	  snprintf( buf, MAX_STRING_LENGTH, "%s flys directly into %s!", ship->name, ship->starsystem->star1 );
	  echo_to_system( AT_ORANGE, ship, buf, NULL );
	  destroy_ship( ship, NULL );
	  continue;
	}
      if( ship->starsystem->star2 && strcmp( ship->starsystem->star2, "" )
	  && abs( ( int )( ship->vx - ship->starsystem->s2x ) ) < 10
	  && abs( ( int )( ship->vy - ship->starsystem->s2y ) ) < 10
	  && abs( ( int )( ship->vz - ship->starsystem->s2z ) ) < 10 )
	{
	  echo_to_cockpit( AT_BLOOD + AT_BLINK, ship, "You fly directly into the sun." );
	  snprintf( buf, MAX_STRING_LENGTH, "%s flys directly into %s!", ship->name, ship->starsystem->star2 );
	  echo_to_system( AT_ORANGE, ship, buf, NULL );
	  destroy_ship( ship, NULL );
	  continue;
	}

      if( ship->currspeed > 0 )
	{
	  if( ship->starsystem->planet1 && strcmp( ship->starsystem->planet1, "" )
	      && abs( ( int )( ship->vx - ship->starsystem->p1x ) ) < 10
	      && abs( ( int )( ship->vy - ship->starsystem->p1y ) ) < 10
	      && abs( ( int )( ship->vz - ship->starsystem->p1z ) ) < 10 )
	    {
	      snprintf( buf, MAX_STRING_LENGTH, "You begin orbitting %s.", ship->starsystem->planet1 );
	      echo_to_cockpit( AT_YELLOW, ship, buf );
	      snprintf( buf, MAX_STRING_LENGTH, "%s begins orbiting %s.", ship->name, ship->starsystem->planet1 );
	      echo_to_system( AT_ORANGE, ship, buf, NULL );
	      ship->currspeed = 0;
	      continue;
	    }
	  if( ship->starsystem->planet2 && strcmp( ship->starsystem->planet2, "" )
	      && abs( ( int )( ship->vx - ship->starsystem->p2x ) ) < 10
	      && abs( ( int )( ship->vy - ship->starsystem->p2y ) ) < 10
	      && abs( ( int )( ship->vz - ship->starsystem->p2z ) ) < 10 )
	    {
	      snprintf( buf, MAX_STRING_LENGTH, "You begin orbitting %s.", ship->starsystem->planet2 );
	      echo_to_cockpit( AT_YELLOW, ship, buf );
	      snprintf( buf, MAX_STRING_LENGTH, "%s begins orbiting %s.", ship->name, ship->starsystem->planet2 );
	      echo_to_system( AT_ORANGE, ship, buf, NULL );
	      ship->currspeed = 0;
	      continue;
	    }
	  if( ship->starsystem->planet3 && strcmp( ship->starsystem->planet3, "" )
	      && abs( ( int )( ship->vx - ship->starsystem->p3x ) ) < 10
	      && abs( ( int )( ship->vy - ship->starsystem->p3y ) ) < 10
	      && abs( ( int )( ship->vz - ship->starsystem->p3z ) ) < 10 )
	    {
	      snprintf( buf, MAX_STRING_LENGTH, "You begin orbitting %s.", ship->starsystem->planet2 );
	      echo_to_cockpit( AT_YELLOW, ship, buf );
	      snprintf( buf, MAX_STRING_LENGTH, "%s begins orbiting %s.", ship->name, ship->starsystem->planet2 );
	      echo_to_system( AT_ORANGE, ship, buf, NULL );
	      ship->currspeed = 0;
	      continue;
	    }
	}
    }

  for( ship = first_ship; ship; ship = ship->next )
    if( ship->collision )
      {
	echo_to_cockpit( AT_WHITE + AT_BLINK, ship, "You have collided with another ship!" );
	echo_to_ship( AT_RED, ship, "A loud explosion shakes the ship violently!" );
	damage_ship( ship, ship->collision, ship->collision );
	ship->collision = 0;
      }
}

void recharge_ships(  )
{
   SHIP_DATA *ship;
   char buf[MAX_STRING_LENGTH];

   for( ship = first_ship; ship; ship = ship->next )
   {

      if( ship->statet0 > 0 )
      {
         ship->energy -= ship->statet0;
         ship->statet0 = 0;
      }
      if( ship->statet1 > 0 )
      {
         ship->energy -= ship->statet1;
         ship->statet1 = 0;
      }
      if( ship->statet2 > 0 )
      {
         ship->energy -= ship->statet2;
         ship->statet2 = 0;
      }

      if( ship->missilestate == MISSILE_RELOAD_2 )
      {
         ship->missilestate = MISSILE_READY;
         if( ship->missiles > 0 )
            echo_to_room( AT_YELLOW, get_room_index( ship->gunseat ), "Missile launcher reloaded." );
      }

      if( ship->missilestate == MISSILE_RELOAD )
      {
         ship->missilestate = MISSILE_RELOAD_2;
      }

      if( ship->missilestate == MISSILE_FIRED )
         ship->missilestate = MISSILE_RELOAD;

      if( ship->torpedostate == MISSILE_RELOAD_2 )
      {
         ship->torpedostate = MISSILE_READY;
         if( ship->torpedos > 0 )
            echo_to_room( AT_YELLOW, get_room_index( ship->gunseat ), "Torpedo launcher reloaded." );
      }
      if( ship->torpedostate == MISSILE_RELOAD )
         ship->torpedostate = MISSILE_RELOAD_2;
      if( ship->torpedostate == MISSILE_FIRED )
         ship->torpedostate = MISSILE_RELOAD;

      if( ship->rocketstate == MISSILE_RELOAD_2 )
      {
         ship->rocketstate = MISSILE_READY;
         if( ship->rockets > 0 )
            echo_to_room( AT_YELLOW, get_room_index( ship->gunseat ), "Rocket launcher reloaded." );
      }
      if( ship->rocketstate == MISSILE_RELOAD )
         ship->rocketstate = MISSILE_RELOAD_2;
      if( ship->rocketstate == MISSILE_FIRED )
         ship->rocketstate = MISSILE_RELOAD;

      if( autofly( ship ) )
      {
         if( ship->starsystem )
         {
            if( ship->target0 && ship->statet0 != LASER_DAMAGED )
            {
               int schance = 50;
               SHIP_DATA *target = ship->target0;
               int shots;

               for( shots = 0; shots <= ship->lasers; shots++ )
               {
                  if( ship->shipstate != SHIP_HYPERSPACE && ship->energy > 25
                      && ship->target0->starsystem == ship->starsystem
                      && abs( ( int )( target->vx - ship->vx ) ) <= 1000
                      && abs( ( int )( target->vy - ship->vy ) ) <= 1000
                      && abs( ( int )( target->vz - ship->vz ) ) <= 1000
		      && ship->statet0 < ship->lasers )
                  {
                     if( ship->ship_class > 1 || is_facing( ship, target ) )
                     {
                        schance += target->ship_class * 25;
                        schance -= target->manuever / 10;
                        schance -= target->currspeed / 20;
                        schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 70 );
                        schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 70 );
                        schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 70 );
                        schance = URANGE( 10, schance, 90 );
                        if( number_percent(  ) > schance )
                        {
                           snprintf( buf, MAX_STRING_LENGTH, "%s fires at you but misses.", ship->name );
                           echo_to_cockpit( AT_ORANGE, target, buf );
                           snprintf( buf, MAX_STRING_LENGTH, "Laserfire from %s barely misses %s.", ship->name, target->name );
                           echo_to_system( AT_ORANGE, target, buf, NULL );
                        }
                        else
                        {
                           snprintf( buf, MAX_STRING_LENGTH, "Laserfire from %s hits %s.", ship->name, target->name );
                           echo_to_system( AT_ORANGE, target, buf, NULL );
                           snprintf( buf, MAX_STRING_LENGTH, "You are hit by lasers from %s!", ship->name );
                           echo_to_cockpit( AT_BLOOD, target, buf );
                           echo_to_ship( AT_RED, target, "A small explosion vibrates through the ship." );
                           damage_ship( target, 5, 10 );
                        }
                        ship->statet0++;
                     }
                  }
               }
            }
         }
      }
   }
}

void update_space(  )
{
   SHIP_DATA *ship;
   SHIP_DATA *target;
   char buf[MAX_STRING_LENGTH];
   int too_close, target_too_close;
   int recharge;

   for( ship = first_ship; ship; ship = ship->next )
   {
      if( ship->starsystem )
      {
         if( ship->energy > 0 && ship->shipstate == SHIP_DISABLED
	     && ship->ship_class != SHIP_PLATFORM )
            ship->energy -= 100;
         else if( ship->energy > 0 )
            ship->energy += ( 5 + ship->ship_class * 5 );
         else
            destroy_ship( ship, NULL );
      }

      if( ship->chaff_released > 0 )
         ship->chaff_released--;

      if( ship->shipstate == SHIP_HYPERSPACE )
      {
         ship->hyperdistance -= ship->hyperspeed * 2;
         if( ship->hyperdistance <= 0 )
         {
            ship_to_starsystem( ship, ship->currjump );

            if( ship->starsystem == NULL )
            {
               echo_to_cockpit( AT_RED, ship, "Ship lost in Hyperspace. Make new calculations." );
            }
            else
            {
               echo_to_room( AT_YELLOW, get_room_index( ship->pilotseat ), "Hyperjump complete." );
               echo_to_ship( AT_YELLOW, ship, "The ship lurches slightly as it comes out of hyperspace." );
               snprintf( buf, MAX_STRING_LENGTH, "%s enters the starsystem at %.0f %.0f %.0f", ship->name, ship->vx, ship->vy, ship->vz );
               echo_to_system( AT_YELLOW, ship, buf, NULL );
               ship->shipstate = SHIP_READY;
               STRFREE( ship->home );
               ship->home = STRALLOC( ship->starsystem->name );
               if( str_cmp( "Public", ship->owner ) )
                  save_ship( ship );
            }
         }
         else
         {
            snprintf( buf, MAX_STRING_LENGTH, "%d", ship->hyperdistance );
            echo_to_room_dnr( AT_YELLOW, get_room_index( ship->pilotseat ), "Remaining jump distance: " );
            echo_to_room( AT_WHITE, get_room_index( ship->pilotseat ), buf );
         }
      }

      /*
       * following was originaly to fix ships that lost their pilot 
       * in the middle of a manuever and are stuck in a busy state 
       * but now used for timed manouevers such as turning 
       */
      if( ship->shipstate == SHIP_BUSY_3 )
      {
         echo_to_room( AT_YELLOW, get_room_index( ship->pilotseat ), "Manuever complete." );
         ship->shipstate = SHIP_READY;
      }
      if( ship->shipstate == SHIP_BUSY_2 )
         ship->shipstate = SHIP_BUSY_3;
      if( ship->shipstate == SHIP_BUSY )
         ship->shipstate = SHIP_BUSY_2;

      if( ship->shipstate == SHIP_LAND_2 )
         landship( ship, ship->dest );
      if( ship->shipstate == SHIP_LAND )
         ship->shipstate = SHIP_LAND_2;

      if( ship->shipstate == SHIP_LAUNCH_2 )
         launchship( ship );
      if( ship->shipstate == SHIP_LAUNCH )
         ship->shipstate = SHIP_LAUNCH_2;

      /* v1.34: SHIP_DOCK/DOCK_2/DOCK_3 progression was never wired into
       * this loop at all - do_tractorbeam (capture) and do_dock
       * (voluntary docking) both set shipstate2 = SHIP_DOCK on success
       * and then nothing ever advanced it, so IS_DOCKED() (which checks
       * for DOCK_3 specifically) could never become true. That silently
       * broke capture, voluntary docking, do_detach (which requires
       * IS_DOCKED to undock), and prep-while-docked-to-a-platform
       * (prepcost's 10x space-resupply pricing). Mirrors the BUSY/LAND/
       * LAUNCH multi-tick pattern already established above. */
      if( ship->shipstate2 == SHIP_DOCK_3 )
      {
         /* steady state - fully docked, nothing to advance */
      }
      else if( ship->shipstate2 == SHIP_DOCK_2 )
      {
         ship->shipstate2 = SHIP_DOCK_3;
         if( ship->docked_ship )
            echo_to_ship( AT_YELLOW, ship, "The docking clamps engage with a heavy clunk. Docking complete." );
      }
      else if( ship->shipstate2 == SHIP_DOCK )
         ship->shipstate2 = SHIP_DOCK_2;


      ship->shield = UMAX( 0, ship->shield - 1 - ship->ship_class );

      if( ship->autorecharge && ship->maxshield > ship->shield && ship->energy > 100 )
      {
         recharge = UMIN( ship->maxshield - ship->shield, 10 + ship->ship_class * 10 );
         recharge = UMIN( recharge, ship->energy / 2 - 100 );
         recharge = UMAX( 1, recharge );
         ship->shield += recharge;
         ship->energy -= recharge;
      }

      if( ship->shield > 0 )
      {
         if( ship->energy < 200 )
         {
            ship->shield = 0;
            echo_to_cockpit( AT_RED, ship, "The ships shields fizzle and die." );
            ship->autorecharge = FALSE;
         }
      }

      if( ship->starsystem && ship->currspeed > 0 )
      {
         snprintf( buf, MAX_STRING_LENGTH, "%d", ship->currspeed );
         echo_to_room_dnr( AT_BLUE, get_room_index( ship->pilotseat ), "Speed: " );
         echo_to_room_dnr( AT_LBLUE, get_room_index( ship->pilotseat ), buf );
         snprintf( buf, MAX_STRING_LENGTH, "%.0f %.0f %.0f", ship->vx, ship->vy, ship->vz );
         echo_to_room_dnr( AT_BLUE, get_room_index( ship->pilotseat ), "  Coords: " );
         echo_to_room( AT_LBLUE, get_room_index( ship->pilotseat ), buf );
         if( ship->pilotseat != ship->coseat )
         {
            snprintf( buf, MAX_STRING_LENGTH, "%d", ship->currspeed );
            echo_to_room_dnr( AT_BLUE, get_room_index( ship->coseat ), "Speed: " );
            echo_to_room_dnr( AT_LBLUE, get_room_index( ship->coseat ), buf );
            snprintf( buf, MAX_STRING_LENGTH, "%.0f %.0f %.0f", ship->vx, ship->vy, ship->vz );
            echo_to_room_dnr( AT_BLUE, get_room_index( ship->coseat ), "  Coords: " );
            echo_to_room( AT_LBLUE, get_room_index( ship->coseat ), buf );
         }
      }

      if( ship->starsystem )
      {
         too_close = ship->currspeed + 50;
         for( target = ship->starsystem->first_ship; target; target = target->next_in_starsystem )
         {
            target_too_close = too_close + target->currspeed;
            if( target != ship
		&& abs( ( int )( ship->vx - target->vx ) ) < target_too_close
		&& abs( ( int )( ship->vy - target->vy ) ) < target_too_close
		&& abs( ( int )( ship->vz - target->vz ) ) < target_too_close )
            {
               snprintf( buf, MAX_STRING_LENGTH, "Proximity alert: %s  %.0f %.0f %.0f", target->name, target->vx, target->vy, target->vz );
               echo_to_room( AT_RED, get_room_index( ship->pilotseat ), buf );
            }
         }
         too_close = ship->currspeed + 100;
         if( ship->starsystem->star1 && strcmp( ship->starsystem->star1, "" )
	     && abs( ( int )( ship->vx - ship->starsystem->s1x ) ) < too_close
	     && abs( ( int )( ship->vy - ship->starsystem->s1y ) ) < too_close
	     && abs( ( int )( ship->vz - ship->starsystem->s1z ) ) < too_close)
         {
            snprintf( buf, MAX_STRING_LENGTH, "Proximity alert: %s  %d %d %d", ship->starsystem->star1,
                     ship->starsystem->s1x, ship->starsystem->s1y, ship->starsystem->s1z );
            echo_to_room( AT_RED, get_room_index( ship->pilotseat ), buf );
         }
         if( ship->starsystem->star2 && strcmp( ship->starsystem->star2, "" )
	     && abs( ( int )( ship->vx - ship->starsystem->s2x ) ) < too_close
	     && abs( ( int )( ship->vy - ship->starsystem->s2y ) ) < too_close
	     && abs( ( int )( ship->vz - ship->starsystem->s2z ) ) < too_close)
         {
            snprintf( buf, MAX_STRING_LENGTH, "Proximity alert: %s  %d %d %d", ship->starsystem->star2,
                     ship->starsystem->s2x, ship->starsystem->s2y, ship->starsystem->s2z );
            echo_to_room( AT_RED, get_room_index( ship->pilotseat ), buf );
         }
         if( ship->starsystem->planet1
	     && strcmp( ship->starsystem->planet1, "" )
	     && abs( ( int )( ship->vx - ship->starsystem->p1x ) ) < too_close
	     && abs( ( int )( ship->vy - ship->starsystem->p1y ) ) < too_close
	     && abs( ( int )( ship->vz - ship->starsystem->p1z ) ) < too_close)
         {
            snprintf( buf, MAX_STRING_LENGTH, "Proximity alert: %s  %d %d %d", ship->starsystem->planet1,
                     ship->starsystem->p1x, ship->starsystem->p1y, ship->starsystem->p1z );
            echo_to_room( AT_RED, get_room_index( ship->pilotseat ), buf );
         }
         if( ship->starsystem->planet2
	     && strcmp( ship->starsystem->planet2, "" )
	     && abs( ( int )( ship->vx - ship->starsystem->p2x ) ) < too_close
	     && abs( ( int )( ship->vy - ship->starsystem->p2y ) ) < too_close
	     && abs( ( int )( ship->vz - ship->starsystem->p2z ) ) < too_close)
         {
            snprintf( buf, MAX_STRING_LENGTH, "Proximity alert: %s  %d %d %d", ship->starsystem->planet2,
                     ship->starsystem->p2x, ship->starsystem->p2y, ship->starsystem->p2z );
            echo_to_room( AT_RED, get_room_index( ship->pilotseat ), buf );
         }
         if( ship->starsystem->planet3
	     && strcmp( ship->starsystem->planet3, "" )
	     && abs( ( int )( ship->vx - ship->starsystem->p3x ) ) < too_close
	     && abs( ( int )( ship->vy - ship->starsystem->p3y ) ) < too_close
	     && abs( ( int )( ship->vz - ship->starsystem->p3z ) ) < too_close)
         {
            snprintf( buf, MAX_STRING_LENGTH, "Proximity alert: %s  %d %d %d", ship->starsystem->planet3,
                     ship->starsystem->p3x, ship->starsystem->p3y, ship->starsystem->p3z );
            echo_to_room( AT_RED, get_room_index( ship->pilotseat ), buf );
         }
      }

      if( ship->target0 )
      {
         snprintf( buf, MAX_STRING_LENGTH, "%s   %.0f %.0f %.0f", ship->target0->name, ship->target0->vx, ship->target0->vy, ship->target0->vz );
         echo_to_room_dnr( AT_BLUE, get_room_index( ship->gunseat ), "Target: " );
         echo_to_room( AT_LBLUE, get_room_index( ship->gunseat ), buf );
         if( ship->starsystem != ship->target0->starsystem )
            ship->target0 = NULL;
      }

      if( ship->target1 )
      {
         snprintf( buf, MAX_STRING_LENGTH, "%s   %.0f %.0f %.0f", ship->target1->name, ship->target1->vx, ship->target1->vy, ship->target1->vz );
         echo_to_room_dnr( AT_BLUE, get_room_index( ship->turret1 ), "Target: " );
         echo_to_room( AT_LBLUE, get_room_index( ship->turret1 ), buf );
         if( ship->starsystem != ship->target1->starsystem )
            ship->target1 = NULL;
      }

      if( ship->target2 )
      {
         snprintf( buf, MAX_STRING_LENGTH, "%s   %.0f %.0f %.0f", ship->target2->name, ship->target2->vx, ship->target2->vy, ship->target2->vz );
         echo_to_room_dnr( AT_BLUE, get_room_index( ship->turret2 ), "Target: " );
         echo_to_room( AT_LBLUE, get_room_index( ship->turret2 ), buf );
         if( ship->starsystem != ship->target2->starsystem )
            ship->target2 = NULL;
      }

      if( ship->energy < 100 && ship->starsystem )
      {
         echo_to_cockpit( AT_RED, ship, "Warning: Ship fuel low." );
      }

      ship->energy = URANGE( 0, ship->energy, ship->maxenergy );
   }

   for( ship = first_ship; ship; ship = ship->next )
   {

      if( ship->autotrack && ship->target0 && ship->ship_class < 3 )
      {
         target = ship->target0;
         too_close = ship->currspeed + 10;
         target_too_close = too_close + target->currspeed;
         if( target != ship && ship->shipstate == SHIP_READY
	     && abs( ( int )( ship->vx - target->vx ) ) < target_too_close
	     && abs( ( int )( ship->vy - target->vy ) ) < target_too_close
	     && abs( ( int )( ship->vz - target->vz ) ) < target_too_close )
         {
            ship->hx = 0 - ( ship->target0->vx - ship->vx );
            ship->hy = 0 - ( ship->target0->vy - ship->vy );
            ship->hz = 0 - ( ship->target0->vz - ship->vz );
            ship->energy -= ship->currspeed / 10;
            echo_to_room( AT_RED, get_room_index( ship->pilotseat ), "Autotrack: Evading to avoid collision!\r\n" );
            if( ship->ship_class == FIGHTER_SHIP
		|| ( ship->ship_class == MIDSIZE_SHIP
		     && ship->manuever > 50 ) )
               ship->shipstate = SHIP_BUSY_3;
            else if( ship->ship_class == MIDSIZE_SHIP
		     || ( ship->ship_class == CAPITAL_SHIP
			  && ship->manuever > 50 ) )
               ship->shipstate = SHIP_BUSY_2;
            else
               ship->shipstate = SHIP_BUSY;
         }
         else if( !is_facing( ship, ship->target0 ) )
         {
            ship->hx = ship->target0->vx - ship->vx;
            ship->hy = ship->target0->vy - ship->vy;
            ship->hz = ship->target0->vz - ship->vz;
            ship->energy -= ship->currspeed / 10;
            echo_to_room( AT_BLUE, get_room_index( ship->pilotseat ), "Autotracking target ... setting new course.\r\n" );
            if( ship->ship_class == FIGHTER_SHIP || ( ship->ship_class == MIDSIZE_SHIP && ship->manuever > 50 ) )
               ship->shipstate = SHIP_BUSY_3;
            else if( ship->ship_class == MIDSIZE_SHIP || ( ship->ship_class == CAPITAL_SHIP && ship->manuever > 50 ) )
               ship->shipstate = SHIP_BUSY_2;
            else
               ship->shipstate = SHIP_BUSY;
         }
      }

      if( autofly( ship ) )
      {
         if( ship->starsystem )
         {
            if( ship->target0 )
            {
               int schance = 50;

               /*
                * auto assist ships 
                */

               for( target = ship->starsystem->first_ship; target; target = target->next_in_starsystem )
               {
                  if( autofly( target ) )
                     if( !str_cmp( target->owner, ship->owner ) && target != ship )
                        if( target->target0 == NULL && ship->target0 != target )
                        {
                           target->target0 = ship->target0;
                           snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
                           echo_to_cockpit( AT_BLOOD, target->target0, buf );
                           break;
                        }
               }

               target = ship->target0;
               ship->autotrack = TRUE;
               if( ship->ship_class != SHIP_PLATFORM )
                  ship->currspeed = ship->realspeed;
               if( ship->energy > 200 )
                  ship->autorecharge = TRUE;


               if( ship->shipstate != SHIP_HYPERSPACE && ship->energy > 25
                   && ship->missilestate == MISSILE_READY && ship->target0->starsystem == ship->starsystem
                   && abs( ( int )( target->vx - ship->vx ) ) <= 1200
                   && abs( ( int )( target->vy - ship->vy ) ) <= 1200
		   && abs( ( int )( target->vz - ship->vz ) ) <= 1200
		   && ship->missiles > 0 )
               {
                  if( ship->ship_class > 1 || is_facing( ship, target ) )
                  {
                     schance -= target->manuever / 5;
                     schance -= target->currspeed / 20;
                     schance += target->ship_class * target->ship_class * 25;
                     schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 100 );
                     schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 100 );
                     schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 100 );
                     schance += ( 30 );
                     schance = URANGE( 10, schance, 90 );

                     if( number_percent(  ) > schance )
                     {
                     }
                     else
                     {
                        new_missile( ship, target, NULL, CONCUSSION_MISSILE );
                        ship->missiles--;
                        snprintf( buf, MAX_STRING_LENGTH, "Incoming missile from %s.", ship->name );
                        echo_to_cockpit( AT_BLOOD, target, buf );
                        snprintf( buf, MAX_STRING_LENGTH, "%s fires a missile towards %s.", ship->name, target->name );
                        echo_to_system( AT_ORANGE, target, buf, NULL );

                        if( ship->ship_class == CAPITAL_SHIP
			    || ship->ship_class == SHIP_PLATFORM )
                           ship->missilestate = MISSILE_RELOAD_2;
                        else
                           ship->missilestate = MISSILE_FIRED;
                     }
                  }
               }

               if( ship->missilestate == MISSILE_DAMAGED )
                  ship->missilestate = MISSILE_READY;
               if( ship->statet0 == LASER_DAMAGED )
                  ship->statet0 = LASER_READY;
               if( ship->shipstate == SHIP_DISABLED )
                  ship->shipstate = SHIP_READY;

            }
            else
            {
               ship->currspeed = 0;

               if( !str_cmp( ship->owner, "The Empire" ) )
                  for( target = first_ship; target; target = target->next )
                     if( ship->starsystem == target->starsystem )
                        if( !str_cmp( target->owner, "The New Republic" ) )
                        {
                           ship->target0 = target;
                           snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", ship->name );
                           echo_to_cockpit( AT_BLOOD, target, buf );
                           break;
                        }
               if( !str_cmp( ship->owner, "The New Republic" ) )
                  for( target = first_ship; target; target = target->next )
                     if( ship->starsystem == target->starsystem )
                        if( !str_cmp( target->owner, "The Empire" ) )
                        {
                           snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", ship->name );
                           echo_to_cockpit( AT_BLOOD, target, buf );
                           ship->target0 = target;
                           break;
                        }

               if( !str_cmp( ship->owner, "Pirates" ) )
                  for( target = first_ship; target; target = target->next )
                     if( ship->starsystem == target->starsystem )
                     {
                        snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", ship->name );
                        echo_to_cockpit( AT_BLOOD, target, buf );
                        ship->target0 = target;
                        break;
                     }
            }
         }
         else
         {
            if( number_range( 1, 25 ) == 25 )
            {
               ship_to_starsystem( ship, starsystem_from_name( ship->home ) );
               ship->vx = number_range( -5000, 5000 );
               ship->vy = number_range( -5000, 5000 );
               ship->vz = number_range( -5000, 5000 );
               ship->hx = 1;
               ship->hy = 1;
               ship->hz = 1;
            }
         }
      }

      if( ( ship->ship_class == CAPITAL_SHIP
	    || ship->ship_class == SHIP_PLATFORM ) && ship->target0 == NULL )
      {
         if( ship->missiles < ship->maxmissiles )
            ship->missiles++;
         if( ship->torpedos < ship->maxtorpedos )
            ship->torpedos++;
         if( ship->rockets < ship->maxrockets )
            ship->rockets++;
      }
   }
}

void write_starsystem_list(  )
{
   SPACE_DATA *tstarsystem;
   FILE *fpout;
   char filename[256];

   snprintf( filename, 256, "%s%s", SPACE_DIR, SPACE_LIST );
   fpout = fopen( filename, "w" );
   if( !fpout )
   {
      bug( "FATAL: %s: cannot open starsystem.lst for writing!\r\n", __func__ );
      return;
   }
   for( tstarsystem = first_starsystem; tstarsystem; tstarsystem = tstarsystem->next )
      fprintf( fpout, "%s\n", tstarsystem->filename );
   fprintf( fpout, "$\n" );
   FCLOSE( fpout );
}

/*
 * Get pointer to space structure from starsystem name.
 */
SPACE_DATA *starsystem_from_name( const char *name )
{
   SPACE_DATA *starsystem;

   for( starsystem = first_starsystem; starsystem; starsystem = starsystem->next )
      if( !str_cmp( name, starsystem->name ) )
         return starsystem;

   for( starsystem = first_starsystem; starsystem; starsystem = starsystem->next )
      if( !str_prefix( name, starsystem->name ) )
         return starsystem;

   return NULL;
}

/*
 * Get pointer to space structure from the dock vnun.
 */
SPACE_DATA *starsystem_from_vnum( int vnum )
{
   SPACE_DATA *starsystem;
   SHIP_DATA *ship;

   for( starsystem = first_starsystem; starsystem; starsystem = starsystem->next )
      if( vnum == starsystem->doc1a || vnum == starsystem->doc2a || vnum == starsystem->doc3a ||
          vnum == starsystem->doc1b || vnum == starsystem->doc2b || vnum == starsystem->doc3b ||
          vnum == starsystem->doc1c || vnum == starsystem->doc2c || vnum == starsystem->doc3c )
         return starsystem;

   for( ship = first_ship; ship; ship = ship->next )
      if( vnum == ship->hanger )
         return ship->starsystem;

   return NULL;
}

/*
 * Save a starsystem's data to its data file
 */
void save_starsystem( SPACE_DATA * starsystem )
{
   FILE *fp;
   char filename[256];

   if( !starsystem )
   {
      bug( "%s: null starsystem pointer!", __func__ );
      return;
   }

   if( !starsystem->filename || starsystem->filename[0] == '\0' )
   {
      bug( "%s: %s has no filename", __func__, starsystem->name );
      return;
   }

   snprintf( filename, 256, "%s%s", SPACE_DIR, starsystem->filename );

   if( ( fp = fopen( filename, "w" ) ) == NULL )
   {
      bug( "%s: fopen", __func__ );
      perror( filename );
   }
   else
   {
      fprintf( fp, "#SPACE\n" );
      fprintf( fp, "Name         %s~\n", starsystem->name );
      fprintf( fp, "Filename     %s~\n", starsystem->filename );
      fprintf( fp, "Planet1      %s~\n", starsystem->planet1 );
      fprintf( fp, "Planet2      %s~\n", starsystem->planet2 );
      fprintf( fp, "Planet3      %s~\n", starsystem->planet3 );
      fprintf( fp, "Star1        %s~\n", starsystem->star1 );
      fprintf( fp, "Star2        %s~\n", starsystem->star2 );
      fprintf( fp, "Location1a      %s~\n", starsystem->location1a );
      fprintf( fp, "Location1b      %s~\n", starsystem->location1b );
      fprintf( fp, "Location1c      %s~\n", starsystem->location1c );
      fprintf( fp, "Location2a       %s~\n", starsystem->location2a );
      fprintf( fp, "Location2b      %s~\n", starsystem->location2b );
      fprintf( fp, "Location2c      %s~\n", starsystem->location2c );
      fprintf( fp, "Location3a      %s~\n", starsystem->location3a );
      fprintf( fp, "Location3b      %s~\n", starsystem->location3b );
      fprintf( fp, "Location3c      %s~\n", starsystem->location3c );
      fprintf( fp, "Doc1a          %d\n", starsystem->doc1a );
      fprintf( fp, "Doc2a          %d\n", starsystem->doc2a );
      fprintf( fp, "Doc3a          %d\n", starsystem->doc3a );
      fprintf( fp, "Doc1b          %d\n", starsystem->doc1b );
      fprintf( fp, "Doc2b          %d\n", starsystem->doc2b );
      fprintf( fp, "Doc3b          %d\n", starsystem->doc3b );
      fprintf( fp, "Doc1c          %d\n", starsystem->doc1c );
      fprintf( fp, "Doc2c          %d\n", starsystem->doc2c );
      fprintf( fp, "Doc3c          %d\n", starsystem->doc3c );
      fprintf( fp, "P1x          %d\n", starsystem->p1x );
      fprintf( fp, "P1y          %d\n", starsystem->p1y );
      fprintf( fp, "P1z          %d\n", starsystem->p1z );
      fprintf( fp, "P2x          %d\n", starsystem->p2x );
      fprintf( fp, "P2y          %d\n", starsystem->p2y );
      fprintf( fp, "P2z          %d\n", starsystem->p2z );
      fprintf( fp, "P3x          %d\n", starsystem->p3x );
      fprintf( fp, "P3y          %d\n", starsystem->p3y );
      fprintf( fp, "P3z          %d\n", starsystem->p3z );
      fprintf( fp, "S1x          %d\n", starsystem->s1x );
      fprintf( fp, "S1y          %d\n", starsystem->s1y );
      fprintf( fp, "S1z          %d\n", starsystem->s1z );
      fprintf( fp, "S2x          %d\n", starsystem->s2x );
      fprintf( fp, "S2y          %d\n", starsystem->s2y );
      fprintf( fp, "S2z          %d\n", starsystem->s2z );
      fprintf( fp, "Gravitys1     %d\n", starsystem->gravitys1 );
      fprintf( fp, "Gravitys2     %d\n", starsystem->gravitys2 );
      fprintf( fp, "Gravityp1     %d\n", starsystem->gravityp1 );
      fprintf( fp, "Gravityp2     %d\n", starsystem->gravityp2 );
      fprintf( fp, "Gravityp3     %d\n", starsystem->gravityp3 );
      fprintf( fp, "Xpos          %d\n", starsystem->xpos );
      fprintf( fp, "Ypos          %d\n", starsystem->ypos );
      fprintf( fp, "End\n\n" );
      fprintf( fp, "#END\n" );
      FCLOSE( fp );
   }
}

/*
 * Read in actual starsystem data.
 */
void fread_starsystem( SPACE_DATA * starsystem, FILE * fp )
{
   const char *word;
   bool fMatch;

   for( ;; )
   {
      word = feof( fp ) ? "End" : fread_word( fp );
      fMatch = FALSE;

      switch ( UPPER( word[0] ) )
      {
         case '*':
            fMatch = TRUE;
            fread_to_eol( fp );
            break;

         case 'D':
            KEY( "Doc1a", starsystem->doc1a, fread_number( fp ) );
            KEY( "Doc2a", starsystem->doc2a, fread_number( fp ) );
            KEY( "Doc3a", starsystem->doc3a, fread_number( fp ) );
            KEY( "Doc1b", starsystem->doc1b, fread_number( fp ) );
            KEY( "Doc2b", starsystem->doc2b, fread_number( fp ) );
            KEY( "Doc3b", starsystem->doc3b, fread_number( fp ) );
            KEY( "Doc1c", starsystem->doc1c, fread_number( fp ) );
            KEY( "Doc2c", starsystem->doc2c, fread_number( fp ) );
            KEY( "Doc3c", starsystem->doc3c, fread_number( fp ) );
            break;

         case 'E':
            if( !str_cmp( word, "End" ) )
            {
               if( !starsystem->name )
                  starsystem->name = STRALLOC( "" );
               if( !starsystem->location1a )
                  starsystem->location1a = STRALLOC( "" );
               if( !starsystem->location2a )
                  starsystem->location2a = STRALLOC( "" );
               if( !starsystem->location3a )
                  starsystem->location3a = STRALLOC( "" );
               if( !starsystem->location1b )
                  starsystem->location1b = STRALLOC( "" );
               if( !starsystem->location2b )
                  starsystem->location2b = STRALLOC( "" );
               if( !starsystem->location3b )
                  starsystem->location3b = STRALLOC( "" );
               if( !starsystem->location1c )
                  starsystem->location1c = STRALLOC( "" );
               if( !starsystem->location2c )
                  starsystem->location2c = STRALLOC( "" );
               if( !starsystem->location3c )
                  starsystem->location3c = STRALLOC( "" );
               if( !starsystem->planet1 )
                  starsystem->planet1 = STRALLOC( "" );
               if( !starsystem->planet2 )
                  starsystem->planet2 = STRALLOC( "" );
               if( !starsystem->planet3 )
                  starsystem->planet3 = STRALLOC( "" );
               if( !starsystem->star1 )
                  starsystem->star1 = STRALLOC( "" );
               if( !starsystem->star2 )
                  starsystem->star2 = STRALLOC( "" );
               return;
            }
            break;

         case 'F':
            KEY( "Filename", starsystem->filename, fread_string_nohash( fp ) );
            break;

         case 'G':
            KEY( "Gravitys1", starsystem->gravitys1, fread_number( fp ) );
            KEY( "Gravitys2", starsystem->gravitys2, fread_number( fp ) );
            KEY( "Gravityp1", starsystem->gravityp1, fread_number( fp ) );
            KEY( "Gravityp2", starsystem->gravityp2, fread_number( fp ) );
            KEY( "Gravityp3", starsystem->gravityp3, fread_number( fp ) );
            break;

         case 'L':
            KEY( "Location1a", starsystem->location1a, fread_string( fp ) );
            KEY( "Location2a", starsystem->location2a, fread_string( fp ) );
            KEY( "Location3a", starsystem->location3a, fread_string( fp ) );
            KEY( "Location1b", starsystem->location1b, fread_string( fp ) );
            KEY( "Location2b", starsystem->location2b, fread_string( fp ) );
            KEY( "Location3b", starsystem->location3b, fread_string( fp ) );
            KEY( "Location1c", starsystem->location1c, fread_string( fp ) );
            KEY( "Location2c", starsystem->location2c, fread_string( fp ) );
            KEY( "Location3c", starsystem->location3c, fread_string( fp ) );
            break;

         case 'N':
            KEY( "Name", starsystem->name, fread_string( fp ) );
            break;

         case 'P':
            KEY( "Planet1", starsystem->planet1, fread_string( fp ) );
            KEY( "Planet2", starsystem->planet2, fread_string( fp ) );
            KEY( "Planet3", starsystem->planet3, fread_string( fp ) );
            KEY( "P1x", starsystem->p1x, fread_number( fp ) );
            KEY( "P1y", starsystem->p1y, fread_number( fp ) );
            KEY( "P1z", starsystem->p1z, fread_number( fp ) );
            KEY( "P2x", starsystem->p2x, fread_number( fp ) );
            KEY( "P2y", starsystem->p2y, fread_number( fp ) );
            KEY( "P2z", starsystem->p2z, fread_number( fp ) );
            KEY( "P3x", starsystem->p3x, fread_number( fp ) );
            KEY( "P3y", starsystem->p3y, fread_number( fp ) );
            KEY( "P3z", starsystem->p3z, fread_number( fp ) );
            break;

         case 'S':
            KEY( "Star1", starsystem->star1, fread_string( fp ) );
            KEY( "Star2", starsystem->star2, fread_string( fp ) );
            KEY( "S1x", starsystem->s1x, fread_number( fp ) );
            KEY( "S1y", starsystem->s1y, fread_number( fp ) );
            KEY( "S1z", starsystem->s1z, fread_number( fp ) );
            KEY( "S2x", starsystem->s2x, fread_number( fp ) );
            KEY( "S2y", starsystem->s2y, fread_number( fp ) );
            KEY( "S2z", starsystem->s2z, fread_number( fp ) );

         case 'X':
            KEY( "Xpos", starsystem->xpos, fread_number( fp ) );

         case 'Y':
            KEY( "Ypos", starsystem->ypos, fread_number( fp ) );

      }

      if( !fMatch )
      {
         bug( "%s: no match: %s", __func__, word );
      }
   }
}

/*
 * Load a starsystem file
 */
bool load_starsystem( const char *starsystemfile )
{
   char filename[256];
   SPACE_DATA *starsystem;
   FILE *fp;
   bool found;

   CREATE( starsystem, SPACE_DATA, 1 );

   found = FALSE;
   snprintf( filename, 256, "%s%s", SPACE_DIR, starsystemfile );

   if( ( fp = fopen( filename, "r" ) ) != NULL )
   {
      found = TRUE;
      LINK( starsystem, first_starsystem, last_starsystem, next, prev );
      for( ;; )
      {
         char letter;
         const char *word;

         letter = fread_letter( fp );
         if( letter == '*' )
         {
            fread_to_eol( fp );
            continue;
         }

         if( letter != '#' )
         {
            bug( "%s: # not found.", __func__ );
            break;
         }

         word = fread_word( fp );
         if( !str_cmp( word, "SPACE" ) )
         {
            fread_starsystem( starsystem, fp );
            break;
         }
         else if( !str_cmp( word, "END" ) )
            break;
         else
         {
            bug( "%s: bad section: %s.", __func__, word );
            break;
         }
      }
      FCLOSE( fp );
   }

   if( !( found ) )
      DISPOSE( starsystem );

   return found;
}

/*
 * Load in all the starsystem files.
 */
void load_space(  )
{
   FILE *fpList;
   const char *filename;
   char starsystemlist[256];

   first_starsystem = NULL;
   last_starsystem = NULL;

   log_string( "Loading space..." );

   snprintf( starsystemlist, 256, "%s%s", SPACE_DIR, SPACE_LIST );
   if( ( fpList = fopen( starsystemlist, "r" ) ) == NULL )
   {
      perror( starsystemlist );
      exit( 1 );
   }

   for( ;; )
   {
      filename = feof( fpList ) ? "$" : fread_word( fpList );
      if( filename[0] == '$' )
         break;

      if( !load_starsystem( filename ) )
      {
         bug( "%s: Cannot load starsystem file: %s", __func__, filename );
      }
   }
   FCLOSE( fpList );
   log_string( " Done starsystems " );
}

void do_setstarsystem( CHAR_DATA * ch, const char *argument )
{
   char arg1[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   SPACE_DATA *starsystem;

   if( IS_NPC( ch ) )
   {
      send_to_char( "Huh?\r\n", ch );
      return;
   }

   argument = one_argument( argument, arg1 );
   argument = one_argument( argument, arg2 );

   if( arg2[0] == '\0' || arg1[0] == '\0' )
   {
      send_to_char( "Usage: setstarsystem <starsystem> <field> <values>\r\n", ch );
      send_to_char( "\r\nField being one of:\r\n", ch );
      send_to_char( "name filename xpos ypos,\r\n", ch );
      send_to_char( "star1 s1x s1y s1z gravitys1\r\n", ch );
      send_to_char( "star2 s2x s2y s2z gravitys2\r\n", ch );
      send_to_char( "planet1 p1x p1y p1z gravityp1\r\n", ch );
      send_to_char( "planet2 p2x p2y p2z gravityp2\r\n", ch );
      send_to_char( "planet3 p3x p3y p3z gravityp3\r\n", ch );
      send_to_char( "location1a location1b location1c doc1a doc1b doc1c\r\n", ch );
      send_to_char( "location2a location2b location2c doc2a doc2b doc2c\r\n", ch );
      send_to_char( "location3a location3b location3c doc3a doc3b doc3c\r\n", ch );
      send_to_char( "", ch );
      return;
   }

   starsystem = starsystem_from_name( arg1 );
   if( !starsystem )
   {
      send_to_char( "No such starsystem.\r\n", ch );
      return;
   }

   if( !str_cmp( arg2, "doc1a" ) )
   {
      starsystem->doc1a = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "doc1b" ) )
   {
      starsystem->doc1b = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "doc1c" ) )
   {
      starsystem->doc1c = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "doc2a" ) )
   {
      starsystem->doc2a = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "doc2b" ) )
   {
      starsystem->doc2b = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "doc2c" ) )
   {
      starsystem->doc2c = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "doc3a" ) )
   {
      starsystem->doc3a = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "doc3b" ) )
   {
      starsystem->doc3b = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "doc3c" ) )
   {
      starsystem->doc3c = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "s1x" ) )
   {
      starsystem->s1x = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "s1y" ) )
   {
      starsystem->s1y = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "s1z" ) )
   {
      starsystem->s1z = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "s2x" ) )
   {
      starsystem->s2x = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "s2y" ) )
   {
      starsystem->s2y = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "s2z" ) )
   {
      starsystem->s2z = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "p1x" ) )
   {
      starsystem->p1x = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "p1y" ) )
   {
      starsystem->p1y = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "p1z" ) )
   {
      starsystem->p1z = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "p2x" ) )
   {
      starsystem->p2x = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "p2y" ) )
   {
      starsystem->p2y = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "p2z" ) )
   {
      starsystem->p2z = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "p3x" ) )
   {
      starsystem->p3x = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "p3y" ) )
   {
      starsystem->p3y = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "p3z" ) )
   {
      starsystem->p3z = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "xpos" ) )
   {
      starsystem->xpos = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "ypos" ) )
   {
      starsystem->ypos = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "gravitys1" ) )
   {
      starsystem->gravitys1 = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "gravitys2" ) )
   {
      starsystem->gravitys2 = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "gravityp1" ) )
   {
      starsystem->gravityp1 = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "gravityp2" ) )
   {
      starsystem->gravityp2 = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "gravityp3" ) )
   {
      starsystem->gravityp3 = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "name" ) )
   {
      STRFREE( starsystem->name );
      starsystem->name = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "star1" ) )
   {
      STRFREE( starsystem->star1 );
      starsystem->star1 = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "star2" ) )
   {
      STRFREE( starsystem->star2 );
      starsystem->star2 = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "planet1" ) )
   {
      STRFREE( starsystem->planet1 );
      starsystem->planet1 = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "planet2" ) )
   {
      STRFREE( starsystem->planet2 );
      starsystem->planet2 = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "planet3" ) )
   {
      STRFREE( starsystem->planet3 );
      starsystem->planet3 = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "location1a" ) )
   {
      STRFREE( starsystem->location1a );
      starsystem->location1a = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "location1b" ) )
   {
      STRFREE( starsystem->location1b );
      starsystem->location1b = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "location1c" ) )
   {
      STRFREE( starsystem->location1c );
      starsystem->location1c = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "location2a" ) )
   {
      STRFREE( starsystem->location2a );
      starsystem->location2a = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "location2b" ) )
   {
      STRFREE( starsystem->location2a );
      starsystem->location2b = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "location2c" ) )
   {
      STRFREE( starsystem->location2c );
      starsystem->location2c = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   if( !str_cmp( arg2, "location3a" ) )
   {
      STRFREE( starsystem->location3a );
      starsystem->location3a = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "location3b" ) )
   {
      STRFREE( starsystem->location3b );
      starsystem->location3b = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }
   if( !str_cmp( arg2, "location3c" ) )
   {
      STRFREE( starsystem->location3c );
      starsystem->location3c = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_starsystem( starsystem );
      return;
   }

   do_setstarsystem( ch, "" );
}

void showstarsystem( CHAR_DATA * ch, SPACE_DATA * starsystem )
{
   ch_printf( ch, "Starsystem:%s     Filename: %s    Xpos: %d   Ypos: %d\r\n",
              starsystem->name, starsystem->filename, starsystem->xpos, starsystem->ypos );
   ch_printf( ch, "Star1: %s   Gravity: %d   Coordinates: %d %d %d\r\n",
              starsystem->star1, starsystem->gravitys1, starsystem->s1x, starsystem->s1y, starsystem->s1z );
   ch_printf( ch, "Star2: %s   Gravity: %d   Coordinates: %d %d %d\r\n",
              starsystem->star2, starsystem->gravitys2, starsystem->s2x, starsystem->s2y, starsystem->s2z );
   ch_printf( ch, "Planet1: %s   Gravity: %d   Coordinates: %d %d %d\r\n",
              starsystem->planet1, starsystem->gravityp1, starsystem->p1x, starsystem->p1y, starsystem->p1z );
   ch_printf( ch, "     Doc1a: %5d (%s)\r\n", starsystem->doc1a, starsystem->location1a );
   ch_printf( ch, "     Doc1b: %5d (%s)\r\n", starsystem->doc1b, starsystem->location1b );
   ch_printf( ch, "     Doc1c: %5d (%s)\r\n", starsystem->doc1c, starsystem->location1c );
   ch_printf( ch, "Planet2: %s   Gravity: %d   Coordinates: %d %d %d\r\n",
              starsystem->planet2, starsystem->gravityp2, starsystem->p2x, starsystem->p2y, starsystem->p2z );
   ch_printf( ch, "     Doc2a: %5d (%s)\r\n", starsystem->doc2a, starsystem->location2a );
   ch_printf( ch, "     Doc2b: %5d (%s)\r\n", starsystem->doc2b, starsystem->location2b );
   ch_printf( ch, "     Doc2c: %5d (%s)\r\n", starsystem->doc2c, starsystem->location2c );
   ch_printf( ch, "Planet3: %s   Gravity: %d   Coordinates: %d %d %d\r\n",
              starsystem->planet3, starsystem->gravityp3, starsystem->p3x, starsystem->p3y, starsystem->p3z );
   ch_printf( ch, "     Doc3a: %5d (%s)\r\n", starsystem->doc3a, starsystem->location3a );
   ch_printf( ch, "     Doc3b: %5d (%s)\r\n", starsystem->doc3b, starsystem->location3b );
   ch_printf( ch, "     Doc3c: %5d (%s)\r\n", starsystem->doc3c, starsystem->location3c );
}

void do_showstarsystem( CHAR_DATA * ch, const char *argument )
{
   SPACE_DATA *starsystem;

   starsystem = starsystem_from_name( argument );

   if( starsystem == NULL )
      send_to_char( "&RNo such starsystem.\r\n", ch );
   else
      showstarsystem( ch, starsystem );
}

void do_makestarsystem( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   char filename[256];
   SPACE_DATA *starsystem;

   if( !argument || argument[0] == '\0' )
   {
      send_to_char( "Usage: makestarsystem <starsystem name>\r\n", ch );
      return;
   }

   CREATE( starsystem, SPACE_DATA, 1 );
   LINK( starsystem, first_starsystem, last_starsystem, next, prev );

   starsystem->name = STRALLOC( argument );

   starsystem->location1a = STRALLOC( "" );
   starsystem->location2a = STRALLOC( "" );
   starsystem->location3a = STRALLOC( "" );
   starsystem->location1b = STRALLOC( "" );
   starsystem->location2b = STRALLOC( "" );
   starsystem->location3b = STRALLOC( "" );
   starsystem->location1c = STRALLOC( "" );
   starsystem->location2c = STRALLOC( "" );
   starsystem->location3c = STRALLOC( "" );
   starsystem->planet1 = STRALLOC( "" );
   starsystem->planet2 = STRALLOC( "" );
   starsystem->planet3 = STRALLOC( "" );
   starsystem->star1 = STRALLOC( "" );
   starsystem->star2 = STRALLOC( "" );

   argument = one_argument( argument, arg );
   snprintf( filename, 256, "%s.system", strlower( arg ) );
   starsystem->filename = strdup( filename );
   save_starsystem( starsystem );
   write_starsystem_list(  );
}

void do_starsystems( CHAR_DATA * ch, const char *argument )
{
   SPACE_DATA *starsystem;
   int count = 0;

   for( starsystem = first_starsystem; starsystem; starsystem = starsystem->next )
   {
      set_char_color( AT_NOTE, ch );
      ch_printf( ch, "%s\r\n", starsystem->name );
      count++;
   }

   if( !count )
   {
      send_to_char( "There are no starsystems currently formed.\r\n", ch );
      return;
   }
}

void echo_to_ship( int color, SHIP_DATA * ship, const char *argument )
{
   int room;

   for( room = ship->firstroom; room <= ship->lastroom; room++ )
   {
      echo_to_room( color, get_room_index( room ), argument );
   }
}

void sound_to_ship( SHIP_DATA * ship, const char *argument )
{
   int roomnum;
   ROOM_INDEX_DATA *room;
   CHAR_DATA *vic;

   for( roomnum = ship->firstroom; roomnum <= ship->lastroom; roomnum++ )
   {
      room = get_room_index( roomnum );
      if( room == NULL )
         continue;

      for( vic = room->first_person; vic; vic = vic->next_in_room )
      {
         if( !IS_NPC( vic ) && IS_SET( vic->act, PLR_SOUND ) )
            send_to_char( argument, vic );
      }
   }
}

void echo_to_cockpit( int color, SHIP_DATA * ship, const char *argument )
{
   int room;

   for( room = ship->firstroom; room <= ship->lastroom; room++ )
   {
      if( room == ship->cockpit || room == ship->navseat
          || room == ship->pilotseat || room == ship->coseat
          || room == ship->gunseat || room == ship->engineroom || room == ship->turret1 || room == ship->turret2 )
         echo_to_room( color, get_room_index( room ), argument );
   }
}

void echo_to_system( int color, SHIP_DATA * ship, const char *argument,
		     SHIP_DATA * ignore )
{
   SHIP_DATA *target;

   if( !ship->starsystem )
      return;

   for( target = ship->starsystem->first_ship; target; target = target->next_in_starsystem )
   {
      if( target != ship && target != ignore )
         echo_to_cockpit( color, target, argument );
   }
}

bool is_facing( SHIP_DATA * ship, SHIP_DATA * target )
{
   float dy, dx, dz, hx, hy, hz;
   float cosofa;

   hx = ship->hx;
   hy = ship->hy;
   hz = ship->hz;

   dx = target->vx - ship->vx;
   dy = target->vy - ship->vy;
   dz = target->vz - ship->vz;

   cosofa = ( hx * dx + hy * dy + hz * dz ) / ( sqrt( hx * hx + hy * hy + hz * hz ) + sqrt( dx * dx + dy * dy + dz * dz ) );

   if( cosofa > 0.75 )
      return TRUE;

   return FALSE;
}

long int get_ship_value( SHIP_DATA * ship )
{
   long int price;

   if( ship->ship_class == FIGHTER_SHIP )
      price = 5000;
   else if( ship->ship_class == MIDSIZE_SHIP )
      price = 50000;
   else if( ship->ship_class == FRIGATE_SHIP )
      price = 100000;   /* v1.22: was falling through to 2000 (SWGD: 100000) */
   else if( ship->ship_class == CAPITAL_SHIP )
      price = 500000;
   else if( ship->ship_class == SUPERCAPITAL_SHIP )
      price = 1000000;
   else if( ship->ship_class == SHIP_PLATFORM )
      price = 500000;
   else
      price = 2000;

   if( ship->ship_class <= CAPITAL_SHIP )
      price += ( ship->manuever * 100 * ( 1 + ship->ship_class ) );

   price += ( ship->tractorbeam * 100 );
   price += ( ship->armor * 50 );      /* v1.22b: SWGD armor pricing */
   price += ( ship->maxarmor * 70 );
   price += ( ship->realspeed * 10 );
   price += ( ship->astro_array * 5 );
   price += ( 5 * ship->maxhull );
   price += ( 2 * ship->maxenergy );
   price += ( 100 * ship->maxchaff );

   if( ship->maxenergy > 5000 )
      price += ( ( ship->maxenergy - 5000 ) * 20 );

   if( ship->maxenergy > 10000 )
      price += ( ( ship->maxenergy - 10000 ) * 50 );

   if( ship->maxhull > 1000 )
      price += ( ( ship->maxhull - 1000 ) * 10 );

   if( ship->maxhull > 10000 )
      price += ( ( ship->maxhull - 10000 ) * 20 );

   if( ship->maxshield > 200 )
      price += ( ( ship->maxshield - 200 ) * 50 );

   if( ship->maxshield > 1000 )
      price += ( ( ship->maxshield - 1000 ) * 100 );

   if( ship->realspeed > 100 )
      price += ( ( ship->realspeed - 100 ) * 500 );

   if( ship->lasers > 5 )
      price += ( ( ship->lasers - 5 ) * 500 );

   if( ship->maxshield )
      price += ( 1000 + 10 * ship->maxshield );

   if( ship->lasers )
      price += ( 500 + 500 * ship->lasers );

   if( ship->maxmissiles )
      price += ( 1000 + 100 * ship->maxmissiles );
   if( ship->maxrockets )
      price += ( 2000 + 200 * ship->maxmissiles );
   if( ship->maxtorpedos )
      price += ( 1500 + 150 * ship->maxmissiles );

   if( ship->missiles )
      price += ( 250 * ship->missiles );
   else if( ship->torpedos )
      price += ( 500 * ship->torpedos );
   else if( ship->rockets )
      price += ( 1000 * ship->rockets );

   if( ship->turret1 )
      price += 5000;

   if( ship->turret2 )
      price += 5000;

   if( ship->hyperspeed )
      price += ( 1000 + ship->hyperspeed * 10 );

   if( ship->hanger )
      price += ( ship->ship_class == MIDSIZE_SHIP ? 50000 : 100000 );

   price = ( long )( price * 1.5 );

   return price;
}

void write_ship_list(  )
{
   SHIP_DATA *tship;
   FILE *fpout;
   char filename[256];

   snprintf( filename, 256, "%s%s", SHIP_DIR, SHIP_LIST );
   fpout = fopen( filename, "w" );
   if( !fpout )
   {
      bug( "FATAL: %s: cannot open ship.lst for writing!\r\n", __func__ );
      return;
   }
   for( tship = first_ship; tship; tship = tship->next )
      fprintf( fpout, "%s\n", tship->filename );
   fprintf( fpout, "$\n" );
   FCLOSE( fpout );
}

SHIP_DATA *ship_in_room( ROOM_INDEX_DATA * room, const char *name )
{
   SHIP_DATA *ship;

   if( !room )
      return NULL;

   for( ship = room->first_ship; ship; ship = ship->next_in_room )
      if( !str_cmp( name, ship->name ) )
         return ship;

   for( ship = room->first_ship; ship; ship = ship->next_in_room )
      if( nifty_is_name_prefix( name, ship->name ) )
         return ship;

   return NULL;
}

/*
 * Get pointer to ship structure from ship name.
 */
SHIP_DATA *get_ship( const char *name )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( !str_cmp( name, ship->name ) )
         return ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( nifty_is_name_prefix( name, ship->name ) )
         return ship;

   return NULL;
}

/*
 * Checks if ships in a starsystem and returns poiner if it is.
 */
SHIP_DATA *get_ship_here( const char *name, SPACE_DATA * starsystem )
{
   SHIP_DATA *ship;

   if( starsystem == NULL )
      return NULL;

   for( ship = starsystem->first_ship; ship; ship = ship->next_in_starsystem )
      if( !str_cmp( name, ship->name ) )
         return ship;

   for( ship = starsystem->first_ship; ship; ship = ship->next_in_starsystem )
      if( nifty_is_name_prefix( name, ship->name ) )
         return ship;

   return NULL;
}

/*
 * Get pointer to ship structure from ship name.
 */
SHIP_DATA *ship_from_pilot( const char *name )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( !str_cmp( name, ship->pilot ) )
         return ship;
   if( !str_cmp( name, ship->copilot ) )
      return ship;
   if( !str_cmp( name, ship->owner ) )
      return ship;
   return NULL;
}

/*
 * Get pointer to ship structure from cockpit, turret, or entrance ramp vnum.
 */

SHIP_DATA *ship_from_cockpit( int vnum )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( vnum == ship->cockpit || vnum == ship->turret1 || vnum == ship->turret2
          || vnum == ship->pilotseat || vnum == ship->coseat || vnum == ship->navseat
          || vnum == ship->gunseat || vnum == ship->engineroom )
         return ship;
   return NULL;
}

SHIP_DATA *ship_from_pilotseat( int vnum )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( vnum == ship->pilotseat )
         return ship;
   return NULL;
}

/* Returns the PC currently sitting in the pilotseat of a ship */
CHAR_DATA *pilot_from_ship( SHIP_DATA * ship )
{
   CHAR_DATA *wch;

   for( wch = first_char; wch; wch = wch->next )
      if( wch->in_room->vnum == ship->pilotseat && !IS_NPC( wch ) )
         return wch;
   return NULL;
}

/* Did the target pilot evade the incoming attack? -- Arcturus / SWL */
bool evaded( SHIP_DATA * ship, SHIP_DATA * target )
{
   CHAR_DATA *victim;
   int echance;

   if( ( victim = pilot_from_ship( target ) ) == NULL )
      return FALSE;   /* No pilot found, can't evade */

   if( target->ship_class > MIDSIZE_SHIP )
      return FALSE;   /* Capital ships can't evade */

   echance = IS_NPC( victim ) ? 10 : victim->pcdata->learned[gsn_evade] / 3 + 10;

   if( victim->subclass == SUBCLASS_WFOCUS )
      echance += 5;

   if( ship->ship_class > MIDSIZE_SHIP )
      echance += ship->ship_class;

   if( number_percent(  ) < echance )
   {
      if( !IS_NPC( victim ) )
         learn_from_success( victim, gsn_evade );
      return TRUE;
   }

   if( !IS_NPC( victim ) )
      learn_from_failure( victim, gsn_evade );
   return FALSE;
}

SHIP_DATA *ship_from_coseat( int vnum )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( vnum == ship->coseat )
         return ship;
   return NULL;
}

SHIP_DATA *ship_from_navseat( int vnum )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( vnum == ship->navseat )
         return ship;
   return NULL;
}

SHIP_DATA *ship_from_gunseat( int vnum )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( vnum == ship->gunseat )
         return ship;
   return NULL;
}

SHIP_DATA *ship_from_engine( int vnum )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
   {
      if( ship->engineroom )
      {
         if( vnum == ship->engineroom )
            return ship;
      }
      else
      {
         if( vnum == ship->cockpit )
            return ship;
      }
   }

   return NULL;
}


SHIP_DATA *ship_from_turret( int vnum )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( vnum == ship->gunseat || vnum == ship->turret1 || vnum == ship->turret2 )
         return ship;
   return NULL;
}

SHIP_DATA *ship_from_entrance( int vnum )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( vnum == ship->entrance )
         return ship;
   return NULL;
}

SHIP_DATA *ship_from_hanger( int vnum )
{
   SHIP_DATA *ship;

   for( ship = first_ship; ship; ship = ship->next )
      if( vnum == ship->hanger )
         return ship;
   return NULL;
}

void save_ship( SHIP_DATA * ship )
{
   FILE *fp;
   char filename[256];

   if( !ship )
   {
      bug( "%s: null ship pointer!", __func__ );
      return;
   }

   if( !ship->filename || ship->filename[0] == '\0' )
   {
      bug( "%s: %s has no filename", __func__, ship->name );
      return;
   }

   snprintf( filename, 256, "%s%s", SHIP_DIR, ship->filename );

   if( ( fp = fopen( filename, "w" ) ) == NULL )
   {
      bug( "%s: fopen", __func__ );
      perror( filename );
   }
   else
   {
      fprintf( fp, "#SHIP\n" );
      fprintf( fp, "SVers        2\n" );  /* v1.22: 2 = post-SUPERCAPITAL enum */
      fprintf( fp, "Name         %s~\n", ship->name );
      fprintf( fp, "Filename     %s~\n", ship->filename );
      fprintf( fp, "Description  %s~\n", ship->description );
      fprintf( fp, "Owner        %s~\n", ship->owner );
      fprintf( fp, "Pilot        %s~\n", ship->pilot );
      fprintf( fp, "Copilot      %s~\n", ship->copilot );
      fprintf( fp, "Class        %d\n", ship->ship_class );
      fprintf( fp, "Tractorbeam  %d\n", ship->tractorbeam );
      fprintf( fp, "Shipyard     %d\n", ship->shipyard );
      fprintf( fp, "Hanger       %d\n", ship->hanger );
      fprintf( fp, "Turret1      %d\n", ship->turret1 );
      fprintf( fp, "Turret2      %d\n", ship->turret2 );
      fprintf( fp, "Statet0      %d\n", ship->statet0 );
      fprintf( fp, "Statet1      %d\n", ship->statet1 );
      fprintf( fp, "Statet2      %d\n", ship->statet2 );
      fprintf( fp, "Ionstate     %d\n", ship->ionstate );
      fprintf( fp, "Lasers       %d\n", ship->lasers );
      fprintf( fp, "Missiles     %d\n", ship->missiles );
      fprintf( fp, "Maxmissiles  %d\n", ship->maxmissiles );
      fprintf( fp, "Rockets     %d\n", ship->rockets );
      fprintf( fp, "Maxrockets  %d\n", ship->maxrockets );
      fprintf( fp, "Torpedos     %d\n", ship->torpedos );
      fprintf( fp, "Maxtorpedos  %d\n", ship->maxtorpedos );
      fprintf( fp, "Lastdoc      %d\n", ship->lastdoc );
      fprintf( fp, "Firstroom    %d\n", ship->firstroom );
      fprintf( fp, "Lastroom     %d\n", ship->lastroom );
      fprintf( fp, "Shield       %d\n", ship->shield );
      fprintf( fp, "Maxshield    %d\n", ship->maxshield );
      fprintf( fp, "Hull         %d\n", ship->hull );
      fprintf( fp, "Maxhull      %d\n", ship->maxhull );
      fprintf( fp, "Maxenergy    %d\n", ship->maxenergy );
      fprintf( fp, "Hyperspeed   %d\n", ship->hyperspeed );
      fprintf( fp, "Comm         %d\n", ship->comm );
      fprintf( fp, "Chaff        %d\n", ship->chaff );
      fprintf( fp, "Maxchaff     %d\n", ship->maxchaff );
      fprintf( fp, "Sensor       %d\n", ship->sensor );
      fprintf( fp, "Astro_array  %d\n", ship->astro_array );
      fprintf( fp, "Realspeed    %d\n", ship->realspeed );
      fprintf( fp, "Ions         %d\n", ship->ions );
      fprintf( fp, "Armor        %d\n", ship->armor );
      fprintf( fp, "Maxarmor     %d\n", ship->maxarmor );
      fprintf( fp, "Autocannon   %d\n", ship->autocannon );
      fprintf( fp, "Autodamage   %d\n", ship->autodamage );
      fprintf( fp, "Autoammo     %d\n", ship->autoammomax );
      fprintf( fp, "Cargo        %d\n", ship->cargo );
      fprintf( fp, "Cargotype    %d\n", ship->cargotype );
      fprintf( fp, "Maxcargo     %d\n", ship->maxcargo );
      fprintf( fp, "Laserdamage  %d\n", ship->laserdamage );
      fprintf( fp, "Type         %d\n", ship->type );
      fprintf( fp, "Cockpit      %d\n", ship->cockpit );
      fprintf( fp, "Coseat       %d\n", ship->coseat );
      fprintf( fp, "Pilotseat    %d\n", ship->pilotseat );
      fprintf( fp, "Gunseat      %d\n", ship->gunseat );
      fprintf( fp, "Navseat      %d\n", ship->navseat );
      fprintf( fp, "Engineroom   %d\n", ship->engineroom );
      fprintf( fp, "Entrance     %d\n", ship->entrance );
      fprintf( fp, "Shipstate    %d\n", ship->shipstate );
      fprintf( fp, "Missilestate %d\n", ship->missilestate );
      fprintf( fp, "Torpedostate %d\n", ship->torpedostate );
      fprintf( fp, "Rocketstate  %d\n", ship->rocketstate );
      fprintf( fp, "Energy       %d\n", ship->energy );
      fprintf( fp, "Manuever     %d\n", ship->manuever );
      fprintf( fp, "Home         %s~\n", ship->home );
      fprintf( fp, "MaxModules   %d\n", ship->maxmodules );
      if( ship->modules > 0 )
      {
         int module;

         for( module = 1; module <= ship->modules; module++ )
            fprintf( fp, "Module       %d\n", ship->module_vnum[module] );
      }
      fprintf( fp, "End\n\n" );
      fprintf( fp, "#END\n" );
      FCLOSE( fp );
   }
}

/*
 * Read in actual ship data.
 */
void fread_ship( SHIP_DATA * ship, FILE * fp )
{
   const char *word;
   bool fMatch;
   int dummy_number __attribute__((unused));
   int module_counter = 1;
   int file_vers = 1;   /* v1.22: files without an SVers key predate SUPERCAPITAL_SHIP */

   ship->modules = 0;

   for( ;; )
   {
      word = feof( fp ) ? "End" : fread_word( fp );
      fMatch = FALSE;

      switch ( UPPER( word[0] ) )
      {
         case '*':
            fMatch = TRUE;
            fread_to_eol( fp );
            break;

         case 'A':
            KEY( "Astro_array", ship->astro_array, fread_number( fp ) );
            KEY( "Armor", ship->armor, fread_number( fp ) );
            KEY( "Autocannon", ship->autocannon, fread_number( fp ) );
            KEY( "Autodamage", ship->autodamage, fread_number( fp ) );
            KEY( "Autoammo", ship->autoammomax, fread_number( fp ) );
            break;

         case 'C':
            KEY( "Cockpit", ship->cockpit, fread_number( fp ) );
            KEY( "Coseat", ship->coseat, fread_number( fp ) );
            KEY( "Class", ship->ship_class, fread_number( fp ) );
            KEY( "Copilot", ship->copilot, fread_string( fp ) );
            KEY( "Cargo", ship->cargo, fread_number( fp ) );
            KEY( "Cargotype", ship->cargotype, fread_number( fp ) );
            KEY( "Comm", ship->comm, fread_number( fp ) );
            KEY( "Chaff", ship->chaff, fread_number( fp ) );
            break;

         case 'D':
            KEY( "Description", ship->description, fread_string( fp ) );
            break;

         case 'E':
            KEY( "Engineroom", ship->engineroom, fread_number( fp ) );
            KEY( "Entrance", ship->entrance, fread_number( fp ) );
            KEY( "Energy", ship->energy, fread_number( fp ) );
            if( !str_cmp( word, "End" ) )
            {
               /* v1.22 migration: SUPERCAPITAL_SHIP was inserted into the
                * enum after CAPITAL_SHIP, shifting SHIP_PLATFORM..WALKER
                * up by one. Pre-SVers-2 files stored the old numbering. */
               if( !ship->autocannon )
                  ship->autocannon = 0;
               if( !ship->autoammomax )
                  ship->autoammomax = 0;
               if( !ship->autodamage )
                  ship->autodamage = 0;
               ship->autoammo = ship->autoammomax;
               if( file_vers < 2 && ship->ship_class >= SUPERCAPITAL_SHIP )
               {
                  ship->ship_class++;
                  log_printf( "SVers migration: %s class %d -> %d",
                              ship->name ? ship->name : "(unnamed)",
                              ship->ship_class - 1, ship->ship_class );
               }
               if( !ship->home )
                  ship->home = STRALLOC( "" );
               if( !ship->name )
                  ship->name = STRALLOC( "" );
               if( !ship->owner )
                  ship->owner = STRALLOC( "" );
               if( !ship->description )
                  ship->description = STRALLOC( "" );
               if( !ship->copilot )
                  ship->copilot = STRALLOC( "" );
               if( !ship->pilot )
                  ship->pilot = STRALLOC( "" );
               if( ship->shipstate != SHIP_DISABLED )
                  ship->shipstate = SHIP_DOCKED;
               if( ship->statet0 != LASER_DAMAGED )
                  ship->statet0 = LASER_READY;
               if( ship->statet1 != LASER_DAMAGED )
                  ship->statet1 = LASER_READY;
               if( ship->statet2 != LASER_DAMAGED )
                  ship->statet2 = LASER_READY;
               if( ship->missilestate != MISSILE_DAMAGED )
                  ship->missilestate = MISSILE_READY;
               if( ship->torpedostate != MISSILE_DAMAGED )
                  ship->torpedostate = MISSILE_READY;
               if( ship->rocketstate != MISSILE_DAMAGED )
                  ship->rocketstate = MISSILE_READY;
               if( ship->shipyard <= 0 )
                  ship->shipyard = ROOM_LIMBO_SHIPYARD;
               if( ship->lastdoc <= 0 )
                  ship->lastdoc = ship->shipyard;
               ship->bayopen = TRUE;
               ship->autopilot = FALSE;
               ship->hatchopen = FALSE;
               if( ship->navseat <= 0 )
                  ship->navseat = ship->cockpit;
               if( ship->gunseat <= 0 )
                  ship->gunseat = ship->cockpit;
               if( ship->coseat <= 0 )
                  ship->coseat = ship->cockpit;
               if( ship->pilotseat <= 0 )
                  ship->pilotseat = ship->cockpit;
               if( ship->missiletype == 1 )
               {
                  ship->torpedos = ship->missiles; /* for back compatability */
                  ship->missiles = 0;
               }
               ship->starsystem = NULL;
               ship->energy = ship->maxenergy;
               ship->hull = ship->maxhull;
               ship->in_room = NULL;
               ship->next_in_room = NULL;
               ship->prev_in_room = NULL;

               return;
            }
            break;

         case 'F':
            KEY( "Filename", ship->filename, fread_string_nohash( fp ) );
            KEY( "Firstroom", ship->firstroom, fread_number( fp ) );
            break;

         case 'G':
            KEY( "Gunseat", ship->gunseat, fread_number( fp ) );
            break;

         case 'H':
            KEY( "Home", ship->home, fread_string( fp ) );
            KEY( "Hyperspeed", ship->hyperspeed, fread_number( fp ) );
            KEY( "Hull", ship->hull, fread_number( fp ) );
            KEY( "Hanger", ship->hanger, fread_number( fp ) );
            break;

         case 'I':
            KEY( "Ions", ship->ions, fread_number( fp ) );
            KEY( "Ionstate", ship->ionstate, fread_number( fp ) );
            break;

         case 'L':
            KEY( "Laserstr", ship->lasers, ( short )( fread_number( fp ) / 10 ) );
            KEY( "Lasers", ship->lasers, fread_number( fp ) );
            KEY( "Laserdamage", ship->laserdamage, fread_number( fp ) );
            KEY( "Lastdoc", ship->lastdoc, fread_number( fp ) );
            KEY( "Lastroom", ship->lastroom, fread_number( fp ) );
            break;

         case 'M':
            KEY( "Manuever", ship->manuever, fread_number( fp ) );
            KEY( "Maxmissiles", ship->maxmissiles, fread_number( fp ) );
            KEY( "Maxtorpedos", ship->maxtorpedos, fread_number( fp ) );
            KEY( "Maxrockets", ship->maxrockets, fread_number( fp ) );
            KEY( "Maxarmor", ship->maxarmor, fread_number( fp ) );
            KEY( "Maxcargo", ship->maxcargo, fread_number( fp ) );
            KEY( "Missiles", ship->missiles, fread_number( fp ) );
            KEY( "Missiletype", ship->missiletype, fread_number( fp ) );
            KEY( "Maxshield", ship->maxshield, fread_number( fp ) );
            KEY( "Maxenergy", ship->maxenergy, fread_number( fp ) );
            KEY( "Missilestate", ship->missilestate, fread_number( fp ) );
            KEY( "Maxhull", ship->maxhull, fread_number( fp ) );
            KEY( "Maxchaff", ship->maxchaff, fread_number( fp ) );
            KEY( "MaxModules", ship->maxmodules, fread_number( fp ) );
            if( !str_cmp( word, "Module" ) )
            {
               if( module_counter <= MAX_SHIP_MODULES )
               {
                  ship->module_vnum[module_counter] = fread_number( fp );
                  ship->modules = module_counter;
                  module_counter++;
                  /* v1.23 SWGD verbatim: rebuild per module line read, so
                   * modularized ships' stats come from components; ships
                   * with no Module lines keep their file stats. */
                  update_ship_modules( ship );
               }
               else
               {
                  bug( "%s: too many modules on ship, discarding extra.", __func__ );
                  fread_number( fp );
               }
               fMatch = TRUE;
               break;
            }
            break;

         case 'N':
            KEY( "Name", ship->name, fread_string( fp ) );
            KEY( "Navseat", ship->navseat, fread_number( fp ) );
            break;

         case 'O':
            KEY( "Owner", ship->owner, fread_string( fp ) );
            KEY( "Objectnum", dummy_number, fread_number( fp ) );
            break;

         case 'P':
            KEY( "Pilot", ship->pilot, fread_string( fp ) );
            KEY( "Pilotseat", ship->pilotseat, fread_number( fp ) );
            break;

         case 'R':
            KEY( "Realspeed", ship->realspeed, fread_number( fp ) );
            KEY( "Rocketstate", ship->rocketstate, fread_number( fp ) );
            KEY( "Rockets", ship->rockets, fread_number( fp ) );
            break;

         case 'S':
            KEY( "SVers", file_vers, fread_number( fp ) );
            KEY( "Shipyard", ship->shipyard, fread_number( fp ) );
            KEY( "Sensor", ship->sensor, fread_number( fp ) );
            KEY( "Shield", ship->shield, fread_number( fp ) );
            KEY( "Shipstate", ship->shipstate, fread_number( fp ) );
            KEY( "Statet0", ship->statet0, fread_number( fp ) );
            KEY( "Statet1", ship->statet1, fread_number( fp ) );
            KEY( "Statet2", ship->statet2, fread_number( fp ) );
            break;

         case 'T':
            KEY( "Type", ship->type, fread_number( fp ) );
            KEY( "Tractorbeam", ship->tractorbeam, fread_number( fp ) );
            KEY( "Torpedostate", ship->torpedostate, fread_number( fp ) );
            KEY( "Turret1", ship->turret1, fread_number( fp ) );
            KEY( "Turret2", ship->turret2, fread_number( fp ) );
            KEY( "Torpedos", ship->torpedos, fread_number( fp ) );
            break;
      }

      if( !fMatch )
      {
         bug( "%s: no match: %s", __func__, word );
      }
   }
}

/*
 * Load a ship file
 */
bool load_ship_file( const char *shipfile )
{
   char filename[256];
   SHIP_DATA *ship;
   FILE *fp;
   bool found;
   ROOM_INDEX_DATA *pRoomIndex;
   CLAN_DATA *clan;

   CREATE( ship, SHIP_DATA, 1 );

   found = FALSE;
   snprintf( filename, 256, "%s%s", SHIP_DIR, shipfile );

   if( ( fp = fopen( filename, "r" ) ) != NULL )
   {
      found = TRUE;
      for( ;; )
      {
         char letter;
         const char *word;

         letter = fread_letter( fp );
         if( letter == '*' )
         {
            fread_to_eol( fp );
            continue;
         }

         if( letter != '#' )
         {
            bug( "%s: # not found.", __func__ );
            break;
         }

         word = fread_word( fp );
         if( !str_cmp( word, "SHIP" ) )
         {
            fread_ship( ship, fp );
            break;
         }
         else if( !str_cmp( word, "END" ) )
            break;
         else
         {
            bug( "%s: bad section: %s.", __func__, word );
            break;
         }
      }
      FCLOSE( fp );
   }

   if( !( found ) )
      DISPOSE( ship );
   else
   {
      LINK( ship, first_ship, last_ship, next, prev );
      if( !str_cmp( "Public", ship->owner ) || ship->type == MOB_SHIP )
      {
         if( ship->ship_class != SHIP_PLATFORM && ship->type != MOB_SHIP && ship->ship_class != CAPITAL_SHIP
             && ship->ship_class != SUPERCAPITAL_SHIP && ship->ship_class != FRIGATE_SHIP )
         {
            extract_ship( ship );
            ship_to_room( ship, ship->shipyard );

            ship->location = ship->shipyard;
            ship->lastdoc = ship->shipyard;
            ship->shipstate = SHIP_DOCKED;
         }

         ship->currspeed = 0;
         ship->energy = ship->maxenergy;
         ship->chaff = ship->maxchaff;
         ship->hull = ship->maxhull;
         ship->shield = 0;

         ship->ionstate = LASER_READY;
         ship->statet1 = LASER_READY;
         ship->statet2 = LASER_READY;
         ship->statet0 = LASER_READY;
         ship->missilestate = MISSILE_READY;
         ship->torpedostate = MISSILE_READY;
         ship->rocketstate = MISSILE_READY;

         ship->currjump = NULL;
         ship->target0 = NULL;
         ship->target1 = NULL;
         ship->target2 = NULL;

         ship->hatchopen = FALSE;
         ship->bayopen = TRUE;

         ship->missiles = ship->maxmissiles;
         ship->torpedos = ship->maxtorpedos;
         ship->rockets = ship->maxrockets;
         ship->autorecharge = FALSE;
         ship->autotrack = FALSE;
         ship->autospeed = FALSE;
      }
      else if( ship->cockpit == ROOM_SHUTTLE_BUS ||
               ship->cockpit == ROOM_SHUTTLE_BUS_2 ||
               ship->cockpit == ROOM_SENATE_SHUTTLE ||
               ship->cockpit == ROOM_CORUSCANT_TURBOCAR || ship->cockpit == ROOM_CORUSCANT_SHUTTLE )
      {
      }
      else if( ( pRoomIndex = get_room_index( ship->lastdoc ) ) != NULL
               && ship->ship_class != CAPITAL_SHIP
	       && ship->ship_class != SHIP_PLATFORM )
      {
         LINK( ship, pRoomIndex->first_ship, pRoomIndex->last_ship, next_in_room, prev_in_room );
         ship->in_room = pRoomIndex;
         ship->location = ship->lastdoc;
      }

      if( ship->ship_class == SHIP_PLATFORM
         || ship->type == MOB_SHIP || ship->ship_class == CAPITAL_SHIP )
      {
         ship_to_starsystem( ship, starsystem_from_name( ship->home ) );
         ship->vx = number_range( -5000, 5000 );
         ship->vy = number_range( -5000, 5000 );
         ship->vz = number_range( -5000, 5000 );
         ship->hx = 1;
         ship->hy = 1;
         ship->hz = 1;
         ship->shipstate = SHIP_READY;
         ship->autopilot = TRUE;
         ship->autorecharge = TRUE;
         ship->shield = ship->maxshield;
      }

      if( ship->type != MOB_SHIP && ( clan = get_clan( ship->owner ) ) != NULL )
      {
         if( ship->ship_class <= SHIP_PLATFORM )
            clan->spacecraft++;
         else
            clan->vehicles++;
      }
   }
   return found;
}

/*
 * Load in all the ship files.
 */
void load_ships(  )
{
   FILE *fpList;
   const char *filename;
   char shiplist[256];

   first_ship = NULL;
   last_ship = NULL;
   first_missile = NULL;
   last_missile = NULL;

   log_string( "Loading ships..." );

   snprintf( shiplist, 256, "%s%s", SHIP_DIR, SHIP_LIST );
   if( ( fpList = fopen( shiplist, "r" ) ) == NULL )
   {
      perror( shiplist );
      exit( 1 );
   }

   for( ;; )
   {
      filename = feof( fpList ) ? "$" : fread_word( fpList );

      if( filename[0] == '$' )
         break;

      if( !load_ship_file( filename ) )
      {
         bug( "%s: Cannot load ship file: %s", __func__, filename );
      }
   }
   FCLOSE( fpList );
   log_string( " Done ships " );
}

void resetship( SHIP_DATA * ship )
{
   ship->shipstate = SHIP_READY;

   if( ship->ship_class != SHIP_PLATFORM && ship->type != MOB_SHIP )
   {
      extract_ship( ship );
      ship_to_room( ship, ship->shipyard );

      ship->location = ship->shipyard;
      ship->lastdoc = ship->shipyard;
      ship->shipstate = SHIP_DOCKED;
   }

   if( ship->starsystem )
      ship_from_starsystem( ship, ship->starsystem );

   /* v1.22: SWGD's resetship clears temporary flags (linked lasers,
    * afterburner, cloak, redirects...) via rem_extshipflags, preserving
    * sabotage + simulator. (v1.23: the module refresh that was here is
    * unnecessary under SWGD-verbatim module semantics.) */
   rem_extshipflags( ship );

   ship->currspeed = 0;
   ship->energy = ship->maxenergy;
   ship->chaff = ship->maxchaff;
   ship->hull = ship->maxhull;
   ship->shield = 0;

   ship->ionstate = LASER_READY;
   ship->statet1 = LASER_READY;
   ship->statet2 = LASER_READY;
   ship->statet0 = LASER_READY;
   ship->missilestate = MISSILE_READY;
   ship->torpedostate = MISSILE_READY;
   ship->rocketstate = MISSILE_READY;

   ship->currjump = NULL;
   ship->target0 = NULL;
   ship->target1 = NULL;
   ship->target2 = NULL;

   ship->hatchopen = FALSE;
   ship->bayopen = TRUE;

   ship->missiles = ship->maxmissiles;
   ship->torpedos = ship->maxtorpedos;
   ship->rockets = ship->maxrockets;
   ship->autorecharge = FALSE;
   ship->autotrack = FALSE;
   ship->autospeed = FALSE;

   if( str_cmp( "Public", ship->owner ) && ship->type != MOB_SHIP )
   {
      CLAN_DATA *clan;

      if( ship->type != MOB_SHIP && ( clan = get_clan( ship->owner ) ) != NULL )
      {
	if( ship->ship_class <= SHIP_PLATFORM )
	  clan->spacecraft--;
	else
	  clan->vehicles--;
      }

      STRFREE( ship->owner );
      ship->owner = STRALLOC( "" );
      STRFREE( ship->pilot );
      ship->pilot = STRALLOC( "" );
      STRFREE( ship->copilot );
      ship->copilot = STRALLOC( "" );
   }

   if( ship->type == SHIP_REPUBLIC || ( ship->type == MOB_SHIP && !str_cmp( ship->owner, "the new republic" ) ) )
   {
      STRFREE( ship->home );
      ship->home = STRALLOC( "coruscant" );
   }
   else if( ship->type == SHIP_IMPERIAL || ( ship->type == MOB_SHIP && !str_cmp( ship->owner, "the empire" ) ) )
   {
      STRFREE( ship->home );
      ship->home = STRALLOC( "byss" );
   }
   else if( ship->type == SHIP_CIVILIAN )
   {
      STRFREE( ship->home );
      ship->home = STRALLOC( "corperate" );
   }

   save_ship( ship );
}

void do_resetship( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;

   ship = get_ship( argument );
   if( ship == NULL )
   {
      send_to_char( "&RNo such ship!", ch );
      return;
   }

   resetship( ship );

   if( ( ship->ship_class == SHIP_PLATFORM || ship->type == MOB_SHIP || ship->ship_class == CAPITAL_SHIP
         || ship->ship_class == SUPERCAPITAL_SHIP ) && ship->home )
   {
      ship_to_starsystem( ship, starsystem_from_name( ship->home ) );
      ship->vx = number_range( -5000, 5000 );
      ship->vy = number_range( -5000, 5000 );
      ship->vz = number_range( -5000, 5000 );
      ship->shipstate = SHIP_READY;
      ship->autopilot = TRUE;
      ship->autorecharge = TRUE;
      ship->shield = ship->maxshield;
   }
}

void do_setship( CHAR_DATA * ch, const char *argument )
{
   char arg1[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   SHIP_DATA *ship;
   int tempnum;
   ROOM_INDEX_DATA *roomindex;

   if( IS_NPC( ch ) )
   {
      send_to_char( "Huh?\r\n", ch );
      return;
   }

   argument = one_argument( argument, arg1 );
   argument = one_argument( argument, arg2 );

   if( arg1[0] == '\0' || arg2[0] == '\0' || arg1[0] == '\0' )
   {
      send_to_char( "Usage: setship <ship> <field> <values>\r\n", ch );
      send_to_char( "\r\nField being one of:\r\n", ch );
      send_to_char( "filename name owner copilot pilot description home\r\n", ch );
      send_to_char( "cockpit entrance turret1 turret2 hanger\r\n", ch );
      send_to_char( "engineroom firstroom lastroom shipyard\r\n", ch );
      send_to_char( "manuever speed hyperspeed tractorbeam\r\n", ch );
      send_to_char( "lasers missiles shield hull energy chaff\r\n", ch );
      send_to_char( "comm sensor astroarray class torpedos\r\n", ch );
      send_to_char( "laserdamage ions armor maxmodules\r\n", ch );
      send_to_char( "autocannon autodamage autoammo\r\n", ch );
      send_to_char( "pilotseat coseat gunseat navseat rockets\r\n", ch );
      send_to_char( "flags\r\n", ch );
      return;
   }

   ship = get_ship( arg1 );
   if( !ship )
   {
      send_to_char( "No such ship.\r\n", ch );
      return;
   }

   if( !str_cmp( arg2, "maxmodules" ) )
   {
      if( atoi( argument ) < ship->modules )
      {
         send_to_char( "Please remove some modules first.\r\n", ch );
         return;
      }
      ship->maxmodules = URANGE( 10, atoi( argument ), MAX_SHIP_MODULES );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "owner" ) )
   {
      CLAN_DATA *clan;
      if( ship->type != MOB_SHIP && ( clan = get_clan( ship->owner ) ) != NULL )
      {
	if( ship->ship_class <= SHIP_PLATFORM )
	  clan->spacecraft--;
	else
	  clan->vehicles--;
      }
      STRFREE( ship->owner );
      ship->owner = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      if( ship->type != MOB_SHIP && ( clan = get_clan( ship->owner ) ) != NULL )
      {
	if( ship->ship_class <= SHIP_PLATFORM )
	  clan->spacecraft++;
	else
	  clan->vehicles++;
      }
      return;
   }

   if( !str_cmp( arg2, "home" ) )
   {
      STRFREE( ship->home );
      ship->home = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "pilot" ) )
   {
      STRFREE( ship->pilot );
      ship->pilot = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "copilot" ) )
   {
      STRFREE( ship->copilot );
      ship->copilot = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "firstroom" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      ship->firstroom = tempnum;
      ship->lastroom = tempnum;
      ship->cockpit = tempnum;
      ship->coseat = tempnum;
      ship->pilotseat = tempnum;
      ship->gunseat = tempnum;
      ship->navseat = tempnum;
      ship->entrance = tempnum;
      ship->turret1 = 0;
      ship->turret2 = 0;
      ship->hanger = 0;
      send_to_char( "You will now need to set the other rooms in the ship.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "lastroom" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom )
      {
         send_to_char( "The last room on a ship must be greater than or equal to the first room.\r\n", ch );
         return;
      }
      if( ship->ship_class == FIGHTER_SHIP
	  && ( tempnum - ship->firstroom ) > 5 )
      {
         send_to_char( "Starfighters may have up to 5 rooms only.\r\n", ch );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP
	  && ( tempnum - ship->firstroom ) > 25 )
      {
         send_to_char( "Midships may have up to 25 rooms only.\r\n", ch );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP
	  && ( tempnum - ship->firstroom ) > 100 )
      {
         send_to_char( "Capital Ships may have up to 100 rooms only.\r\n", ch );
         return;
      }
      ship->lastroom = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "cockpit" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      if( tempnum == ship->turret1 || tempnum == ship->turret2 || tempnum == ship->hanger )
      {
         send_to_char( "That room is already being used by another part of the ship\r\n", ch );
         return;
      }
      ship->cockpit = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "pilotseat" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      if( tempnum == ship->turret1 || tempnum == ship->turret2 || tempnum == ship->hanger )
      {
         send_to_char( "That room is already being used by another part of the ship\r\n", ch );
         return;
      }
      ship->pilotseat = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }
   if( !str_cmp( arg2, "coseat" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      if( tempnum == ship->turret1 || tempnum == ship->turret2 || tempnum == ship->hanger )
      {
         send_to_char( "That room is already being used by another part of the ship\r\n", ch );
         return;
      }
      ship->coseat = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }
   if( !str_cmp( arg2, "navseat" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      if( tempnum == ship->turret1 || tempnum == ship->turret2 || tempnum == ship->hanger )
      {
         send_to_char( "That room is already being used by another part of the ship\r\n", ch );
         return;
      }
      ship->navseat = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }
   if( !str_cmp( arg2, "gunseat" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      if( tempnum == ship->turret1 || tempnum == ship->turret2 || tempnum == ship->hanger )
      {
         send_to_char( "That room is already being used by another part of the ship\r\n", ch );
         return;
      }
      ship->gunseat = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "entrance" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      ship->entrance = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "turret1" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      if( ship->ship_class == FIGHTER_SHIP )
      {
         send_to_char( "Starfighters can't have extra laser turrets.\r\n", ch );
         return;
      }
      if( tempnum == ship->cockpit || tempnum == ship->entrance ||
          tempnum == ship->turret2 || tempnum == ship->hanger || tempnum == ship->engineroom )
      {
         send_to_char( "That room is already being used by another part of the ship\r\n", ch );
         return;
      }
      ship->turret1 = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "turret2" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      if( ship->ship_class == FIGHTER_SHIP )
      {
         send_to_char( "Starfighters can't have extra laser turrets.\r\n", ch );
         return;
      }
      if( tempnum == ship->cockpit || tempnum == ship->entrance ||
          tempnum == ship->turret1 || tempnum == ship->hanger || tempnum == ship->engineroom )
      {
         send_to_char( "That room is already being used by another part of the ship\r\n", ch );
         return;
      }
      ship->turret2 = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "hanger" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      if( tempnum == ship->cockpit || tempnum == ship->entrance ||
          tempnum == ship->turret1 || tempnum == ship->turret2 || tempnum == ship->engineroom )
      {
         send_to_char( "That room is already being used by another part of the ship\r\n", ch );
         return;
      }
      if( ship->ship_class == FIGHTER_SHIP )
      {
         send_to_char( "Starfighters are to small to have hangers for other ships!\r\n", ch );
         return;
      }
      ship->hanger = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "engineroom" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.\r\n", ch );
         return;
      }
      if( tempnum < ship->firstroom || tempnum > ship->lastroom )
      {
         send_to_char( "That room number is not in that ship .. \r\nIt must be between Firstroom and Lastroom.\r\n", ch );
         return;
      }
      if( tempnum == ship->cockpit || tempnum == ship->entrance ||
          tempnum == ship->turret1 || tempnum == ship->turret2 || tempnum == ship->hanger )
      {
         send_to_char( "That room is already being used by another part of the ship\r\n", ch );
         return;
      }
      ship->engineroom = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "shipyard" ) )
   {
      tempnum = atoi( argument );
      roomindex = get_room_index( tempnum );
      if( roomindex == NULL )
      {
         send_to_char( "That room doesn't exist.", ch );
         return;
      }
      ship->shipyard = tempnum;
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "type" ) )
   {
      if( !str_cmp( argument, "republic" ) )
         ship->type = SHIP_REPUBLIC;
      else if( !str_cmp( argument, "imperial" ) )
         ship->type = SHIP_IMPERIAL;
      else if( !str_cmp( argument, "civilian" ) )
         ship->type = SHIP_CIVILIAN;
      else if( !str_cmp( argument, "mob" ) )
         ship->type = MOB_SHIP;
      else
      {
         send_to_char( "Ship type must be either: republic, imperial, civilian or mob.\r\n", ch );
         return;
      }
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "name" ) )
   {
      STRFREE( ship->name );
      ship->name = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "filename" ) )
   {
      DISPOSE( ship->filename );
      ship->filename = strdup( argument );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      write_ship_list(  );
      return;
   }

   if( !str_cmp( arg2, "desc" ) )
   {
      STRFREE( ship->description );
      ship->description = STRALLOC( argument );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   /* v1.22: per-class setship limits ported verbatim from SWGD (Arcturus).
    * OCEAN_SHIP / WHEELED / LAND_CRAWLER have no branches - those classes
    * don't exist in SWGD and are unused here; fields on them will no-op. */
   if( !str_cmp( arg2, "manuever" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->manuever = URANGE( 0, atoi( argument ), 200 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->manuever = URANGE( 0, atoi( argument ), 150 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->manuever = URANGE( 0, atoi( argument ), 75 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->manuever = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->manuever = URANGE( 0, atoi( argument ), 15 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->manuever = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->manuever = URANGE( 0, atoi( argument ), 75 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         ship->manuever = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         ship->manuever = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "laserdamage" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->laserdamage = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->laserdamage = URANGE( 0, atoi( argument ), 10 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->laserdamage = URANGE( 0, atoi( argument ), 15 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->laserdamage = URANGE( 0, atoi( argument ), 20 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->laserdamage = URANGE( 0, atoi( argument ), 25 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->laserdamage = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->laserdamage = URANGE( 0, atoi( argument ), 7 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         ship->laserdamage = URANGE( 0, atoi( argument ), 2 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         ship->laserdamage = URANGE( 0, atoi( argument ), 10 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "lasers" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->lasers = URANGE( 0, atoi( argument ), 4 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->lasers = URANGE( 0, atoi( argument ), 6 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->lasers = URANGE( 0, atoi( argument ), 10 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->lasers = URANGE( 0, atoi( argument ), 15 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->lasers = URANGE( 0, atoi( argument ), 25 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->lasers = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->lasers = URANGE( 0, atoi( argument ), 2 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         ship->lasers = URANGE( 0, atoi( argument ), 1 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         ship->lasers = URANGE( 0, atoi( argument ), 6 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "ions" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->ions = URANGE( 0, atoi( argument ), 2 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->ions = URANGE( 0, atoi( argument ), 3 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->ions = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->ions = URANGE( 0, atoi( argument ), 10 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->ions = URANGE( 0, atoi( argument ), 15 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->ions = URANGE( 0, atoi( argument ), 25 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->ions = URANGE( 0, atoi( argument ), 1 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         send_to_char( "Speeders can't have Ions!\r\n", ch );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         ship->ions = URANGE( 0, atoi( argument ), 3 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "armor" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->maxarmor = URANGE( 0, atoi( argument ), 5 );
         ship->armor = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->maxarmor = URANGE( 0, atoi( argument ), 10 );
         ship->armor = URANGE( 0, atoi( argument ), 10 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->maxarmor = URANGE( 0, atoi( argument ), 25 );
         ship->armor = URANGE( 0, atoi( argument ), 25 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->maxarmor = URANGE( 0, atoi( argument ), 50 );
         ship->armor = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->maxarmor = URANGE( 0, atoi( argument ), 75 );
         ship->armor = URANGE( 0, atoi( argument ), 75 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->maxarmor = URANGE( 0, atoi( argument ), 150 );
         ship->armor = URANGE( 0, atoi( argument ), 150 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         send_to_char( "Armor values for this class are not defined in SWGD.\r\n", ch );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         send_to_char( "Armor values for this class are not defined in SWGD.\r\n", ch );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         send_to_char( "Armor values for this class are not defined in SWGD.\r\n", ch );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "speed" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->realspeed = URANGE( 0, atoi( argument ), 250 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->realspeed = URANGE( 0, atoi( argument ), 150 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->realspeed = URANGE( 0, atoi( argument ), 75 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->realspeed = URANGE( 0, atoi( argument ), 25 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->realspeed = URANGE( 0, atoi( argument ), 15 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->realspeed = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->realspeed = URANGE( 0, atoi( argument ), 75 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         ship->realspeed = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         ship->realspeed = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "hyperspeed" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->hyperspeed = URANGE( 0, atoi( argument ), 100 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->hyperspeed = URANGE( 0, atoi( argument ), 125 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->hyperspeed = URANGE( 0, atoi( argument ), 75 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->hyperspeed = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->hyperspeed = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->hyperspeed = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         send_to_char( "Not On Ground Vehicles.\r\n", ch );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         send_to_char( "Not On Ground Vehicles.\r\n", ch );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         send_to_char( "Not On Ground Vehicles.\r\n", ch );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "tractorbeam" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         send_to_char( "It is impossible to put a TB on a fighter.\r\n", ch );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->tractorbeam = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->tractorbeam = URANGE( 0, atoi( argument ), 25 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->tractorbeam = URANGE( 0, atoi( argument ), 100 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->tractorbeam = URANGE( 0, atoi( argument ), 150 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->tractorbeam = URANGE( 0, atoi( argument ), 200 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         send_to_char( "Impossible.\r\n", ch );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         send_to_char( "Impossible.\r\n", ch );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         send_to_char( "Impossible.\r\n", ch );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "shield" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->maxshield = URANGE( 0, atoi( argument ), 500 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->maxshield = URANGE( 0, atoi( argument ), 1000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->maxshield = URANGE( 0, atoi( argument ), 2500 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->maxshield = URANGE( 0, atoi( argument ), 5000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->maxshield = URANGE( 0, atoi( argument ), 7500 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->maxshield = URANGE( 0, atoi( argument ), 10000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->maxshield = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         send_to_char( "Not on Speeders.\r\n", ch );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         ship->maxshield = URANGE( 0, atoi( argument ), 150 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "hull" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->hull = URANGE( 1, atoi( argument ), 1000 );
         ship->maxhull = URANGE( 1, atoi( argument ), 1000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->hull = URANGE( 1, atoi( argument ), 5000 );
         ship->maxhull = URANGE( 1, atoi( argument ), 5000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->hull = URANGE( 1, atoi( argument ), 10000 );
         ship->maxhull = URANGE( 1, atoi( argument ), 10000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->hull = URANGE( 1, atoi( argument ), 17500 );
         ship->maxhull = URANGE( 1, atoi( argument ), 17500 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->hull = URANGE( 1, atoi( argument ), 25000 );
         ship->maxhull = URANGE( 1, atoi( argument ), 25000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->hull = URANGE( 1, atoi( argument ), 30000 );
         ship->maxhull = URANGE( 1, atoi( argument ), 30000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->hull = URANGE( 1, atoi( argument ), 250 );
         ship->maxhull = URANGE( 1, atoi( argument ), 250 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         ship->hull = URANGE( 1, atoi( argument ), 400 );
         ship->maxhull = URANGE( 1, atoi( argument ), 400 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         ship->hull = URANGE( 1, atoi( argument ), 5000 );
         ship->maxhull = URANGE( 1, atoi( argument ), 5000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "energy" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->energy = URANGE( 1, atoi( argument ), 5000 );
         ship->maxenergy = URANGE( 1, atoi( argument ), 5000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->energy = URANGE( 1, atoi( argument ), 10000 );
         ship->maxenergy = URANGE( 1, atoi( argument ), 10000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->energy = URANGE( 1, atoi( argument ), 25000 );
         ship->maxenergy = URANGE( 1, atoi( argument ), 25000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->energy = URANGE( 1, atoi( argument ), 30000 );
         ship->maxenergy = URANGE( 1, atoi( argument ), 30000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->energy = URANGE( 1, atoi( argument ), 32000 );
         ship->maxenergy = URANGE( 1, atoi( argument ), 32000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->energy = URANGE( 1, atoi( argument ), 32000 );
         ship->maxenergy = URANGE( 1, atoi( argument ), 32000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->energy = URANGE( 1, atoi( argument ), 5000 );
         ship->maxenergy = URANGE( 1, atoi( argument ), 5000 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         ship->energy = URANGE( 1, atoi( argument ), 1500 );
         ship->maxenergy = URANGE( 1, atoi( argument ), 1500 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         ship->energy = URANGE( 1, atoi( argument ), 7500 );
         ship->maxenergy = URANGE( 1, atoi( argument ), 7500 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "sensor" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->sensor = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->sensor = URANGE( 0, atoi( argument ), 100 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->sensor = URANGE( 0, atoi( argument ), 150 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->sensor = URANGE( 0, atoi( argument ), 175 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->sensor = URANGE( 0, atoi( argument ), 200 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->sensor = URANGE( 0, atoi( argument ), 255 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->sensor = URANGE( 0, atoi( argument ), 25 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         ship->sensor = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         ship->sensor = URANGE( 0, atoi( argument ), 15 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "chaff" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->chaff = URANGE( 0, atoi( argument ), 5 );
         ship->maxchaff = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->chaff = URANGE( 0, atoi( argument ), 15 );
         ship->maxchaff = URANGE( 0, atoi( argument ), 15 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->chaff = URANGE( 0, atoi( argument ), 25 );
         ship->maxchaff = URANGE( 0, atoi( argument ), 25 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->chaff = URANGE( 0, atoi( argument ), 50 );
         ship->maxchaff = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->chaff = URANGE( 0, atoi( argument ), 50 );
         ship->maxchaff = URANGE( 0, atoi( argument ), 50 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->chaff = URANGE( 0, atoi( argument ), 75 );
         ship->maxchaff = URANGE( 0, atoi( argument ), 75 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->chaff = URANGE( 0, atoi( argument ), 5 );
         ship->maxchaff = URANGE( 0, atoi( argument ), 5 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         send_to_char( "Not on a Speeder.\r\n", ch );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         send_to_char( "Not on a Walker.\r\n", ch );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "missiles" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->maxmissiles = URANGE( 0, atoi( argument ), 12 );
         ship->missiles = URANGE( 0, atoi( argument ), 12 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->maxmissiles = URANGE( 0, atoi( argument ), 18 );
         ship->missiles = URANGE( 0, atoi( argument ), 18 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->maxmissiles = URANGE( 0, atoi( argument ), 32 );
         ship->missiles = URANGE( 0, atoi( argument ), 32 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->maxmissiles = URANGE( 0, atoi( argument ), 64 );
         ship->missiles = URANGE( 0, atoi( argument ), 64 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->maxmissiles = URANGE( 0, atoi( argument ), 128 );
         ship->missiles = URANGE( 0, atoi( argument ), 128 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->maxmissiles = URANGE( 0, atoi( argument ), 200 );
         ship->missiles = URANGE( 0, atoi( argument ), 200 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         ship->maxmissiles = URANGE( 0, atoi( argument ), 6 );
         ship->missiles = URANGE( 0, atoi( argument ), 6 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         ship->maxmissiles = URANGE( 0, atoi( argument ), 1 );
         ship->missiles = URANGE( 0, atoi( argument ), 1 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         send_to_char( "Walkers Can't Carry Missiles.\r\n", ch );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "torpedos" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->maxtorpedos = URANGE( 0, atoi( argument ), 6 );
         ship->torpedos = URANGE( 0, atoi( argument ), 6 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->maxtorpedos = URANGE( 0, atoi( argument ), 12 );
         ship->torpedos = URANGE( 0, atoi( argument ), 12 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->maxtorpedos = URANGE( 0, atoi( argument ), 24 );
         ship->torpedos = URANGE( 0, atoi( argument ), 24 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->maxtorpedos = URANGE( 0, atoi( argument ), 48 );
         ship->torpedos = URANGE( 0, atoi( argument ), 48 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->maxtorpedos = URANGE( 0, atoi( argument ), 96 );
         ship->torpedos = URANGE( 0, atoi( argument ), 96 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->maxtorpedos = URANGE( 0, atoi( argument ), 200 );
         ship->torpedos = URANGE( 0, atoi( argument ), 200 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         send_to_char( "Cloud Cars Can't Carry Torpedos.\r\n", ch );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         send_to_char( "Speeders Can't Carry Torpedos.\r\n", ch );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         send_to_char( "Walkers Can't Carry Torpedos.\r\n", ch );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "rockets" ) )
   {
      if( ship->ship_class == FIGHTER_SHIP )
      {
         ship->maxrockets = URANGE( 0, atoi( argument ), 1 );
         ship->rockets = URANGE( 0, atoi( argument ), 1 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP )
      {
         ship->maxrockets = URANGE( 0, atoi( argument ), 3 );
         ship->rockets = URANGE( 0, atoi( argument ), 3 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == FRIGATE_SHIP )
      {
         ship->maxrockets = URANGE( 0, atoi( argument ), 9 );
         ship->rockets = URANGE( 0, atoi( argument ), 9 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CAPITAL_SHIP )
      {
         ship->maxrockets = URANGE( 0, atoi( argument ), 27 );
         ship->rockets = URANGE( 0, atoi( argument ), 27 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SUPERCAPITAL_SHIP )
      {
         ship->maxrockets = URANGE( 0, atoi( argument ), 81 );
         ship->rockets = URANGE( 0, atoi( argument ), 81 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == SHIP_PLATFORM )
      {
         ship->maxrockets = URANGE( 0, atoi( argument ), 100 );
         ship->rockets = URANGE( 0, atoi( argument ), 100 );
         send_to_char( "Done.\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->ship_class == CLOUD_CAR )
      {
         send_to_char( "Cloud Cars Can't Deploy Rockets.\r\n", ch );
         return;
      }
      if( ship->ship_class == LAND_SPEEDER )
      {
         send_to_char( "Speeder's Cant possibly hold a rocket.\r\n", ch );
         return;
      }
      if( ship->ship_class == WALKER )
      {
         send_to_char( "Ain't No way a Rocket's Fitting into a Walker.\r\n", ch );
         return;
      }
      return;
   }

   if( !str_cmp( arg2, "autocannon" ) )
   {
      if( get_trust( ch ) < ( MAX_LEVEL - 1 ) )
      {
         send_to_char( "At current, Only Head Imms can modify autocannon settings.\r\n", ch );
         return;
      }
      if( ship->autocannon == 0 )
      {
         ship->autocannon = 1;
         send_to_char( "Autocannon Added!\r\n", ch );
         save_ship( ship );
         return;
      }
      if( ship->autocannon == 1 )
      {
         ship->autocannon = 0;
         send_to_char( "Autocannon Removed!\r\n", ch );
         save_ship( ship );
         return;
      }
   }

   if( !str_cmp( arg2, "autodamage" ) )
   {
      if( get_trust( ch ) < ( MAX_LEVEL - 1 ) )
      {
         send_to_char( "At current, Only Head Imms can modify autocannon settings.\r\n", ch );
         return;
      }
      ship->autodamage = URANGE( 0, atoi( argument ), 255 );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "autoammo" ) )
   {
      if( get_trust( ch ) < ( MAX_LEVEL - 1 ) )
      {
         send_to_char( "At current, Only Head Imms can modify autocannon settings.\r\n", ch );
         return;
      }
      ship->autoammomax = URANGE( 0, atoi( argument ), 255 );
      ship->autoammo = URANGE( 0, atoi( argument ), 255 );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "class" ) )
   {
     ship->ship_class = URANGE( 0, atoi( argument ), WALKER );   /* v1.22: was capped at 9, locking out WALKER */
     send_to_char( "Done.\r\n", ch );
     save_ship( ship );
     return;
   }

   if( !str_cmp( arg2, "astroarray" ) )
   {
      ship->astro_array = URANGE( 0, atoi( argument ), 255 );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "comm" ) )
   {
      ship->comm = URANGE( 0, atoi( argument ), 255 );
      send_to_char( "Done.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( arg2, "flags" ) )
   {
      char farg[MAX_INPUT_LENGTH];
      int flagnum;

      argument = one_argument( argument, farg );

      if( farg[0] == '\0' )
      {
         int x;

         send_to_char( "Possible flags:\r\n", ch );
         for( x = 0; x < 27; x++ )
            ch_printf( ch, "%-16s%s", ship_flags[x], ( x % 4 == 3 ) ? "\r\n" : "" );
         send_to_char( "\r\n", ch );
         return;
      }

      if( ( flagnum = get_shipflag( farg ) ) == -1 )
      {
         ch_printf( ch, "No such flag: %s\r\n", farg );
         return;
      }

      TOGGLE_BIT( ship->flags, ( 1 << flagnum ) );
      ch_printf( ch, "%s is now %s.\r\n", ship_flags[flagnum],
                 IS_SET( ship->flags, ( 1 << flagnum ) ) ? "SET" : "REMOVED" );
      save_ship( ship );
      return;
   }

   do_setship( ch, "" );
}

void do_showship( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;

   if( IS_NPC( ch ) )
   {
      send_to_char( "Huh?\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
   {
      send_to_char( "Usage: showship <ship>\r\n", ch );
      return;
   }

   ship = get_ship( argument );
   if( !ship )
   {
      send_to_char( "No such ship.\r\n", ch );
      return;
   }
   set_char_color( AT_YELLOW, ch );
   ch_printf( ch, "%s %s : %s\r\nFilename: %s\r\n",
              ship->type == SHIP_REPUBLIC ? "New Republic" :
              ( ship->type == SHIP_IMPERIAL ? "Imperial" :
                ( ship->type == SHIP_CIVILIAN ? "Civilian" : "Mob" ) ),
              ship->ship_class == FIGHTER_SHIP ? "Starfighter" :
              ( ship->ship_class == MIDSIZE_SHIP ? "Midship" :
                ( ship->ship_class == FRIGATE_SHIP ? "Frigate" :
                ( ship->ship_class == SUPERCAPITAL_SHIP ? "Supercapital Ship" :
                ( ship->ship_class == CAPITAL_SHIP ? "Capital Ship" :
                  ( ship->ship_class == SHIP_PLATFORM ? "Platform" :
                    ( ship->ship_class == CLOUD_CAR ? "Cloudcar" :
                      ( ship->ship_class == OCEAN_SHIP ? "Boat" :
                        ( ship->ship_class == LAND_SPEEDER ? "Speeder" :
                          ( ship->ship_class == WHEELED ? "Wheeled Transport" :
                            ( ship->ship_class == LAND_CRAWLER ? "Crawler" :
                              ( ship->ship_class == WALKER ? "Walker" : "Unknown" ) ) ) ) ) ) ) ) ) ) ), ship->name, ship->filename );
   ch_printf( ch, "Home: %s   Description: %s\r\nOwner: %s   Pilot: %s   Copilot: %s\r\n",
              ship->home, ship->description, ship->owner, ship->pilot, ship->copilot );
   ch_printf( ch, "Firstroom: %d   Lastroom: %d", ship->firstroom, ship->lastroom );
   ch_printf( ch, "Cockpit: %d   Entrance: %d   Hanger: %d  Engineroom: %d\r\n",
              ship->cockpit, ship->entrance, ship->hanger, ship->engineroom );
   ch_printf( ch, "Pilotseat: %d   Coseat: %d   Navseat: %d  Gunseat: %d\r\n",
              ship->pilotseat, ship->coseat, ship->navseat, ship->gunseat );
   ch_printf( ch, "Location: %d   Lastdoc: %d   Shipyard: %d\r\n", ship->location, ship->lastdoc, ship->shipyard );
   ch_printf( ch, "Tractor Beam: %d   Comm: %d   Sensor: %d   Astro Array: %d\r\n",
              ship->tractorbeam, ship->comm, ship->sensor, ship->astro_array );
   ch_printf( ch, "Lasers: %d  Laser Condition: %s\r\n", ship->lasers, ship->statet0 == LASER_DAMAGED ? "Damaged" : "Good" );
   ch_printf( ch, "Turret One: %d  Condition: %s\r\n", ship->turret1, ship->statet1 == LASER_DAMAGED ? "Damaged" : "Good" );
   ch_printf( ch, "Turret Two: %d  Condition: %s\r\n", ship->turret2, ship->statet2 == LASER_DAMAGED ? "Damaged" : "Good" );
   ch_printf( ch, "Missiles: %d/%d (%s)  Torpedos: %d/%d (%s)  Rockets: %d/%d (%s)\r\n",
              ship->missiles, ship->maxmissiles,
              ship->missilestate == MISSILE_DAMAGED ? "Damaged" : "Good",
              ship->torpedos, ship->maxtorpedos,
              ship->torpedostate == MISSILE_DAMAGED ? "Damaged" : "Good",
              ship->rockets, ship->maxrockets,
              ship->rocketstate == MISSILE_DAMAGED ? "Damaged" : "Good" );
   ch_printf( ch, "Hull: %d/%d  Ship Condition: %s\r\n",
              ship->hull, ship->maxhull, ship->shipstate == SHIP_DISABLED ? "Disabled" : "Running" );

   ch_printf( ch, "Shields: %d/%d   Energy(fuel): %d/%d   Chaff: %d/%d\r\n",
              ship->shield, ship->maxshield, ship->energy, ship->maxenergy, ship->chaff, ship->maxchaff );
   ch_printf( ch, "Current Coordinates: %.0f %.0f %.0f\r\n", ship->vx, ship->vy, ship->vz );
   ch_printf( ch, "Current Heading: %.0f %.0f %.0f\r\n", ship->hx, ship->hy, ship->hz );
   ch_printf( ch, "Speed: %d/%d   Hyperspeed: %d\r\n  Manueverability: %d",
              ship->currspeed, ship->realspeed, ship->hyperspeed, ship->manuever );
}

void do_makeship( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   char arg[MAX_INPUT_LENGTH];

   argument = one_argument( argument, arg );

   if( !argument || argument[0] == '\0' )
   {
      send_to_char( "Usage: makeship <filename> <ship name>\r\n", ch );
      return;
   }

   CREATE( ship, SHIP_DATA, 1 );
   LINK( ship, first_ship, last_ship, next, prev );

   ship->name = STRALLOC( argument );
   ship->description = STRALLOC( "" );
   ship->owner = STRALLOC( "" );
   ship->copilot = STRALLOC( "" );
   ship->pilot = STRALLOC( "" );
   ship->home = STRALLOC( "" );
   ship->type = SHIP_CIVILIAN;
   ship->starsystem = NULL;
   ship->energy = ship->maxenergy;
   ship->hull = ship->maxhull;
   ship->in_room = NULL;
   ship->next_in_room = NULL;
   ship->prev_in_room = NULL;
   ship->currjump = NULL;
   ship->target0 = NULL;
   ship->target1 = NULL;
   ship->target2 = NULL;

   ship->filename = strdup( arg );
   save_ship( ship );
   write_ship_list(  );
}

void do_copyship( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   SHIP_DATA *old;
   char arg[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];

   argument = one_argument( argument, arg );
   argument = one_argument( argument, arg2 );

   if( !argument || argument[0] == '\0' )
   {
      send_to_char( "Usage: copyship <oldshipname> <filename> <newshipname>\r\n", ch );
      return;
   }

   old = get_ship( arg );

   if( !old )
   {
      send_to_char( "Thats not a ship!\r\n", ch );
      return;
   }

   CREATE( ship, SHIP_DATA, 1 );
   LINK( ship, first_ship, last_ship, next, prev );

   ship->name = STRALLOC( argument );
   ship->description = STRALLOC( "" );
   ship->owner = STRALLOC( "" );
   ship->copilot = STRALLOC( "" );
   ship->pilot = STRALLOC( "" );
   ship->home = STRALLOC( "" );
   ship->type = old->type;
   ship->ship_class = old->ship_class;
   ship->lasers = old->lasers;
   ship->maxmissiles = old->maxmissiles;
   ship->maxrockets = old->maxrockets;
   ship->maxtorpedos = old->maxtorpedos;
   ship->maxshield = old->maxshield;
   ship->maxhull = old->maxhull;
   ship->maxenergy = old->maxenergy;
   ship->hyperspeed = old->hyperspeed;
   ship->maxchaff = old->maxchaff;
   ship->realspeed = old->realspeed;
   ship->manuever = old->manuever;
   ship->modules = 0;
   ship->maxmodules = old->maxmodules;   /* SWGD: slot count is per-ship, copied from the original */
   ship->in_room = NULL;
   ship->next_in_room = NULL;
   ship->prev_in_room = NULL;
   ship->currjump = NULL;
   ship->target0 = NULL;
   ship->target1 = NULL;
   ship->target2 = NULL;

   ship->filename = strdup( arg2 );
   save_ship( ship );
   write_ship_list(  );
}

void do_ships( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   int count;

   if( !IS_NPC( ch ) )
   {
      count = 0;
      send_to_char( "&YThe following ships are owned by you or by your organization:\r\n", ch );
      send_to_char( "\r\n&WShip                               Owner\r\n", ch );
      for( ship = first_ship; ship; ship = ship->next )
      {
         if( str_cmp( ship->owner, ch->name ) )
         {
            if( !ch->pcdata || !ch->pcdata->clan || str_cmp( ship->owner, ch->pcdata->clan->name )
                || ship->ship_class > SHIP_PLATFORM )
               continue;
         }

         if( ship->type == MOB_SHIP )
            continue;
         else if( ship->type == SHIP_REPUBLIC )
            set_char_color( AT_BLOOD, ch );
         else if( ship->type == SHIP_IMPERIAL )
            set_char_color( AT_DGREEN, ch );
         else
            set_char_color( AT_BLUE, ch );

         if( ship->in_room )
            ch_printf( ch, "%s - %s\r\n", ship->name, ship->in_room->name );
         else
            ch_printf( ch, "%s\r\n", ship->name );

         count++;
      }

      if( !count )
      {
         send_to_char( "There are no ships owned by you.\r\n", ch );
      }
   }

   count = 0;
   send_to_char( "&Y\r\nThe following ships are docked here:\r\n", ch );

   send_to_char( "\r\n&WShip                               Owner          Cost/Rent\r\n", ch );
   for( ship = first_ship; ship; ship = ship->next )
   {
      if( ship->location != ch->in_room->vnum
	  || ship->ship_class > SHIP_PLATFORM )
         continue;

      if( ship->type == MOB_SHIP )
         continue;
      else if( ship->type == SHIP_REPUBLIC )
         set_char_color( AT_BLOOD, ch );
      else if( ship->type == SHIP_IMPERIAL )
         set_char_color( AT_DGREEN, ch );
      else
         set_char_color( AT_BLUE, ch );

      ch_printf( ch, "%-35s %-15s", ship->name, ship->owner );
      if( ship->type == MOB_SHIP || ship->ship_class == SHIP_PLATFORM )
      {
         ch_printf( ch, "\r\n" );
         continue;
      }
      if( !str_cmp( ship->owner, "Public" ) )
      {
         ch_printf( ch, "%ld to rent.\r\n", get_ship_value( ship ) / 100 );
      }
      else if( str_cmp( ship->owner, "" ) )
         ch_printf( ch, "%s", "\r\n" );
      else
         ch_printf( ch, "%ld to buy.\r\n", get_ship_value( ship ) );

      count++;
   }

   if( !count )
   {
      send_to_char( "There are no ships docked here.\r\n", ch );
   }
}

void do_speeders( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   int count;

   if( !IS_NPC( ch ) )
   {
      count = 0;
      send_to_char( "&YThe following are owned by you or by your organization:\r\n", ch );
      send_to_char( "\r\n&WVehicle                            Owner\r\n", ch );
      for( ship = first_ship; ship; ship = ship->next )
      {
         if( str_cmp( ship->owner, ch->name ) )
         {
            if( !ch->pcdata || !ch->pcdata->clan || str_cmp( ship->owner, ch->pcdata->clan->name )
                || ship->ship_class <= SHIP_PLATFORM )
               continue;
         }
         if( ship->location != ch->in_room->vnum
	     || ship->ship_class <= SHIP_PLATFORM )
            continue;

         if( ship->type == MOB_SHIP )
            continue;
         else if( ship->type == SHIP_REPUBLIC )
            set_char_color( AT_BLOOD, ch );
         else if( ship->type == SHIP_IMPERIAL )
            set_char_color( AT_DGREEN, ch );
         else
            set_char_color( AT_BLUE, ch );

         ch_printf( ch, "%-35s %-15s\r\n", ship->name, ship->owner );

         count++;
      }

      if( !count )
      {
         send_to_char( "There are no land or air vehicles owned by you.\r\n", ch );
      }

   }

   count = 0;
   send_to_char( "&Y\r\nThe following vehicles are parked here:\r\n", ch );

   send_to_char( "\r\n&WVehicle                            Owner          Cost/Rent\r\n", ch );
   for( ship = first_ship; ship; ship = ship->next )
   {
      if( ship->location != ch->in_room->vnum
	  || ship->ship_class <= SHIP_PLATFORM )
         continue;

      if( ship->type == MOB_SHIP )
         continue;
      else if( ship->type == SHIP_REPUBLIC )
         set_char_color( AT_BLOOD, ch );
      else if( ship->type == SHIP_IMPERIAL )
         set_char_color( AT_DGREEN, ch );
      else
         set_char_color( AT_BLUE, ch );

      ch_printf( ch, "%-35s %-15s", ship->name, ship->owner );

      if( !str_cmp( ship->owner, "Public" ) )
      {
         ch_printf( ch, "%ld to rent.\r\n", get_ship_value( ship ) / 100 );
      }
      else if( str_cmp( ship->owner, "" ) )
         ch_printf( ch, "%s", "\r\n" );
      else
         ch_printf( ch, "%ld to buy.\r\n", get_ship_value( ship ) );

      count++;
   }

   if( !count )
   {
      send_to_char( "There are no sea air or land vehicles here.\r\n", ch );
   }
}

void do_allspeeders( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   int count = 0;

   count = 0;
   send_to_char( "&Y\r\nThe following sea/land/air vehicles are currently formed:\r\n", ch );

   send_to_char( "\r\n&WVehicle                            Owner\r\n", ch );
   for( ship = first_ship; ship; ship = ship->next )
   {
      if( ship->ship_class <= SHIP_PLATFORM )
         continue;

      if( ship->type == MOB_SHIP )
         continue;
      else if( ship->type == SHIP_REPUBLIC )
         set_char_color( AT_BLOOD, ch );
      else if( ship->type == SHIP_IMPERIAL )
         set_char_color( AT_DGREEN, ch );
      else
         set_char_color( AT_BLUE, ch );

      ch_printf( ch, "%-35s %-15s ", ship->name, ship->owner );

      if( !str_cmp( ship->owner, "Public" ) )
      {
         ch_printf( ch, "%ld to rent.\r\n", get_ship_value( ship ) / 100 );
      }
      else if( str_cmp( ship->owner, "" ) )
         ch_printf( ch, "%s", "\r\n" );
      else
         ch_printf( ch, "%ld to buy.\r\n", get_ship_value( ship ) );

      count++;
   }

   if( !count )
   {
      send_to_char( "There are none currently formed.\r\n", ch );
      return;
   }
}

void do_allships( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   int count = 0;

   count = 0;
   send_to_char( "&Y\r\nThe following ships are currently formed:\r\n", ch );

   send_to_char( "\r\n&WShip                               Owner\r\n", ch );

   if( IS_IMMORTAL( ch ) )
      for( ship = first_ship; ship; ship = ship->next )
         if( ship->type == MOB_SHIP )
            ch_printf( ch, "&w%-35s %-15s\r\n", ship->name, ship->owner );

   for( ship = first_ship; ship; ship = ship->next )
   {
     if( ship->ship_class > SHIP_PLATFORM )
       continue;

      if( ship->type == MOB_SHIP )
         continue;
      else if( ship->type == SHIP_REPUBLIC )
         set_char_color( AT_BLOOD, ch );
      else if( ship->type == SHIP_IMPERIAL )
         set_char_color( AT_DGREEN, ch );
      else
         set_char_color( AT_BLUE, ch );

      ch_printf( ch, "%-35s %-15s ", ship->name, ship->owner );
      if( ship->type == MOB_SHIP || ship->ship_class == SHIP_PLATFORM )
      {
         ch_printf( ch, "\r\n" );
         continue;
      }
      if( !str_cmp( ship->owner, "Public" ) )
      {
         ch_printf( ch, "%ld to rent.\r\n", get_ship_value( ship ) / 100 );
      }
      else if( str_cmp( ship->owner, "" ) )
         ch_printf( ch, "%s", "\r\n" );
      else
         ch_printf( ch, "%ld to buy.\r\n", get_ship_value( ship ) );

      count++;
   }

   if( !count )
   {
      send_to_char( "There are no ships currently formed.\r\n", ch );
      return;
   }
}

void ship_to_starsystem( SHIP_DATA * ship, SPACE_DATA * starsystem )
{
   if( starsystem == NULL )
      return;

   if( ship == NULL )
      return;

   if( starsystem->first_ship == NULL )
      starsystem->first_ship = ship;

   if( starsystem->last_ship )
   {
      starsystem->last_ship->next_in_starsystem = ship;
      ship->prev_in_starsystem = starsystem->last_ship;
   }

   starsystem->last_ship = ship;

   ship->starsystem = starsystem;
}

void new_missile( SHIP_DATA * ship, SHIP_DATA * target, CHAR_DATA * ch, int missiletype )
{
   SPACE_DATA *starsystem;
   MISSILE_DATA *missile;

   if( ship == NULL )
      return;

   if( target == NULL )
      return;

   if( ( starsystem = ship->starsystem ) == NULL )
      return;

   CREATE( missile, MISSILE_DATA, 1 );
   LINK( missile, first_missile, last_missile, next, prev );

   missile->target = target;
   missile->fired_from = ship;
   if( ch )
      missile->fired_by = STRALLOC( ch->name );
   else
      missile->fired_by = STRALLOC( "" );
   missile->missiletype = missiletype;
   missile->age = 0;
   if( missile->missiletype == HEAVY_BOMB )
      missile->speed = 20;
   else if( missile->missiletype == PROTON_TORPEDO )
      missile->speed = 200;
   else if( missile->missiletype == CONCUSSION_MISSILE )
      missile->speed = 300;
   else
      missile->speed = 50;

   missile->mx = ( int ) ship->vx;
   missile->my = ( int ) ship->vy;
   missile->mz = ( int ) ship->vz;

   if( starsystem->first_missile == NULL )
      starsystem->first_missile = missile;

   if( starsystem->last_missile )
   {
      starsystem->last_missile->next_in_starsystem = missile;
      missile->prev_in_starsystem = starsystem->last_missile;
   }

   starsystem->last_missile = missile;

   missile->starsystem = starsystem;
}

void ship_from_starsystem( SHIP_DATA * ship, SPACE_DATA * starsystem )
{
   if( starsystem == NULL )
      return;

   if( ship == NULL )
      return;

   if( starsystem->last_ship == ship )
      starsystem->last_ship = ship->prev_in_starsystem;

   if( starsystem->first_ship == ship )
      starsystem->first_ship = ship->next_in_starsystem;

   if( ship->prev_in_starsystem )
      ship->prev_in_starsystem->next_in_starsystem = ship->next_in_starsystem;

   if( ship->next_in_starsystem )
      ship->next_in_starsystem->prev_in_starsystem = ship->prev_in_starsystem;

   ship->starsystem = NULL;
   ship->next_in_starsystem = NULL;
   ship->prev_in_starsystem = NULL;
}

void extract_missile( MISSILE_DATA * missile )
{
   SPACE_DATA *starsystem;

   if( missile == NULL )
      return;

   if( ( starsystem = missile->starsystem ) != NULL )
   {

      if( starsystem->last_missile == missile )
         starsystem->last_missile = missile->prev_in_starsystem;

      if( starsystem->first_missile == missile )
         starsystem->first_missile = missile->next_in_starsystem;

      if( missile->prev_in_starsystem )
         missile->prev_in_starsystem->next_in_starsystem = missile->next_in_starsystem;

      if( missile->next_in_starsystem )
         missile->next_in_starsystem->prev_in_starsystem = missile->prev_in_starsystem;

      missile->starsystem = NULL;
      missile->next_in_starsystem = NULL;
      missile->prev_in_starsystem = NULL;

   }

   UNLINK( missile, first_missile, last_missile, next, prev );

   missile->target = NULL;
   missile->fired_from = NULL;
   if( missile->fired_by )
      STRFREE( missile->fired_by );

   DISPOSE( missile );
}

bool is_rental( CHAR_DATA * ch, SHIP_DATA * ship )
{
   if( !str_cmp( "Public", ship->owner ) )
      return TRUE;

   return FALSE;
}

bool check_pilot( CHAR_DATA * ch, SHIP_DATA * ship )
{
   if( !str_cmp( ch->name, ship->owner ) || !str_cmp( ch->name, ship->pilot )
       || !str_cmp( ch->name, ship->copilot ) || !str_cmp( "Public", ship->owner ) )
      return TRUE;

   if( !IS_NPC( ch ) && ch->pcdata && ch->pcdata->clan )
   {
      if( !str_cmp( ch->pcdata->clan->name, ship->owner ) )
      {
         if( !str_cmp( ch->pcdata->clan->leader, ch->name ) )
            return TRUE;
         if( !str_cmp( ch->pcdata->clan->number1, ch->name ) )
            return TRUE;
         if( !str_cmp( ch->pcdata->clan->number2, ch->name ) )
            return TRUE;
         if( ch->pcdata->bestowments && is_name( "pilot", ch->pcdata->bestowments ) )
            return TRUE;
      }
   }

   return FALSE;
}

bool extract_ship( SHIP_DATA * ship )
{
   ROOM_INDEX_DATA *room;

   if( ( room = ship->in_room ) != NULL )
   {
      UNLINK( ship, room->first_ship, room->last_ship, next_in_room, prev_in_room );
      ship->in_room = NULL;
   }
   return TRUE;
}

void damage_ship_ch( SHIP_DATA * ship, int min, int max, CHAR_DATA * ch )
{
   int sdamage, shield_dmg;
   long xp;

   sdamage = number_range( min, max );

   xp = ( exp_level( ch->skill_level[PILOTING_ABILITY] + 1 ) - exp_level( ch->skill_level[PILOTING_ABILITY] ) ) / 25;
   xp = UMIN( get_ship_value( ship ) / 100, xp );
   gain_exp( ch, xp, PILOTING_ABILITY );

   if( ship->shield > 0 )
   {
      shield_dmg = UMIN( ship->shield, sdamage );
      sdamage -= shield_dmg;
      ship->shield -= shield_dmg;
      if( ship->shield == 0 )
         echo_to_cockpit( AT_BLOOD, ship, "Shields down..." );
   }

   /* v1.22b: SWGD ship armor - flat per-hit absorption once shields are
    * gone (damage = damage - ship->armor; nothing through means no hull
    * loss and no disable rolls). Ion damage deliberately ignores armor,
    * matching SWGD. */
   sdamage -= ship->armor;
   if( sdamage <= 0 )
      return;

   if( sdamage > 0 )
   {
      if( number_range( 1, 100 ) <= 5 && ship->shipstate != SHIP_DISABLED )
      {
         echo_to_cockpit( AT_BLOOD + AT_BLINK, ship, "Ships Drive DAMAGED!" );
         ship->shipstate = SHIP_DISABLED;
      }

      if( number_range( 1, 100 ) <= 5 && ship->missilestate != MISSILE_DAMAGED && ship->maxmissiles > 0 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Missile Launcher DAMAGED!" );
         ship->missilestate = MISSILE_DAMAGED;
      }

      if( number_range( 1, 100 ) <= 5 && ship->torpedostate != MISSILE_DAMAGED && ship->maxtorpedos > 0 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Torpedo Launcher DAMAGED!" );
         ship->torpedostate = MISSILE_DAMAGED;
      }

      if( number_range( 1, 100 ) <= 5 && ship->rocketstate != MISSILE_DAMAGED && ship->maxrockets > 0 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Rocket Launcher DAMAGED!" );
         ship->rocketstate = MISSILE_DAMAGED;
      }

      if( number_range( 1, 100 ) <= 2 && ship->statet0 != LASER_DAMAGED )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Lasers DAMAGED!" );
         ship->statet1 = LASER_DAMAGED;
      }

      if( number_range( 1, 100 ) <= 5 && ship->statet1 != LASER_DAMAGED && ship->turret1 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->turret1 ), "Turret DAMAGED!" );
         ship->statet1 = LASER_DAMAGED;
      }

      if( number_range( 1, 100 ) <= 5 && ship->statet2 != LASER_DAMAGED && ship->turret2 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->turret2 ), "Turret DAMAGED!" );
         ship->statet2 = LASER_DAMAGED;
      }
   }

   ship->hull -= sdamage * 5;

   if( ship->hull <= 0 )
   {
      destroy_ship( ship, ch );

      xp = ( exp_level( ch->skill_level[PILOTING_ABILITY] + 1 ) - exp_level( ch->skill_level[PILOTING_ABILITY] ) );
      xp = UMIN( get_ship_value( ship ), xp );
      gain_exp( ch, xp, PILOTING_ABILITY );
      ch_printf( ch, "&WYou gain %ld piloting experience!\r\n", xp );
      return;
   }

   if( ship->hull <= ship->maxhull / 20 )
      echo_to_cockpit( AT_BLOOD + AT_BLINK, ship, "WARNING! Ship hull severely damaged!" );
}

/*
 * iondamage_ship_ch - v1.20, ported from SWGD. Ion weapons work
 * differently from lasers: shields absorb 1.3x the raw damage (ions are
 * effective against shields), and once shields are down, instead of
 * dealing hull damage they disable one ship system at a time (drive,
 * missile/torpedo/rocket launchers, lasers, or the ions themselves) -
 * matching SWGD's real mechanic exactly. No hull damage means no
 * destruction path here; ions disable, they don't kill.
 */
void iondamage_ship_ch( SHIP_DATA * ship, int min, int max, CHAR_DATA * ch )
{
   int idamage, shield_dmg;
   int disable;
   long xp;

   xp = ( exp_level( ch->skill_level[PILOTING_ABILITY] + 1 ) - exp_level( ch->skill_level[PILOTING_ABILITY] ) ) / 25;
   xp = UMIN( get_ship_value( ship ) / 100, xp );
   gain_exp( ch, xp, PILOTING_ABILITY );

   idamage = number_range( min, max );

   if( ship->shield > 0 )
   {
      idamage = ( int )( idamage * 1.3 );
      shield_dmg = UMIN( ship->shield, idamage );
      idamage -= shield_dmg;
      ship->shield -= shield_dmg;
      if( ship->shield == 0 )
         echo_to_cockpit( AT_BLOOD, ship, "Shields down..." );
   }

   if( ship->shield > 0 || idamage == 0 )
      return;   /* shields still up, or no damage got through - no system disabling */

   disable = URANGE( 20, ( int )( ship->hull / ( ship->ship_class + 1 ) ), 100 );

   if( ship->shipstate != SHIP_DISABLED && number_range( 1, disable ) <= idamage * 2 )
   {
      echo_to_cockpit( AT_BLUE, ship, "Blue ion energy washes over the room!" );
      echo_to_cockpit( AT_BLOOD + AT_BLINK, ship, "Ships Drive DAMAGED!" );
      ship->shipstate = SHIP_DISABLED;
      ship->currspeed = 0;
      return;
   }
   if( ship->missilestate != MISSILE_DAMAGED && ship->maxmissiles > 0 && number_range( 1, disable ) <= idamage * 2 )
   {
      echo_to_room( AT_BLUE, get_room_index( ship->gunseat ), "Blue ion energy washes over the room!" );
      echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Missile Launcher DAMAGED!" );
      ship->missilestate = MISSILE_DAMAGED;
      return;
   }
   if( ship->torpedostate != MISSILE_DAMAGED && ship->maxtorpedos > 0 && number_range( 1, disable ) <= idamage * 2 )
   {
      echo_to_room( AT_BLUE, get_room_index( ship->gunseat ), "Blue ion energy washes over the room!" );
      echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Torpedo Launcher DAMAGED!" );
      ship->torpedostate = MISSILE_DAMAGED;
      return;
   }
   if( ship->rocketstate != MISSILE_DAMAGED && ship->maxrockets > 0 && number_range( 1, disable ) <= idamage * 2 )
   {
      echo_to_room( AT_BLUE, get_room_index( ship->gunseat ), "Blue ion energy washes over the room!" );
      echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Rocket Launcher DAMAGED!" );
      ship->rocketstate = MISSILE_DAMAGED;
      return;
   }
   if( ship->statet0 != LASER_DAMAGED && ship->lasers > 0 && number_range( 1, disable ) <= idamage * 2 )
   {
      echo_to_room( AT_BLUE, get_room_index( ship->gunseat ), "Blue ion energy washes over the room!" );
      echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Lasers DAMAGED!" );
      ship->statet0 = LASER_DAMAGED;
      return;
   }
   if( ship->ionstate != LASER_DAMAGED && ship->ions > 0 && number_range( 1, disable ) <= idamage * 2 )
   {
      echo_to_room( AT_BLUE, get_room_index( ship->gunseat ), "Blue ion energy washes over the room!" );
      echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Ion Cannons Damaged!" );
      ship->ionstate = LASER_DAMAGED;
   }
}

void damage_ship( SHIP_DATA * ship, int min, int max )
{
   int sdamage, shield_dmg;

   sdamage = number_range( min, max );

   if( ship->shield > 0 )
   {
      shield_dmg = UMIN( ship->shield, sdamage );
      sdamage -= shield_dmg;
      ship->shield -= shield_dmg;
      if( ship->shield == 0 )
         echo_to_cockpit( AT_BLOOD, ship, "Shields down..." );
   }

   /* v1.22b: SWGD ship armor - flat per-hit absorption once shields are
    * gone (damage = damage - ship->armor; nothing through means no hull
    * loss and no disable rolls). Ion damage deliberately ignores armor,
    * matching SWGD. */
   sdamage -= ship->armor;
   if( sdamage <= 0 )
      return;

   if( sdamage > 0 )
   {
      if( number_range( 1, 100 ) <= 5 && ship->shipstate != SHIP_DISABLED )
      {
         echo_to_cockpit( AT_BLOOD + AT_BLINK, ship, "Ships Drive DAMAGED!" );
         ship->shipstate = SHIP_DISABLED;
      }

      if( number_range( 1, 100 ) <= 5 && ship->missilestate != MISSILE_DAMAGED && ship->maxmissiles > 0 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Missile Launcher DAMAGED!" );
         ship->missilestate = MISSILE_DAMAGED;
      }

      if( number_range( 1, 100 ) <= 5 && ship->torpedostate != MISSILE_DAMAGED && ship->maxtorpedos > 0 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Torpedo Launcher DAMAGED!" );
         ship->torpedostate = MISSILE_DAMAGED;
      }

      if( number_range( 1, 100 ) <= 5 && ship->rocketstate != MISSILE_DAMAGED && ship->maxrockets > 0 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->gunseat ), "Ships Rocket Launcher DAMAGED!" );
         ship->rocketstate = MISSILE_DAMAGED;
      }

      if( number_range( 1, 100 ) <= 2 && ship->statet1 != LASER_DAMAGED && ship->turret1 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->turret1 ), "Turret DAMAGED!" );
         ship->statet1 = LASER_DAMAGED;
      }

      if( number_range( 1, 100 ) <= 2 && ship->statet2 != LASER_DAMAGED && ship->turret2 )
      {
         echo_to_room( AT_BLOOD + AT_BLINK, get_room_index( ship->turret2 ), "Turret DAMAGED!" );
         ship->statet2 = LASER_DAMAGED;
      }
   }

   ship->hull -= sdamage * 5;

   if( ship->hull <= 0 )
   {
      destroy_ship( ship, NULL );
      return;
   }

   if( ship->hull <= ship->maxhull / 20 )
      echo_to_cockpit( AT_BLOOD + AT_BLINK, ship, "WARNING! Ship hull severely damaged!" );
}

void destroy_ship( SHIP_DATA * ship, CHAR_DATA * ch )
{
   char buf[MAX_STRING_LENGTH];
   int roomnum;
   ROOM_INDEX_DATA *room;
   OBJ_DATA *robj;
   CHAR_DATA *rch;

   snprintf( buf, MAX_STRING_LENGTH, "%s explodes in a blinding flash of light!", ship->name );
   echo_to_system( AT_WHITE + AT_BLINK, ship, buf, NULL );

   if( ship->ship_class == FIGHTER_SHIP )

      echo_to_ship( AT_WHITE + AT_BLINK, ship, "A blinding flahs of light burns your eyes..." );
   echo_to_ship( AT_WHITE, ship,
                 "But before you have a schance to scream...\r\nYou are ripped apart as your spacecraft explodes..." );

   for( roomnum = ship->firstroom; roomnum <= ship->lastroom; roomnum++ )
   {
      room = get_room_index( roomnum );

      if( room != NULL )
      {
         rch = room->first_person;
         while( rch )
         {
            if( IS_IMMORTAL( rch ) )
            {
               char_from_room( rch );
               char_to_room( rch, get_room_index( wherehome( rch ) ) );
            }
            else
            {
               if( ch )
                  raw_kill( ch, rch );
               else
                  raw_kill( rch, rch );
            }
            rch = room->first_person;
         }

         for( robj = room->first_content; robj; robj = robj->next_content )
         {
            separate_obj( robj );
            extract_obj( robj );
         }
      }
   }

   resetship( ship );
}

bool ship_to_room( SHIP_DATA * ship, int vnum )
{
   ROOM_INDEX_DATA *shipto;

   if( ( shipto = get_room_index( vnum ) ) == NULL )
      return FALSE;
   LINK( ship, shipto->first_ship, shipto->last_ship, next_in_room, prev_in_room );
   ship->in_room = shipto;
   return TRUE;
}

void do_board( CHAR_DATA * ch, const char *argument )
{
   ROOM_INDEX_DATA *toroom;
   SHIP_DATA *ship;
   SHIP_DATA *in_ship;

   if( !argument || argument[0] == '\0' )
   {
      send_to_char( "Board what?\r\n", ch );
      return;
   }

   /* v1.36: "board dock" - ported from SWGD's own do_board, which has
    * this exact special case. This is the missing link for crossing
    * from your ship into whatever ship is currently docked to it
    * (SHIP_DOCK_3, via v1.34/v1.35's docking-completion fix) - without
    * this, two fully docked ships had no way for anyone to actually
    * walk between them. Must be standing in YOUR ship's entrance room
    * specifically (not just anywhere aboard). */
   if( !str_cmp( argument, "dock" ) )
   {
      if( ( in_ship = ship_from_entrance( ch->in_room->vnum ) ) == NULL )
      {
         send_to_char( "&RYou realize you have to go to the entrance.\r\n", ch );
         return;
      }

      if( in_ship->shipstate2 != SHIP_DOCK_3 )
      {
         if( in_ship->shipstate2 == SHIP_DOCK_2 || in_ship->shipstate2 == SHIP_DOCK )
            send_to_char( "&RWait until the docking sequence is complete.\r\n", ch );
         else
            send_to_char( "&RThis ship isn't currently docked to anything.\r\n", ch );
         return;
      }
      if( IS_SET( in_ship->flags, SHIPFLAG_SIMULATOR ) )
      {
         send_to_char( "Your simulator doesn't lead into there.\r\n", ch );
         return;
      }
      ship = in_ship->docked_ship;
   }
   else if( ( ship = ship_in_room( ch->in_room, argument ) ) == NULL )
   {
      act( AT_PLAIN, "I see no $T here.", ch, NULL, argument, TO_CHAR );
      return;
   }

   if( IS_SET( ch->act, ACT_MOUNTED ) )
   {
      act( AT_PLAIN, "You can't go in there riding THAT.", ch, NULL, argument, TO_CHAR );
      return;
   }

   if( ( toroom = get_room_index( ship->entrance ) ) != NULL )
   {
      /* A fully docked ship's hatch doesn't need to be open - crossing
       * through a sealed docking connection isn't the same as opening
       * a hatch to vacuum. */
      if( !ship->hatchopen && !IS_DOCKED( ship ) )
      {
         send_to_char( "&RThe hatch is closed!\r\n", ch );
         return;
      }

      if( toroom->tunnel > 0 )
      {
         CHAR_DATA *ctmp;
         int count = 0;

         for( ctmp = toroom->first_person; ctmp; ctmp = ctmp->next_in_room )
            if( ++count >= toroom->tunnel )
            {
               send_to_char( "There is no room for you in there.\r\n", ch );
               return;
            }
      }
      if( ship->shipstate == SHIP_LAUNCH || ship->shipstate == SHIP_LAUNCH_2 )
      {
         send_to_char( "&rThat ship has already started launching!\r\n", ch );
         return;
      }

      act( AT_PLAIN, "$n enters $T.", ch, NULL, ship->name, TO_ROOM );
      act( AT_PLAIN, "You enter $T.", ch, NULL, ship->name, TO_CHAR );
      char_from_room( ch );
      char_to_room( ch, toroom );
      act( AT_PLAIN, "$n enters the ship.", ch, NULL, argument, TO_ROOM );
      do_look( ch, "auto" );

   }
   else
      send_to_char( "That ship has no entrance!\r\n", ch );
}

bool rent_ship( CHAR_DATA * ch, SHIP_DATA * ship )
{
   long price;

   if( IS_NPC( ch ) )
      return FALSE;

   price = get_ship_value( ship ) / 100;

   if( ch->gold < price )
   {
      ch_printf( ch, "&RRenting this ship costs %ld. You don't have enough credits!\r\n", price );
      return FALSE;
   }

   ch->gold -= price;
   ch_printf( ch, "&GYou pay %ld credits to rent the ship.\r\n", price );
   return TRUE;
}

void do_leaveship( CHAR_DATA * ch, const char *argument )
{
   ROOM_INDEX_DATA *fromroom;
   ROOM_INDEX_DATA *toroom;
   SHIP_DATA *ship;

   fromroom = ch->in_room;

   if( ( ship = ship_from_entrance( fromroom->vnum ) ) == NULL )
   {
      send_to_char( "I see no exit here.\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "You can't do that here.\r\n", ch );
      return;
   }

   if( ship->lastdoc != ship->location )
   {
      send_to_char( "&rMaybe you should wait until the ship lands.\r\n", ch );
      return;
   }

   if( ship->shipstate != SHIP_DOCKED && ship->shipstate != SHIP_DISABLED )
   {
      send_to_char( "&rPlease wait till the ship is properly docked.\r\n", ch );
      return;
   }

   if( !ship->hatchopen )
   {
      send_to_char( "&RYou need to open the hatch first", ch );
      return;
   }

   if( ( toroom = get_room_index( ship->location ) ) != NULL )
   {
      act( AT_PLAIN, "$n exits the ship.", ch, NULL, argument, TO_ROOM );
      act( AT_PLAIN, "You exit the ship.", ch, NULL, argument, TO_CHAR );
      char_from_room( ch );
      char_to_room( ch, toroom );
      act( AT_PLAIN, "$n steps out of a ship.", ch, NULL, argument, TO_ROOM );
      do_look( ch, "auto" );
   }
   else
      send_to_char( "The exit doesn't seem to be working properly.\r\n", ch );
}

void do_launch( CHAR_DATA * ch, const char *argument )
{
   int schance = 0;
   long price = 0;
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

   if( autofly( ship ) )
   {
      send_to_char( "&RThe ship is set on autopilot, you'll have to turn it off first.\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "You can't do that here.\r\n", ch );
      return;
   }

   if( !check_pilot( ch, ship ) )
   {
      send_to_char( "&RHey, thats not your ship! Try renting a public one.\r\n", ch );
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

   if( ship->ship_class == FIGHTER_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_starfighters] );
   if( ship->ship_class == MIDSIZE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_midships] );
   if( ship->ship_class == FRIGATE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_frigates] );
   if( ship->ship_class == CAPITAL_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_capitalships] );
   if( ship->ship_class == SUPERCAPITAL_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_supercapitalships] );
   if( number_percent(  ) < schance )
   {
      if( is_rental( ch, ship ) )
         if( !rent_ship( ch, ship ) )
            return;
      if( !is_rental( ch, ship ) )
      {
         if( ship->ship_class == FIGHTER_SHIP )
            price = 20;
         if( ship->ship_class == MIDSIZE_SHIP )
            price = 50;
         if( ship->ship_class == FRIGATE_SHIP )
            /* v1.32: was missing entirely - a rented frigate launch was
             * computing a base price of 0. Local number, slotted between
             * midship (50) and capital (500) - adjust if you want. */
            price = 200;
         if( ship->ship_class == CAPITAL_SHIP )
            price = 500;
         if( ship->ship_class == SUPERCAPITAL_SHIP )
            /* v1.32: same gap as frigate, above - local number continuing
             * the existing escalation past capital's 500. */
            price = 1000;

         price += ( ship->maxhull - ship->hull );
         if( ship->missiles )
            price += ( 50 * ( ship->maxmissiles - ship->missiles ) );
         else if( ship->torpedos )
            price += ( 75 * ( ship->maxtorpedos - ship->torpedos ) );
         else if( ship->rockets )
            price += ( 150 * ( ship->maxrockets - ship->rockets ) );

         if( ship->shipstate == SHIP_DISABLED )
            price += 200;
         if( ship->missilestate == MISSILE_DAMAGED )
            price += 100;
         if( ship->statet0 == LASER_DAMAGED )
            price += 50;
         if( ship->statet1 == LASER_DAMAGED )
            price += 50;
         if( ship->statet2 == LASER_DAMAGED )
            price += 50;
      }

      if( ch->pcdata && ch->pcdata->clan && !str_cmp( ch->pcdata->clan->name, ship->owner ) )
      {
         if( ch->pcdata->clan->funds < price )
         {
            ch_printf( ch, "&R%s doesn't have enough funds to prepare this ship for launch.\r\n", ch->pcdata->clan->name );
            return;
         }

         ch->pcdata->clan->funds -= price;
         ch_printf( ch, "&GIt costs %s %ld credits to ready this ship for launch.\r\n", ch->pcdata->clan->name, price );
      }
      else if( str_cmp( ship->owner, "Public" ) )
      {
         if( ch->gold < price )
         {
            ch_printf( ch, "&RYou don't have enough funds to prepare this ship for launch.\r\n" );
            return;
         }

         ch->gold -= price;
         ch_printf( ch, "&GYou pay %ld credits to ready the ship for launch.\r\n", price );

      }

      ship->energy = ship->maxenergy;
      ship->chaff = ship->maxchaff;
      ship->missiles = ship->maxmissiles;
      ship->torpedos = ship->maxtorpedos;
      ship->rockets = ship->maxrockets;
      ship->shield = 0;
      ship->autorecharge = FALSE;
      ship->autotrack = FALSE;
      ship->autospeed = FALSE;
      ship->hull = ship->maxhull;

      ship->missilestate = MISSILE_READY;
      ship->torpedostate = MISSILE_READY;
      ship->rocketstate = MISSILE_READY;
      ship->ionstate = LASER_READY;
      ship->statet0 = LASER_READY;
      ship->statet1 = LASER_READY;
      ship->statet2 = LASER_READY;
      ship->shipstate = SHIP_DOCKED;

      if( ship->hatchopen )
      {
         ship->hatchopen = FALSE;
         snprintf( buf, MAX_STRING_LENGTH, "The hatch on %s closes.", ship->name );
         echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
         echo_to_room( AT_YELLOW, get_room_index( ship->entrance ), "The hatch slides shut." );
         sound_to_room( get_room_index( ship->entrance ), "!!SOUND(door)" );
         sound_to_room( get_room_index( ship->location ), "!!SOUND(door)" );
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
      if( ship->ship_class == FRIGATE_SHIP )
         learn_from_success( ch, gsn_frigates );
      if( ship->ship_class == CAPITAL_SHIP )
         learn_from_success( ch, gsn_capitalships );
      if( ship->ship_class == SUPERCAPITAL_SHIP )
         learn_from_success( ch, gsn_supercapitalships );
      sound_to_ship( ship, "!!SOUND(xwing)" );
      return;
   }
   set_char_color( AT_RED, ch );
   send_to_char( "You fail to work the controls properly!\r\n", ch );
   if( ship->ship_class == FIGHTER_SHIP )
      learn_from_failure( ch, gsn_starfighters );
   if( ship->ship_class == MIDSIZE_SHIP )
      learn_from_failure( ch, gsn_midships );
   if( ship->ship_class == FRIGATE_SHIP )
      learn_from_failure( ch, gsn_frigates );
   if( ship->ship_class == CAPITAL_SHIP )
      learn_from_failure( ch, gsn_capitalships );
   if( ship->ship_class == SUPERCAPITAL_SHIP )
      learn_from_failure( ch, gsn_supercapitalships );
}

void launchship( SHIP_DATA * ship )
{
   char buf[MAX_STRING_LENGTH];
   SHIP_DATA *target;
   int plusminus;

   /*
    * v1.19 - simulator mode fix. do_togglesimulator/SHIPFLAG_SIMULATOR
    * and do_plot's Simulator-awareness were already fully ported, but
    * this redirect - the actual behavior that makes simulator mode do
    * anything - was missing entirely; launching always went to real
    * space regardless of the flag. Matches SWGD's real launchship().
    */
   if( IS_SET( ship->flags, SHIPFLAG_SIMULATOR ) )
   {
      SPACE_DATA *simul = starsystem_from_name( "Simulator" );

      if( simul == NULL )
      {
         echo_to_room( AT_YELLOW, get_room_index( ship->pilotseat ),
                       "Simulation core offline .. Launch aborted." );
         echo_to_ship( AT_YELLOW, ship, "The ship slowly sets back down on the landing pad." );
         snprintf( buf, MAX_STRING_LENGTH, "%s slowly sets back down.", ship->name );
         echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
         ship->shipstate = SHIP_DOCKED;
         return;
      }
      ship_to_starsystem( ship, simul );
   }
   else
      ship_to_starsystem( ship, starsystem_from_vnum( ship->location ) );

   if( ship->starsystem == NULL )
   {
      echo_to_room( AT_YELLOW, get_room_index( ship->pilotseat ), "Launch path blocked .. Launch aborted." );
      echo_to_ship( AT_YELLOW, ship, "The ship slowly sets back back down on the landing pad." );
      snprintf( buf, MAX_STRING_LENGTH, "%s slowly sets back down.", ship->name );
      echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
      ship->shipstate = SHIP_DOCKED;
      return;
   }

   if( ship->ship_class == MIDSIZE_SHIP )
   {
      sound_to_room( get_room_index( ship->location ), "!!SOUND(falcon)" );
      sound_to_ship( ship, "!!SOUND(falcon)" );
   }
   else if( ship->type == SHIP_IMPERIAL )
   {
      sound_to_ship( ship, "!!SOUND(tie)" );
      sound_to_room( get_room_index( ship->location ), "!!SOUND(tie)" );
   }
   else
   {
      sound_to_ship( ship, "!!SOUND(xwing)" );
      sound_to_room( get_room_index( ship->location ), "!!SOUND(xwing)" );
   }

   extract_ship( ship );

   ship->location = 0;

   if( ship->shipstate != SHIP_DISABLED )
      ship->shipstate = SHIP_READY;

   plusminus = number_range( -1, 2 );
   if( plusminus > 0 )
      ship->hx = 1;
   else
      ship->hx = -1;

   plusminus = number_range( -1, 2 );
   if( plusminus > 0 )
      ship->hy = 1;
   else
      ship->hy = -1;

   plusminus = number_range( -1, 2 );
   if( plusminus > 0 )
      ship->hz = 1;
   else
      ship->hz = -1;

   if( ship->lastdoc == ship->starsystem->doc1a ||
       ship->lastdoc == ship->starsystem->doc1b || ship->lastdoc == ship->starsystem->doc1c )
   {
      ship->vx = ship->starsystem->p1x;
      ship->vy = ship->starsystem->p1y;
      ship->vz = ship->starsystem->p1z;
   }
   else if( ship->lastdoc == ship->starsystem->doc2a ||
            ship->lastdoc == ship->starsystem->doc2b || ship->lastdoc == ship->starsystem->doc2c )
   {
      ship->vx = ship->starsystem->p2x;
      ship->vy = ship->starsystem->p2y;
      ship->vz = ship->starsystem->p2z;
   }
   else if( ship->lastdoc == ship->starsystem->doc3a ||
            ship->lastdoc == ship->starsystem->doc3b || ship->lastdoc == ship->starsystem->doc3c )
   {
      ship->vx = ship->starsystem->p3x;
      ship->vy = ship->starsystem->p3y;
      ship->vz = ship->starsystem->p3z;
   }
   else
   {
      for( target = ship->starsystem->first_ship; target; target = target->next_in_starsystem )
      {
         if( ship->lastdoc == target->hanger )
         {
            ship->vx = target->vx;
            ship->vy = target->vy;
            ship->vz = target->vz;
         }
      }
   }

   ship->energy -= ( 100 + 100 * ship->ship_class );

   ship->vx += ( ship->hx * ship->currspeed * 2 );
   ship->vy += ( ship->hy * ship->currspeed * 2 );
   ship->vz += ( ship->hz * ship->currspeed * 2 );

   echo_to_room( AT_GREEN, get_room_index( ship->pilotseat ), "Launch complete.\r\n" );
   echo_to_ship( AT_YELLOW, ship, "The ship leaves the platform far behind as it flies into space." );
   snprintf( buf, MAX_STRING_LENGTH, "%s enters the starsystem at %.0f %.0f %.0f", ship->name, ship->vx, ship->vy, ship->vz );
   echo_to_system( AT_YELLOW, ship, buf, NULL );
   snprintf( buf, MAX_STRING_LENGTH, "%s lifts off into space.", ship->name );
   echo_to_room( AT_YELLOW, get_room_index( ship->lastdoc ), buf );
}

void do_land( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int schance = 0;
   SHIP_DATA *ship;
   SHIP_DATA *target;
   int vx = 0, vy = 0, vz = 0;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

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
      send_to_char( "&RYou need to be in the pilot seat!\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&RYou'll have to turn off the ships autopilot first.\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RYou can't land platforms\r\n", ch );
      return;
   }

   if( ship->ship_class == FRIGATE_SHIP || ship->ship_class == CAPITAL_SHIP || ship->ship_class == SUPERCAPITAL_SHIP )
   {
      /* v1.32: was CAPITAL_SHIP-only - frigates and supercapitals are just
       * as (or more) oversized and were slipping through this check
       * entirely, contradicting do_launch's own per-class piloting skill
       * gates (gsn_frigates/gsn_supercapitalships) which clearly treat
       * these as their own large-hull tier. Mark: these three classes
       * stay in space permanently once launched - board via a shuttle
       * or fighter docking with them instead. */
      send_to_char( "&RThis ship is too big to land. You'll have to take a shuttle.\r\n", ch );
      return;
   }
   if( ship->shipstate == SHIP_DISABLED )
   {
      send_to_char( "&RThe ships drive is disabled. Unable to land.\r\n", ch );
      return;
   }
   if( ship->shipstate == SHIP_DOCKED )
   {
      send_to_char( "&RThe ship is already docked!\r\n", ch );
      return;
   }

   if( ship->shipstate2 == SHIP_DOCK || ship->shipstate2 == SHIP_DOCK_2 )
   {
      send_to_char( "&RNot while docking procedures are going on.\r\n", ch );
      return;
   }
   if( ship->shipstate2 == SHIP_DOCK_3 )
   {
      send_to_char( "&RDetach from the docked ship first.\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_HYPERSPACE )
   {
      send_to_char( "&RYou can only do that in realspace!\r\n", ch );
      return;
   }

   if( ship->shipstate != SHIP_READY )
   {
      send_to_char( "&RPlease wait until the ship has finished its current manouver.\r\n", ch );
      return;
   }
   if( ship->starsystem == NULL )
   {
      send_to_char( "&RThere's nowhere to land around here!", ch );
      return;
   }

   if( ship->energy < ( 25 + 25 * ship->ship_class ) )
   {
      send_to_char( "&RTheres not enough fuel!\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
   {
      set_char_color( AT_CYAN, ch );
      ch_printf( ch, "%s", "Land where?\r\n\r\nChoices: " );

      if( ship->starsystem->doc1a )
         ch_printf( ch, "%s (%s)  %d %d %d\r\n         ",
                    ship->starsystem->location1a,
                    ship->starsystem->planet1, ship->starsystem->p1x, ship->starsystem->p1y, ship->starsystem->p1z );
      if( ship->starsystem->doc1b )
         ch_printf( ch, "%s (%s)  %d %d %d\r\n         ",
                    ship->starsystem->location1b,
                    ship->starsystem->planet1, ship->starsystem->p1x, ship->starsystem->p1y, ship->starsystem->p1z );
      if( ship->starsystem->doc1c )
         ch_printf( ch, "%s (%s)  %d %d %d\r\n         ",
                    ship->starsystem->location1c,
                    ship->starsystem->planet1, ship->starsystem->p1x, ship->starsystem->p1y, ship->starsystem->p1z );
      if( ship->starsystem->doc2a )
         ch_printf( ch, "%s (%s)  %d %d %d\r\n         ",
                    ship->starsystem->location2a,
                    ship->starsystem->planet2, ship->starsystem->p2x, ship->starsystem->p2y, ship->starsystem->p2z );
      if( ship->starsystem->doc2b )
         ch_printf( ch, "%s (%s)  %d %d %d\r\n         ",
                    ship->starsystem->location2b,
                    ship->starsystem->planet2, ship->starsystem->p2x, ship->starsystem->p2y, ship->starsystem->p2z );
      if( ship->starsystem->doc2c )
         ch_printf( ch, "%s (%s)  %d %d %d\r\n         ",
                    ship->starsystem->location2c,
                    ship->starsystem->planet2, ship->starsystem->p2x, ship->starsystem->p2y, ship->starsystem->p2z );
      if( ship->starsystem->doc3a )
         ch_printf( ch, "%s (%s)  %d %d %d\r\n         ",
                    ship->starsystem->location3a,
                    ship->starsystem->planet3, ship->starsystem->p3x, ship->starsystem->p3y, ship->starsystem->p3z );
      if( ship->starsystem->doc3b )
         ch_printf( ch, "%s (%s)  %d %d %d\r\n         ",
                    ship->starsystem->location3b,
                    ship->starsystem->planet3, ship->starsystem->p3x, ship->starsystem->p3y, ship->starsystem->p3z );
      if( ship->starsystem->doc3c )
         ch_printf( ch, "%s (%s)  %d %d %d\r\n         ",
                    ship->starsystem->location3c,
                    ship->starsystem->planet3, ship->starsystem->p3x, ship->starsystem->p3y, ship->starsystem->p3z );
      for( target = ship->starsystem->first_ship; target; target = target->next_in_starsystem )
      {
         if( target->hanger > 0 && target != ship )
            ch_printf( ch, "%s    %.0f %.0f %.0f\r\n         ", target->name, target->vx, target->vy, target->vz );
      }
      ch_printf( ch, "\r\nYour Coordinates: %.0f %.0f %.0f\r\n", ship->vx, ship->vy, ship->vz );
      return;
   }

   if( str_prefix( argument, ship->starsystem->location1a ) &&
       str_prefix( argument, ship->starsystem->location2a ) &&
       str_prefix( argument, ship->starsystem->location3a ) &&
       str_prefix( argument, ship->starsystem->location1b ) &&
       str_prefix( argument, ship->starsystem->location2b ) &&
       str_prefix( argument, ship->starsystem->location3b ) &&
       str_prefix( argument, ship->starsystem->location1c ) &&
       str_prefix( argument, ship->starsystem->location2c ) && str_prefix( argument, ship->starsystem->location3c ) )
   {
      target = get_ship_here( argument, ship->starsystem );
      if( target == NULL )
      {
         send_to_char( "&RI don't see that here. Type land by itself for a list\r\n", ch );
         return;
      }
      if( target == ship )
      {
         send_to_char( "&RYou can't land your ship inside itself!\r\n", ch );
         return;
      }
      if( !target->hanger )
      {
         send_to_char( "&RThat ship has no hanger for you to land in!\r\n", ch );
         return;
      }
      if( ship->ship_class == MIDSIZE_SHIP && target->ship_class == MIDSIZE_SHIP )
      {
         send_to_char( "&RThat ship is not big enough for your ship to land in!\r\n", ch );
         return;
      }
      if( !target->bayopen )
      {
         send_to_char( "&RTheir hanger is closed. You'll have to ask them to open it for you\r\n", ch );
         return;
      }
      if( ( target->vx > ship->vx + 200 ) || ( target->vx < ship->vx - 200 ) ||
          ( target->vy > ship->vy + 200 ) || ( target->vy < ship->vy - 200 ) ||
          ( target->vz > ship->vz + 200 ) || ( target->vz < ship->vz - 200 ) )
      {
         send_to_char( "&R That ship is too far away! You'll have to fly a litlle closer.\r\n", ch );
         return;
      }
   }
   else
   {
      if( !str_prefix( argument, ship->starsystem->location3a ) ||
          !str_prefix( argument, ship->starsystem->location3b ) || !str_prefix( argument, ship->starsystem->location3c ) )
      {
         vx = ship->starsystem->p3x;
         vy = ship->starsystem->p3y;
         vz = ship->starsystem->p3z;
      }
      if( !str_prefix( argument, ship->starsystem->location2a ) ||
          !str_prefix( argument, ship->starsystem->location2b ) || !str_prefix( argument, ship->starsystem->location2c ) )
      {
         vx = ship->starsystem->p2x;
         vy = ship->starsystem->p2y;
         vz = ship->starsystem->p2z;
      }
      if( !str_prefix( argument, ship->starsystem->location1a ) ||
          !str_prefix( argument, ship->starsystem->location1b ) || !str_prefix( argument, ship->starsystem->location1c ) )
      {
         vx = ship->starsystem->p1x;
         vy = ship->starsystem->p1y;
         vz = ship->starsystem->p1z;
      }
      if( ( vx > ship->vx + 200 ) || ( vx < ship->vx - 200 ) ||
          ( vy > ship->vy + 200 ) || ( vy < ship->vy - 200 ) || ( vz > ship->vz + 200 ) || ( vz < ship->vz - 200 ) )
      {
         send_to_char( "&R That platform is too far away! You'll have to fly a litlle closer.\r\n", ch );
         return;
      }
   }

   if( ship->ship_class == FIGHTER_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_starfighters] );
   if( ship->ship_class == MIDSIZE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_midships] );
   if( number_percent(  ) < schance )
   {
      set_char_color( AT_GREEN, ch );
      send_to_char( "Landing sequence initiated.\r\n", ch );
      act( AT_PLAIN, "$n begins the landing sequence.", ch, NULL, argument, TO_ROOM );
      echo_to_ship( AT_YELLOW, ship, "The ship slowly begins its landing aproach." );
      ship->dest = STRALLOC( arg );
      ship->shipstate = SHIP_LAND;
      ship->currspeed = 0;
      if( ship->ship_class == FIGHTER_SHIP )
         learn_from_success( ch, gsn_starfighters );
      if( ship->ship_class == MIDSIZE_SHIP )
         learn_from_success( ch, gsn_midships );
      if( starsystem_from_vnum( ship->lastdoc ) != ship->starsystem )
      {
         int xp = ( exp_level( ch->skill_level[PILOTING_ABILITY] + 1 ) - exp_level( ch->skill_level[PILOTING_ABILITY] ) );
         xp = UMIN( get_ship_value( ship ), xp );
         gain_exp( ch, xp, PILOTING_ABILITY );
         ch_printf( ch, "&WYou gain %d points of flight experience!\r\n", xp );
      }
      return;
   }
   send_to_char( "You fail to work the controls properly.\r\n", ch );
   if( ship->ship_class == FIGHTER_SHIP )
      learn_from_failure( ch, gsn_starfighters );
   else
      learn_from_failure( ch, gsn_midships );
}

void landship( SHIP_DATA * ship, const char *arg )
{
   SHIP_DATA *target;
   char buf[MAX_STRING_LENGTH];
   int destination = 0;

   if( !str_prefix( arg, ship->starsystem->location3a ) )
      destination = ship->starsystem->doc3a;
   if( !str_prefix( arg, ship->starsystem->location3b ) )
      destination = ship->starsystem->doc3b;
   if( !str_prefix( arg, ship->starsystem->location3c ) )
      destination = ship->starsystem->doc3c;
   if( !str_prefix( arg, ship->starsystem->location2a ) )
      destination = ship->starsystem->doc2a;
   if( !str_prefix( arg, ship->starsystem->location2b ) )
      destination = ship->starsystem->doc2b;
   if( !str_prefix( arg, ship->starsystem->location2c ) )
      destination = ship->starsystem->doc2c;
   if( !str_prefix( arg, ship->starsystem->location1a ) )
      destination = ship->starsystem->doc1a;
   if( !str_prefix( arg, ship->starsystem->location1b ) )
      destination = ship->starsystem->doc1b;
   if( !str_prefix( arg, ship->starsystem->location1c ) )
      destination = ship->starsystem->doc1c;

   target = get_ship_here( arg, ship->starsystem );
   if( target != ship && target != NULL && target->bayopen
       && ( ship->ship_class != MIDSIZE_SHIP || target->ship_class != MIDSIZE_SHIP ) )
      destination = target->hanger;

   if( !ship_to_room( ship, destination ) )
   {
      echo_to_room( AT_YELLOW, get_room_index( ship->pilotseat ), "Could not complete aproach. Landing aborted." );
      echo_to_ship( AT_YELLOW, ship, "The ship pulls back up out of its landing sequence." );
      if( ship->shipstate != SHIP_DISABLED )
         ship->shipstate = SHIP_READY;
      return;
   }

   echo_to_room( AT_YELLOW, get_room_index( ship->pilotseat ), "Landing sequence complete." );
   echo_to_ship( AT_YELLOW, ship, "You feel a slight thud as the ship sets down on the ground." );
   snprintf( buf, MAX_STRING_LENGTH, "%s disapears from your scanner.", ship->name );
   echo_to_system( AT_YELLOW, ship, buf, NULL );

   ship->location = destination;
   ship->lastdoc = ship->location;
   if( ship->shipstate != SHIP_DISABLED )
      ship->shipstate = SHIP_DOCKED;
   ship_from_starsystem( ship, ship->starsystem );

   snprintf( buf, MAX_STRING_LENGTH, "%s lands on the platform.", ship->name );
   echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );

   ship->energy = ship->energy - 25 - 25 * ship->ship_class;

   if( !str_cmp( "Public", ship->owner ) )
   {
      ship->energy = ship->maxenergy;
      ship->chaff = ship->maxchaff;
      ship->missiles = ship->maxmissiles;
      ship->torpedos = ship->maxtorpedos;
      ship->rockets = ship->maxrockets;
      ship->shield = 0;
      ship->autorecharge = FALSE;
      ship->autotrack = FALSE;
      ship->autospeed = FALSE;
      ship->hull = ship->maxhull;

      ship->missilestate = MISSILE_READY;
      ship->torpedostate = MISSILE_READY;
      ship->rocketstate = MISSILE_READY;
      ship->ionstate = LASER_READY;
      ship->statet0 = LASER_READY;
      ship->statet1 = LASER_READY;
      ship->statet2 = LASER_READY;
      ship->shipstate = SHIP_DOCKED;

      echo_to_cockpit( AT_YELLOW, ship, "Repairing and refueling ship..." );
   }

   save_ship( ship );
}

void do_accelerate( CHAR_DATA * ch, const char *argument )
{
   int schance = 0;
   int change;
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
      send_to_char( "&RThe controls must be at the pilots chair...\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&RYou'll have to turn off the ships autopilot first.\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RPlatforms can't move!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_HYPERSPACE )
   {
      send_to_char( "&RYou can only do that in realspace!\r\n", ch );
      return;
   }
   if( ship->shipstate == SHIP_DISABLED )
   {
      send_to_char( "&RThe ships drive is disabled. Unable to accelerate.\r\n", ch );
      return;
   }
   if( ship->shipstate == SHIP_DOCKED )
   {
      send_to_char( "&RYou can't do that until after you've launched!\r\n", ch );
      return;
   }

   if( ship->shipstate2 == SHIP_DOCK || ship->shipstate2 == SHIP_DOCK_2 )
   {
      send_to_char( "&RNot while docking procedures are going on.\r\n", ch );
      return;
   }
   if( ship->shipstate2 == SHIP_DOCK_3 )
   {
      send_to_char( "&RDetach from the docked ship first.\r\n", ch );
      return;
   }

   if( ship->energy < abs( ( atoi( argument ) - abs( ship->currspeed ) ) / 10 ) )
   {
      send_to_char( "&RTheres not enough fuel!\r\n", ch );
      return;
   }

   if( ship->ship_class == FIGHTER_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_starfighters] );
   if( ship->ship_class == MIDSIZE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_midships] );
   if( ship->ship_class == FRIGATE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_frigates] );
   if( ship->ship_class == CAPITAL_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_capitalships] );
   if( ship->ship_class == SUPERCAPITAL_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_supercapitalships] );
   if( number_percent(  ) >= schance )
   {
      send_to_char( "&RYou fail to work the controls properly.\r\n", ch );
      if( ship->ship_class == FIGHTER_SHIP )
         learn_from_failure( ch, gsn_starfighters );
      if( ship->ship_class == MIDSIZE_SHIP )
         learn_from_failure( ch, gsn_midships );
      if( ship->ship_class == FRIGATE_SHIP )
         learn_from_failure( ch, gsn_frigates );
      if( ship->ship_class == CAPITAL_SHIP )
         learn_from_failure( ch, gsn_capitalships );
      if( ship->ship_class == SUPERCAPITAL_SHIP )
         learn_from_failure( ch, gsn_supercapitalships );
      return;
   }

   change = atoi( argument );

   act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument, TO_ROOM );

   if( change > ship->currspeed )
   {
      send_to_char( "&GAccelerating\r\n", ch );
      echo_to_cockpit( AT_YELLOW, ship, "The ship begins to accelerate." );
      snprintf( buf, MAX_STRING_LENGTH, "%s begins to speed up.", ship->name );
      echo_to_system( AT_ORANGE, ship, buf, NULL );
   }

   if( change < ship->currspeed )
   {
      send_to_char( "&GDecelerating\r\n", ch );
      echo_to_cockpit( AT_YELLOW, ship, "The ship begins to slow down." );
      snprintf( buf, MAX_STRING_LENGTH, "%s begins to slow down.", ship->name );
      echo_to_system( AT_ORANGE, ship, buf, NULL );
   }

   ship->energy -= abs( ( change - abs( ship->currspeed ) ) / 10 );

   ship->currspeed = URANGE( 0, change, ship->realspeed );

   if( ship->ship_class == FIGHTER_SHIP )
      learn_from_success( ch, gsn_starfighters );
   if( ship->ship_class == MIDSIZE_SHIP )
      learn_from_success( ch, gsn_midships );
   if( ship->ship_class == FRIGATE_SHIP )
      learn_from_success( ch, gsn_frigates );
   if( ship->ship_class == CAPITAL_SHIP )
      learn_from_success( ch, gsn_capitalships );
   if( ship->ship_class == SUPERCAPITAL_SHIP )
      learn_from_success( ch, gsn_supercapitalships );
}

void do_trajectory( CHAR_DATA * ch, const char *argument )
{
   char buf[MAX_STRING_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   char arg3[MAX_INPUT_LENGTH];
   int schance = 0;
   float vx, vy, vz;
   SHIP_DATA *ship;

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
      send_to_char( "&RYour not in the pilots seat.\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&RYou'll have to turn off the ships autopilot first.\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_DISABLED )
   {
      send_to_char( "&RThe ships drive is disabled. Unable to manuever.\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RPlatforms can't turn!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_HYPERSPACE )
   {
      send_to_char( "&RYou can only do that in realspace!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_DOCKED )
   {
      send_to_char( "&RYou can't do that until after you've launched!\r\n", ch );
      return;
   }

   if( ship->shipstate != SHIP_READY )
   {
      send_to_char( "&RPlease wait until the ship has finished its current manouver.\r\n", ch );
      return;
   }

   if( ship->energy < ( ship->currspeed / 10 ) )
   {
      send_to_char( "&RTheres not enough fuel!\r\n", ch );
      return;
   }

   if( ship->ship_class == FIGHTER_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_starfighters] );
   if( ship->ship_class == MIDSIZE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_midships] );
   if( ship->ship_class == FRIGATE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_frigates] );
   if( ship->ship_class == CAPITAL_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_capitalships] );
   if( ship->ship_class == SUPERCAPITAL_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_supercapitalships] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "&RYou fail to work the controls properly.\r\n", ch );
      if( ship->ship_class == FIGHTER_SHIP )
         learn_from_failure( ch, gsn_starfighters );
      if( ship->ship_class == MIDSIZE_SHIP )
         learn_from_failure( ch, gsn_midships );
      if( ship->ship_class == FRIGATE_SHIP )
         learn_from_failure( ch, gsn_frigates );
      if( ship->ship_class == CAPITAL_SHIP )
         learn_from_failure( ch, gsn_capitalships );
      if( ship->ship_class == SUPERCAPITAL_SHIP )
         learn_from_failure( ch, gsn_supercapitalships );
      return;
   }

   argument = one_argument( argument, arg2 );
   argument = one_argument( argument, arg3 );

   vx = atof( arg2 );
   vy = atof( arg3 );
   vz = atof( argument );

   if( vx == ship->vx && vy == ship->vy && vz == ship->vz )
   {
      ch_printf( ch, "The ship is already at %.0f %.0f %.0f !", vx, vy, vz );
   }

   ship->hx = vx - ship->vx;
   ship->hy = vy - ship->vy;
   ship->hz = vz - ship->vz;

   ship->energy -= ( ship->currspeed / 10 );

   ch_printf( ch, "&GNew course set, aproaching %.0f %.0f %.0f.\r\n", vx, vy, vz );
   act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument, TO_ROOM );

   echo_to_cockpit( AT_YELLOW, ship, "The ship begins to turn.\r\n" );
   snprintf( buf, MAX_STRING_LENGTH, "%s turns altering its present course.", ship->name );
   echo_to_system( AT_ORANGE, ship, buf, NULL );

   if( ship->ship_class == FIGHTER_SHIP || ( ship->ship_class == MIDSIZE_SHIP && ship->manuever > 50 ) )
      ship->shipstate = SHIP_BUSY_3;
   else if( ship->ship_class == MIDSIZE_SHIP || ( ship->ship_class == CAPITAL_SHIP && ship->manuever > 50 ) )
      ship->shipstate = SHIP_BUSY_2;
   else
      ship->shipstate = SHIP_BUSY;

   if( ship->ship_class == FIGHTER_SHIP )
      learn_from_success( ch, gsn_starfighters );
   if( ship->ship_class == MIDSIZE_SHIP )
      learn_from_success( ch, gsn_midships );
   if( ship->ship_class == FRIGATE_SHIP )
      learn_from_success( ch, gsn_frigates );
   if( ship->ship_class == CAPITAL_SHIP )
      learn_from_success( ch, gsn_capitalships );
   if( ship->ship_class == SUPERCAPITAL_SHIP )
      learn_from_success( ch, gsn_supercapitalships );
}

void do_buyship( CHAR_DATA * ch, const char *argument )
{
   long price;
   SHIP_DATA *ship;

   if( IS_NPC( ch ) || !ch->pcdata )
   {
      send_to_char( "&ROnly players can do that!\r\n", ch );
      return;
   }

   ship = ship_in_room( ch->in_room, argument );
   if( !ship )
   {
      ship = ship_from_cockpit( ch->in_room->vnum );

      if( !ship )
      {
         act( AT_PLAIN, "I see no $T here.", ch, NULL, argument, TO_CHAR );
         return;
      }
   }

   if( str_cmp( ship->owner, "" ) || ship->type == MOB_SHIP )
   {
      send_to_char( "&RThat ship isn't for sale!", ch );
      return;
   }


   if( ship->type == SHIP_IMPERIAL )
   {
      if( !ch->pcdata->clan || str_cmp( ch->pcdata->clan->name, "the empire" ) )
      {
         if( !ch->pcdata->clan || !ch->pcdata->clan->mainclan || str_cmp( ch->pcdata->clan->mainclan->name, "The Empire" ) )
         {
            send_to_char( "&RThat ship may only be purchaced by the Empire!\r\n", ch );
            return;
         }
      }
   }
   else if( ship->type == SHIP_REPUBLIC )
   {
      if( !ch->pcdata->clan || str_cmp( ch->pcdata->clan->name, "the new republic" ) )
      {
         if( !ch->pcdata->clan || !ch->pcdata->clan->mainclan
             || str_cmp( ch->pcdata->clan->mainclan->name, "The New Republic" ) )
         {
            send_to_char( "&RThat ship may only be purchaced by The New Republic!\r\n", ch );
            return;
         }
      }
   }
   else
   {
      if( ch->pcdata->clan &&
          ( !str_cmp( ch->pcdata->clan->name, "the new republic" ) ||
            ( ch->pcdata->clan->mainclan && !str_cmp( ch->pcdata->clan->mainclan->name, "the new republic" ) ) ) )
      {
         send_to_char( "&RAs a member of the New Republic you may only purchase NR Ships!\r\n", ch );
         return;
      }
      if( ch->pcdata->clan &&
          ( !str_cmp( ch->pcdata->clan->name, "the empire" ) ||
            ( ch->pcdata->clan->mainclan && !str_cmp( ch->pcdata->clan->mainclan->name, "the empire" ) ) ) )
      {
         send_to_char( "&RAs a member of the Empire you may only purchase Imperial Ships!\r\n", ch );
         return;
      }
   }

   price = get_ship_value( ship );

   if( ch->gold < price )
   {
      ch_printf( ch, "&RThis ship costs %ld. You don't have enough credits!\r\n", price );
      return;
   }

   ch->gold -= price;
   ch_printf( ch, "&GYou pay %ld credits to purchace the ship.\r\n", price );

   act( AT_PLAIN, "$n walks over to a terminal and makes a credit transaction.", ch, NULL, argument, TO_ROOM );

   STRFREE( ship->owner );
   ship->owner = STRALLOC( ch->name );
   save_ship( ship );
}

void do_clanbuyship( CHAR_DATA * ch, const char *argument )
{
   long price;
   SHIP_DATA *ship;
   CLAN_DATA *clan;
   CLAN_DATA *mainclan;

   if( IS_NPC( ch ) || !ch->pcdata )
   {
      send_to_char( "&ROnly players can do that!\r\n", ch );
      return;
   }
   if( !ch->pcdata->clan )
   {
      send_to_char( "&RYou aren't a member of any organizations!\r\n", ch );
      return;
   }

   clan = ch->pcdata->clan;
   mainclan = ch->pcdata->clan->mainclan ? ch->pcdata->clan->mainclan : clan;

   if( ( ch->pcdata->bestowments
         && is_name( "clanbuyship", ch->pcdata->bestowments ) ) || !str_cmp( ch->name, clan->leader ) )
      ;
   else
   {
      send_to_char( "&RYour organization hasn't seen fit to bestow you with that ability.\r\n", ch );
      return;
   }

   ship = ship_in_room( ch->in_room, argument );
   if( !ship )
   {
      ship = ship_from_cockpit( ch->in_room->vnum );

      if( !ship )
      {
         act( AT_PLAIN, "I see no $T here.", ch, NULL, argument, TO_CHAR );
         return;
      }
   }

   if( str_cmp( ship->owner, "" ) || ship->type == MOB_SHIP )
   {
      send_to_char( "&RThat ship isn't for sale!\r\n", ch );
      return;
   }

   if( str_cmp( mainclan->name, "The Empire" ) && ship->type == SHIP_IMPERIAL )
   {
      send_to_char( "&RThat ship may only be purchaced by the Empire!\r\n", ch );
      return;
   }

   if( str_cmp( mainclan->name, "The New Republic" ) && ship->type == SHIP_REPUBLIC )
   {
      send_to_char( "&RThat ship may only be purchaced by The New Republic!\r\n", ch );
      return;
   }

   if( !str_cmp( mainclan->name, "The Empire" ) && ship->type != SHIP_IMPERIAL )
   {
      send_to_char( "&RDue to contractual agreements that ship may not be purchaced by the empire!\r\n", ch );
      return;
   }

   if( !str_cmp( mainclan->name, "The New Republic" ) && ship->type != SHIP_REPUBLIC )
   {
      send_to_char( "&RBecause of contractual agreements, the NR can only purchase NR ships!\r\n", ch );
      return;
   }

   price = get_ship_value( ship );

   if( ch->pcdata->clan->funds < price )
   {
      ch_printf( ch, "&RThis ship costs %ld. You don't have enough credits!\r\n", price );
      return;
   }

   clan->funds -= price;
   ch_printf( ch, "&G%s pays %ld credits to purchace the ship.\r\n", clan->name, price );

   act( AT_PLAIN, "$n walks over to a terminal and makes a credit transaction.", ch, NULL, argument, TO_ROOM );

   STRFREE( ship->owner );
   ship->owner = STRALLOC( clan->name );
   save_ship( ship );

   if( ship->ship_class <= SHIP_PLATFORM )
      clan->spacecraft++;
   else
      clan->vehicles++;
}

void do_sellship( CHAR_DATA * ch, const char *argument )
{
   long price;
   SHIP_DATA *ship;

   ship = ship_in_room( ch->in_room, argument );
   if( !ship )
   {
      act( AT_PLAIN, "I see no $T here.", ch, NULL, argument, TO_CHAR );
      return;
   }

   if( str_cmp( ship->owner, ch->name ) )
   {
      send_to_char( "&RThat isn't your ship!", ch );
      return;
   }

   price = get_ship_value( ship );

   ch->gold += ( price - price / 10 );
   ch_printf( ch, "&GYou receive %ld credits from selling your ship.\r\n", price - price / 10 );

   act( AT_PLAIN, "$n walks over to a terminal and makes a credit transaction.", ch, NULL, argument, TO_ROOM );

   STRFREE( ship->owner );
   ship->owner = STRALLOC( "" );
   save_ship( ship );
}

void do_clansellship( CHAR_DATA * ch, const char *argument )
{
   long price;
   SHIP_DATA *ship;
   CLAN_DATA *clan;

   if( !ch->pcdata->clan )
   {
      send_to_char( "&RYou aren't a member of any organization!\r\n", ch );
      return;
   }
   clan = ch->pcdata->clan->mainclan ? ch->pcdata->clan->mainclan : ch->pcdata->clan;

   ship = ship_in_room( ch->in_room, argument );
   if( !ship )
   {
      act( AT_PLAIN, "I see no $T here.", ch, NULL, argument, TO_CHAR );
      return;
   }

   if( str_cmp( ship->owner, clan->name ) )
   {
      send_to_char( "&RThat isn't your clan's ship!\r\n", ch );
      return;
   }

   if( !( ( ch->pcdata->bestowments && is_name( "clansellship", ch->pcdata->bestowments ) )
          || !str_cmp( ch->name, clan->leader ) ) )
   {
      send_to_char( "&RYour organization hasn't seen fit to bestow you with that ability.\r\n", ch );
      return;
   }

   price = get_ship_value( ship );
   clan->funds += price;
   ch_printf( ch, "&GYour clan receives %ld credits from selling the ship.\r\n", price );

   act( AT_PLAIN, "$n walks over to a terminal and makes a credit transaction.", ch, NULL, argument, TO_ROOM );

   STRFREE( ship->owner );
   STRFREE( ship->pilot );
   STRFREE( ship->copilot );
   ship->owner   = STRALLOC( "" );
   ship->pilot   = STRALLOC( "" );
   ship->copilot = STRALLOC( "" );
   save_ship( ship );
   if( ship->ship_class <= SHIP_PLATFORM )
      clan->spacecraft--;
   else
      clan->vehicles--;
   save_clan( clan );
}

void do_info( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   SHIP_DATA *target;

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      if( argument[0] == '\0' )
      {
         act( AT_PLAIN, "Which ship do you want info on?.", ch, NULL, NULL, TO_CHAR );
         return;
      }

      ship = ship_in_room( ch->in_room, argument );
      if( !ship )
      {
         act( AT_PLAIN, "I see no $T here.", ch, NULL, argument, TO_CHAR );
         return;
      }

      target = ship;
   }
   else if( argument[0] == '\0' )
      target = ship;
   else
      target = get_ship_here( argument, ship->starsystem );

   if( target == NULL )
   {
      send_to_char( "&RI don't see that here.\r\nTry the radar, or type info by itself for info on this ship.\r\n", ch );
      return;
   }

   if( abs( ( int )( target->vx - ship->vx ) ) > 500 + ship->sensor * 2
       || abs( ( int )( target->vy - ship->vy ) ) > 500 + ship->sensor * 2
       || abs( ( int )( target->vz - ship->vz ) ) > 500 + ship->sensor * 2 )
   {
      send_to_char( "&RThat ship is to far away to scan.\r\n", ch );
      return;
   }

   ch_printf( ch, "&Y%s %s : %s %s\r\n&B",
              target->type == SHIP_REPUBLIC ? "New Republic" :
              ( target->type == SHIP_IMPERIAL ? "Imperial" : "Civilian" ),
              target->ship_class == FIGHTER_SHIP ? "Starfighter" :
              ( target->ship_class == MIDSIZE_SHIP ? "Midtarget" :
                ( target->ship_class == FRIGATE_SHIP ? "Frigate" :
                ( target->ship_class == SUPERCAPITAL_SHIP ? "Supercapital Ship" :
                ( target->ship_class == CAPITAL_SHIP ? "Capital Ship" :
                  ( ship->ship_class == SHIP_PLATFORM ? "Platform" :
                    ( ship->ship_class == CLOUD_CAR ? "Cloudcar" :
                      ( ship->ship_class == OCEAN_SHIP ? "Boat" :
                        ( ship->ship_class == LAND_SPEEDER ? "Speeder" :
                          ( ship->ship_class == WHEELED ? "Wheeled Transport" :
                            ( ship->ship_class == LAND_CRAWLER ? "Crawler" :
                              ( ship->ship_class == WALKER ? "Walker" : "Unknown" ) ) ) ) ) ) ) ) ) ) ),
              target->name, target->filename );
   ch_printf( ch, "Description: %s\r\nOwner: %s   Pilot: %s   Copilot: %s\r\n",
              target->description, target->owner, target->pilot, target->copilot );
   ch_printf( ch, "Laser cannons: %d  ", target->lasers );
   ch_printf( ch, "Maximum Missiles: %d  ", target->maxmissiles );
   ch_printf( ch, "Max Chaff: %d\r\n", target->maxchaff );
   if( target->autocannon == 1 )
   {
      ch_printf( ch, "&YAutocannon installed. %d shots at %d strength max&B\r\n",
                 target->autoammomax, target->autodamage );
   }
   if( target->interdict == 1 )
   {
      send_to_char( "&YInterdiction System Installed!!!&B\r\n", ch );
   }
   if( IS_SET( target->flags, SHIPFLAG_SIMULATOR ) )
   {
      send_to_char( "&YThis Craft is a Ship Simulator!!&B\r\n", ch );
   }
   if( target->cloak == 1 )
      send_to_char( "&YThis craft has a Cloaking Device!!&B\r\n", ch );
   ch_printf( ch, "Max Hull: %d  ", target->maxhull );
   ch_printf( ch, "Armor ( %d/%d )  Max Chaff: %d\r\n", target->armor, target->maxarmor, target->maxchaff );
   ch_printf( ch, "Max Shields: %d   Max Energy(fuel): %d\r\n", target->maxshield, target->maxenergy );
   ch_printf( ch, "Maximum Speed: %d   Hyperspeed: %d  Maximum Cargo: %d\r\n",
              target->realspeed, target->hyperspeed, target->maxcargo );

   act( AT_PLAIN, "$n checks various gages and displays on the control panel.", ch, NULL, argument, TO_ROOM );
}

void do_autorecharge( CHAR_DATA * ch, const char *argument )
{
   int schance;
   SHIP_DATA *ship;
   int recharge;

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the cockpit of a ship to do that!\r\n", ch );
      return;
   }

   if( ( ship = ship_from_coseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the co-pilots seat!\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&RYou'll have to turn off the ships autopilot first.\r\n", ch );
      return;
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_shipsystems] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "&RYou fail to work the controls properly.\r\n", ch );
      learn_from_failure( ch, gsn_shipsystems );
      return;
   }

   act( AT_PLAIN, "$n flips a switch on the control panell.", ch, NULL, argument, TO_ROOM );

   if( !str_cmp( argument, "on" ) )
   {
      ship->autorecharge = TRUE;
      send_to_char( "&GYou power up the shields.\r\n", ch );
      echo_to_cockpit( AT_YELLOW, ship, "Shields ON. Autorecharge ON." );
   }
   else if( !str_cmp( argument, "off" ) )
   {
      ship->autorecharge = FALSE;
      send_to_char( "&GYou shutdown the shields.\r\n", ch );
      echo_to_cockpit( AT_YELLOW, ship, "Shields OFF. Shield strength set to 0. Autorecharge OFF." );
      ship->shield = 0;
   }
   else if( !str_cmp( argument, "idle" ) )
   {
      ship->autorecharge = FALSE;
      send_to_char( "&GYou let the shields idle.\r\n", ch );
      echo_to_cockpit( AT_YELLOW, ship, "Autorecharge OFF. Shields IDLEING." );
   }
   else
   {
      if( ship->autorecharge == TRUE )
      {
         ship->autorecharge = FALSE;
         send_to_char( "&GYou toggle the shields.\r\n", ch );
         echo_to_cockpit( AT_YELLOW, ship, "Autorecharge OFF. Shields IDLEING." );
      }
      else
      {
         ship->autorecharge = TRUE;
         send_to_char( "&GYou toggle the shields.\r\n", ch );
         echo_to_cockpit( AT_YELLOW, ship, "Shields ON. Autorecharge ON" );
      }
   }

   if( ship->autorecharge )
   {
      recharge = URANGE( 1, ship->maxshield - ship->shield, 25 + ship->ship_class * 25 );
      recharge = UMIN( recharge, ship->energy * 5 + 100 );
      ship->shield += recharge;
      ship->energy -= ( recharge * 2 + recharge * ship->ship_class );
   }

   learn_from_success( ch, gsn_shipsystems );
}

void do_autopilot( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the cockpit of a ship to do that!\r\n", ch );
      return;
   }

   if( ( ship = ship_from_pilotseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the pilots seat!\r\n", ch );
      return;
   }

   if( !check_pilot( ch, ship ) )
   {
      send_to_char( "&RHey! Thats not your ship!\r\n", ch );
      return;
   }

   if( ship->target0 || ship->target1 || ship->target2 )
   {
      send_to_char( "&RNot while the ship is enganged with an enemy!\r\n", ch );
      return;
   }


   act( AT_PLAIN, "$n flips a switch on the control panell.", ch, NULL, argument, TO_ROOM );

   if( ship->autopilot == TRUE )
   {
      ship->autopilot = FALSE;
      send_to_char( "&GYou toggle the autopilot.\r\n", ch );
      echo_to_cockpit( AT_YELLOW, ship, "Autopilot OFF." );
   }
   else
   {
      ship->autopilot = TRUE;
      ship->autorecharge = TRUE;
      send_to_char( "&GYou toggle the autopilot.\r\n", ch );
      echo_to_cockpit( AT_YELLOW, ship, "Autopilot ON." );
   }
}

void do_openhatch( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   char buf[MAX_STRING_LENGTH];

   if( !argument || argument[0] == '\0' || !str_cmp( argument, "hatch" ) )
   {
      ship = ship_from_entrance( ch->in_room->vnum );
      if( ship == NULL )
      {
         send_to_char( "&ROpen what?\r\n", ch );
         return;
      }
      else
      {
         if( !ship->hatchopen )
         {

            if( ship->ship_class == SHIP_PLATFORM )
            {
               send_to_char( "&RTry one of the docking bays!\r\n", ch );
               return;
            }
            if( ship->location != ship->lastdoc || ( ship->shipstate != SHIP_DOCKED && ship->shipstate != SHIP_DISABLED ) )
            {
               send_to_char( "&RPlease wait till the ship lands!\r\n", ch );
               return;
            }
            ship->hatchopen = TRUE;
            send_to_char( "&GYou open the hatch.\r\n", ch );
            act( AT_PLAIN, "$n opens the hatch.", ch, NULL, argument, TO_ROOM );
            snprintf( buf, MAX_STRING_LENGTH, "The hatch on %s opens.", ship->name );
            echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
            sound_to_room( get_room_index( ship->entrance ), "!!SOUND(door)" );
            sound_to_room( get_room_index( ship->location ), "!!SOUND(door)" );
            return;
         }
         else
         {
            send_to_char( "&RIt's already open.\r\n", ch );
            return;
         }
      }
   }

   ship = ship_in_room( ch->in_room, argument );
   if( !ship )
   {
      act( AT_PLAIN, "I see no $T here.", ch, NULL, argument, TO_CHAR );
      return;
   }

   if( ship->shipstate != SHIP_DOCKED && ship->shipstate != SHIP_DISABLED )
   {
      send_to_char( "&RThat ship has already started to launch", ch );
      return;
   }

   if( !check_pilot( ch, ship ) )
   {
      send_to_char( "&RHey! Thats not your ship!\r\n", ch );
      return;
   }

   if( !ship->hatchopen )
   {
      ship->hatchopen = TRUE;
      act( AT_PLAIN, "You open the hatch on $T.", ch, NULL, ship->name, TO_CHAR );
      act( AT_PLAIN, "$n opens the hatch on $T.", ch, NULL, ship->name, TO_ROOM );
      echo_to_room( AT_YELLOW, get_room_index( ship->entrance ), "The hatch opens from the outside." );
      sound_to_room( get_room_index( ship->entrance ), "!!SOUND(door)" );
      sound_to_room( get_room_index( ship->location ), "!!SOUND(door)" );
      return;
   }

   send_to_char( "&GIts already open!\r\n", ch );
}

void do_closehatch( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   char buf[MAX_STRING_LENGTH];

   if( !argument || argument[0] == '\0' || !str_cmp( argument, "hatch" ) )
   {
      ship = ship_from_entrance( ch->in_room->vnum );
      if( ship == NULL )
      {
         send_to_char( "&RClose what?\r\n", ch );
         return;
      }
      else
      {

         if( ship->ship_class == SHIP_PLATFORM )
         {
            send_to_char( "&RTry one of the docking bays!\r\n", ch );
            return;
         }
         if( ship->hatchopen )
         {
            ship->hatchopen = FALSE;
            send_to_char( "&GYou close the hatch.\r\n", ch );
            act( AT_PLAIN, "$n closes the hatch.", ch, NULL, argument, TO_ROOM );
            snprintf( buf, MAX_STRING_LENGTH, "The hatch on %s closes.", ship->name );
            echo_to_room( AT_YELLOW, get_room_index( ship->location ), buf );
            sound_to_room( get_room_index( ship->entrance ), "!!SOUND(door)" );
            sound_to_room( get_room_index( ship->location ), "!!SOUND(door)" );
            return;
         }
         else
         {
            send_to_char( "&RIt's already closed.\r\n", ch );
            return;
         }
      }
   }

   ship = ship_in_room( ch->in_room, argument );
   if( !ship )
   {
      act( AT_PLAIN, "I see no $T here.", ch, NULL, argument, TO_CHAR );
      return;
   }

   if( ship->shipstate != SHIP_DOCKED && ship->shipstate != SHIP_DISABLED )
   {
      send_to_char( "&RThat ship has already started to launch", ch );
      return;
   }
   else
   {
      if( ship->hatchopen )
      {
         ship->hatchopen = FALSE;
         act( AT_PLAIN, "You close the hatch on $T.", ch, NULL, ship->name, TO_CHAR );
         act( AT_PLAIN, "$n closes the hatch on $T.", ch, NULL, ship->name, TO_ROOM );
         echo_to_room( AT_YELLOW, get_room_index( ship->entrance ), "The hatch is closed from outside." );
         sound_to_room( get_room_index( ship->entrance ), "!!SOUND(door)" );
         sound_to_room( get_room_index( ship->location ), "!!SOUND(door)" );

         return;
      }
      else
      {
         send_to_char( "&RIts already closed.\r\n", ch );
         return;
      }
   }
}

void do_status( CHAR_DATA * ch, const char *argument )
{
   int schance;
   SHIP_DATA *ship;
   SHIP_DATA *target;

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the cockpit, turret or engineroom of a ship to do that!\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
      target = ship;
   else
      target = get_ship_here( argument, ship->starsystem );

   if( target == NULL )
   {
      send_to_char( "&RI don't see that here.\r\nTry the radar, or type status by itself for your ships status.\r\n", ch );
      return;
   }

   if( abs( ( int )( target->vx - ship->vx ) ) > 500 + ship->sensor * 2
       || abs( ( int )( target->vy - ship->vy ) ) > 500 + ship->sensor * 2
       || abs( ( int )( target->vz - ship->vz ) ) > 500 + ship->sensor * 2 )
   {
      send_to_char( "&RThat ship is to far away to scan.\r\n", ch );
      return;
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_shipsystems] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "&RYou cant figure out what the readout means.\r\n", ch );
      learn_from_failure( ch, gsn_shipsystems );
      return;
   }

   act( AT_PLAIN, "$n checks various gages and displays on the control panel.", ch, NULL, argument, TO_ROOM );

   ch_printf( ch, "&W%s:\r\n", target->name );
   ch_printf( ch, "&OCurrent Coordinates:&Y %.0f %.0f %.0f\r\n", target->vx, target->vy, target->vz );
   ch_printf( ch, "&OCurrent Heading:&Y %.0f %.0f %.0f\r\n", target->hx, target->hy, target->hz );
   ch_printf( ch, "&OCurrent Speed:&Y %d&O/%d\r\n", target->currspeed, target->realspeed );
   ch_printf( ch, "&OHull:&Y %d&O/%d  Ship Condition:&Y %s\r\n",
              target->hull, target->maxhull, target->shipstate == SHIP_DISABLED ? "Disabled" : "Running" );
   ch_printf( ch, "&OShields:&Y %d&O/%d   Energy(fuel):&Y %d&O/%d\r\n",
              target->shield, target->maxshield, target->energy, target->maxenergy );
   ch_printf( ch, "&OLaser Condition:&Y %s  &OCurrent Target:&Y %s\r\n",
              target->statet0 == LASER_DAMAGED ? "Damaged" : "Good", target->target0 ? target->target0->name : "none" );
   if( target->turret1 )
      ch_printf( ch, "&OTurret One:&Y %s  &OCurrent Target:&Y %s\r\n",
                 target->statet1 == LASER_DAMAGED ? "Damaged" : "Good", target->target1 ? target->target1->name : "none" );
   if( target->turret2 )
      ch_printf( ch, "&OTurret Two:&Y %s  &OCurrent Target:&Y %s\r\n",
                 target->statet2 == LASER_DAMAGED ? "Damaged" : "Good", target->target2 ? target->target2->name : "none" );
   ch_printf( ch, "\r\n&OMissiles:&Y %d&O/%d (&Y%s&O)  Torpedos: &Y%d&O/%d (&Y%s&O)  Rockets: &Y%d&O/%d (&Y%s&O)&w\r\n",
              ship->missiles, ship->maxmissiles,
              ship->missilestate == MISSILE_DAMAGED ? "Damaged" : "Good",
              ship->torpedos, ship->maxtorpedos,
              ship->torpedostate == MISSILE_DAMAGED ? "Damaged" : "Good",
              ship->rockets, ship->maxrockets,
              ship->rocketstate == MISSILE_DAMAGED ? "Damaged" : "Good" );

   learn_from_success( ch, gsn_shipsystems );
}

void do_hyperspace( CHAR_DATA * ch, const char *argument )
{
   int schance = 0;
   SHIP_DATA *ship;
   SHIP_DATA *eShip;
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
      send_to_char( "&RYou aren't in the pilots seat.\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&RYou'll have to turn off the ships autopilot first.\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RPlatforms can't move!\r\n", ch );
      return;
   }
   if( ship->hyperspeed == 0 )
   {
      send_to_char( "&RThis ship is not equipped with a hyperdrive!\r\n", ch );
      return;
   }
   if( ship->shipstate == SHIP_HYPERSPACE )
   {
      send_to_char( "&RYou are already travelling lightspeed!\r\n", ch );
      return;
   }
   if( ship->shipstate == SHIP_DISABLED )
   {
      send_to_char( "&RThe ships drive is disabled. Unable to manuever.\r\n", ch );
      return;
   }
   if( ship->shipstate == SHIP_DOCKED )
   {
      send_to_char( "&RYou can't do that until after you've launched!\r\n", ch );
      return;
   }
   if( ship->shipstate != SHIP_READY )
   {
      send_to_char( "&RPlease wait until the ship has finished its current manouver.\r\n", ch );
      return;
   }
   if( !ship->currjump )
   {
      send_to_char( "&RYou need to calculate your jump first!\r\n", ch );
      return;
   }

   if( ship->energy < ( 200 + ship->hyperdistance * ( 1 + ship->ship_class ) / 3 ) )
   {
      send_to_char( "&RTheres not enough fuel!\r\n", ch );
      return;
   }

   if( ship->currspeed <= 0 )
   {
      send_to_char( "&RYou need to speed up a little first!\r\n", ch );
      return;
   }

   for( eShip = ship->starsystem->first_ship; eShip; eShip = eShip->next_in_starsystem )
   {
      if( eShip == ship )
         continue;

      if( abs( ( int )( eShip->vx - ship->vx ) ) < 500
	  && abs( ( int )( eShip->vy - ship->vy ) ) < 500
	  && abs( ( int )( eShip->vz - ship->vz ) ) < 500 )
      {
         ch_printf( ch, "&RYou are too close to %s to make the jump to lightspeed.\r\n", eShip->name );
         return;
      }
   }

   if( ship->ship_class == FIGHTER_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_starfighters] );
   if( ship->ship_class == MIDSIZE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_midships] );
   if( ship->ship_class == FRIGATE_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_frigates] );
   if( ship->ship_class == CAPITAL_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_capitalships] );
   if( ship->ship_class == SUPERCAPITAL_SHIP )
      schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_supercapitalships] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "&RYou can't figure out which lever to use.\r\n", ch );
      if( ship->ship_class == FIGHTER_SHIP )
         learn_from_failure( ch, gsn_starfighters );
      if( ship->ship_class == MIDSIZE_SHIP )
         learn_from_failure( ch, gsn_midships );
      if( ship->ship_class == FRIGATE_SHIP )
         learn_from_failure( ch, gsn_frigates );
      if( ship->ship_class == CAPITAL_SHIP )
         learn_from_failure( ch, gsn_capitalships );
      if( ship->ship_class == SUPERCAPITAL_SHIP )
         learn_from_failure( ch, gsn_supercapitalships );
      return;
   }
   snprintf( buf, MAX_STRING_LENGTH, "%s disapears from your scanner.", ship->name );
   echo_to_system( AT_YELLOW, ship, buf, NULL );

   ship_from_starsystem( ship, ship->starsystem );
   ship->shipstate = SHIP_HYPERSPACE;

   send_to_char( "&GYou push forward the hyperspeed lever.\r\n", ch );
   act( AT_PLAIN, "$n pushes a lever forward on the control panel.", ch, NULL, argument, TO_ROOM );
   echo_to_ship( AT_YELLOW, ship, "The ship lurches slightly as it makes the jump to lightspeed." );
   echo_to_cockpit( AT_YELLOW, ship, "The stars become streaks of light as you enter hyperspace." );

   ship->energy -= ( 100 + ship->hyperdistance * ( 1 + ship->ship_class ) / 3 );

   ship->vx = ship->jx;
   ship->vy = ship->jy;
   ship->vz = ship->jz;

   if( ship->ship_class == FIGHTER_SHIP )
      learn_from_success( ch, gsn_starfighters );
   if( ship->ship_class == MIDSIZE_SHIP )
      learn_from_success( ch, gsn_midships );
   if( ship->ship_class == FRIGATE_SHIP )
      learn_from_success( ch, gsn_frigates );
   if( ship->ship_class == CAPITAL_SHIP )
      learn_from_success( ch, gsn_capitalships );
   if( ship->ship_class == SUPERCAPITAL_SHIP )
      learn_from_success( ch, gsn_supercapitalships );
}

void do_target( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int schance;
   SHIP_DATA *ship;
   SHIP_DATA *target;
   char buf[MAX_STRING_LENGTH];

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( ( ship = ship_from_turret( ch->in_room->vnum ) ) == NULL )
         {
            send_to_char( "&RYou must be in the gunners seat or turret of a ship to do that!\r\n", ch );
            return;
         }

         if( ship->ship_class > SHIP_PLATFORM )
         {
            send_to_char( "&RThis isn't a spacecraft!\r\n", ch );
            return;
         }

         if( ship->shipstate == SHIP_HYPERSPACE )
         {
            send_to_char( "&RYou can only do that in realspace!\r\n", ch );
            return;
         }
         if( !ship->starsystem )
         {
            send_to_char( "&RYou can't do that until you've finished launching!\r\n", ch );
            return;
         }

         if( autofly( ship ) )
         {
            send_to_char( "&RYou'll have to turn off the ships autopilot first....\r\n", ch );
            return;
         }

         if( arg[0] == '\0' )
         {
            send_to_char( "&RYou need to specify a target!\r\n", ch );
            return;
         }

         if( !str_cmp( arg, "none" ) )
         {
            send_to_char( "&GTarget set to none.\r\n", ch );
            if( ch->in_room->vnum == ship->gunseat )
               ship->target0 = NULL;
            if( ch->in_room->vnum == ship->turret1 )
               ship->target1 = NULL;
            if( ch->in_room->vnum == ship->turret2 )
               ship->target2 = NULL;
            return;
         }

         target = get_ship_here( arg, ship->starsystem );
         if( target == NULL )
         {
            send_to_char( "&RThat ship isn't here!\r\n", ch );
            return;
         }

         if( target == ship )
         {
            send_to_char( "&RYou can't target your own ship!\r\n", ch );
            return;
         }

         if( !str_cmp( target->owner, ship->owner ) && str_cmp( target->owner, "" ) )
         {
            send_to_char( "&RThat ship has the same owner... try targetting an enemy ship instead!\r\n", ch );
            return;
         }

         if( abs( ( int )( ship->vx - target->vx ) ) > 5000
	     || abs( ( int )( ship->vy - target->vy ) ) > 5000
	     || abs( ( int )( ship->vz - target->vz ) ) > 5000 )
         {
            send_to_char( "&RThat ship is too far away to target.\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_weaponsystems] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GTracking target.\r\n", ch );
            act( AT_PLAIN, "$n makes some adjustments on the targeting computer.", ch, NULL, argument, TO_ROOM );
            add_timer( ch, TIMER_DO_FUN, 1, do_target, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou fail to work the controls properly.\r\n", ch );
         learn_from_failure( ch, gsn_weaponsystems );
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
         if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
            return;
         send_to_char( "&RYour concentration is broken. You fail to lock onto your target.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   if( ( ship = ship_from_turret( ch->in_room->vnum ) ) == NULL )
   {
      return;
   }

   target = get_ship_here( arg, ship->starsystem );
   if( target == NULL || target == ship )
   {
      send_to_char( "&RThe ship has left the starsytem. Targeting aborted.\r\n", ch );
      return;
   }

   if( ch->in_room->vnum == ship->gunseat )
      ship->target0 = target;

   if( ch->in_room->vnum == ship->turret1 )
      ship->target1 = target;

   if( ch->in_room->vnum == ship->turret2 )
      ship->target2 = target;

   send_to_char( "&GTarget Locked.\r\n", ch );
   snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", ship->name );
   echo_to_cockpit( AT_BLOOD, target, buf );

   sound_to_room( ch->in_room, "!!SOUND(targetlock)" );
   learn_from_success( ch, gsn_weaponsystems );

   if( autofly( target ) && !target->target0 )
   {
      snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
      echo_to_cockpit( AT_BLOOD, ship, buf );
      target->target0 = ship;
   }
}

void do_fire( CHAR_DATA * ch, const char *argument )
{
   int schance;
   SHIP_DATA *ship;
   SHIP_DATA *target;
   char buf[MAX_STRING_LENGTH];

   if( ( ship = ship_from_turret( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the gunners chair or turret of a ship to do that!\r\n", ch );
      return;
   }

   if( ship->ship_class > SHIP_PLATFORM )
   {
      send_to_char( "&RThis isn't a spacecraft!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_HYPERSPACE )
   {
      send_to_char( "&RYou can only do that in realspace!\r\n", ch );
      return;
   }
   if( ship->starsystem == NULL )
   {
      send_to_char( "&RYou can't do that until after you've finished launching!\r\n", ch );
      return;
   }
   if( ship->energy < 5 )
   {
      send_to_char( "&RTheres not enough energy left to fire!\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&RYou'll have to turn off the ships autopilot first.\r\n", ch );
      return;
   }

   schance = IS_NPC( ch ) ? ch->top_level
      : ( int )( ch->perm_dex * 2 + ch->pcdata->learned[gsn_spacecombat] / 3
                 + ch->pcdata->learned[gsn_spacecombat2] / 3 + ch->pcdata->learned[gsn_spacecombat3] / 3 );

   if( ch->in_room->vnum == ship->gunseat && !str_prefix( argument, "lasers" ) )
   {

      if( ship->statet0 == LASER_DAMAGED )
      {
         send_to_char( "&RThe ships main laser is damaged.\r\n", ch );
         return;
      }
      if( ship->statet0 >= ship->lasers )
      {
         send_to_char( "&RThe lasers are still recharging.\r\n", ch );
         return;
      }
      if( ship->target0 == NULL )
      {
         send_to_char( "&RYou need to choose a target first.\r\n", ch );
         return;
      }
      target = ship->target0;
      if( ship->target0->starsystem != ship->starsystem )
      {
         send_to_char( "&RYour target seems to have left.\r\n", ch );
         ship->target0 = NULL;
         return;
      }
      if( abs( ( int )( target->vx - ship->vx ) ) > 1000
	  || abs( ( int )( target->vy - ship->vy ) ) > 1000
	  || abs( ( int )( target->vz - ship->vz ) ) > 1000 )
      {
         send_to_char( "&RThat ship is out of laser range.\r\n", ch );
         return;
      }
      if( ship->ship_class < 2 && !is_facing( ship, target ) )
      {
         send_to_char( "&RThe main laser can only fire forward. You'll need to turn your ship!\r\n", ch );
         return;
      }
      ship->statet0++;
      schance += target->ship_class * 25;
      schance -= target->manuever / 10;
      schance -= target->currspeed / 20;
      schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 70 );
      schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 70 );
      schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 70 );
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
      {
         schance += 15;
         schance = URANGE( 10, schance, 95 );
      }
      else
         schance = URANGE( 10, schance, 90 );
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      if( number_percent(  ) > schance )
      {
         snprintf( buf, MAX_STRING_LENGTH, "Lasers fire from %s at you but miss.", ship->name );
         echo_to_cockpit( AT_ORANGE, target, buf );
         snprintf( buf, MAX_STRING_LENGTH, "The ships lasers fire at %s but miss.", target->name );
         echo_to_cockpit( AT_ORANGE, ship, buf );
         learn_from_failure( ch, gsn_spacecombat );
         learn_from_failure( ch, gsn_spacecombat2 );
         learn_from_failure( ch, gsn_spacecombat3 );
         snprintf( buf, MAX_STRING_LENGTH, "Laserfire from %s barely misses %s.", ship->name, target->name );
         echo_to_system( AT_ORANGE, ship, buf, target );
         return;
      }
      snprintf( buf, MAX_STRING_LENGTH, "Laserfire from %s hits %s.", ship->name, target->name );
      echo_to_system( AT_ORANGE, ship, buf, target );
      snprintf( buf, MAX_STRING_LENGTH, "You are hit by lasers from %s!", ship->name );
      echo_to_cockpit( AT_BLOOD, target, buf );
      snprintf( buf, MAX_STRING_LENGTH, "Your ships lasers hit %s!.", target->name );
      echo_to_cockpit( AT_YELLOW, ship, buf );
      learn_from_success( ch, gsn_spacecombat );
      learn_from_success( ch, gsn_spacecombat2 );
      learn_from_success( ch, gsn_spacecombat3 );
      echo_to_ship( AT_RED, target, "A small explosion vibrates through the ship." );
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
         damage_ship_ch( target, 8, 13, ch );
      else
         damage_ship_ch( target, 5, 10, ch );

      if( autofly( target ) && target->target0 != ship )
      {
         target->target0 = ship;
         snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
         echo_to_cockpit( AT_BLOOD, ship, buf );
      }

      return;
   }

   /*
    * Ion cannons - v1.20, ported from SWGD. Structurally modeled on the
    * main laser branch above rather than SWGD's more complex version,
    * for consistency with how lasers/missiles/torpedos/rockets are
    * already handled in this file (no evasion roll or sabotage check in
    * the firing path here - matches the existing branches, not
    * something newly introduced). Ions disable ship systems rather than
    * damaging the hull directly - see iondamage_ship_ch.
    */
   if( ch->in_room->vnum == ship->gunseat && !str_prefix( argument, "ions" ) )
   {
      int linkshot = 1;

      if( ship->ionstate == LASER_DAMAGED )
      {
         send_to_char( "&RThe ships ion cannons are damaged.\r\n", ch );
         return;
      }
      if( IS_SET( ship->flags, SHIPFLAG_QUADION ) )
         linkshot = 4;
      else if( IS_SET( ship->flags, SHIPFLAG_TRIION ) )
         linkshot = 3;
      else if( IS_SET( ship->flags, SHIPFLAG_DUALION ) )
         linkshot = 2;
      if( ship->ionstate + linkshot > ship->ions )
      {
         send_to_char( "&RThe ion cannons are still recharging.\r\n", ch );
         return;
      }
      if( ship->target0 == NULL )
      {
         send_to_char( "&RYou need to choose a target first.\r\n", ch );
         return;
      }
      target = ship->target0;
      if( ship->target0->starsystem != ship->starsystem )
      {
         send_to_char( "&RYour target seems to have left.\r\n", ch );
         ship->target0 = NULL;
         return;
      }
      if( abs( ( int )( target->vx - ship->vx ) ) > 800
	  || abs( ( int )( target->vy - ship->vy ) ) > 800
	  || abs( ( int )( target->vz - ship->vz ) ) > 800 )
      {
         send_to_char( "&RThat ship is out of ion range.\r\n", ch );
         return;
      }
      if( ship->ship_class < 2 && !is_facing( ship, target ) )
      {
         send_to_char( "&RThe ion cannons can only fire forward. You'll need to turn your ship!\r\n", ch );
         return;
      }
      ship->ionstate += linkshot;
      schance += target->ship_class * 25;
      schance -= target->manuever / 10;
      schance -= target->currspeed / 20;
      schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 70 );
      schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 70 );
      schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 70 );
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
      {
         schance += 15;
         schance = URANGE( 15, schance, 95 );
      }
      else
         schance = URANGE( 10, schance, 90 );
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      if( number_percent(  ) > schance )
      {
         snprintf( buf, MAX_STRING_LENGTH, "A blast of ionic energy fires from %s at you but misses.", ship->name );
         echo_to_cockpit( AT_BLUE, target, buf );
         snprintf( buf, MAX_STRING_LENGTH, "The ships ion cannons fire at %s but miss.", target->name );
         echo_to_cockpit( AT_BLUE, ship, buf );
         learn_from_failure( ch, gsn_spacecombat );
         learn_from_failure( ch, gsn_spacecombat2 );
         learn_from_failure( ch, gsn_spacecombat3 );
         snprintf( buf, MAX_STRING_LENGTH, "A blast of ionic energy from %s barely misses %s.", ship->name,
                   target->name );
         echo_to_system( AT_BLUE, ship, buf, target );
         return;
      }
      snprintf( buf, MAX_STRING_LENGTH, "An ion blast from %s hits %s.", ship->name, target->name );
      echo_to_system( AT_BLUE, ship, buf, target );
      snprintf( buf, MAX_STRING_LENGTH, "You are hit by an ion blast from %s!", ship->name );
      echo_to_cockpit( AT_BLUE, target, buf );
      snprintf( buf, MAX_STRING_LENGTH, "Your ships ion cannons hit %s!", target->name );
      echo_to_cockpit( AT_BLUE, ship, buf );
      learn_from_success( ch, gsn_spacecombat );
      learn_from_success( ch, gsn_spacecombat2 );
      learn_from_success( ch, gsn_spacecombat3 );
      echo_to_ship( AT_BLUE, target, "Blue sparks shower over the ship as the systems stutter!" );
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
         iondamage_ship_ch( target, 8, 13, ch );
      else
         iondamage_ship_ch( target, 5, 10, ch );

      if( autofly( target ) && target->target0 != ship )
      {
         target->target0 = ship;
         snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
         echo_to_cockpit( AT_BLOOD, ship, buf );
      }

      return;
   }

   if( ch->in_room->vnum == ship->gunseat && !str_prefix( argument, "autocannon" ) )
   {
      int autohits, automiss, bonusdamage = 0;

      if( ship->autocannon != 1 )
      {
         send_to_char( "&RThis ship is held together by gum and prayer, and you want an autocannon?\r\n", ch );
         return;
      }
      if( ship->target0 == NULL )
      {
         send_to_char( "&RYou need to choose a target first.\r\n", ch );
         return;
      }
      target = ship->target0;
      if( ship->target0->starsystem != ship->starsystem || IS_SET( target->flags, SHIPFLAG_CLOAKED ) )
      {
         send_to_char( "&RYour target seems to have left.\r\n", ch );
         ship->target0 = NULL;
         return;
      }
      if( abs( ( int )( target->vx - ship->vx ) ) > 1500
          || abs( ( int )( target->vy - ship->vy ) ) > 1500
          || abs( ( int )( target->vz - ship->vz ) ) > 1500 )
      {
         send_to_char( "&RThat ship is out of autocannon range.\r\n", ch );
         return;
      }
      if( ship->ship_class < 2 && !is_facing( ship, target ) )
      {
         send_to_char( "&RThe autocannon can only fire forward. You'll need to turn your ship!\r\n", ch );
         return;
      }
      if( ship->autoammo == 0 )
      {
         send_to_char( "&RThe autocannon cycles up, but has no ammo to fire!\r\n", ch );
         return;
      }
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      autohits = 1;
      automiss = 0;
      while( automiss == 0 )
      {
         schance = IS_NPC( ch ) ? ch->top_level
            : ( int )( ch->perm_dex * 2 + ch->pcdata->learned[gsn_spacecombat] / 3
                       + ch->pcdata->learned[gsn_spacecombat2] / 3 + ch->pcdata->learned[gsn_spacecombat3] / 3 );
         schance += ( ship->manuever - target->manuever ) / 10;
         schance += ( ship->currspeed - target->currspeed ) / 10;
         schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 120 );
         schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 120 );
         schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 120 );
         if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
         {
            bonusdamage = 1;
            schance += 15;
            schance = URANGE( 15, schance, 95 );
         }
         else
            schance = URANGE( 10, schance, 90 );
         if( number_percent(  ) < schance )
            autohits++;
         else
            automiss++;
      }
      if( autohits > 100 )
         autohits = 100;
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
         bonusdamage = 3;

      snprintf( buf, MAX_STRING_LENGTH, "%s fires a stream of autocannon fire at %s.", ship->name, target->name );
      echo_to_system( AT_CYAN, ship, buf, target );
      snprintf( buf, MAX_STRING_LENGTH, "You are hit by autocannon fire from %s!", ship->name );
      echo_to_cockpit( AT_CYAN, target, buf );
      snprintf( buf, MAX_STRING_LENGTH, "You fire a burst from the autocannon at %s, scoring %d hits!.", target->name, autohits );
      echo_to_cockpit( AT_CYAN, ship, buf );
      learn_from_success( ch, gsn_spacecombat );
      learn_from_success( ch, gsn_spacecombat2 );
      learn_from_success( ch, gsn_spacecombat3 );
      echo_to_ship( AT_CYAN, target, "The ship shudders as thousands of autocannon rounds slam into it!" );
      damage_ship_ch( target, ( ( ship->autodamage + bonusdamage ) * autohits ),
                      ( ( ship->autodamage + bonusdamage ) * autohits * 2 ), ch );
      ship->autoammo = ( ship->autoammo - 1 );
      if( autofly( target ) && target->target0 != ship )
      {
         target->target0 = ship;
         snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
         echo_to_cockpit( AT_BLOOD, ship, buf );
      }
      return;
   }

   if( ch->in_room->vnum == ship->gunseat && !str_prefix( argument, "missile" ) )
   {
      if( ship->missilestate == MISSILE_DAMAGED )
      {
         send_to_char( "&RThe ships missile launchers are dammaged.\r\n", ch );
         return;
      }
      if( ship->missiles <= 0 )
      {
         send_to_char( "&RYou have no missiles to fire!\r\n", ch );
         return;
      }
      if( ship->missilestate != MISSILE_READY )
      {
         send_to_char( "&RThe missiles are still reloading.\r\n", ch );
         return;
      }
      if( ship->target0 == NULL )
      {
         send_to_char( "&RYou need to choose a target first.\r\n", ch );
         return;
      }
      target = ship->target0;
      if( ship->target0->starsystem != ship->starsystem )
      {
         send_to_char( "&RYour target seems to have left.\r\n", ch );
         ship->target0 = NULL;
         return;
      }
      if( abs( ( int )( target->vx - ship->vx ) ) > 1000
	  || abs( ( int )( target->vy - ship->vy ) ) > 1000
	  || abs( ( int )( target->vz - ship->vz ) ) > 1000 )
      {
         send_to_char( "&RThat ship is out of missile range.\r\n", ch );
         return;
      }
      if( ship->ship_class < 2 && !is_facing( ship, target ) )
      {
         send_to_char( "&RMissiles can only fire in a forward. You'll need to turn your ship!\r\n", ch );
         return;
      }
      schance -= target->manuever / 5;
      schance -= target->currspeed / 20;
      schance += target->ship_class * target->ship_class * 25;
      schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 100 );
      schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 100 );
      schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 100 );
      schance += ( 30 );
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
      {
         schance += 15;
         schance = URANGE( 20, schance, 95 );
      }
      else
         schance = URANGE( 20, schance, 80 );
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      if( number_percent(  ) > schance )
      {
         send_to_char( "&RYou fail to lock onto your target!", ch );
         ship->missilestate = MISSILE_RELOAD_2;
         return;
      }
      /* Weapon Focus pilots fire a better missile class */
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
         new_missile( ship, target, ch, PROTON_TORPEDO );
      else
         new_missile( ship, target, ch, CONCUSSION_MISSILE );
      ship->missiles--;
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      echo_to_cockpit( AT_YELLOW, ship, "Missiles launched." );
      snprintf( buf, MAX_STRING_LENGTH, "Incoming missile from %s.", ship->name );
      echo_to_cockpit( AT_BLOOD, target, buf );
      snprintf( buf, MAX_STRING_LENGTH, "%s fires a missile towards %s.", ship->name, target->name );
      echo_to_system( AT_ORANGE, ship, buf, target );
      learn_from_success( ch, gsn_weaponsystems );
      if( ship->ship_class == CAPITAL_SHIP || ship->ship_class == SHIP_PLATFORM )
         ship->missilestate = MISSILE_RELOAD;
      else
         ship->missilestate = MISSILE_FIRED;

      if( autofly( target ) && target->target0 != ship )
      {
         target->target0 = ship;
         snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
         echo_to_cockpit( AT_BLOOD, ship, buf );
      }

      return;
   }
   if( ch->in_room->vnum == ship->gunseat && !str_prefix( argument, "torpedo" ) )
   {
      if( ship->torpedostate == MISSILE_DAMAGED )
      {
         send_to_char( "&RThe ships torpedo launchers are dammaged.\r\n", ch );
         return;
      }
      if( ship->torpedos <= 0 )
      {
         send_to_char( "&RYou have no torpedos to fire!\r\n", ch );
         return;
      }
      if( ship->torpedostate != MISSILE_READY )
      {
         send_to_char( "&RThe torpedos are still reloading.\r\n", ch );
         return;
      }
      if( ship->target0 == NULL )
      {
         send_to_char( "&RYou need to choose a target first.\r\n", ch );
         return;
      }
      target = ship->target0;
      if( ship->target0->starsystem != ship->starsystem )
      {
         send_to_char( "&RYour target seems to have left.\r\n", ch );
         ship->target0 = NULL;
         return;
      }
      if( abs( ( int )( target->vx - ship->vx ) ) > 1000
	  || abs( ( int )( target->vy - ship->vy ) ) > 1000
	  || abs( ( int )( target->vz - ship->vz ) ) > 1000 )
      {
         send_to_char( "&RThat ship is out of torpedo range.\r\n", ch );
         return;
      }
      if( ship->ship_class < 2 && !is_facing( ship, target ) )
      {
         send_to_char( "&RTorpedos can only fire in a forward direction. You'll need to turn your ship!\r\n", ch );
         return;
      }
      schance -= target->manuever / 5;
      schance -= target->currspeed / 20;
      schance += target->ship_class * target->ship_class * 25;
      schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 100 );
      schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 100 );
      schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 100 );
      schance = URANGE( 20, schance, 80 );
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      if( number_percent(  ) > schance )
      {
         send_to_char( "&RYou fail to lock onto your target!", ch );
         ship->torpedostate = MISSILE_RELOAD_2;
         return;
      }
      new_missile( ship, target, ch, PROTON_TORPEDO );
      ship->torpedos--;
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      echo_to_cockpit( AT_YELLOW, ship, "Missiles launched." );
      snprintf( buf, MAX_STRING_LENGTH, "Incoming torpedo from %s.", ship->name );
      echo_to_cockpit( AT_BLOOD, target, buf );
      snprintf( buf, MAX_STRING_LENGTH, "%s fires a torpedo towards %s.", ship->name, target->name );
      echo_to_system( AT_ORANGE, ship, buf, target );
      learn_from_success( ch, gsn_weaponsystems );
      if( ship->ship_class == CAPITAL_SHIP || ship->ship_class == SHIP_PLATFORM )
         ship->torpedostate = MISSILE_RELOAD;
      else
         ship->torpedostate = MISSILE_FIRED;

      if( autofly( target ) && target->target0 != ship )
      {
         target->target0 = ship;
         snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
         echo_to_cockpit( AT_BLOOD, ship, buf );
      }

      return;
   }

   if( ch->in_room->vnum == ship->gunseat && !str_prefix( argument, "rocket" ) )
   {
      if( ship->rocketstate == MISSILE_DAMAGED )
      {
         send_to_char( "&RThe ships missile launchers are damaged.\r\n", ch );
         return;
      }
      if( ship->rockets <= 0 )
      {
         send_to_char( "&RYou have no rockets to fire!\r\n", ch );
         return;
      }
      if( ship->rocketstate != MISSILE_READY )
      {
         send_to_char( "&RThe missiles are still reloading.\r\n", ch );
         return;
      }
      if( ship->target0 == NULL )
      {
         send_to_char( "&RYou need to choose a target first.\r\n", ch );
         return;
      }
      target = ship->target0;
      if( ship->target0->starsystem != ship->starsystem )
      {
         send_to_char( "&RYour target seems to have left.\r\n", ch );
         ship->target0 = NULL;
         return;
      }
      if( abs( ( int )( target->vx - ship->vx ) ) > 800
	  || abs( ( int )( target->vy - ship->vy ) ) > 800
	  || abs( ( int )( target->vz - ship->vz ) ) > 800 )
      {
         send_to_char( "&RThat ship is out of rocket range.\r\n", ch );
         return;
      }
      if( ship->ship_class < 2 && !is_facing( ship, target ) )
      {
         send_to_char( "&RRockets can only fire forward. You'll need to turn your ship!\r\n", ch );
         return;
      }
      schance -= target->manuever / 5;
      schance -= target->currspeed / 20;
      schance += target->ship_class * target->ship_class * 25;
      schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 100 );
      schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 100 );
      schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 100 );
      schance -= 30;
      schance = URANGE( 20, schance, 80 );
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      if( number_percent(  ) > schance )
      {
         send_to_char( "&RYou fail to lock onto your target!", ch );
         ship->rocketstate = MISSILE_RELOAD_2;
         return;
      }
      new_missile( ship, target, ch, HEAVY_ROCKET );
      ship->rockets--;
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      echo_to_cockpit( AT_YELLOW, ship, "Rocket launched." );
      snprintf( buf, MAX_STRING_LENGTH, "Incoming rocket from %s.", ship->name );
      echo_to_cockpit( AT_BLOOD, target, buf );
      snprintf( buf, MAX_STRING_LENGTH, "%s fires a heavy rocket towards %s.", ship->name, target->name );
      echo_to_system( AT_ORANGE, ship, buf, target );
      learn_from_success( ch, gsn_weaponsystems );
      if( ship->ship_class == CAPITAL_SHIP || ship->ship_class == SHIP_PLATFORM )
         ship->rocketstate = MISSILE_RELOAD;
      else
         ship->rocketstate = MISSILE_FIRED;

      if( autofly( target ) && target->target0 != ship )
      {
         target->target0 = ship;
         snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
         echo_to_cockpit( AT_BLOOD, ship, buf );
      }

      return;
   }

   if( ch->in_room->vnum == ship->turret1 && !str_prefix( argument, "lasers" ) )
   {
      if( ship->statet1 == LASER_DAMAGED )
      {
         send_to_char( "&RThe ships turret is damaged.\r\n", ch );
         return;
      }
      if( ship->statet1 > ship->ship_class )
      {
         send_to_char( "&RThe turbolaser is recharging.\r\n", ch );
         return;
      }
      if( ship->target1 == NULL )
      {
         send_to_char( "&RYou need to choose a target first.\r\n", ch );
         return;
      }
      target = ship->target1;
      if( ship->target1->starsystem != ship->starsystem )
      {
         send_to_char( "&RYour target seems to have left.\r\n", ch );
         ship->target1 = NULL;
         return;
      }
      if( abs( ( int )( target->vx - ship->vx ) ) > 1000
	  || abs( ( int )( target->vy - ship->vy ) ) > 1000
	  || abs( ( int )( target->vz - ship->vz ) ) > 1000 )
      {
         send_to_char( "&RThat ship is out of laser range.\r\n", ch );
         return;
      }
      ship->statet1++;
      schance -= target->manuever / 10;
      schance += target->ship_class * 25;
      schance -= target->currspeed / 20;
      schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 70 );
      schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 70 );
      schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 70 );
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
      {
         schance += 15;
         schance = URANGE( 10, schance, 95 );
      }
      else
         schance = URANGE( 10, schance, 90 );
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      if( number_percent(  ) > schance )
      {
         snprintf( buf, MAX_STRING_LENGTH, "Turbolasers fire from %s at you but miss.", ship->name );
         echo_to_cockpit( AT_ORANGE, target, buf );
         snprintf( buf, MAX_STRING_LENGTH, "Turbolasers fire from the ships turret at %s but miss.", target->name );
         echo_to_cockpit( AT_ORANGE, ship, buf );
         snprintf( buf, MAX_STRING_LENGTH, "%s fires at %s but misses.", ship->name, target->name );
         echo_to_system( AT_ORANGE, ship, buf, target );
         learn_from_failure( ch, gsn_spacecombat );
         learn_from_failure( ch, gsn_spacecombat2 );
         learn_from_failure( ch, gsn_spacecombat3 );
         return;
      }
      snprintf( buf, MAX_STRING_LENGTH, "Turboasers fire from %s, hitting %s.", ship->name, target->name );
      echo_to_system( AT_ORANGE, ship, buf, target );
      snprintf( buf, MAX_STRING_LENGTH, "You are hit by turbolasers from %s!", ship->name );
      echo_to_cockpit( AT_BLOOD, target, buf );
      snprintf( buf, MAX_STRING_LENGTH, "Turbolasers fire from the turret, hitting %s!.", target->name );
      echo_to_cockpit( AT_YELLOW, ship, buf );
      learn_from_success( ch, gsn_spacecombat );
      learn_from_success( ch, gsn_spacecombat2 );
      learn_from_success( ch, gsn_spacecombat3 );
      echo_to_ship( AT_RED, target, "A small explosion vibrates through the ship." );
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
         damage_ship_ch( target, 13, 28, ch );
      else
         damage_ship_ch( target, 10, 25, ch );

      if( autofly( target ) && target->target0 != ship )
      {
         target->target0 = ship;
         snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
         echo_to_cockpit( AT_BLOOD, ship, buf );
      }

      return;
   }

   if( ch->in_room->vnum == ship->turret2 && !str_prefix( argument, "lasers" ) )
   {
      if( ship->statet2 == LASER_DAMAGED )
      {
         send_to_char( "&RThe ships turret is damaged.\r\n", ch );
         return;
      }
      if( ship->statet2 > ship->ship_class )
      {
         send_to_char( "&RThe turbolaser is still recharging.\r\n", ch );
         return;
      }
      if( ship->target2 == NULL )
      {
         send_to_char( "&RYou need to choose a target first.\r\n", ch );
         return;
      }
      target = ship->target2;
      if( ship->target2->starsystem != ship->starsystem )
      {
         send_to_char( "&RYour target seems to have left.\r\n", ch );
         ship->target2 = NULL;
         return;
      }
      if( abs( ( int )( target->vx - ship->vx ) ) > 1000
	  || abs( ( int )( target->vy - ship->vy ) ) > 1000
	  || abs( ( int )( target->vz - ship->vz ) ) > 1000 )
      {
         send_to_char( "&RThat ship is out of laser range.\r\n", ch );
         return;
      }
      ship->statet2++;
      schance -= target->manuever / 10;
      schance += target->ship_class * 25;
      schance -= target->currspeed / 20;
      schance -= ( abs( ( int )( target->vx - ship->vx ) ) / 70 );
      schance -= ( abs( ( int )( target->vy - ship->vy ) ) / 70 );
      schance -= ( abs( ( int )( target->vz - ship->vz ) ) / 70 );
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
      {
         schance += 15;
         schance = URANGE( 10, schance, 95 );
      }
      else
         schance = URANGE( 10, schance, 90 );
      act( AT_PLAIN, "$n presses the fire button.", ch, NULL, argument, TO_ROOM );
      if( number_percent(  ) > schance )
      {
         snprintf( buf, MAX_STRING_LENGTH, "Turbolasers fire from %s barely missing %s.", ship->name, target->name );
         echo_to_system( AT_ORANGE, ship, buf, target );
         snprintf( buf, MAX_STRING_LENGTH, "Turbolasers fire from %s at you but miss.", ship->name );
         echo_to_cockpit( AT_ORANGE, target, buf );
         snprintf( buf, MAX_STRING_LENGTH, "Turbolasers fire from the turret missing %s.", target->name );
         echo_to_cockpit( AT_ORANGE, ship, buf );
         learn_from_failure( ch, gsn_spacecombat );
         learn_from_failure( ch, gsn_spacecombat2 );
         learn_from_failure( ch, gsn_spacecombat3 );
         return;
      }
      snprintf( buf, MAX_STRING_LENGTH, "Turbolasers fire from %s, hitting %s.", ship->name, target->name );
      echo_to_system( AT_ORANGE, ship, buf, target );
      snprintf( buf, MAX_STRING_LENGTH, "You are hit by turbolasers from %s!", ship->name );
      echo_to_cockpit( AT_BLOOD, target, buf );
      snprintf( buf, MAX_STRING_LENGTH, "turbolasers fire from the turret hitting %s!.", target->name );
      echo_to_cockpit( AT_YELLOW, ship, buf );
      learn_from_success( ch, gsn_spacecombat );
      learn_from_success( ch, gsn_spacecombat2 );
      learn_from_success( ch, gsn_spacecombat3 );
      echo_to_ship( AT_RED, target, "A small explosion vibrates through the ship." );
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_WFOCUS )
         damage_ship_ch( target, 13, 28, ch );
      else
         damage_ship_ch( target, 10, 25, ch );

      if( autofly( target ) && target->target0 != ship )
      {
         target->target0 = ship;
         snprintf( buf, MAX_STRING_LENGTH, "You are being targetted by %s.", target->name );
         echo_to_cockpit( AT_BLOOD, ship, buf );
      }

      return;
   }

   send_to_char( "&RYou can't fire that!\r\n", ch );
}

void do_calculate( CHAR_DATA * ch, const char *argument )
{
   char arg1[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   char arg3[MAX_INPUT_LENGTH];
   int schance, count = 0;
   SHIP_DATA *ship;
   SPACE_DATA *starsystem;

   argument = one_argument( argument, arg1 );
   argument = one_argument( argument, arg2 );
   argument = one_argument( argument, arg3 );

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

   if( ( ship = ship_from_navseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be at a nav computer to calculate jumps.\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&RYou'll have to turn off the ships autopilot first....\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RAnd what exactly are you going to calculate...?\r\n", ch );
      return;
   }
   if( ship->hyperspeed == 0 )
   {
      send_to_char( "&RThis ship is not equipped with a hyperdrive!\r\n", ch );
      return;
   }
   if( ship->shipstate == SHIP_DOCKED )
   {
      send_to_char( "&RYou can't do that until after you've launched!\r\n", ch );
      return;
   }

   if( ship->shipstate2 == SHIP_DOCK || ship->shipstate2 == SHIP_DOCK_2 )
   {
      send_to_char( "&RNot while docking procedures are going on.\r\n", ch );
      return;
   }
   if( ship->shipstate2 == SHIP_DOCK_3 )
   {
      send_to_char( "&RDetach from the docked ship first.\r\n", ch );
      return;
   }

   if( ship->starsystem == NULL )
   {
      send_to_char( "&RYou can only do that in realspace.\r\n", ch );
      return;
   }
   if( IS_SET( ship->flags, SHIPFLAG_SIMULATOR ) )
   {
      /* v1.29: do_plot already refused to reveal the real starmap from
       * inside a simulator run - do_calculate never got the matching
       * guard, so a trainee could see (and technically jump-plot into)
       * every real starsystem in the game from a simulated flight.
       * Matches do_plot's SWGD-verbatim refusal. */
      send_to_char( "&RThere are no starsystems that need calculating from here.\r\n", ch );
      return;
   }
   if( argument[0] == '\0' )
   {
      send_to_char( "&WFormat: Calculate <starsystem> <entry x> <entry y> <entry z>\r\n&wPossible destinations:\r\n", ch );
      for( starsystem = first_starsystem; starsystem; starsystem = starsystem->next )
      {
         if( !str_cmp( starsystem->name, "Simulator" ) )
            continue;
         set_char_color( AT_NOTE, ch );
         ch_printf( ch, "%-30s %d\r\n", starsystem->name,
                    ( abs( starsystem->xpos - ship->starsystem->xpos ) +
                      abs( starsystem->ypos - ship->starsystem->ypos ) ) / 2 );
         count++;
      }
      if( !count )
      {
         send_to_char( "No Starsystems found.\r\n", ch );
      }
      return;
   }
   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_navigation] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "&RYou cant seem to figure the charts out today.\r\n", ch );
      learn_from_failure( ch, gsn_navigation );
      return;
   }

   ship->currjump = starsystem_from_name( arg1 );
   ship->jx = atoi( arg2 );
   ship->jy = atoi( arg3 );
   ship->jz = atoi( argument );

   if( ship->currjump == NULL )
   {
      send_to_char( "&RYou can't seem to find that starsytem on your charts.\r\n", ch );
      return;
   }
   else
   {
      SPACE_DATA *starsystem2;

      starsystem2 = ship->currjump;

      if( starsystem2->star1 && strcmp( starsystem2->star1, "" )
	  && abs( ( int )( ship->jx - starsystem2->s1x ) ) < 300
	  && abs( ( int )( ship->jy - starsystem2->s1y ) ) < 300
	  && abs( ( int )( ship->jz - starsystem2->s1z ) ) < 300 )
      {
         echo_to_cockpit( AT_RED, ship, "WARNING.. Jump coordinates too close to stellar object." );
         echo_to_cockpit( AT_RED, ship, "WARNING.. Hyperjump NOT set." );
         ship->currjump = NULL;
         return;
      }
      else if( starsystem2->star2 && strcmp( starsystem2->star2, "" )
	       && abs( ( int )( ship->jx - starsystem2->s2x ) ) < 300
	       && abs( ( int )( ship->jy - starsystem2->s2y ) ) < 300
	       && abs( ( int )( ship->jz - starsystem2->s2z ) ) < 300 )
      {
         echo_to_cockpit( AT_RED, ship, "WARNING.. Jump coordinates too close to stellar object." );
         echo_to_cockpit( AT_RED, ship, "WARNING.. Hyperjump NOT set." );
         ship->currjump = NULL;
         return;
      }
      else if( starsystem2->planet1 && strcmp( starsystem2->planet1, "" )
	       && abs( ( int )( ship->jx - starsystem2->p1x ) ) < 300
	       && abs( ( int )( ship->jy - starsystem2->p1y ) ) < 300
	       && abs( ( int )( ship->jz - starsystem2->p1z ) ) < 300 )
      {
         echo_to_cockpit( AT_RED, ship, "WARNING.. Jump coordinates too close to stellar object." );
         echo_to_cockpit( AT_RED, ship, "WARNING.. Hyperjump NOT set." );
         ship->currjump = NULL;
         return;
      }
      else if( starsystem2->planet2 && strcmp( starsystem2->planet2, "" )
	       && abs( ( int )( ship->jx - starsystem2->p2x ) ) < 300
	       && abs( ( int )( ship->jy - starsystem2->p2y ) ) < 300
	       && abs( ( int )( ship->jz - starsystem2->p2z ) ) < 300 )
      {
         echo_to_cockpit( AT_RED, ship, "WARNING.. Jump coordinates too close to stellar object." );
         echo_to_cockpit( AT_RED, ship, "WARNING.. Hyperjump NOT set." );
         ship->currjump = NULL;
         return;
      }
      else if( starsystem2->planet3 && strcmp( starsystem2->planet3, "" )
	       && abs( ( int )( ship->jx - starsystem2->p3x ) ) < 300
	       && abs( ( int )( ship->jy - starsystem2->p3y ) ) < 300
	       && abs( ( int )( ship->jz - starsystem2->p3z ) ) < 300 )
      {
         echo_to_cockpit( AT_RED, ship, "WARNING.. Jump coordinates too close to stellar object." );
         echo_to_cockpit( AT_RED, ship, "WARNING.. Hyperjump NOT set." );
         ship->currjump = NULL;
         return;
      }
      else
      {
         ship->jx += number_range( -250, 250 );
         ship->jy += number_range( -250, 250 );
         ship->jz += number_range( -250, 250 );
      }
   }

   ship->hyperdistance = abs( ship->starsystem->xpos - ship->currjump->xpos );
   ship->hyperdistance += abs( ship->starsystem->ypos - ship->currjump->ypos );
   ship->hyperdistance /= 5;

   if( ship->hyperdistance < 100 )
      ship->hyperdistance = 100;

   ship->hyperdistance += number_range( 0, 200 );

   sound_to_room( ch->in_room, "!!SOUND(computer)" );

   send_to_char( "&GHyperspace course set. Ready for the jump to lightspeed.\r\n", ch );
   act( AT_PLAIN, "$n does some calculations using the ships computer.", ch, NULL, argument, TO_ROOM );

   learn_from_success( ch, gsn_navigation );

   WAIT_STATE( ch, 2 * PULSE_VIOLENCE );
}

void do_recharge( CHAR_DATA * ch, const char *argument )
{
   int recharge;
   int schance;
   SHIP_DATA *ship;

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the cockpit of a ship to do that!\r\n", ch );
      return;
   }
   if( ( ship = ship_from_coseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RThe controls must be at the co-pilot station.\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&R...\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_DISABLED )
   {
      send_to_char( "&RThe ships drive is disabled. Unable to manuever.\r\n", ch );
      return;
   }

   if( ship->energy < 100 )
   {
      send_to_char( "&RTheres not enough energy!\r\n", ch );
      return;
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_shipsystems] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "&RYou fail to work the controls properly.\r\n", ch );
      learn_from_failure( ch, gsn_shipsystems );
      return;
   }

   send_to_char( "&GRecharging shields..\r\n", ch );
   act( AT_PLAIN, "$n pulls back a lever on the control panel.", ch, NULL, argument, TO_ROOM );

   learn_from_success( ch, gsn_shipsystems );

   recharge = UMIN( ship->maxshield - ship->shield, ship->energy * 5 + 100 );
   recharge = URANGE( 1, recharge, 25 + ship->ship_class * 25 );
   ship->shield += recharge;
   ship->energy -= ( recharge * 2 + recharge * ship->ship_class );
}

void do_repairship( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   int schance, change;
   SHIP_DATA *ship;

   strlcpy( arg, argument, MAX_INPUT_LENGTH );

   switch ( ch->substate )
   {
      default:
         if( ( ship = ship_from_engine( ch->in_room->vnum ) ) == NULL )
         {
            send_to_char( "&RYou must be in the engine room of a ship to do that!\r\n", ch );
            return;
         }

         if( str_cmp( argument, "hull" ) && str_cmp( argument, "drive" ) &&
             str_cmp( argument, "launcher" ) && str_cmp( argument, "laser" ) &&
             str_cmp( argument, "ions" ) && str_cmp( argument, "tlauncher" ) &&
             str_cmp( argument, "rlauncher" ) &&
             str_cmp( argument, "turret 1" ) && str_cmp( argument, "turret 2" ) )
         {
            send_to_char( "&RYou need to spceify something to repair:\r\n", ch );
            send_to_char( "&rTry: hull, drive, launcher, laser, turret 1, or turret 2\r\n", ch );
            return;
         }

         schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_shipmaintenance] );
         if( number_percent(  ) < schance )
         {
            send_to_char( "&GYou begin your repairs\r\n", ch );
            act( AT_PLAIN, "$n begins repairing the ships $T.", ch, NULL, argument, TO_ROOM );
            if( !str_cmp( arg, "hull" ) )
               add_timer( ch, TIMER_DO_FUN, 15, do_repairship, 1 );
            else
               add_timer( ch, TIMER_DO_FUN, 5, do_repairship, 1 );
            ch->dest_buf = strdup( arg );
            return;
         }
         send_to_char( "&RYou fail to locate the source of the problem.\r\n", ch );
         learn_from_failure( ch, gsn_shipmaintenance );
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
         if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
            return;
         send_to_char( "&RYou are distracted and fail to finish your repairs.\r\n", ch );
         return;
   }

   ch->substate = SUB_NONE;

   if( ( ship = ship_from_engine( ch->in_room->vnum ) ) == NULL )
   {
      return;
   }

   if( !str_cmp( arg, "hull" ) )
   {
      change = number_range( ( int )( ch->pcdata->learned[gsn_shipmaintenance] / 2 ),
                             ( int )( ch->pcdata->learned[gsn_shipmaintenance] ) );
      /* v1.24 SWGD parity: Mechanics repair twice as effectively */
      if( !IS_NPC( ch ) && ch->subclass == SUBCLASS_MECHANIC )
         change *= 2;
      change = URANGE( 0, change, ( ship->maxhull - ship->hull ) );
      ship->hull += change;
      ch_printf( ch, "&GRepair complete.. Hull strength inreased by %d points.\r\n", change );
   }

   if( !str_cmp( arg, "drive" ) )
   {
      if( ship->location == ship->lastdoc )
         ship->shipstate = SHIP_DOCKED;
      else if( ship->shipstate != SHIP_HYPERSPACE )
         ship->shipstate = SHIP_READY;
      send_to_char( "&GShips drive repaired.\r\n", ch );
      if( IS_SET( ship->flags, SHIPFLAG_SABOTAGEDENGINE ) )
      {
         send_to_char( "Sabotage detected, removed.\r\n", ch );
         REMOVE_BIT( ship->flags, SHIPFLAG_SABOTAGEDENGINE );
      }
   }

   if( !str_cmp( arg, "launcher" ) )
   {
      ship->missilestate = MISSILE_READY;
      send_to_char( "&GMissile launcher repaired.\r\n", ch );
      if( IS_SET( ship->flags, SHIPFLAG_SABOTAGEDLAUNCHERS ) )
      {
         send_to_char( "Sabotage detected, removed.\r\n", ch );
         REMOVE_BIT( ship->flags, SHIPFLAG_SABOTAGEDLAUNCHERS );
      }
   }

   if( !str_cmp( arg, "tlauncher" ) )
   {
      ship->torpedostate = MISSILE_READY;
      send_to_char( "&GTorpedo launcher repaired.\r\n", ch );
      if( IS_SET( ship->flags, SHIPFLAG_SABOTAGEDTLAUNCHERS ) )
      {
         send_to_char( "Sabotage detected, removed.\r\n", ch );
         REMOVE_BIT( ship->flags, SHIPFLAG_SABOTAGEDTLAUNCHERS );
      }
   }

   if( !str_cmp( arg, "rlauncher" ) )
   {
      ship->rocketstate = MISSILE_READY;
      send_to_char( "&GRocket launcher repaired.\r\n", ch );
      if( IS_SET( ship->flags, SHIPFLAG_SABOTAGEDRLAUNCHERS ) )
      {
         send_to_char( "Sabotage detected, removed.\r\n", ch );
         REMOVE_BIT( ship->flags, SHIPFLAG_SABOTAGEDRLAUNCHERS );
      }
   }

   if( !str_cmp( arg, "ions" ) )
   {
      ship->ionstate = LASER_READY;
      send_to_char( "&GIon cannons repaired.\r\n", ch );
      if( IS_SET( ship->flags, SHIPFLAG_SABOTAGEDIONS ) )
      {
         send_to_char( "Sabotage detected, removed.\r\n", ch );
         REMOVE_BIT( ship->flags, SHIPFLAG_SABOTAGEDIONS );
      }
   }

   if( !str_cmp( arg, "laser" ) )
   {
      ship->statet0 = LASER_READY;
      send_to_char( "&GMain laser repaired.\r\n", ch );
      if( IS_SET( ship->flags, SHIPFLAG_SABOTAGEDLASERS ) )
      {
         send_to_char( "Sabotage detected, removed.\r\n", ch );
         REMOVE_BIT( ship->flags, SHIPFLAG_SABOTAGEDLASERS );
      }
   }

   if( !str_cmp( arg, "turret 1" ) )
   {
      ship->statet1 = LASER_READY;
      send_to_char( "&GLaser Turret 1 repaired.\r\n", ch );
      if( IS_SET( ship->flags, SHIPFLAG_SABOTAGEDTURRET1 ) )
      {
         send_to_char( "Sabotage detected, removed.\r\n", ch );
         REMOVE_BIT( ship->flags, SHIPFLAG_SABOTAGEDTURRET1 );
      }
   }

   if( !str_cmp( arg, "turret 2" ) )
   {
      ship->statet2 = LASER_READY;
      send_to_char( "&GLaser Turret 2 repaired.\r\n", ch );
      if( IS_SET( ship->flags, SHIPFLAG_SABOTAGEDTURRET2 ) )
      {
         send_to_char( "Sabotage detected, removed.\r\n", ch );
         REMOVE_BIT( ship->flags, SHIPFLAG_SABOTAGEDTURRET2 );
      }
   }

   act( AT_PLAIN, "$n finishes the repairs.", ch, NULL, argument, TO_ROOM );

   learn_from_success( ch, gsn_shipmaintenance );
}

void do_refuel( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;

   if( ch->gold <= 499 )
   {
      send_to_char( "&RInsufficient funds to refuel ship.\r\n", ch );
      return;
   }

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the cockpit of a ship to do that!\r\n", ch );
      return;
   }

   if( ship->target0 || ship->target1 || ship->target2 )
   {
      send_to_char( "&RNot while the ship is engaged with an enemy!\r\n", ch );
      return;
   }

   if( ship->energy >= ship->maxenergy )
   {
      send_to_char( "&GYour ship's fuel tank is already full.\r\n", ch );
      return;
   }

   ship->energy += ( ( ship->ship_class + 1 ) * 75 );
   ship->energy = UMIN( ship->energy, ship->maxenergy );

   send_to_char( "&GYou refuel your ship a little.\r\n", ch );
   ch->gold -= 500;

   save_ship( ship );
}

void do_addpilot( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the cockpit of a ship to do that!\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RYou can't do that here.\r\n", ch );
      return;
   }

   if( str_cmp( ship->owner, ch->name ) )
   {

      if( !IS_NPC( ch ) && ch->pcdata && ch->pcdata->clan && !str_cmp( ch->pcdata->clan->name, ship->owner ) )
         if( !str_cmp( ch->pcdata->clan->leader, ch->name ) )
            ;
         else if( !str_cmp( ch->pcdata->clan->number1, ch->name ) )
            ;
         else if( !str_cmp( ch->pcdata->clan->number2, ch->name ) )
            ;
         else
         {
            send_to_char( "&RThat isn't your ship!", ch );
            return;
         }
      else
      {
         send_to_char( "&RThat isn't your ship!", ch );
         return;
      }

   }

   if( argument[0] == '\0' )
   {
      send_to_char( "&RAdd which pilot?\r\n", ch );
      return;
   }

   if( str_cmp( ship->pilot, "" ) )
   {
      if( str_cmp( ship->copilot, "" ) )
      {
         send_to_char( "&RYou are ready have a pilot and copilot..\r\n", ch );
         send_to_char( "&RTry rempilot first.\r\n", ch );
         return;
      }

      STRFREE( ship->copilot );
      ship->copilot = STRALLOC( argument );
      send_to_char( "Copilot Added.\r\n", ch );
      save_ship( ship );
      return;

      return;
   }

   STRFREE( ship->pilot );
   ship->pilot = STRALLOC( argument );
   send_to_char( "Pilot Added.\r\n", ch );
   save_ship( ship );
}

void do_rempilot( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the cockpit of a ship to do that!\r\n", ch );
      return;
   }

   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RYou can't do that here.\r\n", ch );
      return;
   }

   if( str_cmp( ship->owner, ch->name ) )
   {

      if( !IS_NPC( ch ) && ch->pcdata && ch->pcdata->clan && !str_cmp( ch->pcdata->clan->name, ship->owner ) )
         if( !str_cmp( ch->pcdata->clan->leader, ch->name ) )
            ;
         else if( !str_cmp( ch->pcdata->clan->number1, ch->name ) )
            ;
         else if( !str_cmp( ch->pcdata->clan->number2, ch->name ) )
            ;
         else
         {
            send_to_char( "&RThat isn't your ship!", ch );
            return;
         }
      else
      {
         send_to_char( "&RThat isn't your ship!", ch );
         return;
      }

   }

   if( argument[0] == '\0' )
   {
      send_to_char( "&RRemove which pilot?\r\n", ch );
      return;
   }

   if( !str_cmp( ship->pilot, argument ) )
   {
      STRFREE( ship->pilot );
      ship->pilot = STRALLOC( "" );
      send_to_char( "Pilot Removed.\r\n", ch );
      save_ship( ship );
      return;
   }

   if( !str_cmp( ship->copilot, argument ) )
   {
      STRFREE( ship->copilot );
      ship->copilot = STRALLOC( "" );
      send_to_char( "Copilot Removed.\r\n", ch );
      save_ship( ship );
      return;
   }

   send_to_char( "&RThat person isn't listed as one of the ships pilots.\r\n", ch );
}

void do_radar( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *target;
   int schance;
   SHIP_DATA *ship;
   MISSILE_DATA *missile;

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the cockpit or turret of a ship to do that!\r\n", ch );
      return;
   }

   if( ship->ship_class > SHIP_PLATFORM )
   {
      send_to_char( "&RThis isn't a spacecraft!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_DOCKED )
   {
      send_to_char( "&RWait until after you launch!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_HYPERSPACE )
   {
      send_to_char( "&RYou can only do that in realspace!\r\n", ch );
      return;
   }

   if( ship->starsystem == NULL )
   {
      send_to_char( "&RYou can't do that unless the ship is flying in realspace!\r\n", ch );
      return;
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_navigation] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "&RYou fail to work the controls properly.\r\n", ch );
      learn_from_failure( ch, gsn_navigation );
      return;
   }

   act( AT_PLAIN, "$n checks the radar.", ch, NULL, argument, TO_ROOM );

   set_char_color( AT_WHITE, ch );
   ch_printf( ch, "%s\r\n\r\n", ship->starsystem->name );
   set_char_color( AT_LBLUE, ch );
   if( ship->starsystem->star1 && str_cmp( ship->starsystem->star1, "" ) )
      ch_printf( ch, "%s   %d %d %d\r\n",
                 ship->starsystem->star1, ship->starsystem->s1x, ship->starsystem->s1y, ship->starsystem->s1z );
   if( ship->starsystem->star2 && str_cmp( ship->starsystem->star2, "" ) )
      ch_printf( ch, "%s   %d %d %d\r\n",
                 ship->starsystem->star2, ship->starsystem->s2x, ship->starsystem->s2y, ship->starsystem->s2z );
   if( ship->starsystem->planet1 && str_cmp( ship->starsystem->planet1, "" ) )
      ch_printf( ch, "%s   %d %d %d\r\n",
                 ship->starsystem->planet1, ship->starsystem->p1x, ship->starsystem->p1y, ship->starsystem->p1z );
   if( ship->starsystem->planet2 && str_cmp( ship->starsystem->planet2, "" ) )
      ch_printf( ch, "%s   %d %d %d\r\n",
                 ship->starsystem->planet2, ship->starsystem->p2x, ship->starsystem->p2y, ship->starsystem->p2z );
   if( ship->starsystem->planet3 && str_cmp( ship->starsystem->planet3, "" ) )
      ch_printf( ch, "%s   %d %d %d\r\n",
                 ship->starsystem->planet3, ship->starsystem->p3x, ship->starsystem->p3y, ship->starsystem->p3z );
   ch_printf( ch, "\r\n" );
   for( target = ship->starsystem->first_ship; target; target = target->next_in_starsystem )
   {
      if( target != ship )
         ch_printf( ch, "%s    %.0f %.0f %.0f\r\n", target->name, target->vx, target->vy, target->vz );
   }
   ch_printf( ch, "\r\n" );
   for( missile = ship->starsystem->first_missile; missile; missile = missile->next_in_starsystem )
   {
      ch_printf( ch, "%s    %d %d %d\r\n",
                 missile->missiletype == CONCUSSION_MISSILE ? "A Concusion missile" :
                 ( missile->missiletype == PROTON_TORPEDO ? "A Torpedo" :
                   ( missile->missiletype == HEAVY_ROCKET ? "A Heavy Rocket" : "A Heavy Bomb" ) ),
                 missile->mx, missile->my, missile->mz );
   }

   ch_printf( ch, "\r\n&WYour Coordinates: %.0f %.0f %.0f\r\n", ship->vx, ship->vy, ship->vz );


   learn_from_success( ch, gsn_navigation );
}

void do_autotrack( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   int schance;

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


   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RPlatforms don't have autotracking systems!\r\n", ch );
      return;
   }
   if( ship->ship_class == CAPITAL_SHIP )
   {
      send_to_char( "&RThis ship is too big for autotracking!\r\n", ch );
      return;
   }

   if( ( ship = ship_from_pilotseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou aren't in the pilots chair!\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&RYou'll have to turn off the ships autopilot first....\r\n", ch );
      return;
   }

   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_shipsystems] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "&RYour notsure which switch to flip.\r\n", ch );
      learn_from_failure( ch, gsn_shipsystems );
      return;
   }

   act( AT_PLAIN, "$n flips a switch on the control panel.", ch, NULL, argument, TO_ROOM );
   if( ship->autotrack )
   {
      ship->autotrack = FALSE;
      echo_to_cockpit( AT_YELLOW, ship, "Autotracking off." );
   }
   else
   {
      ship->autotrack = TRUE;
      echo_to_cockpit( AT_YELLOW, ship, "Autotracking on." );
   }

   learn_from_success( ch, gsn_shipsystems );
}

void do_jumpvector( CHAR_DATA * ch, const char *argument )
{
}

void do_closebay( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   int chance;

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
   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RPlatforms don't have bay doors. That's why they're platforms.\r\n", ch );
      return;
   }

   if( ( ship = ship_from_pilotseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou aren't in the pilots chair!\r\n", ch );
      return;
   }
   if( !ship->bayopen )
   {
      send_to_char( "&RThe bay is already closed.\r\n", ch );
      return;
   }

   chance = IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_shipsystems];
   if( number_percent(  ) > chance )
   {
      send_to_char( "&RWell, it looks a lot easier than it really is.\r\n", ch );
      learn_from_failure( ch, gsn_shipsystems );
      return;
   }

   act( AT_PLAIN, "$n toggles a switch on the control panel.", ch, NULL, argument, TO_ROOM );

   adjust_bay( ship, FALSE );
   learn_from_success( ch, gsn_shipsystems );
}

void do_openbay( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   int chance;

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
   if( ship->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RPlatforms don't have bay doors. That's why they're platforms.\r\n", ch );
      return;
   }

   if( ( ship = ship_from_pilotseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou aren't in the pilots chair!\r\n", ch );
      return;
   }
   if( ship->bayopen )
   {
      send_to_char( "&RThe bay is already open.\r\n", ch );
      return;
   }

   chance = IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_shipsystems];
   if( number_percent(  ) > chance )
   {
      send_to_char( "&RWell, it looks a lot easier than it really is.\r\n", ch );
      learn_from_failure( ch, gsn_shipsystems );
      return;
   }

   act( AT_PLAIN, "$n toggles a switch on the control panel.", ch, NULL, argument, TO_ROOM );

   adjust_bay( ship, TRUE );
   learn_from_success( ch, gsn_shipsystems );
}

void do_tractorbeam( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   SHIP_DATA *target;
   int chance;

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

   if( ship->tractorbeam == 0 )
   {
      send_to_char( "&RYou might want to install a tractor beam module first!\r\n", ch );
      return;
   }

   if( ship->hanger == 0 )
   {
      send_to_char( "&RNo hangar available.\r\n", ch );
      return;
   }

   if( !ship->bayopen )
   {
      send_to_char( "&RYour hangar bay is closed.\r\n", ch );
      return;
   }

   if( ( ship = ship_from_pilotseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou need to be in the pilot seat!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_DISABLED )
   {
      send_to_char( "&RWARNING: drives disabled! No power available.\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_DOCKED || ship->shipstate2 != SHIP_READY )
   {
      send_to_char( "&RYour ship is docked!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_HYPERSPACE )
   {
      send_to_char( "&RYou can only do that in realspace!\r\n", ch );
      return;
   }

   if( ship->shipstate != SHIP_READY )
   {
      send_to_char( "&RPlease wait until the ship has finished its current maneuver.\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
   {
      send_to_char( "&RCapture what?\r\n", ch );
      return;
   }

   target = get_ship_here( argument, ship->starsystem );

   if( target == NULL || IS_SET( target->flags, SHIPFLAG_CLOAKED ) )
   {
      send_to_char( "&RI don't see that here.\r\n", ch );
      return;
   }

   if( target == ship )
   {
      send_to_char( "&RYou can't tractor in your own ship.\r\n", ch );
      return;
   }

   if( IS_DOCKED( target ) )
   {
      send_to_char( "&RThat ship is already docked to something.\r\n", ch );
      return;
   }

   if( target->ship_class == SHIP_PLATFORM )
   {
      send_to_char( "&RYou can't capture platforms.\r\n", ch );
      return;
   }

   if( ship->ship_class <= target->ship_class )
   {
      send_to_char( "&RThat ship is too big for your hangar.\r\n", ch );
      return;
   }

   if( ship->energy < ( 25 + 25 * target->ship_class ) )
   {
      send_to_char( "&RWARNING! Insufficient energy for tractor beam!\r\n", ch );
      return;
   }

   chance = IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_tractorbeams];
   chance = chance * ( ship->tractorbeam / ( target->currspeed + 1 ) );

   if( number_percent(  ) < chance )
   {
      set_char_color( AT_GREEN, ch );
      send_to_char( "Tractor beam initiated.\r\n", ch );
      act( AT_PLAIN, "$n activates the capturing command sequence.", ch, NULL, argument, TO_ROOM );
      echo_to_ship( AT_YELLOW, ship, "ALERT: Hangar tractor beam locked on target." );
      echo_to_ship( AT_YELLOW, target, "The ship shudders as a tractor beam locks on." );

      ship->shipstate2 = SHIP_DOCK;
      target->shipstate2 = SHIP_DOCK;
      ship->docked_ship = target;
      target->docked_ship = ship;

      ship->energy -= ( 25 + 25 * target->ship_class );
      save_ship( ship );
      save_ship( target );

      learn_from_success( ch, gsn_tractorbeams );
   }
   else
   {
      send_to_char( "&RThe target slips out of your tractor beam's lock.\r\n", ch );
      learn_from_failure( ch, gsn_tractorbeams );
   }
}

void do_pluogus( CHAR_DATA * ch, const char *argument )
{
   bool ch_comlink = FALSE;
   OBJ_DATA *obj;
   int next_planet, itt;

   for( obj = ch->last_carrying; obj; obj = obj->prev_content )
   {
      if( obj->pIndexData->item_type == ITEM_COMLINK )
         ch_comlink = TRUE;
   }

   if( !ch_comlink )
   {
      send_to_char( "You need a comlink to do that!\r\n", ch );
      return;
   }

   send_to_char( "Serin Pluogus Schedule Information:\r\n", ch );

   /*
    * current port 
    */
   if( bus_pos < 7 && bus_pos > 1 )
      ch_printf( ch, "The Pluogus is Currently docked at %s.\r\n", bus_stop[bus_planet] );

   /*
    * destinations 
    */
   next_planet = bus_planet;
   send_to_char( "Next stops: ", ch );

   if( bus_pos <= 1 )
      ch_printf( ch, "%s  ", bus_stop[next_planet] );

   for( itt = 0; itt < 3; itt++ )
   {
      next_planet++;
      if( next_planet >= MAX_BUS_STOP )
         next_planet = 0;
      ch_printf( ch, "%s  ", bus_stop[next_planet] );
   }

   ch_printf( ch, "\r\n\r\n" );

   send_to_char( "Serin Tocca Schedule Information:\r\n", ch );

   /*
    * current port 
    */

   if( bus_pos < 7 && bus_pos > 1 )
      ch_printf( ch, "The Tocca is Currently docked at %s.\r\n", bus_stop[bus2_planet] );

   /*
    * destinations 
    */

   next_planet = bus2_planet;
   send_to_char( "Next stops: ", ch );

   if( bus_pos <= 1 )
      ch_printf( ch, "%s  ", bus_stop[next_planet] );

   for( itt = 0; itt < 3; itt++ )
   {
      next_planet++;
      if( next_planet >= MAX_BUS_STOP )
         next_planet = 0;
      ch_printf( ch, "%s  ", bus_stop[next_planet] );
   }

   ch_printf( ch, "\r\n" );
}

void do_fly( CHAR_DATA * ch, const char *argument )
{
}

void do_drive( CHAR_DATA * ch, const char *argument )
{
   int dir;
   SHIP_DATA *ship;

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RYou must be in the drivers seat of a land vehicle to do that!\r\n", ch );
      return;
   }

   if( ship->ship_class < LAND_SPEEDER )
   {
      send_to_char( "&RThis isn't a land vehicle!\r\n", ch );
      return;
   }


   if( ship->shipstate == SHIP_DISABLED )
   {
      send_to_char( "&RThe drive is disabled.\r\n", ch );
      return;
   }

   if( ship->energy < 1 )
   {
      send_to_char( "&RTheres not enough fuel!\r\n", ch );
      return;
   }

   if( ( dir = get_door( argument ) ) == -1 )
   {
      send_to_char( "Usage: drive <direction>\r\n", ch );
      return;
   }

   drive_ship( ch, ship, get_exit( get_room_index( ship->location ), dir ), 0 );
}

ch_ret drive_ship( CHAR_DATA * ch, SHIP_DATA * ship, EXIT_DATA * pexit, int fall )
{
   ROOM_INDEX_DATA *in_room;
   ROOM_INDEX_DATA *to_room;
   ROOM_INDEX_DATA *original;
   char buf[MAX_STRING_LENGTH];
   const char *txt;
   const char *dtxt;
   ch_ret retcode;
   short door;
   bool drunk = FALSE;
   CHAR_DATA *rch;
   CHAR_DATA *next_rch;

   if( !IS_NPC( ch ) )
      if( IS_DRUNK( ch, 2 ) && ( ch->position != POS_SHOVE ) && ( ch->position != POS_DRAG ) )
         drunk = TRUE;

   if( drunk && !fall )
   {
      door = number_door(  );
      pexit = get_exit( get_room_index( ship->location ), door );
   }

#ifdef DEBUG
   if( pexit )
   {
      snprintf( buf, MAX_STRING_LENGTH, "%s: %s to door %d", __func__, ch->name, pexit->vdir );
      log_string( buf );
   }
#endif

   retcode = rNONE;
   txt = NULL;

   in_room = get_room_index( ship->location );

   if( !pexit || ( to_room = pexit->to_room ) == NULL )
   {
      if( drunk )
         send_to_char( "You drive into a wall in your drunken state.\r\n", ch );
      else
         send_to_char( "Alas, you cannot go that way.\r\n", ch );
      return rNONE;
   }

   door = pexit->vdir;

   if( IS_SET( pexit->exit_info, EX_WINDOW ) && !IS_SET( pexit->exit_info, EX_ISDOOR ) )
   {
      send_to_char( "Alas, you cannot go that way.\r\n", ch );
      return rNONE;
   }

   if( IS_SET( pexit->exit_info, EX_PORTAL ) && IS_NPC( ch ) )
   {
      act( AT_PLAIN, "Mobs can't use portals.", ch, NULL, NULL, TO_CHAR );
      return rNONE;
   }

   if( IS_SET( pexit->exit_info, EX_NOMOB ) && IS_NPC( ch ) )
   {
      act( AT_PLAIN, "Mobs can't enter there.", ch, NULL, NULL, TO_CHAR );
      return rNONE;
   }

   if( IS_SET( pexit->exit_info, EX_CLOSED ) && ( IS_SET( pexit->exit_info, EX_NOPASSDOOR ) ) )
   {
      if( !IS_SET( pexit->exit_info, EX_SECRET ) && !IS_SET( pexit->exit_info, EX_DIG ) )
      {
         if( drunk )
         {
            act( AT_PLAIN, "$n drives into the $d in $s drunken state.", ch, NULL, pexit->keyword, TO_ROOM );
            act( AT_PLAIN, "You drive into the $d in your drunken state.", ch, NULL, pexit->keyword, TO_CHAR );
         }
         else
            act( AT_PLAIN, "The $d is closed.", ch, NULL, pexit->keyword, TO_CHAR );
      }
      else
      {
         if( drunk )
            send_to_char( "You hit a wall in your drunken state.\r\n", ch );
         else
            send_to_char( "Alas, you cannot go that way.\r\n", ch );
      }

      return rNONE;
   }

   if( room_is_private( ch, to_room ) )
   {
      send_to_char( "That room is private right now.\r\n", ch );
      return rNONE;
   }

   if( !IS_IMMORTAL( ch ) && !IS_NPC( ch ) && ch->in_room->area != to_room->area )
   {
      if( ch->top_level < to_room->area->low_hard_range )
      {
         set_char_color( AT_TELL, ch );
         switch ( to_room->area->low_hard_range - ch->top_level )
         {
            case 1:
               send_to_char( "A voice in your mind says, 'You are nearly ready to go that way...'", ch );
               break;
            case 2:
               send_to_char( "A voice in your mind says, 'Soon you shall be ready to travel down this path... soon.'", ch );
               break;
            case 3:
               send_to_char( "A voice in your mind says, 'You are not ready to go down that path... yet.'.\r\n", ch );
               break;
            default:
               send_to_char( "A voice in your mind says, 'You are not ready to go down that path.'.\r\n", ch );
         }
         return rNONE;
      }
      else if( ch->top_level > to_room->area->hi_hard_range )
      {
         set_char_color( AT_TELL, ch );
         send_to_char( "A voice in your mind says, 'There is nothing more for you down that path.'", ch );
         return rNONE;
      }
   }

   if( !fall )
   {
      if( IS_SET( to_room->room_flags, ROOM_INDOORS )
          || IS_SET( to_room->room_flags, ROOM_SPACECRAFT ) || to_room->sector_type == SECT_INSIDE )
      {
         send_to_char( "You can't drive indoors!\r\n", ch );
         return rNONE;
      }

      if( IS_SET( to_room->room_flags, ROOM_NO_DRIVING ) )
      {
         send_to_char( "You can't take a vehicle through there!\r\n", ch );
         return rNONE;
      }

      if( in_room->sector_type == SECT_AIR || to_room->sector_type == SECT_AIR || IS_SET( pexit->exit_info, EX_FLY ) )
      {
         if( ship->ship_class > CLOUD_CAR )
         {
            send_to_char( "You'd need to fly to go there.\r\n", ch );
            return rNONE;
         }
      }

      if( in_room->sector_type == SECT_WATER_NOSWIM
          || to_room->sector_type == SECT_WATER_NOSWIM
          || to_room->sector_type == SECT_WATER_SWIM
          || to_room->sector_type == SECT_UNDERWATER || to_room->sector_type == SECT_OCEANFLOOR )
      {

         if( ship->ship_class != OCEAN_SHIP )
         {
            send_to_char( "You'd need a boat to go there.\r\n", ch );
            return rNONE;
         }

      }

      if( IS_SET( pexit->exit_info, EX_CLIMB ) )
      {

         if( ship->ship_class < CLOUD_CAR )
         {
            send_to_char( "You need to fly or climb to get up there.\r\n", ch );
            return rNONE;
         }
      }

   }

   if( to_room->tunnel > 0 )
   {
      CHAR_DATA *ctmp;
      int count = 0;

      for( ctmp = to_room->first_person; ctmp; ctmp = ctmp->next_in_room )
         if( ++count >= to_room->tunnel )
         {
            send_to_char( "There is no room for you in there.\r\n", ch );
            return rNONE;
         }
   }

   if( fall )
      txt = "falls";
   else if( !txt )
   {
      if( ship->ship_class < OCEAN_SHIP )
         txt = "fly";
      else if( ship->ship_class == OCEAN_SHIP )
      {
         txt = "float";
      }
      else if( ship->ship_class > OCEAN_SHIP )
      {
         txt = "drive";
      }
   }
   snprintf( buf, MAX_STRING_LENGTH, "$n %ss the vehicle $T.", txt );
   act( AT_ACTION, buf, ch, NULL, dir_name[door], TO_ROOM );
   snprintf( buf, MAX_STRING_LENGTH, "You %s the vehicle $T.", txt );
   act( AT_ACTION, buf, ch, NULL, dir_name[door], TO_CHAR );
   snprintf( buf, MAX_STRING_LENGTH, "%s %ss %s.", ship->name, txt, dir_name[door] );
   echo_to_room( AT_ACTION, get_room_index( ship->location ), buf );

   extract_ship( ship );
   ship_to_room( ship, to_room->vnum );

   ship->location = to_room->vnum;
   ship->lastdoc = ship->location;

   if( fall )
      txt = "falls";
   else if( ship->ship_class < OCEAN_SHIP )
      txt = "flys in";
   else if( ship->ship_class == OCEAN_SHIP )
   {
      txt = "floats in";
   }
   else if( ship->ship_class > OCEAN_SHIP )
   {
      txt = "drives in";
   }

   switch ( door )
   {
      default:
         dtxt = "somewhere";
         break;
      case 0:
         dtxt = "the south";
         break;
      case 1:
         dtxt = "the west";
         break;
      case 2:
         dtxt = "the north";
         break;
      case 3:
         dtxt = "the east";
         break;
      case 4:
         dtxt = "below";
         break;
      case 5:
         dtxt = "above";
         break;
      case 6:
         dtxt = "the south-west";
         break;
      case 7:
         dtxt = "the south-east";
         break;
      case 8:
         dtxt = "the north-west";
         break;
      case 9:
         dtxt = "the north-east";
         break;
   }

   snprintf( buf, MAX_STRING_LENGTH, "%s %s from %s.", ship->name, txt, dtxt );
   echo_to_room( AT_ACTION, get_room_index( ship->location ), buf );

   for( rch = ch->in_room->last_person; rch; rch = next_rch )
   {
      next_rch = rch->prev_in_room;
      original = rch->in_room;
      char_from_room( rch );
      char_to_room( rch, to_room );
      do_look( rch, "auto" );
      char_from_room( rch );
      char_to_room( rch, original );
   }

/*
    if (  CHECK FOR FALLING HERE
    &&   fall > 0 )
    {
	if (!IS_AFFECTED( ch, AFF_FLOATING )
	|| ( ch->mount && !IS_AFFECTED( ch->mount, AFF_FLOATING ) ) )
	{
	  set_char_color( AT_HURT, ch );
	  send_to_char( "OUCH! You hit the ground!\r\n", ch );
	  WAIT_STATE( ch, 20 );
	  retcode = damage( ch, ch, 50 * fall, TYPE_UNDEFINED );
	}
	else
	{
	  set_char_color( AT_MAGIC, ch );
	  send_to_char( "You lightly float down to the ground.\r\n", ch );
	}
    }

*/
   return retcode;
}

void do_bomb( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   PLANET_DATA *planet;
   int chance, cost, reduction;

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
      send_to_char( "&RYou need to be in the pilot seat!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_DISABLED )
   {
      send_to_char( "&RThe ship's drive is disabled. Unable to attack.\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_DOCKED || ship->shipstate2 != SHIP_READY )
   {
      send_to_char( "&RYour ship is docked!\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_HYPERSPACE )
   {
      send_to_char( "&RYou can only do that in realspace!\r\n", ch );
      return;
   }

   if( ship->shipstate != SHIP_READY )
   {
      send_to_char( "&RPlease wait until the ship has finished its current maneuver.\r\n", ch );
      return;
   }

   if( ship->starsystem == NULL )
   {
      send_to_char( "&RThere's no planet to bomb out here.\r\n", ch );
      return;
   }

   if( argument[0] == '\0' )
   {
      send_to_char( "&RBomb which planet?\r\n", ch );
      return;
   }

   if( ( planet = get_planet( argument ) ) == NULL || planet->starsystem != ship->starsystem )
   {
      send_to_char( "&RI don't see that planet here.\r\n", ch );
      return;
   }

   if( IS_SET( planet->flags, PLANET_NOCAPTURE ) )
   {
      send_to_char( "&RThis planet cannot be bombed.\r\n", ch );
      return;
   }

   if( !planet->governed_by )
   {
      send_to_char( "&RThat planet has no government to undermine.\r\n", ch );
      return;
   }

   cost = 50 + 25 * ship->ship_class;
   if( ship->energy < cost )
   {
      send_to_char( "&RWARNING! Insufficient energy for a bombing run!\r\n", ch );
      return;
   }

   chance = IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_spacecombat];

   if( number_percent(  ) > chance )
   {
      send_to_char( "&RYour bombing run goes wide and does nothing but scorch open ground.\r\n", ch );
      act( AT_PLAIN, "$n makes a bombing run, but misses entirely.", ch, NULL, argument, TO_ROOM );
      ship->energy -= cost;
      learn_from_failure( ch, gsn_spacecombat );
      save_ship( ship );
      return;
   }

   reduction = number_range( 5, 15 ) + ( 3 * ship->ship_class );
   planet->pop_support -= reduction;
   if( planet->pop_support < -100 )
      planet->pop_support = -100;

   ship->energy -= cost;

   set_char_color( AT_GREEN, ch );
   ch_printf( ch, "Your bombing run scores a direct hit on %s!\r\n", planet->name );
   act( AT_PLAIN, "$n makes a bombing run on the planet below.", ch, NULL, argument, TO_ROOM );
   ch_printf( ch, "Popular support for %s's government drops sharply.\r\n", planet->name );

   learn_from_success( ch, gsn_spacecombat );
   save_ship( ship );
   save_planet( planet );
}

void do_chaff( CHAR_DATA * ch, const char *argument )
{
   int schance;
   SHIP_DATA *ship;


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


   if( ( ship = ship_from_coseat( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "&RThe controls are at the copilots seat!\r\n", ch );
      return;
   }

   if( autofly( ship ) )
   {
      send_to_char( "&RYou'll have to turn the autopilot off first...\r\n", ch );
      return;
   }

   if( ship->shipstate == SHIP_HYPERSPACE )
   {
      send_to_char( "&RYou can only do that in realspace!\r\n", ch );
      return;
   }
   if( ship->shipstate == SHIP_DOCKED )
   {
      send_to_char( "&RYou can't do that until after you've launched!\r\n", ch );
      return;
   }
   if( ship->chaff <= 0 )
   {
      send_to_char( "&RYou don't have any chaff to release!\r\n", ch );
      return;
   }
   schance = IS_NPC( ch ) ? ch->top_level : ( int )( ch->pcdata->learned[gsn_weaponsystems] );
   if( number_percent(  ) > schance )
   {
      send_to_char( "&RYou can't figure out which switch it is.\r\n", ch );
      learn_from_failure( ch, gsn_weaponsystems );
      return;
   }

   ship->chaff--;

   ship->chaff_released++;

   send_to_char( "You flip the chaff release switch.\r\n", ch );
   act( AT_PLAIN, "$n flips a switch on the control pannel", ch, NULL, argument, TO_ROOM );
   echo_to_cockpit( AT_YELLOW, ship, "A burst of chaff is released from the ship." );

   learn_from_success( ch, gsn_weaponsystems );
}

bool autofly( SHIP_DATA * ship )
{

   if( !ship )
      return FALSE;

   if( ship->type == MOB_SHIP )
      return TRUE;

   if( ship->autopilot )
      return TRUE;

   return FALSE;
}

/* Generic Pilot Command To use as template

void do_hmm( CHAR_DATA *ch, const char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    int schance;
    SHIP_DATA *ship;
    
    strlcpy( arg, argument, MAX_INPUT_LENGTH );    
    
    switch( ch->substate )
    { 
    	default:
    	        if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
    	        {
    	            send_to_char("&RYou must be in the cockpit of a ship to do that!\r\n",ch);
    	            return;
    	        }
                if (ship->shipstate == SHIP_HYPERSPACE)
                {
                  send_to_char("&RYou can only do that in realspace!\r\n",ch);
                  return;   
                }
                if (ship->shipstate == SHIP_DISABLED)
    	        {
    	            send_to_char("&RThe ships drive is disabled. Unable to manuever.\r\n",ch);
    	            return;
    	        }
    	        if (ship->shipstate == SHIP_DOCKED)
    	        {
    	            send_to_char("&RYou can't do that until after you've launched!\r\n",ch);
    	            return;
    	        }
    	        if (ship->shipstate != SHIP_READY)
    	        {
    	            send_to_char("&RPlease wait until the ship has finished its current manouver.\r\n",ch);
    	            return;
    	        }
        
                if ( ship->energy <1 )
    	        {
    	           send_to_char("&RTheres not enough fuel!\r\n",ch);
    	           return;
    	        }
    	        
                if ( ship->ship_class == FIGHTER_SHIP )
                    schance = IS_NPC(ch) ? ch->top_level
	                 : (int)  (ch->pcdata->learned[gsn_starfighters]) ;
                if ( ship->ship_class == MIDSIZE_SHIP )
                    schance = IS_NPC(ch) ? ch->top_level
	                 : (int)  (ch->pcdata->learned[gsn_midships]) ;
                if ( ship->ship_class == FRIGATE_SHIP )
                    schance = IS_NPC(ch) ? ch->top_level
	                 : (int) (ch->pcdata->learned[gsn_frigates]);
                if ( ship->ship_class == CAPITAL_SHIP )
                    schance = IS_NPC(ch) ? ch->top_level
	                 : (int) (ch->pcdata->learned[gsn_capitalships]);
                if ( ship->ship_class == SUPERCAPITAL_SHIP )
                    schance = IS_NPC(ch) ? ch->top_level
	                 : (int) (ch->pcdata->learned[gsn_supercapitalships]);
                if ( number_percent( ) < schance )
    		{
    		   send_to_char( "&G\r\n", ch);
    		   act( AT_PLAIN, "$n does  ...", ch,
		        NULL, argument , TO_ROOM );
		   echo_to_room( AT_YELLOW , get_room_index(ship->cockpit) , "");
    		   add_timer ( ch , TIMER_DO_FUN , 1 , do_hmm , 1 );
    		   ch->dest_buf = strdup(arg);
    		   return;
	        }
	        send_to_char("&RYou fail to work the controls properly.\r\n",ch);
	        if ( ship->ship_class == FIGHTER_SHIP )
                    learn_from_failure( ch, gsn_starfighters );
                if ( ship->ship_class == MIDSIZE_SHIP )
    	            learn_from_failure( ch, gsn_midships );
                if( ship->ship_class == FRIGATE_SHIP )
                    learn_from_failure( ch, gsn_frigates );
                if( ship->ship_class == CAPITAL_SHIP )
                    learn_from_failure( ch, gsn_capitalships );
                if( ship->ship_class == SUPERCAPITAL_SHIP )
                    learn_from_failure( ch, gsn_supercapitalships );
    	   	return;	
    	
    	case 1:
    		if ( !ch->dest_buf )
    		   return;
    		strlcpy(arg, ch->dest_buf, MAX_INPUT_LENGTH);
    		DISPOSE( ch->dest_buf);
    		break;
    		
    	case SUB_TIMER_DO_ABORT:
    		DISPOSE( ch->dest_buf );
    		ch->substate = SUB_NONE;
    		if ( (ship = ship_from_cockpit(ch->in_room->vnum)) == NULL )
    		      return;    		                                   
    	        send_to_char("&Raborted.\r\n", ch);
    	        echo_to_room( AT_YELLOW , get_room_index(ship->cockpit) , "");
    		if (ship->shipstate != SHIP_DISABLED)
    		   ship->shipstate = SHIP_READY;
    		return;
    }
    
    ch->substate = SUB_NONE;
    
    if ( (ship = ship_from_cockpit(ch->in_room->vnum)) == NULL )
    {  
       return;
    }

    send_to_char( "&G\r\n", ch);
    act( AT_PLAIN, "$n does  ...", ch,
         NULL, argument , TO_ROOM );
    echo_to_room( AT_YELLOW , get_room_index(ship->cockpit) , "");

         
    if ( ship->ship_class == FIGHTER_SHIP )
        learn_from_success( ch, gsn_starfighters );
    if ( ship->ship_class == MIDSIZE_SHIP )
        learn_from_success( ch, gsn_midships );
    if( ship->ship_class == FRIGATE_SHIP )
        learn_from_success( ch, gsn_frigates );
    if( ship->ship_class == CAPITAL_SHIP )
        learn_from_success( ch, gsn_capitalships );
    if( ship->ship_class == SUPERCAPITAL_SHIP )
        learn_from_success( ch, gsn_supercapitalships );
    	
}

void do_hmm( CHAR_DATA *ch, const char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    int schance;
    SHIP_DATA *ship;
    
   
        if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
        {
            send_to_char("&RYou must be in the cockpit of a ship to do that!\r\n",ch);
            return;
        }
        
                if (ship->shipstate == SHIP_HYPERSPACE)
                {
                  send_to_char("&RYou can only do that in realspace!\r\n",ch);
                  return;   
                }
                if (ship->shipstate == SHIP_DISABLED)
    	        {
    	            send_to_char("&RThe ships drive is disabled. Unable to manuever.\r\n",ch);
    	            return;
    	        }
    	        if (ship->shipstate == SHIP_DOCKED)
    	        {
    	            send_to_char("&RYou can't do that until after you've launched!\r\n",ch);
    	            return;
    	        }
    	        if (ship->shipstate != SHIP_READY)
    	        {
    	            send_to_char("&RPlease wait until the ship has finished its current manouver.\r\n",ch);
    	            return;
    	        } 
        
        if ( ship->energy <1 )
        {
              send_to_char("&RTheres not enough fuel!\r\n",ch);
              return;
        }
    	        
        if ( ship->ship_class == FIGHTER_SHIP )
             schance = IS_NPC(ch) ? ch->top_level
             : (int)  (ch->pcdata->learned[gsn_starfighters]) ;
        if ( ship->ship_class == MIDSIZE_SHIP )
             schance = IS_NPC(ch) ? ch->top_level
                 : (int)  (ch->pcdata->learned[gsn_midships]) ;
        if ( ship->ship_class == FRIGATE_SHIP )
              schance = IS_NPC(ch) ? ch->top_level
                 : (int) (ch->pcdata->learned[gsn_frigates]);
        if ( ship->ship_class == CAPITAL_SHIP )
              schance = IS_NPC(ch) ? ch->top_level
                 : (int) (ch->pcdata->learned[gsn_capitalships]);
        if ( ship->ship_class == SUPERCAPITAL_SHIP )
              schance = IS_NPC(ch) ? ch->top_level
                 : (int) (ch->pcdata->learned[gsn_supercapitalships]);
        if ( number_percent( ) > schance )
        {
            send_to_char("&RYou fail to work the controls properly.\r\n",ch);
            if ( ship->ship_class == FIGHTER_SHIP )
               learn_from_failure( ch, gsn_starfighters );
            if ( ship->ship_class == MIDSIZE_SHIP )   
               learn_from_failure( ch, gsn_midships );
            if( ship->ship_class == FRIGATE_SHIP )
                learn_from_failure( ch, gsn_frigates );
            if( ship->ship_class == CAPITAL_SHIP )
                learn_from_failure( ch, gsn_capitalships );
            if( ship->ship_class == SUPERCAPITAL_SHIP )
                learn_from_failure( ch, gsn_supercapitalships );
    	   return;	
        }
        
    send_to_char( "&G\r\n", ch);
    act( AT_PLAIN, "$n does  ...", ch,
         NULL, argument , TO_ROOM );
    echo_to_room( AT_YELLOW , get_room_index(ship->cockpit) , "");
	  
    
    
    if ( ship->ship_class == FIGHTER_SHIP )
        learn_from_success( ch, gsn_starfighters );
    if ( ship->ship_class == MIDSIZE_SHIP )
        learn_from_success( ch, gsn_midships );
    if( ship->ship_class == FRIGATE_SHIP )
        learn_from_success( ch, gsn_frigates );
    if( ship->ship_class == CAPITAL_SHIP )
        learn_from_success( ch, gsn_capitalships );
    if( ship->ship_class == SUPERCAPITAL_SHIP )
        learn_from_success( ch, gsn_supercapitalships );
    	
}

*/


/* ===== GD helper functions for ported space commands ===== */

float speedbonus(SHIP_DATA *ship)
{
   float accelbonus;
   accelbonus = 1.0;
   if(IS_SET(ship->flags, SHIPFLAG_OVERDRIVENODE))
      accelbonus *= 2;
   if(IS_SET(ship->flags, SHIPFLAG_AFTERBURNER))
      accelbonus *= 1.1;
   if(IS_SET(ship->flags, SHIPFLAG_LASERRENGINE))
      accelbonus *= 1.1;
   if(IS_SET(ship->flags, SHIPFLAG_SHIELDRENGINE))
      accelbonus *= 1.1;
   if(IS_SET(ship->flags, SHIPFLAG_ENGINERLASER))
      accelbonus *= .9;
   if(IS_SET(ship->flags, SHIPFLAG_ENGINERSHIELD))
      accelbonus *= .9;
   return accelbonus;
}

/* Canonical class -> pilot-skill map, ported from SWGD v1.22 */
short shipskill( SHIP_DATA *ship )
{
   switch( ship->ship_class )
   {
   default:                return -1;
   case FIGHTER_SHIP:      return gsn_starfighters;
   case MIDSIZE_SHIP:      return gsn_midships;
   case FRIGATE_SHIP:      return gsn_frigates;
   case SUPERCAPITAL_SHIP: return gsn_supercapitalships;
   case CAPITAL_SHIP:      return gsn_capitalships;
   case SHIP_PLATFORM:     return gsn_shipsystems;
   }
}

bool check_sabotage( SHIP_DATA *ship, int check )
{
   char buf[MAX_STRING_LENGTH];

   if( !IS_SET( ship->flags, check ) )
      return FALSE;

   switch( check )
   {
   default: return FALSE; 
   /* Valid sabotage flags below this point
    * -Lajos
    */
   case SHIPFLAG_SABOTAGEDLASERS:
       echo_to_room( AT_BLUE , get_room_index(ship->gunseat) , "Fumes of smoke rise from the control panel, and a small explosion sparks!\n\r");
       echo_to_room( AT_BLOOD + AT_BLINK , get_room_index(ship->gunseat) , "LASER DAMAGED! SYSTEM OFFLINE!\n\r" );
       ship->statet0 = LASER_DAMAGED - 10;
       REMOVE_BIT(ship->flags, SHIPFLAG_SABOTAGEDLASERS);
       sound_to_room( get_room_index(ship->gunseat) , "!!SOUND(lando_fault.wav P=100 V=100)" );
       return TRUE;
   case SHIPFLAG_SABOTAGEDIONS:
       echo_to_room( AT_BLUE , get_room_index(ship->turret2) , "The ion cannons disperse a sudden wave of energy!\n\r");
       echo_to_room( AT_BLOOD + AT_BLINK , get_room_index(ship->turret2) , "WARNING! IONS DAMAGED!\n\r" );
       ship->ionstate = LASER_DAMAGED - 10;
       REMOVE_BIT(ship->flags, SHIPFLAG_SABOTAGEDIONS);
       sound_to_room( get_room_index(ship->gunseat) , "!!SOUND(lando_fault.wav P=100 V=100)" );
       return TRUE;       
   case SHIPFLAG_SABOTAGEDENGINE:
       echo_to_cockpit( AT_BLUE , ship , "A massive explosion rocks your ship as smoke pours through the ventilation system!\n\r");
       echo_to_cockpit( AT_BLOOD + AT_BLINK , ship , "CRITICAL ALERT! SHIP DRIVES DISABLED!");
       ship->shipstate = SHIP_DISABLED;
       if( ship->currspeed )
       {
          echo_to_cockpit( AT_YELLOW , ship , "The ship begins to slow down.");
          sprintf( buf, "%s begins to slow down." , ship->name );
          echo_to_system( AT_ORANGE , ship , buf , NULL );
       }
       ship->currspeed = 0;
       REMOVE_BIT(ship->flags, SHIPFLAG_SABOTAGEDENGINE);
       sound_to_room( get_room_index(ship->pilotseat) , "!!SOUND(lando_fault.wav P=100 V=100)" );
       return TRUE;     
   case SHIPFLAG_SABOTAGEDTURRET1:    
       echo_to_room( AT_BLUE , get_room_index(ship->turret1) , "The laser turret swivels as blue sparks jolt up from your control panel!\n\r");
       echo_to_room( AT_BLOOD + AT_BLINK , get_room_index(ship->turret1) , "WARNING! TURRETS DAMAGED!\n\r" );
       ship->statet1 = LASER_DAMAGED - 10;
       REMOVE_BIT(ship->flags, SHIPFLAG_SABOTAGEDTURRET1);
       sound_to_room( get_room_index(ship->turret1) , "!!SOUND(lando_fault.wav P=100 V=100)" );
       return TRUE;
   case SHIPFLAG_SABOTAGEDTURRET2:
       echo_to_room( AT_BLUE , get_room_index(ship->turret2) , "The control panel begins to smoke as the turbolaser turrets repeatedly recoil!\n\r");
       echo_to_room( AT_BLOOD + AT_BLINK , get_room_index(ship->turret2) , "WARNING! Turbolasers offline!\n\r" );
       ship->statet2 = LASER_DAMAGED - 10;
       REMOVE_BIT(ship->flags, SHIPFLAG_SABOTAGEDTURRET2);
       sound_to_room( get_room_index(ship->turret2) , "!!SOUND(lando_fault.wav P=100 V=100)" );
       return TRUE;    
   case SHIPFLAG_SABOTAGEDLAUNCHERS: 
       echo_to_room( AT_BLUE , get_room_index(ship->gunseat) , "An explosion sways your ship as the missile launcher short-circuits!\n\r");
       echo_to_room( AT_BLOOD + AT_BLINK , get_room_index(ship->gunseat) , "WARNING! MISSILE LAUNCHERS JAMMED!\n\r" );
       ship->missilestate = MISSILE_DAMAGED - 10;
       REMOVE_BIT(ship->flags, SHIPFLAG_SABOTAGEDLAUNCHERS);
       sound_to_room( get_room_index(ship->gunseat) , "!!SOUND(lando_fault.wav P=100 V=100)" );
       return TRUE; 
   case SHIPFLAG_SABOTAGEDRLAUNCHERS: 
       echo_to_room( AT_BLUE , get_room_index(ship->gunseat) , "You hear a loud thud as your rocket launchers lurch and jam!\n\r");
       echo_to_room( AT_BLOOD + AT_BLINK , get_room_index(ship->gunseat) , "WARNING! Rocket jammed in launcher, system offline!\n\r" );
       ship->rocketstate = MISSILE_DAMAGED - 10;
       REMOVE_BIT(ship->flags, SHIPFLAG_SABOTAGEDRLAUNCHERS);
       sound_to_room( get_room_index(ship->gunseat) , "!!SOUND(lando_fault.wav P=100 V=100)" );
       return TRUE;
   case SHIPFLAG_SABOTAGEDTLAUNCHERS: 
       echo_to_room( AT_BLUE , get_room_index(ship->gunseat) , "A crunch sounds from the torpedo launchers as the tracking system flickers!\n\r");
       echo_to_room( AT_BLOOD + AT_BLINK , get_room_index(ship->gunseat) , "WARNING! Torpedo launchers damaged, system offline!\n\r" );
       ship->torpedostate = MISSILE_DAMAGED - 10;
       REMOVE_BIT(ship->flags, SHIPFLAG_SABOTAGEDTLAUNCHERS);
       sound_to_room( get_room_index(ship->gunseat) , "!!SOUND(lando_fault.wav P=100 V=100)" );
       return TRUE;
   }
   return TRUE;
}

void rotate_ship( SHIP_DATA *ship, double angle_x, double angle_y, double angle_z )
{
   /* 3D rotation matrix not implemented in this codebase.
    * GD used xyz_matrix[] for full 3D ship orientation tracking.
    * In this codebase, ship heading uses hx/hy/hz vectors directly.
    */
   ( void ) ship;
   ( void ) angle_x;
   ( void ) angle_y;
   ( void ) angle_z;
}

void adjust_bay( SHIP_DATA *ship, bool fOpen )
{
   char buf[MAX_STRING_LENGTH];

   if( fOpen && ship->bayopen )
      return;
   if( !fOpen && !ship->bayopen )
      return;
   if( fOpen )
   {
      ship->bayopen = TRUE;   
      echo_to_cockpit( AT_YELLOW, ship, "Opening bay doors." );
      sprintf( buf, "%s opens its bay doors.", ship->name );
   }
   else
   {
      ship->bayopen = FALSE;
      echo_to_cockpit( AT_YELLOW, ship, "Closing bay doors." );
      sprintf( buf, "%s closes its bay doors.", ship->name );
   }
   echo_to_system( AT_YELLOW, ship, buf, NULL );
   return;
}

long prepcost( SHIP_DATA *ship )
{
   long price;

   if( !str_cmp( ship->owner, "Public" ) )
      return (500+(ship->ship_class*250));
   switch( ship->ship_class )
   {
   default:                price = 0;  break;
   case FIGHTER_SHIP:      price = 20; break;
   case MIDSIZE_SHIP:      price = 50; break;
   case FRIGATE_SHIP:      price = 250; break;
   case SHIP_PLATFORM:     price = 750; break;
   case SUPERCAPITAL_SHIP: price = 5000; break;
   case CAPITAL_SHIP:      price = 500; break;
   }
	
   if (ship->missiles )
      price += ( 50 * (ship->maxmissiles-ship->missiles) );
   else if (ship->torpedos )
      price += ( 75 * (ship->maxtorpedos-ship->torpedos) );
   else if (ship->rockets )
      price += ( 150 * (ship->maxrockets-ship->rockets) );
   else if (ship->autocannon )
      price += ( 500 * (ship->autoammomax-ship->autoammo) );
   if (ship->shipstate == SHIP_DISABLED )
      price += 200;
   if ( ship->missilestate == MISSILE_DAMAGED )
      price += 100;
   if ( ship->torpedostate == MISSILE_DAMAGED )
      price += 200;
   if ( ship->rocketstate == MISSILE_DAMAGED )
      price += 300;
   if ( ship->ionstate == LASER_DAMAGED )
      price += 100;
   if ( ship->statet0 == LASER_DAMAGED )
      price += 50;
   if ( ship->statet1 == LASER_DAMAGED )
      price += 50;
   if ( ship->statet2 == LASER_DAMAGED )
      price += 50;
   if( ship->hull < ship->maxhull )
      price += 1000 * (ship->maxhull - ship->hull);
   if( IS_DOCKED( ship ) )
      price *= 10; /* Space resupplies are NOT cheap */
   return price;
}

void prepship( SHIP_DATA *ship )
{
    /* v1.22: replaced "ship->flags = 0" with SWGD's rem_extshipflags().
     * The blanket reset had two fidelity bugs: (1) it wiped
     * SHIPFLAG_SIMULATOR set via togglesimulator (non-module path), so a
     * simulator craft silently became a real ship after endsimulator;
     * (2) it cleared sabotage flags, which SWGD deliberately preserves
     * (sabotage persists until repaired, not until the next prep). */
    rem_extshipflags( ship );
    ship->autoammo = ship->autoammomax;
    /* v1.23: the v1.20 update_ship_modules() call here is gone - under
     * SWGD-verbatim semantics module capability lives in fields
     * (ship->overdrive/cloak) that flag clearing never touches, and
     * rem_extshipflags preserves SHIPFLAG_SIMULATOR, so SWGD's prepship
     * needs no module refresh. */
    ship->energy = ship->maxenergy;
    ship->chaff = ship->maxchaff;
    ship->missiles = ship->maxmissiles;
    ship->torpedos = ship->maxtorpedos;
    ship->rockets = ship->maxrockets;
    if( ship->ship_class < FRIGATE_SHIP ) 
       ship->shield = 0;
    else
       ship->shield = ship->maxshield;
    ship->autorecharge = FALSE;
    ship->autotrack = FALSE;
    ship->autospeed = FALSE;
    ship->interdictactive = FALSE;
    ship->hull = ship->maxhull;
    ship->missilestate = MISSILE_READY;
    ship->statet0 = LASER_READY;
    ship->statet1 = LASER_READY;
    ship->statet2 = LASER_READY;
    ship->ionstate = LASER_READY;
    ship->torpedostate = MISSILE_READY;
    ship->rocketstate = MISSILE_READY;

    if( ship->shipstate2 == SHIP_DOCK_3 )
    {
       ship->shipstate = SHIP_READY;
    }
    else
    {
        ship->shipstate = SHIP_DOCKED;
        ship->shipstate2 = SHIP_READY;  
    }
 
    ship->currjump  = NULL;
    save_ship( ship );
}

void initialize_code( char *buf )
{
    int counter = 0;
    int rand = 0;

    while( counter < 9 )
    {
        rand = number_range(1,10) -1;
        buf[counter++] = '0' + rand;
    }
    buf[counter] = '\0';
    return;
}


void do_break( CHAR_DATA *ch, const char *argument )
{
    char  buf[MAX_STRING_LENGTH];
    int chance;
    float angle_x, angle_y, angle_z;

    SHIP_DATA *ship;
    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )     
    {
       send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch); 
       return;
    }
    if ( ship->ship_class > SHIP_PLATFORM )
    {
        send_to_char("&RThis isn't a spacecraft!\n\r",ch);
        return;
    }
    if (  (ship = ship_from_pilotseat(ch->in_room->vnum))  == NULL )
    {
        send_to_char("&RYour not in the pilots seat.\n\r",ch);
        return;
    }
 
    if ( autofly(ship))
    {
         send_to_char("&RYou'll have to turn off the ships autopilot first.\n\r",ch);
         return;
    }
    if (ship->shipstate == SHIP_DISABLED)
    {
       send_to_char("&RThe ships drive is disabled. Unable to maneuver.\n\r",ch);
       return;
    }
    if  ( ship->ship_class == SHIP_PLATFORM )
    {
       send_to_char( "&RPlatforms can't turn!\n\r" , ch );
       return;
    }

    if (ship->shipstate == SHIP_HYPERSPACE)
    {
       send_to_char("&RYou can only do that in realspace!\n\r",ch);
       return;
    }
    if (ship->shipstate == SHIP_DOCKED)
    {
        send_to_char("&RYou can't do that until after you've launched!\n\r",ch);
        return;
    }
    if (ship->shipstate != SHIP_READY)
    {
         send_to_char("&RPlease wait until the ship has finished its current maneuver.\n\r",ch);
         return;
    }
    if( check_sabotage( ship, SHIPFLAG_SABOTAGEDENGINE ) )
       return;

     if ( ship->energy < (ship->currspeed/10) )
     {
         send_to_char("&RTheres not enough fuel!\n\r",ch);
         return;
     }
     if( ship->ship_class > MIDSIZE_SHIP )
     {
         send_to_char("&RThis ship is a little big to pull that kind of maneuver.\n\r", ch);
         return;
     }

     if( argument[0] == '\0' ) 
     {
	send_to_char("&RSyntax: break <left/right/up/down>\n\r", ch );
	return;
     }
     if( !str_cmp( argument, "left" ) )
     {
	 angle_x = 0;
	 angle_y = 0;
	 angle_z = -90;
     }
     else if( !str_cmp(argument, "right" ) ) 
     {
	angle_x = 0;
	angle_y = 0;
	angle_z = 90;
     }
     else if( !str_cmp(argument, "up" ) )
     {
	angle_x = 0;
	angle_y = 90;
	angle_z = 0;
     }
     else if( !str_cmp(argument, "down" ) )
     {
	angle_x = 0;
	angle_y = -90;
	angle_z = 0;
     }
     else
     {
	send_to_char( "&RSyntax: break <left/right/up/down>\n\r", ch );
	return;
     }
     if ( ship->ship_class == FIGHTER_SHIP )
        chance = IS_NPC(ch) ? ch->top_level
                            : (int)  (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_starfighters] ) ) ;
     if ( ship->ship_class == MIDSIZE_SHIP )
        chance = IS_NPC(ch) ? ch->top_level
                            : (int)  (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_midships] )); 
     if ( number_percent( ) > chance )
     {
         send_to_char("&RYou fail to work the controls properly.\n\r",ch);
         if ( ship->ship_class == FIGHTER_SHIP )
             learn_from_failure( ch, gsn_starfighters );
         if ( ship->ship_class == MIDSIZE_SHIP )
             learn_from_failure( ch, gsn_midships );
         return;
     }
  
     ship->energy -= (ship->currspeed/10);
    /*
     ship->hx *= -1;
     ship->hy *= -1;
     ship->hz *= -1;
     */
     rotate_ship( ship, angle_x, angle_y, angle_z );
     ch_printf( ch, "You begin to execute the break maneuver.\n\r");
     act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument , TO_ROOM );
     echo_to_cockpit( AT_YELLOW ,ship, "The ship enters a break maneuver.\n\r" );
     if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED))
     {
        sprintf( buf, "%s enters a break maneuver." , ship->name );
        echo_to_system( AT_ORANGE , ship , buf , NULL );
     }
     if ( ship->ship_class == FIGHTER_SHIP || ( ship->ship_class == MIDSIZE_SHIP && ship->manuever > 50 ) )
        ship->shipstate = SHIP_BUSY_3;
     else if ( ship->ship_class == MIDSIZE_SHIP || ( ship->ship_class == CAPITAL_SHIP && ship->manuever > 50 ) )
        ship->shipstate = SHIP_BUSY_2;
     else
        ship->shipstate = SHIP_BUSY;
     if ( ship->ship_class == FIGHTER_SHIP )
        learn_from_success( ch, gsn_starfighters );
     if ( ship->ship_class == MIDSIZE_SHIP )
        learn_from_success( ch, gsn_midships );
     return;
}

void do_chandelle( CHAR_DATA *ch, const char *argument )
{
    char  buf[MAX_STRING_LENGTH];
    int chance;

    SHIP_DATA *ship;
    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )     
    {
       send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch); 
       return;
    }
    if ( ship->ship_class > SHIP_PLATFORM )
    {
        send_to_char("&RThis isn't a spacecraft!\n\r",ch);
        return;
    }
    if (  (ship = ship_from_pilotseat(ch->in_room->vnum))  == NULL )
    {
        send_to_char("&RYour not in the pilots seat.\n\r",ch);
        return;
    }
 
    if ( autofly(ship))
    {
         send_to_char("&RYou'll have to turn off the ships autopilot first.\n\r",ch);
         return;
    }
    if (ship->shipstate == SHIP_DISABLED)
    {
       send_to_char("&RThe ships drive is disabled. Unable to maneuver.\n\r",ch);
       return;
    }
    if  ( ship->ship_class == SHIP_PLATFORM )
    {
       send_to_char( "&RPlatforms can't turn!\n\r" , ch );
       return;
    }

    if (ship->shipstate == SHIP_HYPERSPACE)
    {
       send_to_char("&RYou can only do that in realspace!\n\r",ch);
       return;
    }
    if (ship->shipstate == SHIP_DOCKED)
    {
        send_to_char("&RYou can't do that until after you've launched!\n\r",ch);
        return;
    }
    if (ship->shipstate != SHIP_READY)
    {
         send_to_char("&RPlease wait until the ship has finished its current maneuver.\n\r",ch);
         return;
    }
    if( check_sabotage( ship, SHIPFLAG_SABOTAGEDENGINE ) )
       return;

     if ( ship->energy < (ship->currspeed/10) )
     {
         send_to_char("&RTheres not enough fuel!\n\r",ch);
         return;
     }
     if( ship->ship_class > MIDSIZE_SHIP )
     {
         send_to_char("&RThis ship is a little big to pull that kind of maneuver.\n\r", ch);
         return;
     }
     if ( ship->ship_class == FIGHTER_SHIP )
         chance = IS_NPC(ch) ? ch->top_level
                : (int)  (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_starfighters] ) );
     if ( ship->ship_class == MIDSIZE_SHIP )
         chance = IS_NPC(ch) ? ch->top_level
                : (int)  (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_midships] ) );
     if ( number_percent( ) > chance )
     {
         send_to_char("&RYou fail to work the controls properly.\n\r",ch);
         if ( ship->ship_class == FIGHTER_SHIP )
             learn_from_failure( ch, gsn_starfighters );
         if ( ship->ship_class == MIDSIZE_SHIP )
             learn_from_failure( ch, gsn_midships );
         return;
     }
  
     ship->energy -= (ship->currspeed/10);
     ship->hx *= -1;
     ship->hy *= -1;
     ship->hz *= -1;
     ch_printf( ch, "You begin to execute the chandelle maneuver.\n\r");
     act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument , TO_ROOM );
     echo_to_cockpit( AT_YELLOW ,ship, "The ship enters a chandelle maneuver.\n\r" );
     if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED))
     {
        sprintf( buf, "%s enters a chandelle maneuver." , ship->name );
        echo_to_system( AT_ORANGE , ship , buf , NULL );
     }
     if ( ship->ship_class == FIGHTER_SHIP || ( ship->ship_class == MIDSIZE_SHIP && ship->manuever > 50 ) )
        ship->shipstate = SHIP_BUSY_3;
     else if ( ship->ship_class == MIDSIZE_SHIP || ( ship->ship_class == CAPITAL_SHIP && ship->manuever > 50 ) )
        ship->shipstate = SHIP_BUSY_2;
     else
        ship->shipstate = SHIP_BUSY;
     if ( ship->ship_class == FIGHTER_SHIP )
        learn_from_success( ch, gsn_starfighters );
     if ( ship->ship_class == MIDSIZE_SHIP )
        learn_from_success( ch, gsn_midships );
     return;
}

void do_cloak( CHAR_DATA *ch, const char *argument )
{
    int chance;
    SHIP_DATA *ship;
    char buf[MAX_STRING_LENGTH];

    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
        return;
    }

    if ( ship->ship_class > SHIP_PLATFORM )
    {
       send_to_char("&RThis isn't a spacecraft!\n\r",ch);
       return;
    }
    if (  (ship = ship_from_pilotseat(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RThe controls must be at the pilots chair...\n\r",ch);
       return;
    }
    if ( autofly(ship) )
    {
       send_to_char("&RYou'll have to turn off the ships autopilot first.\n\r",ch);
       return;
    }
    if (ship->shipstate == SHIP_HYPERSPACE)
    {
       send_to_char("&RYou can only do that in realspace!\n\r",ch);
       return;
    }
    if (ship->shipstate == SHIP_DISABLED)
    {
       send_to_char("&RThe ships drive is disabled. Not enough power to engage cloak.\n\r",ch);
       return;
    }
    if (ship->shipstate == SHIP_DOCKED)
    {
        send_to_char("&RYou can't do that until after you've launched!\n\r",ch);
        return;
    }

    if ((ship->shipstate2 == SHIP_DOCK) || (ship->shipstate2 == SHIP_DOCK_2))
    {
       send_to_char("&RNot while docking procedures are going on.\n\r", ch);
       return;
    }
    if (ship->shipstate2 == SHIP_DOCK_3)
    {
      send_to_char("&RDetach from the docked ship first.\n\r",ch);
      return;
    }
    if (ship->cloak == 0)
    {
      send_to_char("&RTry installing a cloaking device first.\n\r", ch);
      return;
    }
    if(ship->energy < 200)
    {
      send_to_char("&RYou don't have enough fuel.", ch);
      return;
    }
    if(!str_cmp(argument, "off"))
    {
      if (IS_SET(ship->flags, SHIPFLAG_CLOAKED))
      {
        send_to_char("&WYou shut off your cloaking device, and your ship appears.\n\r", ch);
        REMOVE_BIT(ship->flags, SHIPFLAG_CLOAKED);
        act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument , TO_ROOM );
        sprintf( buf, "%s appears out of nowhere." , ship->name );
        echo_to_system( AT_ORANGE , ship , buf , NULL );
        return;
      }
      else
      {
         send_to_char("&WYour cloaking device isn't active.\n\r", ch);
         return;
      }
    }
      if(IS_SET(ship->flags, SHIPFLAG_CLOAKED))
      {
          send_to_char("&WYour ship is already cloaked. Type cloak off to turn it off.\n\r", ch);
          return;
      }
      chance = IS_NPC(ch) ? ch->top_level : (int) (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_cloak] ) );
      if ( number_percent( ) >= chance )
      {
          send_to_char("&RYou fail to work the controls properly.\n\r",ch);
          learn_from_failure(ch, gsn_cloak);
          return;
      }
      learn_from_success(ch, gsn_cloak);
      SET_BIT(ship->flags, SHIPFLAG_CLOAKED);
      send_to_char("Cloaking generator activated.\n\r", ch);
      act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument , TO_ROOM );
      sprintf( buf, "%s disappears into nowhere." , ship->name );
      echo_to_system( AT_ORANGE , ship , buf , NULL );
      ship->energy -= 200; /* fuel used to activate it. */
      return;
}

void do_deleteship( CHAR_DATA *ch, const char *argument )
{    
    SHIP_DATA *ship;
    char shiplog[MAX_STRING_LENGTH];
    char buf[MAX_STRING_LENGTH];

     
     ship = get_ship( argument );
     if (ship == NULL)
     {
        send_to_char("&RNo such ship!",ch);
        return;
     } 
     
        resetship(ship);
        sprintf(shiplog,"Ship Deleted: %s",ship->name);
        log_string(shiplog);
        extract_ship( ship );
        ship_to_room( ship , 5 ); 
   	ship->location = 5;
	ship->shipyard = 5;
        ship->lastdoc  = 5;
        if (ship->starsystem)
            ship_from_starsystem( ship, ship->starsystem );
            
         sprintf( buf, "%s%s", SHIP_DIR, ship->filename );
         remove(buf);
             
        UNLINK( ship, first_ship, last_ship, next, prev );
	  DISPOSE(ship);

        write_ship_list();
          
}

void do_detach( CHAR_DATA *ch, const char *argument)
{
    SHIP_DATA *ship;
    SHIP_DATA *docked_by;
    char buf[MAX_STRING_LENGTH];
    
    if ( (ship = ship_from_pilotseat(ch->in_room->vnum)) == NULL )
    {
       send_to_char("&RYou don't seem to be in the pilot seat!\n\r",ch);
       return;
    }

    if (!IS_DOCKED(ship))
    {
       send_to_char("&RThere doesn't seem to be any ships docked to you.\r\n", ch);
       return;
    }
    docked_by = ship->docked_ship;
    
    ship->docked_ship = NULL;
/*
    ship->shipstate = SHIP_READY;
 */
    ship->shipstate2 = SHIP_READY;
    docked_by->docked_ship = NULL;
/*
    docked_by->shipstate  = SHIP_READY;
 */
    docked_by->shipstate2 = SHIP_READY;
    
    sprintf(buf, "You hear air escaping as the airlocks open, and %s detaches.", docked_by->name);
    echo_to_ship( AT_YELLOW, ship, buf);
    sprintf(buf, "You hear air escaping as the airlocks open, and %s detaches.", ship->name);
    echo_to_ship( AT_YELLOW, docked_by, buf);
    save_ship(ship);
}

void do_docking( CHAR_DATA *ch, const char *argument )
{
    char buf[MAX_STRING_LENGTH];
    char arg[MAX_INPUT_LENGTH];
    int chance;
    SHIP_DATA *ship;
    SHIP_DATA *docktarget;
    
    strcpy( arg, argument );
    
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
    	            send_to_char("&RYou need to be in the pilot seat!\n\r",ch);
    	            return;
    	        }
    	        
    	        if ( autofly(ship) )
    	        {
    	            send_to_char("&RYou'll have to turn off the ship's autopilot first.\n\r",ch);
    	            return;
    	        }
    	        
                if  ( ship->ship_class == SHIP_PLATFORM )
                {
                   send_to_char( "&RYou can't dock platforms!\n\r" , ch );
                   return;
                }   
    
    	        if (ship->shipstate == SHIP_DISABLED)
    	        {
    	            send_to_char("&RThe ship's drive is disabled. Unable to dock.\n\r",ch);
    	            return;
    	        }
                if( IS_SET(ship->flags, SHIPFLAG_CLOAKED))
                {
                   send_to_char("&RThey can't see you to perform in the docking operation.\n\r", ch); 
                   return;
                }
    	        if (IS_DOCKED(ship))
    	        {
    	            send_to_char("&RThe ship is already docked!\n\r",ch);
    	            return;
    	        }
    	        if (ship->shipstate2 == SHIP_DOCK || ship->shipstate2 == SHIP_DOCK_2)
    	        {
    	            send_to_char("&RNot while docking procedures are going on.\r\n",ch);
    	            return;
    	        }
    	                
               if (ship->shipstate == SHIP_HYPERSPACE)
               {
                  send_to_char("&RYou can only do that in realspace!\n\r",ch);
                  return;   
               }

    	        if (ship->shipstate != SHIP_READY)
    	        {
    	            send_to_char("&RPlease wait until the ship has finished its current manuever.\n\r",ch);
    	            return;
    	        }
    	        if ( ship->starsystem == NULL )
    	        {
    	            send_to_char("&Dock with whom?",ch);
    	            return;
    	        }
    	        
    	        if ( ship->energy < (25 + 25*ship->ship_class) )
    	        {
    	           send_to_char("&RThere's not enough fuel!\n\r",ch);
    	           return;
    	        }
    	        
    	        if ( argument[0] == '\0' )
    	        {  
    	           set_char_color(  AT_CYAN, ch );
    	           ch_printf(ch, "Dock with who?\n\r");   	           
    		     return;
    	        }
    	        
    	        docktarget = get_ship_here( arg, ship->starsystem );
                if (  docktarget == NULL || IS_SET(docktarget->flags, SHIPFLAG_CLOAKED))
                {
                    send_to_char("&RThat ship isn't here!\n\r",ch);
                    return;
                }

		if (IS_DOCKED(docktarget))
		{
		   send_to_char("&RThat ship is already docked!\n\r",ch);
		   return;
		}
		if (docktarget->shipstate2 == SHIP_DOCK || docktarget->shipstate2 == SHIP_DOCK_2)
		{
		   send_to_char("&RThat ship is already busy docking with something else.\r\n",ch);
		   return;
		}
		
    	            if ( docktarget == ship )
    	            {
    	                send_to_char("&RYou can't dock your ship inside itself!\n\r",ch);
    	                return;
    	            } 
                   if(docktarget->ship_class > MIDSIZE_SHIP && ship->ship_class <= MIDSIZE_SHIP)
                   {
                        send_to_char("&RThat ship is too large to dock with. Try landing.\n\r", ch);
                        return;
                   }
                   if(ship->ship_class > MIDSIZE_SHIP && docktarget->ship_class <= MIDSIZE_SHIP)
                   {
                        send_to_char("&RYour ship is too large to dock with that. Tractor it.\n\r", ch);
                        return;
                   }                    
                   if (  (docktarget->vx > ship->vx + 200) || (docktarget->vx < ship->vx - 200) ||
    	                  (docktarget->vy > ship->vy + 200) || (docktarget->vy < ship->vy - 200) ||
    	                  (docktarget->vz > ship->vz + 200) || (docktarget->vz < ship->vz - 200) )
    	            {
    	                send_to_char("&R That ship is too far away! You'll have to fly a little closer.\n\r",ch);
    	                return;
    	            }       
                chance = IS_NPC(ch) ? ch->top_level
	                 : (int)  (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_docking] ));
                if ( number_percent( ) < chance )
    		{
    		   set_char_color( AT_GREEN, ch );
		   sprintf( buf, "You are being docked by %s.\n\r"
		   "Docking sequence initiated.\n\r", ship->name);
    		   echo_to_ship( AT_YELLOW, docktarget, buf );
    		   send_to_char( "Docking sequence initiated.\n\r", ch);
    		   act( AT_PLAIN, "$n begins the docking sequence.", ch,
		        NULL, argument , TO_ROOM );
		   echo_to_ship( AT_YELLOW , ship , "The ship slowly begins its docking approach.");
                   if( autofly( docktarget ) && docktarget->ship_class != SHIP_PLATFORM && !docktarget->target0 && docktarget->shipstate != SHIP_DISABLED 
                   && !str_cmp( ship->owner, docktarget->owner ) )
                   {
                       sprintf( buf, "%s has targetted your vessel, abandoning docking attempt.", docktarget->name );
                       echo_to_ship( AT_RED, ship, buf );
                       docktarget->target0 = ship;
                   }
                   else
                   {
      		      ship->shipstate2 = SHIP_DOCK;
    		      ship->currspeed = 0;
	              docktarget->shipstate2 = SHIP_DOCK;
	              docktarget->currspeed = 0;
	              ship->docked_ship = docktarget;
	              docktarget->docked_ship = ship;
                   }	
	           learn_from_success( ch, gsn_docking );
                   return;
	        }
	        send_to_char("You fail to work the controls properly.\n\r",ch);
	        learn_from_failure( ch, gsn_docking );
    	   	return;	
}

void do_freeships( CHAR_DATA *ch, const char *argument )
{
    SHIP_DATA *ship;
    int count = 0;

      count = 0;
      send_to_char( "&Y\n\rThe following ships are currently availiable for purchace:\n\r", ch );
    
      send_to_char( "\n\r&WShip                                                  Class          Price\n\r", ch );
      
      for ( ship = first_ship; ship; ship = ship->next )
      {   
        if ( ship->ship_class > SHIP_PLATFORM ) 
           continue; 
	if (ship->location == 5)
	   continue;      
	if (ship->location == 45)
	   continue;      
	if (ship->location == 133)
	   continue;      

        if (ship->type == MOB_SHIP)
           continue;
        else if (ship->type == SHIP_REPUBLIC)
           set_char_color( AT_BLOOD, ch );
        else if (ship->type == SHIP_IMPERIAL)
           set_char_color( AT_DGREEN, ch );
        else
          set_char_color( AT_BLUE, ch );
        
        if ( !str_cmp(ship->owner, "") )
        {
           const char *sclass = "Unknown";
           if( ship->ship_class == FIGHTER_SHIP )     sclass = "Fighter";
           else if( ship->ship_class == MIDSIZE_SHIP ) sclass = "Midsize";
           else if( ship->ship_class == FRIGATE_SHIP ) sclass = "Frigate";
           else if( ship->ship_class == CAPITAL_SHIP ) sclass = "Capital";
           else if( ship->ship_class == SUPERCAPITAL_SHIP ) sclass = "Supercapital";
           else if( ship->ship_class == SHIP_PLATFORM ) sclass = "Platform";
           ch_printf( ch, "%-53s %-15s %ld\n\r", ship->name, sclass, get_ship_value(ship));
        }
        count++;
      }
    
      if ( count==0 )
      {
        send_to_char( "There are no ships currently free.\n\r", ch );
	return;
      }
    
}

void do_interdict(CHAR_DATA *ch, const char *argument )
{
    SHIP_DATA *ship;
    SHIP_DATA *target;
    char buf[MAX_STRING_LENGTH];
        
        if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
        {
            send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
            return;
        }
        
        if (  (ship = ship_from_pilotseat(ch->in_room->vnum))  == NULL )
        {
            send_to_char("&RYou must be in the pilots seat!\n\r",ch);
            return;
        }

        if (  ship->interdict < 1 )
        {
            send_to_char("&RNo Interdiction Field Generators Installed.\n\r",ch);
            return;
        }
        
        if ( ! check_pilot(ch,ship) )
       	     {
       	       send_to_char("&RHey! Thats not your ship!\n\r",ch);
       	       return;
       	     }
  
        //Originally had code to target specific ship for interdiction
        //realized bad rp, removed left target pointer incase i 
        //change mid, duping the ship and target so the echotosystem doesn't fail.

        target = ship;
         
        act( AT_PLAIN, "$n flips a switch on the control panel.", ch, NULL, argument , TO_ROOM );

        if (ship->interdictactive == TRUE)
        {
           ship->interdictactive = FALSE;
           send_to_char( "&GYou toggle the interdiction field.\n\r", ch);
           sprintf( buf, "%s has deactivated an interdiction field!" , ship->name);
           echo_to_system( AT_ORANGE , ship , buf , target );
           echo_to_cockpit( AT_YELLOW , ship , "Interdiction Field Deactivated!");
        }
        else
        {
           ship->interdictactive = TRUE;
           ship->autorecharge = FALSE;
           send_to_char( "&GYou toggle the interdiction Field.\n\r", ch);
           send_to_char( "&GAutoRecharge NOT availiable while field active.\n\r", ch);
           sprintf( buf, "%s has activated an interdiction field!" , ship->name);
           echo_to_system( AT_ORANGE , ship , buf , target );
           echo_to_cockpit( AT_YELLOW , ship , "Interdiction Field Active!");
        }   
}

/*
 * Returns TRUE if the ship already has an installed module that provides
 * the same core system as the one being installed (engine, generator,
 * tractorbeam, overdrive, laser battery, simulator, or cloak) - ships can
 * only carry one of each of these at a time.
 */
bool module_type_install( OBJ_DATA * obj, SHIP_DATA * ship )
{
   int module;
   AFFECT_DATA *pafship;
   AFFECT_DATA *pafobj;

   for( module = 1; module <= ship->modules; module++ )
   {
      OBJ_INDEX_DATA *installed = get_obj_index( ship->module_vnum[module] );

      if( !installed )
         continue;

      for( pafship = installed->first_affect; pafship; pafship = pafship->next )
      {
         for( pafobj = obj->pIndexData->first_affect; pafobj; pafobj = pafobj->next )
         {
            if( pafship->location == pafobj->location &&
                ( pafobj->location == APPLY_HASSEMBLY || pafobj->location == APPLY_ENGINE
                  || pafobj->location == APPLY_GENERATOR || pafobj->location == APPLY_TRACTORBEAM
                  || pafobj->location == APPLY_OVERDRIVE || pafobj->location == APPLY_LASERBATTERY
                  || pafobj->location == APPLY_SIMULATOR || pafobj->location == APPLY_CLOAK ) )
               return TRUE;
         }
      }
   }
   return FALSE;
}

/*
 * Recomputes every module-driven ship stat from scratch, based on the
 * full set of currently-installed modules. Safe to call after any
 * install or remove - resets first so nothing double-counts.
 */
/* v1.23: apply_module_stat_bonus() (the v1.19 additive-bonus layer) has
 * been removed. Module stats now follow SWGD verbatim: update_ship_modules()
 * is reset-and-rebuild, so a modularized ship's flight stats come entirely
 * from its installed component modules; ships with zero modules never
 * trigger the rebuild and keep their setship-configured stats. */


void update_ship_modules( SHIP_DATA * ship )
{
   AFFECT_DATA *paf;
   int module;
   OBJ_INDEX_DATA *obj;

   ship->hyperspeed = 0;
   ship->realspeed = 0;
   ship->maxshield = 0;
   ship->lasers = 0;
   ship->tractorbeam = 0;
   ship->maxmissiles = 0;
   ship->maxrockets = 0;
   ship->maxtorpedos = 0;
   ship->maxenergy = 0;
   ship->comm = 0;
   ship->sensor = 0;
   ship->astro_array = 0;
   ship->chaff = 0;
   ship->maxchaff = 0;
   ship->manuever = 0;
   ship->laserdamage = 0;
   ship->ions = 0;
   ship->overdrive = 0;
   ship->armor = 0;
   ship->maxcargo = 0;
   ship->mlaunchers = 0;
   ship->tlaunchers = 0;
   ship->rlaunchers = 0;
   ship->cloak = 0;

   if( IS_SET( ship->flags, SHIPFLAG_SIMULATOR ) )
      REMOVE_BIT( ship->flags, SHIPFLAG_SIMULATOR );

   for( module = 1; module <= ship->modules; module++ )
   {
      obj = get_obj_index( ship->module_vnum[module] );
      if( !obj )
         continue;
      for( paf = obj->first_affect; paf; paf = paf->next )
      {
         if( paf->location == APPLY_HYPERSPEED )
            ship->hyperspeed += paf->modifier;
         if( paf->location == APPLY_REALSPEED )
            ship->realspeed += paf->modifier;
         if( paf->location == APPLY_MAXSHIELD )
            ship->maxshield += paf->modifier;
         if( paf->location == APPLY_LASERS )
            ship->lasers += paf->modifier;
         if( paf->location == APPLY_TRACTORBEAM )
            ship->tractorbeam += paf->modifier;
         if( paf->location == APPLY_MAXMISSILES )
            ship->maxmissiles += paf->modifier;
         if( paf->location == APPLY_MAXROCKETS )
            ship->maxrockets += paf->modifier;
         if( paf->location == APPLY_MAXTORPEDOS )
            ship->maxtorpedos += paf->modifier;
         if( paf->location == APPLY_MAXENERGY )
            ship->maxenergy += paf->modifier;
         if( paf->location == APPLY_COMM )
         {
            if( paf->modifier > ship->comm )
               ship->comm = paf->modifier;
         }
         if( paf->location == APPLY_SENSOR )
         {
            if( paf->modifier > ship->sensor )
               ship->sensor = paf->modifier;
         }
         if( paf->location == APPLY_ASTRO_ARRAY )
         {
            if( paf->modifier > ship->astro_array )
               ship->astro_array = paf->modifier;
         }
         if( paf->location == APPLY_CHAFF )
            ship->maxchaff += paf->modifier;
         if( paf->location == APPLY_MANUEVER )
            ship->manuever += paf->modifier;
         if( paf->location == APPLY_LASERDAMAGE )
            ship->laserdamage += paf->modifier;
         if( paf->location == APPLY_IONS )
            ship->ions += paf->modifier;
         if( paf->location == APPLY_OVERDRIVE )
            ship->overdrive = 1;
         if( paf->location == APPLY_CLOAK )
            /* v1.29: module_type_install already gated cloak modules as
             * one-per-ship, but update_ship_modules never actually applied
             * the effect - a purchased/installed cloak module silently did
             * nothing. ship->cloak is otherwise an imm-only setship toggle;
             * this lets the module grant it the same way overdrive does. */
            ship->cloak = 1;
         if( paf->location == APPLY_ARMOR )
            ship->armor += paf->modifier;
         if( paf->location == APPLY_CARGO )
            ship->maxcargo += paf->modifier;
         if( paf->location == APPLY_MLAUNCHER )
            ship->mlaunchers += paf->modifier;
         if( paf->location == APPLY_TLAUNCHER )
            ship->tlaunchers += paf->modifier;
         if( paf->location == APPLY_RLAUNCHER )
            ship->rlaunchers += paf->modifier;
         if( paf->location == APPLY_SIMULATOR )
            SET_BIT( ship->flags, SHIPFLAG_SIMULATOR );
      }
   }
   if( ship->lasers > 0 && ship->laserdamage == 0 )
      ship->laserdamage = 1;
   if( ship->mlaunchers > 2 )
      ship->mlaunchers = 2;
   if( ship->tlaunchers > 2 )
      ship->tlaunchers = 2;
   if( ship->rlaunchers > 2 )
      ship->rlaunchers = 2;
}

/*
 * Admin utility: backfills/resets maxmodules on every currently loaded
 * ship to the standard class-based default, for ships saved before the
 * module system existed (or to simply reset everyone back to default).
 */
/*
 * Admin utility: instantly relocates a ship from anywhere in the game to
 * the room the immortal is currently standing in - but only if that room
 * is actually flagged as a valid landing dock.
 */
void do_transfership( CHAR_DATA *ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   SHIP_DATA *ship;

   one_argument( argument, arg );

   if( arg[0] == '\0' )
   {
      send_to_char( "Transfer which ship?\r\n", ch );
      return;
   }

   if( !ch->in_room )
   {
      send_to_char( "You must be at a dock to do that.\r\n", ch );
      return;
   }

   {
      SPACE_DATA *starsystem;
      bool at_dock = FALSE;
      int vnum = ch->in_room->vnum;

      for( starsystem = first_starsystem; starsystem; starsystem = starsystem->next )
      {
         if( vnum != 0 &&
             ( vnum == starsystem->doc1a || vnum == starsystem->doc2a || vnum == starsystem->doc3a
               || vnum == starsystem->doc1b || vnum == starsystem->doc2b || vnum == starsystem->doc3b
               || vnum == starsystem->doc1c || vnum == starsystem->doc2c || vnum == starsystem->doc3c ) )
         {
            at_dock = TRUE;
            break;
         }
      }

      if( !at_dock )
      {
         send_to_char( "You must be at a dock to do that.\r\n", ch );
         return;
      }
   }

   if( ( ship = get_ship( arg ) ) == NULL )
   {
      send_to_char( "No such ship exists.\r\n", ch );
      return;
   }

   if( ship->in_room == ch->in_room )
   {
      ch_printf( ch, "%s is already docked here.\r\n", ship->name );
      return;
   }

   if( ship->in_room )
      UNLINK( ship, ship->in_room->first_ship, ship->in_room->last_ship, next_in_room, prev_in_room );

   LINK( ship, ch->in_room->first_ship, ch->in_room->last_ship, next_in_room, prev_in_room );
   ship->in_room = ch->in_room;
   ship->location = ch->in_room->vnum;
   ship->lastdoc = ch->in_room->vnum;

   save_ship( ship );

   ch_printf( ch, "You transfer %s to this dock.\r\n", ship->name );
   act( AT_IMMORT, "$n waves $s hand, and a ship materializes at the dock.", ch, NULL, NULL, TO_ROOM );
   log_string_plus( "transfership used.", LOG_NORMAL, get_trust( ch ) );
}

int default_maxmodules( int ship_class )
{
   /* Local per-class slot defaults (Mark, v1.28). SWGD itself has no
    * class-based defaults - every ship is set individually via
    * setship maxmodules - but a sane starting point saves admin time
    * when bulk-backfilling a fleet. */
   switch ( ship_class )
   {
      case FIGHTER_SHIP:      return 15;
      case MIDSIZE_SHIP:      return 30;
      case FRIGATE_SHIP:      return 50;
      case CAPITAL_SHIP:      return 75;
      case SUPERCAPITAL_SHIP: return 99;
      default:                return 10;   /* SWGD floor, for classes
                                             * with no defined default
                                             * (platform, walker, etc) */
   }
}

void do_resetmaxshipmodules( CHAR_DATA *ch, const char *argument )
{
   SHIP_DATA *ship;
   char arg[MAX_INPUT_LENGTH];
   int count = 0;
   int explicit_max = -1;   /* -1 = use per-class defaults */
   bool all = FALSE;

   argument = one_argument( argument, arg );

   if( !str_cmp( arg, "all" ) )
   {
      all = TRUE;
      argument = one_argument( argument, arg );
   }
   if( arg[0] != '\0' && is_number( arg ) )
      explicit_max = URANGE( 10, atoi( arg ), MAX_SHIP_MODULES );

   for( ship = first_ship; ship; ship = ship->next )
   {
      int newmax;

      if( !all && ship->maxmodules != 0 )
         continue;

      newmax = explicit_max >= 0 ? explicit_max : default_maxmodules( ship->ship_class );

      if( newmax < ship->modules )
         continue;   /* would strand installed modules - skip */

      ship->maxmodules = newmax;
      save_ship( ship );
      count++;
   }

   if( explicit_max >= 0 )
      ch_printf( ch, "Set maxmodules to %d on %d ship%s%s.\r\n", explicit_max, count,
                 count == 1 ? "" : "s", all ? " (all ships)" : " (unset ships only)" );
   else
      ch_printf( ch, "Set maxmodules to class defaults (fighter 15, midsize 30, "
                 "frigate 50, capital 75, supercapital 99) on %d ship%s%s.\r\n",
                 count, count == 1 ? "" : "s", all ? " (all ships)" : " (unset ships only)" );
   ch_printf( ch, "Usage: resetmaxshipmodules [all] [value 10-%d]\r\n", MAX_SHIP_MODULES );
   log_string_plus( "resetmaxshipmodules used.", LOG_NORMAL, get_trust( ch ) );
}

void do_install_module( CHAR_DATA *ch, const char *argument )
{
   char arg1[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   SHIP_DATA *ship;

   argument = one_argument( argument, arg1 );
   argument = one_argument( argument, arg2 );

   if( arg1[0] == '\0' )
   {
      send_to_char( "Install what?\r\n", ch );
      return;
   }

   /* v1.33: allow fitting a module from inside the ship's own engine
    * room, not just standing beside the docked hull - frigates, capitals,
    * and supercapitals can never dock again once launched (they're too
    * big to land at all), so the original docked-only path meant they
    * could only ever be fitted out once, before their first launch. No
    * ship name needed here since standing in a specific ship's engine
    * room is already unambiguous. */
   if( ( ship = ship_from_engine( ch->in_room->vnum ) ) == NULL )
   {
      if( arg2[0] == '\0' )
      {
         send_to_char( "In what?\r\n", ch );
         return;
      }

      ship = ship_in_room( ch->in_room, arg2 );
      if( !ship )
      {
         act( AT_PLAIN, "I see no $T here.", ch, NULL, arg2, TO_CHAR );
         return;
      }
   }

   if( !check_pilot( ch, ship ) || !str_cmp( ship->owner, "Public" ) )
   {
      send_to_char( "&RHey, that's not your ship!\r\n", ch );
      return;
   }

   if( ship->maxmodules == ship->modules )
   {
      send_to_char( "The ship has no free module slots!\r\n", ch );
      return;
   }

   if( ms_find_obj( ch ) )
      return;

   if( ( obj = get_obj_carry( ch, arg1 ) ) == NULL )
   {
      send_to_char( "You do not have that item.\r\n", ch );
      return;
   }

   if( !( obj->item_type == ITEM_FIGHTERCOMP || obj->item_type == ITEM_MIDCOMP
          || obj->item_type == ITEM_CAPITALCOMP || obj->item_type == ITEM_FRIGATECOMP
          || obj->item_type == ITEM_SCAPITALCOMP ) )
   {
      send_to_char( "That isn't a ship module.\r\n", ch );
      return;
   }

   if( obj->item_type == ITEM_FIGHTERCOMP && ship->ship_class != FIGHTER_SHIP )
   {
      send_to_char( "That module is designed for a fighter class vessel.\r\n", ch );
      return;
   }

   if( obj->item_type == ITEM_MIDCOMP && ship->ship_class != MIDSIZE_SHIP )
   {
      send_to_char( "That module is designed for a midship class vessel.\r\n", ch );
      return;
   }

   if( obj->item_type == ITEM_FRIGATECOMP && ship->ship_class != FRIGATE_SHIP )
   {
      send_to_char( "That module is designed for a frigate class vessel.\r\n", ch );
      return;
   }

   if( obj->item_type == ITEM_CAPITALCOMP && ship->ship_class != CAPITAL_SHIP )
   {
      send_to_char( "That module is designed for a capital class vessel.\r\n", ch );
      return;
   }
   if( obj->item_type == ITEM_SCAPITALCOMP && ship->ship_class != SUPERCAPITAL_SHIP )
   {
      send_to_char( "That module is designed for a super class vessel.\r\n", ch );
      return;
   }

   if( !can_drop_obj( ch, obj ) )
   {
      send_to_char( "You can't let go of it.\r\n", ch );
      return;
   }

   if( module_type_install( obj, ship ) )
   {
      send_to_char( "Your ship cannot carry another one of those!\r\n", ch );
      return;
   }

   separate_obj( obj );

   act( AT_ACTION, "$n installs $p.", ch, obj, NULL, TO_ROOM );
   act( AT_ACTION, "You install $p.", ch, obj, NULL, TO_CHAR );

   ship->modules += 1;
   ship->module_vnum[ship->modules] = obj->pIndexData->vnum;
   extract_obj( obj );
   save_ship( ship );
   update_ship_modules( ship );

   return;
}

void do_remove_module( CHAR_DATA *ch, const char *argument )
{
   char arg1[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   OBJ_DATA *obj;
   OBJ_INDEX_DATA *obj_index;
   SHIP_DATA *ship;
   int module, rmodule;

   argument = one_argument( argument, arg1 );
   argument = one_argument( argument, arg2 );

   if( arg1[0] == '\0' )
   {
      send_to_char( "Remove what?\r\n", ch );
      return;
   }

   /* v1.33: same engine-room allowance as do_install_module above. */
   if( ( ship = ship_from_engine( ch->in_room->vnum ) ) == NULL )
   {
      if( arg2[0] == '\0' )
      {
         send_to_char( "From what?\r\n", ch );
         return;
      }

      ship = ship_in_room( ch->in_room, arg2 );
      if( !ship )
      {
         act( AT_PLAIN, "I see no $T here.", ch, NULL, arg2, TO_CHAR );
         return;
      }
   }

   if( !check_pilot( ch, ship ) || !str_cmp( ship->owner, "Public" ) )
   {
      send_to_char( "&RHey, that's not your ship!\r\n", ch );
      return;
   }

   for( module = 1; module <= ship->modules; module++ )
   {
      obj_index = get_obj_index( ship->module_vnum[module] );
      if( !obj_index )
      {
         bug( "%s: cannot find module vnum %d.", __func__, ship->module_vnum[module] );
         send_to_char( "No such module installed.\r\n", ch );
         return;
      }

      if( nifty_is_name( arg1, obj_index->name ) )
      {
         obj = create_object( obj_index, 100 );

         if( obj->weight > can_carry_w( ch ) )
         {
            send_to_char( "You can't possibly remove that. It's much too heavy.\r\n", ch );
            extract_obj( obj );
            return;
         }

         act( AT_ACTION, "$n removes $p.", ch, obj, NULL, TO_ROOM );
         act( AT_ACTION, "You remove $p.", ch, obj, NULL, TO_CHAR );
         obj = obj_to_char( obj, ch );

         for( rmodule = module; rmodule < ship->modules; rmodule++ )
            ship->module_vnum[rmodule] = ship->module_vnum[rmodule + 1];

         ship->modules -= 1;
         update_ship_modules( ship );
         save_ship( ship );
         return;
      }
   }

   send_to_char( "No such module installed.\r\n", ch );
   return;
}

void do_show_modules( CHAR_DATA *ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   SHIP_DATA *ship;
   int module;

   one_argument( argument, arg );

   /* v1.33: same engine-room allowance as install_module/remove_module -
    * lets you check your own loadout while flying, not just docked. */
   if( ( ship = ship_from_engine( ch->in_room->vnum ) ) == NULL )
   {
      if( arg[0] == '\0' )
      {
         send_to_char( "Check what ship?\r\n", ch );
         return;
      }

      ship = ship_in_room( ch->in_room, arg );
      if( !ship )
      {
         act( AT_PLAIN, "I see no $T here.", ch, NULL, arg, TO_CHAR );
         return;
      }
   }

   ch_printf( ch, "Modules installed on %s:\r\n\r\n", ship->name );

   if( ship->modules == 0 )
   {
      send_to_char( "No modules installed.\r\n", ch );
      return;
   }

   for( module = 1; module <= ship->modules; module++ )
   {
      OBJ_INDEX_DATA *obj_index = get_obj_index( ship->module_vnum[module] );

      if( !obj_index )
      {
         bug( "%s: cannot find module vnum %d.", __func__, ship->module_vnum[module] );
         continue;
      }
      ch_printf( ch, "%s\r\n", obj_index->short_descr );
   }
   ch_printf( ch, "%d/%d modules installed.\r\n", ship->modules, ship->maxmodules );

   return;
}

/*
 * do_dupe_mods - v1.19, ported from SWGD. Admin utility to copy a
 * module loadout from one ship to another (SWGD's own comment: "similar
 * to a copyship, but the ship already exists"). Resets the destination
 * ship afterward so the copied modules take effect immediately.
 */
void do_dupe_mods( CHAR_DATA *ch, const char *argument )
{
   SHIP_DATA *duplicate;
   SHIP_DATA *original;
   char arg[MAX_INPUT_LENGTH];
   short x;

   argument = one_argument( argument, arg );

   if( arg[0] == '\0' || argument[0] == '\0' )
   {
      send_to_char( "Usage: dupe_mods <original> <duplicate>\r\n", ch );
      return;
   }

   if( ( original = get_ship( arg ) ) == NULL )
   {
      send_to_char( "The original is invalid.\r\n", ch );
      return;
   }

   if( ( duplicate = get_ship( argument ) ) == NULL )
   {
      send_to_char( "The duplicate is invalid.\r\n", ch );
      return;
   }

   if( original->modules == 0 )
   {
      send_to_char( "The original is not modularized.\r\n", ch );
      return;
   }

   for( x = 0; x <= original->modules; x++ )
      duplicate->module_vnum[x] = original->module_vnum[x];

   if( duplicate->modules > original->modules )
   {
      for( x = original->modules + 1; x <= duplicate->modules; x++ )
         duplicate->module_vnum[x] = 0;
   }

   duplicate->modules = original->modules;
   duplicate->maxmodules = original->maxmodules;
   update_ship_modules( duplicate );
   resetship( duplicate );
   send_to_char( "Done.\r\n", ch );
   return;
}

/* =====================================================================
 * Cargo economy system - v1.19, ported from SWGD (Arcturus).
 *
 * Scope note: SWGD gates cargo load/unload on ROOM_IMPORT || ROOM_SPACECRAFT.
 * This codebase's room_flags is a plain 32-bit int with all bits already
 * in use (confirmed via full enum check) - no free bit for ROOM_IMPORT
 * without reclaiming an existing flag. Rather than do that unaudited,
 * this port uses ROOM_SPACECRAFT alone as the gate. Functionally
 * equivalent for any room a builder flags for ship cargo handling; the
 * only loss is not having a *second*, differently-named flag for rooms
 * that want cargo trading without also being a general spacecraft room.
 * ROOM_IMPORT can be added later via a reclaimed bit if that distinction
 * turns out to matter in practice.
 * =====================================================================
 */

/*
 * Return an ascii name for a cargo type, or "None".
 */
const char *get_cargo_name( int sn )
{
   if( sn <= CARGO_NONE || sn > top_cargo_sn )
      return "None";
   return cargo_table[sn]->name;
}

/*
 * Look up a cargo type by exact or prefix name match.
 */
CARGO_DATA *get_cargo( const char *name )
{
   int loop;

   for( loop = CARGO_NONE + 1; loop <= top_cargo_sn; loop++ )
      if( !str_cmp( cargo_table[loop]->name, name ) )
         return cargo_table[loop];
   return NULL;
}

int get_cargo_num( const char *name )
{
   int loop;

   for( loop = CARGO_NONE + 1; loop <= top_cargo_sn; loop++ )
      if( !str_cmp( cargo_table[loop]->name, name ) )
         return loop;
   for( loop = CARGO_NONE + 1; loop <= top_cargo_sn; loop++ )
      if( nifty_is_name_prefix( name, cargo_table[loop]->name ) )
         return loop;
   return CARGO_NONE;
}

/*
 * Recompute a planet's per-ton prices from supply/demand, tax rate, and
 * popular support. Ported verbatim from SWGD's formula (Arcturus).
 */
void update_cargo_costs( PLANET_DATA * planet )
{
   int cost;
   int resource;
   double tfactor;
   double pfactor;
   long taxes = planet->base_value;
   float popsupport = planet->pop_support;

   tfactor = ( double )taxes / AVG_PLANET_VALUE;
   tfactor = tfactor < 0.80 ? 0.80 : tfactor > 1.20 ? 1.20 : tfactor;

   pfactor = 1 - popsupport / 500;

   for( resource = CARGO_NONE + 1; resource <= top_cargo_sn; resource++ )
   {
      double sfactor;
      int consumes;
      int amt;

      consumes = UMAX( 1, planet->consumes[resource] );
      amt = UMAX( 1, planet->resource[resource] );
      sfactor = ( double )consumes / amt;
      sfactor = sfactor > 1.50 ? 1.50 : sfactor < 0.50 ? 0.50 : sfactor;

      cost = cargo_table[resource]->value;
      cost = ( int )( cost * ( sfactor * tfactor * pfactor ) );
      cost = UMAX( 1, cost );
      planet->rcost[resource] = cost;
   }
}

/*
 * Read a single #CARGO entry from system/cargo.dat.
 */
void fread_cargo( FILE * fp )
{
   char buf[MAX_STRING_LENGTH];
   const char *word;
   CARGO_DATA *cargo;
   bool fMatch;

   CREATE( cargo, CARGO_DATA, 1 );

   top_cargo_sn++;
   if( top_cargo_sn >= MAX_CARGO )
   {
      top_cargo_sn--;
      bug( "%s: cargo SNs over max.", __func__ );
      return;
   }

   for( ;; )
   {
      word = feof( fp ) ? "End" : fread_word( fp );
      fMatch = FALSE;

      switch ( UPPER( word[0] ) )
      {
         case '*':
            fMatch = TRUE;
            fread_to_eol( fp );
            break;

         case 'E':
            if( !str_cmp( word, "End" ) )
            {
               cargo->sn = top_cargo_sn;
               cargo_table[top_cargo_sn] = cargo;
               return;
            }
            break;

         case 'N':
            if( !str_cmp( word, "Name" ) )
            {
               cargo->name = fread_string( fp );
               fMatch = TRUE;
            }
            break;

         case 'V':
            KEY( "Value", cargo->value, fread_number( fp ) );
            break;
      }

      if( !fMatch )
      {
         snprintf( buf, MAX_STRING_LENGTH, "%s: no match: %s", __func__, word );
         bug( buf );
      }
   }
}

/*
 * Load system/cargo.dat at boot.
 */
void load_cargo( void )
{
   char filename[256];
   FILE *fp;

   top_cargo_sn = CARGO_NONE;

   snprintf( filename, 256, "%scargo.dat", SYSTEM_DIR );
   if( ( fp = fopen( filename, "r" ) ) == NULL )
   {
      bug( "%s: could not open cargo file.", __func__ );
      return;
   }

   for( ;; )
   {
      char letter;
      const char *word;

      letter = fread_letter( fp );
      if( letter == '*' )
      {
         fread_to_eol( fp );
         continue;
      }
      if( letter != '#' )
      {
         bug( "%s: # not found.", __func__ );
         break;
      }

      word = fread_word( fp );
      if( !str_cmp( word, "CARGO" ) )
      {
         fread_cargo( fp );
         continue;
      }
      else if( !str_cmp( word, "END" ) )
         break;
      else
      {
         char buf[MAX_STRING_LENGTH];

         snprintf( buf, MAX_STRING_LENGTH, "%s: bad section: %s.", __func__, word );
         bug( buf );
         continue;
      }
   }
   fclose( fp );
}

/*
 * Save system/cargo.dat.
 */
void save_cargo( void )
{
   FILE *fp;
   char filename[256];
   int cargonum;

   snprintf( filename, 256, "%scargo.dat", SYSTEM_DIR );
   if( ( fp = fopen( filename, "w" ) ) == NULL )
   {
      bug( "%s: failure to save cargo types.", __func__ );
      return;
   }

   for( cargonum = CARGO_NONE + 1; cargonum <= top_cargo_sn; cargonum++ )
   {
      fprintf( fp, "#CARGO\n" );
      fprintf( fp, "Name      %s~\n", cargo_table[cargonum]->name );
      fprintf( fp, "Value     %d\n", cargo_table[cargonum]->value );
      fprintf( fp, "End\n\n" );
   }
   fprintf( fp, "#END\n" );
   fclose( fp );
}

/*
 * Define a new cargo type - immortal-only, matching the admin-tool
 * pattern used elsewhere in this file.
 */
void do_makecargo( CHAR_DATA * ch, const char *argument )
{
   CARGO_DATA *cargo;

   if( argument[0] == '\0' )
   {
      send_to_char( "Syntax: makecargo <name>\r\n", ch );
      return;
   }
   if( get_cargo( argument ) != NULL )
   {
      send_to_char( "That cargo name is already in use.\r\n", ch );
      return;
   }
   if( top_cargo_sn >= MAX_CARGO )
   {
      send_to_char( "To make more cargo types, update the hardcoded maximum.\r\n", ch );
      return;
   }

   CREATE( cargo, CARGO_DATA, 1 );
   cargo->sn = ++top_cargo_sn;
   cargo->name = str_dup( argument );
   cargo->value = 1;
   cargo_table[top_cargo_sn] = cargo;
   save_cargo(  );
   send_to_char( "Done.\r\n", ch );
}

void do_setcargo( CHAR_DATA * ch, const char *argument )
{
   char arg1[MAX_INPUT_LENGTH];
   char arg2[MAX_INPUT_LENGTH];
   int cargonum;

   if( !str_cmp( argument, "save" ) )
   {
      save_cargo(  );
      send_to_char( "Done.\r\n", ch );
      return;
   }

   argument = one_argument( argument, arg1 );
   argument = one_argument( argument, arg2 );

   if( argument[0] == '\0' )
   {
      send_to_char( "Syntax: setcargo <type> <field> <value>\r\n", ch );
      send_to_char( "Syntax: setcargo save\r\n", ch );
      send_to_char( "Fields are: name, value\r\n", ch );
      return;
   }

   if( ( cargonum = get_cargo_num( arg1 ) ) == CARGO_NONE )
   {
      cargonum = atoi( arg1 );
      if( cargonum <= CARGO_NONE || cargonum > top_cargo_sn )
      {
         send_to_char( "Invalid cargo type.\r\n", ch );
         return;
      }
   }

   if( !str_cmp( arg2, "value" ) )
   {
      if( !is_number( argument ) )
      {
         send_to_char( "Value must be numeric.\r\n", ch );
         return;
      }
      cargo_table[cargonum]->value = atoi( argument );
      send_to_char( "Done.\r\n", ch );
      return;
   }

   if( !str_cmp( arg2, "name" ) )
   {
      if( get_cargo( argument ) != NULL )
      {
         send_to_char( "That name is already in use.\r\n", ch );
         return;
      }
      STRFREE( cargo_table[cargonum]->name );
      cargo_table[cargonum]->name = str_dup( argument );
      send_to_char( "Done.\r\n", ch );
      return;
   }

   do_setcargo( ch, "" );
}

/*
 * List all defined cargo types.
 */
void do_cargo( CHAR_DATA * ch, const char *argument )
{
   int cargonum;

   for( cargonum = CARGO_NONE + 1; cargonum <= top_cargo_sn; cargonum++ )
      ch_printf( ch, "SN: %3d Name: %-18s Value: %d\r\n",
                 cargo_table[cargonum]->sn, cargo_table[cargonum]->name, cargo_table[cargonum]->value );
}

/*
 * Immortal-only: reset every planet's stock back to its consumption
 * baseline and recompute prices. Useful after economy tuning or a wipe.
 */
void do_reset_cargo( CHAR_DATA * ch, const char *argument )
{
   PLANET_DATA *planet;
   int x;

   if( IS_NPC( ch ) || !IS_IMMORTAL( ch ) )
      return;

   for( planet = first_planet; planet; planet = planet->next )
   {
      for( x = CARGO_NONE + 1; x <= top_cargo_sn; x++ )
         planet->resource[x] = planet->consumes[x];
      update_cargo_costs( planet );
      save_planet( planet );
   }
   send_to_char( "Cargo reset.\r\n", ch );
}

/*
 * Galaxy-wide price chart for one cargo type across every planet.
 */
void do_show_cargo( CHAR_DATA * ch, const char *argument )
{
   PLANET_DATA *planet;
   int cargonum;

   if( IS_NPC( ch ) )
      return;

   if( argument[0] == '\0' )
   {
      send_to_char( "&RSyntax: show_cargo <cargotype>\r\n", ch );
      return;
   }

   cargonum = get_cargo_num( argument );
   if( cargonum == CARGO_NONE )
   {
      send_to_char( "Invalid cargotype.\r\n", ch );
      return;
   }

   ch_printf( ch, "&wShowing intergalactic trade chart for: &c%s\r\n", cargo_table[cargonum]->name );
   ch_printf( ch, "&BPlanet            &CPrice    &wBuying    &RSelling\r\n" );
   ch_printf( ch, "&w----------------------------------------------------\r\n" );

   for( planet = first_planet; planet; planet = planet->next )
   {
      ch_printf( ch, "&B%-15s &C%6d    &w%6s    &R%6s\r\n", planet->name, planet->rcost[cargonum],
                 planet->resource[cargonum] < planet->consumes[cargonum] * 2 ? "Yes" : "No",
                 planet->resource[cargonum] > planet->consumes[cargonum] ? "Yes" : "No" );
   }
}

/*
 * Full import/export breakdown for one named planet.
 */
void do_imports( CHAR_DATA * ch, const char *argument )
{
   PLANET_DATA *planet;
   int cargonum;

   if( argument[0] == '\0' )
   {
      send_to_char( "Usage: imports <planet>\r\n", ch );
      return;
   }

   planet = get_planet( argument );
   if( !planet )
   {
      send_to_char( "&RNo such planet.\r\n", ch );
      return;
   }

   ch_printf( ch, "&BImport and Export data for %s:\r\n", planet->name );
   ch_printf( ch, "&GResource                 &CPrice     &wProduces     &RConsumes     &GAmount\r\n" );
   ch_printf( ch, "&G--------                 ------    --------     --------     ------\r\n" );

   for( cargonum = CARGO_NONE + 1; cargonum <= top_cargo_sn; cargonum++ )
      ch_printf( ch, "&G%-18s    &C%5d/ton  &w%6d/ton  &R%6d tons &G%6d tons\r\n",
                 cargo_table[cargonum]->name, planet->rcost[cargonum],
                 planet->produces[cargonum], planet->consumes[cargonum], planet->resource[cargonum] );
}

/*
 * Sell a shipload of cargo to a planet (or transfer to another ship
 * sitting in the same hangar). Ported from SWGD.
 */
void do_unload_cargo( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   SHIP_DATA *target;
   int cargonum;
   int cost;
   int change;
   PLANET_DATA *planet;

   if( argument[0] == '\0' )
   {
      send_to_char( "Which ship do you want to unload?\r\n", ch );
      return;
   }

   if( ( target = ship_in_room( ch->in_room, argument ) ) == NULL )
   {
      act( AT_PLAIN, "I see no $T here.", ch, NULL, argument, TO_CHAR );
      return;
   }

   if( !target->hatchopen )
   {
      send_to_char( "How are you going to manage that with the hatch closed?\r\n", ch );
      return;
   }

   if( target->cargo == 0 )
   {
      send_to_char( "You don't have any cargo.\r\n", ch );
      return;
   }

   if( !IS_SET( ch->in_room->room_flags, ROOM_SPACECRAFT ) )
   {
      send_to_char( "You can't do that here!\r\n", ch );
      return;
   }

   planet = ch->in_room->area->planet;

   if( !planet )
   {
      ship = ship_from_hanger( ch->in_room->vnum );
      if( !ship )
      {
         send_to_char( "You can't do that here!\r\n", ch );
         return;
      }
      if( ( ship->maxcargo - ship->cargo ) < 1 )
      {
         send_to_char( "There is no room for anymore cargo.\r\n", ch );
         return;
      }
      if( ship->cargo == 0 )
         ship->cargotype = CARGO_NONE;
      if( ship->cargo > 0 && ship->cargotype != target->cargotype )
      {
         send_to_char( "They have a different type of cargo.\r\n", ch );
         return;
      }
      if( ship->cargotype == CARGO_NONE )
         ship->cargotype = target->cargotype;

      if( ( ship->maxcargo - ship->cargo ) >= target->cargo )
      {
         ship->cargo += target->cargo;
         target->cargo = 0;
         target->cargotype = CARGO_NONE;
         send_to_char( "Cargo unloaded.\r\n", ch );
         return;
      }
      else
      {
         change = ship->maxcargo - ship->cargo;
         target->cargo -= change;
         ship->cargo = ship->maxcargo;
         ch_printf( ch, "%s loaded, %d tons still in %s's hold.\r\n", ship->name, target->cargo, target->name );
         return;
      }
   }

   cargonum = target->cargotype;
   if( !cargo_table[cargonum] )
   {
      send_to_char( "Serious bug, null cargotype in hold.\r\n", ch );
      bug( "%s: no cargotype in ship %s to unload.", __func__, target->name );
      return;
   }
   if( planet->consumes[cargonum] <= 0 )
   {
      ch_printf( ch, "We don't buy %s.\r\n", cargo_table[cargonum]->name );
      return;
   }
   if( planet->resource[cargonum] > ( planet->consumes[cargonum] * 2 ) )
   {
      ch_printf( ch, "It wouldn't make sense to import more of %s.\r\n", cargo_table[cargonum]->name );
      return;
   }

   change = target->cargo;
   cost = change * planet->rcost[cargonum];

   ch->gold += cost;
   planet->resource[cargonum] += target->cargo;
   target->cargo = 0;
   target->cargotype = CARGO_NONE;
   ch_printf( ch, "You receive %d credits for a load of %s.\r\n", cost, cargo_table[cargonum]->name );
   update_cargo_costs( planet );
   save_planet( planet );
}

/*
 * Buy a shipload of cargo from a planet (or transfer from another ship
 * sitting in the same hangar). Ported from SWGD.
 */
void do_load_cargo( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   SHIP_DATA *target;
   int cost;
   PLANET_DATA *planet;
   char arg1[MAX_INPUT_LENGTH];
   int cargonum;
   int change;
   const char *rest;

   rest = one_argument( argument, arg1 );

   if( arg1[0] == '\0' )
   {
      send_to_char( "Which ship do you want to load?\r\n", ch );
      return;
   }

   if( ( target = ship_in_room( ch->in_room, arg1 ) ) == NULL )
   {
      act( AT_PLAIN, "I see no $T here.", ch, NULL, arg1, TO_CHAR );
      return;
   }

   if( !target->hatchopen )
   {
      send_to_char( "How are you going to manage that with the hatch closed?\r\n", ch );
      return;
   }

   if( !IS_SET( ch->in_room->room_flags, ROOM_SPACECRAFT ) )
   {
      send_to_char( "You can't do that here!\r\n", ch );
      return;
   }

   planet = ch->in_room->area->planet;

   if( !planet )
   {
      ship = ship_from_hanger( ch->in_room->vnum );
      if( !ship )
      {
         send_to_char( "You can't do that here!\r\n", ch );
         return;
      }
      if( ship->cargo == 0 )
      {
         send_to_char( "They don't have any cargo.\r\n", ch );
         return;
      }
      if( ( target->maxcargo - target->cargo ) < 1 )
      {
         send_to_char( "There is no room for anymore cargo.\r\n", ch );
         return;
      }
      if( target->cargotype != CARGO_NONE && ship->cargotype != target->cargotype )
      {
         send_to_char( "Maybe you should deliver your cargo first.\r\n", ch );
         return;
      }
      if( target->cargotype == CARGO_NONE )
         target->cargotype = ship->cargotype;

      if( ( target->maxcargo - target->cargo ) >= ship->cargo )
      {
         target->cargo += ship->cargo;
         ship->cargo = 0;
         send_to_char( "Cargo loaded.\r\n", ch );
         return;
      }
      else
      {
         change = target->maxcargo - target->cargo;
         ship->cargo -= change;
         target->cargo = target->maxcargo;
         send_to_char( "Cargo loaded.\r\n", ch );
         return;
      }
   }

   if( rest[0] == '\0' )
   {
      send_to_char( "&RWhat do you want to load?&w\r\n", ch );
      return;
   }
   if( ( target->maxcargo - target->cargo ) <= 0 )
   {
      send_to_char( "There is no room for more cargo.\r\n", ch );
      return;
   }

   cargonum = get_cargo_num( rest );
   if( cargonum == CARGO_NONE )
   {
      send_to_char( "Invalid cargotype.\r\n", ch );
      return;
   }
   if( target->cargo > 0 && target->cargotype != cargonum )
   {
      send_to_char( "Maybe you should deliver your cargo first.\r\n", ch );
      return;
   }
   if( planet->resource[cargonum] < planet->consumes[cargonum] )
   {
      ch_printf( ch, "&RSorry, we cannot afford to ship more of %s.\r\n", cargo_table[cargonum]->name );
      return;
   }

   if( ( target->maxcargo - target->cargo ) >= planet->resource[cargonum] )
      change = planet->resource[cargonum];
   else
      change = target->maxcargo - target->cargo;

   cost = change * planet->rcost[cargonum];

   if( ch->gold < cost )
   {
      send_to_char( "You can't afford it!\r\n", ch );
      return;
   }
   ch->gold -= cost;

   if( ( target->maxcargo - target->cargo ) >= planet->resource[cargonum] )
   {
      target->cargo += planet->resource[cargonum];
      planet->resource[cargonum] = 0;
      target->cargotype = cargonum;
   }
   else
   {
      planet->resource[cargonum] -= target->maxcargo - target->cargo;
      target->cargo = target->maxcargo;
      target->cargotype = cargonum;
   }

   ch_printf( ch, "You pay %d credits for a load of %s.\r\n", cost, cargo_table[cargonum]->name );
   update_cargo_costs( planet );
   save_planet( planet );
}

void do_overdrive( CHAR_DATA *ch, const char *argument )
{
    int chance;
    SHIP_DATA *ship;
    char buf[MAX_STRING_LENGTH];

    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
        return;
    }

    if ( ship->ship_class > SHIP_PLATFORM )
    {
       send_to_char("&RThis isn't a spacecraft!\n\r",ch);
       return;
    }
    if (  (ship = ship_from_pilotseat(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RThe controls must be at the pilots chair...\n\r",ch);
       return;
    }
    if ( autofly(ship) )
    {
       send_to_char("&RYou'll have to turn off the ships autopilot first.\n\r",ch);
       return;
    }
    if  ( ship->ship_class == SHIP_PLATFORM )
    {
       send_to_char( "&RPlatforms can't move!\n\r" , ch );
       return;
    }

    if (ship->shipstate == SHIP_HYPERSPACE)
    {
       send_to_char("&RYou can only do that in realspace!\n\r",ch);
       return;
    }
    if (ship->shipstate == SHIP_DISABLED)
    {
       send_to_char("&RThe ships drive is disabled. Unable to accelerate.\n\r",ch);
       return;
    }
    if (ship->shipstate == SHIP_DOCKED)
    {
        send_to_char("&RYou can't do that until after you've launched!\n\r",ch);
        return;
    }

    if ((ship->shipstate2 == SHIP_DOCK) || (ship->shipstate2 == SHIP_DOCK_2))
    {
       send_to_char("&RNot while docking procedures are going on.\n\r", ch);
       return;
    }

    if (ship->shipstate2 == SHIP_DOCK_3)
    {
      send_to_char("&RDetach from the docked ship first.\n\r",ch);
      return;
    }        
    if (ship->overdrive == 0)
    {
      send_to_char("&RTry installing a SLAM overdrive node first.\n\r", ch);
      return;
    }
    if (IS_SET(ship->flags, SHIPFLAG_AFTERBURNER))
    {
      send_to_char("&RYour afterburners must be turned off before you can overpower your engine.\n\r", ch);
      return;
    }
    if(ship->energy < 200)
    {
      send_to_char("&RYou don't have enough fuel.", ch);
      return;
    }

    if( check_sabotage( ship, SHIPFLAG_SABOTAGEDENGINE ) )
       return;

    if(!str_cmp(argument, "off"))
    {
      if (IS_SET(ship->flags, SHIPFLAG_OVERDRIVENODE))
      {
        send_to_char("&WYou shut off your overdrive node, and your ship decelerates.\n\r", ch);
        REMOVE_BIT(ship->flags, SHIPFLAG_OVERDRIVENODE);
        act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument , TO_ROOM );
        if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
        {
           sprintf( buf, "%s begins to slow down." , ship->name );
           echo_to_system( AT_ORANGE , ship , buf , NULL );
        }
        ship->currspeed = UMAX(0, ship->realspeed + speedbonus(ship));
        return;
      }
      else
      {
         send_to_char("&WYour overdrive node isn't active.\n\r", ch);
         return;
      }
    }
      if(IS_SET(ship->flags, SHIPFLAG_OVERDRIVENODE))
      {
          send_to_char("&WYour overdrive is already on. Type overdrive off to turn it off.\n\r", ch);
          return;
      }                
      chance = IS_NPC(ch) ? ch->top_level : (int) (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_overdrive] ) );
      if ( number_percent( ) >= chance )
      {
          send_to_char("&RYou fail to work the controls properly.\n\r",ch);
          learn_from_failure(ch, gsn_overdrive);
          return;
      }
      learn_from_success(ch, gsn_overdrive);
      SET_BIT(ship->flags, SHIPFLAG_OVERDRIVENODE);
      send_to_char("Overdrive node active, powering up engines.\n\r", ch);
      act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument , TO_ROOM );
      if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
      {
         sprintf( buf, "%s begins to speed up." , ship->name );
         echo_to_system( AT_ORANGE , ship , buf , NULL );
      }
      ship->currspeed = UMAX(0, (int) ship->realspeed * speedbonus(ship));   
      ship->energy -= 200; /* fuel used to activate it. */
      return;
}

/* AFTERBURNER BY ARCTURUS - ported from SWGD v1.22 */
void do_burn( CHAR_DATA *ch, const char *argument )
{
    int chance;
    SHIP_DATA *ship;
    char buf[MAX_STRING_LENGTH];

    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
        return;
    }

    if ( ship->ship_class > SHIP_PLATFORM )
    {
       send_to_char("&RThis isn't a spacecraft!\n\r",ch);
       return;
    }
    if (  (ship = ship_from_pilotseat(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RThe controls must be at the pilots chair...\n\r",ch);
       return;
    }
    if ( autofly(ship) )
    {
       send_to_char("&RYou'll have to turn off the ships autopilot first.\n\r",ch);
       return;
    }
    if  ( ship->ship_class == SHIP_PLATFORM )
    {
       send_to_char( "&RPlatforms can't move!\n\r" , ch );
       return;
    }

    if (ship->shipstate == SHIP_HYPERSPACE)
    {
       send_to_char("&RYou can only do that in realspace!\n\r",ch);
       return;
    }
    if (ship->shipstate == SHIP_DISABLED)
    {
       send_to_char("&RThe ships drive is disabled. Unable to accelerate.\n\r",ch);
       return;
    }
    if (ship->shipstate == SHIP_DOCKED)
    {
        send_to_char("&RYou can't do that until after you've launched!\n\r",ch);
        return;
    }

    if ((ship->shipstate2 == SHIP_DOCK) || (ship->shipstate2 == SHIP_DOCK_2))
    {
       send_to_char("&RNot while docking procedures are going on.\n\r", ch);
       return;
    }

    if (ship->shipstate2 == SHIP_DOCK_3)
    {
      send_to_char("&RDetach from the docked ship first.\n\r",ch);
      return;
    }        

    if (IS_SET(ship->flags, SHIPFLAG_OVERDRIVENODE))
    {
      send_to_char("&RYour overdrive node must be turned off before you can use your afterburners.\n\r", ch);
      return;
    }
    if(ship->energy < 100)
    {
       send_to_char("You don't have enough fuel.\n\r", ch);
       return;
    }

    if( check_sabotage( ship, SHIPFLAG_SABOTAGEDENGINE ) )
       return;

    if(!str_cmp(argument, "off"))
    {
      if (IS_SET(ship->flags, SHIPFLAG_AFTERBURNER))
      {
        send_to_char("&WYou shut off your afterburners, and your ship decelerates.\n\r", ch);
        REMOVE_BIT(ship->flags, SHIPFLAG_AFTERBURNER);
        act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument , TO_ROOM );
        if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
        {
           sprintf( buf, "%s begins to slow down." , ship->name );
           echo_to_system( AT_ORANGE , ship , buf , NULL );
        }
        ship->currspeed = UMAX(0, ship->realspeed + speedbonus(ship));
        return;
      }
      else
      {
         send_to_char("&WYour afterburners aren't even active.\n\r", ch);
         return;
      }
    }
      if(IS_SET(ship->flags, SHIPFLAG_AFTERBURNER))
      {
          send_to_char("&WYour afterburners are already on. Type burn off to turn them off.\n\r", ch);
          return;
      }                
      chance = IS_NPC(ch) ? ch->top_level : (int) (ch->pcdata->learned[gsn_burn]);
      if ( number_percent( ) >= chance )
      {
          send_to_char("&RYou fail to work the controls properly.\n\r",ch);
          learn_from_failure(ch, gsn_burn);
          return;
      }
      learn_from_success(ch, gsn_burn);
      SET_BIT(ship->flags, SHIPFLAG_AFTERBURNER);
      send_to_char("Afterburners active, accelerating.\n\r", ch);
      act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument , TO_ROOM );
      if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
      {
         sprintf( buf, "%s begins to speed up." , ship->name );
         echo_to_system( AT_ORANGE , ship , buf , NULL );
      }
      ship->currspeed = UMAX(0, (int) ship->realspeed * speedbonus(ship));
      ship->energy -= 100;
      return;
}       

void do_plot(CHAR_DATA *ch, const char *argument )
{
    int cur_distance;
    int new_distance;
    int distance;
    SHIP_DATA *ship;
    SPACE_DATA *starsystem, *target;

    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
       return;
    }

    if ( ship->ship_class > SHIP_PLATFORM )
    {
       send_to_char("&RThis isn't a spacecraft!\n\r",ch);
       return;
    }

    if (  (ship = ship_from_navseat(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RYou must be at a nav computer to calculate jumps.\n\r",ch);
       return;
    }

    if ( autofly(ship)  )
    {
       send_to_char("&RThe autopiloting system refuses your manual over-ride.\n\r",ch);
       return;
    }

    if  ( ship->ship_class == SHIP_PLATFORM )
    {
       send_to_char( "&RAnd what exactly are you going to calculate...?\n\r" , ch );
       return;
    }
  
    if (ship->hyperspeed == 0)
    {
        send_to_char("&RThis ship is not equipped with a hyperdrive!\n\r",ch);
        return;
    }
 
    if (ship->shipstate == SHIP_DOCKED)
    {
        send_to_char("&RYou can't do that until after you've launched!\n\r",ch);
        return;
    }

    if ((ship->shipstate2 == SHIP_DOCK) || (ship->shipstate2 == SHIP_DOCK_2))
    {
        send_to_char("&RNot while docking procedures are going on.\n\r", ch);
        return;
    }

    if (ship->shipstate2 == SHIP_DOCK_3)
    {
        send_to_char("&RDetach from the docked ship first.\n\r",ch);
        return;
    }

    if (ship->starsystem == NULL)
    {
        send_to_char("&RYou can only do that in realspace.\n\r",ch);
        return;
    }
 
    if( IS_SET( ship->flags, SHIPFLAG_SIMULATOR ) )
    {
        send_to_char("&RThere are no starsystems that need plotting from here.\n\r", ch );
        return;
    }
    if (!(argument && argument[0] != '\0'))
    {
        send_to_char("&WFormat: Plot <starsystem>\n\r&wPossible destinations:\n\r", ch );
        for ( starsystem = first_starsystem; starsystem; starsystem = starsystem->next )
        {
           set_char_color( AT_NOTE, ch );
           if ( str_cmp( starsystem->name, "Simulator" ) )
           {
              ch_printf(ch,"%-30s %d\n\r",starsystem->name,
                       (int) sqrt( pow(ship->starsystem->xpos - starsystem->xpos, 2) +
                                   pow(ship->starsystem->ypos - starsystem->ypos, 2) ) );
           }
        }
        return;
    }

    target = starsystem_from_name( argument );

    if( target != NULL && !str_cmp( target->name, "Simulator" ) )
        target = NULL;

    if ( target == NULL )
    {
        send_to_char( "&RYou can't seem to find that starsystem on your charts.\n\r", ch);
        return;
    }
       
    if( target == ship->starsystem )
    {
        send_to_char("&RPlot a course to here, closest ports to here, is here.\n\r", ch );
        return;
    }

    cur_distance = (int) sqrt( pow(ship->starsystem->xpos - target->xpos, 2) +
                               pow(ship->starsystem->ypos - target->ypos, 2) );
    distance = 0;
    set_char_color( AT_NOTE, ch );
    ch_printf( ch, "Nearest starsystems to %s.\n\r", target->name );  
    ch_printf( ch, "%-30s %-10s %-10s Distance from %s\n\r", "Name", "Distance", "Closer by", target->name );
    for( starsystem = first_starsystem; starsystem; starsystem = starsystem->next )
    {
        if( starsystem == target )
           continue;
        if( !str_cmp( starsystem->name, "Simulator") )
           continue;

        new_distance = (int) sqrt( pow(starsystem->xpos - target->xpos, 2) +
                                   pow(starsystem->ypos - target->ypos, 2) );

        if( new_distance >= cur_distance )
            continue;
        distance = (int) sqrt( pow(starsystem->xpos - ship->starsystem->xpos, 2 ) +
                               pow(starsystem->ypos - ship->starsystem->ypos, 2 ) );
        ch_printf( ch, "%-30s %-10d %-10d %-10d\n\r", starsystem->name, 
                   distance, cur_distance - new_distance, new_distance );
    }
    return;
}

void do_prepship(CHAR_DATA *ch, const char *argument )
{
    SHIP_DATA *ship;
    long cost;

    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
       return;
    }

    if( !str_cmp( ship->owner, "Public" ) )
    {
       send_to_char( "&RPublic craft do not need to be prepped.\n\r", ch );
       return;
    }

    if( ship->ship_class < FRIGATE_SHIP )
    {
       if (ship->shipstate != SHIP_DOCKED)
       {
          send_to_char("&RYou can only prepare ships while you are docked.\n\r", ch );
          return;
       }
    }
    else
    {
       if( ship->ship_class > SHIP_PLATFORM )
       {
          send_to_char("&RThis isn't a ship.\n\r", ch );
          return;
       }
       if( autofly( ship ) )
       { 
          send_to_char("&RYou need to deactivate the autopilot first.\n\r", ch );
          return;
       }
       if( !IS_DOCKED( ship ) )
       {
          send_to_char("&RYou need to dock first.\n\r", ch );
          return;
       }
       if( ship->docked_ship->ship_class != SHIP_PLATFORM )
       {
          send_to_char("&ROnly platforms can repair and resupply.\n\r", ch );
          return; 
       }
    }
    cost = prepcost( ship );

    if( cost > ch->gold )
    {
       ch_printf( ch, "It would cost %ld credits to prepare the ship for launch, which you do not have.\n\r", cost );
       return;
    }
    ch->gold -= cost;

    ch_printf( ch, "You pay %ld credits to prepare the ship for launch.\n\r", cost );
    act( AT_PLAIN, "$n begins to prepare the ship for flight.", ch,
         NULL, argument , TO_ROOM );
    prepship( ship );
    if( ship->maxenergy == 0 )
         echo_to_cockpit( AT_BLOOD, ship, "This vessel lacks fuel rods. Auxiliary power is insufficient for space flight.");
    return;
}

void do_redirect( CHAR_DATA * ch, const char *argument )
{
    char arg1[MAX_INPUT_LENGTH];
    char buf[MAX_STRING_LENGTH];
    SHIP_DATA *ship;
    int chance;
    argument = one_argument(argument, arg1);

    if ( str_cmp( argument , "laser" ) && str_cmp( argument , "shield" ) && str_cmp(argument, "engine")
      && str_cmp( argument , "lasers" ) && str_cmp( argument, "shields" )
      && str_cmp( arg1, "laser" ) && str_cmp(arg1, "shield") && str_cmp(arg1, "engine") && str_cmp(arg1, "default") 
      && str_cmp( arg1, "lasers" ) && str_cmp(arg1, "shields") )
    {
       send_to_char("Syntax: redirect <laser/shield/engine> <laser/shield/engine/default>\n\r", ch);
       return;
    }
    if(!str_cmp(arg1, argument))
    {
       send_to_char("Syntax for disabling redirection: redirect <laser/shield/engine> default\n\r", ch);
       return;
    }  
        if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
        {
            send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
            return;
        }
        if (  (ship = ship_from_pilotseat(ch->in_room->vnum))  == NULL )
        {
            send_to_char("&RYou must be in the pilot seat of a ship to do that!\n\r",ch);
            return;
        }
        if(!str_cmp(argument, "laser") || !str_cmp(arg1, "laser"))
        {
           if(ship->lasers == 0)
           {
              send_to_char("Redirect with what lasers?\n\r", ch);
              return;
           }
        }
        if(!str_cmp(argument, "shield") || !str_cmp(arg1, "shield"))
        {
           if(ship->maxshield == 0)
           {
              send_to_char("Redirect with what shields?\n\r", ch);
              return;
           }
        }  
        if ( ship->ship_class > SHIP_PLATFORM )
        {
             send_to_char("&RThis isn't a spacecraft!\n\r",ch);
             return;
        }
        if ( autofly(ship) )
        {
             send_to_char("&RYou'll have to turn off the ships autopilot first.\n\r",ch);
             return;
        }
        if (ship->shipstate == SHIP_HYPERSPACE)
        {
             send_to_char("&RYou can only do that in realspace!\n\r",ch);
             return;
        }
        if (ship->starsystem == NULL)
        {
             send_to_char("&RYou can't do that until after you've finished launching!\n\r",ch);
             return;
        }

    chance = IS_NPC(ch) ? ch->top_level
                 : (int)  (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_shipsystems] ) ) ;
    if ( number_percent( ) > chance )
    {
        send_to_char("&RYou can't quite figure out the controls.\n\r",ch);
        learn_from_failure( ch, gsn_shipsystems );
        return;
    }
    act( AT_PLAIN, "$n manipulates the ships controls.", ch, NULL, argument , TO_ROOM );
    if(!str_cmp(arg1, "laser") || !str_cmp(arg1, "lasers") )
    {
       if(!str_cmp(argument, "default"))
       {
          if(IS_SET(ship->flags, SHIPFLAG_LASERRENGINE))
          {
             REMOVE_BIT(ship->flags, SHIPFLAG_LASERRENGINE);
             if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
             {
                sprintf( buf, "%s begins to slow down." , ship->name );
                echo_to_system( AT_ORANGE , ship , buf , NULL );
             }
             ship->currspeed = UMIN(ship->currspeed, (int) ship->realspeed * speedbonus(ship));
          }
          if(IS_SET(ship->flags, SHIPFLAG_LASERRSHIELD))
             REMOVE_BIT(ship->flags, SHIPFLAG_LASERRSHIELD);
          send_to_char("Lasers recharging at a normal rate.\n\r", ch);
       }
       if(!str_cmp(argument, "shield") || !str_cmp(argument, "shields") )
       {
          if(IS_SET(ship->flags, SHIPFLAG_LASERRENGINE))
          {
             REMOVE_BIT(ship->flags, SHIPFLAG_LASERRENGINE);
             if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
             {
                sprintf( buf, "%s begins to slow down." , ship->name );
                echo_to_system( AT_ORANGE , ship , buf , NULL );
             }
             ship->currspeed = UMIN(ship->currspeed, (int) ship->realspeed * speedbonus(ship));
          } 
          if(!IS_SET(ship->flags, SHIPFLAG_LASERRSHIELD))
             SET_BIT(ship->flags, SHIPFLAG_LASERRSHIELD);
          send_to_char("Laser power redirected to shields, lasers recharging at a decreased rate, shields recharging at increased rate.\n\r", ch);
          if(IS_SET(ship->flags, SHIPFLAG_SHIELDRLASER))
         {
             send_to_char("Shield power transfer to lasers, reverted.\n\r", ch);
             REMOVE_BIT(ship->flags, SHIPFLAG_SHIELDRLASER);
          }
       }
       if(!str_cmp(argument, "engine"))
       {
          if(!IS_SET(ship->flags, SHIPFLAG_LASERRENGINE))
             SET_BIT(ship->flags, SHIPFLAG_LASERRENGINE);
          if(IS_SET(ship->flags, SHIPFLAG_LASERRSHIELD))
             REMOVE_BIT(ship->flags, SHIPFLAG_LASERRSHIELD);
          send_to_char("Laser power being redirected to engines. Lasers recharging at a decreased rate.\n\r", ch);
          if(IS_SET(ship->flags, SHIPFLAG_ENGINERLASER))
          {
             send_to_char("Engine power redirected back to engines.\n\r", ch);
             REMOVE_BIT(ship->flags, SHIPFLAG_ENGINERLASER);
          }
          if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
          {
             sprintf( buf, "%s begins to speed up." , ship->name );
             echo_to_system( AT_ORANGE , ship , buf , NULL );
          }
          ship->currspeed = UMAX(0, (int) ship->realspeed * speedbonus(ship));
       }
    }  

    if(!str_cmp(arg1, "shield") || !str_cmp(arg1, "shields" ) )
    {
       if(!str_cmp(argument, "default"))
       {
          if(IS_SET(ship->flags, SHIPFLAG_SHIELDRENGINE))
          {
             REMOVE_BIT(ship->flags, SHIPFLAG_SHIELDRENGINE);
             if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
             {          
                sprintf( buf, "%s begins to slow down." , ship->name );
                echo_to_system( AT_ORANGE , ship , buf , NULL );
             }
             ship->currspeed = UMIN(ship->currspeed, (int) ship->realspeed * speedbonus(ship));
          }
          if(IS_SET(ship->flags, SHIPFLAG_SHIELDRLASER))
             REMOVE_BIT(ship->flags, SHIPFLAG_SHIELDRLASER);
          send_to_char("Shields recharging at normal rate\n\r", ch);
       }
       if(!str_cmp(argument, "laser") || !str_cmp(argument, "lasers") )
       {
          if(IS_SET(ship->flags, SHIPFLAG_SHIELDRENGINE))
          {
             REMOVE_BIT(ship->flags, SHIPFLAG_SHIELDRENGINE);
             if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
             {
                sprintf( buf, "%s begins to slow down." , ship->name );
                echo_to_system( AT_ORANGE , ship , buf , NULL );
             } 
             ship->currspeed = UMIN(ship->currspeed, (int) ship->realspeed * speedbonus(ship));
          }
          if(!IS_SET(ship->flags, SHIPFLAG_SHIELDRLASER))
             SET_BIT(ship->flags, SHIPFLAG_SHIELDRLASER);
          send_to_char("Shield power redirected to lasers, shield power recharging at a decreased rate.\n\r", ch);
          if(IS_SET(ship->flags, SHIPFLAG_LASERRSHIELD))
          {
             send_to_char("Laser transfer to shields reverted, lasers recharging at an increased rate.\n\r", ch);
             REMOVE_BIT(ship->flags, SHIPFLAG_LASERRSHIELD);
          }
       }
       if(!str_cmp(argument, "engine"))
       {
          if(!IS_SET(ship->flags, SHIPFLAG_SHIELDRENGINE))
             SET_BIT(ship->flags, SHIPFLAG_SHIELDRENGINE);
          if(IS_SET(ship->flags, SHIPFLAG_SHIELDRLASER))
             REMOVE_BIT(ship->flags, SHIPFLAG_SHIELDRLASER);
          send_to_char("Shield power being redirected to engines, shields recharging at a decreased rate.\n\r", ch);
          if(IS_SET(ship->flags, SHIPFLAG_ENGINERSHIELD))
          {
              send_to_char("Engine power redirected back to engines.\n\r", ch);
              REMOVE_BIT(ship->flags, SHIPFLAG_ENGINERSHIELD);
          }
          if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
          {
             sprintf( buf, "%s begins to speed up." , ship->name );
             echo_to_system( AT_ORANGE , ship , buf , NULL );
          }
          ship->currspeed = UMAX(0, (int) ship->realspeed * speedbonus(ship));
       }
    }  
    if(!str_cmp(arg1, "engine"))
    {
       if(!str_cmp(argument, "default"))
       {
          if(IS_SET(ship->flags, SHIPFLAG_ENGINERLASER))
             REMOVE_BIT(ship->flags, SHIPFLAG_ENGINERLASER);
          if(IS_SET(ship->flags, SHIPFLAG_ENGINERSHIELD))
             REMOVE_BIT(ship->flags, SHIPFLAG_ENGINERSHIELD);
          send_to_char("The engines revert power back to normal.\n\r", ch);
          if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
          {
             sprintf( buf, "%s begins to speed up." , ship->name );
             echo_to_system( AT_ORANGE , ship , buf , NULL );
          }
          ship->currspeed = UMAX(0, (int) ship->realspeed * speedbonus(ship));
       }
       if(!str_cmp(argument, "shield") || !str_cmp(argument, "shields") )
       {
          if(IS_SET(ship->flags, SHIPFLAG_ENGINERLASER))
             REMOVE_BIT(ship->flags, SHIPFLAG_ENGINERLASER);
          if(!IS_SET(ship->flags, SHIPFLAG_ENGINERSHIELD))
             SET_BIT(ship->flags, SHIPFLAG_ENGINERSHIELD);
          if(IS_SET(ship->flags, SHIPFLAG_SHIELDRENGINE))
          {
             send_to_char("Shield redirection back to shields.\n\r", ch);
             REMOVE_BIT(ship->flags, SHIPFLAG_SHIELDRENGINE);
          }
          if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
          {
             send_to_char("Redirecting engine power to shields.\n\r", ch);
             sprintf( buf, "%s begins to slow down." , ship->name );
          }
          echo_to_system( AT_ORANGE , ship , buf , NULL );
          ship->currspeed = UMIN(ship->currspeed, (int) ship->realspeed * speedbonus(ship));
       }
       if(!str_cmp(argument, "laser") || !str_cmp(argument, "lasers") )
       {
          if(!IS_SET(ship->flags, SHIPFLAG_ENGINERLASER))
             SET_BIT(ship->flags, SHIPFLAG_ENGINERLASER);
          if(IS_SET(ship->flags, SHIPFLAG_ENGINERSHIELD))
             REMOVE_BIT(ship->flags, SHIPFLAG_ENGINERSHIELD);
          send_to_char("Engine power being redirected to lasers.\n\r", ch);
          if(IS_SET(ship->flags, SHIPFLAG_LASERRENGINE))
          {
              send_to_char("Laser power being redirected back to lasers.\n\r", ch);
              REMOVE_BIT(ship->flags, SHIPFLAG_LASERRENGINE);
          }
          if(!IS_SET(ship->flags, SHIPFLAG_CLOAKED) )
          {
             sprintf( buf, "%s begins to slow down." , ship->name );
             echo_to_system( AT_ORANGE , ship , buf , NULL );
          }
          ship->currspeed = UMIN(ship->currspeed, (int) ship->realspeed * speedbonus(ship));
       }
    }  
    learn_from_success(ch, gsn_shipsystems);
    return;
}

void do_remoteclosebay( CHAR_DATA *ch, const char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    one_argument( argument, arg );
    int chance;
    SHIP_DATA *ship;
    SHIP_DATA *target;

    if (arg[0] == '\0')
    {
        send_to_char("&RYou need to specify a target!\n\r",ch);
        return;
    }
    strcpy( arg, argument );

    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
    {
        send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
        return;
    }

    target = get_ship_here( arg, ship->starsystem );
    if (  target == NULL )
    {
        send_to_char("&RThat ship isn't here!\n\r",ch);
        return;
    }

    if ( !check_pilot( ch , target ) && ship->owner[0] != '\0' )
    {
        send_to_char("&RHey, thats not your ship!!\n\r",ch);
        return;
    }

    if(target->ship_class == SHIP_PLATFORM)
    {
       send_to_char("You can't open or close the bay doors on that.\n\r", ch);
       return;
    }

    if( !target->bayopen )
    {
       send_to_char("The bays on that ship are already closed.\n\r", ch );
       return;
    }
    chance = IS_NPC(ch) ? ch->top_level
                        : (int)  (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_shipsystems] ) );
    if ( number_percent( ) > chance )
    {
        send_to_char("&RYou're not sure which switch to flip.\n\r",ch);
        learn_from_failure( ch, gsn_shipsystems );
        return;
    }

    act( AT_PLAIN, "$n flips a switch on the control panel.", ch,
         NULL, argument , TO_ROOM );

    adjust_bay( target, FALSE);
    learn_from_success( ch, gsn_shipsystems );
}

void do_remoteopenbay( CHAR_DATA *ch, const char *argument )
{
    char arg[MAX_INPUT_LENGTH];
    int chance;
    SHIP_DATA *ship;
    SHIP_DATA *target;

    one_argument( argument, arg );
    if (arg[0] == '\0')
    {
        send_to_char("&RYou need to specify a target!\n\r",ch);
        return;
    }
    strcpy( arg, argument );    

    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
    {
        send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
        return;
    }

    target = get_ship_here( arg, ship->starsystem );
    if (  target == NULL )
    {
        send_to_char("&RThat ship isn't here!\n\r",ch);
        return;
    }

    if ( !check_pilot( ch , target ) && ship->owner[0] != '\0' )
    {
        send_to_char("&RHey, thats not your ship!!\n\r",ch);
        return;
    }

    if(target->ship_class == SHIP_PLATFORM) 
    {
       send_to_char("You can't open or close the bay doors on that.\n\r", ch);
       return;
    }

    if( target->bayopen ) 
    {
       send_to_char("The bays on that ship are already open.\n\r", ch );
       return;
    }
    chance = IS_NPC(ch) ? ch->top_level   
                        : (int)  (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_shipsystems] ) );
    if ( number_percent( ) > chance )
    {
        send_to_char("&RYou're not sure which switch to flip.\n\r",ch);
        learn_from_failure( ch, gsn_shipsystems );
        return;
    }   
           
    act( AT_PLAIN, "$n flips a switch on the control panel.", ch,
         NULL, argument , TO_ROOM );
        
    adjust_bay( target, TRUE );
    learn_from_success( ch, gsn_shipsystems );
}

void do_sabotage( CHAR_DATA * ch, const char *argument )
{
   char arg[MAX_INPUT_LENGTH];
   SHIP_DATA *ship;
   int chance;
    switch( ch->substate )
    {
       default:
       if (  (ship = ship_from_engine(ch->in_room->vnum))  == NULL )
       {
          send_to_char("&RYou must be in the engine room of a ship to sabotage it\n\r", ch);
          return;
       }

   if ( str_cmp( argument , "lasers" ) && str_cmp( argument , "drive" ) &&
        str_cmp( argument , "launcher" ) && str_cmp( argument , "ions" ) &&
        str_cmp( argument , "turret1" ) && str_cmp( argument , "turret2") && 
        str_cmp( argument , "rlauncher" ) && str_cmp( argument , "tlauncher"))
   {
      send_to_char("Syntax: sabotage <lasers/ions/drive/turret1/turret2/launcher/tlauncher/rlauncher>", ch);
      return;
   }
   strcpy(arg, argument);
   if(!str_cmp("public", ship->owner))
   {
      send_to_char("It's not worth my time.\n\r", ch);
      return;
   }
   if(!str_cmp(argument, "lasers"))
   {
      if(IS_SET(ship->flags, SHIPFLAG_SABOTAGEDLASERS))
      {
         send_to_char("Lasers are already sabotaged.\n\r", ch);
         return;
      }
      if(!ship->lasers)
      {
         send_to_char("That ship doesn't have any lasers to sabotage.\n\r", ch);
         return;
      }
   }
   else if(!str_cmp(argument, "ions"))
   {
      if(IS_SET(ship->flags, SHIPFLAG_SABOTAGEDIONS))
      {
         send_to_char("Ions are already sabotaged.\n\r", ch);
         return;
      }
      if(!ship->ions)
      {
         send_to_char("That ship doesn't have any ions to sabotage.\n\r", ch);
         return;
      }
   }
   else if(!str_cmp(argument, "drive"))
   {
      if(IS_SET(ship->flags, SHIPFLAG_SABOTAGEDENGINE))
      {
         send_to_char("Drive is already sabotaged.\n\r", ch);
         return;
      }
   }
   else if(!str_cmp(argument, "turret1"))
   {
       if(IS_SET(ship->flags, SHIPFLAG_SABOTAGEDTURRET1))
       {
          send_to_char("Turret 1 is already sabotaged.\n\r", ch);
          return;
       }
       if(!ship->turret1)
       {
          send_to_char("That ship doesn't have a turret.\n\r", ch);
          return;
       }
   }
   else if(!str_cmp(argument, "turret2"))
   {
      if(IS_SET(ship->flags, SHIPFLAG_SABOTAGEDTURRET2))
      {
         send_to_char("Turret 2 is already sabotaged.\n\r", ch);
         return;
      }
      if(!ship->turret2)
      {
         send_to_char("That ship doesn't have a second turret.\n\r", ch);
         return;
      }
   }
   else if(!str_cmp(argument, "launcher"))
   {
      if(IS_SET(ship->flags, SHIPFLAG_SABOTAGEDLAUNCHERS))
      {
         send_to_char("Launchers are already sabotaged.\n\r", ch);
         return;
      }
      if(!ship->mlaunchers)
      { 
         send_to_char("That ship doesn't have a missile launcher.\n\r", ch);
         return;
      }
   }
   else if(!str_cmp(argument, "tlauncher"))
   {
     if(IS_SET(ship->flags, SHIPFLAG_SABOTAGEDTLAUNCHERS))
     {
        send_to_char("Torpedo Launchers are already sabotaged.\n\r", ch);
        return;
     }
     if(!ship->tlaunchers)
     {
        send_to_char("That ship dosen't have a torpedo launcher.\n\r", ch);
        return;
     }
   }
   else if(!str_cmp(argument, "rlauncher"))
   {
     if(IS_SET(ship->flags, SHIPFLAG_SABOTAGEDRLAUNCHERS))
     {
        send_to_char("Rocket Launchers are already sabotaged.\n\r", ch);
        return;
     }
     if(!ship->rlaunchers)
     {
        send_to_char("That ship doesn't have rocket launchers.\n\r", ch);
        return;
     }
   }
   else
   {
      send_to_char("Syntax: sabotage <lasers/ions/drive/turret1/turret2/launcher/rlauncher/tlauncher>\n\r", ch);
      return;
   }
   chance = IS_NPC(ch) ? ch->top_level
            : (int) (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_sabotage] ) );
   if ( number_percent( ) < chance )
   {
      send_to_char("&GYou begin sabotaging the craft.\n\r", ch);
      if(IS_SET(ship->flags, SHIPFLAG_SIMULATOR))
      {
         send_to_char("You realize its a simulator and and begin to upload a virus, while playing pong.\n\r", ch);
      }
      act( AT_PLAIN, "$n begins fiddling around with the ships $T.", ch,
         NULL, argument , TO_ROOM );
      add_timer ( ch , TIMER_DO_FUN , 5 , do_sabotage , 1 );
      ch->dest_buf = str_dup(arg);
      return;
   }
   send_to_char("You aren't quite sure how to sabotage it.\n\r", ch);
   return;
        case 1:
                if ( !ch->dest_buf )
                   return;
                strlcpy( arg, (const char *)ch->dest_buf, MAX_INPUT_LENGTH );
                DISPOSE( ch->dest_buf);
                break;
        case SUB_TIMER_DO_ABORT:
                DISPOSE( ch->dest_buf );
                ch->substate = SUB_NONE;
                if ( (ship = ship_from_cockpit(ch->in_room->vnum)) == NULL )
                      return;                                                  
                send_to_char("&RYou are distracted and fail to finish your sabotage efforts.\n\r", ch);
                return;
       }
    ch->substate = SUB_NONE;
    if ( (ship = ship_from_engine(ch->in_room->vnum)) == NULL )
    {
       return;
    }
   chance = IS_NPC(ch) ? ch->top_level
            : (int) (( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_sabotage] ));
   if ( number_percent( ) > chance )
   {
      send_to_char( "&RYou fail to properly sabotage it.\n\r", ch);
      learn_from_failure( ch, gsn_sabotage );
      act( AT_PLAIN, "$n finishes fiddling with something.", ch, NULL, argument, TO_ROOM);
      return;
   }
   send_to_char("&GYou finish sabotaging it, and conceal your efforts.\n\r", ch);
   learn_from_success(ch, gsn_sabotage);
   act( AT_PLAIN, "$n finishes fiddling with something.", ch,
        NULL, argument , TO_ROOM );
   if(!str_cmp(arg, "lasers"))
      SET_BIT(ship->flags, SHIPFLAG_SABOTAGEDLASERS);
   if(!str_cmp(arg, "ions"))
      SET_BIT(ship->flags, SHIPFLAG_SABOTAGEDIONS);
   if(!str_cmp(arg, "drive"))
      SET_BIT(ship->flags, SHIPFLAG_SABOTAGEDENGINE);
   if(!str_cmp(arg, "turret1"))
      SET_BIT(ship->flags, SHIPFLAG_SABOTAGEDTURRET1);
   if(!str_cmp(arg, "turret2"))
      SET_BIT(ship->flags, SHIPFLAG_SABOTAGEDTURRET2);
   if(!str_cmp(arg, "launcher"))
      SET_BIT(ship->flags, SHIPFLAG_SABOTAGEDLAUNCHERS);
   if(!str_cmp(arg, "tlauncher"))
      SET_BIT(ship->flags, SHIPFLAG_SABOTAGEDTLAUNCHERS);
   if(!str_cmp(arg, "rlauncher"))
      SET_BIT(ship->flags, SHIPFLAG_SABOTAGEDRLAUNCHERS);
   return;    
}

void do_selfdestruct(CHAR_DATA *ch, const char *argument )
{
    SHIP_DATA *ship;
    char buf[MAX_INPUT_LENGTH];
        
    if (  (ship = ship_from_cockpit(ch->in_room->vnum) ) == NULL )
    {
        send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
        return;
    }

    if (  (ship = ship_from_pilotseat(ch->in_room->vnum))  == NULL )
    {
        send_to_char("&RYou must be in the pilots seat!\n\r",ch);
        return;
    }
    if (  (ship->autopilot == TRUE ) )
    {
        send_to_char("&RYou are locked out of this control with the autopilot enabled.\n\r", ch);
        return;
    }
    if( ship->sdestnum >= 0 )
    {
        send_to_char("Self destruct sequence has already been activated.\n\r", ch);
        return;
    }
    act( AT_PLAIN, "$n lifts a guard panel on the control deck, entering a sequence on a small keypad.", ch,
         NULL, argument , TO_ROOM );
    initialize_code( buf );
    ch->dest_buf = str_dup( buf );
    ch_printf( ch, "You lift a small console guard and begin keying the self-destruct sequence.\n\r" );
    ch_printf( ch, "To override the manual defense input the following: %s\n\r", buf );
    echo_to_ship(AT_DANGER, ship, "The alarm system drones in the strobing emergency signal throughout the ship.\n\r");
    ch->desc->connected = CON_SELF_DESTRUCT;
    ch->substate = SUB_SELF_DESTRUCT_1;
    return;
}

void do_togglesimulator(CHAR_DATA *ch, const char *argument )
{
    int chance;
    SHIP_DATA *ship;
    int mode = 0; /* 0 for off, 1 for on */

    if (  (ship = ship_from_cockpit(ch->in_room->vnum))  == NULL )
    {
       send_to_char("&RYou must be in the cockpit of a ship to do that!\n\r",ch);
       return;
    }

    if( !str_cmp( ship->owner, "Public" ) )
    {
       send_to_char( "&RPublic craft do not support swapping modes.\n\r", ch );
       return;
    }

    if (ship->shipstate != SHIP_DOCKED)
    {
       send_to_char("&RYou can only enter simulation mode when your craft is on the ground.\n\r", ch );
       return;
    }

    if ( !str_cmp(argument,"on" ) )
    {
        if( IS_SET( ship->flags, SHIPFLAG_SIMULATOR ) )
        {
           send_to_char( "&RThe craft is already in simulation mode.\n\r", ch );
           return;
        }
        mode = 1;
    }
    else if ( !str_cmp(argument,"off" ) )
    {
        if( !IS_SET( ship->flags, SHIPFLAG_SIMULATOR ) )
        {
           send_to_char( "&RThe simulation mode is already off.\n\r", ch );
           return;
        }
        mode = 0;
    }
    else /* Toggle */
    {
        if( IS_SET( ship->flags, SHIPFLAG_SIMULATOR ) )
           mode = 0;
        else
           mode = 1;
    }

    chance = IS_NPC(ch) ? ch->top_level
           : (int)  ( ( IS_NPC( ch ) ? ch->top_level : ch->pcdata->learned[gsn_shipsystems] ));
    if ( number_percent( ) > chance )
    {
       send_to_char("&RYou fail to work the controls properly.\n\r",ch);
       learn_from_failure( ch, gsn_shipsystems );
       return;
    }

    act( AT_PLAIN, "$n flips a switch on the control panel.", ch,
         NULL, argument , TO_ROOM );

    if( mode == 1 )
    {
        send_to_char( "&GShip simulation mode ON!\n\r", ch );
        echo_to_cockpit( AT_YELLOW, ship, "Simulation mode ON." );
        SET_BIT( ship->flags, SHIPFLAG_SIMULATOR );
    }
    else
    {
        send_to_char( "&GShip simulation mode OFF!\n\r", ch );
        echo_to_cockpit( AT_YELLOW, ship, "Simulation mode OFF." );
        REMOVE_BIT( ship->flags, SHIPFLAG_SIMULATOR );
    }
    learn_from_success( ch, gsn_shipsystems );
    return;
}

/*
 * do_endsimulator - v1.19, ported from SWGD (written by Ackbar, added by
 * Arcturus per the original source comment). This is the actual exit
 * mechanism for simulator mode - without it, a ship that launches into
 * the Simulator starsystem (per last session's launchship fix) has no
 * way back out, since do_plot explicitly blocks jump-plotting while
 * SHIPFLAG_SIMULATOR is set and the Simulator zone has no real docks to
 * land at normally. Pulls the ship out of the simulator, returns it to
 * its last real dock, fully repairs/refuels/rearms it via prepship(),
 * and opens the hatch.
 */
void do_endsimulator( CHAR_DATA * ch, const char *argument )
{
   SHIP_DATA *ship;
   char buf[MAX_INPUT_LENGTH];

   if( ( ship = ship_from_cockpit( ch->in_room->vnum ) ) == NULL )
   {
      send_to_char( "You must be in the cockpit of a simulator.\r\n", ch );
      return;
   }

   if( !IS_SET( ship->flags, SHIPFLAG_SIMULATOR ) )
   {
      send_to_char( "You must be in the cockpit of a simulator.\r\n", ch );
      return;
   }

   ship->shipstate = SHIP_READY;
   extract_ship( ship );
   ship_to_room( ship, ship->lastdoc );
   ship->location = ship->lastdoc;
   ship->shipstate = SHIP_DOCKED;
   if( ship->starsystem )
      ship_from_starsystem( ship, ship->starsystem );
   prepship( ship );
   save_ship( ship );
   ship->hatchopen = TRUE;

   ship->hx = 0;
   ship->hy = 0;
   ship->hz = 0;

   ship->vx = 0;
   ship->vy = 0;
   ship->vz = 0;

   send_to_char( "The lights dim and the hatch opens.\r\n", ch );
   snprintf( buf, MAX_INPUT_LENGTH, "%s suddenly disappears from your viewscreen and off your radar.\r\n",
             ship->name );
   echo_to_system( AT_WHITE, ship, buf, NULL );
}

void do_transship( CHAR_DATA *ch ,const char *argument )
{
    char arg1[MAX_INPUT_LENGTH];
        char arg2[MAX_INPUT_LENGTH];
        int arg3;
    SHIP_DATA *ship;

    if ( IS_NPC( ch ) )
    {
        send_to_char( "Huh?\n\r", ch );
        return;
    }

    argument = one_argument( argument, arg1 );
    argument = one_argument( argument, arg2 );

    ship = get_ship( arg1 );
        if ( !ship )
    {
        send_to_char( "No such ship.\n\r", ch );
        return;
    }
     
        arg3 = atoi( arg2 );
     
         if ( arg1[0] == '\0' || arg2[0] == '\0' || arg1[0] == '\0' )
   {
        send_to_char( "Usage: transship <ship> <vnum>\n\r", ch );
        return;
    }
     arg3 = atoi( arg2 );

     
     if ( ship->ship_class != SHIP_PLATFORM && ship->type != MOB_SHIP )   
     {
           extract_ship( ship );
           if( !ship_to_room( ship , arg3 ) )
           {
              send_to_char("Transfer failed.\n\r", ch );
              return;
           }
           if( ship->starsystem )
              ship_from_starsystem( ship, ship->starsystem );
           ship->shipstate = SHIP_DOCKED;
     }           
     ship->autopilot = FALSE;
     save_ship(ship);
     send_to_char( "Ship Transfered.\n\r", ch );
}

