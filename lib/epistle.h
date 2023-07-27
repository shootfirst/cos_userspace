#include <fcntl.h>  // for O_RDWR and open
#include <sys/mman.h>
#include <iostream>
#include <atomic>
#include <cassert>

#define EPISTLE_THREAD_RUNNABLE 0
#define EPISTLE_THREAD_IDLE 1

#define HASH_MAP_SIZE 4096

#define smp_mb()        asm volatile("mfence":::"memory")

template <typename T>

class Epistle {
public:
	template <typename V>
	struct Entry {
  		int32_t key;
  		V value;
	};

    Epistle(std::string shm_path, bool is_first) : shm_path_(shm_path) {
        int shm_fd;
        if (is_first) {
            shm_fd = open(shm_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
        } else {
            shm_fd = open(shm_path.c_str(), O_RDWR);
        }
        if (shm_fd < 0) {
            LOG(ERROR) << "open epistle fail!";
            exit(1);
        }

        void* shd_mem_ =
            mmap(NULL, HASH_MAP_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
        if (shd_mem_ == MAP_FAILED) {  
            LOG(ERROR) << "mmap epistle fail!";
            exit(1);
        }

        if (is_first) {
            ftruncate(shm_fd, HASH_MAP_SIZE);
            memset(shd_mem_, 0, HASH_MAP_SIZE);
        }

    	capacity_ = HASH_MAP_SIZE / sizeof(Entry<T>);
    	table_ = reinterpret_cast<Entry<T>*>(shd_mem_);
  	}

  	bool add(int32_t key, T value) {
    	assert(key != 0);
    	for (int idx = hash(key); ; idx = (idx + 1) % capacity_) {
      		if (table_[idx].key == 0) {
        		if (!__sync_bool_compare_and_swap(&table_[idx].key, 0, key)) {
          			continue;
        		}
      		}
      		if (table_[idx].key != key) {
        		continue;
      		}
      		table_[idx].value = value;
			smp_mb();
      		return true;
    	}
  	}

  	T get(int32_t key) {
    	assert(key != 0);
    	for (int idx = hash(key); ; idx = (idx + 1) % capacity_) {
      		if (table_[idx].key == 0) {
        		return {};
      		}
      		if (table_[idx].key != key) {
        		continue;
      		}
      		return table_[idx].value;
    	}
  	}

private:
  	int32_t hash(int32_t key) {
    	return key % capacity_;
  	}

  	void* shd_mem_;
    size_t capacity_;
  	Entry<T>* table_;
    std::string shm_path_;
};

