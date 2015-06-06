#include <queue/sync_object.hh>
#include <queue/exception.hh>
#include <queue/on_return.hh>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

namespace virtdb { namespace queue {

  sync_object::sync_object(const std::string & path)
  : path_{path}
  {
    if( path.empty() ) { THROW_("invalid parameter: path"); }
  }
  
  sync_server::sync_server(const std::string & path)
  : sync_object{path},
    lockfile_fd_{-1}
  {
    struct stat dir_stat;
    bool new_folder_created = false;
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
      new_folder_created = true;
    }

    // check lock object
    std::string lock_path{path+"/sync.lck"};
    struct stat lock_stat;
    bool no_lockfile_yet = false;

    if( ::lstat(lock_path.c_str(), &lock_stat) )
    {
      // lock file doesn't yet exists
      no_lockfile_yet = true;
    }
    
    if( new_folder_created || no_lockfile_yet )
    {
      int semaphore_id_ = ::semget(IPC_PRIVATE, 4, SEM_R|SEM_A|IPC_CREAT );
      if( semaphore_id_ < 0 )
      {
        THROW_("failed to create semaphore");
      }
      // will remove the semaphore if next steps fail
      on_return cleanup_sem_on_failure([semaphore_id_](){ ::semctl(semaphore_id_, 0, IPC_RMID); });
      
      // zero semaphore values
      {
        unsigned short values[4] = { 0,0,0,0 };
        semun_t arg;
        arg.array = values;
        
        if ( ::semctl(semaphore_id_,0,SETALL,arg) < 0 )
        {
          THROW_("couldn't set initial value for semaphores");
        }
      }
      
      {
        lockfile_fd_ = open(lock_path.c_str(), O_WRONLY|O_CREAT|O_EXLOCK|O_EXCL);
        if( lockfile_fd_ < 0 )
        {
          THROW_(std::string{"failed to create and lock lockfile: "}+lock_path);
        }

        // will close the lockfile on failure
        on_return close_lockfile([this](){ ::flock(lockfile_fd_, LOCK_UN); ::close(lockfile_fd_); lockfile_fd_ = -1; });
        
        if( ::fchmod(lockfile_fd_, S_IRUSR|S_IWUSR) )
        {
          THROW_(std::string{"failed to set permissions on lockfile: "}+lock_path);
        }

        std::string lockfile_content{std::to_string(semaphore_id_) + "\n"};
        if( ::write(lockfile_fd_, lockfile_content.c_str(), lockfile_content.size()) != lockfile_content.size() )
        {
          THROW_(std::string{"failed to write lockfile content to: "}+lock_path);
        }
        
        // disarm lockfile failure protection
        close_lockfile.reset();
      }
      
      // disarm semaphore cleanup
      cleanup_sem_on_failure.reset();
    }
    else
    {
      lockfile_fd_ = open(lock_path.c_str(), O_RDWR|O_EXLOCK);
      if( lockfile_fd_ < 0 )
      {
        THROW_(std::string{"failed to exclusively open and lock lockfile: "}+lock_path);
      }
      
      // will close the lockfile on failure
      on_return close_lockfile([this](){ ::flock(lockfile_fd_, LOCK_UN); ::close(lockfile_fd_); lockfile_fd_ = -1; });

      if( lock_stat.st_size > 15 || lock_stat.st_size < 2 )
      {
        THROW_(lock_path+" has unexpected size: "+std::to_string(lock_stat.st_size));
      }

      char tmp_buf[30];
      if( ::read(lockfile_fd_, tmp_buf, lock_stat.st_size) != lock_stat.st_size )
      {
        THROW_(std::string{"failed to read: "}+lock_path);
      }
      
      int semid = ::atoi(tmp_buf);
      if( semid <= 0 )
      {
        THROW_(std::string{"invalid semaphore id in the lockfile: "}+lock_path);
      }
      
      // try to open the semaphore
      int sem_res = ::semget(semid, 4, SEM_R|SEM_A );
      if( sem_res < 0 )
      {
        // not sure ...
        perror("semget failed");
        THROW_(std::string{"failed to open semaphore id: "}+std::to_string(semid));
      }
      
      // disarm lockfile failure protection
      close_lockfile.reset();

    }

    /*
    // ?????
    open_directory(path);
    open_lockfile(path);
    open_semaphores();
     */
  }
  
  sync_server::~sync_server()
  {
    if( lockfile_fd_ > 0 )
    {
      ::flock(lockfile_fd_, LOCK_UN);
      ::close(lockfile_fd_);
    }
  }

  sync_client::sync_client(const std::string & path)
  : sync_object{path}
                
  {
    struct stat dir_stat;
    if( lstat(path.c_str(), &dir_stat) == 0 )
    {
      // is a directory
      if( dir_stat.st_mode & S_IFDIR )
      {
        return true;
      }
    }
    
    // open_directory(path);
    // open_lockfile(path);
    // open_semaphores();
  }
  
}}
