#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <cmath>
#include <iostream>
#include <windows.h>

using namespace cv;
using namespace std;

//Canny params
#define THRESH_LOW 5 // low threshold for canny process
#define THRESH_HIGH 50 // high treshold for canny process
#define APERTURE 3 // 3 or 5 - param for canny process

//Gaussian params
#define KSIZE_X 5
#define KSIZE_Y 5
#define GAUSSIAN_X 0

//Curves smaller than this will be ignored in computing
#define CURVE_SIZE_IGNORING_MARGIN 2000

//Number of frames that are being read from files for each character
#define FRAME_NUMBER 11

//After this amount of frames of not being detected character dissapears
//(implemented in order to avoid character dissapearing or loosing animation if 1 frame out of nowhere doesnt recognize edges)
#define IDLE_FRAMES_TO_DISAPPEAR 20

//if defined then we get better edge detection but we loose performance
#define TEST_MULTIPLE_TIMES

//Vegeta and goku animations
Mat vegeta[FRAME_NUMBER];
Mat goku[FRAME_NUMBER];

//Characters coordinates on map
Point vegetaPoint;
Point gokuPoint;

//These determine how much we need to resize the original image in order to "fit"
Size vegetaSize;
Size gokuSize;

//Determines whether character should be on map atm
bool vegetaFlag = false;
bool gokuFlag = false;

//Indexes of frame a the moment
int vegiFrameIndex = 0;
int gokuFrameIndex = 0;

//Canny 2-d matrix (R G or B plane)
Mat cannyPlane(Mat plane)
{
	Mat result;
	Mat blurredGray;
	GaussianBlur(plane, blurredGray, Size(KSIZE_X, KSIZE_Y), GAUSSIAN_X);

	Canny(blurredGray, result, THRESH_LOW, THRESH_HIGH, APERTURE);
	return result;
}

//Canny R, G, B or whole image
//BGR: 1-B, 2-G, 3-R, else - grayscale whole
Mat cannyImage(const Mat source, const int BGR)
{
	Mat planes[3];
	split(source, planes);

	Mat result;
	if (BGR != 0)
	{
		switch (BGR)
		{
			case 1:
				result = cannyPlane(planes[0]);
				break;
			case 2:
				result = cannyPlane(planes[1]);
				break;
			case 3:
				result = cannyPlane(planes[2]);
				break;
			default:
				break;
		}
	}
	else
	{
		Mat gray;
		cvtColor(source, gray, CV_BGR2GRAY);

		Mat blurredGray;
		GaussianBlur(gray, blurredGray, Size(KSIZE_X, KSIZE_Y), GAUSSIAN_X);

		Canny(blurredGray, result, THRESH_LOW, THRESH_HIGH, APERTURE);
	}
	return result;
}

//Handle situation when the image didn't load properly
bool handleEmptyImage(const Mat source)
{
	if (source.empty())
	{
		cout << "Image didn't load properly" << endl;
		Sleep(2000);
		return true;
	}
	return false;
}

//Show image with edges detected and changed image(result)
void showImages(const Mat edges, const Mat result)
{
	imshow("Edges", edges);
	imshow("Result", result);
}

//Returns mass center of closed curve
Point massCenterOfCurve(vector<Point> curve)
{
	Moments moment = moments(curve, false);
	return Point(moment.m10 / moment.m00, moment.m01 / moment.m00);
}

//Sets given label in the image on specified curve center of mass
void setLabel(const Mat image, const string label, vector<Point> curve)
{
	putText(image, label, massCenterOfCurve(curve), 16, 0.5, CV_RGB(0, 0, 0), 1, 8);
}

//Returns average hue of pixels specified in a square 2*radius x 2*radius with specified center (all in HSV)
int approximateColourInSquare(const Mat sourceHSV, const Point center, int radius)
{	
	int colour = sourceHSV.at<Vec3b>(center.y, center.x)[0];

	int amountOfPixels = 1;

	for (int i = -radius; i <= radius; i++)
	{
		for (int j = -radius; j <= radius; j++)
		{
			colour += sourceHSV.at<Vec3b>(center.y + j, center.x + i)[0];
			amountOfPixels++;
		}
	}

	return 2 * colour / amountOfPixels; // 2 * cause openCV holds hue in [0,180] while the angle might change up to 360
}

bool isTriangleRed(const Mat sourceHSV, vector<Point> triangle)
{
	Point massCenter = massCenterOfCurve(triangle);
	int radius = norm(triangle[0] - triangle[1]) * 2;
	for (int i = 0; i < triangle.size(); i++)
	{
		int candidate = norm(triangle[i] - massCenter);
		radius = candidate < radius ? candidate : radius;
	}
	radius /= 4;
	int colour = approximateColourInSquare(sourceHSV, massCenter, radius);
	if (colour < 40 || colour > 310)
		return true;
	return false;
}

bool isSquareBlue(const Mat sourceHSV, vector<Point> square)
{
	Point massCenter = massCenterOfCurve(square);
	int radius = norm(square[0] - massCenter) / 4; //size of the rectangle in which we will check the colour 
	int colour = approximateColourInSquare(sourceHSV, massCenter, radius);
	if (colour > 170 && colour < 280)
		return true;
	return false;
}


