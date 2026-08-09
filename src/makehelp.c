/***************************************************************************
 * makehelp.c - Web help export for swrfussSWL
 *
 * Ported and adapted from Star Wars Galactic Dominion (SWGD).
 * Generates a static HTML help file database at HELP_WEB_DIR.
 *
 * Usage (in-game): makehelp
 * Requires: ../public_html/helps/ directory to exist and be writable.
 *
 ***************************************************************************/

#include <sys/types.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "mud.h"

/* Used by html_colour/html_colourconv to track open <b> tags */
static bool tagged = FALSE;

/*
 * Convert MUD colour code character to an HTML <b class="..."> span.
 * Returns the number of characters written to string.
 * Ported from GD webwho.c - html_colour()
 */
int html_colour( char type, char *string )
{
   char code[256];
   char *p;
   char colorcode[17] = "xbcgprwOBCGPRWYz";
   char closed_tag[8];

   if( type == '.' )
      type = colorcode[number_range( 0, 15 )];

   if( tagged )
      snprintf( closed_tag, sizeof( closed_tag ), "</b>" );
   else
      closed_tag[0] = '\0';

   if( !tagged )
      tagged = TRUE;

   switch( type )
   {
      default:
      case '\0': code[0] = '\0';                                              break;
      case ' ':  snprintf( code, sizeof(code), " " );                         break;
      case 'x':  snprintf( code, sizeof(code), "%s<b class=\"black\">",   closed_tag ); break;
      case 'b':  snprintf( code, sizeof(code), "%s<b class=\"blue\">",    closed_tag ); break;
      case 'c':  snprintf( code, sizeof(code), "%s<b class=\"cyan\">",    closed_tag ); break;
      case 'g':  snprintf( code, sizeof(code), "%s<b class=\"green\">",   closed_tag ); break;
      case 'p':  snprintf( code, sizeof(code), "%s<b class=\"magenta\">", closed_tag ); break;
      case 'r':  snprintf( code, sizeof(code), "%s<b class=\"red\">",     closed_tag ); break;
      case 'w':  snprintf( code, sizeof(code), "%s<b class=\"white\">",   closed_tag ); break;
      case 'O':  snprintf( code, sizeof(code), "%s<b class=\"brown\">",   closed_tag ); break;
      case 'B':  snprintf( code, sizeof(code), "%s<b class=\"b-blue\">",  closed_tag ); break;
      case 'C':  snprintf( code, sizeof(code), "%s<b class=\"b-cyan\">",  closed_tag ); break;
      case 'G':  snprintf( code, sizeof(code), "%s<b class=\"b-green\">", closed_tag ); break;
      case 'P':  snprintf( code, sizeof(code), "%s<b class=\"b-magenta\">", closed_tag ); break;
      case 'R':  snprintf( code, sizeof(code), "%s<b class=\"b-red\">",   closed_tag ); break;
      case 'W':  snprintf( code, sizeof(code), "%s<b class=\"b-white\">", closed_tag ); break;
      case 'Y':  snprintf( code, sizeof(code), "%s<b class=\"yellow\">",  closed_tag ); break;
      case 'z':  snprintf( code, sizeof(code), "%s<b class=\"b-black\">", closed_tag ); break;
      case '&':  snprintf( code, sizeof(code), "&amp;" );                   break;
   }

   p = code;
   while( *p != '\0' )
   {
      *string   = *p++;
      *++string = '\0';
   }
   return( strlen( code ) );
}

/*
 * Convert MUD colour codes in text to HTML <b class="..."> tags.
 * Also escapes < and > to prevent HTML injection.
 * Ported from GD webwho.c - html_colourconv()
 */
void html_colourconv( char *buffer, const char *txt, CHAR_DATA *ch )
{
   const char *point;
   int skip = 0;

   tagged = FALSE;

   for( point = txt; *point; point++ )
   {
      if( *point == '&' )
      {
         point++;
         if( *point == '\0' )
         {
            point--;
            continue;
         }
         skip = html_colour( *point, buffer );
         while( skip-- > 0 )
            ++buffer;
         continue;
      }
      /* Escape < and > to prevent HTML breakage */
      if( *point == '<' )
      {
         *buffer   = '[';
         *++buffer = '\0';
         continue;
      }
      if( *point == '>' )
      {
         *buffer   = ']';
         *++buffer = '\0';
         continue;
      }
      *buffer   = *point;
      *++buffer = '\0';
   }

   /* Close any open <b> tag */
   if( tagged )
   {
      tagged    = FALSE;
      *buffer   = '<';
      *++buffer = '/';
      *++buffer = 'b';
      *++buffer = '>';
      *++buffer = '\0';
   }
   *buffer = '\0';
}

/*
 * Convert MUD line endings (\r\n) to bare \n for clean HTML <pre> output.
 * Ported from GD makehelp.c - fix_linebreaks()
 */
void fix_linebreaks( char *buffer, const char *txt )
{
   const char *i;

   for( i = txt; *i; i++ )
   {
      if( *i == '\r' )
         continue;
      *buffer   = *i;
      *++buffer = '\0';
   }
   *buffer = '\0';
}

/*
 * Strip MUD colour codes from a string, escaping bare & as &amp;
 * Used to sanitize help keywords for use as HTML text/IDs.
 * Ported from GD makehelp.c - fix_amperstands()
 */
