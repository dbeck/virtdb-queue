#pragma once

#include <string>
#include <atomic>
#include <thread>

namespace virtdb { namespace queue {
  
  class sync_object
  {
    std::string   path_;
    short         base_;
    uint64_t      bases_[5];
    
    // disable copying and default consturction
    // until properly implemented
    sync_object() = delete;
    sync_object(const sync_object &) = delete;
    sync_object& operator=(const sync_object &) = delete;

  protected:
    // only children should be able to construct
    sync_object(const std::string & path);
    
    // accessors
    inline short base() const { return base_; }
    inline uint64_t const * const bases() const { return bases_; }
    
    // for upcalls in get(), other common semaphore code ...
    virtual int semaphore_id() const = 0;
    
    // converting between short arrays and uint64_t
    void convert(uint64_t in, unsigned short out[5]);
    uint64_t convert(unsigned short out[5]);
    
  public:
    virtual ~sync_object() {}
    const std::string & path() const { return path_; }
    uint64_t get();
  };
  
  class sync_server : public sync_object
  {
    typedef std::chrono::steady_clock             clock_t;
    typedef std::chrono::steady_clock::time_point timepoint_t;

    int                        semaphore_id_;
    int                        lockfile_fd_;
    std::string                lockfile_;
    std::atomic<uint64_t>      last_value_;
    uint64_t                   throttle_ms_;
    std::atomic<bool>          stop_;
    std::thread                thread_;

    int semaphore_id() const { return semaphore_id_; }
    void send_signal(uint64_t v);
    void entry();
    
    // last segment written
    // last segment size
    // last message written
    
  public:
    sync_server(const std::string & path,
                uint64_t throttle_ms=1);
    virtual ~sync_server();
    
    bool cleanup_all();
    void signal();
    void set(uint64_t v);
  };
  
  class sync_client : public sync_object
  {
    int           semaphore_id_;
    std::string   lockfile_;

    int semaphore_id() const { return semaphore_id_; }
    
  public:
    sync_client(const std::string & path);
    virtual ~sync_client() {}
    
    uint64_t wait_next(uint64_t prev);
  };
  
}}
