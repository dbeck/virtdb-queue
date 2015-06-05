#include <gtest/gtest.h>
#include <queue/shared_mem.hh>
#include <queue/exception.hh>

// for semaphores
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>

#include <future>

using namespace virtdb::queue;

namespace virtdb { namespace test {
  
  class SysVSemaphore : public ::testing::Test { };
  class QueueTest : public ::testing::Test { };
  
}}

using namespace virtdb::test;

namespace {
  
  
  void reset_counter(int semid)
  {
    unsigned short values[4] = { 0,0,0,0 };
    semun_t arg;
    arg.array = values;
    
    if ( semctl(semid,0,SETALL,arg) < 0 )
    {
      perror("semctl failure Reason:");
    }
    
    if ( semctl(semid,0,GETALL,arg) < 0 )
    {
      perror("semctl failure Reason:");
    }
    
    for( int i=0; i<4; ++i )
    {
      EXPECT_EQ(values[i], 0);
    }
  }
  
  void increase_counter(int semid)
  {
    { // do a first increase on the first semaphore
      struct sembuf ops[1];
      ops[0].sem_num = 0;
      ops[0].sem_op  = 1;
      ops[0].sem_flg = 0;
      
      if( semop(semid,ops,1) < 0 )
      {
        perror("semctl failure Reason:");
      }
    }
    
    { // handle overflow on the first semaphore
      
      struct sembuf ops[2];
      ops[0].sem_num  = 0;
      ops[0].sem_op   = -100;
      ops[0].sem_flg  = IPC_NOWAIT;
      ops[1].sem_num  = 1;
      ops[1].sem_op   = 1;
      ops[1].sem_flg  = 0;
      
      if( semop(semid,ops,2) < 0 )
      {
        return;
      }
    }
    
    { // handle overflow on the second semaphore
      
      struct sembuf ops[2];
      ops[0].sem_num  = 1;
      ops[0].sem_op   = -100;
      ops[0].sem_flg  = IPC_NOWAIT;
      ops[1].sem_num  = 2;
      ops[1].sem_op   = 1;
      ops[1].sem_flg  = 0;
      
      if( semop(semid,ops,2) < 0 )
      {
        return;
      }
    }

    { // handle overflow on the third semaphore
      
      struct sembuf ops[2];
      ops[0].sem_num  = 2;
      ops[0].sem_op   = -100;
      ops[0].sem_flg  = IPC_NOWAIT;
      ops[1].sem_num  = 3;
      ops[1].sem_op   = 1;
      ops[1].sem_flg  = 0;
      
      if( semop(semid,ops,2) < 0 )
      {
        return;
      }
    }
    
    return;
  }
  
  uint64_t wait_next(int semid, uint64_t act, uint64_t lim)
  {
    unsigned short values[4] = { 0,0,0,0 };
    semun_t arg;
    arg.array = values;
    
    if ( semctl(semid,0,GETALL,arg) < 0 )
    {
      perror("semctl failure Reason:");
    }
    
    uint64_t new_val =
      values[0] + (100*values[1]) + (10000*values[2]) + (1000000*values[3]);
    
    EXPECT_LE(new_val, lim);
    
    if( new_val > act )
      return new_val;
    
    unsigned short act_values[4] = {
      (unsigned short)(act%100),
      (unsigned short)((act/100)%100),
      (unsigned short)((act/10000)%100),
      (unsigned short)((act/1000000)%100)
    };
    
    unsigned short lim_values[4] = {
      (unsigned short)(lim%100),
      (unsigned short)((lim/100)%100),
      (unsigned short)((lim/10000)%100),
      (unsigned short)((lim/1000000)%100)
    };
    
    // TODO : wait here
    while( new_val <= act )
    {
      bool done = false;
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
      {
        struct sembuf ops[2];
        ops[0].sem_num  = 3;
        ops[0].sem_op   = -1*(act_values[3]+1);
        ops[0].sem_flg  = IPC_NOWAIT;
        ops[1].sem_num  = 3;
        ops[1].sem_op   = act_values[3]+1;
        ops[1].sem_flg  = 0;
        
        if( semop(semid,ops,2) >= 0 )
        {
          done = true;
        }
      }
      
      if( !done )
      {
        struct sembuf ops[2];
        ops[0].sem_num  = 2;
        ops[0].sem_op   = -1*(act_values[2]+1);
        ops[0].sem_flg  = IPC_NOWAIT;
        ops[1].sem_num  = 2;
        ops[1].sem_op   = act_values[2]+1;
        ops[1].sem_flg  = 0;
        
        if( semop(semid,ops,2) >= 0 )
        {
          done = true;
        }
      }

      if( !done )
      {
        struct sembuf ops[2];
        ops[0].sem_num  = 1;
        ops[0].sem_op   = -1*(act_values[1]+1);
        ops[0].sem_flg  = IPC_NOWAIT;
        ops[1].sem_num  = 1;
        ops[1].sem_op   = act_values[1]+1;
        ops[1].sem_flg  = 0;
        
        if( semop(semid,ops,2) >= 0 )
        {
          done = true;
        }
      }
      
      if( !done && act_values[0] < lim_values[0] && lim_values[0] > 0 )
      {
        struct sembuf ops[2];
        ops[0].sem_num  = 0;
        ops[0].sem_op   = -1*(act_values[0]+1);
        ops[0].sem_flg  = 0;
        ops[1].sem_num  = 0;
        ops[1].sem_op   = act_values[0]+1;
        ops[1].sem_flg  = 0;
        
        semop(semid,ops,2);
      }
      
      if ( semctl(semid,0,GETALL,arg) < 0 )
      {
        perror("semctl failure Reason:");
      }
      
      new_val = values[0] + (100*values[1]) + (10000*values[2]) + (1000000*values[3]);
      EXPECT_LE(new_val, lim);
    }
    
    return new_val;
  }
    
}

