#include "gnss/gnss.h"
#include <string>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <opencv2/imgproc/imgproc.hpp> 
#include <cmath>
#define _USE_MATH_DEFINES

#include <markup/kitti.h>
#include <ocvutils/precomp.h> 
#include <mainframe/mainframe.h>
#include <ctime>
using namespace std;


/*
example of .nmea record produced by akenori
---------------
$GPRMC,090450.00,A,5872.77527,N,03674.93726,E,13.517,0.07,271114,,,A*59
$GSENSOR,336,96,960
$GSENSOR,240,0,1008
$GSENSOR,320,64,912
$GSENSOR,416,-64,1008
$GSENSOR,272,48,928
$GPVTG,0.07,T,,M,13.517,N,25.033,K,A*3C
$GPGGA,090450.00,5872.77527,N,03674.93726,E,1,09,0.96,132.0,M,12.4,M,,*5C
$GPGSA,A,3,22,27,18,19,21,15,04,14,16,,,,1.56,0.96,1.23*0A
$GPGSV,4,1,13,03,29,224,,04,16,279,40,14,13,160,26,15,26,049,37*74
$GPGSV,4,2,13,16,11,224,19,18,62,082,45,19,51,291,43,21,37,104,39*79
$GPGSV,4,3,13,22,67,195,41,26,02,017,,27,64,236,37,28,01,354,23*79
$GPGSV,4,4,13,30,00,327,*4E
$GPGLL,5872.77527,N,03674.93726,E,090450.00,A,A*68
<empty line>
-----------------

5872.77527,N,03674.93726,E, --- where (58 grad 72.77527')
090450.00 --- when (utc)

http://www.radioscanner.ru/info/article166/ description


�	$GPGGA ��������� �������� GPS ������ � ��������������, ������� ����������������, �������� ������, ���������� �������������� ���������, HDOP (������ ��������� �������� �������� ���������), ���������� � ���������������� ��������� � �� �������. 
�	$GPGLL ��������� �������� GPS������� � �������������� ������, ������� � ������� ����������� ���������. 
�	$GPGSA � ���� ��������� ������������ ����� ������ GPS ��������, ��������� ���������, ������������ ��� ������� ������������� ������, ���������� ������� ���������� � ��������� $GPGGA � �������� �������� �������� ����������� ���������. 
�	$GPGSV � ��������� ����������� ���������� ������� ���������, �� ������, ����������, ������, � �������� ��������� ������/��� ��� ������� �� ���. 
�	$GPRMC ��������� RMC �������� ������ � �������, ��������������, ����� � ��������, ������������ ������������� GPS ���������. ����������� ����� ����������� ��� ����� ���������, ��������� �������� �� ������ ��������� 2 �������. ��� ���� ������ ������ ���� ������������, ���� ��� ��� ����� ������. ���������������� ���� ����� ���� ������������, ���� ������ �������� �� ������. 
�	$GPVTG ��������� VTG �������� ������� �������� ����������� ����� (COG) � �������� ������������ ����� (SOG). 
�	$GPZDA ��������� ZDA �������� ���������� � ������� �� UTC, ����������� ����, �����, ��� � ��������� ������� ����. 


*/
bool eat_comma( string& line ) // �������� ��� �� �������. ���� ������� ��� -- ������� false
{
  size_t pos = line.find_first_of(',');
  if (pos == string::npos)
    return false;
  line = line.substr( pos+1, line.length()-pos-1 );
  return true;
}

double normalize_to_decimal_grad( double x ) 
  // �� ����� �������� �����������
  // ggmm.dddd[d] ��� gg -- �������, mm.dddd ������ � ������ ������
{ // �������� � �������� � ������ ������
  double grad = int(x) / 100; // �������� ����� ����� ��������
  double minu = x - grad*100; // �������� ������ � ������� ������
  double res = grad + (minu / 60);
  return res;
} // ���� ����� ���������, ��� �� ������� �� ������ � ������ �������� ������

