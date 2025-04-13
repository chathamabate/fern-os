
#pragma once

/*
 * In this module, a lot of definitions will depend on each other.
 *
 * So this file just does some basic defines with no actual details.
 *
 * FernOS will operate on 3 core structures: Processes, Threads, and Conditions.
 *
 * NOTE: Remember, the below structures will likely ALL depend on the kernel being single threaded.
 *
 * -- Processes --
 *
 * A Process is a memory space which holds the stack(s) of one or multiple executing threads. 
 * When FernOS starts, one singular process will be spawned: the root process. All other process
 * must be spawned via some sort of fork. For this reason, all processes besides the root will have
 * a parent process.
 *
 * When a process exits, all of its threads stop executing, and its exit code is stored until the 
 * parent process "reaps" or cleans up this "zombie" process.
 *
 * If a process exits, but has children processes still executing, the child processes become 
 * children of the root. We will need to make it a responsibility of the root to reap all adopted
 * children.
 *
 * All processes are given a 16-bit process id or pid. This "pid" is globally unique across all
 * processes. (i.e. No 2 processes have the same pid)
 *
 * However, once a process exits AND is reaped, then its pid can be recycled.
 *
 * -- Threads --
 *
 * A Thread is a unit of execution. It will have its own stack and execution context. A thread
 * will always belong to a process which defines the memory space the thread will execute in.
 *
 * A thread is either "executing" or "waiting". 
 *
 * When a thread is "executing", it belongs to some scheduling queue. That is, the kernel has 
 * permission to give the thread control over the CPU.
 *
 * When a thread is "waiting", it does NOT belong to a scheduling queue. While in the "waiting"
 * state, the thread's context will never be entered. When "waiting", a thread is always waiting
 * on some "condition". 
 *
 * All threads have a locally unique 16-bit thread id or tid. No two threads 
 * in the same process have the same tid. However, two threads in different proceses can have the 
 * same tid.
 *
 * Since conditions and the scheduler may care little about what process a thread belongs to, 
 * threads will also have a "global thread id" or gtid. The gtid should equal (pid << 16) | tid.
 * Thus, all threads will have a globaly unique gtid.
 *
 * -- Conditions --
 *
 * A Condition is FernOS's mechanism for preventing busy waiting. They are intended to allow for
 * low-overhead synchronization and event-processing. A condition is just a boolean flag which can
 * be set to ON or OFF.
 *
 * There will be some sort of scheduler for managing executing threads. Conditions are used to 
 * manage waiting threads.
 *
 * A Condition will have a queue which can hold a custom (or unlimited) number of gtid's.
 * It is gauranteed, that when a thread is waiting, its gtid is in one and only one condition queue.
 *
 * What if we want to wait on multiple conditions at once?? is that allowed??
 * Ehhh, I don't think so...
 *
 * Could we chain conditions??
 * That could be kinda cool... Like one condition could trigger another condition??
 * 
 */


// Maybe we could have a notion of a "triggerable"?
// Either a thread, or a condition itself??
// Ok, but what if a condition belongs to 2 other conditions, is that ok??
// I think I'd say that's ok??
// Maybe???





