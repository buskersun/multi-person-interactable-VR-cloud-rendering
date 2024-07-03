/*
**
** LolPlus ABR (based on the dash.js implementation)
**
*/

#include "lolp.h"

namespace ns3 {

  NS_LOG_COMPONENT_DEFINE ("LolPlus");

  NS_OBJECT_ENSURE_REGISTERED (LolPlus);

  LolPlus::LolPlus (
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

  algorithmReply LolPlus::GetNextRep (const int64_t segmentCounter, int64_t clientId)
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
      lastSegmentQuality = 0;
      return answer;
    }

    int currentRepIndex = m_playbackData.playbackIndex.back();

    segmentDuration = m_videoData.segmentDuration;
    if(chunks > 0) { segmentDuration = chunks*m_videoData.segmentDuration; }
    segmentDuration = segmentDuration/1000000; //s

    double safeAverageThroughput;
    SAMPLE_SIZE = AVERAGE_THROUGHPUT_SAMPLE_AMOUNT_LIVE;
    double liveLatency;
    double bufferLevel;
    double playbackRate = 1.0; // Currently, the simulation model supports only 1x playback rate

    std::vector<double> thrptEstimationTmp;
    if(chunks > 0 and cmaf == 1) {

      double tempBytes = 0;
      double tempTime = 0;
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
            thrptEstimationTmp.push_back((8*tempBytes)/(tempTime));
            tempBytes = 0;
            tempTime = 0;
          }
          else {
            tempBytes += m_throughput.bytesReceived.at (sd);
            tempTime += ((double)((m_throughput.transmissionEnd.at (sd) - m_throughput.transmissionStart.at (sd)) / 1000000.0));
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
    safeAverageThroughput = (totalThrpt/thrptEstimationTmp.size ())*0.9;
    answer.bandwidthEstimate = safeAverageThroughput/(double)1000000;

    // Buffer level
    bufferLevel = (m_bufferData.bufferLevelNew.back ()/ (double)1000000 - (timeNow - m_bufferData.timeNow.back())/ (double)1000000);
    // Live Latency
    liveLatency = m_playbackData.currentLiveLatency;

    double minBitrate = m_videoData.averageBitrate.at(0);
    double maxBitrate = m_videoData.averageBitrate.at(m_highestRepIndex);
    qoe voPerSegmentQoe = createQoe(segmentDuration, minBitrate, maxBitrate);

    double currentBitrate = m_videoData.averageBitrate.at(lastSegmentQuality);
    double rebufferTime = 0;
    double lastSegmentDownloadTime = 0;
    lastSegmentDownloadTime = (double)((m_throughput.transmissionEnd.at (segmentCounter-1) - m_throughput.transmissionStart.at (segmentCounter-1)))/(double)1000000;
    if(chunks > 0 and cmaf == 1) {

        double time = 0.0;
        int count = 0;
        for(int index=segmentCounter-1; index>=0; index--)
        {
          time += (m_throughput.transmissionEnd.at (index) - m_throughput.transmissionStart.at (index))/(double)1000000;
          count++;
          if(count > chunks) break;
        }
        lastSegmentDownloadTime = (double)time;

    }

    if(lastSegmentDownloadTime > segmentDuration) { rebufferTime = lastSegmentDownloadTime - segmentDuration; }
    logMetricsInQoe(voPerSegmentQoe, currentBitrate, rebufferTime, liveLatency, playbackRate);


    //learningController setup
    if(!neuronsSetup) {

      // dynamicWeightsSelector
      int valueCount=5;
      double valueList[5] = {0.2, 0.4, 0.6, 0.8, 1};
      
      // should be 625 permutations (5^4)

      int count = 0;
      for(int i=0; i<valueCount; i++) {
        for(int j=0; j<valueCount; j++) {
          for(int k=0; k<valueCount; k++) {
            for(int l=0; l<valueCount; l++) {
              weightOptions[count][0] = valueList[i];
              weightOptions[count][1] = valueList[j];
              weightOptions[count][2] = valueList[k];
              weightOptions[count][3] = valueList[l];
              count++;
            }
          }
        }
      }   

      double magnitude = 0;
      for(int i=0; i<=m_highestRepIndex; i++) {
        magnitude += std::pow(m_videoData.averageBitrate.at(i), 2);
      }
      bitrateNormalizationFactor = sqrt(magnitude);

      for(int i=0; i<=m_highestRepIndex; i++) {
        neuron temp;
        temp.qualityIndex = i;
        temp.bitrate = m_videoData.averageBitrate.at(i);
        temp.state_throughput = m_videoData.averageBitrate.at(i)/bitrateNormalizationFactor;
        temp.state_latency = 0;
        temp.state_rebuffer = 0;
        temp.state_switch = 0;
        somBitrateNeurons.push_back(temp);
      }

      std::vector<std::vector<double>> centers;
      int randomDataSetSize = (int) std::pow(somBitrateNeurons.size(), 2);

      std::srand( (unsigned)time( NULL ) );

      double randomDataSet[randomDataSetSize][4];

      for(int i=0; i<randomDataSetSize; i++) {

        randomDataSet[i][0] = ((double)rand()/RAND_MAX)*getMaxThroughput();
        randomDataSet[i][1] = (double) rand()/RAND_MAX;
        randomDataSet[i][2] = (double) rand()/RAND_MAX;
        randomDataSet[i][3] = (double) rand()/RAND_MAX;

      }

      centers.push_back({randomDataSet[0][0], randomDataSet[0][1], randomDataSet[0][2], randomDataSet[0][3]});

      double distanceWeights[4] = {1, 1, 1, 1};

      for(int i=1; i<=m_highestRepIndex; i++) {

        int nextPoint = 0;
        double maxDistance = 0;
        for(int j=0; j<randomDataSetSize; j++) {

          int currentPoint = j;
          double minDistance = 0;
          for(unsigned int k=0; k<centers.size(); k++) {

            double tempCenter[4] = {centers[k][0], centers[k][1], centers[k][2], centers[k][3]};
            double distance = getDistance(randomDataSet[currentPoint], tempCenter, distanceWeights);
            
            if((i==0 && j==0 && k==0) || distance < minDistance) {
              minDistance = distance;
            }

          }

          if((i==0 && j==0) || minDistance > maxDistance) {

            nextPoint = currentPoint;
            maxDistance = minDistance;

          }
        }
        centers.push_back({randomDataSet[nextPoint][0], randomDataSet[nextPoint][1], randomDataSet[nextPoint][2], randomDataSet[nextPoint][3]});
      }

      double maxDistance = 0;
      int leastSimilarIndex = 0;
      for(unsigned int i=0; i<centers.size(); i++) {

        double distance = 0;

        for(unsigned int j=0; j<centers.size(); j++) {

          if(i == j) continue;
          double tempCenterA[4] = {centers[i][0], centers[i][1], centers[i][2], centers[i][3]};
          double tempCenterB[4] = {centers[j][0], centers[j][1], centers[j][2], centers[j][3]};
          distance += getDistance(tempCenterA, tempCenterB, distanceWeights);

        }

        if(i == 0 || distance > maxDistance) {
          maxDistance = distance;
          leastSimilarIndex = i;
        }

      }

      sortedCenters.push_back(centers[leastSimilarIndex]);
      centers.erase(centers.begin() + leastSimilarIndex);
      while(centers.size() > 0) {

        double minDistance = 0;
        int minIndex = 0;

        for(unsigned int i=0; i<centers.size(); i++) {

          double tempCenterA[4] = {sortedCenters[0][0], sortedCenters[0][1], sortedCenters[0][2], sortedCenters[0][3]};
          double tempCenterB[4] = {centers[i][0], centers[i][1], centers[i][2], centers[i][3]};
          double distance = getDistance(tempCenterA, tempCenterB, distanceWeights);
          if(i == 0 || distance < minDistance) {

            minDistance = distance;
            minIndex = i;

          }

        }

        sortedCenters.push_back(centers[minIndex]);
        centers.erase(centers.begin() + minIndex);

      }

      weights[0] = sortedCenters[sortedCenters.size()-1][0];
      weights[1] = sortedCenters[sortedCenters.size()-1][1];
      weights[2] = sortedCenters[sortedCenters.size()-1][2];
      weights[3] = sortedCenters[sortedCenters.size()-1][3];

      neuronsSetup = true;

    }

    double throughputNormalized = safeAverageThroughput/bitrateNormalizationFactor;
    if(throughputNormalized > 1) {
      throughputNormalized = getMaxThroughput();
    }

    liveLatency = liveLatency/latencyNormalizationFactor;

    double targetLatency = 0;
    double targetRebufferLevel = 0;
    double targetSwitch = 0;
    double throughputDelta = 10000;

    double downloadTime = (somBitrateNeurons[currentRepIndex].bitrate * segmentDuration) / safeAverageThroughput;
    double rebuffer = std::max(0.0, (downloadTime - bufferLevel));

    if(bufferLevel - downloadTime < DWS_BUFFER_MIN) {
      int quality = 0;
      double maxSuitableBitrate = 0;

      for(unsigned int i=0; i<somBitrateNeurons.size(); i++) {

        if(somBitrateNeurons[i].bitrate < somBitrateNeurons[currentRepIndex].bitrate && somBitrateNeurons[i].bitrate > maxSuitableBitrate && safeAverageThroughput > somBitrateNeurons[i].bitrate) {

          maxSuitableBitrate = somBitrateNeurons[i].bitrate;
          quality = somBitrateNeurons[i].qualityIndex;

        }

      }

      lastSegmentQuality = quality;
      answer.nextRepIndex = quality;

      return answer;
    }

    //dynamicWeightSelection
    findWeightVector(liveLatency, bufferLevel, rebuffer, safeAverageThroughput, playbackRate);

    double minDistance = 0;
    int winnerNeuron = 0;

    for(unsigned int i=0; i<somBitrateNeurons.size(); i++) {

      double distanceWeights[4];
      for(int j=0; j<4; j++) { distanceWeights[j] = weights[j]; }

      double downloadTimeTemp = ((somBitrateNeurons[i].bitrate*segmentDuration)/safeAverageThroughput);
      double nextBuffer = 0;
      if(downloadTimeTemp > segmentDuration) {
        nextBuffer = bufferLevel - segmentDuration;
      }
      else {
        nextBuffer = bufferLevel + segmentDuration - downloadTimeTemp;
      }

      bool isBufferLow = nextBuffer < DWS_BUFFER_MIN;

      if(somBitrateNeurons[i].bitrate > safeAverageThroughput - throughputDelta || isBufferLow) {
        if(somBitrateNeurons[i].bitrate != somBitrateNeurons[0].bitrate) {
          distanceWeights[0] = 100;
        }
      }

      double tempA[4] = {somBitrateNeurons[i].state_throughput, somBitrateNeurons[i].state_latency, somBitrateNeurons[i].state_rebuffer, somBitrateNeurons[i].state_switch};
      double tempB[4] = {throughputNormalized, targetLatency, targetRebufferLevel, targetSwitch};
      double distance = getDistance(tempA, tempB, distanceWeights);
      if(i == 0 || distance < minDistance) {
        minDistance = distance;
        winnerNeuron = i;
      }

    }

    double bitrateSwitch = abs(somBitrateNeurons[currentRepIndex].bitrate - somBitrateNeurons[winnerNeuron].bitrate) / bitrateNormalizationFactor;
    updateNeurons(currentRepIndex, throughputNormalized, liveLatency, rebuffer, bitrateSwitch);

    updateNeurons(winnerNeuron, throughputNormalized, targetLatency, targetRebufferLevel, bitrateSwitch);

    lastSegmentQuality = somBitrateNeurons[winnerNeuron].qualityIndex;
  	answer.nextRepIndex = somBitrateNeurons[winnerNeuron].qualityIndex;

    return answer;
  }

// Functions