double GetSecsFromTimeString(const string &timestring) //string format: ddmmyyhhmmss.ss
{
  tm time = {0};
  int msec = 0;  
  sscanf(timestring.c_str(), "%2d%2d%2d%2d%2d%2d.%d", &time.tm_mday, &time.tm_mon,
    &time.tm_year, &time.tm_hour, &time.tm_min, &time.tm_sec, &msec);
  time.tm_year += 2000 - 1900; //+2000 because year is 14 instead of 2014,
                               //-1900 because this is how mktime works (epoch)
  time_t dtime = mktime(&time);
  if (dtime == -1)
    return -1.0;
  return static_cast<double> (dtime) + static_cast<double> (msec) / 100.0;
}

bool gprmc( string& line, GNSSRecord& rec )
{ //$GPRMC,090450.00,A,5872.77527,N,03674.93726,E,13.517,0.07,271114,,,A*59
  /*
  RMC � p������������ ������� GPS / ������������� ������ 
  1	2	3	4	5	6	7	8	9	10	11	12	
  $GPRMC,	Hhmmss.ss,	A,	1111.11,	A,	yyyyy.yy,	a,	x.x ,	x.x,	ddmmyy,	x.x,	A	*hh	<CR><LF> 
  1. ����� �������� �������������� UTC 
  2. ���������: � = ��������������, V = �������������� �������������� �������� 
  3,4. �������������� ������ ��������������, �����/�� 
  5,6. �������������� ������� ��������������, �����/������ (E/W) 
  7. �������� ��� ������������ (SOG) � ����� 
  8. �������� ����������� ����� � �������� 
  9. ����: dd/mm/yy 
  10. ��������� ��������� � �������� 
  11. �����/������ (E/W) 
  12. ����������� ����� ������ (�����������) 
  ������ ���������: 
  $GPRMC,113650.0,A,5548.607,N,03739.387,E,000.01,25 5.6,210403,08.7,E*69 

  */

  //split gprmc string
  istringstream istr(line);
  string word;
  vector<string> gprmcWords;
  gprmcWords.reserve(13); //13 records in gprmc string

  while (getline(istr, word, ','))
    gprmcWords.push_back(word);

  //get time of gprmc record
  rec.time = GetSecsFromTimeString(gprmcWords[9] + gprmcWords[1]);
  if (rec.time < 0.)
    return false;

  //check if gprmc record is good
  if (gprmcWords[2] != "A")
    return false;

  //get nord-east coordinates
  rec.nord = normalize_to_decimal_grad(strtod(gprmcWords[3].c_str(), NULL));
  if (gprmcWords[4] != "N")
    rec.nord *= -1;
  rec.east = normalize_to_decimal_grad(strtod(gprmcWords[5].c_str(), NULL));
  if (gprmcWords[6] != "E")
    rec.nord *= -1;

  //get yaw
  rec.yaw = CV_PI * (strtod(gprmcWords[8].c_str(), NULL)) / 180.0;

  return true;

  /*


  
  size_t pos = string::npos;
  size_t len = line.length();

  // cut command "$GPRMC,"
  string command;
  pos = line.find_first_of(',');
  if (pos != string::npos)
  {
    command = line.substr( 0, pos );
    len -= pos+1;
    line = line.substr( pos+1, len );
  } else 
    return false;

  // cut time "090450.00,"
  string utc_time;
  pos = line.find_first_of(',');
  if (pos != string::npos)
  {
    utc_time = line.substr( 0, pos );
    len -= pos+1;
    line = line.substr( pos+1, len );
  } else 
    return false;
  double hhmmss_ss = atof( utc_time.c_str() );
  int hhmmss = int( hhmmss_ss ); // ��������� ��� ����� ����� ����� -- ���� ������� (HHMMSS.SS)
  int ss = hhmmss%100; // ���������� ������ ������ ����� ������ ������� ������
  double _ss = hhmmss_ss - hhmmss; // ���������� ������� ������
  int hhmm = hhmmss/100; // ��������� ��� ����� ����� ���������� -- ������� (HHMMSS)
  int mm = hhmm%100; // ���������� ������ ����� ����� ������ �������� ���� �����
  int hh = hhmm/100; // ���������� ������ ����� ����� ��������
  rec.time = (hh*60+mm)*60 + ss + _ss;

  eat_comma(line); // skip "A," 

  string nord;
  pos = line.find_first_of(',');
  if (pos != string::npos)
  {
    nord = line.substr( 0, pos );
    len -= pos+1;
    line = line.substr( pos+1, len );
  } else 
    return false;
  rec.nord = normalize_to_decimal_grad( atof( nord.c_str() ) );

  eat_comma(line); // skip "N,"

  string east;
  pos = line.find_first_of(',');
  if (pos != string::npos)
  {
    east = line.substr( 0, pos );
    len -= pos+1;
    line = line.substr( pos+1, len );
  } else 
    return false;
  rec.east = normalize_to_decimal_grad( atof( east.c_str() ) );

  // todo .. direction etc

  return true; */
}

