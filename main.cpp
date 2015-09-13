
#include <iostream>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <linux/types.h>
#include <linux/fb.h>
#include <linux/videodev2.h>
#include <signal.h>

#include <semaphore.h>

#include "rtpsession.h"
#include "rtppacket.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"

#include "rtptransmitter.h"

#include "SsbSipH264Decode.h"
#include "FrameExtractor.h"
#include "H264Frames.h"
#include "LogMsg.h"
#include "performance.h"
#include "lcd.h"
#include "MfcDriver.h"
#include "FileRead.h"
#include "s3c_pp.h"
using namespace std;

#define CLEAR(x)  memset (&(x), 0, sizeof (x))
#define LCD_BPP_V4L2        V4L2_PIX_FMT_RGB565
#define VIDEO_WIDTH   320
#define VIDEO_HEIGHT  240
#define YUV_FRAME_BUFFER_SIZE   VIDEO_WIDTH*VIDEO_HEIGHT*2
#define PP_DEV_NAME     "/dev/s3c-pp"
#define INPUT_BUFFER_SIZE       (204800)
extern int FriendlyARMWidth, FriendlyARMHeight;
#define FB0_WIDTH FriendlyARMWidth
#define FB0_HEIGHT FriendlyARMHeight
#define FB0_BPP         16
#define FB0_COLOR_SPACE RGB16
void sig_del_h264(int signo);

unsigned char delimiter_h264[4]  = {0x00, 0x00, 0x00, 0x01};
#define H264FrameSize (115200)
#define MTU 1500
#define H264 96

 void         *handle;
 int          in_fd;
 int          file_size;
 char         *in_addr;
 int          fb_size;
 int          pp_fd, fb_fd;
 char         *fb_addr;
 bool         IsExtractConfig;
 int          displaycount = 0;

void sig_del_h264(int signo)
{
    printf("[H.264 display] signal handling\n");

    ioctl(fb_fd, SET_OSD_STOP);
    SsbSipH264DecodeDeInit(handle);

    munmap(in_addr, file_size);
    munmap(fb_addr, fb_size);
    close(pp_fd);
    close(fb_fd);
    close(in_fd);

    exit(1);
}

int FriendlyARMWidth, FriendlyARMHeight;
void FBOpen(void)
{
    struct fb_fix_screeninfo FBFix;
    struct fb_var_screeninfo FBVar;
    int FBHandle = -1;

    FBHandle = open("/dev/fb0", O_RDWR);
    if (ioctl(FBHandle, FBIOGET_FSCREENINFO, &FBFix) == -1 ||
        ioctl(FBHandle, FBIOGET_VSCREENINFO, &FBVar) == -1) {
        fprintf(stderr, "Cannot get Frame Buffer information");
        exit(1);
    }

    FriendlyARMWidth  = FBVar.xres;
    FriendlyARMHeight = FBVar.yres;
    close(FBHandle);
}

void checkerror(int rtperr)
{
        if (rtperr < 0)
        {
                std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
                exit(-1);
        }
}



void			*pStrmBuf;
int				nFrameLeng=0;
unsigned int	pYUVBuf[2];

struct stat				s;
FRAMEX_CTX				*pFrameExCtx;	// frame extractor context
FRAMEX_STRM_PTR 		file_strm;
SSBSIP_H264_STREAM_INFO stream_info;

s3c_pp_params_t	pp_param;
s3c_win_info_t	osd_info_to_driver;

struct fb_fix_screeninfo	lcd_info;


RTPSession                   sess;
RTPUDPv4TransmissionParams   transparams;
RTPSessionParams             sessparams;
uint                         portbase,destport;
uint                         destip;
std::string                  ipstr;
int                          status,i,num;
int                          timeStampInc;
long long                    currentTime;

bool                         firstFrameArrived;
bool                         SPSFrameArrived;
bool                         PPSFrameArrived;
bool                         KeyFrameArrived;

int                     pFrameConfig = 0;

int                          nalType;
int                          FramebufPos;
int                          PayloadLen;
int                          SPSFrameLen =0;
int                          PPSFrameLen = 0;
int                          KeyFrameLen = 0;

