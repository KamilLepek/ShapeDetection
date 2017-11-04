#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <cmath>
#include <iostream>
#include <windows.h>

using namespace cv;
using namespace std;

#define THRESH_LOW 5
#define THRESH_HIGH 50
#define APERTURE 3 // 3 or 5
#define KSIZE_X 5
#define KSIZE_Y 5
#define GAUSSIAN_X 0
#define CURVE_SIZE_IGNORING_MARGIN 2000

//Canny R, G, B or whole image
Mat cannyImage(const Mat source, const int BGR)
{
	Mat planes[3];
	split(source, planes);

	Mat result;
	if (BGR == 1)//B
	{
		Mat blurredGray;
		GaussianBlur(planes[0], blurredGray, Size(KSIZE_X, KSIZE_Y), GAUSSIAN_X);

		Canny(blurredGray, result, THRESH_LOW, THRESH_HIGH, APERTURE);
	}
	else if (BGR == 2)//G
	{
		Mat blurredGray;
		GaussianBlur(planes[1], blurredGray, Size(KSIZE_X, KSIZE_Y), GAUSSIAN_X);

		Canny(blurredGray, result, THRESH_LOW, THRESH_HIGH, APERTURE);
	}
	else if (BGR == 3)//R
	{
		Mat blurredGray;
		GaussianBlur(planes[2], blurredGray, Size(KSIZE_X, KSIZE_Y), GAUSSIAN_X);

		Canny(blurredGray, result, THRESH_LOW, THRESH_HIGH, APERTURE);
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

//Show first image, image with edges detected and changed image
void showImages(const Mat source, const Mat edges, const Mat result)
{
	//imshow("Source", source);
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

	return 2 * colour / amountOfPixels;
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
	radius /= 16;
	int colour = approximateColourInSquare(sourceHSV, massCenter, radius);
	if (colour < 30 || colour > 315)
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

//Find shapes in a given set of curves and generate changed image
void findShapes(const Mat sourceHSV, Mat result, vector<vector<Point>> curves)
{
	cout << "Curves found: " << curves.size() << endl;
	vector<Point> approx_curve;
	for (int i = 0; i < curves.size(); i++)
	{
		approxPolyDP(Mat(curves[i]), approx_curve, arcLength(Mat(curves[i]), true)*0.02, true);
		if (fabs(contourArea(curves[i])) < CURVE_SIZE_IGNORING_MARGIN ||
			!isContourConvex(approx_curve)) // ignore if curve is not convex or its relatively small
			continue;
		if (approx_curve.size() == 3)
		{
			if (isTriangleRed(sourceHSV, approx_curve))
			{
				setLabel(result, "Trojkat", curves[i]);
			}
		}
		else if (approx_curve.size() == 4)
		{
			if (isSquareBlue(sourceHSV, approx_curve))
			{
				setLabel(result, "Kwadrat", curves[i]);
			}
		}
	}	
}

void handleFrame(Mat frame, int wait = 1)
{
	Mat sourceHSV;
	cvtColor(frame, sourceHSV, CV_BGR2HSV);
	Mat resultImage = frame.clone();
	if (handleEmptyImage(frame))
		return;

	Mat edgesImageBGR = cannyImage(frame, 0);
	Mat edgesImageB = cannyImage(frame, 1);
	Mat edgesImageG = cannyImage(frame, 2);
	Mat edgesImageR = cannyImage(frame, 3);
	vector<vector<Point>> curves, curves1, curves2, curves3;

	findContours(edgesImageBGR, curves, RETR_TREE, CHAIN_APPROX_SIMPLE);
	findShapes(sourceHSV, resultImage, curves);
	
	findContours(edgesImageB, curves1, RETR_TREE, CHAIN_APPROX_SIMPLE);
	findShapes(sourceHSV, resultImage, curves1);

	findContours(edgesImageG, curves2, RETR_TREE, CHAIN_APPROX_SIMPLE);
	findShapes(sourceHSV, resultImage, curves2);

	findContours(edgesImageR, curves3, RETR_TREE, CHAIN_APPROX_SIMPLE);
	findShapes(sourceHSV, resultImage, curves3);
	
	showImages(frame, edgesImageBGR, resultImage);
	waitKey(wait);
}

void handleFileProcessing()
{
	string path = "G:/image.jpg";
	Mat frame = imread(path);
	handleFrame(frame, 0);
}

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

int main()
{
	int CAMERA_OR_FILE = 1;//1 for input from camera, 0 for input from file

	if (CAMERA_OR_FILE == 1)
		handleCameraProcessing();
	else if (CAMERA_OR_FILE == 0)
		handleFileProcessing();
	else
		cout << "Invalid source" << endl;
	return 0;
}