std::istream & operator>>(std::istream &istr, GNSSRecord &gnss)
{
  double tmp;
  istr >> gnss.nord >> gnss.east >> tmp >> gnss.roll >> gnss.pitch >> gnss.yaw;
  return istr;
}


bool NMEA::load( const std::string& filename ) // currently ".nmea" -- akenori OR ".gps" -- blackvue,
{
  ifstream ifs( filename.c_str() );
  string line;
  if (!ifs.is_open())
    return false;

  double start_time = -1.;

#if 0 // todo
  string format; // "oxts/data" -- kitti style format, ".nmea" -- akenori, ".gps" -- blackvue, 
#endif

  while ( std::getline( ifs, line ) )
  {
    if (line.empty()) // spaces?
      continue;

    if( line.size() > 0 && line[0] == '[' ) // ".gps" -- blackvue
	{
        int start_pos = line.find( "$GP" );
        line = std::string( line, start_pos, line.size() - start_pos );
    }

    if (line.find( "$GP" ) != 0)
      continue;

    if (line.find( "$GPRMC" ) == 0) //$GPRMC,090450.00,A,5872.77527,N,03674.93726,E,13.517,0.07,271114,,,A*59
      /// �	$GPRMC ��������� RMC �������� ������ � �������, ��������������, ����� � ��������, 
      /// ������������ ������������� GPS ���������. ����������� ����� ����������� ��� ����� ���������, 
      /// ��������� �������� �� ������ ��������� 2 �������. ��� ���� ������ ������ ���� ������������, ���� ��� 
      /// ��� ����� ������. ���������������� ���� ����� ���� ������������, ���� ������ �������� �� ������. 
    {
      GNSSRecord rec;
      gprmc( line, rec );
      if (start_time < 0)
        start_time = rec.time;
      rec.time -= start_time;
      if (rec.time < -12*60*60) // ���������� ����� ������� �� UTC
        rec.time += 24*60*60; // todo TEST!

      records.push_back(rec);
    }
  }

  return true;
}


bool NMEA::loadKitti( const std::string &filename )
{
  string timestampfname = filename + "/oxts/timestamps.txt";
  vector<double> timestamps;

  if (!readTimeStamps(timestampfname, timestamps))
  {
    cout << "error reading timestamps file " << timestampfname << endl;
    return false;
  }
  for (unsigned int i = 0; i < timestamps.size(); ++i)
  {
    string oxtfname = filename + format("/oxts/data/%010d.txt", i);
    ifstream fin(oxtfname.c_str());
    if (!fin.good())
    {
      cout << "error reading oxt (gnss) record number " << i << endl;
      return false;
    }
    GNSSRecord gnss;
    fin >> gnss;
    gnss.time = timestamps[i] - timestamps[0];
    records.push_back(gnss);
  }
  return true;
}


inline bool compareByTime( const GNSSRecord& rec, const GNSSRecord& val )
{
  return rec.time < val.time;
}

