#ifndef MAPBOARD_H
#define MAPBOARD_H
//#include "serverdaemon.cpp"
struct mapboard
{

  int rows;
  int cols;
  bool someoneWon; // need to store this in shared memory for every shell to access
  //unsigned char Players;
  pid_t daemonID;
  pid_t Players[5];
  unsigned char map[0];
};

void create_server_daemon();
//create_server_daemon();
#endif