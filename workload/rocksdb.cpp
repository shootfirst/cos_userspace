/* Copyright (c) 2022 Meta Platforms, Inc. and affiliates. */
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <rocksdb/db.h>
#include <rocksdb/table.h>
#include <glog/logging.h>

#include <chrono>  // NOLINT
#include <variant>
#include <csignal>
#include <filesystem>
#include <vector>
#include <sstream>
#include <cstring>
#include <map>
#include <random>

#include "thread.h"  // NOLINT
#include "epistle.h"  // NOLINT


std::string epistle_path = "./epistle";
Epistle<u_int32_t> *epistle = nullptr;

struct Request {
  struct Get {
    uint32_t entry;
  };

  struct Range {
    // The accessed range is [start_entry, start_entry + size).

    // The first entry in the range.
    uint32_t start_entry;
    // The range size.
    uint32_t size;
  };

  // Returns true if this is a Get request. Returns false otherwise (i.e., this
  // is a Range query).
  bool IsGet() const { return work.index() == 0; }

  // Returns true if this is a Range query. Returns false otherwise (i.e., this
  // is a Get request).
  bool IsRange() const { return work.index() == 1; }

  // Unique request identifier.
  uint64_t id;

  // When the request was generated.
  uint64_t request_generated;
  // When the request was picked up by the app.
  uint64_t request_received;
  // When the request was assigned to a worker.
  uint64_t request_assigned;
  // When the request started to be handled by a worker.
  uint64_t request_start;
  // When the worker finished handling the request.
  uint64_t request_finished;

  // The work to do. The request is either a Get request or a Range query.
  std::variant<Get, Range> work;
  // TODO(xiunianjun): different with ghOSt here:
  // We don't receive the response. I think it's unneccesary so I choose to
  // ignore it. string response;
};

/*=======================DATABASE=======================*/

class Database {
 public:
  explicit Database(const std::filesystem::path& path);
  ~Database();

  bool Get(uint32_t entry) const;
  bool RangeQuery(uint32_t start_entry, uint32_t range_size) const;

  // The number of entries in the database.
  static constexpr uint32_t kNumEntries = 1'000'000;

 private:
  // Opens the RocksDB database at 'path' (if it exists) or creates a new
  // RocksDB database at 'path' (if no database exists there yet).
  bool OpenDatabase(const std::filesystem::path& path);

  bool Fill();

  void PrepopulateCache() const;

  static std::string to_string(uint32_t entry) {
    std::string s = std::to_string(entry);
    assert(s.size() <= kNumLength);
    return std::string(kNumLength - s.size(), '0') + s;
  }

  // Returns the key string for 'entry'.
  static std::string Key(uint32_t entry) { return "key" + to_string(entry); }

  // Returns the value string for 'entry'.
  static std::string Value(uint32_t entry) {
    return "value" + to_string(entry);
  }

  rocksdb::DB* db_;
  static constexpr uint32_t kNumLength = 16;
  static constexpr size_t kCacheSize = 1 * 1024 * 1024 * 1024LL;
};

bool Database::OpenDatabase(const std::filesystem::path& path) {
  rocksdb::Options options;
  options.create_if_missing = true;
  options.allow_mmap_reads = true;
  options.allow_mmap_writes = true;
  options.error_if_exists = false;

  rocksdb::BlockBasedTableOptions table_options;
  table_options.block_cache = rocksdb::NewClockCache(kCacheSize, 0);
  assert(table_options.block_cache != nullptr);
  options.table_factory.reset(
      rocksdb::NewBlockBasedTableFactory(table_options));
  options.compression = rocksdb::kNoCompression;
  options.OptimizeLevelStyleCompaction();
  rocksdb::Status status = rocksdb::DB::Open(options, path.string(), &db_);
  auto ok = status.ok();
  return ok;
}

Database::Database(const std::filesystem::path& path) {
  LOG(INFO) << "init database...";
  if (!OpenDatabase(path)) {
    assert(std::filesystem::exists(path));
    assert(std::filesystem::remove_all(path) >= 0);
    assert(OpenDatabase(path));
  }
  LOG(INFO) << "open database success";
  assert(Fill());
  LOG(INFO) << "fill database success";
  PrepopulateCache();
  LOG(INFO) << "warming cache success.";
}

