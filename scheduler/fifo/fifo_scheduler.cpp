#include <glog/logging.h>

#include "fifo_lord.h"

int main(int argc, char* argv[]) {
    // Initialize Google’s logging library.
    google::InitGoogleLogging(argv[0]);

    FifoLord *lord = new FifoLord(4);

    while (true) {
        lord->consume_message();
        lord->schedule();
    }

    return 0;
}