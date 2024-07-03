/*
**
** Stallion ABR
**
 */

#ifndef STALLION_ALGORITHM_H
#define STALLION_ALGORITHM_H

#include "tcp-stream-adaptation-algorithm.h"

namespace ns3 {

/**
 * \ingroup tcpStream
 * \brief Implementation of Stallion ABR
 */
class Stallion : public AdaptationAlgorithm
{
public:
  Stallion (  const videoData &videoData,
                      const playbackData & playbackData,
                      const bufferData & bufferData,
                      const throughputData & throughput,
					  int64_t chunks, int cmaf);

  algorithmReply GetNextRep (const int64_t segmentCounter, int64_t clientId);

private:

  const int64_t m_highestRepIndex;
  int64_t chunks;
  int cmaf;

  int THROUGHPUT_SAFETY_FACTOR = 1;
  int LATENCY_SAFETY_FACTOR = 1.25;
  int const AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE = 3;
  int SAMPLE_SIZE = AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE;
  double const THROUGHPUT_DECREASE_SCALE = 1.3;
  double const THROUGHPUT_INCREASE_SCALE = 1.3;

};

} // namespace ns3
#endif /* STALLION_ALGORITHM_H */