unsigned char           *framebuf;
unsigned char           *payloadbuf;
unsigned char           *FirstFramebuf;

unsigned char           *SPSFramebuf;
unsigned char           *PPSFramebuf;
unsigned char           *KeyFramebuf;

int CreateRTPSession()
{

            // First, we'll ask for the necessary information

            std::cout << "Enter local portbase:" << std::endl;
            std::cin >> portbase;
            std::cout << std::endl;

            std::cout << "Enter the destination IP address" << std::endl;
            std::cin >> ipstr;
            destip = inet_addr(ipstr.c_str());
            if (destip == INADDR_NONE)
            {
                    std::cerr << "Bad IP address specified" << std::endl;
                    return -1;
            }

            // The inet_addr function returns a value in network byte order, but
            // we need the IP address in host byte order, so we use a call to
            // ntohl
            destip = ntohl(destip);

            std::cout << "Enter the destination port" << std::endl;
            std::cin >> destport;


            // Now, we'll create a RTP session, set the destination, send some
            // packets and poll for incoming data.


            // IMPORTANT: The local timestamp unit MUST be set, otherwise
            //            RTCP Sender Report info will be calculated wrong
            // In this case, we'll be sending 90000 samples each second, so we'll
            // put the timestamp unit to (1.0/90000.0)
            sessparams.SetOwnTimestampUnit(1.0/90000.0);
            transparams.SetPortbase(portbase);

            status = sess.Create(sessparams,&transparams);
            checkerror(status);
            RTPIPv4Address addr(destip,destport);

            status = sess.AddDestination(addr);
            checkerror(status);

        }


