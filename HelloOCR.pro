APP_NAME = HelloOCR

CONFIG += qt warn_on cascades10

INCLUDEPATH += ../src /Users/smcveigh/tesseract/dependencies/include/tesseract
INCLUDEPATH += /Users/smcveigh/tesseract/dependencies/include/leptonic
INCLUDEPATH += /Users/smcveigh/tesseract/tesseract-ocr/ccutil
INCLUDEPATH += /Users/smcveigh/tesseract/tesseract-ocr/ccmain
DEPENDPATH += ../src /Users/smcveigh/tesseract/dependencies/include
LIBS += -L/Users/smcveigh/tesseract/dependencies/lib
PRE_TARGETDEPS += /Users/smcveigh/tesseract/dependencies/lib/liblept.so
PRE_TARGETDEPS += /Users/smcveigh/tesseract/dependencies/lib/libtesseract.so
LIBS += -llept -ltesseract -lcamapi -lscreen

include(config.pri)
