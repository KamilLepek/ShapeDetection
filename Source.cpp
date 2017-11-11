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
#define FRAME_NUMBER 12

//After this amount of frames of not being detected character dissapears
//(implemented in order to avoid character dissapearing or loosing animation if 1 frame out of nowhere doesnt recognize edges)
#define IDLE_FRAMES_TO_DISAPPEAR 20

//Constant which we multiple size of image to draw it inside curve
#define IMAGE_SIZE_IN_CURVE 0.66

//Determines how many frames both characters have to appear in order to begin fighting
#define FRAMES_TO_BEGIN_FIGHT 50

//Determines how many frames should we present the same image
#define FRAMES_PER_IMAGE 10

enum character { Vegeta, Goku};

//Handles capturing frames from video camera
VideoCapture cap;

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

//Calculates size of hero image bigger dimension that is required in order to resize original image
int calculateBiggerDimensionSize(vector<Point> curve)
{
	int dist = INT_MAX;
	for (int i = 0; i < curve.size(); i++)
	{
		for (int j = 0; j < curve.size(); j++)
		{
			if (i != j)
			{
				if (norm(curve[i] - curve[j]) < dist)
					dist = norm(curve[i] - curve[j]);
			}
		}
	}
	//%2 to ensure its dividable by 2
	dist = dist * IMAGE_SIZE_IN_CURVE;
	return dist - dist % 2;
}

//Calculates size of hero image smaller dimension that is required in order to resize original image
int calculateSmallerDimensionSize(int biggerDimSize, character hero)
{
	int dist;
	if (hero == Vegeta)
		dist = biggerDimSize * vegeta[vegiFrameIndex].cols / vegeta[vegiFrameIndex].rows;
	if (hero == Goku)
		dist = biggerDimSize * goku[gokuFrameIndex].cols / goku[gokuFrameIndex].rows;
	return dist - dist % 2; //to ensure its dividable by 2
}

void setHeroProperties(Point center, character hero, Size size)
{
	if (hero == Vegeta)
	{
		vegetaFlag = true;
		vegetaPoint = center;
		vegetaSize = size;
	}
	else if (hero == Goku)
	{
		gokuFlag = true;
		gokuPoint = center;
		gokuSize = size;
	}
}

//Resizes hero to desired size property
Mat resizeHero(character hero)
{
	Mat im;
	if (hero == Vegeta)
		resize(vegeta[vegiFrameIndex], im, vegetaSize);
	else if (hero == Goku)
		resize(goku[gokuFrameIndex], im, gokuSize);
	return im;
}

//Masks the background of character image so that we only see the character itself
Mat mask(Mat characterFrame, Mat partOfWholeImage)
{
	Mat outputImage = Mat(characterFrame.size(), characterFrame.type());
	for (int i = 0; i < outputImage.rows; i++)
	{
		for (int j = 0; j < outputImage.cols; j++)
		{
			Vec3b pixelOrig = partOfWholeImage.at<Vec3b>(i, j);
			Vec3b pixelChar = characterFrame.at<Vec3b>(i, j);
			if (characterFrame.at<Vec3b>(i, j)[1] > 200 && //G
				characterFrame.at<Vec3b>(i, j)[0] < 150 && //B
				(characterFrame.at<Vec3b>(i, j)[2] < 200)) //R
				outputImage.at<Vec3b>(i, j) = pixelOrig;
			else
				outputImage.at<Vec3b>(i, j) = pixelChar;
		}
	}
	return outputImage;
}

Mat flipHeroIfNecessary(Mat image)
{
	Mat result;
	if (vegetaPoint.x < gokuPoint.x)
	{
		flip(image, result, 1);
		return result;
	}
	return image;
}

//Draws character inside curve
void drawCharacter(Mat result, vector<Point> curve, character hero)
{
	Point center = massCenterOfCurve(curve);
	int Y = calculateBiggerDimensionSize(curve);
	int X = calculateSmallerDimensionSize(Y, hero);
	Size size = Size(X, Y);
	setHeroProperties(center, hero, size);
	Mat im = resizeHero(hero);
	Mat partOfResult(result, Range(center.y - im.rows / 2, center.y + im.rows / 2), Range(center.x - im.cols / 2, center.x + im.cols / 2));

	Mat flipped = flipHeroIfNecessary(im);

	Mat out = mask(flipped, partOfResult);
	
	out.copyTo(result.rowRange(center.y - out.rows / 2, center.y + out.rows / 2).
		colRange(center.x - out.cols / 2, center.x + out.cols / 2));
}

//If we do not recognize our shape once a while we still print the character
//in order to make it continous
void drawMissingCharacter(Mat result, character hero)
{
	Mat im;
	if (hero == Vegeta && vegetaFlag == true)
	{
		resize(vegeta[vegiFrameIndex], im, vegetaSize);
		Mat partOfResult(result, Range(vegetaPoint.y - im.rows / 2, vegetaPoint.y + im.rows / 2), Range(vegetaPoint.x - im.cols / 2, vegetaPoint.x + im.cols / 2));

		Mat flipped = flipHeroIfNecessary(im);

		Mat out = mask(flipped, partOfResult);

		out.copyTo(result.rowRange(vegetaPoint.y - out.rows / 2, vegetaPoint.y + out.rows / 2).
			colRange(vegetaPoint.x - out.cols / 2, vegetaPoint.x + out.cols / 2));

	}
	else if (hero == Goku && gokuFlag == true)
	{
		resize(goku[gokuFrameIndex], im, gokuSize);
		Mat partOfResult(result, Range(gokuPoint.y - im.rows / 2, gokuPoint.y + im.rows / 2), Range(gokuPoint.x - im.cols / 2, gokuPoint.x + im.cols / 2));

		Mat flipped = flipHeroIfNecessary(im);

		Mat out = mask(flipped, partOfResult);

		out.copyTo(result.rowRange(gokuPoint.y - out.rows / 2, gokuPoint.y + out.rows / 2).
			colRange(gokuPoint.x - out.cols / 2, gokuPoint.x + out.cols / 2));
	}
}

