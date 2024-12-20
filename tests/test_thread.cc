#include "../src/thread/thread.h"
#include <iostream>
#include <memory>
#include <vector>
#include <unistd.h>

using namespace colib;

void func()
{
  std::cout << "id: " << Thread::GetThreadID() << ", name: " << Thread::GetName();
  std::cout << ", this id: " << Thread::GetThis()->getID() << ", this name: " << Thread::GetThis()->getName() << std::endl;

  sleep(60);
}

int main()
{
  std::vector<std::shared_ptr<Thread>> thrs;

  for (int i = 0; i < 5; i++)
  {
    std::shared_ptr<Thread> thr = std::make_shared<Thread>(&func, "thread_" + std::to_string(i));
    thrs.push_back(thr);
  }

  for (int i = 0; i < 5; i++)
  {
    thrs[i]->join();
  }

  return 0;
}