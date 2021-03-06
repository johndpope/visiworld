#ifndef __PAGEDATA_H
#define __PAGEDATA_H

// pagedata.h
#include <string>
#include <vector>
#include <opencv2/core/core.hpp>

#include "ccdata.h"

struct CCBaseMy :
  public CCBase2
{
  int isphere; // ������ ����� � ������, � ������� ���� ��������� ����������, -1 -- �� ���������
  CCBaseMy():isphere(-1){}
};

typedef CCData_<CCBaseMy> CCData;




struct PageData
{
  // input
  std::string source_filename;
  int source_frame_index;

  // ������� �������
  cv::Mat1b src; // ������� �����������, �-�, 0-������ (������), 255-����� (���)
  cv::Mat1b src_dilated; // ����������� �������
  cv::Mat1b src_binarized; // 0-background, 255-foreground

  // ���������� ���������
  cv::Mat1i labels; // ����� ��������� ���������, [2...labels.size()-1] ==> cc[]
  std::vector< CCData > cc; // ���������� � �����������
  // ��������� ��������������
  cv::Rect ROI;

  PageData(){}
  PageData( const char* filename ) {    compute(filename);  }
  bool compute( const char* filename );
};

#endif // __PAGEDATA_H