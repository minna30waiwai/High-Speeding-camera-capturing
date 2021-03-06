/*
This example describes the FIRST STEPS of handling Baumer-GAPI SDK.
The given source code applies to handling one system, one camera and eight images.
Please see "Baumer-GAPI SDK Programmer's Guide" chapter 5.5
*/

#include "stdafx.h"
#include <stdio.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <string>
#include "bgapi2_genicam.hpp"
#include <time.h>
#include <cmath>
#include <future>
#include <stdlib.h>
#include <mutex>
//#include <boost/lockfree/queue.hpp>
#include <windows.h>
#include <math.h>
#include <conio.h>
#include <fstream>
#include <process.h>
#include <string>
#include <vector>
#include <thread>

//以下，opencvで使うもの
#include <opencv2/opencv.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/core.hpp>
#include <opencv2/opencv_modules.hpp>
//以上，opencvで使うもの

using namespace std;
using namespace BGAPI2;
using namespace cv;

template<class T> void operator >> (const std::string& src, T& dst) {
	std::istringstream iss(src);
	iss >> dst;
}

//BGAPI2特有のカメラ使用時の関数
int start_camera(/*BGAPI2::ImageProcessor * imgProcessor, BGAPI2::SystemList *systemList, BGAPI2::System * pSystem, BGAPI2::String sSystemID, BGAPI2::InterfaceList *interfaceList, BGAPI2::Interface * pInterface, BGAPI2::String sInterfaceID, BGAPI2::DeviceList *deviceList, BGAPI2::Device * pDevice, BGAPI2::String sDeviceID, BGAPI2::DataStreamList *datastreamList, BGAPI2::DataStream * pDataStream, BGAPI2::String sDataStreamID, BGAPI2::BufferList *bufferList, BGAPI2::Buffer * pBuffer, BGAPI2::String sBufferID,*/ int returncode);//カメラ開始動作
int end_camera(BGAPI2::System * pSystem, BGAPI2::Interface * pInterface, BGAPI2::Device * pDevice, BGAPI2::DataStream * pDataStream, BGAPI2::DataStreamList *datastreamList, int returncode, BGAPI2::Buffer * pBuffer, BGAPI2::BufferList *bufferList); //カメラ終了動作

																																																														//BGAPI2関係変数宣言
BGAPI2::ImageProcessor * imgProcessor = NULL;

BGAPI2::SystemList *systemList = NULL;
BGAPI2::System * pSystem = NULL;
BGAPI2::String sSystemID;

BGAPI2::InterfaceList *interfaceList = NULL;
BGAPI2::Interface * pInterface = NULL;
BGAPI2::String sInterfaceID;

BGAPI2::DeviceList *deviceList = NULL;
BGAPI2::Device * pDevice = NULL;
BGAPI2::String sDeviceID;

BGAPI2::DataStreamList *datastreamList = NULL;
BGAPI2::DataStream * pDataStream = NULL;
BGAPI2::String sDataStreamID;

BGAPI2::BufferList *bufferList = NULL;
BGAPI2::Buffer * pBuffer = NULL;
BGAPI2::String sBufferID;

//multi thread function
void camera_capture(double frame_count);
void transform_image(void);
void video_write(BGAPI2::Image * pTransformImage);

//画像取得・処理用変数
BGAPI2::Buffer * pBufferFilled = NULL;
BGAPI2::BufferList *BufferListFilled = NULL;
BGAPI2::Image * pTransformImage = NULL;
BGAPI2::Image * pImage = NULL;
//int *count;
char *img = nullptr;

//opencv関係変数
cv::Mat openCvImage;

//fps指定
#define fps 500

//フラグ変数
/*
-1…capture開始前
0…capture中
1…capture終了，Buffer→TransImage中
2…Buffer→TransImage終了，video_write実施中
3…video_write終了
*/
int start_flag = -1;
int finish_flag = -1;
int image_flag = -1;//0ならキャプチャなし,1ならキャプチャあり

					//標準出力ロック用
std::mutex mtx;

//video_check関数
void video_check(void);

class myvactor {
	std::mutex mtx;
	std::vector<cv::Mat> src;
public:
	void push_back(cv::Mat x) {
			std::lock_guard<std::mutex> lock(mtx);
			src.push_back(x);
			std::lock_guard<std::mutex> unlock(mtx);
	}
};
std::vector<cv::Mat> src;


void make_file(void);


