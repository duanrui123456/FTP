#include <stdio.h>
#include <pthread.h>
#include <signal.h>

void *inc(void *);
void message();

int main()
{
  signal(SIGINT, message);

  long x;
  long y;
  pthread_t thread1;

  if((pthread_create(&thread1, NULL, inc, &x)) != 0)
  {
    printf("pthread fail");
  }// if

  for(y = 0; y < 3000000000; y++);
  printf("y finished\n");

  pthread_join(thread1, NULL);
}// main

void *inc(void *x)
{
  long i = *((long *) x);

  for(i = 0; i < 1500000000; i++);
  printf("x finished\n");
}// inc

void message()
{
  printf("no\n");
}// message
