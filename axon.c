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

/* $Id: axon.c,v 1.3 2000/12/19 02:26:50 massie Exp massie $ */

#include "ganglia.h"
#include <glib.h>

#define LOCK_FILE         "/tmp/axon.lock"
#define PID_FILE          "/var/run/axon.pid"
#define ERROR_FILE        "/tmp/axon_messages"

#define GANGLIA_LIST  1
#define MILLSTAT_LIST 2
#define REXEC_LIST    3

/* save hostname info so we don't slam DNS */
GHashTable *HOSTNAMES;       /* hash with key=IP   value=NAME */
GHashTable *HOSTIPS;         /* hash with key=NAME value=IP */
GHashTable *HOSTINFO;        /* key=hostname value=latest info from node */
GHashTable *REXEC_TIMESTAMP; /* key=hostname value=time last packet recv */
GHashTable *GANGLIA_TIMESTAMP;

/* Have to make these query vars global so my sort function can use it */
struct query_msg query;
/* How many tokens of info did they request ? */
int query_number;

/* Can move this from global to local later in process_ganglia_query() */
GSList *HostList = NULL;    

int pid_file, error_file;

void daemonize ( void );
void setup_sockets ( void );
void store_incoming_dendrite_info ( int );
void store_rexec_timestamp ( int );
gpointer get_hostname_from_ip ( gpointer ip_address ); 
int setup_multicast_socket ( gpointer, gushort, gboolean );
int setup_tcp_socket ( gushort );
void make_timestamp ( GHashTable *, gpointer );
void process_ganglia_query ( int data_socket );
void process_vexec_request ( int data_socket );
void process_discovery_request ( int ask_socket, int tell_socket );
struct field_data get_host_value ( char * HOST, char * FIELD );
gint ganglia_query_host_sort ( char * host1, char * host2 );
void create_host_list_sorted ( gpointer host, gpointer host_data, gpointer list_type );
void count_hash_keys ( gpointer key, gpointer value, gpointer num_keys );
void create_millstat_body ( gpointer host, gpointer socket );
gint rexec_query_host_sort ( char * host1, char * host2 );
double rexec_sort_scale ( char * host );
void create_rexec_body ( gpointer host, gpointer VexecArray );

int main ( void ) 
{
   int ganglia_socket, rexec_socket, discovery_ask_socket, 
       discovery_tell_socket, server_socket,  vexec_socket;  
   fd_set active_fd_set, read_fd_set;

   HOSTNAMES         = g_hash_table_new ( (GHashFunc)   g_str_hash,
                                          (GCompareFunc)g_str_equal );
   HOSTIPS           = g_hash_table_new ( (GHashFunc)   g_str_hash,
                                          (GCompareFunc)g_str_equal );
   HOSTINFO          = g_hash_table_new ( (GHashFunc)   g_str_hash,
                                          (GCompareFunc)g_str_equal );
   REXEC_TIMESTAMP   = g_hash_table_new ( (GHashFunc)   g_str_hash,
                                          (GCompareFunc)g_str_equal );
   GANGLIA_TIMESTAMP = g_hash_table_new ( (GHashFunc)   g_str_hash,
                                          (GCompareFunc)g_str_equal );

   daemonize ();
   g_message("axon started");

   rexec_socket     = setup_multicast_socket ( REXEC_CHANNEL, REXEC_PORT, TRUE );
   ganglia_socket   = setup_multicast_socket ( GANGLIA_CHANNEL, GANGLIA_PORT, TRUE );
   discovery_ask_socket  = setup_multicast_socket ( GANGLIA_CHANNEL,
                                               DISCOVERY_ASK_PORT, TRUE );
   discovery_tell_socket = setup_multicast_socket ( GANGLIA_CHANNEL,
                                               DISCOVERY_TELL_PORT, FALSE );
   server_socket    = setup_tcp_socket       ( GANGLIA_PORT );
   vexec_socket     = setup_tcp_socket       (   VEXEC_PORT );

   /* Initialize the file descriptor set. */
   FD_ZERO (&active_fd_set);
   FD_SET (ganglia_socket,   &active_fd_set);
   FD_SET (rexec_socket,     &active_fd_set);
   FD_SET (discovery_ask_socket,    &active_fd_set);
   FD_SET (vexec_socket,     &active_fd_set);
   FD_SET (server_socket,    &active_fd_set);

   for (;;) {

       read_fd_set = active_fd_set;
 
       if( select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) == -1)
          g_error ("main() select");

       if (FD_ISSET ( ganglia_socket , &read_fd_set))
          store_incoming_dendrite_info( ganglia_socket );

       if (FD_ISSET ( rexec_socket,    &read_fd_set))
          store_rexec_timestamp( rexec_socket );

       if (FD_ISSET ( server_socket,   &read_fd_set))
          process_ganglia_query( server_socket );

       if (FD_ISSET (discovery_ask_socket, &read_fd_set))
          process_discovery_request( discovery_ask_socket, discovery_tell_socket );

       if (FD_ISSET (vexec_socket,     &read_fd_set))
          process_vexec_request ( vexec_socket );

   }
 
}

