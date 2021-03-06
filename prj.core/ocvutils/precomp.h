// precomp.h -- project default context -- starter for .cpp files
#ifndef __PRECOMP_H
#define __PRECOMP_H


#include <cassert>
#include <climits>

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <algorithm>

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp> 

#include "ocvutils/ocvutils.h"
#include "soundui/soundui.h"

#ifdef _WINDOWS
#include <conio.h> 
#include <windows.h>
#undef min
#undef max
#else
#endif

using namespace std;
using namespace cv;

inline void dbgPressAnyKey( int sound_ui_mood = 0 )
{
#ifdef _DEBUG
  printf("\n\nPress any key...");
  SoundUI(sound_ui_mood);
  _getch();
#endif
}

inline bool __false( std::string message = "__false():" )
{
  cout << message;
  dbgPressAnyKey( SUI_Alert );
  return false;
}

inline bool __true( std::string message = "" )
{
  SoundUI( SUI_Ok );
  cout << message;
  return true;
}

inline bool file_readable( const char* name )
{
  ifstream ifs( name );
  return ifs.good();
}


inline string name_and_extension( const string &filename )
{
  // Remove directory if present.
  // Do this before extension removal incase directory has a period character.
  const size_t last_slash_idx = filename.find_last_of("\\/");
  string newfname = filename;
  if (std::string::npos != last_slash_idx)
  {
    newfname.erase(0, last_slash_idx + 1);
  }
  return newfname;
}

inline void set_window_text( const char* title, const char* text )
{
#ifdef _WINDOWS
  HWND hWnd = (HWND)cvGetWindowHandle(title);
	HWND hPar = GetParent(hWnd);
  SetWindowText(hPar, text );
#endif 
}

inline bool ensure_folder( string folder )
{
#ifdef _WINDOWS
  if (CreateDirectory(folder.c_str(), NULL) ||
    ERROR_ALREADY_EXISTS == GetLastError())
    return true;
#endif
  cout << "Can't create directory " << folder << endl;
  return false;
}

#endif // __PRECOMP_H