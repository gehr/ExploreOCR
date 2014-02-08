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
#ifndef __HELLOOCR_HPP__
#define __HELLOOCR_HPP__

#include <QtCore/QObject>
#include <QtCore/QMetaType>

#include <bb/cascades/ForeignWindowControl>
#include <bb/cascades/Button>
#include <bb/cascades/TextArea>
#include <bb/cascades/Application>

#include <camera/camera_api.h>
#include <baseapi.h>

using namespace bb::cascades;
using namespace tesseract;

class HelloOCR : public QObject
{
    Q_OBJECT
public slots:
    void onWindowAttached(screen_window_t win,
                          const QString &group,
                          const QString &id);
    void onStartFront();
    void onStartRear();
    void onStopCamera();
    void onAction();
    void onFrameToOCR(unsigned char* buf, int w, int h);

signals:
    void frameToOCR(unsigned char* buf, int w, int h);

public:
    HelloOCR(bb::cascades::Application *app);
    ~HelloOCR();

private:
    static void vfCallBackEntry(camera_handle_t handle,
                                camera_buffer_t* buf,
                                void* arg);
    void processFrame(camera_buffer_t* buf);
    int createViewfinder(camera_unit_t cameraUnit,
                         const QString &group,
                         const QString &id);

    ForeignWindowControl *mViewfinderWindow;
    Button *mStartFrontButton;
    Button *mStartRearButton;
    Button *mStopButton;
    Button *mActionButton;
    TextArea *mText;
    camera_handle_t mCameraHandle;
    camera_unit_t mCameraUnit;
    TessBaseAPI mTess;
    bool mCapFrame;
};

#endif // ifndef __HELLOOCR_HPP__
