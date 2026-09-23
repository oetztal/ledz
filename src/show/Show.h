#ifndef LEDZ_SHOW_H
#define LEDZ_SHOW_H

#include <memory>
#include "strip/Strip.h"

namespace Show {
    using Iteration = uint64_t;

    class Show {
    public:
        virtual ~Show() = default;

        virtual void execute(Strip::Strip& strip, Iteration iteration) = 0;

        [[nodiscard]] virtual bool isComplete() const;
    };

}
#endif // LEDZ_SHOW_H
