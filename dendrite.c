/*
 * "Copyright (c) 2000 by Matt Massie and The Regents of the University
 * of California.  All rights reserved."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES ARISING OUT
 * OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF
 * CALIFORNIA HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */                               

/* $Id: dendrite.c,v 1.3 2000/12/19 02:28:37 massie Exp massie $ */

#include "ganglia.h"

#define ERROR_FILE        "/tmp/dendrite_messages"
#define LOCK_FILE         "/tmp/dendrite.lock"
#define PID_FILE          "/var/run/dendrite.pid"

#define LOAD_THRESHOLD   10   /* 100th of a load */
#define MEM_THRESHOLD    1024 /* Kb */
#define CPU_THRESHOLD    10   /* 10th of a % */

#define BUFFER_SIZE 4096
char   buffer[BUFFER_SIZE];

int    i, pid_file, error_file, multicast_handle;
struct sockaddr_in addr;
time_t last_update = 0;

struct node_state now;
struct node_state then;
struct node_state msg;

/* Function definitions */
void daemonize (void);
void connect_multicast (void);
void multicast_node_state (void);
void process_cpuinfo (void);
void process_meminfo (void);
void process_loadstate(void);
void process_memstate(void);
void process_cpustate(void);
char * skip_whitespace ( const char *p);
char * skip_token ( const char *p);
int  slurpfile ( char * filename );
int  fresh_data ( void );

int main( int argc, char ** argv )
{
   memset(&now,  0, sizeof(now ));
   memset(&then, 0, sizeof(then));   
   memset(&msg,  0, sizeof(msg));

   daemonize();
   connect_multicast(); 
   process_cpuinfo();
   process_meminfo();

   for ( ;; ){
      process_loadstate();
      process_memstate();
      process_cpustate();
      if ( fresh_data() )
           multicast_node_state();
      memcpy( &then, &now, sizeof(now) ); 
      sleep( SECONDS_TO_SLEEP ); 
   } 
}

int fresh_data ( void ) 
{
    
   if(  (time(NULL) - last_update) > 
        SECONDS_TO_FORCE )
        return 1;

   if(  abs( now.load_one - then.load_one )  > 
        LOAD_THRESHOLD )
        return 1;

   if(  abs( now.cpu_idle - then.cpu_idle ) >
        CPU_THRESHOLD )
        return 1;

   if(  abs( ( now.mem_free +  now.mem_buffers) -
             (then.mem_free + then.mem_buffers) ) >
        MEM_THRESHOLD )
        return 1;

   return 0;
}

void process_cpustate ( void ) 
{
   char *p;
   static double cpu_user, cpu_nice, cpu_system, cpu_idle;
   double d;
   double user_jiffies, nice_jiffies, system_jiffies,
          idle_jiffies, relative_jiffies, absolute_jiffies;

   if (! slurpfile ( "/proc/stat" )){
      return;
   }

   p = skip_token ( buffer );
   p = skip_whitespace ( p );

   d            = strtod( p, (char **)NULL ); 
   user_jiffies = d - cpu_user;
   cpu_user     = d;

   p = skip_token ( p );
   p = skip_whitespace ( p );

   d            = strtod( p, (char **)NULL );
   nice_jiffies = d - cpu_nice;
   cpu_nice     = d;

   p = skip_token ( p );
   p = skip_whitespace ( p );
 
   d              = strtod( p, (char **)NULL );
   system_jiffies = d - cpu_system;
   cpu_system     = d;

   p = skip_token ( p );
   p = skip_whitespace ( p );

   d            = strtod( p, (char **)NULL );
   idle_jiffies = d - cpu_idle;
   cpu_idle     = d;

   relative_jiffies = user_jiffies   + nice_jiffies +
                      system_jiffies + idle_jiffies;
 
   absolute_jiffies = cpu_user + cpu_nice +
                      cpu_system + cpu_idle;

   now.cpu_user     = (user_jiffies   /relative_jiffies)*1000.00;
   now.cpu_nice     = (nice_jiffies   /relative_jiffies)*1000.00;
   now.cpu_system   = (system_jiffies /relative_jiffies)*1000.00;
   now.cpu_idle     = (idle_jiffies   /relative_jiffies)*1000.00;
   now.cpu_aidle    = (cpu_idle       /absolute_jiffies)*1000.00;   

}