void process_discovery_request ( int ask_socket, int tell_socket )
{
   char byte;
 
   if ( (read( ask_socket, &byte, sizeof( byte ) )) == -1 )
      g_warning("process_discovery_request() read");

   if ( (write(tell_socket, &byte, sizeof ( byte ) )) == -1 )
      g_warning("process_discovery_request() write");
}

void process_vexec_request ( int data_socket )
{
   struct sockaddr addr;
   int client_socket;
   int addrlen;
   int i;
   struct _vexec_msg {
      int type;
      int bdy_len;
      int bdy;
   } vexec_msg;        
   ssize_t bytes_recv=0;
   int list_type;
   int num_nodes=0;
   GPtrArray *VexecArray;

   addrlen = sizeof( addr );
 
   if ( (client_socket = accept ( data_socket, &addr, &addrlen )) == -1 ){
      g_warning ("process_vexec_request() accept failed\n");
      return;
   }                   

   if ( (bytes_recv = read(client_socket, &vexec_msg, sizeof(vexec_msg) )) 
                    == -1 ){
      g_warning ("process_vexec_request() read failed\n");
      return;
   }    

   VexecArray = g_ptr_array_new();

   switch ( vexec_msg.type ) {

      case 2: /* VEXEC_SELECT_REQUEST (for rexec) */

         g_slist_free( HostList );
         HostList = NULL;
 
         list_type = REXEC_LIST;
         g_hash_table_foreach ( HOSTINFO, (GHFunc)create_host_list_sorted ,&list_type);

         g_slist_foreach ( HostList, (GFunc)create_rexec_body, VexecArray ); 
 
         /* Each host entry is 16 bytes see create_rexec_body() */
         i = 3;                   /* Message Type */
         write ( client_socket, &i, sizeof ( i ) );
         i = ((VexecArray->len) * 16)+4; /* Body size */ 
         write ( client_socket, &i, sizeof ( i ) );
 
         if ( vexec_msg.bdy > VexecArray->len ){
            i = -1;
            write ( client_socket, &i, sizeof ( i ) );
         } else {
            i = 0;
            write ( client_socket, &i, sizeof ( i ) );
            for ( i = 0 ; i < VexecArray->len ; i++ ){
               write ( client_socket, g_ptr_array_index( VexecArray, i ), 16 );
            } 
         }

         /* Change VEXEC here... want -n 0 with REXEC
                 vexec_msg.bdy is the number requested
                 VexecArray->len is the number of nodes available
          */

         for ( i = 0 ; i < VexecArray->len ; i++ ){
            g_free ( g_ptr_array_index ( VexecArray, i ) );
         }

         g_slist_free( HostList );
         HostList = NULL;
         close ( client_socket );
         break;

      case 4: /* VEXEC_ALL_REQUEST  (for millstat)*/ 

         g_slist_free( HostList );
         HostList = NULL;

         list_type = MILLSTAT_LIST;
         g_hash_table_foreach( HOSTINFO, (GHFunc)create_host_list_sorted, &list_type);
     
         g_slist_foreach( HostList, (GFunc)create_millstat_body, VexecArray);

         /* Each entry in array is 48bytes see create_millstat_body() */
         i = 5;                       /* Message type */
         write ( client_socket, &i, sizeof( i ) );
         i = (VexecArray->len) * 48;  /* Body size */
         write ( client_socket, &i, sizeof( i ) );

         for ( i = 0  ; i < VexecArray->len ; i++ ){
            write ( client_socket, g_ptr_array_index( VexecArray, i ), 48 );
            g_free ( g_ptr_array_index ( VexecArray, i ) );
         }
        
         close ( client_socket );
         g_slist_free ( HostList );
         HostList = NULL;
         break;

   } 
   g_ptr_array_free ( VexecArray, TRUE );
}

