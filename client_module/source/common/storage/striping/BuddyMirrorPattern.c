#include <common/toolkit/MathTk.h>
#include "BuddyMirrorPattern.h"

bool BuddyMirrorPattern_deserializePattern(StripePattern* this, DeserializeCtx* ctx)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   RawList mirrorBuddyGroupIDsVec;

   // defaultNumTargets
   if(!Serialization_deserializeUInt(ctx, &thisCast->defaultNumTargets) )
      return false;

   // mirrorBuddyGroupIDs
   if(!Serialization_deserializeUInt16VecPreprocess(ctx, &mirrorBuddyGroupIDsVec) )
      return false;

   if(!Serialization_deserializeUInt16Vec(&mirrorBuddyGroupIDsVec, &thisCast->mirrorBuddyGroupIDs) )
      return false;

   return true;
}

size_t BuddyMirrorPattern_getStripeTargetIndex(StripePattern* this, int64_t pos)
{
   struct BuddyMirrorPattern* p = container_of(this, struct BuddyMirrorPattern, stripePattern);

   return (pos / this->chunkSize) % UInt16Vec_length(&p->mirrorBuddyGroupIDs);
}

uint16_t BuddyMirrorPattern_getStripeTargetID(StripePattern* this, int64_t pos)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   /*
    * Directory patterns carry no assigned targets (only defaultNumTargets).
    * Calling getStripeTargetID() on a directory pattern is a programming error;
    * return 0 (the BeeGFS "no target" sentinel, same as SimplePattern_getStripeTargetID).
    */
   BEEGFS_BUG_ON(!UInt16Vec_length(&thisCast->mirrorBuddyGroupIDs),
      "Stripe pattern has no assigned target IDs; likely a directory pattern "
      "(directories carry only defaultNumTargets, not actual target IDs)");
   if (!UInt16Vec_length(&thisCast->mirrorBuddyGroupIDs))
      return 0;

   return UInt16Vec_at(&thisCast->mirrorBuddyGroupIDs,
      BuddyMirrorPattern_getStripeTargetIndex(this, pos));
}

void BuddyMirrorPattern_getStripeTargetIDsCopy(StripePattern* this, UInt16Vec* outTargetIDs)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   ListTk_copyUInt16ListToVec( (UInt16List*)&thisCast->mirrorBuddyGroupIDs, outTargetIDs);
}

UInt16Vec* BuddyMirrorPattern_getStripeTargetIDs(StripePattern* this)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   return &thisCast->mirrorBuddyGroupIDs;
}

unsigned BuddyMirrorPattern_getMinNumTargets(StripePattern* this)
{
   return 1;
}

unsigned BuddyMirrorPattern_getDefaultNumTargets(StripePattern* this)
{
   BuddyMirrorPattern* thisCast = (BuddyMirrorPattern*)this;

   return thisCast->defaultNumTargets;
}


