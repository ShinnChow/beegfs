#pragma once

#include <common/app/log/LogContext.h>
#include <common/threading/PThread.h>
#include <common/toolkit/StringTk.h>
#include <components/buddyresyncer/SyncCandidate.h>

#include <mutex>

class BuddyResyncerGatherSlave : public PThread
{
   // Grant access to internal mutex
   friend class BuddyResyncer;
   friend class BuddyResyncJob;

   public:
      BuddyResyncerGatherSlave(MetaSyncCandidateStore* syncCandidates);

      void workLoop();

   private:
      Mutex stateMutex;
      Condition isRunningChangeCond;

      AtomicUInt64 numDirsDiscovered;
      AtomicUInt64 numErrors;

      std::string metaBuddyPath;

      bool isRunning;

      MetaSyncCandidateStore* syncCandidates;

      virtual void run();

      void crawlDir(const std::string& path, const MetaSyncDirType type, const unsigned level = 0);

   public:
      bool getIsRunning()
      {
         std::lock_guard<Mutex> lock(stateMutex);
         return this->isRunning;
      }

      struct Stats
      {
         uint64_t dirsDiscovered;
         uint64_t errors;
      };

      Stats getStats()
      {
         return Stats{ numDirsDiscovered.read(), numErrors.read() };
      }


   private:
      void setIsRunning(const bool isRunning)
      {
         std::lock_guard<Mutex> lock(stateMutex);
         this->isRunning = isRunning;
         isRunningChangeCond.broadcast();
      }

      void addCandidate(const std::string& path, const MetaSyncDirType type)
      {
         const std::string& relPath = path.substr(metaBuddyPath.size() + 1);
         syncCandidates->add(MetaSyncCandidateDir(relPath, type), this,
            [relPath] (unsigned waitedMS, unsigned queueLen, unsigned queueLimit)
            {
               LogContext("BuddyResyncerGatherSlave").log(Log_WARNING,
                  "Possible deadlock detected while adding directory candidate to queue "
                  "; relativePath: " + relPath
                  + "; waitedMS: " + StringTk::uintToStr(waitedMS)
                  + "; queuedEntries: " + StringTk::uintToStr(queueLen)
                  + "; queueLimit: " + StringTk::uintToStr(queueLimit)
                  + "; (hint: try increasing tuneResyncQueueLimit)");
            });
      }
};

typedef std::vector<BuddyResyncerGatherSlave*> BuddyResyncerGatherSlaveVec;
typedef BuddyResyncerGatherSlaveVec::iterator BuddyResyncerGatherSlaveVecIter;
