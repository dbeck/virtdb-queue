#include <queue/sync_object.hh>
#include <queue/exception.hh>
#include <queue/on_return.hh>
// C lib
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
// C++11
#include <thread>
#include <chrono>

namespace virtdb { namespace queue {

  sync_object::sync_object(const std::string & path,
                           const params & prms)
  : path_{path},
    base_{16000},
    bases_{
      1,
      (uint64_t)base_,
      ((uint64_t)base_)*((uint64_t)base_),
      ((uint64_t)base_)*((uint64_t)base_)*((uint64_t)base_),
      ((uint64_t)base_)*((uint64_t)base_)*((uint64_t)base_)*((uint64_t)base_)
    },
    parameters_{prms}
  {
    if( path.empty() ) { THROW_("invalid parameter: path"); }
  }
  
  void
  sync_object::convert(uint64_t in,
                       unsigned short out[5])
  {
    if( !out ) { THROW_("invalid parameter"); }
    auto a = base();
    auto b = bases();
    
    out[0] = (in/b[0])%a;
    out[1] = (in/b[1])%a;
    out[2] = (in/b[2])%a;
    out[3] = (in/b[3])%a;
    out[4] = (in/b[4])%a;
  }
  
  uint64_t
  sync_object::convert(unsigned short in[5])
  {
    if( !in ) { THROW_("invalid parameter"); }
    auto b = bases();
    uint64_t ret = (in[0]*b[0]) + (in[1]*b[1]) + (in[2]*b[2]) + (in[3]*b[3]) + (in[4]*b[4]);
    return ret;
  }
  
  uint64_t
  sync_object::get()
  {
    unsigned short vals[5];
    //union semun arg;
    //arg.array = vals;
    
    if ( ::semctl(semaphore_id(),0,GETALL,vals) < 0 )
      THROW_("couldn't get value for semaphores");
    
    return convert(vals);
  }
  
  sync_server::sync_server(const std::string & path,
                           const params & prms)
  : sync_object{path, prms},
    semaphore_id_{-1},
    lockfile_fd_{-1},
    sent_value_{0},
    last_value_{0},
    stop_{false},
    thread_{[this](){entry();}}
  {
    struct stat dir_stat;
    on_return cleanup_on_exit([this](){
      stop_ = true;
      thread_.join();
    });
    
    if( ::lstat(path.c_str(), &dir_stat) == 0 )
    {
      // is a directory
      if( !(dir_stat.st_mode & S_IFDIR) )
      {
        THROW_(std::string{"another non-folder object exists at the path given: "}+path);
      }
      
      // neither group or others can access
      if( ((dir_stat.st_mode & S_IRWXG) | (dir_stat.st_mode & S_IRWXO)) != 0 )
      {
        THROW_(std::string{"permissions allow group or others to access: "}+path);
      }
    }
    else
    {
      if( ::mkdir(path.c_str(), 0700) )
      {
        THROW_(std::string{"failed to create folder at: "}+path);
      }
    }

    // check lock object
    std::string lock_path{path+"/sync.lck"};
    struct stat lock_stat;

    if( ::lstat(lock_path.c_str(), &lock_stat) )
    {
      // lock file doesn't yet exists
      lockfile_fd_ = open(lock_path.c_str(), O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR);
      if( lockfile_fd_ < 0 )
      {
        THROW_(std::string{"failed to create and lock lockfile: "}+lock_path);
      }
      
      lockfile_ = lock_path;
      
      // will close the lockfile on failure
      on_return close_lockfile([this](){
        ::close(lockfile_fd_);
        lockfile_fd_ = -1;
      });
      
      if( ::flock(lockfile_fd_, LOCK_EX|LOCK_NB) )
      {
        THROW_(std::string{"failed to lock lockfile: "}+lock_path);
      }

      // disarm
      close_lockfile.reset();
    }
    else
    {
      // neither group or others can access
      if( ((lock_stat.st_mode & S_IRWXG) | (lock_stat.st_mode & S_IRWXO)) != 0 )
      {
        THROW_(std::string{"permissions allow group or others to access: "}+lock_path);
      }
      
      lockfile_fd_ = open(lock_path.c_str(), O_WRONLY);
      if( lockfile_fd_ < 0 )
      {
        THROW_(std::string{"failed to open lockfile: "}+lock_path);
      }
      
      // will close the lockfile on failure
      on_return close_lockfile([this](){
        ::flock(lockfile_fd_, LOCK_UN);
        ::close(lockfile_fd_);
        lockfile_fd_ = -1;
      });
      
      if( ::flock(lockfile_fd_, LOCK_EX|LOCK_NB) )
      {
        THROW_(std::string{"failed to lock lockfile: "}+lock_path);
      }
    
      lockfile_ = lock_path;
      
      // disarm
      close_lockfile.reset();
    }
    
    {
      key_t semkey = ::ftok(lock_path.c_str(), 1);
      
      semaphore_id_ = ::semget(semkey, 5, 0600 );
      if( semaphore_id_ < 0 )
      {
        semaphore_id_ = ::semget(semkey, 5, 0600|IPC_CREAT );
        
        // zero semaphore values upon creation
        {
          unsigned short values[5] = { 0,0,0,0,0 };

          if ( ::semctl(semaphore_id_,0,SETALL,values) < 0 )
          {
            THROW_("couldn't set initial value for semaphores");
          }
        }
      }
      
      if( semaphore_id_ < 0 )
      {
        THROW_("failed to create semaphore");
      }
    }
    
    // disarm thread cleanup guard
    cleanup_on_exit.reset();
  }
  