int main(int argc, char *argv[])
{

	make_file();

	int returncode = 0;
	//int *count=0;


	returncode = start_camera(/*imgProcessor,systemList,pSystem,sSystemID,interfaceList,pInterface,sInterfaceID,deviceList, pDevice, sDeviceID,datastreamList,pDataStream,sDataStreamID,bufferList,pBuffer,sBufferID,*/returncode);
	//start_vibration();

	try
	{
		double frame_count = 4.0 / (pDevice->GetRemoteNode("ExposureTime")->GetDouble()*std::pow(10, -6.0));
		//clock_t start = clock();
		start_flag = 0;
		std::thread t1(camera_capture, frame_count);

		while (start_flag <1) {}

		t1.join();
		finish_flag = 1;

		std::thread t3(video_write, pTransformImage);
		t3.join();

		video_check();
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}

	returncode = end_camera(pSystem, pInterface, pDevice, pDataStream, datastreamList, returncode, pBuffer, bufferList);
	//　end()に置き換え

	std::cout << std::endl << std::endl;
	std::cout << "Input any number to close the program:";
	int endKey = 0;
	std::cin >> endKey;
	return returncode;

}
cv::String filename_video="test_video.avi";

void video_check(void) {
	cv::VideoCapture capture(filename_video);
	//namedWindow("video");
	int count = 0;
	while (1) {
		cv::Mat frame;
		capture >> frame;
		if (frame.empty()) {
			break;
		}
		count++;
		//namedWindow("video");
		//cv::imshow("video",frame);
		//cv::waitKey(0);
	}
	std::cout << std::endl << std::endl;
	std::cout << "Saved video's number of frame:" << count << std::endl;
}

void camera_capture(double frame_count) {
	finish_flag = 0;
	std::cout << "camera ok." << std::endl;
	mtx.lock();
	start_flag++;
	std::cout << "camera_capture start!!" << std::endl;
	mtx.unlock();
	while (start_flag != 1) {}
	clock_t start, end;
	start = clock();
	int count_t = 1;
	cv::Mat temp;

	BufferListFilled = pDataStream->GetBufferList();
	//pDataStream->GetNodeList()->GetNode("GoodFrames")->GetInt()<<0;
	//cv::Mat temp;
	while (count_t <= (frame_count + 1)) {//取得フレーム数が目的値以下ならループ
		//std::cout << count_t << std::endl;
		pBufferFilled = pDataStream->GetFilledBuffer(1000); //timeout 1000 msec
		if (pBufferFilled == NULL)
		{
			std::cout << "Error: Buffer Timeout after 1000 msec" << std::endl << std::endl;
		}
		else if (pBufferFilled->GetIsIncomplete() == true)
		{
			std::cout << "Error: Image is incomplete" << std::endl << std::endl;
			// queue buffer again
			pBufferFilled->QueueBuffer();
		}
		//push_back frame image
		src.push_back(cv::Mat(cv::Size(640, 480), CV_8UC1, (char*)pBufferFilled->GetMemPtr()).clone());
		pBufferFilled->QueueBuffer();
		count_t++;
	}
	//while (!que_vw.push(nullptr)) {};
	mtx.lock();
	finish_flag = 1;
	mtx.unlock();
	end = clock();
	mtx.lock();
	std::cout << "number of capture:" << count_t << std::endl;
	std::cout << "capture 処理時間:" << (double)(end - start) / CLOCKS_PER_SEC << "sec." << std::endl;	//処理時間の表示
	mtx.unlock();
}

void video_write(BGAPI2::Image * pTransformImage) {
	cv::VideoWriter writer;
	writer.open(filename_video, VideoWriter::fourcc('X', 'V', 'I', 'D'), double(fps), cv::Size(640, 480), false);
	clock_t start, end;
	start = clock();
	mtx.lock();
	std::cout << "video_writer start!!" << std::endl;
	mtx.unlock();
	if (!writer.isOpened()) {
		std::cout << "open error." << std::endl;
	}
	else {
		std::cout << "open success." << std::endl;
	}
	int counter_vw = 1;
	while (start_flag <1) {}

	for (int i = 0; i < src.size(); i++) {
		writer.write(src[i]);
		counter_vw++;
	}
	src.clear();
	//finish_flag = 3;
	writer.release();
	end = clock();
	mtx.lock();
	std::cout << "number of Video_write:" << counter_vw << std::endl;
	std::cout << "write 処理時間:" << (double)(end - start) / CLOCKS_PER_SEC << "sec." << std::endl;	//処理時間の表示
	mtx.unlock();
}

void make_file(void) {

	printf("filename final check\nvideo filename:%s\n",filename_video);
	printf("ok?\n");
	getchar();
}