Database::~Database() { delete db_; }

bool Database::Fill() {
  for (uint32_t i = 0; i < kNumEntries; i++) {
    rocksdb::Status status =
        db_->Put(rocksdb::WriteOptions(), Key(i), Value(i));
    if (!status.ok()) {
      return false;
    }
  }
  return true;
}

void Database::PrepopulateCache() const {
  for (uint32_t i = 0; i < kNumEntries; i++) {
    assert(Get(i));
  }
}

bool Database::Get(uint32_t entry) const {
  std::string value;
  rocksdb::Status status = db_->Get(rocksdb::ReadOptions(), Key(entry), &value);
  if (status.ok()) {
    assert(value == Value(entry));
    return true;
  }
  return false;
}

bool Database::RangeQuery(uint32_t start_entry, uint32_t range_size) const {
  std::string value;
  std::stringstream ss;
  std::unique_ptr<rocksdb::Iterator> it(
      db_->NewIterator(rocksdb::ReadOptions()));
  it->Seek(Key(start_entry));

  for (uint32_t i = 0; i < range_size; i++) {
    if (!it->Valid()) {
      return false;
    }
    assert(it->value().ToString() == Value(start_entry + i));
    ss << it->value().ToString();
    if (i < range_size - 1) {
      ss << ",";
    }
    it->Next();
  }
  value = ss.str();
  return true;
}

Database database("/etc/cos/db");
/*=======================DATABASE=======================*/

struct WorkerWork {
  std::atomic<size_t> num_requests;
  // The requests.
  std::vector<Request> requests;
};

struct option {
  int batch = 12;
  int num_workers = 20;
  uint64_t get_duration = 1000 * 10;      // 0.1ms
  uint64_t range_duration = 1000 * 100;  // 100 microseconds
  double throughput = 10000.0;
  double range_query_ratio = 0.0;
  int experiment_duration = 5;  // 30s
  int discard_duration = 1;      // 1s
  bool print_range = false;
  bool print_get = false;
  bool ns = true;
  bool print_last = false;
} options;

std::vector<std::unique_ptr<CosThread>> workers;
std::map<uint32_t, std::unique_ptr<WorkerWork>> worker_works;
std::map<uint32_t, std::vector<Request>> requests;

std::chrono::high_resolution_clock::time_point start_time;
uint64_t request_id = 0;

/*=======================DATABASE=======================*/

void spin_for(uint64_t duration) {
  while (duration > 0) {
    std::chrono::high_resolution_clock::time_point a =
        std::chrono::high_resolution_clock::now();
    std::chrono::high_resolution_clock::time_point b;

    // Try to minimize the contribution of arithmetic/Now() overhead.
    for (int i = 0; i < 150; i++) {
      b = std::chrono::high_resolution_clock::now();
    }

    uint64_t t =
        std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count();

    // TODO(xiunianjun): could maybe optimised here with our shared memory with
    // agent. Don't count preempted time
    if (t < 100000) {
      if (duration > t)
        duration -= t;
      else
        duration = 0;
    }
  }
}

std::chrono::time_point<std::chrono::high_resolution_clock> GetThreadCpuTime() {
  struct timespec ts;
  assert(clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) == 0);
  return std::chrono::time_point<std::chrono::_V2::system_clock,
                                 std::chrono::nanoseconds>(
      std::chrono::seconds(ts.tv_sec) + std::chrono::nanoseconds(ts.tv_nsec));
}

void HandleGet(Request* request) {
  assert(request->IsGet());

  auto start_duration = GetThreadCpuTime();
  // TODO(xiunianjun): different with ghOSt here:
  // We use fixed service time for handling requests every time. But in ghOSt,
  // if options.get_exponential_mean is ON, the service time should add a random
  // exponential number, whose gen is different in each thread. (See gen_ vector
  // in class Orchestrator)
  auto service_time = options.get_duration;

  Request::Get& get = std::get<Request::Get>(request->work);
  database.Get(get.entry);

  auto now_duration = GetThreadCpuTime();
  uint64_t dura = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      now_duration - start_duration)
                      .count();
  if (dura < service_time) {
    spin_for(service_time - dura);
  }
}