void create_rexec_body ( gpointer host, gpointer VexecArray )
{
   gpointer mem;
  
   mem = g_malloc ( 16 );
   strncpy ( mem, g_hash_table_lookup ( HOSTIPS, host ), 16 );
   g_ptr_array_add ( VexecArray, mem );
}

void create_millstat_body ( gpointer host, gpointer VexecArray )
{
   gpointer entry_mem;
   struct field_data h_info;  
   struct millstat_entry {
      char ip[16];
      struct timeval last_announce;
      double total_rate;
      int num_cpus;
      int proc_run;
      double load_one;
   } entry;

   memset ( &entry, 0, sizeof( entry ) );

   strcpy ( entry.ip, g_hash_table_lookup ( HOSTIPS, host ) );
   h_info = get_host_value ( host, "cpu_num" );
   entry.num_cpus     = h_info.value;
   h_info = get_host_value ( host, "proc_run" );
   entry.proc_run     = (int)h_info.value; 
   h_info = get_host_value ( host, "load_one" );
   entry.load_one     = ((double)h_info.value)/100;

   entry_mem = g_memdup ( &entry, sizeof ( entry ) ); 
   g_ptr_array_add ( VexecArray, entry_mem );   
}

void print_host_data ( char * host, int * socket )
{
   /* Note: a long can be 10 characters long at most so we 
            will output at a fixed length of 11 characters per field */

   register int i;
   struct field_data h_info;
   char *p, *q;
 
   p = g_memdup ( host, strlen ( host ) );
   if ( (q = strchr( p, '.' )) != NULL ){
      *q = '\0';
   }
   sprintf( buffer, "%10.10s ", p );
   g_free ( p );

   write ( *socket, buffer, strlen( buffer ) );

   for ( i = 0 ; i < query_number ; i++ ){
      h_info = get_host_value ( host, query.token_data[i].token );
      sprintf( buffer, "%10.10s ", h_info.text );
      write ( *socket, buffer, strlen ( buffer ) );
   }
   write ( *socket, "\n", 1);
}

gint rexec_query_host_sort ( char * host1, char * host2 )
{
   return (rexec_sort_scale ( host1 ) - rexec_sort_scale ( host2 ));
}

double rexec_sort_scale ( char * host )
{
   struct field_data h_info;
   long cpu_num, cpu_speed, mem_free, mem_buffers, mem_total;
   double load_one, cpu_idle;
 
   h_info      = get_host_value ( host, "cpu_num" );
   cpu_num     = h_info.value;
   h_info      = get_host_value ( host, "cpu_speed" );
   cpu_speed   = h_info.value;
   h_info      = get_host_value ( host, "cpu_idle" );
   cpu_idle    = ((double)h_info.value/10);
   h_info      = get_host_value ( host, "load_one" );
   load_one    = ((double)h_info.value/100);
   h_info      = get_host_value ( host, "mem_free" );
   mem_free    = h_info.value;
   h_info      = get_host_value ( host, "mem_buffers" );
   mem_buffers = h_info.value;
   h_info      = get_host_value ( host, "mem_total" );
   mem_total   = h_info.value;

   /* Avoid divide by zero error */
   if (! mem_total )     mem_total=1;
   if (load_one < 0.01 ) load_one = 0.01;

   return (cpu_num / load_one)*cpu_speed*cpu_idle*
          ((mem_free + mem_buffers)/mem_total);
}

gint millstat_query_host_sort ( char * host1, char * host2 )
{        
   struct field_data h_info;
   double h1_load, h2_load;
   int    h1_cpus, h2_cpus;
  
   h_info  = get_host_value ( host1, "load_one" );
   h1_load = ((double)h_info.value)/100;
   h_info  = get_host_value ( host1, "cpu_num" );  
   h1_cpus = h_info.value;
  
   h_info  = get_host_value ( host2, "load_one" );
   h2_load = ((double)h_info.value)/100;
   h_info  = get_host_value ( host2, "cpu_num" );
   h2_cpus = h_info.value;
  
   return ((h2_cpus - h2_load) - (h1_cpus - h1_load));
}

