#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <cmath>
#include <iostream>
#include <windows.h>

using namespace cv;
using namespace std;

//Convert to grayscale and find edges using canny
Mat cannyImage(const Mat source)
{
	Mat gray;
	cvtColor(source, gray, CV_BGR2GRAY);

	Mat result;
	Canny(gray, result, 100, 200, 5);

	return result;
}

//Handle situation when the image didn't load properly
bool handleEmptyImage(const Mat source)
{
	if (source.empty())
	{
		cout << "Image didn't load properly" << endl;
		cout << "Shutting down application" << endl;
		Sleep(2000);
		return true;
	}
	return false;
}

//Show first image, image with edges detected and changed image
void showImages(const Mat source, const Mat edges, const Mat result)
{
	imshow("Source", source);
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

bool isTriangleYellow(const Mat sourceHSV, vector<Point> triangle)
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
	if (colour > 30 && colour < 75)
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
		if (fabs(contourArea(curves[i])) < 100 || !isContourConvex(approx_curve)) // ignore if curve is not convex or its relatively small
			continue;
		if (approx_curve.size() == 3)
		{
			if (isTriangleYellow(sourceHSV, approx_curve))
			{
				setLabel(result, "T", curves[i]);
			}
		}
		else if (approx_curve.size() == 4)
		{
			if (isSquareBlue(sourceHSV, approx_curve))
			{
				setLabel(result, "S", curves[i]);
			}
		}
	}	
}

int main()
{
	string path = "C:/input/h.png";
	Mat sourceImage = imread(path);
	Mat sourceHSV;
	cvtColor(sourceImage, sourceHSV, CV_BGR2HSV);
	Mat resultImage = sourceImage.clone();
	if (handleEmptyImage(sourceImage))
		return -1;
	Mat edgesImage = cannyImage(sourceImage);
	
	vector<vector<Point>> curves;
	findContours(edgesImage, curves, RETR_TREE, CHAIN_APPROX_SIMPLE);

	findShapes(sourceHSV, resultImage, curves);

	showImages(sourceImage, edgesImage, resultImage);
	waitKey(0);
	return 0;
}