void HandleRange(Request* request) {
  assert(request->IsRange());

  auto start_duration = GetThreadCpuTime();
  auto service_time = options.range_duration;
  Request::Range& range = std::get<Request::Range>(request->work);
  database.RangeQuery(range.start_entry, range.size);

  auto now_duration = GetThreadCpuTime();
  uint64_t dura = std::chrono::duration_cast<std::chrono::nanoseconds>(
                      now_duration - start_duration)
                      .count();
  if (dura < service_time) {
    spin_for(service_time - dura);
  }
}

void HandleRequest(Request* request) {
  if (request->IsGet()) {
    HandleGet(request);
  } else {
    assert(request->IsRange());
    HandleRange(request);
  }
}

/*=======================DATABASE=======================*/

/*=======================NETWORK=======================*/
class Ingress {
 public:
  explicit Ingress(double throughput) : throughput_(throughput), gen_() {}

  // Starts the ingress queue.
  void Start() {
    start_ = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                 std::chrono::high_resolution_clock::now())
                 .time_since_epoch()
                 .count();
  }

  // Models a Poisson arrival process with a lambda of `throughput_`. Returns a
  // pair with 'true' and the arrival time when at least one request is waiting
  // in the ingress queue. Returns a pair with 'false' and an undefined arrival
  // time when no request is waiting in the ingress queue.
  std::pair<bool, uint64_t> HasNewArrival() {
    assert(start_ > 0);
    if ((uint64_t)(std::chrono::time_point_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now())
            .time_since_epoch()
            .count()) >= start_) {
      uint64_t arrival = start_;
      uint64_t next = NextDuration();
      start_ += next;
      return std::make_pair(true, arrival);
    }
    return std::make_pair(false, 0);
  }

 private:
  uint64_t NextDuration() {
    std::exponential_distribution<double> exponential(throughput_ / 1000.0);
    double duration_msec = exponential(gen_);
    return (uint64_t)(duration_msec * 1000000);
  }

  const double throughput_ = 1000;
  std::mt19937 gen_;
  uint64_t start_ = 0;
};

class NetWork {
 public:
  NetWork(double throughput, double range_query_ratio)
      : gen_(), ingress_(throughput), range_query_ratio_(range_query_ratio) {}

  void Start() {
    ingress_.Start();
    is_start_ = true;
  }

  bool PollRequest(Request* request) {
    assert(is_start_);

    const auto [arrived, arrival_time] = ingress_.HasNewArrival();
    if (!arrived) {
      return false;
    }
    // A request is in the ingress queue
    uint64_t received = std::chrono::time_point_cast<std::chrono::nanoseconds>(
                            std::chrono::high_resolution_clock::now())
                            .time_since_epoch()
                            .count();

    std::bernoulli_distribution bernoulli(1.0 - range_query_ratio_);
    bool get = bernoulli(gen_);
    if (get) {
      // Get request
      std::uniform_int_distribution<int> uniform(0, Database::kNumEntries);
      request->work = Request::Get{.entry = (uint32_t)uniform(gen_)};
    } else {
      // Range query
      std::uniform_int_distribution<int> uniform(
          0, Database::kNumEntries - kRangeQuerySize + 1);
      request->work =
          Request::Range{.start_entry = (uint32_t)uniform(gen_), .size = kRangeQuerySize};
    }

    request->request_generated = arrival_time;
    request->request_received = received;
    return true;
  }

  // The size of range queries.
  static constexpr uint32_t kRangeQuerySize = 5000;

 private:
  std::mt19937 gen_;
  Ingress ingress_;
  const double range_query_ratio_;

  bool is_start_ = false;
};

NetWork network(options.throughput, options.range_query_ratio);
/*=======================NETWORK=======================*/

inline void Pause() {
  // struct timespec ts;
  // ts.tv_sec = 0;
  // ts.tv_nsec = 1000 * 1000; // sleep for 1ms
  // nanosleep(&ts, NULL);
// #ifndef __GNUC__
// #error "GCC is needed for the macros in the `Pause()` function."
// #endif
// #if defined(__x86_64__)
//   asm volatile("pause");
// #elif defined(__aarch64__)
//   asm volatile("yield");
// #elif defined(__powerpc64__)
//   asm volatile("or 27,27,27");
// #else
//   // Do nothing.
// #endif
}

