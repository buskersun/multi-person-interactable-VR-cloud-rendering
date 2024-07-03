/*
**
** Learn2Adapt ABR
**
 */
#define CONST_NUMBER_OF_BITRATES 5

#ifndef L2A_ALGORITHM_H
#define L2A_ALGORITHM_H

#include "tcp-stream-adaptation-algorithm.h"

namespace ns3 {

/**
 * \ingroup tcpStream
 * \brief Implementation of Learn2Adapt ABR
 */
class L2A : public AdaptationAlgorithm
{
public:
  L2A (  const videoData &videoData,
                      const playbackData & playbackData,
                      const bufferData & bufferData,
                      const throughputData & throughput,
					  int64_t chunks, int cmaf);

  algorithmReply GetNextRep (const int64_t segmentCounter, int64_t clientId);

private:

  double dotmultiplication (double array1[], double array2[]);

  double* euclideanProjection (double array[]);

  const int64_t m_highestRepIndex;
  int64_t chunks;
  int cmaf;

  int const AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE = 3;
  int SAMPLE_SIZE = AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE;
  double const THROUGHPUT_DECREASE_SCALE = 1.3;
  double const THROUGHPUT_INCREASE_SCALE = 1.3;

  int lastQuality;

  double w[CONST_NUMBER_OF_BITRATES];
  double prev_w[CONST_NUMBER_OF_BITRATES];
  double Q = 0;
  double B_target = 1.5;

  double horizon = 4;
  double vl = pow(horizon, 0.99);
  double alpha = std::max(pow(horizon, 1), vl * sqrt(horizon));

  double react = 2;

};

} // namespace ns3
#endif /* L2A_ALGORITHM_H */