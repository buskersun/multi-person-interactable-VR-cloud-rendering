/*
**
** Learn2Adapt ABR (based on the dash.js implementation)
**
*/

#include "l2a.h"

namespace ns3 {

  NS_LOG_COMPONENT_DEFINE ("L2A");

  NS_OBJECT_ENSURE_REGISTERED (L2A);

  L2A::L2A (
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

  algorithmReply L2A::GetNextRep (const int64_t segmentCounter, int64_t clientId)
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

      Q = vl;
      for(int i=0; i<CONST_NUMBER_OF_BITRATES; i++) {
        prev_w[i] = 0;
      }
      prev_w[0] = 1;

      lastQuality = 0;
      return answer;
    }

    double segDuration = m_videoData.segmentDuration;
    if(chunks > 0) { segDuration = chunks*m_videoData.segmentDuration; }
    segDuration = segDuration/1000000; //s

    double averageThroughput;
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
            tempLatency += ((double)((m_throughput.transmissionStart.at (sd) - m_throughput.transmissionStart.at (sd)) / 1000000.0));
            thrptEstimationTmp.push_back((8*tempBytes)/(tempTime));
            latencyTmp.push_back(tempLatency);
            tempBytes = 0;
            tempTime = 0;
            tempLatency = 0;
          }
          else {
            tempBytes += m_throughput.bytesReceived.at (sd);
            tempTime += ((double)((m_throughput.transmissionEnd.at (sd) - m_throughput.transmissionStart.at (sd)) / 1000000.0));
            tempLatency += ((double)((m_throughput.transmissionStart.at (sd) - m_throughput.transmissionStart.at (sd)) / 1000000.0));
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
          latencyTmp.push_back ((double)((m_throughput.transmissionStart.at (sd) - m_throughput.transmissionStart.at (sd)) / 1000000.0));
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

    // Moving average
    double totalThrpt = 0;
    for (uint i = 0; i < thrptEstimationTmp.size (); i++) {
      totalThrpt += thrptEstimationTmp.at (i);
    }
    averageThroughput = totalThrpt/thrptEstimationTmp.size ();
    averageThroughput = averageThroughput/1000; //kbps

    double diff1[CONST_NUMBER_OF_BITRATES];

    double lastThroughput;    
    lastThroughput = (8.0 * m_throughput.bytesReceived.at (segmentCounter-1)) / ((double)((m_throughput.transmissionEnd.at (segmentCounter-1) - m_throughput.transmissionStart.at (segmentCounter-1)) / 1000000.0));
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

    answer.bandwidthEstimate = lastThroughput/(double)1000000;
    lastThroughput = lastThroughput/1000; //kbps
    if(lastThroughput < 1) { lastThroughput = 1; }

    double V = segDuration;
    double sign = 1;
    double currentPlaybackRate = 1; //currently the sim model only supports 1x playback speed

    double bitrates[CONST_NUMBER_OF_BITRATES];
    for(int i=0; i < CONST_NUMBER_OF_BITRATES; i++) {

      bitrates[i] = m_videoData.averageBitrate.at (i)/1000; //kbps
      if(currentPlaybackRate * bitrates[i] > lastThroughput) {
        sign = -1;
      }

      w[i] = prev_w[i] + sign * (V / (2 * alpha)) * ((Q + vl) * (currentPlaybackRate * bitrates[i] / lastThroughput));

    }

    double *tempW;
    tempW = euclideanProjection(w);
    for(int i=0; i < CONST_NUMBER_OF_BITRATES; i++) {
      w[i] = tempW[i];
    }

    for(int i=0; i < CONST_NUMBER_OF_BITRATES; i++) {

      diff1[i] = w[i] - prev_w[i];
      prev_w[i] = w[i]; 

    }

    Q = std::max((double)0, Q - V + V * currentPlaybackRate * ((dotmultiplication(bitrates, prev_w) + dotmultiplication(bitrates, diff1)) / lastThroughput));

    double tempBitrates[CONST_NUMBER_OF_BITRATES];

    for(int i=0; i < CONST_NUMBER_OF_BITRATES; i++) {

      tempBitrates[i] = abs(bitrates[i] - dotmultiplication(w, bitrates));

    }

    double lowestValue = *std::min_element(tempBitrates, tempBitrates+CONST_NUMBER_OF_BITRATES);
    int quality = 0;
    for(int i=0; i < CONST_NUMBER_OF_BITRATES; i++) {
      if(tempBitrates[i] == lowestValue) {
        quality = i;
        break;
      }
    }
    answer.decisionCase = 1;

    if(quality > lastQuality) {

      if(bitrates[lastQuality+1] <= lastThroughput) {
        quality = lastQuality+1;
        answer.decisionCase = 2;
      }

    }

    if(bitrates[quality] >= lastThroughput) {
      Q = react * std::max(vl, Q);
    }

    lastQuality = quality;

  	answer.nextRepIndex = quality;

    return answer;
  }

  double L2A::dotmultiplication (double array1[], double array2[]) {

    double sumdot = 0;

    for(int i=0; i<CONST_NUMBER_OF_BITRATES; i++) {

      sumdot += array1[i] * array2[i];

    }

    return sumdot;
  }

  double* L2A::euclideanProjection (double array[]) {

    static double x[CONST_NUMBER_OF_BITRATES];

    bool bget = false;
    double temp[CONST_NUMBER_OF_BITRATES];
    double s[CONST_NUMBER_OF_BITRATES];

    for(int i=0; i<CONST_NUMBER_OF_BITRATES; i++) {
      temp[i] = array[i];
      s[i] = array[i];
    }

    sort(s, s + CONST_NUMBER_OF_BITRATES, std::greater<double>());

    double tmpsum = 0;
    double tmax = 0;

    for(int i=0; i<CONST_NUMBER_OF_BITRATES-1; i++) {

      tmpsum += s[i];
      tmax = (tmpsum-1)/(i+1);
      if(tmax >= s[i+1]) {

        bget = true;
        break;

      }

    }

    if(!bget) { tmax = (tmpsum + s[CONST_NUMBER_OF_BITRATES-1] - 1)/CONST_NUMBER_OF_BITRATES; }

    for(int i=0; i<CONST_NUMBER_OF_BITRATES; i++) {

      x[i] = std::max(temp[i] - tmax, (double)0);

    }

    return x;
  }

} // namespace ns3