gint ganglia_query_host_sort ( char * host1, char * host2 )
{
   register int i;
   struct field_data host1_info, host2_info;

   for ( i = 0 ; i < query_number ; i++ ){

      host1_info = get_host_value ( (char *)host1, query.token_data[i].token );
      host2_info = get_host_value ( (char *)host2, query.token_data[i].token ); 

      if ( host1_info.value == host2_info.value )
         continue;
      if ( host1_info.value > host2_info.value ) 
         return query.token_data[i].order;
      else 
         return (-1 * query.token_data[i].order);
   }

   /* All fields are equal at least sort by hostname :) */
   return strcmp ( host1, host2 );
}

int fresh_timestamp ( GHashTable *TABLE, char * hostname, int heartbeat )
{
   time_t now;
   time_t *then;
 
   if( time( &now ) == -1 ){
      g_warning("fresh_timestamp() time");
      return 0;
   } 

   if ( (then = g_hash_table_lookup ( TABLE, hostname )) == NULL ){
      return 0;
   }
      
   if ( abs ( now - *then ) > heartbeat )
      return 0;

   return 1;
}

void create_host_list_sorted ( gpointer host, gpointer host_data, gpointer list_type )
{
   switch ( *(int *)list_type ){ 

      case GANGLIA_LIST:
         if (! fresh_timestamp ( GANGLIA_TIMESTAMP, host, HEARTBEAT ) )
            break;

         HostList = g_slist_insert_sorted (HostList, host, 
                                   (GCompareFunc)ganglia_query_host_sort );
         break;

      case MILLSTAT_LIST:
         if (! fresh_timestamp ( REXEC_TIMESTAMP, host, HEARTBEAT ) )
            break;

         HostList = g_slist_insert_sorted (HostList, host,
                                   (GCompareFunc)millstat_query_host_sort );
         break;

      case REXEC_LIST:
         if (! fresh_timestamp ( REXEC_TIMESTAMP, host, HEARTBEAT ) )
            break;

         HostList = g_slist_insert_sorted (HostList, host,
                                   (GCompareFunc)rexec_query_host_sort );
         break;

      default:
         g_warning ("Invalid list type passed to create_host_list_sorted()\n"); 
         break;

   }
}

void count_hash_keys ( gpointer key, gpointer value, gpointer num_keys )
{
   *(int *)num_keys += 1;   
}

void process_ganglia_query ( int data_socket )
{
   struct sockaddr addr;
   int bytes_read;
   int client_socket;
   int addrlen;
   GSList *li;
   register int i;
   int list_type;
   int num_nodes = 0;
   addrlen = sizeof( addr );

   g_slist_free( HostList );
   HostList = NULL;
 
   if ( (client_socket = accept ( data_socket, &addr, &addrlen )) == -1 ){
      g_warning("process_ganglia_query() accept");
      return;
   }

   /* DO A CHECK OF ADDRESS TO SEE IF IT'S COMING FROM A GANGLIAZED NODE HERE 
   if ( ! in HOSTINFO ) close ( client_socket );  (for example)     
    */

   /* Get new query */
   if ( (bytes_read = read ( client_socket, &query, sizeof(query) )) == -1 ){
      g_warning("procesess_ganglia_query() read");
      return;
   }   
 
   query_number = ( bytes_read - sizeof( short ) ) / sizeof( _token_pair );
  
   /* Change the query to host order HERE*/
   query.num_nodes = ntohs ( query.num_nodes );
   for ( i = 0 ; i < query_number ; i++ ){
      query.token_data[i].order = ntohs (query.token_data[i].order);
   }

   list_type = GANGLIA_LIST;
   g_hash_table_foreach ( HOSTINFO, (GHFunc)create_host_list_sorted , &list_type );
   g_hash_table_foreach ( HOSTINFO, (GHFunc)count_hash_keys, &num_nodes );

   if ( query.num_nodes > 0 ){

      if ( query.num_nodes > num_nodes ){

         sprintf(buffer, "\nSorry, you requested info on %d nodes. I only have %d.\n\n",
                          query.num_nodes, num_nodes ); 
         write( client_socket, buffer, strlen( buffer ));

      } else {

         for ( i = 0, li = HostList; i < query.num_nodes ; i++, li = li->next ){
            print_host_data ( (char *)li->data, &client_socket );   
         } 
      }

   } else {

      g_slist_foreach ( HostList, (GFunc)print_host_data, &client_socket );

   }

   g_slist_free ( HostList );
   HostList = NULL;
   close( client_socket );
}