bool NMEA::getEastNord( double time, double& east, double& nord ) const
{
  if (records.size() <= 0)
    return false;

  GNSSRecord rec(time);
  vector< GNSSRecord >::const_iterator low = std::lower_bound( records.begin(), records.end(), rec, compareByTime );
  if (low == records.end() && records.size() > 0)
  {
    assert( records.back().time < time );
    if (time - records.back().time <= 1.)
      low--;
    else
      return false;
  }
  if (low == records.end()-1)
  {
    east = low->east;
    nord = low->nord;
    return true;
  }

// interpolate
  GNSSRecord a = *low;
  GNSSRecord b = *(low+1);
  east = a.east + (b.east - a.east)*(time-a.time) / (b.time-a.time); 
  nord = a.nord + (b.nord - a.nord)*(time-a.time) / (b.time-a.time);
  return true;
}

////////////////////////////////////////////////////////////////
// OpenCv specific part
#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

using namespace cv;

void NMEA::draw()
{
  Mat3b display(300,200);
  if (records.size() < 1)
    return;

  double mi_nord = records[0].nord;
  double ma_nord = records[0].nord;
  double mi_east = records[0].east;
  double ma_east = records[0].east;
  for (int i=1; i < int( records.size() ); i++)
  {
    mi_nord = min( records[i].nord, mi_nord );
    ma_nord = max( records[i].nord, ma_nord );
    mi_east = min( records[i].east, mi_east );
    ma_east = max( records[i].east, ma_east );
  }
  
  Scalar col( 255,0,255, 0);
  double alpha =  display.cols / (ma_east-mi_east);
  double betha =  display.rows / (ma_nord-mi_nord);
  for (int i=1; i < int( records.size() );i++)
  {
    Point2d p1( ( records[i-1].east - mi_east) * alpha, display.rows - ( records[i-1].nord - mi_nord ) * betha );
    Point2d p2( ( records[i].east   - mi_east) * alpha, display.rows - ( records[i].nord   - mi_nord ) * betha );
    line( display, p1, p2, col, 2 );
  }

  imshow("NMEA", display );

  WaitKey(0);
}

//////////////////////////////////////
#if 0
NMEA � ��� ������ �������� ��������� ����� ������������ ���������. �� �������� � ���� ������� ��������� ��� ������ ����������� ����� �������������� GPS ���������� � ������������� ������������� ����������. ��� ������� � ��������� ���������� � ��������� ASCII ����, ����������� � GPS ��������� ���������� � $GP, � ����� ������ ��������� ������ ���� ������� <CR><LF>. � ��������� ���� ��������� ����� ���� ������� ����������� ����� �������� ���������, ������������ � ����������� *. ����������� ����� 8 � �� ������ (����������� ���) ���� �������� ���������, ������� �������, ������������� ����� ������������� $ � *, �� ������� ���������. ����������������� ��������� ����������� � ��� ASCII ������� (0-9, A-F). 

���������� ��������� ��������� ��������� NMEA ������ 2.1 

�	$GPGGA ��������� �������� GPS ������ � ��������������, ������� ����������������, �������� ������, ���������� �������������� ���������, HDOP (������ ��������� �������� �������� ���������), ���������� � ���������������� ��������� � �� �������. 
�	$GPGLL ��������� �������� GPS������� � �������������� ������, ������� � ������� ����������� ���������. 
�	$GPGSA � ���� ��������� ������������ ����� ������ GPS ��������, ��������� ���������, ������������ ��� ������� ������������� ������, ���������� ������� ���������� � ��������� $GPGGA � �������� �������� �������� ����������� ���������. 
�	$GPGSV � ��������� ����������� ���������� ������� ���������, �� ������, ����������, ������, � �������� ��������� ������/��� ��� ������� �� ���. 
�	$GPRMC ��������� RMC �������� ������ � �������, ��������������, ����� � ��������, ������������ ������������� GPS ���������. ����������� ����� ����������� ��� ����� ���������, ��������� �������� �� ������ ��������� 2 �������. ��� ���� ������ ������ ���� ������������, ���� ��� ��� ����� ������. ���������������� ���� ����� ���� ������������, ���� ������ �������� �� ������. 
�	$GPVTG ��������� VTG �������� ������� �������� ����������� ����� (COG) � �������� ������������ ����� (SOG). 
�	$GPZDA ��������� ZDA �������� ���������� � ������� �� UTC, ����������� ����, �����, ��� � ��������� ������� ����. 

