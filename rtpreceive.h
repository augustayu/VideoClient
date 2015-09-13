#ifndef RTPRECEIVE_H
#define RTPRECEIVE_H
#include "rtpsession.h"
#include "rtpudpv4transmitter.h"
#include "rtpipv4address.h"
#include "rtpsessionparams.h"
#include "rtperrors.h"
#include "rtppacket.h"

class RTPreceive
{
public:
    RTPreceive();
    void CreateSession();
    void OnPollThreadStep();
    void ProcessRTPPacket(const RTPSourceData &srcdat,const RTPPacket &rtppack);
    void OnRTCPCompoundPacket(RTCPCompoundPacket *pack,const RTPTime &receivetime,const RTPAddress *senderaddress);
    void checkerror(int rtperr);
private:
    RTPSession                   sess;
    RTPUDPv4TransmissionParams   transparams;
    RTPSessionParams             sessparams;
    uint                         portbase,destport;
    uint                         destip;
    std::string                  ipstr;
    int                          status,i,num;
    int                          timeStampInc;
    long long                    currentTime;

};

#endif // RTPRECEIVE_H