/*=======================EXIT=======================*/
bool exit_generator = false;
bool exit_worker = false;

bool worker_lock[100] = {0};
/*=======================EXIT=======================*/

void Worker(int lock_id) {
  int tid = gettid();
  // notify1
  worker_lock[lock_id] = true;
  smp_mb();
  epistle->add(tid, EPISTLE_THREAD_IDLE);
  LOG(INFO) << "Worker (TID: " << tid << ")";

  while (!exit_worker) {
    // TODO(xiunianjun)
    // Maybe something like "WaitUntilRunnable".
    if (epistle->get(tid) == EPISTLE_THREAD_IDLE) {
      smp_mb();
      Pause();
      continue;
    }

    assert(epistle->get(tid) == EPISTLE_THREAD_RUNNABLE);
    WorkerWork* work = worker_works[tid].get();

    size_t num_requests = work->num_requests.load(std::memory_order_acquire);
    if (num_requests == 0) {
      continue;
    }
    assert(num_requests <= (uint32_t)options.batch);
    assert(num_requests == work->requests.size());

    for (size_t i = 0; i < num_requests; ++i) {
      Request& request = work->requests[i];
      request.request_start =
          std::chrono::time_point_cast<std::chrono::nanoseconds>(
              std::chrono::high_resolution_clock::now())
              .time_since_epoch()
              .count();
      HandleRequest(&request);
      request.request_finished =
          std::chrono::time_point_cast<std::chrono::nanoseconds>(
              std::chrono::high_resolution_clock::now())
              .time_since_epoch()
              .count();
      requests[tid].push_back(request);
      // PrintRequest(request);
    }
    work->num_requests.store(0, std::memory_order_release);

    // mark as idle
    epistle->add(tid, EPISTLE_THREAD_IDLE);
    smp_mb();
  }
  epistle->add(tid, EPISTLE_THREAD_RUNNABLE);
}

bool generator_lock = false;

void LoadGenerator() {
  cpu_set_t cpuSet;
  CPU_ZERO(&cpuSet);
  CPU_SET(1, &cpuSet);
  sched_setaffinity(gettid(), sizeof(cpuSet), &cpuSet);
  setpriority(PRIO_PROCESS, gettid(), -5);

  // wait2
  while (!generator_lock) {
    smp_mb();
    sleep(1);
    // sched_yield();
  }

  LOG(INFO) << "Load generator (TID: " << gettid() << ")";

  // Set the time that the experiment started at (after initialization).
  start_time = std::chrono::high_resolution_clock::now();

  network.Start();

  while (!exit_generator) {
    for (auto it = workers.begin(); it != workers.end(); it++) {
      int tid = (*it)->tid();
      // TODO(xiunianjun): different with ghOSt here:
      // We directly use shared memory to check for the status of the worker
      // thread in one loop.
      if (epistle->get(tid) == EPISTLE_THREAD_RUNNABLE) {
        smp_mb();
        continue;
      }
      assert(worker_works[tid]->num_requests.load(std::memory_order_relaxed) ==
             0);

      worker_works[tid]->requests.clear();

      Request request;
      for (int i = 0; i < options.batch; ++i) {
        if (network.PollRequest(&request)) {
          request.request_assigned =
              std::chrono::time_point_cast<std::chrono::nanoseconds>(
                  std::chrono::high_resolution_clock::now())
                  .time_since_epoch()
                  .count();
          worker_works[tid]->requests.push_back(request);
        } else {
          // No more requests waiting in the ingress queue, so give the
          // requests we have so far to the worker.
          break;
        }
      }

      if (!worker_works[tid]->requests.empty()) {
        // Assign the batch of requests to the next worker
        assert(worker_works[tid]->requests.size() <= (uint32_t)options.batch);
        worker_works[tid]->num_requests.store(
            worker_works[tid]->requests.size(), std::memory_order_release);

        assert(epistle->get(tid) == EPISTLE_THREAD_IDLE);

        // mark as runnable
        // printf("mark %d runnable\n", tid);
        epistle->add(tid, EPISTLE_THREAD_RUNNABLE);
        smp_mb();
      } else {
        // There is no work waiting in the ingress queue.
        break;
      }
    }
  }
}

