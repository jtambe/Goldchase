/*
 * write template functions that are guaranteed to read and write the 
 * number of bytes desired
 */

#ifndef fancyRW_h
#define fancyRW_h
#include <iostream>
#include <unistd.h>
#include <errno.h>
using namespace std;

template<typename T>
int READ(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  //loop. Read repeatedly until count bytes are read in

  int endOfFile = 0;
  int noOfbytes = count;

  while(noOfbytes > 0)
  {
  	endOfFile = read(fd, addr, noOfbytes);
  	if( endOfFile < 0) // on error -1 is returned
  	{
  		if(errno == EINTR) // handle given error type as per document
  		{
  			endOfFile = 0; // error, 0 bytes read
  		}
  		else // otherwise return -1
  		{
  			return -1;
  		}  		
  	}
  	else if(endOfFile == 0) // file done reading
  	{
  		break;
  	}

  	addr = addr + endOfFile;
  	noOfbytes = noOfbytes - endOfFile;
  }

  return count;

}

template<typename T>
int WRITE(int fd, T* obj_ptr, int count)
{
  char* addr=(char*)obj_ptr;
  //loop. Write repeatedly until count bytes are written out

  int endOfFile = 0;
  int noOfbytes = count;
  
  while(noOfbytes > 0)
  {
  	endOfFile = write(fd, addr, noOfbytes);
  	if( endOfFile < 0) // on error -1 is returned
  	{
  		if(errno == EINTR) // handle given error type as per document
  		{
  			endOfFile = 0;
  		}
  		else // otherwise return -1
  		{
  			return -1;
  		}  		
  	}
  	// else if(endOfFile == 0) // file done reading
  	// {
  	// 	break;
  	// }
  	addr = addr + endOfFile;
  	noOfbytes = noOfbytes - endOfFile;
  }

  return count;
}
#endif

//Example of a method for testing your functions shown here:
//opened/connected/etc to a socket with fd=7
//
// int count=write(7, "Todd Gibson", 11);
// cout << count << endl;//will display a number between 1 and 11
// int count=write(7, "d Gibson", 8);
// //
// //How to test your READ template. Read will be: READ(7, ptr, 11);
// write(7, "To", 2);
// write(7, "DD ", 3);
// write(7, "Gibso", 5);
// write(7, "n", 1);
//
//