TEST_F(SysVSemaphore, TestShared1)
{
  /*
  shared_counter sc1("/tmp");
  {
    shared_counter sc2("/tmp");
  }
  */
}


TEST_F(SysVSemaphore, Parallel2)
{
  int semid = semget(IPC_PRIVATE, 4, SEM_R|SEM_A|IPC_CREAT );
  std::cout << "semget returned : " << semid << "\n";
  if( semid < 0 )
  {
    perror("semget failed Reason:");
    return;
  }
  
  for( int k=0; k<10; ++k )
  {
    std::promise<void> when_started1;
    std::future<void> on_start1{when_started1.get_future()};
    
    std::promise<void> when_started2;
    std::future<void> on_start2{when_started2.get_future()};

    uint64_t lim = (k+10)*12345;
    reset_counter(semid);
    
    std::thread thr1([&](){
      when_started1.set_value();
      for( uint64_t i=0;i<lim;++i )
        increase_counter(semid);
    });
    
    on_start1.wait();
    
    std::thread thr2([&](){
      when_started2.set_value();
      uint64_t v=0;
      while( v!=lim)
        v = wait_next(semid, v, lim);
    });
    
    on_start2.wait();
    
    uint64_t v = 0;
    while( v!=lim )
      v = wait_next(semid, v, lim);
    
    thr1.join();
    thr2.join();
    
    EXPECT_EQ(v, lim);
    std::cout << "done: " << lim << "\n";
  }
  
  if (semctl(semid, 0, IPC_RMID ) == -1 )
  {
    perror("semctl failure while clearing Reason:");
  }
}

TEST_F(SysVSemaphore, Count1)
{
  int semid = semget(IPC_PRIVATE, 4, SEM_R|SEM_A|IPC_CREAT );
  if( semid < 0 )
  {
    perror("semget failed Reason:");
    return;
  }
  
  // more complex counting test
  //for( int k=0; k<100; ++k )
  {
    std::promise<void> when_started;
    std::future<void> on_start{when_started.get_future()};
    reset_counter(semid);
    
    uint64_t val = 0;
    for( uint64_t i=0;i<1000000;++i )
    {
      increase_counter(semid);
      uint64_t nval = wait_next(semid, val, 1000000);
      EXPECT_EQ(nval,val+1);
      val = nval;
    }
    EXPECT_EQ(wait_next(semid, 0, 1000000), 1000000);
  }
  
  if (semctl(semid, 0, IPC_RMID ) == -1 )
  {
    perror("semctl failure while clearing Reason:");
  }
}

TEST_F(SysVSemaphore, SimpleCount)
{
  int semid = semget(IPC_PRIVATE, 4, SEM_R|SEM_A|IPC_CREAT );
  if( semid < 0 )
  {
    perror("semget failed Reason:");
    return;
  }
  
  // simple counting test
  {
    reset_counter(semid);
    for( int i=0;i<1000000;++i )
      increase_counter(semid);
    EXPECT_EQ(wait_next(semid, 0, 1000000), 1000000);
  }
  
  if (semctl(semid, 0, IPC_RMID ) == -1 )
  {
    perror("semctl failure while clearing Reason:");
  }
}

TEST_F(QueueTest, Dummy)
{
}

int main(int argc, char ** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  auto ret = RUN_ALL_TESTS();
  return ret;
  return 0;
}
