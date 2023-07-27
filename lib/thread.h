/* Copyright (c) 2022 Meta, Inc */
#ifndef TOOLS_SCHED_EXT_LIB_COS_THREAD_H_
#define TOOLS_SCHED_EXT_LIB_COS_THREAD_H_

#include <utility>
#include <thread>      // NOLINT
#include <functional>  // for std::function

class CosThread {
public:
	// The kernel scheduling class to run the thread in.
	enum class KernelSchedulerType {
		// Linux Completely Fair Scheduler.
		kCfs,
		// cos.
		kCos,
	};

	explicit CosThread(KernelSchedulerType ksched, std::function<void()> work) {
		work_start_ = false;
		ksched_ = ksched;
		thread_ = std::thread([this, w = std::move(work)] {
			tid_ = gettid();
			NotifyInitComplete();

			// if (ksched_ == KernelSchedulerType::kCos) {
			WaitUntilWork();
			// }

			std::move(w)();
		});
	}
	explicit CosThread(const CosThread&) = delete;
	CosThread& operator=(const CosThread&) = delete;
	~CosThread() = default;

	// Joins the thread.
	void Join() { thread_.join(); }

	void WaitUntilWork() {
		while (!work_start_) {
			// sched_yield();
			sleep(1);
		}
	}

	void NotifyWork() { work_start_ = true; }

	void WaitUntilInitComplete() {
		while (!init_complete_) {
			sched_yield();
		}
	}

	void NotifyInitComplete() { init_complete_ = true; }

	bool Joinable() const { return thread_.joinable(); }

	int tid() { return tid_; }

private:
	volatile bool work_start_;

	volatile bool init_complete_;

	// The thread's TID (thread identifier).
	int tid_;

	// The kernel scheduling class the thread is running in.
	KernelSchedulerType ksched_;

	// The thread.
	std::thread thread_;
};

#endif  // TOOLS_SCHED_EXT_LIB_COS_THREAD_H_
