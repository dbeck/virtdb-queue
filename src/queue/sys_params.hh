#pragma once

namespace virtdb { namespace queue {
  
  struct sys_params
  {
    /* MAC OSX:
       semmap:     30	(# of entries in semaphore map)
       semmni:      8	(# of semaphore identifiers)
       semmns:     64	(# of semaphores in system)
       semmnu:     32	(# of undo structures in system)
       semmsl:  87381	(max # of semaphores per id)
       semopm:      5	(max # of operations per semop call)
       semume:     10	(max # of undo entries per process)
       semusz:     32	(size in bytes of undo structure)
       semvmx:  32767	(semaphore maximum value)
       semaem:  16384	(adjust on exit max value)
     
       Linux: ipcs -sl
     
       -------- Szemaforok korlátai -----------
       a tömbök maximális száma = 128
       szemaforok tömbönként maximális száma = 250
       rendszerszintű szemaforok maximális száma = 32000
       szemafor hívásonkénti műveletek maximális száma = 32
       szemafor maximális értéke = 32767
     
       kernel.sem = 250	32000	32	128
     
     SEMMNI - 128 - System wide maximum number of semaphore sets: policy dependent (on Linux, this limit can be read and modified via the fourth field of /proc/sys/kernel/sem).
     SEMMNS - 32000 - System wide maximum number of semaphores: policy dependent (on Linux, this limit can be read and modified via the second field of /proc/sys/kernel/sem). Values greater than SEMMSL * SEMMNI makes it irrelevant.
     SEMMSL - 250 - Maximum number of semaphores per semid: implementation dependent (on Linux, this limit can be read and modified via the first field of /proc/sys/kernel/sem).
     
       FreeBSD: ipcs -S
     
       semmni:           50	(# of semaphore identifiers)
       semmns:          340	(# of semaphores in system)
       semmnu:          150	(# of undo structures in system)
       semmsl:          340	(max # of semaphores per id)
       semopm:          100	(max # of operations per semop call)
       semume:           50	(max # of undo entries per process)
       semusz:          632	(size in bytes of undo structure)
       semvmx:        32767	(semaphore maximum value)
       semaem:        16384	(adjust on exit max value)
     
     */
  };
  
}}