void drawVegetaCharging(Mat result, vector<Point> curve)
{
	Point center = massCenterOfCurve(curve);
	vegetaFlag = true;
	vegetaPoint = center;
	Mat veg;
	int dist = norm(curve[0] - curve[1]);
	dist = dist % 2 == 1 ? dist - 5 : dist - 4;
	int xDist = dist * vegeta[vegiFrameIndex].cols / vegeta[vegiFrameIndex].rows;
	Size size(xDist - xDist % 2, dist);
	vegetaSize = size;
	resize(vegeta[vegiFrameIndex], veg, size);

	veg.copyTo(result.rowRange(center.y - veg.rows / 2, center.y + veg.rows / 2).
		colRange(center.x - veg.cols / 2, center.x + veg.cols / 2));
}

void drawGokuCharging(Mat result, vector<Point> curve)
{
	
}

void drawMissingGoku(Mat result)
{
	
}

void drawMissingVegeta(Mat result)
{
	if (vegetaFlag == true)
	{
		Mat veg;
		resize(vegeta[vegiFrameIndex], veg, vegetaSize);
		veg.copyTo(result.rowRange(vegetaPoint.y - veg.rows / 2, vegetaPoint.y + veg.rows / 2).
			colRange(vegetaPoint.x - veg.cols / 2, vegetaPoint.x + veg.cols / 2));
	}
}

//To loop "charging animations"
void incrementFrameIndex()
{
	vegiFrameIndex++;
	gokuFrameIndex++;
	if (vegiFrameIndex == 4)
		vegiFrameIndex = 1;
	if (gokuFrameIndex == 4)
		gokuFrameIndex = 1;
}

//Find shapes in a given set of curves and generate changed image
void findShapesAndDrawCharacters(const Mat sourceHSV, Mat result, vector<vector<Point>> curves)
{
	cout << "Curves found: " << curves.size() << endl;
	vector<Point> approx_curve;
	bool vegetaHasBeenPrinted = false;
	static int vegetaIterator = 0; //frames in which vegeta wasnt printed by our alghoritm
	for (int i = 0; i < curves.size(); i++)
	{
		approxPolyDP(Mat(curves[i]), approx_curve, arcLength(Mat(curves[i]), true)*0.02, true);
		if (fabs(contourArea(curves[i])) < CURVE_SIZE_IGNORING_MARGIN ||
			!isContourConvex(approx_curve)) // ignore if curve is not convex or its relatively small
			continue;
		if (approx_curve.size() == 3 && isTriangleRed(sourceHSV, approx_curve))
		{
			drawGokuCharging(result, approx_curve);
			setLabel(result, "Trojkat", curves[i]);
		}
		else if (approx_curve.size() == 4 && isSquareBlue(sourceHSV, approx_curve))
		{
			vegetaHasBeenPrinted = true;
			drawVegetaCharging(result, approx_curve);
			vegetaIterator = 0;
		}
	}	
	if (vegetaHasBeenPrinted == false)
	{
		drawMissingVegeta(result);
		vegetaIterator++;
		if (vegetaIterator > IDLE_FRAMES_TO_DISAPPEAR)
		{
			vegetaIterator = 0;
			vegetaFlag = false;
			vegiFrameIndex = 0;
		}
	}
	incrementFrameIndex();
}

void handleFrame(Mat frame, int wait = 1)
{
	Mat sourceHSV;
	cvtColor(frame, sourceHSV, CV_BGR2HSV);
	Mat resultImage = frame.clone();
	if (handleEmptyImage(frame))
		return;
	
	Mat edgesImageBGR = cannyImage(frame, 0);
	vector<vector<Point>> curves;
	findContours(edgesImageBGR, curves, RETR_TREE, CHAIN_APPROX_SIMPLE);
	findShapesAndDrawCharacters(sourceHSV, resultImage, curves);

#ifdef TEST_MULTIPLE_TIMESn
	Mat edgesImageB = cannyImage(frame, 1);
	Mat edgesImageG = cannyImage(frame, 2);
	Mat edgesImageR = cannyImage(frame, 3);
	vector<vector<Point>> curves1, curves2, curves3;
	
	findContours(edgesImageB, curves1, RETR_TREE, CHAIN_APPROX_SIMPLE);
	findShapesAndDrawCharacters(sourceHSV, resultImage, curves1);

	findContours(edgesImageG, curves2, RETR_TREE, CHAIN_APPROX_SIMPLE);
	findShapesAndDrawCharacters(sourceHSV, resultImage, curves2);

	findContours(edgesImageR, curves3, RETR_TREE, CHAIN_APPROX_SIMPLE);
	findShapesAndDrawCharacters(sourceHSV, resultImage, curves3);
#endif
	showImages(edgesImageBGR, resultImage);
	waitKey(wait);
}

//Main loop of the program
void handleCameraProcessing()
{
	Mat frame;
	VideoCapture cap;
	if (!cap.open(0))
		return;
	for (;;)
	{
		cap >> frame;
		handleFrame(frame);
	}
}

// Method to initialize frames of goku and vegeta from files
void initializeGokuAndVegetaImages(Mat *vegeta, Mat *goku)
{
	for (int i = 0; i < FRAME_NUMBER; i++)
	{
		stringstream veg, gok;
		veg << "ANIMATIONS/vegeta/" << i << ".jpg";
		gok << "ANIMATIONS/goku/" << i << ".jpg";
		vegeta[i] = imread(veg.str());
		goku[i] = imread(gok.str());
	}
}

int main()
{
	initializeGokuAndVegetaImages(vegeta, goku);
	handleCameraProcessing();
	return 0;
}
