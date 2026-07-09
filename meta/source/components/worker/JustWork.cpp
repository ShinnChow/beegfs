#include <common/components/worker/JustWork.h>
#include <common/components/worker/Work.h>
#include <program/Program.h>

struct JustWorkImpl : Work
{
   JustWorkFunc *func;
   void *data;

   JustWorkImpl(JustWorkFunc *func, void *data)
      : func(func), data(data) {}

   void process(char* bufIn, unsigned bufInLen, char* bufOut, unsigned bufOutLen) override
   {
      (void) bufIn;
      (void) bufInLen;
      (void) bufOut;
      (void) bufOutLen;
      func(data);
   }
};

void just_enqueue_work(JustWorkFunc *func, void *data)
{
   auto work = new JustWorkImpl(func, data);
   Program::getApp()->getWorkQueue()->addDirectWork(work);
};
