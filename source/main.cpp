/*! \file main.cpp */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "nixie_clock.h"
#include "face_detection.h"
#include <iostream>

bool gKeepRunning = true;

void *nixieclockthread(void *data)
{
    time_t ta, tb, *tc;
    cNixieClock *clock = (cNixieClock *)data;

    time(&ta);

    do
    {
        time(&tb);
        tc = NULL;
        if(tb != ta)
        {
            tc = &tb;
            ta = tb;
        }
        usleep(clock->Task(tc));
    }
    while(gKeepRunning);

    return NULL;
}

void vu_test(__u32 u32t)
{
    gpio_init();
    cNixieNumbers *numbers = new cNixieNumbers(PIN_N_CLOCK, PIN_N_DATA, PIN_N_BCLOCK, PIN_N_RESET);
    cNixiePwm *vu = new cNixiePwm(PIN_PWM_T);
    cNixiePwm *bright = new cNixiePwm(PIN_PWM_LAMPS);
    bright->SetPercent(0);

    char text[32];
    for(;;)
    {
        int i;
        for(i=0;i<=100;i++)
        {
            sprintf(text,"   %03d", i);
            numbers->SetValue(text);
            vu->SetPercent(i);
            usleep(u32t*1000);
        }
        for(i=100;i>=0;i--)
        {
            sprintf(text,"   %03d", i);
            numbers->SetValue(text);
            vu->SetPercent(i);
            usleep(u32t*1000);
        }
    }
}

void rgb_test()
{
    gpio_init();
    cNixieNumbers *numbers = new cNixieNumbers(PIN_N_CLOCK, PIN_N_DATA, PIN_N_BCLOCK, PIN_N_RESET);
    cNixiePwm *bright = new cNixiePwm(PIN_PWM_LAMPS);
    bright->SetPercent(0);

    cNeoRpiRgbh *rgb = new cNeoRpiRgbh(PIN_RGB_R, PIN_RGB_G, PIN_RGB_B);
    rgb->StartThread();

    for(;;)
    {
        rgb->SetModulator(enuModulationTypeNone, NULL);
        rgb->SetRgb(0,0,0);
        numbers->SetValue("   000");
        sleep(5);

        rgb->SetRgb(255,255,255);
        numbers->SetValue("   255");
        sleep(5);

        rgb->SetRgb(128,128,128);
        numbers->SetValue("   128");
        sleep(5);

        rgb->SetRgb(255,0,0);
        numbers->SetValue("255   ");
        rgb->SetModulator(enuModulationTypeSinusoidal, NULL);
        rgb->GetModulatorPtr()->SetPeriod(3000);
        sleep(15);

        rgb->SetRgb(0,255,0);
        numbers->SetValue("  255 ");
        sleep(15);

        rgb->SetRgb(0,0,255);
        numbers->SetValue("   255");
        sleep(15);
    }



}




#ifdef USE_OPENCV
int main(int argc, char *argv[])
{
    if(argc > 1 && strcmp(argv[1], "-t")== 0)
    {
        vu_test(atoi(argv[2]));
    }
    if(argc > 1 && strcmp(argv[1], "-rgb")== 0)
    {
        rgb_test();
    }



    pthread_t   mThreadClock;

    tinyxml2::XMLDocument   mXmlDoc;
    tinyxml2::XMLElement    *mXmlElement;

    if(mXmlDoc.LoadFile("/home/pi/nixie/nixie.xml") != tinyxml2::XML_SUCCESS)
    {
        return 0;
    }

    gpio_init();

    mXmlElement = mXmlDoc.FirstChildElement("nixie");

    cNixieClock *Nixie = new cNixieClock();

    if(Nixie->Init(mXmlElement))
    {
        if(pthread_create(&mThreadClock, NULL, nixieclockthread, Nixie) != 0)
        {
            mThreadClock = 0;
        }
    }

    bool has_facedetection = start_face_detection(mXmlElement->FirstChildElement("face_detection"));

    if(has_facedetection)
    {
        Nixie->SetFaceDetected(40000);
    }

    do
    {
        __u32 u32facetimeout = has_facedetection?face_detected():0;
        if(u32facetimeout)
        {
            Nixie->SetFaceDetected(u32facetimeout);
        }
        usleep(10000);

    }while(gKeepRunning);

    if(mThreadClock)
        pthread_join(mThreadClock, NULL);

    end_face_detection();

    delete Nixie;

    gpio_end();
    return 0;
}
#else
int main(int argc, char *argv[])
{
    gpio_init();

    cNixieClock *Nixie = new cNixieClock();
    Nixie->Init();

    pthread_t mThreadClock;

//thread nixie
    if(pthread_create(&mThreadClock, NULL, nixieclockthread, Nixie) != 0)
    {
        mThreadClock = 0;
    }

    while(mThreadClock && gKeepRunning)
    {
        sleep(2);
    }

    if(mThreadClock)
        pthread_join(mThreadClock, NULL);


    delete Nixie;
    gpio_end();
    return 0;
}
#endif
