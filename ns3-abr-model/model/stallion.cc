/*
**
** Stallion ABR
**
*/

#include "stallion.h"

namespace ns3 {

  NS_LOG_COMPONENT_DEFINE ("Stallion");

  NS_OBJECT_ENSURE_REGISTERED (Stallion);

  Stallion::Stallion (
    const videoData &videoData,
    const playbackData & playbackData,
    const bufferData & bufferData,
    const throughputData & throughput,
    int64_t chunks, int cmaf) :
      AdaptationAlgorithm (videoData, playbackData, bufferData, throughput),
      m_highestRepIndex (videoData.averageBitrate.size () - 1),
      chunks(chunks),
      cmaf(cmaf)
        {
          NS_LOG_INFO (this);
          NS_ASSERT_MSG (m_highestRepIndex >= 0, "The highest quality representation index should be >= 0");
        }

  algorithmReply Stallion::GetNextRep (const int64_t segmentCounter, int64_t clientId)
  {

    int64_t timeNow = Simulator::Now ().GetMicroSeconds ();
    algorithmReply answer;
    answer.decisionTime = timeNow;
    answer.nextDownloadDelay = 0;
    answer.delayDecisionCase = 0;
    answer.bandwidthEstimate = 0;
    answer.bufferEstimate = 0;
    answer.secondBandwidthEstimate = 0;

    if (segmentCounter < 5) // First 5 segments are transmitted at lowest quality
    {
      answer.nextRepIndex = 0;
      answer.decisionCase = 0;
      return answer;
    }

    double averageThroughput;
    double stdThroughput;
    double averageLatency; //request latency
    double stdLatency;

	  //Moving average
    SAMPLE_SIZE = AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE;
    std::vector<double> thrptEstimationTmp;
    std::vector<double> latencyTmp;
    if(chunks > 0 and cmaf == 1) {

      double tempBytes = 0;
      double tempTime = 0;
      double tempLatency = 0;
      bool expandedSampleSize = false;
        
      for (unsigned sd = m_playbackData.playbackIndex.size (); sd-- > 0; )
      {
        if (m_throughput.bytesReceived.at (sd) == 0)
        {
          continue;
        }
        else
        {
          if((sd % chunks) == 0) { //new seg
            tempBytes += m_throughput.bytesReceived.at (sd);
            tempTime += ((double)((m_throughput.transmissionEnd.at (sd) - m_throughput.transmissionStart.at (sd)) / 1000000.0));
            tempLatency += ((double)((m_throughput.transmissionStart.at (sd) - m_throughput.transmissionRequested.at (sd)) / 1000000.0));
            thrptEstimationTmp.push_back((8*tempBytes)/(tempTime));
            latencyTmp.push_back(tempLatency);
            tempBytes = 0;
            tempTime = 0;
            tempLatency = 0;
          }
          else {
            tempBytes += m_throughput.bytesReceived.at (sd);
            tempTime += ((double)((m_throughput.transmissionEnd.at (sd) - m_throughput.transmissionStart.at (sd)) / 1000000.0));
            tempLatency += ((double)((m_throughput.transmissionStart.at (sd) - m_throughput.transmissionRequested.at (sd)) / 1000000.0));
          }
      
        }
        if (thrptEstimationTmp.size () == (unsigned int)SAMPLE_SIZE)
        {
          if(m_playbackData.playbackIndex.size () > (unsigned int)SAMPLE_SIZE && expandedSampleSize == false) {

            for(unsigned int i=1; i<thrptEstimationTmp.size(); i++) {
              double ratio = thrptEstimationTmp[i]/thrptEstimationTmp[i-1];
              if (ratio >= THROUGHPUT_INCREASE_SCALE || ratio <= 1 / THROUGHPUT_DECREASE_SCALE) {
                SAMPLE_SIZE += 1;
              }
              if((unsigned int)SAMPLE_SIZE >= m_playbackData.playbackIndex.size ()) break;
            }
            expandedSampleSize = true;
            if(SAMPLE_SIZE > AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE) {
              continue;
            }

          }
          break;
        }
      }

    } else {

      bool expandedSampleSize = false;
      for (unsigned sd = m_playbackData.playbackIndex.size (); sd-- > 0; ) {
        if (m_throughput.bytesReceived.at (sd) == 0) {
          continue;
        } else {
          thrptEstimationTmp.push_back ((8.0 * m_throughput.bytesReceived.at (sd)) / ((double)((m_throughput.transmissionEnd.at (sd) - m_throughput.transmissionStart.at (sd)) / 1000000.0)));
          latencyTmp.push_back ((double)((m_throughput.transmissionStart.at (sd) - m_throughput.transmissionRequested.at (sd)) / 1000000.0));
        }
        if (thrptEstimationTmp.size () == (unsigned int)SAMPLE_SIZE) {

          if(m_playbackData.playbackIndex.size () > (unsigned int)SAMPLE_SIZE && expandedSampleSize == false) {

            for(unsigned int i=1; i<thrptEstimationTmp.size(); i++) {
              double ratio = thrptEstimationTmp[i]/thrptEstimationTmp[i-1];
              if (ratio >= THROUGHPUT_INCREASE_SCALE || ratio <= 1 / THROUGHPUT_DECREASE_SCALE) {
                SAMPLE_SIZE += 1;
              }
              if((unsigned int)SAMPLE_SIZE >= m_playbackData.playbackIndex.size ()) break;
            }
            expandedSampleSize = true;
            if(SAMPLE_SIZE > AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE) {
              continue;
            }

          }

          break;
        }
      }

    }

    double totalThrpt = 0;
    for (uint i = 0; i < thrptEstimationTmp.size (); i++) {
      totalThrpt += thrptEstimationTmp.at (i);
    }
    averageThroughput = totalThrpt/thrptEstimationTmp.size ();
    answer.bandwidthEstimate = averageThroughput/(double)1000000;


    // Calculate s.d. of average throughput
    double totalDiff = 0;
    for (uint i = 0; i < thrptEstimationTmp.size (); i++) {
      double tempDiff = (thrptEstimationTmp.at (i) - averageThroughput);
      totalDiff += tempDiff*tempDiff;
    }
    double variance = totalDiff/thrptEstimationTmp.size ();
    stdThroughput = sqrt(variance);

    // Calculate avg. request latency
    double totalLatency = 0;
    for (uint i = 0; i < latencyTmp.size (); i++) {
      totalLatency += latencyTmp.at (i);
    }
    averageLatency = totalLatency/latencyTmp.size ();

    // Calculate s.d. of avg. request latency
    totalDiff = 0;
    for (uint i = 0; i < latencyTmp.size (); i++) {
      double tempDiff = (latencyTmp.at (i) - averageLatency);
      totalDiff += tempDiff*tempDiff;
    }
    variance = totalDiff/latencyTmp.size ();
    stdLatency = sqrt(variance);


    double bitrate = averageThroughput - (THROUGHPUT_SAFETY_FACTOR*stdThroughput);
    double latency = averageLatency + (LATENCY_SAFETY_FACTOR*stdLatency);

    double segDuration = m_videoData.segmentDuration;
    if(chunks > 0) { segDuration = chunks*m_videoData.segmentDuration; }

    if(latency < segDuration) {

  		double deadTimeRatio = latency / (segDuration/1000000);
  		bitrate = bitrate * (1 - deadTimeRatio);

    }

  	answer.nextRepIndex = 0;
    answer.decisionCase = 0;

  	for(int index=m_highestRepIndex; index>=0; index--) {

  		if(bitrate > m_videoData.averageBitrate.at (index)) {

  			answer.nextRepIndex = index;
        answer.decisionCase = 1;

			break;
		}

	}

    return answer;
  }
} // namespace ns3