int start_camera(/*BGAPI2::ImageProcessor * imgProcessor, BGAPI2::SystemList *systemList, BGAPI2::System * pSystem, BGAPI2::String sSystemID, BGAPI2::InterfaceList *interfaceList, BGAPI2::Interface * pInterface, BGAPI2::String sInterfaceID, BGAPI2::DeviceList *deviceList, BGAPI2::Device * pDevice,BGAPI2::String sDeviceID, BGAPI2::DataStreamList *datastreamList, BGAPI2::DataStream * pDataStream, BGAPI2::String sDataStreamID, BGAPI2::BufferList *bufferList, BGAPI2::Buffer * pBuffer, BGAPI2::String sBufferID,*/int returncode) {
	std::cout << std::endl;
	std::cout << "##########################################################" << std::endl;
	std::cout << "# PROGRAMMER'S GUIDE Example 005_PixelTransformation.cpp #" << std::endl;
	std::cout << "##########################################################" << std::endl;
	std::cout << std::endl << std::endl;


	//Load image processor
	try
	{
		imgProcessor = BGAPI2::ImageProcessor::GetInstance();
		std::cout << "ImageProcessor version:    " << imgProcessor->GetVersion() << std::endl;
		if (imgProcessor->GetNodeList()->GetNodePresent("DemosaicingMethod") == true)
		{
			imgProcessor->GetNodeList()->GetNode("DemosaicingMethod")->SetString("NearestNeighbor"); // NearestNeighbor, Bilinear3x3, Baumer5x5
			std::cout << "    Demosaicing method:    " << imgProcessor->GetNodeList()->GetNode("DemosaicingMethod")->GetString() << std::endl;
		}
		std::cout << std::endl;
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}


	std::cout << "SYSTEM LIST" << std::endl;
	std::cout << "###########" << std::endl << std::endl;

	//COUNTING AVAILABLE SYSTEMS (TL producers) 
	try
	{
		systemList = SystemList::GetInstance();
		systemList->Refresh();
		std::cout << "5.1.2   Detected systems:  " << systemList->size() << std::endl;

		//SYSTEM DEVICE INFORMATION
		for (SystemList::iterator sysIterator = systemList->begin(); sysIterator != systemList->end(); sysIterator++)
		{
			std::cout << "  5.2.1   System Name:     " << sysIterator->second->GetFileName() << std::endl;
			std::cout << "          System Type:     " << sysIterator->second->GetTLType() << std::endl;
			std::cout << "          System Version:  " << sysIterator->second->GetVersion() << std::endl;
			std::cout << "          System PathName: " << sysIterator->second->GetPathName() << std::endl << std::endl;
		}
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}


	//OPEN THE FIRST SYSTEM IN THE LIST WITH A CAMERA CONNECTED
	try
	{
		for (SystemList::iterator sysIterator = systemList->begin(); sysIterator != systemList->end(); sysIterator++)
		{
			std::cout << "SYSTEM" << std::endl;
			std::cout << "######" << std::endl << std::endl;

			try
			{
				sysIterator->second->Open();
				std::cout << "5.1.3   Open next system " << std::endl;
				std::cout << "  5.2.1   System Name:     " << sysIterator->second->GetFileName() << std::endl;
				std::cout << "          System Type:     " << sysIterator->second->GetTLType() << std::endl;
				std::cout << "          System Version:  " << sysIterator->second->GetVersion() << std::endl;
				std::cout << "          System PathName: " << sysIterator->second->GetPathName() << std::endl << std::endl;
				sSystemID = sysIterator->first;
				std::cout << "        Opened system - NodeList Information " << std::endl;
				std::cout << "          GenTL Version:   " << sysIterator->second->GetNode("GenTLVersionMajor")->GetValue() << "." << sysIterator->second->GetNode("GenTLVersionMinor")->GetValue() << std::endl << std::endl;

				std::cout << "INTERFACE LIST" << std::endl;
				std::cout << "##############" << std::endl << std::endl;

				try
				{
					interfaceList = sysIterator->second->GetInterfaces();
					//COUNT AVAILABLE INTERFACES
					interfaceList->Refresh(100); // timeout of 100 msec
					std::cout << "5.1.4   Detected interfaces: " << interfaceList->size() << std::endl;

					//INTERFACE INFORMATION
					for (InterfaceList::iterator ifIterator = interfaceList->begin(); ifIterator != interfaceList->end(); ifIterator++)
					{
						std::cout << "  5.2.2   Interface ID:      " << ifIterator->first << std::endl;
						std::cout << "          Interface Type:    " << ifIterator->second->GetTLType() << std::endl;
						std::cout << "          Interface Name:    " << ifIterator->second->GetDisplayName() << std::endl << std::endl;
					}
				}
				catch (BGAPI2::Exceptions::IException& ex)
				{
					returncode = 0 == returncode ? 1 : returncode;
					std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
					std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
					std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
				}


				std::cout << "INTERFACE" << std::endl;
				std::cout << "#########" << std::endl << std::endl;

				//OPEN THE NEXT INTERFACE IN THE LIST
				try
				{
					for (InterfaceList::iterator ifIterator = interfaceList->begin(); ifIterator != interfaceList->end(); ifIterator++)
					{
						try
						{
							std::cout << "5.1.5   Open interface " << std::endl;
							std::cout << "  5.2.2   Interface ID:      " << ifIterator->first << std::endl;
							std::cout << "          Interface Type:    " << ifIterator->second->GetTLType() << std::endl;
							std::cout << "          Interface Name:    " << ifIterator->second->GetDisplayName() << std::endl;
							ifIterator->second->Open();
							//search for any camera is connetced to this interface
							deviceList = ifIterator->second->GetDevices();
							deviceList->Refresh(100);
							if (deviceList->size() == 0)
							{
								std::cout << "5.1.13   Close interface (" << deviceList->size() << " cameras found) " << std::endl << std::endl;
								ifIterator->second->Close();
							}
							else
							{
								sInterfaceID = ifIterator->first;
								std::cout << "   " << std::endl;
								std::cout << "        Opened interface - NodeList Information" << std::endl;
								if (ifIterator->second->GetTLType() == "GEV")
								{
									bo_int64 iIpAddress = ifIterator->second->GetNode("GevInterfaceSubnetIPAddress")->GetInt();
									std::cout << "          GevInterfaceSubnetIPAddress: " << (iIpAddress >> 24) << "."
										<< ((iIpAddress & 0xffffff) >> 16) << "."
										<< ((iIpAddress & 0xffff) >> 8) << "."
										<< (iIpAddress & 0xff) << std::endl;
									bo_int64 iSubnetMask = ifIterator->second->GetNode("GevInterfaceSubnetMask")->GetInt();
									std::cout << "          GevInterfaceSubnetMask:      " << (iSubnetMask >> 24) << "."
										<< ((iSubnetMask & 0xffffff) >> 16) << "."
										<< ((iSubnetMask & 0xffff) >> 8) << "."
										<< (iSubnetMask & 0xff) << std::endl;
								}
								if (ifIterator->second->GetTLType() == "U3V")
								{
									//std::cout << "          NodeListCount:     " << ifIterator->second->GetNodeList()->GetNodeCount() << std::endl;    
								}
								std::cout << "  " << std::endl;
								break;
							}
						}
						catch (BGAPI2::Exceptions::ResourceInUseException& ex)
						{
							returncode = 0 == returncode ? 1 : returncode;
							std::cout << " Interface " << ifIterator->first << " already opened " << std::endl;
							std::cout << " ResourceInUseException: " << ex.GetErrorDescription() << std::endl;
						}
					}
				}
				catch (BGAPI2::Exceptions::IException& ex)
				{
					returncode = 0 == returncode ? 1 : returncode;
					std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
					std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
					std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
				}


				//if a camera is connected to the system interface then leave the system loop
				if (sInterfaceID != "")
				{
					break;
				}
			}
			catch (BGAPI2::Exceptions::ResourceInUseException& ex)
			{
				returncode = 0 == returncode ? 1 : returncode;
				std::cout << " System " << sysIterator->first << " already opened " << std::endl;
				std::cout << " ResourceInUseException: " << ex.GetErrorDescription() << std::endl;
			}
		}
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}

	if (sSystemID == "")
	{
		std::cout << " No System found " << std::endl;
		std::cout << std::endl << "End" << std::endl << "Input any number to close the program:";
		int endKey = 0;
		std::cin >> endKey;
		BGAPI2::SystemList::ReleaseInstance();
		//RELEASE IMAGE PROCESSOR
		BGAPI2::ImageProcessor::ReleaseInstance();
		return returncode;
	}
	else
	{
		pSystem = (*systemList)[sSystemID];
	}


	if (sInterfaceID == "")
	{
		std::cout << " No camera found " << sInterfaceID << std::endl;
		std::cout << std::endl << "End" << std::endl << "Input any number to close the program:";
		int endKey = 0;
		std::cin >> endKey;
		pSystem->Close();
		BGAPI2::SystemList::ReleaseInstance();
		//RELEASE IMAGE PROCESSOR
		BGAPI2::ImageProcessor::ReleaseInstance();
		return returncode;
	}
	else
	{
		pInterface = (*interfaceList)[sInterfaceID];
	}


	std::cout << "DEVICE LIST" << std::endl;
	std::cout << "###########" << std::endl << std::endl;

	try
	{
		//COUNTING AVAILABLE CAMERAS
		deviceList = pInterface->GetDevices();
		deviceList->Refresh(100);
		std::cout << "5.1.6   Detected devices:         " << deviceList->size() << std::endl;

		//DEVICE INFORMATION BEFORE OPENING
		for (DeviceList::iterator devIterator = deviceList->begin(); devIterator != deviceList->end(); devIterator++)
		{
			std::cout << "  5.2.3   Device DeviceID:        " << devIterator->first << std::endl;
			std::cout << "          Device Model:           " << devIterator->second->GetModel() << std::endl;
			std::cout << "          Device SerialNumber:    " << devIterator->second->GetSerialNumber() << std::endl;
			std::cout << "          Device Vendor:          " << devIterator->second->GetVendor() << std::endl;
			std::cout << "          Device TLType:          " << devIterator->second->GetTLType() << std::endl;
			std::cout << "          Device AccessStatus:    " << devIterator->second->GetAccessStatus() << std::endl;
			std::cout << "          Device UserID:          " << devIterator->second->GetDisplayName() << std::endl << std::endl;
		}
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}


	std::cout << "DEVICE" << std::endl;
	std::cout << "######" << std::endl << std::endl;

	//OPEN THE FIRST CAMERA IN THE LIST
	try
	{
		for (DeviceList::iterator devIterator = deviceList->begin(); devIterator != deviceList->end(); devIterator++)
		{
			try
			{
				std::cout << "5.1.7   Open first device " << std::endl;
				std::cout << "          Device DeviceID:        " << devIterator->first << std::endl;
				std::cout << "          Device Model:           " << devIterator->second->GetModel() << std::endl;
				std::cout << "          Device SerialNumber:    " << devIterator->second->GetSerialNumber() << std::endl;
				std::cout << "          Device Vendor:          " << devIterator->second->GetVendor() << std::endl;
				std::cout << "          Device TLType:          " << devIterator->second->GetTLType() << std::endl;
				std::cout << "          Device AccessStatus:    " << devIterator->second->GetAccessStatus() << std::endl;
				std::cout << "          Device UserID:          " << devIterator->second->GetDisplayName() << std::endl << std::endl;
				devIterator->second->Open();
				sDeviceID = devIterator->first;
				std::cout << "        Opened device - RemoteNodeList Information " << std::endl;
				std::cout << "          Device AccessStatus:    " << devIterator->second->GetAccessStatus() << std::endl;

				//SERIAL NUMBER
				if (devIterator->second->GetRemoteNodeList()->GetNodePresent("DeviceSerialNumber"))
					std::cout << "          DeviceSerialNumber:     " << devIterator->second->GetRemoteNode("DeviceSerialNumber")->GetValue() << std::endl;
				else if (devIterator->second->GetRemoteNodeList()->GetNodePresent("DeviceID"))
					std::cout << "          DeviceID (SN):          " << devIterator->second->GetRemoteNode("DeviceID")->GetValue() << std::endl;
				else
					std::cout << "          SerialNumber:           Not Available " << std::endl;

				//DISPLAY DEVICEMANUFACTURERINFO
				if (devIterator->second->GetRemoteNodeList()->GetNodePresent("DeviceManufacturerInfo"))
					std::cout << "          DeviceManufacturerInfo: " << devIterator->second->GetRemoteNode("DeviceManufacturerInfo")->GetValue() << std::endl;


				//DISPLAY DEVICEFIRMWAREVERSION OR DEVICEVERSION
				if (devIterator->second->GetRemoteNodeList()->GetNodePresent("DeviceFirmwareVersion"))
					std::cout << "          DeviceFirmwareVersion:  " << devIterator->second->GetRemoteNode("DeviceFirmwareVersion")->GetValue() << std::endl;
				else if (devIterator->second->GetRemoteNodeList()->GetNodePresent("DeviceVersion"))
					std::cout << "          DeviceVersion:          " << devIterator->second->GetRemoteNode("DeviceVersion")->GetValue() << std::endl;
				else
					std::cout << "          DeviceVersion:          Not Available " << std::endl;

				if (devIterator->second->GetTLType() == "GEV")
				{
					std::cout << "          GevCCP:                 " << devIterator->second->GetRemoteNode("GevCCP")->GetValue() << std::endl;
					std::cout << "          GevCurrentIPAddress:    " << ((devIterator->second->GetRemoteNode("GevCurrentIPAddress")->GetInt() & 0xff000000) >> 24) << "." << ((devIterator->second->GetRemoteNode("GevCurrentIPAddress")->GetInt() & 0x00ff0000) >> 16) << "." << ((devIterator->second->GetRemoteNode("GevCurrentIPAddress")->GetInt() & 0x0000ff00) >> 8) << "." << (devIterator->second->GetRemoteNode("GevCurrentIPAddress")->GetInt() & 0x0000ff) << std::endl;
					std::cout << "          GevCurrentSubnetMask:   " << ((devIterator->second->GetRemoteNode("GevCurrentSubnetMask")->GetInt() & 0xff000000) >> 24) << "." << ((devIterator->second->GetRemoteNode("GevCurrentSubnetMask")->GetInt() & 0x00ff0000) >> 16) << "." << ((devIterator->second->GetRemoteNode("GevCurrentSubnetMask")->GetInt() & 0x0000ff00) >> 8) << "." << (devIterator->second->GetRemoteNode("GevCurrentSubnetMask")->GetInt() & 0x0000ff) << std::endl;
				}
				std::cout << "  " << std::endl;
				break;
			}
			catch (BGAPI2::Exceptions::ResourceInUseException& ex)
			{
				returncode = 0 == returncode ? 1 : returncode;
				std::cout << " Device  " << devIterator->first << " already opened " << std::endl;
				std::cout << " ResourceInUseException: " << ex.GetErrorDescription() << std::endl;
			}
			catch (BGAPI2::Exceptions::AccessDeniedException& ex)
			{
				returncode = 0 == returncode ? 1 : returncode;
				std::cout << " Device  " << devIterator->first << " already opened " << std::endl;
				std::cout << " AccessDeniedException " << ex.GetErrorDescription() << std::endl;
			}
		}
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}

	if (sDeviceID == "")
	{
		std::cout << " No Device found " << sDeviceID << std::endl;
		std::cout << std::endl << "End" << std::endl << "Input any number to close the program:";
		int endKey = 0;
		std::cin >> endKey;
		pInterface->Close();
		pSystem->Close();
		BGAPI2::SystemList::ReleaseInstance();
		//RELEASE IMAGE PROCESSOR
		BGAPI2::ImageProcessor::ReleaseInstance();
		return returncode;
	}
	else
	{
		pDevice = (*deviceList)[sDeviceID];
	}


	std::cout << "DEVICE PARAMETER SETUP" << std::endl;
	std::cout << "######################" << std::endl << std::endl;

	try
	{
		//SET TRIGGER MODE OFF (FreeRun)
		pDevice->GetRemoteNode("TriggerMode")->SetString("Off");
		std::cout << "         TriggerMode:             " << pDevice->GetRemoteNode("TriggerMode")->GetValue() << std::endl;
		std::cout << std::endl;
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}


	std::cout << "DATA STREAM LIST" << std::endl;
	std::cout << "################" << std::endl << std::endl;

	try
	{
		//COUNTING AVAILABLE DATASTREAMS
		datastreamList = pDevice->GetDataStreams();
		datastreamList->Refresh();
		std::cout << "5.1.8   Detected datastreams:     " << datastreamList->size() << std::endl;

		//DATASTREAM INFORMATION BEFORE OPENING
		for (DataStreamList::iterator dstIterator = datastreamList->begin(); dstIterator != datastreamList->end(); dstIterator++)
		{
			std::cout << "  5.2.4   DataStream ID:          " << dstIterator->first << std::endl << std::endl;
		}
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}


	std::cout << "DATA STREAM" << std::endl;
	std::cout << "###########" << std::endl << std::endl;

	//OPEN THE FIRST DATASTREAM IN THE LIST
	try
	{
		for (DataStreamList::iterator dstIterator = datastreamList->begin(); dstIterator != datastreamList->end(); dstIterator++)
		{
			std::cout << "5.1.9   Open first datastream " << std::endl;
			std::cout << "          DataStream ID:          " << dstIterator->first << std::endl << std::endl;
			dstIterator->second->Open();
			sDataStreamID = dstIterator->first;
			std::cout << "        Opened datastream - NodeList Information" << std::endl;
			std::cout << "          StreamAnnounceBufferMinimum:  " << dstIterator->second->GetNode("StreamAnnounceBufferMinimum")->GetValue() << std::endl;
			if (dstIterator->second->GetTLType() == "GEV")
			{
				std::cout << "          StreamDriverModel:            " << dstIterator->second->GetNode("StreamDriverModel")->GetValue() << std::endl;
			}
			std::cout << "  " << std::endl;
			break;
		}
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}

	if (sDataStreamID == "")
	{
		std::cout << " No DataStream found " << sDataStreamID << std::endl;
		std::cout << std::endl << "End" << std::endl << "Input any number to close the program:";
		int endKey = 0;
		std::cin >> endKey;
		pDevice->Close();
		pInterface->Close();
		pSystem->Close();
		BGAPI2::SystemList::ReleaseInstance();
		//RELEASE IMAGE PROCESSOR
		BGAPI2::ImageProcessor::ReleaseInstance();
		return returncode;
	}
	else
	{
		pDataStream = (*datastreamList)[sDataStreamID];
	}


	std::cout << "BUFFER LIST" << std::endl;
	std::cout << "###########" << std::endl << std::endl;

	try
	{
		int MemoryBlock_number=4;
		std::vector<char *> pMemoryBlock(MemoryBlock_number);
		std::vector<unsigned char *> pUserBufferPixeltransformation(MemoryBlock_number);
		//BufferList
		bufferList = pDataStream->GetBufferList();
		bo_uint64 payloadsize=pDataStream->GetDefinesPayloadSize()?pDataStream->GetPayloadSize():pDevice->GetPayloadSize();

		// 4 buffers using internal buffer mode
		for (int i = 0; i<MemoryBlock_number; i++)
		{
			pMemoryBlock[i]=(char *)new char[(unsigned int)payloadsize];
			pUserBufferPixeltransformation[i]=(unsigned char *)new char[(unsigned int)(pDevice->GetRemoteNode("Width")->GetInt()*pDevice->GetRemoteNode("Height")->GetInt())];
			pBuffer = new BGAPI2::Buffer(pMemoryBlock[i],payloadsize, pUserBufferPixeltransformation[i]);
			bufferList->Add(pBuffer);
			/*std::cout<<"add external buffer ["
				     <<pBuffer->GetMemSize()
					 <<"bytes]"
					 <<std::endl;*/
		}
		std::cout << "5.1.10   Announced buffers:       " << bufferList->GetAnnouncedCount() << " using " << pBuffer->GetMemSize() * bufferList->GetAnnouncedCount()/pow(10,6) << " [Mbytes]" << std::endl;
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}

	try
	{
		for (BufferList::iterator bufIterator = bufferList->begin(); bufIterator != bufferList->end(); bufIterator++)
		{
			bufIterator->second->QueueBuffer();
		}
		std::cout << "5.1.11   Queued buffers:          " << bufferList->GetQueuedCount() << std::endl;
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}
	std::cout << " " << std::endl;

	std::cout << "CAMERA START" << std::endl;
	std::cout << "############" << std::endl << std::endl;

	//START DataStream acquisition
	try
	{
		pDataStream->StartAcquisitionContinuous();
		std::cout << "5.1.12   DataStream started " << std::endl;
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}

	//START CAMERA
	try
	{

		std::cout << "5.1.12   " << pDevice->GetModel() << " started " << std::endl;
		pDevice->GetRemoteNode("AcquisitionStart")->Execute();
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}

	//CAPTURE 8 IMAGES
	std::cout << " " << std::endl;
	std::cout << "CAPTURE & TRANSFORM 8 IMAGES" << std::endl;
	std::cout << "############################" << std::endl << std::endl;
}