void process_memstate ( void ) 
{
   char *p;

   if ( ! slurpfile( "/proc/meminfo" ) ){
      return;
   }

   p = strstr( buffer, "MemFree:" );
   p = skip_token(p);
   p = skip_whitespace(p);

   now.mem_free  = strtod( p, (char **)NULL);

   p = strstr( buffer, "MemShared:" );
   p = skip_token(p);
   p = skip_whitespace(p);
   
   now.mem_shared = strtod( p, (char **)NULL);

   p = strstr( buffer, "Buffers:" );
   p = skip_token(p);
   p = skip_whitespace(p);

   now.mem_buffers = strtod( p, (char **)NULL);

   p = strstr( buffer, "Cached:" );
   p = skip_token(p);
   p = skip_whitespace(p);

   now.mem_cached = strtod( p, (char **)NULL);

   p = strstr( buffer, "SwapFree:" );
   p = skip_token(p);
   p = skip_whitespace(p);

   now.swap_free = strtod( p, (char **)NULL );

}

void process_loadstate ( void ) 
{
   char *p, **endptr;

   if ( ! slurpfile ( "/proc/loadavg" )){
      return;
   }
   p = buffer;

   now.load_one     = (short)( strtod(p, (char **)NULL) * 100.00 );

   p = skip_token(p);
   p = skip_whitespace(p);
   now.load_five    = (short)( strtod(p, (char **)NULL) * 100.00 );

   p = skip_token(p);
   p = skip_whitespace(p);
   now.load_fifteen = (short)( strtod(p, (char **)NULL) * 100.00 );

   p = skip_token(p);
   p = skip_whitespace(p);

   now.proc_run     = (short)( strtod(p, (char **)NULL) );

   while (isdigit(*p)) p++;
   p++;  /* Skip the slash */

   now.proc_total   = (short)( strtod(p, (char **)NULL) );

}

void process_meminfo ( void ) 
{
   char *p;

   if ( ! slurpfile ( "/proc/meminfo" )){
      return;
   }

   p = strstr( buffer, "MemTotal:" );
   p = skip_token(p);
   p = skip_whitespace(p);
  
   now.mem_total  = strtol( p, (char **)NULL, 10 );
 
   p = strstr( buffer, "SwapTotal:" );
   p = skip_token(p);
   p = skip_whitespace(p);

   now.swap_total = strtol( p, (char **)NULL, 10 );  

}

void process_cpuinfo ( void ) 
{
   char *p; 

   if ( ! slurpfile ( "/proc/cpuinfo" )) {
      return;
   }

   /* Count the number of processors */
   for ( p = buffer ; *p != '\0' ; p++ ){
      if (! strncmp( p, "processor", 9) ){
         now.cpu_num++;
      } 
   }

   p = buffer;
   p = strstr( p, "cpu MHz" );
   p = strchr( p, ':' );
   p++;
   p = skip_whitespace(p);
   now.cpu_speed = strtol( p, (char **)NULL , 10 ); 

}

int slurpfile ( char * filename ) 
{
   int  fd, length;

   if ( ( fd = open(filename, O_RDONLY) ) == -1 ){
      sprintf( buffer, "open %s", filename );
      perror( buffer );
      return 0;
   }

   if ( ( length = read( fd, buffer, BUFFER_SIZE-1)) == -1 ) {
      sprintf( buffer, "read %s", filename );
      perror( buffer );
      return 0;
   }

   close(fd);
   buffer[length]='\0';

   return length;
}

