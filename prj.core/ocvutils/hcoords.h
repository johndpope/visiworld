#ifndef __HCOORDS_H
#define __HCOORDS_H
#pragma once

#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>

inline double length( const cv::Point3d& v ){  return sqrt(v.ddot(v)); }

//template<class T> 
namespace cv {
bool operator <( const cv::Point3d& _Left,  const cv::Point3d& _Right);
}

inline
bool cv::operator <( const cv::Point3d& _Left,  const cv::Point3d& _Right)
	{	
    //return (   _Left.x < _Right.x ||
		  //    !(_Right.x < _Left.x) && _Left.y < _Right.y ) ||
		  //    !(_Right.y < _Left.y) && _Left.z < _Right.z
    //      ;
    return _Left.x < _Right.x;
	}


inline cv::Point3d& normalize( cv::Point3d& v ) // set length == 1
{
  double len = length(v);
  v = (1./len)* v;
  return v;
}

typedef cv::Point3d HPoint3d; // ����� �� ��������� ����� (as double)
typedef cv::Point3d HLine3d; // �����, ��� �� ������ ������� �� ��������� ����� (as double)

inline 
cv::Point3d force_positive_direction( cv::Point3d& p )
{
  if (p.z < 0)
    p *= -1.;
  // ?????????????????????????????????
  return p;
}


struct HCoords
{
  int width, height, depth; // image bitmap size and camera angle
  HCoords(
  int width, // tmp
  int height, // tmp
  int depth = 0 // tmp
  ): width(width), height(height), depth(depth)
  {
    if (this->depth == 0) //!!!!did not update!!!!
      this->depth = width/2; // �����������, ��� 90 �������� �� �����������
  }

  cv::Point3d convert( // << ��������� = ��������������� ������
  const cv::Point& p // �� ����� ����� � �������� ����������� �����������
                )
  {
    if (depth == 0)
      depth = width/2; // 90 �������� �� ���������
    cv::Point3d res = cv::Point3d( p.x - width/2, p.y - height/2, depth );
    return normalize( res );
  }

  void hline2points( 
    const cv::Point3d& hline, // ����� � ���������� �����������
    cv::Point& pt1, cv::Point& pt2 // ����� ��� ��� ����� �� ����� �������
    )
  {
    // hline == A, B, C,  Ax+By+Cz = 0; z = depth;
    double A = hline.x;
    double B = hline.y;
    double CC = -hline.z*depth;
    cv::Point2d h1, h2;
    if (abs(A) > abs(B))
    {
      h1.y = -height/2.; h1.x = (CC - B*h1.y) / A;
      h2.y =  height/2.; h2.x = (CC - B*h2.y) / A;
    }
    else
    {
      h1.x = -width/2.; h1.y = (CC - A*h1.x) / B;
      h2.x =  width/2.; h2.y = (CC - A*h2.x) / B;
    }
    pt1.x = (int)(h1.x + width/2); 
    pt2.x = (int)(h2.x + width/2); 
    pt1.y = (int)(h1.y + height/2); 
    pt2.y = (int)(h2.y + height/2); 
  }

  HPoint3d point2hpoint( cv::Point& pt ) // �������� ����� �� ����� ������� 1. //// <radius>
  { // <x,y,depth> --> sphere
    pt.x -= width/2;
    pt.y -= height/2;
    double denom = sqrt( double( pt.x*pt.x + pt.y*pt.y + depth*depth ) ); //// / radius == 1.;
    return cv::Point3d( pt.x / denom, pt.y / denom, depth / denom );
  }

  bool segment2hline( cv::Point& pt1, cv::Point& pt2, HLine3d& res_hline, double delta = 0.000001 ) // ������� �� �������� --> ���������� �����
  {
    HPoint3d hp1 = point2hpoint( pt1 );
    HPoint3d hp2 = point2hpoint( pt2 );
    res_hline = hp1.cross( hp2 );
    double len = length(res_hline);
    if ( len < delta )
      return false; // ����� -- ������, ������� ������� �����
    res_hline = (1./len) * res_hline;
    return true;
  }
};


#endif //__HCOORDS_H