  bool
  sync_server::cleanup_all()
  {
    // destructor do less than this as these object should persist
    // accross restarts
    if( semaphore_id_ >= 0 )
    {
      if( ::semctl(semaphore_id_, 0, IPC_RMID) < 0 )
        perror("failed to remove semaphores");
    }
    
    if( lockfile_fd_ > 0 )
    {
      ::flock(lockfile_fd_, LOCK_UN);
      ::close(lockfile_fd_);
      lockfile_fd_ = -1;
    }
    
    ::unlink(lockfile_.c_str());
    return true;
  }
  
  sync_server::~sync_server()
  {
    if( lockfile_fd_ > 0 )
    {
      ::flock(lockfile_fd_, LOCK_UN);
      ::close(lockfile_fd_);
    }
    
    stop_ = true;
    if( thread_.joinable() )
      thread_.join();
  }
  
  void
  sync_server::entry()
  {
    while( !stop_ )
    {
      std::this_thread::sleep_for(std::chrono::milliseconds{parameters().sync_throttle_ms_});
      if( sent_value_ < last_value_ )
      {
        uint64_t last_val = last_value_;
        send_signal(last_val-sent_value_);
        sent_value_ = get();
      }
    }
  }
  
  void
  sync_server::signal(uint64_t v)
  {
    last_value_ = v;
  }

  void
  sync_server::send_signal(uint64_t v)
  {
    while( v > 0 )
    {
      {
        uint64_t mod = v;
        if( mod > (base()*9/10) )
          mod = (base()*9/10);
        
        {
          // adjust semaphore #0 first
          struct sembuf ops[3];
          ops[0].sem_num  = 0;
          ops[0].sem_op   = (short)mod;
          ops[0].sem_flg  = 0;
          ops[1].sem_num  = 0;
          ops[1].sem_op   = -1*(short)base();
          ops[1].sem_flg  = IPC_NOWAIT;
          ops[2].sem_num  = 1;
          ops[2].sem_op   = 1;
          ops[2].sem_flg  = 0;
          
          if( semop(semaphore_id(),ops,3) < 0 )
          {
            struct sembuf ops[1];
            ops[0].sem_num  = 0;
            ops[0].sem_op   = (short)mod;
            ops[0].sem_flg  = 0;
            
            if( semop(semaphore_id(),ops,1) < 0 )
            {
              perror("failed to set semaphore");
            }
          }
        }
        v -= mod;
        {
          // handle overflow on semaphore #2
          struct sembuf ops[2];
          ops[0].sem_num  = 1;
          ops[0].sem_op   = -1*(short)base();
          ops[0].sem_flg  = IPC_NOWAIT;
          ops[1].sem_num  = 2;
          ops[1].sem_op   = 1;
          ops[1].sem_flg  = 0;
          semop(semaphore_id(),ops,2);
        }
        {
          // handle overflow on semaphore #3
          struct sembuf ops[2];
          ops[0].sem_num  = 2;
          ops[0].sem_op   = -1*(short)base();
          ops[0].sem_flg  = IPC_NOWAIT;
          ops[1].sem_num  = 3;
          ops[1].sem_op   = 1;
          ops[1].sem_flg  = 0;
          semop(semaphore_id(),ops,2);
        }
        {
          // handle overflow on semaphore #4
          struct sembuf ops[2];
          ops[0].sem_num  = 3;
          ops[0].sem_op   = -1*(short)base();
          ops[0].sem_flg  = IPC_NOWAIT;
          ops[1].sem_num  = 4;
          ops[1].sem_op   = 1;
          ops[1].sem_flg  = 0;
          semop(semaphore_id(),ops,2);
        }
      }
    }
  }
  