int end_camera(BGAPI2::System * pSystem, BGAPI2::Interface * pInterface, BGAPI2::Device * pDevice, BGAPI2::DataStream * pDataStream, BGAPI2::DataStreamList *datastreamList, int returncode, BGAPI2::Buffer * pBuffer, BGAPI2::BufferList *bufferList) {

	std::cout << " " << std::endl;
	std::cout << "CAMERA STOP" << std::endl;
	std::cout << "###########" << std::endl << std::endl;

	//STOP CAMERA
	try
	{
		//SEARCH FOR 'AcquisitionAbort'
		if (pDevice->GetRemoteNodeList()->GetNodePresent("AcquisitionAbort"))
		{
			pDevice->GetRemoteNode("AcquisitionAbort")->Execute();
			std::cout << "5.1.12   " << pDevice->GetModel() << " aborted " << std::endl;
		}

		pDevice->GetRemoteNode("AcquisitionStop")->Execute();
		std::cout << "5.1.12   " << pDevice->GetModel() << " stopped " << std::endl;
		std::cout << std::endl;

		BGAPI2::String sExposureNodeName = "";
		if (pDevice->GetRemoteNodeList()->GetNodePresent("ExposureTime")) {
			sExposureNodeName = "ExposureTime";
		}
		else if (pDevice->GetRemoteNodeList()->GetNodePresent("ExposureTimeAbs")) {
			sExposureNodeName = "ExposureTimeAbs";
		}
		std::cout << "         ExposureTime:                   " << std::fixed << std::setprecision(0) << pDevice->GetRemoteNode(sExposureNodeName)->GetDouble() << " [" << pDevice->GetRemoteNode(sExposureNodeName)->GetUnit() << "]" << std::endl;
		if (pDevice->GetTLType() == "GEV")
		{
			if (pDevice->GetRemoteNodeList()->GetNodePresent("DeviceStreamChannelPacketSize"))
				std::cout << "         DeviceStreamChannelPacketSize:  " << pDevice->GetRemoteNode("DeviceStreamChannelPacketSize")->GetInt() << " [bytes]" << std::endl;
			else
				std::cout << "         GevSCPSPacketSize:              " << pDevice->GetRemoteNode("GevSCPSPacketSize")->GetInt() << " [bytes]" << std::endl;
			std::cout << "         GevSCPD (PacketDelay):          " << pDevice->GetRemoteNode("GevSCPD")->GetInt() << " [tics]" << std::endl;
		}
		std::cout << std::endl;
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}

	//STOP DataStream acquisition
	try
	{
		if (pDataStream->GetTLType() == "GEV")
		{
			//DataStream Statistic
			std::cout << "         DataStream Statistics " << std::endl;
			std::cout << "           DataBlockComplete:              " << pDataStream->GetNodeList()->GetNode("DataBlockComplete")->GetInt() << std::endl;
			std::cout << "           DataBlockInComplete:            " << pDataStream->GetNodeList()->GetNode("DataBlockInComplete")->GetInt() << std::endl;
			std::cout << "           DataBlockMissing:               " << pDataStream->GetNodeList()->GetNode("DataBlockMissing")->GetInt() << std::endl;
			std::cout << "           PacketResendRequestSingle:      " << pDataStream->GetNodeList()->GetNode("PacketResendRequestSingle")->GetInt() << std::endl;
			std::cout << "           PacketResendRequestRange:       " << pDataStream->GetNodeList()->GetNode("PacketResendRequestRange")->GetInt() << std::endl;
			std::cout << "           PacketResendReceive:            " << pDataStream->GetNodeList()->GetNode("PacketResendReceive")->GetInt() << std::endl;
			std::cout << "           DataBlockDroppedBufferUnderrun: " << pDataStream->GetNodeList()->GetNode("DataBlockDroppedBufferUnderrun")->GetInt() << std::endl;
			std::cout << "           Bitrate:                        " << pDataStream->GetNodeList()->GetNode("Bitrate")->GetDouble() << std::endl;
			std::cout << "           Throughput:                     " << pDataStream->GetNodeList()->GetNode("Throughput")->GetDouble() << std::endl;
			std::cout << std::endl;
		}
		if (pDataStream->GetTLType() == "U3V")
		{
			//DataStream Statistic
			std::cout << "         DataStream Statistics " << std::endl;
			std::cout << "           GoodFrames:            " << pDataStream->GetNodeList()->GetNode("GoodFrames")->GetInt() << std::endl;
			std::cout << "           CorruptedFrames:       " << pDataStream->GetNodeList()->GetNode("CorruptedFrames")->GetInt() << std::endl;
			std::cout << "           LostFrames:            " << pDataStream->GetNodeList()->GetNode("LostFrames")->GetInt() << std::endl;
			std::cout << std::endl;
		}

		pDataStream->StopAcquisition();
		std::cout << "5.1.12   DataStream stopped " << std::endl;
		bufferList->DiscardAllBuffers();
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}
	std::cout << std::endl;


	std::cout << "RELEASE" << std::endl;
	std::cout << "#######" << std::endl << std::endl;

	//Release buffers
	std::cout << "5.1.13   Releasing the resources " << std::endl;
	try
	{
		while (bufferList->size() > 0)
		{
			pBuffer = bufferList->begin()->second;
			bufferList->RevokeBuffer(pBuffer);
			delete pBuffer;
		}
		std::cout << "         buffers after revoke:    " << bufferList->size() << std::endl;

		//Release extarnal buffer memory
		for (int i = 0; i < (int)fps*4.25; i++) {
			//delete [] pMemoryBlock[i];
		}

		pDataStream->Close();
		pDevice->Close();
		pInterface->Close();
		pSystem->Close();
		BGAPI2::SystemList::ReleaseInstance();
		//RELEASE IMAGE PROCESSOR
		BGAPI2::ImageProcessor::ReleaseInstance();
		std::cout << "         ImageProcessor released" << std::endl << std::endl;
	}
	catch (BGAPI2::Exceptions::IException& ex)
	{
		returncode = 0 == returncode ? 1 : returncode;
		std::cout << "ExceptionType:    " << ex.GetType() << std::endl;
		std::cout << "ErrorDescription: " << ex.GetErrorDescription() << std::endl;
		std::cout << "in function:      " << ex.GetFunctionName() << std::endl;
	}

	std::cout << std::endl;
	std::cout << "End" << std::endl << std::endl;

	/*
	std::cout << "Input any number to close the program:";
	int endKey = 0;
	std::cin >> endKey;*/
	return returncode;
}