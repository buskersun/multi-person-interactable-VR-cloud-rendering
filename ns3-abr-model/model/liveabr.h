/*
**
** Live Dash ABR
**
 */

#ifndef LIVEABR_ALGORITHM_H
#define LIVEABR_ALGORITHM_H

#include "tcp-stream-adaptation-algorithm.h"

namespace ns3 {

/**
 * \ingroup tcpStream
 * \brief Implementation of Live DASH ABR
 */
class LiveABR : public AdaptationAlgorithm
{
public:
  LiveABR (  const videoData &videoData,
                      const playbackData & playbackData,
                      const bufferData & bufferData,
                      const throughputData & throughput,
					  int64_t chunks, int cmaf, int abrParameter);

  algorithmReply GetNextRep (const int64_t segmentCounter, int64_t clientId);

private:

  const int64_t m_highestRepIndex;
  int64_t chunks;
  int cmaf;
  int harmonicMeanSize;

};

} // namespace ns3
#endif /* LIVEABR_ALGORITHM_H */