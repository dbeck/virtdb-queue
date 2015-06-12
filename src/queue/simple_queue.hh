#pragma once

#include <queue/sync_object.hh>
#include <queue/mmapped_file.hh>
#include <queue/params.hh>
#include <set>

namespace virtdb { namespace queue {
  
  class simple_queue
  {
    std::string   path_;
    params        parameters_;
    
    // disable copying and default consturction
    // until properly implemented
    simple_queue() = delete;
    simple_queue(const simple_queue &) = delete;
    simple_queue& operator=(const simple_queue &) = delete;
    
  protected:
    const std::string & path()   const { return path_; }
    const params & parameters()  const { return parameters_; }
    simple_queue(const std::string & path,
                 const params & p);
    
    bool list_files(std::set<std::string> & results) const;
    std::string last_file() const;
    
  public:
    virtual ~simple_queue();
  };
  
  class simple_publisher : public simple_queue
  {
    sync_server           sync_;
    mmapped_writer::sptr  writer_sptr_;
    uint64_t              file_offset_;
    
  public:
    simple_publisher(const std::string & path,
                     const params & p = params());
    virtual ~simple_publisher();
    
    void push(const void * data, uint64_t len);
    std::string act_file() const;
  };
  
  class simple_subscriber : public simple_queue
  {
    sync_client           sync_;
    mmapped_reader::sptr  reader_sptr_;
    
  public:
    simple_subscriber(const std::string & path,
                      const params & p = params());
    virtual ~simple_subscriber();
  };
  
}}
