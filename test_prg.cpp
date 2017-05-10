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
#include <sys/wait.h> //wait(NULL)
#include <stdlib.h>
#include <sys/types.h>
#include <sstream>
#include <sys/socket.h>
#include <netdb.h>
#include "fancyRW.h"
#include "mapboard.h"
using namespace std;


int goldCoins;
int mapRows;
int mapCols;

// player working via a given shell
unsigned char currentPlayer;
// position of player from a given shell
int currentPosition;
int thisPlayer;

string currentPlayer_mq_name;

sem_t * sem;
// to read mapfile
string eachLine, mapstr;
// store gameBoard array globally
char * gameBoard;
//Map Board;

// file descriptor for map shared memory
int shm_fd_map; 

Map * mapPtr = NULL;
Map* globalMap;


mqd_t readqueue_fd; //message queue file descriptor
mqd_t writequeue_fd; //message queue file descriptor

mapboard * map;
unsigned char *local_map2;
int clientdpipe[2];
int clientfd;
// struct mapboard
// {

//   int rows;
//   int cols;
//   bool someoneWon; // need to store this in shared memory for every shell to access
//   //unsigned char Players;
//   pid_t daemonID;
//   pid_t Players[5];
//   unsigned char map[0];
// }*map;




void ReadMapFile()
{

  mapRows = 0;
  mapCols = 0;
  ifstream mapFile;
  mapFile.open("mymap.txt");
  perror("cud not open mapfile");
  getline(mapFile, eachLine);
  // no of gold coins are given in first line
  goldCoins = atoi(eachLine.c_str());
  getline(mapFile,eachLine);
  mapstr.append(eachLine);
  // took out 1st row in second getline
  mapRows++;
  // calculating no of columns
  mapCols = eachLine.length();

  while(getline(mapFile,eachLine))
  {
    mapstr.append(eachLine);
    mapRows++;
  }
  mapFile.close();
}

void SetGameBoard()
{
  
  //sem_wait(sem);
  gameBoard = new char[(mapRows*mapCols)];
  for(int i = 0; i < (mapRows*mapCols); i++)
  {
    if(mapstr[i] == ' ') { gameBoard[i] = 0 ;} // blank space on board
    else if(mapstr[i] == '*') { gameBoard[i] = G_WALL ;} // wall on board
    else if(mapstr[i] == '1') { gameBoard[i] = G_PLR0 ;} // player 0
    else if(mapstr[i] == '2') { gameBoard[i] = G_PLR1 ;} // player 1
    else if(mapstr[i] == '3') { gameBoard[i] = G_PLR2 ;} // player 2
    else if(mapstr[i] == '4') { gameBoard[i] = G_PLR3 ;} // player 3
    else if(mapstr[i] == '5') { gameBoard[i] = G_PLR4 ;} // player 4
    else if(mapstr[i] == 'F') { gameBoard[i] = G_FOOL ;} // fools gold
    else if(mapstr[i] == 'G') { gameBoard[i] = G_GOLD ;} // real gold
  }
  //sem_post(sem);

}

int  getrandomLocation()
{

  //cerr << map->rows << " " << map->cols << endl;
  //cerr << rand() << endl;
  int randomLoc = rand()%(map->rows*map->cols);
  return randomLoc;
}


void GenerateMapInSharedMem()
{

  //sem_wait(sem);

  for(int i = 0 ; i < (mapRows*mapCols); i++)
  {
    map->map[i] = gameBoard[i];
  }

  int randomLocation = getrandomLocation();
  for(int i =0; i < goldCoins-1; i++ )
  {    
    while(map->map[randomLocation] != 0)
    {
      randomLocation = getrandomLocation();
    }
    map->map[randomLocation] = map->map[randomLocation] | G_FOOL;
  }

  if(goldCoins > 0)
  {
      while(map->map[randomLocation] != 0)
      {
        randomLocation = getrandomLocation();
      }
      map->map[randomLocation] = map->map[randomLocation] | G_GOLD;    
  }

  map->someoneWon = false;

  //sem_post(sem);
}


