#ifndef INVALREADER_H_
#define INVALREADER_H_

#include <app/config/Config.h>
#include <nodes/NodeStoreEx.h>

typedef struct InvalReader InvalReader;
typedef struct InvalReaderPerMeta InvalReaderPerMeta;

void InvalReader_construct(App* app);
void InvalReader_destruct(InvalReader* this);
void InvalReader_updateMetaNodes(InvalReader* this, NumNodeIDList* added, NumNodeIDList* removed);
void InvalReader_startMetaThread(InvalReader* this, NumNodeID nodeId);
void InvalReader_stopMetaThread(InvalReader* this, NumNodeID nodeId);
void InvalReader_stopInvalReader(InvalReader* this);
int32_t InvalReader_getMetaSessionCnt(InvalReader* this, NumNodeID nodeId);


#endif /*INVALREADER_H_*/