  void
  sync_server::set(uint64_t v)
  {
    unsigned short short_values[5];
    convert(v, short_values);
    // increasing sent value in advance to prevent updates on the other thread
    sent_value_ = v;
    last_value_ = v;

    if ( ::semctl(semaphore_id_,0,SETALL,short_values) < 0 )
      THROW_("couldn't set value for semaphores");
    
    sent_value_ = get();
  }
  
  sync_client::sync_client(const std::string & path,
                           const params & prms)
  : sync_object{path, prms}
  {
    
    struct stat dir_stat;
    if( ::lstat(path.c_str(), &dir_stat) == 0 )
    {
      // is a directory
      if( !(dir_stat.st_mode & S_IFDIR) )
      {
        THROW_(std::string{"another non-folder object exists at the path given: "}+path);
      }
      
      // neither group or others can access
      if( ((dir_stat.st_mode & S_IRWXG) | (dir_stat.st_mode & S_IRWXO)) != 0 )
      {
        THROW_(std::string{"permissions allow group or others to access: "}+path);
      }
    }
    else
    {
      THROW_(std::string{"failed to open folder at: "}+path);
    }
    
    // check lock object
    std::string lock_path{path+"/sync.lck"};
    struct stat lock_stat;
    
    if( ::lstat(lock_path.c_str(), &lock_stat) )
    {
      THROW_(std::string{"failed to open lockfile at: "}+lock_path);
    }
    
    // neither group or others can access
    if( ((lock_stat.st_mode & S_IRWXG) | (lock_stat.st_mode & S_IRWXO)) != 0 )
    {
      THROW_(std::string{"permissions allow group or others to access: "}+lock_path);
    }
    
    {
      key_t semkey = ::ftok(lock_path.c_str(), 1);
      semaphore_id_ = ::semget(semkey, 5, 0600 );
      if( semaphore_id_ < 0 )
      {
        THROW_("failed to open semaphore");
      }
    }
  }
  
  uint64_t
  sync_client::wait_next(uint64_t prev)
  {
    uint64_t act_val = 0;
    
    while( act_val <= prev )
    {
      unsigned short vals[5];
      
      if ( ::semctl(semaphore_id(),0,GETALL,vals) < 0 )
        THROW_("couldn't get value for semaphores");
      
      act_val = convert(vals);
      if( act_val > prev ) return act_val;
      
#ifdef _GNU_SOURCE
      if( vals[0] < (base()*9/10) )
      {
        // timed wait
        struct sembuf ops[2];
        ops[0].sem_num  = 0;
        ops[0].sem_op   = -1*(vals[0]+1);
        ops[0].sem_flg  = 0;
        ops[1].sem_num  = 0;
        ops[1].sem_op   = vals[0]+1;
        ops[1].sem_flg  = 0;

        // 20 ms
        struct timespec ts = { 0, 20*1000000 };
        semtimedop(semaphore_id(),ops,2,&ts);
      }
      else
#endif
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    return act_val;
  }
  
}}
