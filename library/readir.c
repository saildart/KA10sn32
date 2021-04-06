#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// The PDP-10 WAIT SIXBIT code for file names is UPPERCASE only alphanumeric with some punctuation
char *sixbit_table= " !\"" "#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ" "[\\]^_";

typedef unsigned long long uint64;
// Four PDP-10 word UFD block for building SAIL-WAITS directory in [1,1]<prj><prg>.UFD files
typedef struct UFDent{ 
  uint64 filnam:36,:28; // six uppercase FILNAM sixbit
  uint64 date_written_high:3, // hack-pack high-order 3 bits, overflowed 4 Jan 1975.
    creation_date:15,     // Right side
    ext:18,:28;           // Left side three uppercase .EXT extension
  uint64 date_written:12, // Right side
    time_written:11,
    mode:4,
    prot:9,:28; // Left side
  uint64 track:36,:28;
} UFDent_t;

int
main (void)
{
  int o;
  int i;
  char ppn_path[512];
  char ufd_path[512];
  char mfd_path[512];
  char *filnam,*ext;
  DIR *dp,*dp2;
  struct dirent *ep,*ep2;
  UFDent_t u;
  //  
  dp = opendir("/SYS/");
  while (dp && (ep = readdir (dp)))
    {
      if(index(ep->d_name,','))
        {
          sprintf( ppn_path, "/SYS/%s/", ep->d_name );
          sprintf( mfd_path, "/SYS/1.1/%s.UFD", ep->d_name );
          o = open( mfd_path, O_CREAT|O_TRUNC|O_WRONLY,0644 );
          puts( ppn_path );
          dp2 = opendir( ppn_path );
          while (dp2 && (ep2 = readdir (dp2)))
            {
              if(!( !strncmp(ep2->d_name,".",1) || !strncmp(ep2->d_name,"..",2) )){
                printf("%s%s\n",ppn_path,ep2->d_name);
                ext = ep2->d_name;
                filnam = strsep( &ext, "." );
                printf( "filnam='%s' ext='%s'\n", filnam, ext);
                bzero(&u,sizeof(u));
                if(filnam)
                  for(i=strlen(filnam);i;i--){
                    char c = filnam[i-1]; // right to left
                    char *q = index(sixbit_table,c);
                    uint64 qq=0;
                    if(q) qq = q - sixbit_table;
                    if(qq){
                      u.filnam >>= 6;             // right six   1x6
                      u.filnam |= ( qq << 30);    // left thirty 5x6
                      printf("%2d %c %2lld %03llo %06llo\n",i,c,qq,qq,(uint64)u.filnam);
                    }
                  }
                if(ext)
                  for(i=strlen(ext);i;i--){
                    char c = ext[i-1]; // right to left
                    char *q = index(sixbit_table,c);
                    uint64 qq=0;
                    if(q) qq = q - sixbit_table;
                    if(qq){
                      u.ext >>= 6;                // right six bits   1x6
                      u.ext |= ( qq << 12);       // left twelve bits 2x6
                      printf("%2d %c %2lld %03llo %03llo\n",i,c,qq,qq,(uint64)u.ext);
                    }
                  }
                write(o,&u,sizeof(u));
              }
            }
          close(o);
          (void)closedir(dp2);
        }
    }
  (void)closedir(dp);
  return 0;
}