  LolPlus::qoe LolPlus::createQoe(double segmentDuration, double maxBitrate, double minBitrate) {

    qoe newQoe;

    newQoe.weights.bitrateReward = segmentDuration;
    newQoe.weights.bitrateSwitchPenalty = 1;
    newQoe.weights.rebufferPenalty = maxBitrate;

    latency_weight lowerLatency;
    lowerLatency.threshold = 1.1;
    lowerLatency.penalty = minBitrate*0.05;

    latency_weight higherLatency;
    higherLatency.threshold = 100000000;
    higherLatency.penalty = maxBitrate*0.1;

    newQoe.weights.latencyPenaltyLow = lowerLatency;
    newQoe.weights.latencyPenaltyHigh = higherLatency;

    newQoe.weights.playbackSpeedPenalty = minBitrate;


    return newQoe;
  }

  void LolPlus::logMetricsInQoe(qoe &qoeInfo, double bitrate, double rebufferTime, double latency, double playbackSpeed) {

    qoeInfo.bitrateWSum += (qoeInfo.weights.bitrateReward * bitrate);

    if (qoeInfo.lastBitrate > -1) {
      qoeInfo.bitrateSwitchWSum += (qoeInfo.weights.bitrateSwitchPenalty * abs(bitrate - qoeInfo.lastBitrate));
    }

    qoeInfo.lastBitrate = bitrate;
    qoeInfo.rebufferWSum += (qoeInfo.weights.rebufferPenalty * rebufferTime);

    if(latency < qoeInfo.weights.latencyPenaltyLow.threshold) {
      qoeInfo.latencyWSum += (qoeInfo.weights.latencyPenaltyLow.penalty * latency);
    }
    else if(latency < qoeInfo.weights.latencyPenaltyHigh.threshold) {
      qoeInfo.latencyWSum += (qoeInfo.weights.latencyPenaltyHigh.penalty * latency);
    }

    qoeInfo.playbackSpeedWSum += (qoeInfo.weights.playbackSpeedPenalty * abs(1 - playbackSpeed));
    qoeInfo.totalQoe = qoeInfo.bitrateWSum - qoeInfo.bitrateSwitchWSum - qoeInfo.rebufferWSum - qoeInfo.latencyWSum - qoeInfo.playbackSpeedWSum;

  }

