#include <iostream>
#include <vector>
#include <fstream>
#include "goldchase.h"
#include "Map.h"
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <semaphore.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <signal.h>
#include <string.h>
#include <mqueue.h>
#include <string>
#include <stdlib.h>
#include <sys/types.h>
#include <sstream>
#include <sys/socket.h>
#include <netdb.h>
#include "fancyRW.h"
#include "mapboard.h"
using namespace std;
//extern mapboard* map; // check
// extern int rowp;
// extern int colp;
extern sem_t * sem;

int sockfd; //file descriptor for the socket
int status; //for error checking
int new_sockfd; // file descriptor for each connection
unsigned char *local_map; // local copy of map for server
mapboard * servmap_Pointer;

int rowp, colp;

// get currently playing players in system
unsigned char PlayersPlayingNow()
{
  unsigned char playersPlayingNow = 0;

  if(servmap_Pointer->Players[0]!=0)
    playersPlayingNow |= G_PLR0;
  if(servmap_Pointer->Players[1]!=0)
    playersPlayingNow |= G_PLR1;
  if(servmap_Pointer->Players[2]!=0)
    playersPlayingNow |= G_PLR2;
  if(servmap_Pointer->Players[3]!=0)
    playersPlayingNow |= G_PLR3;
  if(servmap_Pointer->Players[4]!=0)
    playersPlayingNow |= G_PLR4;
  return playersPlayingNow;
}

//

void serverSighandler(int signal)
{

  cerr << "at least signal received" << endl;

  if(signal == SIGHUP) // suspend process
  {

    cerr << "sighup recieved" << endl ;
    unsigned char playersPlayingNow = PlayersPlayingNow();
    unsigned char socketPlayer = G_SOCKPLR | playersPlayingNow;
    
    WRITE(new_sockfd, &socketPlayer, sizeof(socketPlayer));

    if( socketPlayer == G_SOCKPLR) // if no player is playing right now
    {
      sem_close(sem);
      sem_unlink("/semSHM");
      shm_unlink("/mapSHM");              
      exit(0);
    } 

    for(int i =0; i < rowp*colp; i++)
    {
      if(local_map[i] == G_WALL)
      { cerr << "*";}
      if(local_map[i] == G_PLR0)
      { cerr << "1";}
      if(local_map[i] == G_PLR1)
      { cerr << "2";}
      if(local_map[i] == G_PLR2)
      { cerr << "3";}

      // cerr << local_map[i];
    }
    cerr << endl;


  }
  else if(signal == SIGUSR1)
  {
    vector<pair<short, unsigned char> > sockmap;
    for(short i=0; i< servmap_Pointer->rows*servmap_Pointer->cols;i++)
    {
      if(local_map[i] != servmap_Pointer->map[i])
      {
        cerr << "some movement noted in server side" << endl;
        sockmap.push_back(std::make_pair(i, servmap_Pointer->map[i]));
        local_map[i]=servmap_Pointer->map[i];
      }
    }

    if(sockmap.size() > 0)
    {
      
      short n = sockmap.size();
      unsigned char byt = 0;
      WRITE(new_sockfd, &byt, sizeof(byt));
      WRITE(new_sockfd, &n, sizeof(n));
      for(short i=0; i< n; i++)
      {
        short offset= sockmap[i].first;
        unsigned char value = sockmap[i].second;
        WRITE(new_sockfd, &offset, sizeof(offset));
        WRITE(new_sockfd, &value, sizeof(value));
      }
    }

  }

}


