//
// Created by Andreas W. on 05.01.26.
//

#ifndef UNTITLED_LEDSHOW_H
#define UNTITLED_LEDSHOW_H
#include "ShowController.h"

namespace Task {
    class LedShow {
        ShowController &controller;
        TaskHandle_t taskHandle = nullptr;


        static void taskWrapper(void *pvParameters);
        void task();
    public:
        LedShow(ShowController& controller);

        void startTask();
    };
}


#endif //UNTITLED_LEDSHOW_H