  double LolPlus::getMaxThroughput() {

    double highestThroughput = 0;

    for(unsigned int i=0; i<somBitrateNeurons.size(); i++) {

      if(somBitrateNeurons[i].state_throughput > highestThroughput) {
        highestThroughput = somBitrateNeurons[i].state_throughput;
      }

    }

    return highestThroughput;
  }

  double LolPlus::getDistance(double a[], double b[], double w[]) {

    double sum = 0;

    for(int i = 0; i < 4; ++i) {
      sum += w[i] * (std::pow((a[i] - b[i]), 2));
    }

    int sign = (sum < 0) ? -1 : 1;

    return sign * sqrt(abs(sum));
  }

  void LolPlus::findWeightVector(double latency, double buffer, double rebuffer, double throughput, double playbackRate) {

    double maxQoe = 0;
    double winnerWeights[4] = {0,0,0,0};
    double winnerBitrate = 0;

    for(unsigned int i=0; i<somBitrateNeurons.size(); i++) {

      for(int j=0; j<(int)std::pow(5.0, weightTypeCount); j++) {

        double downloadTime = ((somBitrateNeurons[i].bitrate * segmentDuration) / throughput);
        double nextBuffer = 0;
        if(downloadTime > segmentDuration) {
          nextBuffer = buffer - segmentDuration;
        }
        else {
          nextBuffer = buffer + segmentDuration - downloadTime;
        }
        double nextRebuffer = std::max(0.00001, (downloadTime - nextBuffer));
        double wt = 0;
        if(weightOptions[j][2] == 0) {
          wt = 10;
        }
        else {
          wt = (1/weightOptions[j][2]);
        }
        double weightedRebuffer = wt * nextRebuffer;

        if(weightOptions[j][1] == 0) {
          wt = 10;
        }
        else {
          wt = (1/weightOptions[j][1]);
        }
        double weightedLatency = wt * somBitrateNeurons[i].state_latency;


        double minBitrate = m_videoData.averageBitrate.at(0);
        double maxBitrate = m_videoData.averageBitrate.at(m_highestRepIndex);
        qoe tempQoe = createQoe(segmentDuration, minBitrate, maxBitrate);
        logMetricsInQoe(tempQoe, somBitrateNeurons[i].bitrate, weightedRebuffer, weightedLatency, playbackRate);
        double totalQoe = tempQoe.totalQoe;
        if((i == 0 && j == 0) || (totalQoe > maxQoe && nextBuffer >= DWS_BUFFER_MIN)) {
          maxQoe = totalQoe;
          for(int k=0; k<4; k++) { winnerWeights[k] = weightOptions[j][k]; }
          winnerBitrate = somBitrateNeurons[i].bitrate;
        }

      }

    }

    if(winnerBitrate > 0) {
      for(int k=0; k<4; k++) { weights[k] = winnerWeights[k]; }
    }

    previousLatency = latency;

  }