GGA - GPS ������ � �������������� 
1	2	3	4	5	6	7	8	9	10	11	12	13	14 15 
$GPGGA,	hhmmss.ss,	1111.11,	a,	yyyyy.yy,	a,	x,	xx, x.x,	xxx,	M,	x.x,	M,	x.x,	xxxx*hh 
1. ����������� ����� �� ������ ����������� ��������������. 
2. �������������� ������ ��������������. 
3. �����/�� (N/S). 
4. �������������� ������� ��������������. 
5. �����/������ (E/W). 
6. ��������� �������� GPS �������: 
0 = ����������� �������������� �� �������� ��� �� �����; 
1 = GPS ����� ������� ��������, �������� ����������� ��������������; 
2 = ���������������� GPS �����, �������� �������, �������� ����������� ��������������; 
3 = GPS ����� ������������ ��������, �������� ����������� ��������������. 
7. ���������� ������������ ��������� (00-12, ����� ���������� �� ����� �������). 
8. ������ ��������� �������� �������� ��������� (HDOP). 
9. ������ ������� �������� ���/���� ������ ����. 
10. ������� ��������� ������ ������������ �������, �����. 
11. ����������� �������� - �������� ����� ������ ����������� WGS-84 � ������� ����(�������), �-� = ������� ���� ���� ����������. 
12. ������� ��������� ��������, �����. 
13. ������� ���������������� ������ GPS - ����� � �������� � ������� ���������� SC104 ���� 1 ��� 9 ����������, ��������� ������, ���� ���������������� ����� �� ������������. 
14. ����������� �������, ���������� ���������������� ��������, ID, 0000-1023. 
15. ����������� ����� ������. 
������ ���������: 
$GPGGA,004241.47,5532.8492,N,03729.0987,E,1,04,2.0 ,-0015,M,,,,*31 

GLL - �������������� ��������� � ������/������� 
1	2	3	4	5	6 7	
$GPGLL,	1111.11,	a,	yyyyy.yy,	a,	hhmmss.ss,	A*hh	< CR><LF> 
1. �������������� ������ ��������������. 
2. �����/�� (N/S). 
3. �������������� ������� ��������������. 
4. �����/������ (E/W). 
5. ����������� ����� �� ������ ����������� ��������������. 
6. ������ A = ������ ����� 
V = ������ �� ����� 
7. ����������� ����� ������. 
������ ���������: 
$GPGLL,5532.8492,N,03729.0987,E,004241.469,A*33 

GSA - GPS ������� �������� � �������� �������� 
� ���� ��������� ������������ ����� ������ GPS ��������, ��������� ���������, ������������ ��� ������� ������������� ������, ���������� ������� ���������� � ��������� $GPGGA � �������� �������� �������� ����������� ���������. 
1	2	3	4	5	6	7	8	9	10	11	12	13	14	15	16	17 18	
$GPGSA,	a,	x,	xx,	xx,	xx,	xx,	xx,	xx,	xx,	xx,	xx,	xx,	xx,	xx,	x.x,	x.x,	x.x*hh	<CR><LF> 
1. �����: M = ������, ������������� ������� 2D ��� 3D �����; 
A = ��������������, ��������� �������. �������� 2D/3D. 
2. �����: 1 = �������������� �� ����������, 2 = 2D, 3 = 3D 
3-14. PRN ������ ���������, �������������� ��� ������� ������ ���������������� (���� ��� ����������������). 
15. ������ PDOP. 
16. ������ HDOP. 
17. ������ VDOP. 
18. ����������� ����� ������. 
������ ���������: 
$GPGSA,A,3,01,02,03,04,,,,,,,,,2.0,2.0,2.0*34 