/*=======================EXIT=======================*/
bool should_exit = false;

void RegisterSignalHandlers() {
  std::signal(SIGINT, [](int signum) {
    static bool force_exit = false;

    assert(signum == SIGINT);
    if (force_exit) {
        LOG(INFO) << "Forcing exit...";
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
    LOG(INFO) << "Timer fired...";
    if (!should_exit) {
      should_exit = true;
    }
  });
}

// Sets a timer signal to fire after 'duration'.
void SetTimer(int duration) {
  assert(duration > 0);
  itimerval itimer = {.it_interval = {.tv_sec = 0, .tv_usec = 0},
                      .it_value = {.tv_sec = duration, .tv_usec = 0}};
  assert(setitimer(ITIMER_REAL, &itimer, nullptr) == 0);
}
/*=======================EXIT=======================*/

/*=======================PRINT=======================*/
template <class T>
struct Results {
  // The total number of requests.
  T total;
  // The throughput;
  T throughput;
  // The min latency.
  T min;
  // The 50th percentile latency.
  T fifty;
  // The 99th percentile latency.
  T ninetynine;
  // The 99.5th percentile latency.
  T ninetyninefive;
  // The 99.9th percentile latency.
  T ninetyninenine;
  // The max latency.
  T max;
};

#define NS2S 1000000000ULL

constexpr size_t kStageLen = 28;
constexpr size_t kTotalRequestsLen = 18;
constexpr size_t kThroughputLen = 22;
constexpr size_t kResultLen = 12;

// Prints the results in human-readable form.
template <class T>
static void PrintLinePretty(std::ostream& os, const std::string& stage,
                            const Results<T>& results,
                            double input_throughput) {
  os << std::left;
  os << std::setw(kStageLen) << stage << " ";
  os << std::setw(kTotalRequestsLen) << results.total << " ";
  os << std::setw(kThroughputLen) << results.throughput << " ";
  os << std::setw(kResultLen) << results.min << " ";
  os << std::setw(kResultLen) << results.fifty << " ";
  os << std::setw(kResultLen) << results.ninetynine << " ";
  os << std::setw(kResultLen) << results.ninetyninefive << " ";
  os << std::setw(kResultLen) << results.ninetyninenine << " ";
  os << std::setw(kResultLen) << results.max;
  os << std::endl;
  // if(stage == "Total"){
  //   std::fstream
  //   f1("rocksdb_data/shinjuku_ghost_throughput.log",std::ios::app);
  //   std::fstream
  //   f2("rocksdb_data/shinjuku_ghost_ninetynine_time.log",std::ios::app);
  //   std::fstream
  //   f3("rocksdb_data/fix/shinjuku_ghost_throughput.log",std::ios::app);
  //   std::fstream
  //   f4("rocksdb_data/fix/shinjuku_ghost_ninetynine_time.log",std::ios::app);
  //   if(!f1 || !f2 || !f3 || !f4) return;
  //   f1<<std::to_string((int)(input_throughput))<<",";
  //   f2<<results.ninetynine<<",";
  //   f3<<std::to_string((int)(input_throughput))<<",";
  //   f4<<results.ninetynine<<",";
  //   f1.close();
  //   f2.close();
  //   f3.close();
  //   f4.close();
  // }
}

static void PrintStage(const std::vector<uint64_t>& durations, uint64_t runtime,
                       const std::string& stage, struct option options,
                       double input_throughput) {
  if (durations.empty()) {
    const Results<std::string> results = {.total = "-",
                                          .throughput = "-",
                                          .min = "-",
                                          .fifty = "-",
                                          .ninetynine = "-",
                                          .ninetyninefive = "-",
                                          .ninetyninenine = "-",
                                          .max = "-"};
    PrintLinePretty(std::cout, stage, results, input_throughput);
    return;
  }

  Results<uint64_t> results;
  uint64_t divisor = options.ns ? 1 : 1000;
  results.total = durations.size();
  results.throughput = (durations.size() / (runtime / NS2S));
  // When the number of latencies is even, I prefer to subtract 1 to get the
  // correct index for a percentile when we multiply the size by the percentile.
  // For example, when the size is 10, I want the 50th percentile to correspond
  // to index 4, which is only possibly when we subtract 1 from 10. When the
  // number of latencies is odd, we do not need to subtract 1. For example, when
  // the number of latencies is 9, 9 * 0.5 = 4 (using integer division), so the
  // 50th percentile corresponds to index 4.
  size_t size =
      durations.size() % 2 == 0 ? durations.size() - 1 : durations.size();

  results.min = durations.front() / divisor;
  results.fifty = durations.at(size * 0.5) / divisor;
  results.ninetynine = durations.at(size * 0.99) / divisor;
  results.ninetyninefive = durations.at(size * 0.995) / divisor;
  results.ninetyninenine = durations.at(size * 0.999) / divisor;
  results.max = durations.back() / divisor;

  PrintLinePretty(std::cout, stage, results, input_throughput);
}

static std::vector<uint64_t> GetStageResults(
    const std::vector<Request>& requests,
    const std::function<bool(const Request&)>& should_include,
    const std::function<uint64_t(const Request&)>& difference) {
  // printf("in GetStageResults, request size = %d\n", requests.size());
  std::vector<uint64_t> results;
  results.reserve(requests.size());

  for (const Request& r : requests) {
    if (should_include(r)) {
      results.push_back(difference(r));
    }
  }
  std::sort(results.begin(), results.end());
  // printf("in GetStageResults, results size = %d\n", results.size());
  return results;
}

#define HANDLE_STAGE(stage, requests, runtime, first_timestamp_name,           \
                     second_timestamp_name, options, input_throughput)         \
  {                                                                            \
    std::function<bool(const Request&)> should_include =                       \
        [](const Request& r) -> bool { return r.second_timestamp_name != 0; }; \
    std::function<uint64_t(const Request&)> difference =                       \
        [](const Request& r) -> uint64_t {                                     \
      return r.second_timestamp_name - r.first_timestamp_name;                 \
    };                                                                         \
    std::vector<uint64_t> results =                                            \
        GetStageResults(requests, should_include, difference);                 \
    PrintStage(results, runtime, stage, options, input_throughput);            \
  }