void fix_amperstands( char *buffer, const char *txt )
{
   const char *i;

   for( i = txt; *i; i++ )
   {
      if( *i == '&' )
      {
         i++;   /* skip the colour code character */
         if( *i == '&' )  /* literal && -> &amp; */
         {
            *buffer   = '&';
            *++buffer = 'a';
            *++buffer = 'm';
            *++buffer = 'p';
            *++buffer = ';';
            *++buffer = '\0';
         }
         /* any other &X colour code is simply dropped */
         continue;
      }
      *buffer   = *i;
      *++buffer = '\0';
   }
   *buffer = '\0';
}

/*
 * Helper: write a standard HTML page header to fp.
 * mud_name is used as the page title prefix.
 */
static void write_html_header( FILE *fp, const char *page_title )
{
   fprintf( fp, "<!DOCTYPE html>\n<html lang=\"en\">\n" );
   fprintf( fp, "<head>\n" );
   fprintf( fp, "  <meta charset=\"UTF-8\" />\n" );
   fprintf( fp, "  <meta name=\"generator\" content=\"swrfussSWL makehelp\" />\n" );
   fprintf( fp, "  <title>%s</title>\n", page_title );
   fprintf( fp, "  <link href=\"../style.css\" rel=\"stylesheet\" type=\"text/css\" />\n" );
   fprintf( fp, "</head>\n<body>\n" );
}

static void write_html_footer( FILE *fp )
{
   fprintf( fp, "</body>\n</html>\n" );
}

/*
 * do_makehelp - generate static HTML helpfile database
 *
 * Creates:
 *   HELP_WEB_DIR/index.html   - table of all public help entries
 *   HELP_WEB_DIR/N.html       - one page per help entry
 *
 * The target directory (HELP_WEB_DIR) must exist and be writable.
 * Immortal-only help entries (level > LEVEL_AVATAR) are excluded.
 */
void do_makehelp( CHAR_DATA *ch, const char *argument )
{
   FILE      *fp;
   HELP_DATA *pHelp;
   char       buf[MAX_STRING_LENGTH];
   char       buf2[MAX_STRING_LENGTH * 2];
   char       buf3[MAX_STRING_LENGTH * 4];
   char       keyword[MAX_STRING_LENGTH];
   char       filepath[256];
   int        number = 0;

   /* Release the reserved handle so we can open files */
   fclose( fpReserve );

   /* --- Build index.html --- */
   snprintf( filepath, sizeof( filepath ), "%sindex.html", HELP_WEB_DIR );
   if( ( fp = fopen( filepath, "w" ) ) == NULL )
   {
      bug( "%s: fopen index.html failed", __func__ );
      perror( filepath );
      fpReserve = fopen( NULL_FILE, "r" );
      return;
   }

   write_html_header( fp, "Galactic Dominion: Rebirth — Helpfile Database" );
   fprintf( fp, "<h1>Galactic Dominion: Rebirth &mdash; Helpfiles</h1>\n" );
   fprintf( fp, "<p>Last updated: %s</p>\n", ctime( &current_time ) );
   fprintf( fp, "<table border=\"1\" cellpadding=\"4\">\n" );
   fprintf( fp, "<tr><th>Level</th><th>Keyword</th></tr>\n" );

   for( pHelp = first_help; pHelp; pHelp = pHelp->next )
   {
      if( pHelp->level > LEVEL_AVATAR )
         continue;
      fix_amperstands( keyword, pHelp->keyword );
      number++;
      fprintf( fp, "<tr id=\"%d\"><td>%d</td><td><a href=\"%d.html\">%s</a></td></tr>\n",
               number, pHelp->level, number, keyword );
   }

   fprintf( fp, "</table>\n" );
   write_html_footer( fp );
   fclose( fp );

   /* --- Build individual help pages --- */
   number = 0;
   for( pHelp = first_help; pHelp; pHelp = pHelp->next )
   {
      if( pHelp->level > LEVEL_AVATAR )
         continue;
      number++;

      snprintf( filepath, sizeof( filepath ), "%s%d.html", HELP_WEB_DIR, number );
      if( ( fp = fopen( filepath, "w" ) ) == NULL )
      {
         bug( "%s: fopen %s failed", __func__, filepath );
         continue;
      }

      fix_amperstands( keyword, pHelp->keyword );

      snprintf( buf, sizeof( buf ), "GDR Help: %.4080s", keyword );
      write_html_header( fp, buf );

      fprintf( fp, "<h1>%s</h1>\n", keyword );
      fprintf( fp, "<pre>\n" );

      html_colourconv( buf2, pHelp->text ? pHelp->text : "", ch );
      fix_linebreaks( buf3, buf2 );
      fprintf( fp, "%s", buf3 );

      fprintf( fp, "</pre>\n" );
      fprintf( fp, "<p><a href=\"index.html#%d\">Back to index</a></p>\n", number );
      write_html_footer( fp );
      fclose( fp );
   }

   /* Re-open the reserved handle */
   fpReserve = fopen( NULL_FILE, "r" );

   ch_printf( ch, "Done. %d help entries exported to %s\r\n", number, HELP_WEB_DIR );
}
