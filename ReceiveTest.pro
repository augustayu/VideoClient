LIBS += -L./lib -ljrtp -ljthread -lpthread

HEADERS += \
    post.h \
    performance.h \
    MfcDrvParams.h \
    MfcDriver.h \
    mfc.h \
    LogMsg.h \
    lcd.h \
    H264Frames.h \
    FrameExtractor.h \
    FileRead.h \
    SsbSipH264Decode.h \
    s3c_pp.h

SOURCES += \
    main.cpp \
    SsbSipH264Decode.c \
    performance.c \
    LogMsg.c \
    H264Frames.c \
    FrameExtractor.c \
    FileRead.c
