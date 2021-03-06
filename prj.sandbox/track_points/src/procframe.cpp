#include <opencv/cv.h>
#include <opencv/highgui.h>

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp> 
#if CV_MAJOR_VERSION > 2
#include <opencv2/xfeatures2d/nonfree.hpp>
#else
#include <opencv2/nonfree/nonfree.hpp>
#endif

using namespace std;
using namespace cv;



int // 0-continue, <0 failed or user breaks
preprocess_frame( Mat& img )
{
  const double TRUNCATE_TOP = 0.22;
  const double TRUNCATE_HEIGHT = 1 - TRUNCATE_TOP - 0.15;

  int yy = (int)(img.rows * TRUNCATE_TOP); // �������� ����� ������
  int hh = (int)(img.rows * TRUNCATE_HEIGHT); // �������� ������

  const double TRUNCATE_LEFT = 0.2;
  const double TRUNCATE_WIDTH = 1 - 2*TRUNCATE_LEFT;

  int xx = (int)(img.cols * TRUNCATE_LEFT); // �������� ����� �����
  int ww = (int)(img.cols * TRUNCATE_WIDTH); // �������� ������

  //Rect roi( 0, yy, img.cols, min( hh, img.rows-yy ) ); ////// <<< danger
  Rect roi( xx, yy, min( ww, img.cols-xx ), min( hh, img.rows-yy ) ); ////// <<< danger
  img = img( roi );

  resize( img, img, Size(0,0), 0.5, 0.5 );

  //static Mat prev_img;
  //if (prev_img.empty())
  //  prev_img = img;
  //Mat diff_img = img - prev_img;
  //prev_img = img.clone();
  //img = diff_img;



  return 0;
}

class FramePoint : public cv::KeyPoint // ������ ����� �� �����������
{

};

void detect_points( Mat& img, vector< KeyPoint >& pts )
{

#if 0
  cv::SURF detector( 4000, 4, 2, 
    true, 
    true  // no orientation
    );
  detector.detect( img, pts );
#endif

#if 0
  SIFT detector( 600 );
  detector.detect( img, pts );
#endif

#if 0
  ORB detector( 100 ); // nfeatures
  detector.detect( img, pts );
#endif

#if 1
  BRISK detector; // slow...
  detector.detect( img, pts );
#endif

#if 0
  MSER detector; // slow
  detector.detect( img, pts );
#endif




  //int minHessian = 400;
  //SurfFeatureDetector detector( minHessian );
  //std::vector< KeyPoint > keypoints_object, keypoints_scene;
  //detector.detect( img_object, keypoints_object );

}

void draw_points( Mat& img, vector< KeyPoint >& pts, Mat& out )
{
  drawKeypoints(img, pts,out, Scalar::all(-1), DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
}

int // 0-continue, <0 failed or user breaks
procframe( Mat& img, int iframe, double fps )
{

  if ( 0 > preprocess_frame(img) )
    return -2;

  vector< KeyPoint > pts;
  detect_points( img, pts );

  Mat out;
  draw_points( img, pts, out );
  imshow("..", out);
  if (27==waitKey(1))
    return -1;

  return 0;
}
