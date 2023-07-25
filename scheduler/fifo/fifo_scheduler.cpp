#include <glog/logging.h>

#include "fifo_lord.h"

void init_glog() {
    // Initialize Google’s logging library.
    google::InitGoogleLogging("fifo scheduler");
    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;
}

int main(int argc, char* argv[]) {
    init_glog();
    
    FifoLord *lord = new FifoLord(4);

    LOG(INFO) << "init cos success!";
    while (true) {
        lord->consume_message();
        lord->schedule();
    }

    return 0;
}