void store_rexec_timestamp ( int data_socket )
{
   int bytes_recv, addr_len;
   gpointer incoming_ip_address;
   gpointer incoming_hostname;
   time_t timestamp;
   time_t *new_timestamp;
   struct sockaddr_in recv_addr;
#define REXEC_MSG_SIZE 56
   char rexec_msg[REXEC_MSG_SIZE+10];
   gpointer orig_key, orig_value;
   
   addr_len = sizeof(recv_addr);
 
   recvfrom:
   if ((bytes_recv = recvfrom( data_socket ,
        (void *)&rexec_msg, sizeof(rexec_msg),
        0, (struct sockaddr *)&recv_addr, &addr_len )) == -1 ){
       if ( (errno == EINTR) )
          goto recvfrom;
       g_warning("recvfrom error");
       return;
   }

   if ( bytes_recv != REXEC_MSG_SIZE )
      return;
 
   if(! (incoming_ip_address = (char *)inet_ntoa( recv_addr.sin_addr )) ){
       g_warning("store_rexec_timestamp() inet_ntoa");
       return;
   }
 
   incoming_hostname = get_hostname_from_ip ( incoming_ip_address );   

   make_timestamp ( REXEC_TIMESTAMP, incoming_hostname );  
}

/* Returns socket */
int setup_tcp_socket (  gushort PORT )
{
   int return_socket;
   struct sockaddr_in bind_structure;

   if ( ( return_socket = socket(PF_INET, SOCK_STREAM, 0) ) == -1 )
      g_error ("setup_tcp_socket() socket");

   memset(&bind_structure, 0, sizeof(bind_structure) );
   bind_structure.sin_family      = PF_INET;
   bind_structure.sin_addr.s_addr = htonl ( INADDR_ANY );
   bind_structure.sin_port        = htons ( PORT );

   if ( bind( return_socket, (struct sockaddr *)&bind_structure,
                              sizeof(bind_structure)) == -1 ) 
      g_error("setup_tcp_socket() bind error");

   if ( listen( return_socket, SOMAXCONN ) == -1 )
      g_error("setup_tcp_socket() listen");

   return return_socket;
}


/* Returns socket */
int setup_multicast_socket ( gpointer CHANNEL, gushort PORT, gboolean JOIN )
{
   struct ip_mreq mreq;
   int return_socket;
   struct sockaddr_in bind_structure;
   struct sockaddr_in connect_structure;
 
   if ( ( return_socket = socket(PF_INET, SOCK_DGRAM, 0) ) == -1 )
      g_error ("setup_multicast_socket() socket");

   if (! JOIN ){
      memset(&connect_structure, 0, sizeof(connect_structure));
      connect_structure.sin_family      = PF_INET;
      connect_structure.sin_addr.s_addr = inet_addr( CHANNEL );
      connect_structure.sin_port        = htons    ( PORT    );

      if ( (connect (return_socket, &connect_structure, 
                             sizeof( connect_structure ))) == -1 )
         g_error("setup_multicast_socket() connect");

      return return_socket;
   }
 
   mreq.imr_multiaddr.s_addr = inet_addr( CHANNEL );
   mreq.imr_interface.s_addr = htonl(     INADDR_ANY  );
 
   if (setsockopt( return_socket, IPPROTO_IP, IP_ADD_MEMBERSHIP,
        &mreq, sizeof(mreq)) == -1 ) 
        g_error("setsockopt to join multicast");
 
   memset(&bind_structure, 0, sizeof(bind_structure) );
   bind_structure.sin_family      = PF_INET;
   bind_structure.sin_addr.s_addr = htonl ( INADDR_ANY );
   bind_structure.sin_port        = htons ( PORT );
 
   if (bind( return_socket, (struct sockaddr *)&bind_structure,
        sizeof(bind_structure)) == -1 ) 
        g_error("bind error");
  
   return return_socket;

}