void MovePlayers(int keyStroke)
{

 
  //cerr << "my msg \n";
  if(keyStroke == 'b')
  {
    // cerr << "pos 1 \n";
    string msg = globalMap->getMessage();
    //cerr << "fd=" << writequeue_fd << endl;    
    char message_text[250];
    // cerr << "pos 2 \n";        
    memset(message_text, 0, 250);
    // cerr << "pos 3 \n";
    strncpy(message_text, msg.c_str(), 250);
    // cerr << "pos 4 \n";

    
    // thisPlayer = -1;
    bool flag = false;
    
    for(int i = 0; i < 5; i++)
    {
      if(map->Players[i] != 0 && map->Players[i] != getpid())
      {
        //thisPlayer = i;
        //flag = true;
        //map->Players[i] = getpid();

        string mqLooping;
        if(i == 0 )
        {
          mqLooping = "/one";
        }
        if(i == 1 )
        {
          mqLooping = "/two";
        }
        if(i == 2 )
        {
          mqLooping = "/three";
        }
        if(i == 3 )
        {
          mqLooping = "/four";
        }
        if(i == 4 )
        {
          mqLooping = "/five";
        } 
        // cerr << "pos 5 \n";       
        if((writequeue_fd=mq_open(mqLooping.c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }
        // cerr << "pos 6 \n";
        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        // cerr << "pos 7 \n";
        mq_close(writequeue_fd);

        // break;
      }
    }

  }

  if(keyStroke == 'm')
  {
    unsigned int playermask = 0;
    int id = getpid();

    string msg = "Player "+ std::to_string(thisPlayer) + " says: ";
    msg = msg + globalMap->getMessage();
    //cerr << "fd=" << writequeue_fd << endl;    
    char message_text[250];
    // cerr << "pos 2 \n";        
    memset(message_text, 0, 250);
    // cerr << "pos 3 \n";
    strncpy(message_text, msg.c_str(), 250);

    
    if(map->Players[0] != 0 && map->Players[0] != getpid())
    {
      playermask |= G_PLR0;
    }
    if(map->Players[1] != 0 && map->Players[1] != getpid())
    {
      playermask |= G_PLR1;
    }
    if(map->Players[2] != 0 && map->Players[2] != getpid())
    {
      playermask |= G_PLR2;
    }
    if(map->Players[3] != 0 && map->Players[3] != getpid())
    {
      playermask |= G_PLR3;
    }
    if(map->Players[4] != 0 && map->Players[4] != getpid())
    {
      playermask |= G_PLR4;
    }


    unsigned int pl = globalMap->getPlayer(playermask);

    if(pl == G_PLR0)
    {
      string mqLooping = "/one";
      if((writequeue_fd=mq_open(mqLooping.c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }
        // cerr << "pos 6 \n";
        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        // cerr << "pos 7 \n";
        mq_close(writequeue_fd);
    }
    if(pl == G_PLR1)
    {
      string mqLooping = "/two";
      if((writequeue_fd=mq_open(mqLooping.c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }
        // cerr << "pos 6 \n";
        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        // cerr << "pos 7 \n";
        mq_close(writequeue_fd);
    }
    if(pl == G_PLR2)
    {
      string mqLooping = "/three";
      if((writequeue_fd=mq_open(mqLooping.c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }
        // cerr << "pos 6 \n";
        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        // cerr << "pos 7 \n";
        mq_close(writequeue_fd);
    }
    if(pl == G_PLR3)
    {
      string mqLooping = "/four";
      if((writequeue_fd=mq_open(mqLooping.c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }
        // cerr << "pos 6 \n";
        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }
        // cerr << "pos 7 \n";
        mq_close(writequeue_fd);
    }
    if(pl == G_PLR4)
    {
      string mqLooping = "/five";
      if((writequeue_fd=mq_open(mqLooping.c_str(), O_WRONLY|O_NONBLOCK))==-1)
        {
          perror("mq_open");
          exit(1);
        }        
        if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
        {
          perror("mq_send");
          exit(1);
        }        
        mq_close(writequeue_fd);
    }



  }

  if(keyStroke == 'h')
  {
    if((currentPosition%map->cols) > 0 && (map->map[currentPosition-1]) != G_WALL)
    {
      map->map[currentPosition] &= (~currentPlayer);
      map->map[currentPosition-1] |= (currentPlayer);
      currentPosition = currentPosition -1; 
      for (int i =0 ; i < 5; i++)
      {
        if (map->Players[i] != 0)
        {
          kill(map->Players[i], SIGUSR1);
        }
      }     
    }    
  }
  if(keyStroke == 'l')
  {
    if((currentPosition%map->cols != map->cols-1) && (map->map[currentPosition + 1]) != G_WALL)
    {
      map->map[currentPosition] &= (~currentPlayer);
      map->map[currentPosition+1] |= (currentPlayer);
      currentPosition = currentPosition +1;
      for (int i =0 ; i < 5; i++)
      {
        if (map->Players[i] != 0)
        {
          kill(map->Players[i], SIGUSR1);
        }
      }      
    }    
  }
  if(keyStroke == 'j')
  {
    if((currentPosition < (map->rows-1)*(map->cols)) && (map->map[currentPosition + map->cols]) != G_WALL)
    {
      // cerr << "key read j \n" ;
      map->map[currentPosition] &= (~currentPlayer);
      map->map[currentPosition+map->cols] |= (currentPlayer);
      currentPosition = currentPosition + map->cols;
      // cerr << "currentPosition: "<< currentPosition <<" \n" ;
      for (int i =0 ; i < 5; i++)
      {
        if (map->Players[i] != 0)
        {
          kill(map->Players[i], SIGUSR1);
        }
      }      
    }    
  }
  if(keyStroke == 'k')
  {
    if((currentPosition > (map->cols)) && (map->map[currentPosition - map->cols]) != G_WALL)
    {
      map->map[currentPosition] &= (~currentPlayer);
      map->map[currentPosition - map->cols] |= (currentPlayer);
      currentPosition = currentPosition - map->cols;
      for (int i =0 ; i < 5; i++)
      {
        if (map->Players[i] != 0)
        {
          kill(map->Players[i], SIGUSR1);
        }
      }      
    }    
  }

}

void clean_up(int)
{

  cerr << "Cleaning up message queue" << endl;
    
  bool flag = true;
  // map->Players[currentPlayer] = 0;
  map->Players[thisPlayer] = 0;
  cerr << "currentPlayer: " << currentPlayer << "\n"; 
  
  for(int i =0; i< 5; i++)
  {
    if(map->Players[i] != 0)
    {
      cerr << "i : " << i << "\n";
      flag = false;
    }
  }

  map->map[currentPosition]&=(~currentPlayer);          
  cerr << "keystroke q entered \n";
  //map->Players&= (~currentPlayer);
  //if(map->Players == 0)
  cerr << "releasing \n";    
  string mqLooping;
  if(thisPlayer == 0 )
  {
    mqLooping = "/one";
  }
  if(thisPlayer == 1 )
  {
    mqLooping = "/two";
  }
  if(thisPlayer == 2 )
  {
    mqLooping = "/three";
  }
  if(thisPlayer == 3 )
  {
    mqLooping = "/four";
  }
  if(thisPlayer == 4 )
  {
    mqLooping = "/five";
  }   
  mq_unlink(mqLooping.c_str());
  delete globalMap;
  exit(1);
}


void read_message(int)
{

  //set up message queue to receive signal whenever
  //message comes in. This is being done inside of
  //the handler function because it has to be "reset"
  //each time it is used.
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);

  //read a message
  int err;
  char msg[250];
  memset(msg, 0, 250);//set all characters to '\0'
  while((err=mq_receive(readqueue_fd, msg, 250, NULL))!=-1)
  {
    globalMap->postNotice(msg);
    //cout << "Message received: " << msg << endl;
    memset(msg, 0, 250);//set all characters to '\0'
  }
  //we exit while-loop when mq_receive returns -1
  //if errno==EAGAIN that is normal: there is no message waiting
  if(errno!=EAGAIN)
  {
    perror("mq_receive");
    exit(1);
  }
}

void ClientSighandler(int signal)
{
  if(signal == SIGHUP) // suspend process
  {
    unsigned char activeplayers=G_SOCKPLR;
    if(map->Players[0]!=0)
      activeplayers |= G_PLR0;
    if(map->Players[1]!=0)
      activeplayers |= G_PLR1;
    if(map->Players[2]!=0)
      activeplayers |= G_PLR2;
    if(map->Players[3]!=0)
      activeplayers |= G_PLR3;
    if(map->Players[4]!=0)
      activeplayers |= G_PLR4;
    WRITE(clientfd, &activeplayers, sizeof(activeplayers));
    if(activeplayers==G_SOCKPLR)
    {
      shm_unlink("/mapSHM");
      sem_close(sem);
      sem_unlink("/semSHM");
      exit(0);
    } 
  }
  else if(signal == SIGUSR1)
  {
    vector<pair<short, unsigned char> > sockmap;
    for(short i=0; i< map->rows*map->cols;i++)
    {
      if(local_map2[i]!=map->map[i])
      {
        sockmap.push_back(std::make_pair(i, map->map[i]));
        local_map2[i]=map->map[i];
      }
    }

    if(sockmap.size() > 0)
    {
      short n = sockmap.size();
      unsigned char byt = 0;
      WRITE(clientfd, &byt, sizeof(byt));
      WRITE(clientfd, &n, sizeof(n));
      for(short i=0; i< n; i++)
      {
        short offset= sockmap[i].first;
        unsigned char value = sockmap[i].second;
        WRITE(clientfd, &offset, sizeof(offset));
        WRITE(clientfd, &value, sizeof(value));
      }
    }

  }

}


void create_client_daemon(string ipaddr) 
{
  WRITE(2, "daemon created1\n", sizeof("daemon created1 "));
  int status;
  int clientrows, clientcols;

  unsigned char *clientsidemap;
  unsigned char clientPlayerSoc;
  if(fork()>0) 
  {
    return;
  }

  if(fork()>0) 
  {
    exit(0);
  }
  WRITE(2, "daemon creation started\n", sizeof("daemon creation started "));
  if(setsid()==-1) //child obtains its own SID & Process Group
    exit(1);
  for(int i=0; i<sysconf(_SC_OPEN_MAX); ++i) 
  {
      //if(i!=clientdpipe[1])
      close(i);
  }
  open("/dev/null", O_RDWR); //fd 0
  open("/dev/null", O_RDWR); //fd 1
  //open("/dev/null", O_RDWR); //fd 2
  int clfd=open("jayfifo", O_WRONLY);
  if(clfd==-1)
  {
    exit(99);
  }
  umask(0);
  chdir("/");

  //now do whatever you want the daemon to do
  WRITE(2, "daemon creation finished\n", sizeof("daemon creation finished "));
  const char* portno="25420";
  struct addrinfo hints;
  memset(&hints, 0, sizeof(hints)); //zero out everything in structure
  hints.ai_family = AF_UNSPEC; //don't care. Either IPv4 or IPv6
  hints.ai_socktype=SOCK_STREAM; // TCP stream sockets

  struct addrinfo *servinfo;
  //instead of "localhost", it could by any domain name
  if((status=getaddrinfo(ipaddr.c_str(), portno, &hints, &servinfo))==-1)
  {
    fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    exit(1);
  }
  clientfd=socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

  if((status=connect(clientfd, servinfo->ai_addr, servinfo->ai_addrlen))==-1)
  {
    perror("connect");
    exit(1);
  }
  //release the information allocated by getaddrinfo()
  //newclientfd=clientfd;
  freeaddrinfo(servinfo);
  READ(clientfd, &clientrows, sizeof(int));
  READ(clientfd, &clientcols, sizeof(int));
  unsigned char square;
  local_map2=new unsigned char[clientrows*clientcols];
  clientsidemap=new unsigned char[clientrows*clientcols];
  for(int i=0;i<clientrows*clientcols;i++)
  {
    READ(clientfd,&square, sizeof(unsigned char));
    clientsidemap[i]=square;
  }
  memcpy(local_map2, clientsidemap, clientrows*clientcols);

  sem=sem_open("/semSHM", O_CREAT,
                       S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH,1);
  sem_wait(sem);
  WRITE(2, "client demon creating shared memory\n", sizeof("client demon creating shared memory "));
  int shm_fd2=shm_open("/mapSHM",O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
  ftruncate(shm_fd2, (clientrows*clientcols)+sizeof(mapboard));
  map=(mapboard*)mmap(NULL, (clientrows*clientcols)+sizeof(mapboard), PROT_READ|PROT_WRITE,
    MAP_SHARED, shm_fd2, 0);
  map->daemonID=getpid();
  map->rows=clientrows;
  map->cols=clientcols;
  memcpy(map->map, local_map2, clientrows*clientcols);

  struct sigaction clientsig_struct;
  sigemptyset(&clientsig_struct.sa_mask);
  clientsig_struct.sa_flags=0;
  clientsig_struct.sa_restorer=NULL;
  clientsig_struct.sa_handler=ClientSighandler;
  sigaction(SIGHUP, &clientsig_struct, NULL);

  clientsig_struct.sa_handler=ClientSighandler;
  sigaction(SIGUSR1, &clientsig_struct, NULL);

  READ(clientfd, &clientPlayerSoc, sizeof(clientPlayerSoc));

  if(clientPlayerSoc & G_PLR1)
  {
    cerr << "got second player on client" << endl;
  }

  unsigned char playerbit[5]={G_PLR0, G_PLR1, G_PLR2, G_PLR3, G_PLR4};
  for(int i=0; i<5; i++)
  {
    if((clientPlayerSoc & playerbit[i]) && map->Players[i]==0)
    {
      map->Players[i]=getpid();
    }
  }
  WRITE(2, "client demon completed setup memory\n", sizeof("client demon completed setup memory "));
  sem_post(sem);

  // int msg=1;
  // WRITE(clientdpipe[1], &msg, sizeof(msg));
  while(true) 
  {
    unsigned char protocol, newmapbyte;
    short noOfchangedmap, offset;
    READ(clientfd, &protocol, sizeof(protocol));

    if(protocol & G_SOCKPLR)
    {
      for(int i=0;i<5; i++)
      {
        if(protocol & playerbit[i] && map->Players[i]==0)
        {
          map->Players[i]=getpid();
        }
        else if((protocol & playerbit[i])==false && map->Players[i]!=0)
        {
          map->Players[i]=0;
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

    else if(protocol==0) {

      READ(clientfd, &noOfchangedmap, sizeof(noOfchangedmap));
      for(short i=0;i<noOfchangedmap; i++)
      {
        READ(clientfd, &offset, sizeof(offset));
        READ(clientfd, &newmapbyte, sizeof(newmapbyte));
        map->map[offset]=newmapbyte;
        local_map2[offset]=newmapbyte;
      }
      for(int i=0;i<5; i++)
      {
        if(map->Players[i]!=0 && map->Players[i]!=getpid())
        {
          kill(map->Players[i], SIGUSR1);
        }
      }
    }
  }
  close(clientfd);
  delete local_map2;
}


void InitializePlayerPosition()
{
  //cerr << "Inside init\n";

  //perror("here");
  thisPlayer = -1;
  bool flag = false;

  int randomLoc = getrandomLocation();
  while(map->map[randomLoc]!=0)
  {
    randomLoc = getrandomLocation();
  }
  currentPosition = randomLoc;

  
  for(int i = 0; i < 5; i++)
  {
    if(map->Players[i] == 0)
    {
      thisPlayer = i;
      flag = true;
      map->Players[i] = getpid();
      break;
    }
  }

  // cerr << "init pos 1 \n";
  const char* mqnames[5]={"/one","/two","/three","/four","/five"};
    
  if(thisPlayer == 0)
  {
    currentPlayer = G_PLR0;
    currentPlayer_mq_name=mqnames[0];
    map->map[currentPosition] |= G_PLR0;        
  }
  else  if(thisPlayer == 1)
  {
    currentPlayer = G_PLR1;
    currentPlayer_mq_name=mqnames[1];
    map->map[currentPosition] |= G_PLR1;    
  }
  else if(thisPlayer == 2)
  {
    currentPlayer = G_PLR2;
    currentPlayer_mq_name=mqnames[2];
    map->map[currentPosition] |= G_PLR2;    
  }
  else if(thisPlayer == 3)
  {
    currentPlayer = G_PLR3;
    currentPlayer_mq_name=mqnames[3];
    map->map[currentPosition] |= G_PLR3;    
  }
  else if(thisPlayer == 4)
  {
    currentPlayer = G_PLR4;
    currentPlayer_mq_name=mqnames[4];
    map->map[currentPosition] |= G_PLR4;    
  }

  

  //I have added this signal-handling
  //code so that if you type ctrl-c to 
  //abort the long, slow loop at the
  //end of main, then your message queue
  //will be properly cleaned up.
  struct sigaction exit_handler;
  exit_handler.sa_handler=clean_up;
  sigemptyset(&exit_handler.sa_mask);
  exit_handler.sa_flags=0;
  sigaction(SIGINT, &exit_handler, NULL);
  sigaction(SIGHUP, &exit_handler, NULL);
  sigaction(SIGTERM, &exit_handler, NULL);

  //make sure we can handle the SIGUSR2
  //message when the message queue
  //notification sends the signal
  struct sigaction action_to_take;
  //handle with this function
  action_to_take.sa_handler=read_message; 
  //zero out the mask (allow any signal to interrupt)
  sigemptyset(&action_to_take.sa_mask); 
  action_to_take.sa_flags=0;
  //tell how to handle SIGINT
  sigaction(SIGUSR2, &action_to_take, NULL); 



  struct mq_attr mq_attributes;
  mq_attributes.mq_flags=0;
  mq_attributes.mq_maxmsg=10;
  mq_attributes.mq_msgsize=250;


  // cerr << "opening mqueue \n";
  if((readqueue_fd=mq_open(currentPlayer_mq_name.c_str(), 
    O_RDONLY|O_CREAT|O_EXCL|O_NONBLOCK,S_IRUSR|S_IWUSR, &mq_attributes))==-1)
  {
    perror("mq_open");
    exit(1);
  }
  // cerr << "opening mqueue done \n";

  //set up message queue to receive signal whenever message comes in
  struct sigevent mq_notification_event;
  mq_notification_event.sigev_notify=SIGEV_SIGNAL;
  mq_notification_event.sigev_signo=SIGUSR2;
  mq_notify(readqueue_fd, &mq_notification_event);


  if(flag == false)
  {
    cout << "Players exceeded \n";
    exit(1);
  }

}

void mapChange(int)
{

  if (mapPtr != NULL)
  {
    mapPtr->drawMap();
  }

}

int main(int argc, char *argv[])
{

  struct sigaction mapRef;
  mapRef.sa_handler = mapChange;
  sigemptyset(&mapRef.sa_mask);
  mapRef.sa_flags = 0;
  mapRef.sa_restorer = NULL;
  sigaction(SIGUSR1, &mapRef, NULL);
  
  //cout << "execution started" << endl;
  srand(time(NULL));

  // perror("here");
  
    if(argv[1]!=NULL) 
    {
       string ipaddr;
       ipaddr=argv[1];
       if(sem==SEM_FAILED)
       {
         create_client_daemon(ipaddr);
         wait(NULL);
         // will fail as long as mapSHM is not read
         while(shm_open("/mapSHM",O_RDWR, S_IRUSR|S_IWUSR)==-1) 
         {
           WRITE(2, "looping mapSHM opening\n",sizeof("looping mapSHM opening "));
           sleep(4);
         }
         WRITE(2, "shared memory found\n", sizeof("shared memory found "));
       }
      //  WRITE(2, "shared memory found\n", sizeof("shared memory found "));
     }


    //write(302, "Here!!!", sizeof("herekkk"));
    // try opening a sempahore assuming it is first player
    sem = sem_open("/semSHM", O_CREAT|O_EXCL, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH, 1);
 
    // semaphore creation failed but error is other than it already exists
    if(sem == SEM_FAILED && errno != EEXIST)
    {
      perror("somethings wrong with semaphore!");
      exit(1);
    }
 
    //cout << "sem not failed" << endl;

    // write(305, "my message", strlen("my message"));
    // if sem creation did not fail and errorno doesn't say it already exists
    // then it is the first player to start this game
    if(sem != SEM_FAILED)
    {      


      sem_wait(sem);
      // mapfile found. now read it
      //ReadMapFile();  
      ifstream mapFile;
      // write(302, "Here!!!", sizeof("herekkk"));
      mapFile.open("mymap.txt");
      // write(302, "Here2", sizeof("herek"));

      if(!mapFile)
      {
         cout << "Ain't no mymap.txt!" << endl;
        return -1;
      }

      mapRows = 0;
      mapCols = 0;

      getline(mapFile, eachLine);
      // no of gold coins are given in first line
      goldCoins = atoi(eachLine.c_str());
      getline(mapFile,eachLine);
      mapstr.append(eachLine);
      // took out 1st row in second getline
      mapRows++;
      // calculating no of columns
      mapCols = eachLine.length();

      while(getline(mapFile,eachLine))
      {
        //write(305,&eachLine, 4);
        mapstr.append(eachLine);
        mapRows++;
      }
      mapFile.close();
      // perror("here");
      sem_post(sem);

      // set gameboard in global array
      SetGameBoard();

      // get semaphore to create map in shared memory
      sem_wait(sem);
      // create a shared memory chunk for map
      shm_fd_map = shm_open("/mapSHM", O_CREAT|O_RDWR, S_IRUSR|S_IWUSR);
      ftruncate(shm_fd_map, (mapRows*mapCols)+sizeof(mapboard));    
      map = (mapboard*)mmap(NULL, ((mapRows*mapCols)+sizeof(mapboard)), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd_map, 0);
      map->rows = mapRows;
      map->cols = mapCols;
      //cerr<< "firstplayer shm_fd_map: " << shm_fd_map << "\n";
      //cerr << map->rows << " " << map->cols << endl;
      write(99, "Here1", sizeof("Here1"));
      GenerateMapInSharedMem();
      // release sempahore
      sem_post(sem);
    }
    else
    {

      //cerr << "Entered second player\n";
      sem = sem_open("/semSHM", O_RDWR);
      //shm_fd_map = shm_open("/mapSHM", O_CREAT|O_EXCL, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH);
      shm_fd_map = shm_open("/mapSHM", O_RDWR, S_IRUSR| S_IWUSR| S_IRGRP| S_IWGRP| S_IROTH| S_IWOTH);
      //cerr<< "subsequent player shm_fd_map: " << shm_fd_map << "\n";
      map = (mapboard*)mmap(NULL,(mapRows*mapCols)+sizeof(mapboard), PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd_map, 0);
      //cerr << "getting out\n";
    }  
  //cerr << "cols="<<map->cols << ", " << "rows=" << map->rows << " received by second\n";
  
  // cerr <<"before initializeplayer \n";
  sem_wait(sem);
  InitializePlayerPosition();
  sem_post(sem);

  //Map Board((unsigned char*)map->map ,map->rows, map->cols);
  globalMap=new Map((unsigned char*)map->map ,map->rows, map->cols);
  Map& Board=*globalMap;
  Board.postNotice("GameBoard notice");

  if(map->daemonID==0)
  {
    create_server_daemon();
    wait(NULL);
  }
  else
  {
    kill(map->daemonID, SIGHUP);
    kill(map->daemonID, SIGUSR1);
  }


  while(1)
  {

    int keyStroke = Board.getKey();
    // cerr << "key=" << keyStroke << " ";
    if(keyStroke == 'Q')
    {


      sem_wait(sem);
      bool flag = true;
      // map->Players[currentPlayer] = 0;
      map->Players[thisPlayer] = 0;
      // cerr << "currentPlayer: " << currentPlayer << "\n"; 
      
      for(int i =0; i< 5; i++)
      {
        if(map->Players[i] != 0)
        {
          // cerr << "i : " << i << "\n";
          flag = false;
        }
      }

      map->map[currentPosition]&=(~currentPlayer);          
      // cerr << "keystroke q entered \n";

      string mqLooping;
      if(thisPlayer == 0 )
      {
        mqLooping = "/one";
      }
      if(thisPlayer == 1 )
      {
        mqLooping = "/two";
      }
      if(thisPlayer == 2 )
      {
        mqLooping = "/three";
      }
      if(thisPlayer == 3 )
      {
        mqLooping = "/four";
      }
      if(thisPlayer == 4 )
      {
        mqLooping = "/five";
      } 
      mq_unlink(mqLooping.c_str());
      //map->Players&= (~currentPlayer);
      //if(map->Players == 0)
      if(flag == true)
      {
        // cerr << "releasing \n";
        sem_close(sem);        
        sem_unlink("/semSHM");
        shm_unlink("/mapSHM"); 
        exit(0);             
      }
      delete globalMap;
      if(flag == false)
      {
        sem_post(sem);  
      }      
      return 0;
    
    }



    sem_wait(sem);
    MovePlayers(keyStroke);

    // get the map after moving the player once again
    Board.drawMap();
    sem_post(sem);

    if(map->map[currentPosition]&G_FOOL)
    {
      Board.postNotice("This is fake gold");
    }

    if(map->map[currentPosition]&G_GOLD && map->someoneWon == false)
    {

      bool exitedMap = false;
      Board.postNotice("You have real Gold!");

      

      while(!exitedMap)
      {
        keyStroke = Board.getKey();
        MovePlayers(keyStroke);
        Board.drawMap();

        if(currentPosition%map->cols == 0 || currentPosition%map->cols == map->cols - 1 
          || currentPosition/map->cols == 0 || currentPosition/map->cols == map->rows - 1 )
        {
          exitedMap = true;
          Board.postNotice("You won this game!");
          map->someoneWon = true;

          string msg = "Player " + to_string(thisPlayer) + " has won";
          char message_text[250];              
          memset(message_text, 0, 250);          
          strncpy(message_text, msg.c_str(), 250);

          // check if no player exists in game
          bool flag = true;
          for(int i =0; i< 5; i++)
          {
            if(map->Players[i] != 0)
            {
              flag = false;
            }
          }

          for(int i = 0; i < 5; i++)
          {
            if(map->Players[i] != 0 && map->Players[i] != getpid())
            {
              string mqLooping;
              if(i == 0 )
              {
                mqLooping = "/one";
              }
              if(i == 1 )
              {
                mqLooping = "/two";
              }
              if(i == 2 )
              {
                mqLooping = "/three";
              }
              if(i == 3 )
              {
                mqLooping = "/four";
              }
              if(i == 4 )
              {
                mqLooping = "/five";
              } 
              if((writequeue_fd=mq_open(mqLooping.c_str(), O_WRONLY|O_NONBLOCK))==-1)
              {
                perror("mq_open");
                exit(1);
              }              
              if(mq_send(writequeue_fd, message_text, strlen(message_text), 0)==-1)
              {
                perror("mq_send");
                exit(1);
              }              
              mq_close(writequeue_fd);              
            }
          }



          //ExitGame
          sem_wait(sem);
          map->map[currentPosition]&=(~currentPlayer);
          //map->Players&= (~currentPlayer);
          map->Players[thisPlayer] = 0;
          string mqLooping;
          if(thisPlayer == 0 )
          {
            mqLooping = "/one";
          }
          if(thisPlayer == 1 )
          {
            mqLooping = "/two";
          }
          if(thisPlayer == 2 )
          {
            mqLooping = "/three";
          }
          if(thisPlayer == 3 )
          {
            mqLooping = "/four";
          }
          if(thisPlayer == 4 )
          {
            mqLooping = "/five";
          } 
          mq_unlink(mqLooping.c_str());
          //if(map->Players == 0)
          if (flag == true)
          {
            sem_close(sem);          
            sem_unlink("/semSHM");
            shm_unlink("/mapSHM"); 
            exit(0);           

          }
          delete globalMap;
          if(flag == false)
          {
            sem_post(sem);  
          }
          return 0;
        }
        if( keyStroke == 'Q')
        {          
          //ExitGame
          sem_wait(sem);
          bool flag = true;
          map->Players[thisPlayer] = 0;
          for(int i =0; i< 5; i++)
          {
            if(map->Players[i] != 0)
            {
              flag = false;
            }
          }

          map->map[currentPosition]&=(~currentPlayer);
          string mqLooping;
          if(thisPlayer == 0 )
          {
            mqLooping = "/one";
          }
          if(thisPlayer == 1 )
          {
            mqLooping = "/two";
          }
          if(thisPlayer == 2 )
          {
            mqLooping = "/three";
          }
          if(thisPlayer == 3 )
          {
            mqLooping = "/four";
          }
          if(thisPlayer == 4 )
          {
            mqLooping = "/five";
          } 
          mq_unlink(mqLooping.c_str());          
          //map->Players&= (~currentPlayer);
          //if(map->Players == 0)
          if(flag == true)
          {
            sem_close(sem);          
            sem_unlink("/semSHM");
            shm_unlink("/mapSHM");
            // delete globalMap;
            exit(0);
          }
          delete globalMap;
          if(flag == false)
          {
            sem_post(sem);  
          }
          return 0;
        }

      }



    }


  }

 

 return 0;

}
