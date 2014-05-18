default: dendrite ganglia axon

dendrite: dendrite.c ganglia.h
	gcc -O2 -o dendrite dendrite.c
axon: axon.c ganglia.h
	gcc -O2 -I/usr/lib/glib/include -o axon axon.c -lglib 
ganglia: ganglia.c ganglia.h 
	gcc -O2 -o ganglia ganglia.c
