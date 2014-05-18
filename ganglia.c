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

/* $Id: ganglia.c,v 1.3 2000/12/19 02:28:00 massie Exp massie $ */

#include "ganglia.h"

struct protoent *proto;
struct sockaddr_in axon_addr;
struct query_msg query;
int discovery_ask_socket, discovery_tell_socket, ganglia_socket;
int token_number = 0;

/* FUNCTION PROTOS */
void parse_command_line ( int , char **, char ** );
void print_help (void);
void init_token_array ( void );
int find_axon_daemon(void) ;
void query_axon(void);
int setup_multicast_socket ( char * , unsigned short, int JOIN );

int main ( int argc, char ** argv, char ** env ) 
{
  char *p;

  if ( argc < 2 ) print_help();

  /* Set everthing in query to 0's */
  memset( &query, 0, sizeof( query )); 

  if ( (p = getenv ( "GANGLIA_MAX" )) != NULL )
       query.num_nodes =  atoi ( p );
 
  parse_command_line(argc, argv, env);
  
  if (! find_axon_daemon() ){
        fprintf(stderr, "Could not find an axon to query\n");
        exit(1);
  } 

  query_axon();
  
}

void query_axon (void) 
{
  int i;
  int axon_socket;
#define BUFFER_MAX 1024
  char buffer[BUFFER_MAX];
  char line[BUFFER_MAX];
  int retval;
  FILE *axon_stream;

  axon_addr.sin_port = htons( GANGLIA_PORT );

  if( (axon_socket = socket(PF_INET, SOCK_STREAM, 0) ) == -1 ){
     perror("axon socket error");
     exit(errno);
  }

  if( connect( axon_socket, (struct sockaddr *)&axon_addr, sizeof(axon_addr)) 
      == -1 ){
     perror("axon connect error");
     exit(errno);
  }

  /* Change query to network byte order */
  query.num_nodes = htons ( query.num_nodes );
  for ( i = 0; i < NUM_TOKENS ; i++ ){
     query.token_data[i].order = htons (query.token_data[i].order);
  }  

  /* Send the query */
  if( send(axon_socket,(void *)&query,(2 + sizeof(_token_pair)*token_number),0) 
      == -1 ){
     perror("axon send error");
     exit(errno);
  } 

  /* Set up the stream */
  if( (axon_stream = fdopen ( axon_socket, "r" )) == NULL ){
     perror("axon fdopen");
     exit(errno);
  }

  /* Use line buffering */
  if( setvbuf( axon_stream, NULL , _IOLBF, BUFFER_MAX ) ){
     perror("axon setvbuf");
     exit(errno);
  }   

  while( ! feof ( axon_stream ) ){
     retval = fread( (void *)buffer, 1, BUFFER_MAX-1, axon_stream );
     buffer[retval]='\0';
     fprintf(stdout, "%s", buffer );
  }

}

int find_axon_daemon ( void ) 
{
  char byte;
  fd_set rfds;
  struct timeval tv;
  int pid, addr_len, retval, bytes_recv;

  memset (&byte, 0, sizeof(byte));
  discovery_ask_socket = setup_multicast_socket ( GANGLIA_CHANNEL,
                                         DISCOVERY_ASK_PORT, 0 );
  discovery_tell_socket= setup_multicast_socket ( GANGLIA_CHANNEL,
                                         DISCOVERY_TELL_PORT, 1 );
  /*
   * Fork a child to whine until an axon responds
   */
  if ((pid=fork()) < 0) {
     perror ("Fork failed");
     exit(errno);
  }

  if (!pid) {
      /* Child */
      for ( ; ; ){
         if ( (write( discovery_ask_socket, &byte, sizeof(byte))) ==-1){
              perror("write");
              exit(errno);
         }  
      }
  }

  addr_len = sizeof(axon_addr);

  FD_ZERO( &rfds );
  FD_SET ( discovery_tell_socket , &rfds );
  tv.tv_sec = DISCOVERY_TIMEOUT;
  tv.tv_usec= 0;

  while (  retval = select ( FD_SETSIZE, &rfds, NULL, NULL, &tv ) ){
    recvfrom:
       if ((bytes_recv = recvfrom(discovery_tell_socket, &byte, sizeof(byte), 0,
                                    &axon_addr, &addr_len)) == -1) {
          if (errno == EINTR)
              goto recvfrom;
          perror("recvfrom error");
          exit(errno);
       } else {
          /* Tell the child to stop whining */
          if( kill ( pid, SIGKILL ) == -1 ){
             perror("could not kill child");
          } 
          return 1;
       }               
  }
  return 0;
}

