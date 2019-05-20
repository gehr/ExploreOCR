/* Copyright (c) 2012 Research In Motion Limited.
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include <bb/cascades/Application>
#include <bb/cascades/ForeignWindowControl>
#include <bb/cascades/Container>
#include <bb/cascades/StackLayout>
#include <bb/cascades/DockLayout>
#include <bb/cascades/LayoutProperties>
#include <bb/cascades/WindowProperty>
#include <bb/cascades/Button>
#include <bb/cascades/Page>
#include <bb/cascades/TextStyle>
#include <bb/cascades/FontSize>
#include <bb/cascades/SystemDefaults>

/* malignant change */
/* benign change this is my addition */
/* another comment */
#include <camera/camera_api.h>
#include <screen/screen.h>
#include <bps/soundplayer.h>
#include <unistd.h>
#include <libgen.h>
#include <fcntl.h>
#include "HelloOCR.hpp"

#include <baseapi.h>
#include <stdlib.h>

#define TESSDATA "/accounts/1000/shared/misc/"

using namespace bb::cascades;

// qDebug() now logs to slogger2, which I find inconvenient since the NDK does not pick this up in the console,
// so I am installing a custom handler to log to stderr.
static void log_to_stderr(QtMsgType msgType, const char *msg)
{
    (void)msgType;  // go away, warning!
    fprintf(stderr, "%s\n", msg);
}


HelloOCR::HelloOCR(bb::cascades::Application *app) :
        QObject(app),
        mCameraHandle(CAMERA_HANDLE_INVALID)
{
    qInstallMsgHandler(log_to_stderr);

    qDebug() << "HelloOCR";
    setenv("TESSDATA_PREFIX", TESSDATA, 1);
    int rc = mTess.Init(TESSDATA,
                        NULL,
                        OEM_TESSERACT_ONLY);
    if (rc) {
        qDebug() << "tesseract init failed:" << rc;
        exit(-1);
    }
    //mTess.SetVariable("tessedit_char_whitelist", "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

    // create our foreign window
    mViewfinderWindow = ForeignWindowControl::create()
        .windowId(QString("cameraViewfinder"));
    // Allow Cascades to update the native window's size, position, and visibility, but not the source-size.
    // Cascades may otherwise attempt to redefine the buffer source-size to match the window size, which would yield
    // undesirable results.  You can experiment with this if you want to see what I mean.
    mViewfinderWindow->setUpdatedProperties(WindowProperty::Position | WindowProperty::Size | WindowProperty::Visible);

    QObject::connect(mViewfinderWindow,
                     SIGNAL(windowAttached(screen_window_t, const QString &, const QString &)),
                     this, SLOT(onWindowAttached(screen_window_t, const QString &,const QString &)));
    QObject::connect(this, SIGNAL(frameToOCR(unsigned char*, int, int)),
                     this, SLOT(onFrameToOCR(unsigned char*, int, int)));

    // create a bunch of camera control buttons
    // NOTE: some of these buttons are not initially visible
    mStartFrontButton = Button::create("Front Camera")
        .onClicked(this, SLOT(onStartFront()));
    mStartRearButton = Button::create("Rear Camera")
        .onClicked(this, SLOT(onStartRear()));
    mStopButton = Button::create("Stop Camera")
        .onClicked(this, SLOT(onStopCamera()));
    mStopButton->setVisible(false);
    mActionButton = Button::create("Scan")
        .onClicked(this, SLOT(onAction()));
    mActionButton->setVisible(false);

    TextStyle *style = new TextStyle(SystemDefaults::TextStyles::bigText());
    style->setColor(Color::Green);
    style->setFontWeight(FontWeight::Bold);

    mText = TextArea::create()
        .backgroundVisible(false)
        .editable(false)
        .textStyle(*style);
    mText->setVisible(false);

    // using dock layout mainly.  the viewfinder foreign window sits in the center,
    // and the buttons live in their own container at the bottom.
    // a single text label sits at the top of the screen to report recording status.
    Container* container = Container::create()
        .layout(DockLayout::create())
        .add(Container::create()
            .horizontal(HorizontalAlignment::Center)
            .vertical(VerticalAlignment::Center)
            .add(mViewfinderWindow))
        .add(Container::create()
            .horizontal(HorizontalAlignment::Left)
            .vertical(VerticalAlignment::Top)
            .add(mText))
        .add(Container::create()
            .horizontal(HorizontalAlignment::Center)
            .vertical(VerticalAlignment::Bottom)
            .layout(StackLayout::create()
    			.orientation(LayoutOrientation::LeftToRight))
			.add(mStartFrontButton)
			.add(mStartRearButton)
			.add(mActionButton)
			.add(mStopButton));


    app->setScene(Page::create().content(container));
}