void store_incoming_dendrite_info ( int data_socket ) 
{
   int bytes_recv, addr_len;

   struct node_state incoming_node_state;
   struct node_state *new_node_state;
   gpointer incoming_ip_address;
   gpointer incoming_hostname;
   gpointer orig_key, orig_value;
   time_t timestamp;
   time_t *new_timestamp;
   struct sockaddr_in recv_addr; 

   addr_len = sizeof(recv_addr);
 
   recvfrom:
   if ((bytes_recv = recvfrom( data_socket ,
        (void *)&incoming_node_state, sizeof(incoming_node_state),
        0, (struct sockaddr *)&recv_addr, &addr_len )) == -1 ){
       if ( (errno == EINTR) )
          goto recvfrom;
       g_warning("recvfrom error");
       return;
   }

   if(! (incoming_ip_address = (char *)inet_ntoa( recv_addr.sin_addr )) ){
       g_warning("inet_ntoa");
       return;
   }   
 
   incoming_hostname = get_hostname_from_ip ( incoming_ip_address );

   /* Store it */
   if (! g_hash_table_lookup_extended ( HOSTINFO, incoming_hostname, 
                                                  &orig_key, &orig_value ) ){ 

      new_node_state = g_memdup( &incoming_node_state, sizeof( incoming_node_state ));
      g_hash_table_insert( HOSTINFO, incoming_hostname, new_node_state );

   } else {

      memcpy( orig_value, &incoming_node_state, sizeof(incoming_node_state) ); 
      g_hash_table_insert( HOSTINFO, orig_key, orig_value );
   } 

   make_timestamp ( GANGLIA_TIMESTAMP, incoming_hostname ); 
 
}

void make_timestamp ( GHashTable *TABLE, gpointer HOSTNAME )
{
   time_t timestamp;
   time_t *new_timestamp;
   gpointer orig_key, orig_value;

   if( time( &timestamp ) == -1 ){
      g_warning ("make_timestamp() time");
      return;
   }
 
   if(! g_hash_table_lookup_extended (TABLE, HOSTNAME, &orig_key, &orig_value)){
 
      new_timestamp = g_memdup( &timestamp, sizeof(timestamp) );
      g_hash_table_insert( TABLE, HOSTNAME, new_timestamp );
 
   } else {
      memcpy( orig_value, &timestamp, sizeof(timestamp) );
      g_hash_table_insert( TABLE, orig_key, orig_value );
   }        

}

gpointer get_hostname_from_ip ( gpointer ip_address )
{
   gpointer return_hostname;
   gpointer new_ip_address;
   gpointer incoming_hostname;
   gpointer new_hostname;
   gpointer orig_key, orig_value;  
   struct hostent *h;
   struct in_addr in;

   if ( (return_hostname = g_hash_table_lookup (HOSTNAMES, ip_address)) == NULL ){
 
       /* hostname was not found for ip address so add it */
       if (! inet_aton(ip_address, &in) ) {
          perror("inet_aton error\n");
          exit(errno);
       }

       if ((h = gethostbyaddr((char *)&in.s_addr, sizeof(in.s_addr),AF_INET))==NULL){
          perror("gethostbyaddr");
          exit(errno);
       }           
 
       new_hostname    = g_memdup( h->h_name,  strlen(h->h_name)+1 );
       return_hostname = new_hostname;
       new_ip_address  = g_memdup( ip_address, strlen(ip_address)+1);
 
       g_hash_table_insert ( HOSTNAMES, new_ip_address, new_hostname );
       g_hash_table_insert ( HOSTIPS,   new_hostname,   new_ip_address );
   }            

   return return_hostname;
}

struct field_data get_host_value ( char * HOST, char * FIELD )
{
   /* Question: should use doubles for the values and shift my 
                decimal around for absolute values or are
                relative values better?  Using relative now */
   gpointer value;
   struct node_state *h_info;
   struct field_data ret;
   register int index;
   
   if ( (value = g_hash_table_lookup ( HOSTINFO, HOST )) == NULL ){
      g_message ("%s not in the HOSTINFO table\n", HOST);
      return;
   }

   /* Use bsearch() instead later? */
   for ( index = 0 ; valid_tokens[index] != NULL ; index++ ){
      if (! strcmp( FIELD, valid_tokens[index] ) )
         break;
   }

   h_info = value;