void parse_command_line ( int argc, char **argv, char **env ) 
{
  char *p;
  register int i;  
  query.num_nodes     = 0;

  for (i = 1 ; i < argc ; i++ ){

     p = argv[i];
     query.token_data[token_number].order = 1;

     if ( isdigit( p[0] ) ){
         if ( !token_number ){
            fprintf(stderr, "\nERROR: you didn't specify any sort tokens\n\n");
            exit(1);
         }else{
            query.num_nodes = atoi ( p );
            break;
         }
     }

     query.token_data[token_number].order = 1;

     if      ( p[0] == '+' ) p++;
     else if ( p[0] == '-' ){
              query.token_data[token_number].order = -1;
              p++;
     }

     if ( is_valid_token( p ) )
         strcpy( query.token_data[token_number].token , p);
     else {
         fprintf(stderr, "ERROR: %s is not a valid token\n", p);
         fprintf(stderr, "\nType \"ganglia\" for command SYNTAX\n\n");
         exit(1);
     }
     token_number++;

     if ( token_number > NUM_TOKENS ){
         fprintf(stderr, "ERROR: only %d tokens can be specified\n", NUM_TOKENS);
         exit(1);
     }

  }
}

int is_valid_token ( char *p )
{
  register int i;

  for ( i = 0; i < NUM_TOKENS ; i++ ){
     if(! strcmp( p, valid_tokens[i] ) )
          return 1; 
  }
  return 0;
}


void print_help (void) 
{
  register int i;

  printf ("\nGANGLIA SYNTAX\n\n" );
  printf ("   ganglia [+,-]token [[+,-]token]...[[+,-]token] [number of nodes]\n\n");
  printf ("   modifiers\n");
  printf ("      + sort ascending   (default)\n");
  printf ("      - sort descending\n\n");
  printf ("   tokens");

  for ( i = 0 ;  valid_tokens[i] ; i++ ){
       if (!( i % 5))
            printf ("\n      ");
       printf ("%s ", valid_tokens[i]);
  }
  printf ("\n\n");
  printf ("   number of nodes\n");
  printf ("      the default is all the nodes in the cluster or GANGLIA_MAX\n\n");
  printf ("   environment variables\n");
  printf ("      GANGLIA_MAX   maximum number of hosts to return\n");
  printf ("                    (can be overidden by command line)\n\n");
  printf ("EXAMPLES\n\n");
  printf ("prompt> ganglia  -cpu_num\n");
  printf ("   would list all (or GANGLIA_MAX) nodes in ascending order ");
  printf ("by number of cpus\n\n");
  printf ("prompt> ganglia -cpu_num 10\n");
  printf ("   would list 10 nodes in descending order by number of cpus\n\n");
  printf ("prompt> ganglia -cpu_user -mem_free 25\n");
  printf ("   would list 25 nodes sorted by %cpu user descending then by memory ");
  printf ("free ascending\n");
  printf ("   (i.e 25 machines with the least cpu user load and most memory");
  printf (" available)\n\n");

  exit(1);
}


/* Returns socket */
int setup_multicast_socket ( char * CHANNEL, unsigned short PORT, int JOIN )
{
   struct ip_mreq mreq;
   int return_socket;
   struct sockaddr_in bind_structure;
   struct sockaddr_in connect_structure;
 
   if ( ( return_socket = socket(PF_INET, SOCK_DGRAM, 0) ) == -1 ){
      perror ("socket");
      exit(errno);
   }

   if (! JOIN ){
      memset(&connect_structure, 0, sizeof(connect_structure));
      connect_structure.sin_family      = PF_INET;
      connect_structure.sin_addr.s_addr = inet_addr( CHANNEL );
      connect_structure.sin_port        = htons    ( PORT    );

      if ( (connect (return_socket, &connect_structure, 
                             sizeof( connect_structure ))) == -1 ){
         perror("setup_multicast_socket() connect");
         exit (errno);
      }

      return return_socket;
   }
 
   mreq.imr_multiaddr.s_addr = inet_addr( CHANNEL );
   mreq.imr_interface.s_addr = htonl(     INADDR_ANY  );
 
   if (setsockopt( return_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
        &mreq, sizeof(mreq)) == -1 ) {
        perror("setsockopt to join multicast");
        exit(errno);
   }
 
   memset(&bind_structure, 0, sizeof(bind_structure) );
   bind_structure.sin_family      = PF_INET;
   bind_structure.sin_addr.s_addr = htonl ( INADDR_ANY );
   bind_structure.sin_port        = htons ( PORT );
 
   if (bind( return_socket, (struct sockaddr *)&bind_structure,
        sizeof(bind_structure)) == -1 ) {
        perror("bind error");
        exit(errno);
   }                        
  
   return return_socket;
}
