//Bharat Kandrigi, BXK190014
#include <stdio.h>
#include <string.h>
#include <stdlib.h>   
#include <stdint.h>
#include <pthread.h>
#include <inttypes.h>  
#include <errno.h>     // for EINTR
#include <fcntl.h>     
#include <unistd.h>    
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <math.h>

#include "common.h"
#include "common_threads.h"
#define BSIZE 4096
// Print out the usage of the program and exit.
int32_t fd;
int i_t = 0;
uint64_t numThreads;
uint64_t bpt;//number of blocks assigned to each thread
size_t height;
void Usage(char*);
void* computeHash(void*);
uint32_t jenkins_one_at_a_time_hash(const uint8_t*, uint64_t);
typedef struct ARGS {
   char* key;
   size_t height;
} ARGS;
int main(int argc, char** argv)//Where the hashing algorithm is initiated, a thread is created which recursively creates a binary tree of threads to compute the file's hash value
{
  void* hash;
  uint64_t nblocks;
  struct stat sb;
  // input checking 
  if (argc != 3)
    Usage(argv[0]);

  height = atoi(argv[2]);
  // open input file
  fd = open(argv[1], O_RDWR);
  if (fd == -1) {
    perror("open failed");
    exit(EXIT_FAILURE);
  }
  
  if (fstat(fd, &sb))
  {
    perror("get file size failed");
    exit(EXIT_FAILURE);
  }
  else
  {
     printf("File size: %ld\n", sb.st_size);  
  }
  // use fstat to get file size
  // calculate nblocks 
  nblocks = sb.st_size/BSIZE; 
  numThreads = (uint32_t)(pow(2.0, height+1));
  bpt = nblocks/numThreads;
  printf(" no. of blocks = %u \n", nblocks); 
  struct timeval start, stop;
  // calculate hash of the input file
  char* ptr = mmap(NULL,BSIZE*nblocks,PROT_READ,MAP_FILE,fd,0);
  ARGS a = {.key = ptr, .height = height};
  gettimeofday(&start, NULL);
  pthread_t p1;
  Pthread_create(&p1, NULL, computeHash, &a);
  Pthread_join(p1, &hash);
  gettimeofday(&stop, NULL);
  printf("hash value = %u \n", hash);
  printf("time taken = %ld \n", (stop.tv_sec - start.tv_sec)*1000000 + stop.tv_usec-start.tv_usec);//Display time taken to compute hash value of file in microseconds
  close(fd);
  return EXIT_SUCCESS;
}
void* computeHash(void* arg)//Take the address of the memory allocated to the file and assign a certain number of blocks of the memory to each thread, which invokes the Jenkins function to get the hash value
{
   struct ARGS* p_struct = (struct ARGS*)arg;//pass in pointer to struct containing the key and height values
   struct ARGS p_str = *p_struct; 
   uint32_t hash = 0;
   if (p_str.height == 0)//create leaf nodes that solely compute a hash value
   {
     void* leftH = malloc(32);
     char* str = malloc(sizeof(uint32_t)*2);
     char* concatStr = malloc(sizeof(leftH)+sizeof(str));
     hash = jenkins_one_at_a_time_hash((uint8_t*)p_str.key, (uint64_t)bpt);
     p_str.key += bpt;
     sprintf(str, "%u", hash);
     i_t++;
     if (i_t == (int)pow(2, height-1)-1)
     {
        p_str.height--;
	pthread_t leftT;
        Pthread_create(&leftT, NULL, computeHash, &p_str);
	Pthread_join(leftT, &leftH);
        strcpy(concatStr, strcat(str, leftH));
        pthread_exit(concatStr);
     }
     else
     {
        pthread_exit(str);
     }
     free(p_struct);
     free(concatStr);
     free(leftH);
   }
   else if (p_str.height < 0)//create extra left child to match total number of nodes for corresponding height
   {
      char* str = malloc(sizeof(uint32_t)*2);
      hash = jenkins_one_at_a_time_hash((uint8_t*)p_str.key, (uint64_t)bpt);
      sprintf(str, "%u", hash);
      p_str.key += bpt;  
      i_t++;
      pthread_exit(str);
      free(p_struct);
   }
   else //internal nodes create left and right threads, using the values returned from each thread to concatenate with its own hash and return to the parent thread
   {
     char* str = malloc(sizeof(uint32_t)*2);
     hash = jenkins_one_at_a_time_hash((uint8_t*)p_str.key, (uint64_t)bpt); 
     p_str.key += bpt;
     p_str.height--;
     int left = 0, right = 0;
     void* leftH = malloc(32);
     void* rightH = malloc(32);
     char* concatStr = malloc(sizeof(leftH)+sizeof(str)+sizeof(rightH));
     sprintf(str, "%u", hash);
     if ((2*i_t)+1 < numThreads)
     {
       left = 1;
       pthread_t leftT;
       Pthread_create(&leftT, NULL, computeHash, &p_str);
       Pthread_join(leftT, &leftH);
     }
     if ((2*i_t)+2 < numThreads)
     {
       right = 1;
       pthread_t rightT;
       Pthread_create(&rightT, NULL, computeHash, &p_str);
       Pthread_join(rightT, &rightH);
     }
     if (left && right)
     {
        strcpy(concatStr, strcat(strcat(str, leftH), rightH));
        free(leftH);
	free(rightH);
	free(str);
	free(p_struct);
	i_t++;
	pthread_exit(concatStr);
     }
     else if (left)
     {
         strcpy(concatStr, strcat(str, leftH));
         free(leftH);
	 free(str);
	 free(p_struct);
	 i_t++;
	 pthread_exit(concatStr);
     }
     else if (right)
     {
        strcpy(concatStr, strcat(str, rightH));
	free(rightH);
	free(str);
	free(p_struct);
	i_t++;
	pthread_exit(concatStr);
     }
     else
     {
       free(p_struct);
       i_t++;
       pthread_exit(str);
     }
   }
}
uint32_t jenkins_one_at_a_time_hash(const uint8_t* key, uint64_t length) //algorithm for computing the hash value of a block of a file
{//pass in ptr to mmap for p_str.key to divide blocks among threads for hashing  
  size_t i = 0;
  uint32_t hash = 0;
     while (i != length) {
       hash += key[i++];
       hash += hash << 10;
       hash ^= hash >> 6;
     }
     hash += hash << 3;
     hash ^= hash >> 11;
     hash += hash << 15;
     return hash; 
}
void Usage(char* s) {
  fprintf(stderr, "Usage: %s filename height \n", s);
  exit(EXIT_FAILURE);
}
