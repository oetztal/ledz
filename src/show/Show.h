//
// Created by Andreas W. on 02.01.26.
//

#ifndef UNTITLED_SHOW_H
#define UNTITLED_SHOW_H

#include <memory>
#include "strip/Strip.h"

namespace Show {
    typedef uint64_t Iteration;

    class Show {
    public:
        virtual ~Show() = default;

        virtual void execute(Strip::Strip &strip, Iteration iteration) = 0;
    };
}
#endif //UNTITLED_SHOW_H