//To loop "charging animations"
void incrementFrameIndex(bool fightingFlag)
{
	if (!fightingFlag)
	{
		vegiFrameIndex++;
		gokuFrameIndex++;
		if (vegiFrameIndex < 1 || vegiFrameIndex > 3)
			vegiFrameIndex = 1;
		if (gokuFrameIndex < 1 || gokuFrameIndex > 3)
			gokuFrameIndex = 1;
	}
	else
	{
		vegiFrameIndex++;
		gokuFrameIndex++;
		if (vegiFrameIndex < 4 || vegiFrameIndex > 11)
			vegiFrameIndex = 4;
		if (gokuFrameIndex < 4 || gokuFrameIndex > 11)
			gokuFrameIndex = 4;
	}
}

//Draw characters that have been on the map for a while even if on this frame particular shapes were not detected
//It creates illusion of continous appearance
void drawMissingCharactersIfNeeded(int *vegetaIterator, int *gokuIterator, bool vegetaHasBeenPrinted, bool gokuHasBeenPrinted, Mat result)
{
	if (vegetaHasBeenPrinted == false)
	{
		drawMissingCharacter(result, Vegeta);
		(*vegetaIterator)++;
		if (*vegetaIterator > IDLE_FRAMES_TO_DISAPPEAR)
		{
			*vegetaIterator = 0;
			vegetaFlag = false;
			vegiFrameIndex = 0;
		}
	}
	if (gokuHasBeenPrinted == false)
	{
		drawMissingCharacter(result, Goku);
		(*gokuIterator)++;
		if (*gokuIterator > IDLE_FRAMES_TO_DISAPPEAR)
		{
			*gokuIterator = 0;
			gokuFlag = false;
			gokuFrameIndex = 0;
		}
	}
}


//Returns true if the main loop(detection) has ended and were about to print ending animation, returns false otherwise
//Handles flag responsible for fighting animation
bool handleFightCounterAndIncrementFrameIndex(int *fightCounter, bool *fightingFlag, int *frameIterator)
{
	if (vegetaFlag && gokuFlag)
		(*fightCounter)++;
	else
		*fightCounter = 0;
	if (*fightCounter > FRAMES_TO_BEGIN_FIGHT)
	{
		*fightingFlag = true;
		if (gokuFrameIndex == 11 && vegiFrameIndex == 11)
			return true;
	}
	else
		*fightingFlag = false;
	if (*frameIterator % FRAMES_PER_IMAGE == 0)
		incrementFrameIndex(*fightingFlag);
	(*frameIterator)++;
	return false;
}

//Find shapes in a given set of curves and generate changed image
bool findShapesAndDrawCharacters(const Mat sourceHSV, Mat result, vector<vector<Point>> curves)
{
	cout << "Curves found: " << curves.size() << endl;
	vector<Point> approx_curve;
	bool vegetaHasBeenPrinted = false, gokuHasBeenPrinted = false;
	bool finishFlag = false, fightingFlag = false;
	static int vegetaIterator = 0, gokuIterator = 0, fightCounter = 0, frameIterator = 0; //frames in which vegeta wasnt printed by our alghoritm
	for (int i = 0; i < curves.size(); i++)
	{
		approxPolyDP(Mat(curves[i]), approx_curve, arcLength(Mat(curves[i]), true)*0.02, true);
		if (fabs(contourArea(curves[i])) < CURVE_SIZE_IGNORING_MARGIN ||
			!isContourConvex(approx_curve)) // ignore if curve is not convex or its relatively small
			continue;
		if (approx_curve.size() == 3 && isTriangleRed(sourceHSV, approx_curve))
		{
			gokuHasBeenPrinted = true;
			drawCharacter(result, approx_curve, Goku);
			gokuIterator = 0;
		}
		else if (approx_curve.size() == 4 && isSquareBlue(sourceHSV, approx_curve))
		{
			vegetaHasBeenPrinted = true;
			drawCharacter(result, approx_curve, Vegeta);
			vegetaIterator = 0;
		}
	}	
	drawMissingCharactersIfNeeded(&vegetaIterator, &gokuIterator, vegetaHasBeenPrinted, gokuHasBeenPrinted, result);
	return handleFightCounterAndIncrementFrameIndex(&fightCounter, &fightingFlag, &frameIterator);
}

//Handle ending animation
void endingAnimation(Mat image)
{
	for (;;);
}

bool handleFrame(Mat frame)
{
	Mat sourceHSV;
	cvtColor(frame, sourceHSV, CV_BGR2HSV);
	Mat resultImage = frame.clone();
	if (handleEmptyImage(frame))
		return true;
	
	Mat edgesImageBGR = cannyImage(frame, 0);
	vector<vector<Point>> curves;
	findContours(edgesImageBGR, curves, RETR_TREE, CHAIN_APPROX_SIMPLE);
	bool finishFlag = findShapesAndDrawCharacters(sourceHSV, resultImage, curves);

	showImages(edgesImageBGR, resultImage);
	waitKey(1);
	if (finishFlag)
		endingAnimation(resultImage);
	return finishFlag;
}

//Main loop of the program
void handleCameraProcessing()
{
	Mat frame;
	if (!cap.open(0))
		return;
	for (;;)
	{
		cap >> frame;
		if (handleFrame(frame))
			return;
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
	cout << "Application finished processing" << endl;
	waitKey(10000);
	return 0;
}
