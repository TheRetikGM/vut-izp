#include <stdio.h>
#include <stdlib.h>

/**
 * Count up the number of total lines, single-comment lines,
 * and multi-comment lines in a file.
 */
int main(int argc, char **argv)
{
  FILE *fp;
  int c;
  unsigned int chars  = 0;
  unsigned int total  = 0;
  unsigned int multi  = 0;
  unsigned int single = 0;

  enum states { TEXT, 
                SAW_SLASH, 
                SAW_STAR, 
                SINGLE_COMMENT, 
                MULTI_COMMENT } state = TEXT;

  if ( argc < 2 )
  {
    fprintf(stderr, "USAGE: %s <filename>\n", argv[0]);
    exit(0);
  }

  fp = fopen( argv[1], "r" );
  if ( !fp )
  {
    fprintf(stderr, "Cannot open file %s\n", argv[1] );
    exit(0);
  }

  while ( (c = fgetc( fp )) != EOF )
  {
    chars++;
    switch( state )
    {
      case TEXT :
        switch( c )
        {
          case '/'  : state = SAW_SLASH; break;
          case '\n' : total++; // fall-through
          default   : break;
        }
        break;

      case SAW_SLASH :
        switch( c )
        {
          case '/'  : state = SINGLE_COMMENT; break;
          case '*'  : state = MULTI_COMMENT; break;
          case '\n' : total++; // fall through
          default   : state = TEXT; break;
        }
        break;

      case SAW_STAR :
        switch( c )
        {
          case '/'  : state = TEXT; multi++; break;
          case '*'  : break;
          case '\n' : total++; multi++; // fall through
          default   : state = MULTI_COMMENT; break;
        }
        break;

      case SINGLE_COMMENT :
        switch( c )
        {
          case '\n' : state = TEXT; total++; single++; // fall through
          default   : break;
        }
        break;

      case MULTI_COMMENT :
        switch( c )
        {
          case '*'  : state = SAW_STAR; break;
          case '\n' : total++; multi++; // fall through
          default   : break;
        }
        break;

      default: // NOT REACHABLE
        break;
    }
  }

  fclose(fp);

  printf( "File                 : %s\n", argv[1] );
  printf( "Total lines          : %8u\n", total );
  printf( "Single-comment lines : %8u\n", single );
  printf( "Multi-comment lines  : %8u\n", multi );
  return 0;
}