int display()
{
    fprintf(stderr,"get in  init_decode() function ok \n");

    if(signal(SIGINT, sig_del_h264) == SIG_ERR) {
            printf("Sinal Error\n");
    }

// initialize buffer
framebuf = (unsigned char *)malloc(H264FrameSize * sizeof(unsigned char ));
if(framebuf == 0)
{
   fprintf(stderr,"framebuf malloc error\n");
}
//payloadbuf = (unsigned char *)malloc(MTU* sizeof(unsigned char ));
FirstFramebuf = (unsigned char *)malloc(H264FrameSize * sizeof(unsigned char ));
if(FirstFramebuf == 0)
{
   fprintf(stderr,"FirstFramebuf malloc error\n");
}
SPSFramebuf = (unsigned char *)malloc(200 * sizeof(unsigned char ));
if(SPSFramebuf == 0)
{
   fprintf(stderr,"SPStFramebuf malloc error\n");
}
PPSFramebuf = (unsigned char *)malloc(200 * sizeof(unsigned char ));
if(PPSFramebuf == 0)
{
   fprintf(stderr,"PPStFramebuf malloc error\n");
}
KeyFramebuf = (unsigned char *)malloc((H264FrameSize - 400) * sizeof(unsigned char ));
if(KeyFramebuf == 0)
{
   fprintf(stderr,"KeytFramebuf malloc error\n");
}

// Post processor open
pp_fd = open(PP_DEV_NAME, O_RDWR);
if(pp_fd < 0)
{
 printf("Post processor open error\n");
 return -1;
}

// LCD frame buffer open
fb_fd = open("/dev/fb1", O_RDWR|O_NDELAY);
if(fb_fd < 0)
{
   printf("LCD frame buffer open error\n");
   return -1;
}

/*  RTPsession crearte
   *********
  *************/
 CreateRTPSession();




///////////////////////////////////
// Get and Process RTP packet //
///////////////////////////////////
IsExtractConfig = false;
firstFrameArrived = false;
SPSFrameArrived   = false;
PPSFrameArrived   = false;
KeyFrameArrived   = false;
FramebufPos = 0;

while(1)
{
#ifndef RTP_SUPPORT_THREAD
                status = sess.Poll();
                checkerror(status);
#endif // RTP_SUPPORT_THREAD
    sess.BeginDataAccess();
   // check incoming packets
  if (sess.GotoFirstSourceWithData())
  {

    do
    {
     RTPPacket *pack;
     RTPSourceData *srcdat;

     srcdat = sess.GetCurrentSourceInfo();
      while ((pack = sess.GetNextPacket()) != NULL)
      {
       // fprintf(stderr," Packet is received\n");

          ///////////////////////////////////////////////////////
          //Process packet data and decode ,then display
          if(pack->GetPayloadType() == H264)
          {
              //  fprintf(stderr,"H264 Packet is received\n");
              payloadbuf =  pack->GetPayloadData();
              PayloadLen = pack->GetPayloadLength();
              nalType = (0x1f) &  payloadbuf[0];
              //fprintf(stderr,"payload's length is %d\n", PayloadLen);

              // receive the whole nal unit's  last packet
              // assemble as a frame
             if(pack->HasMarker())
             {
                 // FU-A's last packet
                 if(nalType == 28)
                 {
                    nalType = (0x1f) & payloadbuf[1]; // reset the nal unit type
                    memcpy((framebuf+FramebufPos),payloadbuf+2, PayloadLen - 2);
                    FramebufPos = FramebufPos + PayloadLen - 2;

                 }
                 //Nal unit, only one packet
                 else if (nalType > 0 && nalType < 24)
                 {
                    //add StartCode
                    FramebufPos = 0;
                    memcpy((framebuf+FramebufPos), delimiter_h264, 4);
                    FramebufPos += 4 ;
                    memcpy((framebuf+FramebufPos), payloadbuf,PayloadLen);
                    FramebufPos += PayloadLen;

                 }
                 else
                 {
                         fprintf(stderr,"NAl type is error\n");
                 }
                // fprintf(stderr," Framebuf's length is %d\n", FramebufPos);
                 //first, SPS is needed
                 if(nalType == 0x07)
                 {
                     SPSFrameArrived = true;
                     SPSFrameLen = FramebufPos;
                     memcpy(FirstFramebuf, framebuf,SPSFrameLen);
                     if(PPSFrameArrived )
                     {
                         memcpy((FirstFramebuf + SPSFrameLen), PPSFramebuf, PPSFrameLen);
                     }
                     if(KeyFrameArrived )
                     {
                         memcpy((FirstFramebuf + SPSFrameLen + PPSFrameLen), KeyFramebuf, KeyFrameLen);
                     }
                     if(KeyFrameArrived && PPSFrameArrived  )
                     {
                         firstFrameArrived = true;
                     }

                     fprintf(stderr,"SPS Frame is received\n");
                 }
                 //second , PPS is needed
                 if(nalType == 0x08)
                 {
                     PPSFrameArrived = true;
                     PPSFrameLen = FramebufPos;
                     // SPS frame is received
                     if(SPSFrameArrived)
                     {

                         memcpy((FirstFramebuf + SPSFrameLen), framebuf, PPSFrameLen);
                         if(KeyFrameArrived )
                         {
                              memcpy((FirstFramebuf + SPSFrameLen + PPSFrameLen), framebuf, KeyFrameLen);
                              firstFrameArrived = true;
                         }

                     }
                     // put in PPS buffer
                     else
                     {
                         memcpy(PPSFramebuf, framebuf, PPSFrameLen);
                     }

                     fprintf(stderr,"PPS Frame is received\n");
                 }
                 /*
                    finally ,IDR Frame is needed, if pFrameConfig == 2,
                    we can the set 'firstFrameArrived'to true.

                  we can config stream now
                  */
                 if(nalType == 0x05)
                 {
                     KeyFrameArrived = true;
                     KeyFrameLen = FramebufPos;
                     if(SPSFrameArrived && PPSFrameArrived  )
                     {
                         firstFrameArrived = true;
                         memcpy((FirstFramebuf + SPSFrameLen + PPSFrameLen), framebuf, KeyFrameLen);
                     }
                     else
                     {
                             memcpy(KeyFramebuf, framebuf, KeyFrameLen);
                     }

                      fprintf(stderr,"Key Frame is received\n");
                 }

                // FirstFrame is arrived,so we can config the stream for the decode
                 // set the first frame extract and decode for display
                 if(firstFrameArrived)
                 {
                     // fprintf(stderr,"Get in firstFrameArrived function\n");
                     if(!IsExtractConfig)
                     {
                         fprintf(stderr,"Get in FrameExtractor Initialization function\n");
                         ///////////////////////////////////
                         // FrameExtractor Initialization //
                         ///////////////////////////////////

                         pFrameExCtx = FrameExtractorInit(FRAMEX_IN_TYPE_MEM, delimiter_h264, sizeof(delimiter_h264), 1);
                         file_strm.p_start = file_strm.p_cur = (unsigned char *)FirstFramebuf;
                         file_strm.p_end = (unsigned char *)(FirstFramebuf + SPSFrameLen + PPSFrameLen + KeyFrameLen );
                         FrameExtractorFirst(pFrameExCtx, &file_strm);

                         IsExtractConfig = true;
                         //////////////////////////////////////
                         //    1. Create new instance      ///
                         //      (SsbSipH264DecodeInit)    ///
                         //////////////////////////////////////
                         handle = SsbSipH264DecodeInit();
                         if (handle == NULL) {
                                 printf("H264_Dec_Init Failed.\n");
                                 exit(-1) ;
                         }

                         /////////////////////////////////////////////
                         //   2. Obtaining the Input Buffer      ///
                         //     (SsbSipH264DecodeGetInBuf)       ///
                         /////////////////////////////////////////////
                         pStrmBuf = SsbSipH264DecodeGetInBuf(handle, nFrameLeng);
                         if (pStrmBuf == NULL) {
                                 printf("SsbSipH264DecodeGetInBuf Failed.\n");
                                 SsbSipH264DecodeDeInit(handle);
                              exit(-1) ;
                         }

                         ////////////////////////////////////
                         //  H264 CONFIG stream extraction //
                         ////////////////////////////////////
                         nFrameLeng = ExtractConfigStreamH264(pFrameExCtx, &file_strm, (unsigned char*)pStrmBuf, INPUT_BUFFER_SIZE, NULL);


                         ////////////////////////////////////////////////////////////////
                         //    3. Configuring the instance with the config stream    ///
                         //      (SsbSipH264DecodeExe)                             ///
                         ////////////////////////////////////////////////////////////////
                         if (SsbSipH264DecodeExe(handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK) {
                                 printf("H.264 Decoder Configuration Failed.\n");
                                exit(-1) ;
                         }


                         /////////////////////////////////////
                         // 4. Get stream information   ///
                         /////////////////////////////////////
                         SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_STREAMINFO, &stream_info);

                 //	printf("\t<STREAMINFO> width=%d   height=%d.\n", stream_info.width, stream_info.height);


                         // set post processor configuration
                         pp_param.src_full_width	    = stream_info.buf_width;
                         pp_param.src_full_height	= stream_info.buf_height;
                         pp_param.src_start_x		= 0;
                         pp_param.src_start_y		= 0;
                         pp_param.src_width			= pp_param.src_full_width;
                         pp_param.src_height			= pp_param.src_full_height;
                         pp_param.src_color_space	= YC420;
                         pp_param.dst_start_x		= 0;
                         pp_param.dst_start_y		= 0;
                         pp_param.dst_full_width	    = FB0_WIDTH;		// destination width
                         pp_param.dst_full_height	= FB0_HEIGHT;		// destination height
                         pp_param.dst_width			= pp_param.dst_full_width;
                         pp_param.dst_height			= pp_param.dst_full_height;
                         pp_param.dst_color_space	= FB0_COLOR_SPACE;
                         pp_param.out_path           = DMA_ONESHOT;

                         ioctl(pp_fd, S3C_PP_SET_PARAMS, &pp_param);

                         // get LCD frame buffer address
                         fb_size = pp_param.dst_full_width * pp_param.dst_full_height * 2;	// RGB565
                         fb_addr = (char *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
                         if (fb_addr == NULL) {
                                 printf("LCD frame buffer mmap failed\n");
                                 exit(-1) ;
                         }

                         osd_info_to_driver.Bpp			= FB0_BPP;	// RGB16
                         osd_info_to_driver.LeftTop_x	= 0;
                         osd_info_to_driver.LeftTop_y	= 0;
                         osd_info_to_driver.Width		= FB0_WIDTH;	// display width
                         osd_info_to_driver.Height		= FB0_HEIGHT;	// display height

                         // set OSD's information
                         if(ioctl(fb_fd, SET_OSD_INFO, &osd_info_to_driver)) {
                                 printf("Some problem with the ioctl SET_OSD_INFO\n");
                                 exit(-1) ;
                         }

                         ioctl(fb_fd, SET_OSD_START);

                     }

                     /**********************
                       **********************
                        decode and display
                       ***********************
                       ************************
                      */                 

                     else
                     {
                         //debug for the speed
                         displaycount++;
                          if(displaycount == 1000)
                              fprintf(stderr,"display 1000 frames now \n ");

                         ////////////////////////////////
                         //    5. DECODE            ///
                         //  (SsbSipH264DecodeExe)   ///
                         //////////////////////////////////
                         if (SsbSipH264DecodeExe(handle, nFrameLeng) != SSBSIP_H264_DEC_RET_OK)
                                 break;

                         //////////////////////////////////////////////
                         //    6. Obtaining the Output Buffer      ///
                         //   (SsbSipH264DecodeGetOutBuf)       ///
                         //////////////////////////////////////////////
                         SsbSipH264DecodeGetConfig(handle, H264_DEC_GETCONF_PHYADDR_FRAM_BUF, pYUVBuf);

                         /*
                            update pstrmbuf (instead of using NextH264Frame function)
                            and decode and display
                          */
                         /////////////////////////////
                         // Next H.264 VIDEO stream //
                         /////////////////////////////
                           nFrameLeng = FramebufPos;
                           memcpy(pStrmBuf,framebuf, FramebufPos);
                           // Post processing
                                   // pp_param.SrcFrmSt에는 MFC의 output buffer의 physical address가
                                   // pp_param.DstFrmSt에는 LCD frame buffer의 physical address가 입력으로 넣어야 한다.
                                   pp_param.src_buf_addr_phy		= pYUVBuf[0];	// MFC output buffer
                                   ioctl(pp_fd, S3C_PP_SET_SRC_BUF_ADDR_PHY, &pp_param);

                                   ioctl(fb_fd, FBIOGET_FSCREENINFO, &lcd_info);
                                   pp_param.dst_buf_addr_phy		= lcd_info.smem_start;			// LCD frame buffer
                                   ioctl(pp_fd, S3C_PP_SET_DST_BUF_ADDR_PHY, &pp_param);
                                   ioctl(pp_fd, S3C_PP_START);


                                   //////////////////////////////////

                     }

                }
                 FramebufPos = 0;

             } // end of HasMarker

             // RTP packet's marker is false, NAl Unit is part.
             else
             {
                 //FU-A nal packet
                 if(nalType == 28)
                 {
                    nalType = (0x1f) & payloadbuf[1]; // reset the nal unit type
                    // the first packet of FU-A
                    if(payloadbuf[1] & 0x80 )
                    {
                        FramebufPos = 0;
                        //add Startcode

                        memcpy(framebuf,delimiter_h264, sizeof(delimiter_h264));
                        FramebufPos += 4 ;
                        memcpy((framebuf+FramebufPos),(payloadbuf+2),PayloadLen - 2);
                        FramebufPos = FramebufPos + PayloadLen - 2;

                    }
                    // the middle packets of FU-A
                    else
                    {

                        memcpy((framebuf+FramebufPos),(payloadbuf+2),PayloadLen - 2);
                        FramebufPos = FramebufPos + PayloadLen - 2;

                    }
                 }

             }


         }   // end of Type is H264




        //////////////////////////////////////////////////////////
        sess.DeletePacket(pack);
      }
    } while (sess.GotoNextSourceWithData());
   }

     sess.EndDataAccess();


}

}


int main(int argc, char **argv)
{
     FBOpen();
     display();

       free(FirstFramebuf);
       free(SPSFramebuf);
       free(PPSFramebuf);
       free(KeyFramebuf);
       free(framebuf);

       ioctl(fb_fd, SET_OSD_STOP);
       SsbSipH264DecodeDeInit(handle);
       munmap(in_addr, file_size);
       munmap(fb_addr, fb_size);
       close(pp_fd);
       close(fb_fd);
       close(in_fd);
     return 0;

}

