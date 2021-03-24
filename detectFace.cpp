#include "opencv2/objdetect.hpp"
#include "opencv2/highgui.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/videoio.hpp"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <time.h>
#include <cstdlib>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <cstring>
#include <csignal>
#include "mqtt/async_client.h"
#include <cstdlib>

using namespace std;
////////////////////////MQTT config /////////////////////////////////////////////////////
const string DFLT_SERVER_ADDRESS	{ "test.mosquitto.org" };
const string CLIENT_ID				{ "paho_cpp_async_publish" };
const string PERSIST_DIR			{ "./persist" };

const string TOPIC { "hello" };
const string LAST_TOPIC { "lasthello" };

const char* PAYLOAD1 = "Hello World!";

const char* LWT_PAYLOAD = "Last will and testament.";

const int  QOS = 1;

const auto TIMEOUT = std::chrono::seconds(10);
mqtt::async_client client(DFLT_SERVER_ADDRESS, CLIENT_ID, PERSIST_DIR);
const string	address  = DFLT_SERVER_ADDRESS;

/////////////////////////////////////////////////////////////////////////////
 
 
 /**
 * A callback class for use with the main MQTT client.
 */
class callback : public virtual mqtt::callback
{
public:
	void connection_lost(const string& cause) override {
		cout << "\nConnection lost" << endl;
		if (!cause.empty())
			cout << "\tcause: " << cause << endl;
	}

	void delivery_complete(mqtt::delivery_token_ptr tok) override {
		//cout << "\tDelivery complete for token: "
			//<< (tok ? tok->get_message_id() : -1) << endl;
	}
};

 
//////////////////////////////////////////////////////////////////////
void exiting() {
	// Disconnect
	cout << "\nDisconnecting from mqtt Broker..." << endl;
	client.disconnect()->wait();
	cout << "  ...OK" << endl;
    std::cout << "Exiting" << endl;
}
/////////////////////////////////////////////////////////////////////////////


 
using namespace cv;
void facedetectAndDisplay( Mat frame );
void sendDectectionMessage();
void computeEventAlgoOnFaceDetection();
void connectToMqttBroker();
CascadeClassifier face_cascade;
int state=0;
int counter =0;
time_t lt = time(NULL);
 double seconds;
 /////////////////////////////////////////////////////////////////////////////

/**
 * Main Function.
 */
int main( int argc, const char** argv )
{
	std::atexit(exiting);
	
	
	
	   //signal requires lam take an int parameter
    //this parameter is equal to the signals value
    auto lam = 
      [] (int i) { cout << "aborting" << endl; exit(0); };

    //^C
    signal(SIGINT, lam);
    //abort()
    signal(SIGABRT, lam);
    //sent by "kill" command
    signal(SIGTERM, lam);
    //^Z
    signal(SIGTSTP, lam);
	
	callback cb;
	client.set_callback(cb);

	connectToMqttBroker();
    
    //-- 1. Load the cascades
	
	if (!face_cascade.load("/home/pi/opencv/data/lbpcascades/lbpcascade_frontalface_improved.xml")) {
	// if (!face_cascade.load("/home/pi/opencv/data/haarcascades/haarcascade_frontalface_default.xml")) {
        std::cerr << "ERROR! Could not load cascade file\n";
    }
    

       // create video stream instance and set the resolution (if not set then maximum available is used)
	cv::VideoCapture capture(0);
    capture.set(cv::CAP_PROP_FRAME_WIDTH, 320);
    capture.set(cv::CAP_PROP_FRAME_HEIGHT, 240);
    capture.set(cv::CAP_PROP_FPS, 5);
    if ( ! capture.isOpened() )
    {
        cout << "--(!)Error opening video capture\n";
        return -1;
    }
    Mat frame;
    while ( capture.read(frame) )
    {
        if( frame.empty() )
        {
            cout << "--(!) No captured frame -- Break!\n";
            break;
        }
		//-- Detect and display faces
        facedetectAndDisplay( frame );
        if( waitKey(10) == 27 )
        {
            break; // escape
        }
    }
    return 0;
}

 /////////////////////////////////////////////////////////////////////////////
 
void facedetectAndDisplay( Mat frame )
{
    Mat frame_gray;
    cvtColor( frame, frame_gray, COLOR_BGR2GRAY );
    equalizeHist( frame_gray, frame_gray );

    std::vector<Rect> faces;
    //-- 3. Apply the haar cascade classifier to the frame
    face_cascade.detectMultiScale( frame_gray, faces );
	//Draw a ceircle boundary in the detected face
    for ( size_t i = 0; i < faces.size(); i++ )
    {
        Point center( faces[i].x + faces[i].width/2, faces[i].y + faces[i].height/2 );
        ellipse( frame, center, Size( faces[i].width/2, faces[i].height/2 ), 0, 0, 360, Scalar( 255, 0, 255 ), 4 );
       
    }
    //-- Show the video stream
    cv::imshow( "Capture - Face detection", frame );
	if (faces.size() > 0) { 
		
        cout << "Face has been detected" << endl;
		computeEventAlgoOnFaceDetection();
	}
	else {
		state =0;
        cout << "Face has not been detected" << endl;
    }
}

 /////////////////////////////////////////////////////////////////////////////

void computeEventAlgoOnFaceDetection()
{
	if(state ==0)
	{
		counter++;
		seconds=difftime(time(NULL),lt);
		printf ("%.f seconds since last face detected.\n", seconds);
		
			if(seconds>5 && counter>=4 )
			{
				counter=0;
				lt = time(NULL);
				state=1;
				sendDectectionMessage();
			}
	}
	else if(state ==1)
	{
		counter++;
		seconds=difftime(time(NULL),lt);
		printf ("%.f seconds since last face detected.\n", seconds);
		if(seconds>5 && counter>=4 )
		{
		counter=0;
		lt = time(NULL);
		state=1;
		sendDectectionMessage();
		}
				
	}
	else
	{
		counter++;
	}
}

 /////////////////////////////////////////////////////////////////////////////

void connectToMqttBroker()
{
	
	cout << "Initializing for mqtt server '" << address << "'..." << endl;
	
	auto connOpts = mqtt::connect_options_builder()
		.clean_session()
		.will(mqtt::message(LAST_TOPIC, LWT_PAYLOAD, QOS))
		.finalize();

	cout << "  ...OK" << endl;

	cout << "\nConnecting to mqtt broker..." << endl;
		mqtt::token_ptr conntok = client.connect(connOpts);
		cout << "Waiting for the connection..." << endl;
		conntok->wait();
		cout << "  ...OK" << endl;
   
}

//////////////////////////////////////////////////////////////////////

void sendDectectionMessage()
{
	cout << "\nSending detection message..." << endl;
		mqtt::message_ptr pubmsg = mqtt::make_message(TOPIC, PAYLOAD1);
		pubmsg->set_qos(QOS);
		client.publish(pubmsg)->wait_for(TIMEOUT);
		cout << "  ...OK" << endl;
}

