#pragma once

#include <string>

namespace virtdb { namespace queue {
  
  class sync_object
  {
    std::string path_;
  protected:
    sync_object(const std::string & path);
    
  public:
    virtual ~sync_object() {}
    const std::string & path() const { return path_; }
  };
  
  class sync_server : public sync_object
  {
    int semaphores_;
    int semaphore_id_;
    
  public:
    sync_server(const std::string & path);
    virtual ~sync_server() {}
  };
  
  class sync_client : public sync_object
  {
    int semaphores_;
    int semaphore_id_;
    
  public:
    sync_client(const std::string & path);
    virtual ~sync_client() {}
  };
  
}}
