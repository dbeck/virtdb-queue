#pragma once

#include <queue/params.hh>
#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <map>

namespace virtdb { namespace queue {
  
  class sync_object
  {
    std::string   path_;
    short         base_;
    uint64_t      bases_[5];
    params        parameters_;
    
    // disable copying and default construction
    // until properly implemented
    sync_object() = delete;
    sync_object(const sync_object &) = delete;
    sync_object& operator=(const sync_object &) = delete;

  protected:
    // only children should be able to construct
    sync_object(const std::string & path,
                const params & prms);
    
    // accessors
    inline short                  base()   const { return base_; }
    inline uint64_t const * const bases()  const { return bases_; }
    
    // for upcalls in get(), other common semaphore code ...
    virtual int semaphore_id() const = 0;
    virtual uint64_t get();
    
    // converting between short arrays and uint64_t
    void convert(uint64_t in,
                 unsigned short out[5]);
    uint64_t convert(unsigned short out[5]);

  public:
    virtual ~sync_object() {}
    
    // public accessors
    inline const std::string & path()  const { return path_; }
    inline const params & parameters() const { return parameters_; }
    
  };
  
  class sync_server : public sync_object
  {
    typedef std::chrono::steady_clock             clock_t;
    typedef std::chrono::steady_clock::time_point timepoint_t;

    int                        semaphore_id_;
    int                        lockfile_fd_;
    std::string                lockfile_;
    std::atomic<uint64_t>      sent_value_;
    std::atomic<uint64_t>      last_value_;
    std::atomic<bool>          stop_;
    std::thread                thread_;
    // stats
    std::atomic<uint64_t>      update_count_;

    int semaphore_id() const { return semaphore_id_; }
    void send_signal(uint64_t v);
    void entry();
        
  public:
    sync_server(const std::string & path,
                const params & prms = params());
    virtual ~sync_server();
    
    bool cleanup_all();
    void signal(uint64_t v);
    void set(uint64_t v);
    uint64_t get() { return sync_object::get(); }
    
    // stats
    uint64_t update_count() const;
  };
  
  class sync_client : public sync_object
  {
    int           semaphore_id_;
    std::string   lockfile_;

    int semaphore_id() const { return semaphore_id_; }
    
  public:
    sync_client(const std::string & path,
                const params & prms = params());
    virtual ~sync_client() {}
    
    uint64_t wait_next(uint64_t prev);
    uint64_t wait_next(uint64_t prev,
                       uint64_t timeout_ms);
    
    uint64_t get() { return sync_object::get(); }
  };
  
  class multi_sync : public sync_object
  {
    typedef std::chrono::steady_clock              clock_t;
    typedef std::chrono::steady_clock::time_point  timepoint_t;
    typedef std::unique_lock<std::mutex>           lock_t;
    
    uint8_t                    n_objs_;
    int                        semaphore_id_;
    int                        init_lockfile_fd_;
    std::string                init_lockfile_;
    
    // number of semaphores in the semaphore set:
    //   5*n_objs_ - to store a counter for each
    //   1         - global change notifier
    //
    
    // at most one object can be ours
    static const uint8_t       unknown_obj_ = 0xff;
    uint8_t                    own_obj_;
    uint64_t                   sent_value_;
    uint64_t                   last_value_;
    std::condition_variable    condvar_;
    mutable std::mutex         mtx_;
    
    std::atomic<bool>          stop_thread_;
    std::thread                thread_;
    // stats
    std::atomic<uint64_t>      update_count_;
    
    int semaphore_id() const { return semaphore_id_; }
    void send_signal(uint64_t v);
    void entry();
    
  public:
    multi_sync(const std::string & path,
               uint8_t n_objs,
               const params & prms = params());
    virtual ~multi_sync();
    
    // ownership
    bool acquire(std::vector<uint8_t> & candidates,
                 uint64_t timeout_ms);
    void release();
    
    // send to our own object
    void signal(uint64_t v);  // send through semop
    void set(uint64_t v);     // send through semctl
    
    // return the number of changed objs
    uint8_t wait_next(std::map<uint8_t,uint64_t> & prev);
    uint8_t wait_next(std::map<uint8_t,uint64_t> & prev,
                      uint64_t timeout_ms);
    
    // stats
    uint64_t update_count() const;

    //
    bool cleanup_all();
  };
  
}}
