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

#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/utsname.h>
#include <sys/time.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <fcntl.h> 
#include <errno.h> 
#include <unistd.h>
#include <signal.h>

#define REXEC_CHANNEL     "225.0.0.50"
#define REXEC_PORT        5050
#define VEXEC_PORT        802
#define GANGLIA_CHANNEL   "225.0.1.50"
#define GANGLIA_PORT        9290 
#define DISCOVERY_ASK_PORT  9291
#define DISCOVERY_TELL_PORT 9292  
#define DISCOVERY_TIMEOUT  2

#define HEARTBEAT         120    
#define SECONDS_TO_SLEEP  5
#define SECONDS_TO_FORCE  60 

#define BUFFER_SIZE 4096
char   buffer[BUFFER_SIZE];

const char *valid_tokens[] = {
  "cpu_num",    "cpu_speed", "cpu_user",   "cpu_nice",    "cpu_system",
  "cpu_idle",  "cpu_aidle",  "load_one",    "load_five",   "load_fifteen",
  "proc_run", "proc_total",  "rexec_up",    "ganglia_up",
  "mem_total",  "mem_free",  "mem_shared", "mem_buffers", "mem_cached",
  "swap_total", "swap_free"
};

#define NUM_TOKENS ( sizeof( valid_tokens ) / sizeof ( char * ) )

typedef struct node_state {
   short cpu_num;
   short cpu_speed;
   short cpu_user;
   short cpu_nice;
   short cpu_system;
   short cpu_idle;
   short cpu_aidle;
   short load_one;
   short load_five;
   short load_fifteen;
   short proc_run;
   short proc_total;
   long  mem_total;
   long  mem_free;
   long  mem_shared;
   long  mem_buffers;
   long  mem_cached;
   long  swap_total;
   long  swap_free;
} _node_state;

#define GANGLIA_MSG_SIZE sizeof(node_state)

typedef struct field_data {
   char text[64];
   long value;
} _field_data;   

typedef struct token_pair {
   char token[16];
   short order;
} _token_pair;

typedef struct query_msg {
   short num_nodes;
   struct token_pair token_data[NUM_TOKENS];
} _query_msg;  


