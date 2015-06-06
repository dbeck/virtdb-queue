#include <queue/sync_object.hh>
#include <queue/exception.hh>
#include <sys/stat.h>

namespace virtdb { namespace queue {

  sync_object::sync_object(const std::string & path)
  : path_{path}
  {
    if( path.empty() ) { THROW_("invalid parameter: path"); }
  }
  
  sync_server::sync_server(const std::string & path)
  : sync_object{path}
  {
    struct stat dir_stat;
    if( lstat(path.c_str(), &dir_stat) == 0 )
    {
      // is a directory
      if( dir_stat.st_mode & S_IFDIR )
      {
        //return true;
      }
    }
    
    {
      struct stat res;
      if( lstat(path.c_str(), &res) == 0 )
      {
        // neither group or others can access
        if( ((res.st_mode & S_IRWXG) | (res.st_mode & S_IRWXO)) == 0 )
        {
          //return true;
        }
      }
    }

    /*
    // ?????
    open_directory(path);
    open_lockfile(path);
    open_semaphores();
     */
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