void CreateServer()
{
  //close(2);
  //int fd = open("jayfifo", O_WRONLY);
  int shm_fd1 = shm_open("/mapSHM", O_RDWR, S_IRUSR|S_IWUSR);
  
  // rowp= map->rows;
  // colp= map->cols;
  read(shm_fd1, &rowp, sizeof(int));
  read(shm_fd1, &colp, sizeof(int));
    // servmap_Pointer =  map;
  servmap_Pointer=(mapboard*)mmap(NULL, (rowp*colp)+sizeof(mapboard), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd1, 0);

  servmap_Pointer->daemonID = getpid();
    // new int(5) => int x = 5    
  local_map = new unsigned char[rowp*colp];
  memcpy(local_map, servmap_Pointer->map, rowp*colp);

  for(int i =0; i < rowp*colp; i++)
  {
    if(local_map[i] == G_WALL)
    { cerr << "*";}
    if(local_map[i] == G_PLR0)
    { cerr << "1";}
    if(local_map[i] == G_PLR1)
    { cerr << "2";}
    if(local_map[i] == G_PLR2)
    { cerr << "3";}

    // cerr << local_map[i];
  }

  struct sigaction serversig_struct; // signal struct
  sigemptyset(&serversig_struct.sa_mask);
  serversig_struct.sa_flags=0;
  serversig_struct.sa_restorer=NULL;
  serversig_struct.sa_handler=serverSighandler;
  //
  sigaction(SIGHUP, &serversig_struct, NULL);
  sigaction(SIGUSR1, &serversig_struct, NULL);
  //sigaction(SIGUSR2, &sig_struct, NULL);
  // ending of setup
  // server connection establishment

  //change this # between 2000-65k before using
  const char* portno="25420";
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets
  hints.ai_flags=AI_PASSIVE; //file in the IP of the server for me
  struct addrinfo *servinfo;
  if((status=getaddrinfo(NULL, portno, &hints, &servinfo))==-1)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  sockfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

  /*avoid "Address already in use" error*/
  int yes=1;
  if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))==-1)
  {
    perror("setsockopt");
    exit(1);
  }
  //We need to "bind" the socket to the port number so that the kernel
  //can match an incoming packet on a port to the proper process
  if((status=bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {
    perror("bind");
    exit(1);
  }
  //when done, release dynamically allocated memory
  freeaddrinfo(servinfo);

  if(listen(sockfd,1)==-1)
  {
    perror("listen");
    exit(1);
  }
  //printf("Blocking, waiting for client to connect\n");
  struct sockaddr_in client_addr;
  socklen_t clientSize=sizeof(client_addr);

  do {
    new_sockfd=accept(sockfd, (struct sockaddr*) &client_addr, &clientSize);
  }while(new_sockfd==-1 && errno==EINTR);
  if(new_sockfd==-1 && errno!=EINTR)
  {
    perror("accept");
    exit(1);
  }

  unsigned char servplayers = PlayersPlayingNow();
  unsigned char playersock = G_SOCKPLR|servplayers;

  if(playersock & G_PLR1)
  {
    cerr << "sending second player" << endl;
  }

  // read and write to the socket will be here

  WRITE(new_sockfd, &rowp, sizeof(rowp));
  WRITE(new_sockfd, &colp, sizeof(colp));
  for(int i=0;i<rowp*colp;i++)
  {
    WRITE(new_sockfd, &local_map[i], sizeof(local_map[i]));
  }
  WRITE(new_sockfd, &playersock, sizeof(playersock));

  while(1) 
  {
    unsigned char protocol, newmapbyte;
    short noOfchangedmap, offset;
    unsigned char playerbit[5]= {G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
    READ(new_sockfd, &protocol, sizeof(protocol));

    if(protocol & G_SOCKPLR)
    {
      for(int i=0;i<5; i++)
      {
        if(protocol & playerbit[i] && servmap_Pointer->Players[i]==0)
        {
          servmap_Pointer->Players[i]=getpid();
        }
        else if((protocol & playerbit[i])==false && servmap_Pointer->Players[i]!=0)
        {
          servmap_Pointer->Players[i]=0;
        }
      }
      if(protocol==G_SOCKPLR)
      {
        shm_unlink("/mapSHM");
        sem_close(sem);
        sem_unlink("/semSHM");
        exit(0);
      }
    }

    else if(protocol==0) 
    {

      READ(new_sockfd, &noOfchangedmap, sizeof(noOfchangedmap));
      for(short i=0;i<noOfchangedmap; i++)
      {
        READ(new_sockfd, &offset, sizeof(offset));
        READ(new_sockfd, &newmapbyte, sizeof(newmapbyte));
        servmap_Pointer->map[offset]=newmapbyte;
        local_map[offset]=newmapbyte;
      }
      for(int i=0;i<5; i++)
      {
        if(servmap_Pointer->Players[i]!=0 && servmap_Pointer->Players[i]!=getpid())
        {
          kill(servmap_Pointer->Players[i], SIGUSR1);
        }
      }
    }
  }

  close(sockfd);
  close(new_sockfd);
  delete local_map;

}



void create_server_daemon()
{
  if(fork() > 0)
  {
    return;
  }
  if(fork()>0)
    exit(0);

  if(setsid()==-1) //child obtains its own SID & Process Group
    exit(1);
  for(int i=0; i<sysconf(_SC_OPEN_MAX); ++i)
    close(i);
  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  //open("/dev/null", O_RDWR); //fd 2
  int fd = open("jayfifo", O_WRONLY);
  if(fd == -1)
  {
    exit(99);
  }

  umask(0);
  chdir("/");
  //now do whatever you want the daemon to do
  CreateServer();
}



