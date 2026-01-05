//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_RAINBOW_H
#define UNTITLED_RAINBOW_H
#include "Show.h"
#include "strip/Strip.h"

namespace Show {
    class Rainbow : public Show {
    public:
        Rainbow();

        void execute(Strip::Strip &strip, Iteration iteration) override;
    };
}


#endif //UNTITLED_RAINBOW_H