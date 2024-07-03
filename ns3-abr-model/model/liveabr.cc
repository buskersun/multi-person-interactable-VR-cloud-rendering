/*
**
** Llama ABR
**
*/

#include "liveabr.h"

namespace ns3 {

  NS_LOG_COMPONENT_DEFINE ("LiveABR");

  NS_OBJECT_ENSURE_REGISTERED (LiveABR);

  LiveABR::LiveABR (
    const videoData &videoData,
    const playbackData & playbackData,
    const bufferData & bufferData,
    const throughputData & throughput,
    int64_t chunks, int cmaf, int abrParameter) :
      AdaptationAlgorithm (videoData, playbackData, bufferData, throughput),
      m_highestRepIndex (videoData.averageBitrate.size () - 1),
      chunks(chunks),
      cmaf(cmaf),
      harmonicMeanSize(abrParameter)
        {
          NS_LOG_INFO (this);
          NS_ASSERT_MSG (m_highestRepIndex >= 0, "The highest quality representation index should be >= 0");
          if(harmonicMeanSize == 0) { harmonicMeanSize = 20; }
        }

  algorithmReply LiveABR::GetNextRep (const int64_t segmentCounter, int64_t clientId)
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

	double lastThroughput;
    //Calculate throughput based on a single segment (lastThroughput)
    lastThroughput = (8.0 * m_throughput.bytesReceived.at (segmentCounter-1)) / ((double)((m_throughput.transmissionEnd.at (segmentCounter-1) - m_throughput.transmissionRequested.at (segmentCounter-1)) / 1000000.0));
    if(chunks > 0 and cmaf == 1) {

        double bytes = 0.0;
        double time = 0.0;
        int count = 0;
        for(int index=segmentCounter-1; index>=0; index--)
        {
          bytes += m_throughput.bytesReceived.at(index);
          time += (m_throughput.transmissionEnd.at (index) - m_throughput.transmissionStart.at (index))/(double)1000000;
          count++;
          if(count > chunks) break;
        }
        lastThroughput = (bytes*8 / (double)time);

    }
    answer.secondBandwidthEstimate = lastThroughput/(double)1000000;

    double smoothedMeasurement;

	//Harmonic mean
    std::vector<double> thrptEstimationTmp;
    if(chunks > 0 and cmaf == 1) {

      double tempBytes = 0;
      double tempTime = 0;
      
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
            tempTime += ((double)((m_throughput.transmissionEnd.at (sd) - m_throughput.transmissionRequested.at (sd)) / 1000000.0));
            thrptEstimationTmp.push_back((8*tempBytes)/(tempTime));
            tempBytes = 0;
            tempTime = 0;
          }
          else {
            tempBytes += m_throughput.bytesReceived.at (sd);
            tempTime += ((double)((m_throughput.transmissionEnd.at (sd) - m_throughput.transmissionRequested.at (sd)) / 1000000.0));
          }
          
        }
        if (thrptEstimationTmp.size () == (unsigned int)harmonicMeanSize)
        {
          break;
        }
      }

    } else {

      for (unsigned sd = m_playbackData.playbackIndex.size (); sd-- > 0; ) {
        if (m_throughput.bytesReceived.at (sd) == 0) {
          continue;
        } else {
          thrptEstimationTmp.push_back ((8.0 * m_throughput.bytesReceived.at (sd)) / ((double)((m_throughput.transmissionEnd.at (sd) - m_throughput.transmissionRequested.at (sd)) / 1000000.0)));
        }
        if (thrptEstimationTmp.size () == (unsigned int)harmonicMeanSize) {
          break;
        }
      }

    }

    // HARMONIC MEAN
    double harmonicMeanDenominator = 0;
    for (uint i = 0; i < thrptEstimationTmp.size (); i++) {
      harmonicMeanDenominator += 1 / (thrptEstimationTmp.at (i));
    }
    double harmonic_bandwidth = thrptEstimationTmp.size () / harmonicMeanDenominator;
    answer.bandwidthEstimate = harmonic_bandwidth/(double)1000000;
    smoothedMeasurement = harmonic_bandwidth;



    int64_t currentRepIndex = m_playbackData.playbackIndex.back ();
    int64_t higherRepIndex = currentRepIndex+1;
    if(higherRepIndex > m_highestRepIndex) { higherRepIndex = m_highestRepIndex; }
    if(currentRepIndex > 0 and m_videoData.averageBitrate.at (currentRepIndex) > lastThroughput) {

          answer.nextRepIndex = currentRepIndex-1;
          answer.decisionCase = 3;

    } else if(currentRepIndex < m_highestRepIndex and m_videoData.averageBitrate.at (currentRepIndex+1) < smoothedMeasurement) {

      if(m_videoData.averageBitrate.at (currentRepIndex+1) > lastThroughput) {

        //fine grain measurement takes priority
        answer.nextRepIndex = currentRepIndex;
        answer.decisionCase = 0;

      } else {

        //Switch up
        answer.nextRepIndex = currentRepIndex+1;
        answer.decisionCase = 1;

      }

    } else {

      //Stay the same
      answer.nextRepIndex = currentRepIndex;
      answer.decisionCase = 0;

    }

    return answer;
  }
} // namespace ns3