GSV - ������� �������� GPS 
� ���� ��������� ������������ ����� ������� ���������(SV), PRN ������ ���� ���������, �� ������ ��� ������� ����������, ������ � ��������� ������/���. � ������ ��������� ����� ���� ���������� �� ����� ��� � ������� ���������, ��������� ������ ����� ���� ����������� � ��������� �� ������� $GPGSV ����������. ������ ����� ������������ ��������� � ����� �������� ��������� ������� � ������ ���� ����� ������� ���������. 
1	2	3	4	5	6	7 8 15	16	17	18	19 20	
$GPGSV,	x,	x,	xx,	xx,	xx,	xxx,	xx...........,	xx,	xx,	xxx,	xx*hh	<CR><LF> 
1. ������ ����� ���������, �� 1 �� 9. 
2. ����� ���������, �� 1 �� 9. 
3. ������ ����� ������� ���������. 
4. PRN ����� ��������. 
5. ������, �������, (90� - ��������). 
6. ������ ��������, �������, �� 000� �� 359�. 
7. ��������� ������/��� �� 00 �� 99 ��, ���� - ����� ��� �������. 
8-11. ����, ��� � 4-7 ��� ������� ��������. 
12-15. ����, ��� � 4-7 ��� �������� ��������. 
16-19. ����, ��� � 4-7 ��� ���������� ��������. 
20. ����������� ����� ������. 
������ ���������: 
$GPGSV,3,1,12,02,86,172,,09,62,237,,22,39,109,,27, 37,301,*7A 
$GPGSV,3,2,12,17,28,050,,29,21,314,,26,18,246,,08, 10,153,*7F 
$GPGSV,3,3,12,07,08,231,,10,08,043,,04,06,170,,30, 00,281,*77 

RMC � p������������ ������� GPS / ������������� ������ 
1	2	3	4	5	6	7	8	9	10	11	12	
$GPRMC,	Hhmmss.ss,	A,	1111.11,	A,	yyyyy.yy,	a,	x.x ,	x.x,	ddmmyy,	x.x,	A	*hh	<CR><LF> 
1. ����� �������� �������������� UTC 
2. ���������: � = ��������������, V = �������������� �������������� �������� 
3,4. �������������� ������ ��������������, �����/�� 
5,6. �������������� ������� ��������������, �����/������ (E/W) 
7. �������� ��� ������������ (SOG) � ����� 
8. �������� ����������� ����� � �������� 
9. ����: dd/mm/yy 
10. ��������� ��������� � �������� 
11. �����/������ (E/W) 
12. ����������� ����� ������ (�����������) 
������ ���������: 
$GPRMC,113650.0,A,5548.607,N,03739.387,E,000.01,25 5.6,210403,08.7,E*69 

VTG � �������� ����������� ����� � �������� ������������ ����� 
1	2	3	4	5	
$GPVTG,	x.x, T	x.x, M	x.x, N	x.x, K	*hh	<CR><LF> 
1. ����������� ����� � ��������, T 
2. ��������� ��������� � ��������, � 
3. �������� ��� ������������ (SOG) � �����, N = ���� 
4. �������� ��� ������������ (SOG) � ��/�, � = ��/� 
5. hh ����������� ����� ������ (�����������) 
������ ���������: 
$GPVTG,217.5,T,208.8,M,000.00,N,000.01,K*4C 

ZDA � ����� � ���� 
1	2	3	4	5	6	7	
$GPZDA,	hhmmss.s,	xx,	xx,	xxxx,	xx,	xx	*hh	<CR><LF > 
1.	����� UTC 
2.	���� (01�� 31) 
3.	����� (01 to 12) 
4.	��� 
5.	������� ����, �������� �� GMT, �� 00 �� � 13 ����� 
6.	������� ����, �������� �� GMT, ������ 
7.	hh ����������� ����� ������ 
������ ���������: 
$GPZDA,172809,12,07,1996,00,00*45 
#endif