// Prints the preface to the results if pretty mode is set.
void PrintPrettyPreface(struct option options) {
  Results<std::string> results;
  std::string unit = options.ns ? "ns" : "us";
  results.total = "Total Requests";
  results.throughput = "Throughput (req/s)";
  results.min = "Min (" + unit + ")";
  results.fifty = "50% (" + unit + ")";
  results.ninetynine = "99% (" + unit + ")";
  results.ninetyninefive = "99.5% (" + unit + ")";
  results.ninetyninenine = "99.9% (" + unit + ")";
  results.max = "Max (" + unit + ")";
  PrintLinePretty(std::cout, std::string("Stage"), results, 0);
}

// Prints all results.
void Print(const std::vector<Request>& requests, uint64_t runtime,
           struct option options, double input_throughput) {
  PrintPrettyPreface(options);

  if (!options.print_last) {
    HANDLE_STAGE("Ingress Queue Time", requests, runtime, request_generated,
                 request_received, options, input_throughput);
    HANDLE_STAGE("Repeatable Handle Time", requests, runtime, request_received,
                 request_assigned, options, input_throughput);
    HANDLE_STAGE("Worker Queue Time", requests, runtime, request_assigned,
                 request_start, options, input_throughput);
    HANDLE_STAGE("Worker Handle Time", requests, runtime, request_start,
                 request_finished, options, input_throughput);
  }
  // Total time in system
  HANDLE_STAGE("Total", requests, runtime, request_generated, request_finished,
               options, input_throughput);
}

void PrintResultsHelper(const std::string& results_name,
                        uint64_t experiment_duration,
                        const std::vector<Request>& requests) {
  std::cout << results_name << ":" << std::endl;
  Print(requests, experiment_duration, options, options.throughput);
}

std::vector<Request> FilterRequests(
    const std::map<uint32_t, std::vector<Request>>& requests,
    std::function<bool(const Request&)> should_include) {
  int sum = 0;
  std::vector<Request> filtered;
  for (const std::pair<const unsigned int, std::vector<Request>>&
           worker_requests : requests) {
    sum += worker_requests.second.size();
    for (const Request& r : worker_requests.second) {
      if (should_include(r)) {
        filtered.push_back(r);
      }
    }
  }
  LOG(INFO) << "FilterRequests: origin requests.size: " << sum;
  LOG(INFO) << "FilterRequests: filtered requests.size: " << filtered.size();
  return filtered;
}