   switch ( index ) {

      case  0: ret.value = (long) ntohs (h_info->cpu_num);
               sprintf( ret.text, "%hd", ret.value );
               return ret;
      case  1: ret.value = (long) ntohs (h_info->cpu_speed);
               sprintf( ret.text, "%hd", ret.value );
               return ret;
      case  2: ret.value = (long) ntohs (h_info->cpu_user);
               sprintf( ret.text, "%.1f", ((float)ret.value)/10 );
               return ret; 
      case  3: ret.value = (long) ntohs (h_info->cpu_nice);
               sprintf( ret.text, "%.1f", ((float)ret.value)/10 );
               return ret;
      case  4: ret.value = (long) ntohs (h_info->cpu_system);
               sprintf( ret.text, "%.1f", ((float)ret.value)/10 );
               return ret;
      case  5: ret.value = (long) ntohs (h_info->cpu_idle);
               sprintf( ret.text, "%.1f", ((float)ret.value)/10 );
               return ret;
      case  6: ret.value = (long) ntohs (h_info->cpu_aidle);
               sprintf( ret.text, "%.1f", ((float)ret.value)/10 );
               return ret;  
      case  7: ret.value = (long) ntohs (h_info->load_one);
               sprintf( ret.text, "%.2f", ((float)ret.value)/100);
               return ret;
      case  8: ret.value = (long) ntohs (h_info->load_five);
               sprintf( ret.text, "%.2f", ((float)ret.value)/100);
               return ret;
      case  9: ret.value = (long) ntohs (h_info->load_fifteen);
               sprintf( ret.text, "%.2f", ((float)ret.value)/100);
               return ret;
      case 10: ret.value = (long) ntohs (h_info->proc_run);
               sprintf( ret.text, "%hd", ret.value );
               return ret;
      case 11: ret.value = (long) ntohs (h_info->proc_total);
               sprintf( ret.text, "%hd", ret.value );
               return ret;
      case 12: ret.value = fresh_timestamp ( REXEC_TIMESTAMP, HOST, HEARTBEAT);
               sprintf( ret.text, "%d", ret.value );
               return ret;
      case 13: ret.value = fresh_timestamp (GANGLIA_TIMESTAMP,HOST, HEARTBEAT);
               sprintf( ret.text, "%d", ret.value );
               return ret;
      case 14: ret.value = ntohl (h_info->mem_total);
               sprintf( ret.text, "%ld", ret.value );
               return ret;
      case 15: ret.value = ntohl (h_info->mem_free);
               sprintf( ret.text, "%ld", ret.value );
               return ret;  
      case 16: ret.value = ntohl (h_info->mem_shared);
               sprintf( ret.text, "%ld", ret.value );
               return ret;
      case 17: ret.value = ntohl (h_info->mem_buffers);
               sprintf( ret.text, "%ld", ret.value );
               return ret;
      case 18: ret.value = ntohl (h_info->mem_cached);
               sprintf( ret.text, "%ld", ret.value );
               return ret;
      case 19: ret.value = ntohl (h_info->swap_total);
               sprintf( ret.text, "%ld", ret.value );
               return ret;
      case 20: ret.value = ntohl (h_info->swap_free);
               sprintf( ret.text, "%ld", ret.value );
               return ret;
      default:
         g_message ("invalid FIELD=%s passed to get_host_value()\n", FIELD);
         ret.value = 0;
         sprintf( ret.text, "%ld", ret.value );
         return ret;
   }
}

void my_log_handler (const gchar *log_domain, GLogLevelFlags log_level,
                     const gchar *message, gpointer user_data)
{
   time_t now;
   char * p, * date_text;
   char msg_type[12];
   char * p_ctime;

   time( &now );
   p_ctime = ctime ( &now ); 

   date_text = g_malloc ( strlen( p_ctime ) + 1 );
   strcpy ( date_text, p_ctime );
   
   p = strchr ( date_text, '\n');
   *p = ':'; /* Get ride of \n */

   if        ( log_level == G_LOG_LEVEL_MESSAGE ){
      strcpy (msg_type, "Message:");
   } else if ( log_level == G_LOG_LEVEL_WARNING ){
      strcpy (msg_type, "Warning:");
   } else if ( log_level == G_LOG_LEVEL_ERROR ){
      strcpy (msg_type, "  Error:");
   } else {
      strcpy (msg_type, "        ");
   }

   p = g_malloc ( strlen(date_text) + strlen(message) + strlen(msg_type) + 10 );

   sprintf( p, "%s %s %s\n", msg_type, date_text, message );

   write( error_file, p, strlen(p) ); 

   g_free ( p );
   g_free ( date_text );
}

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

   g_log_set_handler( NULL, G_LOG_FLAG_FATAL| G_LOG_LEVEL_ERROR |
                      G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING |
                      G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_INFO |
                      G_LOG_LEVEL_DEBUG
                      , my_log_handler, NULL);
 
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
