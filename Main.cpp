// #include <opencv2/core/utility.hpp>
// #include <opencv2/imgproc/imgproc.hpp>
// #include <opencv2/highgui/highgui.hpp>
#include <opencv2/opencv.hpp>
// #include "timer.hpp"
#include "mjpegwriter.hpp"

#include <asm/types.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <linux/videodev2.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace cv;
using namespace std;

#define TEST_MY 1

static unsigned long long capture_images_count = 0;
int udp_fd=0;
int des_port = 51949;
struct sockaddr_in dest_addr;
char udp_buf[1000*655];

typedef struct
{
    void *start;
    int length;
}Data_t;//for video frame

void init_udp_socket(const char *addr)
{
    udp_fd = socket(AF_INET, SOCK_DGRAM, 0);

    dest_addr.sin_family=AF_INET;
    dest_addr.sin_port = htons(des_port);
    if(inet_aton(addr, &dest_addr.sin_addr) == 0)
        printf("inet_aton() failed");
    bzero(&(dest_addr.sin_zero),8);
}

void start_udp_stream(const char *addr,Data_t mjpeg_data)
{
    if(udp_fd <= 0){
        init_udp_socket(addr);
        printf("sending frame-stream to %s:%d \n", addr, des_port);
    }
    int udp_mtu = 1316;
    int len = mjpeg_data.length;
    int send_count = 0;
    int one_send = 0;
    int need_send = 0;
    memcpy(udp_buf, mjpeg_data.start, mjpeg_data.length);
    while(send_count < len) {
        need_send = send_count+ udp_mtu >len? len - send_count : udp_mtu;
        one_send = sendto(udp_fd, udp_buf + send_count, need_send, 0, (const struct sockaddr*)&dest_addr, sizeof(dest_addr));
        if(one_send<0)
        {
            printf("send data error\n");
            return;
        }
        send_count += one_send;
    }
    if(++capture_images_count % 100 == 0)
        printf("frame.len=%d, images_count = %lld\n", len, capture_images_count);
}

int read_mipi_raw(const char* fname, unsigned char*& pbuf)
{
    FILE *f = fopen(fname,"rb");
    fseek(f,0,SEEK_END);
    int flen = ftell(f);
    fseek(f,0,SEEK_SET);
    unsigned char* tmpbuf = (unsigned char*)malloc(flen);

    fread(tmpbuf,1,flen,f);
    assert(flen%5 == 0);

    int length = flen/5*4;
    pbuf = (unsigned char*)malloc(length);

    //mipi to raw
    for(int i=0; i<flen/5; i++)
    {
        unsigned char* puc = tmpbuf + i*5;
        // int b[5];
        for(int j=0; j<4; j++)
        {
            // b[j] = puc[j];
            // printf("%d ",puc[j]);
            pbuf[i*4+j]=puc[j];
        }
        // printf("%d \n",puc[5]);
        // fflush();
    }

    for (int i=0; i<480; i++)
    {
        for(int j=0; j<20; j++)
        {
            printf("%d ",pbuf[i*640+639-j]);
        }
        printf("\n");
    }
    free(tmpbuf);
    return length;
}

int main(int, char**)
{
    VideoCapture cap("2018-08-27-22-27-58.avi");
    jcodec::MjpegWriter * j = new jcodec::MjpegWriter();
    VideoWriter outputVideo;
    double ttotal = 0;
    char addr[20] = {};

    Rect rect(0, 0, 640, 480);
    // Mat frame(rect.size(), CV_8UC3);
    Mat frame;
    cap >> frame;
    int nframes = 0;
    Data_t tmjpg;
    memcpy(addr, "127.0.0.1", sizeof("127.0.0.1"));
#if TEST_MY
    j->Open("out.avi", (uchar)30, frame.cols, frame.rows);
#else
    outputVideo.open("out2.avi", outputVideo.fourcc('M', 'J', 'P', 'G'), 10.0, img.size(), true);
#endif


    unsigned char* pbuf;
    // int len = read_mipi_raw("/home/jimmypan/Downloads/mipi_raw3/2+1_007.mipi_raw",pbuf);
    // Mat mipFrame = Mat(480,640,CV_8UC1,pbuf);

    while(1)
	{
        cap >> frame;
        // imshow("frame",mipFrame);
        // waitKey(10);
        // mipFrame.copyTo(frame);
        if(frame.empty())
        {
            break;
        }
        nframes ++;
        Mat gray;
        cvtColor(frame,gray,COLOR_BGR2GRAY);

        // gray = frame;
        char showstr[20];
        sprintf(showstr,"%d",nframes);
        // imwrite("2-1.jpg",gray);
        cv::putText(gray, showstr, cv::Point(300, 200), CV_FONT_HERSHEY_SIMPLEX, 2, Scalar(255,255,255), 2);
        imshow("frame",gray);
        waitKey(10);
        // double tstart = (double)getTickCount();

#if TEST_MY
        // 
        tmjpg.length=j->toJPGframe(gray.data,gray.cols,gray.rows,12,tmjpg.start);
        j->Write((const uchar*)tmjpg.start,tmjpg.length);
        // tmjpg.length=j->toJPGframe(pbuf,640,480,12,tmjpg.start);
        start_udp_stream(addr,tmjpg);
        free(tmjpg.start);
#else
        outputVideo.write(gray);
#endif
        // double tend = (double)getTickCount();
        // ttotal += tend - tstart;
	}

#if TEST_MY
    j->Close();
#else
    outputVideo.release();
#endif
    // printf("time per frame (including file i/o)=%.1fms\n", (double)tt.get_elapsed_ms()/nframes);

	return 0;
}

int main2()
{
    unsigned char* pbuf;
    string img_root = "/home/jimmypan/Downloads/mipi_raw11/";
    for(int c=0; c<2; c++)
    {
        for(int i=0; i<20; i++)
        {
            char mipi[32];
            // char sjpg[64];
            // int c
            sprintf(mipi,"%d+%d_%03d",c*2,i%2,i);
            // sprintf(mipi,"%d+%d_%03d.",c*2,(i+(c==0?1:0))%2,i);
            string mipi_name = img_root + mipi + ".mipi_raw";
            string save_name = img_root + mipi + "_eh.jpg";
            int len = read_mipi_raw(mipi_name.c_str(),pbuf);
            Mat mipFrame = Mat(480,640,CV_8UC1,pbuf);
            equalizeHist(mipFrame, mipFrame);
            // imshow("test",mipFrame);
            // waitKey(0);
            imwrite(save_name,mipFrame);
            free(pbuf);
        }

    }
}