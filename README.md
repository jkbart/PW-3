# Couple thread safe queues in C

## SimpleQueue
one-way list queue, with two mutexes

## RingsQueue
a queue from a list of cyclic buffers

## LLQueue
lock-free queue from a one-way list

## BLQueue
lock-free queue from a list of buffers

## HazardPointer
Both LLQueue and BLQueue are using my implementation of HazardPointer 

Grade: 9.9/10