void multicast_node_state ( void ) 
{
   /* Convert now to network byte order before sending */
   msg.cpu_num      = htons ( now.cpu_num );
   msg.cpu_speed    = htons ( now.cpu_speed );
   msg.cpu_user     = htons ( now.cpu_user );
   msg.cpu_nice     = htons ( now.cpu_nice );
   msg.cpu_system   = htons ( now.cpu_system );
   msg.cpu_idle     = htons ( now.cpu_idle );
   msg.cpu_aidle    = htons ( now.cpu_aidle );
   msg.load_one     = htons ( now.load_one );
   msg.load_five    = htons ( now.load_five );
   msg.load_fifteen = htons ( now.load_fifteen );
   msg.proc_run     = htons ( now.proc_run );
   msg.proc_total   = htons ( now.proc_total );
   msg.mem_total    = htonl ( now.mem_total );
   msg.mem_free     = htonl ( now.mem_free );
   msg.mem_shared   = htonl ( now.mem_shared );
   msg.mem_buffers  = htonl ( now.mem_buffers );
   msg.mem_cached   = htonl ( now.mem_cached );
   msg.swap_total   = htonl ( now.swap_total );
   msg.swap_free    = htonl ( now.swap_free );

   if ( sendto( multicast_handle, (void *)&msg, sizeof( msg ),
                0, (struct sockaddr *)&addr, sizeof( addr )) == -1 ){
      perror("sendto");
      exit(errno);
   }

   if( (last_update = time(NULL)) == -1 ){
      perror("time");
      exit(errno);
   }

}



void connect_multicast( void ) 
{
   memset(&addr, 0, sizeof(addr));
   addr.sin_family      = AF_INET;
   addr.sin_addr.s_addr = inet_addr( GANGLIA_CHANNEL );
   addr.sin_port        = htons    ( GANGLIA_PORT    );

   if ((multicast_handle = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ){
       perror( "socket");
       exit(errno);
   }


} /* end connect_multicast */


void daemonize (void) 
{
   int lock_file;
   pid_t pid;
 
   if ( (lock_file = open ( LOCK_FILE, O_CREAT|O_RDWR, 0600 )) == -1 ){
      sprintf( buffer, "open on %s", LOCK_FILE );
      perror ( buffer );
      exit(errno);
   }
 
   if ( flock( lock_file, LOCK_EX | LOCK_NB ) ) 
      exit(errno);
 
   if( chdir( "/" ) ) {
      perror ("chdir to /");
      exit(errno);
   }        

   if( (error_file = open (ERROR_FILE, O_CREAT|O_RDWR|O_TRUNC, 0644 )) == -1 ){
      sprintf( buffer, "open on %s", ERROR_FILE );
      perror ( buffer );
      exit(errno);
   } 

   if( dup2( error_file, STDERR_FILENO ) == -1 ){
      sprintf( buffer, "duping stderr to %s", ERROR_FILE );
      perror ( buffer );
      exit(errno);
   }

   if ( ( pid = fork() ) == -1 ){
      perror ( "Erroring forking" );
      exit(errno);
   }

   if ( pid ) exit(0);

   if( (pid = setsid()) == -1 ){
      perror ( "setsid" );
      exit(errno);
   }   

   if( (pid_file = open ( PID_FILE, O_CREAT|O_RDWR|O_TRUNC, 0644 )) == -1){
      sprintf( buffer, "open on %s", PID_FILE );
      perror ( buffer );
      exit(errno);
   }   

   sprintf( buffer, "%d", pid );

   if( (write( pid_file, buffer, strlen(buffer) )) == -1 ){
      sprintf( buffer, "write to %s", PID_FILE); 
      perror ( buffer );
      exit(errno);
   }
   
}

char * skip_whitespace ( const char *p)
{
    while (isspace(*p)) p++;
    return (char *)p;
}

char * skip_token ( const char *p) 
{
    while (isspace(*p)) p++;
    while (*p && !isspace(*p)) p++;
    return (char *)p;

}

