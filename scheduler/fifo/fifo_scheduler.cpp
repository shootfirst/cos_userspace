#include <glog/logging.h>
#include <cassert>
#include <csignal>

#include "fifo_lord.h"

void init_glog() {
    // Initialize Google’s logging library.
    google::InitGoogleLogging("fifo scheduler");
    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;
}

bool should_exit = false;

void RegisterSignalHandlers() {
  std::signal(SIGINT, [](int signum) {
    static bool force_exit = false;

    assert(signum == SIGINT);
    if (force_exit) {
      printf("Forcing exit...\n");
      std::exit(1);
    } else {
      if (!should_exit) {
        should_exit = true;
      }
      force_exit = true;
    }
  });
  std::signal(SIGALRM, [](int signum) {
    assert(signum == SIGALRM);
    printf("Timer fired...\n");
    if (!should_exit) {
      should_exit = true;
    }
  });
}

int main(int argc, char* argv[]) {
    init_glog();
    RegisterSignalHandlers();
    
    FifoLord *lord = new FifoLord(4);

    LOG(INFO) << "init cos success!";
    while (!should_exit) {
      // struct timespec ts;
      // ts.tv_sec = 0;
      // ts.tv_nsec = 1000;  // sleep for 1ms
      // nanosleep(&ts, NULL);
        lord->consume_message();
        lord->schedule();
    }

    return 0;
}