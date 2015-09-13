#include "rtpreceive.h"
#include "rtpsourcedata.h"
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>
#define H264 96
RTPreceive::RTPreceive()
{
}
void checkerror(int rtperr)
{
    if (rtperr < 0)
    {
            std::cout << "ERROR: " << RTPGetErrorString(rtperr) << std::endl;
            exit(-1);
    }
}

int RTPreceive::CreateSession()
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
    sessparams.SetOwnTimestampUnit(1.0/9000.0);
    transparams.SetPortbase(portbase);
    status = sess.Create(sessparams,&transparams);
    checkerror(status);

    RTPIPv4Address addr(destip,destport);

    status = sess.AddDestination(addr);
    checkerror(status);
}

void RTPreceive::OnPollThreadStep()
{
    sess.BeginDataAccess();

            // check incoming packets
            if (GotoFirstSourceWithData())
            {
                    do
                    {
                            RTPPacket *pack;
                            RTPSourceData *srcdat;

                            srcdat = sess.GetCurrentSourceInfo();

                            while ((pack = sess.GetNextPacket()) != NULL)
                            {
                                    ProcessRTPPacket(*srcdat,*pack);
                                    sess.DeletePacket(pack);
                            }
                    } while (sess.GotoNextSourceWithData());
            }

            sess.EndDataAccess();
}
unsigned char* RTPreceive::GetNalUnit()
{
   sess.Poll();
}

void RTPreceive::ProcessRTPPacket(const RTPSourceData &srcdat,const RTPPacket &rtppack)
{
    // You can inspect the packet and the source's info here
    //std::cout<<"Packet type: "<<rtppack.GetPayloadType()<<std::endl;
    //std::cout<<"Packet says: "<<(char *)rtppack.GetPayloadData()<<std::endl;
    //test RTCP packet
    /*int status = this->SendRTCPAPPPacket(0,(uint8_t*)&("123"),(void*)&("hel"),4);
    checkerror(status);*/

    if(rtppack.GetPayloadType() == H264)
    {
            //std::cout<<"Got H264 packet：êo " << rtppack.GetExtendedSequenceNumber() << " from SSRC " << srcdat.GetSSRC() <<std::endl;
            if(rtppack.HasMarker())//如果是最后一包则进行组包
            {
                    m_pVideoData->m_lLength = m_current_size + rtppack.GetPayloadLength();//得到数据包总的长度
                    memcpy(m_pVideoData->m_pBuffer,m_buffer,m_current_size);
                    memcpy(m_pVideoData->m_pBuffer + m_current_size ,rtppack.GetPayloadData(),rtppack.GetPayloadLength());

                    m_ReceiveArray.Add(m_pVideoData);//添加到接收队列

                    memset(m_buffer,0,m_current_size);//清空缓存，为下次做准备
                    m_current_size = 0;
            }
            else//放入缓冲区，在此必须确保有序
            {
                    unsigned char* p = rtppack.GetPayloadData();


                    memcpy(m_buffer + m_current_size,rtppack.GetPayloadData(),rtppack.GetPayloadLength());
                    m_current_size += rtppack.GetPayloadLength();
            }
    }

}