  void LolPlus::updateNeurons(int neuron, double throughput, double latency, double rebuffer, double s_switch) {

    for(unsigned int i=0; i<somBitrateNeurons.size(); i++) {

      double sigma = 0.1;
      double tempA[4] = {somBitrateNeurons[i].state_throughput, somBitrateNeurons[i].state_latency, somBitrateNeurons[i].state_rebuffer, somBitrateNeurons[i].state_switch}; 
      double tempB[4] = {somBitrateNeurons[neuron].state_throughput, somBitrateNeurons[neuron].state_latency, somBitrateNeurons[neuron].state_rebuffer, somBitrateNeurons[neuron].state_switch};
      double tempC[4] = {1,1,1,1};
      double distance = getDistance(tempA, tempB, tempC);
      double neighbourHood = std::exp(-1 * std::pow(distance, 2) / (2 * std::pow(sigma, 2)));

      double w[4] = {0.01, 0.01, 0.01, 0.01};
      somBitrateNeurons[neuron].state_throughput = somBitrateNeurons[neuron].state_throughput + (throughput - somBitrateNeurons[neuron].state_throughput) * w[0] * neighbourHood;
      somBitrateNeurons[neuron].state_latency = somBitrateNeurons[neuron].state_latency + (latency - somBitrateNeurons[neuron].state_latency) * w[1] * neighbourHood;
      somBitrateNeurons[neuron].state_rebuffer = somBitrateNeurons[neuron].state_rebuffer + (rebuffer - somBitrateNeurons[neuron].state_rebuffer) * w[2] * neighbourHood;
      somBitrateNeurons[neuron].state_switch = somBitrateNeurons[neuron].state_switch + (s_switch - somBitrateNeurons[neuron].state_switch) * w[3] * neighbourHood;

    }

  }

} // namespace ns3