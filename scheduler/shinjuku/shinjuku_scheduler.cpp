#include <glog/logging.h>

#include "shinjuku_lord.h"

void init_glog() {
    // Initialize Google’s logging library.
    google::InitGoogleLogging("shinjuku scheduler");
    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;
}

int main(int argc, char* argv[]) {
    init_glog();
    
    ShinjukuLord *lord = new ShinjukuLord(4);

    LOG(INFO) << "init cos success!";
    while (true) {
        lord->consume_message();
        lord->schedule();
    }

    return 0;
}