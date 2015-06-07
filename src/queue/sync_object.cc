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

  sync_object::sync_object(const std::string & path)
  : path_{path},
    base_{16000},
    bases_{
      1,
      (uint64_t)base_,
      ((uint64_t)base_)*((uint64_t)base_),
      ((uint64_t)base_)*((uint64_t)base_)*((uint64_t)base_),
      ((uint64_t)base_)*((uint64_t)base_)*((uint64_t)base_)*((uint64_t)base_)
    }
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
    semun_t arg;
    arg.array = vals;
    
    if ( ::semctl(semaphore_id(),0,GETALL,arg) < 0 )
      THROW_("couldn't set value for semaphores");
    
    return convert(vals);
  }
  
  sync_server::sync_server(const std::string & path,
                           uint64_t throttle_ms)
  : sync_object{path},
    semaphore_id_{-1},
    lockfile_fd_{-1},
    last_value_{0},
    throttle_ms_{throttle_ms},
    next_update_{clock_t::now()+std::chrono::milliseconds{throttle_ms}},
    stop_{false},
    thread_{[this](){entry();}}
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
      lockfile_fd_ = open(lock_path.c_str(), O_WRONLY|O_CREAT|O_EXLOCK|O_EXCL);
      if( lockfile_fd_ < 0 )
      {
        THROW_(std::string{"failed to create and lock lockfile: "}+lock_path);
      }
      
      lockfile_ = lock_path;
      
      // will close the lockfile on failure
      on_return close_lockfile([this](){
        ::flock(lockfile_fd_, LOCK_UN);
        ::close(lockfile_fd_);
        lockfile_fd_ = -1;
      });
      
      if( ::fchmod(lockfile_fd_, S_IRUSR|S_IWUSR) )
      {
        THROW_(std::string{"failed to set permissions on lockfile: "}+lock_path);
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
      
      semaphore_id_ = ::semget(semkey, 5, SEM_R|SEM_A );
      if( semaphore_id_ < 0 )
      {
        semaphore_id_ = ::semget(semkey, 5, SEM_R|SEM_A|IPC_CREAT );
        
        // zero semaphore values upon creation
        {
          unsigned short values[5] = { 0,0,0,0,0 };
          semun_t arg;
          arg.array = values;
          
          if ( ::semctl(semaphore_id_,0,SETALL,arg) < 0 )
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
    thread_.join();
  }
  
  void
  sync_server::entry()
  {
    uint64_t prev = 0;
    while( !stop_ )
    {
      std::this_thread::sleep_for(std::chrono::milliseconds{throttle_ms_});
      if( prev < last_value_ )
        send_signal(last_value_);
      prev = last_value_;
    }
  }
  
  void
  sync_server::signal()
  {
    if( (last_value_%base()) == (base()-2) )
      send_signal(last_value_.load()+1);
    ++last_value_;
  }

  void
  sync_server::send_signal(uint64_t v)
  {
    unsigned short values[5];
    convert(v, values);
    semun_t arg;
    arg.array = values;

    if ( ::semctl(semaphore_id_,0,SETALL,arg) < 0 )
      perror("failed to set semaphore values");
  }
  
  void
  sync_server::set(uint64_t v)
  {
    unsigned short short_values[5];
    convert(v, short_values);
    
    semun_t arg;
    arg.array = short_values;
    
    if ( ::semctl(semaphore_id_,0,SETALL,arg) < 0 )
      THROW_("couldn't set value for semaphores");
  }
  
  sync_client::sync_client(const std::string & path)
  : sync_object{path}
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
      semaphore_id_ = ::semget(semkey, 5, SEM_R|SEM_A );
      if( semaphore_id_ < 0 )
      {
        THROW_("failed to open semaphore");
      }
    }
  }
  
  uint64_t
  sync_client::wait_next(uint64_t prev)
  {
    uint64_t act_val = get();
    if( act_val > prev )
      return act_val;
    
    unsigned short prev_values[5];
    convert(prev, prev_values);
    
    while( act_val <= prev )
    {
      bool done = false;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      
      {
        struct sembuf ops[2];
        ops[0].sem_num  = 4;
        ops[0].sem_op   = -1*(prev_values[4]+1);
        ops[0].sem_flg  = IPC_NOWAIT;
        ops[1].sem_num  = 4;
        ops[1].sem_op   = prev_values[4]+1;
        ops[1].sem_flg  = 0;
        
        if( semop(semaphore_id(),ops,2) >= 0 )
          done = true;
      }

      if( !done )
      {
        struct sembuf ops[2];
        ops[0].sem_num  = 3;
        ops[0].sem_op   = -1*(prev_values[3]+1);
        ops[0].sem_flg  = IPC_NOWAIT;
        ops[1].sem_num  = 3;
        ops[1].sem_op   = prev_values[3]+1;
        ops[1].sem_flg  = 0;
        
        if( semop(semaphore_id(),ops,2) >= 0 )
          done = true;
      }

      if( !done )
      {
        struct sembuf ops[2];
        ops[0].sem_num  = 2;
        ops[0].sem_op   = -1*(prev_values[2]+1);
        ops[0].sem_flg  = IPC_NOWAIT;
        ops[1].sem_num  = 2;
        ops[1].sem_op   = prev_values[2]+1;
        ops[1].sem_flg  = 0;
        
        if( semop(semaphore_id(),ops,2) >= 0 )
          done = true;
      }

      if( !done )
      {
        struct sembuf ops[2];
        ops[0].sem_num  = 1;
        ops[0].sem_op   = -1*(prev_values[1]+1);
        ops[0].sem_flg  = IPC_NOWAIT;
        ops[1].sem_num  = 1;
        ops[1].sem_op   = prev_values[1]+1;
        ops[1].sem_flg  = 0;
        
        if( semop(semaphore_id(),ops,2) >= 0 )
          done = true;
      }

      if( !done /* && prev_values[0] < (base()*2/3) */ )
      {
        struct sembuf ops[2];
        ops[0].sem_num  = 0;
        ops[0].sem_op   = -1*(prev_values[0]+1);
        ops[0].sem_flg  = IPC_NOWAIT; // 0
        ops[1].sem_num  = 0;
        ops[1].sem_op   = prev_values[0]+1;
        ops[1].sem_flg  = 0;
        
        if( semop(semaphore_id(),ops,2) >= 0 )
          done = true;
      }

      act_val = get();
    }
    
    return act_val;
  }
  
}}