HelloOCR::~HelloOCR()
{
}


void HelloOCR::onWindowAttached(screen_window_t win,
                                          const QString &group,
                                          const QString &id)
{
    qDebug() << "onWindowAttached: " << win << ", " << group << ", " << id;
    // set screen properties to mirror if this is the front-facing camera
    int i = (mCameraUnit == CAMERA_UNIT_FRONT);
    screen_set_window_property_iv(win, SCREEN_PROPERTY_MIRROR, &i);
    // put the viewfinder window behind the cascades window
    i = -1;
    screen_set_window_property_iv(win, SCREEN_PROPERTY_ZORDER, &i);
    screen_context_t screen_ctx;
    screen_get_window_property_pv(win, SCREEN_PROPERTY_CONTEXT, (void **)&screen_ctx);
    screen_flush_context(screen_ctx, 0);
}


int HelloOCR::createViewfinder(camera_unit_t cameraUnit,
                                     const QString &group,
                                     const QString &id)
{
    qDebug() << "createViewfinder";
    mCapFrame = false;
    if (mCameraHandle != CAMERA_HANDLE_INVALID) {
        qDebug() << "camera already running";
        return EBUSY;
    }
    mCameraUnit = cameraUnit;
    if (camera_open(mCameraUnit,
                    CAMERA_MODE_RW | CAMERA_MODE_ROLL,
                    &mCameraHandle) != CAMERA_EOK) {
        qDebug() << "could not open camera";
        return EIO;
    }
    qDebug() << "camera opened";
    if (camera_set_videovf_property(mCameraHandle,
                                    CAMERA_IMGPROP_WIN_GROUPID, group.toStdString().c_str(),
                                    CAMERA_IMGPROP_WIN_ID, id.toStdString().c_str()) == CAMERA_EOK) {
        qDebug() << "viewfinder configured";
        if (camera_start_video_viewfinder(mCameraHandle,
                                          &vfCallBackEntry,
                                          NULL,
                                          (void*)this) == CAMERA_EOK) {
            qDebug() << "viewfinder started";
            // toggle button visibility...
            mStartFrontButton->setVisible(false);
            mStartRearButton->setVisible(false);
            mStopButton->setVisible(true);
            mActionButton->setText("Scan");
            mActionButton->setVisible(true);
            mActionButton->setEnabled(true);
            //camera_config_videolight(mCameraHandle, CAMERA_VIDEOLIGHT_ON);
            return EOK;
        }
    }
    qDebug() << "couldn't start viewfinder";
    camera_close(mCameraHandle);
    mCameraHandle = CAMERA_HANDLE_INVALID;
    return EIO;
}


void HelloOCR::onStartFront()
{
    qDebug() << "onStartFront";
    if (mViewfinderWindow) {
        // create a window and see if we can catch the join
        if (createViewfinder(CAMERA_UNIT_FRONT,
                             mViewfinderWindow->windowGroup().toStdString().c_str(),
                             mViewfinderWindow->windowId().toStdString().c_str()) == EOK) {
            qDebug() << "created viewfinder";
        }
    }
}


