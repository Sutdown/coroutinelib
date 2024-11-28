#include "timer.h"
#include <unistd.h>
#include<iostream>
using namespace colib;

void func(int i){
  std::cout << "i: " << i << std::endl;
}

int main(){
  std::shared_ptr<TimerManager> manager(new TimerManager());
  std::vector<std::function<void()>> cbs;

  // list expired cb
  {
    for (int i = 0;i<10;i++){
      manager->addTimer((i + 1) * 1000, std::bind(&func, i), false);
    }
    std::cout << "all timers have been set up" << std::endl;

    sleep(5);
    manager->listExpiredCb(cbs);
    while (!cbs.empty())
    {
      std::function<void()> cb = *cbs.begin();
      cbs.erase(cbs.begin());
      cb();
    }

    sleep(5);
    manager->listExpiredCb(cbs);

    while (!cbs.empty())
    {
      std::function<void()> cb = *cbs.begin();
      cbs.erase(cbs.begin());
      cb();
    }
  }

  // recurring
  {
    manager->addTimer(1000, std::bind(&func, 1000), true);
    int j = 10;
    while(j-- > 0){
      sleep(1);
      manager->listExpiredCb(cbs);
      std::function<void()> cb = *cbs.begin();
      cbs.erase(cbs.begin());
      cb();
    }
  }

  return 0;
}