bool ShouldDiscard(const Request& request) {
  // printf("%lld, %lld\n", request.request_generated,
  // std::chrono::time_point_cast<std::chrono::nanoseconds>(
  //       start_time).time_since_epoch().count()
  //       + options.discard_duration * NS2S);
  return request.request_generated <
         std::chrono::time_point_cast<std::chrono::nanoseconds>(start_time)
                 .time_since_epoch()
                 .count() +
             options.discard_duration * NS2S;
}

void PrintResults(uint64_t experiment_duration) {
  std::cout << "Stats:" << std::endl;
  // We discard some of the results, so subtract this discard period from the
  // experiment duration so that the correct throughput is calculated.
  uint64_t tracked_duration =
      experiment_duration - options.discard_duration * NS2S;
  PrintResultsHelper("All", tracked_duration,
                     FilterRequests(requests, [](const Request& r) -> bool {
                       return !ShouldDiscard(r);
                     }));
}
/*=======================PRINT=======================*/

void init_glog() {
    // Initialize Google’s logging library.
    google::InitGoogleLogging("shinjuku scheduler");
    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;
}

int main() {
    init_glog();
    
    LOG(INFO) << "==========rocksdb experiments begin.==========";
    RegisterSignalHandlers();

    epistle = new Epistle<u_int32_t>(epistle_path, false);

    cpu_set_t cpuSet;
    CPU_ZERO(&cpuSet);
    CPU_SET(0, &cpuSet);

    auto generator = std::thread(LoadGenerator);
    sched_setaffinity(gettid(), sizeof(cpuSet), &cpuSet);

    LOG(INFO) << "create generator";

    // TODO(xiunianjun): different with ghOSt here:
    // We simply implement the thread pool in the main function.
    for (int i = 0; i < options.num_workers; i++) {
        workers.emplace_back(new CosThread(Worker, i));
    }
    
    LOG(INFO) << "create all workers!";

    int wl_i = 0;
    for (auto& t : workers) {
        // wait1
        while(!worker_lock[wl_i]) {
          smp_mb();
          // sched_yield();
          sleep(1);
        }
        // printf("%d\n", t->tid());

        wl_i++;
        worker_works.insert(
            std::make_pair(t->tid(), std::make_unique<WorkerWork>()));
        worker_works[t->tid()]->num_requests = 0;
        worker_works[t->tid()]->requests.reserve(options.batch);

        requests.insert(std::make_pair(t->tid(), std::vector<Request>()));
        const uint64_t reserve_duration = std::min(options.experiment_duration, 30);
        const size_t reserve_size = (reserve_duration + 1) * options.throughput;
        requests[t->tid()].reserve(reserve_size);

        // The first insert into a vector is slow, likely because the allocator is
        // lazy and needs to assign the physical pages on the first insert. In other
        // words, the virtual pages seem to be allocated but physical pages are not
        // assigned on the call to 'reserve'. We insert and remove a few items here
        // to handle the overhead now. Workers should not handle this initialization
        // overhead since that is not what the benchmark wants to measure.
        for (int i = 0; i < 1000; ++i) {
            requests[t->tid()].emplace_back();
        }
        requests[t->tid()].clear();
    }

    LOG(INFO) << "Init complete! Notify generator to work...";
    // notify2
    generator_lock = true;
    smp_mb();

    SetTimer(options.experiment_duration);

    while (!should_exit) {
        Pause();
    }

    // Terminal
    exit_generator = true;

    // Same with ghOSt here: calculate runtime before join all threads.
    const uint64_t runtime =
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::high_resolution_clock::now() - start_time)
            .count();
    PrintResults(runtime);
    generator.join();

    exit_worker = true;
    for (auto& t : workers) {
      epistle->add(t->tid(), EPISTLE_THREAD_RUNNABLE);
    }
    for (auto& t : workers) t->join();

    PrintResults(runtime);
    LOG(INFO) << "==========rocksdb experiments end.==========";
    return 0;
}