void HelloOCR::onStartRear()
{
    qDebug() << "onStartRear";
    if (mViewfinderWindow) {
        // create a window and see if we can catch the join
        if (createViewfinder(CAMERA_UNIT_REAR,
                             mViewfinderWindow->windowGroup().toStdString().c_str(),
                             mViewfinderWindow->windowId().toStdString().c_str()) == EOK) {
            qDebug() << "created viewfinder";
        }
    }
}


void HelloOCR::onStopCamera()
{
    qDebug() << "onStopCamera";
    mText->setVisible(false);
    if (mCameraHandle != CAMERA_HANDLE_INVALID) {
        // closing the camera handle causes the viewfinder to stop which will in turn
        // cause it to detach from the foreign window
        camera_close(mCameraHandle);
        mCameraHandle = CAMERA_HANDLE_INVALID;
        // reset button visibility
        mActionButton->setVisible(false);
        mStopButton->setVisible(false);
        mStartFrontButton->setVisible(true);
        mStartRearButton->setVisible(true);
    }
}


void HelloOCR::vfCallBackEntry(camera_handle_t handle,
                               camera_buffer_t* buf,
                               void* arg)
{
    HelloOCR* inst = (HelloOCR*)arg;
    if (inst->mCapFrame) {
        inst->processFrame(buf);
        inst->mCapFrame = false;
    }
}


static int save_pgm_buffer(const char *fn, unsigned char* data, int width, int height, int stride)
{
    int rc=0;
    char header[256];

    int lenheader = snprintf(header, sizeof header, "P5\n%d %d\n255\n", width, height);

    int fd = open(fn, O_RDWR|O_CREAT|O_TRUNC, 0660);

    if (fd == -1) {
        perror("save_vf_buffer()/open():");
        return -1;
    }

    write(fd, header, lenheader);

    for (int i=0; i<height; i++) {
        rc = write(fd, data, width);
        if (rc != width) {
            printf("%s(): write(%d) returns %d\n", __func__, width, rc);
            break;
        }
        data += stride;
    }

    close(fd);

    return 0;
}



void HelloOCR::processFrame(camera_buffer_t* buf)
{
    if (buf->frametype != CAMERA_FRAMETYPE_NV12) {
        qDebug() << "unexpected frametype";
    } else {
        int width = buf->framedesc.nv12.width;
        int height = buf->framedesc.nv12.height;
        int stride = buf->framedesc.nv12.stride;
        qDebug() << "processing buffer:" << width << "x" << height;
        // let's memcpy the buffer to heap since image buffer may not be cached.
        int w = width;
        int h = height;
        unsigned char* pbuf = new unsigned char[w*h];
        unsigned char* dst = pbuf;
        for (int i=0; i<h; i++) {
            unsigned char* src = &(buf->framebuf[i*stride]);
            for (int j=0; j<w; j++, src++, dst++) {
                *dst = *src;
#if 0
                *dst = (*src < 100);
#endif
            }
        }
        // ping the OCR signal
        emit frameToOCR(pbuf, w, h);
    }
}


void HelloOCR::onFrameToOCR(unsigned char* buf, int w, int h)
{
    mText->setVisible(false);
    save_pgm_buffer("/accounts/1000/shared/misc/vf.ppm", buf, w, h, w);
    mTess.SetImage(buf, w, h, 1, w);
    mTess.SetRectangle(0, h/4, w, h/2);
    char *text = mTess.GetUTF8Text();
    if (!text) {
        qDebug() << "no data returned";
    } else {
        qDebug() << "got text: " << text;
        mText->setText(text);
        mText->setVisible(true);
        delete[] text;
    }
    delete[] buf;
    mActionButton->setEnabled(true);
}



void HelloOCR::onAction()
{
    qDebug() << "onAction";
    mActionButton->setEnabled(false);
    if (mCameraHandle != CAMERA_HANDLE_INVALID) {
        mCapFrame = true;
    }
}
